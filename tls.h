/* tls.h - клиент TLS 1.2: ECDHE (X25519) + AES-128-GCM, SNI. Чистый C, транспорт через указатели.
   ВАЖНО: сертификат сервера НЕ проверяется (трафик шифруется, но личность сервера не подтверждается). */
#ifndef TLS_H
#define TLS_H
#ifndef XBSS
#define XBSS
#endif
#include "crypto.h"

#define TLSE_NONE      0
#define TLSE_TRANSPORT 1     /* обрыв/таймаут */
#define TLSE_ALERT     2     /* сервер прислал alert (код в tls_alert) */
#define TLSE_PROTOCOL  3     /* неожиданное сообщение */
#define TLSE_UNSUPP    4     /* сервер выбрал то, что мы не умеем */
#define TLSE_CRYPTO    5     /* не сошлась проверка (MAC/Finished) */
#define TLSE_SIZE      6     /* слишком большое сообщение */

static u8 *tls_raw;                   /* сырой поток TCP (его заполняет транспорт) */
static int tls_rlen, tls_rpos;
static u8 *tls_plain;                 /* расшифрованные данные приложения */
static int tls_plen, tls_pmax, tls_poverflow;
static int tls_err, tls_alert, tls_closed, tls_up;
static void (*tls_pump)(void);        /* принять ещё байты и обновить tls_rlen */
static int (*tls_send_raw)(const u8 *, int);
static int (*tls_dead)(void);         /* 1 если транспорт закрыт */
static u32 (*tls_ms)(void);
static void (*tls_rand)(u8 *, int);

static u8 tls_hs[40000] XBSS;         /* сборка handshake-сообщений */
static int tls_hslen;
static u8 tls_rec[17000] XBSS;        /* буфер расшифрованной записи */
static u8 tls_out[17000] XBSS;        /* буфер исходящей записи */
static struct aes tls_cw, tls_sw;
static u8 tls_civ[4], tls_siv[4];
static u32 tls_cseq, tls_sseq;
static int tls_enc_r, tls_enc_w;
static struct sha256 tls_tr;

static const char *tls_err_str(void) {
    switch (tls_err) {
        case TLSE_TRANSPORT: return "connection lost or timed out during TLS";
        case TLSE_ALERT: return "the server rejected the TLS handshake (alert)";
        case TLSE_PROTOCOL: return "unexpected TLS message";
        case TLSE_UNSUPP: return "server needs TLS features Cravgos lacks (needs TLS 1.2, X25519, AES-GCM)";
        case TLSE_CRYPTO: return "TLS integrity check failed";
        case TLSE_SIZE: return "TLS message too large";
    }
    return "TLS error";
}
static u32 t_be32(const u8 *p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; }
static int t_be16(const u8 *p) { return (p[0] << 8) | p[1]; }
static void t_put16(u8 *p, int v) { p[0] = v >> 8; p[1] = v; }

