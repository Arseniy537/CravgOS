#!/bin/bash
set -e
SRC=$(cd "$(dirname "$0")/.." && pwd)
D=$(mktemp -d); cd "$D"
gcc -O1 -Wall -Wno-unused-function -Wno-unused-variable -o t_https "$SRC/tests/test_https.c"
openssl req -x509 -newkey rsa:2048 -nodes -keyout k.pem -out c.pem -subj /CN=localhost -days 2 2>/dev/null
head -c 250000 /dev/urandom | base64 > big.txt
echo "tiny https page" > small.txt
printf '%s' '{"Status":0,"Question":[{"name":"doh.test","type":1}],"Answer":[{"name":"doh.test","type":1,"TTL":300,"data":"203.0.113.77"}]}' > 'dns-query?type=A&name=doh.test'
printf '%s' '{ "Status": 0, "Answer": [ { "name": "g.test.", "type": 1, "TTL": 1, "data": "8.8.4.4" } ] }' > 'resolve?type=A&name=g.test'
printf '%s' '{"Status":3,"Question":[{"name":"gone.test","type":1}]}' > 'dns-query?type=A&name=gone.test'
printf '%s' '{"Status":3}' > 'resolve?type=A&name=gone.test'
openssl s_server -quiet -accept 14434 -tls1_2 -cipher ECDHE-RSA-AES128-GCM-SHA256 -cert c.pem -key k.pem -WWW >/dev/null 2>&1 &
SP=$!; sleep 0.7
./t_https 14434 && rc=0 || rc=1
kill $SP 2>/dev/null || true; wait $SP 2>/dev/null || true
rm -rf "$D"; exit $rc
