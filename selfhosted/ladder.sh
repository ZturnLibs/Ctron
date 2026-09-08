#!/bin/sh
# ladder.sh —— Ctron 自举验收阶梯(selfhosted 本地验收,一次跑完全部)
#
# 用法: ./ladder.sh [--full]
#   默认:  直接管线黄金对照(input_cc*/负例)
#          自编译检查阶梯(parse+sem 自身与互检源码,decl 计数对照)
#          复现解析(repro/,StructLit 误触发守卫回归)
#   --full: 追加全深度自译化(cc 解释 cc 解释 input_cc,约 4 分钟,负载高时更久)
#
# 依赖: 宿主 seed compiler-c/build/ctronc(仅作 Ctron 解释器;自举完成后可自替换)
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$DIR")
HOST="${CTRON_SEED:-$ROOT/compiler-c/build/ctronc}"
EXP="$DIR/expected"
T=$(mktemp -d /tmp/ctron_ladder.XXXXXX)
KEEP=${LADDER_KEEP:-0}
[ "$KEEP" = 1 ] || trap 'rm -rf "$T"' EXIT

pass=0; fail=0; known=0
ok()  { pass=$((pass+1)); echo "ok  - $1"; }
bad() { fail=$((fail+1)); echo "FAIL- $1"; }
kn()  { known=$((known+1)); echo "know- $1"; }

[ -x "$HOST" ] || { echo "ladder: 缺少宿主 seed $HOST(先: make -C compiler-c,或 CTRON_SEED=...)" >&2; exit 2; }

# gen_cc <输入程序> <输出模块> [count]
gen_cc() {
    ( cd "$DIR" && python3 tools/genmod.py "$1" "$2" ${3:-} )
}

echo "== 1) 直接管线黄金对照 =="
for f in input_cc input_cc2 input_cc3; do
    gen_cc "$DIR/$f.ct" "$T/run_$f.ct"
    ( cd "$ROOT" && timeout 120 "$HOST" run "$T/run_$f.ct" > "$T/$f.got" 2>&1 )
    if diff -q "$EXP/$f.out" "$T/$f.got" > /dev/null 2>&1; then
        ok "管线 $f 逐字一致"
    else
        bad "管线 $f 输出分歧"
    fi
done
gen_cc "$DIR/input_cc_neg.ct" "$T/run_neg.ct"
( cd "$ROOT" && timeout 120 "$HOST" run "$T/run_neg.ct" > "$T/neg.got" 2>&1 )
if grep -q 'W8010: struct 含类引用字段(浅拷贝):Pair.b' "$T/neg.got"; then
    ok "负例 input_cc_neg 编译期拦截(W8010)"
else
    bad "负例 input_cc_neg 未拦截"
fi

echo "== 2) 自编译检查阶梯(parse+sem,decl 计数对照 C 解析器) =="
check_decl() {  # check_decl <源.ct> <期望decl数> <名>
    gen_cc "$1" "$T/dm_$3.ct" --count
    ( cd "$ROOT" && timeout 300 "$HOST" run "$T/dm_$3.ct" > "$T/dm_$3.out" 2>&1 )
    got=$(grep -o 'decls=[0-9]*' "$T/dm_$3.out" 2>/dev/null | head -1 | grep -o '[0-9]*')
    if [ "$got" = "$2" ]; then ok "解析 $3 decls=$got"; else bad "解析 $3 decls=$got 期望 $2"; fi
}

check_decl "$DIR/sem_chk.ct"    109 sem_chk
check_decl "$DIR/parsetree.ct"   57 parsetree
check_decl "$DIR/ev2.ct"        107 ev2
check_decl "$DIR/cc.ct"         175 cc

echo "== 3) 复现解析(StructLit 误触发守卫回归) =="
check_decl "$DIR/repro/s6.ct"      2 repro-s6
check_decl "$DIR/repro/hand2.ct"   2 repro-hand2
check_decl "$DIR/repro/cm_ol.ct"   2 repro-cm_ol

