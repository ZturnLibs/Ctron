#!/bin/sh
# tests/gui/w4_hotreload/run.sh —— W4:热重载机制验证(headless 断言 + --run 窗口)
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "w4_hotreload: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh"

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$EMIT" run "$DIR/src/main.ct" > "$T/w4_hotreload.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/raylib" -o "$T/w4_hotreload.bin" \
   "$T/w4_hotreload.c" "$DIR"/c_src/*.c "$ROOT/vendor/gui/build/libraylib.a" $FW

echo "w4_hotreload: 构建+链接 OK"

# --run:窗口交互验收(mtime 轮询重载 + 状态保持,肉眼确认面)
if [ "${1:-}" = "--run" ]; then
    cd "$DIR"
    echo "Hello" > "$DIR/app.txt"
    echo "w4_hotreload: 窗口已启动 —— 编辑 tests/gui/w4_hotreload/app.txt 看实时刷新;点击窗口看 clicks 计数跨重载保持;关窗退出"
    exec "$T/w4_hotreload.bin"
fi

# headless 热重载证明:单进程内 mtime 轮询 → 外部改写 → 重读生效
cd "$DIR"
W4_READY="$T/ready"
export W4_READY
rm -f "$W4_READY"
echo "Hello" > "$DIR/app.txt"
W4_HOTRELOAD=1 "$T/w4_hotreload.bin" > "$T/out.txt" 2>&1 &
BIN=$!
# 就绪握手:初始 mtime 读定后再改写(启动慢于改写则轮询必超时,曾 0.6s sleep 踩中)
READY=0
for i in $(seq 1 100); do
    [ -f "$W4_READY" ] && { READY=1; break; }
    sleep 0.1
done
if [ "$READY" != 1 ]; then
    echo "w4_hotreload: 子进程未就绪" >&2
    kill $BIN 2>/dev/null || true
    exit 1
fi
echo "World" > "$DIR/app.txt"
wait $BIN || true

if grep -q "initial: Hello" "$T/out.txt" && grep -q "reloaded: World" "$T/out.txt"; then
    echo "w4_hotreload: 热重载 mtime 轮询→重读 PASS(headless ✓,单进程)"
else
    echo "w4_hotreload: 重读验证 FAIL(输出:)" >&2
    cat "$T/out.txt" >&2
    exit 1
fi
