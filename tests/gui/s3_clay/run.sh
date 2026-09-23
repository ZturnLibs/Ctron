#!/bin/sh
# tests/gui/s3_clay/run.sh —— S3:Clay 布局桥 headless 断言(全自动,不开窗)
# 链接 libraylib.a:shim 的 flush/measure 引用 raylib 符号(主流程不触窗口)
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s3: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh"

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$EMIT" run "$DIR/src/main.ct" > "$T/s3.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s3: unsupported platform" >&2; exit 1 ;;
esac
export CTRON_GUI_FT_OFF=1  # 坐标/黄金口径钉 0.55 启发式测量(M3 合流后默认 FT 实测;钉值跨平台稳定)


cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/s3.bin" \
   "$T/s3.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW
"$T/s3.bin" run "$DIR/src/main.ct"
echo "s3: Clay 布局桥全绿(headless)"
