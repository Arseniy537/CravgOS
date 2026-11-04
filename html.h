/* html.h - HTML -> текстовая страница 80 колонок с цветами и ссылками. Чистый C. */
#ifndef HTML_H
#define HTML_H
#ifndef XBSS
#define XBSS
#endif
#define PG_W    80
#define PG_MAXL 2400
#define PG_MAXLK 600
#define LK_LEN  160

/* атрибуты, которыми рисует рендерер (темы могут их подменять при показе) */
#define PA_TEXT 0x07
#define PA_LINK 0x0B
#define PA_HEAD1 0x0E
#define PA_HEAD2 0x0F
#define PA_BOLD 0x0F
#define PA_DIM  0x08
#define PA_BULLET 0x0A

static u8  pg_ch[PG_MAXL][PG_W] XBSS;
static u8  pg_at[PG_MAXL][PG_W] XBSS;
static u16 pg_lk[PG_MAXL][PG_W] XBSS;
static u8  pg_len[PG_MAXL] XBSS;
static u8  pg_im[PG_MAXL] XBSS;                          /* 1 - строка занята картинкой (атрибуты не меняются темой) */
#define IMG_MAX 6
#define IMG_ROWS 12
static char img_src[IMG_MAX][LK_LEN], img_alt[IMG_MAX][40];
static int img_line[IMG_MAX], img_cnt;
static u16 img_lk[IMG_MAX];
static char lk_url[PG_MAXLK][LK_LEN] XBSS;
static int pg_lines, lk_count;
static char pg_title[80];

static int rl, rc, psp, rpre, rskip, rtitle, rfull, rbold, rhd;
static u8 ra;
static u16 rlk;
static char wch[PG_W + 1];
static u8 wat[PG_W + 1];
static u16 wlk[PG_W + 1];
static int wn;

static void r_line_init(int l) {
    for (int x = 0; x < PG_W; x++) { pg_ch[l][x] = ' '; pg_at[l][x] = PA_TEXT; pg_lk[l][x] = 0; }
    pg_len[l] = 0; pg_im[l] = 0;
}
static void r_newline(void) {
    if (rl + 1 >= PG_MAXL) { rfull = 1; return; }
    pg_len[rl] = rc;
    rl++; r_line_init(rl); rc = 0; pg_lines = rl + 1;
}
static void r_put(char c, u8 at, u16 lk) {
    if (rfull) return;
    if (rc >= PG_W) { r_newline(); if (rfull) return; }
    pg_ch[rl][rc] = (u8)c; pg_at[rl][rc] = at; pg_lk[rl][rc] = lk;
    rc++; pg_len[rl] = rc;
}
static void r_flush(void) {
    if (!wn) return;
    if (!rpre) {
        int need = wn + ((psp && rc > 0) ? 1 : 0);
        if (rc + need > PG_W && rc > 0) { r_newline(); psp = 0; }
        if (psp && rc > 0) { u16 sl = (pg_lk[rl][rc - 1] == wlk[0]) ? wlk[0] : 0; r_put(' ', sl ? PA_LINK : PA_TEXT, sl); }
        psp = 0;
    }
    for (int i = 0; i < wn; i++) r_put(wch[i], wat[i], wlk[i]);
    wn = 0;
}
static void r_break(int blank) {
    r_flush(); psp = 0;
    if (rc > 0) r_newline();
    if (blank && rl > 0 && rc == 0 && pg_len[rl - 1] > 0) r_newline();
}
static void r_text(char c) {
    if (rskip) return;
    if (rtitle) {
        int n = 0; while (pg_title[n]) n++;
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        if (c == ' ' && (n == 0 || pg_title[n - 1] == ' ')) return;
        if (n < 78) { pg_title[n] = c; pg_title[n + 1] = 0; }
        return;
    }
    if (rpre) {
        if (c == '\n') r_newline();
        else if (c == '\t') { for (int i = 0; i < 4; i++) r_put(' ', ra, rlk); }
        else if (c != '\r') r_put(c, ra, rlk);
        return;
    }
    if (c == ' ' || c == '\n' || c == '\t' || c == '\r') { r_flush(); psp = 1; return; }
    if (wn >= PG_W) r_flush();
    wch[wn] = c; wat[wn] = ra; wlk[wn] = rlk; wn++;
}
static void r_str(const char *s) { while (*s) r_text(*s++); }

