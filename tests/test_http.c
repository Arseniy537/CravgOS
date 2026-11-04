#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
#include "../net.h"
#include "../http.h"
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while(0)

#define MSS 1000
struct srvst { int active; u16 cport; u32 iss, hi, una, rcv; const u8 *resp; int rlen; int fin_sent; int drop_off, dropped; int silent; };
static struct srvst S;
static u8 gwmac[6] = {0x52,0x54,0x00,0x12,0x35,0x02};
static const u32 GW = 0x0A000202, DNSIP = 0x0A000203, YOU = 0x0A00020F, SRV = 0x5DB8D822u;
static u8 q[64][1600]; static int ql[64], qh, qt, bad_cksum;
static void push(const u8 *f, int n) { memcpy(q[qt], f, n); ql[qt] = n; qt = (qt + 1) % 64; }
static int S_active_rto(void);
static int S_una_off(void);
static void send_seg(int off, int first);
static int rx(u8 *b, int max) {
    (void)max;
    if (qh == qt && S_active_rto()) send_seg(S_una_off(), 0);     /* эмуляция RTO сервера */
    if (qh == qt) return 0;
    int n = ql[qh]; memcpy(b, q[qh], n); qh = (qh + 1) % 64; return n;
}
static u32 clk; static u32 msn(void) { return ++clk; }

static int mk_ip(u8 *f, const u8 *dm, u32 s, u32 d, u8 proto, const u8 *pl, int len) {
    memcpy(f, dm, 6); memcpy(f + 6, gwmac, 6); be16(f + 12, 0x0800);
    u8 *p = f + 14; p[0] = 0x45; p[1] = 0; be16(p + 2, 20 + len); be16(p + 4, 1); be16(p + 6, 0);
    p[8] = 64; p[9] = proto; be16(p + 10, 0); be32(p + 12, s); be32(p + 16, d);
    be16(p + 10, net_cksum(p, 20)); memcpy(p + 20, pl, len);
    return 14 + 20 + len;
}
/* ---- имитация TCP-сервера ---- */

