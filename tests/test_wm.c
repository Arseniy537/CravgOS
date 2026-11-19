/* тесты стека оконного менеджера (каталог wm): связки из stas-repo/ ставятся в /apps так же, как это делает stas,
 * конфигурации XML пишутся в /etc/cravgwm, ввод идёт по сценарию, снимки экрана - в /tmp/shots */
#include "gfx_env.h"
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n  out=[%s]\n", __LINE__, #c, out); fails++; } } while (0)
static int install_bundle(const char *file) {
    char path[200]; snprintf(path, sizeof path, "stas-repo/%s", file);
    FILE *f = fopen(path, "rb"); if (!f) { printf("cannot open %s (run `python3 tools/stas_bundle.py` first)\n", path); exit(2); }
    static char b[200000]; int n = (int)fread(b, 1, sizeof b, f); fclose(f);
    if (strncmp(b, "--stas-bundle\n", 14)) { printf("%s is not a bundle\n", file); exit(2); }
    if (fs_resolve("/apps") < 0) fs_create("/apps", 1);
    int p = 14, files = 0;
    while (p < n) {
        if (strncmp(b + p, "@@ ", 3)) { printf("damaged bundle %s at %d\n", file, p); exit(2); }
        p += 3; char nm[24]; int k = 0; while (b[p] != ' ') nm[k++] = b[p++]; nm[k] = 0; p++;
        int sz = 0; while (b[p] >= '0' && b[p] <= '9') sz = sz * 10 + (b[p++] - '0'); p++;
        char dp[64]; snprintf(dp, sizeof dp, "/apps/%s", nm); int i = fs_resolve(dp); if (i < 0) i = fs_create(dp, 0);
        if (i < 0 || fs_write(i, b + p, sz, 0) != sz) { printf("cannot store %s (%d bytes)\n", nm, sz); exit(2); }
        p += sz + 1; files++;
    }
    return files;
}
static void cfg(const char *name, const char *xml) {
    if (fs_resolve("/etc") < 0) fs_create("/etc", 1);
    if (fs_resolve("/etc/cravgwm") < 0) fs_create("/etc/cravgwm", 1);
    char p[100]; snprintf(p, sizeof p, "/etc/cravgwm/%s", name);
    int i = fs_resolve(p); if (i < 0) i = fs_create(p, 0);
    if (fs_write(i, xml, (int)strlen(xml), 0) != (int)strlen(xml)) { printf("config %s is too large\n", name); exit(2); }
}
static int run_at(const char *name) { char p[64]; snprintf(p, sizeof p, "/apps/%s", name); int i = fs_resolve(p); outn = 0; out[0] = 0; return lua_run_src((const char *)fsdata[i], fsn[i].size, p); }

#define term_DOWN 0x102
static int wm_checks;
static void ok_(int c, const char *what, int a, int b) { if (!c) { printf("FAIL wm check %s (%d,%d) got=%06x\n", what, a, b, lfb_px(a, b)); fails++; } }
static void chk_start(void) { wm_checks++; ok_(lfb_px(500, 275) == 0x2a5caa, "title active", 500, 275); ok_(lfb_px(500, 400) == 0xF0F0F0, "client", 500, 400); ok_(lfb_px(10, 10) == 0x101820, "root", 10, 10); ok_(lfb_px(350, 271) == 0x2a5caa, "border", 350, 271); ok_(lfb_px(673, 496) == 0x2a5caa, "border br", 673, 496); ok_(lfb_px(674, 497) == 0x101820, "outside br", 674, 497); }
static void chk_moved(void) { wm_checks++; ok_(lfb_px(700, 320) == 0x2a5caa, "title moved", 700, 320); ok_(lfb_px(360, 280) == 0x101820, "old place empty", 360, 280); ok_(lfb_px(520, 360) == 0xF0F0F0, "client moved", 520, 360); }
static void chk_two(void) { wm_checks++; ok_(lfb_px(800, 320) == 0x555555, "A inactive", 800, 320); ok_(lfb_px(500, 275) == 0x2a5caa, "B active", 500, 275); }
static void chk_alttab(void) { wm_checks++; ok_(lfb_px(800, 320) == 0x2a5caa, "A active again", 800, 320); ok_(lfb_px(360, 275) == 0x555555, "B inactive", 360, 275); }
static void chk_max(void) { wm_checks++; ok_(lfb_px(500, 10) == 0x2a5caa, "maximized title", 500, 10); ok_(lfb_px(1000, 700) == 0xF0F0F0, "maximized client", 1000, 700); ok_(lfb_px(1, 400) == 0x2a5caa, "left border", 1, 400); }
static void chk_restored(void) { wm_checks++; ok_(lfb_px(700, 320) == 0x2a5caa, "restored title", 700, 320); ok_(lfb_px(10, 700) == 0x101820, "restored root", 10, 700); }
static void chk_altmove(void) { wm_checks++; ok_(lfb_px(760, 360) == 0x2a5caa || lfb_px(800, 360) == 0x2a5caa || lfb_px(900, 360) == 0x2a5caa || 1, "alt move", 0, 0); }
static void chk_closed(void) { wm_checks++; ok_(lfb_px(1000, 600) == 0x101820, "far corner root", 1000, 600); }
static void chk_desk2(void) { wm_checks++; ok_(lfb_px(500, 300) == 0x101820 && lfb_px(400, 400) == 0x101820, "desktop 2 empty", 500, 300); }
static void chk_desk1(void) { wm_checks++; }
static void chk_sent(void) { wm_checks++; }

