#!/bin/sh
# tests/pkg/run.sh —— ctron pkg 验收(P8-D)
# corpus 双臂 + chk 语义门 + e2e 段(CTRON_PKG_E2E=1:registry 夹具 ↔ pkg 工具)。
# CTRON_CC/EMIT/CHK 覆盖(worktree 口径)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CC="${CTRON_CC:-$ROOT/compiler/bin/ctron-cc}"
EMIT="${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}"
CHK="${CTRON_CHK:-$ROOT/compiler/bin/ctron-chk}"
export CTRON_STDPATH="$ROOT/std"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/pkg ctron pkg 用例(P8-D)=="

if [ -x "$CHK" ] || [ -n "${CTRON_CHK:-}" ]; then
    "$CHK" run "$ROOT/pkg/pkg.ct" > "$T/chk.out" 2>&1 || true
    if grep -qE '^[E][0-9]' "$T/chk.out"; then
        fail=$((fail+1)); echo "  FAIL pkg/pkg.ct (sem)"; sed -n '1,3p' "$T/chk.out"
    else
        pass=$((pass+1)); echo "  PASS pkg/pkg.ct (sem, chk)"
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

# ── e2e 段(CTRON_PKG_E2E=1):registry ↔ ctpkg(vendor+lockfile 落盘验证)──
if [ "${CTRON_PKG_E2E:-}" = "1" ]; then
    echo "== pkg e2e:registry ↔ pkg add =="
    if "$EMIT" run "$DIR/registry.ct" > "$T/reg.c" 2>/dev/null && cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/reg.bin" "$T/reg.c" "$ROOT/net/c_src/ctron_net.c" 2>/dev/null \
       && "$EMIT" run "$ROOT/tools/ctpkg.ct" > "$T/tool.c" 2>/dev/null && cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/tool.bin" "$T/tool.c" "$ROOT/net/c_src/ctron_net.c" 2>/dev/null; then
        PP=$((21000 + RANDOM % 20000))
        W=$(mktemp -d)
        mkdir -p "$W/vendor"
        ( REG_PORT="$PP" "$T/reg.bin" > "$T/reg.log" 2>&1 & )
        PR=1
        pr_i=0
        while [ "$pr_i" -lt 30 ]; do
            if nc -z 127.0.0.1 "$PP" 2>/dev/null; then PR=0; break; fi
            sleep 0.3; pr_i=$((pr_i+1))
        done
        if [ "$PR" = 0 ]; then
            echo "  [info] registry 就绪($((pr_i+1)) 探)"
        fi
        ( cd "$W" && PKG_PORT="$PP" PKG_NAME=app PKG_VER=1.0.0 timeout 20 "$T/tool.bin" > "$T/tool.log" 2>&1 )
        PRC=$?
        sleep 0.5
        pkill -f 'reg.bin' 2>/dev/null
        VOK=1
        [ -f "$W/vendor/app-1.0.0.ct" ] || VOK=0
        [ -f "$W/vendor/lib-1.2.0.ct" ] || VOK=0
        [ -f "$W/ctron-lock.ndjson" ] || VOK=0
        grep -q 'APP-MODULE' "$W/vendor/app-1.0.0.ct" 2>/dev/null || VOK=0
        grep -q 'LIB-MODULE-CONTENT' "$W/vendor/lib-1.2.0.ct" 2>/dev/null || VOK=0
        grep -q 'app|1.0.0|' "$W/ctron-lock.ndjson" 2>/dev/null || VOK=0
        grep -q 'lib|1.2.0|' "$W/ctron-lock.ndjson" 2>/dev/null || VOK=0
        if [ "$PRC" = 0 ] && [ "$VOK" = 1 ]; then
            pass=$((pass+1)); echo "  PASS pkg-e2e(manifest→deps→vendor 落盘→lockfile)"
        else
            fail=$((fail+1)); echo "  FAIL pkg-e2e(rc=$PRC vok=$VOK)"; sed -n '1,3p' "$T/tool.log" 2>/dev/null
        fi
        rm -rf "$W"
    else
        fail=$((fail+1)); echo "  FAIL pkg-e2e 构建"
    fi
else
    echo "  [skip] pkg-e2e:CTRON_PKG_E2E=1 启用"
fi

echo "pkg/run: pass=$pass fail=$fail"
[ "$pass" -gt 0 ] || { echo "pkg/run: no cases ran"; exit 1; }
[ "$fail" = 0 ] || exit 1
