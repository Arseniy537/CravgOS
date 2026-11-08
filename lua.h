/* lua.h - Lua-подмножество для Cravgos. Чистый C, без libc, без malloc (арена в памяти ядра).
 *
 * Что есть: nil/boolean/целые (64 бита, дробных чисел НЕТ: "/" делит нацело, как "//")/строки/таблицы/
 *   функции с замыканиями; local, if/while/repeat/for (числовой и for-in), function, return, break;
 *   операторы + - * / // % ^ .. == ~= < <= > >= and or not # (унарный -); вызов f(x), obj:m(x), f{...};
 *   метатаблицы только с __index (таблица или функция); pcall/error/load/dofile.
 * Библиотеки: string (len sub upper lower rep byte char find(без шаблонов) format reverse), table (insert remove
 *   concat sort unpack), math (floor abs max min random randomseed sqrt), os (time date ms sleep),
 *   term (экран/клавиатура), fs (файлы; запись и удаление только в /lua), edit(путь) - встроенный редактор.
 * Чего нет: дробные числа, шаблоны (match/gsub/gmatch), goto, ..., корутины, сборщик мусора:
 *   память освобождается целиком, когда программа завершилась (при нехватке: "not enough memory").
 * Esc прерывает программу, которая долго считает и не читает клавиатуру.
 */
#ifndef LUA_H
#define LUA_H
#ifndef LUA_HEAP_SIZE
#define LUA_HEAP_SIZE (1024 * 1024)
#endif
#ifndef LUA_HEAP_ATTR
#define LUA_HEAP_ATTR
#endif
#define LUA_STACKN 4096
#define LUA_MAXDEPTH 100
#define LUA_MAXRET 8
#define LUA_MAXARGS 12
#define LUA_MAXP 8
#define LUA_MAXUP 16
#define LUA_MAXACT 60
#define LUA_PDEPTH 60

typedef long long lint;
enum { LT_NIL, LT_BOOL, LT_INT, LT_STR, LT_TAB, LT_FUN, LT_CFN, LT_BOX };
struct lstr; struct ltab; struct lclo; struct lbox;
struct lval { u8 t; union { lint i; struct lstr *s; struct ltab *tb; struct lclo *f; struct lbox *bx; } u; };
struct lstr { u32 len, hash; char s[]; };
struct lbox { struct lval v; };
struct lent { struct lval k, v; };
struct ltab { struct lval *arr; u32 an, acap; struct lent *h; u32 hcap, hcnt; struct ltab *meta; };

struct lvar { u16 slot; u8 captured; struct lstr *name; };
struct nd {
    u8 k, op; u16 line;
    struct nd *a, *b, *c, *d, *nx;
    union { lint i; struct lstr *s; struct lvar *lv; struct fproto *fp; int idx; } u;
};
struct fproto {
    struct lvar *params[LUA_MAXP]; int np;
    struct nd *body; int nslots, nup;
    u8 upfl[LUA_MAXUP]; u16 upix[LUA_MAXUP]; struct lstr *upname[LUA_MAXUP];
    const char *chunk;
};
struct lclo { struct fproto *fp; struct lbox *up[LUA_MAXUP]; };
struct frame { struct lval *v; struct lclo *cl; };

static struct lval lua_stk[LUA_STACKN] LUA_HEAP_ATTR;
static struct {
    int sp, depth, line, err, nrv, abort_on, interrupted;
    struct lval rv[LUA_MAXRET], errv;
    char emsg[200];
    const char *chunk;
    struct ltab *globals, *strlib;
    struct lstr *s_index;
    u32 steps, since_key, seed;
    int pend[16], npend;
} L;

/* ---------- Ошибки ---------- */
static int lua_slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static void lerr3(const char *a, const char *b, const char *c) {
    if (L.err) return;
    int n = 0;
    const char *ch = L.chunk ? L.chunk : "?";
    for (int i = 0; ch[i] && n < 40; i++) L.emsg[n++] = ch[i];
    L.emsg[n++] = ':';
    int ln = L.line; char t[12]; int m = 0;
    if (!ln) t[m++] = '0';
    while (ln) { t[m++] = (char)('0' + ln % 10); ln /= 10; }
    while (m) L.emsg[n++] = t[--m];
    L.emsg[n++] = ':'; L.emsg[n++] = ' ';
    for (int i = 0; a[i] && n < (int)sizeof L.emsg - 1; i++) L.emsg[n++] = a[i];
    for (int i = 0; b && b[i] && n < (int)sizeof L.emsg - 1; i++) L.emsg[n++] = b[i];
    for (int i = 0; c && c[i] && n < (int)sizeof L.emsg - 1; i++) L.emsg[n++] = c[i];
    L.emsg[n] = 0;
    L.err = 1; L.errv.t = LT_NIL;
}
static void lerr2(const char *a, const char *b) { lerr3(a, b, 0); }
static void lerr(const char *a) { lerr3(a, 0, 0); }

/* ---------- Память ----------
 * Одна куча со сборщиком мусора (mark-sweep, неподвижные объекты): строки, таблицы, замыкания, коробки
 * и дерево разбора программ (блоки GK_AST просматриваются консервативно, поэтому код, который больше
 * не нужен, тоже освобождается). Корни: глобальные таблицы, стек Lua, L.rv, состояние разбора,
 * а также машинный стек и регистры (консервативно). */
struct gch { u32 size; u8 kind, flags, pad[2]; };            /* заголовок блока: 8 байт, блоки кратны 16 */
enum { GK_FREE, GK_STR, GK_VALS, GK_TAB, GK_CLO, GK_BOX, GK_AST };
#define GC_NIL 0xFFFFFFFFu
static u8 lua_heap[LUA_HEAP_SIZE] LUA_HEAP_ATTR __attribute__((aligned(16)));
static u32 gc_bm[LUA_HEAP_SIZE / 16 / 32 + 1] LUA_HEAP_ATTR;  /* бит на начало каждого блока */
static u32 gc_ms[2048] LUA_HEAP_ATTR;
static u32 lua_top, gc_free, gc_msn, gc_ovf;
static u32 gc_count;                                          /* число сборок (для тестов) */
static void *lgalloc(u32 n, u8 kind);
static void *lperm(u32 n) { return lgalloc(n, GK_AST); }      /* объекты разбора: AST, прототипы, переменные */
static struct gch *gc_h(u32 off) { return (struct gch *)(lua_heap + off); }
static u32 *gc_nextp(u32 off) { return (u32 *)(lua_heap + off + 8); }
static void gc_bmset(u32 off) { u32 i = off >> 4; gc_bm[i >> 5] |= 1u << (i & 31); }
static int gc_bmget(u32 i) { return (gc_bm[i >> 5] >> (i & 31)) & 1; }
static void gc_push(u32 off) {
    if (gc_msn < (u32)(sizeof gc_ms / sizeof gc_ms[0])) gc_ms[gc_msn++] = off; else gc_ovf = 1;
}
static void gc_markp(const void *p) {                          /* точный указатель на начало данных объекта */
    const u8 *b = (const u8 *)p;
    if (b < lua_heap + 8 || b >= lua_heap + lua_top) return;
    u32 off = (u32)(b - lua_heap) - 8;
    if ((off & 15) || !gc_bmget(off >> 4)) return;
    struct gch *h = gc_h(off);
    if (h->kind == GK_FREE || (h->flags & 1) || h->size < 16 || off + h->size > lua_top) return;
    h->flags |= 1; gc_push(off);
}
static void gc_markv(struct lval v) {
    switch (v.t) {
        case LT_STR: gc_markp(v.u.s); break;
        case LT_TAB: gc_markp(v.u.tb); break;
        case LT_FUN: gc_markp(v.u.f); break;
        case LT_BOX: gc_markp(v.u.bx); break;
        default: break;
    }
}
static void gc_markw(u64 w) {                                   /* слово со стека: может быть указателем внутрь объекта */
    if (w < (u64)(unsigned long long)lua_heap || w >= (u64)(unsigned long long)(lua_heap + lua_top)) return;
    u32 idx = (u32)((w - (u64)(unsigned long long)lua_heap) >> 4);
    while (idx > 0 && !gc_bmget(idx)) idx--;
    u32 off = idx << 4;
    struct gch *h = gc_h(off);
    if (h->kind == GK_FREE || (h->flags & 1) || h->size < 16 || off + h->size > lua_top) return;
    if (w >= (u64)(unsigned long long)lua_heap + off + h->size) return;
    h->flags |= 1; gc_push(off);
}
static void gc_scan_block(u32 off) {
    struct gch *h = gc_h(off);
    h->flags |= 2;
    u8 *pl = lua_heap + off + 8;
    switch (h->kind) {
        case GK_TAB: { struct ltab *t = (struct ltab *)pl; gc_markp(t->arr); gc_markp(t->h); gc_markp(t->meta); break; }
        case GK_VALS: { struct lval *v = (struct lval *)pl; u32 n = (h->size - 8) / 16; for (u32 i = 0; i < n; i++) gc_markv(v[i]); break; }
        case GK_CLO: { struct lclo *c = (struct lclo *)pl; gc_markp(c->fp); for (int i = 0; i < LUA_MAXUP; i++) gc_markp(c->up[i]); break; }
        case GK_AST: { u64 *w = (u64 *)pl; u32 n = (h->size - 8) / 8; for (u32 i = 0; i < n; i++) gc_markw(w[i]); break; }
        case GK_BOX: gc_markv(((struct lbox *)pl)->v); break;
        default: break;
    }
}
static void gc_drain(void) {
    for (;;) {
        while (gc_msn) gc_scan_block(gc_ms[--gc_msn]);
        if (!gc_ovf) break;
        gc_ovf = 0;                                             /* стек переполнялся: ищем помеченные, но не просмотренные блоки */
        for (u32 off = 0; off < lua_top; off += gc_h(off)->size) { struct gch *h = gc_h(off); if (h->kind != GK_FREE && (h->flags & 1) && !(h->flags & 2)) gc_push(off); }
    }
}
static void *lua_stack_base;
__attribute__((noinline, no_sanitize_address)) static void gc_scan_stack(void) {
    u64 *lo = (u64 *)__builtin_frame_address(0), *hi = (u64 *)lua_stack_base;
    for (u64 *p = lo; p < hi; p++) gc_markw(*p);
}
static void lerr(const char *a);
static void gc_mark_extra(void);                                /* корни разбора и т.п. (определена ниже) */
__attribute__((noinline)) static void lua_gc(void) {
    __builtin_unwind_init();                                    /* регистры - на стек */
    gc_count++;
    gc_msn = 0; gc_ovf = 0;
    for (u32 off = 0; off < lua_top; off += gc_h(off)->size) gc_h(off)->flags &= (u8)~3;
    gc_markp(L.globals); gc_markp(L.strlib); gc_markp(L.s_index); gc_markv(L.errv); gc_mark_extra();
    for (int i = 0; i < LUA_MAXRET; i++) gc_markv(L.rv[i]);
    for (int i = 0; i < L.sp + 1 && i < LUA_STACKN; i++) gc_markv(lua_stk[i]);
    gc_scan_stack();
    gc_drain();
    for (u32 i = 0; i < sizeof gc_bm / sizeof gc_bm[0]; i++) gc_bm[i] = 0;   /* сборка: склеиваем свободные участки */
    gc_free = GC_NIL;
    u32 tail = GC_NIL, run = GC_NIL, off = 0;
    while (off < lua_top) {
        struct gch *h = gc_h(off); u32 sz = h->size;
        if (h->kind != GK_FREE && (h->flags & 1)) {
            if (run != GC_NIL) {
                struct gch *f = gc_h(run); f->size = off - run; f->kind = GK_FREE; f->flags = 0; *gc_nextp(run) = GC_NIL; gc_bmset(run);
                if (tail == GC_NIL) gc_free = run; else *gc_nextp(tail) = run;
                tail = run; run = GC_NIL;
            }
            h->flags &= (u8)~3; gc_bmset(off);
        } else if (run == GC_NIL) run = off;
        off += sz;
    }
    if (run != GC_NIL) lua_top = run;                           /* хвост возвращается в «пузырь» */
}
static void *lgalloc(u32 n, u8 kind) {
    u32 need = (n + 8 + 15) & ~15u;
    for (int attempt = 0; attempt < 2; attempt++) {
#ifdef LUA_GC_STRESS
        if (attempt == 0) lua_gc();
#endif
        u32 prev = GC_NIL, cur = gc_free;
        while (cur != GC_NIL) {
            struct gch *h = gc_h(cur); u32 next = *gc_nextp(cur);
            if (h->size >= need) {
                if (h->size - need >= 32) {
                    u32 rem = cur + need; struct gch *r = gc_h(rem);
                    r->size = h->size - need; r->kind = GK_FREE; r->flags = 0; *gc_nextp(rem) = next; gc_bmset(rem);
                    h->size = need; next = rem;
                }
                if (prev == GC_NIL) gc_free = next; else *gc_nextp(prev) = next;
                h->kind = kind; h->flags = 0;
                u8 *pl = lua_heap + cur + 8; for (u32 i = 0; i < h->size - 8; i++) pl[i] = 0;
                return pl;
            }
            prev = cur; cur = next;
        }
        if (lua_top + need <= LUA_HEAP_SIZE) {
            struct gch *h = gc_h(lua_top); h->size = need; h->kind = kind; h->flags = 0; gc_bmset(lua_top);
            u8 *pl = lua_heap + lua_top + 8; for (u32 i = 0; i < need - 8; i++) pl[i] = 0;
            lua_top += need;
            return pl;
        }
        if (attempt == 0) lua_gc();
    }
    lerr("not enough memory");
    return 0;
}
static const struct lval LNIL = { LT_NIL, { 0 } };
static struct lval lint_v(lint i) { struct lval v; v.t = LT_INT; v.u.i = i; return v; }
static struct lval lbool_v(int b) { struct lval v; v.t = LT_BOOL; v.u.i = b != 0; return v; }
static int ltruthy(struct lval v) { return !(v.t == LT_NIL || (v.t == LT_BOOL && !v.u.i)); }

