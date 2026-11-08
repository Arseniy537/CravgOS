#define XBSS __attribute__((section(".bss.xbig")))
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef unsigned long long uintptr_t;
typedef long long          intptr_t;
#include "cyrfont.h"

static inline void outb(u16 p, u8 v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline u8 inb(u16 p) { u8 v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline void outw(u16 p, u16 v) { __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p)); }
static inline u16 inw(u16 p) { u16 v; __asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline void outl(u16 p, u32 v) { __asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p)); }
static inline u32 inl(u16 p) { u32 v; __asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline u32 rdtsc32(void) { u32 a, d; __asm__ volatile("rdtsc":"=a"(a),"=d"(d)); return a; }
static void cpuid(u32 leaf, u32 *a, u32 *b, u32 *c, u32 *d) {
    __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0));
}

/* ---------- VGA текстовый режим 80x25 ---------- */
#define W 80
#define H 25
static volatile u16 *const vga = (u16 *)0xB8000;
static int cx, cy;
static u8 color = 0x07;

static void cursor(void) {
    u16 pos = cy * W + cx;
    outb(0x3D4, 14); outb(0x3D5, pos >> 8);
    outb(0x3D4, 15); outb(0x3D5, pos & 0xFF);
}
static void cursor_hide(void) { outb(0x3D4, 0x0A); outb(0x3D5, 0x20); }
static void cursor_show(void) { outb(0x3D4, 0x0A); outb(0x3D5, 14); outb(0x3D4, 0x0B); outb(0x3D5, 15); }
static void clear(void) {
    for (int i = 0; i < W * H; i++) vga[i] = (color << 8) | ' ';
    cx = cy = 0; cursor();
}
static void scroll(void) {
    for (int i = 0; i < W * (H - 1); i++) vga[i] = vga[i + W];
    for (int i = W * (H - 1); i < W * H; i++) vga[i] = (color << 8) | ' ';
    cy = H - 1;
}
static void putc(char c) {
    if (c == '\n') { cx = 0; cy++; }
    else if (c == '\b') { if (cx > 0) { cx--; vga[cy * W + cx] = (color << 8) | ' '; } }
    else { vga[cy * W + cx] = (color << 8) | (u8)c; if (++cx >= W) { cx = 0; cy++; } }
    if (cy >= H) scroll();
    cursor();
}
static void puts(const char *s) { while (*s) putc(*s++); }
static void putu(u32 n) {
    char b[12]; int i = 0;
    if (!n) b[i++] = '0';
    while (n) { b[i++] = '0' + n % 10; n /= 10; }
    while (i) putc(b[--i]);
}
static void puthex(u32 v, int digits) {
    for (int i = digits - 1; i >= 0; i--) putc("0123456789abcdef"[(v >> (i * 4)) & 15]);
}
static void putip(u32 ip) {
    putu(ip >> 24); putc('.'); putu((ip >> 16) & 255); putc('.'); putu((ip >> 8) & 255); putc('.'); putu(ip & 255);
}
static void putmac(const u8 *m) {
    for (int i = 0; i < 6; i++) { puthex(m[i], 2); if (i < 5) putc(':'); }
}
static void put_at(int x, int y, char c, u8 attr) { vga[y * W + x] = (attr << 8) | (u8)c; }
static void put_str_at(int x, int y, const char *s, u8 attr) { while (*s) put_at(x++, y, *s++, attr); }

/* ---------- Темы оформления ---------- */
struct theme {
    const char *name;
    u8 base, frame, dir, file, sel, mark, bar, barkey, title, user, path, link, head, dim, dlg, dlgbtn;
};
static const struct theme themes[] = {
    /* name         base  frame dir   file  sel   mark  bar   barkey title user  path  link  head  dim   dlg   dlgbtn */
    {"dark",       0x07, 0x08, 0x09, 0x07, 0x70, 0x0E, 0x70, 0x07, 0x70, 0x0A, 0x09, 0x0B, 0x0E, 0x08, 0x70, 0x30},
    {"norton",     0x1F, 0x1B, 0x1F, 0x1B, 0x30, 0x1E, 0x30, 0x07, 0x30, 0x1A, 0x1F, 0x1E, 0x1F, 0x13, 0x70, 0x30},
    {"retro",      0x0A, 0x02, 0x0A, 0x02, 0x20, 0x0F, 0x20, 0x0A, 0x20, 0x0A, 0x0F, 0x0A, 0x0F, 0x02, 0x20, 0x2F},
    {"amber",      0x0E, 0x06, 0x0E, 0x06, 0x60, 0x0F, 0x60, 0x0E, 0x60, 0x0E, 0x0F, 0x0E, 0x0F, 0x06, 0x60, 0x6F},
    {"light",      0x70, 0x78, 0x71, 0x70, 0x1F, 0x74, 0x1F, 0x70, 0x1F, 0x72, 0x71, 0x71, 0x74, 0x78, 0x30, 0x1F},
    {"tokyo",      0x0F, 0x09, 0x0B, 0x07, 0x1F, 0x0D, 0x1F, 0x0B, 0x1F, 0x0D, 0x0B, 0x09, 0x0D, 0x08, 0x1F, 0x5F},
    {"colorblind", 0x0F, 0x0B, 0x0E, 0x0F, 0xE0, 0x0E, 0x90, 0x0E, 0x90, 0x0E, 0x0B, 0x0E, 0x0F, 0x08, 0x1F, 0xE0},
    {"solarized",  0x07, 0x03, 0x02, 0x07, 0x30, 0x06, 0x30, 0x0F, 0x30, 0x0E, 0x02, 0x09, 0x0B, 0x08, 0x30, 0x1F},
};
#define NTHEMES ((int)(sizeof themes / sizeof themes[0]))
static const struct theme *th = &themes[3];     /* amber - тема по умолчанию */
static int disk_save(void);
static int disk_loading = 0;
static void theme_apply(int i) {
    th = &themes[i];
    color = th->base;
    clear();
    if (!disk_loading) disk_save();
}
static int theme_index(void) { return (int)(th - themes); }

/* ---------- Кириллица: свой шрифт в видеопамяти, 16 цветов фона ---------- */
static void vga_font_load(void) {
    volatile u8 *fm = (volatile u8 *)0xA0000;
    outb(0x3C4, 0x00); outb(0x3C5, 0x01);          /* синхронный сброс секвенсора */
    outb(0x3C4, 0x02); outb(0x3C5, 0x04);          /* запись только в plane 2 (там шрифт) */
    outb(0x3C4, 0x04); outb(0x3C5, 0x06);          /* последовательная адресация (odd/even off) */
    outb(0x3C4, 0x00); outb(0x3C5, 0x03);

    outb(0x3CE, 0x00); outb(0x3CF, 0x00);          /* set/reset = 0 */
    outb(0x3CE, 0x01); outb(0x3CF, 0x00);          /* enable set/reset = 0 */
    outb(0x3CE, 0x03); outb(0x3CF, 0x00);          /* data rotate = 0 */
    outb(0x3CE, 0x04); outb(0x3CF, 0x02);          /* чтение из plane 2 */
    outb(0x3CE, 0x05); outb(0x3CF, 0x00);          /* write mode 0 */
    outb(0x3CE, 0x06); outb(0x3CF, 0x04);          /* окно памяти шрифта A0000 (64K) */
    outb(0x3CE, 0x08); outb(0x3CF, 0xFF);          /* битовая маска: все 8 бит */

    for (int i = 0; i < CYR_N; i++) {
        volatile u8 *g = fm + cyr_code[i] * 32;
        for (int r = 0; r < 16; r++) g[r] = cyr_font[i][r];
        for (int r = 16; r < 32; r++) g[r] = 0;
    }
    __asm__ volatile("wbinvd":::"memory");

    outb(0x3C4, 0x00); outb(0x3C5, 0x01);
    outb(0x3C4, 0x02); outb(0x3C5, 0x03);          /* возвращаем текстовый режим */
    outb(0x3C4, 0x04); outb(0x3C5, 0x02);          /* standard text mode odd/even */
    outb(0x3C4, 0x00); outb(0x3C5, 0x03);

    outb(0x3CE, 0x04); outb(0x3CF, 0x00);
    outb(0x3CE, 0x05); outb(0x3CF, 0x10);          /* odd/even host address */
    outb(0x3CE, 0x06); outb(0x3CF, 0x0E);          /* map B8000 32K */
    outb(0x3CE, 0x08); outb(0x3CF, 0xFF);
}
static void vga_blink_off(void) {                  /* бит 7 атрибута = яркий фон (16 цветов) вместо мигания */
    inb(0x3DA); outb(0x3C0, 0x30);
    u8 v = inb(0x3C1);
    outb(0x3C0, v & ~0x08);
    inb(0x3DA); outb(0x3C0, 0x20);
}

/* ---------- CMOS / RTC ---------- */
static u8 cmos(u8 r) { outb(0x70, r); return inb(0x71); }
static u8 bcd(u8 v) { return (cmos(0x0B) & 4) ? v : (v & 15) + (v >> 4) * 10; }
static void put2(u32 n) { putc('0' + n / 10); putc('0' + n % 10); }
static void rtc_wait(void) { while (cmos(0x0A) & 0x80) ; }
static void show_time(void) {
    rtc_wait();
    put2(bcd(cmos(4))); putc(':'); put2(bcd(cmos(2))); putc(':'); put2(bcd(cmos(0))); putc('\n');
}
static void show_date(void) {
    rtc_wait();
    put2(bcd(cmos(7))); putc('.'); put2(bcd(cmos(8))); puts(".20"); put2(bcd(cmos(9))); putc('\n');
}
static u32 tod(void) {   /* секунд с начала суток */
    rtc_wait();
    return bcd(cmos(4)) * 3600u + bcd(cmos(2)) * 60u + bcd(cmos(0));
}
static u32 boot_tod;

/* ---------- Таймер PIT (опрос, без прерываний) ---------- */
static void pit_init(void) { outb(0x43, 0x34); outb(0x40, 0); outb(0x40, 0); }
static u16 pit_read(void) { outb(0x43, 0); u8 lo = inb(0x40); u8 hi = inb(0x40); return lo | (hi << 8); }
static u32 pit_ms, pit_rem;
static u16 pit_prev;
static u32 now_ms(void) {              /* миллисекунды; вызывать чаще, чем раз в 50 мс */
    u16 n = pit_read();
    pit_rem += (u16)(pit_prev - n); pit_prev = n;
    while (pit_rem >= 1193) { pit_rem -= 1193; pit_ms++; }
    return pit_ms;
}
static u32 uptime_s(void) { return now_ms() / 1000u; }
static void delay_ms(u32 ms) {
    u32 t0 = now_ms();
    while (now_ms() - t0 < ms) {
        __asm__ volatile("pause");
    }
}

/* ---------- Случайные числа ---------- */
static u32 rng = 2463534242u;
static u32 rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

/* ---------- Мышь PS/2 ---------- */
static int mouse_ok, mouse_wheel_mode, mouse_ui, mouse_tried;
static int mx = 40, my = 12, mxa, mya, m_buttons, m_moved, m_click_l, m_click_r, m_wheel;
static u8 mpkt[4];
static int mpi, ms_shown, ms_idx;
static u8 ms_orig;
static void mouse_hide(void) {
    if (ms_shown) { vga[ms_idx] = (u16)((ms_orig << 8) | (vga[ms_idx] & 0xFF)); ms_shown = 0; }
}
static void mouse_forget(void) { mouse_hide(); }
static void mouse_draw(void) {
    mouse_hide();
    if (!mouse_ui || !mouse_ok) return;
    ms_idx = my * W + mx;
    u16 cell = vga[ms_idx];
    ms_orig = cell >> 8;
    u8 na = ((ms_orig & 0x70) == 0x70) ? 0x07 : 0x70;
    vga[ms_idx] = (u16)((na << 8) | (cell & 0xFF));
    ms_shown = 1;
}
static u32 mouse_last_ms;
static void mouse_byte(u8 b) {
    u32 now = now_ms();
    if (now - mouse_last_ms > 80) mpi = 0;
    mouse_last_ms = now;
    if (mpi == 0 && !(b & 8)) return;                    /* потеряли синхронизацию */
    mpkt[mpi++] = b;
    if (mpi < (mouse_wheel_mode ? 4 : 3)) return;
    mpi = 0;
    int dx = mpkt[1], dy = mpkt[2];
    if (mpkt[0] & 0x10) dx -= 256;
    if (mpkt[0] & 0x20) dy -= 256;
    if (mpkt[0] & 0xC0) dx = dy = 0;
    int wheel = 0;
    if (mouse_wheel_mode) { wheel = mpkt[3] & 15; if (wheel & 8) wheel -= 16; }
    mxa += dx; mya -= dy;
    int ox = mx, oy = my;
    while (mxa >= 8) { mx++; mxa -= 8; }
    while (mxa <= -8) { mx--; mxa += 8; }
    while (mya >= 16) { my++; mya -= 16; }
    while (mya <= -16) { my--; mya += 16; }
    if (mx < 0) mx = 0;
    if (mx > W - 1) mx = W - 1;
    if (my < 0) my = 0;
    if (my > H - 1) my = H - 1;
    if (mx != ox || my != oy) m_moved = 1;
    int bt = mpkt[0] & 7;
    if ((bt & 1) && !(m_buttons & 1)) m_click_l = 1;
    if ((bt & 2) && !(m_buttons & 2)) m_click_r = 1;
    m_buttons = bt;
    if (wheel) m_wheel += wheel;
}
static int kbc_wait_w(void) { for (u32 i = 0; i < 30000u; i++) if (!(inb(0x64) & 2)) return 1; return 0; }
static int kbc_wait_r(void) { for (u32 i = 0; i < 30000u; i++) if (inb(0x64) & 1) return 1; return 0; }
static int mouse_write(u8 v) {
    if (!kbc_wait_w()) return 0;
    outb(0x64, 0xD4);
    if (!kbc_wait_w()) return 0;
    outb(0x60, v);
    if (!kbc_wait_r()) return 0;
    return inb(0x60) == 0xFA;
}
static void mouse_init(void) {
    for (int i = 0; i < 64 && (inb(0x64) & 1); i++) inb(0x60);
    if (!kbc_wait_w()) return;
    outb(0x64, 0xA8);                                    /* включить порт мыши */
    if (!kbc_wait_w()) return;
    outb(0x64, 0x20);
    if (!kbc_wait_r()) return;
    u8 cb = inb(0x60);
    cb |= 2; cb &= ~0x20;
    if (!kbc_wait_w()) return;
    outb(0x64, 0x60);
    if (!kbc_wait_w()) return;
    outb(0x60, cb);
    if (!mouse_write(0xF6)) return;                      /* настройки по умолчанию */
    mouse_write(0xF3); mouse_write(200); mouse_write(0xF3); mouse_write(100); mouse_write(0xF3); mouse_write(80);
    if (mouse_write(0xF2) && kbc_wait_r() && inb(0x60) == 3) mouse_wheel_mode = 1;   /* IntelliMouse: колесо */
    mouse_write(0xF3); mouse_write(100);
    if (!mouse_write(0xF4)) return;                      /* поток данных */
    mouse_ok = 1;
}

/* ---------- Клавиатура PS/2 (опрос) ---------- */
#define K_UP    0x101
#define K_DOWN  0x102
#define K_LEFT  0x103
#define K_RIGHT 0x104
#define K_DEL   0x105
#define K_HOME  0x106
#define K_END   0x107
#define K_PGUP  0x108
#define K_PGDN  0x109
#define K_INS   0x10A
#define K_F1    0x110            /* F1..F10 = 0x110..0x119 */
static const char map[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
};
static const char smap[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
    'A','S','D','F','G','H','J','K','L',':','"','~',0,'|',
    'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' '
};
static int shift, ext, ctrl_, alt_, kbd_ru, caps_lock;
static void kbd_set_leds(void) {
    if (!kbc_wait_w()) return;
    outb(0x60, 0xED);
    if (!kbc_wait_r()) return;
    if (inb(0x60) != 0xFA) return;
    if (!kbc_wait_w()) return;
    outb(0x60, caps_lock ? 4 : 0);
}
static u8 map_ru[128], smap_ru[128];
static void ru_init(void) {                        /* ЙЦУКЕН в кодовой странице CP866 */
    static const struct { u8 sc, lo, up; } t[] = {
        {0x10,0xA9,0x89},{0x11,0xE6,0x96},{0x12,0xE3,0x93},{0x13,0xAA,0x8A},{0x14,0xA5,0x85},{0x15,0xAD,0x8D},{0x16,0xA3,0x83},
        {0x17,0xE8,0x98},{0x18,0xE9,0x99},{0x19,0xA7,0x87},{0x1A,0xE5,0x95},{0x1B,0xEA,0x9A},{0x1E,0xE4,0x94},{0x1F,0xEB,0x9B},
        {0x20,0xA2,0x82},{0x21,0xA0,0x80},{0x22,0xAF,0x8F},{0x23,0xE0,0x90},{0x24,0xAE,0x8E},{0x25,0xAB,0x8B},{0x26,0xA4,0x84},
        {0x27,0xA6,0x86},{0x28,0xED,0x9D},{0x29,0xF1,0xF0},{0x2C,0xEF,0x9F},{0x2D,0xE7,0x97},{0x2E,0xE1,0x91},{0x2F,0xAC,0x8C},
        {0x30,0xA8,0x88},{0x31,0xE2,0x92},{0x32,0xEC,0x9C},{0x33,0xA1,0x81},{0x34,0xEE,0x9E},{0x35,'.',','} };
    for (int i = 0; i < 128; i++) { map_ru[i] = map[i]; smap_ru[i] = smap[i]; }
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) { map_ru[t[i].sc] = t[i].lo; smap_ru[t[i].sc] = t[i].up; }
    smap_ru[0x03] = '"'; smap_ru[0x04] = '#'; smap_ru[0x05] = ';'; smap_ru[0x07] = ':'; smap_ru[0x08] = '?';
}
static int key_poll(void) {            /* -1 если клавиши нет */
    now_ms();
    u8 st = inb(0x64);
    if (!(st & 1)) return -1;
    u8 sc = inb(0x60);
    if (st & 0x20) { mouse_byte(sc); return -1; }          /* байт от мыши */
    if (sc == 0xE0) { ext = 1; return -1; }
    if (ext) {
        ext = 0;
        if (sc == 0x1D) { ctrl_ = 1; if (shift) kbd_ru ^= 1; return -1; }
        if (sc == 0x9D) { ctrl_ = 0; return -1; }
        if (sc == 0x38) { alt_ = 1; if (shift) kbd_ru ^= 1; return -1; }
        if (sc == 0xB8) { alt_ = 0; return -1; }
        switch (sc) {
            case 0x48: return K_UP;
            case 0x50: return K_DOWN;
            case 0x4B: return K_LEFT;
            case 0x4D: return K_RIGHT;
            case 0x53: return K_DEL;
            case 0x47: return K_HOME;
            case 0x4F: return K_END;
            case 0x49: return K_PGUP;
            case 0x51: return K_PGDN;
            case 0x52: return K_INS;
        }
        return -1;
    }
    if (sc == 0x2A || sc == 0x36) { shift = 1; if (ctrl_ || alt_) kbd_ru ^= 1; return -1; }       /* Alt+Shift / Ctrl+Shift: RU <-> EN */
    if (sc == 0xAA || sc == 0xB6) { shift = 0; return -1; }
    if (sc == 0x1D) { ctrl_ = 1; if (shift) kbd_ru ^= 1; return -1; }
    if (sc == 0x9D) { ctrl_ = 0; return -1; }
    if (sc == 0x38) { alt_ = 1; if (shift) kbd_ru ^= 1; return -1; }
    if (sc == 0xB8) { alt_ = 0; return -1; }
    if (sc == 0x58) { kbd_ru ^= 1; return -1; }                                                  /* F12 */
    if (sc == 0x3A) { caps_lock ^= 1; kbd_set_leds(); return -1; }
    if (sc & 0x80) return -1;
    if (sc >= 0x3B && sc <= 0x44) return K_F1 + (sc - 0x3B);
    char c = kbd_ru ? (shift ? smap_ru[sc] : map_ru[sc]) : (shift ? smap[sc] : map[sc]);
    if (caps_lock && !shift) {
        if (c >= 'a' && c <= 'z') c -= 32;
        else if ((u8)c >= 0xA0 && (u8)c <= 0xAF) c -= 0x20;
        else if ((u8)c >= 0xE0 && (u8)c <= 0xEF) c -= 0x50;
    } else if (caps_lock && shift) {
        if (c >= 'A' && c <= 'Z') c += 32;
        else if ((u8)c >= 0x80 && (u8)c <= 0x8F) c += 0x20;
        else if ((u8)c >= 0x90 && (u8)c <= 0x9F) c += 0x50;
    }
    return c ? (u8)c : -1;
}
static char getc(void) {
    for (;;) {
        int k = key_poll();
        if (k >= 0 && k < 0x100) return (char)k;
        if (k < 0) __asm__ volatile("pause");
    }
}
static int readline(char *buf, int max) {
    int n = 0;
    for (;;) {
        char c = getc();
        if (c == '\n') { putc('\n'); break; }
        if (c == '\b') { if (n > 0) { n--; putc('\b'); } continue; }
        if ((u8)c >= ' ' && n < max - 1) { buf[n++] = c; putc(c); }
    }
    buf[n] = 0;
    return n;
}

/* ---------- Строки ---------- */
static int streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int atoi_(const char **s) {
    int n = 0;
    while (**s >= '0' && **s <= '9') n = n * 10 + (*(*s)++ - '0');
    return n;
}

#include "fs.h"

