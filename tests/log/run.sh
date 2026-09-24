#!/bin/sh
# tests/log/run.sh —— std/log 结构化 kv 日志验收(P7-A)
# 口径(tests/json_fidelity 同款双臂;std/log 为零 use 纯 Ctron 叶模块):
#   种子漂移守卫 + inline tests + corpus/*.ct 逐文件;解释器主环 + emit 同源
#   对拍,两臂同一夹具集双计 pass;纯层无 rt 依赖(CTRON_RT 矩阵不适用)。
#   CC/EMIT 支持 CTRON_CC/CTRON_EMIT 覆盖(worktree 隔离构建验证口径)。
# 前置:compiler/native.sh、cc(副臂)
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CC="${CTRON_CC:-$ROOT/compiler/bin/ctron-cc}"
EMIT="${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}"
export CTRON_STDPATH="$ROOT/std"
if [ ! -x "$CC" ] || [ ! -x "$EMIT" ]; then echo "log/run: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/log 结构化 kv 日志用例(P7-A)=="

# ── 种子副本漂移守卫(smoke.sh 同款,本地即查)──
if diff -q "$ROOT/std/log.ct" "$ROOT/compiler/test/stdpkg/std/log.ct" > /dev/null 2>&1; then
    pass=$((pass+1)); echo "  PASS std/log.ct 种子副本无漂移"
else
    fail=$((fail+1)); echo "  FAIL std/log.ct 种子副本漂移(cp std/log.ct compiler/test/stdpkg/std/)"
fi

# ── inline tests:双臂 ──
if "$CC" run "$ROOT/std/log.ct" > "$T/li.out" 2>&1; then
    pass=$((pass+1)); echo "  PASS std/log.ct (inline, interp)"
else
    fail=$((fail+1)); echo "  FAIL std/log.ct (inline, interp)"; sed -n '1,5p' "$T/li.out"
fi
if "$EMIT" run "$ROOT/std/log.ct" > "$T/li.c" 2>"$T/li.err" && cc -O1 -w -o "$T/li.bin" "$T/li.c" 2>"$T/li.cc.err" && "$T/li.bin" > "$T/li2.out" 2>&1; then
    pass=$((pass+1)); echo "  PASS std/log.ct (inline, emit)"
else
    fail=$((fail+1)); echo "  FAIL std/log.ct (inline, emit)"; sed -n '1,5p' "$T/li.err" "$T/li.cc.err" "$T/li2.out" 2>/dev/null
fi

# ── corpus:双臂逐文件 ──
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

echo "log/run: pass=$pass fail=$fail"
[ "$pass" -gt 0 ] || { echo "log/run: no cases ran"; exit 1; }
[ "$fail" = 0 ] || exit 1