static struct { u32 len, hash; char s[8]; } lua_empty_store;
#define lua_empty_s (*(struct lstr *)&lua_empty_store)
static u32 lhash(const char *s, u32 n) { u32 h = 2166136261u; for (u32 i = 0; i < n; i++) { h ^= (u8)s[i]; h *= 16777619u; } return h; }
static struct lstr *lmks(const char *s, u32 n) {
    struct lstr *r = lgalloc(8 + n + 1, GK_STR);
    if (!r) return &lua_empty_s;
    r->len = n; r->hash = lhash(s, n);
    for (u32 i = 0; i < n; i++) r->s[i] = s[i];
    r->s[n] = 0;
    return r;
}
#define lmks_p lmks
static struct lval lstr_v(const char *s, u32 n) { struct lval v; v.t = LT_STR; v.u.s = lmks(s, n); return v; }
static struct lval lcstr_v(const char *s) { return lstr_v(s, (u32)lua_slen(s)); }
static int lseq(const struct lstr *a, const struct lstr *b) {
    if (a == b) return 1;
    if (a->len != b->len || a->hash != b->hash) return 0;
    for (u32 i = 0; i < a->len; i++) if (a->s[i] != b->s[i]) return 0;
    return 1;
}
static int lraweq(struct lval a, struct lval b) {
    if (a.t != b.t) return 0;
    switch (a.t) {
        case LT_NIL: return 1;
        case LT_STR: return lseq(a.u.s, b.u.s);
        case LT_TAB: return a.u.tb == b.u.tb;
        case LT_FUN: return a.u.f == b.u.f;
        default: return a.u.i == b.u.i;
    }
}
static const char *ltname(struct lval v) {
    switch (v.t) { case LT_NIL: return "nil"; case LT_BOOL: return "boolean"; case LT_INT: return "number"; case LT_STR: return "string";
        case LT_TAB: return "table"; default: return "function"; }
}

/* ---------- Таблицы ---------- */
static struct ltab lua_dummy_tab;
static struct ltab *lnewtab(void) { struct ltab *t = lgalloc(sizeof *t, GK_TAB); return t ? t : &lua_dummy_tab; }
static u32 lkhash(struct lval k) {
    switch (k.t) {
        case LT_STR: return k.u.s->hash;
        case LT_TAB: return (u32)((unsigned long long)k.u.tb >> 4) * 2654435761u;
        case LT_FUN: return (u32)((unsigned long long)k.u.f >> 4) * 2654435761u;
        default: return (u32)k.u.i * 2654435761u + k.t;
    }
}
static struct lent *lhfind(struct ltab *t, struct lval k) {
    if (!t->hcap) return 0;
    u32 m = t->hcap - 1, i = lkhash(k) & m;
    for (u32 n = 0; n < t->hcap; n++, i = (i + 1) & m) {
        struct lent *e = &t->h[i];
        if (e->k.t == LT_NIL) return 0;
        if (lraweq(e->k, k)) return e;
    }
    return 0;
}
static struct lval lrawget(struct ltab *t, struct lval k) {
    if (k.t == LT_INT && k.u.i >= 1 && (u64)k.u.i <= t->an) return t->arr[k.u.i - 1];
    struct lent *e = lhfind(t, k);
    return e ? e->v : LNIL;
}
static void lhgrow(struct ltab *t) {
    u32 nc = t->hcap ? t->hcap * 2 : 4;
    struct lent *nh = lgalloc(nc * (u32)sizeof(struct lent), GK_VALS);
    if (!nh) return;
    struct lent *old = t->h; u32 oc = t->hcap;
    t->h = nh; t->hcap = nc; t->hcnt = 0;
    for (u32 i = 0; i < oc; i++) if (old[i].k.t != LT_NIL && old[i].v.t != LT_NIL) {
        u32 m = nc - 1, j = lkhash(old[i].k) & m;
        while (nh[j].k.t != LT_NIL) j = (j + 1) & m;
        nh[j] = old[i]; t->hcnt++;
    }
}
static void lhset(struct ltab *t, struct lval k, struct lval v) {
    struct lent *e = lhfind(t, k);
    if (e) { e->v = v; return; }
    if (v.t == LT_NIL) return;
    if (!t->hcap || (t->hcnt + 1) * 4 > t->hcap * 3) { lhgrow(t); if (L.err) return; }
    u32 m = t->hcap - 1, i = lkhash(k) & m;
    while (t->h[i].k.t != LT_NIL) i = (i + 1) & m;
    t->h[i].k = k; t->h[i].v = v; t->hcnt++;
}
static void lrawset(struct ltab *t, struct lval k, struct lval v) {
    if (t == &lua_dummy_tab) return;
    if (k.t == LT_NIL) { lerr("table index is nil"); return; }
    if (k.t == LT_INT && k.u.i >= 1 && (u64)k.u.i <= t->an) {
        t->arr[k.u.i - 1] = v;
        if (v.t == LT_NIL) while (t->an && t->arr[t->an - 1].t == LT_NIL) t->an--;
        return;
    }
    if (k.t == LT_INT && (u64)k.u.i == (u64)t->an + 1 && v.t != LT_NIL) {
        if (t->an == t->acap) {
            u32 nc = t->acap ? t->acap * 2 : 4;
            struct lval *na = lgalloc(nc * (u32)sizeof(struct lval), GK_VALS);
            if (!na) return;
            for (u32 i = 0; i < t->an; i++) na[i] = t->arr[i];
            t->arr = na; t->acap = nc;
        }
        t->arr[t->an++] = v;
        if (t->hcnt) {                                  /* ключи, ждавшие своей очереди в хеш-части */
            struct lent *e;
            while ((e = lhfind(t, lint_v((lint)t->an + 1))) && e->v.t != LT_NIL) {
                struct lval mv = e->v; e->v = LNIL;
                if (t->an == t->acap) {
                    u32 nc = t->acap * 2; struct lval *na = lgalloc(nc * (u32)sizeof(struct lval), GK_VALS);
                    if (!na) return;
                    for (u32 i = 0; i < t->an; i++) na[i] = t->arr[i];
                    t->arr = na; t->acap = nc;
                }
                t->arr[t->an++] = mv;
            }
        }
        return;
    }
    lhset(t, k, v);
}
/* обход: следующая пара после ключа k (nil = с начала); 0 - конец */
static int lnext(struct ltab *t, struct lval k, struct lval *ok, struct lval *ov) {
    u32 i = 0;                                          /* позиция: 0..an-1 массив, дальше хеш */
    if (k.t != LT_NIL) {
        if (k.t == LT_INT && k.u.i >= 1 && (u64)k.u.i <= t->an) i = (u32)k.u.i;
        else {
            struct lent *e = lhfind(t, k);
            if (!e) return 0;
            i = t->an + (u32)(e - t->h) + 1;
        }
    }
    for (; i < t->an; i++) if (t->arr[i].t != LT_NIL) { *ok = lint_v((lint)i + 1); *ov = t->arr[i]; return 1; }
    for (u32 j = i - t->an; j < t->hcap; j++)
        if (t->h[j].k.t != LT_NIL && t->h[j].v.t != LT_NIL) { *ok = t->h[j].k; *ov = t->h[j].v; return 1; }
    return 0;
}
static struct lval ltab_v(struct ltab *t) { struct lval v; v.t = LT_TAB; v.u.tb = t; return v; }
static struct lval lgetfield(struct ltab *t, const char *name) { return lrawget(t, lcstr_v(name)); }
static void lsetfield(struct ltab *t, const char *name, struct lval v) { lrawset(t, lcstr_v(name), v); }

/* ---------- Лексер ---------- */
static const char *PCHUNK;
static void pfail(const char *m);
enum { TK_EOF = 256, TK_NAME, TK_STR, TK_NUM, TK_EQ, TK_NE, TK_LE, TK_GE, TK_IDIV, TK_CAT, TK_DOTS,
       TK_AND, TK_BREAK, TK_DO, TK_ELSE, TK_ELSEIF, TK_END, TK_FALSE, TK_FOR, TK_FUNCTION, TK_IF, TK_IN,
       TK_LOCAL, TK_NIL, TK_NOT, TK_OR, TK_REPEAT, TK_RETURN, TK_THEN, TK_TRUE, TK_UNTIL, TK_WHILE };
static const char *const lua_kw[] = { "and", "break", "do", "else", "elseif", "end", "false", "for", "function", "if", "in",
    "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while" };
struct lxs { const char *p, *end; int line, t; lint n; struct lstr *s; };
static struct lxs LX;

static int lisalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static int lisdigit(int c) { return c >= '0' && c <= '9'; }
static int lhexv(int c) { return lisdigit(c) ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1; }
static void lx_next(void) {
    for (;;) {
        if (LX.p >= LX.end) { LX.t = TK_EOF; return; }
        int c = (u8)*LX.p;
        if (c == '\n') { LX.line++; LX.p++; continue; }
        if (c == ' ' || c == '\t' || c == '\r') { LX.p++; continue; }
        if (c == '-' && LX.p + 1 < LX.end && LX.p[1] == '-') {
            LX.p += 2;
            if (LX.p + 1 < LX.end && LX.p[0] == '[' && LX.p[1] == '[') {
                LX.p += 2;
                while (LX.p + 1 < LX.end && !(LX.p[0] == ']' && LX.p[1] == ']')) { if (*LX.p == '\n') LX.line++; LX.p++; }
                LX.p += 2;
            } else while (LX.p < LX.end && *LX.p != '\n') LX.p++;
            continue;
        }
        break;
    }
    int c = (u8)*LX.p;
    if (lisalpha(c)) {
        const char *s = LX.p;
        while (LX.p < LX.end && (lisalpha((u8)*LX.p) || lisdigit((u8)*LX.p))) LX.p++;
        u32 n = (u32)(LX.p - s);
        for (int i = 0; i < 21; i++) {
            const char *k = lua_kw[i]; u32 j = 0;
            while (j < n && k[j] == s[j]) j++;
            if (j == n && !k[j]) { LX.t = TK_AND + i; return; }
        }
        LX.t = TK_NAME; LX.s = lmks_p(s, n); return;
    }
    if (lisdigit(c)) {
        lint v = 0;
        if (c == '0' && LX.p + 1 < LX.end && (LX.p[1] == 'x' || LX.p[1] == 'X')) {
            LX.p += 2;
            while (LX.p < LX.end && lhexv((u8)*LX.p) >= 0) { v = (lint)((unsigned long long)v * 16 + (unsigned)lhexv((u8)*LX.p)); LX.p++; }
        } else {
            while (LX.p < LX.end && lisdigit((u8)*LX.p)) { v = (lint)((unsigned long long)v * 10 + (unsigned)(*LX.p - '0')); LX.p++; }
            if (LX.p < LX.end && (*LX.p == '.' || *LX.p == 'e' || *LX.p == 'E') && !(LX.p[0] == '.' && LX.p + 1 < LX.end && LX.p[1] == '.')) {
                pfail("only integer numbers are supported"); return;
            }
        }
        LX.t = TK_NUM; LX.n = v; return;
    }
    if (c == '"' || c == '\'') {
        int q = c; LX.p++;
        char buf[512]; int n = 0;
        while (LX.p < LX.end && *LX.p != q) {
            char ch = *LX.p++;
            if (ch == '\n') { pfail("unfinished string"); return; }
            if (ch == '\\' && LX.p < LX.end) {
                ch = *LX.p++;
                if (ch == 'n') ch = '\n'; else if (ch == 't') ch = '\t'; else if (ch == 'r') ch = '\r';
                else if (ch == 'a') ch = 7; else if (ch == '0' && !lisdigit((u8)*LX.p)) ch = 0;
                else if (ch == 'x' && LX.p + 1 < LX.end) { int h = lhexv((u8)LX.p[0]), l = lhexv((u8)LX.p[1]); if (h >= 0 && l >= 0) { ch = (char)(h * 16 + l); LX.p += 2; } }
                else if (lisdigit((u8)ch)) { int v = ch - '0', k = 1; while (k < 3 && LX.p < LX.end && lisdigit((u8)*LX.p)) { v = v * 10 + (*LX.p++ - '0'); k++; } ch = (char)v; }
                else if (ch == '\n') LX.line++;
            }
            if (n < (int)sizeof buf) buf[n++] = ch; else { pfail("string too long"); return; }
        }
        if (LX.p >= LX.end) { pfail("unfinished string"); return; }
        LX.p++;
        LX.t = TK_STR; LX.s = lmks_p(buf, (u32)n); return;
    }
    if (c == '[' && LX.p + 1 < LX.end && LX.p[1] == '[') {
        LX.p += 2;
        if (LX.p < LX.end && *LX.p == '\n') { LX.p++; LX.line++; }
        const char *s = LX.p;
        while (LX.p + 1 < LX.end && !(LX.p[0] == ']' && LX.p[1] == ']')) { if (*LX.p == '\n') LX.line++; LX.p++; }
        LX.t = TK_STR; LX.s = lmks_p(s, (u32)(LX.p - s)); LX.p += 2; return;
    }
    LX.p++;
    char n1 = LX.p < LX.end ? *LX.p : 0;
    if (c == '=' && n1 == '=') { LX.p++; LX.t = TK_EQ; return; }
    if (c == '~' && n1 == '=') { LX.p++; LX.t = TK_NE; return; }
    if (c == '<' && n1 == '=') { LX.p++; LX.t = TK_LE; return; }
    if (c == '>' && n1 == '=') { LX.p++; LX.t = TK_GE; return; }
    if (c == '/' && n1 == '/') { LX.p++; LX.t = TK_IDIV; return; }
    if (c == '.' && n1 == '.') { LX.p++; if (LX.p < LX.end && *LX.p == '.') { LX.p++; LX.t = TK_DOTS; } else LX.t = TK_CAT; return; }
    LX.t = c;
}
static int lx_peek(void) {
    struct lxs sv = LX;
    lx_next(); int t = LX.t; LX = sv; return t;
}

