#!/bin/sh
# examples/gui_calc/run.sh —— 仿 macOS 计算器综合示例
# 默认:构建 + headless 断言(CTRON_GUI_HEADLESS=1,注入点击/按键,无显示依赖,全自动)
# --run:追加启动真实窗口(交互验收:鼠标点按键;键盘 0-9 . + - * / = Enter ESC % 均可用)
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/std"
[ -x "$EMIT" ] || { echo "gui_calc: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
build() {
    "$EMIT" run "$DIR/src/main.ct" > "$T/gui_calc.c"
    cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/gui_calc.bin" \
       "$T/gui_calc.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW
}

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "gui_calc: unsupported platform" >&2; exit 1 ;;
esac

build
echo "gui_calc: 构建+链接 OK"

cd "$DIR"
CTRON_GUI_HEADLESS=1 "$T/gui_calc.bin"

if [ "${1:-}" = "--run" ]; then
    "$T/gui_calc.bin"
    echo "gui_calc: 窗口退出 rc=$?"
fi