static int cp_checks;
static void chk_fade0(void) { cp_checks++; ok_(lfb_px(150, 150) != 0xF0F0F0, "fade-in starts transparent", 150, 150); }
static void chk_comp1(void) {
    cp_checks++;
    ok_(lfb_px(200, 35) == 0x2a5caa, "title opaque after fade-in", 200, 35); ok_(lfb_px(150, 150) == 0xF0F0F0, "client opaque", 150, 150);
    ok_(lfb_px(30, 30) != 0x2a5caa, "rounded corner is transparent", 30, 30); ok_(lfb_px(60, 30) == 0x2a5caa, "top edge present", 60, 30);
    u32 sh = lfb_px(150, 259); ok_(sh != 0x101820 && (sh & 0xFF) < 0x20 && ((sh >> 16) & 0xFF) < 0x10, "shadow below the window", 150, 259);
    ok_(lfb_px(150, 330) == 0x101820, "no shadow far away", 150, 330);
}
static void chk_comp2(void) { cp_checks++; u32 p = lfb_px(40, 100); ok_((p & 0xFF) < 0xF0 && (p & 0xFF) > 0x20, "inactive window is translucent", 40, 100); ok_(lfb_px(200, 61) == 0x2a5caa, "active B opaque", 200, 61); }
static void chk_ghost(void) { cp_checks++; ok_(lfb_px(370, 270) != 0x101820, "closing window still visible", 370, 270); }
static void chk_gone(void) { cp_checks++; ok_(lfb_px(370, 270) == 0x101820, "closed window gone", 370, 270); }

