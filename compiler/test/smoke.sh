#!/bin/sh
# smoke.sh —— compiler/ 目录验收冒烟
#
# 用法: ./smoke.sh [--full]
#   快面: 黄金对照 ×3 + 负例拦截 + check 自编译面(decl 锁定) + 发射往返 trans_v0–v3
#   --full: 追加自发射收官(发射 run 驱动编译器 → gcc → 原生解释器跑黄金)
#           与自举固定点(原生发射器 vs seed 发射器逐字节复现)
#
# 夹具与黄金基线沿用 selfhosted/(自举唯一差分源),本目录不复制。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$DIR/../.." && pwd)
HOST="$ROOT/compiler-c/build/ctronc"
COMP="$ROOT/compiler"
SH="$ROOT/selfhosted"
EXP="$SH/expected"
T=$(mktemp -d /tmp/ctron_smoke.XXXXXX)
pass=0; fail=0
ok()  { echo "  ok  : $1"; pass=$((pass+1)); }
bad() { echo "  FAIL: $1"; fail=$((fail+1)); }

[ -x "$HOST" ] || { echo "smoke.sh: 缺少宿主 seed $HOST(先: make -C \"$ROOT/compiler-c\")" >&2; exit 2; }

echo "== 1) 黄金对照(run: parse → 语义 12 项 → 解释执行) =="
for f in input_cc input_cc2 input_cc3; do
    "$COMP/ctc.sh" "$SH/$f.ct" > "$T/$f.out" 2>&1
    if diff -q "$EXP/$f.out" "$T/$f.out" > /dev/null 2>&1; then ok "$f 逐字一致"; else bad "$f 输出分歧"; fi
done
"$COMP/ctc.sh" "$SH/input_cc_neg.ct" > "$T/neg.out" 2>&1; rc=$?
if [ $rc -eq 1 ] && grep -q 'W8010: struct 含类引用字段(浅拷贝):Pair.b' "$T/neg.out"; then
    ok "负例编译期拦截(W8010, rc=1)"
else
    bad "负例未拦截(rc=$rc)"
fi

echo "== 2) check 模式(自编译面,decl 锁定) =="
"$COMP/ctc.sh" check "$COMP/build/cc_run.ct" > "$T/chk.out" 2>&1
grep -q 'check OK decls=273' "$T/chk.out" && ok "自检 cc_run 绿,decls=273" || bad "自检 cc_run: $(cat "$T/chk.out")"
check_decl() { # <源.ct> <期望decl>
    "$COMP/ctc.sh" check "$1" > "$T/cd.out" 2>&1
    grep -q "check OK decls=$2" "$T/cd.out" && ok "$(basename "$1") decls=$2(与 C 解析器锁定一致)" || bad "$(basename "$1") 期望 decls=$2, got $(cat "$T/cd.out")"
}
check_decl "$SH/sem_chk.ct"    109
check_decl "$SH/parsetree.ct"   57
check_decl "$SH/ev2.ct"        107
check_decl "$SH/cc.ct"         175

echo "== 2b) JSON 诊断契约(§10.2 v0) =="
"$COMP/ctc.sh" check "$COMP/test/fx_json_neg.ct" --format=json > "$T/js.out" 2>&1
jrc=$?
if [ $jrc -eq 1 ] && grep -q '"code":"W8010"' "$T/js.out" \
   && grep -q '"line_start":6' "$T/js.out" \
   && python3 -c "import json,sys; json.load(sys.stdin)" < "$T/js.out" 2>/dev/null; then
    ok "JSON 诊断合法且 span 精确(W8010@6, rc=1)"
else
    bad "JSON 诊断面异常(rc=$jrc): $(cat "$T/js.out")"
