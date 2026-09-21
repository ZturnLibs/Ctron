#!/bin/sh
# tests/http/run.sh —— HTTP/1.1 协议半层验收(P4-A;§11.7)
# 口径:std/http 为零 use 纯 Ctron 半层(不 import std.net:加载器菱形 use
# 误报 E5020 规避 + 网络垫片在解释口径无绑定,divergences (c)/(g) ⑥),故:
#   主环 = 解释器(ctron-cc run):corpus/*.ct 断言式夹具逐文件跑,纯层无需
#          原生速度;与 std/*.ct 种子单测同口径(smoke.sh 3e 先例)。
#   副 = emit 同源对拍(ctron-emit → cc → 原生):std/http 无 extern,发射零
#        链接需求;对拍即 divergences (h)(跨模块 struct 形)的常设回归哨。
#   两臂同一夹具集双计 pass;任一臂红即红。pass>0 空集守卫(tests/net 同款)。
#   双运行时矩阵(CTRON_RT=coro)不适用:纯层无停车点,无 rt 依赖。
# 网络端到端行为(HTTP over TCP)归框架半层波次(P4-B+),届时入 tests/net
# 双矩阵口径;本目录 CI 纪律同 tests/net:仅回环、零外联(纯层实为离线)。
# 前置:compiler/native.sh、cc(副臂)
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CC="$ROOT/compiler/bin/ctron-cc"
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/std"
if [ ! -x "$CC" ]; then echo "http/run: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/http 协议半层用例(§11.7;P4-A)=="

# ── std 模块 inline tests(与 std/*.ct 种子单测同口径)──
for m in parse message; do
    if "$CC" run "$ROOT/std/http/$m.ct" > "$T/std_$m.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS std/http/$m.ct (inline)"
    else
        fail=$((fail+1)); echo "  FAIL std/http/$m.ct (inline)"; sed -n '1,5p' "$T/std_$m.out"
    fi
done

# ── corpus:主环 = 解释器 ──
for f in "$DIR"/corpus/*.ct; do
    name=$(basename "$f" .ct)
    if "$CC" run "$f" > "$T/$name.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS $name (interp)"
    else
        fail=$((fail+1)); echo "  FAIL $name (interp)"; sed -n '1,5p' "$T/$name.out"
    fi
done

# ── corpus:副臂 = emit 对拍((h) 回归哨;无 c_src,直链)──
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

echo "http/run: pass=$pass fail=$fail"
# 空集口径:一例未跑与全败同罪
[ "$pass" -gt 0 ] || { echo "http/run: no cases ran"; exit 1; }
[ "$fail" = 0 ]
