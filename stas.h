/* stas.h - пакетный менеджер stas и мини-язык скриптов пакетов. Чистый C, без libc.
 *
 * Пакет = один текстовый файл <имя>.stp (до 2048 байт, лимит RAMFS) на сервере репозитория.
 * Репозиторий = обычный HTTP(S)-каталог с файлами:
 *     index.txt        строки вида:  ИМЯ SHA256(64 hex) РАЗМЕР описание...
 *     <имя>.stp        сами пакеты (скрипты на языке stas, см. ниже)
 * Установка: скачать, проверить размер и SHA-256 по index.txt, сохранить в /apps/<имя>.
 * После установки пакет запускается как команда: просто <имя> (или "stas run <имя>").
 *
 * Язык скриптов (одна инструкция в строке, переменные a..z - целые числа):
 *     # комментарий           :метка
 *     say ТЕКСТ               печать со строкой ($x - значение переменной, $$ - знак $, $_ - пробел в конце)
 *     put ТЕКСТ               то же без перевода строки
 *     ask x                   ввод числа с клавиатуры (не число -> -1)
 *     let x = A               let x = A + B   (+ - * / %)      let x = rnd N  (1..N)
 *     if A op B goto метка    op: = != < > <= >=
 *     goto метка | pause МС | cls | end
 * Скрипт не может писать файлы и ходить в сеть: пакет не опаснее игры. Esc прерывает работу.
 */
#ifndef STAS_H
#define STAS_H
#define STAS_REPO_DEFAULT "https://raw.githubusercontent.com/Arseniy537/CravgOS/main/stas-repo"
#define STAS_DIR "/apps"
#define STAS_MAXL 160
#define STAS_STEPS 300000u

static const char *stas_err;
/* получить файл по URL: 0 - успех (body/len указывают в буфер), <0 - ошибка (stas_err) */
static int stas_http_fetch(const char *url, const char **body, int *len);
static int (*stas_fetch)(const char *url, const char **body, int *len) = stas_http_fetch;

#ifndef STAS_NO_HTTP
static int stas_http_fetch(const char *url, const char **body, int *len) {
    if (!net_ready()) { stas_err = "no network"; return -1; }
    http_buf = http_data; http_max = sizeof http_data - 1; http_abort = br_abort;
    br_res.url[0] = 0;
    int r = http_get_follow(url, &br_res);
    http_abort = 0;
    if (r < 0) { stas_err = http_err_str(r); return -1; }
    if (br_res.status != 200) { stas_err = "server did not answer 200 OK"; return -1; }
    if (br_res.truncated) { stas_err = "reply is too large"; return -1; }
    *body = (const char *)http_data + br_res.body_off; *len = br_res.body_len;
    return 0;
}
#else
static int stas_http_fetch(const char *url, const char **body, int *len) { (void)url; (void)body; (void)len; stas_err = "no http"; return -1; }
#endif

