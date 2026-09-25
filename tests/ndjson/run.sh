#!/bin/sh
# tests/ndjson/run.sh —— NDJSON 逐行游标验收(P8-C)
# corpus 双臂 + chk 语义门。CTRON_CC/EMIT/CHK 覆盖(worktree 口径)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CC="${CTRON_CC:-$ROOT/compiler/bin/ctron-cc}"
EMIT="${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}"
CHK="${CTRON_CHK:-$ROOT/compiler/bin/ctron-chk}"
export CTRON_STDPATH="$ROOT/std"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/ndjson 逐行游标用例(P8-C)=="

if [ -x "$CHK" ] || [ -n "${CTRON_CHK:-}" ]; then
    "$CHK" run "$ROOT/std/ndjson.ct" > "$T/chk.out" 2>&1 || true
    if grep -qE '^[E][0-9]' "$T/chk.out"; then
        fail=$((fail+1)); echo "  FAIL std/ndjson.ct (sem)"; sed -n '1,3p' "$T/chk.out"
    else
        pass=$((pass+1)); echo "  PASS std/ndjson.ct (sem, chk)"
    fi
fi

if diff -q "$ROOT/std/ndjson.ct" "$ROOT/compiler/test/stdpkg/std/ndjson.ct" > /dev/null 2>&1; then
    pass=$((pass+1)); echo "  PASS std/ndjson.ct 种子副本无漂移"
else
    fail=$((fail+1)); echo "  FAIL std/ndjson.ct 种子副本漂移"
fi

for f in "$DIR"/corpus/*.ct; do
    name=$(basename "$f" .ct)
    if "$CC" run "$f" > "$T/$name.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS $name (interp)"
    else
        fail=$((fail+1)); echo "  FAIL $name (interp)"; sed -n '1,3p' "$T/$name.out"
    fi
    if "$EMIT" run "$f" > "$T/$name.c" 2>/dev/null && cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/$name.bin" "$T/$name.c" "$ROOT/net/c_src/ctron_net.c" 2>/dev/null && "$T/$name.bin" > "$T/$name.run" 2>&1; then
        pass=$((pass+1)); echo "  PASS $name (emit)"
    else
        fail=$((fail+1)); echo "  FAIL $name (emit)"; sed -n '1,3p' "$T/$name.run" 2>/dev/null
    fi
done

echo "ndjson/run: pass=$pass fail=$fail"
[ "$pass" -gt 0 ] || { echo "ndjson/run: no cases ran"; exit 1; }
[ "$fail" = 0 ] || exit 1