fi
echo "== 2c) 类型检查 v1 负例(E2020 全量/E2010 统一/变体 arity) =="
tc_fx() { # <夹具名> <期望诊断片段>
    "$COMP/ctc.sh" check "$COMP/test/$1.ct" > "$T/tc_$1.out" 2>&1
    rc=$?
    if [ $rc -eq 1 ] && grep -q "$2" "$T/tc_$1.out"; then
        ok "$1 拦截($2)"
    else
        bad "$1 异常(rc=$rc): $(cat "$T/tc_$1.out")"
    fi
}
tc_fx fx_unresolved_read_neg "E2020: 未解析的名称"
tc_fx fx_type_neg "E2010: let 初始化类型不匹配"
tc_fx fx_variant_neg "E2010: 调用实参数不匹配"
tc_fx fx_bound_neg "E2050"
tc_fx fx_litfit_neg "E2040"
tc_fx fx_litfit_as_neg "E2040"
tc_fx fx_litfit_ret_neg "E2040"
tc_fx fx_litfit_arg_neg "E2040"
tc_fx fx_bound_ann_neg "E2050"
tc_fx fx_trusted_neg "W8050"
tc_fx fx_trusted_fn_neg "E4040"
"$COMP/ctc.sh" emit "$COMP/test/fx_genrec_neg.ct" "$T/gr.c" > "$T/gr.out" 2>&1
grc=$?
if [ $grc -eq 1 ] && grep -q "递归超限" "$T/gr.out"; then
    ok "递归泛型拦截(递归超限, rc=1)"
else
    bad "递归泛型异常(rc=$grc): $(cat "$T/gr.out")"
fi
tc_fx fx_unused_neg "W8030: 未使用绑定"
tc_fx fx_shadow_neg "W8040: 遮蔽前奏符号"
tc_fx fx_capture_mut_neg "E3070: 闭包可变捕获"
echo "== 2d) comptime v0(§8 编译期求值) =="
"$COMP/ctc.sh" check "$COMP/test/fx_comp_ok.ct" > "$T/cp.out" 2>&1
if [ $? -eq 0 ] && grep -q "check OK" "$T/cp.out"; then
    ok "fx_comp_ok 检查面通过"
else
    bad "fx_comp_ok 异常: $(cat "$T/cp.out")"
fi
"$COMP/ctc.sh" "$COMP/test/fx_comp_ok.ct" > "$T/cpr.out" 2>&1
printf '49\nababab\nhi "ctron"\ntrue\n' > "$T/cp.exp"
if diff -q "$T/cp.exp" "$T/cpr.out" > /dev/null 2>&1; then
    ok "fx_comp_ok 运行输出 49/ababab/hi\"ctron\"/true"
else
    bad "fx_comp_ok 运行分歧: $(cat "$T/cpr.out")"
fi
tc_fx fx_comp_neg "E6020: comptime 函数含副作用"
tc_fx fx_comp_type_neg "E2010: const 类型不匹配"
"$COMP/ctc.sh" check "$COMP/test/fx_comp_eval_neg.ct" > "$T/cpe.out" 2>&1
if [ $? -eq 1 ] && grep -q "division by zero" "$T/cpe.out"; then
    ok "fx_comp_eval_neg 编译期中止(division by zero, rc=1)"
else
    bad "fx_comp_eval_neg 异常: $(cat "$T/cpe.out")"
fi
"$COMP/ctc.sh" check "$COMP/test/fx_forlist.ct" --format=json > "$T/js2.out" 2>&1
jrc2=$?
if [ $jrc2 -eq 0 ] && [ "$(cat "$T/js2.out")" = '{"diagnostics":[]}' ]; then
    ok "JSON 干净面 = {\"diagnostics\":[]}(rc=0)"
else
    bad "JSON 干净面异常(rc=$jrc2): $(cat "$T/js2.out")"
fi

echo "== 3) 发射往返(C 代码生成 → gcc → 原生执行,fixtures 全扫) =="
for v in v0 v1 v2 v3 v4 v5; do
    if "$COMP/ctc.sh" emit "$SH/fixtures/trans_$v.ct" "$T/tr_$v.c" > /dev/null 2>&1 \
       && cc -O1 -w -o "$T/tr_$v.bin" "$T/tr_$v.c" 2>/dev/null; then
        ( cd "$ROOT" && "$T/tr_$v.bin" > "$T/tr_$v.got" 2>&1 )
        ( cd "$ROOT" && "$HOST" run "$SH/fixtures/trans_$v.ct" > "$T/tr_$v.iv" 2>&1 )
        diff -q "$T/tr_$v.got" "$T/tr_$v.iv" > /dev/null 2>&1 && ok "trans_$v 往返逐字一致" || bad "trans_$v 输出分歧"
    else
        bad "trans_$v 发射/编译失败"
    fi