/* ---------- Разбор ---------- */
enum { N_NIL, N_TRUE, N_FALSE, N_INT, N_STR, N_LOCAL, N_UP, N_GLOBAL, N_INDEX, N_CALL, N_METHOD, N_FUNC, N_BIN, N_NOT, N_NEG, N_LEN,
       N_AND, N_OR, N_TABLE, N_PAREN,
       S_LOCAL, S_ASSIGN, S_CALL, S_DO, S_WHILE, S_REPEAT, S_IF, S_FORN, S_FORIN, S_RETURN, S_BREAK, S_LFUNC };
struct fstate { struct fstate *up; struct fproto *fp; struct lvar *act[LUA_MAXACT]; int nact, nslots; };
static struct fstate *FS;
static int pdepth;
static void gc_mark_extra(void) {
    gc_markw((u64)(unsigned long long)LX.p); gc_markp(LX.s); gc_markw((u64)(unsigned long long)PCHUNK); gc_markp(FS); gc_markw((u64)(unsigned long long)L.chunk);
}

static struct nd *nnew(int k) {
    struct nd *n = lperm(sizeof *n);
    if (!n) { static struct nd dummy; return &dummy; }
    n->k = (u8)k; n->line = (u16)LX.line;
    return n;
}
static void pfail(const char *m) { if (!L.err) { const char *oc = L.chunk; int ol = L.line; L.chunk = PCHUNK; L.line = LX.line; lerr(m); L.chunk = oc; L.line = ol; } LX.t = TK_EOF; }
static int paccept(int t) { if (LX.t == t) { lx_next(); return 1; } return 0; }
static void pexpect(int t, const char *m) { if (LX.t == t) lx_next(); else pfail(m); }
static struct lstr *pname(void) {
    if (LX.t != TK_NAME) { pfail("name expected"); return &lua_empty_s; }
    struct lstr *s = LX.s; lx_next(); return s;
}
static struct lvar *pdeclare(struct lstr *name) {
    struct lvar *lv = lperm(sizeof *lv);
    if (!lv || FS->nact >= LUA_MAXACT) { pfail("too many local variables"); static struct lvar d; return &d; }
    lv->name = name; lv->slot = (u16)FS->nact; FS->act[FS->nact++] = lv;
    if (FS->nact > FS->nslots) FS->nslots = FS->nact;
    return lv;
}
static struct lvar *pfindlocal(struct fstate *fs, struct lstr *name) {
    for (int i = fs->nact - 1; i >= 0; i--) if (fs->act[i]->name && lseq(fs->act[i]->name, name)) return fs->act[i];
    return 0;
}
static int pupval(struct fstate *fs, struct lstr *name) {   /* индекс upvalue или -1 */
    for (int i = 0; i < fs->fp->nup; i++) if (lseq(fs->fp->upname[i], name)) return i;
    if (!fs->up) return -1;
    struct lvar *lv = pfindlocal(fs->up, name);
    int fromlocal = 1, idx;
    if (lv) { lv->captured = 1; idx = lv->slot; }
    else { idx = pupval(fs->up, name); if (idx < 0) return -1; fromlocal = 0; }
    if (fs->fp->nup >= LUA_MAXUP) { pfail("too many upvalues"); return -1; }
    int u = fs->fp->nup++;
    fs->fp->upfl[u] = (u8)fromlocal; fs->fp->upix[u] = (u16)idx; fs->fp->upname[u] = name;
    return u;
}
static struct nd *pvar(struct lstr *name) {
    struct nd *n;
    struct lvar *lv = pfindlocal(FS, name);
    if (lv) { n = nnew(N_LOCAL); n->u.lv = lv; return n; }
    int u = pupval(FS, name);
    if (u >= 0) { n = nnew(N_UP); n->u.idx = u; return n; }
    n = nnew(N_GLOBAL); n->u.s = name; return n;
}
static struct nd *pexpr(int limit);
static struct nd *pblock(void);
static void pfuncbody(struct nd *fn, int ismethod);

static struct nd *pexplist(void) {                       /* список выражений через nx */
    struct nd *first = pexpr(0), *last = first;
    while (paccept(',')) { last->nx = pexpr(0); last = last->nx; }
    return first;
}
static struct nd *pargs(struct nd *fnode) {
    struct nd *call = fnode;
    if (LX.t == '(') {
        lx_next();
        if (LX.t != ')') call->b = pexplist();
        pexpect(')', "')' expected");
    } else if (LX.t == TK_STR) { struct nd *s = nnew(N_STR); s->u.s = LX.s; lx_next(); call->b = s; }
    else if (LX.t == '{') {
        struct nd *t = nnew(N_TABLE); lx_next();
        struct nd *last = 0;
        while (LX.t != '}' && LX.t != TK_EOF) {
            struct nd *item = nnew(N_NIL);
            if (LX.t == '[') { lx_next(); item->a = pexpr(0); pexpect(']', "']' expected"); pexpect('=', "'=' expected"); item->b = pexpr(0); item->op = 1; }
            else if (LX.t == TK_NAME && lx_peek() == '=') { struct nd *k = nnew(N_STR); k->u.s = LX.s; lx_next(); lx_next(); item->a = k; item->b = pexpr(0); item->op = 1; }
            else { item->b = pexpr(0); item->op = 0; }
            if (last) last->nx = item; else t->a = item;
            last = item;
            if (!paccept(',') && !paccept(';')) break;
        }
        pexpect('}', "'}' expected");
        call->b = t;
    } else pfail("function arguments expected");
    return call;
}
static struct nd *psimple(void) {
    struct nd *n;
    switch (LX.t) {
        case TK_NUM: n = nnew(N_INT); n->u.i = LX.n; lx_next(); return n;
        case TK_STR: n = nnew(N_STR); n->u.s = LX.s; lx_next(); return n;
        case TK_NIL: n = nnew(N_NIL); lx_next(); return n;
        case TK_TRUE: n = nnew(N_TRUE); lx_next(); return n;
        case TK_FALSE: n = nnew(N_FALSE); lx_next(); return n;
        case TK_FUNCTION: n = nnew(N_FUNC); lx_next(); pfuncbody(n, 0); return n;
        case '{': { struct nd *dummy = nnew(N_CALL); pargs(dummy); return dummy->b; }
        case TK_DOTS: pfail("'...' is not supported"); return nnew(N_NIL);
        default: break;
    }
    if (LX.t == TK_NAME) n = pvar(pname());
    else if (LX.t == '(') { lx_next(); struct nd *e = pexpr(0); pexpect(')', "')' expected"); n = nnew(N_PAREN); n->a = e; }
    else { pfail("unexpected symbol"); return nnew(N_NIL); }
    for (;;) {
        if (LX.t == '.') { lx_next(); struct nd *i = nnew(N_INDEX); struct nd *k = nnew(N_STR); k->u.s = pname(); i->a = n; i->b = k; n = i; }
        else if (LX.t == '[') { lx_next(); struct nd *i = nnew(N_INDEX); i->a = n; i->b = pexpr(0); pexpect(']', "']' expected"); n = i; }
        else if (LX.t == ':') { lx_next(); struct nd *m = nnew(N_METHOD); m->a = n; m->u.s = pname(); pargs(m); n = m; }
        else if (LX.t == '(' || LX.t == TK_STR || LX.t == '{') { struct nd *c = nnew(N_CALL); c->a = n; pargs(c); n = c; }
        else return n;
        if (L.err) return n;
    }
}
static int bprio(int t, int *rp) {                        /* приоритет левой части; *rp - правой */
    switch (t) {
        case TK_OR: *rp = 1; return 1;
        case TK_AND: *rp = 2; return 2;
        case '<': case '>': case TK_LE: case TK_GE: case TK_NE: case TK_EQ: *rp = 3; return 3;
        case TK_CAT: *rp = 8; return 9;
        case '+': case '-': *rp = 10; return 10;
        case '*': case '/': case '%': case TK_IDIV: *rp = 11; return 11;
        case '^': *rp = 13; return 14;
        default: return -1;
    }
}
static struct nd *pexpr(int limit) {
    if (++pdepth > LUA_PDEPTH) { pfail("expression is too complex"); pdepth--; return nnew(N_NIL); }
    struct nd *l;
    if (LX.t == TK_NOT || LX.t == '-' || LX.t == '#') {
        int t = LX.t; lx_next();
        struct nd *o = pexpr(12);
        l = nnew(t == TK_NOT ? N_NOT : t == '-' ? N_NEG : N_LEN); l->a = o;
    } else l = psimple();
    int rp = 0, p;
    while (!L.err && (p = bprio(LX.t, &rp)) > limit) {
        int op = LX.t; lx_next();
        struct nd *r = pexpr(rp);
        struct nd *b = nnew(op == TK_AND ? N_AND : op == TK_OR ? N_OR : N_BIN);
        b->op = (u8)(op < 256 ? op : op - 256 + 200); b->a = l; b->b = r; l = b;
    }
    pdepth--;
    return l;
}
static void pfuncbody(struct nd *fn, int ismethod) {
    struct fproto *fp = lperm(sizeof *fp);
    if (!fp) { pfail("not enough memory"); return; }
    struct fstate *nfs = lperm(sizeof *nfs);
    if (!nfs) { pfail("not enough memory"); return; }
    nfs->up = FS; nfs->fp = fp; fp->chunk = PCHUNK; FS = nfs;
    if (ismethod) fp->params[fp->np++] = pdeclare(lmks_p("self", 4));
    pexpect('(', "'(' expected");
    if (LX.t != ')') {
        do {
            if (LX.t == TK_DOTS) { pfail("'...' is not supported"); break; }
            if (fp->np >= LUA_MAXP) { pfail("too many parameters"); break; }
            fp->params[fp->np++] = pdeclare(pname());
        } while (paccept(','));
    }
    pexpect(')', "')' expected");
    fp->body = pblock();
    pexpect(TK_END, "'end' expected");
    fp->nslots = nfs->nslots;
    FS = nfs->up;
    fn->u.fp = fp;
}
static int pblockend(void) { return LX.t == TK_EOF || LX.t == TK_END || LX.t == TK_ELSE || LX.t == TK_ELSEIF || LX.t == TK_UNTIL; }
static struct nd *pstat(void) {
    struct nd *n;
    switch (LX.t) {
        case ';': lx_next(); return 0;
        case TK_BREAK: n = nnew(S_BREAK); lx_next(); return n;
        case TK_DO: { n = nnew(S_DO); lx_next(); int sa = FS->nact; n->a = pblock(); FS->nact = sa; pexpect(TK_END, "'end' expected"); return n; }
        case TK_WHILE: { n = nnew(S_WHILE); lx_next(); n->a = pexpr(0); pexpect(TK_DO, "'do' expected"); int sa = FS->nact; n->b = pblock(); FS->nact = sa; pexpect(TK_END, "'end' expected"); return n; }
        case TK_REPEAT: { n = nnew(S_REPEAT); lx_next(); int sa = FS->nact; n->a = pblock(); pexpect(TK_UNTIL, "'until' expected"); n->b = pexpr(0); FS->nact = sa; return n; }
        case TK_IF: {
            n = nnew(S_IF); lx_next();
            struct nd *cur = n;
            for (;;) {
                cur->a = pexpr(0); pexpect(TK_THEN, "'then' expected");
                int sa = FS->nact; cur->b = pblock(); FS->nact = sa;
                if (LX.t == TK_ELSEIF) { lx_next(); struct nd *e = nnew(S_IF); cur->c = e; cur = e; continue; }
                if (LX.t == TK_ELSE) { lx_next(); struct nd *e = nnew(S_DO); sa = FS->nact; e->a = pblock(); FS->nact = sa; cur->c = e; }
                break;
            }
            pexpect(TK_END, "'end' expected");
            return n;
        }
        case TK_FOR: {
            lx_next();
            struct lstr *n1 = pname();
            int sa = FS->nact;
            if (LX.t == '=') {
                n = nnew(S_FORN); lx_next();
                n->a = pexpr(0); pexpect(',', "',' expected"); n->b = pexpr(0);
                if (paccept(',')) n->c = pexpr(0);
                pexpect(TK_DO, "'do' expected");
                n->u.lv = pdeclare(n1);
                n->d = pblock();
            } else {
                n = nnew(S_FORIN);
                struct lstr *names[3]; int nn = 0; names[nn++] = n1;
                while (paccept(',')) { if (nn < 3) names[nn++] = pname(); else pname(); }
                pexpect(TK_IN, "'in' expected");
                n->b = pexplist();
                pexpect(TK_DO, "'do' expected");
                struct nd *last = 0;
                for (int i = 0; i < nn; i++) { struct nd *v = nnew(N_LOCAL); v->u.lv = pdeclare(names[i]); if (last) last->nx = v; else n->a = v; last = v; }
                n->d = pblock();
            }
            FS->nact = sa;
            pexpect(TK_END, "'end' expected");
            return n;
        }
        case TK_FUNCTION: {
            lx_next();
            struct nd *target = pvar(pname()); int ismethod = 0;
            while (LX.t == '.' || LX.t == ':') {
                int colon = LX.t == ':'; lx_next();
                struct nd *i = nnew(N_INDEX), *k = nnew(N_STR); k->u.s = pname(); i->a = target; i->b = k; target = i;
                if (colon) { ismethod = 1; break; }
            }
            n = nnew(S_ASSIGN); struct nd *fn = nnew(N_FUNC);
            pfuncbody(fn, ismethod);
            n->a = target; n->b = fn;
            return n;
        }
        case TK_LOCAL: {
            lx_next();
            if (LX.t == TK_FUNCTION) {
                lx_next(); n = nnew(S_LFUNC);
                n->u.lv = pdeclare(pname());
                n->a = nnew(N_FUNC); pfuncbody(n->a, 0);
                return n;
            }
            struct lstr *names[LUA_MAXP + 8]; int nn = 0;
            do { if (nn < LUA_MAXP + 8) names[nn++] = pname(); else pname(); } while (paccept(','));
            n = nnew(S_LOCAL);
            if (paccept('=')) n->b = pexplist();
            struct nd *last = 0;
            for (int i = 0; i < nn; i++) { struct nd *v = nnew(N_LOCAL); v->u.lv = pdeclare(names[i]); if (last) last->nx = v; else n->a = v; last = v; }
            return n;
        }
        case TK_RETURN: {
            n = nnew(S_RETURN); lx_next();
            if (!pblockend() && LX.t != ';') n->a = pexplist();
            paccept(';');
            if (!pblockend()) pfail("'return' must be the last statement");
            return n;
        }
        default: {
            struct nd *e = psimple();
            if (LX.t == '=' || LX.t == ',') {
                n = nnew(S_ASSIGN);
                struct nd *last = e; n->a = e;
                if (e->k != N_LOCAL && e->k != N_UP && e->k != N_GLOBAL && e->k != N_INDEX) { pfail("cannot assign to this expression"); return n; }
                while (paccept(',')) {
                    struct nd *t = psimple();
                    if (t->k != N_LOCAL && t->k != N_UP && t->k != N_GLOBAL && t->k != N_INDEX) { pfail("cannot assign to this expression"); return n; }
                    last->nx = t; last = t;
                }
                pexpect('=', "'=' expected");
                n->b = pexplist();
                return n;
            }
            if (e->k != N_CALL && e->k != N_METHOD) { pfail("syntax error (expression is not a statement)"); return nnew(S_DO); }
            n = nnew(S_CALL); n->a = e; return n;
        }
    }
}
static struct nd *pblock(void) {
    struct nd *first = 0, *last = 0;
    if (++pdepth > LUA_PDEPTH) { pfail("blocks are nested too deeply"); pdepth--; return 0; }
    while (!pblockend() && !L.err) {
        int isret = LX.t == TK_RETURN;
        struct nd *s = pstat();
        if (s) { if (last) last->nx = s; else first = s; last = s; }
        if (isret) break;
    }
    pdepth--;
    return first;
}
/* разобрать исходник в прототип главной функции; 0 при ошибке (сообщение в L.emsg) */
static struct fproto *lua_parse(const char *src, int len, const char *chunk) {
    LX.p = src; LX.end = src + len; LX.line = 1; LX.t = 0;
    PCHUNK = chunk; pdepth = 0;
    struct fproto *fp = lperm(sizeof *fp); struct fstate *fs = lperm(sizeof *fs);
    if (!fp || !fs) return 0;
    fs->fp = fp; fp->chunk = chunk; FS = fs;
    lx_next();
    fp->body = pblock();
    if (LX.t != TK_EOF) pfail("unexpected symbol (extra 'end'?)");
    fp->nslots = fs->nslots;
    FS = 0;
    return L.err ? 0 : fp;
}