/* есть ли целая запись в сыром потоке: 1 да, 0 нет, -1 мусор */
static int tls_peek_record(int *type, const u8 **pl, int *plen) {
    if (tls_rlen - tls_rpos < 5) return 0;
    const u8 *h = tls_raw + tls_rpos;
    int len = t_be16(h + 3);
    if (h[1] != 3 || len > 16384 + 2048) return -1;
    if (tls_rlen - tls_rpos < 5 + len) return 0;
    *type = h[0]; *pl = h + 5; *plen = len;
    return 1;
}
static int tls_wait_record(int *type, const u8 **pl, int *plen, u32 deadline) {
    for (;;) {
        int r = tls_peek_record(type, pl, plen);
        if (r < 0) { tls_err = TLSE_PROTOCOL; return -1; }
        if (r > 0) return 0;
        tls_pump();
        if (tls_peek_record(type, pl, plen) > 0) return 0;
        if (tls_dead && tls_dead()) { tls_err = TLSE_TRANSPORT; return -1; }
        if (tls_ms() > deadline) { tls_err = TLSE_TRANSPORT; return -1; }
    }
}
static int tls_send_plain_rec(u8 type, const u8 *d, int len, int ver) {
    tls_out[0] = type; tls_out[1] = 3; tls_out[2] = ver; t_put16(tls_out + 3, len);
    for (int i = 0; i < len; i++) tls_out[5 + i] = d[i];
    return tls_send_raw(tls_out, 5 + len);
}
/* расшифровка записи сервера: payload = explicit(8) | ct | tag(16); результат в tls_rec, длина в *olen */
static int tls_decrypt(int type, const u8 *pl, int plen, int *olen) {
    if (plen < 24) { tls_err = TLSE_CRYPTO; return -1; }
    int n = plen - 24;
    u8 nonce[12], aad[13], tag[16];
    for (int i = 0; i < 4; i++) nonce[i] = tls_siv[i];
    for (int i = 0; i < 8; i++) nonce[4 + i] = pl[i];
    aad[0] = aad[1] = aad[2] = aad[3] = 0; aad[4] = tls_sseq >> 24; aad[5] = tls_sseq >> 16; aad[6] = tls_sseq >> 8; aad[7] = tls_sseq;
    aad[8] = type; aad[9] = 3; aad[10] = 3; t_put16(aad + 11, n);
    gcm_crypt(&tls_sw, nonce, aad, 13, pl + 8, n, tls_rec, tag, 1);
    u8 diff = 0;
    for (int i = 0; i < 16; i++) diff |= tag[i] ^ pl[8 + n + i];
    if (diff) { tls_err = TLSE_CRYPTO; return -1; }
    tls_sseq++;
    *olen = n;
    return 0;
}
static int tls_send_enc_rec(u8 type, const u8 *d, int len) {
    if (len > 16000) len = 16000;
    u8 nonce[12], aad[13], *o = tls_out;
    for (int i = 0; i < 4; i++) nonce[i] = tls_civ[i];
    u8 seq[8] = {0, 0, 0, 0, tls_cseq >> 24, tls_cseq >> 16, tls_cseq >> 8, tls_cseq};
    for (int i = 0; i < 8; i++) { nonce[4 + i] = seq[i]; aad[i] = seq[i]; }
    aad[8] = type; aad[9] = 3; aad[10] = 3; t_put16(aad + 11, len);
    o[0] = type; o[1] = 3; o[2] = 3; t_put16(o + 3, 8 + len + 16);
    for (int i = 0; i < 8; i++) o[5 + i] = seq[i];
    gcm_crypt(&tls_cw, nonce, aad, 13, d, len, o + 13, o + 13 + len, 0);
    tls_cseq++;
    return tls_send_raw(o, 5 + 8 + len + 16);
}

