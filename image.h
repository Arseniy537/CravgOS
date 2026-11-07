/* image.h - декодеры PNG / JPEG / GIF для текстового режима.
 * Картинка сразу уменьшается до сетки im_ow x im_oh "пикселей" (один пиксель = половинка знакоместа),
 * поэтому большие кадры не хранятся целиком. Вывод: символ 0xDF (верхняя половинка), цвет и фон из 16 цветов VGA. */
#ifndef IMAGE_H
#define IMAGE_H
#ifndef XBSS
#define XBSS
#endif

#define IM_MAXW 78
#define IM_MAXH (IMG_ROWS * 2)
static u32 im_r[IM_MAXW * IM_MAXH] XBSS, im_g[IM_MAXW * IM_MAXH] XBSS, im_b[IM_MAXW * IM_MAXH] XBSS;
static u16 im_n[IM_MAXW * IM_MAXH] XBSS;
static int im_w, im_h, im_ow, im_oh;
static u16 im_map_x[4096] XBSS;
static u16 im_map_y[4096] XBSS;
static u8 pal_lut[16][16][16] XBSS;
static int pal_lut_ready;

static int im_setup(int w, int h) {
    if (w < 1 || h < 1 || w > 16384 || h > 16384) return -2;
    im_w = w; im_h = h;
    int ow = w < IM_MAXW ? w : IM_MAXW;
    int oh = (int)((u32)h * ow / w);
    if (oh > IM_MAXH) { oh = IM_MAXH; ow = (int)((u32)w * oh / h); }
    if (ow < 1) ow = 1;
    if (oh < 1) oh = 1;
    im_ow = ow; im_oh = oh;
    int lim_w = w < 4096 ? w : 4096;
    for (int x = 0; x < lim_w; x++) im_map_x[x] = (u16)((u32)x * ow / w);
    int lim_h = h < 4096 ? h : 4096;
    for (int y = 0; y < lim_h; y++) im_map_y[y] = (u16)((u32)y * oh / h);
    for (int i = 0; i < ow * oh; i++) { im_r[i] = im_g[i] = im_b[i] = 0; im_n[i] = 0; }
    return 0;
}
static inline void im_px(int x, int y, int r, int g, int b) {
    if ((u32)x >= (u32)im_w || (u32)y >= (u32)im_h) return;
    int mx = (x < 4096) ? im_map_x[x] : (int)((u32)x * im_ow / im_w);
    int my = (y < 4096) ? im_map_y[y] : (int)((u32)y * im_oh / im_h);
    int i = my * im_ow + mx;
    im_r[i] += r; im_g[i] += g; im_b[i] += b;
    if (im_n[i] < 65535) im_n[i]++;
}

/* ================= inflate (zlib), потоковый ================= */
struct ii_huff { u16 count[16]; u16 sym[288]; };
static const u8 *ii_d; static int ii_len, ii_pos, ii_cend;   /* поток IDAT: данные, длина, позиция, конец текущего чанка */
static u32 ii_bb; static int ii_bc, ii_err;
static u8 ii_win[32768] XBSS; static u32 ii_wp;
static void (*ii_sink)(u8);
static int ii_stop;