/* ---------- Вычисление ---------- */
enum { ST_NONE, ST_BREAK, ST_RET };
static void lcheck_abort(void);
#define LSTEP() do { if ((++L.steps & 0x1FFF) == 0) { lcheck_abort(); if (L.err) return ST_NONE; } } while (0)

static struct lval lev(struct nd *n, struct frame *f);
static int lexec_block(struct nd *n, struct frame *f);
static int lcall(struct lval fn, struct lval *args, int n);
static int lcfn(int id, struct lval *a, int n);

static struct lval lindex(struct lval o, struct lval k) {
    for (int guard = 0; guard < 8; guard++) {
        if (o.t == LT_TAB) {
            struct lval v = lrawget(o.u.tb, k);
            if (v.t != LT_NIL || !o.u.tb->meta) return v;
            struct lval h = lrawget(o.u.tb->meta, (struct lval){ LT_STR, { .s = L.s_index } });
            if (h.t == LT_TAB) { o = h; continue; }
            if (h.t == LT_FUN || h.t == LT_CFN) { struct lval a[2] = { o, k }; lcall(h, a, 2); return L.nrv ? L.rv[0] : LNIL; }
            return LNIL;
        }
        if (o.t == LT_STR && L.strlib) return lrawget(L.strlib, k);
        break;
    }
    lerr3("attempt to index a ", ltname(o), " value"); return LNIL;
}
static int lnum(struct lval v, lint *out) {
    if (v.t == LT_INT) { *out = v.u.i; return 1; }
    if (v.t == LT_STR) {
        const char *s = v.u.s->s; int neg = 0; lint r = 0, d = 0;
        while (*s == ' ') s++;
        if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { s += 2; while (lhexv((u8)*s) >= 0) { r = r * 16 + lhexv((u8)*s); s++; d++; } }
        else while (lisdigit((u8)*s)) { r = r * 10 + (*s - '0'); s++; d++; }
        while (*s == ' ') s++;
        if (!d || *s) return 0;
        *out = neg ? -r : r; return 1;
    }
    return 0;
}
static int lts(struct lval v, char *buf, const char **out) {      /* tostring: возвращает длину */
    int n = 0;
    switch (v.t) {
        case LT_NIL: *out = "nil"; return 3;
        case LT_BOOL: *out = v.u.i ? "true" : "false"; return v.u.i ? 4 : 5;
        case LT_STR: *out = v.u.s->s; return (int)v.u.s->len;
        case LT_INT: {
            unsigned long long u = v.u.i < 0 ? 0ull - (unsigned long long)v.u.i : (unsigned long long)v.u.i;
            char t[24]; int m = 0;
            if (!u) t[m++] = '0';
            while (u) { t[m++] = (char)('0' + u % 10); u /= 10; }
            if (v.u.i < 0) buf[n++] = '-';
            while (m) buf[n++] = t[--m];
            buf[n] = 0; *out = buf; return n;
        }
        default: {
            const char *pre = v.t == LT_TAB ? "table: 0x" : v.t == LT_FUN ? "function: 0x" : "function: builtin: 0x";
            while (pre[n]) { buf[n] = pre[n]; n++; }
            unsigned long long p = v.t == LT_TAB ? (unsigned long long)v.u.tb : v.t == LT_FUN ? (unsigned long long)v.u.f : (unsigned long long)v.u.i;
            for (int i = 7; i >= 0; i--) buf[n++] = "0123456789abcdef"[(p >> (i * 4)) & 15];
            buf[n] = 0; *out = buf; return n;
        }
    }
}
static int lless(struct lval a, struct lval b) {
    if (a.t == LT_INT && b.t == LT_INT) return a.u.i < b.u.i;
    if (a.t == LT_STR && b.t == LT_STR) {
        u32 n = a.u.s->len < b.u.s->len ? a.u.s->len : b.u.s->len;
        for (u32 i = 0; i < n; i++) if (a.u.s->s[i] != b.u.s->s[i]) return (u8)a.u.s->s[i] < (u8)b.u.s->s[i];
        return a.u.s->len < b.u.s->len;
    }
    lerr3("attempt to compare ", ltname(a), " with "); if (L.err) { int n = lua_slen(L.emsg); const char *t = ltname(b); for (int i = 0; t[i] && n < 199; i++) L.emsg[n++] = t[i]; L.emsg[n] = 0; }
    return 0;
}
static struct lval lconcat(struct lval a, struct lval b) {
    char b1[32], b2[32]; const char *s1, *s2; int n1, n2;
    if ((a.t != LT_STR && a.t != LT_INT) || (b.t != LT_STR && b.t != LT_INT)) { lerr3("attempt to concatenate a ", ltname((a.t != LT_STR && a.t != LT_INT) ? a : b), " value"); return LNIL; }
    n1 = lts(a, b1, &s1); n2 = lts(b, b2, &s2);
    struct lstr *r = lgalloc(8 + (u32)n1 + (u32)n2 + 1, GK_STR);
    if (!r) return LNIL;
    r->len = (u32)(n1 + n2);
    for (int i = 0; i < n1; i++) r->s[i] = s1[i];
    for (int i = 0; i < n2; i++) r->s[n1 + i] = s2[i];
    r->s[n1 + n2] = 0; r->hash = lhash(r->s, r->len);
    struct lval v; v.t = LT_STR; v.u.s = r; return v;
}
static struct lval larith(int op, struct lval a, struct lval b) {
    lint x, y;
    if (!lnum(a, &x) || !lnum(b, &y)) { lerr3("attempt to perform arithmetic on a ", ltname(lnum(a, &x) ? b : a), " value"); return LNIL; }
    unsigned long long ux = (unsigned long long)x, uy = (unsigned long long)y;
    switch (op) {
        case '+': return lint_v((lint)(ux + uy));
        case '-': return lint_v((lint)(ux - uy));
        case '*': return lint_v((lint)(ux * uy));
        case '/': case 200 + (TK_IDIV - 256): {
            if (!y) { lerr("attempt to divide by zero"); return LNIL; }
            if (y == -1) return lint_v((lint)(0ull - ux));
            lint q = x / y; if ((x % y != 0) && ((x < 0) != (y < 0))) q--;
            return lint_v(q);
        }
        case '%': {
            if (!y) { lerr("attempt to perform 'n%0'"); return LNIL; }
            if (y == -1) return lint_v(0);
            lint r = x % y; if (r != 0 && ((r < 0) != (y < 0))) r += y;
            return lint_v(r);
        }
        case '^': {
            if (y < 0) return lint_v(0);
            unsigned long long r = 1, bs = ux;
            for (lint e = y; e > 0; e >>= 1) { if (e & 1) r *= bs; bs *= bs; }
            return lint_v((lint)r);
        }
    }
    return LNIL;
}
static struct lval lget_local(struct lvar *lv, struct frame *f) {
    struct lval v = f->v[lv->slot];
    return lv->captured ? v.u.bx->v : v;
}
static void lset_local(struct lvar *lv, struct frame *f, struct lval v) {
    if (lv->captured) f->v[lv->slot].u.bx->v = v; else f->v[lv->slot] = v;
}
static void ldeclare_local(struct lvar *lv, struct frame *f, struct lval v) {   /* новая переменная (для captured - новая коробка) */
    if (lv->captured) {
        struct lbox *b = lgalloc(sizeof *b, GK_BOX);
        if (!b) return;
        b->v = v; f->v[lv->slot].t = LT_BOX; f->v[lv->slot].u.bx = b;
    } else f->v[lv->slot] = v;
}
static int levlist(struct nd *list, struct frame *f, struct lval *out, int max) {
    int n = 0;
    for (struct nd *e = list; e; e = e->nx) {
        if (L.err) return 0;
        int last = e->nx == 0;
        if (last && (e->k == N_CALL || e->k == N_METHOD)) {
            lev(e, f);
            if (L.err) return 0;
            for (int i = 0; i < L.nrv; i++) { if (n >= max) { lerr("too many values"); return 0; } out[n++] = L.rv[i]; }
        } else {
            if (n >= max) { lerr("too many values"); return 0; }
            out[n++] = lev(e, f);
        }
    }
    return L.err ? 0 : n;
}
static void lassign(struct nd *t, struct frame *f, struct lval v) {
    switch (t->k) {
        case N_LOCAL: lset_local(t->u.lv, f, v); break;
        case N_UP: f->cl->up[t->u.idx]->v = v; break;
        case N_GLOBAL: lrawset(L.globals, (struct lval){ LT_STR, { .s = t->u.s } }, v); break;
        case N_INDEX: {
            struct lval o = lev(t->a, f); if (L.err) return;
            struct lval k = lev(t->b, f); if (L.err) return;
            if (o.t != LT_TAB) { lerr3("attempt to index a ", ltname(o), " value"); return; }
            lrawset(o.u.tb, k, v);
            break;
        }
    }
}
static struct lval lev(struct nd *n, struct frame *f) {
    switch (n->k) {
        case N_NIL: return LNIL;
        case N_TRUE: return lbool_v(1);
        case N_FALSE: return lbool_v(0);
        case N_INT: return lint_v(n->u.i);
        case N_STR: { struct lval v; v.t = LT_STR; v.u.s = n->u.s; return v; }
        case N_LOCAL: return lget_local(n->u.lv, f);
        case N_UP: return f->cl->up[n->u.idx]->v;
        case N_GLOBAL: return lrawget(L.globals, (struct lval){ LT_STR, { .s = n->u.s } });
        case N_PAREN: return lev(n->a, f);
        case N_INDEX: {
            struct lval o = lev(n->a, f); if (L.err) return LNIL;
            struct lval k = lev(n->b, f); if (L.err) return LNIL;
            return lindex(o, k);
        }
        case N_AND: { struct lval a = lev(n->a, f); if (L.err || !ltruthy(a)) return a; return lev(n->b, f); }
        case N_OR: { struct lval a = lev(n->a, f); if (L.err || ltruthy(a)) return a; return lev(n->b, f); }
        case N_NOT: { struct lval a = lev(n->a, f); return lbool_v(!ltruthy(a)); }
        case N_NEG: {
            struct lval a = lev(n->a, f); lint x;
            if (L.err) return LNIL;
            if (!lnum(a, &x)) { lerr3("attempt to perform arithmetic on a ", ltname(a), " value"); return LNIL; }
            return lint_v((lint)(0ull - (unsigned long long)x));
        }
        case N_LEN: {
            struct lval a = lev(n->a, f);
            if (L.err) return LNIL;
            if (a.t == LT_STR) return lint_v((lint)a.u.s->len);
            if (a.t == LT_TAB) return lint_v((lint)a.u.tb->an);
            lerr3("attempt to get length of a ", ltname(a), " value"); return LNIL;
        }
        case N_BIN: {
            struct lval a = lev(n->a, f); if (L.err) return LNIL;
            struct lval b = lev(n->b, f); if (L.err) return LNIL;
            switch (n->op) {
                case TK_CAT - 256 + 200: return lconcat(a, b);
                case TK_EQ - 256 + 200: return lbool_v(lraweq(a, b));
                case TK_NE - 256 + 200: return lbool_v(!lraweq(a, b));
                case '<': return lbool_v(lless(a, b));
                case '>': return lbool_v(lless(b, a));
                case TK_LE - 256 + 200: { int r = lless(b, a); return lbool_v(!r && !L.err); }
                case TK_GE - 256 + 200: { int r = lless(a, b); return lbool_v(!r && !L.err); }
                default: return larith(n->op, a, b);
            }
        }
        case N_TABLE: {
            struct ltab *t = lnewtab(); struct lval tv = ltab_v(t);
            lint idx = 1;
            for (struct nd *it = n->a; it; it = it->nx) {
                if (it->op) {
                    struct lval k = lev(it->a, f); if (L.err) return LNIL;
                    struct lval v = lev(it->b, f); if (L.err) return LNIL;
                    lrawset(t, k, v);
                } else if (it->nx == 0 && (it->b->k == N_CALL || it->b->k == N_METHOD)) {
                    lev(it->b, f); if (L.err) return LNIL;
                    for (int i = 0; i < L.nrv; i++) lrawset(t, lint_v(idx++), L.rv[i]);
                } else {
                    struct lval v = lev(it->b, f); if (L.err) return LNIL;
                    lrawset(t, lint_v(idx++), v);
                }
                if (L.err) return LNIL;
            }
            return tv;
        }
        case N_FUNC: {
            struct fproto *fp = n->u.fp;
            struct lclo *c = lgalloc(sizeof *c, GK_CLO);
            if (!c) return LNIL;
            c->fp = fp;
            for (int i = 0; i < fp->nup; i++)
                c->up[i] = fp->upfl[i] ? f->v[fp->upix[i]].u.bx : f->cl->up[fp->upix[i]];
            struct lval v; v.t = LT_FUN; v.u.f = c; return v;
        }
        case N_CALL: case N_METHOD: {
            struct lval args[LUA_MAXARGS], fn;
            int na = 0;
            if (n->k == N_CALL) {
                fn = lev(n->a, f); if (L.err) return LNIL;
                if (n->b) { na = levlist(n->b, f, args, LUA_MAXARGS); if (L.err) return LNIL; }
            } else {
                struct lval o = lev(n->a, f); if (L.err) return LNIL;
                struct lval key; key.t = LT_STR; key.u.s = n->u.s;
                fn = lindex(o, key); if (L.err) return LNIL;
                args[0] = o; na = 1;
                if (n->b) { int m = levlist(n->b, f, args + 1, LUA_MAXARGS - 1); if (L.err) return LNIL; na += m; }
            }
            L.line = n->line;
            lcall(fn, args, na);
            if (L.err) return LNIL;
            return L.nrv ? L.rv[0] : LNIL;
        }
    }
    return LNIL;
}
static int lexec(struct nd *n, struct frame *f) {
    switch (n->k) {
        case S_LOCAL: {
            struct lval vals[LUA_MAXP + 8]; int nv = 0;
            if (n->b) { nv = levlist(n->b, f, vals, LUA_MAXP + 8); if (L.err) return ST_NONE; }
            int i = 0;
            for (struct nd *v = n->a; v; v = v->nx, i++) ldeclare_local(v->u.lv, f, i < nv ? vals[i] : LNIL);
            return ST_NONE;
        }
        case S_LFUNC: {
            ldeclare_local(n->u.lv, f, LNIL);
            struct lval fv = lev(n->a, f); if (L.err) return ST_NONE;
            lset_local(n->u.lv, f, fv);
            return ST_NONE;
        }
        case S_ASSIGN: {
            if (!n->a->nx && !n->b->nx) { struct lval v = lev(n->b, f); if (L.err) return ST_NONE; lassign(n->a, f, v); return ST_NONE; }
            struct lval vals[LUA_MAXP + 8]; int nv = levlist(n->b, f, vals, LUA_MAXP + 8); if (L.err) return ST_NONE;
            int i = 0;
            for (struct nd *t = n->a; t; t = t->nx, i++) { lassign(t, f, i < nv ? vals[i] : LNIL); if (L.err) return ST_NONE; }
            return ST_NONE;
        }
        case S_CALL: lev(n->a, f); return ST_NONE;
        case S_DO: return lexec_block(n->a, f);
        case S_WHILE:
            for (;;) {
                LSTEP();
                struct lval c = lev(n->a, f); if (L.err) return ST_NONE;
                if (!ltruthy(c)) return ST_NONE;
                int st = lexec_block(n->b, f);
                if (st == ST_BREAK) return ST_NONE;
                if (st) return st;
            }
        case S_REPEAT:
            for (;;) {
                LSTEP();
                int st = lexec_block(n->a, f);
                if (st == ST_BREAK) return ST_NONE;
                if (st) return st;
                struct lval c = lev(n->b, f); if (L.err) return ST_NONE;
                if (ltruthy(c)) return ST_NONE;
            }
        case S_IF: {
            struct lval c = lev(n->a, f); if (L.err) return ST_NONE;
            if (ltruthy(c)) return lexec_block(n->b, f);
            if (n->c) return lexec(n->c, f);
            return ST_NONE;
        }
        case S_FORN: {
            struct lval a = lev(n->a, f); if (L.err) return ST_NONE;
            struct lval b = lev(n->b, f); if (L.err) return ST_NONE;
            struct lval c = n->c ? lev(n->c, f) : lint_v(1); if (L.err) return ST_NONE;
            lint x, y, s;
            if (!lnum(a, &x) || !lnum(b, &y) || !lnum(c, &s)) { lerr("'for' initial value must be a number"); return ST_NONE; }
            if (s == 0) { lerr("'for' step is zero"); return ST_NONE; }
            for (;;) {
                LSTEP();
                if (s > 0 ? x > y : x < y) break;
                ldeclare_local(n->u.lv, f, lint_v(x));
                int st = lexec_block(n->d, f);
                if (L.err) return ST_NONE;
                if (st == ST_BREAK) return ST_NONE;
                if (st) return st;
                if (s > 0 ? (y - x < s) : (x - y < -s)) break;
                x += s;
            }
            return ST_NONE;
        }
        case S_FORIN: {
            struct lval init[3]; int ni = levlist(n->b, f, init, 3);
            if (L.err) return ST_NONE;
            struct lval fn = ni > 0 ? init[0] : LNIL, state = ni > 1 ? init[1] : LNIL, ctl = ni > 2 ? init[2] : LNIL;
            for (;;) {
                LSTEP();
                struct lval args[2] = { state, ctl };
                lcall(fn, args, 2);
                if (L.err) return ST_NONE;
                if (!L.nrv || L.rv[0].t == LT_NIL) return ST_NONE;
                struct lval rv[LUA_MAXRET]; int nr = L.nrv;
                for (int i = 0; i < nr; i++) rv[i] = L.rv[i];
                ctl = rv[0];
                int i = 0;
                for (struct nd *v = n->a; v; v = v->nx, i++) ldeclare_local(v->u.lv, f, i < nr ? rv[i] : LNIL);
                int st = lexec_block(n->d, f);
                if (L.err) return ST_NONE;
                if (st == ST_BREAK) return ST_NONE;
                if (st) return st;
            }
        }
        case S_RETURN: {
            struct lval vals[LUA_MAXRET]; int nv = 0;
            if (n->a) { nv = levlist(n->a, f, vals, LUA_MAXRET); if (L.err) return ST_NONE; }
            for (int i = 0; i < nv; i++) L.rv[i] = vals[i];
            L.nrv = nv;
            return ST_RET;
        }
        case S_BREAK: return ST_BREAK;
    }
    return ST_NONE;
}
static void lcheck_abort(void);
static int lexec_block(struct nd *n, struct frame *f) {
    for (; n; n = n->nx) {
        L.line = n->line;
        if ((++L.steps & 0x1FFF) == 0) { lcheck_abort(); if (L.err) return ST_NONE; }
        int st = lexec(n, f);
        if (L.err) return ST_NONE;
        if (st) return st;
    }
    return ST_NONE;
}
static int lcall(struct lval fn, struct lval *args, int n) {
    if (L.err) return 0;
    if (fn.t == LT_CFN) { L.nrv = 0; int r = lcfn((int)fn.u.i, args, n); L.nrv = L.err ? 0 : r; return L.nrv; }
    if (fn.t != LT_FUN) { lerr3("attempt to call a ", ltname(fn), " value"); return 0; }
    struct fproto *fp = fn.u.f->fp;
    if (L.depth >= LUA_MAXDEPTH) { lerr("stack overflow"); return 0; }
    if (L.sp + fp->nslots + 1 > LUA_STACKN) { lerr("stack overflow"); return 0; }
    struct lval *base = lua_stk + L.sp; int sp0 = L.sp;
    L.sp += fp->nslots + 1; L.depth++;
    for (int i = 0; i < fp->nslots + 1; i++) base[i] = LNIL;
    struct frame fr = { base, fn.u.f };
    for (int i = 0; i < fp->np; i++) ldeclare_local(fp->params[i], &fr, i < n ? args[i] : LNIL);
    int sl = L.line; const char *sc = L.chunk;
    L.chunk = fp->chunk;
    int st = lexec_block(fp->body, &fr);
    L.sp = sp0; L.depth--;
    L.chunk = sc; if (!L.err) L.line = sl;
    if (st != ST_RET) L.nrv = 0;
    return L.err ? 0 : L.nrv;
}

