#!/bin/sh
# tests/gui/s1_smoke/run.sh —— S1:构建 + 链接断言(--run 才启动窗口做交互冒烟)
# 前置:compiler/bin/{ctron-emit}(FFI 批次内含 Str 编组)、cc
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s1: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh"

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$EMIT" run "$DIR/src/main.ct" > "$T/s1.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s1: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/raylib" -o "$T/s1.bin" \
   "$T/s1.c" "$DIR"/c_src/*.c "$ROOT/vendor/gui/build/libraylib.a" $FW
echo "s1: 构建+链接 OK -> $T/s1.bin"
if [ "${1:-}" = "--run" ]; then
    "$T/s1.bin"
    echo "s1: 交互冒烟 rc=$?"
fi