/* ---------- Вспомогательное ---------- */
static void stas_cat(char *d, int *n, int max, const char *s) {
    while (*s && *n < max - 1) d[(*n)++] = *s++;
    d[*n] = 0;
}
static int stas_okname(const char *s) {            /* a-z 0-9 - _ , до 15 символов */
    int n = 0;
    for (; s[n]; n++) {
        char c = s[n];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return 0;
    }
    return n > 0 && n < FS_NAME;
}
static void stas_mkdir(void) { if (fs_resolve(STAS_DIR) < 0) fs_create(STAS_DIR, 1); }
static int stas_node(const char *name) {           /* номер файла /apps/<name> или -1 */
    char p[40]; int n = 0; p[0] = 0;
    stas_cat(p, &n, sizeof p, STAS_DIR "/"); stas_cat(p, &n, sizeof p, name);
    int i = fs_resolve(p);
    return (i > 0 && !fsn[i].dir) ? i : -1;
}
static int stas_put(const char *name, const char *d, int len) {
    stas_mkdir();
    char p[40]; int n = 0; p[0] = 0;
    stas_cat(p, &n, sizeof p, STAS_DIR "/"); stas_cat(p, &n, sizeof p, name);
    int i = fs_resolve(p);
    if (i < 0) i = fs_create(p, 0);
    if (i < 0) return i;
    if (fsn[i].dir) return FS_EISDIR;
    if (len > FS_FSZ) return FS_EBAD;
    fs_write(i, d, len, 0);
    fs_sync();
    return 0;
}
static void stas_puti(int v) {
    long long x = v;
    if (x < 0) { putc('-'); x = -x; }
    putu((u32)x);
}
static void stas_repo_get(char *out, int max) {
    int i = stas_node(".repo"), n = 0;
    if (i >= 0)
        for (int k = 0; k < fsn[i].size && n < max - 1; k++) {
            char c = (char)fsdata[i][k];
            if (c == '\n' || c == '\r') break;
            out[n++] = c;
        }
    if (n == 0) { const char *d = STAS_REPO_DEFAULT; while (*d && n < max - 1) out[n++] = *d++; }
    while (n > 0 && out[n - 1] == '/') n--;
    out[n] = 0;
}
static int stas_get(const char *file, const char **b, int *n) {
    char url[256], repo[200]; int k = 0; url[0] = 0;
    stas_repo_get(repo, sizeof repo);
    stas_cat(url, &k, sizeof url, repo); stas_cat(url, &k, sizeof url, "/"); stas_cat(url, &k, sizeof url, file);
    puts("GET "); puts(url); putc('\n');
    stas_err = "unknown error";
    int r = stas_fetch(url, b, n);
    if (r < 0) { puts("stas: download failed: "); puts(stas_err); putc('\n'); }
    return r;
}
static void stas_sha_hex(const u8 *d, int len, char *out) {
    struct sha256 c; u8 h[32];
    sha256_init(&c); sha256_update(&c, d, len); sha256_final(&c, h);
    for (int i = 0; i < 32; i++) { out[2 * i] = "0123456789abcdef"[h[i] >> 4]; out[2 * i + 1] = "0123456789abcdef"[h[i] & 15]; }
    out[64] = 0;
}
static int stas_hexeq(const char *a, const char *b) {
    for (int i = 0; i < 64; i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'F') x += 32;
        if (y >= 'A' && y <= 'F') y += 32;
        if (x != y) return 0;
    }
    return b[64] == 0;
}

/* ---------- Индекс ---------- */
struct stas_ent { char name[16]; char hash[65]; int size; const char *desc; int dlen; };
static int stas_idx_next(const u8 *d, int len, int *pos, struct stas_ent *e) {
    while (*pos < len) {
        int s = *pos, t = s;
        while (t < len && d[t] != '\n') t++;
        *pos = t < len ? t + 1 : t;
        int end = t; if (end > s && d[end - 1] == '\r') end--;
        int p = s, k = 0;
        while (p < end && d[p] != ' ' && k < 15) e->name[k++] = (char)d[p++];
        e->name[k] = 0;
        if (p >= end || d[p] != ' ' || !stas_okname(e->name)) continue;
        while (p < end && d[p] == ' ') p++;
        k = 0;
        while (p < end && d[p] != ' ' && k < 64) e->hash[k++] = (char)d[p++];
        e->hash[k] = 0;
        if (k != 64 || p >= end || d[p] != ' ') continue;
        while (p < end && d[p] == ' ') p++;
        int digits = 0; e->size = 0;
        while (p < end && d[p] >= '0' && d[p] <= '9') { if (e->size < 100000) e->size = e->size * 10 + (d[p] - '0'); p++; digits++; }
        if (!digits) continue;
        while (p < end && d[p] == ' ') p++;
        e->desc = (const char *)d + p; e->dlen = end - p;
        return 1;
    }
    return 0;
}
/* 1 - найден, 0 - нет в индексе, -1 - индекса нет (сообщение уже выведено) */
static int stas_find(const char *name, struct stas_ent *e) {
    int i = stas_node(".index");
    if (i < 0) { puts("stas: no package index yet. Run: stas update\n"); return -1; }
    int pos = 0;
    while (stas_idx_next(fsdata[i], fsn[i].size, &pos, e)) if (streq(e->name, name)) return 1;
    return 0;
}

