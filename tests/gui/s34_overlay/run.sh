#!/bin/sh
# tests/gui/s34_overlay —— 波次三 T4:浮层+模态语义(§2.3;显隐/居中/8位遮罩色/穿透遮蔽/键盘模态 Esc 自锁修复)
# style 直写 "#aa33cc" → RECT 命令 bg r/g/b = 170/51/204;d_cmd_bg_* 通路验证
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s34: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s34.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux) FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s34: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/s34.bin" \
   "$T/s34.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$DIR"
"$T/s34.bin"
echo "s34: overlay 模态全绿"
