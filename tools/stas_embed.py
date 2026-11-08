#!/usr/bin/env python3
"""Встраивает Lua-пакеты из stas-repo/ в ядро для офлайн-режима (stas repo offline).
Запуск: python3 tools/stas_embed.py  ->  stas_lua_pkgs.h (подключается из stas.h)."""
import hashlib, os, re, sys
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
d = os.path.join(root, 'stas-repo')
def cstr(data):
    out = []
    for line in data.decode('ascii').split('\n'):
        out.append('"' + line.replace('\\', '\\\\').replace('"', '\\"').replace('?', '\\?') + '\\n"')
    if data.endswith(b'\n'): out.pop()
    return '\n    '.join(out) if out else '""'
idx, pk = [], []
for f in sorted(os.listdir(d)):
    if not f.endswith('.stp'): continue
    data = open(os.path.join(d, f), 'rb').read()
    if not data.startswith(b'--lua'): continue
    name = f[:-4]
    desc = name
    for first in data.split(b'\n')[:4]:
        m = re.match(r'--\s*' + re.escape(name) + r'\s*-\s*(.*)', first.decode())
        if m: desc = m.group(1).strip(); break
    idx.append('%s %s %d %s' % (name, hashlib.sha256(data).hexdigest(), len(data), desc))
    pk.append((f, data))
o = ['/* создано tools/stas_embed.py - не править вручную: Lua-пакеты для офлайн-режима stas */']
o.append('static const char STAS_LUA_INDEX[] =\n    ' + '\n    '.join('"%s\\n"' % l for l in idx) + ';')
for i, (f, data) in enumerate(pk):
    o.append('static const char STAS_LUA_PKG_%d[] =\n    %s;' % (i, cstr(data)))
o.append('#define STAS_LUA_NPKGS %d' % len(pk))
o.append('static const struct { const char *file; const char *data; } STAS_LUA_PKGS[] = {')
for i, (f, data) in enumerate(pk): o.append('    {"%s", STAS_LUA_PKG_%d},' % (f, i))
o.append('};')
open(os.path.join(root, 'stas_lua_pkgs.h'), 'w', newline='\n').write('\n'.join(o) + '\n')
print('stas_lua_pkgs.h: %d Lua packages' % len(pk))
