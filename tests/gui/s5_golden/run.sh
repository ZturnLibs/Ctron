#!/bin/sh
# tests/gui/s5_golden/run.sh —— S5:.ctml 运行时解析 + 黄金 IR 差分 + 布局断言(全自动)
# 注意:发射二进制的 read_file 锚 = 烘焙的 "app.ctml"(main 首个 read_file 字面量),
#       故工作目录须为本夹具目录;勿用 `run <file>` 传参(会顶替锚内容)。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s5: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh"

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$EMIT" run "$DIR/src/main.ct" > "$T/s5.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s5: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -o "$T/s5.bin" \
   "$T/s5.c" "$ROOT/std/gui/c_src/ctron_gui.c" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$DIR"
"$T/s5.bin" > "$T/out.txt"

if diff -u "$DIR/expected.txt" "$T/out.txt" > "$T/diff.txt" 2>&1; then
    echo "s5: .ctml 解析黄金差分 + 布局断言全绿"
else
    echo "s5: 黄金差分失败:" >&2
    cat "$T/diff.txt" >&2
    exit 1
fi
