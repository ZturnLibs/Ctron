#!/bin/sh
# tests/gui/s24_model_d/run.sh —— SL-5:Box 活模型域包组合夹具(Box 绑定+when+each+input+多帧;Todo 迁移高危面回归门)
# 全绿 = 11 参桥收口后 Todo 域包迁移可行性成立(上回二试 when 不渲染症状的同根验证)
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s24: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s24.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux) FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s24: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -o "$T/s24.bin" \
   "$T/s24.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$DIR"
"$T/s24.bin"
echo "s24: Box 活模型组合全绿(Box 活模型+when+each+input+多帧重入)"