static u8 r_style(void) { return rlk ? PA_LINK : (rhd == 1 ? PA_HEAD1 : (rhd ? PA_HEAD2 : (rbold ? PA_BOLD : PA_TEXT))); }

static int strcmp_(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (u8)*a - (u8)*b; }
/* кириллица выводится в кодовой странице CP866 (для неё в ядре загружается шрифт) */
static u8 uni_to_866(u32 cp) {
    if (cp >= 0x410 && cp <= 0x42F) return 0x80 + (cp - 0x410);
    if (cp >= 0x430 && cp <= 0x43F) return 0xA0 + (cp - 0x430);
    if (cp >= 0x440 && cp <= 0x44F) return 0xE0 + (cp - 0x440);
    switch (cp) {
        case 0x401: return 0xF0; case 0x451: return 0xF1; case 0x404: return 0xF2; case 0x454: return 0xF3;
        case 0x407: return 0xF4; case 0x457: return 0xF5; case 0x40E: return 0xF6; case 0x45E: return 0xF7;
        case 0x406: return 'I'; case 0x456: return 'i'; case 0x490: return 0x83; case 0x491: return 0xA3;
    }
    return 0;
}
#define CS_UTF8 0
#define CS_1251 1
#define CS_KOI8 2
#define CS_866  3
#define CS_LAT1 4
static int html_charset;
static u32 sb_to_uni(int cs, u8 b) {                 /* однобайтовая кодировка -> Unicode */
    if (b < 0x80) return b;
    if (cs == CS_LAT1) return b;
    if (cs == CS_1251) {
        if (b >= 0xC0) return 0x410 + (b - 0xC0);
        switch (b) {
            case 0xA8: return 0x401; case 0xB8: return 0x451; case 0xAA: return 0x404; case 0xBA: return 0x454;
            case 0xAF: return 0x407; case 0xBF: return 0x457; case 0xA1: return 0x40E; case 0xA2: return 0x45E;
            case 0xB2: return 0x406; case 0xB3: return 0x456; case 0xA5: return 0x490; case 0xB4: return 0x491;
            case 0x93: return 0x201C; case 0x94: return 0x201D; case 0x91: return 0x2018; case 0x92: return 0x2019;
            case 0x96: return 0x2013; case 0x97: return 0x2014; case 0x85: return 0x2026; case 0x95: return 0x2022;
            case 0xAB: return 171; case 0xBB: return 187; case 0xA0: return 160; case 0xB9: return 0x2116;
            case 0xA9: return 169; case 0xAE: return 174;
        }
        return '?';
    }
    if (cs == CS_KOI8) {
        static const u16 lo[32] = {0x44E,0x430,0x431,0x446,0x434,0x435,0x444,0x433,0x445,0x438,0x439,0x43A,0x43B,0x43C,0x43D,0x43E,
                                   0x43F,0x44F,0x440,0x441,0x442,0x443,0x436,0x432,0x44C,0x44B,0x437,0x448,0x44D,0x449,0x447,0x44A};
        if (b >= 0xC0 && b <= 0xDF) return lo[b - 0xC0];
        if (b >= 0xE0) return lo[b - 0xE0] - 0x20;
        if (b == 0xA3) return 0x451;
        if (b == 0xB3) return 0x401;
        return '?';
    }
    return '?';
}
static int cs_from_name(const char *n, int len) {
    char t[24]; int k = 0;
    for (int i = 0; i < len && k < 23; i++) { char c = n[i]; if (c >= 'A' && c <= 'Z') c += 32; t[k++] = c; }
    t[k] = 0;
    static const struct { const char *n; int cs; } m[] = {
        {"utf-8", CS_UTF8}, {"utf8", CS_UTF8}, {"windows-1251", CS_1251}, {"cp1251", CS_1251}, {"win-1251", CS_1251}, {"x-cp1251", CS_1251},
        {"koi8-r", CS_KOI8}, {"koi8-u", CS_KOI8}, {"koi8r", CS_KOI8}, {"cp866", CS_866}, {"ibm866", CS_866}, {"866", CS_866},
        {"iso-8859-1", CS_LAT1}, {"latin1", CS_LAT1}, {"windows-1252", CS_LAT1}, {"us-ascii", CS_LAT1}};
    for (unsigned i = 0; i < sizeof m / sizeof *m; i++) if (!strcmp_(t, m[i].n)) return m[i].cs;
    return -1;
}
static int html_valid_utf8(const char *s, int len, int *high) {
    int hi = 0;
    for (int i = 0; i < len; i++) {
        u8 c = (u8)s[i];
        if (c < 0x80) continue;
        hi = 1;
        int n = (c >= 0xF0 && c < 0xF8) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC2 && c < 0xE0) ? 1 : -1;
        if (n < 0 || c >= 0xF8) { *high = hi; return 0; }
        for (int k = 1; k <= n; k++) if (i + k >= len || ((u8)s[i + k] & 0xC0) != 0x80) { *high = hi; return 0; }
        i += n;
    }
    *high = hi;
    return 1;
}
/* кодировка страницы: заголовок Content-Type, затем <meta>, затем эвристика (fallback - для не-UTF-8 текста) */
static int html_detect_charset(const char *body, int len, const char *ctype, int fallback) {
    const char *src[2] = {ctype, 0};
    for (int pass = 0; pass < 2; pass++) {
        const char *p = src[pass]; int pl = 0;
        if (pass == 0) { if (!p) continue; pl = 0; while (p[pl]) pl++; }
        else { p = body; pl = len < 2048 ? len : 2048; }
        for (int i = 0; i + 8 < pl; i++) {
            if ((p[i] | 32) == 'c' && (p[i + 1] | 32) == 'h' && (p[i + 2] | 32) == 'a' && (p[i + 3] | 32) == 'r' && (p[i + 4] | 32) == 's' && (p[i + 5] | 32) == 'e' && (p[i + 6] | 32) == 't') {
                int j = i + 7;
                while (j < pl && (p[j] == ' ' || p[j] == '=')) j++;
                if (j < pl && (p[j] == '"' || p[j] == '\'')) j++;
                int e = j;
                while (e < pl && p[e] != '"' && p[e] != '\'' && p[e] != ' ' && p[e] != ';' && p[e] != '>' && p[e] != '/' && p[e] != '\r' && p[e] != '\n') e++;
                int cs = cs_from_name(p + j, e - j);
                if (cs >= 0) return cs;
            }
        }
    }
    int hi;
    if (html_valid_utf8(body, len, &hi)) return CS_UTF8;
    return hi ? fallback : CS_UTF8;
}
static const char *cp_str(u32 cp, char *tmp) {
    if (cp >= 128) {
        u8 b = uni_to_866(cp);
        if (b) { tmp[0] = (char)b; tmp[1] = 0; return tmp; }
    }
    if (cp < 128) { tmp[0] = (char)cp; tmp[1] = 0; return tmp; }
    switch (cp) {
        case 160: return " ";
        case 169: return "(c)"; case 174: return "(R)"; case 8482: return "(tm)";
        case 8211: case 8212: return "-";
        case 8216: case 8217: return "'";
        case 8220: case 8221: return "\"";
        case 8226: case 183: return "*";
        case 8230: return "...";
        case 171: case 187: return "\"";
        case 0x2116: return "No";
        case 8594: return "->"; case 8592: return "<-";
    }
    return "?";
}
static const char *ent_str(const char *n, int len) {
    static const struct { const char *k; const char *v; } t[] = {
        {"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"quot", "\""}, {"apos", "'"}, {"nbsp", " "},
        {"copy", "(c)"}, {"reg", "(R)"}, {"trade", "(tm)"}, {"mdash", "-"}, {"ndash", "-"},
        {"hellip", "..."}, {"laquo", "\""}, {"raquo", "\""}, {"rsquo", "'"}, {"lsquo", "'"},
        {"ldquo", "\""}, {"rdquo", "\""}, {"bull", "*"}, {"middot", "*"}, {"rarr", "->"}, {"larr", "<-"}
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        int k = 0;
        while (t[i].k[k] && k < len && t[i].k[k] == n[k]) k++;
        if (!t[i].k[k] && k == len) return t[i].v;
    }
    return 0;
}

