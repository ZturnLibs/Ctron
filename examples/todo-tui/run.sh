#!/bin/sh
# todo-tui e2e —— TUI 验收应用全链(W4;设计 §10 验收应用):
#   script(pty)供真 tty → 键序 a/新增 "buy milk"/Enter → q 退出 → 数据文件断言。
#   相位二:重启应用 → 文件回载 → 新条目仍在(持久化)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$DIR/../.." && pwd)
EMIT="${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}"
[ -x "$EMIT" ] || { echo "todo-tui: 缺 compiler/bin/ctron-emit" >&2; exit 2; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
export CTRON_STDPATH="$ROOT/lib/std"

if ! "$EMIT" run "$DIR/src/main.ct" > "$T/app.c" 2>"$T/emit.err"; then
    echo "todo-tui: emit 失败: $(tail -1 "$T/emit.err")" >&2; exit 1
fi
if ! cc -O1 -w -pthread -o "$T/app.bin" "$T/app.c" "$ROOT/pkgs/tui/c_src/tui_term.c" "$ROOT/lib/fio/c_src/ctron_fio.c" 2>"$T/cc.err"; then
    echo "todo-tui: cc 失败: $(head -3 "$T/cc.err" | tr '\n' ' ')" >&2; exit 1
fi

pass=0; fail=0
# 相位一:新增 + 退出(演示模式 tick 注入脚本键;macOS script 不转发管道 stdin 在册)
cd "$T"
TODO_TUI_DEMO="a|s:buy milk|RET|q" timeout 20 script -q /dev/null "$T/app.bin" > "$T/p1.out" 2>&1
if [ -f "$T/todo-tui.data" ] && grep -qx "buy milk" "$T/todo-tui.data"; then
    pass=$((pass+1)); echo "  ok  : 新增 buy milk 落盘"
else
    fail=$((fail+1)); echo "  FAIL: 新增落盘(文件: $(cat "$T/todo-tui.data" 2>/dev/null | tr '\n' '|'))"
fi
if grep -q '?1006l' "$T/p1.out"; then
    pass=$((pass+1)); echo "  ok  : 退出还原(alt screen 退)"
else
    fail=$((fail+1)); echo "  FAIL: 退出还原缺失"
fi
# 相位二:重启回载 + 再加一条 + 退出
TODO_TUI_DEMO="a|s:ship the thing|RET|q" timeout 20 script -q /dev/null "$T/app.bin" > "$T/p2.out" 2>&1
if grep -qx "buy milk" "$T/todo-tui.data" && grep -qx "ship the thing" "$T/todo-tui.data"; then
    pass=$((pass+1)); echo "  ok  : 重启回载+第二条落盘(持久化)"
else
    fail=$((fail+1)); echo "  FAIL: 持久化(文件: $(cat "$T/todo-tui.data" 2>/dev/null | tr '\n' '|'))"
fi
# 相位三:删除(d 在首条)→ ship 幸存
TODO_TUI_DEMO="d|q" timeout 20 script -q /dev/null "$T/app.bin" > "$T/p3.out" 2>&1
if grep -qx "ship the thing" "$T/todo-tui.data" && ! grep -qx "buy milk" "$T/todo-tui.data"; then
    pass=$((pass+1)); echo "  ok  : 删除生效,幸存项保持"
else
    fail=$((fail+1)); echo "  FAIL: 删除面(文件: $(cat "$T/todo-tui.data" 2>/dev/null | tr '\n' '|'))"
fi

echo "todo-tui-e2e: pass=$pass fail=$fail"
[ "$fail" -eq 0 ] || exit 1
