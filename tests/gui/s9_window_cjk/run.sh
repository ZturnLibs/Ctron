#!/bin/sh
# tests/gui/s9_window_cjk/run.sh —— M3 窗口口:FreeType 中文渲染 + 真实点击
# 默认只构建链接;--run 启动窗口(交互验收:点蓝色按钮,中文计数增长)
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s9: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh"

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$EMIT" run "$DIR/src/main.ct" > "$T/s9.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s9: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/freetype/include" -I"$ROOT/vendor/gui/raylib" -o "$T/s9.bin" \
   "$T/s9.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW
echo "s9: 构建+链接 OK -> $T/s9.bin"
if [ "${1:-}" = "--run" ]; then
    cd "$DIR"
    "$T/s9.bin"
    echo "s9: 窗口退出 rc=$?"
fi
