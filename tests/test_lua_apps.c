/* тесты программ из stas-repo/ (calendar, tetris, luaide) на интерпретаторе lua.h:
 * клавиши подаются по сценарию, экран снимается при каждом чтении клавиши */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32; typedef unsigned long long u64;
#include "../fs.h"
#define W 80
#define H 25
static int cx, cy; static u8 color = 0x07;
static char out[1 << 16]; static int outn;
static u16 scr[W * H];
static void t_putc(char c) { if (outn < (int)sizeof out - 1) out[outn++] = c; out[outn] = 0; }
#define putc t_putc
static void puts_(const char *s) { while (*s) t_putc(*s++); }
#define puts puts_
static void put_at(int x, int y, char c, u8 attr) { scr[y * W + x] = (u16)((attr << 8) | (u8)c); }
static void clear(void) { for (int i = 0; i < W * H; i++) scr[i] = (u16)((color << 8) | ' '); cx = cy = 0; }
static void cursor(void) {} static void cursor_hide(void) {} static void cursor_show(void) {}
#define MAXSNAP 400
static u16 snaps[MAXSNAP][W * H]; static int nsnap;
static int keyq[2000], kq_n, kq_i;
static int blocking_keys;                         /* считаем getkey/key: после очереди выдаём 'q' */
static int key_poll(void) {
    if (nsnap < MAXSNAP) memcpy(snaps[nsnap++], scr, sizeof scr);
    if (kq_i < kq_n) return keyq[kq_i++];
    return 'q';
}
static const char *linesq[16]; static int lq_n, lq_i;
static int readline(char *b, int max) { const char *s = lq_i < lq_n ? linesq[lq_i++] : ""; int n = 0; while (s[n] && n < max - 1) { b[n] = s[n]; n++; } b[n] = 0; return n; }
static u32 now_ms(void) { static u32 t; return t += 7; }
static void delay_ms(u32 ms) { (void)ms; }
static void fs_sync(void) {}
static void lua_rtc(int *y, int *mo, int *d, int *h, int *mi, int *s) { *y = 2026; *mo = 11; *d = 7; *h = 12; *mi = 34; *s = 56; }
static char edited[8][64]; static int nedit; static int lua_edit(const char *p) { if (nedit < 8) strncpy(edited[nedit++], p, 63); return 1; }
#include "../lua.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
static void keys(const int *k, int n) { kq_n = n; kq_i = 0; for (int i = 0; i < n; i++) keyq[i] = k[i]; }
static int row_has(const u16 *s, int y, const char *t) {          /* строка экрана y содержит текст t */
    char row[W + 1]; for (int x = 0; x < W; x++) row[x] = (char)(s[y * W + x] & 255); row[W] = 0;
    return strstr(row, t) != 0;
}
static int snap_has(int i, const char *t) { for (int y = 0; y < H; y++) if (row_has(snaps[i], y, t)) return 1; return 0; }
static int any_snap_has(const char *t) { for (int i = 0; i < nsnap; i++) if (snap_has(i, t)) return 1; return 0; }
static int install(const char *name) {
    char path[100]; snprintf(path, sizeof path, "stas-repo/%s.stp", name);
    FILE *f = fopen(path, "rb"); if (!f) { printf("cannot open %s (run from repo root)\n", path); exit(2); }
    char b[4096]; int n = (int)fread(b, 1, sizeof b, f); fclose(f);
    if (n > FS_FSZ) { printf("FAIL: %s is %d bytes (max 2048)\n", name, n); fails++; return -1; }
    if (fs_resolve("/apps") < 0) fs_create("/apps", 1);
    char p[64]; snprintf(p, sizeof p, "/apps/%s", name);
    int i = fs_resolve(p); if (i < 0) i = fs_create(p, 0);
    fs_write(i, b, n, 0);
    return i;
}
static int run_app(const char *name) {
    char p[64]; snprintf(p, sizeof p, "/apps/%s", name); int i = fs_resolve(p);
    outn = 0; out[0] = 0; nsnap = 0; lq_i = 0; nedit = 0;
    return lua_run_src((const char *)fsdata[i], fsn[i].size, name);
}
static void run_src(const char *src) { outn = 0; out[0] = 0; nsnap = 0; lua_run_src(src, (int)strlen(src), "t"); }