/* ---------- Команды менеджера ---------- */
static void stas_update(void) {
    const char *b; int n;
    if (stas_get("index.txt", &b, &n) < 0) return;
    if (n <= 0) { puts("stas: the repository index is empty\n"); return; }
    if (n > FS_FSZ) { puts("stas: index is too big (max 2048 bytes)\n"); return; }
    int r = stas_put(".index", b, n);
    if (r < 0) { fs_err(r); return; }
    int i = stas_node(".index"), pos = 0, cnt = 0; struct stas_ent e;
    while (stas_idx_next(fsdata[i], fsn[i].size, &pos, &e)) cnt++;
    puts("Index updated: "); putu(cnt); puts(cnt == 1 ? " package\n" : " packages\n");
    if (!cnt) puts("stas: warning - no valid lines (NAME SHA256 SIZE description)\n");
}
static void stas_list(void) {
    int i = stas_node(".index");
    if (i < 0) { puts("No package index yet. Run: stas update\n"); return; }
    int pos = 0, cnt = 0; struct stas_ent e;
    while (stas_idx_next(fsdata[i], fsn[i].size, &pos, &e)) {
        cnt++;
        puts(stas_node(e.name) >= 0 ? "[x] " : "[ ] ");
        puts(e.name);
        for (int k = slen(e.name); k < 14; k++) putc(' ');
        putu(e.size); puts(" B  ");
        for (int k = 0; k < e.dlen && k < 50; k++) putc(e.desc[k]);
        putc('\n');
    }
    if (!cnt) puts("The index has no packages.\n");
    else puts("[x] = installed.   stas install NAME\n");
}
static void stas_info(const char *name) {
    struct stas_ent e; int f = stas_find(name, &e);
    if (f < 0) return;
    if (!f) { puts("stas: no such package: "); puts(name); putc('\n'); return; }
    puts("Name:      "); puts(e.name); putc('\n');
    puts("Size:      "); putu(e.size); puts(" bytes\n");
    puts("SHA-256:   "); puts(e.hash); putc('\n');
    puts("Info:      "); for (int k = 0; k < e.dlen; k++) putc(e.desc[k]); putc('\n');
    puts("Installed: "); puts(stas_node(name) >= 0 ? "yes\n" : "no\n");
}
static void stas_install(const char *name) {
    if (!stas_okname(name)) { puts("stas: bad package name (a-z 0-9 - _ , up to 15 characters)\n"); return; }
    struct stas_ent e; int f = stas_find(name, &e);
    if (f < 0) return;
    if (!f) { puts("stas: no such package in the index: "); puts(name); puts("   (try: stas update, stas list)\n"); return; }
    if (cmd_find(name)) { puts("stas: this name clashes with a built-in command\n"); return; }
    char fn[24]; int k = 0; fn[0] = 0;
    stas_cat(fn, &k, sizeof fn, name); stas_cat(fn, &k, sizeof fn, ".stp");
    const char *b; int n;
    if (stas_get(fn, &b, &n) < 0) return;
    if (n > FS_FSZ) { puts("stas: package is too big (max 2048 bytes)\n"); return; }
    if (n != e.size) { puts("stas: size differs from the index - package rejected\n"); return; }
    char hx[65]; stas_sha_hex((const u8 *)b, n, hx);
    if (!stas_hexeq(hx, e.hash)) {
        puts("stas: SHA-256 mismatch - package rejected\n  index: "); puts(e.hash); puts("\n  file:  "); puts(hx); putc('\n');
        return;
    }
    int r = stas_put(name, b, n);
    if (r < 0) { fs_err(r); return; }
    puts("Installed "); puts(name); puts(" ("); putu(n); puts(" B, sha256 ok). Run it: "); puts(name); putc('\n');
}
static void stas_remove(const char *name) {
    if (!stas_okname(name)) { puts("stas: bad package name\n"); return; }
    int i = stas_node(name);
    if (i < 0) { puts("stas: not installed: "); puts(name); putc('\n'); return; }
    int r = fs_remove(i);
    if (r < 0) { fs_err(r); return; }
    fs_sync();
    puts("Removed "); puts(name); putc('\n');
}

/* ---------- Интерпретатор скриптов ---------- */
static int stas_v[26];
static char stas_src[FS_FSZ + 1];
static char *stas_ln[STAS_MAXL];
static int stas_nl;

