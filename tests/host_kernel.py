#!/usr/bin/env python3
"""Собирает хост-стенд из настоящего kernel.c: порты -> эмуляция, VGA -> память, ASAN/UBSAN."""
import re, subprocess, sys, os
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
k = open(os.path.join(root, 'kernel.c')).read()
k = re.sub(r'static inline void outb\(u16 p, u8 v\) \{[^\n]*\}', 'static inline void outb(u16 p, u8 v) { host_outb(p, v); }', k)
k = re.sub(r'static inline u8 inb\(u16 p\) \{[^\n]*\}', 'static inline u8 inb(u16 p) { return host_inb(p); }', k)
k = re.sub(r'static inline void outl\(u16 p, u32 v\) \{[^\n]*\}', 'static inline void outl(u16 p, u32 v) { host_outl(p, v); }', k)
k = re.sub(r'static inline u32 inl\(u16 p\) \{[^\n]*\}', 'static inline u32 inl(u16 p) { return host_inl(p); }', k)
assert 'host_outb' in k and 'host_inb' in k and 'host_outl' in k and 'host_inl' in k, "port patching failed"
k = k.replace('void kmain(void) {', 'void kmain_unused(void) {')
# символы линкера -> память стенда; таблица ISR -> фиктивная; lidt и переход в оболочку -> заглушки
k = re.sub(r'extern char _start16\[\][^\n]*', 'static char host_mem[4000000];\n#define _start16 (host_mem)\n#define __bss_start (host_mem+90000)\n#define __bss_end (host_mem+240000)\n#define __bigbss_start (host_mem+300000)\n#define __bigbss_end (host_mem+1800000)', k)
k = k.replace('extern const u32 isr_table[];', 'static const u32 isr_table[33] = {' + ','.join(str(0x1000 + 16*i) for i in range(33)) + '};')
k = k.replace('__asm__ volatile("lidt %0" : : "m"(d));', '(void)d;')
k = k.replace('__asm__ volatile("movl $0x90000, %%esp\\n\\tcall shell_main\\n" : : : "memory");', 'host_exc_done();')
assert 'host_exc_done' in k and 'host_mem' in k, "host patching failed"
pre = 'void host_exc_done(void);\nunsigned char host_inb(unsigned short); void host_outb(unsigned short, unsigned char); unsigned host_inl(unsigned short); void host_outl(unsigned short, unsigned);\n'
mainfile = sys.argv[1] if len(sys.argv) > 1 else 'host_main.c'
out = pre + k + '\n' + open(os.path.join(root, 'tests', mainfile)).read()
open('/tmp/host_kernel_gen.c', 'w').write(out)
cmd = ['gcc', '-I', root, '-g', '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-builtin', '-w', '-o', '/tmp/' + (sys.argv[2] if len(sys.argv) > 2 else 'host_kernel'), '/tmp/host_kernel_gen.c']
r = subprocess.run(cmd, capture_output=True, text=True)
if r.returncode: print(r.stderr[:4000]); sys.exit(1)
print("host harness built")
