/* crypto.h - SHA-256, HMAC, TLS-PRF, AES-128-GCM, X25519. Чистый C без libc. */
#ifndef CRYPTO_H
#define CRYPTO_H
typedef long long c_i64;

/* ---------- SHA-256 ---------- */
struct sha256 { u32 h[8]; u8 buf[64]; u32 total; int bl; };
static const u32 sha_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha256_init(struct sha256 *c) {
    static const u32 iv[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    for (int i = 0; i < 8; i++) c->h[i] = iv[i];
    c->total = 0; c->bl = 0;
}
static void sha256_block(struct sha256 *c, const u8 *p) {
    u32 w[64], a, b, cc, d, e, f, g, h;
    for (int i = 0; i < 16; i++) w[i] = ((u32)p[4*i] << 24) | ((u32)p[4*i+1] << 16) | ((u32)p[4*i+2] << 8) | p[4*i+3];
    for (int i = 16; i < 64; i++) {
        u32 s0 = ROR(w[i-15], 7) ^ ROR(w[i-15], 18) ^ (w[i-15] >> 3);
        u32 s1 = ROR(w[i-2], 17) ^ ROR(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a = c->h[0]; b = c->h[1]; cc = c->h[2]; d = c->h[3]; e = c->h[4]; f = c->h[5]; g = c->h[6]; h = c->h[7];
    for (int i = 0; i < 64; i++) {
        u32 S1 = ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25), ch = (e & f) ^ (~e & g);
        u32 t1 = h + S1 + ch + sha_k[i] + w[i];
        u32 S0 = ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22), mj = (a & b) ^ (a & cc) ^ (b & cc);
        u32 t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1; d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d; c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}
static void sha256_update(struct sha256 *c, const u8 *d, int len) {
    c->total += len;
    while (len > 0) {
        int n = 64 - c->bl; if (n > len) n = len;
        for (int i = 0; i < n; i++) c->buf[c->bl + i] = d[i];
        c->bl += n; d += n; len -= n;
        if (c->bl == 64) { sha256_block(c, c->buf); c->bl = 0; }
    }
}
static void sha256_final(struct sha256 *c, u8 *out) {         /* портит c; для "снимка" копируйте структуру */
    u32 hi = c->total >> 29, lo = c->total << 3;
    u8 pad = 0x80;
    sha256_update(c, &pad, 1);
    pad = 0;
    while (c->bl != 56) sha256_update(c, &pad, 1);
    u8 L[8] = {hi >> 24, hi >> 16, hi >> 8, hi, lo >> 24, lo >> 16, lo >> 8, lo};
    sha256_update(c, L, 8);
    for (int i = 0; i < 8; i++) { out[4*i] = c->h[i] >> 24; out[4*i+1] = c->h[i] >> 16; out[4*i+2] = c->h[i] >> 8; out[4*i+3] = c->h[i]; }
}
static void hmac_sha256(const u8 *key, int kl, const u8 *msg, int ml, u8 *out) {
    u8 k[64], t[32], ip[64], op[64];
    for (int i = 0; i < 64; i++) k[i] = 0;
    if (kl > 64) { struct sha256 c; sha256_init(&c); sha256_update(&c, key, kl); sha256_final(&c, k); }
    else for (int i = 0; i < kl; i++) k[i] = key[i];
    for (int i = 0; i < 64; i++) { ip[i] = k[i] ^ 0x36; op[i] = k[i] ^ 0x5c; }
    struct sha256 c;
    sha256_init(&c); sha256_update(&c, ip, 64); sha256_update(&c, msg, ml); sha256_final(&c, t);
    sha256_init(&c); sha256_update(&c, op, 64); sha256_update(&c, t, 32); sha256_final(&c, out);
}
static void tls_prf(const u8 *sec, int sl, const char *label, const u8 *seed, int seedl, u8 *out, int outl) {
    u8 ls[160], a[32], tmp[32 + 160], h[32];
    int ll = 0; while (label[ll]) { ls[ll] = label[ll]; ll++; }
    for (int i = 0; i < seedl; i++) ls[ll + i] = seed[i];
    int n = ll + seedl;
    hmac_sha256(sec, sl, ls, n, a);
    int done = 0;
    while (done < outl) {
        for (int i = 0; i < 32; i++) tmp[i] = a[i];
        for (int i = 0; i < n; i++) tmp[32 + i] = ls[i];
        hmac_sha256(sec, sl, tmp, 32 + n, h);
        for (int i = 0; i < 32 && done < outl; i++) out[done++] = h[i];
        hmac_sha256(sec, sl, a, 32, a);
    }
}

/* ---------- AES-128 (только шифрование блока) + GCM ---------- */
struct aes { u8 rk[176]; };
static u8 aes_sbox[256];
static int aes_ready;
static void aes_init_tables(void) {
    u8 p = 1, q = 1;
    do {
        p = p ^ (u8)(p << 1) ^ ((p & 0x80) ? 0x1B : 0);
        q ^= q << 1; q ^= q << 2; q ^= q << 4;
        if (q & 0x80) q ^= 0x09;
        u8 x = q ^ (u8)((q << 1) | (q >> 7)) ^ (u8)((q << 2) | (q >> 6)) ^ (u8)((q << 3) | (q >> 5)) ^ (u8)((q << 4) | (q >> 4));
        aes_sbox[p] = x ^ 0x63;
    } while (p != 1);
    aes_sbox[0] = 0x63;
    aes_ready = 1;
}
static u8 xt(u8 x) { return (u8)((x << 1) ^ ((x & 0x80) ? 0x1B : 0)); }
static void aes_set_key(struct aes *a, const u8 *key) {
    if (!aes_ready) aes_init_tables();
    for (int i = 0; i < 16; i++) a->rk[i] = key[i];
    u8 rc = 1;
    for (int i = 16; i < 176; i += 4) {
        u8 t[4] = {a->rk[i-4], a->rk[i-3], a->rk[i-2], a->rk[i-1]};
        if (i % 16 == 0) {
            u8 t0 = t[0];
            t[0] = aes_sbox[t[1]] ^ rc; t[1] = aes_sbox[t[2]]; t[2] = aes_sbox[t[3]]; t[3] = aes_sbox[t0];
            rc = xt(rc);
        }
        for (int k = 0; k < 4; k++) a->rk[i + k] = a->rk[i - 16 + k] ^ t[k];
    }
}
static void aes_enc(const struct aes *a, const u8 *in, u8 *out) {
    u8 s[16], t[16];
    for (int i = 0; i < 16; i++) s[i] = in[i] ^ a->rk[i];
    for (int r = 1; r <= 10; r++) {
        for (int i = 0; i < 16; i++) s[i] = aes_sbox[s[i]];
        for (int c = 0; c < 4; c++) for (int rw = 0; rw < 4; rw++) t[4*c + rw] = s[4*((c + rw) & 3) + rw];     /* ShiftRows */
        if (r < 10) {
            for (int c = 0; c < 4; c++) {
                u8 *x = t + 4*c, a0 = x[0], a1 = x[1], a2 = x[2], a3 = x[3], all = a0 ^ a1 ^ a2 ^ a3;
                s[4*c]   = a0 ^ all ^ xt(a0 ^ a1);
                s[4*c+1] = a1 ^ all ^ xt(a1 ^ a2);
                s[4*c+2] = a2 ^ all ^ xt(a2 ^ a3);
                s[4*c+3] = a3 ^ all ^ xt(a3 ^ a0);
            }
        } else for (int i = 0; i < 16; i++) s[i] = t[i];
        for (int i = 0; i < 16; i++) s[i] ^= a->rk[16*r + i];
    }
    for (int i = 0; i < 16; i++) out[i] = s[i];
}
static void gf_mul(u8 *x, const u8 *h) {                  /* x = x * h в GF(2^128) */
    u8 z[16], v[16];
    for (int i = 0; i < 16; i++) { z[i] = 0; v[i] = h[i]; }
    for (int i = 0; i < 128; i++) {
        if (x[i >> 3] & (0x80 >> (i & 7))) for (int k = 0; k < 16; k++) z[k] ^= v[k];
        int carry = v[15] & 1;
        for (int k = 15; k > 0; k--) v[k] = (v[k] >> 1) | (v[k-1] << 7);
        v[0] >>= 1;
        if (carry) v[0] ^= 0xE1;
    }
    for (int i = 0; i < 16; i++) x[i] = z[i];
}
static void ghash_upd(u8 *y, const u8 *h, const u8 *d, int len) {
    while (len > 0) {
        int n = len < 16 ? len : 16;
        for (int i = 0; i < n; i++) y[i] ^= d[i];
        gf_mul(y, h);
        d += n; len -= n;
    }
}
/* AES-128-GCM: шифрует (decrypt=0) или расшифровывает (decrypt=1); tag - 16 байт.
   При расшифровке tag вычисляется по шифртексту: сравнение делает вызывающий. in может совпадать с out. */
static void gcm_crypt(const struct aes *a, const u8 *iv, const u8 *aad, int al, const u8 *in, int len, u8 *out, u8 *tag, int decrypt) {
    u8 h[16], zero[16], y[16], ctr[16], ks[16], j0e[16];
    for (int i = 0; i < 16; i++) { zero[i] = 0; y[i] = 0; }
    aes_enc(a, zero, h);
    for (int i = 0; i < 12; i++) ctr[i] = iv[i];
    ctr[12] = ctr[13] = ctr[14] = 0; ctr[15] = 1;
    aes_enc(a, ctr, j0e);
    ghash_upd(y, h, aad, al);
    if (decrypt) ghash_upd(y, h, in, len);
    for (int off = 0; off < len; off += 16) {
        for (int k = 15; k >= 12; k--) if (++ctr[k]) break;
        aes_enc(a, ctr, ks);
        int n = len - off < 16 ? len - off : 16;
        for (int i = 0; i < n; i++) out[off + i] = in[off + i] ^ ks[i];
    }
    if (!decrypt) ghash_upd(y, h, out, len);
    u8 L[16];
    /* длины в битах, big-endian 64 */
    {
      u32 bl = (u32)len << 3, bh = (u32)len >> 29, al_ = (u32)al << 3, ah = (u32)al >> 29;
      L[0] = ah >> 24; L[1] = ah >> 16; L[2] = ah >> 8; L[3] = ah; L[4] = al_ >> 24; L[5] = al_ >> 16; L[6] = al_ >> 8; L[7] = al_;
      L[8] = bh >> 24; L[9] = bh >> 16; L[10] = bh >> 8; L[11] = bh; L[12] = bl >> 24; L[13] = bl >> 16; L[14] = bl >> 8; L[15] = bl;
    }
    ghash_upd(y, h, L, 16);
    for (int i = 0; i < 16; i++) tag[i] = y[i] ^ j0e[i];
}

/* ---------- X25519 (по TweetNaCl) ---------- */
typedef c_i64 gf[16];
static const gf gf_121665 = {0xDB41, 1};
static void c25519(gf o) {
    c_i64 c;
    for (int i = 0; i < 16; i++) {
        o[i] += (1LL << 16);
        c = o[i] >> 16;
        o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
        o[i] -= c * 65536;
    }
}
static void sel25519(gf p, gf q, int b) {
    c_i64 t, c = ~(c_i64)(b - 1);
    for (int i = 0; i < 16; i++) { t = c & (p[i] ^ q[i]); p[i] ^= t; q[i] ^= t; }
}
static void pack25519(u8 *o, const gf n) {
    int b; gf m, t;
    for (int i = 0; i < 16; i++) t[i] = n[i];
    c25519(t); c25519(t); c25519(t);
    for (int j = 0; j < 2; j++) {
        m[0] = t[0] - 0xffed;
        for (int i = 1; i < 15; i++) { m[i] = t[i] - 0xffff - ((m[i-1] >> 16) & 1); m[i-1] &= 0xffff; }
        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
        b = (m[15] >> 16) & 1;
        m[14] &= 0xffff;
        sel25519(t, m, 1 - b);
    }
    for (int i = 0; i < 16; i++) { o[2*i] = t[i] & 0xff; o[2*i+1] = t[i] >> 8; }
}
static void unpack25519(gf o, const u8 *n) {
    for (int i = 0; i < 16; i++) o[i] = n[2*i] + ((c_i64)n[2*i+1] << 8);
    o[15] &= 0x7fff;
}
static void gA(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] + b[i]; }
static void gZ(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] - b[i]; }
static void gM(gf o, const gf a, const gf b) {
    c_i64 t[31];
    for (int i = 0; i < 31; i++) t[i] = 0;
    for (int i = 0; i < 16; i++) for (int j = 0; j < 16; j++) t[i + j] += a[i] * b[j];
    for (int i = 0; i < 15; i++) t[i] += 38 * t[i + 16];
    for (int i = 0; i < 16; i++) o[i] = t[i];
    c25519(o); c25519(o);
}
static void gS(gf o, const gf a) { gM(o, a, a); }
static void inv25519(gf o, const gf i) {
    gf c;
    for (int a = 0; a < 16; a++) c[a] = i[a];
    for (int a = 253; a >= 0; a--) { gS(c, c); if (a != 2 && a != 4) gM(c, c, i); }
    for (int a = 0; a < 16; a++) o[a] = c[a];
}
static void x25519(u8 *q, const u8 *n, const u8 *p) {
    u8 z[32]; c_i64 x[80], r; gf a, b, c, d, e, f;
    for (int i = 0; i < 31; i++) z[i] = n[i];
    z[31] = (n[31] & 127) | 64; z[0] &= 248;
    unpack25519(x, p);
    for (int i = 0; i < 16; i++) { b[i] = x[i]; d[i] = a[i] = c[i] = 0; }
    a[0] = d[0] = 1;
    for (int i = 254; i >= 0; --i) {
        r = (z[i >> 3] >> (i & 7)) & 1;
        sel25519(a, b, (int)r); sel25519(c, d, (int)r);
        gA(e, a, c); gZ(a, a, c); gA(c, b, d); gZ(b, b, d);
        gS(d, e); gS(f, a); gM(a, c, a); gM(c, b, e);
        gA(e, a, c); gZ(a, a, c); gS(b, a); gZ(c, d, f);
        gM(a, c, gf_121665); gA(a, a, d); gM(c, c, a); gM(a, d, f);
        gM(d, b, x); gS(b, e);
        sel25519(a, b, (int)r); sel25519(c, d, (int)r);
    }
    for (int i = 0; i < 16; i++) { x[i + 16] = a[i]; x[i + 32] = c[i]; x[i + 48] = b[i]; x[i + 64] = d[i]; }
    inv25519(x + 32, x + 32);
    gM(x + 16, x + 16, x + 32);
    pack25519(q, x + 16);
}
#endif
