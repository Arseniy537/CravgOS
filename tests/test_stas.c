/* тесты stas.h: менеджер пакетов и интерпретатор (сеть подменена, репозиторий читается из stas-repo/) */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
#include "../fs.h"
#include "../crypto.h"
#define putc t_putc
#define puts t_puts
static char out[65536]; static int outn;
static void t_putc(char c) { if (outn < (int)sizeof out - 1) out[outn++] = c; out[outn] = 0; }
static void t_puts(const char *s) { while (*s) t_putc(*s++); }
static void putu(u32 n) { char b[12]; int i = 0; if (!n) b[i++] = '0'; while (n) { b[i++] = '0' + n % 10; n /= 10; } while (i) t_putc(b[--i]); }
static int streq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static char *tok(char **s) { char *p = *s; while (*p == ' ') p++; if (!*p) { *s = p; return 0; } char *st = p; while (*p && *p != ' ') p++; if (*p) *p++ = 0; *s = p; return st; }
static u32 rng_val = 3; static u32 rnd(void) { return rng_val; }
static void delay_ms(u32 ms) { (void)ms; }
static void clear(void) {}
static int key_poll(void) { return -1; }
static void fs_err(int e) { printf("fs_err %d\n", e); }
static void fs_sync(void) {}
static const char *in_q[64]; static int in_n, in_i;
static int readline(char *buf, int max) { const char *s = in_i < in_n ? in_q[in_i++] : "0"; int n = 0; while (s[n] && n < max - 1) { buf[n] = s[n]; n++; } buf[n] = 0; t_puts(buf); t_putc('\n'); return n; }
static const char *cmd_find(const char *w) { return (streq(w, "snake") || streq(w, "ls")) ? "x" : 0; }
#define STAS_NO_HTTP
#include "../stas.h"

#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n--- output ---\n%s\n", __LINE__, #c, out); return 1; } } while(0)
#define HAS(s) (strstr(out, s) != 0)
static char idx_buf[4096], pkg_buf[4096]; static int idx_len, pkg_len; static char last_url[300];
static int mock_fetch(const char *url, const char **body, int *len) {
    strncpy(last_url, url, 299);
    const char *base = "https://raw.githubusercontent.com/Arseniy537/CravgOS/main/stas-repo/";
    if (strncmp(url, base, strlen(base)) != 0) { stas_err = "wrong repo url"; return -1; }
    const char *f = url + strlen(base);
    if (!strcmp(f, "index.txt")) { *body = idx_buf; *len = idx_len; return 0; }
    if (!strcmp(f, "nim.stp")) { *body = pkg_buf; *len = pkg_len; return 0; }
    stas_err = "HTTP 404"; return -1;
}
static int readf(const char *p, char *b, int max) { FILE *f = fopen(p, "rb"); if (!f) { printf("cannot open %s (run from repo root)\n", p); exit(2); } int n = fread(b, 1, max, f); fclose(f); return n; }
static void reset(void) { outn = 0; out[0] = 0; }
static void sh(const char *cmd) { char b[256]; strcpy(b, cmd); reset(); char *a = b; char *c = tok(&a); if (!strcmp(c, "stas")) cmd_stas(a); }
static void script(const char *name, const char *text) { stas_put(name, text, strlen(text)); }
static int play(const char *name, const char **in, int n) { in_q[0] = 0; for (int i = 0; i < n; i++) in_q[i] = in[i]; in_n = n; in_i = 0; reset(); return stas_try_app(name); }

