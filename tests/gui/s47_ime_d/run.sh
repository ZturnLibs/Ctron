#!/bin/sh
# tests/gui/s47_ime_d/run.sh —— P-M3:IME 组词内联渲染域包夹具(headless)
# d_focus 聚焦 + d_ime_set 注入组词态 → 命令缓冲 TEXT 含组词串(marked range
# 内联可见,§10.5)+ mirror 模型值不动 + 清除回落。构建配方与 s30 同款。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s47: 缺 compiler/bin/ctron-emit" >&2; exit 1; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/lib/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s47.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s47: unsupported platform" >&2; exit 1 ;;
esac
export CTRON_GUI_FT_OFF=1

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/s47.bin" \
   "$T/s47.c" "$ROOT/pkgs/gui/c_src/ctron_gui.c" "$ROOT/pkgs/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$DIR"
"$T/s47.bin"
echo "s47: IME 组词内联全绿(注入/内联可见/模型不动/清除回落)"
