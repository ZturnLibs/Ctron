#!/bin/sh
# tests/proc/run.sh —— 零 shell 进程控制回环验收(lib/proc;CW-F3b)
# 口径(tests/fio 同构):ctron-emit → cc 链 c_src/*.c(软链 lib/proc/c_src/ctron_proc.c)
# → 原生运行;仅本机 /bin 工具,零网络。
# 前置:compiler/native.sh、cc。
set -u
DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/lib/std"
if [ ! -x "$EMIT" ]; then echo "proc/run: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
echo "== tests/proc 回环用例 =="
if "$EMIT" run "$DIR/src/main.ct" > "$T/proc.c" 2>"$T/proc.err"; then
    if cc -O1 -w -I"$ROOT/lib/proc/c_src" -o "$T/proc" "$T/proc.c" "$DIR"/c_src/*.c 2>"$T/proc.cc.err"; then
        if "$T/proc" run "$DIR/src/main.ct" >"$T/proc.out" 2>&1; then
            echo "  PASS proc"
        else
            echo "  FAIL proc (run)"; sed -n '1,10p' "$T/proc.out"; exit 1
        fi
    else
        echo "  FAIL proc (cc)"; sed -n '1,10p' "$T/proc.cc.err"; exit 1
    fi
else
    echo "  FAIL proc (emit)"; sed -n '1,10p' "$T/proc.err"; exit 1
fi
