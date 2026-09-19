#!/bin/sh
# examples/gui_counter/run.sh —— GUI 声明式路径正式示例
# 默认:构建 + headless 断言(CTRON_GUI_HEADLESS=1,无显示依赖,全自动);
# --run:追加启动真实窗口(交互验收:点 +1/clear 按钮,计数变化)
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "gui_counter: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$EMIT" run "$DIR/src/main.ct" > "$T/gui_counter.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "gui_counter: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -o "$T/gui_counter.bin" \
   "$T/gui_counter.c" "$ROOT/std/gui/c_src/ctron_gui.c" "$ROOT/vendor/gui/build/libraylib.a" $FW
echo "gui_counter: 构建+链接 OK"

cd "$DIR"
CTRON_GUI_HEADLESS=1 "$T/gui_counter.bin"
if [ "${1:-}" = "--run" ]; then
    "$T/gui_counter.bin"
    echo "gui_counter: 窗口退出 rc=$?"
fi
