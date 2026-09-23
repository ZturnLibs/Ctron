#!/bin/sh
# tests/gui/s28_ft_flush/run.sh —— M3 合流:域库 ft 实测测量 + 纹理缓存 headless 断言
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s28: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh"

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$EMIT" run "$DIR/src/main.ct" > "$T/s28.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s28: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" \
   -o "$T/s28.bin" \
   "$T/s28.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" \
   "$ROOT/vendor/gui/build/libraylib.a" "$ROOT/vendor/gui/build/libfreetype.a" $FW
"$T/s28.bin"
echo "s28_ft_flush: 实测测量+纹理缓存全绿(headless)"
