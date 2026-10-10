#!/bin/sh
# tests/wasm/gate.sh —— T37 wasm 档门包装(自 ci.sh 移植;tail 压缩壳)。
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
WASM_OUT=$(mktemp)
if sh "$ROOT/tests/wasm/run.sh" > "$WASM_OUT" 2>&1; then
    tail -6 "$WASM_OUT"
else
    echo "[FAIL] wasm 档门红(T37;见上)" >&2
    cat "$WASM_OUT" >&2
    rm -f "$WASM_OUT"
    exit 1
fi
rm -f "$WASM_OUT"
