#!/bin/sh
# tests/gui/e8_corpus/run.sh —— W3 检查面语料:--dump-gui 口径按期望码断言
# 期待码从各文件的 "// gui expect: EXXXX" 注释解析
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
echo "e8_corpus: $pass 过 / $fail 败"
[ "$fail" -eq 0 ]