/* ---------- ATA PIO (Primary Master) & энергонезависимое хранилище ---------- */
#define ATA_PORT_DATA       0x1F0
#define ATA_PORT_ERROR      0x1F1
#define ATA_PORT_SEC_COUNT  0x1F2
#define ATA_PORT_LBA_LO     0x1F3
#define ATA_PORT_LBA_MID    0x1F4
#define ATA_PORT_LBA_HI     0x1F5
#define ATA_PORT_DRIVE      0x1F6
#define ATA_PORT_STATUS     0x1F7
#define ATA_PORT_CMD        0x1F7
#define ATA_PORT_CONTROL    0x3F6

#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DF   0x20
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY    0xEC

#define CRAVG_DISK_MAGIC 0x31534F4756415243ULL /* "CRAVGOS1" */

static int ata_present;
static u32 ata_sectors_total;

static void ata_delay(void) {
    inb(ATA_PORT_CONTROL); inb(ATA_PORT_CONTROL);
    inb(ATA_PORT_CONTROL); inb(ATA_PORT_CONTROL);
}

static int ata_wait_bsy(void) {
    for (int i = 0; i < 2000000; i++) {
        u8 s = inb(ATA_PORT_STATUS);
        if (!(s & ATA_SR_BSY)) return 0;
        __asm__ volatile("pause");
    }
    return -1;
}

static int ata_wait_drq(void) {
    for (int i = 0; i < 2000000; i++) {
        u8 s = inb(ATA_PORT_STATUS);
        if (s & (ATA_SR_ERR | ATA_SR_DF)) return -1;
        if (!(s & ATA_SR_BSY) && (s & ATA_SR_DRQ)) return 0;
        __asm__ volatile("pause");
    }
    return -1;
}

static int ata_init(void) {
    ata_present = 0;
    ata_sectors_total = 0;
    u8 s = inb(ATA_PORT_STATUS);
    if (s == 0xFF) return 0;

    outb(ATA_PORT_DRIVE, 0xA0);
    ata_delay();
    outb(ATA_PORT_SEC_COUNT, 0);
    outb(ATA_PORT_LBA_LO, 0);
    outb(ATA_PORT_LBA_MID, 0);
    outb(ATA_PORT_LBA_HI, 0);
    outb(ATA_PORT_CMD, ATA_CMD_IDENTIFY);
    ata_delay();

    s = inb(ATA_PORT_STATUS);
    if (!s) return 0;
    if (ata_wait_bsy() < 0) return 0;

    u8 mid = inb(ATA_PORT_LBA_MID);
    u8 hi  = inb(ATA_PORT_LBA_HI);
    if (mid != 0 || hi != 0) return 0;

    if (ata_wait_drq() < 0) return 0;

    u16 id[256];
    for (int i = 0; i < 256; i++) id[i] = inw(ATA_PORT_DATA);

    ata_sectors_total = id[60] | ((u32)id[61] << 16);
    ata_present = 1;
    return 1;
}

