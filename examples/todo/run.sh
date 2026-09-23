#!/bin/sh
# examples/todo/run.sh —— 规范 §10 Todo 演绎(当前能力版;M1-e 门面示例)
# 默认:构建 + headless 全链路断言(键入/添加/列表/空态/删除,无显示依赖);
# --run:追加真实窗口(键入文字 + 点 add/del,交互验收)
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "todo: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
# L1 域包形态:use std.gui 需 std 三级解析(旧形态自包含不需,迁移后补设)
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/main.ct" > "$T/todo.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "todo: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/todo.bin" \
   "$T/todo.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW
echo "todo: 构建+链接 OK"

cd "$DIR"
CTRON_GUI_HEADLESS=1 "$T/todo.bin"

if [ "${1:-}" = "--run" ]; then
    CTRON_GUI_RUN=1 "$T/todo.bin"
    echo "todo: 窗口退出 rc=$?"
fi
