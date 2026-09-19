#!/bin/sh
# examples/gui_counter/run.sh —— GUI 声明式路径正式示例
# 默认:构建 + headless 断言(CTRON_GUI_HEADLESS=1,无显示依赖,全自动)
#       + 快照恢复往返证明(W4 原生口径,裁决 1A);
# --run:追加启动真实窗口(交互验收:点 +1/clear 按钮,计数变化);
# --hot:热重载环——编辑 app.ctml 保存 → 自动重编译+重启,计数经快照恢复(W4 原生口径)。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "gui_counter: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
build() {
    "$EMIT" run "$DIR/src/main.ct" > "$T/gui_counter.c"
    cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -o "$T/gui_counter.bin" \
       "$T/gui_counter.c" "$ROOT/std/gui/c_src/ctron_gui.c" "$ROOT/vendor/gui/build/libraylib.a" $FW
}

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "gui_counter: unsupported platform" >&2; exit 1 ;;
esac

build
echo "gui_counter: 构建+链接 OK"

cd "$DIR"
CTRON_GUI_HEADLESS=1 "$T/gui_counter.bin"

# W4 原生口径往返证明:注入 count=7 → 恢复 → 首帧即为 7 → 回传快照
OUT=$(CTRON_GUI_STATE="count=7" CTRON_GUI_HEADLESS=1 "$T/gui_counter.bin")
echo "$OUT" | grep -q "CTRON_GUI_STATE count=7" || { echo "gui_counter: 快照恢复往返失败" >&2; exit 1; }
echo "gui_counter: 快照恢复往返 OK(count=7 → 恢复 → 回传 7)"

# W4-E3 进程内原址替换探针:重载 app2.ctml(gap 8→20)→ 断言骨架更新 + count=2 保留
CTRON_GUI_RELOAD_SRC="app2.ctml" CTRON_GUI_HEADLESS=1 "$T/gui_counter.bin" | grep -q "E3 原址替换全绿" \
    || { echo "gui_counter: E3 原址替换探针失败" >&2; exit 1; }
echo "gui_counter: E3 原址替换 OK(gap 变更生效,count 保留)"

if [ "${1:-}" = "--hot" ]; then
    mtime() {
        if [ "$(uname)" = "Darwin" ]; then stat -f %m "$DIR/app.ctml" 2>/dev/null || echo 0
        else stat -c %Y "$DIR/app.ctml" 2>/dev/null || echo 0; fi
    }
    STATE=""
    LAST=$(mtime)
    echo "gui_counter: --hot 热重载环启动(编辑 app.ctml 保存即重载;Ctrl-C 退出)"
    while :; do
        build
        CTRON_GUI_STATE="$STATE" "$T/gui_counter.bin" > "$T/state.out" 2>&1 &
        APP=$!
        while kill -0 "$APP" 2>/dev/null; do
            NOW=$(mtime)
            if [ "$NOW" != "$LAST" ]; then
                kill "$APP" 2>/dev/null || true
                break
            fi
            sleep 1
        done
        wait "$APP" 2>/dev/null || true
        NEW=$(grep -o "count=[0-9]*" "$T/state.out" 2>/dev/null | tail -1 | cut -d= -f2 || true)
        if [ -n "$NEW" ]; then STATE="$NEW"; fi
        LAST=$(mtime)
        echo "gui_counter: --hot 重载完成,恢复 count = ${STATE:-0}"
    done
fi

if [ "${1:-}" = "--run" ]; then
    "$T/gui_counter.bin"
    echo "gui_counter: 窗口退出 rc=$?"
fi