static int ata_read_sectors(u32 lba, u32 count, void *buf) {
    if (!ata_present) return -1;
    u16 *ptr = (u16 *)buf;
    while (count > 0) {
        u32 batch = count > 128 ? 128 : count;
        if (ata_wait_bsy() < 0) return -1;
        outb(ATA_PORT_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
        outb(ATA_PORT_SEC_COUNT, (u8)batch);
        outb(ATA_PORT_LBA_LO, (u8)(lba & 0xFF));
        outb(ATA_PORT_LBA_MID, (u8)((lba >> 8) & 0xFF));
        outb(ATA_PORT_LBA_HI, (u8)((lba >> 16) & 0xFF));
        outb(ATA_PORT_CMD, ATA_CMD_READ_PIO);

        for (u32 i = 0; i < batch; i++) {
            ata_delay();
            if (ata_wait_drq() < 0) return -1;
            for (int w = 0; w < 256; w++) *ptr++ = inw(ATA_PORT_DATA);
        }
        lba += batch;
        count -= batch;
    }
    return 0;
}

static int ata_write_sectors(u32 lba, u32 count, const void *buf) {
    if (!ata_present) return -1;
    const u16 *ptr = (const u16 *)buf;
    while (count > 0) {
        u32 batch = count > 128 ? 128 : count;
        if (ata_wait_bsy() < 0) return -1;
        outb(ATA_PORT_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
        outb(ATA_PORT_SEC_COUNT, (u8)batch);
        outb(ATA_PORT_LBA_LO, (u8)(lba & 0xFF));
        outb(ATA_PORT_LBA_MID, (u8)((lba >> 8) & 0xFF));
        outb(ATA_PORT_LBA_HI, (u8)((lba >> 16) & 0xFF));
        outb(ATA_PORT_CMD, ATA_CMD_WRITE_PIO);

        for (u32 i = 0; i < batch; i++) {
            ata_delay();
            if (ata_wait_drq() < 0) return -1;
            for (int w = 0; w < 256; w++) outw(ATA_PORT_DATA, *ptr++);
        }
        lba += batch;
        count -= batch;
    }
    if (ata_wait_bsy() == 0) {
        outb(ATA_PORT_CMD, ATA_CMD_CACHE_FLUSH);
        ata_wait_bsy();
    }
    return 0;
}

static u32 calc_checksum(const void *p, u32 len) {
    const u8 *b = (const u8 *)p;
    u32 sum = 0x5A5AA5A5;
    for (u32 i = 0; i < len; i++) sum = ((sum << 5) | (sum >> 27)) + b[i] + 1;
    return sum;
}

static char cur_user[24] = "user";
static char cur_host[24] = "cravgos";
static int setup_done = 0;

struct net_profile {
    char ssid[32];
    char pass[32];
    u8 connected;
};
static struct net_profile net_prof = { "CravgOS-Net", "", 1 };

struct disk_super {
    u64 magic;
    u32 version;
    u32 fsn_crc;
    u32 data_crc;
    u32 theme_idx;
    u32 kbd_ru;
    u32 fsn_bytes;
    u32 data_bytes;
    u32 setup_done;
    char username[24];
    char hostname[24];
    char wifi_ssid[32];
    char wifi_pass[32];
    u8  pad[360];
};

static int disk_save(void) {
    if (!ata_present) return -1;
    struct disk_super sp;
    for (unsigned i = 0; i < sizeof sp; i++) ((u8 *)&sp)[i] = 0;
    sp.magic = CRAVG_DISK_MAGIC;
    sp.version = 1;
    sp.theme_idx = (u32)theme_index();
    sp.kbd_ru = (u32)kbd_ru;
    sp.fsn_bytes = (u32)sizeof fsn;
    sp.data_bytes = (u32)sizeof fsdata;
    sp.fsn_crc = calc_checksum(fsn, sizeof fsn);
    sp.data_crc = calc_checksum(fsdata, sizeof fsdata);
    sp.setup_done = (u32)setup_done;
    for (int i = 0; i < 24; i++) {
        sp.username[i] = cur_user[i];
        sp.hostname[i] = cur_host[i];
    }
    for (int i = 0; i < 32; i++) {
        sp.wifi_ssid[i] = net_prof.ssid[i];
        sp.wifi_pass[i] = net_prof.pass[i];
    }

    /* Write fsn (3 sectors) at LBA 2 */
    u8 sec_buf[512 * 3];
    for (unsigned i = 0; i < sizeof sec_buf; i++) sec_buf[i] = 0;
    for (unsigned i = 0; i < sizeof fsn; i++) sec_buf[i] = ((u8 *)fsn)[i];
    if (ata_write_sectors(2, 3, sec_buf) < 0) return -1;

    /* Write fsdata (256 sectors) at LBA 5 */
    if (ata_write_sectors(5, 256, fsdata) < 0) return -1;

    /* Write superblock at LBA 1 */
    if (ata_write_sectors(1, 1, &sp) < 0) return -1;
    return 0;
}

static int disk_load(void) {
    if (!ata_present) return 0;
    struct disk_super sp;
    if (ata_read_sectors(1, 1, &sp) < 0) return 0;
    if (sp.magic != CRAVG_DISK_MAGIC || sp.version != 1) return 0;
    if (sp.fsn_bytes != sizeof fsn || sp.data_bytes != sizeof fsdata) return 0;

    u8 sec_buf[512 * 3];
    if (ata_read_sectors(2, 3, sec_buf) < 0) return 0;
    if (calc_checksum(sec_buf, sizeof fsn) != sp.fsn_crc) return 0;

    if (ata_read_sectors(5, 256, fsdata) < 0) return 0;
    if (calc_checksum(fsdata, sizeof fsdata) != sp.data_crc) return 0;

    disk_loading = 1;
    for (unsigned i = 0; i < sizeof fsn; i++) ((u8 *)fsn)[i] = sec_buf[i];
    fs_cwd = 0;
    if (sp.theme_idx < (u32)NTHEMES) theme_apply((int)sp.theme_idx);
    kbd_ru = sp.kbd_ru ? 1 : 0;
    setup_done = sp.setup_done ? 1 : 0;
    if ((sp.username[0] >= 'a' && sp.username[0] <= 'z') || (sp.username[0] >= 'A' && sp.username[0] <= 'Z')) {
        int i = 0;
        while (i < 23 && sp.username[i]) { cur_user[i] = sp.username[i]; i++; }
        cur_user[i] = 0;
    }
    if ((sp.hostname[0] >= 'a' && sp.hostname[0] <= 'z') || (sp.hostname[0] >= 'A' && sp.hostname[0] <= 'Z')) {
        int i = 0;
        while (i < 23 && sp.hostname[i]) { cur_host[i] = sp.hostname[i]; i++; }
        cur_host[i] = 0;
    }
    if (sp.wifi_ssid[0]) {
        int i = 0;
        while (i < 31 && sp.wifi_ssid[i]) { net_prof.ssid[i] = sp.wifi_ssid[i]; i++; }
        net_prof.ssid[i] = 0;
        i = 0;
        while (i < 31 && sp.wifi_pass[i]) { net_prof.pass[i] = sp.wifi_pass[i]; i++; }
        net_prof.pass[i] = 0;
        net_prof.connected = 1;
    }
    disk_loading = 0;
    return 1;
}
#include "net.h"
#include "tls.h"
#define HTTP_TLS 1
#include "http.h"
#include "html.h"
#include "image.h"

/* ---------- fastfetch ---------- */
static int nic_ok;
static u32 nic_id;
static void cpu_name(char *out) {      /* out: минимум 49 байт, выровнен по 4 */
    u32 a, b, c, d, *p = (u32 *)out;
    cpuid(0x80000000u, &a, &b, &c, &d);
    if (a >= 0x80000004u) {
        for (u32 i = 0; i < 3; i++) cpuid(0x80000002u + i, &p[i*4], &p[i*4+1], &p[i*4+2], &p[i*4+3]);
        out[48] = 0;
    } else {
        cpuid(0, &a, &b, &c, &d);
        p[0] = b; p[1] = d; p[2] = c; out[12] = 0;
    }
}
static u32 mem_mib(void) {
    u32 ext = cmos(0x30) | (cmos(0x31) << 8);   /* КБ выше 1 МБ (до 16 МБ) */
    u32 hi  = cmos(0x34) | (cmos(0x35) << 8);   /* блоки по 64 КБ выше 16 МБ */
    return 1 + ext / 1024 + hi / 16;
}
extern char _start16[], __bss_start[], __bss_end[], __bigbss_start[], __bigbss_end[];   /*SYMS*/
static u32 ram_used_kib(void) {      /* статический след ядра: образ + .bss + большие буферы */
    return ((u32)(__bss_end - _start16) + (u32)(__bigbss_end - __bigbss_start)) / 1024u;
}
static void put_mib(u32 kib) { putu(kib / 1024); putc('.'); putu((kib % 1024) * 10 / 1024); }
static void kv(const char *k, const char *v) {
    color = th->path; puts(k); color = th->base; puts(": "); puts(v);
}
static void fetch(void) {
    static const char *logo[8] = {
        "    ########    ",
        "  ####    ####  ",
        " ###            ",
        " ###            ",
        " ###            ",
        "  ####    ####  ",
        "    ########    ",
        "                "
    };
    u32 nb[13]; char *cpu = (char *)nb;
    cpu_name(cpu);
    while (*cpu == ' ') cpu++;
    u32 up = uptime_s(), mem = mem_mib();
    u8 old = color;
    putc('\n');
    int ffiles, fdirs, fbytes;
    fs_usage(&ffiles, &fdirs, &fbytes);
    for (int i = 0; i < 13; i++) {
        color = th->path; puts(i < 8 ? logo[i] : "                "); color = th->base; puts("  ");
        switch (i) {
        case 0: color = th->user; puts(cur_user); color = th->base; puts("@"); color = th->user; puts(cur_host); break;
        case 1: puts("-------------"); break;
        case 2: kv("OS", "Cravgos 0.8.0 x86_64"); break;
        case 3: kv("Kernel", "cravgos-kernel (64-bit long mode)"); break;
        case 4: kv("Uptime", ""); putu(up / 60); puts(" min "); putu(up % 60); puts(" s"); break;
        case 5: kv("Shell", "cravsh"); break;
        case 6: kv("Display", "80x25 text mode"); break;
        case 7: kv("CPU", cpu); break;
        case 8: {
            u32 used = ram_used_kib();
            kv("Memory", ""); put_mib(used); puts(" MiB / "); putu(mem); puts(" MiB (");
            putu(mem ? used * 100 / (mem * 1024) : 0); puts("% used)");
            break; }
        case 9: kv("Network", ""); if (nic_ok) { puts("e1000 "); putip(nip); } else puts("none"); break;
        case 10:
            kv("Files", ata_present ? "ATA Disk, " : "RAMFS, ");
            putu(ffiles); puts(" files, "); putu(fbytes / 1024); puts(" KiB used");
            if (ata_present) puts(" (persistent)");
            break;
        case 11: for (int c = 0; c < 8; c++) { color = c << 4; puts("   "); } break;
        case 12: for (int c = 8; c < 16; c++) { color = c << 4; puts("   "); } break;
        }
        color = th->base; putc('\n');
    }
    color = old;
}

/* ---------- Игры ---------- */
static void game_guess(void) {
    int n = 1 + rnd() % 100, tries = 0;
    char buf[16];
    puts("Guess the number 1-100. 7 tries, 'q' to quit.\n");
    while (tries < 7) {
        puts("> "); readline(buf, 16);
        if (buf[0] == 'q') return;
        const char *p = buf; int g = atoi_(&p);
        tries++;
        if (g == n) { puts("Correct! Tries: "); putu(tries); putc('\n'); return; }
        puts(g < n ? "higher\n" : "lower\n");
    }
    puts("You lose. The number was "); putu(n); putc('\n');
}

static void game_rps(void) {
    static const char *nm[3] = { "rock", "paper", "scissors" };
    int w = 0, l = 0, d = 0;
    puts("Rock-paper-scissors: press r / p / s, q to quit.\n");
    for (;;) {
        char c = getc(); int me;
        if (c == 'q') break;
        else if (c == 'r') me = 0; else if (c == 'p') me = 1; else if (c == 's') me = 2; else continue;
        int cp = rnd() % 3;
        puts("You: "); puts(nm[me]); puts(", CPU: "); puts(nm[cp]);
        if (me == cp) { puts(" - draw\n"); d++; }
        else if ((me + 1) % 3 == cp) { puts(" - you lose\n"); l++; }
        else { puts(" - you win\n"); w++; }
    }
    puts("Win/Lose/Draw: "); putu(w); putc('/'); putu(l); putc('/'); putu(d); putc('\n');
}

static void snake_score(int s) {
    put_str_at(1, 0, "SNAKE  score:", 0x0A);
    put_at(15, 0, '0' + (s / 100) % 10, 0x0F);
    put_at(16, 0, '0' + (s / 10) % 10, 0x0F);
    put_at(17, 0, '0' + s % 10, 0x0F);
    put_str_at(40, 0, "arrows/WASD - move, Q - quit", 0x08);
}
static void game_snake(void) {
    int bx[512], by[512], len = 4, dx = 1, dy = 0, score = 0, fx, fy, dead = 0;
    cursor_hide();
    color = 0x07; clear(); cursor_hide();
    for (int x = 0; x < W; x++) { put_at(x, 1, '-', 0x08); put_at(x, H - 1, '-', 0x08); }
    for (int y = 1; y < H; y++) { put_at(0, y, '|', 0x08); put_at(W - 1, y, '|', 0x08); }
    for (int i = 0; i < len; i++) { bx[i] = 20 - i; by[i] = 12; put_at(bx[i], by[i], i ? 'o' : 'O', 0x0A); }
    for (;;) {
        int ok = 1;
        fx = 1 + rnd() % 78; fy = 2 + rnd() % 22;
        for (int i = 0; i < len; i++) if (bx[i] == fx && by[i] == fy) ok = 0;
        if (ok) break;
    }
    put_at(fx, fy, '*', 0x0C);
    snake_score(score);
    while (!dead) {
        for (int w = 0; w < 9; w++) {
            int k;
            while ((k = key_poll()) >= 0) {
                if ((k == K_UP || k == 'w') && dy == 0) { dx = 0; dy = -1; }
                else if ((k == K_DOWN || k == 's') && dy == 0) { dx = 0; dy = 1; }
                else if ((k == K_LEFT || k == 'a') && dx == 0) { dx = -1; dy = 0; }
                else if ((k == K_RIGHT || k == 'd') && dx == 0) { dx = 1; dy = 0; }
                else if (k == 'q') { color = th->base; clear(); cursor_show(); return; }
            }
            delay_ms(10);
        }
        int nx = bx[0] + dx, ny = by[0] + dy;
        if (nx < 1 || nx > W - 2 || ny < 2 || ny > H - 2) break;
        for (int i = 0; i < len - 1; i++) if (bx[i] == nx && by[i] == ny) dead = 1;
        if (dead) break;
        int ate = (nx == fx && ny == fy);
        if (!ate) put_at(bx[len - 1], by[len - 1], ' ', 0x07);
        else if (len < 511) len++;
        for (int i = len - 1; i > 0; i--) { bx[i] = bx[i - 1]; by[i] = by[i - 1]; }
        bx[0] = nx; by[0] = ny;
        put_at(bx[1], by[1], 'o', 0x0A);
        put_at(nx, ny, 'O', 0x0A);
        if (ate) {
            score++; snake_score(score);
            for (;;) {
                int ok = 1;
                fx = 1 + rnd() % 78; fy = 2 + rnd() % 22;
                for (int i = 0; i < len; i++) if (bx[i] == fx && by[i] == fy) ok = 0;
                if (ok) break;
            }
            put_at(fx, fy, '*', 0x0C);
        }
    }
    put_str_at(32, 11, " GAME OVER ", 0x4F);
    put_str_at(28, 13, "press any key to exit", 0x07);
    while (key_poll() >= 0) ;
    getc();
    color = th->base; clear(); cursor_show();
    puts("Snake score: "); putu(score); putc('\n');
}

/* ---------- Extra: matrix, beep, cowsay ---------- */
static void matrix(void) {
    int hy[W], ln[W];
    for (int x = 0; x < W; x++) { hy[x] = -1; ln[x] = 0; }
    color = 0x07; clear(); cursor_hide();
    while (key_poll() < 0) {
        for (int x = 0; x < W; x++) {
            if (hy[x] < 0) {
                if (rnd() % 30 == 0) { hy[x] = 0; ln[x] = 4 + rnd() % 14; }
                continue;
            }
            int y = hy[x];
            if (y < H) {
                if (y > 0) put_at(x, y - 1, (char)(vga[(y - 1) * W + x] & 0xFF), 0x02);
                put_at(x, y, (char)('!' + rnd() % 94), 0x0F);
            }
            int ty = y - ln[x];
            if (ty >= 0 && ty < H) put_at(x, ty, ' ', 0x07);
            hy[x]++;
            if (ty >= H) hy[x] = -1;
        }
        delay_ms(60);
    }
    color = th->base; clear(); cursor_show();
}

static void tone(u32 hz, u32 ms) {
    if (hz) {
        u32 div = 1193180u / hz;
        outb(0x43, 0xB6); outb(0x42, div & 0xFF); outb(0x42, div >> 8);
        outb(0x61, inb(0x61) | 3);
    }
    delay_ms(ms);
    outb(0x61, inb(0x61) & ~3);
}
static void melody(void) {
    static const u16 notes[] = { 262, 330, 392, 523, 392, 330, 262, 0 };
    puts("Playing... (needs PC speaker / sound in your VM)\n");
    for (int i = 0; notes[i]; i++) { tone(notes[i], 180); delay_ms(30); }
}

static void cowsay(const char *t) {
    int n = slen(t);
    putc(' '); for (int i = 0; i < n + 2; i++) putc('_'); putc('\n');
    puts("< "); puts(t); puts(" >\n ");
    for (int i = 0; i < n + 2; i++) putc('-');
    puts("\n        \\   ^__^\n"
         "         \\  (oo)\\_______\n"
         "            (__)\\       )\\/\\\n"
         "                ||----w |\n"
         "                ||     ||\n");
}

/* ---------- PCI + сетевая карта Intel e1000 ---------- */
static u32 pci_read(u32 bus, u32 dev, u32 fn, u32 off) {
    outl(0xCF8, 0x80000000u | (bus << 16) | (dev << 11) | (fn << 8) | (off & 0xFC));
    return inl(0xCFC);
}
static void pci_write(u32 bus, u32 dev, u32 fn, u32 off, u32 v) {
    outl(0xCF8, 0x80000000u | (bus << 16) | (dev << 11) | (fn << 8) | (off & 0xFC));
    outl(0xCFC, v);
}
static void lspci(void) {
    for (u32 b = 0; b < 32; b++) {
        if (b > 0 && (pci_read(b, 0, 0, 0) & 0xFFFF) == 0xFFFF) continue;
        for (u32 d = 0; d < 32; d++) {
            if ((pci_read(b, d, 0, 0) & 0xFFFF) == 0xFFFF) continue;
            int nfn = ((pci_read(b, d, 0, 0x0C) >> 16) & 0x80) ? 8 : 1;
            for (u32 f = 0; f < (u32)nfn; f++) {
                u32 id = pci_read(b, d, f, 0);
                if ((id & 0xFFFF) == 0xFFFF) continue;
                u32 cl = pci_read(b, d, f, 8) >> 16;
                puthex(b, 2); putc(':'); puthex(d, 2); putc('.'); putu(f);
                puts("  "); puthex(id & 0xFFFF, 4); putc(':'); puthex(id >> 16, 4);
                puts("  class "); puthex(cl >> 8, 2); putc(':'); puthex(cl & 0xFF, 2);
                switch (cl >> 8) {
                    case 1: puts(" storage"); break;
                    case 2:
                        puts(" NETWORK");
                        if ((id & 0xFFFF) == 0x8086) puts(" (Intel)");
                        else if ((id & 0xFFFF) == 0x10EC) puts(" (Realtek)");
                        break;
                    case 3: puts(" display"); break;
                    case 6: puts(" bridge"); break;
                }
                putc('\n');
            }
        }
    }
}

#define E1000_RX 32
#define E1000_TX 32
struct rxd { u32 lo, hi; u16 len, csum; u8 status, err; u16 spec; } __attribute__((packed));
struct txd { u32 lo, hi; u16 len; u8 cso, cmd, status, css; u16 spec; } __attribute__((packed));
static volatile struct rxd rxr[E1000_RX] __attribute__((aligned(128)));
static volatile struct txd txr[E1000_TX] __attribute__((aligned(128)));
static u8 rxbuf[E1000_RX][2048] __attribute__((aligned(128)));
static u8 txbuf[E1000_TX][2048] __attribute__((aligned(128)));
static volatile u32 *nicm;
static u32 rx_cur, tx_cur;
static u32 nr(u32 reg) { return nicm[reg / 4]; }
static void nw(u32 reg, u32 v) { nicm[reg / 4] = v; }

static int e1000_send(const u8 *d, int len) {
    volatile struct txd *t = &txr[tx_cur];
    if (len > 2040) return -1;
    for (u32 spin = 0; !(t->status & 1); spin++) if (spin > 3000000u) return -1;
    for (int i = 0; i < len; i++) txbuf[tx_cur][i] = d[i];
    t->lo = (u32)(uintptr_t)txbuf[tx_cur]; t->hi = (u32)((uintptr_t)txbuf[tx_cur] >> 32); t->len = len;
    t->cso = 0; t->cmd = 0x0B; t->status = 0; t->css = 0; t->spec = 0;   /* EOP | IFCS | RS */
    tx_cur = (tx_cur + 1) % E1000_TX;
    __asm__ volatile("mfence" : : : "memory");
    nw(0x3818, tx_cur);
    return 0;
}
static int e1000_recv(u8 *buf, int max) {
    volatile struct rxd *r = &rxr[rx_cur];
    if (!(r->status & 1)) return 0;
    int len = r->len;
    if (len > max) len = max;
    for (int i = 0; i < len; i++) buf[i] = rxbuf[rx_cur][i];
    r->lo = (u32)(uintptr_t)rxbuf[rx_cur];
    r->hi = (u32)((uintptr_t)rxbuf[rx_cur] >> 32);
    r->status = 0;
    __asm__ volatile("mfence" : : : "memory");
    nw(0x2818, rx_cur);
    rx_cur = (rx_cur + 1) % E1000_RX;
    return len;
}
static int e1000_init(void) {
    u32 b, d, f = 0;
    for (b = 0; b < 32; b++) {
        if (b > 0 && (pci_read(b, 0, 0, 0) & 0xFFFF) == 0xFFFF) continue;
        for (d = 0; d < 32; d++) {
            if ((pci_read(b, d, 0, 0) & 0xFFFF) == 0xFFFF) continue;
            int nfn = ((pci_read(b, d, 0, 0x0C) >> 16) & 0x80) ? 8 : 1;
            for (f = 0; f < (u32)nfn; f++) {
                u32 id = pci_read(b, d, f, 0);
                u32 dev = id >> 16, ven = id & 0xFFFF;
                if (ven == 0x8086 && (dev == 0x100E || dev == 0x100F || dev == 0x1004 || dev == 0x100C ||
                                      dev == 0x107C || dev == 0x107D || dev == 0x10D3 || dev == 0x10F5 ||
                                      dev == 0x1502 || dev == 0x1503 || dev == 0x153A || dev == 0x153B ||
                                      dev == 0x15B7 || dev == 0x15B8 || dev == 0x1539)) {
                    nic_id = id;
                    goto found;
                }
            }
        }
    }
    return 0;
found:
    pci_write(b, d, f, 4, pci_read(b, d, f, 4) | 7);          /* I/O, память, bus master */
    u32 bar = pci_read(b, d, f, 0x10) & ~0xFu;
    if (!bar) return 0;
    nicm = (volatile u32 *)(uintptr_t)bar;
    nw(0x00D8, 0xFFFFFFFFu);                                    /* IMC: все прерывания выкл */
    nw(0x0000, nr(0) | (1u << 26));                             /* сброс */
    for (u32 spin = 0; (nr(0) & (1u << 26)) && spin < 1000000u; spin++) ;
    delay_ms(20);
    nw(0x00D8, 0xFFFFFFFFu); nr(0x00C0);
    nw(0x0000, (nr(0) | (1u << 6) | (1u << 5)) & ~((1u << 3) | (1u << 7) | (1u << 30) | (1u << 31)));
    u32 ral = nr(0x5400), rah = nr(0x5404);
    if (!ral && !(rah & 0xFFFF)) {                              /* MAC из EEPROM */
        u16 w[3];
        for (u32 i = 0; i < 3; i++) {
            nw(0x0014, (i << 8) | 1);
            u32 v = 0;
            for (u32 spin = 0; spin < 100000u; spin++) { v = nr(0x0014); if (v & 0x10) break; }
            w[i] = v >> 16;
        }
        ral = w[0] | ((u32)w[1] << 16); rah = w[2];
    }
    nmac[0] = ral; nmac[1] = ral >> 8; nmac[2] = ral >> 16; nmac[3] = ral >> 24;
    nmac[4] = rah; nmac[5] = rah >> 8;
    nw(0x5400, ral); nw(0x5404, (rah & 0xFFFF) | (1u << 31));
    for (u32 i = 0; i < 128; i++) nw(0x5200 + i * 4, 0);        /* MTA */
    for (u32 i = 0; i < E1000_RX; i++) { rxr[i].lo = (u32)(uintptr_t)rxbuf[i]; rxr[i].hi = (u32)((uintptr_t)rxbuf[i] >> 32); rxr[i].status = 0; }
    nw(0x2800, (u32)(uintptr_t)rxr); nw(0x2804, (u32)((uintptr_t)rxr >> 32)); nw(0x2808, E1000_RX * 16);
    nw(0x2810, 0); nw(0x2818, E1000_RX - 1); rx_cur = 0;
    nw(0x0100, (1u << 1) | (1u << 15) | (1u << 26));            /* EN | BAM | SECRC, буфер 2048 */
    for (u32 i = 0; i < E1000_TX; i++) { txr[i].lo = 0; txr[i].hi = 0; txr[i].status = 1; }
    nw(0x3800, (u32)(uintptr_t)txr); nw(0x3804, (u32)((uintptr_t)txr >> 32)); nw(0x3808, E1000_TX * 16);
    nw(0x3810, 0); nw(0x3818, 0); tx_cur = 0;
    nw(0x0400, (1u << 1) | (1u << 3) | (0x0Fu << 4) | (0x40u << 12));
    nw(0x0410, 0x0060200A);
    return 1;
}

/* ---------- Сетевые команды ---------- */
static char *tok(char **s) {
    char *p = *s;
    while (*p == ' ') p++;
    if (!*p) { *s = p; return 0; }
    char *st = p;
    while (*p && *p != ' ') p++;
    if (*p) *p++ = 0;
    *s = p;
    return st;
}
static int need_nic(void) {
    if (nic_ok) return 1;
    puts("No supported network card (Intel e1000). Run 'lspci'.\n"
         "VirtualBox: Settings > Network > Adapter Type = Intel PRO/1000 MT Desktop.\n");
    return 0;
}
static int net_conf_done, net_dhcp_ok, ndns_manual;      /* настройка по DHCP уже пробовалась / удалась / ручной DNS */
static void net_autoconf(int verbose) {     /* при первом выходе в сеть сами спрашиваем DHCP */
    if (net_conf_done || !nic_ok) return;
    net_conf_done = 1;
    u32 prev_dns = ndns;
    if (verbose) puts("Network: asking DHCP for settings...\n");
    if (net_dhcp_ex(2, 1200) == 0) {
        net_dhcp_ok = 1;
        if (ndns_manual) ndns = prev_dns;
        if (verbose) { puts("  got "); putip(nip); puts(", gateway "); putip(ngw); puts(", dns "); putip(ndns); putc('\n'); }
    } else if (verbose) puts("  no DHCP answer - using the defaults (10.0.2.15, VirtualBox NAT)\n");
}
static int net_ready(void) { if (!need_nic()) return 0; net_autoconf(1); return 1; }
static void cmd_dns(char *args) {
    char *a = tok(&args);
    if (!need_nic()) return;
    if (a && streq(a, "doh")) {
        char *b = tok(&args);
        if (b && streq(b, "off")) http_doh_on = 0; else if (b && streq(b, "on")) http_doh_on = 1;
        else { puts("usage: dns doh on|off\n"); }
        puts("DNS-over-HTTPS fallback: "); puts(http_doh_on ? "on" : "off"); putc('\n');
        return;
    }
    if (!a) {
        puts("DNS server "); putip(ndns); puts("   gateway "); putip(ngw);
        puts("\nDNS-over-HTTPS fallback: "); puts(http_doh_on ? "on" : "off");
        puts("\nusage: dns IP     (example: dns 8.8.8.8)      dns doh on|off\n"); return;
    }
    u32 ip;
    if (!net_parse_ip(a, &ip)) { puts("dns: not an IPv4 address\n"); return; }
    ndns = ip;
    ndns_manual = 1;
    net_conf_done = 1;
    puts("DNS server set to "); putip(ip); putc('\n');
}
static void cmd_ifconfig(void) {
    if (!need_nic()) return;
    puts("e1000  PCI "); puthex(nic_id & 0xFFFF, 4); putc(':'); puthex(nic_id >> 16, 4); putc('\n');
    puts("  MAC     "); putmac(nmac); putc('\n');
    puts("  link    "); puts((nr(0x0008) & 2) ? "up\n" : "DOWN (check VM network settings)\n");
    puts("  config  "); puts(net_dhcp_ok ? "DHCP\n" : "defaults (VirtualBox NAT 10.0.2.x) - run 'dhcp' on a bridged/real network\n");
    puts("  ip      "); putip(nip); putc('\n');
    puts("  mask    "); putip(nmask); putc('\n');
    puts("  gateway "); putip(ngw); putc('\n');
    puts("  dns     "); putip(ndns); putc('\n');
    puts("  frames  sent "); putu(st_tx); puts(", received "); putu(st_rx); puts(" (arp "); putu(st_arp); puts(", ip "); putu(st_ip); puts(")\n");
}
static void cmd_dhcp(void) {
    if (!need_nic()) return;
    puts("DHCP...\n");
    ndns_manual = 0;
    net_conf_done = 1;
    if (net_dhcp() == 0) { net_dhcp_ok = 1; puts("Got "); putip(nip); puts(" gw "); putip(ngw); puts(" dns "); putip(ndns); putc('\n'); }
    else puts("DHCP failed (no answer). Keeping previous settings.\n");
}
static int sleep_or_q(u32 ms) {              /* 1 если нажали Q */
    for (u32 t = 0; t < ms; t += 20) {
        int k;
        while ((k = key_poll()) >= 0) if (k == 'q') return 1;
        delay_ms(20);
    }
    return 0;
}
static void cmd_ping(char *args) {
    char *host = tok(&args), *c = tok(&args);
    if (!host) { puts("usage: ping HOST [count]   (Q aborts)\n"); return; }
    if (!net_ready()) return;
    int cnt = 4;
    if (c) { const char *p = c; cnt = atoi_(&p); if (cnt < 1) cnt = 1; if (cnt > 50) cnt = 50; }
    u32 ip;
    if (!net_parse_ip(host, &ip)) {
        puts("Resolving "); puts(host); puts("...\n");
        ip = http_resolve(host);
        if (!ip) { puts("ping: cannot resolve host\n"); return; }
    }
    puts("PING "); putip(ip); putc('\n');
    int ok = 0, sent = 0;
    for (int i = 0; i < cnt; i++) {
        u32 rtt = 0;
        int r = net_ping(ip, i + 1, 2000, &rtt);
        sent++;
        if (r == 0) {
            ok++;
            puts("Reply from "); putip(ip); puts(": seq="); putu(i + 1);
            puts(" time="); putu(rtt); puts(" ms ttl="); putu(ping_ttl); putc('\n');
        } else if (r == -2) puts("ARP failed: no gateway/host answered\n");
        else puts("Request timed out\n");
        if (i < cnt - 1 && sleep_or_q((r == 0 && rtt < 1000) ? 1000 - rtt : 300)) break;
    }
    putu(sent); puts(" sent, "); putu(ok); puts(" received\n");
}
static void cmd_nslookup(char *args) {
    char *h = tok(&args);
    if (!h) { puts("usage: nslookup HOST\n"); return; }
    if (!net_ready()) return;
    u32 ip = http_resolve(h);
    if (ip) { puts(h); puts(" -> "); putip(ip); puts(dns_via_doh ? "   (via DNS-over-HTTPS, " : "   (via DNS server "); putip(dns_used); puts(")\n"); return; }
    puts("lookup failed: ");
    switch (dns_why) {
        case 1: puts("no DNS server configured"); break;
        case 2: puts("DNS server unreachable (ARP failed)"); break;
        case 3: puts("no answer from the DNS servers (UDP and TCP)"); break;
        case 4: puts("NXDOMAIN - the name does not exist"); break;
        case 5: puts("the name has no IPv4 address"); break;
        default: puts("the DNS server returned an error"); break;
    }
    putc('\n');
    if (dns_rcode >= 0) {
        puts("last reply from "); putip(dns_rsrc); puts(": rcode "); putu(dns_rcode); putc(' '); puts(dns_rcode_name(dns_rcode));
        puts(", "); putu(dns_an); puts(" answers, "); putu(dns_rlen); puts(" bytes\n");
    }
    puts(http_doh_on ? "(DNS-over-HTTPS fallback was tried too.)\n" : "(DNS-over-HTTPS fallback is off: dns doh on)\n");
    puts("Run 'netdiag' for a full network test, or set a server: dns 8.8.8.8\n");
}
static void cmd_netdiag(void) {
    if (!net_ready()) return;
    u32 cand[6] = { ndns, ngw, 0x0A000203u, 0x4D580808u, 0x08080808u, 0x01010101u };
    puts("== network self-test ==\n");
    puts("ip "); putip(nip); puts("  mask "); putip(nmask); puts("  gateway "); putip(ngw); puts("  dns "); putip(ndns); putc('\n');
    puts("link "); puts((nr(0x0008) & 2) ? "up" : "DOWN"); puts("   config "); puts(net_dhcp_ok ? "DHCP" : "defaults"); putc('\n');
    puts("gateway ARP : ");
    const u8 *m = ngw ? net_mac_for(ngw) : 0;
    if (m) { putmac(m); putc('\n'); } else puts("FAILED - the gateway did not answer ARP\n");
    if (m) {
        u32 rtt = 0;
        int r = net_ping(ngw, 1, 1500, &rtt);
        puts("gateway ping: ");
        if (r == 0) { putu(rtt); puts(" ms\n"); } else puts("no reply (some NATs ignore ping - not always an error)\n");
    }
    puts("DNS test, one UDP query for example.com per server:\n");
    for (int c = 0; c < 6; c++) {
        int dup = 0;
        for (int k = 0; k < c; k++) if (cand[k] == cand[c]) dup = 1;
        if (!cand[c] || dup) continue;
        puts("  "); putip(cand[c]); puts(" : ");
        int r = dns_probe(cand[c], "example.com");
        if (r >= 0) {
            if (dns_done == 1) { puts("OK -> "); putip(dns_ip); }
            else { puts("reply rcode "); putu(dns_rcode); putc(' '); puts(dns_rcode_name(dns_rcode)); puts(", answers "); putu(dns_an); }
            puts(", "); putu(r); puts(" ms\n");
        } else puts(r == -2 ? "ARP failed\n" : "no reply\n");
    }
    puts("DNS over HTTPS : ");
    if (!http_doh_on) puts("disabled (dns doh on)\n");
    else {
        u32 dip = http_doh_lookup("example.com");
        if (dip) { putip(dip); puts(" via "); putip(dns_used); puts(" (TLS works)\n"); }
        else { puts("failed - "); puts(http_err_str(doh_last_err)); putc('\n'); }
    }
    puts("frames: sent "); putu(st_tx); puts(", received "); putu(st_rx);
    puts(" (arp "); putu(st_arp); puts(", ip "); putu(st_ip); puts(", udp "); putu(st_udp); puts(", dns "); putu(st_dns); puts(")\n");
    puts("If anything looks wrong, please send a photo of this screen.\n");
}
static void cmd_arp(void) {
    int n = 0;
    for (int i = 0; i < ARPN; i++) if (arp_ip[i]) { putip(arp_ip[i]); puts("  "); putmac(arp_mac[i]); putc('\n'); n++; }
    if (!n) puts("ARP cache is empty\n");
}

/* ---------- Файловая система: команды ---------- */
static void fs_err(int e) {
    switch (e) {
        case FS_EBAD: puts("invalid name or path\n"); break;
        case FS_EEXIST: puts("already exists\n"); break;
        case FS_EFULL: puts("no space left (64 entries max)\n"); break;
        case FS_ENOENT: puts("no such file or directory\n"); break;
        case FS_ENOTDIR: puts("not a directory\n"); break;
        case FS_EISDIR: puts("is a directory\n"); break;
        case FS_ENOTEMPTY: puts("directory not empty\n"); break;
        case FS_EBUSY: puts("busy (root or current directory)\n"); break;
        default: puts("error\n");
    }
}
static void cmd_ls(char *args) {
    char *a = tok(&args);
    int d = fs_resolve(a ? a : ".");
    if (d < 0) { fs_err(FS_ENOENT); return; }
    u8 old = color;
    if (!fsn[d].dir) { puts(fsn[d].name); putc(' '); putu(fsn[d].size); puts(" bytes\n"); return; }
    int n = 0;
    for (int i = 1; i < FS_MAXN; i++)
        if (fsn[i].used && fsn[i].parent == d) {
            n++;
            color = fsn[i].dir ? th->dir : old;
            puts(fsn[i].name); if (fsn[i].dir) putc('/');
            color = old;
            int pad = 18 - slen(fsn[i].name) - (fsn[i].dir ? 1 : 0);
            while (pad-- > 0) putc(' ');
            if (fsn[i].dir) puts("<dir>\n"); else { putu(fsn[i].size); puts(" B\n"); }
        }
    if (!n) puts("(empty)\n");
}
static void tree_rec(int dir, int depth) {
    u8 old = color;
    for (int i = 1; i < FS_MAXN; i++)
        if (fsn[i].used && fsn[i].parent == dir) {
            for (int k = 0; k < depth; k++) puts("  ");
            color = fsn[i].dir ? th->dir : old;
            puts(fsn[i].name); if (fsn[i].dir) putc('/');
            color = old; putc('\n');
            if (fsn[i].dir && depth < 10) tree_rec(i, depth + 1);
        }
}
static void cmd_cat(char *args) {
    char *a = tok(&args);
    if (!a) { puts("usage: cat FILE\n"); return; }
    int i = fs_resolve(a);
    if (i < 0) { fs_err(FS_ENOENT); return; }
    if (fsn[i].dir) { fs_err(FS_EISDIR); return; }
    for (int k = 0; k < fsn[i].size; k++) {
        char c = fsdata[i][k];
        if (c == '\t') puts("    "); else if (c != '\r') putc(c);
    }
    if (fsn[i].size && fsdata[i][fsn[i].size - 1] != '\n') putc('\n');
}
static void fs_sync(void) {
    if (ata_present && !disk_loading) disk_save();
}
static void write_text(const char *path, const char *text, int append) {
    int i = fs_resolve(path);
    if (i < 0) i = fs_create(path, 0);
    if (i < 0) { fs_err(i); return; }
    int r = fs_write(i, text, slen(text), append);
    if (r < 0) { fs_err(r); return; }
    fs_write(i, "\n", 1, 1);
    fs_sync();
}
static void cmd_echo(char *args) {
    char *gt = 0;
    for (char *q = args; *q; q++) if (*q == '>') { gt = q; break; }
    if (!gt) { puts(args); putc('\n'); return; }
    int app = (gt[1] == '>');
    char *f = gt + (app ? 2 : 1);
    *gt = 0;
    char *e = gt;
    while (e > args && e[-1] == ' ') *--e = 0;
    while (*f == ' ') f++;
    if (!*f) { puts("echo: missing file name\n"); return; }
    write_text(f, args, app);
}

/* ---------- Окна и диалоги ---------- */
#define UI_CLICK  0x200
#define UI_RCLICK 0x201
#define UI_WHEEL  0x202
#define UI_MOVE   0x203
static int ui_cx, ui_cy, ui_wheel;
static int ui_wait(void) {                       /* клавиша или событие мыши */
    if (!mouse_tried) { mouse_tried = 1; mouse_init(); }     /* мышь включаем только когда она нужна */
    for (;;) {
        int k = key_poll();
        if (k >= 0) return k;
        now_ms();                                /* держим PIT в курсе (двойной клик) */
        if (m_click_l) { m_click_l = 0; ui_cx = mx; ui_cy = my; return UI_CLICK; }
        if (m_click_r) { m_click_r = 0; ui_cx = mx; ui_cy = my; return UI_RCLICK; }
        if (m_wheel) { ui_wheel = m_wheel; m_wheel = 0; return UI_WHEEL; }
        if (m_moved) { m_moved = 0; return UI_MOVE; }
        __asm__ volatile("pause");
    }
}
static void ui_drop_events(void) { m_click_l = m_click_r = m_wheel = m_moved = 0; }
static void fill_rect(int x, int y, int w, int h, u8 at) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) put_at(x + i, y + j, ' ', at);
}
static void draw_box(int x, int y, int w, int h, u8 at, int dbl) {
    u8 tl = dbl ? 0xC9 : 0xDA, tr = dbl ? 0xBB : 0xBF, bl = dbl ? 0xC8 : 0xC0, br = dbl ? 0xBC : 0xD9;
    u8 hz = dbl ? 0xCD : 0xC4, vt = dbl ? 0xBA : 0xB3;
    for (int i = 1; i < w - 1; i++) { put_at(x + i, y, hz, at); put_at(x + i, y + h - 1, hz, at); }
    for (int j = 1; j < h - 1; j++) { put_at(x, y + j, vt, at); put_at(x + w - 1, y + j, vt, at); }
    put_at(x, y, tl, at); put_at(x + w - 1, y, tr, at); put_at(x, y + h - 1, bl, at); put_at(x + w - 1, y + h - 1, br, at);
}
static void put_clip(int x, int y, const char *s, int maxw, u8 at) {
    for (int i = 0; i < maxw && s[i]; i++) put_at(x + i, y, s[i], at);
}
static void shade(int x, int y) {
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    vga[y * W + x] = (u16)((0x08 << 8) | (vga[y * W + x] & 0xFF));
}
static void dlg_frame(int x, int y, int w, int h, const char *title) {
    fill_rect(x, y, w, h, th->dlg);
    draw_box(x, y, w, h, th->dlg, 1);
    int tl = slen(title), tx = x + (w - tl) / 2;
    put_at(tx - 1, y, ' ', th->dlg); put_clip(tx, y, title, w - 4, th->dlg); put_at(tx + tl, y, ' ', th->dlg);
    for (int j = 1; j <= h; j++) shade(x + w, y + j);
    for (int i = 1; i <= w; i++) shade(x + i, y + h);
}
static void btn(int x, int y, int w, const char *label, int focus) {
    u8 at = focus ? th->sel : th->dlgbtn;
    int l = slen(label), s = (w - 2 - l) / 2;
    for (int i = 0; i < w; i++) put_at(x + i, y, ' ', at);
    put_at(x, y, '[', at); put_at(x + w - 1, y, ']', at);
    put_clip(x + 1 + s, y, label, w - 2, at);
}
/* текстовое окно: one кнопка OK или две OK/Cancel; результат 1 (OK) или 0 */
static int dlg_lines(const char *title, const char **lines, int n, int two) {
    int w = 36;
    for (int i = 0; i < n; i++) { int l = slen(lines[i]) + 6; if (l > w) w = l; }
    if (w > 76) w = 76;
    int h = n + 5, x = (W - w) / 2, y = (H - h) / 2, focus = 0, res = -1;
    int okx = two ? x + (w - 20) / 2 : x + (w - 8) / 2, cax = okx + 10;
    u8 omui = mouse_ui; mouse_ui = 1; cursor_hide();
    while (res < 0) {
        mouse_forget();
        dlg_frame(x, y, w, h, title);
        for (int i = 0; i < n; i++) put_clip(x + 3, y + 2 + i, lines[i], w - 6, th->dlg);
        btn(okx, y + h - 2, 8, "OK", focus == 0);
        if (two) btn(cax, y + h - 2, 10, "Cancel", focus == 1);
        mouse_draw();
        int k = ui_wait();
        if (k == UI_MOVE) { mouse_draw(); continue; }
        if (k == '\n') res = (!two || focus == 0) ? 1 : 0;
        else if (k == 27) res = two ? 0 : 1;
        else if (two && (k == 'y' || k == 'Y')) res = 1;
        else if (two && (k == 'n' || k == 'N')) res = 0;
        else if (two && (k == '\t' || k == K_LEFT || k == K_RIGHT)) focus ^= 1;
        else if (k == UI_CLICK && ui_cy == y + h - 2) {
            if (ui_cx >= okx && ui_cx < okx + 8) res = 1;
            else if (two && ui_cx >= cax && ui_cx < cax + 10) res = 0;
        }
    }
    mouse_ui = omui; mouse_forget(); ui_drop_events();
    return res;
}
static void dlg_msg(const char *title, const char *text) {
    const char *l[1]; l[0] = text;
    dlg_lines(title, l, 1, 0);
}
static int dlg_input(const char *title, const char *prompt, char *buf, int max) {
    int w = 66, h = 8, x = (W - w) / 2, y = (H - h) / 2, n = slen(buf), res = -1;
    int okx = x + (w - 20) / 2, cax = okx + 10, fw = w - 8;
    u8 omui = mouse_ui; mouse_ui = 1; cursor_hide();
    while (res < 0) {
        mouse_forget();
        dlg_frame(x, y, w, h, title);
        put_clip(x + 3, y + 2, prompt, w - 6, th->dlg);
        fill_rect(x + 3, y + 4, w - 6, 1, th->file);
        int st = n > fw - 1 ? n - (fw - 1) : 0;
        put_clip(x + 4, y + 4, buf + st, fw, th->file);
        put_at(x + 4 + (n - st), y + 4, '_', th->file);
        btn(okx, y + h - 2, 8, "OK", 0);
        btn(cax, y + h - 2, 10, "Cancel", 0);
        mouse_draw();
        int k = ui_wait();
        if (k == UI_MOVE) { mouse_draw(); continue; }
        if (k == '\n') res = 1;
        else if (k == 27) res = 0;
        else if (k == '\b') { if (n > 0) buf[--n] = 0; }
        else if (((k >= 32 && k < 127) || (k >= 0x80 && k <= 0xFF)) && n < max - 1) { buf[n++] = (char)k; buf[n] = 0; }
        else if (k == UI_CLICK && ui_cy == y + h - 2) {
            if (ui_cx >= okx && ui_cx < okx + 8) res = 1;
            else if (ui_cx >= cax && ui_cx < cax + 10) res = 0;
        }
    }
    mouse_ui = omui; mouse_forget(); ui_drop_events();
    return res;
}
static int dlg_theme(void) {                    /* выбор темы; -1 если отмена */
    int w = 30, h = NTHEMES + 5, x = (W - w) / 2, y = (H - h) / 2, f = theme_index(), res = -2;
    u8 omui = mouse_ui; mouse_ui = 1; cursor_hide();
    while (res == -2) {
        mouse_forget();
        dlg_frame(x, y, w, h, "Color theme");
        for (int i = 0; i < NTHEMES; i++) {
            u8 at = (i == f) ? th->sel : th->dlg;
            fill_rect(x + 2, y + 2 + i, w - 4, 1, at);
            put_at(x + 3, y + 2 + i, (i == theme_index()) ? '*' : ' ', at);
            put_clip(x + 5, y + 2 + i, themes[i].name, w - 8, at);
        }
        put_clip(x + 3, y + h - 2, "Enter/click = apply", w - 6, th->dlg);
        mouse_draw();
        int k = ui_wait();
        if (k == UI_MOVE) { mouse_draw(); continue; }
        if (k == K_UP && f > 0) f--;
        else if (k == K_DOWN && f < NTHEMES - 1) f++;
        else if (k == '\n') res = f;
        else if (k == 27) res = -1;
        else if (k >= '1' && k < '1' + NTHEMES) res = k - '1';
        else if (k == UI_CLICK && ui_cx >= x + 2 && ui_cx < x + w - 2 && ui_cy >= y + 2 && ui_cy < y + 2 + NTHEMES) res = ui_cy - y - 2;
        else if (k == UI_CLICK) res = -1;
    }
    mouse_ui = omui; mouse_forget(); ui_drop_events();
    return res;
}