done

echo "== 3b) 并发发射面(Phase 4:spawn/Channel/Mutex/Atomic/parallel/cancel)+ 枚举(Phase 5) =="
for cv in spawn chan mutex atomic parallel joinor cancel; do
    FXC="$COMP/test/fx_conc_$cv.ct"
    if "$COMP/ctc.sh" emit "$FXC" "$T/cn_$cv.c" > /dev/null 2>&1 \
       && cc -O1 -w -o "$T/cn_$cv.bin" "$T/cn_$cv.c" 2>/dev/null; then
        timeout 15 "$T/cn_$cv.bin" > "$T/cn_$cv.got" 2>&1
        "$COMP/ctc.sh" "$FXC" > "$T/cn_$cv.iv" 2>&1
        diff -q "$T/cn_$cv.got" "$T/cn_$cv.iv" > /dev/null 2>&1 && ok "conc_$cv 原生==解释 逐字一致" || bad "conc_$cv 分歧"
    else
        bad "conc_$cv 发射/编译失败"
    fi
done
for cv in fnval cloval clostr enumres fnret try tlist own generic gstruct derive optstr boxalias gprobe2 gprobe fmap fs time drop; do
    if "$COMP/ctc.sh" emit "$COMP/test/fx_$cv.ct" "$T/cn_$cv.c" > /dev/null 2>&1 \
       && cc -O1 -w -o "$T/cn_$cv.bin" "$T/cn_$cv.c" 2>/dev/null; then
        timeout 15 "$T/cn_$cv.bin" > "$T/cn_$cv.got" 2>&1
        "$COMP/ctc.sh" "$COMP/test/fx_$cv.ct" > "$T/cn_$cv.iv" 2>&1
        diff -q "$T/cn_$cv.got" "$T/cn_$cv.iv" > /dev/null 2>&1 && ok "conc_$cv 原生==解释 逐字一致" || bad "conc_$cv 分歧"
    else
        bad "conc_$cv 发射/编译失败"
    fi
done
if "$COMP/ctc.sh" emit "$COMP/test/fx_enum.ct" "$T/en.c" > /dev/null 2>&1 \
   && cc -O1 -w -o "$T/en.bin" "$T/en.c" 2>/dev/null; then
    timeout 15 "$T/en.bin" > "$T/en.got" 2>&1
    "$COMP/ctc.sh" "$COMP/test/fx_enum.ct" > "$T/en.iv" 2>&1
    diff -q "$T/en.got" "$T/en.iv" > /dev/null 2>&1 && ok "用户枚举 原生==解释 逐字一致" || bad "用户枚举 分歧"
else
    bad "用户枚举 发射/编译失败"
fi
echo "== 3c) --profile bare 档(文件级 no_alloc → E3040)+ \\u{HEX} 解码 =="
"$COMP/ctc.sh" check "$COMP/test/fx_bare_neg.ct" --profile=bare > "$T/bare.out" 2>&1
brc=$?
if [ $brc -eq 1 ] && grep -q 'E3040' "$T/bare.out"; then
    ok "bare 档隐式分配拦截(E3040, rc=1)"
else
    bad "bare 档未拦截(rc=$brc)"
fi
"$COMP/ctc.sh" check "$COMP/test/fx_bare_neg.ct" > "$T/baref.out" 2>&1
if [ $? -eq 0 ]; then
    ok "full 档同源通过(档位门控生效)"
else
    bad "full 档误拦"
