#!/bin/sh
# examples/gui_cjk/run.sh —— GUI 中文一等正式示例(FreeType 纹理窗口)
# 默认:构建 + headless 渲染冒烟(CTRON_GUI_HEADLESS=1;无 CJK 字体则 skip);
# --run:追加启动真实窗口(交互验收:点蓝色按钮,中文计数增长)
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "gui_cjk: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$EMIT" run "$DIR/src/main.ct" > "$T/gui_cjk.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "gui_cjk: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/freetype/include" -I"$ROOT/vendor/gui/raylib" -o "$T/gui_cjk.bin" \
   "$T/gui_cjk.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW
echo "gui_cjk: 构建+链接 OK"

cd "$DIR"
CTRON_GUI_HEADLESS=1 "$T/gui_cjk.bin"
if [ "${1:-}" = "--run" ]; then
    "$T/gui_cjk.bin"
    echo "gui_cjk: 窗口退出 rc=$?"
fi
