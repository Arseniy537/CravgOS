/* fbcon.h - текстовая консоль поверх пиксельного режима и команды wm / wmboot / textboot / gfxinfo.
 * В пиксельном режиме видеопамять 0xB8000 не отображается, поэтому ядро продолжает писать в неё как раньше,
 * а fbc_sync() перерисовывает изменившиеся клетки 80x25 на экран шрифтом 8x16 (масштаб - целое число раз). */
#ifndef FBCON_H
#define FBCON_H
static int stas_try_app(const char *cmd);              /* stas.h */
static void stas_install(const char *name);
static void stas_update(void);
static void reboot(void);
static u16 fbc_shadow[W * H];
static int fbc_scale = 1, fbc_ox, fbc_oy, fbc_curx = -1, fbc_cury = -1, fbc_curvis;
static u32 fbc_last;
static const u32 fbc_pal[16] = { 0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
                                 0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF };
static inline void fbc_put(int px, int py, u32 rgb) {
    volatile u32 *d = (volatile u32 *)(gx_lfb + (u32)py * gx_pitch) + px;
    if (gx_rp == 16 && gx_gp == 8 && gx_bp == 0) *d = rgb;
    else *d = ((rgb >> 16) & 255) << gx_rp | ((rgb >> 8) & 255) << gx_gp | (rgb & 255) << gx_bp;
}
static void fbc_cell(int x, int y, u16 v, int cursor_here) {
    u32 fg = fbc_pal[(v >> 8) & 15], bg = fbc_pal[(v >> 12) & 15];
    const u8 *g = gx_font + (v & 255) * 16;
    int s = fbc_scale;
    for (int r = 0; r < 16; r++) {
        u8 bits = g[r];
        int under = cursor_here && r >= 14;
        for (int c = 0; c < 8; c++) {
            u32 col = (under || (bits & (0x80 >> c))) ? fg : bg;
            for (int sy = 0; sy < s; sy++) for (int sx = 0; sx < s; sx++) fbc_put(fbc_ox + (x * 8 + c) * s + sx, fbc_oy + (y * 16 + r) * s + sy, col);
        }
    }
}
static void fbc_sync(void) {
    if (!gx_ok || gx_wm) return;
    u32 now = now_ms();
    if (!fbc_force && now - fbc_last < 12) return;
    fbc_last = now;
    if (fbc_force) {                                   /* полная перерисовка: чёрный фон и все клетки заново */
        for (int j = 0; j < gx_h; j++) for (int i = 0; i < gx_w; i++) fbc_put(i, j, 0);
        for (int i = 0; i < W * H; i++) fbc_shadow[i] = (u16)~vga[i];
        fbc_curx = fbc_cury = -1; fbc_force = 0;
    }
    int vis = fbc_cur && ((now >> 9) & 1) == 0 && cx >= 0 && cx < W && cy >= 0 && cy < H;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        u16 v = vga[y * W + x];
        int cur_here = vis && x == cx && y == cy;
        int was_cur = fbc_curvis && x == fbc_curx && y == fbc_cury;
        if (v != fbc_shadow[y * W + x] || cur_here != was_cur) { fbc_cell(x, y, v, cur_here); fbc_shadow[y * W + x] = v; }
    }
    fbc_curx = cx; fbc_cury = cy; fbc_curvis = vis;
}
/* вызывается при старте ядра: включить пиксельный режим, если загрузчик его включил (fbinfo) */
static void gfx_boot(void) {
    if (*(const u32 *)fbinfo != 0x31494246u || fbinfo[14] != 32) return;
    vga = vga_mem;
    for (int i = 0; i < W * H; i++) vga_mem[i] = (0x0E << 8) | ' ';
    u32 base = *(const u32 *)(fbinfo + 4), pitch = *(const u16 *)(fbinfo + 12);
    int w = *(const u16 *)(fbinfo + 8), h = *(const u16 *)(fbinfo + 10);
    u32 mb = mem_mib();
    if (mb < 40) return;                               /* на поверхности нужно хотя бы около 20 МиБ после 16 МиБ ядра */
    u32 avail = (mb - 17) * 1024u * 1024u;
    if (avail > 96u * 1024u * 1024u) avail = 96u * 1024u * 1024u;
    int nz = 0;
    for (int i = 0; i < 4096; i++) { gx_font[i] = romfont[i]; if (romfont[i]) nz++; }
    gx_font_ok = nz > 200;
    for (int i = 0; i < CYR_N; i++) for (int r = 0; r < 16; r++) gx_font[cyr_code[i] * 16 + r] = cyr_font[i][r];
    if (gx_setup((u8 *)(uintptr_t)base, pitch, w, h, fbinfo[15], fbinfo[16], fbinfo[17], (u8 *)0x1000000, avail) < 0) return;
    fbc_scale = 1;
    while (w >= 640 * (fbc_scale + 1) && h >= 400 * (fbc_scale + 1)) fbc_scale++;
    fbc_ox = (w - 640 * fbc_scale) / 2; fbc_oy = (h - 400 * fbc_scale) / 2;
    fbc_force = 1;
    fbc_sync();
}
static void cmd_gfxinfo(void) {
    if (!gx_ok) {
        puts("Text mode: no pixel framebuffer. Run 'wmboot' to reboot into 1024x768 pixel mode.\n");
        if (*(const u32 *)fbinfo == 0x31494246u) puts("(the loader set a mode, but it was not usable: not enough RAM or unsupported pixel format)\n");
        return;
    }
    puts("Pixel mode: "); putu((u32)gx_w); putc('x'); putu((u32)gx_h); puts(" 32 bpp, framebuffer at 0x");
    puthex((u32)(uintptr_t)gx_lfb, 8); puts(", pitch "); putu(gx_pitch);
    puts("\nColor order: R<<"); putu((u32)gx_rp); puts(" G<<"); putu((u32)gx_gp); puts(" B<<"); putu((u32)gx_bp);
    puts("\nSurface memory: "); putu(gx_memsz / 1024 / 1024); puts(" MiB, free "); putu(gx_free_bytes() / 1024 / 1024); puts(" MiB\n");
    puts("Console font: "); puts(gx_font_ok ? "BIOS 8x16 + Cyrillic" : "missing (text will be blank)"); puts(", text scale x"); putu((u32)fbc_scale); putc('\n');
}
static void cmd_wm(char *args) {
    (void)args;
    if (!gx_ok) { puts("wm needs pixel mode. Run 'wmboot' (reboots into 1024x768), then 'wm'.\n"); return; }
    if (fs_resolve("/etc/cravgwm/exec-allow") < 0) {
        fs_create("/etc", 1);
        fs_create("/etc/cravgwm", 1);
        int f = fs_create("/etc/cravgwm/exec-allow", 0);
        if (f >= 0) fs_write(f, "/apps/cravgterm\n", 16, 0);
    }
    if (fs_resolve("/apps/cravgwm") < 0) {
        puts("Installing cravgwm and companion packages...\n");
        if (fs_resolve("/lua/.index") < 0) stas_update();
        stas_install("cravgwm");
    }
    if (!stas_try_app("cravgwm")) puts("cravgwm is not installed: stas update, then stas install cravgwm\n");
    fbc_force = 1;                                     /* вернувшись из WM, перерисовать консоль */
}
static void cmd_wmboot(void) {
    if (gx_ok) { puts("Already in pixel mode ("); putu((u32)gx_w); putc('x'); putu((u32)gx_h); puts("). Start the WM with 'wm'.\n"); return; }
    puts("Rebooting into pixel mode. If the screen stays black, power-cycle the machine:\nthe next boot is a normal text boot.\n");
    outb(0x70, 0x7D); outb(0x71, 0x57);                /* одноразовый флаг для загрузчика (boot_iso.s) */
    reboot();
}
static void cmd_textboot(void) {
    if (!gx_ok) { puts("Already in text mode.\n"); return; }
    puts("Rebooting into text mode...\n");
    outb(0x70, 0x7D); outb(0x71, 0);
    reboot();
}
#endif
