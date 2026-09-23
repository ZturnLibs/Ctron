#!/bin/sh
# tests/gui/s20_embed/run.sh —— SL-7α:内嵌 view/style 块端到端(无 app.ctml 独立运行)
# main.ct 自带 view/style GuiBlock,源经 ctron_embedded() 取编译期重建——
# 二进制在无 app.ctml 的目录运行即证内嵌兜底(L2 "内嵌形态"首片,§5.1)。
# 重建口径:空格化 token + </ 粘连 + 剥行尾 ~ 标记(域包词法三处适配,见 gui_parse.ct)。
# 已登记:域包 test() 驱动解释口径整体不通(⑳ 家族 struct-List 读残留,read_file 源同炸),
# 本夹具按阶梯惯例走原生口径。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s20: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s20.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s20: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -o "$T/s20.bin" \
   "$T/s20.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$T"
"$T/s20.bin"
echo "s20: 内嵌 view/style 块独立运行全绿"
