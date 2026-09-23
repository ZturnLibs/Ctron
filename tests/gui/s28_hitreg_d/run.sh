#!/bin/sh
# tests/gui/s28_hitreg_d/run.sh —— P-H1:每帧交互注册表(rt_hit_name hits/hinst)
# 顶层按钮命中 = 裸名;each 实例命中 = "名:下标"(实例索引分发缺口收口);
# 直接驱动 rt_draw_frame/rt_hit_name,注册表随帧刷新。原生口径。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s28: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s28.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux) FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s28: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/s28.bin" \
   "$T/s28.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libsheenbidi.a" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$DIR"
"$T/s28.bin"
echo "s28: 交互注册表全绿(顶层裸名+each 实例 名:下标)"
