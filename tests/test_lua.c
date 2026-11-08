/* тесты lua.h: интерпретатор Lua-подмножества (экран и клавиатура подменены) */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32; typedef unsigned long long u64;
#include "../fs.h"
#define W 80
#define H 25
static int cx, cy; static u8 color = 0x07;
static char out[1 << 16]; static int outn;
static u16 scr[W * H];
static void t_putc(char c) { if (outn < (int)sizeof out - 1) out[outn++] = c; out[outn] = 0; }
#define putc t_putc
static void puts_(const char *s) { while (*s) t_putc(*s++); }
#define puts puts_
static void put_at(int x, int y, char c, u8 attr) { scr[y * W + x] = (attr << 8) | (u8)c; }
static void clear(void) { for (int i = 0; i < W * H; i++) scr[i] = 0x0720; cx = cy = 0; }
static void cursor(void) {} static void cursor_hide(void) {} static void cursor_show(void) {}
static int keyq[256], kq_n, kq_i;
static int fuzz_mode;
static int key_poll(void) { if (fuzz_mode) return 27; return kq_i < kq_n ? keyq[kq_i++] : -1; }
static const char *linesq[16]; static int lq_n, lq_i;
static int readline(char *b, int max) { const char *s = lq_i < lq_n ? linesq[lq_i++] : ""; int n = 0; while (s[n] && n < max - 1) { b[n] = s[n]; n++; } b[n] = 0; return n; }
static u32 now_ms(void) { static u32 t; return t += 7; }
static u32 slept; static void delay_ms(u32 ms) { slept += ms; }
static void fs_sync(void) {}
static void lua_rtc(int *y, int *mo, int *d, int *h, int *mi, int *s) { *y = 2026; *mo = 11; *d = 7; *h = 12; *mi = 34; *s = 56; }
static char edited[64]; static int lua_edit(const char *p) { strncpy(edited, p, 63); return 1; }
#include "../lua.h"

static int fails;
#ifdef LUA_GC_STRESS
#define FUZZ_N 1500
#else
#define FUZZ_N 20000
#endif
static void reset(void) { cx = cy = 0; outn = 0; out[0] = 0; kq_n = kq_i = 0; lq_n = lq_i = 0; }
static void run(const char *src) { if (getenv("V")) { fprintf(stderr, "RUN: %.90s\n", src); } reset(); lua_run_src(src, (int)strlen(src), "t"); }
#define TP(src, pre) do { run(src); if (strncmp(out, pre, strlen(pre))) { printf("FAIL: %s\n  got:  [%s]\n  want prefix: [%s]\n", src, out, pre); fails++; } } while (0)
#define T(src, want) do { run(src); if (strcmp(out, want)) { printf("FAIL: %s\n  got:  [%s]\n  want: [%s]\n", src, out, want); fails++; } } while (0)

