#!/bin/sh
# bench.sh —— compiler/ 模块化自举编译器性能基线
#
# 四阶段:
#   S1 微基准三路(fib/loop/str/list):seed 解释 / native 解释 / 原生代码生成
#   S2 编译器前端:check 自编译面(decls=243)——seed 解释 vs 全深度(native 解释检查驱动)
#   S3 编译器后端:发射 cc_run.ct 完整 C——seed 发射 vs native 发射(逐字节 diff)
#   S4 黄金解释:seed 解释 cc_run 跑 input_cc vs native ctron-cc 同源
#
# 用法: ./bench.sh
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$DIR")
SEED="$ROOT/compiler-c/build/ctronc"
NC="$DIR/bin/ctron-cc"
NE="$DIR/bin/ctron-emit"
T=$(mktemp -d /tmp/ctron_cbench.XXXXXX)
trap 'rm -rf "$T"' EXIT

cat > "$T/timeit.py" <<'PYEOF'
import subprocess, sys, time
out, to, cmd = sys.argv[1], float(sys.argv[2]), sys.argv[3:]
t = time.time()
try:
    with open(out, 'w') as f:
        r = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, timeout=to)
    rc = r.returncode
except subprocess.TimeoutExpired:
    rc = 124
print(f"{rc} {time.time()-t:.3f}")
PYEOF

run_timed() {
    local out=$1; shift
    local to=$1; shift
    local res
    res=$(python3 "$T/timeit.py" "$out" "$to" "$@")
    RC=${res%% *}
    RT=${res#* }
}

min3() {
    local out=$1; shift
    local to=$1; shift
    local best=9999
    local r=0
    while [ $r -lt 3 ]; do
        run_timed "$out" "$to" "$@"
        if perl -e "exit(($RT < $best) ? 0 : 1)"; then best=$RT; fi
        r=$((r+1))
    done
    RT=$best
}

[ -x "$SEED" ] || { echo "bench: 缺少种子 $SEED" >&2; exit 2; }
[ -x "$NC" ] || { echo "bench: 缺少 $NC(先 ./native.sh)" >&2; exit 2; }
[ -x "$NE" ] || { echo "bench: 缺少 $NE(先 ./native.sh)" >&2; exit 2; }
"$DIR/build.sh" >/dev/null

echo "== S1) 微基准三路对比(预热后 3 取最小) =="
printf "%-8s %12s %14s %16s %10s\n" "夹具" "seed解释" "native解释" "原生代码生成" "正确性"
for fx in fib loop str list; do
    src="$ROOT/selfhosted/bench/$fx.ct"
    min3 "$T/b_${fx}.seed" 600 "$SEED" run "$src"
    ts=$RT
    min3 "$T/b_${fx}.nc" 600 "$NC" run "$src"
    tn=$RT
    min3 "$T/b_${fx}_em.c" 600 "$NE" run "$src"
    te=$RT
    cc -O1 -w -o "$T/g_$fx.bin" "$T/b_${fx}_em.c" 2>/dev/null
    min3 "$T/b_${fx}.gen" 60 "$T/g_$fx.bin"
    tg=$RT
    ok="✓"
    if ! diff -q "$T/b_${fx}.seed" "$T/b_${fx}.nc" >/dev/null 2>&1 || \
       ! diff -q "$T/b_${fx}.seed" "$T/b_${fx}.gen" >/dev/null 2>&1; then
        ok="✗分歧"
    fi
    printf "%-8s %10ss %12ss %14ss %10s\n" "$fx" "$ts" "$tn" "$tg" "$ok"
done

echo ""
echo "== S2) 编译器前端:check 自编译面(cc_run.ct,decls 锁定) =="
run_timed "$T/fe_seed.out" 900 "$DIR/ctc.sh" check "$DIR/build/cc_run.ct"
t1=$RT; r1=$(grep -o 'decls=[0-9]*' "$T/fe_seed.out" | tail -1)
sed -e "s|ANCHORINPUT|$DIR/build/cc_run.ct|" -e "s|ANCHORFMT|0|" -e "s|ANCHORPROFILE|full|" "$DIR/build/cc_check.ct" > "$T/chk_native.ct"
run_timed "$T/fe_nc.out" 900 "$NC" run "$T/chk_native.ct"
t2=$RT; r2=$(grep -o 'decls=[0-9]*' "$T/fe_nc.out" | tail -1)
perl -e "printf 'seed 解释检查驱动: %.2fs (%s)   全深度(native 解释检查驱动): %.2fs (%s)   全深度/种子 = %.2f', $t1, '$r1', $t2, '$r2', $t2/$t1" && echo ""

echo ""
echo "== S3) 编译器后端:发射 cc_run.ct 的完整 C =="
run_timed "$T/em_seed.log" 900 "$DIR/ctc.sh" emit "$DIR/build/cc_run.ct" "$T/em_seed.c"
t1=$RT; s1=$RC
run_timed "$T/em_nc.c" 900 "$NE" run "$DIR/build/cc_run.ct"
t2=$RT; s2=$RC
l1=$(wc -l < "$T/em_seed.c" | tr -d ' '); l2=$(wc -l < "$T/em_nc.c" | tr -d ' ')
perl -e "printf 'seed 发射: %.2fs rc=$s1(%s 行)   native 发射: %.2fs rc=$s2(%s 行)   原生/种子 = %.2f', $t1, '$l1', $t2, '$l2', $t2/$t1" && echo ""
diff -q "$T/em_seed.c" "$T/em_nc.c" >/dev/null 2>&1 && echo "两形态发射产物逐字节一致 ✓" || echo "两形态发射产物存在差异"

echo ""
echo "== S4) 黄金解释:seed 解释 cc_run 跑 input_cc vs native ctron-cc 同源 =="
# 输入锚为中性 ANCHORINPUT:seed 面须先 sed 换靶(无 CWD 依赖,不再有假分歧 footgun)
sed "s|ANCHORINPUT|$ROOT/selfhosted/input_cc.ct|" "$DIR/build/cc_run.ct" > "$T/cc_run_seed.ct"
run_timed "$T/g_seed.out" 900 sh -c "\"$SEED\" run \"$T/cc_run_seed.ct\""
t1=$RT
run_timed "$T/g_nc.out" 900 "$NC" run "$ROOT/selfhosted/input_cc.ct"
t2=$RT
perl -e "printf 'seed: %.2fs   native: %.2fs   原生/种子 = %.2f', $t1, $t2, $t2/$t1" && echo ""
diff -q "$T/g_seed.out" "$T/g_nc.out" >/dev/null 2>&1 && echo "双形态输出一致 ✓" || echo "双形态输出分歧"

echo ""
echo "== 汇总 =="
echo "native 二进制: $(wc -c < "$NC" | tr -d ' ') 字节 / $(wc -c < "$NE" | tr -d ' ') 字节"
