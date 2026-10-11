#!/bin/sh
# tests/thread/run.sh —— G3 线程定型门族(scope join-all / 超时 / 取消 / 池;
# W2+ 夹具随波增补)。pthread 默认形态 + CTRON_RT=coro 双臂。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$DIR/../.." && pwd)
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "thread: 缺编译器二进制(先: compiler/build.sh && compiler/native.sh)" >&2; exit 2; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0

for d in "$DIR"/*/; do
    [ -f "$d/src/main.ct" ] || continue
    name=$(basename "$d")
    CTRON_STDPATH="$ROOT/lib" "$EMIT" run "$d/src/main.ct" > "$T/$name.c" 2>"$T/$name.err" \
        || { echo "  FAIL $name (emit): $(tail -1 "$T/$name.err")"; fail=$((fail+1)); continue; }
    cc -O1 -w -pthread -o "$T/$name.bin" "$T/$name.c" || { echo "  FAIL $name (cc)"; fail=$((fail+1)); continue; }
    for arm in "CTRON_GC=off" "CTRON_RT=coro"; do
        if env $arm "$T/$name.bin" > "$T/$name.out" 2>&1 && grep -qE "$name-probe .+" "$T/$name.out"; then
            pass=$((pass+1)); echo "  PASS $name ($arm)"
        else
            echo "  FAIL $name ($arm): $(tail -1 "$T/$name.out")"
            fail=$((fail+1))
        fi
    done
done

echo "thread: pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
