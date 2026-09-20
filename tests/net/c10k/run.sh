#!/bin/sh
# c10k —— P2-F 门禁一:C10K 并发连接(本地/nightly 专用,不入 CI 主环)。
# 拓扑:本脚本构建 ctecho(emit → cc 链 net 垫片 + ctron_rt.c)与 c_src/driver.c
# 驱动,后台起 CTRON_RT=coro 服务,驱动并发建 N 条连接(全程保持,并发度=N)
# 各做一次 64B 回显。判据 = N/N 全部成功 + 服务存活/fd 无泄漏探活(收尾再
# 单连一轮回显成功)。CI 可选冒烟:C10K_N=100(百连接级,秒级)。
# 主环跳过口径:本目录无 src/main.ct(纯 C 目录,同 rt_*_smoke 守卫),
# tests/net/run.sh 主环按 main.ct 守卫跳过;本脚本独立驱动。
# ulimit 前置:软限默认 256(macOS)先于任何真实瓶颈挡住 10k——脚本尽力
# 抬升(无 sudo 只能到硬限);抬不满则按 上限-128 封顶并发并登记 capped run
# (部分通过口径,明示"满额 10k 需抬高 maxfiles")。
# 用法: run.sh [N]   (N 缺省取 C10K_N,再缺省 10000;budget 秒 = C10K_BUDGET_S 缺省 180)
set -u
DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/std"
[ -x "$EMIT" ] || { echo "c10k: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; }

N=${1:-${C10K_N:-10000}}
BUDGET=${C10K_BUDGET_S:-180}
case "$N" in ''|*[!0-9]*) echo "c10k: 非法 N=$N" >&2; exit 2;; esac

# ── ulimit 尽力抬升(软限 < N+256 时;无 sudo 只能抬到硬限)──
CUR=$(ulimit -n)
case "$CUR" in ''|*[!0-9]*) CUR=1048576 ;; esac   # "unlimited"(Linux 面保险口径)
if [ "$CUR" -lt $((N + 256)) ]; then
    TARGET=$((N + 256))
    HARD=$(ulimit -Hn 2>/dev/null || echo "$CUR")
    if [ "$HARD" != "unlimited" ]; then
        case "$HARD" in
            ''|*[!0-9]*) ;;
            *) [ "$TARGET" -gt "$HARD" ] && TARGET=$HARD ;;
        esac
    fi
    ulimit -n "$TARGET" 2>/dev/null || true
    CUR=$(ulimit -n)
fi
CONC=$N
CAPPED=""
if [ $((CUR - 128)) -lt "$N" ]; then
    CONC=$((CUR - 128))
    CAPPED="capped run(ulimit -n=$CUR 抬不满;满额 $N 需抬高 maxfiles:sudo 或 launchctl limit maxfiles 65536)"
    echo "c10k: $CAPPED"
fi

T=$(mktemp -d)
SRV=""
cleanup() {
    [ -n "$SRV" ] && kill "$SRV" 2>/dev/null
    wait 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

# ── 构建:ctecho(链 rt,coro 生效面)+ 驱动 ──
echo "== c10k: 构建 ctecho(coro)+ driver(N=$N budget=${BUDGET}s) =="
"$EMIT" run "$ROOT/examples/ctecho/src/main.ct" > "$T/ctecho.c" \
    || { echo "c10k: ctecho emit 失败" >&2; exit 1; }
cc -O1 -w -pthread -I"$ROOT/std/net/c_src" -o "$T/ctecho" "$T/ctecho.c" \
    "$ROOT/std/net/c_src/ctron_net.c" "$ROOT/std/net/c_src/ctron_rt.c" \
    || { echo "c10k: ctecho 编译失败" >&2; exit 1; }
cc -O1 -w -o "$T/driver" "$DIR/c_src/driver.c" \
    || { echo "c10k: driver 编译失败" >&2; exit 1; }

# ── 起服务(CTRON_RT=coro;高随机端口 $$ 派生,POSIX sh 无 RANDOM——
#    P1 台账 M-T7-4 同款规避;回环 only;探活就绪 = 驱动单连一轮回显)──
PORT=$(( ($$ % 20000) + 30000 ))
CTECHO_PORT=$PORT CTRON_RT=coro "$T/ctecho" > "$T/srv.log" 2>&1 &
SRV=$!
i=0
while ! "$T/driver" "$PORT" 1 10 >/dev/null 2>&1; do
    if ! kill -0 "$SRV" 2>/dev/null; then
        echo "c10k: 服务未就绪即退出" >&2; sed -n '1,10p' "$T/srv.log" >&2; exit 1
    fi
    sleep 0.2; i=$((i + 1))
    if [ "$i" -gt 50 ]; then
        echo "c10k: 服务 10s 未就绪" >&2; sed -n '1,10p' "$T/srv.log" >&2; exit 1
    fi
done
echo "c10k: ctecho(coro)= 127.0.0.1:$PORT 就绪"

# ── 门禁主体:N 并发建连 + 各一轮回显 ──
"$T/driver" "$PORT" "$CONC" "$BUDGET"
RC=$?

# ── 存活/fd 泄漏探活:负载清空后服务仍在,且还能完成一次全新回显 ──
ALIVE=0
if [ "$RC" -eq 0 ] && kill -0 "$SRV" 2>/dev/null && "$T/driver" "$PORT" 1 15 >/dev/null 2>&1; then
    ALIVE=1
fi
kill "$SRV" 2>/dev/null
SRV=""
wait 2>/dev/null

if [ "$RC" -eq 0 ] && [ "$ALIVE" -eq 1 ]; then
    echo "c10k: PASS(并发 $CONC/$CONC 回显全绿 + 探活绿)$CAPPED"
    exit 0
fi
[ "$RC" -ne 0 ] && echo "c10k: FAIL(驱动 rc=$RC,见上 driver 输出)" >&2
[ "$ALIVE" -ne 1 ] && echo "c10k: FAIL(探活败:服务死亡或新连接不可用)" >&2
sed -n '1,10p' "$T/srv.log" >&2 2>/dev/null || true
exit 1
