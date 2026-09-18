#!/bin/sh
# tests/gui/w1_block/run.sh —— W1:view/style 块认领(check 全链无感;黄金 decls 差分)
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
out=$("$ROOT/compiler/ctc.sh" check "$DIR/src/main.ct")
echo "$out" > "$DIR/actual.tmp.txt" 2>/dev/null || true
if [ "$out" = "$(cat "$DIR/expected.txt")" ]; then
    rm -f "$DIR/actual.tmp.txt"
    echo "w1: view/style 块认领全绿($out)"
else
    echo "w1: 黄金差分失败: 得 [$out] 期望 [$(cat "$DIR/expected.txt")]" >&2
    rm -f "$DIR/actual.tmp.txt"
    exit 1
fi
