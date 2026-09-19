#!/bin/sh
# tests/gui/s14_bidi/run.sh —— M3 地基:SheenBidi vendored 库构建 + 双向文本 run 冒烟
# 纯 C 层断言(UBA run/level);Ctron FFI 绑定随 M3 接入
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")

sh "$ROOT/vendor/gui/build.sh"
[ -f "$ROOT/vendor/gui/build/libsheenbidi.a" ] || { echo "s14: 缺 libsheenbidi.a" >&2; exit 2; }

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
cc -O1 -w -I"$ROOT/vendor/gui/sheenbidi/Headers" -o "$T/smoke.bin" \
   "$DIR"/c_src/smoke.c "$ROOT/vendor/gui/build/libsheenbidi.a"

"$T/smoke.bin"
echo "s14_bidi: SheenBidi 冒烟全绿"
