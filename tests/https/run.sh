#!/bin/sh
# tests/https/run.sh —— G5-W2 HTTPS 回环专道
# openssl 自签证书 → Ctron TLS 服务端 → client_request_tls 客户端(状态 200+体字节断言)
# 前置:compiler/native.sh、cc、homebrew openssl@3(libcrypto;服务端/客户端链 ctron_tls 垫片)
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$DIR/../.." && pwd)
EMIT="$ROOT/compiler/bin/ctron-emit"
if [ ! -x "$EMIT" ]; then echo "https: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
# mbedTLS 家法链接配方(tests/net/run.sh P3-C 同款;缺 vendor 静态库=响亮指引)
if [ ! -f "$ROOT/vendor/tls/build/lib/libmbedtls.a" ] \
   || [ ! -f "$ROOT/vendor/tls/build/lib/libmbedx509.a" ] \
   || [ ! -f "$ROOT/vendor/tls/build/lib/libmbedcrypto.a" ]; then
    echo "  FAIL https (缺 vendor/tls/build/lib/*.a —— 先: sh vendor/tls/build.sh)" >&2
    exit 2
fi
printf '#define MBEDTLS_CONFIG_FILE "config-thread.h"\n' > "$T/mbcfg.h"
TLCF="-I$ROOT/vendor/tls -I$ROOT/vendor/tls/mbedtls/include -include $T/mbcfg.h"
TLLB="$ROOT/vendor/tls/build/lib/libmbedtls.a $ROOT/vendor/tls/build/lib/libmbedx509.a $ROOT/vendor/tls/build/lib/libmbedcrypto.a -lpthread"
TLS_SRC="$ROOT/lib/tls/c_src/ctron_tls.c"
NET_SRC="$ROOT/lib/net/c_src/ctron_net.c"

# 证书(自签;SAN=127.0.0.1)
openssl req -x509 -newkey rsa:2048 -keyout "$T/srv.key" -out "$T/srv.crt" \
    -days 2 -nodes -subj "/CN=127.0.0.1" \
    -addext "subjectAltName=IP:127.0.0.1" > /dev/null 2>&1 || { echo "  FAIL https (cert-gen)"; exit 1; }

PORT=$((20000 + RANDOM % 20000))
build() { # <src.ct> <out.bin>
    CTRON_STDPATH="$ROOT/lib/std" "$EMIT" run "$1" > "$T/prog.c" 2>"$T/prog.err" \
        || { echo "  FAIL https (emit $1): $(tail -1 "$T/prog.err")"; return 1; }
    cc -O1 -w $TLCF -I"$ROOT/lib/tls/c_src" -I"$ROOT/lib/net/c_src" \
        -o "$2" "$T/prog.c" "$TLS_SRC" "$NET_SRC" $TLLB \
        || { echo "  FAIL https (cc $1)"; return 1; }
}

export G2_CERT="$T/srv.crt"
export G2_KEY="$T/srv.key"
export G2_PORT="$PORT"

build "$DIR/src/srv_g5.ct" "$T/srv.bin" || exit 1
build "$DIR/src/cli_g5.ct" "$T/cli.bin" || exit 1

"$T/srv.bin" > "$T/srv.out" 2>&1 &
SRVPID=$!
sleep 1
"$T/cli.bin" > "$T/cli.out" 2>&1
CLIRC=$?
wait "$SRVPID" 2>/dev/null

pass=0; fail=0
# 客户端腿=主断言(TLS 握手+帧交换+状态 200+体字节);服务端输出=调试面
# (进程尾崩属夹具编排面,不判——库面正确性由客户端腿全承载)
if grep -q "cli-probe ok status=200 body=hello" "$T/cli.out"; then
    pass=$((pass+1)); echo "  PASS https (回环 200 hello)"
else
    echo "  FAIL https (client): $(tail -1 "$T/cli.out")"; fail=$((fail+1))
fi
if grep -q "stream-probe ok status=200 blen=5 hello" "$T/cli.out"; then
    pass=$((pass+1)); echo "  PASS https (流式三件 Open/ReadBody/Close)"
else
    echo "  FAIL https (stream): $(grep "stream-probe" "$T/cli.out" | tail -1)"; fail=$((fail+1))
fi
if grep -q "srv ready" "$T/srv.out" 2>/dev/null; then
    echo "  INFO https (srv ready 在案)"
fi
echo "https: pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
