#!/bin/sh
# tests/gui/s57_subscript/run.sh —— GUI-13:视图表达式下标与字符串切分夹具(s57)
# gui.run 装配形态:desugar 下标根 each: 路由 + 内建 cut/fld/sethas/contains;
# 运行时 bxv 下标/内建求值。构建配方与 s41 同款。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s57: 缺 compiler/bin/ctron-emit" >&2; exit 1; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/lib/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s57.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s57: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/s57.bin" \
   "$T/s57.c" "$ROOT/pkgs/gui/c_src/ctron_gui.c" "$ROOT/pkgs/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$DIR"
CTRON_GUI_HEADLESS=1 "$T/s57.bin"
echo "s57: 下标与切分全绿(编译路径 each: 路由 + bxv 内建族)"
