#!/usr/bin/env python3
"""Собирает пакеты-связки stas из каталогов wm/<имя>/*.lua в stas-repo/<имя>.stp.
Связка - один файл, который stas ставит одним запросом: "--stas-bundle", затем блоки "@@ имя размер" + ровно размер байт.
В каталоге должен быть файл <имя>.lua (точка входа, запускается по имени). Остальные .lua - модули (/apps/<модуль>).
В точку входа автоматически добавляется строка "--members <модули>" (по ней stas remove удаляет всю связку).
Использование:  python3 tools/stas_bundle.py [каталог-со-связками]   (по умолчанию wm)"""
import os, re, sys
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = os.path.join(root, sys.argv[1] if len(sys.argv) > 1 else 'wm')
dst = os.path.join(root, 'stas-repo')
NAME = re.compile(r'^[a-z0-9_-]{1,15}$')
def build(name, d):
    files = {f[:-4]: open(os.path.join(d, f), 'rb').read() for f in sorted(os.listdir(d)) if f.endswith('.lua')}
    assert name in files, f'{name}: нет точки входа {name}.lua'
    for n, data in files.items():
        assert NAME.match(n), f'{name}: недопустимое имя модуля {n}'
        assert len(data) <= 8192, f'{name}/{n}.lua: {len(data)} байт, максимум 8192 (лимит файла RAMFS)'
        assert b'\r' not in data, f'{name}/{n}.lua: CRLF, нужен LF'
    members = [n for n in files if n != name]
    entry = files[name].split(b'\n')
    assert entry[0].strip() == b'--lua', f'{name}.lua должен начинаться со строки --lua'
    head = [i for i, l in enumerate(entry[:4]) if l.startswith(b'--members ')]
    line = b'--members ' + ' '.join(members).encode()
    assert len(line) < 190, f'{name}: слишком длинный список модулей'
    if head: entry[head[0]] = line
    else:
        at = 2 if len(entry) > 1 and entry[1].startswith(b'--needs ') else 1
        entry.insert(at, line)
    files[name] = b'\n'.join(entry)
    assert len(files[name]) <= 8192, f'{name}.lua после вставки --members длиннее 8192'
    out = b'--stas-bundle\n'
    for n in [name] + members:
        out += b'@@ ' + n.encode() + b' ' + str(len(files[n])).encode() + b'\n' + files[n] + b'\n'
    assert len(out) < 200000, f'{name}: связка больше буфера загрузки'
    open(os.path.join(dst, name + '.stp'), 'wb').write(out)
    print(f'{name}.stp: {len(members) + 1} файлов, {len(out)} байт')
if os.path.isdir(src):
    for name in sorted(os.listdir(src)):
        d = os.path.join(src, name)
        if os.path.isdir(d): build(name, d)
