#!/bin/sh
# bench_cycle.sh —— P6-F 全请求周期回环基准 + 热路径 no_alloc 断言
# (门禁表 spec §九:「HTTP 全请求周期 vs 手写 C epoll ≤1.05×(硬)|P6」
#  「热路径分配 no_alloc 断言零违例|P6」;P4 遗留承诺本波兑现)
# 本地/nightly 门禁,不入 CI 主环;CTRON_CYCLE_BENCH=1 启用(bench 家族惯例)。
#
# 口径:
#   - 协议 = HTTP GET → 定长 83 字节响应 → close(P6-E 生产形态,
#     Connection:close 一连接一请求;keep-alive 志向在册)。两侧响应逐字节
#     同文,digest pin(FNV-1a 64,同源 C 客户端累计)不等即 FAIL 禁采数。
#   - 四端同测:ctron 默认 RT / ctron coro RT(CTRON_RT=coro;归档不作门,
#     P2「coro-vs-C 仅供归档」先例)/ C 阻塞串行 / C kqueue·epoll 事件环
#     (spec §九字面口径;darwin=kqueue,linux=epoll)。对照 = min(两 C 端)
#     —— 最强诚实基线。
#   - 计时 = 同一 C 客户端(baseline_cycle client)驱动全部服务端;热身
#     200 轮不计时 + 正式 N 轮(env CTRON_CYCLE_BENCH_N 缺省 10000),
#     CLOCK_MONOTONIC 整段;采数 ×3 各取最小(bench.sh 家族惯例);客户端
#     SO_LINGER(1,0) RST 收尾 → 全程零 TIME_WAIT(两侧同口径,长跑免端口
#     耗尽;响应字节在 RST 前已按序送达)。
#   - 门禁一(全请求周期):ratio = ctron_min / C_min,门 ≤1.05(硬)。
#     编码沿 tests/net/bench.sh 登记档语义:≤1.05 GREEN;1.05–1.15 登记档
#     rc=0 带档注(回环压测并行泳道噪声在案,P4-A 实测同向波动 ~40%);
#     >1.15 RED rc=1。coro 比值照录归档。
#   - 门禁二(no_alloc):热路径按构造零分配(bench_cycle.ct 循环体零 Box、
#     响应 const 定长串 lane 直填、无 Str 拼接),堵漏证明 = 计数构建
#     (awk 锚 ctron_amalloc 首行注计数器;锚不命中 = 发射形漂移,FAIL rc=2)
#     双跑差分:A 跑(热身 500 + 停机)与 B 跑(热身 500 + N2 次健康请求 +
#     停机,N2=CTRON_CYCLE_NOALLOC_N 缺省 100000),bump 计数差 = B − A
#     即稳态分配调用数,门 = 0。口径注:net 垫片服务路径无堆分配(grep
#     实证,DNS malloc 仅 connect 面);coro 栈 mmap 属连接生命周期,不入
#     热路径门。
# 退出码:0 = 门内(登记档含档注)/ 1 = 超门 / 2 = 环境缺件。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}"   # 覆盖口:worktree 隔离构建验证用
export CTRON_STDPATH="$ROOT/std"

if [ "${CTRON_CYCLE_BENCH:-}" != "1" ]; then
    echo "bench-cycle: SKIP(置 CTRON_CYCLE_BENCH=1 启用;本地/nightly 门禁,不入 CI 主环)"
    exit 0
