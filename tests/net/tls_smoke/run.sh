#!/bin/sh
# tests/net/tls_smoke/run.sh —— P3-C TLS 夹具单独驱动(默认 pthread 面 +
# CTRON_RT=coro 面各整跑一遍;coro 面 = BIO-over-hybrid 混合化继承的机械证明:
# 服务端协程 TLS 握手/读写停车于 net 垫片,与主面客户端并发)。
# 主体入 tests/net/run.sh 主环(双矩阵随 run.sh 双跑各计一次);本脚本为
# 开发迭代入口,同 rt_core_smoke/run.sh 形。前置:compiler/native.sh、cc、
# openssl;vendor/tls/build/lib/*.a 缺席时自动构建(离线)。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/std"
[ -x "$EMIT" ] || { echo "tls_smoke: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; }
[ -f "$ROOT/vendor/tls/build/lib/libmbedcrypto.a" ] || sh "$ROOT/vendor/tls/build.sh"

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout "$T/key.pem" -out "$T/cert.pem" -days 2 -subj "/CN=127.0.0.1" >/dev/null 2>&1
cp "$T/cert.pem" "$T/ca.pem"

"$EMIT" run "$DIR/src/main.ct" > "$T/main.c"
printf '#define MBEDTLS_CONFIG_FILE "config-thread.h"\n' > "$T/mbcfg.h"
cc -O1 -w -pthread -I"$ROOT/net/c_src" \
   -I"$ROOT/vendor/tls" -I"$ROOT/vendor/tls/mbedtls/include" -include "$T/mbcfg.h" \
   -o "$T/tls_smoke" "$T/main.c" "$DIR"/c_src/*.c \
   "$ROOT/vendor/tls/build/lib/libmbedtls.a" \
   "$ROOT/vendor/tls/build/lib/libmbedx509.a" \
   "$ROOT/vendor/tls/build/lib/libmbedcrypto.a" -lpthread

for MODE in default coro; do
    if [ "$MODE" = coro ]; then
        ENV="CTRON_RT=coro"
    else
        ENV=""
    fi
    if env $ENV CTRON_SMOKE_CERT="$T/cert.pem" CTRON_SMOKE_KEY="$T/key.pem" \
        CTRON_SMOKE_CA="$T/ca.pem" "$T/tls_smoke" > "$T/$MODE.out" 2>&1; then
        echo "PASS tls_smoke ($MODE)"
    else
        echo "FAIL tls_smoke ($MODE)"; sed -n '1,20p' "$T/$MODE.out" >&2
        exit 1
    fi
done
echo "tls_smoke: 2/2"
