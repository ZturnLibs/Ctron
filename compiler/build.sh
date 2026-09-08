#!/bin/sh
# build.sh —— 按模块拼接出单文件编译器(宿主 seed 可解释的 .ct)
#
# Ctron 当前为单文件程序模型(无本地多文件模块),故以拼接实现模块化:
#   cc_run.ct  = lex + parse + sem + eval + driver_run   (解释执行)
#   cc_check.ct= lex + parse + sem + eval + driver_check (仅 parse+语义 12 项)
#   cc_emit.ct = lex + parse + sem + eval + trans + driver_emit (发射 C)
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SRC="$DIR/src"
OUT="$DIR/build"
mkdir -p "$OUT"
CORE="$SRC/lex.ct $SRC/parse.ct $SRC/sem.ct $SRC/eval.ct"
cat $CORE "$SRC/driver_run.ct"   > "$OUT/cc_run.ct"
cat $CORE "$SRC/driver_check.ct" > "$OUT/cc_check.ct"
cat $CORE "$SRC/trans.ct" "$SRC/driver_emit.ct" > "$OUT/cc_emit.ct"
echo "build: cc_run=$(wc -l < "$OUT/cc_run.ct") 行 / cc_check=$(wc -l < "$OUT/cc_check.ct") 行 / cc_emit=$(wc -l < "$OUT/cc_emit.ct") 行"