/* ищет значение атрибута name="..." внутри тега [s, e) */
static int tag_attr(const char *s, const char *e, const char *name, char *out, int max) {
    int nl = 0; while (name[nl]) nl++;
    const char *p = s;
    while (p < e && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;      /* имя тега */
    while (p < e) {
        while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '/')) p++;
        const char *ns = p;
        while (p < e && *p != '=' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != '>' && *p != '/') p++;
        int ln = p - ns;
        while (p < e && (*p == ' ' || *p == '\t')) p++;
        int hasv = 0; const char *vs = p, *ve = p;
        if (p < e && *p == '=') {
            hasv = 1; p++;
            while (p < e && (*p == ' ' || *p == '\t')) p++;
            if (p < e && (*p == '"' || *p == '\'')) {
                char q = *p++; vs = p;
                while (p < e && *p != q) p++;
                ve = p; if (p < e) p++;
            } else { vs = p; while (p < e && *p != ' ' && *p != '\t' && *p != '\n' && *p != '>') p++; ve = p; }
        }
        if (ln == nl) {
            int same = 1;
            for (int i = 0; i < ln; i++) if ((ns[i] | 32) != name[i]) { same = 0; break; }
            if (same && hasv) {
                int k = 0;
                for (const char *q = vs; q < ve && k < max - 1; q++) out[k++] = *q;
                out[k] = 0;
                return 1;
            }
        }
    }
    return 0;
}

