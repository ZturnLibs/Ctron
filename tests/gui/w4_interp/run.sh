#!/bin/sh
# tests/gui/w4_interp/run.sh —— W4 E2:解释口径 extern 直调(ctron-cc + dlsym 桥)
# 无 cc 链接用户程序:bin/ctron-cc 自身携带域库符号源(native.sh 链接),
# 解释执行用户 extern 调用(W4-E1 桥;dlsym RTLD_DEFAULT)。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
[ -x "$ROOT/compiler/bin/ctron-cc" ] || { echo "w4_interp: 缺 compiler/bin/ctron-cc(先 sh compiler/native.sh)" >&2; exit 2; }

cd "$DIR"
"$ROOT/compiler/bin/ctron-cc" run src/main.ct
echo "w4_interp: 解释口径 extern 直调全绿"
