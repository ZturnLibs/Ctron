#!/bin/sh
# tests/gc/p99/run.sh —— G1-T6 P99/RSS 双柱证据采集(GC=off vs GC=1,各三遍)
# 门=六遍判词全在+rc0;RSS/P99 数值=证据呈报(非硬门),写 stdout 供 COVERAGE 抄录
# RSS 曲线=外壳 ps 采样(0.5s 步进),max 峰值随判词行输出
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$DIR/../../.." && pwd)
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "gc/p99: 缺编译器二进制(先: compiler/build.sh && compiler/native.sh)" >&2; exit 2; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0

CTRON_STDPATH="$ROOT/lib" "$EMIT" run "$DIR/src/main.ct" > "$T/p99.c" 2>"$T/p99.err" \
    || { echo "  FAIL p99 (emit): $(tail -1 "$T/p99.err")"; exit 1; }
cc -O1 -w -pthread -o "$T/p99.bin" "$T/p99.c" || { echo "  FAIL p99 (cc)"; exit 1; }

for col in off on; do
    for run in 1 2 3; do
        if [ "$col" = "on" ]; then export CTRON_GC=1 CTRON_GC_THRESHOLD=2097152; else export CTRON_GC=off; fi
        "$T/p99.bin" > "$T/out.$col.$run" 2>&1 &
        pid=$!
        maxrss=0
        while kill -0 "$pid" 2>/dev/null; do
            kb=$(ps -o rss= -p "$pid" 2>/dev/null | tr -d ' ')
            [ -n "$kb" ] && [ "$kb" -gt "$maxrss" ] && maxrss=$kb
            sleep 0.1
        done
        wait "$pid"; rc=$?
        if [ "$rc" -eq 0 ] && grep -qE "^p99-probe w=[0-9]+," "$T/out.$col.$run"; then
            pass=$((pass+1))
            echo "  PASS p99 col=gc-$col run=$run max_rss=$((maxrss / 1024))MB  $(grep '^p99-probe ' "$T/out.$col.$run")"
        else
            echo "  FAIL p99 col=gc-$col run=$run rc=$rc: $(tail -1 "$T/out.$col.$run")"
            fail=$((fail+1))
        fi
    done
done

echo "gc/p99: pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
