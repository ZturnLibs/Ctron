#!/bin/sh
# tests/net/run.sh —— 服务器泳道回环验收(§11;CI 纪律:仅回环、:0、零外联)
# 口径:行为夹具(目录含 c_src/ 且含 src/main.ct):ctron-emit → cc 链 c_src/*.c → 原生运行;
#       纯 C 冒烟目录(rt_core_smoke/rt_reactor_smoke,无 main.ct)由各自 run.sh 承载,不入主环;
#       coro_hybrid 的 main.ct 入主环(裸线程面),其 c_smoke.c 由下方专属块承载(P2-C)
# 前置:compiler/native.sh、cc
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
# Task 5 起夹具 use std.net.*:显式指路源码树 std(gui_counter/run.sh 同款)
export CTRON_STDPATH="$ROOT/std"
if [ ! -x "$EMIT" ]; then echo "net/run: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/net 回环用例(§11)=="
for d in "$DIR"/*/; do
    [ -d "$d/c_src" ] || continue
    name=$(basename "$d"); [ "$name" = "bench" ] && continue
    e="$d/src/main.ct"
    [ -f "$e" ] || continue              # 纯 C 冒烟目录(rt_*_smoke)无 main.ct:不入主环
    if "$EMIT" run "$e" > "$T/$name.c" 2>"$T/$name.err"; then
        if cc -O1 -w -o "$T/$name" "$T/$name.c" "$d"/c_src/*.c 2>"$T/$name.cc.err"; then
            if "$T/$name" run "$e" >"$T/$name.out" 2>&1; then
                pass=$((pass+1)); echo "  PASS $name"
            else
                fail=$((fail+1)); echo "  FAIL $name (run)"; sed -n '1,5p' "$T/$name.out"
            fi
        else
            fail=$((fail+1)); echo "  FAIL $name (cc)"; sed -n '1,5p' "$T/$name.cc.err"
        fi
    else
        fail=$((fail+1)); echo "  FAIL $name (emit)"; sed -n '1,5p' "$T/$name.err"
    fi
done
# coro_hybrid C 冒烟(P2-C):net 垫片协程停车证明 —— net+rt 双链、CTRON_RT=coro、
# workers=1 与 4 各整跑(1 = 停车严格证:垫片若滞留 worker,进度协程即饿死)。
# .ct 部分已入主环(裸线程面,rt 链入但 current()==NULL → P1 原路径)。
smoke="$DIR/coro_hybrid/c_smoke.c"
if [ -f "$smoke" ]; then
    name="coro_hybrid_c_smoke"
    if cc -O1 -w -pthread -o "$T/$name" "$smoke" "$DIR"/coro_hybrid/c_src/*.c 2>"$T/$name.cc.err"; then
        for W in 1 4; do
            if CTRON_RT=coro CTRON_RT_WORKERS=$W "$T/$name" >"$T/$name.out" 2>&1; then
                pass=$((pass+1)); echo "  PASS $name (workers=$W)"
            else
                fail=$((fail+1)); echo "  FAIL $name (run, workers=$W)"; sed -n '1,5p' "$T/$name.out"
            fi
        done
    else
        fail=$((fail+1)); echo "  FAIL $name (cc)"; sed -n '1,5p' "$T/$name.cc.err"
    fi
fi
echo "net/run: pass=$pass fail=$fail"
# M-T4-2:空集口径——一例未跑(pass=0)与全败同罪,防夹具被静默跳过
[ "$pass" -gt 0 ] || { echo "net/run: no cases ran"; exit 1; }
[ "$fail" = 0 ]