static int name_is(const char *n, int len, const char *w) {
    int i = 0;
    while (w[i]) { if (i >= len || (n[i] | 32) != w[i]) return 0; i++; }
    return i == len;
}

static void r_tag(int close, const char *n, int len, const char *ts, const char *te) {
    if (rskip) {
        if (close && (name_is(n, len, "script") || name_is(n, len, "style"))) rskip = 0;
        return;
    }
    if (name_is(n, len, "script") || name_is(n, len, "style")) { if (!close) rskip = 1; return; }
    if (name_is(n, len, "title")) { rtitle = !close; return; }
    if (name_is(n, len, "br")) { r_flush(); psp = 0; r_newline(); return; }
    if (name_is(n, len, "p") || name_is(n, len, "ul") || name_is(n, len, "ol") || name_is(n, len, "table") || name_is(n, len, "blockquote")) { r_break(1); return; }
    if (name_is(n, len, "div") || name_is(n, len, "section") || name_is(n, len, "article") || name_is(n, len, "header") ||
        name_is(n, len, "footer") || name_is(n, len, "nav") || name_is(n, len, "main") || name_is(n, len, "form") ||
        name_is(n, len, "tr") || name_is(n, len, "dl") || name_is(n, len, "dt") || name_is(n, len, "dd") || name_is(n, len, "aside")) { r_break(0); return; }
    if (len == 2 && (n[0] | 32) == 'h' && n[1] >= '1' && n[1] <= '6') {
        r_break(1);
        if (close) { rhd = 0; } else { rhd = (n[1] <= '2') ? 1 : 2; }
        ra = r_style();
        return;
    }
    if (name_is(n, len, "li")) {
        if (!close) { r_break(0); r_put('*', PA_BULLET, 0); r_put(' ', PA_TEXT, 0); }
        return;
    }
    if (name_is(n, len, "a")) {
        if (close) { rlk = 0; ra = r_style(); return; }
        char href[LK_LEN];
        if (tag_attr(ts, te, "href", href, sizeof href) && href[0] && lk_count < PG_MAXLK - 1) {
            lk_count++;
            int k = 0; while (href[k]) { lk_url[lk_count][k] = href[k]; k++; }
            lk_url[lk_count][k] = 0;
            rlk = lk_count; ra = PA_LINK;
        }
        return;
    }
    if (name_is(n, len, "b") || name_is(n, len, "strong")) { rbold = !close; ra = r_style(); return; }
    if (name_is(n, len, "pre")) { r_break(1); rpre = !close; r_flush(); r_break(1); return; }
    if (name_is(n, len, "hr")) {
        r_break(0);
        for (int i = 0; i < PG_W - 2; i++) r_put('-', PA_DIM, 0);
        r_break(0);
        return;
    }
    if (name_is(n, len, "img")) {
        char alt[60], src[LK_LEN];
        int ha = tag_attr(ts, te, "alt", alt, sizeof alt) && alt[0];
        if (!rskip && !rtitle && !rpre && img_cnt < IMG_MAX && rl + IMG_ROWS + 3 < PG_MAXL && !rfull &&
            (tag_attr(ts, te, "src", src, sizeof src) || tag_attr(ts, te, "data-src", src, sizeof src)) && src[0] && !ci_prefix(src, "data:")) {
            r_break(0);
            int k = img_cnt++, i = 0;
            while (src[i] && i < LK_LEN - 1) { img_src[k][i] = src[i]; i++; }
            img_src[k][i] = 0;
            for (i = 0; ha && alt[i] && i < 39; i++) img_alt[k][i] = alt[i];
            img_alt[k][ha ? i : 0] = 0;
            img_lk[k] = rlk; img_line[k] = rl;
            r_put('[', PA_DIM, rlk);
            if (ha) for (i = 0; img_alt[k][i] && rc < PG_W - 2; i++) r_put(img_alt[k][i], PA_DIM, rlk);
            else { const char *w = "image"; while (*w) r_put(*w++, PA_DIM, rlk); }
            r_put(']', PA_DIM, rlk);
            for (i = 0; i < IMG_ROWS; i++) r_newline();
            return;
        }
        if (ha) { r_text('['); r_str(alt); r_text(']'); }
        return;
    }
    if (name_is(n, len, "td") || name_is(n, len, "th")) { r_flush(); psp = 1; return; }
}

