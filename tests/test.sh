#!/bin/bash

echo "[~] Start proxy"
../build/AsyncHttpProxy 9997 &
PROXY_PID=$!

sleep 1

EXPECTED=4096

# TEST 1
timeout 3 python3 -c 'print("HTTP/1.1 200 OK\r\nContent-Length: 4096\r\n\r\n" + "A"*4096, end="")' | nc -l 127.0.0.1 -p 8088 > /dev/null 2>&1 &
sleep 1
wget -e use_proxy=yes -e http_proxy=127.0.0.1:9997 http://127.0.0.1:8088/ -O simple.html -q
SIZE1=$(wc -c < simple.html 2>/dev/null || echo 0)

if [ "$SIZE1" -eq "$EXPECTED" ]; then
    echo "[+] TEST 1"
    rm -f simple.html
else
    echo "[!] TEST 1: $SIZE1 байт "
fi



# TEST 2
timeout 3 python3 -c 'print("HTTP/1.1 200 OK\r\n\r\n" + "A"*4096, end="")' | nc -l 127.0.0.1 -p 8088 > /dev/null 2>&1 &
sleep 1
wget -e use_proxy=yes -e http_proxy=127.0.0.1:9997 http://127.0.0.1:8088/ -O empty.html -q
SIZE2=$(wc -c < empty.html 2>/dev/null || echo 0)
if [ "$SIZE2" -eq 0 ]; then
    echo "[+] TEST 2"
    rm -f empty.html
else
    echo "[!] TEST 2:"
fi

kill $PROXY_PID 2>/dev/null
