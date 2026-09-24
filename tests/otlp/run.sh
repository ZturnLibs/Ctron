#!/bin/sh
# tests/otlp/run.sh —— OTLP 编码面验收(P7-E;发送/接收 e2e 在 todo 关联 fixture 后补)
# 口径(tests/trace 同款):chk 语义门 + corpus/*.ct 双臂。CTRON_CC/EMIT/CHK 覆盖。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CC="${CTRON_CC:-$ROOT/compiler/bin/ctron-cc}"
EMIT="${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}"
CHK="${CTRON_CHK:-$ROOT/compiler/bin/ctron-chk}"
export CTRON_STDPATH="$ROOT/std"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/otlp 编码面用例(P7-E)=="

if [ -x "$CHK" ] || [ -n "${CTRON_CHK:-}" ]; then
    if "$CHK" run "$ROOT/http/frm/otlp.ct" > "$T/chk.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS http/frm/otlp.ct (sem, chk)"
    else
        fail=$((fail+1)); echo "  FAIL http/frm/otlp.ct (sem, chk)"; sed -n '1,5p' "$T/chk.out"
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

echo "otlp/run: pass=$pass fail=$fail"
[ "$pass" -gt 0 ] || { echo "otlp/run: no cases ran"; exit 1; }
[ "$fail" = 0 ] || exit 1
