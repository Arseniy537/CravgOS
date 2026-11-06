/* ---- сценарии stas поверх хост-стенда (см. tests/host_stas.py) ---- */
const u8 cyr_font[CYR_N][16] = {{0}};
static volatile u32 fake_regs[0x10000];
static void cmd(const char *s) { char b[200]; strcpy(b, s); printf("\n$ %s\n", s); fflush(stdout); run(b); }
static void dumpscreen(const char *title) {
    printf("--- screen: %s ---\n", title);
    for (int y = 0; y < H; y++) { char ln[W + 1]; for (int x = 0; x < W; x++) { u8 c = vga[y * W + x] & 0xFF; ln[x] = (c >= 32 && c < 127) ? c : (c ? '#' : ' '); } ln[W] = 0; int e = W; while (e > 0 && ln[e - 1] == ' ') e--; ln[e] = 0; printf("%s\n", ln); }
    printf("--- end ---\n");
}
#define EXPECT(c) do { if (!(c)) { printf("EXPECT FAILED line %d: %s\n", __LINE__, #c); sc_fail++; } } while (0)
int main(void) {
    if (mmap((void *)0xB8000, 0x8000, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0) == MAP_FAILED) { perror("mmap vga"); return 9; }
    memset((void *)0xB8000, 0, 0x8000);
    { FILE *f = fopen("stas-repo/index.txt", "rb"); if (!f) { puts("run from the repo root"); return 2; } fread(repo_index, 1, sizeof repo_index - 1, f); fclose(f);
      f = fopen("stas-repo/nim.stp", "rb"); fread(repo_nim, 1, sizeof repo_nim - 1, f); fclose(f); }
    key_init(); pit_init(); pit_prev = pit_read(); boot_tod = tod(); rng = 12345; fs_boot();
    nic_tx = h_tx; nic_rx = h_rx; ms_now = now_ms; nic_ok = 1; nip = 0x0A00020F; nmask = 0xFFFFFF00u; ngw = 0x0A000202; ndns = 0x0A000203;
    nmac[0] = 8; nmac[2] = 0x27; nmac[5] = 7; nic_id = 0x100E8086u; mouse_ok = 1; nicm = fake_regs; fake_regs[2] = 2; mouse_tried = 1;
    http_raw = tls_rawbuf; http_rawmax = sizeof tls_rawbuf - 1; http_rand = kernel_rand;
    color = th->base; clear();

    clear(); cmd("help");
    for (int i = 0; i < NCMD; i++) if (!screen_has(cmdtab[i].name)) { printf("help screen lacks command: %s\n", cmdtab[i].name); sc_fail++; }
    for (int i = 0; i < NCMD; i++) { char b[64]; sprintf(b, "help %s", cmdtab[i].name); clear(); cmd(b); EXPECT(screen_has("USAGE")); }
    clear(); cmd("help stas"); EXPECT(screen_has("stas update") && screen_has("own repository"));

    printf("\n##### STAS over the mock TCP/HTTP stack\n");
    clear(); cmd("stas"); EXPECT(screen_has("package manager"));
    clear(); cmd("stas list"); EXPECT(screen_has("stas update"));
    cmd("stas repo http://example.com/stas-repo");
    clear(); cmd("stas update"); EXPECT(screen_has("Index updated: 1 package")); dumpscreen("stas update");
    clear(); cmd("stas list"); EXPECT(screen_has("[ ] nim")); dumpscreen("stas list");
    clear(); cmd("stas install nim"); EXPECT(screen_has("Installed nim") && screen_has("sha256 ok")); dumpscreen("stas install");
    EXPECT(fs_resolve("/apps/nim") > 0);
    clear(); cmd("stas list"); EXPECT(screen_has("[x] nim"));
    clear(); cmd("tree"); EXPECT(screen_has("apps") && screen_has("nim"));
    /* игра как обычная команда: берём по 1, пока не проиграем (старт зависит от rng) */
    for (int i = 0; i < 12; i++) { type("1"); kdown(0x1C); }
    type("0"); kdown(0x1C);
    clear(); cmd("nim"); EXPECT(screen_has("NIM") || screen_has("Sticks left") || screen_has("Bye") || screen_has("lose")); dumpscreen("nim (installed package run as a command)"); hqh = hqt = 0;
    type("0"); kdown(0x1C); cmd("stas run nim"); hqh = hqt = 0;
    /* порча пакета на «сервере» */
    repo_nim[15] ^= 1; clear(); cmd("stas remove nim"); cmd("stas install nim"); EXPECT(screen_has("SHA-256 mismatch")); EXPECT(fs_resolve("/apps/nim") < 0); repo_nim[15] ^= 1;
    clear(); cmd("stas install nim"); EXPECT(screen_has("Installed nim"));
    clear(); cmd("stas remove nim"); EXPECT(screen_has("Removed nim")); EXPECT(fs_resolve("/apps/nim") < 0);
    clear(); cmd("nim"); EXPECT(screen_has("unknown command"));
    clear(); cmd("stas install guess"); EXPECT(screen_has("no such package"));
    cmd("save");
    printf("\n##### RESULT: %s\n", sc_fail ? "FAILURES" : "stas scenarios finished OK");
    return sc_fail ? 1 : 0;
}
