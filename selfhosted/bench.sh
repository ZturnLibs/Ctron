#!/bin/sh
# bench.sh —— Ctron 自举版本性能基准
#
# 三种执行形态对比:
#   seed    = C 宿主 ctronc(树遍历解释器,C 实现)
#   native  = 原生自举 cc(Ctron 写的解释器,经 Ctron 写的代码生成 + gcc 编译)
#   codegen = 原生代码生成(Ctron 写的发射器发射 C → gcc → 机器码直跑)
#
# 用法: ./bench.sh [--quick]   (--quick 跳过 S3/S4 重型阶段)
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$DIR")
SEED="$ROOT/compiler_c/build/ctronc"
T=$(mktemp -d /tmp/ctron_bench.XXXXXX)
trap 'rm -rf "$T"' EXIT

# 精确计时器:python 内计时子进程,无解释器启动噪声;输出 "rc 秒"
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

# run_timed <outfile> <timeout秒> <cmd...> → RC/RT 全局
run_timed() {
    local out=$1; shift
    local to=$1; shift
    local res
    res=$(python3 "$T/timeit.py" "$out" "$to" "$@")
    RC=${res%% *}
    RT=${res#* }
}

[ -x "$SEED" ] || { echo "bench: 缺少种子 $SEED" >&2; exit 2; }

echo "== 阶段 0:构建原生自举 cc(arena) =="
( cd "$DIR" && python3 tools/genmod.py "$DIR/cc.ct" "$T/boot.ct" --trans ) || exit 2
run_timed "$T/boot.c" 300 "$SEED" run "$T/boot.ct"
echo "种子发射 cc.ct: ${RT}s rc=$RC"
cc -O1 -w -o "$T/nc.bin" "$T/boot.c" || exit 2
echo "原生 cc 就绪($(wc -c < "$T/boot.c" | tr -d ' ') 字节 C / $(wc -l < "$T/boot.c" | tr -d ' ') 行)"

echo ""
echo "== S1) 算法夹具三路对比(递归/循环/字符串/List;预热后 3 取最小) =="
printf "%-8s %12s %14s %16s %10s\n" "夹具" "seed解释" "native解释" "原生代码生成" "正确性"
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
for fx in fib loop str list; do
    src="$DIR/bench/$fx.ct"
    min3 "$T/b_${fx}.seed" 600 "$SEED" run "$src"
    ts=$RT
    min3 "$T/b_${fx}.nc" 600 "$T/nc.bin" run "$src"
    tn=$RT
    ( cd "$DIR" && python3 tools/genmod.py "$src" "$T/em_$fx.ct" --trans )
    run_timed "$T/g_$fx.c" 300 "$T/nc.bin" run "$T/em_$fx.ct"
    te=$RT
    cc -O1 -w -o "$T/g_$fx.bin" "$T/g_$fx.c" 2>/dev/null
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
echo "== S2) 编译器前端:parse+sem cc.ct(175 decls)计数 =="
( cd "$DIR" && python3 tools/genmod.py "$DIR/cc.ct" "$T/dm_cc.ct" --count )
run_timed "$T/fe_seed.out" 600 "$SEED" run "$T/dm_cc.ct"
t1=$RT; r1=$(grep -o 'decls=[0-9]*' "$T/fe_seed.out" | tail -1)
run_timed "$T/fe_nc.out" 600 "$T/nc.bin" run "$T/dm_cc.ct"
t2=$RT; r2=$(grep -o 'decls=[0-9]*' "$T/fe_nc.out" | tail -1)
perl -e "printf 'seed: %.2fs (%s)   native: %.2fs (%s)   原生/种子 = %.2f', $t1, '$r1', $t2, '$r2', $t2/$t1" && echo ""

echo ""
echo "== S3) 编译器后端:发射 cc.ct 的完整 C =="
run_timed "$T/em_seed.c" 900 "$SEED" run "$T/boot.ct"
t1=$RT; s1=$RC
run_timed "$T/em_nc.c" 900 "$T/nc.bin" run "$T/boot.ct"
t2=$RT; s2=$RC
l1=$(wc -l < "$T/em_seed.c" | tr -d ' '); l2=$(wc -l < "$T/em_nc.c" | tr -d ' ')
perl -e "printf 'seed 发射: %.2fs rc=$s1(%s 行)   native 发射: %.2fs rc=$s2(%s 行)   原生/种子 = %.2f', $t1, '$l1', $t2, '$l2', $t2/$t1" && echo ""
diff -q "$T/em_seed.c" "$T/em_nc.c" >/dev/null 2>&1 && echo "两形态发射产物逐字节一致 ✓" || echo "两形态发射产物存在差异"

echo ""
echo "== S4) 全深度自译化:cc×cc 解释 input_cc =="
( cd "$DIR" && python3 tools/genmod.py "$DIR/cc.ct" "$T/self.ct" )
run_timed "$T/st_seed.out" 900 "$SEED" run "$T/self.ct"
t1=$RT
run_timed "$T/st_nc.out" 900 "$T/nc.bin" run "$T/self.ct"
t2=$RT
perl -e "printf 'seed: %.2fs   native: %.2fs   原生/种子 = %.2f', $t1, $t2, $t2/$t1" && echo ""
diff -q "$T/st_seed.out" "$T/st_nc.out" >/dev/null 2>&1 && echo "双形态输出一致 ✓" || echo "双形态输出分歧(检查 seed 臂是否被系统内存管理终止)"

echo ""
echo "== 汇总 =="
echo "原生 cc 二进制: $(wc -c < "$T/nc.bin" | tr -d ' ') 字节;发射产物: $(wc -l < "$T/boot.c" | tr -d ' ') 行 C"
