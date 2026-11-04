#!/usr/bin/env python3
"""Собирает загрузочный ISO (ISO9660 + El Torito) из плоского образа. Без grub/xorriso."""
import struct, sys, datetime
SEC = 2048
img = open(sys.argv[1], 'rb').read()
out = sys.argv[2]
now = datetime.datetime.now(datetime.timezone.utc)

def b32(n): return struct.pack('<I', n) + struct.pack('>I', n)
def b16(n): return struct.pack('<H', n) + struct.pack('>H', n)
def pad(s, n): return s.encode().ljust(n, b' ')
def dt7(): return bytes([now.year - 1900, now.month, now.day, now.hour, now.minute, now.second, 0])
def dt17(): return now.strftime('%Y%m%d%H%M%S').encode() + b'00' + b'\x00'
def dirrec(extent, length, flags, name):
    L = 33 + len(name)
    L += L % 2
    r = bytes([L, 0]) + b32(extent) + b32(length) + dt7() + bytes([flags, 0, 0]) + b16(1) + bytes([len(name)]) + name
    return r.ljust(L, b'\0')

IMG_SEC = 23
img_sectors = -(-len(img) // SEC)
total = IMG_SEC + img_sectors
root = dirrec(21, SEC, 2, b'\x00')

pvd = bytearray(SEC)
pvd[0] = 1; pvd[1:6] = b'CD001'; pvd[6] = 1
pvd[8:40] = pad('', 32); pvd[40:72] = pad('CRAVGOS_64', 32)
pvd[80:88] = b32(total)
pvd[120:124] = b16(1); pvd[124:128] = b16(1); pvd[128:132] = b16(SEC)
pvd[132:140] = b32(10)
pvd[140:144] = struct.pack('<I', 19)
pvd[148:152] = struct.pack('>I', 20)
pvd[156:190] = root
for off, n in ((190, 128), (318, 128), (446, 128), (574, 128), (702, 37), (739, 37), (776, 37)):
    pvd[off:off + n] = pad('', n)
pvd[813:830] = dt17(); pvd[830:847] = dt17()
pvd[847:864] = b'0' * 16 + b'\x00'; pvd[864:881] = dt17()
pvd[881] = 1

brvd = bytearray(SEC)
brvd[0] = 0; brvd[1:6] = b'CD001'; brvd[6] = 1
brvd[7:39] = b'EL TORITO SPECIFICATION'.ljust(32, b'\0')
brvd[71:75] = struct.pack('<I', 22)

term = bytearray(SEC); term[0] = 255; term[1:6] = b'CD001'; term[6] = 1

ptl = bytearray(SEC); ptl[0:10] = struct.pack('<BBIH', 1, 0, 21, 1) + b'\x00\x00'
ptm = bytearray(SEC); ptm[0:10] = struct.pack('>BBIH', 1, 0, 21, 1) + b'\x00\x00'

rootdir = bytearray(SEC)
d = dirrec(21, SEC, 2, b'\x00') + dirrec(21, SEC, 2, b'\x01') + dirrec(IMG_SEC, len(img), 0, b'CRAVGOS.BIN;1')
rootdir[:len(d)] = d

cat = bytearray(SEC)
v = bytearray(32); v[0] = 1; v[1] = 0; v[4:28] = b'CRAVGOS'.ljust(24, b'\0'); v[30] = 0x55; v[31] = 0xAA
s = sum(struct.unpack('<16H', bytes(v))) & 0xFFFF
v[28:30] = struct.pack('<H', (-s) & 0xFFFF)
e = bytearray(32); e[0] = 0x88; e[1] = 0
e[6:8] = struct.pack('<H', -(-len(img) // 512)); e[8:12] = struct.pack('<I', IMG_SEC)
cat[0:32] = v; cat[32:64] = e

iso = bytearray(16 * SEC)
for sec in (pvd, brvd, term, ptl, ptm, rootdir, cat):
    iso += sec
iso += img.ljust(img_sectors * SEC, b'\0')
assert len(iso) == total * SEC
open(out, 'wb').write(iso)
print(f"{out}: {len(iso)} bytes, boot image {len(img)} bytes ({-(-len(img)//512)} sectors)")