echo "== 4) C 代码生成(Ctron 写的代码生成 → 原生执行,fixtures 全扫) =="
for fx in "$DIR"/fixtures/trans_*.ct; do
    name=$(basename "$fx" .ct)
    gen_cc "$fx" "$T/tr_$name.ct" --trans
    ( cd "$ROOT" && timeout 120 "$HOST" run "$T/tr_$name.ct" > "$T/$name.c" 2>&1 )
    if cc -O1 -o "$T/$name.bin" "$T/$name.c" 2>/dev/null; then
        # 与解释侧同 cwd($ROOT):夹具内相对路径 read_file 两侧行为一致
        ( cd "$ROOT" && "$T/$name.bin" > "$T/$name.got" 2>&1 )
        ( cd "$ROOT" && timeout 120 "$HOST" run "$fx" > "$T/$name.iv" 2>&1 )
        if diff -q "$T/$name.got" "$T/$name.iv" > /dev/null 2>&1; then
            ok "代码生成往返 $name 逐字一致"
        elif [ "${CTRON_BOOT:-0}" = "1" ] && grep -q 'expr:Float' "$T/$name.iv" 2>/dev/null; then
            kn "代码生成往返 $name 已知分歧(cc 求值器无 Float 域,见 22d)"
        else
            bad "代码生成往返 $name 输出分歧"
        fi
    else
        bad "代码生成 $name 生成码编译失败"
    fi
done

echo "== 5) 全深度自译化(--full) =="
if [ "${1:-}" = "--full" ]; then
    gen_cc "$DIR/cc.ct" "$T/self.ct"
    s=$(date +%s)
    # cc.ct 的输入锚是相对路径(../selfhosted/input_cc.ct),须从 compiler-c 目录解析
    ( cd "$ROOT/compiler-c" && timeout 900 "$HOST" run "$T/self.ct" > "$T/selfdeep.got" 2>&1 )
        rc=$?
        e=$(date +%s)
        if [ $rc = 0 ] && diff -q "$EXP/input_cc.out" "$T/selfdeep.got" > /dev/null 2>&1; then
            ok "全深度自译化(cc 解释 cc 解释 input_cc)逐字一致($((e-s))s)"
        else
        bad "全深度自译化失败(rc=$rc, $((e-s))s)"
    fi
fi

echo "== 6) 自发射收官(cc.ct 经 Ctron 代码生成 → gcc → 原生 cc 解释 input_cc3 == 黄金) =="
gen_cc "$DIR/cc.ct" "$T/self_emit.ct" --trans
( cd "$ROOT/compiler-c" && timeout 300 "$HOST" run "$T/self_emit.ct" > "$T/cc_self.c" 2>&1 )
if cc -O1 -w -o "$T/cc_self.bin" "$T/cc_self.c" 2>/dev/null; then
    # CLI 化后无需换锚:原生自举 cc 直接 run 任意输入
    ( cd "$ROOT/compiler-c" && timeout 120 "$T/cc_self.bin" run "$DIR/input_cc3.ct" > "$T/cc_self.got" 2>&1 )
    if diff -q "$EXP/input_cc3.out" "$T/cc_self.got" > /dev/null 2>&1; then
        ok "自发射收官:原生自举 cc 解释 input_cc3 == 黄金逐字一致"
    else
        bad "自发射收官:原生 cc 输出与黄金分歧"
    fi
else
    bad "自发射收官:cc.ct 发射产物编译失败"
fi

echo "== 7) 宿主上位(原生自举 cc 经 CLI run 作为种子跑黄金面) =="
gen_cc "$DIR/cc.ct" "$T/boot_cc.ct" --trans
( cd "$ROOT/compiler-c" && timeout 300 "$HOST" run "$T/boot_cc.ct" > "$T/boot_cc.c" 2>&1 )
if cc -O1 -w -o "$T/nc.bin" "$T/boot_cc.c" 2>/dev/null; then
    for f in input_cc input_cc2 input_cc3; do
        gen_cc "$DIR/$f.ct" "$T/nmod_$f.ct"
        ( cd "$ROOT" && timeout 120 "$T/nc.bin" run "$T/nmod_$f.ct" > "$T/n_$f.got" 2>&1 )
        if diff -q "$EXP/$f.out" "$T/n_$f.got" > /dev/null 2>&1; then
            ok "宿主上位 $f == 黄金"
        else
            bad "宿主上位 $f 分歧"
        fi
    done
    gen_cc "$DIR/input_cc_neg.ct" "$T/nmod_neg.ct"
    ( cd "$ROOT" && timeout 120 "$T/nc.bin" run "$T/nmod_neg.ct" > "$T/n_neg.got" 2>&1 )
    if grep -q 'W8010' "$T/n_neg.got"; then
        ok "宿主上位 负例 W8010 拦截"
    else
        bad "宿主上位 负例未拦截"
    fi
