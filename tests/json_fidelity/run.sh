#!/bin/sh
# tests/json_fidelity/run.sh —— std/json 数值/布尔保真验收(P5-A;§P5 Task 1)
# 口径:std/json 为零 use 纯 Ctron 叶模块(无 extern/无 std 依赖),tests/http
# 同款双臂跑法(纯层):
#   主环 = 解释器(ctron-cc run):std/json.ct 种子单测 + corpus/*.ct 断言式
#          夹具逐文件跑。
#   副 = emit 同源对拍(ctron-emit → cc → 原生):无 extern 零链接需求;
#        对拍即 (i) 族双口径常设哨(解释器 F64 值域定宽 vs emit IEEE double)。
#   x_ 前缀 = 仅 emit 臂入计(tests/http enc_fixtures 同款结构性登记):
#          x_p53_rounding 承载解释器 F64 超 2^53 精确不舍入而不可观测的
#          IEEE 舍入面(C12「发射线夹具承载」先例)。
#   两臂同一夹具集双计 pass;任一臂红即红。pass>0 空集守卫(tests/net 同款)。
#   双运行时矩阵(CTRON_RT=coro)不适用:纯层无停车点,无 rt 依赖。
# 前置:compiler/native.sh、cc(副臂)
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CC="$ROOT/compiler/bin/ctron-cc"
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/std"
if [ ! -x "$CC" ]; then echo "json_fidelity/run: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/json_fidelity 数值/布尔保真用例(P5-A)=="

# ── std 规范源与种子副本漂移守卫(smoke.sh 3d 同款,本地即查)──
if diff -q "$ROOT/std/json.ct" "$ROOT/compiler/test/stdpkg/std/json.ct" > /dev/null 2>&1; then
    pass=$((pass+1)); echo "  PASS std/json.ct 种子副本无漂移"
else
    fail=$((fail+1)); echo "  FAIL std/json.ct 种子副本漂移(cp std/json.ct compiler/test/stdpkg/std/)"
fi

# ── std/json.ct inline tests(解释器;种子单测同口径)──
if "$CC" run "$ROOT/std/json.ct" > "$T/std_json.out" 2>&1; then
    pass=$((pass+1)); echo "  PASS std/json.ct (inline, interp)"
else
    fail=$((fail+1)); echo "  FAIL std/json.ct (inline, interp)"; sed -n '1,5p' "$T/std_json.out"
fi

# ── corpus:主环 = 解释器(x_ 仅 emit 臂,不入计)──
for f in "$DIR"/corpus/*.ct; do
    name=$(basename "$f" .ct)
    case "$name" in
        x_*) continue ;;
    esac
    if "$CC" run "$f" > "$T/$name.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS $name (interp)"
    else
        fail=$((fail+1)); echo "  FAIL $name (interp)"; sed -n '1,5p' "$T/$name.out"
    fi
done

# ── corpus:副臂 = emit 对拍(含 x_;无 extern,直链)──
if [ -x "$EMIT" ]; then
    for f in "$DIR"/corpus/*.ct; do
        name=$(basename "$f" .ct)
        if "$EMIT" run "$f" > "$T/$name.e.c" 2>"$T/$name.e.err" \
           && cc -O1 -w -o "$T/$name.e.bin" "$T/$name.e.c" 2>"$T/$name.e.cc.err" \
           && "$T/$name.e.bin" > "$T/$name.e.out" 2>&1; then
            pass=$((pass+1)); echo "  PASS $name (emit)"
        else
            fail=$((fail+1)); echo "  FAIL $name (emit)"; sed -n '1,5p' "$T/$name.e.out" "$T/$name.e.cc.err" "$T/$name.e.err" 2>/dev/null
        fi
    done
else
    echo "  [skip] emit 臂:缺 ctron-emit(先: compiler/native.sh)——不入计例,interp 臂已覆盖"
fi

# ── 空集守卫 ──
if [ "$pass" -eq 0 ]; then
    echo "json_fidelity/run: 零 pass(空集或全红),拒判绿" >&2
    exit 1
fi

echo "== json_fidelity: pass=$pass fail=$fail =="
if [ "$fail" -gt 0 ]; then exit 1; fi
exit 0
