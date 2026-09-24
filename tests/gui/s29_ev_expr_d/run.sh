#!/bin/sh
# tests/gui/s29_ev_expr_d/run.sh —— SL-8c-3:on: 表达式事件域包夹具
# 调用形态事件骨架承载 + d_click 按头按压 + ev_fire 直驱四形态断言;
# props 视图实参根门联验(8c-1 通道)。构建配方与 s25 同款(原生口径)。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s29: 缺 compiler/bin/ctron-emit" >&2; exit 1; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s29.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s29: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/s29.bin" \
   "$T/s29.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$DIR"
"$T/s29.bin"
echo "s29: 表达式事件全绿(调用形态/按头分发/实例后缀透传/旧契约零破坏)"
