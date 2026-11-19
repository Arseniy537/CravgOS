#!/usr/bin/env python3
"""Проверка 16-битного кода vbe_setup из boot_iso.s без BIOS: текст ассемблера исполняется маленьким
интерпретатором (набор команд, нужный функции), INT 10h изображает VBE BIOS с настраиваемым набором режимов.
Запуск из корня репозитория:  python3 tests/test_vbe_asm.py"""
import re, sys, struct

SRC = open('boot_iso.s', encoding='utf-8').read()
m = re.search(r'^vbe_setup:\n(.*?)^\s+ret\n', SRC, re.S | re.M)
body = m.group(1)
want = [int(x) for x in re.search(r'^vbe_want:\s*\.word\s*(.*)$', SRC, re.M).group(1).split(',')]

SYM = {'fbinfo': 0x9000, 'romfont': 0x9100, 'vbe_buf': 0xA100, 'vbe_mi': 0xA300, 'vbe_want': 0xA400}
VAR = ['vbe_wx', 'vbe_wy', 'vbe_si', 'vbe_wp', 'vbe_mode', 'vbe_seg', 'vbe_lst']
for i, v in enumerate(VAR): SYM[v] = 0xA500 + 2 * i

# ---- разбор текста ----
prog, labels = [], {}
for raw in body.split('\n'):
    line = raw.split('#')[0].strip()
    if not line: continue
    if line.endswith(':'): labels[line[:-1]] = len(prog); continue
    mm = re.match(r'(\S+)\s*(.*)', line)
    ops = [o.strip() for o in re.split(r',(?![^(]*\))', mm.group(2))] if mm.group(2) else []
    prog.append((mm.group(1), ops))

R32 = ['ax', 'cx', 'dx', 'bx', 'sp', 'bp', 'si', 'di']
class CPU:
    def __init__(self, bios):
        self.mem = bytearray(0x100000); self.r = {n: 0 for n in R32}; self.seg = {'ds': 0, 'es': 0, 'fs': 0, 'ss': 0}
        self.zf = 0; self.stack = []; self.bios = bios; self.steps = 0
        self.r['sp'] = 0x7C00
        for i, v in enumerate(want): self.w16(SYM['vbe_want'] + 2 * i, v)
    def w16(self, a, v): self.mem[a:a + 2] = struct.pack('<H', v & 0xFFFF)
    def r16(self, a): return struct.unpack('<H', self.mem[a:a + 2])[0]
    def w32(self, a, v): self.mem[a:a + 4] = struct.pack('<I', v & 0xFFFFFFFF)
    def r32(self, a): return struct.unpack('<I', self.mem[a:a + 4])[0]
    def getreg(self, n):
        if n in R32: return self.r[n] & 0xFFFF
        if n in self.seg: return self.seg[n]
        if n[0] == 'e' and n[1:] in R32: return self.r[n[1:]]
        base = {'a': 'ax', 'c': 'cx', 'd': 'dx', 'b': 'bx'}[n[0]]
        return (self.r[base] >> (8 if n[1] == 'h' else 0)) & 0xFF
    def setreg(self, n, v):
        if n in R32: self.r[n] = (self.r[n] & 0xFFFF0000) | (v & 0xFFFF); return
        if n in self.seg: self.seg[n] = v & 0xFFFF; return
        if n[0] == 'e' and n[1:] in R32: self.r[n[1:]] = v & 0xFFFFFFFF; return
        base = {'a': 'ax', 'c': 'cx', 'd': 'dx', 'b': 'bx'}[n[0]]
        sh = 8 if n[1] == 'h' else 0
        self.r[base] = (self.r[base] & ~(0xFF << sh)) | ((v & 0xFF) << sh)
    def num(self, t):
        t = t.strip()
        if t.startswith('$'): t = t[1:]
        if re.fullmatch(r'-?(0x[0-9a-fA-F]+|\d+)', t): return int(t, 0)
        mm = re.fullmatch(r'(\w+)\s*\+\s*(0x[0-9a-fA-F]+|\d+)', t)
        if mm: return SYM[mm.group(1)] + int(mm.group(2), 0)
        return SYM[t]
    def addr(self, t):
        seg = 'ds'
        mm = re.match(r'%(\w+):(.*)', t)
        if mm: seg, t = mm.group(1), mm.group(2)
        mm = re.fullmatch(r'(-?\w*)\((%\w+)\)', t)
        if mm: off = (self.num(mm.group(1)) if mm.group(1) else 0) + self.getreg(mm.group(2)[1:])
        else: off = self.num(t)
        return self.seg[seg] * 16 + (off & 0xFFFF)
    def isreg(self, t): return re.fullmatch(r'%\w+', t) is not None
    def get(self, t, size):
        if self.isreg(t): return self.getreg(t[1:])
        if t.startswith('$'): return self.num(t)
        a = self.addr(t)
        return {1: self.mem[a], 2: self.r16(a), 4: self.r32(a)}[size]
    def put(self, t, v, size):
        if self.isreg(t): self.setreg(t[1:], v); return
        a = self.addr(t)
        if size == 1: self.mem[a] = v & 0xFF
        elif size == 2: self.w16(a, v)
        else: self.w32(a, v)
    def run(self):
        pc = 0
        while pc < len(prog):
            self.steps += 1
            if self.steps > 200000: raise RuntimeError('infinite loop')
            op, a = prog[pc]; pc += 1
            if op == 'pushal': self.stack.append(dict(self.r)); continue
            if op == 'popal': self.r = self.stack.pop(); continue
            if op == 'pushw': self.stack.append(self.get(a[0], 2)); continue
            if op == 'popw': self.put(a[0], self.stack.pop(), 2); continue
            if op == 'cld': continue
            if op == 'rep':
                assert a[0].startswith('movsw')
                for _ in range(self.getreg('cx')):
                    self.w16(self.seg['es'] * 16 + self.getreg('di'), self.r16(self.seg['ds'] * 16 + self.getreg('si')))
                    self.setreg('si', self.getreg('si') + 2); self.setreg('di', self.getreg('di') + 2)
                self.setreg('cx', 0); continue
            if op == 'int':
                assert a[0] == '$0x10'; self.bios(self); continue
            if op in ('jne', 'je', 'jz', 'jmp'):
                if op == 'jmp' or (op == 'jne' and not self.zf) or (op in ('je', 'jz') and self.zf): pc = labels[a[0]]
                continue
            size = {'b': 1, 'w': 2, 'l': 4}[op[-1]]
            base = op[:-1]
            if base == 'mov': self.put(a[1], self.get(a[0], size), size); continue
            if base == 'cmp':
                x, y = self.get(a[1], size), self.get(a[0], size); self.zf = int((x & ((1 << 8 * size) - 1)) == (y & ((1 << 8 * size) - 1))); continue
            if base == 'test': v = self.get(a[0], size) & self.get(a[1], size); self.zf = int(v == 0); continue
            if base in ('add', 'and', 'or', 'xor'):
                x, y = self.get(a[1], size), self.get(a[0], size)
                v = {'add': x + y, 'and': x & y, 'or': x | y, 'xor': x ^ y}[base]
                self.put(a[1], v, size); self.zf = int((v & ((1 << 8 * size) - 1)) == 0); continue
            raise RuntimeError('unsupported: %s %s' % (op, a))

