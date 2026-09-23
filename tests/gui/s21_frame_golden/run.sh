#!/bin/sh
# tests/gui/s21_frame_golden/run.sh —— SL-7:渲染黄金帧差分(整帧命令缓冲 diff)
# 合成面(字面量 label/button/when/each/input + 三组样式)经内嵌源渲染,整帧命令
# 逐条倾倒(type/box×100/text)与 expected.txt 差分。黄金 = 骨架 IR 换源(SL-7)/
# {expr} 绑定(SL-8)/热重载宿主(SL-9)的渲染安全网——动渲染面前先跑本夹具。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s21: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s21.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s21: unsupported platform" >&2; exit 1 ;;
esac
export CTRON_GUI_FT_OFF=1  # 坐标/黄金口径钉 0.55 启发式测量(M3 合流后默认 FT 实测;钉值跨平台稳定)


cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/s21.bin" \
   "$T/s21.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$T"
"$T/s21.bin" > "$T/out.txt"

if diff -u "$DIR/expected.txt" "$T/out.txt" > "$T/diff.txt" 2>&1; then
    echo "s21: 渲染黄金帧差分全绿"
else
    echo "s21: 黄金帧差分失败:" >&2
    cat "$T/diff.txt" >&2
    exit 1
fi
