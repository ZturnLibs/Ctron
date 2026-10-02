#!/bin/sh
# tests/gc/run.sh —— GC 档验收(§6.2/T29):泄漏锚(循环回收)双档
# gc=on:CTRON_GC=1,断言 live bytes 回落;gc=off:bump 档行为面跑通
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "gc/run: 缺编译器二进制(先: compiler/native.sh)" >&2; exit 2; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
for d in "$DIR"/*/; do
    [ -f "$d/src/main.ct" ] || continue
    name=$(basename "$d")
    [ "$name" = "bench" ] && continue
    CTRON_STDPATH="$ROOT/std" "$EMIT" run "$d/src/main.ct" > "$T/$name.c" 2>"$T/$name.err" \
        || { echo "  FAIL $name (emit)"; fail=$((fail+1)); continue; }
    cc -O1 -w -o "$T/$name.bin" "$T/$name.c" || { echo "  FAIL $name (cc)"; fail=$((fail+1)); continue; }
    if CTRON_GC=1 "$T/$name.bin" run "$d/src/main.ct" > "$T/$name.on" 2>&1 && grep -q "cycle reclaim ok\|bump 档跑通\|deep stack ok\|precise root ok" "$T/$name.on"; then
        pass=$((pass+1)); echo "  PASS $name (gc=on)"
    else
        echo "  FAIL $name (gc=on): $(tail -1 "$T/$name.on")"; fail=$((fail+1))
    fi
    if CTRON_GC=off "$T/$name.bin" run "$d/src/main.ct" > "$T/$name.off" 2>&1 && grep -q "bump 档跑通\|deep stack ok\|precise root ok" "$T/$name.off"; then
        pass=$((pass+1)); echo "  PASS $name (gc=off)"
    else
        echo "  FAIL $name (gc=off): $(tail -1 "$T/$name.off")"; fail=$((fail+1))
    fi
done
echo "gc/run: pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
