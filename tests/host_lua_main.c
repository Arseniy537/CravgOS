/* ---- сценарии Lua поверх хост-стенда: настоящий kernel.c (см. tests/host_lua.py) ---- */
/* cyr_font defined in host_kernel.py */
static volatile u32 fake_regs[0x10000];
static void cmd(const char *s) { char b[200]; strcpy(b, s); printf("\n$ %s\n", s); fflush(stdout); run(b); }
static void dumpscreen(const char *title) {
    printf("--- screen: %s ---\n", title);
    for (int y = 0; y < H; y++) { char ln[W + 1]; for (int x = 0; x < W; x++) { u8 c = vga[y * W + x] & 0xFF; ln[x] = (c >= 32 && c < 127) ? c : (c ? '#' : ' '); } ln[W] = 0; int e = W; while (e > 0 && ln[e - 1] == ' ') e--; ln[e] = 0; printf("%s\n", ln); }
    printf("--- end ---\n");
}
#define EXPECT(c) do { if (!(c)) { printf("EXPECT FAILED line %d: %s\n", __LINE__, #c); sc_fail++; } } while (0)
static void ext_key(unsigned char sc) { hpush(0xE0, 0); kdown(sc); }
static int cell(int x, int y) { return vga[y * W + x]; }
int main(void) {
#ifdef _WIN32
    if (!VirtualAlloc((void *)0xB8000, 0x8000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) { perror("VirtualAlloc vga"); return 9; }
#else
    if (mmap((void *)0xB8000, 0x8000, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0) == MAP_FAILED) { perror("mmap vga"); return 9; }
#endif
    memset((void *)0xB8000, 0, 0x8000);
    key_init(); pit_init(); pit_prev = pit_read(); boot_tod = tod(); rng = 12345; fs_boot();
    nic_tx = h_tx; nic_rx = h_rx; ms_now = now_ms; nic_ok = 1; nip = 0x0A00020F; nmask = 0xFFFFFF00u; ngw = 0x0A000202; ndns = 0x0A000203;
    nmac[0] = 8; nmac[2] = 0x27; nmac[5] = 7; nic_id = 0x100E8086u; mouse_ok = 1; nicm = fake_regs; fake_regs[2] = 2; mouse_tried = 1;
    http_raw = tls_rawbuf; http_rawmax = sizeof tls_rawbuf - 1; http_rand = kernel_rand;
    color = th->base; clear();

    clear(); cmd("help");
    for (int i = 0; i < NCMD; i++) if (!screen_has(cmdtab[i].name)) { printf("help screen lacks command: %s\n", cmdtab[i].name); sc_fail++; }
    for (int i = 0; i < NCMD; i++) { char b[64]; sprintf(b, "help %s", cmdtab[i].name); clear(); cmd(b); EXPECT(screen_has("USAGE")); }

    printf("\n##### Lua in the real kernel\n");
    clear(); cmd("lua print(6*7)"); EXPECT(screen_has("42"));
    clear(); cmd("lua print(1//0)"); EXPECT(screen_has("attempt to divide by zero"));
    clear(); cmd("lua"); EXPECT(screen_has("lua - run a Lua program"));
    clear(); cmd("lua local t = {} for i = 1, 5 do t[#t+1] = i * i end print(table.concat(t, ','))"); EXPECT(screen_has("1,4,9,16,25"));
    /* коды клавиш настоящего драйвера: стрелка вправо, 'a', Esc, Enter, Backspace, F1 */
    ext_key(0x4D); type("a"); esc(); kdown(0x1C); kdown(0x0E); kdown(0x3B);
    clear(); cmd("lua local a,b,c,d,e,f = term.getkey(), term.getkey(), term.getkey(), term.getkey(), term.getkey(), term.getkey() print(a..'/'..b..'/'..c..'/'..d..'/'..e..'/'..f)");
    EXPECT(screen_has("260/97/27/10/8/272"));
    type("hello"); kdown(0x1C);
    clear(); cmd("lua local s = term.readline() print('got ['..s..']')"); EXPECT(screen_has("got [hello]"));
    clear(); cmd("lua term.color(14, 1) term.put(10, 5, 'AB') term.fill(0, 10, 3, 2, '#') term.put(78, 3, 'XYZ')");
    EXPECT(cell(10, 5) == ((0x1E << 8) | 'A') && cell(11, 5) == ((0x1E << 8) | 'B') && (cell(0, 10) & 255) == '#' && (cell(2, 11) & 255) == '#' && (cell(79, 3) & 255) == 'Y');
    clear(); cmd("lua local d = os.date() print(tostring(d.year >= 2000)..tostring(d.month >= 1 and d.month <= 12)..tostring(d.wday >= 1 and d.wday <= 7)..tostring(os.time() > 1000000000))"); EXPECT(screen_has("truetruetruetrue"));
    clear(); cmd("lua print(fs.write('/lua/h.lua', 'print(123)'))"); EXPECT(screen_has("true"));
    clear(); cmd("lua /lua/h.lua"); EXPECT(screen_has("123"));
    clear(); cmd("ls /lua"); EXPECT(screen_has("h.lua"));
    clear(); cmd("lua local s=0 for i=1,60000 do local t=('x'):rep(20)..i s=s+#t end print(s)"); EXPECT(screen_has("1488894"));
    esc(); clear(); cmd("lua while true do end"); EXPECT(screen_has("interrupted"));
    hqh = hqt = 0;

    printf("\n##### stas: Lua packages (offline repository)\n");
    cmd("stas repo offline"); clear(); cmd("stas update"); EXPECT(screen_has("Index updated:"));
    clear(); cmd("stas install luaide"); EXPECT(screen_has("Installed luaide ")); EXPECT(fs_resolve("/apps/luaide-ui") > 0 && fs_resolve("/apps/luaide-hl") > 0 && fs_resolve("/apps/luaide-help") > 0 && fs_resolve("/apps/luaide-repl") > 0);
    clear(); cmd("stas install tetris"); EXPECT(screen_has("Installed tetris ")); EXPECT(fs_resolve("/apps/tetris-lib") > 0);
    clear(); cmd("stas install calendar"); EXPECT(screen_has("Installed calendar "));
    clear(); cmd("stas list"); EXPECT(screen_has("luaide") && screen_has("tetris") && screen_has("calendar"));
    type("hqq"); clear(); cmd("luaide"); EXPECT(!screen_has("lua: "));            /* IDE: справка, закрыть, выйти */
    type("q"); clear(); cmd("calendar"); EXPECT(!screen_has("lua: "));
    type("  q"); clear(); cmd("tetris"); EXPECT(!screen_has("lua: "));
    { int i = fs_resolve("/lua/.tetris"); (void)i; }
    type("q"); clear(); cmd("stas run calendar"); EXPECT(!screen_has("lua: "));
    hqh = hqt = 0;
    clear(); cmd("stas remove luaide"); EXPECT(screen_has("Removed luaide"));
    cmd("save");
    printf("\n##### RESULT: %s\n", sc_fail ? "FAILURES" : "lua scenarios finished OK");
    return sc_fail ? 1 : 0;
}
