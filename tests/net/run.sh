#!/bin/sh
# tests/net/run.sh —— 服务器泳道回环验收(§11;CI 纪律:仅回环、:0、零外联)
# 口径:行为夹具(目录含 c_src/ 且含 src/main.ct):ctron-emit → cc 链 c_src/*.c → 原生运行;
#       纯 C 冒烟目录(rt_core_smoke/rt_reactor_smoke,无 main.ct)由各自 run.sh 承载,不入主环
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
echo "net/run: pass=$pass fail=$fail"
# M-T4-2:空集口径——一例未跑(pass=0)与全败同罪,防夹具被静默跳过
[ "$pass" -gt 0 ] || { echo "net/run: no cases ran"; exit 1; }
[ "$fail" = 0 ]