class Bios:
    """Настраиваемый VBE BIOS: modes = [(номер, ширина, высота, бит, модель, атрибуты)]."""
    def __init__(self, modes, ctrl_ok=True, setmode_ok=True, sig=b'VESA'):
        self.modes, self.ctrl_ok, self.setmode_ok, self.sig = modes, ctrl_ok, setmode_ok, sig
        self.set_calls = []; self.font_calls = 0
    def __call__(self, c):
        ax = c.getreg('ax'); es = c.seg['es']
        if ax == 0x1130:
            assert c.getreg('bh') == 6; self.font_calls += 1
            c.seg['es'] = 0xC000; c.setreg('bp', 0x1000)
            for i in range(4096): c.mem[0xC0000 + 0x1000 + i] = (i * 7 + 3) & 0xFF
            return
        if ax == 0x4F00:
            di = es * 16 + c.getreg('di')
            assert bytes(c.mem[di:di + 4]) == b'VBE2'
            if not self.ctrl_ok: c.setreg('ax', 0x014F); return
            c.mem[di:di + 4] = self.sig; c.w16(di + 0x0E, 0x2000); c.w16(di + 0x10, 0xC000)
            lst = 0xC0000 + 0x2000
            for i, md in enumerate(self.modes): c.w16(lst + 2 * i, md[0])
            c.w16(lst + 2 * len(self.modes), 0xFFFF)
            c.setreg('ax', 0x004F); return
        if ax == 0x4F01:
            di = es * 16 + c.getreg('di'); mode = c.getreg('cx')
            hit = [md for md in self.modes if md[0] == mode]
            if not hit: c.setreg('ax', 0x014F); return
            n, w, h, bpp, model, attr = hit[0]
            c.mem[di:di + 256] = bytes(256)
            c.w16(di, attr); c.w16(di + 0x10, w * bpp // 8); c.w16(di + 0x12, w); c.w16(di + 0x14, h)
            c.mem[di + 0x19] = bpp; c.mem[di + 0x1B] = model
            c.mem[di + 0x20] = 16; c.mem[di + 0x22] = 8; c.mem[di + 0x24] = 0
            c.w32(di + 0x28, 0xE0000000 + n)
            c.setreg('ax', 0x004F); return
        if ax == 0x4F02:
            self.set_calls.append(c.getreg('bx'))
            c.setreg('ax', 0x004F if self.setmode_ok else 0x014F); return
        raise RuntimeError('unexpected int 10h ax=%04x' % ax)

def run(bios):
    cpu = CPU(bios); cpu.run()
    fb = cpu.mem[SYM['fbinfo']:SYM['fbinfo'] + 18]
    magic, base, w, h, pitch, bpp, rp, gp, bp_ = struct.unpack('<IIHHHBBBB', fb)
    return cpu, dict(magic=magic, base=base, w=w, h=h, pitch=pitch, bpp=bpp, rpos=rp, gpos=gp, bpos=bp_)

STD = (0x9B)                      # поддерживается, графика, линейный буфер
fails = 0
def check(c, name):
    global fails
    if not c: print('FAIL:', name); fails += 1

# 1. нормальный BIOS: выбирается 1024x768x32
bios = Bios([(0x101, 640, 480, 8, 4, 0x9B), (0x112, 640, 480, 32, 6, 0x9B), (0x115, 800, 600, 32, 6, 0x9B), (0x118, 1024, 768, 32, 6, 0x9B), (0x11B, 1280, 1024, 32, 6, 0x9B)])
cpu, fb = run(bios)
check(fb['magic'] == 0x31494246 and fb['w'] == 1024 and fb['h'] == 768 and fb['pitch'] == 4096 and fb['bpp'] == 32, '1024x768 selected: %s' % fb)
check(fb['base'] == 0xE0000118 and (fb['rpos'], fb['gpos'], fb['bpos']) == (16, 8, 0), 'base and masks copied')
check(bios.set_calls == [0x4118], 'mode set with LFB bit: %s' % bios.set_calls)
check(bytes(cpu.mem[SYM['romfont']:SYM['romfont'] + 4096]) == bytes((i * 7 + 3) & 0xFF for i in range(4096)) and bios.font_calls == 1, 'ROM font copied')
check(cpu.seg['ds'] == 0 and cpu.seg['es'] == 0 and cpu.r['sp'] == 0x7C00 and not cpu.stack, 'DS=ES=0, stack balanced')
# 2. нет 1024x768 - берём 800x600
bios = Bios([(0x112, 640, 480, 32, 6, 0x9B), (0x115, 800, 600, 32, 6, 0x9B)])
cpu, fb = run(bios)
check(fb['magic'] == 0x31494246 and (fb['w'], fb['h']) == (800, 600) and bios.set_calls == [0x4115], 'fallback to 800x600: %s %s' % (fb, bios.set_calls))
# 3. нет подходящих режимов (8 бит, не линейный буфер, неверная модель)
bios = Bios([(0x105, 1024, 768, 8, 4, 0x9B), (0x118, 1024, 768, 32, 6, 0x1B), (0x119, 1024, 768, 32, 4, 0x9B), (0x11A, 1024, 768, 32, 6, 0x00)])
cpu, fb = run(bios)
check(fb['magic'] == 0 and bios.set_calls == [], 'no suitable mode: text mode stays, no set call')
# 4. контроллер не отвечает / неверная подпись
cpu, fb = run(Bios([(0x118, 1024, 768, 32, 6, 0x9B)], ctrl_ok=False)); check(fb['magic'] == 0, 'VBE controller error')
bios = Bios([(0x118, 1024, 768, 32, 6, 0x9B)], sig=b'XXXX'); cpu, fb = run(bios); check(fb['magic'] == 0 and bios.set_calls == [], 'bad VESA signature')
# 5. режим не включился: magic обнуляется
bios = Bios([(0x118, 1024, 768, 32, 6, 0x9B)], setmode_ok=False); cpu, fb = run(bios)
check(fb['magic'] == 0 and bios.set_calls == [0x4118], 'mode set failed: magic cleared')
check(cpu.seg['ds'] == 0 and cpu.seg['es'] == 0 and not cpu.stack, 'registers restored on failure')
# 6. пустой список режимов
cpu, fb = run(Bios([])); check(fb['magic'] == 0, 'empty mode list')
if fails: print('test_vbe_asm: %d FAILED' % fails); sys.exit(1)
print('test_vbe_asm: OK')
