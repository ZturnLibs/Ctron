#!/bin/sh
# tests/gui/s32_state —— 波次一 T1:驱动器颜色读回通路(八主题换装;交互态/主题折叠地基)
# style 直写 "#aa33cc" → RECT 命令 bg r/g/b = 170/51/204;d_cmd_bg_* 通路验证
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s32: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s32.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux) FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s32: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/s32.bin" \
   "$T/s32.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$DIR"
O1=$("$T/s32.bin")
echo "$O1" | grep -q "S32-AUTO-GREEN" || { echo "s32: auto 段红" >&2; exit 1; }
O2=$(CTRON_GUI_THEME=linux_dark "$T/s32.bin")
echo "$O2" | grep -q "S32-PIN-GREEN" || { echo "s32: 钉值段红" >&2; exit 1; }
echo "s32: 八主题+auto+钉值全绿"
