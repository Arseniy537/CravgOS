/* Полная цепочка: наш TCP + TLS + HTTP против настоящего `openssl s_server`.
   Мок-"сеть" принимает кадры клиента и проксирует TCP-поток в локальный сокет. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
#include "../net.h"
#include "../tls.h"
#define HTTP_TLS 1
#include "../http.h"
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while(0)

static u8 gwmac[6] = {0x52,0x54,0x00,0x12,0x35,0x02};
static const u32 GW = 0x0A000202, YOU = 0x0A00020F, SRV = 0x0A090909u;
static u8 q[256][1600]; static int ql[256], qh, qt, bad_cksum;
static int sock = -1, eof_seen, fin_sent, port;
static u32 iss = 7000, hi, rcv; static u16 cport;
static u32 s_src = 0x0A090909u; static u16 s_port = 443;
static void push(const u8 *f, int n) { memcpy(q[qt], f, n); ql[qt] = n; qt = (qt + 1) % 256; }
static int mk_ip(u8 *f, const u8 *dm, u32 s, u32 d, u8 proto, const u8 *pl, int len) {
    memcpy(f, dm, 6); memcpy(f + 6, gwmac, 6); be16(f + 12, 0x0800);
    u8 *p = f + 14; p[0] = 0x45; p[1] = 0; be16(p + 2, 20 + len); be16(p + 4, 1); be16(p + 6, 0);
    p[8] = 64; p[9] = proto; be16(p + 10, 0); be32(p + 12, s); be32(p + 16, d);
    be16(p + 10, net_cksum(p, 20)); memcpy(p + 20, pl, len);
    return 14 + 20 + len;
}
static void tcp_out(u8 flags, u32 seq, u32 ack, const u8 *d, int dl) {
    u8 seg[1500]; memset(seg, 0, 20);
    be16(seg, s_port); be16(seg + 2, cport); be32(seg + 4, seq); be32(seg + 8, ack);
    seg[12] = 5 << 4; seg[13] = flags; be16(seg + 14, 65535); if (dl) memcpy(seg + 20, d, dl);
    be16(seg + 16, tcp_sum(s_src, YOU, seg, 20 + dl));
    u8 f[1600]; int n = mk_ip(f, nmac, s_src, YOU, 6, seg, 20 + dl); push(f, n);
}
static u32 msn(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000u + t.tv_nsec / 1000000; }
static int rx(u8 *b, int max) {
    (void)max;
    if (qh == qt && sock >= 0 && !eof_seen) {
        struct pollfd p = {sock, POLLIN, 0};
        if (poll(&p, 1, 1) > 0) {
            u8 d[1000]; ssize_t n = read(sock, d, sizeof d);
            if (n > 0) { tcp_out(0x10, hi, rcv, d, n); hi += n; }
            else if (n == 0) eof_seen = 1;
        }
    }
    if (qh == qt && eof_seen && !fin_sent) { tcp_out(0x11, hi, rcv, 0, 0); hi++; fin_sent = 1; }
    if (qh == qt) return 0;
    int n = ql[qh]; memcpy(b, q[qh], n); qh = (qh + 1) % 256; return n;
}
static int tx(const u8 *f, int n) {
    (void)n;
    u16 type = rd16(f + 12); const u8 *p = f + 14;
    if (type == 0x0806) {
        if (rd16(p + 6) == 1 && rd32(p + 24) == GW) {
            u8 a[60]; memset(a, 0, 60); memcpy(a, f + 6, 6); memcpy(a + 6, gwmac, 6); be16(a + 12, 0x0806);
            u8 *r = a + 14; be16(r, 1); be16(r + 2, 0x0800); r[4] = 6; r[5] = 4; be16(r + 6, 2);
            memcpy(r + 8, gwmac, 6); be32(r + 14, GW); memcpy(r + 18, f + 6, 6); be32(r + 24, rd32(p + 14)); push(a, 60);
        }
        return 0;
    }
    if (type != 0x0800 || p[9] != 6) return 0;
    if (net_cksum(p, 20) != 0) bad_cksum++;
    u32 s = rd32(p + 12), d = rd32(p + 16); const u8 *u = p + 20; int ul = rd16(p + 2) - 20;
    { u16 dp0 = rd16(u + 2); u16 sp0 = rd16(u);
      if (d != SRV && dp0 != 443 && dp0 != 53) return 0;
      s_src = d; s_port = dp0;
      if (dp0 == 53) { cport = sp0; if (u[13] & 0x02) tcp_out(0x14, 0, rd32(u + 4) + 1, 0, 0); return 0; } }
    if (tcp_sum(s, d, u, ul) != 0) bad_cksum++;
    u16 sport = rd16(u); u32 seq = rd32(u + 4); int hl = (u[12] >> 4) * 4; u8 fl = u[13]; const u8 *dd = u + hl; int dl = ul - hl;
    if (fl & 0x02) {
        cport = sport; rcv = seq + 1; hi = iss + 1; eof_seen = fin_sent = 0;
        if (sock >= 0) close(sock);
        sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons(port); inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
        if (connect(sock, (struct sockaddr *)&a, sizeof a) < 0) { perror("connect"); exit(3); }
        tcp_out(0x12, iss, rcv, 0, 0);
        return 0;
    }
    if (dl > 0) {
        if (seq == rcv) { rcv += dl; if (write(sock, dd, dl) != dl) exit(4); }
        tcp_out(0x10, hi, rcv, 0, 0);
    } else if (fl & 0x01) { rcv++; tcp_out(0x10, hi, rcv, 0, 0); }
    return 0;
}
static void rnd(u8 *b, int n) { FILE *f = fopen("/dev/urandom", "rb"); if (fread(b, 1, n, f) != (size_t)n) exit(9); fclose(f); }
static u8 buf[1000000], raw[1000000], want[1000000];
int main(int argc, char **argv) {
    if (argc < 2) return 2;
    port = atoi(argv[1]);
    u8 mac[6] = {0x08,0,0x27,1,2,3}; memcpy(nmac, mac, 6);
    nic_tx = tx; nic_rx = rx; ms_now = msn;
    nip = YOU; nmask = 0xFFFFFF00u; ngw = GW; ndns = 0;
    http_buf = buf; http_max = sizeof buf - 1; http_raw = raw; http_rawmax = sizeof raw - 1; http_rand = rnd;
    struct hres r;
    const char *files[] = {"small.txt", "big.txt"};
    for (int k = 0; k < 2; k++) {
        char url[100]; sprintf(url, "https://10.9.9.9/%s", files[k]);
        int e = http_get_follow(url, &r);
        if (e < 0) { printf("FAIL %s: error %d (%s)\n", url, e, http_err_str(e)); return 1; }
        FILE *f = fopen(files[k], "rb"); size_t wl = fread(want, 1, sizeof want, f); fclose(f);
        printf("%s: status=%d ctype=[%s] body=%d expected=%zu truncated=%d\n", url, r.status, r.ctype, r.body_len, wl, r.truncated);
        CHECK(r.status == 200 && (size_t)r.body_len == wl && memcmp(buf + r.body_off, want, wl) == 0 && !r.truncated);
        CHECK(tcp_state == TCP_CLOSED && bad_cksum == 0);
    }
    /* --- DNS через HTTPS: обычный DNS молчит, ответ приходит по TLS (Cloudflare-стиль, затем Google-стиль, затем NXDOMAIN) --- */
    {
        ndns = 0; ngw = GW; http_doh_on = 1;
        u32 ip = http_resolve("doh.test");
        printf("doh.test -> %u.%u.%u.%u via_doh=%d used=%08x\n", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255, dns_via_doh, dns_used);
        CHECK(ip == 0xCB00714Du && dns_via_doh == 1 && dns_used == 0x01010101u);
        ip = http_resolve("g.test");                                   /* у 1.1.1.1 файла нет (404) -> 8.8.8.8 */
        CHECK(ip == 0x08080404u && dns_via_doh == 1 && dns_used == 0x08080808u);
        ip = http_resolve("gone.test");
        CHECK(ip == 0 && dns_why == 4);
        http_doh_on = 0; ip = http_resolve("doh.test"); CHECK(ip == 0 && dns_via_doh == 0); http_doh_on = 1;
    }
    /* неправильный порт сервера: ошибка, а не зависание */
    printf("https end-to-end OK (TCP + TLS + HTTP vs openssl)\n");
    return 0;
}