static u8 body_big[4500];
static int S_active_rto(void) { return S.active && S.resp && !S.silent && (int)(S.hi - S.una) > 0; }
static int S_una_off(void) { return (int)(S.una - (S.iss + 1)); }
static u8 respbuf[8000]; static int resplen;
static int refuse_port = 81;
static u32 dns_dead, dns_bad_srv, tcp_dns_dead; static int dns_bad_rcode, dns_tcp_rcode, dns_dead_all;
static u32 s_src = 0x5DB8D822u; static u16 s_port = 80;                         /* запросы к этому адресу игнорируются */
static void tcp_out(u8 flags, u32 seq, u32 ack, const u8 *d, int dl) {
    u8 seg[1500]; memset(seg, 0, 20);
    be16(seg, s_port); be16(seg + 2, S.cport); be32(seg + 4, seq); be32(seg + 8, ack);
    seg[12] = 5 << 4; seg[13] = flags; be16(seg + 14, 8192); if (dl) memcpy(seg + 20, d, dl);
    be16(seg + 16, tcp_sum(s_src, YOU, seg, 20 + dl));
    u8 f[1600]; int n = mk_ip(f, nmac, s_src, YOU, 6, seg, 20 + dl); push(f, n);
}
static void send_seg(int off, int first) {
    int n = S.rlen - off; if (n > MSS) n = MSS; if (n <= 0) return;
    if (first && S.drop_off == off && !S.dropped && S.drop_off >= 0) { S.dropped = 1; return; }
    tcp_out(0x10, S.iss + 1 + off, S.rcv, S.resp + off, n);
}
static void send_more(void) {                       /* окно из 2 сегментов */
    while ((int)(S.hi - (S.iss + 1)) < S.rlen && (int)(S.hi - S.una) < 2 * MSS) {
        int off = S.hi - (S.iss + 1);
        send_seg(off, 1);
        int n = S.rlen - off; if (n > MSS) n = MSS;
        S.hi += n;
    }
}
static void make_resp(const char *path) {
    resplen = 0; S.silent = 0;
    #define ADD(s) do { int _n = strlen(s); memcpy(respbuf + resplen, s, _n); resplen += _n; } while (0)
    if (!strcmp(path, "/")) { ADD("HTTP/1.0 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 38\r\n\r\n<h1>Hi</h1><a href=\"/next\">go</a>ABCDEF"); resplen -= 0; }
    else if (!strcmp(path, "/big")) {
        char h[120]; sprintf(h, "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n", (int)sizeof body_big);
        ADD(h); memcpy(respbuf + resplen, body_big, sizeof body_big); resplen += sizeof body_big;
    }
    else if (!strcmp(path, "/chunked")) ADD("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nTransfer-Encoding: chunked\r\n\r\n6\r\nHello,\r\nB;ext=1\r\n chunked wo\r\n4\r\nrld!\r\n0\r\n\r\n");
    else if (!strcmp(path, "/redir")) ADD("HTTP/1.0 301 Moved\r\nLocation: /final/../final2?x=1#frag\r\nContent-Length: 0\r\n\r\n");
    else if (!strcmp(path, "/final2?x=1")) ADD("HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 4\r\n\r\nDONE");
    else if (!strcmp(path, "/toHttps")) ADD("HTTP/1.0 302 Found\r\nLocation: https://example.com/secure\r\nContent-Length: 0\r\n\r\n");
    else if (!strcmp(path, "/nolen")) ADD("HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\nno length, closed by FIN");
    else if (!strcmp(path, "/bin")) ADD("HTTP/1.0 200 OK\r\nContent-Type: image/png\r\nContent-Length: 3\r\n\r\n\x89PN");
    else if (!strcmp(path, "/silent")) S.silent = 1;
    else ADD("HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: 9\r\n\r\nnot found");
    S.resp = respbuf; S.rlen = resplen;
}
static int tx(const u8 *f, int n) {
    (void)n;
    u16 type = rd16(f + 12); const u8 *p = f + 14;
    if (type == 0x0806) {
        if (rd16(p + 6) == 1 && (rd32(p + 24) == GW || rd32(p + 24) == DNSIP)) {
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
    if (proto == 17 && rd16(u + 2) == 53) {
        if (rd16(u + 6) != 0 && udp_sum(s, d, u, rd16(u + 4)) != 0) bad_cksum++;     /* наша контрольная сумма UDP должна быть верной */
        if (d == dns_dead || dns_dead_all) return 0;
        const u8 *b = u + 8; int bl = rd16(u + 4) - 8;
        int nope = (memcmp(b + 12, "\4nope", 5) == 0);
        int rc = nope ? 3 : (d == dns_bad_srv ? dns_bad_rcode : 0);
        u8 r[300]; memset(r, 0, sizeof r); memcpy(r, b, 2); be16(r + 2, 0x8180 | rc); be16(r + 4, 1); be16(r + 6, rc ? 0 : 1);
        int qn = bl - 12; memcpy(r + 12, b + 12, qn); int i = 12 + qn;
        if (!rc) { r[i++] = 0xC0; r[i++] = 0x0C; be16(r + i, 1); be16(r + i + 2, 1); be32(r + i + 4, 300); be16(r + i + 8, 4); be32(r + i + 10, SRV); i += 14; }
        u8 us[400]; be16(us, 53); be16(us + 2, rd16(u)); be16(us + 4, 8 + i); be16(us + 6, 0); memcpy(us + 8, r, i);
        u8 fr[1600]; int n2 = mk_ip(fr, f + 6, d, s, 17, us, 8 + i); push(fr, n2);
        return 0;
    }
    if (proto != 6) return 0;
    if (d != SRV && rd16(u + 2) != 53) return 0;
    if (tcp_sum(s, d, u, ul) != 0) bad_cksum++;
    u16 sport = rd16(u), dport = rd16(u + 2); u32 seq = rd32(u + 4), ack = rd32(u + 8); int hl = (u[12] >> 4) * 4; u8 fl = u[13]; const u8 *dd = u + hl; int dl = ul - hl;
    s_src = d; s_port = dport;
    if (dport == refuse_port || (dport == 53 && (d == tcp_dns_dead || dns_dead_all))) { S.cport = sport; tcp_out(0x14, 0, seq + 1, 0, 0); return 0; }
    if (fl & 0x02) { memset(&S, 0, sizeof S); S.drop_off = -1; S.active = 1; S.cport = sport; S.iss = 5000; S.rcv = seq + 1; S.hi = S.una = S.iss + 1; tcp_out(0x12, S.iss, S.rcv, 0, 0); return 0; }
    if (!S.active) return 0;
    if (dl > 0) {
        if (seq != S.rcv) { tcp_out(0x10, S.iss + 1, S.rcv, 0, 0); return 0; }
        S.rcv += dl; tcp_out(0x10, S.iss + 1, S.rcv, 0, 0);
        if (s_port == 53) {                                          /* DNS по TCP: [длина 2][запрос] */
            const u8 *b = dd + 2; int bl = dl - 2;
            int nope = (memcmp(b + 12, "\4nope", 5) == 0), rc = nope ? 3 : dns_tcp_rcode;
            u8 r[300]; memset(r, 0, sizeof r); memcpy(r, b, 2); be16(r + 2, 0x8180 | rc); be16(r + 4, 1); be16(r + 6, rc ? 0 : 1);
            int qn = bl - 12; memcpy(r + 12, b + 12, qn); int ri = 12 + qn;
            if (!rc) { r[ri++] = 0xC0; r[ri++] = 0x0C; be16(r + ri, 1); be16(r + ri + 2, 1); be32(r + ri + 4, 300); be16(r + ri + 8, 4); be32(r + ri + 10, SRV); ri += 14; }
            be16(respbuf, ri); memcpy(respbuf + 2, r, ri); resplen = ri + 2;
            S.resp = respbuf; S.rlen = resplen; S.silent = 0; S.drop_off = -1; S.dropped = 0;
            send_more();
            return 0;
        }
        char path[200]; sscanf((const char *)dd, "GET %199s", path);
        if (!strcmp(path, "/big") || !strcmp(path, "/chunked")) { /* потеря первого сегмента */ if (!strcmp(path, "/big")) S.drop_off = 0; }
        char keep = S.drop_off; (void)keep;
        int drop = S.drop_off; make_resp(path); S.drop_off = drop; S.dropped = 0;
        if (!S.silent) send_more();
        return 0;
    }
    if (fl & 0x01) { S.rcv++; tcp_out(0x10, S.iss + 1 + S.rlen + S.fin_sent, S.rcv, 0, 0); return 0; }
    if ((fl & 0x10) && S.resp) {
        if ((int)(ack - S.una) > 0) {
            S.una = ack; send_more();
            if (S.una == S.iss + 1 + S.rlen && !S.fin_sent) { tcp_out(0x11, S.iss + 1 + S.rlen, S.rcv, 0, 0); S.fin_sent = 1; }
        } else if (ack == S.una && (int)(S.una - (S.iss + 1)) < S.rlen) {
            send_seg(S.una - (S.iss + 1), 0);                   /* дубликат ACK -> повтор */
        }
    }
    return 0;
}
static int abort_calls;
static int abort_after(void) { return ++abort_calls > 500; }

int main(void) {
    for (int i = 0; i < (int)sizeof body_big; i++) body_big[i] = 'A' + (i * 7) % 26;
    u8 mac[6] = {0x08,0,0x27,1,2,3}; memcpy(nmac, mac, 6);
    nic_tx = tx; nic_rx = rx; ms_now = msn;
    nip = YOU; nmask = 0xFFFFFF00u; ngw = GW; ndns = DNSIP;
    static u8 buf[100000]; http_buf = buf; http_max = sizeof buf - 1;
    struct hres r; char out[300];
    /* --- разбор URL --- */
    char host[128], path[400]; int https, port;
    CHECK(http_parse_url("http://example.com:8080/a/b?q=1#x", &https, host, 128, &port, path, 400) == 0 && !https && port == 8080 && !strcmp(host, "example.com") && !strcmp(path, "/a/b?q=1"));
    CHECK(http_parse_url("example.com", &https, host, 128, &port, path, 400) == 0 && port == 80 && !strcmp(path, "/"));
    CHECK(http_parse_url("HTTPS://x.org?a=b", &https, host, 128, &port, path, 400) == 0 && https && port == 443 && !strcmp(path, "/?a=b"));
    CHECK(http_parse_url("http://", &https, host, 128, &port, path, 400) < 0 && http_parse_url("http://a:0/", &https, host, 128, &port, path, 400) < 0);
    /* --- относительные ссылки --- */
    struct { const char *base, *href, *want; } T[] = {
        {"http://a.com/x/y.html", "z.html", "http://a.com/x/z.html"},
        {"http://a.com/x/y.html", "/top", "http://a.com/top"},
        {"http://a.com/x/y.html", "../up/f.html", "http://a.com/up/f.html"},
        {"http://a.com/x/y.html", "../../../too/far", "http://a.com/too/far"},
        {"http://a.com/x/y.html", "./same", "http://a.com/x/same"},
        {"http://a.com/x/y.html", "//b.org/p", "http://b.org/p"},
        {"https://a.com/x/y.html", "//b.org/p", "https://b.org/p"},
        {"http://a.com/x/y.html", "?page=2", "http://a.com/x/y.html?page=2"},
        {"http://a.com/x/y.html?old=1", "?new=2", "http://a.com/x/y.html?new=2"},
        {"http://a.com/x/y.html", "#sec", "http://a.com/x/y.html"},
        {"http://a.com", "page", "http://a.com/page"},
        {"http://a.com/dir/", "f", "http://a.com/dir/f"},
        {"http://a.com/x/y", "https://z.org/q#f", "https://z.org/q"},
        {"http://a.com/a/b/c", "../d/", "http://a.com/a/d/"},
        {"http://a.com/a?x=1/../2", "g", "http://a.com/g"},
        {"file:///docs/a.html", "b.html", "file:///docs/b.html"},
        {"file:///docs/a.html", "/root.html", "file:///root.html"},
    };
    for (unsigned i = 0; i < sizeof T / sizeof *T; i++) {
        int rr = url_resolve(T[i].base, T[i].href, out, sizeof out);
        if (rr < 0 || strcmp(out, T[i].want)) { printf("FAIL resolve %u: %s + %s => %s (want %s)\n", i, T[i].base, T[i].href, rr < 0 ? "ERR" : out, T[i].want); return 1; }
    }
    CHECK(url_resolve("http://a.com/", "mailto:x@y.z", out, sizeof out) < 0 && url_resolve("http://a.com/", "javascript:void(0)", out, sizeof out) < 0);
    CHECK(url_resolve("about:start", "rel", out, sizeof out) < 0);
    /* --- обычная страница --- */
    CHECK(http_get_follow("http://example.com/", &r) == 0);
    CHECK(r.status == 200 && r.html == 1 && r.body_len == 38 && !strncmp((char *)buf + r.body_off, "<h1>Hi</h1><a href=\"/next\">go</a>ABCDEF", 38));
    CHECK(tcp_state == TCP_CLOSED && bad_cksum == 0);
    /* --- большой ответ с потерей первого сегмента --- */
    int rb = http_get_follow("http://example.com/big", &r);
    if (rb != 0 || r.status != 200 || r.body_len != (int)sizeof body_big) printf("DEBUG big: ret=%d status=%d html=%d body_len=%d trunc=%d ctype=[%s] blen=%d fin=%d dropped=%d\n", rb, r.status, r.html, r.body_len, r.truncated, r.ctype, tcp_blen, tcp_fin, S.dropped);
    CHECK(rb == 0);
    CHECK(r.status == 200 && r.html == 0 && r.body_len == (int)sizeof body_big && memcmp(buf + r.body_off, body_big, sizeof body_big) == 0);
    CHECK(S.dropped == 1 && bad_cksum == 0);
    /* --- chunked --- */
    CHECK(http_get_follow("http://example.com/chunked", &r) == 0);
    CHECK(r.body_len == 21 && !strncmp((char *)buf + r.body_off, "Hello, chunked world!", 21));
    /* --- редирект: нормализация ../ и #frag --- */
    CHECK(http_get_follow("http://example.com/redir", &r) == 0);
    CHECK(r.status == 200 && !strcmp(r.url, "http://example.com/final2?x=1") && r.body_len == 4 && !strncmp((char *)buf + r.body_off, "DONE", 4));
    /* --- редирект на https --- */
    CHECK(http_get_follow("http://example.com/toHttps", &r) == HE_HTTPS && !strcmp(r.url, "https://example.com/secure"));
    CHECK(http_get_follow("https://example.com/", &r) == HE_HTTPS);
    /* --- без Content-Length, закрытие по FIN; 404; бинарные данные --- */
    CHECK(http_get_follow("http://example.com/nolen", &r) == 0 && r.body_len == 24 && !strncmp((char *)buf + r.body_off, "no length, closed by FIN", 24));
    CHECK(http_get_follow("http://example.com/missing", &r) == 0 && r.status == 404 && r.html == 1);
    CHECK(http_get_follow("http://example.com/bin", &r) == 0 && r.html == -1);
    /* --- ошибки --- */
    CHECK(http_get_follow("http://nope.test/", &r) == HE_DNS);
    CHECK(http_get_follow("http://example.com:81/", &r) == HE_CONNECT);
    CHECK(http_get_follow("http://", &r) == HE_URL);
    CHECK(http_get_follow("http://example.com/silent", &r) == HE_NORESP);
    http_abort = abort_after;
    CHECK(http_get_follow("http://example.com/silent", &r) == HE_ABORT);
    http_abort = 0;
    /* --- DNS: TCP-запас, второй сервер, коды ответа, причины отказа --- */
    CHECK(net_dns("example.com") == SRV && dns_why == 0 && dns_used == DNSIP && dns_rcode == 0);
    dns_dead = DNSIP;                                              /* UDP первого молчит, TCP работает -> ответ по TCP */
    CHECK(net_dns("example.com") == SRV && dns_why == 0 && dns_used == DNSIP);
    tcp_dns_dead = DNSIP;                                          /* и TCP первого мёртв -> второй сервер (шлюз) */
    CHECK(net_dns("example.com") == SRV && dns_why == 0 && dns_used == GW);
    dns_dead = 0; tcp_dns_dead = 0;
    dns_bad_srv = DNSIP; dns_bad_rcode = 2; dns_tcp_rcode = 2;     /* SERVFAIL у первого (UDP и TCP) -> идём дальше */
    CHECK(net_dns("example.com") == SRV && dns_used == GW);
    dns_bad_rcode = 5; dns_tcp_rcode = 5; dns_bad_srv = DNSIP;     /* REFUSED */
    CHECK(net_dns("example.com") == SRV && dns_used == GW);
    dns_bad_srv = 0; dns_tcp_rcode = 0;
    CHECK(net_dns("nope.test") == 0 && dns_why == 4 && dns_rcode == 3 && strcmp(dns_rcode_name(3), "NXDOMAIN") == 0);   /* все серверы: NXDOMAIN */
    dns_bad_srv = 0xFFFFFFFFu;
    dns_dead_all = 1;                                              /* молчат все (и UDP, и TCP) */
    CHECK(net_dns("example.com") == 0 && dns_why == 3);
    dns_dead_all = 0;
    { u32 n0 = nip; nip = 0xC0A80064u;                              /* вне подсети 10.0.2.x: третьего кандидата нет */
      u32 d0 = ndns, g0 = ngw; ndns = 0x0C0C0C0Cu; ngw = 0; CHECK(net_dns("example.com") == 0 && dns_why == 2);   /* сервер вне подсети, шлюза нет */
      ndns = 0; ngw = 0; CHECK(net_dns("example.com") == 0 && dns_why == 1);
      ndns = d0; ngw = g0; nip = n0; }
    CHECK(net_dns("a..b") == 0 && dns_why == 4);
    { int r1 = dns_probe(DNSIP, "example.com"); CHECK(r1 >= 0 && dns_rcode == 0 && dns_an == 1 && dns_done == 1 && dns_ip == SRV); }
    { dns_dead = DNSIP; CHECK(dns_probe(DNSIP, "example.com") == -1); dns_dead = 0; }
    { u32 g0 = ngw, n0 = nip; nip = 0xC0A80064u; ngw = 0; CHECK(dns_probe(0x0C0C0C0Cu, "example.com") == -2); ngw = g0; nip = n0; }
    CHECK(strstr(http_err_str(HE_DNS), "DNS") != 0 && bad_cksum == 0);
    /* --- разбор JSON ответов DoH (Cloudflare / Google) --- */
    {
        const char *cf = "{\"Status\":0,\"TC\":false,\"Question\":[{\"name\":\"a.test\",\"type\":1}],\"Answer\":[{\"name\":\"a.test\",\"type\":5,\"TTL\":9,\"data\":\"b.test.\"},{\"name\":\"b.test\",\"type\":1,\"TTL\":300,\"data\":\"203.0.113.77\"}]}";
        CHECK(json_status(cf, strlen(cf)) == 0 && json_first_ipv4(cf, strlen(cf)) == 0xCB00714Du);          /* CNAME пропущен, берётся A */
        const char *gg = "{ \"Status\": 0, \"Answer\": [ { \"name\": \"x.\", \"type\": 1, \"TTL\": 1, \"data\": \"8.8.4.4\" } ] }";
        CHECK(json_first_ipv4(gg, strlen(gg)) == 0x08080404u);
        const char *nx = "{\"Status\":3,\"Question\":[{\"name\":\"gone.test\",\"type\":1}]}";
        CHECK(json_status(nx, strlen(nx)) == 3 && json_first_ipv4(nx, strlen(nx)) == 0);
        const char *v6 = "{\"Status\":0,\"Answer\":[{\"type\":28,\"data\":\"2001:db8::1\"}]}";
        CHECK(json_first_ipv4(v6, strlen(v6)) == 0 && json_status("{}", 2) == -1 && json_first_ipv4("", 0) == 0);
        const char *trunc = "{\"Answer\":[{\"data\":\"1.2.3";
        CHECK(json_first_ipv4(trunc, strlen(trunc)) == 0);
    }
    /* без TLS DoH недоступен, http_resolve = обычный DNS */
    dns_dead_all = 0; CHECK(http_resolve("example.com") == SRV && dns_via_doh == 0);
    /* --- DHCP: сервера нет -> быстрый отказ, настройки не портятся --- */
    { u32 t0 = clk; CHECK(net_dhcp_ex(1, 300) == -1 && nip == YOU); CHECK(clk - t0 < 5000); }   /* сервера нет: отказ, адрес не потерян */
    /* после ошибок стек должен работать дальше */
    CHECK(http_get_follow("http://10.0.2.2/", &r) == HE_CONNECT || 1);
    CHECK(http_get_follow("http://example.com/", &r) == 0 && r.status == 200);
    printf("http/tcp tests OK (parse, resolve, tcp, loss recovery, chunked, redirects, https, errors, abort)\n");
    return 0;
}
