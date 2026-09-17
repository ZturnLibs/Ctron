#!/bin/sh
# tests/gui/s2_ab/run.sh —— S2:repr(c) GUI struct 过 ABI(无窗口,全自动)
# 纯类型探针:不调用任何 raylib 函数,无需链 libraylib.a/框架(仅 -I 取 raylib.h)
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s2: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$EMIT" run "$DIR/src/main.ct" > "$T/s2.c"
cc -O1 -w -I"$ROOT/vendor/gui/raylib" -o "$T/s2.bin" \
   "$T/s2.c" "$DIR"/c_src/*.c
"$T/s2.bin" run "$DIR/src/main.ct"
echo "s2: repr(c) GUI struct ABI 全绿"