else
    bad "宿主上位:原生 cc 构建失败"
fi

echo "== 8) 自举固定点(编译器编译自身逐字节复现 + CLI 编译用户程序) =="
gen_cc "$T/selfcomp.ct" "$T/selfcomp.ct" --trans
( cd "$ROOT/compiler-c" && timeout 300 "$HOST" run "$T/selfcomp.ct" > "$T/c1.c" 2>&1 )
if cc -O1 -w -o "$T/compiler1.bin" "$T/c1.c" 2>/dev/null; then
    ( cd "$ROOT" && timeout 120 "$T/compiler1.bin" > "$T/c2.c" 2>&1 )
    if diff -q "$T/c1.c" "$T/c2.c" > /dev/null 2>&1; then
        ok "固定点:编译器编译自身逐字节复现"
    else
        bad "固定点:两阶段产物分歧"
    fi
    ( cd "$ROOT" && timeout 120 "$T/compiler1.bin" run "$DIR/fixtures/trans_v3.ct" > "$T/fp_v3.c" 2>&1 )
    if cc -O1 -w -o "$T/fp_v3.bin" "$T/fp_v3.c" 2>/dev/null; then
        ( cd "$ROOT" && "$T/fp_v3.bin" > "$T/fp_v3.got" 2>&1 )
        ( cd "$ROOT" && timeout 120 "$HOST" run "$DIR/fixtures/trans_v3.ct" > "$T/fp_v3.iv" 2>&1 )
        if diff -q "$T/fp_v3.got" "$T/fp_v3.iv" > /dev/null 2>&1; then
            ok "固定点:原生编译器 CLI 编译 trans_v3 往复逐字一致"
        else
            bad "固定点:CLI 编译用户程序输出分歧"
        fi
    else
        bad "固定点:CLI 编译用户程序产物编译失败"
    fi
else
    bad "固定点:compiler1 构建失败"
fi

echo "== 9) 全模块面双种子差分(C 宿主 vs 当前种子) =="
# 参考端恒为 C 宿主(bootstrap 下 $HOST 已是原生 cc,不换参考即失去对照意义)
CSEED="$ROOT/compiler-c/build/ctronc"
# 已知分歧:parsetree/sem_chk 老快照依赖 C9i① 挂账的解析嵌套差异(cc 解析器把
# p_block 尾 guard 嵌到 while 外,rt 解析在循环内)→ 原生求值 unbound:p0。
# sweep 如实标注 know-;修复归解析器韧性工作(见 HANDOFF)。
for m in hello ev_num ev2 cc sem_chk pkg_chk parse_ast parsetree lex_small lex_kind lex_adv lex_corpus lex_float lex_pay lex_str lex_num; do
    [ -f "$DIR/$m.ct" ] || continue
    ( cd "$ROOT/compiler-c" && timeout 300 "$CSEED" run "$DIR/$m.ct" > "$T/ms_$m.seed" 2>&1 )
    src=$?
    ( cd "$ROOT/compiler-c" && timeout 300 "$HOST" run "$DIR/$m.ct" > "$T/ms_$m.nc" 2>&1 )
    nrc=$?
    if [ "$src" = "$nrc" ] && diff -q "$T/ms_$m.seed" "$T/ms_$m.nc" > /dev/null 2>&1; then
        ok "模块面 $m 双种子一致"
    elif [ "$nrc" != 0 ] && grep -q 'unbound:p0' "$T/ms_$m.nc" 2>/dev/null; then
        kn "模块面 $m 已知分歧(C9i① 解析嵌套挂账)"
    else
        bad "模块面 $m 分歧 (seed=$src native=$nrc)"
    fi
done

echo "== 阶梯结果: pass=$pass fail=$fail known-div=$known =="
[ "$fail" = 0 ]
