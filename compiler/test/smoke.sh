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
grep -q 'check OK decls=390' "$T/chk.out" && ok "自检 cc_run 绿,decls=366" || bad "自检 cc_run: $(cat "$T/chk.out")"
check_decl() { # <源.ct> <期望decl>
    "$COMP/ctc.sh" check "$1" > "$T/cd.out" 2>&1
    grep -q "check OK decls=$2" "$T/cd.out" && ok "$(basename "$1") decls=$2(与 C 解析器锁定一致)" || bad "$(basename "$1") 期望 decls=$2, got $(cat "$T/cd.out")"
}
check_decl "$SH/sem_chk.ct"    109
check_decl "$SH/parsetree.ct"   57
check_decl "$SH/ev2.ct"        107
check_decl "$SH/cc.ct"         175

echo "== 2e) 自举解析器韧性(P0-E:非法输入报 E1001 不崩,native 面) =="
for pf in 01i_semicolon 01j_impl_for; do
    "$COMP/bin/ctron-cc" run "$ROOT/tests/$pf.neg.ct" > "$T/pe_$pf.out" 2>&1
    prc=$?
    if [ $prc -eq 1 ] && grep -q 'E1001' "$T/pe_$pf.out"; then
        ok "native $pf E1001 拦截(rc=1,不崩)"
    else
        bad "native $pf 异常(rc=$prc): $(cat "$T/pe_$pf.out")"
    fi
done

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
    tc_fx fx_res_class_neg "E4050"
tc_fx fx_bound_ann_neg "E2050"
tc_fx fx_str_esc_neg "非法转义"
tc_fx fx_interp_neg "未终止的插值"
tc_fx fx_trusted_neg "W8050"
tc_fx fx_trusted_fn_neg "E4040"
echo "== 2d) 诊断 i18n(§10.8;ANCHORLANG 构建锚) =="
"$COMP/ctc.sh" check "$COMP/test/fx_type_neg.ct" --lang=en > "$T/tc_en.out" 2>&1
erc=$?
if [ $erc -eq 1 ] && grep -q "let initializer type mismatch" "$T/tc_en.out"; then
    ok "诊断 i18n en 面(--lang=en,E2010.let 英文文案)"
else
    bad "诊断 i18n en 异常(rc=$erc): $(cat "$T/tc_en.out")"
fi
"$COMP/ctc.sh" check "$COMP/test/fx_type_neg.ct" > "$T/tc_zh.out" 2>&1
if grep -q "let 初始化类型不匹配" "$T/tc_zh.out"; then
    ok "诊断 i18n 缺省 zh(不随 en 用例漂移)"
else
    bad "诊断 i18n 缺省语言异常: $(cat "$T/tc_zh.out")"
fi
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
for v in v0 v1 v2 v3 v4 v5 v6; do
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
for cv in fnval cloval clostr enumres fnret try tlist own generic gstruct derive optstr boxalias gprobe2 gprobe fmap fs time drop slice simd fnval_multi u64 drop_unwind w8; do
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
echo "== 3i2) ctron vendor 原型(use 闭集拷贝+钉版清单;09-21 spec §5.4) =="
if python3 "$ROOT/tools/ctron_vendor.py" std.str,std.fmap,std.sort "$T/vendp" >/dev/null 2>&1 \
   && diff -q "$COMP/test/stdpkg/std/str.ct" "$T/vendp/str.ct" >/dev/null 2>&1 \
   && diff -q "$COMP/test/stdpkg/std/fmap.ct" "$T/vendp/fmap.ct" >/dev/null 2>&1 \
   && diff -q "$COMP/test/stdpkg/std/sort.ct" "$T/vendp/sort.ct" >/dev/null 2>&1 \
   && grep -q "^str sha256:" "$T/vendp/VENDOR.lock" 2>/dev/null \
   && ! python3 "$ROOT/tools/ctron_vendor.py" std.net "$T/vendn" >/dev/null 2>&1; then
    ok "vendor 原型:T1 闭集拷贝+钉版清单绿,域包 fail-closed 拒绝"
else
    bad "vendor 原型异常(闭集拷贝/lock/域包拒绝)"
