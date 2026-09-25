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

# ── e2e 段(CTRON_OTLP_E2E=1 启用;真回环 socket,bench 家族惯例默认跳)──
if [ "${CTRON_OTLP_E2E:-}" = "1" ]; then
    echo "== otlp e2e:send → recv 解码比对 =="
    if "$EMIT" run "$DIR/recv.ct" > "$T/recv.c" 2>"$T/recv.err" && cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/recv.bin" "$T/recv.c" "$ROOT/net/c_src/ctron_net.c" 2>"$T/recv.cc.err"        && "$EMIT" run "$DIR/send.ct" > "$T/send.c" 2>"$T/send.err" && cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/send.bin" "$T/send.c" "$ROOT/net/c_src/ctron_net.c" 2>"$T/send.cc.err"; then
        EPORT=$((21000 + RANDOM % 20000))
        ( OTLP_PORT="$EPORT" OTLP_EXP_TID=4bf92f3577b34da6a3ce929d0e0e4736 OTLP_EXP_NAME=tr "$T/recv.bin" > "$T/recv.log" 2>&1 & )
        sleep 1
        OTLP_PORT="$EPORT" OTLP_TID=4bf92f3577b34da6a3ce929d0e0e4736 OTLP_SID=00f067aa0ba902b7 OTLP_NAME=tr timeout 8 "$T/send.bin" > "$T/send.log" 2>&1
        SRC=$?
        sleep 0.5
        pkill -f 'recv.bin' 2>/dev/null
        if [ "$SRC" = 0 ] && grep -q "OTLP-OK" "$T/recv.log"; then
            pass=$((pass+1)); echo "  PASS otlp-e2e(send 200 + recv tid/name 解码一致)"
        else
            fail=$((fail+1)); echo "  FAIL otlp-e2e(send_rc=$SRC)"; sed -n '1,3p' "$T/recv.log" "$T/send.log" 2>/dev/null
        fi
    else
        fail=$((fail+1)); echo "  FAIL otlp-e2e 构建"; sed -n '1,3p' "$T/recv.err" "$T/send.err" 2>/dev/null
    fi
else
    echo "  [skip] otlp-e2e:CTRON_OTLP_E2E=1 启用"
fi

echo "otlp/run: pass=$pass fail=$fail"
[ "$pass" -gt 0 ] || { echo "otlp/run: no cases ran"; exit 1; }
[ "$fail" = 0 ] || exit 1
