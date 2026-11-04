#include <stdio.h>
#include <string.h>
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
#include "../net.h"
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while(0)

static u8 gwmac[6] = {0x52,0x54,0x00,0x12,0x35,0x02};
static u8 q[32][1600]; static int ql[32], qh, qt;
static int arp_req_seen, icmp_req_seen, bad_cksum, drop_icmp;
static u8 last_tx[1600]; static int last_n;
static const u32 GW = 0x0A000202, DNS = 0x0A000203, YOU = 0x0A00020F;

static void push(const u8 *f, int n) { memcpy(q[qt], f, n); ql[qt] = n; qt = (qt + 1) % 32; }
static int rx(u8 *b, int max) { (void)max; if (qh == qt) return 0; int n = ql[qh]; memcpy(b, q[qh], n); qh = (qh + 1) % 32; return n; }
static u32 clk; static u32 msn(void) { return ++clk; }

static int mk_ip(u8 *f, const u8 *dm, const u8 *sm, u32 s, u32 d, u8 proto, const u8 *pl, int len) {
    memcpy(f, dm, 6); memcpy(f + 6, sm, 6); be16(f + 12, 0x0800);
    u8 *p = f + 14; p[0] = 0x45; p[1] = 0; be16(p + 2, 20 + len); be16(p + 4, 1); be16(p + 6, 0);
    p[8] = 64; p[9] = proto; be16(p + 10, 0); be32(p + 12, s); be32(p + 16, d);
    be16(p + 10, net_cksum(p, 20)); memcpy(p + 20, pl, len);
    return 14 + 20 + len;
}
static int mk_udp(u8 *f, const u8 *dm, u32 s, u32 d, u16 sp, u16 dp, const u8 *pl, int len) {
    u8 u[1500]; be16(u, sp); be16(u + 2, dp); be16(u + 4, 8 + len); be16(u + 6, 0); memcpy(u + 8, pl, len);
    return mk_ip(f, dm, gwmac, s, d, 17, u, 8 + len);
}
static void dhcp_reply(const u8 *req, int type) {
    u8 r[300]; memset(r, 0, sizeof r);
    r[0] = 2; r[1] = 1; r[2] = 6; memcpy(r + 4, req + 4, 4); be32(r + 16, YOU); memcpy(r + 28, req + 28, 6);
    be32(r + 236, 0x63825363u);
    int i = 240; r[i++] = 53; r[i++] = 1; r[i++] = type;
    r[i++] = 1; r[i++] = 4; be32(r + i, 0xFFFFFF00u); i += 4;
    r[i++] = 3; r[i++] = 4; be32(r + i, GW); i += 4;
    r[i++] = 6; r[i++] = 4; be32(r + i, DNS); i += 4;
    r[i++] = 54; r[i++] = 4; be32(r + i, GW); i += 4; r[i++] = 255;
    u8 f[1600]; int n = mk_udp(f, bcast, GW, 0xFFFFFFFFu, 67, 68, r, 300); push(f, n);
}
static int tx(const u8 *f, int n) {
    memcpy(last_tx, f, n); last_n = n;
    u16 type = rd16(f + 12); const u8 *p = f + 14;
    if (type == 0x0806) {
        if (rd16(p + 6) == 1 && (rd32(p + 24) == GW || rd32(p + 24) == DNS)) {
            arp_req_seen++;
            u8 a[60]; memset(a, 0, 60); memcpy(a, f + 6, 6); memcpy(a + 6, gwmac, 6); be16(a + 12, 0x0806);
            u8 *r = a + 14; be16(r, 1); be16(r + 2, 0x0800); r[4] = 6; r[5] = 4; be16(r + 6, 2);
            memcpy(r + 8, gwmac, 6); be32(r + 14, rd32(p + 24)); memcpy(r + 18, f + 6, 6); be32(r + 24, rd32(p + 14));
            push(a, 60);
        }
        return 0;
    }
    if (type != 0x0800) return 0;
    if (net_cksum(p, 20) != 0) bad_cksum++;
    u32 s = rd32(p + 12), d = rd32(p + 16); u8 proto = p[9]; const u8 *u = p + 20; int ul = rd16(p + 2) - 20;
    if (proto == 1 && u[0] == 8) {
        icmp_req_seen++;
        if (drop_icmp) return 0;
        if (net_cksum(u, ul) != 0) bad_cksum++;
        if (memcmp(f, gwmac, 6) != 0) bad_cksum += 100;     /* внешние адреса - через шлюз */
        u8 ic[100]; memcpy(ic, u, ul); ic[0] = 0; be16(ic + 2, 0); be16(ic + 2, net_cksum(ic, ul));
        u8 fr[1600]; int n2 = mk_ip(fr, f + 6, gwmac, d, s, 1, ic, ul); push(fr, n2);
    } else if (proto == 17 && rd16(u + 2) == 67) {
        const u8 *b = u + 8; int t = 0;
        for (int i = 240; b[i] != 255; i += 2 + b[i + 1]) if (b[i] == 53) t = b[i + 2];
        if (t == 1) dhcp_reply(b, 2); else if (t == 3) dhcp_reply(b, 5);
    } else if (proto == 17 && rd16(u + 2) == 53) {
        const u8 *b = u + 8; int bl = rd16(u + 4) - 8;
        u8 r[300]; memset(r, 0, sizeof r); memcpy(r, b, 2); be16(r + 2, 0x8180); be16(r + 4, 1); be16(r + 6, 1);
        int qn = bl - 12; memcpy(r + 12, b + 12, qn); int i = 12 + qn;
        r[i++] = 0xC0; r[i++] = 0x0C; be16(r + i, 1); be16(r + i + 2, 1); be32(r + i + 4, 300); be16(r + i + 8, 4);
        be32(r + i + 10, 0x5DB8D822u); i += 14;       /* 93.184.216.34 */
        u8 fr[1600]; int n2 = mk_udp(fr, f + 6, DNS, s, 53, rd16(u), r, i); push(fr, n2);
    }
    return 0;
}
int main(void) {
    u8 mac[6] = {0x08,0x00,0x27,0xAA,0xBB,0xCC}; memcpy(nmac, mac, 6);
    nic_tx = tx; nic_rx = rx; ms_now = msn;
    /* контрольная сумма: известный пример заголовка IPv4 -> 0xb861 */
    u8 hdr[20] = {0x45,0,0,0x73,0,0,0x40,0,0x40,0x11,0,0,0xc0,0xa8,0,1,0xc0,0xa8,0,0xc7};
    CHECK(net_cksum(hdr, 20) == 0xb861);
    u32 ip; CHECK(net_parse_ip("10.0.2.15", &ip) && ip == YOU);
    CHECK(!net_parse_ip("10.0.2", &ip) && !net_parse_ip("10.0.2.256", &ip) && !net_parse_ip("a.b.c.d", &ip) && !net_parse_ip("1.2.3.4x", &ip));
    /* DHCP */
    CHECK(net_dhcp() == 0);
    CHECK(nip == YOU && nmask == 0xFFFFFF00u && ngw == GW && ndns == DNS);
    /* ping шлюза (ARP + ICMP) и внешнего адреса через шлюз */
    u32 rtt;
    CHECK(net_ping(GW, 1, 2000, &rtt) == 0 && arp_req_seen >= 1);
    CHECK(net_ping(0x08080808u, 2, 2000, &rtt) == 0 && ping_ttl == 64);
    CHECK(icmp_req_seen == 2 && bad_cksum == 0);
    /* таймаут: шлюз не отвечает на ICMP */
    drop_icmp = 1;
    CHECK(net_ping(GW, 3, 1000, &rtt) == -1);
    drop_icmp = 0;
    /* DNS (ответ со сжатым именем) */
    CHECK(net_dns("example.com") == 0x5DB8D822u);
    CHECK(net_dns("a..b") == 0);                       /* пустая метка */
    /* нас спрашивают по ARP: должны ответить */
    u8 f[1600]; memset(f, 0, sizeof f);
    memcpy(f, bcast, 6); memcpy(f + 6, gwmac, 6); be16(f + 12, 0x0806);
    u8 *a = f + 14; be16(a, 1); be16(a + 2, 0x0800); a[4] = 6; a[5] = 4; be16(a + 6, 1);
    memcpy(a + 8, gwmac, 6); be32(a + 14, GW); be32(a + 24, YOU);
    push(f, 60); last_n = 0; net_poll();
    CHECK(last_n >= 60 && rd16(last_tx + 12) == 0x0806 && rd16(last_tx + 14 + 6) == 2 && memcmp(last_tx + 14 + 8, nmac, 6) == 0);
    CHECK(memcmp(last_tx, gwmac, 6) == 0);
    /* нас пингуют: должны ответить корректным echo reply */
    u8 ic[40]; ic[0] = 8; ic[1] = 0; be16(ic + 2, 0); be16(ic + 4, 0x7777); be16(ic + 6, 9);
    for (int i = 0; i < 32; i++) ic[8 + i] = i;
    be16(ic + 2, net_cksum(ic, 40));
    int n = mk_ip(f, nmac, gwmac, GW, YOU, 1, ic, 40); push(f, n); last_n = 0; net_poll();
    CHECK(last_n > 0 && last_tx[14 + 20] == 0 && net_cksum(last_tx + 14 + 20, 40) == 0 && net_cksum(last_tx + 14, 20) == 0);
    CHECK(rd32(last_tx + 14 + 16) == GW && rd16(last_tx + 14 + 20 + 4) == 0x7777 && rd16(last_tx + 14 + 20 + 6) == 9);
    /* мусор и чужие пакеты не должны ломать приём */
    u8 junk[20] = {1, 2, 3}; push(junk, 20); net_poll();
    n = mk_ip(f, nmac, gwmac, GW, 0x0A0002FFu, 1, ic, 40); push(f, n); last_n = 0; net_poll(); CHECK(last_n == 0);
    printf("net tests OK (dhcp, arp, ping, dns, replies, timeout)\n");
    return 0;
}