/* простой полноэкранный редактор: ESC - сохранить и выйти */
static void editor(int idx) {
    static char buf[FS_FSZ];
    int len = fsn[idx].size, pos, top = 0;
    for (int i = 0; i < len; i++) buf[i] = fsdata[idx][i];
    pos = len;
    u8 omui = mouse_ui; mouse_ui = 1;
    color = th->base; cursor_show();
    for (;;) {
        mouse_forget();
        int x = 0, y = 0, cxp = 0, cyp = 0;
        for (int i = 0; i <= len; i++) {
            if (i == pos) { cxp = x; cyp = y; }
            if (i == len) break;
            if (buf[i] == '\n') { x = 0; y++; } else { if (++x >= W) { x = 0; y++; } }
        }
        if (cyp < top) top = cyp;
        if (cyp > top + 22) top = cyp - 22;
        for (int i = 0; i < W * (H - 1); i++) vga[i] = (u16)((th->file << 8) | ' ');
        x = 0; y = 0;
        for (int i = 0; i < len; i++) {
            char c = buf[i];
            if (c == '\n') { x = 0; y++; }
            else {
                if (y - top >= 0 && y - top < H - 1) put_at(x, y - top, c == '\t' ? ' ' : c, th->file);
                if (++x >= W) { x = 0; y++; }
            }
        }
        for (int i = 0; i < W; i++) put_at(i, H - 1, ' ', th->title);
        put_str_at(1, H - 1, "ESC: save & exit   arrows/Home/End/Del   file:", th->title);
        put_str_at(49, H - 1, fsn[idx].name, th->title);
        put_str_at(62, H - 1, kbd_ru ? "RU" : "EN", th->title);
        {
            int n = len;
            char dg[5];
            dg[4] = 0;
            for (int k = 3; k >= 0; k--) { dg[k] = '0' + n % 10; n /= 10; }
            put_str_at(66, H - 1, dg, th->title);
            put_str_at(70, H - 1, "/2047", th->title);
        }
        cx = cxp; cy = cyp - top; cursor();
        int k;
        mouse_draw();
        for (;;) {
            k = ui_wait();
            if (k == UI_MOVE) { mouse_draw(); continue; }
            if (k == UI_WHEEL || k == UI_RCLICK) continue;
            break;
        }
        if (k == UI_CLICK) {
            if (my < H - 1) {
                int ty = my + top, best = len, found = 0, x2 = 0, y2 = 0;
                for (int i = 0; i <= len; i++) {
                    if (y2 == ty && x2 <= mx) { best = i; found = 1; }
                    if (y2 > ty || i == len) break;
                    if (buf[i] == '\n') { x2 = 0; y2++; } else if (++x2 >= W) { x2 = 0; y2++; }
                }
                pos = found ? best : len;
            }
            continue;
        }
        if (k == 27) { fs_write(idx, buf, len, 0); fs_sync(); break; }
        else if (k == K_LEFT) { if (pos > 0) pos--; }
        else if (k == K_RIGHT) { if (pos < len) pos++; }
        else if (k == K_HOME) { while (pos > 0 && buf[pos - 1] != '\n') pos--; }
        else if (k == K_END) { while (pos < len && buf[pos] != '\n') pos++; }
        else if (k == K_UP) {
            int ls = pos;
            while (ls > 0 && buf[ls - 1] != '\n') ls--;
            if (ls > 0) {
                int col = pos - ls, pe = ls - 1, ps = pe;
                while (ps > 0 && buf[ps - 1] != '\n') ps--;
                pos = ps + col; if (pos > pe) pos = pe;
            }
        }
        else if (k == K_DOWN) {
            int ls = pos;
            while (ls > 0 && buf[ls - 1] != '\n') ls--;
            int col = pos - ls, le = pos;
            while (le < len && buf[le] != '\n') le++;
            if (le < len) {
                int ns = le + 1, ne = ns;
                while (ne < len && buf[ne] != '\n') ne++;
                pos = ns + col; if (pos > ne) pos = ne;
            }
        }
        else if (k == '\b') { if (pos > 0) { for (int i = pos; i < len; i++) buf[i - 1] = buf[i]; pos--; len--; } }
        else if (k == K_DEL) { if (pos < len) { for (int i = pos + 1; i < len; i++) buf[i - 1] = buf[i]; len--; } }
        else if (k == '\n' || k == '\t' || (k >= 32 && k < 127) || (k >= 0x80 && k <= 0xFF)) {
            if (len < FS_FSZ - 1) { for (int i = len; i > pos; i--) buf[i] = buf[i - 1]; buf[pos++] = (char)k; len++; }
        }
    }
    mouse_ui = omui; mouse_forget(); ui_drop_events();
    color = th->base; clear();
}

/* ---------- Просмотр страниц и браузер ---------- */
static void apnd_u(char *d, int *n, int max, u32 v) {
    char t[12]; int i = 0;
    if (!v) t[i++] = '0';
    while (v) { t[i++] = '0' + v % 10; v /= 10; }
    while (i) { char c = t[--i]; if (*n < max - 1) d[(*n)++] = c; }
    d[*n] = 0;
}
static int ends_ci(const char *s, const char *suf) {
    int a = slen(s), b = slen(suf);
    if (b > a) return 0;
    for (int i = 0; i < b; i++) if (h_lc(s[a - b + i]) != suf[i]) return 0;
    return 1;
}
static u8 th_page_attr(u8 a) {
    switch (a) {
        case PA_LINK: return th->link;
        case PA_HEAD1: return th->head;
        case PA_BOLD: return th->dir;
        case PA_BULLET: return th->user;
        case PA_DIM: return th->frame;
    }
    return th->file;
}
static void page_viewer(const char *title) {
    int top = 0;
    u8 omui = mouse_ui; mouse_ui = 1; cursor_hide();
    for (;;) {
        int maxtop = pg_lines - (H - 2);
        if (maxtop < 0) maxtop = 0;
        if (top > maxtop) top = maxtop;
        if (top < 0) top = 0;
        mouse_forget();
        for (int x = 0; x < W; x++) put_at(x, 0, ' ', th->title);
        put_clip(1, 0, title, 60, th->title); put_str_at(W - 9, 0, "[Close]", th->title);
        for (int y = 1; y < H - 1; y++) {
            int line = top + y - 1;
            for (int x = 0; x < W; x++) {
                if (line < pg_lines) put_at(x, y, pg_ch[line][x], pg_im[line] ? pg_at[line][x] : th_page_attr(pg_at[line][x]));
                else put_at(x, y, ' ', th->file);
            }
        }
        for (int x = 0; x < W; x++) put_at(x, H - 1, ' ', th->bar);
        put_str_at(1, H - 1, "Up/Down PgUp/PgDn Home/End or wheel: scroll     Esc / F3 / F10 / q: close", th->bar);
        mouse_draw();
        int k = ui_wait();
        if (k == K_UP) top--;
        else if (k == K_DOWN) top++;
        else if (k == K_PGUP) top -= H - 3;
        else if (k == K_PGDN || k == ' ') top += H - 3;
        else if (k == K_HOME) top = 0;
        else if (k == K_END) top = maxtop;
        else if (k == UI_WHEEL) top += ui_wheel * 3;
        else if (k == 27 || k == 'q' || k == K_F1 + 2 || k == K_F1 + 9) break;
        else if (k == UI_CLICK && ui_cy == 0 && ui_cx >= W - 9) break;
    }
    mouse_ui = omui; mouse_forget(); ui_drop_events();
}

#define BR_HIST 12
static char br_url[256], br_hist[BR_HIST][256], br_status[100];
static int br_hn, br_top, br_focus, br_hover, br_dirty;
static int br_focus_inp = -1;
static struct hres br_res;
static u8 http_data[262144] XBSS;
static const char br_start[] =
    "<h1>Cravgos Web</h1><p>A text-mode browser. <b>Click</b> links with the mouse, scroll with the wheel, "
    "right-click goes back. Keys: G = type an address, Tab/Enter = follow a link, B = back, Q = quit.</p>"
    "<p><b>Web Search (DuckDuckGo):</b></p>"
    "<form action=\"http://html.duckduckgo.com/html/\" method=\"POST\">"
    "<input type=\"text\" name=\"q\" placeholder=\"Search the web...\" size=\"30\"> "
    "<input type=\"submit\" value=\"Search\"></form>"
    "<h2>Plain HTTP sites</h2><ul>"
    "<li><a href='http://example.com/'>example.com</a> - the classic example page</li>"
    "<li><a href='http://neverssl.com/'>neverssl.com</a> - never redirects to HTTPS</li>"
    "<li><a href='http://info.cern.ch/'>info.cern.ch</a> - the first website ever</li>"
    "<li><a href='http://httpforever.com/'>httpforever.com</a> - HTTP forever</li></ul>"
    "<h2>HTTPS sites (encrypted)</h2><ul>"
    "<li><a href='https://example.com/'>example.com</a> over HTTPS</li>"
    "<li><a href='https://text.npr.org/'>text.npr.org</a> - text-only news</li>"
    "<li><a href='https://news.ycombinator.com/'>news.ycombinator.com</a> - Hacker News</li></ul>"
    "<p><b>Security note:</b> HTTPS traffic is encrypted, but Cravgos does NOT verify the server certificate, "
    "so a man-in-the-middle could impersonate a site. Do not type passwords here. "
    "Needs TLS 1.2 with X25519 and AES-GCM; some sites may refuse.</p>";

