#!/usr/bin/env python3
"""Делает /tmp/font8x16.bin (256 глифов 8x16) из DejaVu Sans Mono - только для снимков экрана в тестах
(в настоящем ядре используется шрифт BIOS, скопированный загрузчиком)."""
import sys, glob
from PIL import Image, ImageDraw, ImageFont
cands = glob.glob('/usr/share/fonts/**/DejaVuSansMono.ttf', recursive=True) + glob.glob('/usr/share/fonts/**/*Mono*.ttf', recursive=True)
if not cands: print('no mono font found'); sys.exit(1)
font = ImageFont.truetype(cands[0], 13, layout_engine=ImageFont.Layout.BASIC)
out = bytearray()
for code in range(256):
    ch = bytes([code]).decode('cp437')
    img = Image.new('L', (8, 16), 0)
    if code >= 32 and code != 127:
        try: ImageDraw.Draw(img).text((0, 1), ch, fill=255, font=font)
        except Exception: pass
    for y in range(16):
        b = 0
        for x in range(8): b = (b << 1) | (1 if img.getpixel((x, y)) > 110 else 0)
        out.append(b)
open('/tmp/font8x16.bin', 'wb').write(out)
print('font ok:', cands[0])
