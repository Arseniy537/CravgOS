/* gfx_env.h - общая среда для тестов графики и оконного менеджера: подложное железо (экран 1024x768 в памяти,
 * часы, клавиатура и мышь по сценарию), запись снимков экрана в /tmp/shots/ (ppm) */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32; typedef unsigned long long u64;
#include "../fs.h"
#define W 80
#define H 25
static int cx, cy; static u8 color = 0x07;
static char out[1 << 16]; static int outn;
static void t_putc(char c) { if (outn < (int)sizeof out - 1) out[outn++] = c; out[outn] = 0; }
#define putc t_putc
static void puts_(const char *s) { while (*s) t_putc(*s++); }
#define puts puts_
static void put_at(int x, int y, char c, u8 attr) { (void)x; (void)y; (void)c; (void)attr; }
static void clear(void) { cx = cy = 0; }
static void cursor(void) {} static void cursor_hide(void) {} static void cursor_show(void) {}
static u32 vclock;
static u32 now_ms(void) { return vclock += 1; }
static void delay_ms(u32 ms) { vclock += ms; }
static void fs_sync(void) {}
static int readline(char *b, int max) { b[0] = 0; (void)max; return 0; }
static void lua_rtc(int *y, int *mo, int *d, int *h, int *mi, int *s) { *y = 2026; *mo = 11; *d = 9; *h = 14; *mi = 5; *s = 30; }
static int lua_edit(const char *p) { (void)p; return 1; }

/* --- сценарий ввода: K клавиша(a, mods=b), M мышь(dx,dy,wheel,btn,mods), A мышь в точку(x,y,btn,mods), W конец опроса --- */
struct step { char t; int a, b, c, d, e; void (*fn)(void); };
static struct step sc[20000]; static int sc_n, sc_i, idle;
static int cur_mods;
static void sc_reset(void) { sc_n = sc_i = idle = 0; cur_mods = 0; }
static void sK(int key, int mods) { sc[sc_n++] = (struct step){ 'K', key, mods, 0, 0, 0, 0 }; }
static void sM(int dx, int dy, int wheel, int btn, int mods) { sc[sc_n++] = (struct step){ 'M', dx, dy, wheel, btn, mods, 0 }; }
static void sA(int x, int y, int btn, int mods) { sc[sc_n++] = (struct step){ 'A', x, y, 0, btn, mods, 0 }; }
static void sW(int n) { for (int i = 0; i < n; i++) sc[sc_n++] = (struct step){ 'W', 0, 0, 0, 0, 0, 0 }; }
static const char *shot_names[200]; static int shot_nn;
static void sS(const char *name) { shot_names[shot_nn] = name; sc[sc_n++] = (struct step){ 'S', shot_nn++, 0, 0, 0, 0, 0 }; }   /* снимок экрана */
static void sF(void (*fn)(void)) { sc[sc_n++] = (struct step){ 'F', 0, 0, 0, 0, 0, fn }; }                                 /* проверка в C между кадрами */
static void sT(const char *text) { for (; *text; text++) { sK(*text, 0); sW(1); } }     /* набрать строку */
static int gx_mx_, gx_my_;
static int env_pending(void);
static int key_poll(void) { if (sc_i < sc_n && sc[sc_i].t == 'K') { cur_mods = sc[sc_i].b; return sc[sc_i++].a; } return -1; }
#define GX_KBD_PENDING() env_pending()
#define GX_KBASE(k) ((k) >= 'A' && (k) <= 'Z' ? (k) + 32 : (k))
#define GX_MODS() cur_mods
static void env_shot(const char *name);
#define GX_SHOT(name) env_shot(name)
static char lua_exec_buf[4096];
static int lua_exec(const char *cmd, char *buf, int max) {
    if (!strncmp(cmd, "echo ", 5)) { int n = (int)strlen(cmd + 5); if (n > max - 2) n = max - 2; memcpy(buf, cmd + 5, (size_t)n); buf[n++] = '\n'; return n; }
    if (!strcmp(cmd, "ver")) { strcpy(buf, "Cravgos test\n"); return 13; }
    return -1;
}
#include "../gfx.h"
#define LUA_EXT
#include "../lua.h"
#include "../luagfx.h"
static int env_pending(void) {
    for (;;) {
        if (sc_i >= sc_n) { if (++idle > 300000) { fprintf(stderr, "HARNESS: script exhausted but the program keeps polling\n"); exit(3); } return 0; }
        struct step *s = &sc[sc_i];
        if (s->t == 'K') return 1;
        sc_i++;
        if (s->t == 'M') { cur_mods = s->e; gx_mouse_feed(s->a, s->b, s->c, s->d); }
        else if (s->t == 'A') { cur_mods = s->e; gx_mouse_feed(s->a - gx_mx, s->b - gx_my, 0, s->d); }
        else if (s->t == 'S') env_shot(shot_names[s->a]);
        else if (s->t == 'F') s->fn();
        else if (s->t == 'W') return 0;
    }
}

static u8 fake_lfb[1280 * 1024 * 4];
static u8 gx_arena[64 * 1024 * 1024];
static int env_w = 1024, env_h = 768;
static int env_init(void) {
    FILE *f = fopen("/tmp/font8x16.bin", "rb");
    if (f) { size_t n = fread(gx_font, 1, 4096, f); (void)n; fclose(f); gx_font_ok = 1; }
    if (gx_setup(fake_lfb, (u32)env_w * 4, env_w, env_h, 16, 8, 0, gx_arena, sizeof gx_arena) < 0) return -1;
    return 0;
}
static u32 lfb_px(int x, int y) { return *(u32 *)(fake_lfb + (u32)y * gx_pitch + (u32)x * 4) & 0xFFFFFF; }
static int shot_n;
static void env_shot(const char *name) {
    gx_flip_dirty();
    char path[200]; snprintf(path, sizeof path, "/tmp/shots/%s.ppm", name);
    FILE *f = fopen(path, "wb"); if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", gx_w, gx_h);
    for (int y = 0; y < gx_h; y++) for (int x = 0; x < gx_w; x++) { u32 c = lfb_px(x, y); u8 rgb[3] = { (u8)(c >> 16), (u8)(c >> 8), (u8)c }; fwrite(rgb, 1, 3, f); }
    fclose(f); shot_n++;
}
static int run_lua(const char *src) { outn = 0; out[0] = 0; return lua_run_src(src, (int)strlen(src), "t"); }