int main(void) {
    fs_init();
    static const char *all[] = { "calendar", "tetris", "tetris-lib", "luaide", "luaide-ui", "luaide-hl", "luaide-help", "luaide-repl" };
    for (int i = 0; i < 8; i++) install(all[i]);
    if (fails) return 1;

    /* ---- calendar ---- */
    clear(); color = 0x07; keys(0, 0); run_app("calendar");
    CHECK(!strstr(out, "lua: "));
    CHECK(snap_has(0, "November 2026"));
    CHECK(snap_has(0, "Mo  Tu  We  Th  Fr  Sa  Su"));
    CHECK(row_has(snaps[0], 7, " 1") && (snaps[0][7 * W + 50] & 255) == ' ' && (snaps[0][7 * W + 51] & 255) == '1');   /* 1 ноября 2026 - воскресенье */
    CHECK((snaps[0][9 * W + 46] & 255) == ' ' && (snaps[0][9 * W + 47] & 255) == '7' && (snaps[0][9 * W + 47] >> 8) == 0xA0);   /* сегодня 7-е: подсвечено, суббота */
    CHECK(row_has(snaps[0], 17, "30"));                                                                 /* 30 ноября - понедельник */
    CHECK(!snap_has(0, "31"));                                                                          /* в ноябре 30 дней */
    CHECK((snaps[0][7 * W + 51] >> 8) == 0x0C);                                                         /* воскресенье красным */
    { int k[] = { K_RIGHT, K_RIGHT, K_RIGHT, 't', 'g', K_DOWN };
      clear(); keys(k, 6); linesq[0] = "2024 2"; lq_n = 1; run_app("calendar");
      CHECK(!strstr(out, "lua: "));
      CHECK(snap_has(1, "December 2026") && (snaps[1][7 * W + 31] & 255) == '1' && (snaps[1][7 * W + 30] & 255) == ' ');   /* 1 декабря - вторник */
      CHECK(snap_has(2, "January 2027") && (snaps[2][7 * W + 43] & 255) == '1');                       /* пятница */
      CHECK(snap_has(3, "February 2027") && row_has(snaps[3], 13, "28") && !snap_has(3, "29"));
      CHECK(snap_has(4, "November 2026"));
      CHECK(snap_has(5, "February 2024") && row_has(snaps[5], 15, "29") && !snap_has(5, "30") && (snaps[5][7 * W + 39] & 255) == '1');
      CHECK(snap_has(6, "February 2023") && !snap_has(6, "29"));
      CHECK(nsnap == 7); }

    /* ---- tetris-lib: логика ---- */
    run_src("local G = dofile('/apps/tetris-lib') math.randomseed(7) G.new()\n"
            "print(G.fits({0,0,0,1,0,2,0,3}, -1, 0), G.fits({0,0,0,1,0,2,0,3}, 9, 16), G.fits({0,0,0,1,0,2,0,3}, 9, 17))\n"
            "for y = 18, 19 do for x = 0, 8 do G.B[y * 10 + x] = 1 end end\n"
            "G.t = 1 G.c = {0,-3,0,-2,0,-1,0,0} G.px, G.py = 9, 19 G.lock()\n"
            "print(G.ln, G.sc, G.B[190], G.B[189], G.B[179], G.dead)\n"
            "G.t = 3 G.c = G.S[3] G.px, G.py = 4, 5 local o = table.concat(G.c, ',') for i = 1, 4 do G.rot() end\n"
            "print(table.concat(G.c, ',') == o)\n"
            "G.t = 2 G.c = G.S[2] G.px, G.py = 0, 5 for i = 1, 3 do G.rot() end print(G.px >= 0 and G.fits(G.c, G.px, G.py))\n"
            "for x = 0, 9 do G.B[x] = 1 G.B[10 + x] = 1 end G.spawn() print(G.dead) G.new() print(G.dead, G.sc, G.ln)\n");
    CHECK(!strstr(out, "lua: "));
    CHECK(!strcmp(out, "false\ttrue\tfalse\n2\t300\tnil\t1\tnil\tfalse\ntrue\ntrue\ntrue\nfalse\t0\t0\n"));
    if (strcmp(out, "false\ttrue\tfalse\n2\t300\tnil\t1\tnil\tfalse\ntrue\ntrue\ntrue\nfalse\t0\t0\n")) printf("  tetris-lib output: [%s]\n", out);

    /* ---- tetris: игра ---- */
    { int k[60]; int n = 0;
      k[n++] = K_LEFT; k[n++] = K_LEFT; k[n++] = K_UP; k[n++] = K_RIGHT; k[n++] = K_DOWN;
      for (int i = 0; i < 4; i++) k[n++] = ' ';
      int after4 = n;
      for (int i = 0; i < 20; i++) k[n++] = ' ';            /* башня до верха: игра заканчивается */
      int before_enter = n; k[n++] = 10;                     /* Enter: новая игра */
      for (int i = 0; i < 2; i++) k[n++] = ' ';
      { int old = fs_resolve("/lua/.tetris"); if (old >= 0) fs_remove(old); }
      clear(); keys(k, n); run_app("tetris");
      CHECK(!strstr(out, "lua: "));
      CHECK(snap_has(0, "SCORE 0") && snap_has(0, "NEXT") && snap_has(0, "LEVEL 1"));
      CHECK(snap_has(0, "+--------------------+"));
      { int cells = 0; for (int y = 2; y < 22; y++) for (int x = 31; x < 51; x++) if ((snaps[1][y * W + x] & 255) == '[') cells++; CHECK(cells == 4); }   /* на старте на поле одна фигура */
      CHECK(snap_has(after4, "SCORE ") && !snap_has(after4, "SCORE 0 "));                                   /* очки за падение растут */
      CHECK(any_snap_has("GAME OVER - Enter"));
      CHECK(!snap_has(before_enter + 1, "GAME OVER"));
      CHECK(snap_has(before_enter + 1, "SCORE 0"));                                                         /* после Enter - новая игра */
      CHECK(snap_has(before_enter + 1, "BEST  ") && !snap_has(before_enter + 1, "BEST  0"));              /* рекорд сохранился */
      { int i = fs_resolve("/lua/.tetris"); CHECK(i >= 0 && fsn[i].size > 0 && fsdata[i][0] >= '1' && fsdata[i][0] <= '9'); } }

    /* ---- luaide ---- */
    fs_create("/lua", 1);
    { int i; const char *a = "print(\"hi\")\n", *b = "x = = 1\n", *z = "term.write('ran-z')\n";
      i = fs_create("/lua/a.lua", 0); fs_write(i, a, (int)strlen(a), 0);
      i = fs_create("/lua/bad.lua", 0); fs_write(i, b, (int)strlen(b), 0);
      i = fs_create("/lua/z.lua", 0); fs_write(i, z, (int)strlen(z), 0); }
    { int k[] = { K_DOWN, 'c', K_UP, 'c', 'r', ' ', 'k', 'h', 'a', 'q', 'n', 'd', K_DOWN, K_DOWN, 'x', ' ', 10 };
      static const char *ln[] = { "1+2", "x=5", "x*2", "nosuch(", "exit", "new", "y" };
      for (int q = 0; q < 7; q++) { linesq[q] = ln[q]; }
      lq_n = 7;
      clear(); keys(k, 17); run_app("luaide");
      CHECK(!strstr(out, "lua: "));
      CHECK(snap_has(0, "Lua IDE") && snap_has(0, "3 files") && snap_has(0, "a.lua") && snap_has(0, "bad.lua") && snap_has(0, "z.lua"));
      CHECK(snap_has(0, "print(\"hi\")") && snap_has(0, "Enter: edit"));
      CHECK(snap_has(1, "x = = 1"));                                                                        /* выбран bad.lua: предпросмотр */
      CHECK(snap_has(2, "bad.lua:1: unexpected symbol"));                                                   /* проверка синтаксиса */
      CHECK(snap_has(4, "syntax OK: a.lua"));
      CHECK(strstr(out, "hi\n") && strstr(out, "-- finished -- press any key"));                            /* запуск */
      CHECK(snap_has(5, "ran a.lua") || snap_has(6, "ran a.lua"));
      CHECK(strstr(out, "Lua console") && strstr(out, "3  \n") && strstr(out, "10  \n") && strstr(out, "console:1:"));
      CHECK(any_snap_has("Lua reference 1/2") && any_snap_has("Lua reference 2/2") && strstr(out, "term.getkey() waits"));
      CHECK(nedit == 3 && !strcmp(edited[0], "/lua/new.lua") && !strcmp(edited[1], "/lua/z.lua") && !strcmp(edited[2], "/lua/z.lua"));
      CHECK(fs_resolve("/lua/new.lua") < 0);                                                                /* создан и удалён */
      CHECK(any_snap_has("deleted new.lua"));
      CHECK(strstr(out, "ran-z"));
      CHECK(any_snap_has("new.lua") && any_snap_has("Hello, Cravgos"));                                      /* новый файл виден в списке с шаблоном */ }

    /* IDE: защита от ошибок в пользовательской программе */
    { int i = fs_create("/lua/boom.lua", 0); const char *b = "error('boom')\n"; fs_write(i, b, (int)strlen(b), 0);
      int k[] = { 'r', ' ' };
      /* boom.lua идёт после a.lua и bad.lua (список отсортирован): a, bad, boom, z */
      int k2[] = { K_DOWN, K_DOWN, 'r', ' ' };
      (void)k; clear(); keys(k2, 4); run_app("luaide");
      CHECK(!strstr(out, "lua: ") && strstr(out, "boom.lua:1: boom") && any_snap_has("error in boom.lua")); }

    if (fails) { printf("test_lua_apps: %d FAILED\n", fails); return 1; }
    printf("test_lua_apps: OK\n");
    return 0;
}
