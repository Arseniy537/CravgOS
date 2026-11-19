/* ===== хост-стенд: запускает настоящий код ядра с эмуляцией железа ===== */
#define putc stdio_putc
#define puts stdio_puts
#define getc stdio_getc
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef putc
#undef puts
#undef getc
#include <sys/mman.h>
#include <setjmp.h>
static jmp_buf exc_jb;
void host_exc_done(void) { longjmp(exc_jb, 1); }
#include <time.h>
typedef struct { unsigned char b, aux; } hq_t;
static hq_t hq[65536]; static int hqh, hqt, hidle;
static int pit_cnt = 60000, pit_lat, pit_idx, cmos_idx;
unsigned char host_inb(unsigned short p) {
    switch (p) {
    case 0x64: { unsigned char st = 0; if (hqh != hqt) { st |= 1; if (hq[hqh].aux) st |= 0x20; } else if (++hidle > 3000000) { fprintf(stderr, "HARNESS: UI waits for input but the script is exhausted\n"); exit(3); } return st; }
    case 0x60: hidle = 0; if (hqh != hqt) return hq[hqh++ & 0xFFFF].b; return 0;
    case 0x40: return (pit_idx++ == 0) ? (pit_lat & 0xFF) : (pit_lat >> 8);
    case 0x71: { time_t t = time(0); struct tm *tm = gmtime(&t);
        switch (cmos_idx) { case 0x0A: return 0; case 0x0B: return 0x06; case 0: return tm->tm_sec; case 2: return tm->tm_min; case 4: return tm->tm_hour;
            case 7: return tm->tm_mday; case 8: return tm->tm_mon + 1; case 9: return tm->tm_year % 100;
            case 0x30: return 0x00; case 0x31: return 0x3C; case 0x34: return 0x00; case 0x35: return 0x01; } return 0; }
    }
    return 0;
}
void host_outb(unsigned short p, unsigned char v) {
    if (p == 0x43 && v == 0) { pit_cnt -= 997; if (pit_cnt < 0) pit_cnt += 65536; pit_lat = pit_cnt; pit_idx = 0; }
    if (p == 0x70) cmos_idx = v;
}
void host_outl(unsigned short p, unsigned v) { (void)p; (void)v; }
unsigned host_inl(unsigned short p) { (void)p; return 0xFFFFFFFFu; }

/* ---- ввод ---- */
static unsigned char rev[256], revs[256];
static void hpush(unsigned char b, int aux) { hq[hqt++ & 0xFFFF].b = b; hq[(hqt - 1) & 0xFFFF].aux = aux; }
static void key_init(void) { for (int sc = 0; sc < 128; sc++) { if (map[sc] && !rev[(u8)map[sc]]) rev[(u8)map[sc]] = sc; } for (int sc = 0; sc < 128; sc++) if (smap[sc] && !rev[(u8)smap[sc]] && !revs[(u8)smap[sc]]) revs[(u8)smap[sc]] = sc; }
static void kdown(unsigned char sc) { hpush(sc, 0); hpush(sc | 0x80, 0); }
static void type(const char *s) { for (; *s; s++) { u8 c = (u8)*s; if (rev[c]) kdown(rev[c]); else if (revs[c]) { hpush(0x2A, 0); kdown(revs[c]); hpush(0xAA, 0); } else { fprintf(stderr, "cannot type 0x%02x\n", c); exit(5); } } }
static void kext(unsigned char sc) { hpush(0xE0, 0); hpush(sc, 0); hpush(0xE0, 0); hpush(sc | 0x80, 0); }
static void kfn(int n) { kdown(0x3B + n - 1); }
#define KUP 0x48
#define KDOWN 0x50
#define KLEFT 0x4B
#define KRIGHT 0x4D
#define KHOME 0x47
#define KEND 0x4F
#define KDEL 0x53
#define KPGDN 0x51
#define KINS 0x52
static void esc(void) { kdown(1); }
static void hmpkt(int dx, int dy, int btn) { hpush(0x08 | btn | (dx < 0 ? 0x10 : 0) | (dy < 0 ? 0x20 : 0), 1); hpush((unsigned char)dx, 1); hpush((unsigned char)dy, 1); }
/* переместить указатель в клетку (cx,cy) и кликнуть: у ядра 8 отсчётов на клетку по X и 16 по Y */
static void mclick(int cx_, int cy_, int btn) {
    int tx = cx_ * 8 + 4, ty = cy_ * 16 + 8;
    int curx = mx * 8 + mxa, cury = my * 16 - mya;
    int dx = tx - curx, dy = ty - cury;
    while (dx || dy) { int sx = dx > 100 ? 100 : dx < -100 ? -100 : dx, sy = dy > 100 ? 100 : dy < -100 ? -100 : dy; hmpkt(sx, -sy, 0); dx -= sx; dy -= sy; }
    hmpkt(0, 0, btn); hmpkt(0, 0, 0);
}

