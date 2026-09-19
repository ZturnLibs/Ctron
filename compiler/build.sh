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
# 核心模块按编译管道分段(段内文件沿原单文件的相对顺序,拼接序即语义序):
#   lex       词法器
#   parse_*   解析器:树节点 → 表达式 → 语句 → 声明 → 模块加载
#   sem_*     语义 12 项 + 类型/调用/comptime/闭包检查(主控 sem_main.ct)
#   eval_*    树行走求值器:值域 → 环境/方法/模式 → 表达式 → 调用 → 语句
#   trans_*   C 代码生成器:类型基础 → 表达式 → 语句 → 函数/样板(仅 cc_emit)
CORE="$SRC/lex.ct \
$SRC/diag_msg.ct \
$SRC/parse_node.ct $SRC/parse_expr.ct $SRC/parse_stmt.ct $SRC/parse_decl.ct $SRC/gui_parse.ct $SRC/parse_pkg.ct \
$SRC/sem_walk.ct $SRC/sem_send.ct $SRC/sem_own.ct $SRC/sem_pure.ct $SRC/sem_spawn.ct \
$SRC/sem_move.ct $SRC/sem_exh.ct $SRC/sem_alloc.ct $SRC/sem_main.ct $SRC/sem_type.ct \
$SRC/sem_calls.ct $SRC/sem_comptime.ct $SRC/sem_ceval.ct $SRC/sem_closure.ct \
$SRC/eval_val.ct $SRC/eval_width.ct $SRC/eval_float.ct $SRC/eval_env.ct $SRC/eval_trait.ct \
$SRC/eval_pat.ct $SRC/eval_expr.ct $SRC/eval_call.ct $SRC/eval_run.ct"
TRANS="$SRC/trans_ty.ct $SRC/trans_expr.ct $SRC/trans_stmt.ct $SRC/trans_conc.ct $SRC/trans_emit.ct"
cat $CORE "$SRC/driver_run.ct"   > "$OUT/cc_run.ct"
cat $CORE "$SRC/driver_check.ct" > "$OUT/cc_check.ct"
cat $CORE $TRANS "$SRC/driver_emit.ct" > "$OUT/cc_emit.ct"
# ANCHORVERSION 注入:三产物同源版本串(黄金语料不含该锚,逐字不受影响)
VER=$(git -C "$DIR/.." describe --tags --always 2>/dev/null || echo "0.0.1-dev")
for P in cc_run cc_check cc_emit; do
    sed "s|ANCHORVERSION|$VER|" "$OUT/$P.ct" > "$OUT/$P.ct.tmp" && mv "$OUT/$P.ct.tmp" "$OUT/$P.ct"
done
echo "build: cc_run=$(wc -l < "$OUT/cc_run.ct") 行 / cc_check=$(wc -l < "$OUT/cc_check.ct") 行 / cc_emit=$(wc -l < "$OUT/cc_emit.ct") 行"
