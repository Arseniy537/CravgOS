/* gfx.h - пиксельная графика: видеобуфер (линейный, 32 бита), поверхности в оперативной памяти, примитивы.
 * Чистый C без libc. Рисование идёт в теневой буфер (поверхность 0 = экран) и в поверхности окон;
 * gx_flip() копирует на экран только изменённый прямоугольник.
 * Пиксель поверхности: 0xAARRGGBB. Экран (поверхность 0) альфа-канал игнорирует.
 * Шрифт 8x16 (VGA): 256 глифов по 16 байт, старший бит байта - левый пиксель. */
#ifndef GFX_H
#define GFX_H
#define GFX_MAXS 192
#define GFX_MAXB 320
struct gsurf { u32 *px; int w, h, used; };
struct gblk { u32 off, size; u8 used; };
static struct gsurf gx_s[GFX_MAXS];
static struct gblk gx_b[GFX_MAXB];
static int gx_nb;
static u8 *gx_mem;
static u32 gx_memsz;
static u8 gx_font[4096];
static int gx_font_ok;
static int gx_ok;                                   /* пиксельный режим доступен */
static int gx_wm;                                   /* экраном владеет программа с gfx (WM), консоль не рисуется */
static volatile u8 *gx_lfb;
static u32 gx_pitch;
static int gx_w, gx_h, gx_rp, gx_gp, gx_bp;
static int gx_tgt;                                  /* поверхность, в которую рисуем */
static int gx_cx0, gx_cy0, gx_cx1, gx_cy1;          /* прямоугольник отсечения [x0,x1) x [y0,y1) */
static int gx_dx0, gx_dy0, gx_dx1, gx_dy1;          /* изменённая область экрана */

