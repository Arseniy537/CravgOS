/* Интероперабельность: наш TLS-клиент <-> настоящий `openssl s_server` на localhost. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
#include "../tls.h"
static int sock, eof;
static u8 rawbuf[900000], plainbuf[900000];
static void pump(void) {
    struct pollfd p = {sock, POLLIN, 0};
    if (poll(&p, 1, 20) > 0) {
        ssize_t n = read(sock, rawbuf + tls_rlen, sizeof rawbuf - tls_rlen);
        if (n > 0) tls_rlen += n; else if (n == 0) eof = 1;
    }
}
static int dead(void) { return eof; }
static int sendraw(const u8 *d, int n) { while (n > 0) { ssize_t w = write(sock, d, n); if (w <= 0) return -1; d += w; n -= w; } return 0; }
static u32 ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000u + t.tv_nsec / 1000000; }
static void rnd(u8 *b, int n) { FILE *f = fopen("/dev/urandom", "rb"); if (fread(b, 1, n, f) != (size_t)n) exit(9); fclose(f); }
int main(int argc, char **argv) {
    if (argc < 4) { printf("usage: port path expectedfile\n"); return 2; }
    int port = atoi(argv[1]);
    sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons(port); inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if (connect(sock, (struct sockaddr *)&a, sizeof a) < 0) { perror("connect"); return 3; }
    tls_raw = rawbuf; tls_plain = plainbuf; tls_pmax = sizeof plainbuf;
    tls_pump = pump; tls_send_raw = sendraw; tls_dead = dead; tls_ms = ms; tls_rand = rnd;
    if (tls_handshake("localhost") < 0) { printf("HANDSHAKE FAILED: err=%d (%s) alert=%d\n", tls_err, tls_err_str(), tls_alert); return 1; }
    char req[300]; int n = sprintf(req, "GET %s HTTP/1.0\r\nHost: localhost\r\n\r\n", argv[2]);
    if (tls_send((u8 *)req, n) < 0) { printf("send failed\n"); return 1; }
    u32 t0 = ms(), last = t0; int lastlen = 0;
    for (;;) {
        pump(); tls_process();
        if (tls_plen != lastlen) { lastlen = tls_plen; last = ms(); }
        if (tls_closed) break;
        if (eof) { tls_process(); break; }
        if (ms() - last > 5000 || ms() - t0 > 30000) { printf("TIMEOUT: got %d bytes\n", tls_plen); return 1; }
    }
    FILE *f = fopen(argv[3], "rb"); static u8 want[900000]; size_t wl = fread(want, 1, sizeof want, f); fclose(f);
    int hdr = 0;                                               /* openssl -WWW добавляет HTTP-заголовки */
    for (int i = 0; i + 3 < tls_plen; i++) if (!memcmp(plainbuf + i, "\r\n\r\n", 4)) { hdr = i + 4; break; }
    printf("handshake ok, received %d bytes (headers %d, body %d, expected %zu), closed_by_alert=%d overflow=%d\n", tls_plen, hdr, tls_plen - hdr, wl, tls_closed, tls_poverflow);
    if ((size_t)(tls_plen - hdr) != wl || memcmp(plainbuf + hdr, want, wl) != 0) { printf("MISMATCH\n"); return 1; }
    tls_close_notify();
    printf("data matches byte for byte\n");
    return 0;
}
