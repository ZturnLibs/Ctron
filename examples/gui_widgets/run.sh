#!/bin/sh
# gui_widgets —— 组件陈列室(Select/WList/Dialog 交互演示;headless 断言;--run 真窗)
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "gui_widgets: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/main.ct" > "$T/gw.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux) FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "gui_widgets: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/gw.bin" \
   "$T/gw.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$DIR"
if [ "${1:-}" = "--run" ]; then
    "$T/gw.bin"
else
    CTRON_GUI_HEADLESS=1 "$T/gw.bin"
fi
echo "gui_widgets: 全绿"
