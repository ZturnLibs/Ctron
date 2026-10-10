#!/bin/sh
# tests/bare/gate.sh —— T40/T42 bare 档门包装(自 ci.sh 移植;tail 压缩壳)。
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
BARE_OUT=$(mktemp)
if sh "$ROOT/tests/bare/run.sh" > "$BARE_OUT" 2>&1; then
    tail -6 "$BARE_OUT"
else
    echo "[FAIL] bare 档门红(T40/T42;见上)" >&2
    cat "$BARE_OUT" >&2
    rm -f "$BARE_OUT"
    exit 1
fi
rm -f "$BARE_OUT"
