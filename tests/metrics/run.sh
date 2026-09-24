#!/bin/sh
# tests/metrics/run.sh —— Prometheus exposition 写面验收(P7-D)
# 口径(tests/trace 同款):chk 语义门 + corpus/*.ct 双臂。CTRON_CC/EMIT/CHK 覆盖。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CC="${CTRON_CC:-$ROOT/compiler/bin/ctron-cc}"
EMIT="${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}"
CHK="${CTRON_CHK:-$ROOT/compiler/bin/ctron-chk}"
export CTRON_STDPATH="$ROOT/std"
if [ ! -x "$CC" ] || [ ! -x "$EMIT" ]; then echo "metrics/run: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/metrics exposition 用例(P7-D)=="

if [ -x "$CHK" ] || [ -n "${CTRON_CHK:-}" ]; then
    if "$CHK" run "$ROOT/http/frm/metrics.ct" > "$T/chk.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS http/frm/metrics.ct (sem, chk)"
    else
        fail=$((fail+1)); echo "  FAIL http/frm/metrics.ct (sem, chk)"; sed -n '1,5p' "$T/chk.out"
    fi
fi

for f in "$DIR"/corpus/*.ct; do
    name=$(basename "$f" .ct)
    if "$CC" run "$f" > "$T/$name.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS $name (interp)"
    else
        fail=$((fail+1)); echo "  FAIL $name (interp)"; sed -n '1,5p' "$T/$name.out"
    fi
    if "$EMIT" run "$f" > "$T/$name.c" 2>"$T/$name.err" && cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/$name.bin" "$T/$name.c" "$ROOT/net/c_src/ctron_net.c" 2>"$T/$name.cc.err" && "$T/$name.bin" > "$T/$name.run" 2>&1; then
        pass=$((pass+1)); echo "  PASS $name (emit)"
    else
        fail=$((fail+1)); echo "  FAIL $name (emit)"; sed -n '1,5p' "$T/$name.err" "$T/$name.cc.err" "$T/$name.run" 2>/dev/null
    fi
done

echo "metrics/run: pass=$pass fail=$fail"
[ "$pass" -gt 0 ] || { echo "metrics/run: no cases ran"; exit 1; }
[ "$fail" = 0 ] || exit 1
