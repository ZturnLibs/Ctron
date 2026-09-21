#!/bin/sh
# tests/gui/s22_sk_equiv/run.sh —— SL-7β1:编译期骨架解析 ⇔ 运行时 gt_parse 树等价
# 双端差分:A 端 = ctc.sh check app.ctml --dump-gui 的 sk 段(编译器 gui_sk_dump_all);
# B 端 = 域包探针 gt_parse+sk_dump(原生口径)。两端逐字节 diff——骨架换源(β2/β3)的
# 正确性锚:编译期解析必须与运行时解析产出同一棵树。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
CTC="$ROOT/compiler/ctc.sh"
[ -x "$EMIT" ] || { echo "s22: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT

# A 端:编译器骨架倾倒(独立 .ctml 检查路径;取 sk 段 = 自首行 "n0 " 起至文件尾)
"$CTC" check "$DIR/app.ctml" --dump-gui > "$T/sk_compiler.txt" 2>&1
awk '/^n0 /{f=1} f' "$T/sk_compiler.txt" > "$T/A.txt"

# B 端:域包 gt_parse+sk_dump(原生)
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/probe.ct" > "$T/probe.c"
case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s22: unsupported platform" >&2; exit 1 ;;
esac
cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -o "$T/s22.bin" \
   "$T/probe.c" "$ROOT/std/gui/c_src/ctron_gui.c" "$ROOT/vendor/gui/build/libraylib.a" $FW
cp "$DIR/app.ctml" "$T/app.ctml"
cd "$T"
./s22.bin > B.txt

if diff -u A.txt B.txt > diff.txt 2>&1; then
    echo "s22: 编译期骨架与运行时 gt_parse 树等价全绿"
else
    echo "s22: 骨架树等价差分失败:" >&2
    cat diff.txt >&2
    exit 1
fi