fi
[ -x "$EMIT" ] || { echo "bench-cycle: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; }
command -v cc >/dev/null || { echo "bench-cycle: 缺 cc" >&2; exit 2; }

T=$(mktemp -d)
PIDS=""
killall_srv() {
    for p in $PIDS; do kill "$p" 2>/dev/null; done
    sleep 0.2
    for p in $PIDS; do kill -9 "$p" 2>/dev/null; done
    wait 2>/dev/null
    rm -rf "$T"
}
cleanup() {
    killall_srv
}
trap cleanup EXIT INT TERM

PORT=${BENCH_CYCLE_PORT:-$(awk 'BEGIN{srand();print 21000+int(rand()*20000)}')}
N="${CTRON_CYCLE_BENCH_N:-10000}"
N2="${CTRON_CYCLE_NOALLOC_N:-100000}"
GATE_FAIL=0

# ── 构建 ──
echo "== bench-cycle: 全请求周期(P6-F,N=$N ×3 取最小;端口 $PORT)=="
"$EMIT" run "$DIR/bench_cycle.ct" > "$T/ctron.c" 2>"$T/ctron.err" \
    || { echo "bench-cycle: FAIL emit"; sed -n '1,5p' "$T/ctron.err"; exit 2; }
cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/ctron.bin" "$T/ctron.c" "$ROOT/net/c_src/ctron_net.c" 2>"$T/ctron.cc.err" \
    || { echo "bench-cycle: FAIL ctron 构建"; sed -n '1,5p' "$T/ctron.cc.err"; exit 2; }
cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/ctron_coro.bin" "$T/ctron.c" "$ROOT/net/c_src/ctron_net.c" "$ROOT/net/c_src/ctron_rt.c" 2>>"$T/ctron.cc.err" \
    || { echo "bench-cycle: FAIL coro 构建"; sed -n '1,5p' "$T/ctron.cc.err"; exit 2; }
cc -O1 -w -o "$T/base.bin" "$DIR/baseline_cycle.c" 2>"$T/base.cc.err" \
    || { echo "bench-cycle: FAIL C 基线构建"; sed -n '1,5p' "$T/base.cc.err"; exit 2; }

start_srv() { # $1=bin $2=mode:空=默认RT | coro(CTRON_RT=coro) | serve | serve-reactor(base.bin argv)
    case "${2:-}" in
        coro)               env CTRON_RT=coro BENCH_CYCLE_PORT="$PORT" "$1" > "$T/srv.log" 2>&1 & ;;
        serve|serve-reactor) env BENCH_CYCLE_PORT="$PORT" "$1" "$2" "$PORT" > "$T/srv.log" 2>&1 & ;;
        *)                  env BENCH_CYCLE_PORT="$PORT" "$1" > "$T/srv.log" 2>&1 & ;;
    esac
    PIDS="$PIDS $!"
    local i=0
    while [ "$i" -lt 50 ]; do
        timeout 30 "$T/base.bin" client "$PORT" 1 >/dev/null 2>&1 && return 0
        sleep 0.1; i=$((i+1))
    done
    echo "bench-cycle: FAIL 服务未就绪(mode=${2:-})"
    for p in $PIDS; do
        if kill -0 "$p" 2>/dev/null; then
            kill -9 "$p" 2>/dev/null
            wait "$p" 2>/dev/null
            echo "  诊断: pid $p 活但不应答(已强杀)"
        else
            wait "$p" 2>/dev/null
            echo "  诊断: pid $p 已退出 rc=$?"
        fi
    done
    sed -n '1,8p' "$T/srv.log"
    return 1
}
stop_srv() {
    for p in $PIDS; do kill "$p" 2>/dev/null; done
    sleep 0.2
    for p in $PIDS; do kill -9 "$p" 2>/dev/null; done
    wait 2>/dev/null
    PIDS=""
}

client_round() { # $1=rounds → 全局 OUT/NS/DG
    OUT=$(timeout 120 "$T/base.bin" client "$PORT" "$1" 2>/dev/null)
    NS=$(printf '%s\n' "$OUT" | sed -n 's/^ns_per_req=//p')
    DG=$(printf '%s\n' "$OUT" | sed -n 's/^digest=//p')
}

# ── digest pin(两侧响应逐字节同文才可对拍)──
start_srv "$T/ctron.bin" || exit 2
client_round 300
DIG_CT="$DG"
stop_srv
start_srv "$T/base.bin" serve || exit 2
client_round 300
DIG_C="$DG"
stop_srv
if [ -z "$DIG_CT" ] || [ -z "$DIG_C" ] || [ "$DIG_CT" != "$DIG_C" ] || [ "$DIG_CT" = "cbf29ce484222325" ]; then
    echo "bench-cycle: FAIL 响应字节 digest 不一致/空(ctron=$DIG_CT base=$DIG_C)—— 禁止采数"
    exit 1
fi
echo "  digest pin: $DIG_CT(两侧同字节,83B/resp)"

# ── 门禁一:四端 ×3 采数 ──
bench_side() { # $1=名 $2=bin $3=mode → 全局 MIN
    start_srv "$2" "$3" || exit 2
    MIN=""; R=1
    while [ "$R" -le 3 ]; do
        client_round "$N"
        if [ -z "$NS" ] || [ "$NS" = "0" ]; then
            echo "bench-cycle: FAIL $2 round $R 采数空/零"; GATE_FAIL=1; stop_srv; return 1
        fi
        if [ -z "$MIN" ] || [ "$NS" -lt "$MIN" ]; then MIN="$NS"; fi
        R=$((R+1))
    done
    stop_srv
    echo "  $1: min ${MIN} ns/req"
}

bench_side "ctron 默认RT" "$T/ctron.bin" ""
MIN_CT="$MIN"
bench_side "C 阻塞串行 " "$T/base.bin" serve
MIN_CS="$MIN"
bench_side "C 事件环  " "$T/base.bin" serve-reactor
MIN_CR="$MIN"
bench_side "ctron coro  " "$T/ctron_coro.bin" coro
MIN_CO="$MIN"

