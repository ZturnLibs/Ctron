#!/bin/sh
# tests/gui/s25_expr_d/run.sh —— SL-8b:表达式绑定槽域包夹具(求值器端到端)
# 叶串接/比较条件/算术渲染/带型通道/逐帧活值;Todo 高危面同款原生口径
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s25: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s25.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux) FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s25: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -o "$T/s25.bin" \
   "$T/s25.c" "$ROOT/std/gui/c_src/ctron_gui.c" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$DIR"
"$T/s25.bin"
echo "s25: 表达式绑定槽全绿(叶串接/比较条件/算术渲染/带型通道)"