/* убирает из зарезервированного под картинку блока строки [line+keep, line+IMG_ROWS) */
static void html_img_shrink(int k, int keep) {
    int a = img_line[k] + keep, b = img_line[k] + IMG_ROWS, d = b - a;
    if (d <= 0) return;
    for (int l = a; l + d < pg_lines; l++) {
        for (int x = 0; x < PG_W; x++) { pg_ch[l][x] = pg_ch[l + d][x]; pg_at[l][x] = pg_at[l + d][x]; pg_lk[l][x] = pg_lk[l + d][x]; }
        pg_len[l] = pg_len[l + d]; pg_im[l] = pg_im[l + d];
    }
    pg_lines -= d;
    for (int j = k + 1; j < img_cnt; j++) img_line[j] -= d;
}

static void html_render(const char *s, int len, int plain) {
    rl = 0; rc = 0; psp = 0; rpre = plain; rskip = 0; rtitle = 0; rfull = 0; rbold = 0; rhd = 0; rlk = 0; ra = PA_TEXT; wn = 0;
    pg_lines = 1; lk_count = 0; pg_title[0] = 0; img_cnt = 0;
    r_line_init(0);
    int i = 0;
    while (i < len && !rfull) {
        char c = s[i];
        if (plain && (u8)c < 0x80) { r_text(c); i++; continue; }
        if (c == '<') {
            char c1 = (i + 1 < len) ? s[i + 1] : 0;
            if (c1 == '!' && i + 3 < len && s[i + 2] == '-' && s[i + 3] == '-') {
                int j = i + 4;
                while (j + 2 < len && !(s[j] == '-' && s[j + 1] == '-' && s[j + 2] == '>')) j++;
                i = (j + 2 < len) ? j + 3 : len;
                continue;
            }
            if ((c1 >= 'a' && c1 <= 'z') || (c1 >= 'A' && c1 <= 'Z') || c1 == '/' || c1 == '!' || c1 == '?') {
                int j = i + 1; char q = 0;
                while (j < len && (q || s[j] != '>')) { if (q) { if (s[j] == q) q = 0; } else if (s[j] == '"' || s[j] == '\'') q = s[j]; j++; }
                if (c1 == '!' || c1 == '?') { i = (j < len) ? j + 1 : len; continue; }
                int close = (c1 == '/');
                int ns = i + 1 + close, ne = ns;
                while (ne < j && ((s[ne] >= 'a' && s[ne] <= 'z') || (s[ne] >= 'A' && s[ne] <= 'Z') || (s[ne] >= '0' && s[ne] <= '9'))) ne++;
                r_tag(close, s + ns, ne - ns, s + ns, s + j);
                i = (j < len) ? j + 1 : len;
                continue;
            }
        }
        if (rskip) { i++; continue; }
        if (c == '&') {
            int j = i + 1;
            while (j < len && j - i < 12 && s[j] != ';' && s[j] != ' ' && s[j] != '<' && s[j] != '&') j++;
            if (j < len && s[j] == ';' && j > i + 1) {
                char tmp[2]; const char *rep = 0;
                if (s[i + 1] == '#') {
                    u32 v = 0; int k = i + 2, hex = 0;
                    if (k < j && (s[k] | 32) == 'x') { hex = 1; k++; }
                    int ok = k < j;
                    for (; k < j; k++) {
                        char d = s[k]; int dv;
                        if (d >= '0' && d <= '9') dv = d - '0';
                        else if (hex && (d | 32) >= 'a' && (d | 32) <= 'f') dv = (d | 32) - 'a' + 10;
                        else { ok = 0; break; }
                        v = v * (hex ? 16 : 10) + dv;
                    }
                    if (ok) rep = cp_str(v, tmp);
                } else rep = ent_str(s + i + 1, j - i - 1);
                if (rep) { r_str(rep); i = j + 1; continue; }
            }
            r_text('&'); i++; continue;
        }
        if ((u8)c >= 0x80 && html_charset != CS_UTF8) {
            if (html_charset == CS_866) { char t1[2] = {c, 0}; r_str(t1); i++; continue; }
            char tmp[2];
            r_str(cp_str(sb_to_uni(html_charset, (u8)c), tmp));
            i++;
            continue;
        }
        if ((u8)c >= 0x80) {
            u8 b = (u8)c; int nb = (b >= 0xF0) ? 3 : (b >= 0xE0) ? 2 : (b >= 0xC0) ? 1 : 0;
            if (nb == 0) { r_text('?'); i++; continue; }
            u32 cp = b & (0x3F >> nb);
            int k = 1;
            while (k <= nb && i + k < len && ((u8)s[i + k] & 0xC0) == 0x80) { cp = (cp << 6) | ((u8)s[i + k] & 0x3F); k++; }
            char tmp[2];
            r_str(cp_str(cp, tmp));
            i += k;
            continue;
        }
        r_text(c); i++;
    }
    r_flush();
    pg_len[rl] = rc;
    pg_lines = rl + 1;
    while (pg_lines > 1 && pg_len[pg_lines - 1] == 0) pg_lines--;
    html_charset = CS_UTF8;                                  /* кодировка действует на один вызов */
}
#endif