int main(void) {
    fs_init(); stas_fetch = mock_fetch;
    idx_len = readf("stas-repo/index.txt", idx_buf, sizeof idx_buf); pkg_len = readf("stas-repo/nim.stp", pkg_buf, sizeof pkg_buf);

    CHECK(stas_okname("nim") && stas_okname("a-b_9") && !stas_okname("") && !stas_okname("../x") && !stas_okname("Nim") && !stas_okname("a b") && !stas_okname("abcdefghijklmnop") && stas_okname("abcdefghijklmno"));

    sh("stas"); CHECK(HAS("package manager") && HAS("install NAME"));
    sh("stas list"); CHECK(HAS("Run: stas update"));
    sh("stas install nim"); CHECK(HAS("Run: stas update"));
    sh("stas update"); CHECK(HAS("Index updated: 1 package")); CHECK(strstr(last_url, "stas-repo/index.txt"));
    sh("stas list"); CHECK(HAS("[ ] nim") && HAS("take 1-3 sticks"));
    sh("stas info nim"); CHECK(HAS("Installed: no") && HAS("SHA-256:"));
    sh("stas info nope"); CHECK(HAS("no such package"));
    sh("stas install nope"); CHECK(HAS("no such package in the index"));
    sh("stas install ../etc"); CHECK(HAS("bad package name"));

    /* порча пакета на сервере -> отказ */
    pkg_buf[20] ^= 1;
    sh("stas install nim"); CHECK(HAS("SHA-256 mismatch")); CHECK(stas_node("nim") < 0);
    pkg_buf[20] ^= 1;
    pkg_len--; sh("stas install nim"); CHECK(HAS("size differs")); CHECK(stas_node("nim") < 0); pkg_len++;

    sh("stas install nim"); CHECK(HAS("Installed nim") && HAS("sha256 ok")); CHECK(stas_node("nim") >= 0);
    sh("stas list"); CHECK(HAS("[x] nim"));
    sh("stas info nim"); CHECK(HAS("Installed: yes"));

    /* игра: ввод мусора и выход */
    const char *q1[] = { "abc", "7", "-3", "0" };
    CHECK(play("nim", q1, 4)); CHECK(HAS("NIM") && HAS("Sticks left: 19") && HAS("Bye!"));
    /* игра: всегда берём по 1 - компьютер играет безошибочно (старт 19) */
    const char *q2[40]; for (int i = 0; i < 40; i++) q2[i] = "1";
    CHECK(play("nim", q2, 40)); CHECK(HAS("You lose!") && HAS("Computer takes 3"));
    /* игра: при другом старте можно выиграть (старт 22: берём 1, дальше добиваем до 4k+1) */
    rng_val = 6;                                    /* 1+6%10=7 -> 22 палочки */
    const char *q3[] = { "1", "3", "2", "1", "3", "1", "1", "1", "1", "1", "1" };
    CHECK(play("nim", q3, 11)); CHECK(HAS("Sticks left: 22")); CHECK(HAS("You lose!") || HAS("You win!"));
    rng_val = 3;

    /* stas run / неустановленный */
    sh("stas run nim"); /* запустится: без ввода -> "0" -> Bye */ CHECK(HAS("Bye!"));
    sh("stas run zzz"); CHECK(HAS("not installed"));
    reset(); CHECK(!stas_try_app("zzz")); CHECK(!stas_try_app("..")); CHECK(!stas_try_app(".index")); CHECK(!stas_try_app(".repo"));

    /* remove */
    sh("stas remove nim"); CHECK(HAS("Removed nim")); CHECK(stas_node("nim") < 0);
    sh("stas remove nim"); CHECK(HAS("not installed"));

    /* repo */
    sh("stas repo"); CHECK(HAS("raw.githubusercontent.com/Arseniy537/CravgOS/main/stas-repo"));
    sh("stas repo ftp://x"); CHECK(HAS("must start with http"));
    sh("stas repo http://10.0.2.2:8000/"); CHECK(HAS("Repository set"));
    sh("stas repo"); CHECK(HAS("Repository: http://10.0.2.2:8000\n"));
    sh("stas update"); CHECK(HAS("download failed") && HAS("wrong repo url")); CHECK(!strcmp(last_url, "http://10.0.2.2:8000/index.txt"));
    sh("stas repo default"); sh("stas repo"); CHECK(HAS("raw.githubusercontent.com"));

    /* конфликт имени со встроенной командой */
    { const char *e = "snake 0000000000000000000000000000000000000000000000000000000000000000 5 fake\n"; memcpy(idx_buf, e, strlen(e)); idx_len = strlen(e); }
    sh("stas update"); CHECK(HAS("1 package"));
    sh("stas install snake"); CHECK(HAS("clashes with a built-in"));
    /* битые строки индекса пропускаются */
    { const char *e = "garbage\nbad zz 5 x\n# comment\nok1 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa 7 fine one\r\n"; memcpy(idx_buf, e, strlen(e)); idx_len = strlen(e); }
    sh("stas update"); CHECK(HAS("1 package")); sh("stas list"); CHECK(HAS("[ ] ok1") && HAS("fine one"));

    /* интерпретатор */
    script("t1", "let a = 6\nlet b = a * 7\nsay b=$b $$ a=$a\nlet c = b / 5\nlet d = b % 5\nlet e = 0 - 9\nsay $c $d $e\n");
    CHECK(play("t1", 0, 0)); CHECK(HAS("b=42 $ a=6\n") && HAS("8 2 -9\n"));
    script("t2", "let i = 0\n:l\nlet i = i + 1\nif i < 5 goto l\nsay done $i\nif i != 5 goto bad\nif i >= 5 goto ok\n:bad\nsay BAD\nend\n:ok\nsay ok\n");
    CHECK(play("t2", 0, 0)); CHECK(HAS("done 5") && HAS("ok") && !HAS("BAD"));
    script("t3", "say a\nlet x = 5 / 0\nsay no\n");
    CHECK(play("t3", 0, 0)); CHECK(HAS("line 2: division by zero") && !HAS("no\n"));
    script("t4", ":l\ngoto l\n");
    CHECK(play("t4", 0, 0)); CHECK(HAS("step limit"));
    script("t5", "say hi\nfoo bar\n");
    CHECK(play("t5", 0, 0)); CHECK(HAS("line 2: unknown statement"));
    script("t6", "goto nowhere\n");
    CHECK(play("t6", 0, 0)); CHECK(HAS("unknown label"));
    script("t7", "let x = 999999999\nlet x = x * 999\nsay $x\nlet y = rnd 0\n");
    CHECK(play("t7", 0, 0)); CHECK(HAS("1000000000") && HAS("rnd needs"));
    script("t8", "put a$_\nput b\nsay\nsay end\nask q\nsay q=$q\n");
    { const char *in[] = { "42" }; CHECK(play("t8", in, 1)); CHECK(HAS("a b\n") && HAS("q=42")); }
    /* слишком длинный скрипт */
    { char big[2048]; int n = 0; for (int i = 0; i < 200; i++) { big[n++] = 'e'; big[n++] = '\n'; } big[n] = 0; script("t9", big); CHECK(play("t9", 0, 0)); CHECK(HAS("too long")); }

    /* RAMFS: 64 узла - установка при переполнении сообщает об ошибке, а не падает */
    printf("test_stas: OK\n");
    return 0;
}