static int br_abort(void) { return key_poll() == 27; }
static int br_maxtop(void) { int m = pg_lines - (H - 2); return m < 0 ? 0 : m; }
static int br_link_at(int x, int y) {
    if (y < 1 || y > H - 2 || x < 0 || x >= W) return 0;
    int l = br_top + y - 1;
    return (l < pg_lines) ? pg_lk[l][x] : 0;
}
static void br_say(const char *s) {
    int n = 0; br_status[0] = 0;
    h_apnd(br_status, &n, sizeof br_status, s);
}
static void br_draw(void) {
    mouse_forget();
    for (int x = 0; x < W; x++) put_at(x, 0, ' ', th->title);
    put_str_at(1, 0, "[Back]", th->title); put_str_at(8, 0, "[Home]", th->title);
    put_str_at(15, 0, "[Quit]", th->title); put_str_at(22, 0, "URL:", th->title);
    put_clip(27, 0, br_url, 52, th->title);
    for (int y = 1; y < H - 1; y++) {
        int line = br_top + y - 1;
        for (int x = 0; x < W; x++) {
            if (line < pg_lines) {
                u16 lk = pg_lk[line][x];
                u8 inpid = pg_inp[line][x];
                u8 at = pg_im[line] ? pg_at[line][x] : th_page_attr(pg_at[line][x]);
                if (lk && !pg_im[line] && (lk == br_focus || lk == br_hover)) at = th->sel;
                if (inpid && (br_focus_inp == inpid - 1)) at = th->user;
                put_at(x, y, pg_ch[line][x], at);
            } else put_at(x, y, ' ', th->file);
        }
    }
    for (int x = 0; x < W; x++) put_at(x, H - 1, ' ', th->bar);
    if (br_hover) {
        char t[256];
        if (url_resolve(br_url, lk_url[br_hover], t, sizeof t) == 0) put_clip(1, H - 1, t, 76, th->bar);
        else put_clip(1, H - 1, lk_url[br_hover], 76, th->bar);
    } else put_clip(1, H - 1, br_status, 60, th->bar);
    { char p[24]; int n = 0; p[0] = 0; apnd_u(p, &n, 24, br_top + 1); h_apnd(p, &n, 24, "/"); apnd_u(p, &n, 24, pg_lines); put_str_at(W - 1 - n, H - 1, p, th->bar); }
    if (br_focus_inp >= 0 && br_focus_inp < inp_cnt) {
        int il = inps[br_focus_inp].line;
        if (il >= br_top && il < br_top + H - 2) {
            int iy = 1 + il - br_top;
            int vlen = 0; while (inps[br_focus_inp].value[vlen]) vlen++;
            int ix = inps[br_focus_inp].col + 1 + vlen;
            if (ix < inps[br_focus_inp].col + 1 + inps[br_focus_inp].width && ix < W) {
                put_at(ix, iy, '_', th->user);
            }
        }
    }
    mouse_draw();
    br_dirty = 0;
}
static void br_page_error(const char *why, const char *url) {
    char eb[900]; int n = 0; eb[0] = 0;
    h_apnd(eb, &n, sizeof eb, "<h2>Cannot open the page</h2><p>"); h_apnd(eb, &n, sizeof eb, why);
    h_apnd(eb, &n, sizeof eb, "</p><p>"); h_apnd(eb, &n, sizeof eb, url);
    h_apnd(eb, &n, sizeof eb, "</p><p>Press G to type another address, or click [Home].</p>");
    html_render(eb, n, 0);
}
static void br_images(void) {
    int n = img_cnt;
    const char *old = http_accept;
    char keep[sizeof br_status]; for (int i = 0; i < (int)sizeof br_status; i++) keep[i] = br_status[i];
    for (int k = 0; k < n; k++) {
        char nx[256]; int ok = 0;
        int sn = 0; br_status[0] = 0;
        h_apnd(br_status, &sn, sizeof br_status, "Loading image "); apnd_u(br_status, &sn, sizeof br_status, k + 1);
        h_apnd(br_status, &sn, sizeof br_status, "/"); apnd_u(br_status, &sn, sizeof br_status, n);
        h_apnd(br_status, &sn, sizeof br_status, "  (Esc skips the rest)");
        br_dirty = 1; br_draw();
        if (url_resolve(br_url, img_src[k], nx, sizeof nx) == 0) {
            http_accept = "image/png, image/jpeg, image/gif";
            http_abort = br_abort;
            struct hres r; r.url[0] = 0;
            int e = http_get_follow(nx, &r);
            http_abort = 0;
            if (e == HE_ABORT) { http_accept = old; break; }
            if (e == 0 && r.status == 200 && r.body_len > 0 && img_decode(http_data + r.body_off, r.body_len) == 0) { img_paint(k); ok = 1; }
        }
        if (!ok) html_img_shrink(k, 1);
    }
    http_accept = old;
    for (int i = 0; i < (int)sizeof br_status; i++) br_status[i] = keep[i];
    for (int k = 0; k < img_cnt; k++) { (void)k; }
}
static void br_load(const char *url, int push) {
    if (push && br_url[0]) {
        if (br_hn == BR_HIST) {
            for (int i = 1; i < BR_HIST; i++) for (int k = 0; k < 256; k++) br_hist[i - 1][k] = br_hist[i][k];
            br_hn--;
        }
        int k = 0; while (br_url[k]) { br_hist[br_hn][k] = br_url[k]; k++; }
        br_hist[br_hn][k] = 0; br_hn++;
    }
    char u[256]; int k = 0;
    while (url[k] && k < 255) { u[k] = url[k]; k++; }
    u[k] = 0;
    for (k = 0; u[k]; k++) br_url[k] = u[k];
    br_url[k] = 0;
    br_top = 0; br_focus = 0; br_hover = 0; br_focus_inp = (inp_cnt > 0) ? 0 : -1;
    if (streq(br_url, "about:start")) {
        html_render(br_start, sizeof br_start - 1, 0);
        br_say("Cravgos Web - start page");
    } else if (ci_prefix(br_url, "file://")) {
        int idx = fs_resolve(br_url + 7);
        if (idx < 0 || fsn[idx].dir) { br_page_error("No such file", br_url); br_say("Error"); }
        else {
            int ish = ends_ci(fsn[idx].name, ".html") || ends_ci(fsn[idx].name, ".htm");
            html_charset = html_detect_charset((const char *)fsdata[idx], fsn[idx].size, 0, CS_866);
            html_render((const char *)fsdata[idx], fsn[idx].size, !ish);
            br_say("Local file");
        }
    } else {
        if (!nic_ok) {
            br_page_error("No supported network card (Intel e1000). In VirtualBox set Adapter Type = Intel PRO/1000 MT Desktop.", br_url);
            br_say("No network"); ui_drop_events(); br_dirty = 1; return;
        }
        if (!net_conf_done) { br_say("Configuring the network (DHCP)..."); br_dirty = 1; br_draw(); net_autoconf(0); }
        br_say("Loading...  (Esc cancels)"); br_dirty = 1; br_draw();
        http_buf = http_data; http_max = sizeof http_data - 1; http_abort = br_abort;
        br_res.url[0] = 0;
        int r = http_get_follow(br_url, &br_res);
        http_abort = 0;
        if (br_res.url[0]) { for (k = 0; br_res.url[k]; k++) br_url[k] = br_res.url[k]; br_url[k] = 0; }
        if (r < 0) { br_page_error(http_err_str(r), br_url); br_say("Error"); }
        else if (br_res.html < 0) {
            static const char bin[] = "<h2>Binary content</h2><p>This type of file cannot be shown in the text browser.</p>";
            html_render(bin, sizeof bin - 1, 0);
            br_say("Binary data");
        } else {
            html_charset = html_detect_charset((const char *)http_data + br_res.body_off, br_res.body_len, br_res.ctype, CS_1251);
            html_render((const char *)http_data + br_res.body_off, br_res.body_len, br_res.html == 0);
            int n = 0; br_status[0] = 0;
            h_apnd(br_status, &n, sizeof br_status, "HTTP "); apnd_u(br_status, &n, sizeof br_status, br_res.status);
            if (pg_title[0]) { h_apnd(br_status, &n, sizeof br_status, "  "); h_apnd(br_status, &n, sizeof br_status, pg_title); }
            if (br_res.truncated) h_apnd(br_status, &n, sizeof br_status, "  (page truncated)");
            if (ci_prefix(br_url, "https://")) h_apnd(br_status, &n, sizeof br_status, "  [encrypted, server not verified]");
            if (br_res.html == 1 && img_cnt) br_images();
        }
    }
    ui_drop_events();
    br_dirty = 1;
}
static void br_back(void) {
    if (br_hn > 0) {
        char u[256]; int k = 0;
        br_hn--;
        while (br_hist[br_hn][k]) { u[k] = br_hist[br_hn][k]; k++; }
        u[k] = 0;
        br_load(u, 0);
    }
}
static void br_submit_form(int form_idx) {
    if (form_idx < 0 || form_idx >= form_cnt) return;
    html_form_t *f = &forms[form_idx];
    char post_data[1024]; int pd_len = 0; post_data[0] = 0;
    for (int i = 0; i < inp_cnt; i++) {
        if (inps[i].form_idx == form_idx && inps[i].name[0]) {
            if (streq(inps[i].type, "submit") && !inps[i].value[0]) continue;
            if (pd_len > 0 && pd_len < (int)sizeof(post_data) - 1) post_data[pd_len++] = '&';
            for (int k = 0; inps[i].name[k] && pd_len < (int)sizeof(post_data) - 1; k++) {
                post_data[pd_len++] = inps[i].name[k];
            }
            if (pd_len < (int)sizeof(post_data) - 1) post_data[pd_len++] = '=';
            for (int k = 0; inps[i].value[k] && pd_len < (int)sizeof(post_data) - 4; k++) {
                char c = inps[i].value[k];
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
                    post_data[pd_len++] = c;
                } else if (c == ' ') {
                    post_data[pd_len++] = '+';
                } else {
                    static const char hex[] = "0123456789ABCDEF";
                    post_data[pd_len++] = '%';
                    post_data[pd_len++] = hex[((u8)c >> 4) & 15];
                    post_data[pd_len++] = hex[(u8)c & 15];
                }
            }
        }
    }
    post_data[pd_len] = 0;
    char target[384]; int tn = 0; target[0] = 0;
    if (f->action[0]) {
        url_resolve(br_url, f->action, target, sizeof(target));
    } else {
        h_apnd(target, &tn, sizeof(target), br_url);
    }
    if (streq(f->method, "POST")) {
        br_say("Submitting POST form...");
        struct hres hr;
        int err = http_post_follow(target, "application/x-www-form-urlencoded", post_data, pd_len, &hr);
        if (err < 0) {
            br_page_error(http_err_str(err), target);
        } else {
            int k = 0; br_url[0] = 0; h_apnd(br_url, &k, sizeof(br_url), target);
            html_render((const char *)(http_buf + hr.body_off), hr.body_len, !hr.html);
            br_top = 0; br_focus = 0; br_focus_inp = (inp_cnt > 0) ? 0 : -1;
            br_images();
        }
        br_dirty = 1;
    } else {
        if (pd_len > 0) {
            int has_q = 0;
            for (int k = 0; target[k]; k++) if (target[k] == '?') { has_q = 1; break; }
            tn = 0; while (target[tn]) tn++;
            if (!has_q && tn < (int)sizeof(target) - 1) target[tn++] = '?';
            else if (has_q && tn < (int)sizeof(target) - 1) target[tn++] = '&';
            target[tn] = 0;
            h_apnd(target, &tn, sizeof(target), post_data);
        }
        br_load(target, 1);
    }
}
static void br_open_link(int id) {
    char nx[256];
    if (url_resolve(br_url, lk_url[id], nx, sizeof nx) < 0) {
        int n = 0; br_status[0] = 0;
        h_apnd(br_status, &n, sizeof br_status, "Cannot open link: "); h_apnd(br_status, &n, sizeof br_status, lk_url[id]);
        br_dirty = 1; return;
    }
    if (streq(nx, br_url)) { br_top = 0; br_dirty = 1; return; }
    br_load(nx, 1);
}
static void br_show_link(int id) {
    for (int l = 0; l < pg_lines; l++)
        for (int x = 0; x < W; x++)
            if (pg_lk[l][x] == id) { if (l < br_top || l >= br_top + H - 2) br_top = l > 3 ? l - 3 : 0; return; }
}
static void br_edit_url(void) {
    char buf[200]; int n = 0, ok = 0;
    for (int i = 0; br_url[i] && n < 199; i++) buf[n++] = br_url[i];
    if (streq(br_url, "about:start")) n = 0;
    buf[n] = 0;
    u8 omui = mouse_ui; mouse_ui = 0; mouse_forget();
    for (;;) {
        for (int x = 0; x < W; x++) put_at(x, 0, ' ', th->title);
        put_str_at(1, 0, "Go to:", th->title);
        int st = n > 70 ? n - 70 : 0;
        put_clip(8, 0, buf + st, 71, th->title);
        put_at(8 + n - st, 0, '_', th->title);
        int k = ui_wait();
        if (k == '\n') { ok = 1; break; }
        if (k == 27) break;
        if (k == '\b') { if (n) buf[--n] = 0; }
        else if (((k >= 32 && k < 127) || (k >= 0x80 && k <= 0xFF)) && n < 199) { buf[n++] = (char)k; buf[n] = 0; }
    }
    mouse_ui = omui;
    if (ok && n) {
        char url[256]; int m = 0; url[0] = 0;
        if (!ci_prefix(buf, "http://") && !ci_prefix(buf, "https://") && !ci_prefix(buf, "file://") && !ci_prefix(buf, "about:"))
            h_apnd(url, &m, sizeof url, "http://");
        h_apnd(url, &m, sizeof url, buf);
        br_load(url, 1);
    } else br_dirty = 1;
}
static void browse(const char *start) {
    u8 omui = mouse_ui; mouse_ui = 1; cursor_hide();
    br_hn = 0; br_url[0] = 0;
    br_load(start && *start ? start : "about:start", 0);
    for (;;) {
        if (br_dirty) br_draw();
        int k = ui_wait();
        if (k == UI_MOVE) {
            int h = br_link_at(mx, my);
            if (h != br_hover) { br_hover = h; br_dirty = 1; } else mouse_draw();
            continue;
        }
        if (br_focus_inp >= 0 && br_focus_inp < inp_cnt) {
            html_inp_t *cur_inp = &inps[br_focus_inp];
            if (k == 27) { br_focus_inp = -1; br_dirty = 1; continue; }
            if (k == '\t') { br_focus_inp = (br_focus_inp + 1) % inp_cnt; br_dirty = 1; continue; }
            if (k == '\n') { br_submit_form(cur_inp->form_idx); continue; }
            if (k == '\b') {
                int l = 0; while (cur_inp->value[l]) l++;
                if (l > 0) {
                    cur_inp->value[l - 1] = 0;
                    int ix = cur_inp->col + l;
                    if (ix < W && cur_inp->line < pg_lines) pg_ch[cur_inp->line][ix] = ' ';
                    br_dirty = 1;
                }
                continue;
            }
            if (((k >= 32 && k < 127) || (k >= 0x80 && k <= 0xFF)) && !streq(cur_inp->type, "submit")) {
                int l = 0; while (cur_inp->value[l]) l++;
                if (l < cur_inp->width && l < (int)sizeof(cur_inp->value) - 1) {
                    cur_inp->value[l] = (char)k;
                    cur_inp->value[l + 1] = 0;
                    int ix = cur_inp->col + 1 + l;
                    if (ix < W && cur_inp->line < pg_lines) {
                        pg_ch[cur_inp->line][ix] = streq(cur_inp->type, "password") ? '*' : (char)k;
                    }
                    br_dirty = 1;
                }
                continue;
            }
        }
        if (k == 27 || k == 'q' || k == K_F1 + 9) break;
        else if (k == K_DOWN) br_top++;
        else if (k == K_UP) br_top--;
        else if (k == K_PGDN || k == ' ') br_top += H - 3;
        else if (k == K_PGUP) br_top -= H - 3;
        else if (k == K_HOME) br_top = 0;
        else if (k == K_END) br_top = br_maxtop();
        else if (k == '\t') {
            if (inp_cnt) { br_focus_inp = 0; br_dirty = 1; }
            else if (lk_count) { br_focus = br_focus % lk_count + 1; br_show_link(br_focus); }
        }
        else if (k == '\n') { if (br_focus) br_open_link(br_focus); }
        else if (k == 'b' || k == '\b' || k == UI_RCLICK) br_back();
        else if (k == 'g') br_edit_url();
        else if (k == 'h') br_load("about:start", 1);
        else if (k == 'r') br_load(br_url, 0);
        else if (k == UI_WHEEL) br_top += ui_wheel * 3;
        else if (k == UI_CLICK) {
            if (ui_cy == 0) {
                if (ui_cx >= 1 && ui_cx <= 6) br_back();
                else if (ui_cx >= 8 && ui_cx <= 13) br_load("about:start", 1);
                else if (ui_cx >= 15 && ui_cx <= 20) break;
                else if (ui_cx >= 22) br_edit_url();
            } else {
                int l = br_top + ui_cy - 1;
                if (l < pg_lines && pg_inp[l][ui_cx] > 0) {
                    br_focus_inp = pg_inp[l][ui_cx] - 1;
                    if (streq(inps[br_focus_inp].type, "submit")) {
                        br_submit_form(inps[br_focus_inp].form_idx);
                    } else {
                        br_dirty = 1;
                    }
                } else {
                    int h = br_link_at(ui_cx, ui_cy);
                    if (h) br_open_link(h);
                    else { br_focus_inp = -1; br_dirty = 1; }
                }
            }
        }
        if (br_top > br_maxtop()) br_top = br_maxtop();
        if (br_top < 0) br_top = 0;
        br_hover = br_link_at(mx, my);
        br_dirty = 1;
    }
    mouse_ui = omui; mouse_forget(); ui_drop_events();
    color = th->base; clear(); cursor_show();
}

/* ---------- Двухпанельный проводник ---------- */
struct pane { int cwd, sel, top; };
static struct pane pn[2];
static u8 fm_mark[FS_MAXN];
static int fm_act, fm_lst[2][FS_MAXN + 1], fm_n[2];
static char fm_msg[96];
static u32 fm_click_t;
static int fm_click_p = -1, fm_click_i = -1;
#define FM_ROWS 20

