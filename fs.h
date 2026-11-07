/* fs.h - файловая система в памяти (RAMFS). Чистый C, без ввода-вывода. */
#ifndef FS_H
#define FS_H
#define FS_MAXN 64
#define FS_FSZ  2048
#define FS_NAME 16

#define FS_EBAD     -1
#define FS_EEXIST   -2
#define FS_EFULL    -3
#define FS_ENOENT   -4
#define FS_ENOTDIR  -5
#define FS_EISDIR   -6
#define FS_ENOTEMPTY -7
#define FS_EBUSY    -8

#ifndef XBSS
#define XBSS
#endif

struct fsnode { char name[FS_NAME]; u8 used, dir, parent; u16 size; };
static struct fsnode fsn[FS_MAXN];
static u8 fsdata[FS_MAXN][FS_FSZ] XBSS;
static int fs_cwd;

static int fs_scmp(const char *a, int alen, const char *b) {
    for (int i = 0; i < alen; i++) if (b[i] != a[i]) return 0;
    return b[alen] == 0;
}
static void fs_init(void) {
    for (int i = 0; i < FS_MAXN; i++) fsn[i].used = 0;
    fsn[0].used = 1; fsn[0].dir = 1; fsn[0].parent = 0; fsn[0].name[0] = 0; fsn[0].size = 0;
    fs_cwd = 0;
}
static int fs_find(int dir, const char *name, int len) {
    for (int i = 1; i < FS_MAXN; i++)
        if (fsn[i].used && fsn[i].parent == dir && fs_scmp(name, len, fsn[i].name)) return i;
    return -1;
}
static int fs_resolve(const char *p) {
    int cur = (*p == '/') ? 0 : fs_cwd;
    for (;;) {
        while (*p == '/') p++;
        if (!*p) return cur;
        const char *s = p;
        while (*p && *p != '/') p++;
        int n = p - s;
        if (n == 1 && s[0] == '.') continue;
        if (n == 2 && s[0] == '.' && s[1] == '.') { cur = fsn[cur].parent; continue; }
        if (!fsn[cur].dir) return -1;
        int c = fs_find(cur, s, n);
        if (c < 0) return -1;
        cur = c;
    }
}
static int fs_is_anc(int a, int b) {      /* a - предок b или сам b */
    for (;;) { if (b == a) return 1; if (b == 0) return 0; b = fsn[b].parent; }
}
static int fs_split(const char *path, int *par, char *name) {
    int n = 0, ls = -1;
    while (path[n]) n++;
    if (n == 0 || n >= 64) return FS_EBAD;
    for (int i = 0; i < n; i++) if (path[i] == '/') ls = i;
    const char *base = path + ls + 1;
    int bl = n - (ls + 1);
    if (bl <= 0 || bl >= FS_NAME) return FS_EBAD;
    if ((bl == 1 && base[0] == '.') || (bl == 2 && base[0] == '.' && base[1] == '.')) return FS_EBAD;
    if (ls < 0) *par = fs_cwd;
    else if (ls == 0) *par = 0;
    else {
        char tmp[64];
        for (int i = 0; i < ls; i++) tmp[i] = path[i];
        tmp[ls] = 0;
        int pi = fs_resolve(tmp);
        if (pi < 0) return FS_ENOENT;
        *par = pi;
    }
    if (!fsn[*par].dir) return FS_ENOTDIR;
    for (int i = 0; i < bl; i++) name[i] = base[i];
    name[bl] = 0;
    return 0;
}
static int fs_newnode(int par, const char *name, int isdir) {
    for (int i = 1; i < FS_MAXN; i++)
        if (!fsn[i].used) {
            fsn[i].used = 1; fsn[i].dir = isdir; fsn[i].parent = par; fsn[i].size = 0;
            int k = 0;
            while (name[k] && k < FS_NAME - 1) { fsn[i].name[k] = name[k]; k++; }
            fsn[i].name[k] = 0;
            return i;
        }
    return FS_EFULL;
}
static int slen_(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int fs_create(const char *path, int isdir) {
    int par; char nm[FS_NAME];
    int r = fs_split(path, &par, nm);
    if (r < 0) return r;
    if (fs_find(par, nm, slen_(nm)) >= 0) return FS_EEXIST;
    return fs_newnode(par, nm, isdir);
}
static int fs_write(int idx, const char *data, int len, int append) {
    if (fsn[idx].dir) return FS_EISDIR;
    int start = append ? fsn[idx].size : 0, n = len;
    if (start + n > FS_FSZ) n = FS_FSZ - start;
    for (int i = 0; i < n; i++) fsdata[idx][start + i] = (u8)data[i];
    fsn[idx].size = start + n;
    return n;
}
static int fs_remove(int idx) {
    if (idx <= 0) return FS_EBUSY;
    if (fs_is_anc(idx, fs_cwd)) return FS_EBUSY;
    if (fsn[idx].dir)
        for (int i = 1; i < FS_MAXN; i++) if (fsn[i].used && fsn[i].parent == idx) return FS_ENOTEMPTY;
    fsn[idx].used = 0; fsn[idx].size = 0; fsn[idx].name[0] = 0;
    return 0;
}
static int fs_dest(const char *dst, const char *srcname, int *par, char *name) {
    int d = fs_resolve(dst);
    if (d >= 0 && fsn[d].dir) {
        *par = d;
        int k = 0; while (srcname[k]) { name[k] = srcname[k]; k++; }
        name[k] = 0;
        return 0;
    }
    return fs_split(dst, par, name);
}
static int fs_move(const char *src, const char *dst) {
    int s = fs_resolve(src);
    if (s < 0) return FS_ENOENT;
    if (s == 0) return FS_EBUSY;
    int par; char nm[FS_NAME];
    int r = fs_dest(dst, fsn[s].name, &par, nm);
    if (r < 0) return r;
    if (fs_is_anc(s, par)) return FS_EBAD;
    if (fs_find(par, nm, slen_(nm)) >= 0) return FS_EEXIST;
    fsn[s].parent = par;
    int k = 0; while (nm[k]) { fsn[s].name[k] = nm[k]; k++; }
    fsn[s].name[k] = 0;
    return 0;
}
static int fs_copy_tree(int s, int par, const char *name, int depth) {
    if (depth > 12) return FS_EBAD;
    if (fs_find(par, name, slen_(name)) >= 0) return FS_EEXIST;
    int n = fs_newnode(par, name, fsn[s].dir);
    if (n < 0) return n;
    if (!fsn[s].dir) {
        for (int i = 0; i < fsn[s].size; i++) fsdata[n][i] = fsdata[s][i];
        fsn[n].size = fsn[s].size;
        return n;
    }
    for (int i = 1; i < FS_MAXN; i++)
        if (fsn[i].used && fsn[i].parent == s && i != n) {
            int r = fs_copy_tree(i, n, fsn[i].name, depth + 1);
            if (r < 0) return r;
        }
    return n;
}
static int fs_copy(const char *src, const char *dst) {
    int s = fs_resolve(src);
    if (s < 0) return FS_ENOENT;
    if (s == 0) return FS_EBUSY;
    int par; char nm[FS_NAME];
    int r = fs_dest(dst, fsn[s].name, &par, nm);
    if (r < 0) return r;
    if (fs_is_anc(s, par)) return FS_EBAD;           /* папку внутрь себя нельзя */
    return fs_copy_tree(s, par, nm, 0);
}
static void fs_rm_rec(int idx) {
    if (fsn[idx].dir)
        for (int i = 1; i < FS_MAXN; i++) if (fsn[i].used && fsn[i].parent == idx) fs_rm_rec(i);
    fsn[idx].used = 0; fsn[idx].size = 0; fsn[idx].name[0] = 0;
}
static int fs_remove_tree(int idx) {
    if (idx <= 0) return FS_EBUSY;
    if (fs_is_anc(idx, fs_cwd)) return FS_EBUSY;
    fs_rm_rec(idx);
    return 0;
}
static int fs_cmp(const char *a, const char *b) {
    for (;;) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return x < y ? -1 : 1;
        if (!x) return 0;
        a++; b++;
    }
}
static int fs_before(int a, int b) {
    if (fsn[a].dir != fsn[b].dir) return fsn[a].dir > fsn[b].dir;
    return fs_cmp(fsn[a].name, fsn[b].name) < 0;
}
static int fs_list(int dir, int *out) {            /* папки сверху, затем по имени */
    int n = 0;
    for (int i = 1; i < FS_MAXN; i++) if (fsn[i].used && fsn[i].parent == dir) out[n++] = i;
    for (int i = 1; i < n; i++) {
        int v = out[i], j = i - 1;
        while (j >= 0 && fs_before(v, out[j])) { out[j + 1] = out[j]; j--; }
        out[j + 1] = v;
    }
    return n;
}
static void fs_path(int idx, char *out) {
    int st[16], n = 0, o = 0;
    while (idx != 0 && n < 16) { st[n++] = idx; idx = fsn[idx].parent; }
    if (n == 0) { out[0] = '/'; out[1] = 0; return; }
    while (n--) {
        out[o++] = '/';
        for (int k = 0; fsn[st[n]].name[k]; k++) out[o++] = fsn[st[n]].name[k];
    }
    out[o] = 0;
}
static void fs_usage(int *files, int *dirs, int *bytes) {
    *files = *dirs = *bytes = 0;
    for (int i = 1; i < FS_MAXN; i++)
        if (fsn[i].used) { if (fsn[i].dir) (*dirs)++; else { (*files)++; *bytes += fsn[i].size; } }
}
#endif
