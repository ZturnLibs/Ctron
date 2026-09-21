#!/bin/sh
# tests/gui/s23_sk_native/run.sh —— SL-7β3:发射口径骨架直通(零运行时 CTML 解析)
# gui_sk_load() 由编译器发射为 C 静态构造(driver_emit β3 段),域包 test_sk 消费:
# 树等价已由 s22 双端差分锁定,本夹具锁渲染面(文本命中 + 命令数 + RECT + 几何)。
# gui_sk_load 解释口径干净 panic(β2 ABI 评审中)——本夹具按阶梯惯例走原生。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s23: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s23.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s23: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -o "$T/s23.bin" \
   "$T/s23.c" "$ROOT/std/gui/c_src/ctron_gui.c" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$T"
"$T/s23.bin"
echo "s23: 发射口径骨架直通全绿"