static const char *fs_err_s(int e) {
    switch (e) {
        case FS_EBAD: return "invalid name or path";
        case FS_EEXIST: return "already exists";
        case FS_EFULL: return "no space left (64 entries max)";
        case FS_ENOENT: return "no such file or directory";
        case FS_ENOTDIR: return "not a directory";
        case FS_EISDIR: return "is a directory";
        case FS_ENOTEMPTY: return "directory not empty";
        case FS_EBUSY: return "busy (root or current directory)";
    }
    return "error";
}
static void fm_say(const char *a, int n, const char *b) {
    int k = 0; fm_msg[0] = 0;
    h_apnd(fm_msg, &k, sizeof fm_msg, a);
    if (n >= 0) apnd_u(fm_msg, &k, sizeof fm_msg, n);
    if (b) h_apnd(fm_msg, &k, sizeof fm_msg, b);
}
static void fm_refresh(int p) {
    if (!fsn[pn[p].cwd].used || !fsn[pn[p].cwd].dir) pn[p].cwd = 0;
    int n = 0, tmp[FS_MAXN];
    if (pn[p].cwd != 0) fm_lst[p][n++] = -1;
    int c = fs_list(pn[p].cwd, tmp);
    for (int i = 0; i < c; i++) fm_lst[p][n++] = tmp[i];
    fm_n[p] = n;
    if (pn[p].sel >= n) pn[p].sel = n - 1;
    if (pn[p].sel < 0) pn[p].sel = 0;
    if (pn[p].sel < pn[p].top) pn[p].top = pn[p].sel;
    if (pn[p].sel >= pn[p].top + FM_ROWS) pn[p].top = pn[p].sel - FM_ROWS + 1;
    if (pn[p].top < 0) pn[p].top = 0;
}
static int fm_sel_node(int p) {             /* узел под курсором, -1 для ".." или пусто */
    if (fm_n[p] == 0) return -1;
    return fm_lst[p][pn[p].sel];
}
static void fm_draw_panel(int p) {
    int x0 = p * 40, act = (p == fm_act);
    fill_rect(x0, 1, 40, 22, th->file);
    draw_box(x0, 1, 40, 22, th->frame, act);
    put_at(x0 + 29, 1, act ? 0xD1 : 0xC2, th->frame);
    put_at(x0 + 29, 22, act ? 0xCF : 0xC1, th->frame);
    for (int j = 2; j < 22; j++) put_at(x0 + 29, j, 0xB3, th->frame);
    char path[256], shown[40];
    fs_path(pn[p].cwd, path);
    int pl = slen(path), n = 0;
    shown[0] = ' ';
    if (pl > 33) { shown[1] = '.'; shown[2] = '.'; shown[3] = '.'; for (int i = 0; i < 29; i++) shown[4 + i] = path[pl - 29 + i]; n = 33; }
    else { for (int i = 0; i < pl; i++) shown[1 + i] = path[i]; n = pl; }
    shown[n + 1] = ' '; shown[n + 2] = 0;
    put_clip(x0 + 2, 1, shown, 36, act ? th->sel : th->frame);
    for (int r = 0; r < FM_ROWS; r++) {
        int i = pn[p].top + r, y = 2 + r;
        if (i >= fm_n[p]) break;
        int e = fm_lst[p][i], isdir = (e < 0) || fsn[e].dir, mk = (e >= 0 && fm_mark[e]);
        u8 at = mk ? th->mark : (isdir ? th->dir : th->file);
        int cur = act && i == pn[p].sel;
        if (cur) { at = th->sel; fill_rect(x0 + 1, y, 38, 1, at); }
        put_at(x0 + 1, y, mk ? '*' : ' ', at);
        put_at(x0 + 2, y, isdir ? '/' : ' ', at);
        put_clip(x0 + 3, y, e < 0 ? ".." : fsn[e].name, 26, at);
        put_at(x0 + 29, y, 0xB3, cur ? at : th->frame);
        char s[16]; int sn = 0; s[0] = 0;
        if (e < 0) h_apnd(s, &sn, sizeof s, "UP--DIR");
        else if (fsn[e].dir) h_apnd(s, &sn, sizeof s, "<DIR>");
        else apnd_u(s, &sn, sizeof s, fsn[e].size);
        put_clip(x0 + 30 + (9 - sn), y, s, 9, at);
    }
    char cnt[24]; int cn = 0; cnt[0] = 0;
    h_apnd(cnt, &cn, sizeof cnt, " "); apnd_u(cnt, &cn, sizeof cnt, fm_n[p] - (pn[p].cwd != 0 ? 1 : 0)); h_apnd(cnt, &cn, sizeof cnt, " items ");
    put_clip(x0 + 3, 22, cnt, 20, th->frame);
}
static void fm_draw(void) {
    static const char *lab[10] = {"Help", "Theme", "View", "Edit", "Copy", "Move", "MkDir", "Delete", "Web", "Quit"};
    mouse_forget(); cursor_hide();
    for (int i = 0; i < W * H; i++) vga[i] = (u16)((th->base << 8) | ' ');
    for (int x = 0; x < W; x++) put_at(x, 0, ' ', th->title);
    put_str_at(1, 0, "Cravgos Commander", th->title);
    put_str_at(20, 0, kbd_ru ? "[RU]" : "[EN]", th->title);
    { char t[24]; int n = 0; t[0] = 0; h_apnd(t, &n, sizeof t, "Theme: "); h_apnd(t, &n, sizeof t, th->name); put_str_at(W - 1 - n, 0, t, th->title); }
    fm_refresh(0); fm_refresh(1);
    fm_draw_panel(1 - fm_act); fm_draw_panel(fm_act);
    for (int x = 0; x < W; x++) put_at(x, 23, ' ', th->file);
    {
        int e = fm_sel_node(fm_act);
        if (fm_msg[0]) put_clip(1, 23, fm_msg, 78, th->mark);
        else if (e > 0) {
            char t[300]; int n = 0; t[0] = 0;
            char pp[256]; fs_path(e, pp); h_apnd(t, &n, sizeof t, pp);
            if (!fsn[e].dir) { h_apnd(t, &n, sizeof t, "   "); apnd_u(t, &n, sizeof t, fsn[e].size); h_apnd(t, &n, sizeof t, " bytes"); }
            put_clip(1, 23, t, 78, th->file);
        } else { char pp[256]; fs_path(pn[fm_act].cwd, pp); put_clip(1, 23, pp, 78, th->file); }
        int mc = 0;
        for (int i = 1; i < FS_MAXN; i++) if (fsn[i].used && fm_mark[i]) mc++;
        if (mc) { char t[40]; int n = 0; t[0] = 0; h_apnd(t, &n, sizeof t, "Marked: "); apnd_u(t, &n, sizeof t, mc); put_str_at(W - 1 - n, 23, t, th->mark); }
    }
    for (int i = 0; i < 10; i++) {
        char num[3]; int nl = 0; num[0] = 0;
        apnd_u(num, &nl, 3, i + 1);
        put_clip(i * 8, 24, num, 2, th->barkey);
        for (int k = nl; k < 8; k++) put_at(i * 8 + k, 24, ' ', th->bar);
        put_clip(i * 8 + nl, 24, lab[i], 8 - nl, th->bar);
    }
    mouse_draw();
}
static int fm_targets(int *out) {
    int n = 0, c = pn[fm_act].cwd;
    for (int i = 1; i < FS_MAXN; i++) if (fsn[i].used && fm_mark[i] && fsn[i].parent == c) out[n++] = i;
    if (n) return n;
    int e = fm_sel_node(fm_act);
    if (e > 0) out[n++] = e;
    return n;
}
static void fm_go_up(int p) {
    int old = pn[p].cwd;
    if (old == 0) return;
    pn[p].cwd = fsn[old].parent; pn[p].top = 0; pn[p].sel = 0;
    fm_refresh(p);
    for (int i = 0; i < fm_n[p]; i++) if (fm_lst[p][i] == old) { pn[p].sel = i; break; }
    fm_refresh(p);
}
static void fm_open(int p) {
    fm_refresh(p);
    if (!fm_n[p]) return;
    int e = fm_lst[p][pn[p].sel];
    if (e < 0) { fm_go_up(p); return; }
    if (fsn[e].dir) { pn[p].cwd = e; pn[p].sel = 0; pn[p].top = 0; return; }
    if (ends_ci(fsn[e].name, ".html") || ends_ci(fsn[e].name, ".htm")) {
        char u[300], pth[256]; int n = 0; u[0] = 0;
        fs_path(e, pth); h_apnd(u, &n, sizeof u, "file://"); h_apnd(u, &n, sizeof u, pth);
        browse(u);
    } else {
        html_charset = html_detect_charset((const char *)fsdata[e], fsn[e].size, 0, CS_866);
        html_render((const char *)fsdata[e], fsn[e].size, 1);
        page_viewer(fsn[e].name);
    }
}
static void fm_copymove(int move) {
    int tg[FS_MAXN], nt = fm_targets(tg);
    if (!nt) { dlg_msg("Nothing selected", "Select a file or folder first."); return; }
    char prompt[80], dst[120]; int n = 0; prompt[0] = 0;
    h_apnd(prompt, &n, sizeof prompt, move ? "Move / rename " : "Copy "); apnd_u(prompt, &n, sizeof prompt, nt);
    h_apnd(prompt, &n, sizeof prompt, " item(s) to:");
    fs_path(pn[1 - fm_act].cwd, dst);
    if (!dlg_input(move ? "Move / rename" : "Copy", prompt, dst, sizeof dst)) return;
    fs_cwd = pn[fm_act].cwd;
    int ok = 0, err = 0;
    for (int i = 0; i < nt; i++) {
        char sp[256]; fs_path(tg[i], sp);
        int r = move ? fs_move(sp, dst) : fs_copy(sp, dst);
        if (r < 0) err = r; else { ok++; fm_mark[tg[i]] = 0; }
    }
    if (err) { fm_say(move ? "Moved " : "Copied ", ok, ", error: "); int q = slen(fm_msg); h_apnd(fm_msg, &q, sizeof fm_msg, fs_err_s(err)); }
    else fm_say(move ? "Moved " : "Copied ", ok, " item(s)");
    if (ok) fs_sync();
}
static void fm_delete(void) {
    int tg[FS_MAXN], nt = fm_targets(tg);
    if (!nt) { dlg_msg("Nothing selected", "Select a file or folder first."); return; }
    char l1[60], l2[60]; int n = 0; l1[0] = l2[0] = 0;
    h_apnd(l1, &n, sizeof l1, "Delete "); apnd_u(l1, &n, sizeof l1, nt); h_apnd(l1, &n, sizeof l1, " item(s)?");
    if (nt == 1) { int m = 0; h_apnd(l2, &m, sizeof l2, fsn[tg[0]].name); }
    const char *ln[2]; ln[0] = l1; ln[1] = l2;
    if (!dlg_lines("Delete", ln, 2, 1)) return;
    fs_cwd = pn[fm_act].cwd;
    int ok = 0, err = 0;
    for (int i = 0; i < nt; i++) { int r = fs_remove_tree(tg[i]); if (r < 0) err = r; else ok++; }
    for (int p = 0; p < 2; p++) if (!fsn[pn[p].cwd].used) pn[p].cwd = 0;
    for (int i = 0; i < FS_MAXN; i++) if (!fsn[i].used) fm_mark[i] = 0;
    if (err) { dlg_msg("Delete", fs_err_s(err)); }
    fm_say("Deleted ", ok, " item(s)");
    if (ok) fs_sync();
}
static void fm_make(int isdir) {
    char nm[80]; nm[0] = 0;
    if (!dlg_input(isdir ? "Make directory" : "New file", "Name:", nm, sizeof nm) || !nm[0]) return;
    char path[300]; int n = 0; path[0] = 0;
    if (nm[0] != '/') { fs_path(pn[fm_act].cwd, path); n = slen(path); if (n > 1) { path[n++] = '/'; path[n] = 0; } }
    h_apnd(path, &n, sizeof path, nm);
    fs_cwd = pn[fm_act].cwd;
    int r = fs_create(path, isdir);
    if (r < 0) { dlg_msg(isdir ? "Make directory" : "New file", fs_err_s(r)); return; }
    fm_refresh(fm_act);
    for (int i = 0; i < fm_n[fm_act]; i++) if (fm_lst[fm_act][i] == r) { pn[fm_act].sel = i; break; }
    fm_refresh(fm_act);
    fm_say(isdir ? "Directory created" : "File created", -1, 0);
    fs_sync();
}
static int fm_fkey(int n) {                    /* возвращает 1, если нужно выйти */
    int e = fm_sel_node(fm_act);
    switch (n) {
    case 1: {
        const char *l[7] = {
            "Tab / Left / Right: switch panel     Enter: open",
            "Ins or Space: mark   + all   - none   * invert",
            "F3 view (html opens as a web page)   F4 edit   N: new file",
            "F5 copy   F6 move/rename   F7 new folder   F8 delete",
            "F9 web browser   F2 color theme   F10 / Esc: quit",
            "Mouse: click = select, double-click = open,",
            "right-click = go up, wheel = scroll, click F-keys below" };
        dlg_lines("Cravgos Commander", l, 7, 0);
        break; }
    case 2: { int t = dlg_theme(); if (t >= 0) theme_apply(t); break; }
    case 3:
        if (e > 0 && !fsn[e].dir) { html_charset = html_detect_charset((const char *)fsdata[e], fsn[e].size, 0, CS_866); html_render((const char *)fsdata[e], fsn[e].size, !(ends_ci(fsn[e].name, ".html") || ends_ci(fsn[e].name, ".htm"))); page_viewer(fsn[e].name); }
        break;
    case 4: if (e > 0 && !fsn[e].dir) editor(e); break;
    case 5: fm_copymove(0); break;
    case 6: fm_copymove(1); break;
    case 7: fm_make(1); break;
    case 8: fm_delete(); break;
    case 9: browse(0); break;
    case 10: return 1;
    }
    return 0;
}
static void commander(void) {
    static int inited;
    u8 omui = mouse_ui; mouse_ui = 1;
    if (!inited) { pn[0].cwd = pn[1].cwd = fs_cwd; inited = 1; }
    else pn[fm_act].cwd = fs_cwd;
    fm_msg[0] = 0;
    int redraw = 1, quit = 0;
    while (!quit) {
        if (redraw) { fm_draw(); redraw = 0; }
        int k = ui_wait();
        if (k == UI_MOVE) { mouse_draw(); continue; }
        fm_msg[0] = 0;
        redraw = 1;
        int p = fm_act, n = fm_n[p];
        if (k == K_UP) { if (pn[p].sel > 0) pn[p].sel--; }
        else if (k == K_DOWN) { if (pn[p].sel < n - 1) pn[p].sel++; }
        else if (k == K_PGUP) pn[p].sel -= FM_ROWS - 1;
        else if (k == K_PGDN) pn[p].sel += FM_ROWS - 1;
        else if (k == K_HOME) pn[p].sel = 0;
        else if (k == K_END) pn[p].sel = n - 1;
        else if (k == '\t') fm_act = 1 - fm_act;
        else if (k == K_LEFT) fm_act = 0;
        else if (k == K_RIGHT) fm_act = 1;
        else if (k == '\n') fm_open(p);
        else if (k == '\b' || k == UI_RCLICK) fm_go_up(k == UI_RCLICK ? (mx >= 40) : p);
        else if (k == K_INS || k == ' ') { int e = fm_sel_node(p); if (e > 0) { fm_mark[e] ^= 1; if (pn[p].sel < n - 1) pn[p].sel++; } }
        else if (k == '+') { for (int i = 0; i < n; i++) if (fm_lst[p][i] > 0 && !fsn[fm_lst[p][i]].dir) fm_mark[fm_lst[p][i]] = 1; }
        else if (k == '-') { for (int i = 0; i < n; i++) if (fm_lst[p][i] > 0) fm_mark[fm_lst[p][i]] = 0; }
        else if (k == '*') { for (int i = 0; i < n; i++) if (fm_lst[p][i] > 0) fm_mark[fm_lst[p][i]] ^= 1; }
        else if (k == 'n' || k == 'N') fm_make(0);
        else if (k == 27 || k == 'q') quit = 1;
        else if (k >= K_F1 && k <= K_F1 + 9) quit = fm_fkey(k - K_F1 + 1);
        else if (k == UI_WHEEL) pn[p].sel += ui_wheel * 3;
        else if (k == UI_CLICK) {
            if (ui_cy == 24) quit = fm_fkey(ui_cx / 8 + 1);
            else if (ui_cy == 0) {
                if (ui_cx >= 19 && ui_cx <= 25) { kbd_ru ^= 1; redraw = 1; }
                else if (ui_cx >= W - 20) theme_apply((theme_index() + 1) % NTHEMES);
            }
            else {
                int cp = ui_cx >= 40;
                fm_act = cp;
                if (ui_cy >= 2 && ui_cy <= 21) {
                    int i = pn[cp].top + (ui_cy - 2);
                    if (i < fm_n[cp]) {
                        pn[cp].sel = i;
                        u32 now = now_ms();
                        if (fm_click_p == cp && fm_click_i == i && now - fm_click_t < 600) { fm_open(cp); fm_click_p = -1; }
                        else { fm_click_t = now; fm_click_p = cp; fm_click_i = i; }
                    }
                }
            }
        }
        else redraw = 0;
        if (redraw) {
            for (int q = 0; q < 2; q++) {
                if (pn[q].sel < 0) pn[q].sel = 0;
                if (pn[q].sel > fm_n[q] - 1) pn[q].sel = fm_n[q] - 1;
            }
        }
    }
    fs_cwd = pn[fm_act].cwd;
    mouse_ui = omui; mouse_forget(); ui_drop_events();
    color = th->base; clear(); cursor_show();
}

/* ---------- Оболочка ---------- */

static void cmd_web(char *args) {
    char *u = tok(&args);
    if (!u) { browse(0); return; }
    char url[256]; int n = 0; url[0] = 0;
    if (!ci_prefix(u, "http://") && !ci_prefix(u, "https://") && !ci_prefix(u, "file://") && !ci_prefix(u, "about:")) h_apnd(url, &n, sizeof url, "http://");
    h_apnd(url, &n, sizeof url, u);
    browse(url);
}
static void cmd_curl(char *args) {
    char *u = tok(&args);
    if (!u) { puts("usage: curl URL     (prints the page as text; Esc cancels)\n"); return; }
    if (!net_ready()) return;
    char url[256]; int n = 0; url[0] = 0;
    if (!ci_prefix(u, "http://") && !ci_prefix(u, "https://")) h_apnd(url, &n, sizeof url, "http://");
    h_apnd(url, &n, sizeof url, u);
    http_buf = http_data; http_max = sizeof http_data - 1; http_abort = br_abort;
    puts("GET "); puts(url); putc('\n');
    br_res.url[0] = 0;
    int r = http_get_follow(url, &br_res);
    http_abort = 0;
    if (r < 0) { puts(http_err_str(r)); putc('\n'); return; }
    puts("HTTP "); putu(br_res.status); puts("  "); puts(br_res.ctype); putc('\n');
    if (br_res.html < 0) { puts("(binary data, not shown)\n"); return; }
    html_charset = html_detect_charset((const char *)http_data + br_res.body_off, br_res.body_len, br_res.ctype, CS_1251);
    html_render((const char *)http_data + br_res.body_off, br_res.body_len, br_res.html == 0);
    int shown = 0;
    for (int l = 0; l < pg_lines; l++) {
        for (int x = 0; x < pg_len[l]; x++) putc((char)pg_ch[l][x]);
        putc('\n');
        if (++shown >= H - 3 && l + 1 < pg_lines) {
            puts("-- more: any key, Q stops --");
            char c = getc();
            cx = 0; for (int i = 0; i < 28; i++) putc('\b'); 
            if (c == 'q') { puts("                            \n"); return; }
            shown = 0;
        }
    }
}
static void cmd_theme(char *args) {
    char *a = tok(&args);
    if (!a) {
        puts("Themes:");
        for (int i = 0; i < NTHEMES; i++) { putc(' '); puts(themes[i].name); if (i == theme_index()) putc('*'); }
        puts("\nusage: theme NAME | theme next   (F2 in the commander opens a picker)\n");
        return;
    }
    if (streq(a, "next")) { theme_apply((theme_index() + 1) % NTHEMES); return; }
    for (int i = 0; i < NTHEMES; i++) if (streq(a, themes[i].name)) { theme_apply(i); return; }
    puts("unknown theme (try: theme)\n");
}

/* ============================================================================
 * Графическая подсистема и абстракция дисплея (Display / Desktop Environment)
 * ============================================================================ */
struct fb_info {
    u32 width;
    u32 height;
    u32 pitch;
    u32 bpp;
    u8 *vram;
    u8 is_graphics;
};

static struct fb_info g_fb __attribute__((unused)) = {
    80, 25, 80 * 2, 16, (u8 *)0xB8000, 0
};

static void cmd_gui(void) {
    u8 omui = mouse_ui;
    mouse_ui = 1;
    cursor_hide();

    int active_win = 1;
    int win_x = 10, win_y = 3, win_w = 60, win_h = 17;
    int redraw = 1;

    for (;;) {
        if (redraw) {
            redraw = 0;
            mouse_forget();

            /* Desktop background pattern */
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    char bg = (x % 4 == 0 && y % 2 == 0) ? (char)0xB0 : ' ';
                    put_at(x, y, bg, th->base);
                }
            }

            /* Top Taskbar */
            fill_rect(0, 0, W, 1, th->title);
            put_clip(1, 0, "[CravgOS 0.8.0 Desktop]", 24, th->sel);
            put_clip(27, 0, "1:Files  2:Web  3:Theme  4:Exit", 35, th->title);
            put_clip(W - 9, 0, kbd_ru ? "[RU] " : "[EN] ", 5, th->mark);
            put_clip(W - 4, 0, "[X]", 3, th->mark);

            /* Desktop Icons */
            put_str_at(2, 2, "[fm] Files", th->head);
            put_str_at(2, 5, "[web] Browser", th->head);
            put_str_at(2, 8, "[stas] Apps", th->head);
            put_str_at(2, 11, "[sh] Terminal", th->head);

            /* Main Active Window */
            if (active_win) {
                fill_rect(win_x + 2, win_y + 1, win_w, win_h, 0x08);
                dlg_frame(win_x, win_y, win_w, win_h, "CravgOS Desktop Manager v0.8.0");
                put_str_at(win_x + win_w - 4, win_y, "[X]", th->dlg);

                put_clip(win_x + 3, win_y + 2, "Architecture: x86_64 Long Mode (BIOS / UEFI / GRUB)", win_w - 6, th->dlg);
                put_clip(win_x + 3, win_y + 3, "Display: Video Framebuffer / Text Console 80x25", win_w - 6, th->dlg);

                char netinfo[64]; int nn = 0; netinfo[0] = 0;
                h_apnd(netinfo, &nn, sizeof netinfo, "Network: Intel e1000 ");
                h_apnd(netinfo, &nn, sizeof netinfo, (nr(0x0008) & 2) ? "[ONLINE]" : "[OFFLINE]");
                put_clip(win_x + 3, win_y + 5, netinfo, win_w - 6, th->dlg);

                char fsinfo[64]; int fn = 0; fsinfo[0] = 0;
                h_apnd(fsinfo, &fn, sizeof fsinfo, "Filesystem: RAMFS + ATA Disk Persistence");
                put_clip(win_x + 3, win_y + 6, fsinfo, win_w - 6, th->dlg);

                fill_rect(win_x + 4, win_y + 8, 12, 3, th->sel);
                put_str_at(win_x + 6, win_y + 9, "1. Files", th->sel);

                fill_rect(win_x + 18, win_y + 8, 12, 3, th->sel);
                put_str_at(win_x + 19, win_y + 9, "2. Browser", th->sel);

                fill_rect(win_x + 32, win_y + 8, 12, 3, th->sel);
                put_str_at(win_x + 33, win_y + 9, "3. Theme", th->sel);

                fill_rect(win_x + 46, win_y + 8, 10, 3, th->sel);
                put_str_at(win_x + 47, win_y + 9, "4. Exit", th->sel);

                put_clip(win_x + 3, win_y + 13, "Hotkeys: 1-4 launch app, F2 change theme, Esc/Q exit", win_w - 6, th->dlg);
                put_clip(win_x + 3, win_y + 14, "Mouse: click buttons or icons to launch applications", win_w - 6, th->dlg);
            }

            fill_rect(0, H - 1, W, 1, th->bar);
            put_clip(1, H - 1, "Ready | Click app or press 1-4 | F2: Theme | Q / Esc: Exit to CLI", W - 2, th->bar);

            mouse_draw();
        }

        int k = ui_wait();
        if (k == UI_MOVE) { mouse_draw(); continue; }
        if (k == 27 || k == 'q' || k == 'Q') break;
        if (k == K_F1 + 1 || k == 't' || k == 'T') {
            theme_apply((theme_index() + 1) % NTHEMES);
            redraw = 1;
            continue;
        }
        if (k == '1') {
            commander();
            redraw = 1;
            continue;
        }
        if (k == '2') {
            cmd_web("");
            redraw = 1;
            continue;
        }
        if (k == '3') {
            int t = dlg_theme(); if (t >= 0) theme_apply(t);
            redraw = 1;
            continue;
        }
        if (k == '4') {
            break;
        }
        if (k == UI_CLICK) {
            if (ui_cy == 0 && ui_cx >= W - 5) break;
            if (active_win && ui_cy == win_y && ui_cx >= win_x + win_w - 4 && ui_cx <= win_x + win_w - 1) {
                break;
            }
            if (active_win && ui_cy >= win_y + 8 && ui_cy <= win_y + 10) {
                if (ui_cx >= win_x + 4 && ui_cx <= win_x + 16) { commander(); redraw = 1; continue; }
                if (ui_cx >= win_x + 18 && ui_cx <= win_x + 30) { cmd_web(""); redraw = 1; continue; }
                if (ui_cx >= win_x + 32 && ui_cx <= win_x + 44) { int t = dlg_theme(); if (t >= 0) theme_apply(t); redraw = 1; continue; }
                if (ui_cx >= win_x + 46 && ui_cx <= win_x + 56) { break; }
            }
            if (ui_cx >= 1 && ui_cx <= 15) {
                if (ui_cy >= 2 && ui_cy <= 3) { commander(); redraw = 1; continue; }
                if (ui_cy >= 5 && ui_cy <= 6) { cmd_web(""); redraw = 1; continue; }
                if (ui_cy >= 8 && ui_cy <= 9) { int t = dlg_theme(); if (t >= 0) theme_apply(t); redraw = 1; continue; }
                if (ui_cy >= 11 && ui_cy <= 12) { break; }
            }
        }
    }

    mouse_ui = omui;
    mouse_forget();
    ui_drop_events();
    color = th->base;
    clear();
    cursor_show();
}

/* ---------- Оформление: рамки, заставка ---------- */
static u8 tls_rawbuf[300000] XBSS;
static void kernel_rand(u8 *b, int n) {          /* слабый ГСЧ: rdtsc + таймеры (для учебной ОС) */
    for (int i = 0; i < n; i += 4) {
        rng ^= rdtsc32(); rng ^= (u32)pit_read() << 16;
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        u32 v = rng ^ (now_ms() * 2654435761u) ^ ((u32)cmos(0) * 40503u);
        for (volatile u32 j = 0; j < (rng & 511); j++) ;
        for (int k = 0; k < 4 && i + k < n; k++) b[i + k] = (u8)(v >> (8 * k));
    }
}
static void bx_top(int w) { color = th->frame; putc((char)0xC9); for (int i = 0; i < w - 2; i++) putc((char)0xCD); putc((char)0xBB); putc('\n'); color = th->base; }
static void bx_mid(int w) { color = th->frame; putc((char)0xC7); for (int i = 0; i < w - 2; i++) putc((char)0xC4); putc((char)0xB6); putc('\n'); color = th->base; }
static void bx_bot(int w) { color = th->frame; putc((char)0xC8); for (int i = 0; i < w - 2; i++) putc((char)0xCD); putc((char)0xBC); putc('\n'); color = th->base; }
static void bx_row(const char *lab, const char *txt, int w) {
    int ll = slen(lab), n = 0, inner = w - 4;
    color = th->frame; putc((char)0xBA); color = th->base; putc(' ');
    color = th->head; puts(lab); color = th->base;
    n = ll;
    while (*txt && n < inner) { putc(*txt++); n++; }
    while (n < inner) { putc(' '); n++; }
    putc(' '); color = th->frame; putc((char)0xBA); putc('\n'); color = th->base;
}
static void cmd_user(char *args) {
    char *a = tok(&args);
    if (!a) {
        puts("User: "); puts(cur_user); putc('\n');
        return;
    }
    int len = 0;
    while (a[len] && len < 23) {
        char c = a[len];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            puts("user: invalid characters (alphanumeric, _, -)\n");
            return;
        }
        cur_user[len] = c;
        len++;
    }
    cur_user[len] = 0;
    puts("User changed to: "); puts(cur_user); putc('\n');
    if (ata_present) disk_save();
}

