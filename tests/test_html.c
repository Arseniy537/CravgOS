#include <stdio.h>
#include <string.h>
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
static int ci_prefix(const char *s, const char *p) { for (; *p; s++, p++) { char a = *s, b = *p; if (a >= 65 && a <= 90) a += 32; if (b >= 65 && b <= 90) b += 32; if (a != b) return 0; } return 1; }
#include "../html.h"
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); dump(); return 1; } } while(0)
static void dump(void) { for (int l = 0; l < pg_lines && l < 40; l++) { printf("%2d|", l); for (int x = 0; x < pg_len[l]; x++) putchar(pg_ch[l][x]); printf("|\n"); } }
static char *line(int l) { static char b[100]; int n = pg_len[l]; memcpy(b, pg_ch[l], n); b[n] = 0; return b; }
int main(void) {
    const char *h = "<html><head><title>My  Page</title><style>p{color:red}</style></head><body><h1>Hello</h1>"
        "<p>Some <b>bold</b> text &amp; <a href=\"/x?a=1&b=2\">a link</a>.</p><ul><li>One</li><li>Two</li></ul>"
        "<script>var x=\"<b>\";</script><!-- comment --><pre>a  b\nc</pre><p>caf&eacute; &#65;&#x42; &copy; 1 < 2</p></body></html>";
    html_render(h, strlen(h), 0);
    CHECK(strcmp(pg_title, "My Page") == 0);
    CHECK(pg_lines == 11);
    CHECK(strcmp(line(0), "Hello") == 0 && pg_at[0][0] == PA_HEAD1);
    CHECK(strcmp(line(1), "") == 0);
    CHECK(strcmp(line(2), "Some bold text & a link.") == 0);
    CHECK(pg_at[2][5] == PA_BOLD && pg_at[2][0] == PA_TEXT);
    CHECK(lk_count == 1 && strcmp(lk_url[1], "/x?a=1&b=2") == 0);
    CHECK(pg_lk[2][17] == 1 && pg_lk[2][18] == 1 && pg_lk[2][19] == 1 && pg_lk[2][22] == 1 && pg_at[2][18] == PA_LINK);   /* "a link" сплошная область */
    CHECK(pg_lk[2][16] == 0 && pg_lk[2][23] == 0);
    CHECK(strcmp(line(4), "* One") == 0 && strcmp(line(5), "* Two") == 0);
    CHECK(strcmp(line(7), "a  b") == 0 && strcmp(line(8), "c") == 0);
    CHECK(strcmp(line(10), "caf&eacute; AB (c) 1 < 2") == 0);
    /* перенос по словам */
    char big[2000] = "<p>"; for (int i = 0; i < 40; i++) strcat(big, "word-number ");
    html_render(big, strlen(big), 0);
    for (int l = 0; l < pg_lines; l++) { CHECK(pg_len[l] <= PG_W); CHECK(l == pg_lines - 1 || pg_len[l] > 60); }
    CHECK(pg_lines >= 6 && pg_lines <= 8);
    /* очень длинное слово режется */
    char lw[300]; memset(lw, 'x', 250); lw[250] = 0;
    html_render(lw, 250, 0);
    CHECK(pg_lines == 4 && pg_len[0] == 80 && pg_len[3] == 10);
    /* UTF-8 */
    html_render("a \xE2\x80\x94 b \xC3\xA9 c \xD0\x96", 15, 0);
    CHECK(strcmp(line(0), "a - b ? c \x86") == 0);                    /* Ж -> CP866 0x86 */
    /* кириллица: UTF-8, windows-1251, KOI8-R, CP866, сущности */
    html_render("\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82, \xD0\xBC\xD0\xB8\xD1\x80! \xD0\x81\xD1\x91", 27, 0);
    CHECK(strcmp(line(0), "\x8F\xE0\xA8\xA2\xA5\xE2, \xAC\xA8\xE0! \xF0\xF1") == 0);
    html_charset = CS_1251; html_render("\xCF\xF0\xE8\xE2\xE5\xF2 \xB8\xA8", 9, 0);
    CHECK(strcmp(line(0), "\x8F\xE0\xA8\xA2\xA5\xE2 \xF1\xF0") == 0);
    html_charset = CS_KOI8; html_render("\xF0\xD2\xC9\xD7\xC5\xD4 \xA3", 8, 0);
    CHECK(strcmp(line(0), "\x8F\xE0\xA8\xA2\xA5\xE2 \xF1") == 0);
    html_charset = CS_866; html_render("\x8F\xE0\xA8", 3, 0);
    CHECK(strcmp(line(0), "\x8F\xE0\xA8") == 0);
    html_charset = CS_UTF8;
    html_render("&#1087;&#x43F; &laquo;a&raquo; &#8470;", 38, 0);
    CHECK(strcmp(line(0), "\xAF\xAF \"a\" No") == 0);
    html_charset = CS_1251; html_render("\xCF\xF0 <b>\xE8</b>", 12, 1);                    /* plain: теги не разбираются */
    CHECK(strcmp(line(0), "\x8F\xE0 <b>\xA8</b>") == 0);
    html_charset = CS_UTF8;
    CHECK(html_detect_charset("x", 1, "text/html; charset=windows-1251", 0) == CS_1251);
    CHECK(html_detect_charset("<meta charset=\"KOI8-R\">", 22, "text/html", 0) == CS_KOI8);
    CHECK(html_detect_charset("<meta http-equiv='Content-Type' content='text/html; charset=windows-1251'>", 73, 0, 0) == CS_1251);
    CHECK(html_detect_charset("plain ascii", 11, 0, CS_1251) == CS_UTF8);
    CHECK(html_detect_charset("\xCF\xF0\xE8\xE2\xE5\xF2", 6, "text/html", CS_1251) == CS_1251);     /* не UTF-8 -> запасная */
    CHECK(html_detect_charset("\xD0\x9F\xD1\x80", 4, "text/html", CS_1251) == CS_UTF8);
    CHECK(html_detect_charset("\x8F\xE0", 2, 0, CS_866) == CS_866);
    /* обычный текст */
    html_render("a\n<b>\n\tx", 8, 1);
    CHECK(strcmp(line(0), "a") == 0 && strcmp(line(1), "<b>") == 0 && strcmp(line(2), "    x") == 0 && lk_count == 0);
    /* ссылки: несколько, без href, картинки с alt, br, hr */
    const char *h2 = "<a href='http://a.com/'>A</a> <a name=x>no</a> <a href=b.html>B</a><br><img alt=\"pic\" src=z.png><hr>";
    html_render(h2, strlen(h2), 0);
    CHECK(lk_count == 2 && strcmp(lk_url[1], "http://a.com/") == 0 && strcmp(lk_url[2], "b.html") == 0);
    CHECK(strcmp(line(0), "A no B") == 0 && pg_lk[0][0] == 1 && pg_lk[0][5] == 2 && pg_lk[0][3] == 0);
    CHECK(strcmp(line(1), "[pic]") == 0 && img_cnt == 1 && img_line[0] == 1 && pg_len[13] == 78 && strcmp(img_src[0], "z.png") == 0);
    /* мусор и обрезанный HTML не должны падать */
    const char *bad[] = {"<", "<a href=", "<a href=\"x", "&", "&#", "&#x;", "<!--", "<b", "</", "<<>><//>", "a<script>b"};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) html_render(bad[i], strlen(bad[i]), 0);
    /* переполнение страницы */
    static char huge[200000]; int n = 0;
    for (int i = 0; i < 20000 && n < 199990; i++) n += sprintf(huge + n, "<p>line %d</p>", i);
    html_render(huge, n, 0);
    CHECK(pg_lines <= PG_MAXL && pg_lines > 2000);
    /* формы и поля ввода */
    const char *h_form = "<form action=\"/search\" method=\"GET\"><input type=\"text\" name=\"q\" value=\"cravgos\" size=\"15\">"
                         "<input type=\"submit\" value=\"Search\"></form>";
    html_render(h_form, strlen(h_form), 0);
    CHECK(form_cnt == 1 && strcmp(forms[0].action, "/search") == 0 && strcmp(forms[0].method, "GET") == 0);
    CHECK(inp_cnt == 2);
    CHECK(strcmp(inps[0].name, "q") == 0 && strcmp(inps[0].value, "cravgos") == 0 && inps[0].width == 15);
    CHECK(strcmp(inps[1].type, "submit") == 0 && strcmp(inps[1].value, "Search") == 0);
    CHECK(pg_inp[0][1] == 1 && pg_inp[0][2] == 1);
    printf("html tests OK\n");
    return 0;
}