/* ---- мок-сеть ---- */
static unsigned char gwm[6] = {0x52, 0x54, 0, 0x12, 0x35, 2};
#define SRVIP 0x5DB8D822u
static unsigned char hq2[128][1600]; static int hl2[128], h2h, h2t;
static void hnet_push(const unsigned char *f, int n) { memcpy(hq2[h2t & 127], f, n); hl2[h2t & 127] = n; h2t++; }
static int hmk_ip(unsigned char *f, const unsigned char *dm, u32 s, u32 d, u8 pr, const unsigned char *pl, int len) {
    memcpy(f, dm, 6); memcpy(f + 6, gwm, 6); be16(f + 12, 0x0800);
    u8 *p = f + 14; p[0] = 0x45; p[1] = 0; be16(p + 2, 20 + len); be16(p + 4, 1); be16(p + 6, 0); p[8] = 64; p[9] = pr; be16(p + 10, 0);
    be32(p + 12, s); be32(p + 16, d); be16(p + 10, net_cksum(p, 20)); memcpy(p + 20, pl, len); return 34 + len;
}
static u32 s_iss, s_rcv; static u16 s_cport; static int s_resp_sent, s_active;
static void htcp(u8 fl, u32 seq, u32 ack, const unsigned char *d, int dl) {
    u8 seg[1500]; memset(seg, 0, 20); be16(seg, 80); be16(seg + 2, s_cport); be32(seg + 4, seq); be32(seg + 8, ack);
    seg[12] = 5 << 4; seg[13] = fl; be16(seg + 14, 8192); if (dl) memcpy(seg + 20, d, dl);
    be16(seg + 16, tcp_sum(SRVIP, nip, seg, 20 + dl));
    u8 f[1600]; int n = hmk_ip(f, nmac, SRVIP, nip, 6, seg, 20 + dl); hnet_push(f, n);
}
static const char *page = "<html><head><title>Mock Page</title></head><body><h1>Mock</h1><p>Hello <a href='/next'>next page</a> and <a href='http://example.com/other'>other</a>. &amp; more text here to wrap the line nicely.</p><ul><li>one</li><li>two</li></ul></body></html>";
static int h_rx(u8 *b, int max) { (void)max; if (h2h == h2t) return 0; int n = hl2[h2h & 127]; memcpy(b, hq2[h2h & 127], n); h2h++; return n; }
static int h_tx(const u8 *f, int n) {
    (void)n; u16 type = rd16(f + 12); const u8 *p = f + 14;
    if (type == 0x0806) { if (rd16(p + 6) == 1) { u8 a[60]; memset(a, 0, 60); memcpy(a, f + 6, 6); memcpy(a + 6, gwm, 6); be16(a + 12, 0x0806); u8 *r = a + 14; be16(r, 1); be16(r + 2, 0x0800); r[4] = 6; r[5] = 4; be16(r + 6, 2); memcpy(r + 8, gwm, 6); be32(r + 14, rd32(p + 24)); memcpy(r + 18, f + 6, 6); be32(r + 24, rd32(p + 14)); hnet_push(a, 60); } return 0; }
    if (type != 0x0800) return 0;
    u32 s = rd32(p + 12), d = rd32(p + 16); u8 pr = p[9]; const u8 *u = p + 20; int ul = rd16(p + 2) - 20;
    if (pr == 1 && u[0] == 8) { u8 ic[100]; memcpy(ic, u, ul); ic[0] = 0; be16(ic + 2, 0); be16(ic + 2, net_cksum(ic, ul)); u8 fr[1600]; int n2 = hmk_ip(fr, f + 6, d, s, 1, ic, ul); hnet_push(fr, n2); }
    else if (pr == 17 && rd16(u + 2) == 67) {
        const u8 *bb = u + 8; int t = 0; for (int i = 240; bb[i] != 255; i += 2 + bb[i + 1]) if (bb[i] == 53) t = bb[i + 2];
        u8 r[300]; memset(r, 0, sizeof r); r[0] = 2; r[1] = 1; r[2] = 6; memcpy(r + 4, bb + 4, 4); be32(r + 16, 0x0A00020F); memcpy(r + 28, bb + 28, 6); be32(r + 236, 0x63825363u);
        int i = 240; r[i++] = 53; r[i++] = 1; r[i++] = (t == 1) ? 2 : 5; r[i++] = 1; r[i++] = 4; be32(r + i, 0xFFFFFF00u); i += 4; r[i++] = 3; r[i++] = 4; be32(r + i, 0x0A000202); i += 4; r[i++] = 6; r[i++] = 4; be32(r + i, 0x0A000203); i += 4; r[i++] = 54; r[i++] = 4; be32(r + i, 0x0A000202); i += 4; r[i++] = 255;
        u8 us[400]; be16(us, 67); be16(us + 2, 68); be16(us + 4, 308); be16(us + 6, 0); memcpy(us + 8, r, 300); u8 fr[1600]; static const u8 bc[6] = {255, 255, 255, 255, 255, 255}; int n2 = hmk_ip(fr, bc, 0x0A000202, 0xFFFFFFFFu, 17, us, 308); hnet_push(fr, n2);
    } else if (pr == 17 && rd16(u + 2) == 53) {
        const u8 *bb = u + 8; int bl = rd16(u + 4) - 8; u8 r[300]; memset(r, 0, sizeof r); memcpy(r, bb, 2); be16(r + 2, 0x8180); be16(r + 4, 1); be16(r + 6, 1);
        int qn = bl - 12; memcpy(r + 12, bb + 12, qn); int i = 12 + qn; r[i++] = 0xC0; r[i++] = 0x0C; be16(r + i, 1); be16(r + i + 2, 1); be32(r + i + 4, 300); be16(r + i + 8, 4); be32(r + i + 10, SRVIP); i += 14;
        u8 us[400]; be16(us, 53); be16(us + 2, rd16(u)); be16(us + 4, 8 + i); be16(us + 6, 0); memcpy(us + 8, r, i); u8 fr[1600]; int n2 = hmk_ip(fr, f + 6, 0x0A000203, s, 17, us, 8 + i); hnet_push(fr, n2);
    } else if (pr == 6) {
        u16 sport = rd16(u), dport = rd16(u + 2); u32 seq = rd32(u + 4); int hl = (u[12] >> 4) * 4; u8 fl = u[13]; int dl = ul - hl;
        s_cport = sport;
        if (dport != 80) { htcp(0x14, 0, seq + 1, 0, 0); return 0; }
        if (fl & 2) { s_iss = 9000; s_rcv = seq + 1; s_active = 1; s_resp_sent = 0; htcp(0x12, s_iss, s_rcv, 0, 0); return 0; }
        if (dl > 0 && s_active && !s_resp_sent) {
            s_rcv += dl; char hdr[200]; int pl = strlen(page); int hn = sprintf(hdr, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n", pl);
            htcp(0x10, s_iss + 1, s_rcv, 0, 0);
            u8 all[2000]; memcpy(all, hdr, hn); memcpy(all + hn, page, pl); int tot = hn + pl; htcp(0x18, s_iss + 1, s_rcv, all, tot); s_resp_sent = 1; htcp(0x11, s_iss + 1 + tot, s_rcv, 0, 0);
        } else if (fl & 1) { s_rcv++; htcp(0x10, s_iss + 1 + (u32)(strlen(page) + 70), s_rcv, 0, 0); }
    }
    return 0;
}

/* ---- сценарии ---- */
static int sc_fail;
static int screen_has(const char *needle) {
    for (int y = 0; y < H; y++) { char ln[W + 1]; for (int x = 0; x < W; x++) { u8 c = vga[y * W + x] & 0xFF; ln[x] = (c >= 32 && c < 127) ? c : ' '; } ln[W] = 0; if (strstr(ln, needle)) return 1; }
    return 0;
}
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
    key_init(); pit_init(); pit_prev = pit_read(); boot_tod = tod(); rng = 12345; fs_boot();
    nic_tx = h_tx; nic_rx = h_rx; ms_now = now_ms; nic_ok = 1; nip = 0x0A00020F; nmask = 0xFFFFFF00u; ngw = 0x0A000202; ndns = 0x0A000203;
    nmac[0] = 8; nmac[2] = 0x27; nmac[5] = 7; nic_id = 0x100E8086u; mouse_ok = 1; nicm = fake_regs; fake_regs[2] = 2; mouse_tried = 1;   /* STATUS: link up */
    http_raw = tls_rawbuf; http_rawmax = sizeof tls_rawbuf - 1; http_rand = kernel_rand;
    EXPECT(streq(th->name, "dark"));                              /* тема по умолчанию */
    color = th->base; clear();

    printf("##### BASIC\n");
    clear(); cmd("help");
    for (int i = 0; i < NCMD; i++) if (!screen_has(cmdtab[i].name)) { printf("help screen lacks command: %s\n", cmdtab[i].name); sc_fail++; }
    clear(); cmd("help ping"); EXPECT(screen_has("USAGE") && screen_has("ping HOST") && screen_has("Example"));
    clear(); cmd("man fetch"); EXPECT(screen_has("ALIASES") && screen_has("neofetch"));
    clear(); cmd("help nosuch"); EXPECT(screen_has("No such command"));
    for (int i = 0; i < NCMD; i++) { char b[64]; sprintf(b, "help %s", cmdtab[i].name); clear(); cmd(b); EXPECT(screen_has("USAGE")); }
    clear(); cmd("fastfetch"); EXPECT(screen_has("Memory") && screen_has("MiB /") && screen_has("used)")); dumpscreen("fastfetch");
    clear(); cmd("free"); EXPECT(screen_has("Total RAM") && screen_has("Used by Cravgos") && screen_has("Free")); dumpscreen("free");
    cmd("ver"); cmd("theme"); cmd("theme norton"); EXPECT(streq(th->name, "norton")); cmd("theme dark"); EXPECT(streq(th->name, "dark"));
    cmd("ls"); cmd("tree"); cmd("df"); cmd("calc 6*7"); cmd("cowsay hi"); cmd("uptime"); cmd("time"); cmd("date"); cmd("lspci"); cmd("nosuchcmd");
    printf("\n##### FILES\n");
    cmd("touch a.txt"); cmd("echo hi > b.txt"); cmd("echo more >> b.txt"); cmd("write c.txt hello world"); cmd("cat c.txt"); cmd("cat b.txt");
    cmd("mkdir d"); cmd("cp a.txt d"); cmd("mv b.txt d/b2.txt"); cmd("rm a.txt"); cmd("ls d"); cmd("cd d"); cmd("pwd"); cmd("cd .."); cmd("tree");
    printf("\n##### EDITOR (new file)\n");
    type("Hello"); kdown(0x1C); type("world"); kext(KUP); type("X"); kext(KDOWN); kext(KEND); type("!"); kext(KHOME); kext(KLEFT); kext(KRIGHT); kext(KDEL);
    for (int i = 0; i < 3; i++) kdown(0x0E);                       /* backspace */
    mclick(5, 0, 1); type("m"); esc();
    cmd("edit new.txt"); clear(); cmd("cat new.txt");
    { int i = fs_resolve("new.txt"); EXPECT(i > 0 && fsn[i].size > 0); }
    EXPECT(screen_has("Hello") || screen_has("world"));
    printf("\n##### EDITOR (existing file, big input, wrapping)\n");
    for (int i = 0; i < 200; i++) type("0123456789 abcdefghij ");
    kdown(0x1C); for (int i = 0; i < 30; i++) kext(KUP); for (int i = 0; i < 30; i++) kext(KDOWN); kext(KPGDN); esc();
    cmd("edit new.txt"); cmd("df");
    cmd("edit"); cmd("edit d");
    printf("\n##### NETWORK\n");
    clear(); cmd("ifconfig"); EXPECT(screen_has("10.0.2.15") && screen_has("link") && screen_has("defaults"));
    EXPECT(!net_conf_done);
    clear(); cmd("dns"); EXPECT(screen_has("DNS server 10.0.2.3") && screen_has("DNS-over-HTTPS fallback: on"));
    cmd("dns doh off"); EXPECT(http_doh_on == 0 && screen_has("fallback: off")); cmd("dns doh on"); EXPECT(http_doh_on == 1);
    cmd("dns x.y"); EXPECT(screen_has("not an IPv4"));
    { u32 keep = ndns; cmd("dns 8.8.4.4"); EXPECT(ndns == 0x08080404u); ndns = keep; }
    cmd("arp"); clear(); cmd("ping 10.0.2.2 2"); EXPECT(screen_has("asking DHCP") && screen_has("Reply from 10.0.2.2") && screen_has("2 received") && net_conf_done && net_dhcp_ok);
    clear(); cmd("ifconfig"); EXPECT(screen_has("config  DHCP"));
    clear(); cmd("nslookup example.com"); EXPECT(screen_has("93.184.216.34"));
    clear(); cmd("netdiag"); EXPECT(screen_has("network self-test") && screen_has("gateway ARP") && screen_has("frames:")); dumpscreen("netdiag");
    clear(); cmd("dhcp"); EXPECT(screen_has("Got 10.0.2.15"));
    clear(); cmd("ping example.com 1"); EXPECT(screen_has("Reply from 93.184.216.34"));
    clear(); cmd("curl example.com"); EXPECT(screen_has("HTTP 200") && screen_has("Mock") && screen_has("next page"));
    clear(); cmd("curl https://example.com"); dumpscreen("curl https (no server)");
    clear(); cmd("curl nosuch.invalid");
    printf("\n##### BROWSER\n");
    kdown(0x0F); kdown(0x1C);                                       /* Tab, Enter: открыть первую ссылку */
    type("b"); kdown(0x0F); kdown(0x0F); kdown(0x1C); type("b");   /* назад; вторая ссылка; назад */
    mclick(10, 2, 1); type("g"); type("example.com/x"); kdown(0x1C); type("q");   /* клик, ввод адреса */
    for (int q = 0; q < 40; q++) esc(); cmd("web"); hqh = hqt = 0;
    mclick(10, 2, 1); for (int q = 0; q < 40; q++) esc(); cmd("web http://example.com/"); hqh = hqt = 0;
    kdown(0x0F); kdown(0x0F); kdown(0x1C); for (int q = 0; q < 40; q++) esc(); cmd("web http://example.com/"); hqh = hqt = 0;   /* Tab Tab Enter: открыть вторую ссылку */
    printf("\n##### EXCEPTION CATCHER + IDT\n");
    idt_init();
    for (int v = 0; v < 33; v++) { u32 hd = isr_table[v]; u8 *gt = idt_tab + v * 8; EXPECT(gt[0] == (u8)hd && gt[1] == (u8)(hd >> 8) && gt[2] == 8 && gt[3] == 0 && gt[4] == 0 && gt[5] == 0x8E && gt[6] == (u8)(hd >> 16) && gt[7] == (u8)(hd >> 24)); }
    for (int v = 32; v < 256; v++) { u8 *gt = idt_tab + v * 8; u32 hd = isr_table[32]; EXPECT(gt[0] == (u8)hd && gt[5] == 0x8E); }
    strcpy(last_cmd, "crashtest");
    { u32 frame[5] = {13, 0x1234, 0xC0DE1234u, 8, 2}; for (int q = 0; q < 70; q++) hpush(0x9C, 0); kdown(0x1C);
      if (!setjmp(exc_jb)) exception_handler(frame);
      dumpscreen("after exception handler returned to shell"); }
    EXPECT(1);
    printf("\n##### COMMANDER (fm)\n");
    kdown(0x0F); kext(KDOWN); kext(KUP); kdown(0x1C); kdown(0x0E);                 /* Tab, вниз, вверх, Enter, Backspace */
    kfn(7); type("newdir"); kdown(0x1C);                                           /* F7 mkdir */
    kdown(0x39); kdown(0x39); kfn(5); kdown(0x1C);                                 /* mark, F5 copy */
    kfn(6); esc(); kfn(8); kdown(0x1C); kfn(1); kdown(0x1C); kfn(2); kext(KDOWN); kdown(0x1C);
    type("n"); type("fresh.txt"); kdown(0x1C); kfn(3); esc(); kfn(4); type("abc"); esc(); kfn(9); esc();
    mclick(20, 5, 1); mclick(20, 5, 1); mclick(60, 3, 1); mclick(0, 3, 2); mclick(75, 0, 1); mclick(3, 24, 1); 
    for (int q = 0; q < 40; q++) esc();
    cmd("fm"); hqh = hqt = 0;
    printf("\n##### GAMES\n");
    type("q"); kdown(0x1C); cmd("guess"); hqh = hqt = 0;
    type("q"); cmd("snake"); hqh = hqt = 0;
    type("r"); type("q"); cmd("rps"); hqh = hqt = 0;
    type("x"); cmd("matrix"); hqh = hqt = 0;
    cmd("beep");
    printf("\n##### RESULT: %s\n", sc_fail ? "FAILURES" : "host kernel scenarios finished without crash");
    return sc_fail ? 1 : 0;
}
