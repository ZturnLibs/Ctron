#!/bin/sh
# tests/gui/s33_run_d/run.sh —— SL-8c-4②:gui.run 装配合成域包夹具(s41)
# 三层验收:合成面(check 全量语义核对)/直驱面(headless 驱动合成 fn)/
# 真窗面(CTRON_GUI_RUN=1 手动,CI 不开窗)。构建配方与 s25 同款。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s33: 缺 compiler/bin/ctron-emit" >&2; exit 1; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s33.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s33: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/s33.bin" \
   "$T/s33.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$DIR"
CTRON_GUI_HEADLESS=1 "$T/s33.bin"
echo "s33: 装配合成全绿(合成面/直驱面;真窗 CTRON_GUI_RUN=1)"