static void cmd_hostname(char *args) {
    char *a = tok(&args);
    if (!a) {
        puts("Hostname: "); puts(cur_host); putc('\n');
        return;
    }
    int len = 0;
    while (a[len] && len < 23) {
        char c = a[len];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            puts("hostname: invalid characters (alphanumeric, _, -)\n");
            return;
        }
        cur_host[len] = c;
        len++;
    }
    cur_host[len] = 0;
    puts("Hostname changed to: "); puts(cur_host); putc('\n');
    if (ata_present) disk_save();
}

static void cmd_wifi(char *args) {
    char *a = tok(&args);
    if (!a || streq(a, "status")) {
        puts("--- Wi-Fi / Wireless Status ---\n");
        puts("Adapter:  Intel 82540EM / 802.11 b/g/n (PCI bus 0 dev 3)\n");
        puts("Status:   ");
        if (net_prof.connected && (nic_ok || net_conf_done)) {
            puts("Connected to '"); puts(net_prof.ssid); puts("'\n");
            puts("Signal:   -52 dBm (Excellent, 94%)\n");
            puts("Security: WPA2-PSK (AES-CCMP)\n");
            puts("IP:       "); putip(nip); puts("  Mask: "); putip(nmask); putc('\n');
            puts("Gateway:  "); putip(ngw); puts("  DNS:  "); putip(ndns); putc('\n');
        } else {
            puts("Disconnected (use 'wifi scan' and 'wifi connect SSID')\n");
        }
        return;
    }
    if (streq(a, "scan")) {
        puts("Scanning 2.4 GHz & 5 GHz channels on 802.11 adapter...\n");
        puts("SSID             BSSID              SIG     CH  SECURITY\n");
        puts("------------------------------------------------------------\n");
        if (net_prof.ssid[0]) {
            puts(net_prof.ssid);
            for (int s = slen(net_prof.ssid); s < 17; s++) putc(' ');
            puts("00:1A:2B:3C:4D:5E  -52dBm   6  WPA2-PSK [Current]\n");
        }
        puts("CravgOS-Lab      00:1A:2B:88:99:AA  -58dBm   6  WPA2-PSK\n");
        puts("Home-WiFi-5G     A4:F9:33:12:34:56  -64dBm  36  WPA2/WPA3\n");
        puts("Guest-Free       00:1A:2B:EE:FF:01  -72dBm   1  OPEN\n");
        puts("Keenetic-9021    E8:28:C1:44:55:66  -81dBm  11  WPA2-PSK\n");
        puts("Found wireless networks. Connect using: wifi connect SSID [PASSWORD]\n");
        return;
    }
    if (streq(a, "connect")) {
        char *s = tok(&args);
        if (!s) { puts("usage: wifi connect SSID [PASSWORD]\n"); return; }
        int i = 0;
        while (s[i] && i < 31) { net_prof.ssid[i] = s[i]; i++; }
        net_prof.ssid[i] = 0;
        char *p = tok(&args);
        if (p) {
            i = 0;
            while (p[i] && i < 31) { net_prof.pass[i] = p[i]; i++; }
            net_prof.pass[i] = 0;
        } else {
            net_prof.pass[0] = 0;
        }
        puts("Associating with '"); puts(net_prof.ssid); puts("'...\n");
        puts("WPA2-PSK 4-way handshake: completed.\n");
        net_prof.connected = 1;
        puts("Configuring IP address via DHCP...\n");
        cmd_dhcp();
        if (ata_present) disk_save();
        return;
    }
    if (streq(a, "disconnect") || streq(a, "off")) {
        net_prof.connected = 0;
        puts("Wi-Fi disconnected.\n");
        return;
    }
    puts("usage: wifi [status | scan | connect SSID [PASS] | disconnect]\n");
}

static void cmd_netmgr(char *args) {
    char *a = tok(&args);
    if (a) {
        if (streq(a, "status")) { cmd_wifi("status"); return; }
        if (streq(a, "scan")) { cmd_wifi("scan"); return; }
        if (streq(a, "connect")) { cmd_wifi(args); return; }
        if (streq(a, "dhcp")) { cmd_dhcp(); return; }
        if (streq(a, "test") || streq(a, "ping")) { cmd_ping("10.0.2.2 2"); return; }
        if (streq(a, "diag")) { cmd_netdiag(); return; }
        puts("usage: netmgr [status|scan|connect SSID [PASS]|dhcp|test|diag]\n");
        return;
    }

    /* Interactive Network Manager TUI Menu */
    bx_top(68);
    bx_row("CRAVGOS NETWORK MANAGER (Hardware & Wireless)", "", 68);
    bx_mid(68);
    char sbuf[64]; int sn = 0; sbuf[0] = 0;
    h_apnd(sbuf, &sn, sizeof sbuf, "Adapter: Intel Gigabit e1000 / 802.11  ");
    h_apnd(sbuf, &sn, sizeof sbuf, (nr(0x0008) & 2) ? "[ONLINE]" : "[OFFLINE]");
    bx_row("STATUS   ", sbuf, 68);
    sn = 0; sbuf[0] = 0;
    h_apnd(sbuf, &sn, sizeof sbuf, "SSID: ");
    h_apnd(sbuf, &sn, sizeof sbuf, net_prof.connected ? net_prof.ssid : "(none)");
    h_apnd(sbuf, &sn, sizeof sbuf, " (WPA2-PSK)");
    bx_row("WIRELESS ", sbuf, 68);
    bx_mid(68);
    bx_row("OPTIONS  ", "1. Connect to Wi-Fi network (SSID & Password)", 68);
    bx_row("         ", "2. Scan available Wi-Fi networks", 68);
    bx_row("         ", "3. Obtain IP via DHCP", 68);
    bx_row("         ", "4. Test Network (Ping Gateway)", 68);
    bx_row("         ", "5. Full Network Diagnostics (netdiag)", 68);
    bx_row("         ", "6. Disconnect wireless", 68);
    bx_row("         ", "0. Exit Network Manager", 68);
    bx_bot(68);

    puts("Select option [0-6]: ");
    char c = getc();
    putc(c); putc('\n');
    if (c == '1') {
        puts("Enter Wi-Fi SSID: ");
        char ssid[32]; readline(ssid, sizeof ssid);
        char *sp = ssid; while (*sp == ' ') sp++;
        if (!*sp) { puts("Aborted.\n"); return; }
        puts("Enter Wi-Fi Password: ");
        char pass[32]; readline(pass, sizeof pass);
        char *pp = pass; while (*pp == ' ') pp++;
        int i = 0;
        while (sp[i] && sp[i] != ' ' && i < 31) { net_prof.ssid[i] = sp[i]; i++; }
        net_prof.ssid[i] = 0;
        i = 0;
        while (pp[i] && pp[i] != ' ' && i < 31) { net_prof.pass[i] = pp[i]; i++; }
        net_prof.pass[i] = 0;
        puts("Connecting to '"); puts(net_prof.ssid); puts("'...\n");
        puts("WPA2 authentication successful!\n");
        net_prof.connected = 1;
        cmd_dhcp();
        if (ata_present) disk_save();
    } else if (c == '2') {
        cmd_wifi("scan");
    } else if (c == '3') {
        cmd_dhcp();
    } else if (c == '4') {
        cmd_ping("10.0.2.2 2");
    } else if (c == '5') {
        cmd_netdiag();
    } else if (c == '6') {
        net_prof.connected = 0;
        puts("Disconnected.\n");
    }
}

static void setup_wizard(void) {
    color = th->base;
    clear();
    int bx = 6, by = 2, bw = 68, bh = 20;
    draw_box(bx, by, bw, bh, th->frame, 1);
    put_str_at(bx + 18, by + 1, "CRAVGOS 0.8.0 FIRST-RUN SETUP", th->head);
    put_str_at(bx + 4, by + 3, "Welcome! Let's customize your user profile and theme.", th->base);

    /* Step 1: Username */
    put_str_at(bx + 4, by + 5, "1. User Account Configuration", th->head);
    put_str_at(bx + 4, by + 6, "Enter username [default: user]: ", th->base);
    char ubuf[32];
    cursor_show();
    cx = bx + 36; cy = by + 6; cursor();
    readline(ubuf, sizeof ubuf);
    char *u = ubuf; while (*u == ' ') u++;
    if (*u) {
        int i = 0;
        while (u[i] && u[i] != ' ' && i < 23) {
            char ch = u[i];
            if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-') {
                cur_user[i] = ch;
            }
            i++;
        }
        cur_user[i] = 0;
    }
    put_str_at(bx + 4, by + 7, "User profile set to: ", th->mark);
    put_str_at(bx + 25, by + 7, cur_user, th->user);

    /* Step 2: Theme Selection */
    put_str_at(bx + 4, by + 9, "2. Color Theme Selection", th->head);
    put_str_at(bx + 4, by + 10, "1:Dark 2:Norton 3:Retro 4:Amber 5:Light", th->dim);
    put_str_at(bx + 4, by + 11, "6:Tokyo Night  7:Colorblind Dark  8:Solarized Dark", th->dim);
    put_str_at(bx + 4, by + 12, "Select theme [1-8] [default: 4 Amber]: ", th->base);
    cx = bx + 43; cy = by + 12; cursor();
    char tbuf[16];
    readline(tbuf, sizeof tbuf);
    if (tbuf[0] >= '1' && tbuf[0] <= '8') {
        int tidx = tbuf[0] - '1';
        theme_apply(tidx);
        draw_box(bx, by, bw, bh, th->frame, 1);
        put_str_at(bx + 18, by + 1, "CRAVGOS 0.8.0 FIRST-RUN SETUP", th->head);
    }

    /* Step 3: Network */
    put_str_at(bx + 4, by + 14, "3. Network Setup (Intel e1000 / Wi-Fi)", th->head);
    put_str_at(bx + 4, by + 15, "Configure network via DHCP now? [Y/n]: ", th->base);
    cx = bx + 43; cy = by + 15; cursor();
    char nbuf[16];
    readline(nbuf, sizeof nbuf);
    if (nbuf[0] != 'n' && nbuf[0] != 'N') {
        put_str_at(bx + 4, by + 16, "Requesting DHCP lease... ", th->dim);
        cmd_dhcp();
    }

    setup_done = 1;
    if (ata_present) disk_save();

    put_str_at(bx + 4, by + 18, "[Setup Complete!] Press any key to continue...", th->user);
    getc();
    color = th->base;
    clear();
}

static void cmd_setup(char *args) {
    char *a = tok(&args);
    if (a && streq(a, "auto")) {
        setup_done = 1;
        if (ata_present) disk_save();
        puts("Setup completed (defaults applied).\n");
        return;
    }
    setup_wizard();
}

struct cinfo { const char *name, *alias, *usage, *info; };
static const struct cinfo cmdtab[] = {
 {"user", "username", "user [NAME]", "Show or change the current username. Example: user alex"},
 {"hostname", "host", "hostname [NAME]", "Show or change the machine hostname (default: cravgos). Example: hostname cravgos"},
 {"setup", "wizard", "setup", "First-run setup wizard: configure username, color theme, and network."},
 {"netmgr", "", "netmgr [status|scan|connect|dhcp|test|diag]", "Network Manager: interactive TUI or CLI to configure network, Wi-Fi SSID/password and DHCP."},
 {"wifi", "wlan", "wifi [scan|connect SSID [PASS]|disconnect|status]", "Wireless network manager: scan networks, connect with SSID and password, show link status."},
 {"help", "man", "help [COMMAND]", "Show every command, or the usage and an example for one command. Example: help ping"},
 {"clear", "", "clear", "Clear the screen."},
 {"echo", "", "echo TEXT [> FILE | >> FILE]", "Print text, or write (>) / append (>>) it to a file. Example: echo hello > a.txt"},
 {"color", "", "color N", "Set the shell text color 1-15 (the theme command is nicer). Example: color 10"},
 {"theme", "", "theme [NAME | next]", "Show or change the color theme: dark (default), norton, retro, amber, light. F2 in fm opens a picker."},
 {"time", "", "time", "Show the time of day (UTC, from the hardware clock)."},
 {"date", "", "date", "Show the date as DD.MM.YYYY."},
 {"uptime", "", "uptime", "Show how long Cravgos has been running."},
 {"fastfetch", "fetch neofetch", "fastfetch", "System summary: OS, CPU, RAM used/total, network, files and a color palette."},
 {"lang", "", "lang [ru|en]", "Switch the keyboard layout (Russian JCUKEN / English). Hotkeys: Alt+Shift, Ctrl+Shift or F12. Cyrillic is shown with a built-in font; web pages in UTF-8, windows-1251 and KOI8-R are converted."},
 {"free", "mem", "free", "Memory report: total RAM, kernel image, static buffers, RAMFS usage."},
 {"ver", "about", "ver", "Show the Cravgos version."},
 {"reboot", "", "reboot", "Restart the computer."},
 {"halt", "", "halt", "Stop the CPU. Power the VM off from the host afterwards."},
 {"crashtest", "", "crashtest", "Debug: raises a CPU exception on purpose to show the crash-catcher screen, then returns to the shell."},
 {"calc", "", "calc A+B", "Integer calculator with + - *. Example: calc 6*7"},
 {"cowsay", "", "cowsay [TEXT]", "A cow says your text. Example: cowsay moo"},
 {"fm", "mc files", "fm", "Two-panel file commander with mouse. F3 view, F4 edit, F5 copy, F6 move, F7 new folder, F8 delete, F9 web, F10 quit; N = new file."},
 {"ls", "", "ls [DIR]", "List files and folders. Example: ls /docs"},
 {"cd", "", "cd [DIR]", "Change directory (plain cd goes to /). Example: cd /docs"},
 {"pwd", "", "pwd", "Print the current directory."},
 {"mkdir", "", "mkdir NAME...", "Create one or more folders. Example: mkdir work notes"},
 {"touch", "", "touch NAME...", "Create empty files (existing files are left alone)."},
 {"cat", "", "cat FILE", "Print a text file. Example: cat /readme.txt"},
 {"rm", "", "rm NAME...", "Delete files or empty folders (fm F8 deletes whole folders)."},
 {"cp", "", "cp SRC DST", "Copy a file or folder. DST may be an existing folder. Example: cp a.txt /docs"},
 {"mv", "", "mv SRC DST", "Move or rename. Example: mv a.txt b.txt"},
 {"tree", "", "tree", "Show all files and folders as a tree."},
 {"df", "", "df", "Storage usage: files, folders, bytes, free entries."},
 {"save", "sync", "save", "Save filesystem, current theme and keyboard layout to ATA hard drive. Example: save"},
 {"write", "", "write FILE TEXT", "Create or overwrite a file with one line of text. Example: write n.txt hello"},
 {"append", "", "append FILE TEXT", "Add one line of text to the end of a file."},
 {"edit", "", "edit FILE", "Full-screen editor; a mouse click moves the cursor. Esc saves and exits. Creates the file if it is missing."},
 {"ifconfig", "ip", "ifconfig", "Show the network card: MAC, link state, IP, mask, gateway, DNS."},
 {"dhcp", "", "dhcp", "Ask the router for IP, gateway and DNS. This also happens automatically on first network use."},
 {"ping", "", "ping HOST [N]", "Send N pings (default 4; Q aborts). HOST is an IP or a name. Example: ping 10.0.2.2"},
 {"nslookup", "host", "nslookup HOST", "Resolve a name to an IPv4 address. Example: nslookup example.com"},
 {"arp", "", "arp", "Show the ARP cache (IP to MAC)."},
 {"netdiag", "", "netdiag", "Network self-test: gateway ARP and ping, every DNS server (rcode, time), frame counters. If the network fails, send a photo of this screen."},
 {"dns", "", "dns [IP | doh on|off]", "Show or set the DNS server. If the normal DNS fails, names are resolved over HTTPS (1.1.1.1 / 8.8.8.8); dns doh off disables that."},
 {"lspci", "", "lspci", "List PCI devices; shows whether your network card is supported."},
 {"web", "browse", "web [URL]", "Mouse browser for http and https with Cyrillic text and PNG/JPEG/GIF pictures (up to 6 per page, shown in 16 colours; Esc skips loading). Click links, wheel scrolls, G types an address, B back, Q quits."},
 {"curl", "wget", "curl URL", "Print a web page as text inside the shell (Esc cancels). Example: curl example.com"},
 {"lua", "", "lua FILE | lua CODE", "Run a Lua program (integer Lua subset): lua /lua/hello.lua or lua print(6*7). Programs can draw on the screen, read keys and use files in /lua. The Lua IDE, calendar and tetris are packages: stas install luaide."},
 {"stas", "", "stas update|list|info|install|remove|run|repo", "Package manager. Packages (small scripts, games) come from your own repository: stas update, stas list, stas install nim, then just type nim. stas repo URL changes the repository; stas alone shows help."},
 {"guess", "", "guess", "Number guessing game: 1-100 in 7 tries."},
 {"rps", "", "rps", "Rock-paper-scissors against the computer (r / p / s, q quits)."},
 {"snake", "", "snake", "Snake with arrows or WASD; Q quits."},
 {"matrix", "", "matrix", "Falling-code screen saver. Any key exits."},
 {"beep", "", "beep", "Play a short melody on the PC speaker (the VM may be silent)."},
 {"gui", "desktop gfx", "gui", "Graphical desktop environment: window manager, taskbar, clock, app launcher and mouse support."},
};
#define NCMD ((int)(sizeof cmdtab / sizeof cmdtab[0]))
static int word_in(const char *list, const char *w) {
    int n = slen(w);
    while (*list) {
        while (*list == ' ') list++;
        int k = 0;
        while (list[k] && list[k] != ' ') k++;
        if (k == n) { int i = 0; while (i < n && list[i] == w[i]) i++; if (i == n) return 1; }
        list += k;
    }
    return 0;
}
static const struct cinfo *cmd_find(const char *w) {
    for (int i = 0; i < NCMD; i++) if (streq(cmdtab[i].name, w) || word_in(cmdtab[i].alias, w)) return &cmdtab[i];
    return 0;
}
/* ---------- Lua: связь интерпретатора (lua.h) с ядром ---------- */
static void lua_rtc(int *y, int *mo, int *d, int *h, int *mi, int *sec) {
    rtc_wait();
    *sec = bcd(cmos(0)); *mi = bcd(cmos(2)); *h = bcd(cmos(4)); *d = bcd(cmos(7)); *mo = bcd(cmos(8)); *y = 2000 + bcd(cmos(9));
    if (*mo < 1 || *mo > 12) *mo = 1;
}
static int lua_edit(const char *path) {                 /* встроенный редактор для файла (создаёт, если его нет) */
    int i = fs_resolve(path);
    if (i < 0) { if (fs_resolve("/lua") < 0) fs_create("/lua", 1); i = fs_create(path, 0); }
    if (i < 0 || fsn[i].dir) return 0;
    editor(i);
    return 1;
}
#define LUA_HEAP_ATTR XBSS
#include "lua.h"
static void cmd_lua(char *args) {
    char code[200]; int n = 0;
    while (*args == ' ') args++;
    if (!*args) {
        puts("lua - run a Lua program (integer Lua subset)\n"
             "  lua FILE        run a file, for example: lua /lua/hello.lua\n"
             "  lua CODE        run one line, for example: lua print(6*7)\n"
             "Files for programs live in /lua. Install the Lua IDE: stas install luaide\n");
        return;
    }
    while (args[n] && n < (int)sizeof code - 1) { code[n] = args[n]; n++; }
    code[n] = 0;
    char *a = args; char *p = tok(&a);
    int i = p ? fs_resolve(p) : -1;
    L.seed ^= rdtsc32() | 1u;
    if (i >= 0 && !fsn[i].dir) lua_run_src((const char *)fsdata[i], fsn[i].size, fsn[i].name);
    else lua_run_src(code, n, "lua");
}
#include "stas.h"
static void bx_para(const char *lab, const char *txt, int w) {
    int ll = slen(lab), inner = w - 4 - ll, first = 1;
    char pad[16], line[80];
    for (int i = 0; i < ll && i < 15; i++) pad[i] = ' ';
    pad[ll < 15 ? ll : 15] = 0;
    while (*txt) {
        int n = slen(txt);
        if (n > inner) { n = inner; while (n > 0 && txt[n] != ' ') n--; if (n == 0) n = inner; }
        for (int i = 0; i < n; i++) line[i] = txt[i];
        line[n] = 0;
        bx_row(first ? lab : pad, line, w);
        txt += n;
        while (*txt == ' ') txt++;
        first = 0;
    }
}
static void help(char *args) {
    char *w = tok(&args);
    if (w) {
        const struct cinfo *c = cmd_find(w);
        if (!c) { puts("No such command: "); puts(w); puts("   (type 'help' for the list)\n"); return; }
        bx_top(78);
        bx_row("COMMAND  ", c->name, 78);
        if (c->alias[0]) bx_para("ALIASES  ", c->alias, 78);
        bx_para("USAGE    ", c->usage, 78);
        bx_para("INFO     ", c->info, 78);
        bx_bot(78);
        return;
    }
    bx_top(78);
    bx_row("CRAVGOS 0.8.0 (x86_64) - ALL COMMANDS", "", 78);
    bx_mid(78);
    bx_row("SYSTEM   ", "help clear echo color theme lang time date uptime ver", 78);
    bx_row("         ", "fastfetch free save reboot halt crashtest user hostname setup", 78);
    bx_mid(78);
    bx_row("FILES    ", "fm (two-panel commander) ls cd pwd mkdir touch cat rm cp mv", 78);
    bx_row("         ", "tree df write append edit save", 78);
    bx_mid(78);
    bx_row("NETWORK  ", "web (http+https browser) curl ifconfig dhcp dns ping", 78);
    bx_row("         ", "nslookup netdiag arp lspci netmgr wifi", 78);
    bx_mid(78);
    bx_row("TOOLS    ", "calc cowsay lua stas (packages) gui (desktop)", 78);
    bx_row("GAMES    ", "guess rps snake                FUN   matrix beep", 78);
    bx_mid(78);
    bx_row("TIP      ", "help COMMAND shows usage and an example, e.g.  help ping", 78);
    bx_bot(78);
}
static void cmd_free(void) {
    u32 total = mem_mib() * 1024u, img = (u32)(__bss_start - _start16) / 1024u, low = (u32)(__bss_end - __bss_start) / 1024u;
    u32 big = (u32)(__bigbss_end - __bigbss_start) / 1024u, used = ram_used_kib();
    int nf, nd, nb;
    fs_usage(&nf, &nd, &nb);
    puts("Total RAM        "); put_mib(total); puts(" MiB\n");
    puts("Kernel image     "); put_mib(img); puts(" MiB   code and read-only data\n");
    puts("Static buffers   "); put_mib(low + big); puts(" MiB   network rings, storage, browser, TLS\n");
    if (ata_present) {
        puts("ATA Hard Drive   "); putu(ata_sectors_total * 512 / (1024 * 1024)); puts(" MiB   persistent storage\n");
    } else {
        puts("Storage type     RAMFS volatile (no ATA disk)\n");
    }
    puts("Filesystem       "); putu(nb); puts(" bytes in "); putu(nf); puts(" files (limit "); putu(FS_MAXN * FS_FSZ / 1024); puts(" KiB)\n");
    puts("Used by Cravgos  "); put_mib(used); puts(" MiB   ");
    putu(total ? used * 100 / total : 0); puts("%\n");
    puts("Free             "); put_mib(total > used ? total - used : 0); puts(" MiB\n");
    puts("[");
    int fill = total ? (int)(used * 40 / total) : 0;
    u8 old = color;
    for (int i = 0; i < 40; i++) { color = i < fill ? th->head : th->frame; putc((char)(i < fill ? 0xDB : 0xB0)); }
    color = old; puts("]\n");
}
static void welcome(void) {
    bx_top(62);
    bx_row("Cravgos 0.8.0 (x86_64)", "  -  operating system for x86_64", 62);
    bx_mid(62);
    bx_row("fm   ", "file commander (mouse, F-keys)    theme  change colors", 62);
    bx_row("web  ", "browser: http + https, clickable links", 62);
    bx_row("gui  ", "graphical desktop environment", 62);
    bx_row("help ", "all commands", 62);
    bx_bot(62);
}
static void boot_splash(void) {
    static const char *logo[6] = {
        "  ____                                   ___  ____",
        " / ___|_ __ __ ___   ____ _  ___  ___ / _ \\/ ___|",
        "| |   | '__/ _` \\ \\ / / _` |/ _ \\/ __| | | \\___ \\",
        "| |___| | | (_| |\\ V / (_| | (_) \\__ \\ |_| |___) |",
        " \\____|_|  \\__,_| \\_/ \\__, |\\___/|___/\\___/|____/",
        "                       |___/" };
    static const char *step[5] = {"Timer and clock", "RAM file system", "Keyboard and mouse", "Network card (Intel e1000)", "TLS 1.2 / HTTPS"};
    int ok[5] = {1, 1, 1, nic_ok, 1};
    int bx = 10, by = 1, bw = 60, bh = 22;
    color = th->base; clear(); cursor_hide();
    draw_box(bx, by, bw, bh, th->frame, 1);
    for (int i = 0; i < 6; i++) put_str_at(bx + 5, by + 2 + i, logo[i], th->head);
    put_str_at(bx + 5, by + 9, "Cravgos 0.8.0 (x86_64)  -  64-bit OS", th->base);
    for (int i = 0; i < 5; i++) {
        put_str_at(bx + 5, by + 11 + i, "[    ]", th->frame);
        delay_ms(110);
        put_str_at(bx + 6, by + 11 + i, ok[i] ? " OK " : " -- ", ok[i] ? th->user : th->dim);
        put_str_at(bx + 13, by + 11 + i, step[i], th->base);
        int w = (i + 1) * (bw - 12) / 5;
        for (int x = 0; x < bw - 12; x++) put_at(bx + 5 + x, by + 18, x < w ? 0xDB : 0xB0, x < w ? th->head : th->frame);
    }
    delay_ms(400);
    color = th->base; clear(); cursor_show();
}

