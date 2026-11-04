#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
#include "../fs.h"
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while(0)
int main(void) {
    char b[256];
    fs_init();
    CHECK(fs_create("/home", 1) > 0);
    CHECK(fs_create("/home", 1) == FS_EEXIST);
    CHECK(fs_create("/a/b", 0) == FS_ENOENT);
    int d = fs_resolve("/home"); CHECK(d > 0 && fsn[d].dir);
    fs_cwd = d;
    int f = fs_create("note.txt", 0); CHECK(f > 0);
    CHECK(fs_resolve("/home/note.txt") == f);
    CHECK(fs_resolve("./note.txt") == f);
    CHECK(fs_resolve("../home/note.txt") == f);
    CHECK(fs_resolve("..") == 0);
    CHECK(fs_resolve("../..") == 0);
    CHECK(fs_resolve("nothing") == -1);
    CHECK(fs_write(f, "hello", 5, 0) == 5);
    CHECK(fs_write(f, " world", 6, 1) == 6);
    CHECK(fsn[f].size == 11 && memcmp(fsdata[f], "hello world", 11) == 0);
    CHECK(fs_write(f, "x", 1, 0) == 1 && fsn[f].size == 1);   /* перезапись */
    char big[3000]; memset(big, 'z', sizeof big);
    CHECK(fs_write(f, big, 3000, 0) == FS_FSZ);                 /* лимит */
    fs_path(f, b); CHECK(strcmp(b, "/home/note.txt") == 0);
    fs_path(0, b); CHECK(strcmp(b, "/") == 0);
    /* cp / mv */
    fs_write(f, "data", 4, 0);
    CHECK(fs_copy("note.txt", "copy.txt") > 0);
    CHECK(fs_copy("note.txt", "copy.txt") == FS_EEXIST);
    int c = fs_resolve("copy.txt"); CHECK(fsn[c].size == 4 && memcmp(fsdata[c], "data", 4) == 0);
    CHECK(fs_create("/docs", 1) > 0);
    CHECK(fs_move("copy.txt", "/docs") == 0);                   /* в существующую папку */
    CHECK(fs_resolve("/docs/copy.txt") == c);
    CHECK(fs_move("/docs/copy.txt", "/home/renamed") == 0);     /* переименование */
    CHECK(fs_resolve("/home/renamed") == c);
    CHECK(fs_move("/home", "/home/sub") == FS_EBAD);            /* папку в себя нельзя */
    CHECK(fs_move("/home", "/docs/h") == 0);                    /* папка с содержимым */
    CHECK(fs_resolve("/docs/h/note.txt") == f);
    /* rm */
    fs_cwd = 0;
    CHECK(fs_remove(fs_resolve("/docs")) == FS_ENOTEMPTY);
    fs_cwd = fs_resolve("/docs/h");
    CHECK(fs_remove(fs_resolve("/docs/h")) == FS_EBUSY);        /* текущую папку нельзя */
    fs_cwd = 0;
    CHECK(fs_remove(f) == 0 && fs_resolve("/docs/h/note.txt") == -1);
    CHECK(fs_remove(0) == FS_EBUSY);
    CHECK(fs_create("/this_name_is_way_too_long", 0) == FS_EBAD);
    CHECK(fs_create("/a/", 0) == FS_EBAD);
    /* заполнение */
    int n = 0; char nm[16];
    for (int i = 0; i < 100; i++) { sprintf(nm, "/f%d", i); int r = fs_create(nm, 0); if (r > 0) n++; else { CHECK(r == FS_EFULL); break; } }
    CHECK(n > 0 && n < 64);
    int files, dirs, bytes; fs_usage(&files, &dirs, &bytes);
    CHECK(files + dirs == 63 || files + dirs + 1 == 64);
    /* рекурсивные операции и сортировка */
    fs_init();
    fs_create("/src", 1); fs_create("/src/sub", 1); fs_create("/src/a.txt", 0); fs_create("/src/sub/b.txt", 0);
    fs_write(fs_resolve("/src/sub/b.txt"), "deep", 4, 0);
    CHECK(fs_copy("/src", "/dst") > 0);
    CHECK(fs_resolve("/dst/sub/b.txt") > 0 && fsn[fs_resolve("/dst/sub/b.txt")].size == 4);
    CHECK(memcmp(fsdata[fs_resolve("/dst/sub/b.txt")], "deep", 4) == 0);
    CHECK(fs_copy("/src", "/src/sub") == FS_EBAD);              /* внутрь себя */
    CHECK(fs_copy("/src", "/dst") > 0 && fs_resolve("/dst/src/sub/b.txt") > 0);   /* в существующую папку */
    CHECK(fs_copy("/src", "/dst") == FS_EEXIST);
    int lst[FS_MAXN]; fs_create("/Zed.txt", 0); fs_create("/apple.txt", 0);
    int ln = fs_list(0, lst);
    CHECK(ln == 4 && fsn[lst[0]].dir && fsn[lst[1]].dir && strcmp(fsn[lst[0]].name, "dst") == 0 && strcmp(fsn[lst[1]].name, "src") == 0);
    CHECK(strcmp(fsn[lst[2]].name, "apple.txt") == 0 && strcmp(fsn[lst[3]].name, "Zed.txt") == 0);
    fs_cwd = fs_resolve("/dst/sub");
    CHECK(fs_remove_tree(fs_resolve("/dst")) == FS_EBUSY);       /* cwd внутри */
    fs_cwd = 0;
    CHECK(fs_remove_tree(fs_resolve("/dst")) == 0 && fs_resolve("/dst") == -1 && fs_resolve("/dst/sub/b.txt") == -1);
    CHECK(fs_resolve("/src/sub/b.txt") > 0);                    /* оригинал цел */
    CHECK(fs_remove_tree(0) == FS_EBUSY);
    printf("fs tests OK (files=%d dirs=%d)\n", files, dirs);
    return 0;
}
