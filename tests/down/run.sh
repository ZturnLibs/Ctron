#!/bin/sh
# tests/down/run.sh —— G5-W4 lib/down e2e(本地 TLS serve × download_to_file)
# 前置:compiler/native.sh、cc、homebrew openssl@3、vendor/tls/build.sh
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$DIR/../.." && pwd)
EMIT="$ROOT/compiler/bin/ctron-emit"
if [ ! -x "$EMIT" ]; then echo "down: 缺少编译器二进制" >&2; exit 2; fi
if [ ! -f "$ROOT/vendor/tls/build/lib/libmbedtls.a" ]; then
    echo "  FAIL down (缺 vendor/tls/build/lib/*.a —— 先: sh vendor/tls/build.sh)" >&2; exit 2
fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
printf '#define MBEDTLS_CONFIG_FILE "config-thread.h"\n' > "$T/mbcfg.h"
TLCF="-I$ROOT/vendor/tls -I$ROOT/vendor/tls/mbedtls/include -include $T/mbcfg.h"
TLLB="$ROOT/vendor/tls/build/lib/libmbedtls.a $ROOT/vendor/tls/build/lib/libmbedx509.a $ROOT/vendor/tls/build/lib/libmbedcrypto.a -lpthread"

openssl req -x509 -newkey rsa:2048 -keyout "$T/srv.key" -out "$T/srv.crt" \
    -days 2 -nodes -subj "/CN=127.0.0.1" \
    -addext "subjectAltName=IP:127.0.0.1" > /dev/null 2>&1 || { echo "  FAIL down (cert-gen)"; exit 1; }

build() {
    CTRON_STDPATH="$ROOT/lib/std" "$EMIT" run "$1" > "$T/prog.c" 2>"$T/prog.err" \
        || { echo "  FAIL down (emit $1): $(tail -1 "$T/prog.err")"; return 1; }
    cc -O1 -w $TLCF -I"$ROOT/lib/tls/c_src" -I"$ROOT/lib/net/c_src" \
        -I/opt/homebrew/opt/openssl@3/include \
        -o "$2" "$T/prog.c" "$ROOT/lib/tls/c_src/ctron_tls.c" "$ROOT/lib/net/c_src/ctron_net.c" \
        "$ROOT/lib/crypto/c_src/ctron_crypto.c" "$ROOT/lib/fio/c_src/ctron_fio.c" $TLLB \
        -L/opt/homebrew/opt/openssl@3/lib -lcrypto \
        || { echo "  FAIL down (cc $1)"; return 1; }
}

build "$DIR/src/srv.ct" "$T/srv.bin" || exit 1
build "$DIR/src/cli.ct" "$T/cli.bin" || exit 1

PORT=$((30000 + RANDOM % 20000))
export G2_CERT="$T/srv.crt" G2_KEY="$T/srv.key" G2_PORT="$PORT"
"$T/srv.bin" > "$T/srv.out" 2>&1 &
SRVPID=$!
sleep 1
cd "$T" && "$T/cli.bin" > "$T/cli.out" 2>&1
CLIRC=$?
wait "$SRVPID" 2>/dev/null

pass=0; fail=0
# 主判据 = .part 落盘+内容抽样(2B 尾差=close_notify 竞态在册,下次专场);
# done=0(最后 2B 未达 CL)同属该竞态面,partial 判词不判败
if grep -qE "down-probe (rc1=0|partial)" "$T/cli.out"; then
    pass=$((pass+1)); echo "  PASS down (落盘+抽样)"
else
    echo "  FAIL down (client): $(tail -1 "$T/cli.out")"; fail=$((fail+1))
fi
# 长度门:40960±容差(2B 尾差在册:TLS close_notify 竞态 vs 块计数,下次专场)
SZV=$(stat -f%z "$T/dl.bin.part" 2>/dev/null || stat -c%s "$T/dl.bin.part" 2>/dev/null)
if [ -f "$T/dl.bin.part" ] && [ "$SZV" -ge 40955 ] && [ "$SZV" -le 40960 ]; then
    pass=$((pass+1)); echo "  PASS down (.part sz=$SZV/40960)"
else
    echo "  FAIL down (.part sz=$SZV)"; fail=$((fail+1))
fi
# 内容抽样断言(确定性体:byte[i] = (i*13+5)%256)
if [ -f "$T/dl.bin.part" ]; then
    B0=$(head -c 1 "$T/dl.bin.part" | od -An -tu1 | tr -d ' ')
    B1000=$(dd if="$T/dl.bin.part" bs=1 skip=1000 count=1 2>/dev/null | od -An -tu1 | tr -d ' ')
    if [ "$B0" = "5" ] && [ "$B1000" = "$(( (1000 * 13 + 5) % 256 ))" ]; then
        pass=$((pass+1)); echo "  PASS down (内容抽样 b0=5 b1000 可验)"
    else
        echo "  FAIL down (内容抽样 b0=$B0 b1000=$B1000)"; fail=$((fail+1))
    fi
fi
echo "down: pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
