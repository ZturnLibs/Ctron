#!/bin/sh
# tests/gui/s18_scroll/run.sh —— 滚动容器能力测试(滚轮→裁剪/几何断言;headless 全自动)
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s18: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$EMIT" run "$DIR/src/main.ct" > "$T/s18.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s18: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -o "$T/s18.bin" \
   "$T/s18.c" "$ROOT/std/gui/c_src/ctron_gui.c" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$DIR"
"$T/s18.bin"
echo "s18: 滚动容器全绿"
