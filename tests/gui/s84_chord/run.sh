#!/bin/sh
# tests/gui/s84_chord —— G 档 GUI-43 chord 序列组合键 headless 验收
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s84: 缺 ctron-emit" >&2; exit 1; }
sh "$ROOT/vendor/gui/build.sh" > /dev/null
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/lib/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s84.c"
case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s84: unsupported" >&2; exit 1 ;;
esac
export CTRON_GUI_FT_OFF=1
cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/s84.bin" \
   "$T/s84.c" "$ROOT/pkgs/gui/c_src/ctron_gui.c" "$ROOT/pkgs/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW
cd "$DIR"
if [ "${1:-}" = "--run" ]; then
    "$T/s84.bin"
else
    CTRON_GUI_HEADLESS=1 "$T/s84.bin"
fi
echo "s84: chord 完成/超时/干扰不吞/单段回归全绿"
