#!/bin/sh
# bench_tls.sh —— P3-D 握手吞吐门禁四件(本地/nightly,不入 CI 主环;bench.sh 同门)
# 默认跳过(CTRON_NET_BENCH 未置 → SKIP rc=0);置 1 才跑。
#
# 口径:整握手周期 = connect + TLS 握手 + 一笔 GET/应答 + 关闭,双方打同一
#   openssl s_server 实例(-www -quiet -tls1_2,同协议对端),比值只隔离客户端
#   栈成本。我方 = tls_interop/src/main.ct client 模式(INTEROP_LOOP 批处理,
#   与互操作矩阵同一代码路径 —— 夹具即基准,无第二实现)。
# 基线 = openssl s_client 每握手一进程(LibreSSL s_client 无进程内全握手循环
#   模式;-reconnect 是会话恢复非全握手)→ 基线侧计入进程 exec 成本,方向性
#   利于我方。为不作糊账,另录两枚对照(归档,不作门):
#     ① 同构 spawn-per-handshake:我方二进制亦每握手一进程(双侧同付 exec),
#        比值隔离「exec + 栈」的对称口径;
#     ② 绝对值:两侧 µs/整握手。
# 门禁(任务书口径):ratio = ours_batch / baseline ≤ 1.5 硬门,>1.5 出口红;
#   本件【无】登记档(与 bench.sh 门禁一不同 —— 设计门即硬 ≤1.5,>1.5 必红)。
# 验证策略对齐(比值不混验证策略差):双方 -CAfile 同一张自签证书(链验证开),
#   均不做主机名匹配(s_client 无 -verify_hostname = 我方 IP opt-out 同口径)。
# 计时:perl Time::HiRes(macOS 原生)µs 级,双侧同法外测;3 取最小(bench.sh
#   「3 取最小」同款)。N 默认 2000(CTRON_TLS_BENCH_N 可调,下限 200 供开发
#   冒烟;nightly 口径 2000 = 任务书下限)。
# 面:默认 pthread 面(门禁隔离 TLS 栈成本;coro 停车成本由 bench.sh 门禁三在
#   echo 面覆盖,协程面互操作行为由 tls_interop/run.sh 双面矩阵覆盖)。
# 纪律:仅回环、高随机端口($$ 派生,M-T7-4)、零外联;trap 兜杀 s_server。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/std"
OPENSSL="${CTRON_INTEROP_OPENSSL:-/usr/bin/openssl}"

if [ "${CTRON_NET_BENCH:-}" != "1" ]; then
    echo "bench-tls: SKIP(置 CTRON_NET_BENCH=1 启用;本地/nightly 门禁,不入 CI 主环)"
    exit 0
