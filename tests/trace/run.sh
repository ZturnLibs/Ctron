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

# ── 传播 e2e 段(CTRON_TP_PROP=1 启用):A 提取→再注入→B 回显,trace-id 一致 ──
if [ "${CTRON_TP_PROP:-}" = "1" ]; then
    echo "== trace 传播 e2e:双服务串联 trace-id 一致 =="
    if "$EMIT" run "$DIR/prop_a.ct" > "$T/pa.c" 2>"$T/pa.err" && cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/pa.bin" "$T/pa.c" "$ROOT/net/c_src/ctron_net.c" 2>"$T/pa.cc.err"        && "$EMIT" run "$DIR/prop_b.ct" > "$T/pbb.c" 2>"$T/pbb.err" && cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/pbb.bin" "$T/pbb.c" "$ROOT/net/c_src/ctron_net.c" 2>"$T/pbb.cc.err"; then
        AP=$((21000 + RANDOM % 9000))
        BP=$((30000 + RANDOM % 20000))
        ( PROP_B_PORT="$BP" "$T/pbb.bin" > "$T/pb.log" 2>&1 & )
        sleep 1
        ( PROP_A_PORT="$AP" PROP_B_PORT="$BP" "$T/pa.bin" > "$T/pa.log" 2>&1 & )
        sleep 1
        RESP=$(printf 'GET /hop HTTP/1.1\r\nHost: x\r\ntraceparent: 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01\r\n\r\n' | timeout 8 nc 127.0.0.1 "$AP" 2>/dev/null)
        sleep 0.3
        pkill -f 'pa.bin' 2>/dev/null; pkill -f 'pbb.bin' 2>/dev/null
        if echo "$RESP" | grep -q "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01"; then
            pass=$((pass+1)); echo "  PASS tp-prop(trace-id 跨服务一致)"
        else
            fail=$((fail+1)); echo "  FAIL tp-prop(响应缺期望 tid)"
        fi
    else
        fail=$((fail+1)); echo "  FAIL tp-prop 构建"; sed -n '1,3p' "$T/pa.err" "$T/pbb.err" 2>/dev/null
    fi
else
    echo "  [skip] tp-prop:CTRON_TP_PROP=1 启用"
fi

echo "trace/run: pass=$pass fail=$fail"
[ "$pass" -gt 0 ] || { echo "trace/run: no cases ran"; exit 1; }
[ "$fail" = 0 ] || exit 1
