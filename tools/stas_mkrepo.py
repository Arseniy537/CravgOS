#!/usr/bin/env python3
"""Генерирует index.txt для репозитория stas: python3 tools/stas_mkrepo.py stas-repo
Описание пакета берётся из первой строки-комментария вида '# имя - описание' (скрипты stas)
или '-- имя - описание' (пакеты Lua, у них первая строка '--lua', вторая может быть '--needs ...')."""
import hashlib, os, sys, re
d = sys.argv[1] if len(sys.argv) > 1 else 'stas-repo'
lines = []
for f in sorted(os.listdir(d)):
    if not f.endswith('.stp'): continue
    name = f[:-4]
    assert re.fullmatch(r'[a-z0-9_-]{1,15}', name), f"bad package name: {name}"
    data = open(os.path.join(d, f), 'rb').read()
    bundle = data.startswith(b'--stas-bundle')
    assert bundle or len(data) <= 8192, f"{f}: {len(data)} bytes, max 8192 (RAMFS limit)"
    desc = name
    text = data.split(b'@@ ' + name.encode() + b' ', 1)[-1] if bundle else data
    for first in text.split(b'\n')[:6]:
        m = re.match(r'(?:#|--)\s*' + re.escape(name) + r'\s*-\s*(.*)', first.decode())
        if m: desc = m.group(1).strip(); break
    lines.append(f"{name} {hashlib.sha256(data).hexdigest()} {len(data)} {desc}")
out = '\n'.join(lines) + '\n'
assert len(out) <= 8192, "index.txt must stay under 8192 bytes"
open(os.path.join(d, 'index.txt'), 'w', newline='\n').write(out)
print(out, end='')