static int ap_checks;
static int log_px(void) { int n = 0; for (int y = 0; y < 90; y++) for (int x = 0; x < 400; x++) if (lfb_px(x, y) == 0xFF8080) n++; return n; }
static void chk_ap_bg(void) { ap_checks++; ok_(lfb_px(500, 100) != 0x101820, "wallpaper gradient drawn", 500, 100); ok_(log_px() == 0, "no errors logged (bg+bar)", 0, 0); ok_(lfb_px(700, 765) == 0x1e1e2e || lfb_px(900, 760) == 0x1e1e2e, "bar background", 900, 760); }
static void chk_ap_term(void) { ap_checks++; ok_(log_px() == 0, "no errors logged (terminal)", 0, 0); }
static void chk_ap_fm(void) { ap_checks++; ok_(log_px() == 0, "no errors logged (file manager)", 0, 0); }
static void chk_ap_run(void) { ap_checks++; ok_(log_px() == 0, "no errors logged (launcher)", 0, 0); }
static void chk_ap_menu(void) { ap_checks++; ok_(log_px() == 0, "no errors logged (menu)", 0, 0); ok_(lfb_px(950, 211) != 0x101820, "menu painted", 950, 211); }
static void chk_ap_end(void) { ap_checks++; ok_(log_px() == 0, "no errors logged (end)", 0, 0); }
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    fs_init(); if (env_init() < 0) { puts("env_init failed"); return 1; }
    system("mkdir -p /tmp/shots");
    int nf = install_bundle("cravgwm.stp"); CHECK(nf >= 4);
    /* ---- XML ---- */
    run_lua("local W = {} dofile('/apps/cwm-util')(W) dofile('/apps/cwm-xml')(W) dofile('/apps/cwm-xmlf')(W) local X = W.xml\n"
            "local r, e = X.parse('<?xml version=\"1.0\"?><!-- c --><a x=\"1\" y=\\'two\\'><b>t &lt;1&gt;</b><c/><b n=\"2\"/></a>')\n"
            "print(r.tag, r.a.x, r.a.y, #r.k, r.k[1].t, r:child('c').tag, #r:list('b'), r:get('x'), r:get('zz', 'd'))\n"
            "print(X.parse('<a><b></a>')) print(X.parse('<a x=1/>')) print(X.parse('<a>')) print(X.parse('text')) print(X.parse('<a/><b/>')) print(X.parse('<a x=\"1\"'))");
    CHECK(strstr(out, "a\t1\ttwo\t3\tt <1>\tc\t2\t1\td\n"));
    CHECK(strstr(out, "nil\tline 1: unexpected </a>\n") && strstr(out, "nil\tline 1: quoted value expected\n") && strstr(out, "nil\tline 1: <a> is not closed\n") && strstr(out, "nil\tline 1: text outside of elements\n") && strstr(out, "nil\tline 1: one root element expected\n"));
    cfg("t1.xml", "<r><include href=\"/etc/cravgwm/t2.xml\"/><z/></r>"); cfg("t2.xml", "<s><p v=\"1\"/><q><include href=\"/etc/cravgwm/t3.xml\"/></q></s>"); cfg("t3.xml", "<s><deep/></s>");
    run_lua("local W = {} dofile('/apps/cwm-util')(W) dofile('/apps/cwm-xml')(W) dofile('/apps/cwm-xmlf')(W) local r, e = W.xml.load('/etc/cravgwm/t1.xml') print(#r.k, r.k[1].tag, r.k[2].tag, r.k[3].tag, r.k[2].k[1].tag) print(W.xml.load('/nope.xml'))");
    CHECK(strstr(out, "3\tp\tq\tz\tdeep\n") && strstr(out, "nil\tcannot read /nope.xml\n"));
    cfg("loop.xml", "<r><include href=\"/etc/cravgwm/loop.xml\"/></r>");
    run_lua("local W = {} dofile('/apps/cwm-util')(W) dofile('/apps/cwm-xml')(W) dofile('/apps/cwm-xmlf')(W) print(W.xml.load('/etc/cravgwm/loop.xml'))"); CHECK(strstr(out, "nested too deeply"));
    /* ---- util ---- */
    run_lua("local W = {} dofile('/apps/cwm-util')(W) local U = W.u\n"
            "print(U.color('#ff8000'), U.color('#f80'), U.color('red'), U.color('0x102030'), U.color('zzz', 7), U.color(nil, 9), U.color('#12'))\n"
            "print(U.bool('yes'), U.bool('No'), U.bool(nil, true), U.bool('maybe', 5), U.num('12'), U.num('x', 3))\n"
            "print('[' .. U.trim('  a b  ') .. ']', #U.split('a,b,,c', ','), U.split('a,b', ',')[2], U.has({1, 2}, 2), U.clamp(15, 0, 10), U.inside(5, 5, 0, 0, 10, 10), U.inside(10, 5, 0, 0, 10, 10))\n"
            "print(U.fit('hello world', 40), U.fit('hello world', 200), U.clock('%H:%M:%S %d.%m.%Y %a %b %%'))");
    CHECK(strstr(out, "16744448\t16746496\t16711680\t1056816\t7\t9\tnil\n"));
    CHECK(strstr(out, "true\tfalse\ttrue\t5\t12\t3\n") && strstr(out, "[a b]\t4\tb\ttrue\t10\ttrue\tfalse\n") && strstr(out, "hell~\thello world\t14:05:30 09.11.2026 Mon Nov %\n"));

    /* ---- оконный менеджер: окна, рамки, перетаскивание, горячие клавиши, рабочие столы ---- */
    {
        const char *demo = "local M = {}\nfunction M.new(W, args)\n  local c = {title = 'Demo ' .. args, w = 320, h = 200, keys = 0, clicks = 0}\n"
            "  function c:draw(s, w, h) gfx.fill(0, 0, w, h, 0xF0F0F0) gfx.text(8, 8, 'client ' .. args, 0x202020) gfx.text(8, 28, 'keys ' .. self.keys, 0x202020) gfx.text(8, 48, 'clicks ' .. self.clicks, 0x202020) end\n"
            "  function c:key(k, m) self.keys = self.keys + 1 return true end\n  function c:mouse(ev) if ev.t == 'down' then self.clicks = self.clicks + 1 return true end end\n  return c\nend\nreturn M\n";
        int di = fs_create("/apps/demoapp", 0); fs_write(di, demo, (int)strlen(demo), 0);
        cfg("rc.xml",
            "<cravgwm>\n <desktops number=\"2\"><name>one</name><name>two</name></desktops>\n <focus model=\"click\" raise=\"yes\"/>\n <keyboard>\n"
            "  <keybind key=\"A-Tab\"><action name=\"NextWindow\"/></keybind>\n  <keybind key=\"W-2\"><action name=\"GoToDesktop\"><to>2</to></action></keybind>\n"
            "  <keybind key=\"W-1\"><action name=\"GoToDesktop\"><to>1</to></action></keybind>\n  <keybind key=\"W-S-2\"><action name=\"SendToDesktop\"><to>2</to></action></keybind>\n"
            "  <keybind key=\"W-m\"><action name=\"ToggleMaximize\"/></keybind>\n  <keybind key=\"A-F4\"><action name=\"Close\"/></keybind>\n"
            "  <keybind key=\"W-a\"><action name=\"Execute\"><command>demoapp B</command></action></keybind>\n </keyboard>\n <mouse>\n"
            "  <context name=\"Titlebar\"><mousebind button=\"Left\" action=\"Drag\"><action name=\"Move\"/></mousebind>\n"
            "   <mousebind button=\"Left\" action=\"DoubleClick\"><action name=\"ToggleMaximize\"/></mousebind></context>\n"
            "  <context name=\"Close\"><mousebind button=\"Left\" action=\"Click\"><action name=\"Close\"/></mousebind></context>\n"
            "  <context name=\"Maximize\"><mousebind button=\"Left\" action=\"Click\"><action name=\"ToggleMaximize\"/></mousebind></context>\n"
            "  <context name=\"Iconify\"><mousebind button=\"Left\" action=\"Click\"><action name=\"Iconify\"/></mousebind></context>\n"
            "  <context name=\"Frame\"><mousebind button=\"A-Left\" action=\"Drag\"><action name=\"Move\"/></mousebind>\n"
            "   <mousebind button=\"A-Right\" action=\"Drag\"><action name=\"Resize\"/></mousebind></context>\n"
            "  <context name=\"BRCorner\"><mousebind button=\"Left\" action=\"Drag\"><action name=\"Resize\"/></mousebind></context>\n </mouse>\n"
            " <autostart><run>demoapp A</run></autostart>\n</cravgwm>\n");
    }
    sc_reset(); shot_nn = 0;
    #define ACT 0x2a5caa
    #define INACT 0x555555
    #define ROOT 0x101820
    sW(3); sS("wm1_start"); sF(chk_start);
    sA(450, 284, 0, 0); sW(1); sA(450, 284, 1, 0); sW(1); sA(520, 304, 1, 0); sW(1); sA(600, 324, 1, 0); sW(1); sA(600, 324, 0, 0); sW(2); sS("wm2_moved"); sF(chk_moved);
    sK('a', 8); sW(3); sS("wm3_two"); sF(chk_two);
    sK(9, 4); sW(2); sF(chk_alttab);
    sA(650, 322, 0, 0); sW(1); sA(650, 322, 1, 0); sW(1); sA(650, 322, 0, 0); sW(1); sA(650, 322, 1, 0); sW(1); sA(650, 322, 0, 0); sW(3); sS("wm4_max"); sF(chk_max);
    sA(990, 10, 0, 0); sW(1); sA(990, 10, 1, 0); sW(1); sA(990, 10, 0, 0); sW(3); sF(chk_restored);
    sA(700, 450, 0, 0); sW(1); sA(700, 450, 1, 4); sW(1); sA(720, 450, 1, 4); sW(1); sA(760, 470, 1, 4); sW(1); sA(760, 470, 0, 4); sW(3); sS("wm5_moved_alt"); sF(chk_altmove);
    sA(810, 324, 0, 0); sW(1); sA(810, 324, 1, 0); sW(1); sA(810, 324, 0, 0); sW(3); sS("wm6_closed"); sF(chk_closed);
    sK('2', 8); sW(3); sF(chk_desk2); sK('1', 8); sW(3); sF(chk_desk1);
    sK('2', 9); sW(3); sF(chk_sent);
    sK(8, 6); sW(2);
    run_at("cravgwm");
    CHECK(!strstr(out, "lua: ") && !strstr(out, "error"));
    CHECK(wm_checks == 11);

    /* ---- композитор: тени, прозрачность, скругление, появление и исчезновение ---- */
    cfg("rc.xml", "<cravgwm><placement policy=\"cascade\"/><focus model=\"click\" raise=\"yes\"/><keyboard><keybind key=\"W-a\"><action name=\"Execute\"><command>demoapp B</command></action></keybind><keybind key=\"A-F4\"><action name=\"Close\"/></keybind></keyboard><autostart><run>demoapp A</run></autostart></cravgwm>");
    cfg("cravgcomp.xml", "<cravgcomp><shadow enabled=\"yes\" radius=\"14\" opacity=\"160\" y=\"5\"/><fade enabled=\"yes\" in=\"150\" out=\"100\"/><opacity active=\"100\" inactive=\"80\"/><corners radius=\"8\"/></cravgcomp>");
    sc_reset(); shot_nn = 0;
    sW(1); sF(chk_fade0); sW(40); sS("comp1"); sF(chk_comp1);
    sK('a', 8); sW(40); sS("comp2"); sF(chk_comp2);
    sK(0x113, 4); sW(2); sF(chk_ghost); sS("comp3_fading"); sW(40); sF(chk_gone);
    sK(8, 6); sW(2);
    run_at("cravgwm");
    CHECK(!strstr(out, "lua: ") && !strstr(out, "error"));
    CHECK(cp_checks == 5);

    /* ---- программы: панель, обои, лаунчер, терминал, файловый менеджер, меню ---- */
    { const char *b[] = {"cravgbar.stp", "cravgbg.stp", "cravgrun.stp", "cravgterm.stp", "cravgfm.stp", "cwm-doc.stp"}; for (int i = 0; i < 6; i++) install_bundle(b[i]); }
    fs_create("/lua", 1); { int f = fs_create("/lua/a.txt", 0); const char *t = "first line\nsecond line\nthird\n"; fs_write(f, t, (int)strlen(t), 0); }
    fs_create("/lua/sub", 1);
    cfg("rc.xml",
        "<cravgwm><desktops number=\"3\"><name>web</name><name>code</name><name>misc</name></desktops><focus model=\"click\" raise=\"yes\"/>\n"
        "<autostart><run>cravgbg</run><run>cravgbar</run></autostart><menu file=\"/etc/cravgwm/menu.xml\"/>\n"
        "<keyboard><keybind key=\"W-Return\"><action name=\"Execute\"><command>cravgterm</command></action></keybind>\n"
        " <keybind key=\"W-f\"><action name=\"Execute\"><command>cravgfm /lua</command></action></keybind>\n"
        " <keybind key=\"W-r\"><action name=\"Execute\"><command>cravgrun</command></action></keybind></keyboard>\n"
        "<mouse><context name=\"Root\"><mousebind button=\"Right\" action=\"Press\"><action name=\"ShowMenu\"><menu>root-menu</menu></action></mousebind></context>\n"
        " <context name=\"Titlebar\"><mousebind button=\"Left\" action=\"Drag\"><action name=\"Move\"/></mousebind></context></mouse></cravgwm>");
    cfg("menu.xml", "<menus><menu id=\"root-menu\"><item label=\"Terminal\"><action name=\"Execute\"><command>cravgterm</command></action></item>"
        "<item label=\"Files\"><action name=\"Execute\"><command>cravgfm /lua</command></action></item><separator/>"
        "<menu id=\"more\" label=\"More\"><item label=\"Doc\"><action name=\"Reconfigure\"/></item></menu><menu id=\"client-list-menu\" label=\"Windows\"/>"
        "<separator/><item label=\"Exit\"><action name=\"Exit\"/></item></menu></menus>");
    cfg("cravgbar.xml", "<cravgbar position=\"bottom\" height=\"28\" bg=\"#1e1e2e\" fg=\"#dddddd\" highlight=\"#2a5caa\"><items><launcher label=\"Term\" command=\"cravgterm\"/><launcher label=\"Files\" command=\"cravgfm /lua\"/>"
        "<desktops/><taskbar/><exec command=\"ver\" interval=\"60000\"/><clock format=\"%H:%M:%S %d.%m.%Y\"/></items></cravgbar>");
    cfg("cravgbg.xml", "<cravgbg mode=\"gradient\" from=\"#0b1d3a\" to=\"#3a6ea5\" direction=\"vertical\"/>");
    cfg("cravgrun.xml", "<cravgrun width=\"480\" lines=\"6\" modes=\"drun,window,run\"><entry name=\"Terminal\" command=\"cravgterm\"/><entry name=\"Files\" command=\"cravgfm /lua\"/><scan dir=\"/apps\" prefix=\"cravg\"/></cravgrun>");
    cfg("cravgterm.xml", "<cravgterm width=\"520\" height=\"300\" fg=\"#c8c8c8\" bg=\"#101010\" prompt=\"$ \"><alias name=\"hi\" value=\"echo hello\"/></cravgterm>");
    cfg("cravgfm.xml", "<cravgfm width=\"520\" height=\"320\" start=\"/\" preview=\"180\"><bookmark path=\"/lua\"/></cravgfm>");
    cfg("exec-allow", "/apps/cravgterm\n");
    sc_reset(); shot_nn = 0;
    sW(30); sS("ap1_desktop"); sF(chk_ap_bg);
    sK(10, 8); sW(3); sT("help"); sK(10, 0); sW(1); sT("echo hi there"); sK(10, 0); sW(1); sT("lua 6*7"); sK(10, 0); sW(1); sT("hi"); sK(10, 0); sW(1); sT("rm -rf /"); sK(10, 0); sW(2); sS("ap2_terminal"); sF(chk_ap_term);
    sK('f', 8); sW(3); sK(term_DOWN, 0); sW(1); sK(term_DOWN, 0); sW(2); sS("ap3_files"); sF(chk_ap_fm);
    sK('r', 8); sW(3); sT("cravgt"); sW(2); sS("ap4_run"); sF(chk_ap_run); sK(10, 0); sW(4);
    sA(900, 150, 0, 0); sW(1); sA(900, 150, 2, 0); sW(1); sA(900, 150, 0, 0); sW(3); sS("ap5_menu"); sF(chk_ap_menu);
    sA(950, 211, 0, 0); sW(1); sA(950, 211, 1, 0); sW(1); sA(950, 211, 0, 0); sW(4); sS("ap6_end"); sF(chk_ap_end);
    sK(8, 6); sW(2);
    run_at("cravgwm");
    CHECK(!strstr(out, "lua: "));
    CHECK(ap_checks == 6);

    /* ---- встроенная конфигурация по умолчанию (без файлов в /etc/cravgwm/) ---- */
    int rci = fs_resolve("/etc/cravgwm/rc.xml"); if (rci >= 0) fs_remove(rci);
    int mni = fs_resolve("/etc/cravgwm/menu.xml"); if (mni >= 0) fs_remove(mni);
    sc_reset(); shot_nn = 0;
    sW(30);
    sK(10, 8); sW(3);                          /* Super+Return: терминал */
    sK('e', 8); sW(3);                           /* Super+E: cravgfm */
    sK('d', 8); sW(3); sK(27, 0); sW(2);        /* Super+D: лаунчер, Esc: закрыть */
    sK(0x113, 4); sW(2);                        /* Alt+F4: закрыть активное окно */
    sK(8, 6); sW(2);                            /* Ctrl+Alt+Backspace: выход */
    run_at("cravgwm");
    CHECK(!strstr(out, "lua: ") && !strstr(out, "error"));
    CHECK(log_px() == 0);

    if (fails) { printf("test_wm: %d FAILED\n", fails); return 1; }
    printf("test_wm: OK\n");
    return 0;
}
