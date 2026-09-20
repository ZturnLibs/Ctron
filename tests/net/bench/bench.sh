#!/bin/sh
# bench.sh —— P1-E 吞吐对比门禁:ctecho(被测)vs 手写 C thread-per-conn 基线
# 默认跳过(CTRON_NET_BENCH 未置 → SKIP rc=0);置 1 才跑(本地/nightly,不入 CI 主环)。
# 协议(brief Task 8):单连接 10 万次 64B write/read 往返,gettimeofday 计时,
# 3 取最小;比值 = ctecho/基线,门禁 ≤1.05(1.05–1.15 登记归因;>1.15 出口红)。
# 口径:仅回环、内核分配端口(:0 读回)、同一客户端驱动双端、双双 -O1(同仓约定);
# trap 兜杀服务进程,无悬挂路径(客户端驱动 + wait_port 超时)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/std"

if [ "${CTRON_NET_BENCH:-}" != "1" ]; then
    echo "bench: SKIP(置 CTRON_NET_BENCH=1 启用;本地/nightly 门禁,不入 CI 主环)"
    exit 0
fi
[ -x "$EMIT" ] || { echo "bench: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; }

T=$(mktemp -d)
BASE_PID=""; CTE_PID=""
cleanup() {
    [ -n "$BASE_PID" ] && kill "$BASE_PID" 2>/dev/null
    [ -n "$CTE_PID" ] && kill "$CTE_PID" 2>/dev/null
    wait 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

# 1) 构建:基线(服务器+客户端同体,cc -lpthread);ctecho(emit → cc 链垫片)
cc -O1 -w -o "$T/baseline" "$DIR/baseline_echo.c" -lpthread \
    || { echo "bench: 基线编译失败"; exit 1; }
"$EMIT" run "$ROOT/examples/ctecho/src/main.ct" > "$T/ctecho.c" \
    || { echo "bench: ctecho emit 失败"; exit 1; }
cc -O1 -w -o "$T/ctecho" "$T/ctecho.c" "$ROOT/std/net/c_src/ctron_net.c" \
    || { echo "bench: ctecho 编译失败"; exit 1; }

# 2) 起服务(高随机端口,回环 only;就绪判定 = 连接探活而非 stdout——
#    ctecho 的 println 重定向到文件是块缓冲,读回端口不可靠)
BASE_PORT=$(( (RANDOM % 20000) + 30000 ))
CTE_PORT=$(( (RANDOM % 20000) + 30000 ))
if [ "$CTE_PORT" = "$BASE_PORT" ]; then CTE_PORT=$(( CTE_PORT + 1 )); fi
wait_ready() { # $1=端口 $2=超时秒 → 0 就绪
    i=0
    while [ "$i" -lt $(( $2 * 5 )) ]; do
        "$T/baseline" client "$1" 1 >/dev/null 2>&1 && return 0
        sleep 0.2; i=$((i + 1))
    done
    return 1
}

CTECHO_PORT=$BASE_PORT "$T/baseline" > "$T/base.log" 2>&1 & BASE_PID=$!
wait_ready "$BASE_PORT" 10 || { echo "bench: 基线未就绪"; cat "$T/base.log"; exit 1; }
CTECHO_PORT=$CTE_PORT "$T/ctecho" > "$T/cte.log" 2>&1 & CTE_PID=$!
wait_ready "$CTE_PORT" 10 || { echo "bench: ctecho 未就绪"; cat "$T/cte.log"; exit 1; }
echo "bench: 基线=127.0.0.1:$BASE_PORT ctecho=127.0.0.1:$CTE_PORT(探活已过)"

# 3) 同一客户端驱动双端(客户端内部 3 轮取最小,输出 µs)
BASE_US=$("$T/baseline" client "$BASE_PORT") || { echo "bench: 基线压测失败"; exit 1; }
CTE_US=$("$T/baseline" client "$CTE_PORT") || { echo "bench: ctecho 压测失败"; exit 1; }

# 4) 比值与门禁(≤1.05)
RATIO=$(awk -v c="$CTE_US" -v b="$BASE_US" \
    'BEGIN { if (b + 0 <= 0) { print "inf"; exit } printf "%.3f", c / b }')
echo "bench: baseline=${BASE_US}us ctecho=${CTE_US}us (10 万次 64B 往返,3 取最小)"
echo "bench: ratio=$RATIO gate<=1.05"
if [ "$(awk -v r="$RATIO" 'BEGIN { print (r + 0 <= 1.05) ? 1 : 0 }')" = 1 ]; then
    echo "bench: PASS(≤1.05,P1 出口门禁绿)"
    exit 0
fi
echo "bench: FAIL(>1.05;1.05–1.15 登记归因不强堵,>1.15 出口红)" >&2
exit 1
