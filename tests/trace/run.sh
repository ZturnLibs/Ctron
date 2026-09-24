#!/bin/sh
# tests/trace/run.sh —— W3C traceparent 提取/注入验收(P7-C)
# 口径(tests/log、tests/pb 同款):corpus/*.ct 双臂;http/frm/trace.ct 为
# 零 use 纯 Ctron 模块(http 域,不入 stdpkg 种子镜像)。CTRON_CC/EMIT 覆盖。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CC="${CTRON_CC:-$ROOT/compiler/bin/ctron-cc}"
EMIT="${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}"
export CTRON_STDPATH="$ROOT/std"
if [ ! -x "$CC" ] || [ ! -x "$EMIT" ]; then echo "trace/run: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/trace traceparent 用例(P7-C)=="

# 模块语义门(零 inline test 大模块惯例;chk 语义 + 依赖形 emit 由 corpus 覆盖)
CHK="${CTRON_CHK:-$ROOT/compiler/bin/ctron-chk}"
if [ -x "$CHK" ] || [ -n "${CTRON_CHK:-}" ]; then
    if "$CHK" run "$ROOT/http/frm/trace.ct" > "$T/chk.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS http/frm/trace.ct (sem, chk)"
    else
        fail=$((fail+1)); echo "  FAIL http/frm/trace.ct (sem, chk)"; sed -n '1,5p' "$T/chk.out"
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

echo "trace/run: pass=$pass fail=$fail"
[ "$pass" -gt 0 ] || { echo "trace/run: no cases ran"; exit 1; }
[ "$fail" = 0 ] || exit 1