/* ---------- Стандартная библиотека ---------- */
#ifndef K_UP
#define K_UP 0x101
#define K_DOWN 0x102
#define K_LEFT 0x103
#define K_RIGHT 0x104
#define K_DEL 0x105
#define K_HOME 0x106
#define K_END 0x107
#define K_PGUP 0x108
#define K_PGDN 0x109
#define K_INS 0x10A
#define K_F1 0x110
#endif
enum { C_PRINT = 1, C_TOSTRING, C_TONUMBER, C_TYPE, C_PAIRS, C_IPAIRS, C_NEXT, C_INEXT, C_PCALL, C_ERROR, C_ASSERT, C_LOAD, C_DOFILE,
       C_SETMT, C_GETMT, C_RAWGET, C_RAWSET, C_COLLECT,
       C_S_LEN, C_S_SUB, C_S_UPPER, C_S_LOWER, C_S_REP, C_S_BYTE, C_S_CHAR, C_S_FIND, C_S_FORMAT, C_S_REVERSE,
       C_T_INSERT, C_T_REMOVE, C_T_CONCAT, C_T_SORT, C_T_UNPACK,
       C_M_FLOOR, C_M_ABS, C_M_MAX, C_M_MIN, C_M_RANDOM, C_M_RSEED, C_M_SQRT,
       C_O_TIME, C_O_DATE, C_O_MS, C_O_SLEEP,
       C_TM_CLEAR, C_TM_AT, C_TM_COLOR, C_TM_WRITE, C_TM_PUT, C_TM_FILL, C_TM_KEY, C_TM_GETKEY, C_TM_READLINE, C_TM_CURSOR, C_TM_SIZE,
       C_FS_LIST, C_FS_READ, C_FS_WRITE, C_FS_APPEND, C_FS_REMOVE, C_FS_EXISTS, C_FS_ISDIR, C_FS_MKDIR, C_EDIT };