fi
echo "== 3d) std 种子包(use std.*:IntMap/IntSet) =="
drift=0
for f in "$ROOT"/std/*.ct; do
    b=$(basename "$f")
    diff -q "$f" "$COMP/test/stdpkg/std/$b" > /dev/null 2>&1 || drift=1
done
for f in "$COMP"/test/stdpkg/std/*.ct; do
    b=$(basename "$f")
    if [ ! -f "$ROOT/std/$b" ]; then drift=1; fi
done
if [ $drift -eq 0 ]; then
    ok "std 规范源与种子副本一致(无漂移)"
else
    bad "std 漂移:std/ 与 compiler/test/stdpkg/std 不一致,先同步"
fi
if "$COMP/bin/ctron-emit" run "$COMP/test/stdpkg/src/main.ct" > "$T/sd_native.c" 2>/dev/null; then
    cc -O1 -w -o "$T/sd.bin" "$T/sd_native.c" 2>/dev/null
    "$COMP/ctc.sh" "$COMP/test/stdpkg/src/main.ct" > "$T/sd_seed.out" 2>&1
    timeout 15 "$T/sd.bin" > "$T/sd.got" 2>&1
    diff -q "$T/sd.got" "$T/sd_seed.out" > /dev/null 2>&1 && ok "std 包 原生==解释 逐字一致" || bad "std 包 分歧"
else
    bad "std 包 发射失败"
fi
echo "== 3d-) use 撞名拦截(E5030,原静默遮蔽) =="
"$COMP/ctc.sh" check "$COMP/test/stdpkg_neg/src/main.ct" > "$T/n5030.out" 2>&1
if [ $? -eq 1 ] && grep -q "E5030" "$T/n5030.out"; then
    ok "use 撞名拦截(E5030)"
else
    bad "use 撞名未拦截: $(cat "$T/n5030.out")"
fi
echo "== 3e) 示例应用 examples/ctwc(wc 式统计,与真 wc 对数)+ str 种子单测 =="
CTWC="$ROOT/examples/ctwc"
SAMPLE="$T/sample.txt"
printf 'the quick brown fox\njumps over the lazy dog\nthe end\n' > "$SAMPLE"
if "$COMP/bin/ctron-emit" run "$CTWC/src/main.ct" > "$T/ctwc.c" 2>/dev/null \
   && cc -O1 -w -o "$T/ctwc.bin" "$T/ctwc.c" 2>/dev/null; then
    timeout 15 "$T/ctwc.bin" run "$SAMPLE" > "$T/ctwc.got" 2>&1
    WANT=$(wc "$SAMPLE" | awk '{print $1" "$2" "$3" "$4}')
    GOT=$(cat "$T/ctwc.got")
    [ "$GOT" = "$WANT" ] && ok "ctwc 与真 wc 对数一致($GOT)" || bad "ctwc 分歧: got[$GOT] want[$WANT]"
else
    bad "ctwc 发射/编译失败"
fi
if "$COMP/bin/ctron-cc" run "$CTWC/std/str.ct" > /dev/null 2>&1; then
    ok "str 种子单测通过(原生解释)"
else
    bad "str 种子单测失败"
fi
echo "== 3f) web 档(stdweb.dom 最小 API,§9.2) =="
"$COMP/ctc.sh" check "$ROOT/tests/10_web_dom.ct" --profile=web > "$T/w1.out" 2>&1
if [ $? -eq 0 ] && grep -q "check OK" "$T/w1.out"; then
    ok "web check 面通过(dom 解析)"
else
    bad "web check 异常: $(cat "$T/w1.out")"
fi
"$COMP/ctc.sh" check "$ROOT/tests/10_web_dom.ct" > "$T/w2.out" 2>&1
wrc=$?
if [ $wrc -eq 1 ] && grep -q "E2020" "$T/w2.out"; then
    ok "full 档 stdweb 拦截(E2020, rc=1)"
else
    bad "full 档未拦截(rc=$wrc): $(cat "$T/w2.out")"
fi
"$COMP/ctc.sh" "$ROOT/tests/10_web_dom.ct" --profile=web > "$T/w3.out" 2>&1
if [ $? -eq 0 ]; then
    ok "web 运行面 dom 往返"
else
    bad "web 运行面异常: $(cat "$T/w3.out")"
fi
echo "== 3g) #[trusted] 信任边界审计(§9.6) =="
"$COMP/ctc.sh" check "$ROOT/tests/modules/ffi_math/src/main.ct" --trusted > "$T/tr.out" 2>&1
if [ $? -eq 0 ] && grep -q "trusted: ctron_add@" "$T/tr.out"; then
    ok "信任边界审计枚举(--trusted)"
else
    bad "信任边界审计异常: $(cat "$T/tr.out")"
fi


echo "== 3g2) 示例应用 examples/ctwf(词频统计,对 sort/uniq 对数)+ fmap/sort 种子单测 =="
CTWF="$ROOT/examples/ctwf"
if "$COMP/bin/ctron-emit" run "$CTWF/src/main.ct" > "$T/ctwf.c" 2>/dev/null    && cc -O1 -w -o "$T/ctwf.bin" "$T/ctwf.c" 2>/dev/null; then
    timeout 15 "$T/ctwf.bin" run "$SAMPLE" > "$T/ctwf.got" 2>&1
    WANT=$(tr -s '[:space:]' '\n' < "$SAMPLE" | grep -v '^$' | sort | uniq -c | awk '{print $2" "$1}' | sort)
    GOT=$(grep -v '^distinct=' "$T/ctwf.got" | grep -v '^top:' | sort)
    [ "$GOT" = "$WANT" ] && ok "ctwf 词频与 sort/uniq 对数一致" || bad "ctwf 词频分歧: got[$GOT] want[$WANT]"
    DW=$(tr -s '[:space:]' '\n' < "$SAMPLE" | grep -v '^$' | sort | uniq | wc -l | tr -d ' ')
    TW=$(tr -s '[:space:]' '\n' < "$SAMPLE" | grep -v '^$' | wc -l | tr -d ' ')
    grep -q "distinct=$DW|total=$TW|$SAMPLE" "$T/ctwf.got" && ok "ctwf 汇总行对数(distinct=$DW total=$TW)" || bad "ctwf 汇总行分歧"
    timeout 15 "$T/ctwf.bin" run "$SAMPLE" > "$T/ctwf.got2" 2>&1
    diff -q "$T/ctwf.got" "$T/ctwf.got2" > /dev/null 2>&1 && ok "ctwf 确定性双跑逐字一致" || bad "ctwf 双跑分歧"
    grep -q "^top:the=3,quick=1,brown=1$" "$T/ctwf.got" && ok "ctwf top3 对数(count 降序,同 count 稳定)" || bad "ctwf top3 分歧: $(grep '^top:' "$T/ctwf.got")"
else
    bad "ctwf 发射/编译失败"
fi
if "$COMP/bin/ctron-cc" run "$COMP/test/stdpkg/std/fmap.ct" > /dev/null 2>&1; then
    ok "fmap 种子单测通过(原生解释)"
else
    bad "fmap 种子单测失败"
fi
if "$COMP/bin/ctron-cc" run "$COMP/test/stdpkg/std/sort.ct" > /dev/null 2>&1; then
    ok "sort 种子单测通过(原生解释)"
else
    bad "sort 种子单测失败"
fi
echo "== 3i) vendored std 一致性(examples 随包副本与 stdpkg 同步) =="
vend_ok=1
for pair in "ctwf:str" "ctwf:fmap" "ctwf:sort" "ctwc:str"; do
    app=${pair%%:*}
    mod=${pair##*:}
    diff -q "$COMP/test/stdpkg/std/$mod.ct" "$ROOT/examples/$app/std/$mod.ct" >/dev/null 2>&1 || vend_ok=0
done
if [ $vend_ok -eq 1 ]; then
    ok "vendored std 与 stdpkg 同步"
else
    bad "vendored std 漂移(cp compiler/test/stdpkg/std/*.ct examples/<app>/std/ 同步)"
fi
for st in map set fs; do
    if "$COMP/bin/ctron-cc" run "$COMP/test/stdpkg/std/$st.ct" > /dev/null 2>&1; then
        ok "$st 种子单测通过(原生解释)"
    else
        bad "$st 种子单测失败"
    fi
done
echo "== 3h) 示例应用 examples/ctgrep(子串 grep,与 grep -F -n 对数 + 退出码语义) =="
CTGREP="$ROOT/examples/ctgrep"
GSAMPLE="$T/gsample.txt"
printf 'the quick brown fox\njumps over the lazy dog\nthe end\nnothing here\n' > "$GSAMPLE"
if "$COMP/bin/ctron-emit" run "$CTGREP/src/main.ct" > "$T/ctgrep.c" 2>/dev/null \
   && cc -O1 -w -o "$T/ctgrep.bin" "$T/ctgrep.c" 2>/dev/null; then
    timeout 15 "$T/ctgrep.bin" run "the $GSAMPLE" > "$T/ctgrep.got" 2>&1
    GRC=$?
    grep -F -n "the" "$GSAMPLE" > "$T/ctgrep.want" 2>/dev/null
    if [ $GRC -eq 0 ] && diff -q "$T/ctgrep.want" "$T/ctgrep.got" > /dev/null 2>&1; then
        ok "ctgrep 与 grep -F -n 对数一致"
    else
        bad "ctgrep 分歧(rc=$GRC): got[$(cat "$T/ctgrep.got")] want[$(cat "$T/ctgrep.want")]"
    fi
    timeout 15 "$T/ctgrep.bin" run "zzz $GSAMPLE" > "$T/ctgrep.none" 2>&1
    if [ $? -eq 1 ] && [ ! -s "$T/ctgrep.none" ]; then
        ok "ctgrep 无命中 rc=1 无输出(grep 口径)"
    else
        bad "ctgrep 无命中语义分歧"
    fi
    timeout 15 "$T/ctgrep.bin" run "no-space-arg" > /dev/null 2>&1
    if [ $? -eq 2 ]; then
        ok "ctgrep 用法拦截 rc=2"
    else
        bad "ctgrep 用法语义分歧"
    fi
    if "$COMP/bin/ctron-cc" run "$CTGREP/std/str.ct" > /dev/null 2>&1; then
        ok "ctgrep str 种子单测通过(原生解释)"
    else
        bad "ctgrep str 种子单测失败"
    fi
else
    bad "ctgrep 发射/编译失败"
fi
for cv in uhex; do
    if "$COMP/ctc.sh" emit "$COMP/test/fx_$cv.ct" "$T/cn_$cv.c" > /dev/null 2>&1 \
       && cc -O1 -w -o "$T/cn_$cv.bin" "$T/cn_$cv.c" 2>/dev/null; then
        timeout 15 "$T/cn_$cv.bin" > "$T/cn_$cv.got" 2>&1
        "$COMP/ctc.sh" "$COMP/test/fx_$cv.ct" > "$T/cn_$cv.iv" 2>&1
        diff -q "$T/cn_$cv.got" "$T/cn_$cv.iv" > /dev/null 2>&1 && ok "conc_$cv 原生==解释 逐字一致" || bad "conc_$cv 分歧"
    else
        bad "conc_$cv 发射/编译失败"
    fi
done

if [ "${1:-}" = "--full" ]; then
    echo "== 4) 自发射收官(发射 run 驱动编译器 → 原生解释器) =="
    if "$COMP/ctc.sh" emit "$COMP/build/cc_run.ct" "$T/cc_self.c" > /dev/null 2>&1 \
       && cc -O1 -w -o "$T/cc_interp.bin" "$T/cc_self.c" 2>/dev/null; then
        for f in input_cc input_cc2 input_cc3; do
            ( cd "$ROOT/compiler-c" && "$T/cc_interp.bin" run "$SH/$f.ct" > "$T/n_$f.out" 2>&1 )
            diff -q "$EXP/$f.out" "$T/n_$f.out" > /dev/null 2>&1 && ok "原生解释 $f == 黄金" || bad "原生解释 $f 分歧"
        done
        ( cd "$ROOT/compiler-c" && "$T/cc_interp.bin" run "$SH/input_cc_neg.ct" > "$T/n_neg.out" 2>&1 )
        grep -q 'W8010' "$T/n_neg.out" && ok "原生负例拦截" || bad "原生负例未拦截"
    else
        bad "自发射/编译失败"
    fi
    echo "== 4b) I64 溢出检查(seed 与原生发射双面一致,§4.5 检查算术) =="
    cat > "$T/ov.ct" <<'P'
fn main() -> I32 {
    var a: I64 = 9223372036854775807
    var b: I64 = a + a
    println(b.to_string())
    return 0
}
P
    "$COMP/ctc.sh" "$T/ov.ct" > "$T/ov_s.out" 2>&1
    grep -q 'integer overflow' "$T/ov_s.out" && ok "I64 溢出 seed 拦截" || bad "I64 溢出 seed 未拦截: $(tail -1 "$T/ov_s.out")"
    if "$COMP/ctc.sh" emit "$T/ov.ct" "$T/ov.c" > /dev/null 2>&1 && cc -O2 -o "$T/ov_bin" "$T/ov.c" 2>/dev/null; then
        "$T/ov_bin" > "$T/ov_n.out" 2>&1
        grep -q 'integer overflow' "$T/ov_n.out" && ok "I64 溢出原生拦截" || bad "I64 溢出原生未拦截: $(tail -1 "$T/ov_n.out")"
    else
        bad "I64 溢出发射编译失败"
    fi
    # 复合赋值与乘法路径(帮手路由 + c6 界门镜像)
    cat > "$T/ov2.ct" <<'P'
fn main() -> I32 {
    var c: I64 = 9223372036854775807
    c += 1
    println(c.to_string())
    return 0
}
P
    cat > "$T/ov3.ct" <<'P'
fn main() -> I32 {
    var m: I64 = 3037000500
    var q = m * m
    println(q.to_string())
    return 0
}
P
    "$COMP/ctc.sh" "$T/ov2.ct" > "$T/ov2_s.out" 2>&1
    grep -q 'integer overflow' "$T/ov2_s.out" && ok "I64 复合赋值溢出 seed 拦截" || bad "I64 复合赋值溢出 seed 未拦截"
    "$COMP/ctc.sh" "$T/ov3.ct" > "$T/ov3_s.out" 2>&1
    grep -q 'integer overflow' "$T/ov3_s.out" && ok "I64 乘法溢出 seed 拦截" || bad "I64 乘法溢出 seed 未拦截"
    if "$COMP/ctc.sh" emit "$T/ov2.ct" "$T/ov2.c" > /dev/null 2>&1 && cc -O2 -o "$T/ov2_bin" "$T/ov2.c" 2>/dev/null && "$COMP/ctc.sh" emit "$T/ov3.ct" "$T/ov3.c" > /dev/null 2>&1 && cc -O2 -o "$T/ov3_bin" "$T/ov3.c" 2>/dev/null; then
        "$T/ov2_bin" > "$T/ov2_n.out" 2>&1
        grep -q 'integer overflow' "$T/ov2_n.out" && ok "I64 复合赋值溢出原生拦截" || bad "I64 复合赋值溢出原生未拦截"
        "$T/ov3_bin" > "$T/ov3_n.out" 2>&1
        grep -q 'integer overflow' "$T/ov3_n.out" && ok "I64 乘法溢出原生拦截" || bad "I64 乘法溢出原生未拦截"
    else
        bad "I64 溢出扩展发射编译失败"
    fi
    echo "== 5) 自举固定点(原生发射器 vs seed 发射器,逐字节) =="
    if "$COMP/ctc.sh" emit "$COMP/build/cc_emit.ct" "$T/cc_emit.c" > /dev/null 2>&1 \
       && cc -O1 -w -o "$T/cc_emitter.bin" "$T/cc_emit.c" 2>/dev/null; then
        "$T/cc_emitter.bin" run "$COMP/build/cc_run.ct" > "$T/c2.c" 2>&1
        diff -q "$T/cc_self.c" "$T/c2.c" > /dev/null 2>&1 && ok "固定点:发射产物逐字节复现" || bad "固定点:两路发射产物分歧"
    else
        bad "发射器自发射失败"
    fi
fi

echo "== 结果: $pass ok / $fail fail =="
rm -rf "$T"
[ $fail -eq 0 ]
