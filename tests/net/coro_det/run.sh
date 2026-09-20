#!/bin/sh
# coro_det —— P2-E 确定性调度驱动(CTRON_RT_SEED 同种子逐字节重放)。
# main.ct 本体入 tests/net/run.sh 主环(默认 pthread 面与 CTRON_RT=coro 无
# 种子面各跑一次,仅要求完成——输出序可变,故 main.ct 零顺序断言);本脚本
# 承载种子面:构建一次,CTRON_RT=coro + CTRON_RT_SEED 下
#   1) SEED=42 双跑 diff(PASS 判据:逐字节同);
#   2) SEED=7 / SEED=99 各跑一次,仅要求完成——不同种子允许同序巧合,不作
#      差异断言;
#   3) 无种子 coro 面完成性;
#   4) CI 种子循环:0..99 各双跑 cmp(100 种子;每晚全量 1000 见尾部注释)。
# 契约口径:spawn→join(main 首个 join 开 rt spawn 闸)、纯通道、无 sleep/fd
# ——见 std/net/c_src/ctron_rt.c 头注 P2-E 段。
set -eu
cd "$(dirname "$0")"
ROOT=$(dirname "$(dirname "$(dirname "$(pwd)")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/std"
if [ ! -x "$EMIT" ]; then echo "coro_det: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT

"$EMIT" run src/main.ct > "$T/main.c"
cc -O1 -w -pthread -I"$ROOT/std/net/c_src" -o "$T/coro_det" "$T/main.c" c_src/*.c

run_seed() {                                      # $1=seed  $2=outfile
    CTRON_RT=coro CTRON_RT_SEED="$1" "$T/coro_det" > "$2"
}

echo "== coro_det: SEED=42 同种子双跑重放 =="
run_seed 42 "$T/a"
run_seed 42 "$T/b"
if ! cmp -s "$T/a" "$T/b"; then
    echo "coro_det FAIL: SEED=42 两跑输出不同" >&2
    diff "$T/a" "$T/b" | sed -n '1,10p' >&2
    exit 1
fi

echo "== coro_det: 异种子完成性(7 / 99;同序巧合不断言差异)=="
run_seed 7  "$T/s7"
run_seed 99 "$T/s99"

echo "== coro_det: 无种子 coro 面完成性 =="
CTRON_RT=coro "$T/coro_det" > "$T/noseed"

echo "== coro_det: CI 种子循环 0..99 双跑重放 =="
s=0
while [ "$s" -lt 100 ]; do
    run_seed "$s" "$T/r1"
    run_seed "$s" "$T/r2"
    cmp -s "$T/r1" "$T/r2" || { echo "coro_det FAIL: SEED=$s 重放漂移" >&2; exit 1; }
    s=$((s+1))
done
# 证据计数:同种子双跑 cmp = SEED=42 一对 + 0..99 循环 100 对 = 101 对
echo "== coro_det GREEN(同种子重放 cmp 101/101 全绿:SEED=42 ×1 + 循环 ×100)=="

# nightly 口径(P2-E 验收"1000 种子"):CI 只抽 0..99;全量把上循环上界换
# 1000 即可——
#   s=0; while [ "$s" -lt 1000 ]; do
#       run_seed "$s" "$T/r1"; run_seed "$s" "$T/r2"
#       cmp -s "$T/r1" "$T/r2" || exit 1
#       s=$((s+1))
#   done
# (每种子双跑 ≈ 毫秒级;1000 种子 ≈ 秒级,留夜间回归,不入 CI。)
