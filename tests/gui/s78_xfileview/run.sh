#!/bin/sh
# tests/gui/s78_xfileview/run.sh —— GUI-27 组件跨文件 view 导入夹具
# 运行面:gt_anchor_src 锚合并(components.ctml 先入,app.ctml 后)→ 无合并则
# Root 实例展开期 panic「未知组件」;断言导入组件渲染/事件/slot 投影。
# 检查面:ctc.sh check 金路绿(语料负例见 e8_corpus e8100_use_*/viewcall_unknown)。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s78: 缺 compiler/bin/ctron-emit" >&2; exit 1; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

# 检查面金路:app.ctml 走 use 装载器(合并 + 可见性门)必须干净
chk=$("$ROOT/compiler/ctc.sh" check "$DIR/app.ctml" 2>&1) || {
    echo "s78: 检查面红:$chk" >&2
    exit 1
}
case "$chk" in
    "check OK"*) : ;;
    *) echo "s78: 检查面口径异:$chk" >&2; exit 1 ;;
esac

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/lib/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s78.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s78: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/s78.bin" \
   "$T/s78.c" "$ROOT/pkgs/gui/c_src/ctron_gui.c" "$ROOT/pkgs/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$DIR"
CTRON_GUI_HEADLESS=1 "$T/s78.bin"
echo "s78: 组件跨文件 view 导入全绿"