fi
[ -x "$EMIT" ] || { echo "bench-tls: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; }
[ -x "$OPENSSL" ] || { echo "bench-tls: 缺 openssl($OPENSSL)" >&2; exit 2; }

N="${CTRON_TLS_BENCH_N:-2000}"
[ "$N" -ge 200 ] || N=200

T=$(mktemp -d)
PIDS=""
cleanup() {
    for p in $PIDS; do kill "$p" 2>/dev/null || true; done
    wait 2>/dev/null || true
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

# ---- 证书 + 构建(同 tls_interop/run.sh:自签一张,单二进制 client 模式) ----
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout "$T/key.pem" -out "$T/cert.pem" -days 2 -subj "/CN=127.0.0.1" >/dev/null 2>&1
cp "$T/cert.pem" "$T/ca.pem"
"$EMIT" run "$DIR/../tls_interop/src/main.ct" > "$T/main.c" \
    || { echo "bench-tls: emit 失败"; exit 1; }
printf '#define MBEDTLS_CONFIG_FILE "config-thread.h"\n' > "$T/mbcfg.h"
cc -O1 -w -pthread -I"$ROOT/std/net/c_src" \
   -I"$ROOT/vendor/tls" -I"$ROOT/vendor/tls/mbedtls/include" -include "$T/mbcfg.h" \
   -o "$T/bench_tls_cli" "$T/main.c" \
   "$ROOT/std/net/c_src/ctron_net.c" "$ROOT/std/net/c_src/ctron_rt.c" "$ROOT/std/net/c_src/ctron_tls.c" \
   "$ROOT/vendor/tls/build/lib/libmbedtls.a" \
   "$ROOT/vendor/tls/build/lib/libmbedx509.a" \
   "$ROOT/vendor/tls/build/lib/libmbedcrypto.a" -lpthread \
   || { echo "bench-tls: 编译失败"; exit 1; }
BIN="$T/bench_tls_cli"

# ---- 对端:一个 s_server 实例服全场(-www 应答一笔即关,串行 accept) ----
PORT=$(( ($$ % 20000) + 30000 ))
"$OPENSSL" s_server -accept "$PORT" -www -quiet -tls1_2 \
    -cert "$T/cert.pem" -key "$T/key.pem" > "$T/srv.log" 2>&1 & PIDS="$PIDS $!"

OSSL_V=$("$OPENSSL" version)
echo "bench-tls: openssl = $OSSL_V;对端 s_server -www -quiet -tls1_2 :$PORT;N=$N 整握手 × 3 取最小"

# 就绪:真客户端探活(非 stdout,bench.sh 口径);同时验证整路径
i=0
while [ "$i" -lt 50 ]; do
    if INTEROP_ROLE=client INTEROP_PORT="$PORT" INTEROP_ALPN= INTEROP_LOOP=1 \
        CTRON_SMOKE_CA="$T/ca.pem" "$BIN" >/dev/null 2>&1; then
        break
    fi
    sleep 0.1; i=$((i + 1))
done
if [ "$i" = 50 ]; then echo "bench-tls: FAIL(s_server 未就绪)"; sed -n '1,5p' "$T/srv.log"; exit 1; fi

now_us() { perl -MTime::HiRes=time -e 'printf "%.0f\n", time() * 1e6'; }

# ---- 我方批处理:单进程 N 整握手(connect+握手+GET/应答+close) ----
run_ours_batch() {
    INTEROP_ROLE=client INTEROP_PORT="$PORT" INTEROP_ALPN= INTEROP_LOOP="$N" \
        CTRON_SMOKE_CA="$T/ca.pem" "$BIN" >/dev/null 2>&1
}
# ---- 对照①:我方 spawn-per-handshake(双侧同付 exec) ----
run_ours_spawn() {
    i=0
    while [ "$i" -lt "$N" ]; do
        INTEROP_ROLE=client INTEROP_PORT="$PORT" INTEROP_ALPN= INTEROP_LOOP=1 \
            CTRON_SMOKE_CA="$T/ca.pem" "$BIN" >/dev/null 2>&1 || return 1
        i=$((i + 1))
    done
}
# ---- 基线:s_client 每握手一进程(-CAfile 同证书,链验证开;无主机名匹配)
#      登记桩:LibreSSL 3.3.6 s_client -quiet 下 stdout 指 /dev/null 必现
#      "poll error" 退 1(实证,/tmp 对照;指文件则净退 0)→ 重定向往文件。
run_baseline() {
    i=0
    while [ "$i" -lt "$N" ]; do
        printf 'GET /bench HTTP/1.0\r\n\r\n' | "$OPENSSL" s_client \
            -connect "127.0.0.1:$PORT" -quiet -tls1_2 -CAfile "$T/ca.pem" \
            >"$T/base_cli.out" 2>&1 || return 1
        i=$((i + 1))
    done
}

best3() { # $1=侧名 $2=命令(经 eval)→ 三跑取最小 µs,stdout;败 → 空
    _name="$1"; _cmd="$2"; _best=0; _r=0
    while [ "$_r" -lt 3 ]; do
        _t0=$(now_us)
        if ! eval "$_cmd"; then
            echo "bench-tls: FAIL($_name 第 $((_r + 1)) 轮运行失败)" >&2
            return 1
        fi
        _t1=$(now_us)
        _d=$((_t1 - _t0))
        if [ "$_best" = 0 ] || [ "$_d" -lt "$_best" ]; then _best=$_d; fi
        _r=$((_r + 1))
    done
    echo "$_best"
}

OURS_BEST=$(best3 ours_batch run_ours_batch) || exit 1
SPAWN_BEST=$(best3 ours_spawn run_ours_spawn) || exit 1
BASE_BEST=$(best3 baseline run_baseline) || exit 1
[ "$BASE_BEST" -gt 0 ] || { echo "bench-tls: FAIL(基线计时段为空)"; exit 1; }

ratio() { awk -v a="$1" -v b="$2" 'BEGIN { if (b + 0 <= 0) { print "inf"; exit } printf "%.3f", a / b }'; }
gate_over() { awk -v r="$1" -v g="$2" 'BEGIN { print (r + 0 > g) ? 1 : 0 }'; }
per_hs() { awk -v a="$1" -v b="$2" 'BEGIN { printf "%.1f", a / b }'; }

RATIO=$(ratio "$OURS_BEST" "$BASE_BEST")
S_RATIO=$(ratio "$SPAWN_BEST" "$BASE_BEST")
echo "bench-tls: ours_batch=${OURS_BEST}us baseline(s_client exec/握手)=${BASE_BEST}us ours_spawn(对照,exec/握手)=${SPAWN_BEST}us"
echo "bench-tls: 绝对值 ours_batch=$(per_hs "$OURS_BEST" "$N")us/握手 baseline=$(per_hs "$BASE_BEST" "$N")us/握手 ours_spawn=$(per_hs "$SPAWN_BEST" "$N")us/握手"
echo "bench-tls: 门禁四 ratio(ours_batch/baseline)=$RATIO(门 ≤1.5 硬,无登记档;基线含 s_client exec 成本,见头注)"
echo "bench-tls: 对照① ratio(ours_spawn/baseline)=$S_RATIO(双侧同付 exec,归档不作门)"

if [ "$(gate_over "$RATIO" 1.5)" = 1 ]; then
    echo "bench-tls: FAIL ratio=$RATIO > 1.5 出口红" >&2
    exit 1
fi
echo "bench-tls: PASS(≤1.5 硬门绿)"
exit 0
