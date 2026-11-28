/* luagfx.h - функции gfx.* и os.exec для Lua (расширение lua.h; подключается после lua.h и gfx.h).
 *
 * gfx.init() -> ширина, высота | nil, причина   захватить экран (консоль перестаёт рисоваться)
 * gfx.close()                                   вернуть экран консоли (вызывается и автоматически при выходе)
 * gfx.size() -> w, h     gfx.sizeof(s) -> w, h     gfx.mem() -> свободно КБ, всего КБ
 * gfx.surface(w, h) -> s | nil, причина         gfx.free(s)       gfx.target(s)  (0 = экран)    gfx.clip(x, y, w, h) | gfx.clip()
 * gfx.fill(x, y, w, h, rgb [, alpha])           gfx.raw(x, y, w, h, argb)    gfx.px(x, y) -> argb
 * gfx.text(x, y, str, rgb [, scale 1..4]) -> ширина     gfx.textw(str [, scale]) -> ширина
 * gfx.blit(src, x, y [, alpha [, sx, sy, w, h]])  без alpha - копия, с alpha - смешивание (альфа пикселя * alpha / 255)
 * gfx.scale(src, x, y, w, h [, alpha])          gfx.grad(x, y, w, h, rgb1, rgb2 [, vertical])
 * gfx.round(x, y, w, h, r, rgb [, alpha])       gfx.shadow(x, y, w, h, r, alpha [, rgb [, flags]])  flags: 1 без интерьера, 2 запись ARGB
 * gfx.bmp(path) -> s | nil, причина             gfx.flip([x, y, w, h])
 * gfx.poll() -> nil | {type="key", key=, base=, mods=} | {type="mouse", x=, y=, dx=, dy=, btn=, down=, up=, wheel=}
 * gfx.shot(name)                              снимок экрана (только в тестах)
 * gfx.mouse() -> x, y, btn      gfx.setmouse(x, y)       gfx.mods() -> маска: 1 shift, 2 ctrl, 4 alt, 8 super
 * os.exec(cmd) -> текст | nil, причина          команда оболочки с захватом вывода; только для файлов из /etc/cravgwm/exec-allow
 * Цвета - целые 0xRRGGBB. Кнопки мыши: 1 левая, 2 правая, 4 средняя. */
#ifndef LUAGFX_H
#define LUAGFX_H
enum { G_INIT = C_EXT, G_CLOSE, G_SIZE, G_SIZEOF, G_MEM, G_SURF, G_FREE, G_TARGET, G_CLIP, G_FILL, G_RAW, G_PX, G_TEXT, G_TEXTW,
       G_BLIT, G_SCALE, G_GRAD, G_ROUND, G_SHADOW, G_BMP, G_FLIP, G_POLL, G_MOUSE, G_SETMOUSE, G_MODS, G_SHOT, G_SUSPEND, O_EXEC };

/* среда: ядро (или тест) определяет эти функции до подключения файла */
#ifndef GX_KBD_PENDING
#define GX_KBD_PENDING() 0
#endif
#ifndef GX_MODS
#define GX_MODS() 0
#endif
#ifndef GX_SHOT
#define GX_SHOT(name) do {} while (0)          /* тесты: сохранить снимок экрана */
#endif
#ifndef GX_KBASE
#define GX_KBASE(k) (k)                              /* физическая клавиша (латинская, нижний регистр), не зависит от раскладки */
#endif
#ifndef GX_RELEASE_HOOK
#define GX_RELEASE_HOOK() do {} while (0)
#endif
#ifndef GX_MOUSE_INIT
#define GX_MOUSE_INIT() do {} while (0)
#endif

struct gev { int k, mods, base; };
static struct gev gx_q[64];
static u32 gx_qh, gx_qt;
static int gx_mx, gx_my, gx_mb, gx_pmb, gx_wheel, gx_mdx, gx_mdy, gx_mmoved;

