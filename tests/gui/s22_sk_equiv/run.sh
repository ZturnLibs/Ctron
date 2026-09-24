#!/bin/sh
# tests/gui/s22_sk_equiv/run.sh —— SL-7β1:编译期骨架解析 ⇔ 运行时 gt_parse 树等价
# 双端差分:A 端 = ctc.sh check <app> --dump-gui 的 sk 段(编译器 gui_sk_dump_all);
# B 端 = 域包探针 gt_parse+sk_dump(原生口径)。两端逐字节 diff——骨架换源(β2/β3)的
# 正确性锚:编译期解析必须与运行时解析产出同一棵树。
# 8c-2:取 sk 段锚改自 "props="(props 名表行 = sk 段首行);双轮 = app.ctml(裸名)
# + props.ctml(props 正例,数据面双端同形正程)。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
CTC="$ROOT/compiler/ctc.sh"
[ -x "$EMIT" ] || { echo "s22: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT

# B 端探针编译一次(原生),双轮复用
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/probe.ct" > "$T/probe.c"
case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s22: unsupported platform" >&2; exit 1 ;;
esac
cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/s22.bin" \
   "$T/probe.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$T"
fail2=""
for F in app.ctml props.ctml; do
    "$CTC" check "$DIR/$F" --dump-gui > "$T/sk_compiler.txt" 2>&1
    awk '/^props=/{f=1} f' "$T/sk_compiler.txt" > "$T/A.txt"
    cp "$DIR/$F" "$T/app.ctml"
    ./s22.bin > B.txt
    if ! diff -u A.txt B.txt > diff.txt 2>&1; then
        echo "s22: $F 骨架树等价差分失败:" >&2
        cat diff.txt >&2
        fail2=1
    fi
done
[ -z "$fail2" ] || exit 1
echo "s22: 编译期骨架与运行时 gt_parse 树等价全绿(app+props 双轮)"