/* обработка накопленных handshake-сообщений до ServerHelloDone */
static u8 tls_crand[32], tls_srand[32], tls_spub[32];
static int tls_got_hello, tls_got_kx, tls_got_done;
static int tls_hs_process(void) {
    while (tls_hslen >= 4) {
        int mt = tls_hs[0], ml = (tls_hs[1] << 16) | (tls_hs[2] << 8) | tls_hs[3];
        if (ml > (int)sizeof tls_hs - 4) { tls_err = TLSE_SIZE; return -1; }
        if (tls_hslen < 4 + ml) break;
        const u8 *m = tls_hs + 4;
        sha256_update(&tls_tr, tls_hs, 4 + ml);
        if (mt == 2) {                                         /* ServerHello */
            if (ml < 38 || m[0] != 3 || m[1] != 3) { tls_err = TLSE_UNSUPP; return -1; }
            for (int i = 0; i < 32; i++) tls_srand[i] = m[2 + i];
            int sl = m[34];
            if (ml < 35 + sl + 3) { tls_err = TLSE_PROTOCOL; return -1; }
            int cs = t_be16(m + 35 + sl);
            if (cs != 0xC02F && cs != 0xC02B) { tls_err = TLSE_UNSUPP; return -1; }
            if (m[37 + sl] != 0) { tls_err = TLSE_UNSUPP; return -1; }
            tls_got_hello = 1;
        } else if (mt == 12) {                                 /* ServerKeyExchange */
            if (ml < 4 + 32 || m[0] != 3 || t_be16(m + 1) != 0x001d || m[3] != 32) { tls_err = TLSE_UNSUPP; return -1; }
            for (int i = 0; i < 32; i++) tls_spub[i] = m[4 + i];
            tls_got_kx = 1;
        } else if (mt == 14) {                                 /* ServerHelloDone */
            tls_got_done = 1;
        }                                                      /* Certificate (11), CertificateRequest (13): пропускаем */
        int used = 4 + ml;
        for (int i = used; i < tls_hslen; i++) tls_hs[i - used] = tls_hs[i];
        tls_hslen -= used;
    }
    return 0;
}
static int tls_handshake(const char *host) {
    u8 priv[32], pub[32], base[32], pms[32], master[48], kb[40], seed[64], tmp[600];
    for (int i = 0; i < 32; i++) base[i] = 0;
    base[0] = 9;
    tls_err = TLSE_NONE; tls_alert = 0; tls_closed = 0; tls_up = 0; tls_plen = 0; tls_poverflow = 0;
    tls_hslen = 0; tls_rpos = 0; tls_cseq = tls_sseq = 0; tls_enc_r = tls_enc_w = 0;
    tls_got_hello = tls_got_kx = tls_got_done = 0;
    u32 deadline = tls_ms() + 20000;
    tls_rand(tls_crand, 32); tls_rand(priv, 32);
    x25519(pub, priv, base);
    /* ---- ClientHello ---- */
    int hl = 0; while (host[hl]) hl++;
    if (hl > 200) { tls_err = TLSE_SIZE; return -1; }
    u8 *b = tmp; int n = 0;
    b[n++] = 1; b[n++] = 0; b[n++] = 0; b[n++] = 0;                 /* длина допишем */
    b[n++] = 3; b[n++] = 3;
    for (int i = 0; i < 32; i++) b[n++] = tls_crand[i];
    b[n++] = 0;                                                     /* session id */
    b[n++] = 0; b[n++] = 6; b[n++] = 0xC0; b[n++] = 0x2F; b[n++] = 0xC0; b[n++] = 0x2B; b[n++] = 0x00; b[n++] = 0xFF;
    b[n++] = 1; b[n++] = 0;                                         /* compression: null */
    int ext_len_pos = n; n += 2;
    /* SNI */
    b[n++] = 0; b[n++] = 0; t_put16(b + n, hl + 5); n += 2; t_put16(b + n, hl + 3); n += 2; b[n++] = 0; t_put16(b + n, hl); n += 2;
    for (int i = 0; i < hl; i++) b[n++] = host[i];
    /* supported_groups */
    b[n++] = 0; b[n++] = 10; b[n++] = 0; b[n++] = 6; b[n++] = 0; b[n++] = 4; b[n++] = 0; b[n++] = 0x1d; b[n++] = 0; b[n++] = 0x17;   /* x25519, затем secp256r1 (нужна серверам с ECDSA P-256) */
    /* ec_point_formats */
    b[n++] = 0; b[n++] = 11; b[n++] = 0; b[n++] = 2; b[n++] = 1; b[n++] = 0;
    /* signature_algorithms */
    static const u8 sa[] = {0, 13, 0, 14, 0, 12, 4, 3, 8, 4, 4, 1, 5, 3, 8, 5, 5, 1};
    for (unsigned i = 0; i < sizeof sa; i++) b[n++] = sa[i];
    /* renegotiation_info */
    b[n++] = 0xFF; b[n++] = 0x01; b[n++] = 0; b[n++] = 1; b[n++] = 0;
    t_put16(b + ext_len_pos, n - ext_len_pos - 2);
    b[1] = 0; b[2] = (n - 4) >> 8; b[3] = (n - 4);
    sha256_init(&tls_tr);
    sha256_update(&tls_tr, b, n);
    if (tls_send_plain_rec(22, b, n, 1) < 0) { tls_err = TLSE_TRANSPORT; return -1; }
    /* ---- ответ сервера до ServerHelloDone ---- */
    while (!tls_got_done) {
        int type, plen; const u8 *pl;
        if (tls_wait_record(&type, &pl, &plen, deadline) < 0) return -1;
        if (type == 21) { tls_err = TLSE_ALERT; tls_alert = plen >= 2 ? pl[1] : 0; return -1; }
        if (type != 22) { tls_err = TLSE_PROTOCOL; return -1; }
        if (tls_hslen + plen > (int)sizeof tls_hs) { tls_err = TLSE_SIZE; return -1; }
        for (int i = 0; i < plen; i++) tls_hs[tls_hslen + i] = pl[i];
        tls_hslen += plen;
        tls_rpos += 5 + plen;
        if (tls_hs_process() < 0) return -1;
    }
    if (!tls_got_hello || !tls_got_kx) { tls_err = TLSE_PROTOCOL; return -1; }
    /* ---- ключи ---- */
    x25519(pms, priv, tls_spub);
    u8 nz = 0; for (int i = 0; i < 32; i++) nz |= pms[i];
    if (!nz) { tls_err = TLSE_CRYPTO; return -1; }
    for (int i = 0; i < 32; i++) { seed[i] = tls_crand[i]; seed[32 + i] = tls_srand[i]; }
    tls_prf(pms, 32, "master secret", seed, 64, master, 48);
    for (int i = 0; i < 32; i++) { seed[i] = tls_srand[i]; seed[32 + i] = tls_crand[i]; }
    tls_prf(master, 48, "key expansion", seed, 64, kb, 40);
    aes_set_key(&tls_cw, kb); aes_set_key(&tls_sw, kb + 16);
    for (int i = 0; i < 4; i++) { tls_civ[i] = kb[32 + i]; tls_siv[i] = kb[36 + i]; }
    for (int i = 0; i < 32; i++) pms[i] = 0;
    /* ---- ClientKeyExchange, ChangeCipherSpec, Finished ---- */
    u8 cke[37];
    cke[0] = 16; cke[1] = 0; cke[2] = 0; cke[3] = 33; cke[4] = 32;
    for (int i = 0; i < 32; i++) cke[5 + i] = pub[i];
    sha256_update(&tls_tr, cke, 37);
    if (tls_send_plain_rec(22, cke, 37, 3) < 0) { tls_err = TLSE_TRANSPORT; return -1; }
    static const u8 ccs[1] = {1};
    if (tls_send_plain_rec(20, ccs, 1, 3) < 0) { tls_err = TLSE_TRANSPORT; return -1; }
    u8 hash[32], fin[16];
    { struct sha256 c = tls_tr; sha256_final(&c, hash); }
    fin[0] = 20; fin[1] = 0; fin[2] = 0; fin[3] = 12;
    tls_prf(master, 48, "client finished", hash, 32, fin + 4, 12);
    sha256_update(&tls_tr, fin, 16);
    if (tls_send_enc_rec(22, fin, 16) < 0) { tls_err = TLSE_TRANSPORT; return -1; }
    tls_enc_w = 1;
    /* ---- ChangeCipherSpec и Finished сервера ---- */
    int type, plen; const u8 *pl;
    if (tls_wait_record(&type, &pl, &plen, deadline) < 0) return -1;
    if (type == 21) { tls_err = TLSE_ALERT; tls_alert = plen >= 2 ? pl[1] : 0; return -1; }
    if (type != 20) { tls_err = TLSE_PROTOCOL; return -1; }
    tls_rpos += 5 + plen;
    if (tls_wait_record(&type, &pl, &plen, deadline) < 0) return -1;
    if (type == 21) { tls_err = TLSE_ALERT; tls_alert = plen >= 2 ? pl[1] : 0; return -1; }
    if (type != 22) { tls_err = TLSE_PROTOCOL; return -1; }
    int olen;
    if (tls_decrypt(22, pl, plen, &olen) < 0) return -1;
    tls_rpos += 5 + plen;
    u8 want[12];
    { struct sha256 c = tls_tr; sha256_final(&c, hash); }
    tls_prf(master, 48, "server finished", hash, 32, want, 12);
    if (olen != 16 || tls_rec[0] != 20 || tls_rec[3] != 12) { tls_err = TLSE_PROTOCOL; return -1; }
    u8 diff = 0; for (int i = 0; i < 12; i++) diff |= want[i] ^ tls_rec[4 + i];
    if (diff) { tls_err = TLSE_CRYPTO; return -1; }
    tls_enc_r = 1; tls_up = 1;
    for (int i = 0; i < 48; i++) master[i] = 0;
    for (int i = 0; i < 32; i++) priv[i] = 0;
    return 0;
}
static int tls_send(const u8 *d, int len) {
    while (len > 0) {
        int n = len > 16000 ? 16000 : len;
        if (tls_send_enc_rec(23, d, n) < 0) return -1;
        d += n; len -= n;
    }
    return 0;
}
/* разбирает все полные записи из сырого потока; расшифрованные данные добавляются в tls_plain */
static void tls_process(void) {
    for (;;) {
        int type, plen; const u8 *pl;
        int r = tls_peek_record(&type, &pl, &plen);
        if (r < 0) { tls_err = TLSE_PROTOCOL; tls_closed = 1; return; }
        if (r == 0) return;
        if (type == 23 && tls_up) {
            int olen;
            if (tls_decrypt(23, pl, plen, &olen) < 0) { tls_closed = 1; tls_rpos += 5 + plen; return; }
            int room = tls_pmax - tls_plen, c = olen < room ? olen : room;
            for (int i = 0; i < c; i++) tls_plain[tls_plen + i] = tls_rec[i];
            tls_plen += c;
            if (c < olen) tls_poverflow = 1;
        } else if (type == 21) {
            int olen = plen; const u8 *ap = pl;
            if (tls_up && plen >= 24) { if (tls_decrypt(21, pl, plen, &olen) == 0) ap = tls_rec; else { tls_rpos += 5 + plen; tls_closed = 1; return; } }
            if (olen >= 2) { tls_alert = ap[1]; tls_closed = 1; }
        }                                                      /* прочее после handshake игнорируем */
        tls_rpos += 5 + plen;
    }
}
static void tls_close_notify(void) {
    static const u8 a[2] = {1, 0};
    if (tls_up && !tls_closed) tls_send_enc_rec(21, a, 2);
    tls_up = 0;
}
#endif
