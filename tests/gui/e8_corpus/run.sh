#!/bin/sh
# tests/gui/e8_corpus/run.sh —— W3/SL-6 检查面语料:--dump-gui 口径按期望码断言
# 负例(*.neg.ct):期待 rc≠0 且输出含 "// gui expect: EXXXX" 注记的码;
# 正例(*.pos.ct):期待 rc=0 且无 E81xx 诊断(回归锁:检查面结构走查不得误伤/崩溃);
# 警告(*.warn.ct):期待 rc=0 且输出含期望码(E8193 警告级可关,§4.4)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
pass=0
fail=0
for f in "$DIR"/*.neg.ct; do
    name=$(basename "$f")
    exp=$(grep -o "gui expect: [A-Z0-9]*" "$f" | awk '{print $3}')
    out=$("$ROOT/compiler/ctc.sh" check "$f" --dump-gui 2>&1)
    rc=$?
    if [ "$rc" -ne 0 ] && echo "$out" | grep -q "$exp"; then
        echo "  [ok] $name ($exp)"
        pass=$((pass + 1))
    else
        echo "  [FAIL] $name — 期待 $exp,rc=$rc"
        fail=$((fail + 1))
    fi
done
for f in "$DIR"/*.pos.ct; do
    name=$(basename "$f")
    out=$("$ROOT/compiler/ctc.sh" check "$f" --dump-gui 2>&1)
    rc=$?
    if [ "$rc" -eq 0 ] && ! echo "$out" | grep -q "E81"; then
        echo "  [ok] $name (干净通过)"
        pass=$((pass + 1))
    else
        echo "  [FAIL] $name — 期待干净通过,rc=$rc"
        fail=$((fail + 1))
    fi
done
for f in "$DIR"/*.warn.ct; do
    name=$(basename "$f")
    exp=$(grep -o "gui expect: [A-Z0-9]*" "$f" | awk '{print $3}')
    out=$("$ROOT/compiler/ctc.sh" check "$f" --dump-gui 2>&1)
    rc=$?
    if [ "$rc" -eq 0 ] && echo "$out" | grep -q "$exp"; then
        echo "  [ok] $name (warn $exp)"
        pass=$((pass + 1))
    else
        echo "  [FAIL] $name — 期待 warn $exp,rc=$rc"
        fail=$((fail + 1))
    fi
done
echo "e8_corpus: $pass 过 / $fail 败"
[ "$fail" -eq 0 ]
