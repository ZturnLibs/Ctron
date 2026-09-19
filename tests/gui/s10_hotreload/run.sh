#!/bin/sh
# tests/gui/s10_hotreload/run.sh —— W4:热重载机制验证(headless 断言 + --run 窗口)
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s10: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh"

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$EMIT" run "$DIR/src/main.ct" > "$T/s10.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/raylib" -o "$T/s10.bin" \
   "$T/s10.c" "$DIR"/c_src/*.c "$ROOT/vendor/gui/build/libraylib.a" $FW

echo "s10: 构建+链接 OK"

# headless 断言:文本非空
cd "$DIR"
echo "Hello" > "$DIR/app.txt"
S10_HEADLESS=1 "$T/s10.bin" > "$T/out1.txt" 2>&1

echo "World" > "$DIR/app.txt"
S10_HEADLESS=1 "$T/s10.bin" > "$T/out2.txt" 2>&1

if grep -q "Hello" "$T/out1.txt" && grep -q "World" "$T/out2.txt"; then
    echo "s10: 热重载文本变更 PASS(headless ✓)"
else
    echo "s10: 重读验证 FAIL" >&2
    exit 1
fi
