#!/bin/sh
# tests/gc/seg/run.sh —— G1 探针一:任务段回收专道
# 断言:三档跑通+判词在 + max RSS 有界(段泄漏即数 GB 越界,响亮红)
# RSS 采样:/usr/bin/time -l(darwin)/-v(linux GNU time);取不到采样器则降级为跑通断言
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$DIR/../../.." && pwd)
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "gc/seg: 缺编译器二进制(先: compiler/build.sh && compiler/native.sh)" >&2; exit 2; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0

CTRON_STDPATH="$ROOT/lib" "$EMIT" run "$DIR/src/main.ct" > "$T/seg.c" 2>"$T/seg.err" \
    || { echo "  FAIL seg (emit): $(tail -1 "$T/seg.err")"; exit 1; }
cc -O1 -w -pthread -o "$T/seg.bin" "$T/seg.c" || { echo "  FAIL seg (cc)"; exit 1; }

envf=""
for env in "CTRON_GC=1" "CTRON_GC=off" "CTRON_GC=1 CTRON_GC_CONSERV=0"; do
    if env $env "$T/seg.bin" > "$T/seg.out" 2>&1 && grep -qE "seg-probe ok acc=[0-9]+" "$T/seg.out"; then
        pass=$((pass+1)); echo "  PASS seg ($env)"
    else
        echo "  FAIL seg ($env): $(tail -1 "$T/seg.out")"; fail=$((fail+1))
    fi
done

rss_mb=0
if [ "$(uname)" = "Darwin" ]; then
    rss_bytes=$(/usr/bin/time -l "$T/seg.bin" 2>&1 >/dev/null | awk '/maximum resident set size/ {print $1}')
    [ -n "${rss_bytes:-}" ] && rss_mb=$((rss_bytes / 1048576))
else
    rss_kb=$(/usr/bin/time -v "$T/seg.bin" 2>&1 >/dev/null | awk '/Maximum resident set size/ {print $NF}')
    [ -n "${rss_kb:-}" ] && rss_mb=$((rss_kb / 1024))
fi
if [ "$rss_mb" -gt 0 ]; then
    if [ "$rss_mb" -lt 1024 ]; then
        pass=$((pass+1)); echo "  PASS seg (rss=${rss_mb}MB < 1024MB 界)"
    else
        echo "  FAIL seg (rss=${rss_mb}MB ≥ 1024MB——任务段泄漏形态)"; fail=$((fail+1))
    fi
else
    echo "  WARN seg (RSS 采样器缺席,降级跑通断言)"
fi

echo "gc/seg: pass=$pass fail=$fail"

# G1 探针二腿:跨堆窗口压测(阈值 4KB 逼主线程高频收集)× 三遍,判词含值语义钉
winfail=0
CTRON_STDPATH="$ROOT/lib" "$EMIT" run "$DIR/src/window.ct" > "$T/win.c" 2>"$T/win.err" \
    || { echo "  FAIL window (emit): $(tail -1 "$T/win.err")"; exit 1; }
cc -O1 -w -pthread -o "$T/win.bin" "$T/win.c" || { echo "  FAIL window (cc)"; exit 1; }
w=0
while [ $w -lt 3 ]; do
    if CTRON_GC=1 CTRON_GC_THRESHOLD=4096 "$T/win.bin" > "$T/win.out" 2>&1 && grep -qE "window-probe acc=340000" "$T/win.out"; then
        echo "  PASS window run=$w"
    else
        echo "  FAIL window run=$w: $(tail -1 "$T/win.out")"
        winfail=$((winfail+1))
    fi
    w=$((w+1))
done
if [ "$winfail" -gt 0 ]; then fail=$((fail+winfail)); fi

[ "$fail" -eq 0 ]