/* ---------- Память поверхностей: первое подходящее, слияние свободных ---------- */
static void gx_arena_init(u8 *mem, u32 size) {
    gx_mem = mem; gx_memsz = size;
    gx_b[0].off = 0; gx_b[0].size = size; gx_b[0].used = 0; gx_nb = 1;
    for (int i = 0; i < GFX_MAXS; i++) gx_s[i].used = 0;
}
static void *gx_alloc(u32 size) {
    size = (size + 63u) & ~63u;
    for (int i = 0; i < gx_nb; i++) {
        if (gx_b[i].used || gx_b[i].size < size) continue;
        if (gx_b[i].size > size) {
            if (gx_nb >= GFX_MAXB) continue;
            for (int j = gx_nb; j > i + 1; j--) gx_b[j] = gx_b[j - 1];
            gx_b[i + 1].off = gx_b[i].off + size; gx_b[i + 1].size = gx_b[i].size - size; gx_b[i + 1].used = 0;
            gx_nb++; gx_b[i].size = size;
        }
        gx_b[i].used = 1;
        return gx_mem + gx_b[i].off;
    }
    return 0;
}
static void gx_release(void *p) {
    u32 off = (u32)((u8 *)p - gx_mem);
    for (int i = 0; i < gx_nb; i++) {
        if (gx_b[i].off != off || !gx_b[i].used) continue;
        gx_b[i].used = 0;
        if (i + 1 < gx_nb && !gx_b[i + 1].used) { gx_b[i].size += gx_b[i + 1].size; for (int j = i + 1; j + 1 < gx_nb; j++) gx_b[j] = gx_b[j + 1]; gx_nb--; }
        if (i > 0 && !gx_b[i - 1].used) { gx_b[i - 1].size += gx_b[i].size; for (int j = i; j + 1 < gx_nb; j++) gx_b[j] = gx_b[j + 1]; gx_nb--; }
        return;
    }
}
static u32 gx_free_bytes(void) { u32 f = 0; for (int i = 0; i < gx_nb; i++) if (!gx_b[i].used) f += gx_b[i].size; return f; }
static int gx_new(int w, int h) {                   /* новая поверхность (прозрачная), номер или -1 */
    if (w < 1 || h < 1 || w > 4096 || h > 4096) return -1;
    for (int s = 1; s < GFX_MAXS; s++) {
        if (gx_s[s].used) continue;
        u32 *p = (u32 *)gx_alloc((u32)w * (u32)h * 4u);
        if (!p) return -1;
        for (u32 i = 0, n = (u32)w * (u32)h; i < n; i++) p[i] = 0;
        gx_s[s].px = p; gx_s[s].w = w; gx_s[s].h = h; gx_s[s].used = 1;
        return s;
    }
    return -1;
}
static void gx_del(int s) {
    if (s < 1 || s >= GFX_MAXS || !gx_s[s].used) return;
    gx_release(gx_s[s].px); gx_s[s].used = 0;
    if (gx_tgt == s) { gx_tgt = 0; gx_cx0 = gx_cy0 = 0; gx_cx1 = gx_s[0].w; gx_cy1 = gx_s[0].h; }
}
static void gx_target(int s) {
    if (s < 0 || s >= GFX_MAXS || !gx_s[s].used) return;
    gx_tgt = s; gx_cx0 = gx_cy0 = 0; gx_cx1 = gx_s[s].w; gx_cy1 = gx_s[s].h;
}
static void gx_clip(int x, int y, int w, int h) {
    struct gsurf *t = &gx_s[gx_tgt];
    gx_cx0 = x < 0 ? 0 : x; gx_cy0 = y < 0 ? 0 : y;
    gx_cx1 = x + w > t->w ? t->w : x + w; gx_cy1 = y + h > t->h ? t->h : y + h;
}
static void gx_dirty(int x0, int y0, int x1, int y1) {
    if (gx_dx0 >= gx_dx1) { gx_dx0 = x0; gx_dy0 = y0; gx_dx1 = x1; gx_dy1 = y1; return; }
    if (x0 < gx_dx0) gx_dx0 = x0;
    if (y0 < gx_dy0) gx_dy0 = y0;
    if (x1 > gx_dx1) gx_dx1 = x1;
    if (y1 > gx_dy1) gx_dy1 = y1;
}
static inline u32 gx_mix(u32 d, u32 s, u32 a) {       /* смесь: a (0..255) - вес s; альфа приёмника сохраняется */
    u32 ia = 255 - a;
    u32 r = ((d >> 16) & 255) * ia + ((s >> 16) & 255) * a + 128;
    u32 g = ((d >> 8) & 255) * ia + ((s >> 8) & 255) * a + 128;
    u32 b = (d & 255) * ia + (s & 255) * a + 128;
    r = (r + (r >> 8)) >> 8; g = (g + (g >> 8)) >> 8; b = (b + (b >> 8)) >> 8;
    return (d & 0xFF000000u) | (r << 16) | (g << 8) | b;
}
typedef u64 __attribute__((__may_alias__)) gx_u64a;
typedef __UINTPTR_TYPE__ gx_uptr;
static inline u32 gx_div255(u32 v) { u32 t = v + 128u; return (t + (t >> 8)) >> 8; }
static inline u32 gx_over_alpha(u32 da, u32 sa) { return da + sa - gx_div255(da * sa); }
static inline void gx_fill_row32(u32 *dst, int n, u32 v) {
    int i = 0;
    if (i < n && (((gx_uptr)dst) & 7u)) { dst[0] = v; i = 1; }
    u64 v64 = (u64)v | ((u64)v << 32);
    for (; i + 7 < n; i += 8) {
        gx_u64a *p = (gx_u64a *)(dst + i);
        p[0] = v64; p[1] = v64; p[2] = v64; p[3] = v64;
    }
    for (; i + 1 < n; i += 2) *(gx_u64a *)(dst + i) = v64;
    if (i < n) dst[i] = v;
}
static inline void gx_copy_row32(u32 *dst, const u32 *src, int n) {
    int i = 0;
    if (i < n && (((gx_uptr)dst) & 7u)) { dst[0] = src[0]; i = 1; }
    if ((((gx_uptr)(src + i)) & 7u) == 0) {
        for (; i + 7 < n; i += 8) {
            gx_u64a *d = (gx_u64a *)(dst + i);
            const gx_u64a *s = (const gx_u64a *)(src + i);
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
        }
        for (; i + 1 < n; i += 2) *(gx_u64a *)(dst + i) = *(const gx_u64a *)(src + i);
    } else {
        for (; i + 3 < n; i += 4) {
            gx_u64a *d = (gx_u64a *)(dst + i);
            d[0] = (u64)src[i] | ((u64)src[i + 1] << 32);
            d[1] = (u64)src[i + 2] | ((u64)src[i + 3] << 32);
        }
        for (; i + 1 < n; i += 2) *(gx_u64a *)(dst + i) = (u64)src[i] | ((u64)src[i + 1] << 32);
    }
    if (i < n) dst[i] = src[i];
}

