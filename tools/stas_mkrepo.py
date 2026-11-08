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
    assert len(data) <= 2048, f"{f}: {len(data)} bytes, max 2048 (RAMFS limit)"
    desc = name
    for first in data.split(b'\n')[:4]:
        m = re.match(r'(?:#|--)\s*' + re.escape(name) + r'\s*-\s*(.*)', first.decode())
        if m: desc = m.group(1).strip(); break
    lines.append(f"{name} {hashlib.sha256(data).hexdigest()} {len(data)} {desc}")
out = '\n'.join(lines) + '\n'
assert len(out) <= 2048, "index.txt must stay under 2048 bytes"
open(os.path.join(d, 'index.txt'), 'w', newline='\n').write(out)
print(out, end='')
