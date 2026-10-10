#!/bin/sh
# tests/gc/determinism.sh —— T31 事故回归探针(2026-10-05):编译器在 CTRON_GC=1 下
# 自身被 GC 化,发射产物必须与 off 逐字一致(s/N 槽注册+盒类 bump 的常驻守门哨)。
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
PT=$(mktemp -d)
CTRON_STDPATH="$ROOT/lib/std" CTRON_GC=1 "$ROOT/compiler/bin/ctron-emit" run "$ROOT/compiler/build/cc_run.ct" > "$PT/emit_gc1.c" 2>/dev/null
env -u CTRON_GC CTRON_STDPATH="$ROOT/lib/std" "$ROOT/compiler/bin/ctron-emit" run "$ROOT/compiler/build/cc_run.ct" > "$PT/emit_off.c" 2>/dev/null
if ! diff -q "$PT/emit_gc1.c" "$PT/emit_off.c" > /dev/null 2>&1; then
    echo "[FAIL] 发射确定性双环境探针分歧(编译器 GC 化输出漂移;归 GC 泳道)" >&2
    rm -rf "$PT"
    exit 1
fi
rm -rf "$PT"
echo "  ok  : 发射确定性双环境探针(GC=1/off 大语料发射逐字一致)"