/* ---------- Примитивы ---------- */
static void gx_fill(int x, int y, int w, int h, u32 rgb, u32 a) {
    struct gsurf *t = &gx_s[gx_tgt];
    int x1 = x + w, y1 = y + h;
    if (x < gx_cx0) x = gx_cx0;
    if (y < gx_cy0) y = gx_cy0;
    if (x1 > gx_cx1) x1 = gx_cx1;
    if (y1 > gx_cy1) y1 = gx_cy1;
    if (x >= x1 || y >= y1 || a == 0) return;
    u32 full = 0xFF000000u | (rgb & 0xFFFFFFu);
    int cw = x1 - x;
    if (a >= 255) {
        for (int j = y; j < y1; j++) gx_fill_row32(t->px + (u32)j * (u32)t->w + (u32)x, cw, full);
    } else {
        u32 ia = 255u - a;
        u32 sr_a = ((full >> 16) & 255u) * a + 128u;
        u32 sg_a = ((full >> 8) & 255u) * a + 128u;
        u32 sb_a = (full & 255u) * a + 128u;
        for (int j = y; j < y1; j++) {
            u32 *row = t->px + (u32)j * (u32)t->w;
            for (int i = x; i < x1; i++) {
                u32 d = row[i];
                u32 r = ((d >> 16) & 255u) * ia + sr_a;
                u32 g = ((d >> 8) & 255u) * ia + sg_a;
                u32 b = (d & 255u) * ia + sb_a;
                r = (r + (r >> 8)) >> 8; g = (g + (g >> 8)) >> 8; b = (b + (b >> 8)) >> 8;
                row[i] = (d & 0xFF000000u) | (r << 16) | (g << 8) | b;
            }
        }
    }
    if (gx_tgt == 0) gx_dirty(x, y, x1, y1);
}
static void gx_raw(int x, int y, int w, int h, u32 argb) {   /* записать точное значение ARGB без смешивания */
    struct gsurf *t = &gx_s[gx_tgt];
    int x1 = x + w, y1 = y + h;
    if (x < gx_cx0) x = gx_cx0;
    if (y < gx_cy0) y = gx_cy0;
    if (x1 > gx_cx1) x1 = gx_cx1;
    if (y1 > gx_cy1) y1 = gx_cy1;
    if (x >= x1 || y >= y1) return;
    int cw = x1 - x;
    for (int j = y; j < y1; j++) gx_fill_row32(t->px + (u32)j * (u32)t->w + (u32)x, cw, argb);
    if (gx_tgt == 0) gx_dirty(x, y, x1, y1);
}
static u32 gx_getpx(int x, int y) {
    struct gsurf *t = &gx_s[gx_tgt];
    if (x < 0 || y < 0 || x >= t->w || y >= t->h) return 0;
    return t->px[(u32)y * (u32)t->w + (u32)x];
}
static int gx_text(int x, int y, const char *s, int len, u32 rgb, int scale) {   /* возвращает ширину в пикселях */
    struct gsurf *t = &gx_s[gx_tgt];
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    u32 full = 0xFF000000u | (rgb & 0xFFFFFF);
    int x0 = x;
    for (int k = 0; k < len; k++, x += 8 * scale) {
        if (x + 8 * scale <= gx_cx0 || x >= gx_cx1 || y + 16 * scale <= gx_cy0 || y >= gx_cy1) continue;
        const u8 *g = gx_font + ((u8)s[k]) * 16;
        for (int r = 0; r < 16; r++) {
            u8 bits = g[r];
            if (!bits) continue;
            for (int sy = 0; sy < scale; sy++) {
                int py = y + r * scale + sy;
                if (py < gx_cy0 || py >= gx_cy1) continue;
                u32 *row = t->px + (u32)py * (u32)t->w;
                for (int c = 0; c < 8; c++) {
                    if (!(bits & (0x80 >> c))) continue;
                    for (int sx = 0; sx < scale; sx++) {
                        int px = x + c * scale + sx;
                        if (px >= gx_cx0 && px < gx_cx1) row[px] = full;
                    }
                }
            }
        }
    }
    if (gx_tgt == 0 && len > 0) gx_dirty(x0 < gx_cx0 ? gx_cx0 : x0, y < gx_cy0 ? gx_cy0 : y, x > gx_cx1 ? gx_cx1 : x, y + 16 * scale > gx_cy1 ? gx_cy1 : y + 16 * scale);
    return len * 8 * scale;
}
/* копирование/смешивание части поверхности src в цель; ga < 0 - копия, иначе смешивание с учётом альфы пикселя и ga (0..255) */
static void gx_blit(int src, int dx, int dy, int sx, int sy, int w, int h, int ga) {
    if (src < 0 || src >= GFX_MAXS || !gx_s[src].used || src == gx_tgt || ga == 0) return;
    struct gsurf *s = &gx_s[src], *t = &gx_s[gx_tgt];
    if (w <= 0 || sx + w > s->w) w = s->w - sx;
    if (h <= 0 || sy + h > s->h) h = s->h - sy;
    if (sx < 0) { dx -= sx; w += sx; sx = 0; }
    if (sy < 0) { dy -= sy; h += sy; sy = 0; }
    int x0 = dx, y0 = dy, x1 = dx + w, y1 = dy + h;
    if (x0 < gx_cx0) { sx += gx_cx0 - x0; x0 = gx_cx0; }
    if (y0 < gx_cy0) { sy += gx_cy0 - y0; y0 = gx_cy0; }
    if (x1 > gx_cx1) x1 = gx_cx1;
    if (y1 > gx_cy1) y1 = gx_cy1;
    if (x0 >= x1 || y0 >= y1) return;
    int cw = x1 - x0;
    if (ga < 0) {
        for (int j = y0; j < y1; j++) {
            const u32 *sr = s->px + (u32)(sy + j - y0) * (u32)s->w + (u32)sx;
            u32 *dr = t->px + (u32)j * (u32)t->w + (u32)x0;
            gx_copy_row32(dr, sr, cw);
        }
    } else if (ga >= 255) {
        for (int j = y0; j < y1; j++) {
            const u32 *sr = s->px + (u32)(sy + j - y0) * (u32)s->w + (u32)sx;
            u32 *dr = t->px + (u32)j * (u32)t->w + (u32)x0;
            for (int i = 0; i < cw; i++) {
                u32 p = sr[i], a = p >> 24;
                if (a == 0) continue;
                if (a == 255) dr[i] = p;
                else { u32 d = dr[i]; u32 m = gx_mix(d, p, a); dr[i] = (m & 0xFFFFFFu) | (gx_over_alpha(d >> 24, a) << 24); }
            }
        }
    } else {
        u32 uga = (u32)ga;
        for (int j = y0; j < y1; j++) {
            const u32 *sr = s->px + (u32)(sy + j - y0) * (u32)s->w + (u32)sx;
            u32 *dr = t->px + (u32)j * (u32)t->w + (u32)x0;
            for (int i = 0; i < cw; i++) {
                u32 p = sr[i], pa = p >> 24;
                if (pa == 0) continue;
                u32 a = gx_div255(pa * uga);
                if (a == 0) continue;
                if (a >= 255) dr[i] = 0xFF000000u | (p & 0xFFFFFFu);
                else { u32 d = dr[i]; u32 m = gx_mix(d, p, a); dr[i] = (m & 0xFFFFFFu) | (gx_over_alpha(d >> 24, a) << 24); }
            }
        }
    }
    if (gx_tgt == 0) gx_dirty(x0, y0, x1, y1);
}
static void gx_scale(int src, int dx, int dy, int dw, int dh, int ga) {   /* масштабирование (ближайший сосед) всей поверхности */
    if (src < 0 || src >= GFX_MAXS || !gx_s[src].used || src == gx_tgt || dw <= 0 || dh <= 0) return;
    struct gsurf *s = &gx_s[src], *t = &gx_s[gx_tgt];
    int x0 = dx, y0 = dy, x1 = dx + dw, y1 = dy + dh;
    if (x0 < gx_cx0) x0 = gx_cx0;
    if (y0 < gx_cy0) y0 = gx_cy0;
    if (x1 > gx_cx1) x1 = gx_cx1;
    if (y1 > gx_cy1) y1 = gx_cy1;
    if (x0 >= x1 || y0 >= y1) return;
    for (int j = y0; j < y1; j++) {
        const u32 *sr = s->px + (u32)(((long long)(j - dy) * s->h) / dh) * (u32)s->w;
        u32 *dr = t->px + (u32)j * (u32)t->w;
        for (int i = x0; i < x1; i++) {
            u32 p = sr[((long long)(i - dx) * s->w) / dw];
            if (ga < 0) { dr[i] = p; continue; }
            u32 a = ((p >> 24) * (u32)ga + 127) / 255;
            if (a >= 255) dr[i] = 0xFF000000u | (p & 0xFFFFFF); else if (a) dr[i] = gx_mix(dr[i], p, a);
        }
    }
    if (gx_tgt == 0) gx_dirty(x0, y0, x1, y1);
}
static void gx_grad(int x, int y, int w, int h, u32 c1, u32 c2, int vertical) {
    struct gsurf *t = &gx_s[gx_tgt];
    int x1 = x + w, y1 = y + h, ox = x, oy = y;
    if (x < gx_cx0) x = gx_cx0;
    if (y < gx_cy0) y = gx_cy0;
    if (x1 > gx_cx1) x1 = gx_cx1;
    if (y1 > gx_cy1) y1 = gx_cy1;
    if (x >= x1 || y >= y1 || w <= 0 || h <= 0) return;
    int cw = x1 - x;
    if (vertical) {
        u32 div = (u32)(h > 1 ? h - 1 : 1);
        for (int j = y; j < y1; j++) {
            u32 k = (u32)(j - oy) * 255u / div;
            u32 c = 0xFF000000u | (gx_mix(c1, c2, k) & 0xFFFFFFu);
            gx_fill_row32(t->px + (u32)j * (u32)t->w + (u32)x, cw, c);
        }
    } else {
        u32 div = (u32)(w > 1 ? w - 1 : 1);
        u32 *first = t->px + (u32)y * (u32)t->w + (u32)x;
        for (int i = x; i < x1; i++) {
            u32 k = (u32)(i - ox) * 255u / div;
            first[i - x] = 0xFF000000u | (gx_mix(c1, c2, k) & 0xFFFFFFu);
        }
        for (int j = y + 1; j < y1; j++) gx_copy_row32(t->px + (u32)j * (u32)t->w + (u32)x, first, cw);
    }
    if (gx_tgt == 0) gx_dirty(x, y, x1, y1);
}
static int gx_isqrt(int v) { int r = 0; while ((r + 1) * (r + 1) <= v) r++; return r; }
static void gx_round(int x, int y, int w, int h, int r, u32 rgb, u32 a) {   /* скруглённый прямоугольник */
    if (r < 1) { gx_fill(x, y, w, h, rgb, a); return; }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    for (int j = 0; j < h; j++) {
        int inset = 0;
        if (j < r) inset = r - gx_isqrt(r * r - (r - j) * (r - j));
        else if (j >= h - r) inset = r - gx_isqrt(r * r - (j - (h - r - 1)) * (j - (h - r - 1)));
        gx_fill(x + inset, y + j, w - 2 * inset, 1, rgb, a);
    }
}
/* мягкая тень вокруг прямоугольника: flags 1 - не рисовать под самим прямоугольником, 2 - записывать ARGB (для поверхности с альфой) */
static void gx_shadow(int x, int y, int w, int h, int r, u32 alpha, u32 rgb, int flags) {
    struct gsurf *t = &gx_s[gx_tgt];
    if (r < 1 || alpha == 0) return;
    int ax0 = x - r, ay0 = y - r, ax1 = x + w + r, ay1 = y + h + r;
    int cx0 = ax0 < gx_cx0 ? gx_cx0 : ax0, cy0 = ay0 < gx_cy0 ? gx_cy0 : ay0;
    int cx1 = ax1 > gx_cx1 ? gx_cx1 : ax1, cy1 = ay1 > gx_cy1 ? gx_cy1 : ay1;
    u8 alut[128];
    int lut_n = r <= 128 ? r : 0;
    for (int d = 0; d < lut_n; d++) { u32 tt = (u32)((r - d) * 256 / r); alut[d] = (u8)((alpha * (tt * tt >> 8)) >> 8); }
    u32 col = 0xFF000000u | (rgb & 0xFFFFFFu), crgb = rgb & 0xFFFFFFu;
    int hole = (flags & 1), argb = (flags & 2);
    for (int j = cy0; j < cy1; j++) {
        u32 *row = t->px + (u32)j * (u32)t->w;
        int dy = j < y ? y - j : (j >= y + h ? j - (y + h - 1) : 0);
        for (int i = cx0; i < cx1; i++) {
            if (hole && dy == 0 && i >= x && i < x + w) { i = x + w - 1; continue; }
            int dx = i < x ? x - i : (i >= x + w ? i - (x + w - 1) : 0);
            int mx = dx > dy ? dx : dy, mn = dx > dy ? dy : dx;
            int d = mx + (mn * 2) / 5;
            if (d >= r) continue;
            u32 a = lut_n ? alut[d] : ((alpha * ((((u32)((r - d) * 256 / r)) * ((u32)((r - d) * 256 / r))) >> 8)) >> 8);
            if (a == 0) continue;
            if (argb) { u32 pa = row[i] >> 24; if (a > pa) row[i] = (a << 24) | crgb; }
            else row[i] = gx_mix(row[i], col, a);
        }
    }
    if (gx_tgt == 0) gx_dirty(cx0, cy0, cx1, cy1);
}
/* BMP (без сжатия; 1, 4, 8, 24, 32 бита) -> новая поверхность */
static int gx_loadbmp(const u8 *d, int n) {
    if (n < 54 || d[0] != 'B' || d[1] != 'M') return -1;
    #define LE32(o) ((u32)d[o] | (u32)d[(o) + 1] << 8 | (u32)d[(o) + 2] << 16 | (u32)d[(o) + 3] << 24)
    u32 off = LE32(10), hs = LE32(14);
    if (hs < 40) return -1;
    int w = (int)LE32(18), h = (int)LE32(22), bpp = d[28] | d[29] << 8, top = 0;
    u32 comp = LE32(30), ncol = LE32(46);
    if (h < 0) { h = -h; top = 1; }
    if (w < 1 || h < 1 || comp != 0 || (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32)) return -1;
    if (ncol == 0 && bpp <= 8) ncol = 1u << bpp;
    u32 pal = 14 + hs;
    u32 stride = ((u32)w * (u32)bpp + 31) / 32 * 4;
    if ((u64)off + (u64)stride * (u64)h > (u64)n) return -1;
    int s = gx_new(w, h);
    if (s < 0) return -1;
    u32 *px = gx_s[s].px;
    for (int y = 0; y < h; y++) {
        const u8 *row = d + off + stride * (u32)(top ? y : h - 1 - y);
        for (int x = 0; x < w; x++) {
            u32 c;
            if (bpp == 24) c = row[x * 3 + 2] << 16 | row[x * 3 + 1] << 8 | row[x * 3];
            else if (bpp == 32) c = row[x * 4 + 2] << 16 | row[x * 4 + 1] << 8 | row[x * 4];
            else {
                u32 idx = bpp == 8 ? row[x] : bpp == 4 ? (row[x / 2] >> (x & 1 ? 0 : 4)) & 15 : (row[x / 8] >> (7 - (x & 7))) & 1;
                if (idx >= ncol || pal + idx * 4 + 3 > (u32)n) idx = 0;
                const u8 *p = d + pal + idx * 4;
                c = p[2] << 16 | p[1] << 8 | p[0];
            }
            px[(u32)y * (u32)w + (u32)x] = 0xFF000000u | c;
        }
    }
    #undef LE32
    return s;
}
/* копирование изменённой части теневого буфера на экран */
static void gx_flip(int x0, int y0, int x1, int y1) {
    if (!gx_ok || !gx_s[0].used) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > gx_w) x1 = gx_w;
    if (y1 > gx_h) y1 = gx_h;
    if (x0 >= x1 || y0 >= y1) return;
    const u32 *src = gx_s[0].px;
    int fast = gx_rp == 16 && gx_gp == 8 && gx_bp == 0;
    for (int j = y0; j < y1; j++) {
        volatile u32 *dst = (volatile u32 *)(gx_lfb + (u32)j * gx_pitch);
        const u32 *sr = src + (u32)j * (u32)gx_w;
        if (fast) {
            int i = x0;
            if (i < x1 && (((gx_uptr)(dst + i)) & 7u)) { dst[i] = sr[i] & 0xFFFFFFu; i++; }
            if ((((gx_uptr)(sr + i)) & 7u) == 0) {
                for (; i + 7 < x1; i += 8) {
                    const gx_u64a *s64 = (const gx_u64a *)(sr + i);
                    volatile gx_u64a *d64 = (volatile gx_u64a *)(dst + i);
                    d64[0] = s64[0] & 0x00FFFFFF00FFFFFFULL;
                    d64[1] = s64[1] & 0x00FFFFFF00FFFFFFULL;
                    d64[2] = s64[2] & 0x00FFFFFF00FFFFFFULL;
                    d64[3] = s64[3] & 0x00FFFFFF00FFFFFFULL;
                }
                for (; i + 1 < x1; i += 2) *(volatile gx_u64a *)(dst + i) = (*(const gx_u64a *)(sr + i)) & 0x00FFFFFF00FFFFFFULL;
            } else {
                for (; i + 3 < x1; i += 4) {
                    volatile gx_u64a *d64 = (volatile gx_u64a *)(dst + i);
                    d64[0] = (u64)(sr[i] & 0xFFFFFFu) | ((u64)(sr[i + 1] & 0xFFFFFFu) << 32);
                    d64[1] = (u64)(sr[i + 2] & 0xFFFFFFu) | ((u64)(sr[i + 3] & 0xFFFFFFu) << 32);
                }
                for (; i + 1 < x1; i += 2) *(volatile gx_u64a *)(dst + i) = (u64)(sr[i] & 0xFFFFFFu) | ((u64)(sr[i + 1] & 0xFFFFFFu) << 32);
            }
            if (i < x1) dst[i] = sr[i] & 0xFFFFFFu;
        } else {
            for (int i = x0; i < x1; i++) { u32 c = sr[i]; dst[i] = ((c >> 16) & 255u) << gx_rp | ((c >> 8) & 255u) << gx_gp | (c & 255u) << gx_bp; }
        }
    }
}
static void gx_flip_dirty(void) {
    if (gx_dx0 < gx_dx1) gx_flip(gx_dx0, gx_dy0, gx_dx1, gx_dy1);
    gx_dx0 = gx_dy0 = gx_dx1 = gx_dy1 = 0;
}
/* подготовка: lfb - адрес видеопамяти, mem/size - память под поверхности; 0 - успех */
static int gx_setup(u8 *lfb, u32 pitch, int w, int h, int rp, int gp, int bp, u8 *mem, u32 memsz) {
    if (w < 320 || h < 200 || pitch < (u32)w * 4) return -1;
    gx_arena_init(mem, memsz);
    u32 *p = (u32 *)gx_alloc((u32)w * (u32)h * 4u);
    if (!p) return -1;
    for (u32 i = 0, n = (u32)w * (u32)h; i < n; i++) p[i] = 0xFF000000u;
    gx_s[0].px = p; gx_s[0].w = w; gx_s[0].h = h; gx_s[0].used = 1;
    gx_lfb = lfb; gx_pitch = pitch; gx_w = w; gx_h = h; gx_rp = rp; gx_gp = gp; gx_bp = bp;
    gx_tgt = 0; gx_cx0 = gx_cy0 = 0; gx_cx1 = w; gx_cy1 = h; gx_dx0 = gx_dy0 = gx_dx1 = gx_dy1 = 0;
    gx_ok = 1;
    return 0;
}
#endif
