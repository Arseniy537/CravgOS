#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
static int ci_prefix(const char *s, const char *p) { (void)s; (void)p; return 0; }
#include "../html.h"
#include "../image.h"
static void shrink_dummy(void) {}
static u8 *rd(const char *p, int *n) { FILE *f = fopen(p, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET); u8 *b = malloc(*n + 1); if (fread(b, 1, *n, f) != (size_t)*n) return 0; fclose(f); return b; }
int main(int argc, char **argv) {
    (void)shrink_dummy; (void)ci_prefix;
    int bad = 0;
    for (int i = 1; i < argc; i++) {
        int n; u8 *b = rd(argv[i], &n);
        if (!b) { printf("cannot read %s\n", argv[i]); bad++; continue; }
        img_cnt = 1; pg_lines = 20; img_line[0] = 1; img_lk[0] = 0;
        int e = img_decode(b, n);
        if (e) { printf("%s: decode error %d\n", argv[i], e); bad++; free(b); continue; }
        img_paint(0);
        int l = 1 + (im_oh + 1) / 4;   /* middle row */
        int x1 = im_ow / 4, x2 = im_ow * 3 / 4;
        printf("%s: %dx%d -> %dx%d  UL=%X/%X UR=%X/%X LL=%X/%X LR=%X/%X\n", argv[i], im_w, im_h, im_ow, im_oh,
            pg_at[1][x1] & 15, pg_at[1][x1] >> 4, pg_at[1][x2] & 15, pg_at[1][x2] >> 4,
            pg_at[(im_oh + 1) / 2][x1] & 15, pg_at[(im_oh + 1) / 2][x1] >> 4, pg_at[(im_oh + 1) / 2][x2] & 15, pg_at[(im_oh + 1) / 2][x2] >> 4);
        (void)l;
        /* усечённые и испорченные данные не должны падать */
        for (int cut = 0; cut < n; cut += n / 37 + 1) { img_cnt = 1; pg_lines = 20; img_decode(b, cut); }
        u8 *c = malloc(n);
        for (int t = 0; t < 300; t++) { memcpy(c, b, n); for (int k = 0; k < 4; k++) c[rand() % n] ^= 1 << (rand() % 8); img_cnt = 1; pg_lines = 20; img_line[0] = 1; if (img_decode(c, n) == 0) img_paint(0); }
        free(c); free(b);
    }
    printf(bad ? "image tests FAILED\n" : "image tests OK\n");
    return bad;
}
