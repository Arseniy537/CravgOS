/* net.h - сетевой стек: Ethernet, ARP, IPv4, ICMP, UDP, DHCP, DNS.
   Чистый C: драйвер карты и таймер подключаются через указатели. */
#ifndef NET_H
#define NET_H
#define ETH_ARP 0x0806
#define ETH_IP  0x0800

static u8  nmac[6];
static u32 nip, nmask, ngw, ndns;          /* адреса в порядке a.b.c.d = a<<24|b<<16|c<<8|d */
static int (*nic_tx)(const u8 *, int);
static int (*nic_rx)(u8 *, int);           /* длина кадра или 0 */
static u32 (*ms_now)(void);
static u8 txb[1600], rxb[1600], ipb[1600], udpb[1500], icb[1500];
static u32 st_tx, st_rx, st_arp, st_ip, st_udp, st_dns;     /* счётчики кадров для диагностики */
static const u8 bcast[6] = {255, 255, 255, 255, 255, 255};
static const u8 zero6[6] = {0, 0, 0, 0, 0, 0};

static void be16(u8 *p, u16 v) { p[0] = v >> 8; p[1] = v; }
static void be32(u8 *p, u32 v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static u16 rd16(const u8 *p) { return (u16)((p[0] << 8) | p[1]); }
static u32 rd32(const u8 *p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; }
static u16 net_cksum(const u8 *d, int len) {
    u32 s = 0;
    for (; len > 1; len -= 2, d += 2) s += (d[0] << 8) | d[1];
    if (len) s += d[0] << 8;
    while (s >> 16) s = (s & 0xFFFF) + (s >> 16);
    return (u16)~s;
}

/* ---- ARP ---- */
#define ARPN 8
static u32 arp_ip[ARPN];
static u8 arp_mac[ARPN][6];
static int arp_next;
static const u8 *arp_find(u32 ip) {
    for (int i = 0; i < ARPN; i++) if (ip && arp_ip[i] == ip) return arp_mac[i];
    return 0;
}
static void arp_add(u32 ip, const u8 *m) {
    int i;
    for (i = 0; i < ARPN; i++) if (arp_ip[i] == ip) break;
    if (i == ARPN) { i = arp_next; arp_next = (arp_next + 1) % ARPN; }
    arp_ip[i] = ip;
    for (int k = 0; k < 6; k++) arp_mac[i][k] = m[k];
}
static void arp_clear(void) { for (int i = 0; i < ARPN; i++) arp_ip[i] = 0; }

static int eth_send(const u8 *dst, u16 type, const u8 *pl, int len) {
    if (len > 1500) return -1;
    for (int i = 0; i < 6; i++) { txb[i] = dst[i]; txb[6 + i] = nmac[i]; }
    be16(txb + 12, type);
    for (int i = 0; i < len; i++) txb[14 + i] = pl[i];
    int n = 14 + len;
    while (n < 60) txb[n++] = 0;
    st_tx++;
    return nic_tx(txb, n);
}
static void arp_send(u16 op, const u8 *dmac, u32 tip, const u8 *tmac) {
    u8 p[28];
    be16(p, 1); be16(p + 2, 0x0800); p[4] = 6; p[5] = 4; be16(p + 6, op);
    for (int i = 0; i < 6; i++) { p[8 + i] = nmac[i]; p[18 + i] = tmac[i]; }
    be32(p + 14, nip); be32(p + 24, tip);
    eth_send(dmac, ETH_ARP, p, 28);
}

/* ---- IPv4 ---- */
static u16 ip_id;
static int ip_send_to(const u8 *dmac, u32 src, u32 dst, u8 proto, const u8 *pl, int len) {
    if (len > 1480) return -1;
    u8 *p = ipb;
    p[0] = 0x45; p[1] = 0; be16(p + 2, 20 + len); be16(p + 4, ++ip_id); be16(p + 6, 0x4000);
    p[8] = 64; p[9] = proto; be16(p + 10, 0); be32(p + 12, src); be32(p + 16, dst);
    be16(p + 10, net_cksum(p, 20));
    for (int i = 0; i < len; i++) p[20 + i] = pl[i];
    return eth_send(dmac, ETH_IP, p, 20 + len);
}
static void net_poll(void);
static const u8 *net_mac_for(u32 dst) {
    u32 hop = ((dst & nmask) == (nip & nmask)) ? dst : ngw;
    if (!hop) return 0;
    const u8 *m = arp_find(hop);
    if (m) return m;
    for (int t = 0; t < 4; t++) {
        arp_send(1, bcast, hop, zero6);
        u32 t0 = ms_now();
        while (ms_now() - t0 < 600) {
            net_poll();
            m = arp_find(hop);
            if (m) return m;
        }
    }
    return 0;
}
static u16 udp_sum(u32 src, u32 dst, const u8 *seg, int len) {
    u32 sum = (src >> 16) + (src & 0xFFFF) + (dst >> 16) + (dst & 0xFFFF) + 17 + len;
    int n = len;
    for (; n > 1; n -= 2, seg += 2) sum += (seg[0] << 8) | seg[1];
    if (n) sum += seg[0] << 8;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (u16)~sum;
}
static int udp_send(const u8 *dmac, u32 src, u32 dst, u16 sp, u16 dp, const u8 *d, int len) {
    if (len > 1400) return -1;
    be16(udpb, sp); be16(udpb + 2, dp); be16(udpb + 4, 8 + len); be16(udpb + 6, 0);
    for (int i = 0; i < len; i++) udpb[8 + i] = d[i];
    { u16 ck = udp_sum(src, dst, udpb, 8 + len); be16(udpb + 6, ck ? ck : 0xFFFF); }
    return ip_send_to(dmac, src, dst, 17, udpb, 8 + len);
}

/* ---- ICMP / ping ---- */
static u16 ping_id = 0x4352, ping_seq_rx;
static int ping_rx;
static u8 ping_ttl;
static int net_ping(u32 dst, u16 seq, u32 timeout, u32 *rtt) {
    u8 p[40];
    p[0] = 8; p[1] = 0; be16(p + 2, 0); be16(p + 4, ping_id); be16(p + 6, seq);
    for (int i = 0; i < 32; i++) p[8 + i] = 'a' + (i % 23);
    be16(p + 2, net_cksum(p, 40));
    const u8 *m = (dst == 0xFFFFFFFFu) ? bcast : net_mac_for(dst);
    if (!m) return -2;
    ping_rx = 0;
    u32 t0 = ms_now();
    ip_send_to(m, nip, dst, 1, p, 40);
    while (ms_now() - t0 < timeout) {
        net_poll();
        if (ping_rx && ping_seq_rx == seq) { *rtt = ms_now() - t0; return 0; }
    }
    return -1;
}

/* ---- DHCP ---- */
static u32 dhcp_xid = 0x43525647u;
static int dhcp_got;
static u32 dhcp_yi, dhcp_srv, dhcp_mask_, dhcp_gw_, dhcp_dns_;
static void dhcp_in(const u8 *d, int len) {
    if (len < 240 || d[0] != 2 || rd32(d + 4) != dhcp_xid || rd32(d + 236) != 0x63825363u) return;
    for (int i = 0; i < 6; i++) if (d[28 + i] != nmac[i]) return;
    int type = 0, i = 240;
    u32 mask = 0, gw = 0, dns = 0, srv = 0;
    while (i < len && d[i] != 255) {
        if (d[i] == 0) { i++; continue; }
        if (i + 1 >= len) break;
        int ol = d[i + 1];
        if (i + 2 + ol > len) break;
        const u8 *o = d + i + 2;
        switch (d[i]) {
            case 53: type = o[0]; break;
            case 1:  if (ol >= 4) mask = rd32(o); break;
            case 3:  if (ol >= 4) gw = rd32(o); break;
            case 6:  if (ol >= 4) dns = rd32(o); break;
            case 54: if (ol >= 4) srv = rd32(o); break;
        }
        i += 2 + ol;
    }
    if (type == 2 || type == 5) {
        dhcp_got = type; dhcp_yi = rd32(d + 16); dhcp_srv = srv;
        dhcp_mask_ = mask; dhcp_gw_ = gw; dhcp_dns_ = dns;
    }
}
static void dhcp_send(int type) {
    static u8 b[300];
    for (int i = 0; i < 300; i++) b[i] = 0;
    b[0] = 1; b[1] = 1; b[2] = 6;
    be32(b + 4, dhcp_xid); be16(b + 10, 0x8000);
    for (int i = 0; i < 6; i++) b[28 + i] = nmac[i];
    be32(b + 236, 0x63825363u);
    int i = 240;
    b[i++] = 53; b[i++] = 1; b[i++] = type;
    if (type == 3) {
        b[i++] = 50; b[i++] = 4; be32(b + i, dhcp_yi); i += 4;
        b[i++] = 54; b[i++] = 4; be32(b + i, dhcp_srv); i += 4;
    }
    b[i++] = 55; b[i++] = 3; b[i++] = 1; b[i++] = 3; b[i++] = 6;
    b[i++] = 255;
    udp_send(bcast, 0, 0xFFFFFFFFu, 68, 67, b, 300);
}
static int dhcp_wait(int want, u32 ms) {
    u32 t0 = ms_now();
    while (ms_now() - t0 < ms) { net_poll(); if (dhcp_got == want) return 1; }
    return 0;
}
static int net_dhcp_ex(int tries, u32 ms) {
    u32 save = nip;
    nip = 0;
    for (int t = 0; t < tries; t++) {
        dhcp_got = 0;
        dhcp_send(1);
        if (!dhcp_wait(2, ms)) continue;
        dhcp_send(3);
        dhcp_got = 0;
        if (dhcp_wait(5, ms)) {
            nip = dhcp_yi;
            nmask = dhcp_mask_ ? dhcp_mask_ : 0xFFFFFF00u;
            ngw = dhcp_gw_; ndns = dhcp_dns_;
            arp_clear();
            return 0;
        }
    }
    nip = save;
    return -1;
}

static int net_dhcp(void) { return net_dhcp_ex(3, 2000); }

/* ---- DNS ---- */
static u16 dns_id = 0x1234;
static u16 dns_port = 53053;
static int dns_done;
static u32 dns_ip;
static int dns_skip(const u8 *d, int len, int i) {
    while (i < len) {
        int c = d[i];
        if (c == 0) return i + 1;
        if ((c & 0xC0) == 0xC0) return (i + 2 <= len) ? i + 2 : -1;
        i += c + 1;
    }
    return -1;
}

/* ---- TCP (клиент, одно соединение) ---- */
#define TCP_CLOSED 0
#define TCP_SYN    1
#define TCP_ESTAB  2
#define TCP_FINW   3
static int tcp_state;
static u32 tcp_rip, tcp_snd_nxt, tcp_snd_una, tcp_rcv_nxt, tcp_iss;
static u16 tcp_lport = 40000, tcp_rport;
static u8 tcp_dmac[6];
static u8 *tcp_buf;
static int tcp_blen, tcp_bmax, tcp_fin, tcp_rst, tcp_overflow;
static u8 tcp_seg[1500];

static u16 tcp_sum(u32 src, u32 dst, const u8 *s, int len) {
    u32 sum = (src >> 16) + (src & 0xFFFF) + (dst >> 16) + (dst & 0xFFFF) + 6 + len;
    int n = len;
    for (; n > 1; n -= 2, s += 2) sum += (s[0] << 8) | s[1];
    if (n) sum += s[0] << 8;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (u16)~sum;
}
static void tcp_emit(u8 flags, const u8 *data, int len) {
    u8 *h = tcp_seg;
    int hl = (flags & 0x02) ? 24 : 20;
    be16(h, tcp_lport); be16(h + 2, tcp_rport);
    be32(h + 4, tcp_snd_nxt); be32(h + 8, (flags & 0x10) ? tcp_rcv_nxt : 0);
    h[12] = (hl / 4) << 4; h[13] = flags; be16(h + 14, 8192); be16(h + 16, 0); be16(h + 18, 0);
    if (hl == 24) { h[20] = 2; h[21] = 4; be16(h + 22, 1460); }
    for (int i = 0; i < len; i++) h[hl + i] = data[i];
    be16(h + 16, tcp_sum(nip, tcp_rip, h, hl + len));
    ip_send_to(tcp_dmac, nip, tcp_rip, 6, h, hl + len);
}
static int tcp_connect(u32 ip, u16 port) {
    tcp_state = TCP_CLOSED; tcp_fin = tcp_rst = tcp_overflow = 0; tcp_blen = 0;
    const u8 *m = net_mac_for(ip);
    if (!m) return -2;
    for (int i = 0; i < 6; i++) tcp_dmac[i] = m[i];
    tcp_rip = ip; tcp_rport = port;
    if (++tcp_lport > 60000) tcp_lport = 40000;
    tcp_iss = ms_now() * 64000u + 12345u + ip;
    tcp_snd_una = tcp_snd_nxt = tcp_iss; tcp_rcv_nxt = 0;
    tcp_state = TCP_SYN;
    for (int t = 0; t < 4; t++) {
        tcp_emit(0x02, 0, 0);
        u32 t0 = ms_now();
        while (ms_now() - t0 < 1500) {
            net_poll();
            if (tcp_state == TCP_ESTAB) return 0;
            if (tcp_rst) { tcp_state = TCP_CLOSED; return -1; }
        }
    }
    tcp_state = TCP_CLOSED;
    return -1;
}
static void tcp_in(u32 src, u32 dst, const u8 *seg, int len) {
    if (len < 20 || tcp_state == TCP_CLOSED) return;
    if (src != tcp_rip || rd16(seg) != tcp_rport || rd16(seg + 2) != tcp_lport) return;
    if (tcp_sum(src, dst, seg, len) != 0) return;
    u32 seq = rd32(seg + 4), ack = rd32(seg + 8);
    int hl = (seg[12] >> 4) * 4;
    u8 fl = seg[13];
    if (hl < 20 || hl > len) return;
    const u8 *d = seg + hl;
    int dl = len - hl;
    if (fl & 0x04) { tcp_rst = 1; tcp_state = TCP_CLOSED; return; }
    if (tcp_state == TCP_SYN) {
        if ((fl & 0x12) == 0x12 && ack == tcp_snd_nxt + 1) {
            tcp_rcv_nxt = seq + 1; tcp_snd_nxt++; tcp_snd_una = tcp_snd_nxt;
            tcp_state = TCP_ESTAB;
            tcp_emit(0x10, 0, 0);
        }
        return;
    }
    if (fl & 0x10) {
        if ((int)(ack - tcp_snd_una) > 0 && (int)(ack - tcp_snd_nxt) <= 0) tcp_snd_una = ack;
    }
    if (dl > 0 || (fl & 0x01)) {
        if (seq == tcp_rcv_nxt) {
            if (dl > 0) {
                int room = tcp_bmax - tcp_blen, n = dl < room ? dl : room;
                for (int i = 0; i < n; i++) tcp_buf[tcp_blen + i] = d[i];
                tcp_blen += n;
                if (n < dl) tcp_overflow = 1;
                tcp_rcv_nxt += dl;
            }
            if (fl & 0x01) { tcp_rcv_nxt++; tcp_fin = 1; }
        }
        tcp_emit(0x10, 0, 0);                 /* ACK; при потере/дубле - с ожидаемым номером */
    }
}
static int tcp_send(const u8 *data, int len) {
    while (len > 0) {
        int n = len > 1400 ? 1400 : len, ok = 0;
        u32 seq0 = tcp_snd_nxt;
        for (int t = 0; t < 5 && !ok; t++) {
            tcp_snd_nxt = seq0; tcp_emit(0x18, data, n); tcp_snd_nxt = seq0 + n;
            u32 t0 = ms_now();
            while (ms_now() - t0 < 1000) {
                net_poll();
                if (tcp_rst || tcp_state == TCP_CLOSED) return -1;
                if ((int)(tcp_snd_una - tcp_snd_nxt) >= 0) { ok = 1; break; }
            }
        }
        if (!ok) return -1;
        data += n; len -= n;
    }
    return 0;
}
static void tcp_close(void) {
    if (tcp_state == TCP_ESTAB) {
        tcp_state = TCP_FINW;
        tcp_emit(0x11, 0, 0); tcp_snd_nxt++;
        u32 t0 = ms_now();
        while (ms_now() - t0 < 800) { net_poll(); if ((int)(tcp_snd_una - tcp_snd_nxt) >= 0) break; }
    }
    tcp_state = TCP_CLOSED;
}

/* ---- DNS-клиент: UDP, несколько серверов, запасной TCP, коды ответа ---- */
static int dns_why;                /* 0 ok, 1 DNS не задан, 2 сервер недостижим (ARP), 3 нет ответа, 4 имени нет, 5 нет IPv4-адреса, 6 ошибка сервера */
static u32 dns_used;               /* какой сервер опрашивался последним */
static int dns_rcode = -1, dns_an, dns_rlen;   /* что было в последнем ответе */
static u32 dns_rsrc;
static const char *dns_rcode_name(int r) {
    switch (r) { case 0: return "NOERROR"; case 1: return "FORMERR"; case 2: return "SERVFAIL"; case 3: return "NXDOMAIN"; case 4: return "NOTIMP"; case 5: return "REFUSED"; }
    return "?";
}
static void dns_parse(const u8 *d, int len) {            /* dns_done: 1 - адрес найден, 2 - ошибка/нет адреса */
    if (len < 12 || rd16(d) != dns_id) return;
    u16 flags = rd16(d + 2);
    if (!(flags & 0x8000)) return;
    st_dns++;
    dns_rcode = flags & 15; dns_rlen = len; dns_an = rd16(d + 6);
    if (flags & 15) { dns_done = 2; return; }
    int qd = rd16(d + 4), an = rd16(d + 6), i = 12;
    for (int q = 0; q < qd; q++) { i = dns_skip(d, len, i); if (i < 0) return; i += 4; }
    for (int a = 0; a < an; a++) {
        i = dns_skip(d, len, i);
        if (i < 0 || i + 10 > len) return;
        int type = rd16(d + i), rdl = rd16(d + i + 8);
        i += 10;
        if (i + rdl > len) return;
        if (type == 1 && rdl == 4) { dns_ip = rd32(d + i); dns_done = 1; return; }
        i += rdl;
    }
    dns_done = 2;
}
static void dns_in(const u8 *d, int len) { dns_parse(d, len); }
static int dns_build(const char *name, u8 *q) {           /* возвращает длину запроса или 0 */
    for (int i = 0; i < 12; i++) q[i] = 0;
    be16(q, ++dns_id); be16(q + 2, 0x0100); be16(q + 4, 1);
    int i = 12;
    const char *s = name;
    while (*s) {
        int ln = 0; const char *e = s;
        while (*e && *e != '.') { e++; ln++; }
        if (ln == 0 || ln > 63 || i + ln + 1 > 270) return 0;
        q[i++] = ln;
        for (int k = 0; k < ln; k++) q[i++] = s[k];
        s = e;
        if (*s == '.') s++;
    }
    q[i++] = 0; be16(q + i, 1); be16(q + i + 2, 1);
    return i + 4;
}
static u8 dns_tcpbuf[1400];
static int dns_tcp(u32 srv, const u8 *q, int qlen) {      /* тот же запрос по TCP/53; 1 если получен ответ */
    u8 *sbuf = tcp_buf; int smax = tcp_bmax;
    int prev = dns_done, prc = dns_rcode, got = 0;
    dns_done = 0;
    tcp_buf = dns_tcpbuf; tcp_bmax = sizeof dns_tcpbuf;
    if (tcp_connect(srv, 53) == 0) {
        u8 m[2 + 300];
        be16(m, qlen);
        for (int i = 0; i < qlen; i++) m[2 + i] = q[i];
        if (tcp_send(m, qlen + 2) == 0) {
            u32 t0 = ms_now();
            while (ms_now() - t0 < 2500) {
                net_poll();
                if (tcp_blen >= 2) { int rl = rd16(tcp_buf); if (tcp_blen >= 2 + rl) { dns_parse(tcp_buf + 2, rl); break; } }
                if (tcp_fin || tcp_rst || tcp_state == TCP_CLOSED) break;
            }
        }
        tcp_close();
    }
    got = dns_done != 0;
    tcp_buf = sbuf; tcp_bmax = smax;
    if (!got) { dns_done = prev; dns_rcode = prc; }
    return got;
}
/* один пробный UDP-запрос к серверу (для netdiag): >=0 время ответа в мс, -1 нет ответа, -2 ARP не удался, -3 плохое имя */
static int dns_probe(u32 srv, const char *name) {
    u8 q[300];
    int ql = dns_build(name, q);
    if (!ql) return -3;
    const u8 *m = net_mac_for(srv);
    if (!m) return -2;
    dns_done = 0; dns_rcode = -1; dns_an = 0; dns_rlen = 0;
    u32 t0 = ms_now();
    udp_send(m, nip, srv, dns_port, 53, q, ql);
    while (ms_now() - t0 < 1500) { net_poll(); if (dns_done) return (int)(ms_now() - t0); }
    return -1;
}
static u32 net_dns(const char *name) {
    u8 q[300];
    int ql = dns_build(name, q);
    dns_why = 1; dns_used = 0; dns_rcode = -1;
    if (!ql) { dns_why = 4; return 0; }
    u32 cand[3] = { ndns, ngw, ((nip >> 8) == 0x0A0002u) ? 0x0A000203u : 0 }, servers[3];
    int ns = 0;
    for (int c = 0; c < 3; c++) {
        int dup = 0;
        for (int k = 0; k < ns; k++) if (servers[k] == cand[c]) dup = 1;
        if (cand[c] && !dup) servers[ns++] = cand[c];
    }
    int nx = 0, err = 0, nodata = 0, silent = 0, noarp = 0;
    for (int si = 0; si < ns; si++) {
        u32 srv = servers[si];
        dns_used = srv;
        const u8 *m = net_mac_for(srv);
        if (!m) { noarp = 1; continue; }
        dns_done = 0; dns_ip = 0;
        for (int t = 0; t < (si == 0 ? 2 : 1) && !dns_done; t++) {
            udp_send(m, nip, srv, dns_port, 53, q, ql);
            u32 t0 = ms_now();
            while (ms_now() - t0 < 1500) { net_poll(); if (dns_done) break; }
        }
        if (dns_done != 1 && si == 0) dns_tcp(srv, q, ql);        /* UDP молчит или ошибка: пробуем TCP */
        if (dns_done == 1) { dns_why = 0; return dns_ip; }
        if (dns_done == 2) { if (dns_rcode == 3) nx = 1; else if (dns_rcode > 0) err = 1; else nodata = 1; }
        else silent = 1;
    }
    dns_why = nodata ? 5 : nx ? 4 : err ? 6 : silent ? 3 : noarp ? 2 : 1;
    return 0;
}

/* ---- приём ---- */
static void udp_in(u32 src, u16 sp, u16 dp, const u8 *d, int len) {
    st_udp++;
    if (sp == 67 && dp == 68) dhcp_in(d, len);
    else if (sp == 53 && dp == dns_port) { dns_rsrc = src; dns_in(d, len); }
}
static void ip_in(const u8 *smac, const u8 *p, int len) {
    if (len < 20 || (p[0] >> 4) != 4) return;
    int ihl = (p[0] & 15) * 4, tl = rd16(p + 2);
    if (ihl < 20 || len < ihl || tl > len || tl < ihl) return;
    u32 src = rd32(p + 12), dst = rd32(p + 16);
    u8 proto = p[9];
    if (!(dst == nip || dst == 0xFFFFFFFFu || nip == 0)) return;
    const u8 *q = p + ihl;
    int ql = tl - ihl;
    if (proto == 1 && ql >= 8) {
        if (q[0] == 8 && dst == nip && ql <= 1480) {
            for (int i = 0; i < ql; i++) icb[i] = q[i];
            icb[0] = 0; be16(icb + 2, 0);
            be16(icb + 2, net_cksum(icb, ql));
            ip_send_to(smac, nip, src, 1, icb, ql);
        } else if (q[0] == 0 && rd16(q + 4) == ping_id) {
            ping_seq_rx = rd16(q + 6); ping_ttl = p[8]; ping_rx = 1;
        }
    } else if (proto == 17 && ql >= 8) {
        int ul = rd16(q + 4);
        if (ul < 8 || ul > ql) return;
        udp_in(src, rd16(q), rd16(q + 2), q + 8, ul - 8);
    } else if (proto == 6) {
        tcp_in(src, dst, q, ql);
    }
}
static void net_poll(void) {
    int n = nic_rx(rxb, sizeof rxb);
    if (n < 14) return;
    st_rx++;
    u16 type = rd16(rxb + 12);
    const u8 *p = rxb + 14;
    int len = n - 14;
    if (type == ETH_ARP && len >= 28) {
        st_arp++;
        if (rd16(p) == 1 && rd16(p + 2) == 0x0800 && p[4] == 6 && p[5] == 4) {
            u16 op = rd16(p + 6);
            u32 sip = rd32(p + 14), tip = rd32(p + 24);
            if (op == 2 || (op == 1 && nip && tip == nip)) arp_add(sip, p + 8);
            if (op == 1 && nip && tip == nip) arp_send(2, p + 8, sip, p + 8);
        }
    } else if (type == ETH_IP) {
        st_ip++;
        ip_in(rxb + 6, p, len);
    }
}

static int net_parse_ip(const char *s, u32 *out) {
    u32 v = 0;
    for (int part = 0; part < 4; part++) {
        if (*s < '0' || *s > '9') return 0;
        u32 n = 0;
        while (*s >= '0' && *s <= '9') { n = n * 10 + (*s++ - '0'); if (n > 255) return 0; }
        v = (v << 8) | n;
        if (part < 3) { if (*s != '.') return 0; s++; }
    }
    if (*s) return 0;
    *out = v;
    return 1;
}
#endif
