#!/bin/sh
# tests/gui/s26_reload_d/run.sh —— SL-9:热重载宿主域包形态验收(§11.5/§6.3 M0-M2 面)
# 流程:V1 启动(探针行 S26P n=.. cmds=..)→ 改写 app.ctml(样式 size + 结构加行)→
# 断言:重载后探针行 = 新结构入帧(cmds 2→3)且模型状态保留(n=7)。退出还原 app.ctml。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s26: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s26.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux) FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s26: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -o "$T/s26.bin" \
   "$T/s26.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$DIR"
cp app.ctml "$T/app.bak"
trap 'cp "$T/app.bak" app.ctml' EXIT

S26_HOTRELOAD=1 CTRON_GUI_STATE=7 "$T/s26.bin" > "$T/out.txt" 2>&1 &
BIN=$!

# 就绪握手:等 V1 首帧探针(cmds=2),防启动慢于改写的竞态
i=0
while ! grep -q "cmds=2" "$T/out.txt" 2>/dev/null; do
    sleep 0.1
    i=$((i + 1))
    if [ "$i" -gt 50 ]; then
        echo "s26: 超时未见到 V1 首帧" >&2
        kill $BIN 2>/dev/null || true
        exit 1
    fi
done

# 改写:样式(size 30→60)+ 结构(加 extra 行)——content 比对必触发重解析
cat > app.ctml <<'EOF2'
view S26 {
  <vbox class="root">
    <label class="mark">V2</label>
    <label class="extra">extra row</label>
    <label class="cnt">count {n}</label>
  </vbox>
}
style root { direction: column gap: 8 padding: 16 }
style mark { fg: "#22c55e" size: 60 }
style cnt { fg: "#ffffff" }
style extra { fg: "#f97316" }
EOF2

# 等重载生效:新结构入帧(cmds 2→3)
i=0
while ! grep -q "cmds=3" "$T/out.txt" 2>/dev/null; do
    sleep 0.1
    i=$((i + 1))
    if [ "$i" -gt 50 ]; then
        echo "s26: 超时未见重载后新帧(cmds=3)" >&2
        kill $BIN 2>/dev/null || true
        exit 1
    fi
done

# 状态保留:重载后最新探针行 = n=7(闭包 Box 状态跨重载存活)
sleep 0.3
LAST=$(tail -1 "$T/out.txt")
kill $BIN 2>/dev/null || true
wait $BIN 2>/dev/null || true
case "$LAST" in
    *"n=7 cmds=3"*)
        echo "s26: 热重载宿主域包形态全绿(样式+结构重载,状态保留 n=7)"
        exit 0
        ;;
    *)
        echo "s26: 重载后状态异常,末行: $LAST" >&2
        exit 1
        ;;
esac
