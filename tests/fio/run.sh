#!/bin/sh
# tests/fio/run.sh —— 文件字节平面 IO 回环验收(lib/fio;CW-F3a)
# 口径(tests/crypto/run.sh 同构):ctron-emit → cc 链 c_src/*.c(软链
# lib/fio/c_src/ctron_fio.c)→ 原生运行;仅 /tmp 夹具路径,零网络。
# 前置:compiler/native.sh、cc。
set -u
DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
# 夹具 use fio.*:显式指路源码树 std(tests/net/run.sh 同款)
export CTRON_STDPATH="$ROOT/lib/std"
if [ ! -x "$EMIT" ]; then echo "fio/run: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
echo "== tests/fio 回环用例 =="
if "$EMIT" run "$DIR/src/main.ct" > "$T/fio.c" 2>"$T/fio.err"; then
    if cc -O1 -w -I"$ROOT/lib/fio/c_src" -o "$T/fio" "$T/fio.c" "$DIR"/c_src/*.c 2>"$T/fio.cc.err"; then
        if "$T/fio" run "$DIR/src/main.ct" >"$T/fio.out" 2>&1; then
            echo "  PASS fio"
        else
            echo "  FAIL fio (run)"; sed -n '1,10p' "$T/fio.out"; exit 1
        fi
    else
        echo "  FAIL fio (cc)"; sed -n '1,10p' "$T/fio.cc.err"; exit 1
    fi
else
    echo "  FAIL fio (emit)"; sed -n '1,10p' "$T/fio.err"; exit 1
fi
