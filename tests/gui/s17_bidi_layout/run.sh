#!/bin/sh
# tests/gui/s17_bidi_layout/run.sh —— M3 整合:SheenBidi run 切分 → Clay 定位排布(headless 断言)
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s17: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh"

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$EMIT" run "$DIR/src/main.ct" > "$T/s17.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s17: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/sheenbidi/Headers" \
   -o "$T/s17.bin" \
   "$T/s17.c" "$ROOT/std/gui/c_src/ctron_gui.c" "$DIR"/c_src/sb_shim.c \
   "$ROOT/vendor/gui/build/libraylib.a" "$ROOT/vendor/gui/build/libsheenbidi.a" $FW
"$T/s17.bin" run "$DIR/src/main.ct"
echo "s17_bidi_layout: 整合全绿(headless)"