int main(void) {
    fs_init(); fs_create("/lua", 1);
    T("print(1+2*3, 10-4, 7//2, -7//2, 7%3, -7%3, 2^10)", "7\t6\t3\t-4\t1\t2\t1024\n");
    T("print('a'..'b'..1, #'hello', 'x'=='x', 1<2, 'a'<'b')", "ab1\t5\ttrue\ttrue\ttrue\n");
    T("local a, b = 1, 2; a, b = b, a; print(a, b)", "2\t1\n");
    T("local t = {1,2,3,x=5,[10]=7}; print(#t, t.x, t[10], t[2])", "3\t5\t7\t2\n");
    T("local s=0; for i=1,10 do s=s+i end; print(s)", "55\n");
    T("local s=0; for i=10,1,-3 do s=s+i end; print(s)", "22\n");
    T("local s=0; local i=0; while i<5 do i=i+1; if i==3 then break end; s=s+i end; print(s,i)", "3\t3\n");
    T("local i=0; repeat local k=i; i=i+1 until k>=3; print(i)", "4\n");
    T("local function fib(n) if n<2 then return n end return fib(n-1)+fib(n-2) end print(fib(15))", "610\n");
    T("local function mk() local c=0; return function() c=c+1; return c end end local f,g=mk(),mk(); f();f(); print(f(), g())", "3\t1\n");
    T("local fs={}; for i=1,3 do fs[i]=function() return i end end; print(fs[1](), fs[2](), fs[3]())", "1\t2\t3\n");
    T("local t={}; for i=1,5 do t[#t+1]=i*i end; print(table.concat(t,','))", "1,4,9,16,25\n");
    T("local t={5,2,8,1}; table.sort(t); print(table.concat(t,' '))", "1 2 5 8\n");
    T("local t={5,2,8,1}; table.sort(t,function(a,b) return a>b end); print(table.concat(t,' '))", "8 5 2 1\n");
    T("local t={'b','c','a'}; table.sort(t); print(table.concat(t))", "abc\n");
    T("local t={}; table.insert(t,'a'); table.insert(t,'c'); table.insert(t,2,'b'); print(table.concat(t), table.remove(t), table.remove(t,1), #t)", "abc\tc\ta\t1\n");
    T("local n=0; for k,v in pairs({a=1,b=2,c=3,10,20}) do n=n+v end print(n)", "36\n");
    T("local s=''; for i,v in ipairs({'x','y','z'}) do s=s..i..v end print(s)", "1x2y3z\n");
    T("print(('hello'):upper(), ('Hello'):lower(), ('abc'):rep(3,'-'), ('hello'):sub(2,4), ('hello'):sub(-3), ('abc'):byte(), string.char(72,105))", "HELLO\thello\tabc-abc-abc\tell\tllo\t97\tHi\n");
    T("print(('hello world'):find('o w'), ('hello'):find('xyz'), ('abc'):reverse(), ('abc'):len())", "5\tnil\tcba\t3\n");
    T("print(string.format('%d|%5d|%-5d|%05d|%s|%5s|%-5s|%x|%c|%%', 42, 42, 42, 42, 'hi', 'hi', 'hi', 255, 65))", "42|   42|42   |00042|hi|   hi|hi   |ff|A|%\n");
    T("print(tostring(12), tostring(nil), tostring(true), tonumber('42'), tonumber('x'), tonumber('ff',16), tonumber('  7  '), type(print), type({}), type('s'), type(nil))", "12\tnil\ttrue\t42\tnil\t255\t7\tfunction\ttable\tstring\tnil\n");
    T("print(math.max(3,9,4), math.min(3,9,4), math.abs(-5), math.floor(7), math.sqrt(99), math.sqrt(100))", "9\t3\t5\t7\t9\t10\n");
    T("math.randomseed(1); local ok=true; for i=1,200 do local r=math.random(6); if r<1 or r>6 then ok=false end end print(ok, math.random(5,5))", "true\t5\n");
    T("print(pcall(function() error('boom') end))", "false\tt:1: boom\n");
    T("print(pcall(function() error('boom', 0) end))", "false\tboom\n");
    T("print(pcall(function() local x=nil; return x.y end))", "false\tt:1: attempt to index a nil value\n");
    T("print(pcall(function() return 1+{} end))", "false\tt:1: attempt to perform arithmetic on a table value\n");
    T("print(pcall(function() return 1//0 end))", "false\tt:1: attempt to divide by zero\n");
    T("print(pcall(function() return nofn() end))", "false\tt:1: attempt to call a nil value\n");
    T("print(pcall(function() return 1<'a' end))", "false\tt:1: attempt to compare number with string\n");
        T("local ok, e = pcall(error, {code=7}); print(ok, e.code)", "false\t7\n");
    T("print(assert(1, 'x'), pcall(assert, false, 'nope'))", "1\tfalse\tnope\n");
    T("local f, e = load('return 1+1'); print(f(), e)", "2\tnil\n");
    T("local f, e = load('return +'); print(f, e)", "nil\tload:1: unexpected symbol\n");
    T("local f = load('x = 5'); f(); print(x)", "5\n");
    T("local Account = {}; Account.__index = Account; function Account.new(b) return setmetatable({bal=b}, Account) end; function Account:dep(n) self.bal = self.bal + n; return self end; local a = Account.new(10); a:dep(5):dep(1); print(a.bal)", "16\n");
    T("local t = setmetatable({}, {__index=function(t,k) return k..'!' end}); print(t.foo, t.bar)", "foo!\tbar!\n");
    T("local a = {}; a.b = {}; a.b.c = function(x) return x*2 end; print(a.b.c(21)); function a.b.d(x) return x+1 end print(a.b.d(1))", "42\n2\n");
    T("local function f(...) end", "lua: t:1: '...' is not supported\n");
    T("local function f() return 1,2,3 end; local a,b,c,d = f(); print(a,b,c,d); print(f()); print((f())); local t={f()}; print(#t)", "1\t2\t3\tnil\n1\t2\t3\n1\n3\n");
    T("print(#arg == nil)", "lua: t:1: attempt to get length of a nil value\n");
    T("x = 1 y = 2 print(x+y) ; ; print(0x10, 'a\\nb', [[long\nstr]])", "3\n16\ta\nb\tlong\nstr\n");
    T("-- comment\nprint(1) --[[ block\ncomment ]] print(2)", "1\n2\n");
    T("local t = {} t[1]=1 t[2]=2 t[4]=4 t[3]=3 print(#t)", "4\n");
    T("local t = {1,2,3} t[#t]=nil print(#t) t[#t+1]='z' print(#t, t[3])", "2\n3\tz\n");
    T("local s = 0 for k, v in pairs({10,20,30,a=1}) do s = s + v end print(s)", "61\n");
    T("local t={a=1,b=2,c=3}; for k in pairs(t) do t[k]=nil end; print(next(t))", "nil\n");
    T("print(1 and 2, nil and 2, false or 3, nil or false, not nil, not 0)", "2\tnil\t3\tfalse\ttrue\tfalse\n");
    T("print(2^3^2, -2^2, 1 .. 2 .. 3, 1+2 < 4, 'a'..'b' == 'ab')", "512\t-4\t123\ttrue\ttrue\n");
    T("print(7 // -2, 7 % -2, -7 % 2, math.maxinteger + 1 == math.mininteger)", "-4\t-1\t1\ttrue\n");
    T("local a <const> = 1", "lua: t:1: unexpected symbol\n");
    T("print(('x'):rep(0), #('x'):rep(100))", "\t100\n");
    T("print(os.date().year, os.date().month, os.date().day, os.date().wday, os.time())", "2026\t11\t7\t7\t1794054896\n");
    T("local function r(n) return r(n+1) end r(1)", "lua: t:1: stack overflow\n");
    T("print(fs.write('/etc/x','hi'))", "false\twriting is allowed only under /lua\n");
    T("print(fs.write('/lua/a.txt','hello'), fs.read('/lua/a.txt'), fs.exists('/lua/a.txt'), fs.isdir('/lua'), #fs.list('/lua'), fs.append('/lua/a.txt',' w'), fs.read('/lua/a.txt'), fs.remove('/lua/a.txt'), fs.exists('/lua/a.txt'))", "true\thello\ttrue\ttrue\t1\ttrue\thello w\ttrue\tfalse\n");
    T("print(fs.write('/lua/../etc/x','1'), fs.mkdir('/lua/d'), fs.isdir('/lua/d'), fs.remove('/lua/d'), fs.remove('/lua'))", "false\ttrue\ttrue\ttrue\tfalse\tremoving is allowed only under /lua\n");
    T("print(edit('/lua/p.lua'), edit('/etc/passwd'))", "true\tfalse\tedit() works only with files under /lua\n");
    fs_write(fs_create("/lua/m.lua", 0), "return {v=42}", 13, 0);
    T("local m = dofile('/lua/m.lua'); print(m.v)", "42\n");
    T("print(dofile('/lua/nope.lua'))", "lua: t:1: cannot open /lua/nope.lua\n");
    T("print(pcall(dofile, '/lua/nope.lua'))", "false\tt:1: cannot open /lua/nope.lua\n");
    T("local function a() local x=1; return function() return function() x=x+1; return x end end end local f=a()() print(f(), f())", "2\t3\n");
    T("local x=1; do local x=2 end print(x); local x=x+1; print(x)", "1\n2\n");
    T("local f; local function g(n) if n==0 then return 'done' end return f(n-1) end f=g print(f(3))", "done\n");
    T("local function f() for i=1,10 do if i==4 then return i end end end print(f())", "4\n");
    T("local function f() local i=0 while true do i=i+1 if i>5 then return i, 'x' end end end print(f())", "6\tx\n");
    T("local function c() local n=0 return function() n=n+1 return n end, function() return n end end local inc,get=c() inc() inc() print(get())", "2\n");
    T("local t={} for i=1,3 do local j=i*10 t[i]=function() j=j+1 return j end end print(t[1](), t[1](), t[2](), t[3]())", "11\t12\t21\t31\n");
    T("local function f(a,b) return a+(b or 100) end print(f(1), f(1,2))", "101\t3\n");
    T("local t={n=0} function t.inc() t.n=t.n+1 return t end t.inc().inc().inc() print(t.n)", "3\n");
    T("local s=0 for _,v in ipairs({1,2,3}) do for _,w in ipairs({10,20}) do s=s+v*w end end print(s)", "180\n");
    T("print(pcall(function() local function r(n) if n==0 then error('deep') end r(n-1) end r(20) end))", "false\tt:1: deep\n");
    T("local ok,e=pcall(function() local t=nil t.x=1 end) print(ok,e) print(pcall(pcall,error,'x'))", "false\tt:1: attempt to index a nil value\ntrue\tfalse\tt:1: x\n");
    T("local t = {10,20,30} local n=0 for i=#t,1,-1 do n=n*10+t[i]/10 end print(n)", "321\n");
    T("print(#{1,2,nil,4}, #{n=1}, #'')", "2\t0\t0\n");   /* граница: t[3] = nil */
    T("local a = {} for i=1,100 do a[i]=i end for i=1,50 do table.remove(a,1) end print(#a, a[1], a[50])", "50\t51\t100\n");
    T("local t={} t.a=1 t.b=2 t.a=nil local n=0 for k,v in pairs(t) do n=n+1 end print(n, t.a, t.b)", "1\tnil\t2\n");
    /* сборщик мусора: много мусора не приводит к нехватке памяти, живые данные целы */
    T("local keep={} for i=1,200 do keep[i]='k'..i end local s=0 for i=1,60000 do local t=('x'):rep(20)..i s=s+#t end local ok=true for i=1,200 do if keep[i] ~= 'k'..i then ok=false end end print(s, ok)", "1488894\ttrue\n");
    T("local fs={} for i=1,50 do local c=i fs[i]=function() c=c+1 return c end end for i=1,20000 do local g={i,tostring(i)} end local s=0 for i=1,50 do s=s+fs[i]() end print(s)", "1325\n");
    T("local t={} for r=1,300 do t={} for i=1,30 do t[i]={name='n'..i, v={i}} end end local s=0 for i=1,30 do s=s+t[i].v[1]+#t[i].name end print(s)", "546\n");
    T("local a={} for i=1,100 do a['key'..i]=i end for r=1,3000 do local x=('y'):rep(40) end local s=0 for k,v in pairs(a) do s=s+v end print(s, a.key77)", "5050\t77\n");
    T("print(1, collectgarbage(), ('a'):rep(3), collectgarbage('count') >= 0)", "1\t0\taaa\ttrue\n");
    T("local function mk(n) local t={} for i=1,n do t[i]=function() return i*n end end return t end local all={} for r=1,20 do all[r]=mk(10) end local s=0 for r=1,20 do s=s+all[r][10]() end print(s)", "2000\n");
    T("local s='' for i=1,2000 do s=('z'):rep(10)..i end print(s, #s)", "zzzzzzzzzz2000\t14\n");
    /* терминал */
    reset(); clear(); { const char *s = "term.put(0,0,'Hi'); term.color(14,1); term.put(79,24,'XYZ'); term.fill(2,2,3,2,'#'); term.at(5,5); term.write('w',1)"; lua_run_src(s, (int)strlen(s), "t"); }
    if (((scr[0] & 255) != 'H') || scr[1] != (0x07 << 8 | 'i') || scr[24 * 80 + 79] != ((1 << 4 | 14) << 8 | 'X') || (scr[2 * 80 + 2] & 255) != '#' || (scr[3 * 80 + 4] & 255) != '#' || cx != 5 || cy != 5 || strcmp(out, "w1")) { printf("FAIL term: %x %x %x cx=%d cy=%d out=[%s]\n", scr[0], scr[1], scr[24 * 80 + 79], cx, cy, out); fails++; }
    T("print(term.size()); print(term.UP, term.ESC, term.ENTER, term.F1)", "80\t25\n257\t27\t10\t272\n");
    reset(); keyq[0] = 97; keyq[1] = 257; keyq[2] = 98; kq_n = 3; { const char *s = "print(term.key(), term.key(), term.key(), term.key())"; lua_run_src(s, (int)strlen(s), "t"); }
    if (strcmp(out, "97\t257\t98\tnil\n")) { printf("FAIL key(): [%s]\n", out); fails++; }
    reset(); keyq[0] = 120; kq_n = 1; linesq[0] = "hello"; lq_n = 1; { const char *s = "print(term.getkey(), term.readline())"; lua_run_src(s, (int)strlen(s), "t"); }
    if (strcmp(out, "120\thello\n")) { printf("FAIL keys: [%s]\n", out); fails++; }
    /* Esc прерывает долгий счёт; клавиши не теряются */
    reset(); keyq[0] = 'q'; keyq[1] = 27; kq_n = 2; { const char *s = "local i=0 while true do i=i+1 end"; lua_run_src(s, (int)strlen(s), "t"); }
    if (!strstr(out, "interrupted")) { printf("FAIL esc: [%s]\n", out); fails++; }
    /* пустые циклы тоже прерываются по Esc */
    { static const char *loops[] = { "while true do end", "repeat until false", "for i = 1, 9223372036854775807 do end", "for k, v in pairs({1}) do while true do end end" };
      for (int q = 0; q < 4; q++) { reset(); keyq[0] = 27; kq_n = 1; lua_run_src(loops[q], (int)strlen(loops[q]), "t");
        if (!strstr(out, "interrupted")) { printf("FAIL esc in empty loop: %s -> [%s]\n", loops[q], out); fails++; } } }
    /* нехватка памяти не падает */
    T("local t={} for i=1,1000000 do t[i]=('x'):rep(50)..i end", "lua: t:1: not enough memory\n");
    T("local s = 'a' for i=1,20 do s = s..s end print(#s)", "lua: t:1: not enough memory\n");
    /* ошибка в чужой функции показывает её строку */
    T("local function f()\n  local x = nil\n  return x.y\nend\nf()", "lua: t:3: attempt to index a nil value\n");
    T("print(('x'):bad())", "lua: t:1: attempt to call a nil value\n");
    T("local t = {} t[nil] = 1", "lua: t:1: table index is nil\n");
    T("print(1.5)", "lua: t:1: only integer numbers are supported\n");
    TP("error({1})", "lua: t:1: (error object)\nerror value: table: 0x");

    /* фаззинг: испорченные программы не должны ронять интерпретатор (ASAN/UBSAN ловят выход за границы) */
    {
        static const char *corpus[] = {
            "local function fib(n) if n<2 then return n end return fib(n-1)+fib(n-2) end print(fib(10))",
            "local t={} for i=1,20 do t[#t+1]=i*i end table.sort(t,function(a,b) return a>b end) print(table.concat(t,','))",
            "local Acc={} Acc.__index=Acc function Acc.new(b) return setmetatable({b=b},Acc) end function Acc:d(n) self.b=self.b+n return self end print(Acc.new(1):d(2).b)",
            "local function mk() local c=0 return function() c=c+1 return c end end local f=mk() f() print(f(), ('abc'):rep(3,'-'), string.format('%5d|%-3s|%x', 42, 'a', 255))",
            "for k,v in pairs({a=1,b=2,3}) do print(k,v) end local s=0 for i=10,1,-2 do s=s+i end print(s) repeat s=s-1 until s<0",
            "print(pcall(function() error({1}) end)) print(select, load('return 1+')) local x = load('return {1,2,3}')() print(#x, tostring(x):sub(1,5))",
            "term.put(1,1,'x') term.fill(0,0,3,3,'#') term.at(2,2) term.color(1,2) term.write(1,2,3) print(fs.read('/lua/m.lua'), os.date().year, fs.list('/lua')[1])",
            "local a,b,c = 1,2 a,b = b,a local t = {f=function(x) return x end, [3]=4, 5, 6} print(t.f(t[1]), t[3], a, b, c, 2^3^2, -2^2, #t)",
        };
        u32 rs = 12345; int nc = (int)(sizeof corpus / sizeof corpus[0]);
        static const char junk[] = "(){}[]=<>~+-*/%^#.,;:\"' \n0123456789endofunctionlocalifthenreturnwhilebreak";
        fuzz_mode = 1;
        for (int it = 0; it < FUZZ_N; it++) {
            char b[400]; const char *c = corpus[it % nc]; int n = (int)strlen(c); memcpy(b, c, (size_t)n + 1);
            int muts = 1 + (int)(rs = rs * 1664525u + 1013904223u) % 4;
            for (int m = 0; m < muts; m++) {
                rs = rs * 1664525u + 1013904223u; int pos = (int)((rs >> 8) % (u32)(n ? n : 1)); rs = rs * 1664525u + 1013904223u; int kind = (int)((rs >> 8) % 4);
                if (kind == 0 && n > 1) { memmove(b + pos, b + pos + 1, (size_t)(n - pos)); n--; }
                else if (kind == 1 && n < 380) { memmove(b + pos + 1, b + pos, (size_t)(n - pos + 1)); b[pos] = junk[(rs >> 12) % (sizeof junk - 1)]; n++; }
                else if (kind == 2) b[pos] = junk[(rs >> 12) % (sizeof junk - 1)];
                else n = pos ? pos : 1, b[n] = 0;
            }
            b[n] = 0; reset(); fuzz_mode = 1;
            lua_run_src(b, n, "f");
            if (L.sp != 0 && !L.err) { printf("FAIL fuzz: stack not balanced after: %s\n", b); fails++; break; }
        }
        fuzz_mode = 0;
    }
    if (fails) { printf("test_lua: %d FAILED\n", fails); return 1; }
    printf("test_lua: OK\n");
    return 0;
}
