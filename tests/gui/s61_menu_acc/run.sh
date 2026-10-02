#!/bin/sh
# tests/gui/s61_menu_acc —— checkbox switch/radio 变体(裸属性+事件表承载)headless 验收
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s59: 缺 ctron-emit" >&2; exit 1; }
sh "$ROOT/vendor/gui/build.sh" > /dev/null
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/lib/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s61.c"
case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s59: unsupported" >&2; exit 1 ;;
esac
export CTRON_GUI_FT_OFF=1
cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/s61.bin" \
   "$T/s61.c" "$ROOT/pkgs/gui/c_src/ctron_gui.c" "$ROOT/pkgs/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW
cd "$DIR"
"$T/s61.bin"
echo "s59: menu 加速器+键盘导航全绿"
