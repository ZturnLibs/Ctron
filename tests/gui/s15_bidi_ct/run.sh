#!/bin/sh
# tests/gui/s15_bidi_ct/run.sh —— M3 地基:Ctron FFI 驱动 SheenBidi(句柄 I64 + Ctron 侧 assert_eq)
# 纯计算无窗口:发射 → 链接 libsheenbidi.a → 运行,断言失败即非零退出
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s15_bidi_ct: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh"

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$EMIT" run "$DIR/src/main.ct" > "$T/s15.c"
cc -O1 -w -I"$ROOT/vendor/gui/sheenbidi/Headers" -o "$T/s15.bin" \
   "$T/s15.c" "$DIR"/c_src/*.c "$ROOT/vendor/gui/build/libsheenbidi.a"

"$T/s15.bin" > "$T/out.txt"
grep -q "run 切分全绿" "$T/out.txt"
echo "s15_bidi_ct: Ctron FFI 驱动全绿"
