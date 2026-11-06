#!/usr/bin/env python3
"""Run all unit tests across platforms."""
import subprocess, os, sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(root)

tests = ['test_fs', 'test_net', 'test_html', 'test_http', 'test_crypto', 'test_stas']
failed = 0
for t in tests:
    print(f"=== Running {t} ===")
    exe = f"{t}.exe" if os.name == 'nt' else f"./{t}"
    compile_cmd = ['gcc', '-Wall', '-Wno-unused-function', '-I.', '-o', exe, f'tests/{t}.c']
    c = subprocess.run(compile_cmd, capture_output=True, text=True)
    if c.returncode != 0:
        print(f"Compilation of {t} failed:\n{c.stderr}")
        failed += 1
        continue
    r = subprocess.run([exe], capture_output=True, text=True)
    if r.returncode != 0:
        print(f"{t} failed with returncode {r.returncode}:\n{r.stdout}\n{r.stderr}")
        failed += 1
    else:
        print(r.stdout.strip())
    if os.path.exists(exe):
        try: os.remove(exe)
        except OSError: pass

if failed:
    print(f"\n{failed} test(s) failed!")
    sys.exit(1)
print("\nAll unit tests passed successfully!")
