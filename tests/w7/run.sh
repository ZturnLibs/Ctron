#!/bin/sh
# tests/w7/run.sh —— W7 并发深水泳道(T34 栈经济)行为锚
# stack_ovf:CTRON_RT_STACK_KB=64 小栈钉死,深递归任务触 guard → 诊断消息 rc=101
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "w7/run: 缺编译器二进制(先: compiler/native.sh)" >&2; exit 2; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
for d in "$DIR"/*/; do
    [ -f "$d/src/main.ct" ] || continue
    name=$(basename "$d")
    CTRON_STDPATH="$ROOT/lib/std" "$EMIT" run "$d/src/main.ct" > "$T/$name.c" 2>"$T/$name.err" \
        || { echo "  FAIL $name (emit)"; fail=$((fail+1)); continue; }
    cc -O1 -w -o "$T/$name.bin" "$T/$name.c" "$ROOT/lib/net/c_src/ctron_net.c" "$ROOT/lib/net/c_src/ctron_rt.c" -pthread \
        || { echo "  FAIL $name (cc)"; fail=$((fail+1)); continue; }
    CTRON_RT=coro CTRON_RT_STACK_KB=64 "$T/$name.bin" run "$d/src/main.ct" > "$T/$name.out" 2>&1
    rc=$?
    if [ $rc -ne 0 ] && grep -q "stack overflow" "$T/$name.out"; then
        pass=$((pass+1)); echo "  PASS $name (触顶诊断 rc=$rc)"
    else
        echo "  FAIL $name (期望触顶诊断; rc=$rc out=$(tail -1 "$T/$name.out"))"; fail=$((fail+1))
    fi
done
echo "w7/run: pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
