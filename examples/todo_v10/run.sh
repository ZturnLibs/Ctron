#!/bin/sh
# examples/todo_v10/run.sh —— SL-8c-4④:§10.3 Todo 合成用户面
# 默认 headless 断言;CTRON_GUI_RUN=1 走合成 __gui_run 真窗(§10.3 交互验收)。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "todo_v10: 缺 compiler/bin/ctron-emit" >&2; exit 1; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/main.ct" > "$T/todo_v10.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "todo_v10: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/todo_v10.bin" \
   "$T/todo_v10.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$DIR"
if [ "${CTRON_GUI_RUN:-}" = "1" ]; then
    "$T/todo_v10.bin"
    echo "todo_v10: 真窗交互(合成装配)退出"
else
    CTRON_GUI_HEADLESS=1 "$T/todo_v10.bin"
    echo "todo_v10: §10.3 合成面 headless 全绿"
fi