fi
for st in map set fs; do
    if "$COMP/bin/ctron-cc" run "$COMP/test/stdpkg/std/$st.ct" > /dev/null 2>&1; then
        ok "$st 种子单测通过(原生解释)"
    else
        bad "$st 种子单测失败"
    fi
done
echo "== 3j2) std 全模块自举单测(parity 矩阵自举臂;extern/c_src 依赖面除外) =="
std_parity=0
std_total=0
for f in "$ROOT"/std/*.ct; do
    b=$(basename "$f")
    # config/net/tls:extern/c_src 依赖面;fmap/crypto:arena 大户——自举解释器
    # 每步 ~2K 个 16B arena 小对象且 bump 无回收(插桩实证:fmap 饱和测 1.63 亿
    # 次/3GB;fput 每调用 ~50 万次),runner 7GB 必被 SIGTERM。语义双臂在
    # parity 矩阵与 macOS 本地 smoke 常绿;恢复条件=解释器 arena 回收/每步
    # 分配量治理落地(编译器线在册债)
    case $b in config.ct|net.ct|tls.ct|fmap.ct|crypto.ct) continue ;; esac
    std_total=$((std_total+1))
    if ! "$COMP/bin/ctron-cc" run "$f" > /dev/null 2>&1; then
        bad "std 单测自举红: $b"
        std_parity=1
    fi
done
if [ $std_parity -eq 0 ]; then
    ok "std 全模块自举单测绿($std_total 模块)"
fi
echo "== 3j3) std 全模块 Rust 臂 parity(内建面欠账清单驱动;清账即翻绿提醒) =="
RUSTBIN="$ROOT/compiler-rust/target/release/ctron"
if [ -x "$RUSTBIN" ]; then
    known="json sort fs unicode gui time"
    newred=0
    knownred=0
    flipped=""
    for f in "$ROOT"/std/*.ct; do
        b=$(basename "$f" .ct)
        if echo " $known " | grep -q " $b "; then
            if ! CTRON_MAX_STEPS=0 "$RUSTBIN" test "$f" > /dev/null 2>&1; then
                knownred=$((knownred+1))
            else
                flipped="$flipped $b"
            fi
        else
            if ! CTRON_MAX_STEPS=0 "$RUSTBIN" test "$f" > /dev/null 2>&1; then
                bad "std Rust 臂新红: $b(登记 interp 线内建面)"
                newred=1
            fi
        fi
    done
    if [ $newred -eq 0 ]; then
        ok "std Rust 臂无新红(欠账 $knownred 模块待 interp 线:fs_exists/now_ms/宽整型)"
    fi
    if [ -n "$flipped" ]; then
        bad "Rust 臂翻绿待清账:$flipped(从欠账清单移除)"
    fi
else
    ok "Rust 参考臂未构建,跳过(ci 全量跑)"
fi
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

echo "== 3j) FFI 发射链接面(§9.6·§9.8,tests/ffi/) =="
# linux:主程序符号默认不进 dlsym 全局域,#[dlsym] thunk 需 -rdynamic(mac 两级
# 命名空间默认可达,历史未暴露)
case $(uname) in Linux) FFI_RDL="-rdynamic" ;; *) FFI_RDL="" ;; esac
ffi_case() { # <目录名>
    local d="$ROOT/tests/ffi/$1"
    if "$COMP/bin/ctron-emit" run "$d/src/main.ct" > "$T/ffi_$1.c" 2>/dev/null \
       && cc -O1 -w $FFI_RDL -o "$T/ffi_$1.bin" "$T/ffi_$1.c" "$d"/c_src/*.c 2>/dev/null \
       && "$T/ffi_$1.bin" run "$d/src/main.ct" > "$T/ffi_$1.out" 2>&1; then
        ok "ffi/$1 emit+链接+原生运行"
    else
        bad "ffi/$1 发射链接运行失败: $(head -c 100 "$T/ffi_$1.out" 2>/dev/null)"
    fi
}
ffi_case callback
ffi_case repr_c
ffi_case variadic
ffi_case dyn_link
ffi_case layout
ffi_case ext_fn_ret
"$COMP/bin/ctron-cc" run "$ROOT/tests/ffi/variadic_nonext.neg.ct" > "$T/ffi_va.out" 2>&1; rc=$?
if [ $rc -eq 1 ] && grep -q "E4044" "$T/ffi_va.out"; then
    ok "FFI 负例拦截(变参非 extern E4044, rc=1)"
else
    bad "FFI 变参负例未拦截(rc=$rc)"
fi
"$COMP/bin/ctron-cc" run "$ROOT/tests/ffi/closure_cb.neg.ct" > "$T/ffi_neg.out" 2>&1; rc=$?
if [ $rc -eq 1 ] && grep -q "E4042" "$T/ffi_neg.out"; then
    ok "FFI 负例拦截(捕获闭包回调 E4042, rc=1)"
else
    bad "FFI 负例未拦截(rc=$rc)"
fi
"$COMP/bin/ctron-cc" run "$ROOT/tests/ffi/ext_nonabi_param.lint.ct" > "$T/ffi_lint.out" 2>&1
grep -q "W8052" "$T/ffi_lint.out" && ok "FFI lint 警示(extern 非 C-ABI 形参 W8052)" || bad "FFI lint 缺 W8052"

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
    echo "== 4c) std 全模块发射原生 parity(矩阵第三臂;known=heap/opt 闭包面欠账登记 trans 线) =="
    emit_green=0
    emit_knownred=0
    emit_newred=0
    emit_flipped=""
    # 4c known 欠账表按平台:heap 在 linux 双臂已绿(翻绿清账),mac 解释臂仍红
    case $(uname) in
        Darwin) KNOWN4C=" heap opt " ;;
        *)      KNOWN4C=" opt " ;;
    esac
    for f in "$ROOT"/std/*.ct; do
        b=$(basename "$f" .ct)
        # fmap/crypto:arena 大户(解释臂 GB 级,见 3j2 注)——runner 必被 SIGTERM
        case $b in config|net|tls|fmap|crypto) continue ;; esac
        arm_ok=0
        if "$COMP/bin/ctron-emit" run "$f" > "$T/pe_$b.c" 2>/dev/null \
           && cc -O1 -w -o "$T/pe_$b.bin" "$T/pe_$b.c" 2>/dev/null; then
            out_n=$("$T/pe_$b.bin" run "$f" 2>&1); rc_n=$?
            out_i=$("$COMP/bin/ctron-cc" run "$f" 2>&1); rc_i=$?
            [ $rc_n -eq 0 ] && [ $rc_i -eq 0 ] && [ "$out_n" = "$out_i" ] && arm_ok=1
        fi
        if echo " $KNOWN4C " | grep -q " $b "; then
            if [ $arm_ok -eq 1 ]; then
                emit_flipped="$emit_flipped $b"
            else
                emit_knownred=$((emit_knownred+1))
            fi
        else
            if [ $arm_ok -eq 1 ]; then
                emit_green=$((emit_green+1))
            else
                bad "std 发射臂新红: $b(emit/cc/run/两臂输出对数)"
                emit_newred=1
            fi
        fi
    done
    if [ $emit_newred -eq 0 ]; then
        ok "std 发射臂 parity 绿($emit_green 模块;known $emit_knownred 待 trans 线:闭包结构值/比较器形参)"
    fi
    if [ -n "$emit_flipped" ]; then
        bad "发射臂翻绿待清账:$emit_flipped(从 known 移除)"
    fi
fi

echo "== 3f) fmt(R-P2d 自举面:金样/双口径/幂等/负例) =="
"$COMP/ctc.sh" fmt "$COMP/test/fx_fmt_golden.ct" > "$T/fmt_g.out" 2>&1
if diff -q "$COMP/test/fx_fmt_golden.expected" "$T/fmt_g.out" > /dev/null 2>&1; then
    ok "金样逐字节(seed 面,期望=Rust 参考实现冻结)"
else
    bad "金样 seed 分歧: $(diff "$COMP/test/fx_fmt_golden.expected" "$T/fmt_g.out" | head -3)"
fi
"$COMP/bin/ctron-fmt" run "$COMP/test/fx_fmt_golden.ct" > "$T/fmt_n.out" 2>&1
if diff -q "$T/fmt_g.out" "$T/fmt_n.out" > /dev/null 2>&1; then
    ok "native==seed 双口径一致(ctron-fmt)"
else
    bad "native/seed 双口径分歧"
fi
"$COMP/ctc.sh" fmt "$COMP/test/fx_fmt_golden.expected" > "$T/fmt_idem.out" 2>&1
if diff -q "$COMP/test/fx_fmt_golden.expected" "$T/fmt_idem.out" > /dev/null 2>&1; then
    ok "幂等:fmt(fmt(x))==fmt(x)"
else
    bad "幂等分歧"
fi
printf 'fn main() {\nvar x = 1;\n}\n' > "$T/fmt_neg.ct"
"$COMP/ctc.sh" fmt "$T/fmt_neg.ct" > "$T/fmt_neg.out" 2>&1
fnrc=$?
if [ $fnrc -eq 1 ] && grep -q 'E1001' "$T/fmt_neg.out"; then
    ok "词法脏报错退出(规范 R8, rc=1)"
else
    bad "词法脏异常(rc=$fnrc)"
fi

echo "== 3g) doc(iface 投影:S0 面——文本金样/幂等/JSON 形准/负例) =="
# decls=6:P1b 选择性合并后 geom 的私有 _hidden/MAXW(未请求、零引用)不再搭车合并
"$COMP/ctc.sh" doc "$ROOT/tests/doc_fix/main.ct" > "$T/doc1.out" 2>&1
if [ $? -eq 0 ] && grep -q "decls=6$" "$T/doc1.out"; then
    tail -n +2 "$T/doc1.out" > "$T/doc1.body"
    cat > "$T/doc1.gold" <<'EOD'
// doc_fix 入口夹具(iface 投影金样)
pub struct Point { let x: I32, let y: I32 }
pub enum Shape { Circle(I32), Rect { let w: I32, let h: I32 } }
trait Show
  fn show(self) -> Str
impl Show for Point
  fn show(self) -> Str
pub fn area(s: Shape) -> I32
iface symbols=3
EOD
    if diff -q "$T/doc1.gold" "$T/doc1.body" > /dev/null 2>&1; then
        ok "doc 文本金样逐字一致(全形态夹具)"
    else
        bad "doc 文本金样分歧: $(head -3 "$T/doc1.body")"
    fi
else
    bad "doc 文本面异常: $(cat "$T/doc1.out")"
fi
"$COMP/ctc.sh" doc "$ROOT/tests/doc_fix/main.ct" > "$T/doc2.out" 2>&1
if diff -q "$T/doc1.out" "$T/doc2.out" > /dev/null 2>&1; then
    ok "幂等:doc(x)==doc(x)"
else
    bad "doc 非幂等"
fi
"$COMP/ctc.sh" doc "$ROOT/tests/doc_fix/main.ct" --format=json > "$T/doc3.out" 2>&1
if [ $? -eq 0 ] \
    && grep -qF '{"entry":' "$T/doc3.out" \
    && grep -qF '"doc":"doc_fix' "$T/doc3.out" \
    && grep -q '"kind":"struct"' "$T/doc3.out" \
    && grep -q '"kind":"enum"' "$T/doc3.out" \
    && grep -q '"kind":"trait"' "$T/doc3.out" \
    && grep -q '"kind":"impl"' "$T/doc3.out" \
    && grep -q '"kind":"fn"' "$T/doc3.out" \
    && tail -c 3 "$T/doc3.out" | grep -q '\]}'; then
    ok "doc JSON 形准(六形态齐,首尾闭合)"
else
    bad "doc JSON 面异常: $(head -c 120 "$T/doc3.out")"
fi
"$COMP/ctc.sh" doc "$ROOT/tests/doc_fix_neg/main.ct" > "$T/doc4.out" 2>&1
if [ $? -eq 1 ] && grep -q "E5030" "$T/doc4.out"; then
    ok "doc 负例拦截(E5030 同判, rc=1)"
else
    bad "doc 负例未拦截: $(cat "$T/doc4.out")"
fi

"$COMP/ctc.sh" doc "$ROOT/tests/doc_fix/geom.ct" > "$T/doc5.out" 2>&1
if [ $? -eq 0 ] && grep -qF '// 面积:Circle 取 r 平方,Rect 取 w*h' "$T/doc5.out" && grep -qF 'pub fn area(s: Shape) -> I32' "$T/doc5.out"; then
    ok "doc per-fn 契约注释(§5.3 首片,name-keyed,紧邻注释块)"
else
    bad "doc per-fn 注释异常: $(cat "$T/doc5.out")"
fi

(cd "$ROOT" && "$ROOT/ctc" doc std.str --format=json > "$T/doc6.out" 2>&1)
if [ $? -eq 0 ] && grep -qF '"entry":"std/str.ct"' "$T/doc6.out" && grep -qF '"kind":"fn"' "$T/doc6.out"; then
    ok "doc std.<module> 形(§5.3:std 根四级解析,entry 归一)"
else
    bad "doc std.<module> 异常: $(head -c 120 "$T/doc6.out")"
fi

"$COMP/ctc.sh" ast "$ROOT/tests/doc_fix/geom.ct" > "$T/doc7.out" 2>&1
if [ $? -eq 0 ] && grep -q "ast roundtrip OK" "$T/doc7.out"; then
    ok "ast 往返固定点(.ctast S1a,geom 夹具)"
else
    bad "ast 往返异常: $(head -c 120 "$T/doc7.out")"
fi
"$COMP/ctc.sh" ast "$COMP/test/stdpkg/src/main.ct" > "$T/doc8.out" 2>&1
if [ $? -eq 0 ] && grep -q "ast roundtrip OK" "$T/doc8.out"; then
    ok "ast 往返固定点(stdpkg 合并语料)"
else
    bad "ast stdpkg 往返异常: $(head -c 120 "$T/doc8.out")"
fi
"$COMP/ctc.sh" ast "$COMP/test/fx_use_alias.ct" > "$T/doc9.out" 2>&1
if [ $? -eq 0 ] && grep -q "ast roundtrip OK" "$T/doc9.out"; then
    ok "ast 往返固定点(use 别名交替对 Syms)"
else
    bad "ast 往返异常(use 别名): $(head -c 120 "$T/doc9.out")"
fi

echo "== 3k) 示例应用 examples/ctecho(echo server:run.sh 自冒烟,3 探针逐字)=="
if command -v nc >/dev/null 2>&1; then
    (cd "$ROOT/examples/ctecho" && timeout 90 sh run.sh > "$T/ctecho.out" 2>&1)
    CRC=$?
    if [ "$CRC" -eq 0 ] && grep -q "3/3 echo" "$T/ctecho.out"; then
        ok "ctecho 回环服务自冒烟(3 探针逐字一致)"
    else
        bad "ctecho 自冒烟失败(rc=$CRC): $(tail -2 "$T/ctecho.out")"
    fi
else
    ok "ctecho 跳过(环境无 nc)"
fi

echo "== 3l) S2a 闭源工件流(seal→deps 消费→正确性 + 碰撞负例)=="
AD="$T/s2a"
mkdir -p "$AD/art/impl" "$AD/c1/deps/mygeom.ctart" "$AD/c2/deps/mygeom.ctart"
if "$COMP/ctc.sh" ast "$ROOT/tests/artifact_demo/provider/geom.ct" --ast=seal --astout="$AD/art" --astname=mygeom > "$T/seal.out" 2>&1 \
   && grep -q "ast seal OK" "$T/seal.out" \
   && cp "$ROOT/tests/artifact_demo/consumer/main.ct" "$AD/c1/" \
   && cp -r "$AD/art/." "$AD/c1/deps/mygeom.ctart/" \
   && (cd "$AD/c1" && "$COMP/ctc.sh" main.ct > "$T/c1.out" 2>&1) \
   && grep -q "rect=12" "$T/c1.out" && grep -q "circle=27" "$T/c1.out"; then
    ok "S2a 工件消费(仅 deps 工件源码缺席,行为正确)"
else
    bad "S2a 工件消费失败: $(tail -2 "$T/c1.out" 2>/dev/null; tail -1 "$T/seal.out")"
fi
if cp "$ROOT/tests/artifact_demo/consumer_neg/main.ct" "$AD/c2/" \
   && cp -r "$AD/art/." "$AD/c2/deps/mygeom.ctart/" \
   && (cd "$AD/c2" && "$COMP/ctc.sh" main.ct > "$T/c2.out" 2>&1); then
    bad "S2a 碰撞未拦截"
else
    grep -q "E5030" "$T/c2.out" && ok "S2a 碰撞负例拦截(E5030 同判)" || bad "S2a 碰撞负例异常: $(head -1 "$T/c2.out")"
fi

mkdir -p "$AD/stduse/impl" "$AD/c3/deps/mygeom.ctart"
if "$COMP/ctc.sh" ast "$ROOT/tests/artifact_demo/provider/geom_str.ct" --ast=seal --astout="$AD/stduse" --astname=mygeom > "$T/seal3.out" 2>&1 \
   && grep -q "ast seal OK" "$T/seal3.out" \
   && ln -sfn "$ROOT/std" "$AD/std" \
   && cp "$ROOT/tests/artifact_demo/consumer_str/main.ct" "$AD/c3/" \
   && cp -r "$AD/stduse/." "$AD/c3/deps/mygeom.ctart/" \
   && (cd "$AD/c3" && "$COMP/ctc.sh" main.ct > "$T/c3.out" 2>&1) \
   && grep -q "GEO" "$T/c3.out"; then
    ok "S2a 工件 std.use(seed 回落口径, std 依赖不密封)"
else
    bad "S2a 工件 std.use 异常: $(tail -2 "$T/c3.out" 2>/dev/null; tail -1 "$T/seal3.out")"
fi

echo "== 3m) S2a 版本与传递依赖(双版本共存/同名碰撞/传递/缺传递负例)=="
V="$T/ver"
for d in a1 a2 ab ap a2c; do mkdir -p "$V/$d/impl"; done
mkdir -p "$V/cv/deps/mygeom.ctart" "$V/cv/deps/mygeom2.ctart" "$V/cc/deps/mygeom.ctart" "$V/cc/deps/mygeom2.ctart" "$V/ct/deps/mybase.ctart" "$V/ct/deps/mypkg.ctart" "$V/ctn/deps/mypkg.ctart"
"$COMP/ctc.sh" ast "$ROOT/tests/artifact_demo/provider/geom_v1/geom.ct" --ast=seal --astout="$V/a1" --astname=mygeom > "$T/v1.out" 2>&1
"$COMP/ctc.sh" ast "$ROOT/tests/artifact_demo/provider_v2/geom.ct" --ast=seal --astout="$V/a2" --astname=mygeom2 > "$T/v2.out" 2>&1
"$COMP/ctc.sh" ast "$ROOT/tests/artifact_demo/base/base.ct" --ast=seal --astout="$V/ab" --astname=mybase > "$T/vb.out" 2>&1
"$COMP/ctc.sh" ast "$ROOT/tests/artifact_demo/calc/calc.ct" --ast=seal --astout="$V/ap" --astname=mypkg > "$T/vp.out" 2>&1
cp -r "$V/a1/." "$V/cv/deps/mygeom.ctart/" && cp -r "$V/a2/." "$V/cv/deps/mygeom2.ctart/" && cp "$ROOT/tests/artifact_demo/consumer_ver/main.ct" "$V/cv/"
(cd "$V/cv" && "$COMP/ctc.sh" main.ct > "$T/cv.out" 2>&1)
if grep -q "v1=12" "$T/cv.out" && grep -q "v2p=14" "$T/cv.out"; then
    ok "同包双版本共存(v1 area_rect + v2 perim_rect 同程序)"
else
    bad "双版本共存异常: $(head -2 "$T/cv.out")"
fi
cp -r "$V/a1/." "$V/cc/deps/mygeom.ctart/" && cp "$ROOT/tests/artifact_demo/consumer_clash/main.ct" "$V/cc/"
mkdir -p "$V/a2c/impl"
"$COMP/ctc.sh" ast "$ROOT/tests/artifact_demo/provider_clash/geom.ct" --ast=seal --astout="$V/a2c" --astname=mygeom2 > "$T/v2c.out" 2>&1
cp -r "$V/a2c/." "$V/cc/deps/mygeom2.ctart/"
(cd "$V/cc" && "$COMP/ctc.sh" main.ct > "$T/cc.out" 2>&1)
if grep -q "E5030" "$T/cc.out"; then
    ok "同符号跨版本碰撞拦截(E5030,显式收敛口径)"
else
    bad "碰撞负例异常: $(head -1 "$T/cc.out")"
fi
cp -r "$V/ab/." "$V/ct/deps/mybase.ctart/" && cp -r "$V/ap/." "$V/ct/deps/mypkg.ctart/" && cp "$ROOT/tests/artifact_demo/consumer_trans/main.ct" "$V/ct/"
(cd "$V/ct" && "$COMP/ctc.sh" main.ct > "$T/ct.out" 2>&1)
if grep -q "t=12" "$T/ct.out"; then
    ok "传递依赖(calc→mybase 经消费端 deps 扁平解析)"
else
    bad "传递依赖异常: $(head -1 "$T/ct.out")"
fi
cp "$ROOT/tests/artifact_demo/consumer_trans/main.ct" "$V/ctn/"
(cd "$V/ctn" && "$COMP/ctc.sh" main.ct > "$T/ctn.out" 2>&1)
if grep -q "E2020" "$T/ctn.out"; then
    ok "缺传递依赖负例拦截(E2020;S4 改进:错误应点名 deps/<pkg>.ctart 缺失)"
else
    bad "缺传递依赖负例异常: $(head -1 "$T/ctn.out")"
fi

echo "== 3n) S2a 真实依赖图(共享传递去重 + 三层链 + 双版本改名共存)=="
R="$T/realdep"
for d in ac af1 af2 an ai; do mkdir -p "$R/$d/impl"; done
mkdir -p "$R/app/deps/codecore.ctart" "$R/app/deps/fmtkit.ctart" "$R/app/deps/fmtkit2.ctart" "$R/app/deps/netlite.ctart" "$R/app/deps/invoiceapi.ctart"
"$COMP/ctc.sh" ast "$ROOT/tests/realdep_demo/pkgs/codecore/codecore.ct" --ast=seal --astout="$R/ac" --astname=codecore > "$T/r1.out" 2>&1
"$COMP/ctc.sh" ast "$ROOT/tests/realdep_demo/pkgs/fmtkit1/fmtkit.ct" --ast=seal --astout="$R/af1" --astname=fmtkit > "$T/r2.out" 2>&1
"$COMP/ctc.sh" ast "$ROOT/tests/realdep_demo/pkgs/fmtkit2/fmtkit.ct" --ast=seal --astout="$R/af2" --astname=fmtkit2 > "$T/r3.out" 2>&1
"$COMP/ctc.sh" ast "$ROOT/tests/realdep_demo/pkgs/netlite/netlite.ct" --ast=seal --astout="$R/an" --astname=netlite > "$T/r4.out" 2>&1
"$COMP/ctc.sh" ast "$ROOT/tests/realdep_demo/pkgs/invoiceapi/invoiceapi.ct" --ast=seal --astout="$R/ai" --astname=invoiceapi > "$T/r5.out" 2>&1
cp -r "$R/ac/." "$R/app/deps/codecore.ctart/" && cp -r "$R/af1/." "$R/app/deps/fmtkit.ctart/" && cp -r "$R/af2/." "$R/app/deps/fmtkit2.ctart/" && cp -r "$R/an/." "$R/app/deps/netlite.ctart/" && cp -r "$R/ai/." "$R/app/deps/invoiceapi.ctart/" && cp "$ROOT/tests/realdep_demo/app/main.ct" "$R/app/"
(cd "$R/app" && CTRON_STDPATH="$ROOT/std" "$COMP/ctc.sh" main.ct > "$T/rapp.out" 2>&1)
if grep -q "legacy=1,234,567" "$T/rapp.out" && grep -q "total=CNY 1,234,567" "$T/rapp.out" && grep -q "paid=75%" "$T/rapp.out" && grep -q "ref8=" "$T/rapp.out" && grep -q "resp=OK" "$T/rapp.out"; then
    ok "真实依赖图五行锚定(菱形去重 + 三层链 + 双版本改名共存)"
else
    bad "真实依赖图异常: $(head -3 "$T/rapp.out")"
fi

echo "== 结果: $pass ok / $fail fail =="
rm -rf "$T"
[ $fail -eq 0 ]
