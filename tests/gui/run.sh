#!/bin/sh
# tests/gui/run.sh —— GUI 泳道统一验收:S1–S7 阶梯一键全跑
# 口径:任一夹具红即非零退出;s7 仅构建链接(窗口交互验收用 --run 单独启动)
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
pass=0
fail=0
failed=""

for s in s1_smoke s2_ab s3_clay s4_events s5_golden s6_demo s7_window; do
    if sh "$DIR/$s/run.sh" > /tmp/gui_ladder_$s.log 2>&1; then
        echo "  [ok] $s"
        pass=$((pass + 1))
    else
        echo "  [FAIL] $s(日志: /tmp/gui_ladder_$s.log)"
        fail=$((fail + 1))
        failed="$failed $s"
    fi
done

echo "gui: $pass 过 / $fail 败"
[ "$fail" -eq 0 ] || { echo "失败夹具:$failed" >&2; exit 1; }
