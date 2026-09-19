#!/bin/sh
# tests/gui/peer_status.sh —— 并发泳道收敛检查(集成前例行)
# 检查:①FFI 套件;②GUI 阶梯;③w2 dump 是否被对端修复(期待含 on:click=inc 且无 OOB)
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")

echo "== peer status =="
ffi=$(sh "$ROOT/tests/ffi/run.sh" 2>&1 | tail -1)
echo "ffi : $ffi"

gui=$(sh "$DIR/run.sh" 2>&1 | tail -1)
echo "gui : $gui"

dump=$(cd "$ROOT/compiler" && ./bin/ctron-chk run ../tests/gui/w2_fold/src/main.ct --dump-gui 2>&1)
if echo "$dump" | grep -q "on:click=inc" && ! echo "$dump" | grep -q "index out of bounds"; then
    echo "dump: 已修复(含 on:click=inc)→ 可集成 w2_fold 黄金"
else
    echo "dump: 对端未修复(gui_parse 叶游标)→ w2_fold 继续等待"
fi

if echo "$ffi" | grep -q "0 败"; then
    echo "s0  : FFI 已收敛(历史批次已由泳道落库)"
else
    echo "s0  : FFI 未收敛"
fi
