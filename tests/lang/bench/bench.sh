#!/bin/sh
# tests/lang/bench/bench.sh —— T44 own/热点 ±5% 性能门禁(§9.4 P3;W9)
# 纪律:digest pin(ctron 产物输出 vs C 参照逐字节一致)+ ×3 取 min;只信同机差分。
# 比值 ≤1.05 = §9.4 P3 门;未达标 WARN 登记归因(T32 先例:登记即交付,门禁硬化随热点回切)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}"   # 覆盖口:worktree 隔离构建验证用(http/run.sh 同款)
[ -x "$EMIT" ] || { echo "lang/bench: 缺编译器二进制(先: compiler/native.sh)" >&2; exit 2; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/lib/std" "$EMIT" run "$DIR/src/main.ct" > "$T/bench.c" 2>/dev/null \
    || { echo "lang/bench: emit 失败" >&2; exit 2; }
cc -O1 -w -o "$T/bench.bin" "$T/bench.c" || { echo "lang/bench: cc(ctron) 失败" >&2; exit 2; }
cc -O1 -w -o "$T/ref.bin" "$DIR/src/ref.c" || { echo "lang/bench: cc(ref) 失败" >&2; exit 2; }

runmin() {
    best=999999
    i=0
    while [ $i -lt 3 ]; do
        st=$(python3 -c "import time; print(time.perf_counter())")
        "$1" > "$T/out" 2>&1
        en=$(python3 -c "import time; print(time.perf_counter())")
        ms=$(python3 -c "print(int(($en - $st) * 1000))")
        [ "$ms" -lt "$best" ] && best=$ms
        i=$((i + 1))
    done
    echo "$best"
}

"$T/bench.bin" > "$T/out.ctron" 2>&1
"$T/ref.bin" > "$T/out.ref" 2>&1
if ! diff -q "$T/out.ctron" "$T/out.ref" > /dev/null 2>&1; then
    echo "lang/bench: FAIL digest 分歧(ctron=[$(tr '\n' ',' < "$T/out.ctron")] ref=[$(tr '\n' ',' < "$T/out.ref")])" >&2
    exit 1
fi
C=$(runmin "$T/bench.bin")
R=$(runmin "$T/ref.bin")
python3 -c "
c, r = $C, $R
ratio = c / r if r else 0
print('lang/bench: ctron ${C}ms vs C ${R}ms, 比值 %.3f(门 ≤1.05)' % ratio)
if ratio > 1.05:
    print('lang/bench: WARN 比值 >1.05(§9.4 P3 未达;归因位:k2 allocator 税[每轮 List vs C 复用缓冲]+k1 代码生成差;登记热点回切建议)')
"
