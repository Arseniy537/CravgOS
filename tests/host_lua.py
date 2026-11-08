#!/usr/bin/env python3
"""Хост-стенд для stas: настоящий kernel.c + мок-сеть, отдающая stas-repo/ по HTTP.
Запуск из корня репозитория:  python3 tests/host_lua.py && /tmp/host_lua"""
import re, subprocess, sys, os
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
hm = open(os.path.join(root, 'tests', 'host_main.c')).read()
head = hm.split('/* ---- сценарии ---- */')[0]
# страница мока: мутабельный указатель + выбор ответа по пути запроса
head = re.sub(r'static const char \*page = ("(?:[^"\\]|\\.)*");', r'static const char *page_default = \1; static const char *page; static char repo_index[4096], repo_nim[4096];', head, count=1)
assert 'page_default' in head
head = head.replace('s_rcv += dl; char hdr[200];',
    's_rcv += dl; { const char *rq = (const char *)u + hl; page = page_default; if (strstr(rq, "stas-repo/index.txt")) page = repo_index; else if (strstr(rq, "stas-repo/nim.stp")) page = repo_nim; } char hdr[200];', 1)
assert 'repo_index; else' in head
head = head.replace('int main(void)', 'int main_unused(void)')
foot = open(os.path.join(root, 'tests', 'host_lua_main.c')).read()
# сценарии стенда используют screen_has / sc_fail из хвоста host_main.c: берём их оттуда
helpers = re.search(r'static int sc_fail;.*?\n\}\n', hm, re.S).group(0)
import tempfile
tmp_dir = '/tmp' if os.path.exists('/tmp') else tempfile.gettempdir()
hl_main = os.path.join(tmp_dir, 'hl_main.c')
open(hl_main, 'w').write(head + '\n' + helpers + '\n' + foot)
sys.exit(subprocess.call([sys.executable, os.path.join(root, 'tests', 'host_kernel.py'), hl_main, 'host_lua']))
