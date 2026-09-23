#!/bin/sh
# tests/gui/w4_ctml_reload/run.sh —— W4 完整版:CTML 解析模型热重载(headless 单进程证明 + --run 窗口)
# 与 w4_hotreload(裸文本)的差异:重读后走重解析→模型→Clay 全量重放,结构/样式变更同样生效
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "w4_ctml_reload: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh"

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$EMIT" run "$DIR/src/main.ct" > "$T/w4r.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "w4_ctml_reload: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" \
   -o "$T/w4r.bin" \
   "$T/w4r.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$DIR"/c_src/reload_util.c \
   "$ROOT/vendor/gui/build/libraylib.a" $FW

echo "w4_ctml_reload: 构建+链接 OK"

seed_ctml() {
    cat > "$DIR/app.ctml" <<CTML
view V {
  <label>$1</label>
  <label>static-two</label>
}
style root { gap: 8; padding: 16 }
CTML
}

# --run:窗口交互验收(改 app.ctml 实时重排)
if [ "${1:-}" = "--run" ]; then
    cd "$DIR"
    seed_ctml "alpha"
    echo "w4_ctml_reload: 窗口已启动 —— 编辑 app.ctml 的 label 文本或样式 gap/padding,实时重排;关窗退出"
    exec "$T/w4r.bin"
fi

# headless 单进程:解析模型 → 布局 → 就绪握手 → 外部改写 → 重解析 → 新模型
cd "$DIR"
W4R_READY="$T/ready"
export W4R_READY
rm -f "$W4R_READY"
seed_ctml "alpha"
W4R_HEADLESS=1 "$T/w4r.bin" > "$T/out.txt" 2>&1 &
BIN=$!
READY=0
for i in $(seq 1 100); do
    [ -f "$W4R_READY" ] && { READY=1; break; }
    sleep 0.1
done
if [ "$READY" != 1 ]; then
    echo "w4_ctml_reload: 子进程未就绪" >&2
    kill $BIN 2>/dev/null || true
    exit 1
fi
seed_ctml "beta hot"
wait $BIN || true

if grep -q "before: alpha" "$T/out.txt" && grep -q "after: beta hot" "$T/out.txt"; then
    echo "w4_ctml_reload: CTML 重解析热重载 PASS(headless ✓,单进程)"
else
    echo "w4_ctml_reload: 重载验证 FAIL(输出:)" >&2
    cat "$T/out.txt" >&2
    exit 1
fi