static int ii_getbyte(void) {
    while (ii_pos >= ii_cend) {
        int p = ii_cend + 4;                               /* CRC */
        if (ii_cend < 0 || p + 8 > ii_len) return -1;
        u32 l = ((u32)ii_d[p] << 24) | ((u32)ii_d[p + 1] << 16) | ((u32)ii_d[p + 2] << 8) | ii_d[p + 3];
        if (ii_d[p + 4] != 'I' || ii_d[p + 5] != 'D' || ii_d[p + 6] != 'A' || ii_d[p + 7] != 'T') return -1;
        ii_pos = p + 8;
        ii_cend = (l > (u32)(ii_len - ii_pos)) ? ii_len : ii_pos + (int)l;
        if (ii_pos >= ii_len) return -1;
    }
    return ii_d[ii_pos++];
}
static int ii_bits(int need) {
    u32 v = ii_bb;
    while (ii_bc < need) {
        int b = ii_getbyte();
        if (b < 0) { ii_err = 1; return 0; }
        v |= (u32)b << ii_bc; ii_bc += 8;
    }
    ii_bb = v >> need; ii_bc -= need;
    return (int)(v & ((1u << need) - 1));
}
static int ii_build(struct ii_huff *h, const u16 *len, int n) {
    for (int l = 0; l < 16; l++) h->count[l] = 0;
    for (int s = 0; s < n; s++) h->count[len[s]]++;
    if (h->count[0] == n) return 0;
    int left = 1;
    for (int l = 1; l < 16; l++) { left <<= 1; left -= h->count[l]; if (left < 0) return left; }
    u16 offs[16]; offs[1] = 0;
    for (int l = 1; l < 15; l++) offs[l + 1] = offs[l] + h->count[l];
    for (int s = 0; s < n; s++) if (len[s]) h->sym[offs[len[s]]++] = s;
    return left;
}
static int ii_decode(const struct ii_huff *h) {
    int code = 0, first = 0, index = 0;
    for (int l = 1; l < 16; l++) {
        code |= ii_bits(1);
        int c = h->count[l];
        if (code - c < first) return h->sym[index + (code - first)];
        index += c; first += c; first <<= 1; code <<= 1;
        if (ii_err) return -1;
    }
    return -1;
}
static inline void ii_out(u8 c) { ii_win[ii_wp & 32767] = c; ii_wp++; ii_sink(c); }
static const u16 ii_lb[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const u16 ii_le[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const u16 ii_db[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const u16 ii_de[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
static struct ii_huff ii_lc, ii_dc;
static int ii_codes(void) {
    for (;;) {
        int s = ii_decode(&ii_lc);
        if (s < 0 || ii_err) return -1;
        if (s < 256) { ii_out((u8)s); if (ii_stop) return 0; }
        else if (s == 256) return 0;
        else {
            s -= 257; if (s >= 29) return -1;
            int len = ii_lb[s] + ii_bits(ii_le[s]);
            int ds = ii_decode(&ii_dc); if (ds < 0 || ds >= 30) return -1;
            int dist = ii_db[ds] + ii_bits(ii_de[ds]);
            if ((u32)dist > ii_wp || ii_err) return -1;
            while (len--) { ii_out(ii_win[(ii_wp - dist) & 32767]); if (ii_stop) return 0; }
        }
    }
}
static int ii_fixed(void) {
    u16 l[288]; int i;
    for (i = 0; i < 144; i++) l[i] = 8;
    for (; i < 256; i++) l[i] = 9;
    for (; i < 280; i++) l[i] = 7;
    for (; i < 288; i++) l[i] = 8;
    ii_build(&ii_lc, l, 288);
    for (i = 0; i < 30; i++) l[i] = 5;
    ii_build(&ii_dc, l, 30);
    return ii_codes();
}
static int ii_dynamic(void) {
    static const u8 ord[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    u16 l[320];
    int nl = ii_bits(5) + 257, nd = ii_bits(5) + 1, nc = ii_bits(4) + 4;
    if (ii_err || nl > 286 || nd > 30) return -1;
    int i;
    for (i = 0; i < 19; i++) l[i] = 0;
    for (i = 0; i < nc; i++) l[ord[i]] = ii_bits(3);
    struct ii_huff cl;
    if (ii_build(&cl, l, 19) != 0) return -1;
    i = 0;
    while (i < nl + nd) {
        int s = ii_decode(&cl);
        if (s < 0 || ii_err) return -1;
        if (s < 16) l[i++] = s;
        else {
            int prev = 0, rep;
            if (s == 16) { if (!i) return -1; prev = l[i - 1]; rep = 3 + ii_bits(2); }
            else if (s == 17) rep = 3 + ii_bits(3);
            else rep = 11 + ii_bits(7);
            if (i + rep > nl + nd) return -1;
            while (rep--) l[i++] = prev;
        }
    }
    if (l[256] == 0) return -1;
    ii_build(&ii_lc, l, nl);
    ii_build(&ii_dc, l + nl, nd);
    return ii_codes();
}
/* d - начало потока IDAT внутри данных PNG (после заголовка чанка), cend - конец первого чанка */
static int ii_run(const u8 *d, int len, int pos, int cend, void (*sink)(u8)) {
    ii_d = d; ii_len = len; ii_pos = pos; ii_cend = cend; ii_bb = 0; ii_bc = 0; ii_err = 0; ii_wp = 0; ii_stop = 0; ii_sink = sink;
    ii_getbyte(); ii_getbyte();                               /* заголовок zlib */
    for (;;) {
        int last = ii_bits(1), type = ii_bits(2), r;
        if (ii_err) return ii_stop ? 0 : -1;
        if (type == 0) {
            ii_bb = 0; ii_bc = 0;
            int a = ii_getbyte(), b = ii_getbyte(), c = ii_getbyte(), e = ii_getbyte();
            if (e < 0) return -1;
            int n = a | (b << 8);
            if ((n ^ 0xFFFF) != (c | (e << 8))) return -1;
            while (n--) { int x = ii_getbyte(); if (x < 0) return ii_stop ? 0 : -1; ii_out((u8)x); if (ii_stop) return 0; }
            r = 0;
        } else if (type == 1) r = ii_fixed();
        else if (type == 2) r = ii_dynamic();
        else return -1;
        if (ii_stop) return 0;
        if (r < 0) return -1;
        if (last) return 0;
    }
}

/* ================= PNG ================= */
static int pn_w, pn_h, pn_depth, pn_ct, pn_inter, pn_chan, pn_bpp, pn_plen;
static u8 pn_pal[256][3];
static u8 pn_row[2][65536 + 8] XBSS, pn_raw[65536 + 8] XBSS;
static int pn_pass, pn_y, pn_pw, pn_ph, pn_rb, pn_rp, pn_cur, pn_x0, pn_dx, pn_y0, pn_dy, pn_done;
static const u8 pn_ax0[7] = {0,4,0,2,0,1,0}, pn_ay0[7] = {0,0,4,0,2,0,1}, pn_adx[7] = {8,8,4,4,2,2,1}, pn_ady[7] = {8,8,8,4,4,2,2};

static int pn_start_pass(void) {
    for (;;) {
        if (!pn_inter) { if (pn_pass > 0) return 0; pn_x0 = 0; pn_y0 = 0; pn_dx = 1; pn_dy = 1; pn_pw = pn_w; pn_ph = pn_h; }
        else {
            if (pn_pass > 6) return 0;
            pn_x0 = pn_ax0[pn_pass]; pn_y0 = pn_ay0[pn_pass]; pn_dx = pn_adx[pn_pass]; pn_dy = pn_ady[pn_pass];
            pn_pw = (pn_w - pn_x0 + pn_dx - 1) / pn_dx; pn_ph = (pn_h - pn_y0 + pn_dy - 1) / pn_dy;
            if (pn_w <= pn_x0) pn_pw = 0;
            if (pn_h <= pn_y0) pn_ph = 0;
        }
        if (pn_pw > 0 && pn_ph > 0) break;
        pn_pass++;
    }
    pn_rb = (pn_pw * pn_chan * pn_depth + 7) / 8;
    for (int i = 0; i < pn_rb + 8; i++) pn_row[0][i] = pn_row[1][i] = 0;
    pn_cur = 0; pn_y = 0; pn_rp = 0;
    return 1;
}
static void pn_emit(const u8 *r, int y) {
    int oy = pn_y0 + y * pn_dy;
    for (int i = 0; i < pn_pw; i++) {
        int R, G, B, A = 255;
        if (pn_depth < 8) {
            int bit = i * pn_depth, sh = 8 - pn_depth - (bit & 7), mask = (1 << pn_depth) - 1;
            int v = (r[bit >> 3] >> sh) & mask;
            if (pn_ct == 3) { if (v >= pn_plen) v = 0; R = pn_pal[v][0]; G = pn_pal[v][1]; B = pn_pal[v][2]; }
            else R = G = B = v * 255 / mask;
        } else {
            int sb = pn_depth / 8;                                  /* байт на отсчёт: берём старший */
            const u8 *p = r + i * pn_chan * sb;
            if (pn_ct == 0) R = G = B = p[0];
            else if (pn_ct == 2) { R = p[0]; G = p[sb]; B = p[2 * sb]; }
            else if (pn_ct == 3) { int v = p[0]; if (v >= pn_plen) v = 0; R = pn_pal[v][0]; G = pn_pal[v][1]; B = pn_pal[v][2]; }
            else if (pn_ct == 4) { R = G = B = p[0]; A = p[sb]; }
            else { R = p[0]; G = p[sb]; B = p[2 * sb]; A = p[3 * sb]; }
        }
        if (A < 255) { R = (R * A + 128 * (255 - A)) / 255; G = (G * A + 128 * (255 - A)) / 255; B = (B * A + 128 * (255 - A)) / 255; }
        im_px(pn_x0 + i * pn_dx, oy, R, G, B);
    }
}
static void pn_sink(u8 b) {
    if (pn_done) return;
    pn_raw[pn_rp++] = b;
    if (pn_rp < pn_rb + 1) return;
    int ft = pn_raw[0];
    u8 *cur = pn_row[pn_cur], *prv = pn_row[pn_cur ^ 1];
    const u8 *s = pn_raw + 1;
    int bpp = pn_bpp;
    if (ft == 0) {
        for (int i = 0; i < pn_rb; i++) cur[i] = s[i];
    } else {
        for (int i = 0; i < pn_rb; i++) {
            int a = i >= bpp ? cur[i - bpp] : 0, up = prv[i], c = i >= bpp ? prv[i - bpp] : 0, x = s[i];
            switch (ft) {
                case 1: x += a; break;
                case 2: x += up; break;
                case 3: x += (a + up) >> 1; break;
                case 4: { int p = a + up - c, pa = p > a ? p - a : a - p, pb = p > up ? p - up : up - p, pc = p > c ? p - c : c - p;
                          x += (pa <= pb && pa <= pc) ? a : (pb <= pc ? up : c); break; }
            }
            cur[i] = (u8)x;
        }
    }
    pn_emit(cur, pn_y);
    pn_cur ^= 1; pn_rp = 0;
    if (++pn_y >= pn_ph) { pn_pass++; if (!pn_start_pass()) { pn_done = 1; ii_stop = 1; } }
}
static int png_decode(const u8 *d, int len) {
    if (len < 33 || d[12] != 'I' || d[13] != 'H' || d[14] != 'D' || d[15] != 'R') return -2;
    pn_w = ((u32)d[16] << 24) | (d[17] << 16) | (d[18] << 8) | d[19];
    pn_h = ((u32)d[20] << 24) | (d[21] << 16) | (d[22] << 8) | d[23];
    pn_depth = d[24]; pn_ct = d[25]; pn_inter = d[28];
    if (d[26] != 0 || d[27] != 0 || pn_inter > 1) return -1;
    switch (pn_ct) { case 0: pn_chan = 1; break; case 2: pn_chan = 3; break; case 3: pn_chan = 1; break; case 4: pn_chan = 2; break; case 6: pn_chan = 4; break; default: return -1; }
    if (pn_depth != 1 && pn_depth != 2 && pn_depth != 4 && pn_depth != 8 && pn_depth != 16) return -1;
    if (pn_w < 1 || pn_h < 1 || pn_w > 8192 || (pn_w * pn_chan * pn_depth + 7) / 8 > 65536) return -3;
    if (pn_depth < 8 && pn_ct != 0 && pn_ct != 3) return -1;
    pn_bpp = pn_chan * pn_depth / 8; if (pn_bpp < 1) pn_bpp = 1;
    pn_plen = 0;
    int p = 33, idat = -1;
    while (p + 8 <= len) {
        u32 l = ((u32)d[p] << 24) | ((u32)d[p + 1] << 16) | ((u32)d[p + 2] << 8) | d[p + 3];
        if (d[p + 4] == 'P' && d[p + 5] == 'L' && d[p + 6] == 'T' && d[p + 7] == 'E') {
            int n = l / 3; if (n > 256) n = 256;
            for (int i = 0; i < n && p + 8 + i * 3 + 2 < len; i++) { pn_pal[i][0] = d[p + 8 + i * 3]; pn_pal[i][1] = d[p + 9 + i * 3]; pn_pal[i][2] = d[p + 10 + i * 3]; pn_plen = i + 1; }
        } else if (d[p + 4] == 'I' && d[p + 5] == 'D' && d[p + 6] == 'A' && d[p + 7] == 'T') { idat = p; break; }
        if (l > 0x7FFFFFFF || (u32)p + 12 + l > (u32)len) return -2;
        p += 12 + (int)l;
    }
    if (idat < 0) return -2;
    if (pn_ct == 3 && !pn_plen) return -2;
    int r = im_setup(pn_w, pn_h);
    if (r < 0) return r;
    pn_pass = 0; pn_done = 0;
    if (!pn_start_pass()) return -2;
    u32 l = ((u32)d[idat] << 24) | ((u32)d[idat + 1] << 16) | ((u32)d[idat + 2] << 8) | d[idat + 3];
    int st = idat + 8, ce = (l > (u32)(len - st)) ? len : st + (int)l;
    ii_run(d, len, st, ce, pn_sink);
    return pn_done || pn_y > 0 || pn_pass > 0 ? 0 : -2;
}

/* ================= JPEG (baseline полностью, progressive - только по DC) ================= */
struct ij_huff { u8 count[17]; u8 sym[256]; };
static struct ij_huff ij_dcT[4], ij_acT[4];
static u16 ij_q[4][64];
static const u8 ij_zz[64] = {0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
                             35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63};

static const short ij_cs[8][8] = {{1448,2009,1892,1703,1448,1138,784,400},{1448,1703,784,-400,-1448,-2009,-1892,-1138},
    {1448,1138,-784,-2009,-1448,400,1892,1703},{1448,400,-1892,-1138,1448,1703,-784,-2009},
    {1448,-400,-1892,1138,1448,-1703,-784,2009},{1448,-1138,-784,2009,-1448,-400,1892,-1703},
    {1448,-1703,784,400,-1448,2009,-1892,1138},{1448,-2009,1892,-1703,1448,-1138,784,-400}};
static const u8 *ij_p, *ij_end;
static u32 ij_cur; static int ij_cnt, ij_mark, ij_eof;
static int ij_w, ij_h, ij_nc, ij_prog, ij_dri, ij_hmax, ij_vmax, ij_mx, ij_my, ij_sofd;
static struct { int id, h, v, tq, td, ta, pred, stride, cbw, cbh; } ij_c[3];
static short ij_dcs[3][131072] XBSS;
static u8 ij_pl[3][16 * 16];
static int ij_eobrun;

static int ij_bit(void) {
    if (!ij_cnt) {
        int b = 0;
        if (ij_p < ij_end && !ij_mark) {
            b = *ij_p;
            if (b == 0xFF) {
                int n = (ij_p + 1 < ij_end) ? ij_p[1] : 0xD9;
                if (n == 0) ij_p += 2; else { ij_mark = 1; b = 0; }
            } else ij_p++;
        } else ij_eof = 1;
        ij_cur = (u32)b; ij_cnt = 8;
    }
    ij_cnt--;
    return (ij_cur >> ij_cnt) & 1;
}
static int ij_recv(int n) { int v = 0; while (n--) v = (v << 1) | ij_bit(); return v; }
static int ij_ext(int v, int n) { return (n && v < (1 << (n - 1))) ? v - (1 << n) + 1 : v; }
static int ij_huffd(const struct ij_huff *h) {
    int code = 0, first = 0, index = 0;
    for (int l = 1; l <= 16; l++) {
        code |= ij_bit();
        int c = h->count[l];
        if (code - c < first) return h->sym[index + (code - first)];
        index += c; first += c; first <<= 1; code <<= 1;
    }
    return -1;
}
/* блок: DC -> *dc (после умножения на квантователь), AC -> F (если F != 0) */
static int ij_block(int ci, short *F, int *dcout, int skipac) {
    int t = ij_huffd(&ij_dcT[ij_c[ci].td]);
    if (t < 0 || t > 16) return -1;
    int diff = t ? ij_ext(ij_recv(t), t) : 0;
    ij_c[ci].pred += diff;
    const u16 *q = ij_q[ij_c[ci].tq];
    *dcout = ij_c[ci].pred * q[0];
    if (F) F[0] = (short)(ij_c[ci].pred * q[0]);
    for (int k = 1; k < 64; k++) {
        int rs = ij_huffd(&ij_acT[ij_c[ci].ta]);
        if (rs < 0) return -1;
        int r = rs >> 4, s = rs & 15;
        if (!s) { if (r < 15) break; k += 15; continue; }
        k += r;
        if (k > 63) break;
        int v = ij_ext(ij_recv(s), s);
        if (F && !skipac) F[ij_zz[k]] = (short)(v * q[k]);
    }
    return 0;
}
static void ij_idct(const short *F, u8 *out, int stride) {
    int has_ac = 0;
    for (int i = 1; i < 64; i++) { if (F[i]) { has_ac = 1; break; } }
    if (!has_ac) {
        int s = ((int)F[0] + 4) >> 3;
        s += 128;
        u8 val = (u8)(s < 0 ? 0 : s > 255 ? 255 : s);
        for (int y = 0; y < 8; y++) {
            u8 *row = out + y * stride;
            for (int x = 0; x < 8; x++) row[x] = val;
        }
        return;
    }
    int tmp[64];
    for (int v = 0; v < 8; v++)
        for (int x = 0; x < 8; x++) {
            int s = 0;
            for (int u = 0; u < 8; u++) s += ij_cs[x][u] * F[v * 8 + u];
            tmp[v * 8 + x] = s >> 12;
        }
    for (int x = 0; x < 8; x++)
        for (int y = 0; y < 8; y++) {
            int s = 0;
            for (int v = 0; v < 8; v++) s += ij_cs[y][v] * tmp[v * 8 + x];
            s = (s >> 12) + 128;
            out[y * stride + x] = (u8)(s < 0 ? 0 : s > 255 ? 255 : s);
        }
}
static inline int ij_cl(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
static void ij_color(int x, int y, int Y, int cb, int cr) {
    if (ij_nc == 1) { im_px(x, y, Y, Y, Y); return; }
    cb -= 128; cr -= 128;
    im_px(x, y, ij_cl(Y + ((91881 * cr) >> 16)), ij_cl(Y - ((22554 * cb + 46802 * cr) >> 16)), ij_cl(Y + ((116130 * cb) >> 16)));
}
static int ij_restart(void) {
    ij_cnt = 0; ij_mark = 0;
    while (ij_p + 1 < ij_end) {
        if (ij_p[0] == 0xFF && ij_p[1] >= 0xD0 && ij_p[1] <= 0xD7) { ij_p += 2; for (int i = 0; i < 3; i++) ij_c[i].pred = 0; ij_eobrun = 0; return 0; }
        if (ij_p[0] == 0xFF && ij_p[1] != 0 && ij_p[1] != 0xFF) return -1;
        ij_p++;
    }
    return -1;
}
/* разбор одного скана. nsc/idx - компоненты скана. mode: 0 - полный baseline, 1 - только DC в массивы */
static int ij_scan(int nsc, const int *idx, int mode, int al) {
    for (int i = 0; i < 3; i++) ij_c[i].pred = 0;
    ij_cnt = 0; ij_mark = 0; ij_eof = 0;
    int total, mcuw;
    if (nsc > 1) { total = ij_mx * ij_my; mcuw = ij_mx; }
    else { total = ij_c[idx[0]].cbw * ij_c[idx[0]].cbh; mcuw = ij_c[idx[0]].cbw; }
    for (int m = 0; m < total; m++) {
        if (ij_dri && m && m % ij_dri == 0 && ij_restart() < 0) return -1;
        if (ij_eof) return 0;                                  /* данные оборваны - показываем то, что успели */
        int mx = m % mcuw, my = m / mcuw;
        if (mode == 0) {
            for (int s = 0; s < nsc; s++) {
                int ci = idx[s];
                for (int vy = 0; vy < ij_c[ci].v; vy++)
                    for (int hx = 0; hx < ij_c[ci].h; hx++) {
                        short F[64]; int dc;
                        for (int i = 0; i < 64; i++) F[i] = 0;
                        if (ij_block(ci, F, &dc, 0) < 0) return -1;
                        ij_idct(F, &ij_pl[ci][vy * 8 * 16 + hx * 8], 16);
                    }
            }
            for (int py = 0; py < 8 * ij_vmax; py++)
                for (int px = 0; px < 8 * ij_hmax; px++) {
                    int X = mx * 8 * ij_hmax + px, Y = my * 8 * ij_vmax + py;
                    if (X >= ij_w || Y >= ij_h) continue;
                    int yy = ij_pl[0][(py * ij_c[0].v / ij_vmax) * 16 + px * ij_c[0].h / ij_hmax], cb = 128, cr = 128;
                    if (ij_nc == 3) {
                        cb = ij_pl[1][(py * ij_c[1].v / ij_vmax) * 16 + px * ij_c[1].h / ij_hmax];
                        cr = ij_pl[2][(py * ij_c[2].v / ij_vmax) * 16 + px * ij_c[2].h / ij_hmax];
                    }
                    ij_color(X, Y, yy, cb, cr);
                }
        } else {
            for (int s = 0; s < nsc; s++) {
                int ci = idx[s];
                int bh = nsc > 1 ? ij_c[ci].v : 1, bw = nsc > 1 ? ij_c[ci].h : 1;
                for (int vy = 0; vy < bh; vy++)
                    for (int hx = 0; hx < bw; hx++) {
                        int dc, bx = mx * bw + hx, by = my * bh + vy;
                        if (ij_prog) {
                            int t = ij_huffd(&ij_dcT[ij_c[ci].td]);
                            if (t < 0 || t > 16) return -1;
                            ij_c[ci].pred += t ? ij_ext(ij_recv(t), t) : 0;
                            dc = (ij_c[ci].pred * (1 << al)) * ij_q[ij_c[ci].tq][0];
                        } else if (ij_block(ci, 0, &dc, 1) < 0) return -1;
                        long ix = (long)by * ij_c[ci].stride + bx;
                        if (ix < 131072) ij_dcs[ci][ix] = (short)(dc > 32767 ? 32767 : dc < -32768 ? -32768 : dc);
                    }
            }
        }
    }
    return 0;
}
static void ij_render_dc(int st) {
    int bwp = 8 * ij_hmax / ij_c[0].h, bhp = 8 * ij_vmax / ij_c[0].v;      /* пикселей на блок яркости */
    for (int by = 0; by < ij_c[0].cbh; by++)
        for (int bx = 0; bx < ij_c[0].cbw; bx++) {
            int Y = 128 + ij_dcs[0][(long)by * ij_c[0].stride + bx] / 8, cb = 128, cr = 128;
            int x0 = bx * bwp, y0 = by * bhp;
            if (ij_nc == 3) {
                for (int c = 1; c < 3; c++) {
                    int cx = (x0 + bwp / 2) * ij_c[c].h / (8 * ij_hmax), cy = (y0 + bhp / 2) * ij_c[c].v / (8 * ij_vmax);
                    long ix = (long)cy * ij_c[c].stride + cx;
                    int v = 128 + (ix < 131072 ? ij_dcs[c][ix] : 0) / 8;
                    if (c == 1) cb = v; else cr = v;
                }
            }
            Y = ij_cl(Y); cb = ij_cl(cb); cr = ij_cl(cr);
            for (int py = 0; py < bhp; py += st)
                for (int px = 0; px < bwp; px += st)
                    ij_color(x0 + px + (st > 1 ? st / 2 : 0), y0 + py + (st > 1 ? st / 2 : 0), Y, cb, cr);
        }
}
static int jpeg_decode(const u8 *d, int len) {
    int pos = 2, got = 0, dcmode = 0, dcdone = 0, st = 1, sof = 0;
    ij_dri = 0; ij_end = d + len; ij_prog = 0;
    for (int i = 0; i < 3; i++) { ij_c[i].td = ij_c[i].ta = ij_c[i].tq = 0; }
    for (int i = 0; i < 4; i++) for (int k = 0; k < 64; k++) ij_q[i][k] = 1;
    while (pos + 4 <= len) {
        if (d[pos] != 0xFF) { pos++; continue; }
        int m = d[pos + 1];
        if (m == 0xFF) { pos++; continue; }
        if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7) || m == 0) { pos += 2; continue; }
        if (m == 0xD9) break;
        int L = (d[pos + 2] << 8) | d[pos + 3];
        const u8 *s = d + pos + 4; int sl = L - 2;
        if (L < 2 || pos + 2 + L > len) { if (m == 0xDA && sof) { /* обрезанный скан - всё равно пробуем */ } else break; }
        if (m == 0xDB) {
            while (sl >= 65) {
                int pq = s[0] >> 4, tq = s[0] & 15;
                if (tq > 3 || pq) { if (pq) return -1; break; }
                for (int k = 0; k < 64; k++) ij_q[tq][k] = s[1 + k];
                s += 65; sl -= 65;
            }
        } else if (m == 0xC4) {
            while (sl >= 17) {
                int tc = s[0] >> 4, th = s[0] & 15, n = 0;
                if (th > 3) break;
                struct ij_huff *h = tc ? &ij_acT[th] : &ij_dcT[th];
                h->count[0] = 0;
                for (int i = 1; i <= 16; i++) { h->count[i] = s[i]; n += s[i]; }
                if (n > 256 || 17 + n > sl) break;
                for (int i = 0; i < n; i++) h->sym[i] = s[17 + i];
                s += 17 + n; sl -= 17 + n;
            }
        } else if (m == 0xC0 || m == 0xC1 || m == 0xC2) {
            if (sl < 6 || s[0] != 8) return -1;
            ij_prog = (m == 0xC2); ij_h = (s[1] << 8) | s[2]; ij_w = (s[3] << 8) | s[4]; ij_nc = s[5];
            if ((ij_nc != 1 && ij_nc != 3) || sl < 6 + 3 * ij_nc) return -1;
            ij_hmax = ij_vmax = 1;
            for (int i = 0; i < ij_nc; i++) {
                ij_c[i].id = s[6 + i * 3]; ij_c[i].h = s[7 + i * 3] >> 4; ij_c[i].v = s[7 + i * 3] & 15; ij_c[i].tq = s[8 + i * 3] & 3;
                if (ij_c[i].h < 1 || ij_c[i].h > 2 || ij_c[i].v < 1 || ij_c[i].v > 2) return -1;
                if (ij_c[i].h > ij_hmax) ij_hmax = ij_c[i].h;
                if (ij_c[i].v > ij_vmax) ij_vmax = ij_c[i].v;
            }
            int r = im_setup(ij_w, ij_h); if (r < 0) return r;
            ij_mx = (ij_w + 8 * ij_hmax - 1) / (8 * ij_hmax); ij_my = (ij_h + 8 * ij_vmax - 1) / (8 * ij_vmax);
            for (int i = 0; i < ij_nc; i++) {
                ij_c[i].stride = ij_mx * ij_c[i].h;
                ij_c[i].cbw = ((ij_w * ij_c[i].h + ij_hmax - 1) / ij_hmax + 7) / 8;
                ij_c[i].cbh = ((ij_h * ij_c[i].v + ij_vmax - 1) / ij_vmax + 7) / 8;
                if (ij_c[i].stride * ij_my * ij_c[i].v > 131072) return -3;
                if (ij_nc == 1) { ij_c[i].stride = ij_c[i].cbw; }
            }
            if (ij_nc == 1) { ij_hmax = ij_vmax = 1; ij_c[0].h = ij_c[0].v = 1; ij_mx = ij_c[0].cbw; ij_my = ij_c[0].cbh; ij_c[0].stride = ij_mx; }
            sof = 1;
            st = (ij_w >= im_ow * 8 && ij_h >= im_oh * 8) ? 8 : 1;
            dcmode = ij_prog || st == 8;
            for (int i = 0; i < 3; i++) for (long k = 0; k < 131072 && dcmode; k += 4096) { long e = k + 4096 > 131072 ? 131072 : k + 4096; for (long j = k; j < e; j++) ij_dcs[i][j] = 0; }
        } else if ((m >= 0xC3 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC)) {
            return -1;                                                      /* арифметическое / lossless */
        } else if (m == 0xDD) { if (sl >= 2) ij_dri = (s[0] << 8) | s[1]; }
        else if (m == 0xDA) {
            if (!sof) return -2;
            int ns = s[0], idx[3];
            if (ns < 1 || ns > ij_nc || sl < 1 + ns * 2 + 3) return -2;
            for (int i = 0; i < ns; i++) {
                int cid = s[1 + i * 2], tb = s[2 + i * 2], c = -1;
                for (int k = 0; k < ij_nc; k++) if (ij_c[k].id == cid) c = k;
                if (c < 0) return -2;
                ij_c[c].td = (tb >> 4) & 3; ij_c[c].ta = tb & 3; idx[i] = c;
            }
            int ss = s[1 + ns * 2], ah = s[3 + ns * 2] >> 4, al = s[3 + ns * 2] & 15;
            ij_p = d + pos + 2 + L;
            if (pos + 2 + L > len) ij_p = ij_end;
            int dcscan = !ij_prog || (ss == 0 && ah == 0);
            if (dcscan && !dcdone) {
                if (!dcmode && ns != ij_nc) return -1;
                int r = ij_scan(ns, idx, dcmode ? 1 : 0, al);
                if (r < 0 && !got) return r;
                got = 1;
                if (!ij_prog) { if (dcmode) ij_render_dc(st); return 0; }
                if (ns == ij_nc) dcdone = 1;
                else { int all = 1; (void)all; }
            }
            if (!ij_prog && !dcmode) return 0;
            /* пропускаем данные скана до следующего маркера */
            while (ij_p + 1 < ij_end && !(ij_p[0] == 0xFF && ij_p[1] != 0 && !(ij_p[1] >= 0xD0 && ij_p[1] <= 0xD7) && ij_p[1] != 0xFF)) ij_p++;
            pos = (int)(ij_p - d);
            if (ij_prog && got && dcdone) { ij_render_dc(st); return 0; }
            continue;
        }
        pos += 2 + L;
    }
    if (ij_prog && got) { ij_render_dc(st); return 0; }
    return got ? 0 : -2;
}

/* ================= GIF (первый кадр) ================= */
static u16 ig_pre[4096]; static u8 ig_suf[4096], ig_stk[4097];
static const u8 *ig_d; static int ig_len, ig_p, ig_rem;
static int ig_byte(void) {
    if (ig_rem == 0) { if (ig_p >= ig_len) return -1; ig_rem = ig_d[ig_p++]; if (!ig_rem) return -1; }
    if (ig_p >= ig_len) return -1;
    ig_rem--; return ig_d[ig_p++];
}
static int gif_decode(const u8 *d, int len) {
    if (len < 14) return -2;
    int sw = d[6] | (d[7] << 8), sh = d[8] | (d[9] << 8), fl = d[10], p = 13;
    const u8 *gct = 0; int gn = 0;
    if (fl & 0x80) { gn = 2 << (fl & 7); gct = d + p; p += gn * 3; }
    while (p < len) {
        int b = d[p++];
        if (b == 0x21) { p++; while (p < len && d[p]) p += d[p] + 1; p++; continue; }
        if (b != 0x2C) return -2;
        if (p + 9 > len) return -2;
        int ix = d[p] | (d[p + 1] << 8), iy = d[p + 2] | (d[p + 3] << 8), iw = d[p + 4] | (d[p + 5] << 8), ih = d[p + 6] | (d[p + 7] << 8), lf = d[p + 8];
        p += 9;
        const u8 *pal = gct; int pn = gn;
        if (lf & 0x80) { pn = 2 << (lf & 7); pal = d + p; p += pn * 3; }
        if (!pal || iw < 1 || ih < 1 || p + 1 > len) return -2;
        if (sw < ix + iw) sw = ix + iw;
        if (sh < iy + ih) sh = iy + ih;
        int r = im_setup(sw, sh); if (r < 0) return r;
        int mincs = d[p++];
        if (mincs < 2 || mincs > 11) return -2;
        ig_d = d; ig_len = len; ig_p = p; ig_rem = 0;
        int clear = 1 << mincs, eoi = clear + 1, next = eoi + 1, size = mincs + 1, prev = -1, first = 0;
        u32 bb = 0; int bc = 0, x = 0, y = 0, pass = 0, inter = (lf & 0x40) != 0;
        static const u8 ps[4] = {0, 4, 2, 1}, pst[4] = {8, 8, 4, 2};
        if (inter) y = 0;
        for (;;) {
            while (bc < size) { int c = ig_byte(); if (c < 0) goto done; bb |= (u32)c << bc; bc += 8; }
            int code = bb & ((1u << size) - 1); bb >>= size; bc -= size;
            if (code == clear) { size = mincs + 1; next = eoi + 1; prev = -1; continue; }
            if (code == eoi) break;
            int in = code, sp = 0;
            if (prev < 0) { if (code >= clear) goto done; ig_stk[sp++] = (u8)code; first = code; }
            else {
                if (code >= next) { if (code > next) goto done; ig_stk[sp++] = (u8)first; code = prev; }
                while (code >= clear) { if (sp >= 4096) goto done; ig_stk[sp++] = ig_suf[code]; code = ig_pre[code]; }
                first = code; ig_stk[sp++] = (u8)code;
                if (next < 4096) { ig_pre[next] = prev; ig_suf[next] = (u8)first; next++; if (next == (1 << size) && size < 12) size++; }
            }
            prev = in;
            while (sp) {
                int idx = ig_stk[--sp];
                if (idx < pn && y < ih) im_px(ix + x, iy + y, pal[idx * 3], pal[idx * 3 + 1], pal[idx * 3 + 2]);
                if (++x >= iw) {
                    x = 0;
                    if (inter) { y += pst[pass]; while (y >= ih && pass < 3) { pass++; y = ps[pass]; } }
                    else y++;
                    if (y >= ih) goto done;
                }
            }
        }
    done:
        return 0;
    }
    return -2;
}

/* ================= общий вход и вывод в страницу ================= */
static const u8 im_pal[16][3] = {{0,0,0},{0,0,170},{0,170,0},{0,170,170},{170,0,0},{170,0,170},{170,85,0},{170,170,170},
    {85,85,85},{85,85,255},{85,255,85},{85,255,255},{255,85,85},{255,85,255},{255,255,85},{255,255,255}};
static const signed char im_bay[4][4] = {{-24,8,-18,14},{20,-12,26,-4},{-14,18,-20,12},{30,-2,24,-8}};
static void pal_lut_init(void) {
    if (pal_lut_ready) return;
    for (int r = 0; r < 16; r++)
        for (int g = 0; g < 16; g++)
            for (int b = 0; b < 16; b++) {
                int R = r * 17, G = g * 17, B = b * 17;
                int best = 0, bd = 1 << 30;
                for (int i = 0; i < 16; i++) {
                    int dr = R - im_pal[i][0], dg = G - im_pal[i][1], db = B - im_pal[i][2];
                    int dd = dr * dr * 3 + dg * dg * 4 + db * db * 2;
                    if (dd < bd) { bd = dd; best = i; }
                }
                pal_lut[r][g][b] = (u8)best;
            }
    pal_lut_ready = 1;
}
static int im_nearest(int r, int g, int b) {
    if (!pal_lut_ready) pal_lut_init();
    int ri = (r >> 4) & 15, gi = (g >> 4) & 15, bi = (b >> 4) & 15;
    return pal_lut[ri][gi][bi];
}
static int img_decode(const u8 *d, int len) {
    if (len > 8 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G') return png_decode(d, len);
    if (len > 4 && d[0] == 0xFF && d[1] == 0xD8) return jpeg_decode(d, len);
    if (len > 6 && d[0] == 'G' && d[1] == 'I' && d[2] == 'F') return gif_decode(d, len);
    return -1;
}
static const char *img_err_str(int e) {
    return e == -1 ? "unsupported image format" : e == -3 ? "image too large" : "damaged image";
}
/* рисует декодированную картинку в зарезервированные строки k-й картинки страницы */
static void img_paint(int k) {
    int rows = (im_oh + 1) / 2, l0 = img_line[k];
    for (int r = 0; r < IMG_ROWS; r++) { r_line_init(l0 + r); }
    for (int r = 0; r < rows; r++) {
        int l = l0 + r;
        for (int x = 0; x < im_ow; x++) {
            int c[2];
            for (int h = 0; h < 2; h++) {
                int y = r * 2 + h;
                if (y >= im_oh) { c[h] = 0; continue; }
                int i = y * im_ow + x, n = im_n[i];
                int R = 0, G = 0, B = 0;
                if (n) { R = im_r[i] / n; G = im_g[i] / n; B = im_b[i] / n; }
                int dz = im_bay[y & 3][x & 3];
                c[h] = im_nearest(ij_cl(R + dz), ij_cl(G + dz), ij_cl(B + dz));
            }
            pg_ch[l][x] = 0xDF; pg_at[l][x] = (u8)(c[0] | (c[1] << 4)); pg_lk[l][x] = img_lk[k];
        }
        pg_len[l] = im_ow; pg_im[l] = 1;
    }
    html_img_shrink(k, rows);
}
#endif