static int stas_load(int idx) {
    int n = fsn[idx].size;
    for (int i = 0; i < n; i++) stas_src[i] = (char)fsdata[idx][i];
    stas_src[n] = 0;
    stas_nl = 0;
    char *p = stas_src;
    for (;;) {
        char *s = p;
        while (*p && *p != '\n') p++;
        char *e = p; int last = (*p == 0);
        *e = 0;
        while (*s == ' ' || *s == '\t') s++;
        while (e > s && (e[-1] == ' ' || e[-1] == '\r' || e[-1] == '\t')) *--e = 0;
        if (stas_nl >= STAS_MAXL) return -1;
        stas_ln[stas_nl++] = s;
        if (last) break;
        p++;
    }
    return 0;
}
static int stas_clamp(long long v) { if (v > 1000000000LL) v = 1000000000LL; if (v < -1000000000LL) v = -1000000000LL; return (int)v; }
static int stas_isvar(const char *t) { return t[0] >= 'a' && t[0] <= 'z' && t[1] == 0; }
static int stas_val(const char *t, int *ok) {
    if (stas_isvar(t)) { *ok = 1; return stas_v[t[0] - 'a']; }
    int neg = 0, d = 0; long long n = 0;
    if (*t == '-') { neg = 1; t++; }
    while (*t >= '0' && *t <= '9') { if (n < 100000000LL) n = n * 10 + (*t - '0'); t++; d++; }
    *ok = (d > 0 && *t == 0);
    return stas_clamp(neg ? -n : n);
}
static int stas_label(const char *name) {
    for (int i = 0; i < stas_nl; i++) if (stas_ln[i][0] == ':' && streq(stas_ln[i] + 1, name)) return i;
    return -1;
}
static void stas_say(const char *s) {
    while (*s) {
        if (s[0] == '$' && s[1] == '$') { putc('$'); s += 2; }
        else if (s[0] == '$' && s[1] == '_') { putc(' '); s += 2; }
        else if (s[0] == '$' && s[1] >= 'a' && s[1] <= 'z') { stas_puti(stas_v[s[1] - 'a']); s += 2; }
        else putc(*s++);
    }
}
static void stas_fail(int lno, const char *m) { puts("stas: line "); putu(lno); puts(": "); puts(m); putc('\n'); }
#define STAS_ERR(m) do { stas_fail(lno, m); return; } while (0)

static void stas_exec(void) {
    for (int i = 0; i < 26; i++) stas_v[i] = 0;
    int pc = 0; u32 steps = 0;
    while (pc < stas_nl) {
        const char *ln = stas_ln[pc];
        int lno = ++pc;
        if (!*ln || *ln == '#' || *ln == ':') continue;
        if (++steps > STAS_STEPS) { puts("stas: step limit reached - script stopped\n"); return; }
        if ((steps & 255) == 0 && key_poll() == 27) { puts("stas: stopped (Esc)\n"); return; }
        char b[128]; int k = 0;
        while (ln[k] && k < 127) { b[k] = ln[k]; k++; }
        b[k] = 0;
        char *a = b, *cmd = tok(&a);
        while (*a == ' ') a++;
        if (streq(cmd, "say")) { stas_say(a); putc('\n'); }
        else if (streq(cmd, "put")) stas_say(a);
        else if (streq(cmd, "cls")) clear();
        else if (streq(cmd, "end")) return;
        else if (streq(cmd, "pause")) {
            char *t = tok(&a); int ok = 0, ms = t ? stas_val(t, &ok) : 0;
            if (!ok) STAS_ERR("pause needs milliseconds");
            if (ms > 5000) ms = 5000;
            if (ms > 0) delay_ms((u32)ms);
        }
        else if (streq(cmd, "goto")) {
            char *t = tok(&a); int l = t ? stas_label(t) : -1;
            if (l < 0) STAS_ERR("unknown label");
            pc = l;
        }
        else if (streq(cmd, "ask")) {
            char *t = tok(&a);
            if (!t || !stas_isvar(t)) STAS_ERR("ask needs a variable a-z");
            char in[16]; readline(in, sizeof in);
            const char *p = in; int neg = 0, d = 0; long long n = 0;
            while (*p == ' ') p++;
            if (*p == '-') { neg = 1; p++; }
            while (*p >= '0' && *p <= '9') { if (n < 100000000LL) n = n * 10 + (*p - '0'); p++; d++; }
            stas_v[t[0] - 'a'] = d ? stas_clamp(neg ? -n : n) : -1;
        }
        else if (streq(cmd, "let")) {
            char *x = tok(&a), *eq = tok(&a), *t1 = tok(&a);
            if (!x || !stas_isvar(x) || !eq || !streq(eq, "=") || !t1) STAS_ERR("let: let x = A [op B]  or  let x = rnd N");
            int ok; long long r;
            if (streq(t1, "rnd")) {
                char *t2 = tok(&a); int n = t2 ? stas_val(t2, &ok) : 0;
                if (!t2 || !ok || n < 1) STAS_ERR("rnd needs a positive number");
                r = 1 + (long long)(rnd() % (u32)n);
            } else {
                int A = stas_val(t1, &ok);
                if (!ok) STAS_ERR("bad number or variable");
                r = A;
                char *op = tok(&a);
                if (op) {
                    char *t2 = tok(&a); int B = t2 ? stas_val(t2, &ok) : 0;
                    if (!t2 || !ok || op[1]) STAS_ERR("let: bad expression");
                    switch (op[0]) {
                        case '+': r = (long long)A + B; break;
                        case '-': r = (long long)A - B; break;
                        case '*': r = (long long)A * B; break;
                        case '/': if (!B) STAS_ERR("division by zero"); r = A / B; break;
                        case '%': if (!B) STAS_ERR("division by zero"); r = A % B; break;
                        default: STAS_ERR("let: operator must be + - * / %");
                    }
                }
            }
            stas_v[x[0] - 'a'] = stas_clamp(r);
        }
        else if (streq(cmd, "if")) {
            char *t1 = tok(&a), *op = tok(&a), *t2 = tok(&a), *g = tok(&a), *lab = tok(&a);
            int ok1 = 0, ok2 = 0;
            if (!t1 || !op || !t2 || !g || !lab || !streq(g, "goto")) STAS_ERR("if: if A op B goto label");
            int A = stas_val(t1, &ok1), B = stas_val(t2, &ok2), c;
            if (!ok1 || !ok2) STAS_ERR("bad number or variable");
            if (streq(op, "=") || streq(op, "==")) c = A == B;
            else if (streq(op, "!=")) c = A != B;
            else if (streq(op, "<")) c = A < B;
            else if (streq(op, ">")) c = A > B;
            else if (streq(op, "<=")) c = A <= B;
            else if (streq(op, ">=")) c = A >= B;
            else STAS_ERR("if: operator must be = != < > <= >=");
            if (c) {
                int l = stas_label(lab);
                if (l < 0) STAS_ERR("unknown label");
                pc = l;
            }
        }
        else STAS_ERR("unknown statement");
    }
}
static void stas_run_idx(int idx) {
    if (stas_load(idx) < 0) { puts("stas: script is too long (max 160 lines)\n"); return; }
    stas_exec();
}
static void stas_run_name(const char *name) {
    if (!stas_okname(name)) { puts("stas: bad package name\n"); return; }
    int i = stas_node(name);
    if (i < 0) { puts("stas: not installed: "); puts(name); puts("   (stas install NAME)\n"); return; }
    stas_run_idx(i);
}
/* команда, неизвестная оболочке: вдруг это установленный пакет */
static int stas_try_app(const char *cmd) {
    if (!stas_okname(cmd)) return 0;
    int i = stas_node(cmd);
    if (i < 0) return 0;
    stas_run_idx(i);
    return 1;
}

