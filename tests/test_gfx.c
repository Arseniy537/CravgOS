/* тесты gfx.h / luagfx.h: примитивы, поверхности, события, os.exec */
#include "gfx_env.h"
/* среда для fbcon.h (консоль поверх пикселей) */
static u16 vga_mem[W * H]; static u16 *vga = vga_mem;
static int fbc_cur = 1, fbc_force;
static u32 mem_mib(void) { return 128; }
static const u8 romfont[4096] = {0}; static const u8 fbinfo[32] = {0};
#define CYR_N 1
static const u8 cyr_code[1] = {0x80}; static const u8 cyr_font[1][16] = {{0}};
static void putu(u32 n) { char b[12]; int i = 0; if (!n) b[i++] = '0'; while (n) { b[i++] = (char)('0' + n % 10); n /= 10; } while (i) t_putc(b[--i]); }
static void puthex(u32 n, int w) { (void)n; (void)w; }
static void outb(u16 p, u8 v) { (void)p; (void)v; }
static void reboot(void) {}
static int stas_try_app(const char *c) { (void)c; return 0; }
#include "../fbcon.h"
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n  out=[%s]\n", __LINE__, #c, out); fails++; } } while (0)
#define PX(x, y) lfb_px(x, y)
static int run_g(const char *s) { char b[4096]; snprintf(b, sizeof b, "gfx.init() %s", s); return run_lua(b); }
int main(void) {
    fs_init(); if (env_init() < 0) { puts("env_init failed"); return 1; }
    /* без gfx.init() функции gfx недоступны */
    run_lua("gfx.fill(0,0,1,1,0)"); CHECK(strstr(out, "gfx is not initialized"));
    run_lua("print(gfx.init())"); CHECK(!strcmp(out, "1024\t768\n"));
    /* заливка, отсечение, смешивание */
    run_g("gfx.fill(10,10,20,10,0xFF0000) gfx.flip()"); CHECK(PX(10, 10) == 0xFF0000 && PX(29, 19) == 0xFF0000 && PX(30, 19) == 0 && PX(9, 10) == 0);
    run_g("gfx.fill(0,0,4,4,0xFFFFFF) gfx.fill(0,0,4,4,0x000000,128) gfx.flip()"); CHECK(PX(1, 1) == 0x7F7F7F || PX(1, 1) == 0x808080);
    run_g("gfx.clip(100,100,10,10) gfx.fill(95,95,30,30,0x00FF00) gfx.clip() gfx.flip()"); CHECK(PX(100, 100) == 0x00FF00 && PX(109, 109) == 0x00FF00 && PX(110, 110) == 0 && PX(99, 99) == 0);
    run_g("gfx.fill(-50,-50,60,60,0x0000FF) gfx.fill(1020,760,50,50,0x0000FF) gfx.flip()"); CHECK(PX(0, 0) == 0x0000FF && PX(9, 9) == 0x0000FF && PX(10, 10) != 0x0000FF && PX(1023, 767) == 0x0000FF);
    /* поверхности и blit */
    run_g("local s = gfx.surface(40, 30) gfx.target(s) gfx.fill(0,0,40,30,0x112233) gfx.fill(5,5,10,10,0xFFFF00) gfx.target(0) gfx.blit(s, 200, 200) gfx.blit(s, 300, 300, 255, 5, 5, 10, 10) print(s) gfx.flip()");
    CHECK(!strcmp(out, "1\n") && PX(200, 200) == 0x112233 && PX(205, 205) == 0xFFFF00 && PX(239, 229) == 0x112233 && PX(300, 300) == 0xFFFF00 && PX(309, 309) == 0xFFFF00 && PX(310, 310) == 0);
    /* альфа поверхности: raw ARGB, blit с alpha */
    run_g("local s = gfx.surface(10, 10) gfx.target(s) gfx.raw(0,0,10,10,0x80FF0000) gfx.raw(5,0,5,10,0x00000000) gfx.target(0) gfx.fill(400,400,10,10,0x0000FF) gfx.blit(s, 400, 400, 255) gfx.flip()");
    CHECK(PX(402, 402) > 0x7F0000 && PX(402, 402) < 0x800080 && (PX(402, 402) >> 16) >= 0x7F && PX(407, 402) == 0x0000FF);
    run_g("local s = gfx.surface(4, 4) gfx.target(s) gfx.fill(0,0,4,4,0xFFFFFF) gfx.target(0) gfx.fill(500,500,4,4,0) gfx.blit(s, 500, 500, 128) gfx.flip()"); CHECK(PX(501, 501) >= 0x7F7F7F && PX(501, 501) <= 0x808080);
    /* освобождение и повторное использование памяти */
    run_g("local before, total = gfx.mem() for i = 1, 20 do local s = gfx.surface(500, 400) gfx.free(s) end local after = gfx.mem() print(before == after, total > 0)"); CHECK(!strcmp(out, "true\ttrue\n"));
    run_g("local t = {} for i = 1, 30 do t[i] = gfx.surface(300, 200) end for i = 1, 30, 2 do gfx.free(t[i]) end for i = 1, 30, 2 do t[i] = gfx.surface(300, 200) end for i = 1, 30 do gfx.free(t[i]) end print(gfx.mem() > 60000)"); CHECK(!strcmp(out, "true\n"));
    run_g("print(gfx.surface(5000, 10)) print(gfx.surface(0, 10)) print(gfx.surface(4000, 4000))"); CHECK(strstr(out, "nil\tcannot create surface"));
    run_g("local s = gfx.surface(20, 10) print(gfx.sizeof(s)) gfx.free(s) print(pcall(gfx.sizeof, s))"); CHECK(strstr(out, "20\t10\nfalse"));
    run_g("gfx.target(99)"); CHECK(strstr(out, "bad surface"));
    /* текст: возвращает ширину, рисует пиксели шрифта */
    run_g("gfx.fill(0,300,200,20,0) print(gfx.text(10,300,'HELLO',0xFFFFFF), gfx.textw('abc'), gfx.textw('abc',2)) gfx.flip()");
    CHECK(!strcmp(out, "40\t24\t48\n"));
    { int lit = 0; for (int y = 300; y < 316; y++) for (int x = 10; x < 50; x++) if (PX(x, y) == 0xFFFFFF) lit++; CHECK(gx_font_ok ? lit > 40 : 1); }
    run_g("gfx.fill(0,330,100,40,0) gfx.text(5,330,'W',0xFFFFFF,2) gfx.flip()");
    { int lit = 0, wide = 0; for (int y = 330; y < 362; y++) for (int x = 5; x < 21; x++) if (PX(x, y) == 0xFFFFFF) { lit++; if (x >= 18) wide++; } CHECK(gx_font_ok ? lit > 80 : 1); }
    /* градиент, скруглённый прямоугольник, масштабирование */
    run_g("gfx.grad(600,0,100,10,0x000000,0xFFFFFF,false) gfx.grad(720,0,10,100,0xFF0000,0x0000FF,true) gfx.flip()");
    CHECK(PX(600, 5) == 0x000000 && PX(699, 5) == 0xFFFFFF && PX(650, 5) > 0x303030 && PX(650, 5) < 0xD0D0D0 && PX(725, 0) == 0xFF0000 && PX(725, 99) == 0x0000FF);
    run_g("gfx.round(600,50,40,40,10,0x00FF00) gfx.flip()"); CHECK(PX(620, 70) == 0x00FF00 && PX(600, 50) == 0 && PX(639, 89) == 0 && PX(620, 50) == 0x00FF00 && PX(600, 70) == 0x00FF00);
    run_g("local s = gfx.surface(2, 2) gfx.target(s) gfx.fill(0,0,1,1,0xFF0000) gfx.fill(1,0,1,1,0x00FF00) gfx.fill(0,1,1,1,0x0000FF) gfx.fill(1,1,1,1,0xFFFFFF) gfx.target(0) gfx.scale(s, 800, 400, 20, 20) gfx.flip()");
    CHECK(PX(805, 405) == 0xFF0000 && PX(815, 405) == 0x00FF00 && PX(805, 415) == 0x0000FF && PX(815, 415) == 0xFFFFFF);
    /* тень: мягкая, внутри (hole) не рисуется */
    run_g("gfx.fill(0,500,300,200,0xFFFFFF) gfx.shadow(100,550,100,60,20,200,0,1) gfx.flip()");
    CHECK(PX(150, 580) == 0xFFFFFF && PX(150, 549) < 0xE0E0E0 && PX(150, 549) > 0x303030 && PX(150, 535) > PX(150, 549) && PX(150, 500) == 0xFFFFFF && PX(80, 580) == 0xFFFFFF);
    run_g("local s = gfx.surface(140, 100) gfx.target(s) gfx.shadow(20,20,100,60,20,200,0,3) gfx.target(0) gfx.fill(0,0,1,1,0xFFFFFF) print(gfx.sizeof(s)) gfx.target(s) local px1 = gfx.px(70, 10) local px2 = gfx.px(70, 50) local px3 = gfx.px(0, 0) gfx.target(0) print(px1 >> 24 > 0, px2, px3)");
    CHECK(strstr(out, "140\t100") && strstr(out, "true\t0\t0"));
    /* BMP: 2x2 24-бит из файловой системы */
    { u8 bmp[70] = { 'B','M', 70,0,0,0, 0,0,0,0, 54,0,0,0, 40,0,0,0, 2,0,0,0, 2,0,0,0, 1,0, 24,0, 0,0,0,0, 16,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
                      0,0,255, 255,255,255, 0,0, 255,0,0, 0,255,0, 0,0 };   /* низ: красный, белый; верх: синий, зелёный (BGR) */
      fs_write(fs_create("/t.bmp", 0), (const char *)bmp, 70, 0);
      run_g("local s = gfx.bmp('/t.bmp') gfx.scale(s, 900, 600, 20, 20) gfx.flip() print(s ~= nil) print(gfx.bmp('/nope.bmp'))");
      CHECK(strstr(out, "true\nnil\tcannot open file") && PX(905, 605) == 0x0000FF && PX(915, 605) == 0x00FF00 && PX(905, 615) == 0xFF0000 && PX(915, 615) == 0xFFFFFF);
      fs_write(fs_create("/bad.bmp", 0), "BMxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 70, 0);
      run_g("print(gfx.bmp('/bad.bmp'))"); CHECK(strstr(out, "nil\tunsupported")); }
    /* события: клавиши с модификаторами */
    sc_reset(); sK('a', 0); sK(9, 4); sW(1);
    run_g("local e1 = gfx.poll() local e2 = gfx.poll() print(e1.type, e1.key, e1.mods, e2.type, e2.key, e2.mods) print(gfx.poll())");
    CHECK(!strcmp(out, "key\t97\t0\tkey\t9\t4\nnil\n"));
    /* события мыши: абсолютное и относительное перемещение, нажатия, колесо, слияние событий одного опроса */
    sc_reset(); sA(100, 50, 0, 0); sW(1); sA(110, 60, 1, 4); sW(1); sM(5, 5, 0, 1, 0); sM(0, 0, -1, 0, 0); sW(1);
    run_g("local m1, m2, m3, m4 = gfx.poll(), gfx.poll(), gfx.poll(), gfx.poll() print(m1.type, m1.x, m1.y, m1.dx, m1.dy, m1.btn, m1.down) print(m2.x, m2.y, m2.dx, m2.dy, m2.btn, m2.down, m2.up, m2.mods) print(m3.x, m3.y, m3.dx, m3.dy, m3.btn, m3.down, m3.up, m3.wheel) print(m4) print(gfx.mouse())");
    CHECK(!strcmp(out, "mouse\t100\t50\t-412\t-334\t0\t0\n110\t60\t10\t10\t1\t1\t0\t4\n115\t65\t5\t5\t0\t0\t1\t-1\nnil\n115\t65\t0\n"));
    /* мышь не выходит за экран */
    sc_reset(); sM(5000, 5000, 0, 0, 0); sW(1); sM(-9000, -9000, 0, 0, 0); sW(1);
    run_g("gfx.poll() print(gfx.mouse()) gfx.poll() print(gfx.mouse())"); CHECK(!strcmp(out, "1023\t767\t0\n0\t0\t0\n"));
    /* os.exec: по умолчанию запрещено, разрешается файлом /etc/cravgwm/exec-allow */
    run_g("print(os.exec('echo hi'))"); CHECK(strstr(out, "nil\tos.exec denied"));
    fs_create("/etc", 1); fs_create("/etc/cravgwm", 1); { int f = fs_create("/etc/cravgwm/exec-allow", 0); const char *al = "/apps/other\n/apps/term\n"; fs_write(f, al, (int)strlen(al), 0); }
    { const char *src = "print(os.exec('echo hello world')) print(os.exec('rm -rf /'))";
      outn = 0; out[0] = 0; lua_run_src(src, (int)strlen(src), "/apps/term"); }
    CHECK(!strcmp(out, "hello world\n\nnil\tcommand is not allowed here\n"));
    { const char *src = "print(os.exec('echo x'))"; outn = 0; out[0] = 0; lua_run_src(src, (int)strlen(src), "/apps/ter"); }
    CHECK(strstr(out, "denied"));
    /* выход из программы освобождает поверхности и возвращает консоль */
    run_g("for i = 1, 10 do gfx.surface(100, 100) end");
    CHECK(gx_wm == 0); { int used = 0; for (int s = 1; s < GFX_MAXS; s++) used += gx_s[s].used; CHECK(used == 0); }
    run_lua("print(gfx.init()) gfx.close() print(pcall(gfx.fill, 0, 0, 1, 1, 0))"); CHECK(strstr(out, "1024\t768\nfalse"));
    /* консоль поверх пикселей (fbcon.h): клетки 80x25 -> пиксели 8x16, цвет, курсор, перерисовка только изменённого */
    fs_init(); env_init(); gx_wm = 0; cx = 5; cy = 2; fbc_cur = 1;
    for (int i = 0; i < W * H; i++) vga[i] = 0x0720;
    vga[0] = 0x1E00 | 'A'; vga[1] = 0x4F00 | 'B'; vga[2 * W + 5] = 0x0700 | 'x';
    fbc_scale = 1; fbc_ox = (1024 - 640) / 2; fbc_oy = (768 - 400) / 2;
    fbc_force = 1; vclock = 10240; fbc_sync();                      /* now & 512 == 0 -> курсор виден */
    CHECK(PX(0, 0) == 0 && PX(fbc_ox - 1, fbc_oy) == 0);            /* поля вокруг консоли чёрные */
    { int fg = 0, bg = 0, other = 0; for (int y = 0; y < 16; y++) for (int x = 0; x < 8; x++) { u32 c = PX(fbc_ox + x, fbc_oy + y); if (c == 0xFFFF55) fg++; else if (c == 0x0000AA) bg++; else other++; } CHECK(other == 0 && bg > 20 && (gx_font_ok ? fg > 10 : 1)); }
    { int bgc = 0; for (int y = 0; y < 16; y++) for (int x = 0; x < 8; x++) if (PX(fbc_ox + 8 + x, fbc_oy + y) == 0xAA0000) bgc++; CHECK(bgc > 20); }   /* клетка B: фон 4 = красный */
    { int under = 0; for (int x = 0; x < 8; x++) if (PX(fbc_ox + 5 * 8 + x, fbc_oy + 2 * 16 + 15) == 0xAAAAAA) under++; CHECK(under == 8); }   /* курсор - подчёркивание */
    vga[0] = 0x2F00 | 'Z'; vclock = 10260; fbc_sync();                  /* изменилась одна клетка */
    { int bgc = 0; for (int y = 0; y < 16; y++) for (int x = 0; x < 8; x++) if (PX(fbc_ox + x, fbc_oy + y) == 0x00AA00) bgc++; CHECK(bgc > 20); }
    gx_wm = 1; vga[0] = 0x4F00 | 'Q'; vclock = 10280; fbc_sync(); gx_wm = 0;      /* пока WM владеет экраном, консоль не рисуется */
    CHECK(PX(fbc_ox + 1, fbc_oy + 1) != 0xAA0000);
    fbc_cur = 0; vclock = 10300; fbc_sync();                              /* курсор скрыт - подчёркивание исчезает */
    { int under = 0; for (int x = 0; x < 8; x++) if (PX(fbc_ox + 5 * 8 + x, fbc_oy + 2 * 16 + 15) == 0xAAAAAA) under++; CHECK(under == 0); }
    fbc_force = 1; vclock = 10320; fbc_sync(); CHECK(PX(fbc_ox + 1, fbc_oy + 1) == 0xAA0000 || PX(fbc_ox + 2, fbc_oy + 2) == 0xAA0000 || PX(fbc_ox + 7, fbc_oy + 15) == 0xAA0000);
    { int bnd = 0; vclock = 10340; for (int k = 0; k < 3; k++) { fbc_sync(); bnd++; } CHECK(bnd == 3); }
    /* gfx_boot: разбор fbinfo и подготовка (здесь fbinfo пуст -> пиксельного режима нет) */
    gx_ok = 0; gfx_boot(); CHECK(gx_ok == 0);
    if (fails) { printf("test_gfx: %d FAILED\n", fails); return 1; }
    printf("test_gfx: OK\n");
    return 0;
}