static const struct lreg { const char *lib, *name; u8 id; } lua_reg[] = {
    {0,"print",C_PRINT},{0,"tostring",C_TOSTRING},{0,"tonumber",C_TONUMBER},{0,"type",C_TYPE},{0,"pairs",C_PAIRS},{0,"ipairs",C_IPAIRS},
    {0,"next",C_NEXT},{0,"pcall",C_PCALL},{0,"error",C_ERROR},{0,"assert",C_ASSERT},{0,"load",C_LOAD},{0,"dofile",C_DOFILE},
    {0,"setmetatable",C_SETMT},{0,"getmetatable",C_GETMT},{0,"rawget",C_RAWGET},{0,"rawset",C_RAWSET},{0,"collectgarbage",C_COLLECT},{0,"edit",C_EDIT},
    {"string","len",C_S_LEN},{"string","sub",C_S_SUB},{"string","upper",C_S_UPPER},{"string","lower",C_S_LOWER},{"string","rep",C_S_REP},
    {"string","byte",C_S_BYTE},{"string","char",C_S_CHAR},{"string","find",C_S_FIND},{"string","format",C_S_FORMAT},{"string","reverse",C_S_REVERSE},
    {"table","insert",C_T_INSERT},{"table","remove",C_T_REMOVE},{"table","concat",C_T_CONCAT},{"table","sort",C_T_SORT},{"table","unpack",C_T_UNPACK},
    {"math","floor",C_M_FLOOR},{"math","ceil",C_M_FLOOR},{"math","abs",C_M_ABS},{"math","max",C_M_MAX},{"math","min",C_M_MIN},
    {"math","random",C_M_RANDOM},{"math","randomseed",C_M_RSEED},{"math","sqrt",C_M_SQRT},
    {"os","time",C_O_TIME},{"os","date",C_O_DATE},{"os","ms",C_O_MS},{"os","sleep",C_O_SLEEP},
    {"term","clear",C_TM_CLEAR},{"term","at",C_TM_AT},{"term","color",C_TM_COLOR},{"term","write",C_TM_WRITE},{"term","put",C_TM_PUT},
    {"term","fill",C_TM_FILL},{"term","key",C_TM_KEY},{"term","getkey",C_TM_GETKEY},{"term","readline",C_TM_READLINE},
    {"term","cursor",C_TM_CURSOR},{"term","size",C_TM_SIZE},
    {"fs","list",C_FS_LIST},{"fs","read",C_FS_READ},{"fs","write",C_FS_WRITE},{"fs","append",C_FS_APPEND},{"fs","remove",C_FS_REMOVE},
    {"fs","exists",C_FS_EXISTS},{"fs","isdir",C_FS_ISDIR},{"fs","mkdir",C_FS_MKDIR},
};
#define LUA_NREG ((int)(sizeof lua_reg / sizeof lua_reg[0]))
static int lua_curid;
static const char *lcname(int id) { for (int i = 0; i < LUA_NREG; i++) if (lua_reg[i].id == id) return lua_reg[i].name; return "?"; }
static void largerr(int i, const char *what) {
    char nb[8]; nb[0] = (char)('0' + i + 1); nb[1] = 0;
    lerr3("bad argument #", nb, " to '"); if (L.err) { int n = lua_slen(L.emsg); const char *nm = lcname(lua_curid); for (int k = 0; nm[k] && n < 150; k++) L.emsg[n++] = nm[k]; L.emsg[n++] = '\''; L.emsg[n++] = ' '; L.emsg[n++] = '('; for (int k = 0; what[k] && n < 197; k++) L.emsg[n++] = what[k]; L.emsg[n++] = ')'; L.emsg[n] = 0; }
}
static int ret1(struct lval v) { L.rv[0] = v; return 1; }
static int arg_i(struct lval *a, int n, int i, lint *out) {
    if (i < n && lnum(a[i], out)) return 1;
    largerr(i, "number expected"); return 0;
}
static lint arg_io(struct lval *a, int n, int i, lint def) { lint v; if (i >= n || a[i].t == LT_NIL) return def; if (lnum(a[i], &v)) return v; largerr(i, "number expected"); return def; }
static int arg_s(struct lval *a, int n, int i, const char **s, u32 *len) {
    if (i < n && a[i].t == LT_STR) { *s = a[i].u.s->s; *len = a[i].u.s->len; return 1; }
    if (i < n && a[i].t == LT_INT) { struct lval t = lcstr_v(""); char b[32]; const char *p; int m = lts(a[i], b, &p); t = lstr_v(p, (u32)m); a[i] = t; *s = t.u.s->s; *len = t.u.s->len; return 1; }
    largerr(i, "string expected"); return 0;
}
static int arg_t(struct lval *a, int n, int i, struct ltab **t) {
    if (i < n && a[i].t == LT_TAB) { *t = a[i].u.tb; return 1; }
    largerr(i, "table expected"); return 0;
}
static void lua_putstr(const char *s, int n) { for (int i = 0; i < n; i++) putc(s[i]); }
static void lcheck_abort(void) {
    L.since_key += 0x2000;
    if (L.since_key > 200000u) {
        int k = key_poll();
        if (k >= 0) {
            if (k == 27) { lerr("interrupted (Esc)"); L.interrupted = 1; return; }
            if (L.npend < 16) L.pend[L.npend++] = k;
        }
    }
}
static int lpoll_key(void) {                               /* -1 если клавиш нет */
    L.since_key = 0;
    if (L.npend) { int k = L.pend[0]; for (int i = 1; i < L.npend; i++) L.pend[i - 1] = L.pend[i]; L.npend--; return k; }
    return key_poll();
}
static u32 lrand32(void) { u32 x = L.seed; x ^= x << 13; x ^= x >> 17; x ^= x << 5; L.seed = x ? x : 2463534242u; return L.seed; }

