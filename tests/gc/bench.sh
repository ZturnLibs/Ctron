#!/bin/sh
# tests/gc/bench.sh —— T32 GC 性能门禁(§9.4):GC 档 vs bump 档同机差分
# 纪律:digest pin(两档输出逐字一致)+ ×3 取 min;比值 ≤1.15=§9.4 目标,
#       未达标打印 WARN 登记归因(首版允许,门禁硬化随 GC 调优批次)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "gc/bench: 缺编译器二进制(先: compiler/native.sh)" >&2; exit 2; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
FX="$DIR/bench/src/main.ct"
CTRON_STDPATH="$ROOT/lib/std" "$EMIT" run "$FX" > "$T/bench.c" 2>/dev/null || { echo "gc/bench: emit 失败" >&2; exit 2; }
cc -O1 -w -o "$T/bench.bin" "$T/bench.c" || { echo "gc/bench: cc 失败" >&2; exit 2; }

runmin() {
    local mode="$1" best=999999
    for i in 1 2 3; do
        local st=$(python3 -c "import time; print(time.perf_counter())")
        if [ "$mode" = "gc" ]; then
            CTRON_GC=1 "$T/bench.bin" > "$T/out.$mode" 2>&1
        else
            CTRON_GC=off "$T/bench.bin" > "$T/out.$mode" 2>&1
        fi
        local en=$(python3 -c "import time; print(time.perf_counter())")
        local ms=$(python3 -c "print(int(($en - $st) * 1000))")
        [ "$ms" -lt "$best" ] && best=$ms
    done
    echo "$best"
}

G=$(runmin gc)
B=$(runmin bump)
if ! diff -q "$T/out.gc" "$T/out.bump" > /dev/null 2>&1; then
    echo "gc/bench: FAIL digest 分歧(gc=[$(cat "$T/out.gc")] bump=[$(cat "$T/out.bump")])" >&2
    exit 1
fi
python3 -c "
g, b = $G, $B
r = g / b if b else 0
print('gc/bench: GC 档 ${G}ms vs bump 档 ${B}ms, 比值 %.3f' % r)
if r > 1.15:
    print('gc/bench: WARN 比值 >1.15(§9.4 目标未达;登记归因,门禁硬化随 GC 调优批次)')
"