if [ "$GATE_FAIL" = "0" ]; then
    CMIN="$MIN_CS"
    [ "$MIN_CR" -lt "$CMIN" ] && CMIN="$MIN_CR"
    RATIO=$(awk -v c="$MIN_CT" -v b="$CMIN" 'BEGIN { printf "%.3f", c / b }')
    echo "  对照 = min(C 阻塞 ${MIN_CS} | C 事件环 ${MIN_CR}) = ${CMIN} ns/req"
    echo "  ratio ctron/C = ${RATIO}x"
    VERDICT=$(awk -v r="$RATIO" 'BEGIN { print (r <= 1.05) ? "GREEN" : (r <= 1.15) ? "BAND" : "RED" }')
    case "$VERDICT" in
        GREEN) echo "  门禁一 ≤1.05(硬): PASS" ;;
        BAND)  echo "  门禁一 ≤1.05(硬): 登记档(${RATIO}x ∈ 1.05–1.15;回环并行泳道噪声在册,P4-A ~40% 同向波动先例)——复跑口径,rc=0" ;;
        RED)   echo "  门禁一 ≤1.05(硬): RED(${RATIO}x > 1.15 出口红)—— 数字照录,登记归文档" ; GATE_FAIL=1 ;;
    esac
    RATIO_CO=$(awk -v c="$MIN_CO" -v b="$CMIN" 'BEGIN { printf "%.3f", c / b }')
    echo "  [归档] coro-vs-C = ${RATIO_CO}x(coro ${MIN_CO} ns/req;不作门,P2 先例)"
fi

# ── 门禁二:no_alloc(计数构建双跑差分)──
echo "== bench-cycle: no_alloc 热路径断言(A/B 双跑差分,门 = 0)=="
ANCHOR='static void* ctron_amalloc(size_t n) { n = (n + 15)'
if [ "$(grep -cF "$ANCHOR" "$T/ctron.c")" -lt 1 ]; then
    echo "bench-cycle: FAIL 发射形漂移(ctron_amalloc 锚不命中)—— 计数注入拒行,归编译泳道核查"
    exit 2
fi
awk -v anchor="$ANCHOR" '
!done && index($0, anchor) == 1 {
    pre = "static void* ctron_amalloc(size_t n) { "
    print "static unsigned long ctron_bump_n = 0;"
    print "static void ctron_bump_rep(void) { fprintf(stderr, \"bump_calls=%lu\\n\", ctron_bump_n); }"
    print "__attribute__((constructor)) static void ctron_bump_ctor(void) { atexit(ctron_bump_rep); }"
    print pre "ctron_bump_n += 1; " substr($0, length(pre) + 1)
    done = 1; next
}
{ print }
' "$T/ctron.c" > "$T/ctron_cnt.c"
grep -q "ctron_bump_n += 1" "$T/ctron_cnt.c" || { echo "bench-cycle: FAIL 计数注入未生效"; exit 2; }
cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/ctron_cnt.bin" "$T/ctron_cnt.c" "$ROOT/net/c_src/ctron_net.c" 2>"$T/cnt.cc.err" \
    || { echo "bench-cycle: FAIL 计数构建"; sed -n '1,5p' "$T/cnt.cc.err"; exit 2; }

cnt_run() { # $1=健康请求数 → 全局 BUMP
    start_srv "$T/ctron_cnt.bin" || { GATE_FAIL=1; return 1; }
    client_round 500 >/dev/null
    client_round "$1" >/dev/null
    timeout 10 "$T/base.bin" client "$PORT" 1 /__done >/dev/null 2>&1
    for p in $PIDS; do
        local i=0
        while [ "$i" -lt 25 ] && kill -0 "$p" 2>/dev/null; do
            sleep 0.2; i=$((i+1))
        done
        kill "$p" 2>/dev/null
        wait "$p" 2>/dev/null
    done
    PIDS=""
    BUMP=$(sed -n 's/^bump_calls=//p' "$T/srv.log")
}

cnt_run 0
BUMP_A="$BUMP"
cnt_run "$N2"
BUMP_B="$BUMP"
if [ -z "$BUMP_A" ] || [ -z "$BUMP_B" ]; then
    echo "bench-cycle: FAIL bump 计数缺失(A=$BUMP_A B=$BUMP_B)"; GATE_FAIL=1
else
    DELTA=$((BUMP_B - BUMP_A))
    if [ "$DELTA" -eq 0 ]; then
        echo "  no_alloc: PASS($N2 请求稳态 bump 计数差 = 0;A=$BUMP_A B=$BUMP_B 含启动)"
    else
        PERREQ=$(awk -v d="$DELTA" -v n="$N2" 'BEGIN { printf "%.4f", d / n }')
        echo "  no_alloc: FAIL(稳态 bump 差 = $DELTA ≈ ${PERREQ}/req)—— 热路径分配实证,登记归文档"
        GATE_FAIL=1
    fi
fi

echo "== bench-cycle 小结:GATE_FAIL=$GATE_FAIL(0=门内) =="
exit $GATE_FAIL