/* вызывается драйвером мыши: приращения (вниз - положительное dy), колесо, маска кнопок */
static void gx_mouse_feed(int dx, int dy, int wheel, int btn) {
    int ox = gx_mx, oy = gx_my;
    gx_mx += dx; gx_my += dy;
    if (gx_mx < 0) gx_mx = 0;
    if (gx_my < 0) gx_my = 0;
    if (gx_w && gx_mx >= gx_w) gx_mx = gx_w - 1;
    if (gx_h && gx_my >= gx_h) gx_my = gx_h - 1;
    gx_mdx += dx; gx_mdy += dy; gx_wheel += wheel; gx_mb = btn;
    if (dx || dy || gx_mx != ox || gx_my != oy) gx_mmoved = 1;
}
static void gx_pump(void) {                                     /* забрать все клавиши из контроллера в очередь */
    int guard = 0;
    while (GX_KBD_PENDING() && guard++ < 256) {
        int k = key_poll();
        if (k >= 0 && gx_qt - gx_qh < 64) { gx_q[gx_qt & 63].k = k; gx_q[gx_qt & 63].mods = GX_MODS(); gx_q[gx_qt & 63].base = GX_KBASE(k); gx_qt++; }
    }
}
static void gx_suspend(int on) {                                /* временно отдать экран консоли (1) или вернуть WM (0) */
    if (!gx_ok || !gx_s[0].used) return;
    if (on) {
        if (gx_wm) { gx_wm = 0; GX_RELEASE_HOOK(); }
        gx_qh = gx_qt = 0;
    } else {
        gx_wm = 1;
        gx_target(0); gx_clip(0, 0, gx_w, gx_h);
        gx_dx0 = 0; gx_dy0 = 0; gx_dx1 = gx_w; gx_dy1 = gx_h;
        gx_flip_dirty();
        gx_qh = gx_qt = 0; gx_mdx = gx_mdy = gx_wheel = 0; gx_mmoved = 0; gx_pmb = gx_mb;
    }
}
static void gx_closeall(void) {                                 /* освободить все поверхности, вернуть экран консоли */
    if (!gx_ok) return;
    for (int s = 1; s < GFX_MAXS; s++) if (gx_s[s].used) gx_del(s);
    gx_target(0);
    if (gx_wm) { gx_wm = 0; GX_RELEASE_HOOK(); }
}
static int cfn_gfx_rgb(struct lval *a, int n, int i, u32 *out) { lint v; if (!arg_i(a, n, i, &v)) return 0; *out = (u32)v & 0xFFFFFF; return 1; }
static int lua_ext(int id, struct lval *a, int n) {
    lint x, y, w, h, r, v;
    u32 c, c2;
    if (id == O_EXEC) {
        const char *cmd; u32 cl;
        if (!arg_s(a, n, 0, &cmd, &cl)) return 0;
        const char *ch = L.chunk ? L.chunk : "";
        int ok = 0, fi = fs_resolve("/etc/cravgwm/exec-allow");
        if (fi >= 0 && !fsn[fi].dir) {
            const u8 *d = fsdata[fi]; int sz = fsn[fi].size, p = 0;
            while (p < sz && !ok) {
                int e = p; while (e < sz && d[e] != '\n' && d[e] != '\r') e++;
                int ln = e - p, k = 0;
                while (k < ln && ch[k] && (u8)ch[k] == d[p + k]) k++;
                if (ln > 0 && k == ln && !ch[k]) ok = 1;
                p = e + 1;
            }
        }
        if (!ok) { L.rv[0] = LNIL; L.rv[1] = lcstr_v("os.exec denied: add this program's path to /etc/cravgwm/exec-allow"); return 2; }
        char *buf = (char *)lua_exec_buf; int m = lua_exec(cmd, buf, (int)sizeof lua_exec_buf);
        if (m < 0) { L.rv[0] = LNIL; L.rv[1] = lcstr_v("command is not allowed here"); return 2; }
        return ret1(lstr_v(buf, (u32)m));
    }
    if (id == G_INIT) {
        if (!gx_ok) { L.rv[0] = LNIL; L.rv[1] = lcstr_v("no pixel mode (boot with wmboot)"); return 2; }
        GX_MOUSE_INIT();
        for (int s = 1; s < GFX_MAXS; s++) if (gx_s[s].used) gx_del(s);
        gx_target(0); gx_clip(0, 0, gx_w, gx_h);
        for (u32 i = 0, k = (u32)gx_w * (u32)gx_h; i < k; i++) gx_s[0].px[i] = 0xFF000000u;
        gx_wm = 1; gx_dx0 = gx_dy0 = 0; gx_dx1 = gx_w; gx_dy1 = gx_h;
        gx_mx = gx_w / 2; gx_my = gx_h / 2; gx_mb = gx_pmb = 0; gx_wheel = gx_mdx = gx_mdy = 0; gx_mmoved = 0; gx_qh = gx_qt = 0;
        gx_flip_dirty();
        L.rv[0] = lint_v(gx_w); L.rv[1] = lint_v(gx_h); return 2;
    }
    if (id == G_SUSPEND) {
        if (!gx_ok || !gx_s[0].used) { lerr("gfx is not initialized (call gfx.init() first)"); return 0; }
        gx_suspend(n > 0 ? ltruthy(a[0]) : 1);
        return 0;
    }
    if (!gx_ok || !gx_wm) { lerr("gfx is not initialized (call gfx.init() first)"); return 0; }
    switch (id) {
    case G_CLOSE: gx_closeall(); return 0;
    case G_SIZE: L.rv[0] = lint_v(gx_w); L.rv[1] = lint_v(gx_h); return 2;
    case G_SIZEOF: { if (!arg_i(a, n, 0, &x)) return 0; if (x < 0 || x >= GFX_MAXS || !gx_s[x].used) { lerr("bad surface"); return 0; } L.rv[0] = lint_v(gx_s[x].w); L.rv[1] = lint_v(gx_s[x].h); return 2; }
    case G_MEM: { u32 f = gx_free_bytes(); L.rv[0] = lint_v(f / 1024); L.rv[1] = lint_v(gx_memsz / 1024); return 2; }
    case G_SURF: {
        if (!arg_i(a, n, 0, &w) || !arg_i(a, n, 1, &h)) return 0;
        int s = gx_new((int)w, (int)h);
        if (s < 0) { L.rv[0] = LNIL; L.rv[1] = lcstr_v("cannot create surface (size or memory)"); return 2; }
        return ret1(lint_v(s));
    }
    case G_FREE: { if (!arg_i(a, n, 0, &x)) return 0; gx_del((int)x); return 0; }
    case G_TARGET: { if (!arg_i(a, n, 0, &x)) return 0; if (x < 0 || x >= GFX_MAXS || !gx_s[x].used) { lerr("bad surface"); return 0; } gx_target((int)x); return 0; }
    case G_CLIP: {
        if (n == 0) { gx_clip(0, 0, gx_s[gx_tgt].w, gx_s[gx_tgt].h); return 0; }
        if (!arg_i(a, n, 0, &x) || !arg_i(a, n, 1, &y) || !arg_i(a, n, 2, &w) || !arg_i(a, n, 3, &h)) return 0;
        gx_clip((int)x, (int)y, (int)w, (int)h); return 0;
    }
    case G_FILL: {
        if (!arg_i(a, n, 0, &x) || !arg_i(a, n, 1, &y) || !arg_i(a, n, 2, &w) || !arg_i(a, n, 3, &h) || !cfn_gfx_rgb(a, n, 4, &c)) return 0;
        v = arg_io(a, n, 5, 255); if (L.err) return 0;
        gx_fill((int)x, (int)y, (int)w, (int)h, c, v < 0 ? 0 : v > 255 ? 255 : (u32)v); return 0;
    }
    case G_RAW: { if (!arg_i(a, n, 0, &x) || !arg_i(a, n, 1, &y) || !arg_i(a, n, 2, &w) || !arg_i(a, n, 3, &h) || !arg_i(a, n, 4, &v)) return 0; gx_raw((int)x, (int)y, (int)w, (int)h, (u32)v); return 0; }
    case G_PX: { if (!arg_i(a, n, 0, &x) || !arg_i(a, n, 1, &y)) return 0; return ret1(lint_v(gx_getpx((int)x, (int)y))); }
    case G_TEXT: {
        const char *s; u32 l;
        if (!arg_i(a, n, 0, &x) || !arg_i(a, n, 1, &y) || !arg_s(a, n, 2, &s, &l) || !cfn_gfx_rgb(a, n, 3, &c)) return 0;
        v = arg_io(a, n, 4, 1); if (L.err) return 0;
        return ret1(lint_v(gx_text((int)x, (int)y, s, (int)l, c, (int)v)));
    }
    case G_TEXTW: { const char *s; u32 l; if (!arg_s(a, n, 0, &s, &l)) return 0; v = arg_io(a, n, 1, 1); if (L.err) return 0; if (v < 1) v = 1; if (v > 4) v = 4; return ret1(lint_v((lint)l * 8 * v)); }
    case G_BLIT: {
        lint src, sx = 0, sy = 0, bw = 0, bh = 0, al = -1;
        if (!arg_i(a, n, 0, &src) || !arg_i(a, n, 1, &x) || !arg_i(a, n, 2, &y)) return 0;
        if (n > 3 && a[3].t != LT_NIL) { if (!arg_i(a, n, 3, &al)) return 0; if (al < 0) al = 0; if (al > 255) al = 255; }
        sx = arg_io(a, n, 4, 0); sy = arg_io(a, n, 5, 0); bw = arg_io(a, n, 6, 0); bh = arg_io(a, n, 7, 0); if (L.err) return 0;
        gx_blit((int)src, (int)x, (int)y, (int)sx, (int)sy, (int)bw, (int)bh, (int)al); return 0;
    }
    case G_SCALE: {
        lint src, al = -1;
        if (!arg_i(a, n, 0, &src) || !arg_i(a, n, 1, &x) || !arg_i(a, n, 2, &y) || !arg_i(a, n, 3, &w) || !arg_i(a, n, 4, &h)) return 0;
        if (n > 5 && a[5].t != LT_NIL) { if (!arg_i(a, n, 5, &al)) return 0; if (al < 0) al = 0; if (al > 255) al = 255; }
        gx_scale((int)src, (int)x, (int)y, (int)w, (int)h, (int)al); return 0;
    }
    case G_GRAD: {
        if (!arg_i(a, n, 0, &x) || !arg_i(a, n, 1, &y) || !arg_i(a, n, 2, &w) || !arg_i(a, n, 3, &h) || !cfn_gfx_rgb(a, n, 4, &c) || !cfn_gfx_rgb(a, n, 5, &c2)) return 0;
        gx_grad((int)x, (int)y, (int)w, (int)h, c, c2, n > 6 && ltruthy(a[6])); return 0;
    }
    case G_ROUND: {
        if (!arg_i(a, n, 0, &x) || !arg_i(a, n, 1, &y) || !arg_i(a, n, 2, &w) || !arg_i(a, n, 3, &h) || !arg_i(a, n, 4, &r) || !cfn_gfx_rgb(a, n, 5, &c)) return 0;
        v = arg_io(a, n, 6, 255); if (L.err) return 0;
        gx_round((int)x, (int)y, (int)w, (int)h, (int)r, c, v < 0 ? 0 : v > 255 ? 255 : (u32)v); return 0;
    }
    case G_SHADOW: {
        lint al, fl;
        if (!arg_i(a, n, 0, &x) || !arg_i(a, n, 1, &y) || !arg_i(a, n, 2, &w) || !arg_i(a, n, 3, &h) || !arg_i(a, n, 4, &r) || !arg_i(a, n, 5, &al)) return 0;
        c = 0; if (n > 6 && a[6].t != LT_NIL && !cfn_gfx_rgb(a, n, 6, &c)) return 0;
        fl = arg_io(a, n, 7, 0); if (L.err) return 0;
        if (al < 0) al = 0;
        if (al > 255) al = 255;
        gx_shadow((int)x, (int)y, (int)w, (int)h, (int)r, (u32)al, c, (int)fl); return 0;
    }
    case G_BMP: {
        const char *p; u32 pl; if (!arg_s(a, n, 0, &p, &pl)) return 0;
        int fi = fs_resolve(p);
        if (fi < 0 || fsn[fi].dir) { L.rv[0] = LNIL; L.rv[1] = lcstr_v("cannot open file"); return 2; }
        int s = gx_loadbmp(fsdata[fi], fsn[fi].size);
        if (s < 0) { L.rv[0] = LNIL; L.rv[1] = lcstr_v("unsupported or damaged BMP (24/32/8/4/1 bit, no compression, up to 8192 bytes)"); return 2; }
        return ret1(lint_v(s));
    }
    case G_FLIP: {
        if (n >= 4) { if (!arg_i(a, n, 0, &x) || !arg_i(a, n, 1, &y) || !arg_i(a, n, 2, &w) || !arg_i(a, n, 3, &h)) return 0; gx_flip((int)x, (int)y, (int)(x + w), (int)(y + h)); }
        else gx_flip_dirty();
        return 0;
    }
    case G_POLL: {
        gx_pump(); L.since_key = 0;
        if (gx_qh != gx_qt) {
            struct gev e = gx_q[gx_qh & 63]; gx_qh++;
            struct ltab *t = lnewtab(); struct lval tv = ltab_v(t);
            lsetfield(t, "type", lcstr_v("key")); lsetfield(t, "key", lint_v(e.k)); lsetfield(t, "mods", lint_v(e.mods)); lsetfield(t, "base", lint_v(e.base));
            return ret1(tv);
        }
        if (gx_mmoved || gx_wheel || gx_mb != gx_pmb) {
            struct ltab *t = lnewtab(); struct lval tv = ltab_v(t);
            lsetfield(t, "type", lcstr_v("mouse")); lsetfield(t, "x", lint_v(gx_mx)); lsetfield(t, "y", lint_v(gx_my));
            lsetfield(t, "dx", lint_v(gx_mdx)); lsetfield(t, "dy", lint_v(gx_mdy)); lsetfield(t, "btn", lint_v(gx_mb));
            lsetfield(t, "down", lint_v(gx_mb & ~gx_pmb)); lsetfield(t, "up", lint_v(gx_pmb & ~gx_mb)); lsetfield(t, "wheel", lint_v(gx_wheel));
            lsetfield(t, "mods", lint_v(GX_MODS()));
            gx_pmb = gx_mb; gx_mdx = gx_mdy = gx_wheel = 0; gx_mmoved = 0;
            return ret1(tv);
        }
        return ret1(LNIL);
    }
    case G_MOUSE: L.rv[0] = lint_v(gx_mx); L.rv[1] = lint_v(gx_my); L.rv[2] = lint_v(gx_mb); return 3;
    case G_SETMOUSE: { if (!arg_i(a, n, 0, &x) || !arg_i(a, n, 1, &y)) return 0; gx_mx = (int)x < 0 ? 0 : (int)x >= gx_w ? gx_w - 1 : (int)x; gx_my = (int)y < 0 ? 0 : (int)y >= gx_h ? gx_h - 1 : (int)y; return 0; }
    case G_MODS: return ret1(lint_v(GX_MODS()));
    case G_SHOT: { const char *p; u32 pl; if (!arg_s(a, n, 0, &p, &pl)) return 0; GX_SHOT(p); return 0; }
    }
    lerr("unknown gfx function"); return 0;
}
static void lua_ext_init(void) {
    static const struct { const char *lib, *name; int id; } xr[] = {
        {"gfx","init",G_INIT},{"gfx","close",G_CLOSE},{"gfx","size",G_SIZE},{"gfx","sizeof",G_SIZEOF},{"gfx","mem",G_MEM},{"gfx","surface",G_SURF},
        {"gfx","free",G_FREE},{"gfx","target",G_TARGET},{"gfx","clip",G_CLIP},{"gfx","fill",G_FILL},{"gfx","raw",G_RAW},{"gfx","px",G_PX},
        {"gfx","text",G_TEXT},{"gfx","textw",G_TEXTW},{"gfx","blit",G_BLIT},{"gfx","scale",G_SCALE},{"gfx","grad",G_GRAD},{"gfx","round",G_ROUND},
        {"gfx","shadow",G_SHADOW},{"gfx","bmp",G_BMP},{"gfx","flip",G_FLIP},{"gfx","poll",G_POLL},{"gfx","mouse",G_MOUSE},{"gfx","setmouse",G_SETMOUSE},
        {"gfx","mods",G_MODS},{"gfx","shot",G_SHOT},{"gfx","suspend",G_SUSPEND},{"os","exec",O_EXEC},
    };
    for (unsigned i = 0; i < sizeof xr / sizeof xr[0]; i++) lua_regfn(xr[i].lib, xr[i].name, xr[i].id);
}
static void lua_ext_done(void) { gx_closeall(); }
#endif
