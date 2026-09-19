#!/bin/sh
# tests/gui/run.sh —— GUI 泳道统一验收:S1–S9 阶梯一键全跑
# 口径:任一夹具红即非零退出;s7/s9 仅构建链接(窗口交互验收用 --run 单独启动);
# 全程 headless(无显示依赖),ci.sh [8/8] 门禁即跑本脚本。

set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
pass=0
fail=0
failed=""

for s in s1_smoke s2_ab s3_clay s4_events s5_golden s6_demo s7_window s8_cjk s9_window_cjk e8_corpus w2_fold w4_interp s10_when s11_each s12_input s13_item w4_hotreload s14_bidi s15_bidi_ct s16_measure s17_bidi_layout; do
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
