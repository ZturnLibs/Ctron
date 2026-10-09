#!/bin/sh
# tests/crypto/run.sh —— 原生密码学回环验收(lib/crypto;CW-F1a)
# 口径(tests/net/run.sh 同构,单夹具形态):ctron-emit → cc 链 c_src/*.c(软链
# lib/crypto/c_src/ctron_crypto.c)→ 原生运行;纯计算,零网络零 fs。
# 前置:compiler/native.sh、cc、homebrew openssl@3(libcrypto)。
set -u
DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
# 夹具 use crypto.*:显式指路源码树 std(tests/net/run.sh 同款,STDPATH 父目录即 lib 根)
export CTRON_STDPATH="$ROOT/lib/std"
if [ ! -x "$EMIT" ]; then echo "crypto/run: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
OSSL_CF="-I/opt/homebrew/opt/openssl@3/include"
OSSL_LB="-L/opt/homebrew/opt/openssl@3/lib -lcrypto"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
echo "== tests/crypto 回环用例 =="
if "$EMIT" run "$DIR/src/main.ct" > "$T/crypto.c" 2>"$T/crypto.err"; then
    if cc -O1 -w $OSSL_CF -I"$ROOT/lib/crypto/c_src" -o "$T/crypto" "$T/crypto.c" "$DIR"/c_src/*.c $OSSL_LB 2>"$T/crypto.cc.err"; then
        if "$T/crypto" run "$DIR/src/main.ct" >"$T/crypto.out" 2>&1; then
            echo "  PASS crypto"
        else
            echo "  FAIL crypto (run)"; sed -n '1,10p' "$T/crypto.out"; exit 1
        fi
    else
        echo "  FAIL crypto (cc)"; sed -n '1,10p' "$T/crypto.cc.err"; exit 1
    fi
else
    echo "  FAIL crypto (emit)"; sed -n '1,10p' "$T/crypto.err"; exit 1
fi
