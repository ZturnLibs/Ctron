#!/bin/sh
# tests/connect/run.sh —— Connect 协议客户端验收(P8-A)
# corpus 双臂 + e2e 段(CTRON_CONN_E2E=1;真回环 srv/send 双进程)。
# CTRON_CC/EMIT/CHK 覆盖(worktree 隔离构建口径)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CC="${CTRON_CC:-$ROOT/compiler/bin/ctron-cc}"
EMIT="${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}"
CHK="${CTRON_CHK:-$ROOT/compiler/bin/ctron-chk}"
export CTRON_STDPATH="$ROOT/std"
[ -x "$EMIT" ] || { echo "connect/run: 缺少编译器二进制" >&2; exit 2; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/connect Connect 客户端用例(P8-A)=="

if [ -x "$CHK" ] || [ -n "${CTRON_CHK:-}" ]; then
    "$CHK" run "$ROOT/http/frm/connect.ct" > "$T/chk.out" 2>&1 || true
    if grep -qE '^[E][0-9]' "$T/chk.out"; then
        fail=$((fail+1)); echo "  FAIL http/frm/connect.ct (sem, chk)"; sed -n '1,3p' "$T/chk.out"
    else
        pass=$((pass+1)); echo "  PASS http/frm/connect.ct (sem, chk;W 告警放行=net/bind 既有)"
    fi
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
        fail=$((fail+1)); echo "  FAIL $name (emit)"
    fi
done

if [ "${CTRON_CONN_E2E:-}" = "1" ]; then
    echo "== connect e2e:srv ↔ send =="
    if "$EMIT" run "$DIR/srv.ct" > "$T/srv.c" 2>/dev/null && cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/srv.bin" "$T/srv.c" "$ROOT/net/c_src/ctron_net.c" 2>/dev/null \
       && "$EMIT" run "$DIR/send.ct" > "$T/send.c" 2>/dev/null && cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/send.bin" "$T/send.c" "$ROOT/net/c_src/ctron_net.c" 2>/dev/null; then
        CPORT=$((21000 + RANDOM % 20000))
        ( CONN_PORT="$CPORT" "$T/srv.bin" > "$T/srv.log" 2>&1 & )
        sleep 1
        CONN_PORT="$CPORT" timeout 10 "$T/send.bin" > "$T/send.log" 2>&1
        SRC=$?
        sleep 0.3
        pkill -f 'srv.bin' 2>/dev/null
        R1=1; R2=1
        if [ "$SRC" = 0 ] && grep -q "CONNECT-OK" "$T/send.log"; then R1=0; fi
        # 失败分类轮:CONN_FAIL 服务(500)+ CONN_EXPECT=err 客户端
        CPORT2=$((21000 + RANDOM % 20000))
        ( CONN_FAIL=1 CONN_PORT="$CPORT2" "$T/srv.bin" > "$T/srv2.log" 2>&1 & )
        sleep 1
        CONN_EXPECT=err CONN_PORT="$CPORT2" timeout 10 "$T/send.bin" > "$T/send2.log" 2>&1
        SRC2=$?
        sleep 0.3
        pkill -f 'srv.bin' 2>/dev/null
        if [ "$SRC2" = 0 ] && grep -q "CONNECT-ERR-OK" "$T/send2.log"; then R2=0; fi
        if [ "$R1" = 0 ] && [ "$R2" = 0 ]; then
            pass=$((pass+1)); echo "  PASS connect-e2e(成功帧 + 500 失败分类)"
        else
            fail=$((fail+1)); echo "  FAIL connect-e2e(ok=$R1 err=$R2)"; sed -n '1,3p' "$T/send.log" "$T/send2.log" 2>/dev/null
        fi
    else
        fail=$((fail+1)); echo "  FAIL connect-e2e 构建"
    fi
else
    echo "  [skip] connect-e2e:CTRON_CONN_E2E=1 启用"
fi

echo "connect/run: pass=$pass fail=$fail"
[ "$pass" -gt 0 ] || { echo "connect/run: no cases ran"; exit 1; }
[ "$fail" = 0 ] || exit 1
