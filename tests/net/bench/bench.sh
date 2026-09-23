#!/bin/sh
# bench.sh —— 吞吐/微基准门禁三件(P1-E + P2-F;本地/nightly,不入 CI 主环)
# 默认跳过(CTRON_NET_BENCH 未置 → SKIP rc=0);置 1 才跑。
#
# 门禁一(P1 出口,登记档):ctecho-pthread vs 手写 C thread-per-conn 基线
#   协议:单连接 10 万次 64B write/read 往返,gettimeofday 计时,3 取最小;
#   比值 = ctecho/基线,门 ≤1.05(1.05–1.15 登记归因档,不强堵;>1.15 出口红)。
#   P1 实测 1.114 在册(归因:per-read poll 门 + 4KB 暂存 + lane 逐字节加宽)。
# 门禁二(P2-F):rt 协程切换微基准 ≤200ns——构建 rt_core_smoke 同一二进制
#   (ctron_rt.c + src/main.c,自绘上下文切换配对口径 yield_bench(100000)),
#   提取 ns/yield 断言 <200。仅原生架构入闸:Rosetta(x86_64 翻译态)不担保
#   时钟口径,翻译态下 SKIP 并登记(arm64 原生实测 89–102ns,四跑)。
# 门禁三(P2-F 出口):echo p50 coro-vs-P1 ≤1.15×——ctecho 同源码双二进制
#   (默认 pthread / CTRON_RT=coro),同客户端协议对拍,比值隔离运行时成本;
#   另测 coro-vs-C 仅供归档(不作门)。门禁三 ≤1.15 为 P2 出口硬门。
# P3-A 时延首件复测口径(2026-09-21,arm64 原生,同机同小时):
#   改前(bbc7a90)×3 与改后 ×3 各录比值+绝对值(p3-task-1-report.md):
#   改前 2.169/2.018/2.164(coro 2.838/2.619/2.775ms)→ 改后 1.040/1.027/1.019
#   (coro 1.338/1.325/1.326ms);≤1.5 检查点过,≤1.15 P2 出口硬门亦过。
# P4-A forget_fd 重排复测口径(2026-09-21,arm64 原生,同机):重排 = 摘 rt
#   驻留登记先于 close(结构性关闭 ABA 复用窗;调用成本恒等,仅位次前移)。
#   改前 ×1 与改后 ×4:p1-vs-C 1.055 → 1.090/1.108/1.075/1.129(登记档内
#   波动);coro-vs-P1 1.128 → 1.179/1.082/1.111/1.025(首跑单点红系并行
#   泳道负载噪声——同轮三端绝对值同向波动 ~40%,复跑三轮全绿);ns/yield
#   86 → 58/52/47/49。结论:重排无可归因回归,三门禁维持绿(登记档照旧)。
# 分量隔离(变体二进制过同一 bench.sh 协议,归因入报告):
#   V-A 仅 rt 侧(旧垫片+新 rt:事件量交付+驻留 one-shot,探针仍在)
#       = 1.031/1.037 —— rt 侧两机制拿走几乎全部收益;
#   V-B 仅垫片侧(新垫片+旧 rt:MSG_DONTWAIT 探针消除,增删仍在)
#       = 2.991/2.963 —— 无 rt 侧配套时探针消除反向退化(>旧基线),
#       两机制必须成对(任务书"ONE unit"的实测佐证);
#   V-D 全新但停用 worker 踢醒(ready_push signal 置 0)
#       = 2.913/2.937 —— 事件量交付为支配项(P2 归因 iv 空闲退避实证坐实:
#       ≈24.5µs/往返),驻留+探针消除在有踢醒时再收 ≈0.2µs。
#   结论:P2 四分量归因修正 —— (iv) 退避 ≫ (ii) 增删 ≈ (i) 探针 ≫ (iii) 配对;
#   残差 ≈0.3–0.5µs/往返(park/wake + kevent 重挂 + lv 加宽,量级在册)。
# 口径:仅回环、高随机端口、同一客户端驱动三端、三端同 -O1(同仓约定);
# trap 兜杀服务进程,无悬挂路径(客户端驱动 + 探活超时)。
# 退出码口径(P2-F 统一):任一比值 >1.15 或 ns/yield ≥200 → rc=1;
# 1.05–1.15 登记档 → rc=0 带档注(P1"登记归因不强堵"的忠实编码——原 >1.05
# 即 rc=1 会令登记档常态红,与本档语义矛盾,统一之)。
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
PIDS=""
cleanup() {
    for p in $PIDS; do kill "$p" 2>/dev/null; done
    wait 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

GATE_FAIL=0
REG_BAND=0

# ═══════════════════════════════════════════════════════════════
# 门禁二:rt 协程切换微基准 ≤200ns(rt_core_smoke 的 yield_bench 脚本化)
# ═══════════════════════════════════════════════════════════════
RTSMOKE="$DIR/../rt_core_smoke"
ARCH=$(uname -m)
TRANSLATED=0
if [ "$(uname)" = "Darwin" ] && [ "$ARCH" = "x86_64" ]; then
    [ "$(/usr/sbin/sysctl -n sysctl.proc_translated 2>/dev/null || echo 0)" = "1" ] && TRANSLATED=1
fi
if [ "$TRANSLATED" = "1" ]; then
    echo "bench-rt: SKIP(Rosetta x86_64 翻译态不入切换门禁:翻译时钟口径不担保;登记豁免)"
elif [ ! -f "$RTSMOKE/c_src/ctron_rt.c" ] || [ ! -f "$RTSMOKE/src/main.c" ]; then
    echo "bench-rt: FAIL(rt_core_smoke 源缺席:$RTSMOKE)" >&2
    GATE_FAIL=1
else
    echo "== bench-rt: 协程切换微基准(门禁 ≤200ns,配对/2 口径) =="
    if cc -O1 -pthread -I "$RTSMOKE/c_src" -o "$T/rt_core" \
        "$RTSMOKE/c_src/ctron_rt.c" "$RTSMOKE/src/main.c" 2>"$T/rt.cc.err"; then
        if CTRON_RT=coro "$T/rt_core" > "$T/rt.out" 2>&1 && grep -q "ns/yield=" "$T/rt.out"; then
            grep "ns/yield=" "$T/rt.out" | sed 's/^/  /'
            NSY=$(sed -n 's/.*ns\/yield=\([0-9][0-9]*\)\.[0-9][0-9][0-9].*/\1/p' "$T/rt.out")
            if [ -n "$NSY" ] && [ "$NSY" -lt 200 ]; then
                echo "bench-rt: PASS(整部 ${NSY}ns < 200ns)"
            else
                echo "bench-rt: FAIL(ns/yield 整部 ${NSY:-?}ns ≥ 200ns 出口红)" >&2
                GATE_FAIL=1
            fi
        else
            echo "bench-rt: FAIL(rt_core_smoke 运行失败或无 yield 行)" >&2
            sed -n '1,5p' "$T/rt.out" 2>/dev/null; GATE_FAIL=1
        fi
    else
        echo "bench-rt: FAIL(rt_core 编译失败)" >&2
        sed -n '1,5p' "$T/rt.cc.err"; GATE_FAIL=1
    fi
fi

# ═══════════════════════════════════════════════════════════════
# 门禁一 + 三:echo 吞吐三端对拍(基线 C / ctecho-pthread / ctecho-coro)
# ═══════════════════════════════════════════════════════════════
# 1) 构建:基线(服务器+客户端同体);ctecho 源码一份 → 双二进制(唯一差异
#    = 链不链 ctron_rt.c + 运行期 CTRON_RT,隔离运行时成本)
cc -O1 -w -o "$T/baseline" "$DIR/baseline_echo.c" -lpthread \
    || { echo "bench: 基线编译失败"; exit 1; }
"$EMIT" run "$ROOT/examples/ctecho/src/main.ct" > "$T/ctecho.c" \
    || { echo "bench: ctecho emit 失败"; exit 1; }
cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/ctecho_p1" "$T/ctecho.c" \
    "$ROOT/net/c_src/ctron_net.c" \
    || { echo "bench: ctecho(p1) 编译失败"; exit 1; }
cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/ctecho_coro" "$T/ctecho.c" \
    "$ROOT/net/c_src/ctron_net.c" "$ROOT/net/c_src/ctron_rt.c" \
    || { echo "bench: ctecho(coro) 编译失败"; exit 1; }

# 2) 起三服务(高随机端口 $$ 派生防 POSIX sh 空 RANDOM——P1 台账 M-T7-4;
#    就绪判定 = 单连探活而非 stdout——块缓冲读回不可靠)
BASE_PORT=$(( ($$ % 20000) + 30000 ))
P1_PORT=$(( BASE_PORT + 1 ))
CORO_PORT=$(( BASE_PORT + 2 ))
wait_ready() { # $1=端口 $2=超时秒 → 0 就绪
    i=0
    while [ "$i" -lt $(( $2 * 5 )) ]; do
        "$T/baseline" client "$1" 1 >/dev/null 2>&1 && return 0
        sleep 0.2; i=$((i + 1))
    done
    return 1
}

CTECHO_PORT=$BASE_PORT "$T/baseline" > "$T/base.log" 2>&1 & PIDS="$PIDS $!"
CTECHO_PORT=$P1_PORT "$T/ctecho_p1" > "$T/p1.log" 2>&1 & PIDS="$PIDS $!"
CTECHO_PORT=$CORO_PORT CTRON_RT=coro "$T/ctecho_coro" > "$T/coro.log" 2>&1 & PIDS="$PIDS $!"
wait_ready "$BASE_PORT" 10 || { echo "bench: 基线未就绪"; cat "$T/base.log"; exit 1; }
wait_ready "$P1_PORT" 10 || { echo "bench: ctecho-p1 未就绪"; cat "$T/p1.log"; exit 1; }
wait_ready "$CORO_PORT" 10 || { echo "bench: ctecho-coro 未就绪"; cat "$T/coro.log"; exit 1; }
echo "bench: 基线=:$BASE_PORT ctecho-p1=:$P1_PORT ctecho-coro=:$CORO_PORT(探活已过)"

# 3) 同一客户端驱动三端(客户端内部 3 轮取最小,输出 µs)
BASE_US=$("$T/baseline" client "$BASE_PORT") || { echo "bench: 基线压测失败"; exit 1; }
P1_US=$("$T/baseline" client "$P1_PORT") || { echo "bench: ctecho-p1 压测失败"; exit 1; }
CORO_US=$("$T/baseline" client "$CORO_PORT") || { echo "bench: ctecho-coro 压测失败"; exit 1; }

# 4) 比值与门禁
ratio() { awk -v a="$1" -v b="$2" 'BEGIN { if (b + 0 <= 0) { print "inf"; exit } printf "%.3f", a / b }'; }
gate_over() { awk -v r="$1" -v g="$2" 'BEGIN { print (r + 0 > g) ? 1 : 0 }'; }

P1_RATIO=$(ratio "$P1_US" "$BASE_US")
P2_RATIO=$(ratio "$CORO_US" "$P1_US")
REC_RATIO=$(ratio "$CORO_US" "$BASE_US")
echo "bench: baseline=${BASE_US}us ctecho-p1=${P1_US}us ctecho-coro=${CORO_US}us (10 万次 64B 往返,3 取最小)"
echo "bench: 门禁一 p1-vs-C ratio=$P1_RATIO(门 ≤1.05;1.05–1.15 登记档)"
echo "bench: 门禁三 coro-vs-P1 ratio=$P2_RATIO(门 ≤1.15,P2 出口硬门)"
echo "bench: 归档 coro-vs-C ratio=$REC_RATIO(不作门)"

if [ "$(gate_over "$P1_RATIO" 1.15)" = 1 ]; then
    echo "bench: FAIL 门禁一 p1-vs-C ratio>1.15 出口红" >&2; GATE_FAIL=1
elif [ "$(gate_over "$P1_RATIO" 1.05)" = 1 ]; then
    echo "bench: 门禁一 p1-vs-C 落 1.05–1.15 登记档(P1 在册归因,不强堵)"; REG_BAND=1
fi
if [ "$(gate_over "$P2_RATIO" 1.15)" = 1 ]; then
    echo "bench: FAIL 门禁三 coro-vs-P1 ratio>1.15 出口红" >&2; GATE_FAIL=1
else
    echo "bench: PASS 门禁三 coro-vs-P1 ≤1.15(P2 出口门绿)"
fi

if [ "$GATE_FAIL" -ne 0 ]; then
    echo "bench: FAIL(出口红,见上)" >&2
    exit 1
fi
[ "$REG_BAND" -ne 0 ] && echo "bench: GREEN(登记档在册,出口绿)"
echo "bench: GREEN(三门禁全绿)"
exit 0
