#!/bin/sh
# examples/gui_zitie/run.sh —— 中文字帖(中文显示验证专例:竖排列 72px 大字)
# 默认:构建 + headless 全链路断言(逐字字节/实测/命令数/翻页,无显示依赖);
# --run:追加真实窗口交互验收
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "gui_zitie: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
# L1 域包形态:use gui 需 std 三级解析根
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/main.ct" > "$T/gui_zitie.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "gui_zitie: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/gui_zitie.bin" \
   "$T/gui_zitie.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW
echo "gui_zitie: 构建+链接 OK"

cd "$DIR"
CTRON_GUI_HEADLESS=1 "$T/gui_zitie.bin"

if [ "${1:-}" = "--run" ]; then
    CTRON_GUI_RUN=1 "$T/gui_zitie.bin"
    echo "gui_zitie: 窗口退出 rc=$?"
fi
