#!/bin/sh
# tests/gui/run.sh —— GUI 泳道统一验收:S1–S9 阶梯一键全跑
# 口径:任一夹具红即非零退出;s7/s9 仅构建链接(窗口交互验收用 --run 单独启动);
# 全程 headless(无显示依赖),ci.sh [8/8] 门禁即跑本脚本。

set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
pass=0
fail=0
failed=""

for s in s1_smoke s2_ab s3_clay s4_events s5_golden s6_demo s7_window s8_cjk s9_window_cjk e8_corpus w2_fold w4_interp s10_when s11_each s12_input s13_item w4_hotreload w4_ctml_reload s14_bidi s15_bidi_ct s16_measure s17_bidi_layout s18_scroll s19_input_d s20_embed s21_frame_golden s23_sk_native s24_model_d s25_expr_d s26_reload_d s27_checkbox_d s28_hitreg_d s28_ft_flush s29_ev_expr_d s30_props_d s31_state s32_theme s33_focus s34_overlay s35_image s36_tick s37_size s38_hotkey s40_ev_args_d s41_run_d s41_font s42_comp s39_eachcomp s43_tab s44_textarea; do
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
