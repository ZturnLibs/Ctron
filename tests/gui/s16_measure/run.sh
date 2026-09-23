#!/bin/sh
# tests/gui/s16_measure/run.sh —— M3:FreeType 实测宽度接入 Clay(fixture 本地 hook,不动共享 gui)
# 链接 libfreetype + libraylib:hook 自持 FT face;ctron_gui.c 引用 raylib 符号
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s16: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh"

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$EMIT" run "$DIR/src/main.ct" > "$T/s16.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s16: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" \
   -o "$T/s16.bin" \
   "$T/s16.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" \
   "$ROOT/vendor/gui/build/libraylib.a" "$ROOT/vendor/gui/build/libfreetype.a" $FW
"$T/s16.bin" run "$DIR/src/main.ct"
echo "s16_measure: 测量桥全绿(headless)"