static void cmd_stas(char *args) {
    char *sub = tok(&args), *a = tok(&args);
    if (!sub) {
        puts("stas - package manager (packages come from your own repository)\n"
             "  stas update            download the package index\n"
             "  stas list              show packages ([x] = installed)\n"
             "  stas info NAME         details of a package\n"
             "  stas install NAME      download, verify (SHA-256) and install\n"
             "  stas remove NAME       uninstall\n"
             "  stas run NAME          run (an installed package is also a command)\n"
             "  stas repo [URL|default]  show or set the repository URL\n");
        return;
    }
    if (streq(sub, "update")) stas_update();
    else if (streq(sub, "list") || streq(sub, "ls")) stas_list();
    else if (streq(sub, "info")) { if (a) stas_info(a); else puts("usage: stas info NAME\n"); }
    else if (streq(sub, "install") || streq(sub, "add")) { if (a) stas_install(a); else puts("usage: stas install NAME\n"); }
    else if (streq(sub, "remove") || streq(sub, "rm")) { if (a) stas_remove(a); else puts("usage: stas remove NAME\n"); }
    else if (streq(sub, "run")) { if (a) stas_run_name(a); else puts("usage: stas run NAME\n"); }
    else if (streq(sub, "repo")) {
        if (!a) { char r[200]; stas_repo_get(r, sizeof r); puts("Repository: "); puts(r); putc('\n'); }
        else if (streq(a, "default")) {
            int i = stas_node(".repo");
            if (i >= 0) { fs_remove(i); fs_sync(); }
            puts("Repository reset to the default.\n");
        } else if ((a[0] == 'h' && a[1] == 't' && a[2] == 't' && a[3] == 'p') && slen(a) < 190) {
            int r = stas_put(".repo", a, slen(a));
            if (r < 0) fs_err(r); else { puts("Repository set. Now run: stas update\n"); }
        } else puts("stas: the URL must start with http:// or https://\n");
    }
    else { puts("stas: unknown subcommand: "); puts(sub); puts("   (type 'stas' for help)\n"); }
}
#endif
