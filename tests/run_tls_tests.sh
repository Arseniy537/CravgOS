#!/bin/bash
# Поднимает локальный openssl s_server и проверяет наш TLS-клиент (RSA и ECDSA сертификаты, большой файл).
set -e
SRC=$(cd "$(dirname "$0")/.." && pwd)
D=$(mktemp -d); cd "$D"
gcc -O2 -Wall -Wno-unused-function -o t_tls "$SRC/tests/test_tls.c" 2>&1 | grep -v "^$" || true
openssl req -x509 -newkey rsa:2048 -nodes -keyout rsa.key -out rsa.crt -subj /CN=localhost -days 2 2>/dev/null
openssl ecparam -name prime256v1 -genkey -noout -out ec.key 2>/dev/null
openssl req -x509 -new -key ec.key -out ec.crt -subj /CN=localhost -days 2 2>/dev/null
head -c 300000 /dev/urandom | base64 > big.txt
echo "tiny page" > small.txt
fail=0
run() {   # имя  cert key cipher  файл
  openssl s_server -quiet -accept 14433 -tls1_2 -cipher "$4" -cert "$2" -key "$3" -WWW >/dev/null 2>&1 &
  SP=$!; sleep 0.7
  echo "== $1 / $5"
  if ./t_tls 14433 "/$5" "$5"; then :; else fail=1; fi
  kill $SP 2>/dev/null || true; wait $SP 2>/dev/null || true
}
run "RSA cert, ECDHE-RSA-AES128-GCM"   rsa.crt rsa.key ECDHE-RSA-AES128-GCM-SHA256   small.txt
run "RSA cert, ECDHE-RSA-AES128-GCM"   rsa.crt rsa.key ECDHE-RSA-AES128-GCM-SHA256   big.txt
run "EC cert, ECDHE-ECDSA-AES128-GCM"  ec.crt  ec.key  ECDHE-ECDSA-AES128-GCM-SHA256 big.txt
rm -rf "$D"
if [ $fail = 0 ]; then echo "TLS interop tests OK"; else echo "TLS interop tests FAILED"; exit 1; fi
