#!/bin/sh
# density.sh —— P9 栈经济·生长态密度采样(c10k 载具复用;本地测量件,不入 CI 主环)
# 形:run.sh [N] 全套判据(N/N 回显+探活+fd 判泄漏)在后台跑;本壳外部轮询
# ctecho 服务进程 RSS 取峰值(0.3s 步),运行毕并排打印。
# 用法: density.sh <N> [附加 env 由调用方前置:CTRON_RT_STACK_KB / CTRON_MORESTACK]
# 口径:RSS 为 macOS ps 实测(物理驻留,含共享页);同机 A/B 差分才有意义(bench 家族惯例)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
N=${1:-10000}
MAXRSS=0
sh "$DIR/run.sh" "$N" > "/tmp/c10k_dens_$$.log" 2>&1 &
RUNPID=$!
while kill -0 "$RUNPID" 2>/dev/null; do
    for pid in $(pgrep -f '/ctecho'); do
        R=$(ps -o rss= -p "$pid" 2>/dev/null | tr -d ' ')
        [ -n "$R" ] && [ "$R" -gt "$MAXRSS" ] && MAXRSS=$R
    done
    sleep 0.3
done
wait "$RUNPID"
RC=$?
echo "== density: N=$N max_rss=${MAXRSS}KB run_rc=$RC =="
tail -2 "/tmp/c10k_dens_$$.log"
rm -f "/tmp/c10k_dens_$$.log"
exit "$RC"