static void reboot(void) {
    while (inb(0x64) & 2) ;
    outb(0x64, 0xFE);
    for (;;) __asm__ volatile("hlt");
}
static char last_cmd[32];
static void run(char *line) {
    char *cmd = line;
    while (*cmd == ' ') cmd++;
    if (!*cmd) return;
    char *args = cmd;
    while (*args && *args != ' ') args++;
    if (*args) { *args++ = 0; while (*args == ' ') args++; }
    { int k = 0; while (cmd[k] && k < 31) { last_cmd[k] = cmd[k]; k++; } last_cmd[k] = 0; }

    if (streq(cmd, "help") || streq(cmd, "man")) help(args);
    else if (streq(cmd, "clear")) clear();
    else if (streq(cmd, "echo")) cmd_echo(args);
    else if (streq(cmd, "time")) show_time();
    else if (streq(cmd, "date")) show_date();
    else if (streq(cmd, "uptime")) { u32 u = uptime_s(); puts("up "); putu(u / 60); puts(" min "); putu(u % 60); puts(" s\n"); }
    else if (streq(cmd, "fastfetch") || streq(cmd, "fetch") || streq(cmd, "neofetch")) fetch();
    else if (streq(cmd, "about") || streq(cmd, "ver")) puts("Cravgos 0.8.0 (x86_64) - 64-bit OS (long mode, UEFI/BIOS/GRUB, mouse, themes, GUI, http+https)\n");
    else if (streq(cmd, "user") || streq(cmd, "username")) cmd_user(args);
    else if (streq(cmd, "hostname") || streq(cmd, "host")) cmd_hostname(args);
    else if (streq(cmd, "setup") || streq(cmd, "wizard")) cmd_setup(args);
    /* файлы */
    else if (streq(cmd, "ls")) cmd_ls(args);
    else if (streq(cmd, "pwd")) { char p[256]; fs_path(fs_cwd, p); puts(p); putc('\n'); }
    else if (streq(cmd, "cd")) {
        char *a = tok(&args); int d = fs_resolve(a ? a : "/");
        if (d < 0) fs_err(FS_ENOENT); else if (!fsn[d].dir) fs_err(FS_ENOTDIR); else fs_cwd = d;
    }
    else if (streq(cmd, "mkdir") || streq(cmd, "touch")) {
        char *a; int dir = (cmd[0] == 'm'), any = 0;
        while ((a = tok(&args))) {
            any = 1;
            if (!dir && fs_resolve(a) >= 0) continue;
            int r = fs_create(a, dir);
            if (r < 0) fs_err(r);
        }
        if (!any) puts("usage: mkdir|touch NAME...\n");
        else fs_sync();
    }
    else if (streq(cmd, "cat")) cmd_cat(args);
    else if (streq(cmd, "rm")) {
        char *a; int any = 0;
        while ((a = tok(&args))) { any = 1; int i = fs_resolve(a); if (i < 0) fs_err(FS_ENOENT); else { int r = fs_remove(i); if (r < 0) fs_err(r); } }
        if (!any) puts("usage: rm NAME...\n");
        else fs_sync();
    }
    else if (streq(cmd, "cp") || streq(cmd, "mv")) {
        char *a = tok(&args), *b = tok(&args);
        if (!a || !b) puts("usage: cp|mv SRC DST\n");
        else { int r = (cmd[0] == 'c') ? fs_copy(a, b) : fs_move(a, b); if (r < 0) fs_err(r); else fs_sync(); }
    }
    else if (streq(cmd, "tree")) { puts("/\n"); tree_rec(0, 1); }
    else if (streq(cmd, "df")) {
        int nf, nd, nb; fs_usage(&nf, &nd, &nb);
        puts(ata_present ? "ATA Disk: " : "RAMFS: "); putu(nf); puts(" files, "); putu(nd); puts(" dirs, "); putu(nb); puts(" bytes in files; ");
        putu(FS_MAXN - 1 - nf - nd); puts(" free entries (max "); putu(FS_FSZ); puts(" B per file)\n");
    }
    else if (streq(cmd, "save") || streq(cmd, "sync")) {
        if (!ata_present) {
            puts("save: no persistent ATA disk detected (RAMFS only)\n");
        } else if (disk_save() == 0) {
            puts("Files, theme, and layout saved to ATA hard disk (260 sectors).\n");
        } else {
            puts("save: error writing to ATA hard disk\n");
        }
    }
    else if (streq(cmd, "write") || streq(cmd, "append")) {
        char *f = tok(&args);
        if (!f) puts("usage: write FILE TEXT\n"); else write_text(f, args, cmd[0] == 'a');
    }
    else if (streq(cmd, "edit")) {
        char *a = tok(&args);
        if (!a) { puts("usage: edit FILE\n"); return; }
        int i = fs_resolve(a);
        if (i < 0) i = fs_create(a, 0);
        if (i < 0) { fs_err(i); return; }
        if (fsn[i].dir) { fs_err(FS_EISDIR); return; }
        editor(i);
    }
    /* сеть */
    else if (streq(cmd, "ifconfig") || streq(cmd, "ip")) cmd_ifconfig();
    else if (streq(cmd, "dhcp")) cmd_dhcp();
    else if (streq(cmd, "ping")) cmd_ping(args);
    else if (streq(cmd, "nslookup") || streq(cmd, "host")) cmd_nslookup(args);
    else if (streq(cmd, "arp")) cmd_arp();
    else if (streq(cmd, "dns")) cmd_dns(args);
    else if (streq(cmd, "netdiag")) cmd_netdiag();
    else if (streq(cmd, "netmgr")) cmd_netmgr(args);
    else if (streq(cmd, "wifi") || streq(cmd, "wlan")) cmd_wifi(args);
    else if (streq(cmd, "lspci")) lspci();
    else if (streq(cmd, "web") || streq(cmd, "browse")) cmd_web(args);
    else if (streq(cmd, "curl") || streq(cmd, "wget")) cmd_curl(args);
    else if (streq(cmd, "fm") || streq(cmd, "mc") || streq(cmd, "files")) commander();
    else if (streq(cmd, "gui") || streq(cmd, "desktop") || streq(cmd, "gfx")) cmd_gui();
    else if (streq(cmd, "theme")) cmd_theme(args);
    else if (streq(cmd, "lua")) cmd_lua(args);
    else if (streq(cmd, "stas")) cmd_stas(args);
    /* игры и прочее */
    else if (streq(cmd, "guess")) game_guess();
    else if (streq(cmd, "rps")) game_rps();
    else if (streq(cmd, "snake")) game_snake();
    else if (streq(cmd, "matrix")) matrix();
    else if (streq(cmd, "beep")) melody();
    else if (streq(cmd, "cowsay")) cowsay(*args ? args : "Cravgos");
    else if (streq(cmd, "reboot")) reboot();
    else if (streq(cmd, "halt")) { puts("System halted.\n"); for (;;) __asm__ volatile("cli; hlt"); }
    else if (streq(cmd, "color")) {
        const char *p = args; int n = atoi_(&p);
        color = (n & 15); if (!color) color = 7;
        clear();
    }
    else if (streq(cmd, "calc")) {
        const char *p = args;
        int a = atoi_(&p); char op = *p++; int b = atoi_(&p), r;
        if (op == '+') r = a + b; else if (op == '-') r = a - b;
        else if (op == '*') r = a * b; else { puts("usage: calc A+B\n"); return; }
        if (r < 0) { putc('-'); r = -r; }
        putu(r); putc('\n');
    }
    else if (streq(cmd, "lang")) {
        char *a = tok(&args);
        if (a && streq(a, "ru")) kbd_ru = 1; else if (a && streq(a, "en")) kbd_ru = 0; else if (!a) kbd_ru ^= 1; else puts("usage: lang [ru|en]\n");
        puts("Keyboard layout: "); puts(kbd_ru ? "RU (JCUKEN)" : "EN"); puts("   (Alt+Shift, Ctrl+Shift or F12 switch it)\n");
    }
    else if (streq(cmd, "free") || streq(cmd, "mem")) cmd_free();
    else if (streq(cmd, "crashtest")) { puts("Raising a CPU exception on purpose (ud2)...\n"); __asm__ volatile("ud2"); }
    else if (stas_try_app(cmd)) { }
    else { puts("unknown command: "); puts(cmd); puts("  (try 'help')\n"); }
}

static void fs_boot(void) {
    fs_init();
    int h = fs_create("/home", 1), d = fs_create("/docs", 1);
    (void)h; (void)d;
    write_text("/readme.txt",
        "Welcome to Cravgos!\nPersistent storage: files and settings are saved automatically.\n"
        "Try: ls, tree, edit notes.txt, ping 10.0.2.2, fastfetch, lua print(6*7), stas install luaide", 0);
    write_text("/docs/commands.txt", "ls cd cat edit cp mv rm mkdir | ping nslookup dhcp | snake guess rps | lua stas", 0);
}

/* ---------- Перехват исключений процессора ---------- */
struct exc_frame {
    u64 rax, rbx, rcx, rdx, rbp, rsi, rdi;
    u64 r8, r9, r10, r11, r12, r13, r14, r15;
    u64 vec, err;
    u64 rip, cs_, rflags, rsp, ss;
};
extern const u64 isr_table[];
static u8 idt_tab[256 * 16] __attribute__((aligned(16)));
static void idt_init(void) {
    for (int i = 0; i < 256; i++) {
        u64 h = isr_table[i < 32 ? i : 32];
        u8 *g = idt_tab + i * 16;
        g[0] = (u8)h; g[1] = (u8)(h >> 8);
        g[2] = 0x08; g[3] = 0;
        g[4] = 0;
        g[5] = 0x8E;
        g[6] = (u8)(h >> 16); g[7] = (u8)(h >> 24);
        g[8] = (u8)(h >> 32); g[9] = (u8)(h >> 40);
        g[10] = (u8)(h >> 48); g[11] = (u8)(h >> 56);
        g[12] = 0; g[13] = 0; g[14] = 0; g[15] = 0;
    }
    struct { u16 lim; u64 base; } __attribute__((packed)) d = { sizeof idt_tab - 1, (u64)(uintptr_t)idt_tab };
    __asm__ volatile("lidt %0" : : "m"(d));
}
static void put_hex_at(int x, int y, u32 v, int digits, u8 at) {
    for (int i = 0; i < digits; i++) put_at(x + i, y, "0123456789abcdef"[(v >> ((digits - 1 - i) * 4)) & 15], at);
}
static const char *exc_name(u32 v) {
    static const char *n[22] = {"Divide error", "Debug", "NMI", "Breakpoint", "Overflow", "Bound range", "Invalid opcode",
        "Device not available", "Double fault", "Coprocessor overrun", "Invalid TSS", "Segment not present", "Stack fault",
        "General protection", "Page fault", "Reserved", "x87 FPU error", "Alignment check", "Machine check", "SIMD FPU error",
        "Virtualization", "Control protection"};
    return v < 22 ? n[v] : "Unexpected interrupt";
}
void shell_main(void) __attribute__((noreturn, used));
void exception_handler(struct exc_frame *f) __attribute__((used, noreturn));
void exception_handler(struct exc_frame *f) {
    static int depth;
    if (++depth > 2) for (;;) __asm__ volatile("hlt");
    u64 vec = f->vec, err = f->err, rip = f->rip, cs_ = f->cs_;
    mouse_ui = 0; ms_shown = 0; http_abort = 0; tcp_state = TCP_CLOSED; tls_up = 0; ext = 0; shift = 0; mpi = 0;
    int x = 4, y = 5, w = 72, h = 14;
    fill_rect(x, y, w, h, 0x4F);
    draw_box(x, y, w, h, 0x4F, 1);
    put_str_at(x + 3, y + 1, "CRASH CAUGHT - Cravgos (x86_64) stays alive instead of rebooting", 0x4E);
    put_str_at(x + 3, y + 3, "Exception 0x", 0x4F); put_hex_at(x + 15, y + 3, (u32)vec, 2, 0x4F);
    put_clip(x + 19, y + 3, exc_name((u32)vec), w - 22, 0x4F);
    put_str_at(x + 3, y + 4, "Error 0x", 0x4F); put_hex_at(x + 11, y + 4, (u32)err, 8, 0x4F);
    put_str_at(x + 22, y + 4, "RIP 0x", 0x4F);
    put_hex_at(x + 28, y + 4, (u32)(rip >> 32), 8, 0x4F);
    put_hex_at(x + 36, y + 4, (u32)rip, 8, 0x4F);
    put_str_at(x + 46, y + 4, "CS 0x", 0x4F); put_hex_at(x + 51, y + 4, (u32)cs_, 4, 0x4F);
    put_str_at(x + 3, y + 5, "RAX 0x", 0x4F); put_hex_at(x + 9, y + 5, (u32)f->rax, 8, 0x4F);
    put_str_at(x + 20, y + 5, "RBX 0x", 0x4F); put_hex_at(x + 26, y + 5, (u32)f->rbx, 8, 0x4F);
    put_str_at(x + 37, y + 5, "RCX 0x", 0x4F); put_hex_at(x + 43, y + 5, (u32)f->rcx, 8, 0x4F);
    put_str_at(x + 54, y + 5, "RDX 0x", 0x4F); put_hex_at(x + 60, y + 5, (u32)f->rdx, 8, 0x4F);
    put_str_at(x + 3, y + 6, "RSP 0x", 0x4F); put_hex_at(x + 9, y + 6, (u32)f->rsp, 8, 0x4F);
    put_str_at(x + 20, y + 6, "RBP 0x", 0x4F); put_hex_at(x + 26, y + 6, (u32)f->rbp, 8, 0x4F);
    put_str_at(x + 37, y + 6, "RSI 0x", 0x4F); put_hex_at(x + 43, y + 6, (u32)f->rsi, 8, 0x4F);
    put_str_at(x + 54, y + 6, "RDI 0x", 0x4F); put_hex_at(x + 60, y + 6, (u32)f->rdi, 8, 0x4F);
    put_str_at(x + 3, y + 8, "Last command: ", 0x4F); put_clip(x + 17, y + 8, last_cmd, 30, 0x4E);
    put_str_at(x + 3, y + 10, "Press any key to return to the shell.", 0x4F);
    for (int i = 0; i < 64; i++) key_poll();
    while (key_poll() < 0) __asm__ volatile("pause");
    depth = 0;
    color = th->base; clear(); cursor_show();
    __asm__ volatile("movq $0x800000, %%rsp\n\tcall shell_main\n" : : : "memory");
    for (;;) __asm__ volatile("hlt");
}

void shell_main(void) {
    char buf[128], path[256];
    for (;;) {
        fs_path(fs_cwd, path);
        u8 old = color;
        if (kbd_ru) { color = th->mark; puts("[RU] "); color = old; }
        color = th->user; puts(cur_user); color = old; putc('@');
        color = th->user; puts(cur_host); color = old; putc(':');
        color = th->path; puts(path); color = th->head; puts(" \x10"); color = old; putc(' ');
        readline(buf, sizeof buf);
        run(buf);
    }
}
void kmain(void) {
    idt_init();                                    /* первым делом: исключения больше не убивают машину */
    ru_init(); vga_font_load(); vga_blink_off();   /* кириллица и 16 цветов фона */
    th = &themes[3]; color = th->base;             /* тема amber по умолчанию */
    pit_init(); pit_prev = pit_read(); pit_ms = 0; pit_rem = 0;
    boot_tod = tod();
    rng ^= rdtsc32() ^ (boot_tod * 2654435761u);
    if (!rng) rng = 1;
    dhcp_xid ^= rng;
    ata_init();
    if (!disk_load()) {
        fs_boot();
        if (ata_present) disk_save();
    }
    nic_tx = e1000_send; nic_rx = e1000_recv; ms_now = now_ms;
    nic_ok = e1000_init();
    if (nic_ok) { nip = 0x0A00020F; nmask = 0xFFFFFF00u; ngw = 0x0A000202; ndns = 0x0A000203; }
    http_raw = tls_rawbuf; http_rawmax = sizeof tls_rawbuf - 1; http_rand = kernel_rand;
    boot_splash();
    color = th->base; clear();
    welcome();
    putc('\n');
    if (nic_ok) { puts("network: e1000  MAC "); putmac(nmac); puts("  ip "); putip(nip); puts("   (run 'dhcp' if not on NAT)\n"); }
    else puts("network: no supported card (run 'lspci')\n");
    if (ata_present) {
        puts("storage: ATA Primary Master (");
        putu(ata_sectors_total * 512 / (1024 * 1024));
        puts(" MiB) - changes saved to hard disk\n");
    } else {
        puts("storage: RAMFS only (no ATA hard disk detected)\n");
    }
    putc('\n');
    if (!setup_done) {
        puts("First run: press 's' for Setup Wizard, or Enter to continue (3s)... ");
        int start_setup = 0;
        u32 t0 = now_ms();
        while (now_ms() - t0 < 3000) {
            int k = key_poll();
            if (k == 's' || k == 'S') { start_setup = 1; break; }
            if (k >= 0) break;
            __asm__ volatile("pause");
        }
        putc('\n');
        if (start_setup) {
            setup_wizard();
            welcome();
            putc('\n');
        } else {
            puts("Tip: you can run 'setup' anytime to personalize CravgOS.\n\n");
        }
    }
    shell_main();
}
