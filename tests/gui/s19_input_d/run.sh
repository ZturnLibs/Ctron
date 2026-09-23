#!/bin/sh
# tests/gui/s19_input_d/run.sh —— SL-5:input 域包端到端(std.gui test 驱动;headless 全自动)
# 与 s12_input 同验收、新形态:s12 走裸 extern 桥 + #[trusted] 样板,本夹具走 L1 域包
# (use gui.{test,...})——用户面收敛后的目标形态。d_type_char 注入 h/i →
# d_expect_text "hi";backspace(259)→ "h";空点击钩子 |name| { } 覆盖 ⑰ 空体闭包。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s19: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s19.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s19: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -o "$T/s19.bin" \
   "$T/s19.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$DIR"
"$T/s19.bin"
echo "s19: input 域包端到端全绿"
