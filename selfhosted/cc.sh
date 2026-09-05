#!/bin/sh
# cc.sh —— Ctron 自举编译器驱动(cc.ct = Ctron 写的编译器单文件快照)
#
# 用法:
#   ./cc.sh <input.ct>        parse → 单文件语义 12 项 → 解释执行(fn main 或 test 块)
#
# 说明:cc.ct 是自举编译器(parser + 语义检查 + 求值器,全部 Ctron 实现);
# 本脚本只负责把输入路径换入 cc.ct 的 read_file 模板锚并以宿主 seed 运行。
# 宿主 seed(compiler_c/build/ctronc)仅充当 Ctron 解释器;自举完成后可自替换。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$DIR")
HOST="$ROOT/compiler_c/build/ctronc"

if [ ! -x "$HOST" ]; then
    echo "cc.sh: 缺少宿主 seed $HOST(先: make -C \"$ROOT/compiler_c\")" >&2
    exit 2
fi
if [ $# -lt 1 ] || [ ! -f "$1" ]; then
    echo "用法: cc.sh <input.ct>" >&2
    exit 2
fi

IN="$1"
TMP=$(mktemp /tmp/ctron_cc.XXXXXX)
# 换靶:cc.ct 的 main 以字面量 ../selfhosted/input_cc.ct 为 read_file 锚
sed "s|\.\./selfhosted/input_cc\.ct|$IN|" "$DIR/cc.ct" > "$TMP"
"$HOST" run "$TMP"
rc=$?
rm -f "$TMP"
exit $rc