static int lfs_wok(const char *p) {                        /* писать можно только в /lua */
    if (p[0] != '/' || p[1] != 'l' || p[2] != 'u' || p[3] != 'a' || (p[4] && p[4] != '/')) return 0;
    for (int i = 0; p[i]; i++) if (p[i] == '.' && p[i + 1] == '.') return 0;
    return 1;
}
static struct lval lmkclosure(struct fproto *fp) {
    struct lclo *c = lgalloc(sizeof *c, GK_CLO); struct lval v; v.t = LT_FUN; v.u.f = c;
    if (c) c->fp = fp;
    return v;
}
static int lreadfile(const char *path, const char **data, int *len) {
    int i = fs_resolve(path);
    if (i < 0 || fsn[i].dir) return 0;
    *data = (const char *)fsdata[i]; *len = fsn[i].size; return 1;
}
static struct lval lsetmsg(void) {                         /* значение текущей ошибки (для pcall/load) */
    if (L.errv.t != LT_NIL) return L.errv;
    return lcstr_v(L.emsg);
}
static void lclearerr(void) { L.err = 0; L.errv = LNIL; L.interrupted = 0; }
static int lfmt(struct lval *a, int n, char *out, int max) {
    const char *f; u32 fl; int o = 0, ai = 1;
    if (!arg_s(a, n, 0, &f, &fl)) return -1;
    for (u32 i = 0; i < fl; i++) {
        if (f[i] != '%') { if (o < max - 1) out[o++] = f[i]; continue; }
        i++;
        if (i >= fl) { lerr("invalid format"); return -1; }
        if (f[i] == '%') { if (o < max - 1) out[o++] = '%'; continue; }
        int left = 0, zero = 0, width = 0, prec = -1;
        while (i < fl && (f[i] == '-' || f[i] == '0')) { if (f[i] == '-') left = 1; else zero = 1; i++; }
        while (i < fl && lisdigit((u8)f[i])) { width = width * 10 + (f[i] - '0'); i++; }
        if (i < fl && f[i] == '.') { i++; prec = 0; while (i < fl && lisdigit((u8)f[i])) { prec = prec * 10 + (f[i] - '0'); i++; } }
        if (i >= fl) { lerr("invalid format"); return -1; }
        char c = f[i], tmp[80]; const char *s = tmp; int sl = 0;
        if (c == 'd' || c == 'i' || c == 'u' || c == 'x' || c == 'X' || c == 'c') {
            lint v; if (ai >= n || !lnum(a[ai], &v)) { largerr(ai, "number expected"); return -1; }
            ai++;
            if (c == 'c') { tmp[0] = (char)v; sl = 1; }
            else if (c == 'd' || c == 'i' || c == 'u') { const char *p; sl = lts(lint_v(v), tmp, &p); }
            else { unsigned long long u = (unsigned long long)v; char t[20]; int m = 0; const char *dg = c == 'x' ? "0123456789abcdef" : "0123456789ABCDEF"; if (!u) t[m++] = '0'; while (u) { t[m++] = dg[u & 15]; u >>= 4; } while (m) tmp[sl++] = t[--m]; }
        } else if (c == 's') {
            if (ai >= n) { largerr(ai, "value expected"); return -1; }
            char b2[40]; const char *p; int m = lts(a[ai], b2, &p); ai++;
            if (prec >= 0 && m > prec) m = prec;
            s = p; sl = m;
            char sb[1]; (void)sb;
            int pad = width > sl ? width - sl : 0;
            if (!left) while (pad-- > 0 && o < max - 1) out[o++] = ' ';
            for (int k = 0; k < sl && o < max - 1; k++) out[o++] = s[k];
            if (left) while (pad-- > 0 && o < max - 1) out[o++] = ' ';
            continue;
        } else { lerr("invalid format (use %d %s %c %x %%)"); return -1; }
        int pad = width > sl ? width - sl : 0;
        if (!left && zero && c != 'c') {
            int k = 0; if (tmp[0] == '-') { if (o < max - 1) out[o++] = '-'; k = 1; }
            while (pad-- > 0 && o < max - 1) out[o++] = '0';
            for (; k < sl && o < max - 1; k++) out[o++] = tmp[k];
        } else {
            if (!left) while (pad-- > 0 && o < max - 1) out[o++] = ' ';
            for (int k = 0; k < sl && o < max - 1; k++) out[o++] = tmp[k];
            if (left) while (pad-- > 0 && o < max - 1) out[o++] = ' ';
        }
    }
    out[o] = 0;
    return o;
}
static int lsort_less(struct lval cmp, struct lval x, struct lval y) {
    if (cmp.t == LT_NIL) return lless(x, y);
    struct lval a[2] = { x, y };
    lcall(cmp, a, 2);
    return !L.err && L.nrv && ltruthy(L.rv[0]);
}
static void lsort(struct ltab *t, struct lval cmp) {            /* пирамидальная сортировка копии массивной части */
    int n = (int)t->an;
    if (n < 2) return;
    struct lval *v = lgalloc((u32)n * (u32)sizeof(struct lval), GK_VALS);
    if (!v) return;
    for (int i = 0; i < n; i++) v[i] = t->arr[i];
    for (int start = n / 2 - 1; start >= -1; start--) {
        if (start < 0) {
            for (int end = n - 1; end > 0; end--) {
                struct lval tmp = v[0]; v[0] = v[end]; v[end] = tmp;
                int root = 0;
                for (;;) {
                    int ch = root * 2 + 1; if (ch >= end) break;
                    if (ch + 1 < end && lsort_less(cmp, v[ch], v[ch + 1])) ch++;
                    if (L.err) return;
                    if (lsort_less(cmp, v[root], v[ch])) { struct lval t2 = v[root]; v[root] = v[ch]; v[ch] = t2; root = ch; } else break;
                    if (L.err) return;
                }
            }
            break;
        }
        int root = start;
        for (;;) {
            int ch = root * 2 + 1; if (ch >= n) break;
            if (ch + 1 < n && lsort_less(cmp, v[ch], v[ch + 1])) ch++;
            if (L.err) return;
            if (lsort_less(cmp, v[root], v[ch])) { struct lval t2 = v[root]; v[root] = v[ch]; v[ch] = t2; root = ch; } else break;
            if (L.err) return;
        }
    }
    if (t->an == (u32)n) for (int i = 0; i < n; i++) t->arr[i] = v[i];   /* если сравнение не меняло таблицу */
}
static int cfn_term_fill(struct lval *a, int n) {
    lint x, y, w, h; if (!arg_i(a, n, 0, &x) || !arg_i(a, n, 1, &y) || !arg_i(a, n, 2, &w) || !arg_i(a, n, 3, &h)) return 0;
    char ch = ' ';
    if (n > 4) { if (a[4].t == LT_STR && a[4].u.s->len) ch = a[4].u.s->s[0]; else if (a[4].t == LT_INT) ch = (char)a[4].u.i; }
    for (lint j = y; j < y + h; j++) for (lint i = x; i < x + w; i++) if (i >= 0 && i < W && j >= 0 && j < H) put_at((int)i, (int)j, ch, color);
    return 0;
}
static int lcfn(int id, struct lval *a, int n) {
    lua_curid = id;
    switch (id) {
    case C_PRINT: {
        for (int i = 0; i < n; i++) { char b[40]; const char *p; int m = lts(a[i], b, &p); if (i) putc('\t'); lua_putstr(p, m); }
        putc('\n'); return 0;
    }
    case C_TOSTRING: { if (!n) { largerr(0, "value expected"); return 0; } char b[40]; const char *p; int m = lts(a[0], b, &p); return ret1(a[0].t == LT_STR ? a[0] : lstr_v(p, (u32)m)); }
    case C_TONUMBER: {
        if (!n) { largerr(0, "value expected"); return 0; }
        lint v, base = arg_io(a, n, 1, 10);
        if (base != 10 && a[0].t == LT_STR) {
            const char *s = a[0].u.s->s; int neg = 0, d = 0; v = 0;
            if (*s == '-') { neg = 1; s++; }
            while (*s) { int c = lhexv((u8)*s), dv = (*s >= 'g' && *s <= 'z') ? *s - 'a' + 10 : (*s >= 'G' && *s <= 'Z') ? *s - 'A' + 10 : c; if (dv < 0 || dv >= base) { d = 0; break; } v = v * base + dv; s++; d++; }
            return ret1(d ? lint_v(neg ? -v : v) : LNIL);
        }
        return ret1(lnum(a[0], &v) ? lint_v(v) : LNIL);
    }
    case C_TYPE: { if (!n) { largerr(0, "value expected"); return 0; } return ret1(lcstr_v(ltname(a[0]))); }
    case C_PAIRS: { struct ltab *t; if (!arg_t(a, n, 0, &t)) return 0; L.rv[0].t = LT_CFN; L.rv[0].u.i = C_NEXT; L.rv[1] = a[0]; L.rv[2] = LNIL; return 3; }
    case C_IPAIRS: { struct ltab *t; if (!arg_t(a, n, 0, &t)) return 0; L.rv[0].t = LT_CFN; L.rv[0].u.i = C_INEXT; L.rv[1] = a[0]; L.rv[2] = lint_v(0); return 3; }
    case C_NEXT: { struct ltab *t; if (!arg_t(a, n, 0, &t)) return 0; struct lval k, v; if (lnext(t, n > 1 ? a[1] : LNIL, &k, &v)) { L.rv[0] = k; L.rv[1] = v; return 2; } return ret1(LNIL); }
    case C_INEXT: { struct ltab *t; lint i; if (!arg_t(a, n, 0, &t) || !arg_i(a, n, 1, &i)) return 0; struct lval v = lrawget(t, lint_v(i + 1)); if (v.t == LT_NIL) return ret1(LNIL); L.rv[0] = lint_v(i + 1); L.rv[1] = v; return 2; }
    case C_PCALL: {
        if (!n) { largerr(0, "value expected"); return 0; }
        int sp0 = L.sp, d0 = L.depth; const char *ch0 = L.chunk; int ln0 = L.line;
        lcall(a[0], a + 1, n - 1);
        if (L.err) {
            if (L.interrupted) return 0;
            struct lval m = lsetmsg(); lclearerr();
            L.sp = sp0; L.depth = d0; L.chunk = ch0; L.line = ln0;
            L.rv[0] = lbool_v(0); L.rv[1] = m; return 2;
        }
        int nr = L.nrv; if (nr > LUA_MAXRET - 1) nr = LUA_MAXRET - 1;
        for (int i = nr; i > 0; i--) L.rv[i] = L.rv[i - 1];
        L.rv[0] = lbool_v(1); return nr + 1;
    }
    case C_ERROR: {
        struct lval v = n ? a[0] : LNIL; lint lvl = arg_io(a, n, 1, 1);
        if (v.t == LT_STR) {
            if (lvl == 0) { int m = 0; for (u32 i = 0; i < v.u.s->len && m < 199; i++) L.emsg[m++] = v.u.s->s[i]; L.emsg[m] = 0; L.err = 1; L.errv = LNIL; }
            else lerr(v.u.s->s);
        } else if (v.t == LT_NIL) lerr("nil");
        else { lerr("(error object)"); L.errv = v; }
        return 0;
    }
    case C_ASSERT: {
        if (n && ltruthy(a[0])) { for (int i = 0; i < n && i < LUA_MAXRET; i++) L.rv[i] = a[i]; return n < LUA_MAXRET ? n : LUA_MAXRET; }
        if (n > 1 && a[1].t == LT_STR) { int m = 0; for (u32 i = 0; i < a[1].u.s->len && m < 199; i++) L.emsg[m++] = a[1].u.s->s[i]; L.emsg[m] = 0; L.err = 1; L.errv = LNIL; }
        else lerr("assertion failed!");
        return 0;
    }
    case C_LOAD: {
        const char *s; u32 sl; if (!arg_s(a, n, 0, &s, &sl)) return 0;
        const char *nm = "load"; u32 nl = 4;
        if (n > 1 && a[1].t == LT_STR) { nm = a[1].u.s->s; nl = a[1].u.s->len; if (nl && (nm[0] == '=' || nm[0] == '@')) { nm++; nl--; } }
        struct fproto *fp = lua_parse(s, (int)sl, lmks_p(nm, nl)->s);
        if (!fp) { struct lval m = lsetmsg(); lclearerr(); L.rv[0] = LNIL; L.rv[1] = m; return 2; }
        return ret1(lmkclosure(fp));
    }
    case C_DOFILE: {
        const char *p; u32 pl; if (!arg_s(a, n, 0, &p, &pl)) return 0;
        const char *d; int dl;
        if (!lreadfile(p, &d, &dl)) { lerr2("cannot open ", p); return 0; }
        struct lval src = lstr_v(d, (u32)dl);
        struct fproto *fp = lua_parse(src.u.s->s, dl, lmks_p(p, pl)->s);
        if (!fp) return 0;
        lcall(lmkclosure(fp), a, 0);
        return L.nrv;
    }
    case C_SETMT: { struct ltab *t; if (!arg_t(a, n, 0, &t)) return 0; t->meta = (n > 1 && a[1].t == LT_TAB) ? a[1].u.tb : 0; return ret1(a[0]); }
    case C_GETMT: { if (n && a[0].t == LT_TAB && a[0].u.tb->meta) return ret1(ltab_v(a[0].u.tb->meta)); return ret1(LNIL); }
    case C_RAWGET: { struct ltab *t; if (!arg_t(a, n, 0, &t)) return 0; return ret1(lrawget(t, n > 1 ? a[1] : LNIL)); }
    case C_RAWSET: { struct ltab *t; if (!arg_t(a, n, 0, &t)) return 0; lrawset(t, n > 1 ? a[1] : LNIL, n > 2 ? a[2] : LNIL); return ret1(a[0]); }
    case C_COLLECT: {
        lua_gc();
        if (n && a[0].t == LT_STR && a[0].u.s->len == 5) {          /* "count": занято, КБ */
            u32 used = 0; for (u32 off = 0; off < lua_top; off += gc_h(off)->size) if (gc_h(off)->kind != GK_FREE) used += gc_h(off)->size;
            return ret1(lint_v((lint)(used / 1024)));
        }
        return ret1(lint_v(0));
    }
    /* string */
    case C_S_LEN: { const char *s; u32 l; if (!arg_s(a, n, 0, &s, &l)) return 0; return ret1(lint_v(l)); }
    case C_S_SUB: {
        const char *s; u32 l; if (!arg_s(a, n, 0, &s, &l)) return 0;
        lint i = arg_io(a, n, 1, 1), j = arg_io(a, n, 2, -1); if (L.err) return 0;
        if (i < 0) { i = (lint)l + i + 1; if (i < 1) i = 1; } else if (i == 0) i = 1;
        if (j < 0) j = (lint)l + j + 1; else if (j > (lint)l) j = (lint)l;
        if (i > j) return ret1(lcstr_v(""));
        return ret1(lstr_v(s + i - 1, (u32)(j - i + 1)));
    }
    case C_S_UPPER: case C_S_LOWER: {
        const char *s; u32 l; if (!arg_s(a, n, 0, &s, &l)) return 0;
        struct lval r = lstr_v(s, l); char *p = r.u.s->s;
        if (r.u.s->len != l) return 0;                       /* не хватило памяти */
        for (u32 i = 0; i < l; i++) { if (id == C_S_UPPER && p[i] >= 'a' && p[i] <= 'z') p[i] -= 32; else if (id == C_S_LOWER && p[i] >= 'A' && p[i] <= 'Z') p[i] += 32; }
        r.u.s->hash = lhash(p, l); return ret1(r);
    }
    case C_S_REVERSE: {
        const char *s; u32 l; if (!arg_s(a, n, 0, &s, &l)) return 0;
        struct lval r = lstr_v(s, l); char *p = r.u.s->s;
        if (r.u.s->len != l) return 0;
        for (u32 i = 0; i < l; i++) p[i] = s[l - 1 - i];
        r.u.s->hash = lhash(p, l); return ret1(r);
    }
    case C_S_REP: {
        const char *s; u32 l; lint cnt; if (!arg_s(a, n, 0, &s, &l) || !arg_i(a, n, 1, &cnt)) return 0;
        const char *sep = ""; u32 sl = 0; if (n > 2 && a[2].t == LT_STR) { sep = a[2].u.s->s; sl = a[2].u.s->len; }
        if (cnt <= 0) return ret1(lcstr_v(""));
        if ((unsigned long long)cnt * (l + sl) > 100000) { lerr("resulting string too large"); return 0; }
        struct lstr *r = lgalloc(8 + (u32)cnt * (l + sl) + 1, GK_STR); if (!r) return 0;
        u32 o = 0;
        for (lint k = 0; k < cnt; k++) { for (u32 i = 0; i < l; i++) r->s[o++] = s[i]; if (k + 1 < cnt) for (u32 i = 0; i < sl; i++) r->s[o++] = sep[i]; }
        r->len = o; r->s[o] = 0; r->hash = lhash(r->s, o);
        struct lval v; v.t = LT_STR; v.u.s = r; return ret1(v);
    }
    case C_S_BYTE: {
        const char *s; u32 l; if (!arg_s(a, n, 0, &s, &l)) return 0;
        lint i = arg_io(a, n, 1, 1), j = arg_io(a, n, 2, i); if (L.err) return 0;
        if (i < 0) i = (lint)l + i + 1;
        if (j < 0) j = (lint)l + j + 1;
        if (i < 1) i = 1;
        if (j > (lint)l) j = (lint)l;
        int c = 0; for (lint k = i; k <= j && c < LUA_MAXRET; k++) L.rv[c++] = lint_v((u8)s[k - 1]);
        return c;
    }
    case C_S_CHAR: {
        char b[LUA_MAXARGS + 1]; for (int i = 0; i < n && i < LUA_MAXARGS; i++) { lint v; if (!arg_i(a, n, i, &v)) return 0; b[i] = (char)v; }
        return ret1(lstr_v(b, (u32)(n < LUA_MAXARGS ? n : LUA_MAXARGS)));
    }
    case C_S_FIND: {
        const char *s, *p; u32 l, pl; if (!arg_s(a, n, 0, &s, &l) || !arg_s(a, n, 1, &p, &pl)) return 0;
        lint init = arg_io(a, n, 2, 1); if (L.err) return 0;
        if (init < 0) { init = (lint)l + init + 1; if (init < 1) init = 1; } else if (init == 0) init = 1;
        for (lint i = init - 1; i + (lint)pl <= (lint)l; i++) {
            u32 k = 0; while (k < pl && s[i + k] == p[k]) k++;
            if (k == pl) { L.rv[0] = lint_v(i + 1); L.rv[1] = lint_v(i + (lint)pl); return 2; }
        }
        return ret1(LNIL);
    }
    case C_S_FORMAT: { char out[400]; int m = lfmt(a, n, out, sizeof out); if (m < 0) return 0; return ret1(lstr_v(out, (u32)m)); }
    /* table */
    case C_T_INSERT: {
        struct ltab *t; if (!arg_t(a, n, 0, &t)) return 0;
        if (n == 2) { lrawset(t, lint_v((lint)t->an + 1), a[1]); return 0; }
        if (n == 3) {
            lint pos; if (!arg_i(a, n, 1, &pos)) return 0;
            if (pos < 1 || pos > (lint)t->an + 1) { largerr(1, "position out of bounds"); return 0; }
            if (a[2].t == LT_NIL) { largerr(2, "nil values are not supported here"); return 0; }
            lrawset(t, lint_v((lint)t->an + 1), t->an ? t->arr[t->an - 1] : a[2]);
            for (lint k = (lint)t->an - 1; k > pos; k--) t->arr[k - 1] = t->arr[k - 2];
            t->arr[pos - 1] = a[2];
            return 0;
        }
        largerr(1, "wrong number of arguments"); return 0;
    }
    case C_T_REMOVE: {
        struct ltab *t; if (!arg_t(a, n, 0, &t)) return 0;
        lint len = (lint)t->an;
        if (!len) return ret1(LNIL);
        lint pos = arg_io(a, n, 1, len); if (L.err) return 0;
        if (pos < 1 || pos > len) return ret1(LNIL);
        struct lval v = t->arr[pos - 1];
        for (lint k = pos; k < len; k++) t->arr[k - 1] = t->arr[k];
        t->arr[len - 1] = LNIL; t->an--;
        while (t->an && t->arr[t->an - 1].t == LT_NIL) t->an--;
        return ret1(v);
    }
    case C_T_CONCAT: {
        struct ltab *t; if (!arg_t(a, n, 0, &t)) return 0;
        const char *sep = ""; u32 sl = 0; if (n > 1 && a[1].t == LT_STR) { sep = a[1].u.s->s; sl = a[1].u.s->len; }
        lint i = arg_io(a, n, 2, 1), j = arg_io(a, n, 3, (lint)t->an); if (L.err) return 0;
        u32 total = 0;
        for (lint k = i; k <= j; k++) { struct lval v = lrawget(t, lint_v(k)); if (v.t != LT_STR && v.t != LT_INT) { lerr("invalid value in table for 'concat'"); return 0; } char b[32]; const char *p; total += (u32)lts(v, b, &p) + sl; }
        struct lstr *r = lgalloc(8 + total + 1, GK_STR); if (!r) return 0;
        u32 o = 0;
        for (lint k = i; k <= j; k++) { struct lval v = lrawget(t, lint_v(k)); char b[32]; const char *p; int m = lts(v, b, &p); for (int q = 0; q < m; q++) r->s[o++] = p[q]; if (k < j) for (u32 q = 0; q < sl; q++) r->s[o++] = sep[q]; }
        r->len = o; r->s[o] = 0; r->hash = lhash(r->s, o);
        struct lval v; v.t = LT_STR; v.u.s = r; return ret1(v);
    }
    case C_T_SORT: { struct ltab *t; if (!arg_t(a, n, 0, &t)) return 0; lsort(t, n > 1 ? a[1] : LNIL); return 0; }
    case C_T_UNPACK: {
        struct ltab *t; if (!arg_t(a, n, 0, &t)) return 0;
        lint i = arg_io(a, n, 1, 1), j = arg_io(a, n, 2, (lint)t->an); if (L.err) return 0;
        int c = 0; for (lint k = i; k <= j; k++) { if (c >= LUA_MAXRET) { lerr("too many results to unpack"); return 0; } L.rv[c++] = lrawget(t, lint_v(k)); }
        return c;
    }
    /* math */
    case C_M_FLOOR: { lint v; if (!arg_i(a, n, 0, &v)) return 0; return ret1(lint_v(v)); }
    case C_M_ABS: { lint v; if (!arg_i(a, n, 0, &v)) return 0; return ret1(lint_v(v < 0 ? -v : v)); }
    case C_M_MAX: case C_M_MIN: {
        lint best; if (!arg_i(a, n, 0, &best)) return 0;
        for (int i = 1; i < n; i++) { lint v; if (!arg_i(a, n, i, &v)) return 0; if (id == C_M_MAX ? v > best : v < best) best = v; }
        return ret1(lint_v(best));
    }
    case C_M_RANDOM: {
        lint lo = 1, hi;
        if (n == 0) { lerr("integer Lua: use math.random(n) or math.random(m, n)"); return 0; }
        if (n == 1) { if (!arg_i(a, n, 0, &hi)) return 0; } else { if (!arg_i(a, n, 0, &lo) || !arg_i(a, n, 1, &hi)) return 0; }
        if (lo > hi) { largerr(n - 1, "interval is empty"); return 0; }
        unsigned long long span = (unsigned long long)(hi - lo) + 1;
        return ret1(lint_v(lo + (lint)(((unsigned long long)lrand32() << 16 ^ lrand32()) % span)));
    }
    case C_M_RSEED: { lint v; if (!arg_i(a, n, 0, &v)) return 0; L.seed = (u32)v ? (u32)v : 1; return 0; }
    case C_M_SQRT: {
        lint v; if (!arg_i(a, n, 0, &v)) return 0;
        if (v < 0) { largerr(0, "negative number"); return 0; }
        lint r = 0; while ((r + 1) * (r + 1) <= v && r < 3037000499LL) r++;
        return ret1(lint_v(r));
    }
    /* os */
    case C_O_TIME: {
        int y, mo, d, h, mi, s; lua_rtc(&y, &mo, &d, &h, &mi, &s);
        lint yy = y, mm = mo; if (mm <= 2) yy--;
        lint era = yy / 400, yoe = yy - era * 400, doy = (153 * (mm + (mm > 2 ? -3 : 9)) + 2) / 5 + d - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        lint days = era * 146097 + doe - 719468;
        return ret1(lint_v(days * 86400 + h * 3600 + mi * 60 + s));
    }
    case C_O_DATE: {
        int y, mo, d, h, mi, s; lua_rtc(&y, &mo, &d, &h, &mi, &s);
        static const int tt[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
        int yy = mo < 3 ? y - 1 : y; int wd = (yy + yy / 4 - yy / 100 + yy / 400 + tt[mo - 1] + d) % 7;   /* 0 = воскресенье */
        struct ltab *t = lnewtab();
        lsetfield(t, "year", lint_v(y)); lsetfield(t, "month", lint_v(mo)); lsetfield(t, "day", lint_v(d));
        lsetfield(t, "hour", lint_v(h)); lsetfield(t, "min", lint_v(mi)); lsetfield(t, "sec", lint_v(s)); lsetfield(t, "wday", lint_v(wd + 1));
        return ret1(ltab_v(t));
    }
    case C_O_MS: return ret1(lint_v((lint)now_ms()));
    case C_O_SLEEP: { lint ms = arg_io(a, n, 0, 0); if (L.err) return 0; if (ms > 10000) ms = 10000; if (ms > 0) delay_ms((u32)ms); L.since_key += 30000; return 0; }
    /* term */
    case C_TM_CLEAR: clear(); return 0;
    case C_TM_AT: { lint x, y; if (!arg_i(a, n, 0, &x) || !arg_i(a, n, 1, &y)) return 0; cx = x < 0 ? 0 : x >= W ? W - 1 : (int)x; cy = y < 0 ? 0 : y >= H ? H - 1 : (int)y; cursor(); return 0; }
    case C_TM_COLOR: { lint f, b = (color >> 4) & 15; if (!arg_i(a, n, 0, &f)) return 0; b = arg_io(a, n, 1, b); color = (u8)(((b & 15) << 4) | (f & 15)); return 0; }
    case C_TM_WRITE: { for (int i = 0; i < n; i++) { char b[40]; const char *p; int m = lts(a[i], b, &p); lua_putstr(p, m); } return 0; }
    case C_TM_PUT: {
        lint x, y; const char *s; u32 l; if (!arg_i(a, n, 0, &x) || !arg_i(a, n, 1, &y)) return 0;
        if (n > 2 && a[2].t != LT_STR && a[2].t != LT_INT) { largerr(2, "string expected"); return 0; }
        if (!arg_s(a, n, 2, &s, &l)) return 0;
        if (y >= 0 && y < H) for (u32 i = 0; i < l; i++) { lint px = x + (lint)i; if (px >= 0 && px < W) put_at((int)px, (int)y, s[i], color); }
        return 0;
    }
    case C_TM_FILL: return cfn_term_fill(a, n);
    case C_TM_KEY: { int k = lpoll_key(); return ret1(k >= 0 ? lint_v(k) : LNIL); }
    case C_TM_GETKEY: { int k; while ((k = lpoll_key()) < 0) { } return ret1(lint_v(k)); }
    case C_TM_READLINE: { char b[160]; L.since_key = 0; int m = readline(b, sizeof b); return ret1(lstr_v(b, (u32)m)); }
    case C_TM_CURSOR: { if (n && ltruthy(a[0])) cursor_show(); else cursor_hide(); return 0; }
    case C_TM_SIZE: L.rv[0] = lint_v(W); L.rv[1] = lint_v(H); return 2;
    /* fs */
    case C_FS_LIST: {
        const char *p; u32 pl; if (!arg_s(a, n, 0, &p, &pl)) return 0;
        int d = fs_resolve(p);
        if (d < 0 || !fsn[d].dir) return ret1(LNIL);
        struct ltab *t = lnewtab(); lint k = 1;
        for (int i = 1; i < FS_MAXN; i++) if (fsn[i].used && fsn[i].parent == d && i != d) {
            char nm[FS_NAME + 2]; int m = 0; while (fsn[i].name[m] && m < FS_NAME) { nm[m] = fsn[i].name[m]; m++; }
            if (fsn[i].dir) nm[m++] = '/';
            lrawset(t, lint_v(k++), lstr_v(nm, (u32)m));
        }
        return ret1(ltab_v(t));
    }
    case C_FS_READ: {
        const char *p; u32 pl; if (!arg_s(a, n, 0, &p, &pl)) return 0;
        const char *d; int dl; if (!lreadfile(p, &d, &dl)) return ret1(LNIL);
        return ret1(lstr_v(d, (u32)dl));
    }
    case C_FS_WRITE: case C_FS_APPEND: {
        const char *p, *s; u32 pl, sl; if (!arg_s(a, n, 0, &p, &pl) || !arg_s(a, n, 1, &s, &sl)) return 0;
        if (!lfs_wok(p)) { L.rv[0] = lbool_v(0); L.rv[1] = lcstr_v("writing is allowed only under /lua"); return 2; }
        int i = fs_resolve(p);
        if (i < 0) i = fs_create(p, 0);
        if (i < 0 || fsn[i].dir) { L.rv[0] = lbool_v(0); L.rv[1] = lcstr_v("cannot create the file"); return 2; }
        if ((id == C_FS_WRITE ? sl : (u32)fsn[i].size + sl) > FS_FSZ) { L.rv[0] = lbool_v(0); L.rv[1] = lcstr_v("file is too big (max 2048 bytes)"); return 2; }
        fs_write(i, s, (int)sl, id == C_FS_APPEND); fs_sync();
        return ret1(lbool_v(1));
    }
    case C_FS_REMOVE: {
        const char *p; u32 pl; if (!arg_s(a, n, 0, &p, &pl)) return 0;
        if (!lfs_wok(p) || p[4] == 0) { L.rv[0] = lbool_v(0); L.rv[1] = lcstr_v("removing is allowed only under /lua"); return 2; }
        int i = fs_resolve(p);
        if (i < 0 || fs_remove(i) < 0) return ret1(lbool_v(0));
        fs_sync(); return ret1(lbool_v(1));
    }
    case C_FS_EXISTS: { const char *p; u32 pl; if (!arg_s(a, n, 0, &p, &pl)) return 0; return ret1(lbool_v(fs_resolve(p) >= 0)); }
    case C_FS_ISDIR: { const char *p; u32 pl; if (!arg_s(a, n, 0, &p, &pl)) return 0; int i = fs_resolve(p); return ret1(lbool_v(i >= 0 && fsn[i].dir)); }
    case C_FS_MKDIR: {
        const char *p; u32 pl; if (!arg_s(a, n, 0, &p, &pl)) return 0;
        if (!lfs_wok(p)) return ret1(lbool_v(0));
        if (fs_resolve(p) >= 0) return ret1(lbool_v(1));
        int r = fs_create(p, 1); if (r >= 0) fs_sync();
        return ret1(lbool_v(r >= 0));
    }
    case C_EDIT: {
        const char *p; u32 pl; if (!arg_s(a, n, 0, &p, &pl)) return 0;
        if (!lfs_wok(p) || p[4] == 0) { L.rv[0] = lbool_v(0); L.rv[1] = lcstr_v("edit() works only with files under /lua"); return 2; }
        L.since_key = 0;
        return ret1(lbool_v(lua_edit(p)));
    }
    }
    lerr("unknown builtin"); return 0;
}

/* ---------- Запуск ---------- */
static void lua_init(void) {
    lua_top = 0; gc_free = GC_NIL; gc_msn = 0; gc_ovf = 0;
    for (u32 i = 0; i < sizeof gc_bm / sizeof gc_bm[0]; i++) gc_bm[i] = 0;
    L.globals = 0; L.strlib = 0; L.s_index = 0;
    for (int i = 0; i < LUA_MAXRET; i++) L.rv[i] = LNIL;
    L.sp = L.depth = L.line = L.err = L.nrv = L.interrupted = 0; L.errv = LNIL; L.chunk = 0; L.steps = 0; L.since_key = 0; L.npend = 0;
    if (!L.seed) L.seed = 2463534242u;
    L.globals = lnewtab(); L.s_index = lmks_p("__index", 7);
    for (int i = 0; i < LUA_NREG; i++) {
        struct ltab *dst = L.globals;
        if (lua_reg[i].lib) {
            struct lval t = lgetfield(L.globals, lua_reg[i].lib);
            if (t.t != LT_TAB) { t = ltab_v(lnewtab()); lsetfield(L.globals, lua_reg[i].lib, t); if (lua_reg[i].lib[0] == 's' && lua_reg[i].lib[1] == 't') L.strlib = t.u.tb; }
            dst = t.u.tb;
        }
        struct lval f; f.t = LT_CFN; f.u.i = lua_reg[i].id;
        lsetfield(dst, lua_reg[i].name, f);
    }
    struct lval mt = lgetfield(L.globals, "math"), tm = lgetfield(L.globals, "term");
    lsetfield(mt.u.tb, "maxinteger", lint_v(0x7fffffffffffffffLL)); lsetfield(mt.u.tb, "mininteger", lint_v((lint)(-0x7fffffffffffffffLL - 1)));
    lsetfield(mt.u.tb, "huge", lint_v(0x7fffffffffffffffLL));
    static const struct { const char *n; int v; } kc[] = { {"UP", K_UP}, {"DOWN", K_DOWN}, {"LEFT", K_LEFT}, {"RIGHT", K_RIGHT}, {"DEL", K_DEL}, {"HOME", K_HOME},
        {"END", K_END}, {"PGUP", K_PGUP}, {"PGDN", K_PGDN}, {"INS", K_INS}, {"F1", K_F1}, {"F2", K_F1 + 1}, {"F3", K_F1 + 2}, {"F4", K_F1 + 3}, {"F5", K_F1 + 4},
        {"F6", K_F1 + 5}, {"F7", K_F1 + 6}, {"F8", K_F1 + 7}, {"F9", K_F1 + 8}, {"F10", K_F1 + 9}, {"ESC", 27}, {"ENTER", 10}, {"BS", 8}, {"TAB", 9} };
    for (int i = 0; i < (int)(sizeof kc / sizeof kc[0]); i++) lsetfield(tm.u.tb, kc[i].n, lint_v(kc[i].v));
    if (fs_resolve("/lua") < 0) fs_create("/lua", 1);                 /* каталог для программ и их данных */
    lsetfield(L.globals, "_G", ltab_v(L.globals));
    lsetfield(L.globals, "_VERSION", lcstr_v("Lua (Cravgos, integer subset)"));
}
/* выполнить исходник; 0 - успех, 1 - ошибка (сообщение уже выведено) */
static int lua_run_src(const char *src, int len, const char *chunk) {
    u8 oc = color; int ocx = cx, ocy = cy;
    lua_stack_base = (u8 *)__builtin_frame_address(0) + 64;
    lua_init();
    struct fproto *fp = lua_parse(src, len, lmks_p(chunk, (u32)lua_slen(chunk))->s);
    int bad = 0;
    if (fp) { lcall(lmkclosure(fp), 0, 0); }
    if (L.err) {
        bad = 1;
        color = oc; cursor_show();
        if (cx) putc('\n');
        puts("lua: "); puts(L.emsg); putc('\n');
        if (L.errv.t != LT_NIL) { char b[40]; const char *p; int m = lts(L.errv, b, &p); puts("error value: "); lua_putstr(p, m); putc('\n'); }
    }
    color = oc; (void)ocx; (void)ocy;
    cursor_show();
    return bad;
}
#endif
