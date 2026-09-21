#!/bin/sh
# tests/net/tls_interop/run.sh —— P3-D HTTPS 回环互操作矩阵单独驱动
#(我们的 TLS 栈 ↔ 本机 openssl 双向;2×2×2 = 方向{d1,d2} × 版本{tls1_2,tls1_3}
# × RT 面{default,coro} = 8 cell,全绿为门)。
#   D1 = our client ↔ openssl s_server(-www -quiet -alpn http/1.1,-tls1_2/-tls1_3
#        强制版本);D2 = openssl s_client(-quiet -alpn http/1.1 -CAfile)↔ our
#        server(固定应答)。ALPN 断言取我方面(tls_alpn()==提供值,权威面)。
# 主环不入(tests/net/run.sh 只收带 c_src/ 的行为夹具;本目录无 c_src/,垫片
# 直链 std/net/c_src 三件,13/13 口径不变)——本脚本为开发迭代入口,形同
# tls_smoke/run.sh。前置:compiler/native.sh、cc、本机 openssl。
# openssl 选点:/usr/bin/openssl(macOS LibreSSL;3.3.6 实测 TLS1.3 客户端/
# 服务端 + s_server/s_client -alpn 均可用 —— 2026-09-21 本机验证,见 p3-task-4
# 报告);CTRON_INTEROP_OPENSSL 可换点(如 homebrew OpenSSL 3.x)。TLS1.3 cell
# 即能力探针:对端缺 TLS1.3 → 该 cell 红(响亮失败,不静默降级)。
# 公平口径:D2 s_client -CAfile 同一张自签证书(链验证开)、不给
# -verify_hostname(= 我方 IP opt-out 同口径);D1 我方 ca=cert 链验证开 +
# 主机名 opt-out —— 双端验证策略对齐。
# CI 纪律:仅回环、高随机端口($$ 派生,bench.sh M-T7-4 同款)、零外联;
# trap 兜杀全部 openssl/夹具进程。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/std"
OPENSSL="${CTRON_INTEROP_OPENSSL:-/usr/bin/openssl}"
[ -x "$EMIT" ] || { echo "tls_interop: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; }
[ -x "$OPENSSL" ] || { echo "tls_interop: 缺 openssl($OPENSSL;可 CTRON_INTEROP_OPENSSL 指点)" >&2; exit 2; }
[ -f "$ROOT/vendor/tls/build/lib/libmbedcrypto.a" ] || sh "$ROOT/vendor/tls/build.sh"

T=$(mktemp -d)
PIDS=""
cleanup() {
    for p in $PIDS; do kill "$p" 2>/dev/null || true; done
    wait 2>/dev/null || true
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

# ---- 证书夹具:自签一张(CN=127.0.0.1),cert=key=ca 同一张(零外联) ----
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout "$T/key.pem" -out "$T/cert.pem" -days 2 -subj "/CN=127.0.0.1" >/dev/null 2>&1
cp "$T/cert.pem" "$T/ca.pem"

# ---- 构建:emit 一份 .ct → 单二进制,双角色运行期环境变量选择 ----
"$EMIT" run "$DIR/src/main.ct" > "$T/main.c"
printf '#define MBEDTLS_CONFIG_FILE "config-thread.h"\n' > "$T/mbcfg.h"
cc -O1 -w -pthread -I"$ROOT/std/net/c_src" \
   -I"$ROOT/vendor/tls" -I"$ROOT/vendor/tls/mbedtls/include" -include "$T/mbcfg.h" \
   -o "$T/tls_interop" "$T/main.c" \
   "$ROOT/std/net/c_src/ctron_net.c" "$ROOT/std/net/c_src/ctron_rt.c" "$ROOT/std/net/c_src/ctron_tls.c" \
   "$ROOT/vendor/tls/build/lib/libmbedtls.a" \
   "$ROOT/vendor/tls/build/lib/libmbedx509.a" \
   "$ROOT/vendor/tls/build/lib/libmbedcrypto.a" -lpthread
BIN="$T/tls_interop"

OSSL_V=$("$OPENSSL" version)
echo "tls_interop: openssl = $OSSL_V(TLS1.3/ALPN cell 即能力探针,红=缺能力响亮失败)"
BASE=$(( ($$ % 20000) + 30000 ))
pass=0; fail=0; k=0

# ---- D1 cell:our client ↔ openssl s_server ----
d1() { # $1=RTENV $2=版本(tls1_2|tls1_3)
    _re="$1"; _v="$2"; _mode="$3"
    PORT=$((BASE + k)); k=$((k + 1))
    SLOG="$T/d1_${_mode}_${_v}.srv.log"; CLOG="$T/d1_${_mode}_${_v}.cli.log"
    "$OPENSSL" s_server -accept "$PORT" -www -quiet -alpn http/1.1 -"$_v" \
        -cert "$T/cert.pem" -key "$T/key.pem" >"$SLOG" 2>&1 & SPID=$!
    PIDS="$PIDS $SPID"
    ok=0; i=0
    while [ "$i" -lt 50 ]; do
        # 探活 = 真客户端单连(bench.sh「探活非 stdout」口径);对端拒连 → 重试
        if env $_re INTEROP_ROLE=client INTEROP_PORT="$PORT" INTEROP_ALPN=http/1.1 \
            INTEROP_LOOP=1 CTRON_SMOKE_CA="$T/ca.pem" "$BIN" >"$CLOG" 2>&1; then
            ok=1; break
        fi
        sleep 0.1; i=$((i + 1))
    done
    kill "$SPID" 2>/dev/null || true
    wait "$SPID" 2>/dev/null || true
    if [ "$ok" = 1 ]; then
        echo "  PASS d1 ctron-client->openssl-s_server $_v ($_mode): handshake+链验证REQUIRED+alpn=http/1.1+HTTP200+eof(openssl 版本强制 -$_v)"
    else
        echo "  FAIL d1 ctron-client->openssl-s_server $_v ($_mode)" >&2
        sed -n '1,8p' "$CLOG" >&2; sed -n '1,4p' "$SLOG" >&2
        return 1
    fi
}

# ---- D2 cell:openssl s_client ↔ our server ----
d2() { # $1=RTENV $2=版本 $3=面名
    _re="$1"; _v="$2"; _mode="$3"
    PORT=$((BASE + k)); k=$((k + 1))
    SLOG="$T/d2_${_mode}_${_v}.srv.log"; CLOG="$T/d2_${_mode}_${_v}.cli.log"
    env $_re INTEROP_ROLE=server INTEROP_PORT="$PORT" INTEROP_CONNS=2 INTEROP_ALPN=http/1.1 \
        CTRON_SMOKE_CERT="$T/cert.pem" CTRON_SMOKE_KEY="$T/key.pem" CTRON_SMOKE_CA="$T/ca.pem" \
        "$BIN" >"$SLOG" 2>&1 & SPID=$!
    PIDS="$PIDS $SPID"
    # 探活 = s_client 全握手一连(消费 INTEROP_CONNS 第 1 连;println 块缓冲
    # 不可作就绪判据);-quiet 隐含 -ign_eof → 我方服务端应答后先关,探活即退
    ok=0; i=0
    while [ "$i" -lt 50 ]; do
        if printf 'GET /probe HTTP/1.0\r\n\r\n' | "$OPENSSL" s_client \
            -connect "127.0.0.1:$PORT" -quiet -"$_v" -alpn http/1.1 \
            -CAfile "$T/ca.pem" >"$CLOG.probe" 2>&1; then
            ok=1; break
        fi
        sleep 0.1; i=$((i + 1))
    done
    CRC=1
    if [ "$ok" = 1 ]; then
        # 正式 cell:第 2 连;断言 = s_client 退 0 + 应答字节 + 我方 rc(join 断言含 ALPN)
        if printf 'interop-ping-from-openssl\n' | "$OPENSSL" s_client \
            -connect "127.0.0.1:$PORT" -quiet -"$_v" -alpn http/1.1 \
            -CAfile "$T/ca.pem" >"$CLOG" 2>"$CLOG.err"; then
            CRC=0
        fi
    else
        kill "$SPID" 2>/dev/null || true     # 未就绪:先杀再 wait,防悬挂
    fi
    if wait "$SPID" 2>/dev/null; then SRC=0; else SRC=1; fi
    if [ "$ok" = 1 ] && [ "$CRC" = 0 ] && [ "$SRC" = 0 ] \
        && grep -q "interop-pong-from-ctron" "$CLOG"; then
        echo "  PASS d2 openssl-s_client->ctron-server $_v ($_mode): s_client退0+pong应答+我方alpn=http/1.1断言(openssl 版本强制 -$_v;-CAfile 链验证开)"
    else
        echo "  FAIL d2 openssl-s_client->ctron-server $_v ($_mode) (probe=$ok s_client=$CRC server=$SRC)" >&2
        sed -n '1,8p' "$SLOG" >&2; sed -n '1,4p' "$CLOG" >&2; sed -n '1,4p' "$CLOG.err" >&2
        return 1
    fi
}

for MODE in default coro; do
    if [ "$MODE" = coro ]; then RTENV="CTRON_RT=coro"; else RTENV=""; fi
    # coro 面:D2 server 协程 TLS 停车于 net 垫片(对端真外进程,混合化继承的
    # 外部对端证明);D1 client 主面 P1 路径(tls_smoke/coro_hybrid 同口径)
    for V in tls1_2 tls1_3; do
        if d1 "$RTENV" "$V" "$MODE"; then pass=$((pass + 1)); else fail=$((fail + 1)); fi
        if d2 "$RTENV" "$V" "$MODE"; then pass=$((pass + 1)); else fail=$((fail + 1)); fi
    done
done

echo "tls_interop: $pass/8 (2×2×2 = 方向{d1,d2} × 版本{tls1_2,tls1_3} × 面{default,coro};openssl=$OSSL_V)"
[ "$fail" = 0 ] || exit 1
