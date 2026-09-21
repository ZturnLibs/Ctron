#!/bin/sh
# tests/net/run.sh —— 服务器泳道回环验收(§11;CI 纪律:仅回环、:0、零外联)
# 口径:行为夹具(目录含 c_src/ 且含 src/main.ct):ctron-emit → cc 链 c_src/*.c → 原生运行;
#       纯 C 冒烟目录(rt_core_smoke/rt_reactor_smoke,无 main.ct)由各自 run.sh 承载,不入主环;
#       coro_hybrid 的 main.ct 入主环(裸线程面),其 c_smoke.c 由下方专属块承载(P2-C);
#       tls_smoke(P3-C)入主环:c_src 含 ctron_tls.c 的夹具自动补 mbedTLS 头/库与
#       自签证书(见环内 TLCF/TLLB/TLENV 块),双矩阵随本脚本双跑各计一次
# 前置:compiler/native.sh、cc
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
# Task 5 起夹具 use std.net.*:显式指路源码树 std(gui_counter/run.sh 同款)
export CTRON_STDPATH="$ROOT/std"
if [ ! -x "$EMIT" ]; then echo "net/run: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/net 回环用例(§11)=="
for d in "$DIR"/*/; do
    [ -d "$d/c_src" ] || continue
    name=$(basename "$d"); [ "$name" = "bench" ] && continue
    e="$d/src/main.ct"
    [ -f "$e" ] || continue              # 纯 C 冒烟目录(rt_*_smoke)无 main.ct:不入主环
    # P2-D coro 矩阵:CTRON_RT=coro 时给未自链 rt 的夹具统一补链 ctron_rt.c
    #(已自链者如 coro_hybrid/coro_conc 不重链——重复强定义链接报错);默认环境
    # RTSRC 为空,链接行与 P2-C 前同形。主环 cc 补 -pthread(rt 用 pthread)+
    # -I(rt.h 解析;ctron_net.c 自 P2-D 收账 include ctron_rt.h)。
    #(终审收账:补链条件收紧为仅 =coro,其他 CTRON_RT 值不再过匹配补链。)
    RTSRC=""
    if [ "${CTRON_RT:-}" = "coro" ] && [ ! -e "$d/c_src/ctron_rt.c" ]; then
        RTSRC="$ROOT/std/net/c_src/ctron_rt.c"
    fi
    # P3-C TLS 夹具:c_src 含 ctron_tls.c 时补 mbedTLS 头/库与自签证书。
    # 证书 openssl req -x509 本机生成(临时目录,零外联),路径经环境变量
    # 注入(std 无 getenv 面,夹具 c_src/ct_smoke_env.c 垫)。MBEDTLS_CONFIG_FILE
    # 的引号宏经 -include 头注入(避 shell 引号穿越);库缺席响亮失败并指路
    # vendor/tls/build.sh(非静默跳过)。链接序:垫片对象在前,三 .a 随后。
    TLCF=""
    TLLB=""
    TLENV=""
    if [ -e "$d/c_src/ctron_tls.c" ]; then
        if [ ! -f "$ROOT/vendor/tls/build/lib/libmbedtls.a" ] \
           || [ ! -f "$ROOT/vendor/tls/build/lib/libmbedx509.a" ] \
           || [ ! -f "$ROOT/vendor/tls/build/lib/libmbedcrypto.a" ]; then
            fail=$((fail+1)); echo "  FAIL $name (缺 vendor/tls/build/lib/*.a —— 先: sh vendor/tls/build.sh)"
            continue
        fi
        printf '#define MBEDTLS_CONFIG_FILE "config-thread.h"\n' > "$T/$name.mbcfg.h"
        TLCF="-I$ROOT/vendor/tls -I$ROOT/vendor/tls/mbedtls/include -include $T/$name.mbcfg.h"
        TLLB="$ROOT/vendor/tls/build/lib/libmbedtls.a $ROOT/vendor/tls/build/lib/libmbedx509.a $ROOT/vendor/tls/build/lib/libmbedcrypto.a -lpthread"
        mkdir -p "$T/$name.pki"
        if ! openssl req -x509 -newkey rsa:2048 -nodes \
             -keyout "$T/$name.pki/key.pem" -out "$T/$name.pki/cert.pem" \
             -days 2 -subj "/CN=127.0.0.1" >/dev/null 2>&1; then
            fail=$((fail+1)); echo "  FAIL $name (openssl 自签证书生成失败)"
            continue
        fi
        cp "$T/$name.pki/cert.pem" "$T/$name.pki/ca.pem"
        TLENV="CTRON_SMOKE_CERT=$T/$name.pki/cert.pem CTRON_SMOKE_KEY=$T/$name.pki/key.pem CTRON_SMOKE_CA=$T/$name.pki/ca.pem"
    fi
    if "$EMIT" run "$e" > "$T/$name.c" 2>"$T/$name.err"; then
        if cc -O1 -w -pthread -I"$ROOT/std/net/c_src" $TLCF -o "$T/$name" "$T/$name.c" "$d"/c_src/*.c $RTSRC $TLLB 2>"$T/$name.cc.err"; then
            # 展开词不作赋值前缀(shell 语义)→ 经 env 注入夹具环境(TLENV 空 = 仅透传)
            if env $TLENV "$T/$name" run "$e" >"$T/$name.out" 2>&1; then
                pass=$((pass+1)); echo "  PASS $name"
            else
                fail=$((fail+1)); echo "  FAIL $name (run)"; sed -n '1,5p' "$T/$name.out"
            fi
        else
            fail=$((fail+1)); echo "  FAIL $name (cc)"; sed -n '1,5p' "$T/$name.cc.err"
        fi
    else
        fail=$((fail+1)); echo "  FAIL $name (emit)"; sed -n '1,5p' "$T/$name.err"
    fi
done
# coro_hybrid C 冒烟(P2-C):net 垫片协程停车证明 —— net+rt 双链、CTRON_RT=coro、
# workers=1 与 4 各整跑(1 = 停车严格证:垫片若滞留 worker,进度协程即饿死)。
# .ct 部分已入主环(裸线程面,rt 链入但 current()==NULL → P1 原路径)。
smoke="$DIR/coro_hybrid/c_smoke.c"
if [ ! -f "$smoke" ]; then
    fail=$((fail+1)); echo "  FAIL coro_hybrid_c_smoke (c_smoke.c 缺席:静默跳过守卫,P2-C 收账)"
else
    name="coro_hybrid_c_smoke"
    if cc -O1 -w -pthread -o "$T/$name" "$smoke" "$DIR"/coro_hybrid/c_src/*.c 2>"$T/$name.cc.err"; then
        for W in 1 4; do
            if CTRON_RT=coro CTRON_RT_WORKERS=$W "$T/$name" >"$T/$name.out" 2>&1; then
                pass=$((pass+1)); echo "  PASS $name (workers=$W)"
            else
                fail=$((fail+1)); echo "  FAIL $name (run, workers=$W)"; sed -n '1,5p' "$T/$name.out"
            fi
        done
    else
        fail=$((fail+1)); echo "  FAIL $name (cc)"; sed -n '1,5p' "$T/$name.cc.err"
    fi
fi
# coro_det 种子重放(P2-E 审查 Important 收口):确定性调度器的常设回归
# 保护——本目录 run.sh 承载 SEED=42 双跑 cmp + 异种子/无种子完成性 + CI
# 100 种子(0..99)双跑重放(nightly 1000 口径见其尾注)。专属块镜像上方
# c_smoke 形:缺席响亮失败,计入 pass/fail(fail → run.sh 非零退出)。
det="$DIR/coro_det/run.sh"
if [ ! -f "$det" ]; then
    fail=$((fail+1)); echo "  FAIL coro_det_replay (run.sh 缺席:静默跳过守卫)"
else
    if sh "$det" >"$T/coro_det_replay.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS coro_det_replay (同种子 cmp 101/101 全绿)"
    else
        fail=$((fail+1)); echo "  FAIL coro_det_replay (构建败或种子重放漂移)"; sed -n '1,5p' "$T/coro_det_replay.out"
    fi
fi
echo "net/run: pass=$pass fail=$fail"
# M-T4-2:空集口径——一例未跑(pass=0)与全败同罪,防夹具被静默跳过
[ "$pass" -gt 0 ] || { echo "net/run: no cases ran"; exit 1; }
[ "$fail" = 0 ]
