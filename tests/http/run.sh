#!/bin/sh
# tests/http/run.sh —— HTTP/1.1 协议半层验收(P4-A;§11.7)
# 口径:http 为零 use 纯 Ctron 半层(不 import std.net:加载器菱形 use
# 误报 E5020 规避 + 网络垫片在解释口径无绑定,divergences (c)/(g) ⑥),故:
#   主环 = 解释器(ctron-cc run):corpus/*.ct 断言式夹具逐文件跑,纯层无需
#          原生速度;与 std/*.ct 种子单测同口径(smoke.sh 3e 先例)。
#   副 = emit 同源对拍(ctron-emit → cc → 原生):http 无 extern,发射零
#        链接需求;对拍即 divergences (h)(跨模块 struct 形)的常设回归哨。
#   两臂同一夹具集双计 pass;任一臂红即红。pass>0 空集守卫(tests/net 同款)。
#   双运行时矩阵(CTRON_RT=coro)不适用:纯层无停车点,无 rt 依赖。
# 网络端到端行为(HTTP over TCP)归框架半层波次(P4-B+),届时入 tests/net
# 双矩阵口径;本目录 CI 纪律同 tests/net:仅回环、零外联(纯层实为离线)。
# 前置:compiler/native.sh、cc(副臂)
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CC="$ROOT/compiler/bin/ctron-cc"
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/std"
if [ ! -x "$CC" ]; then echo "http/run: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/http 协议半层用例(§11.7;P4-A)=="

# ── std 模块 inline tests(与 std/*.ct 种子单测同口径)──
# P4-C:sse/ws 纯叶入 interp;client.ct 触 net/bind 子树(W8052 使 interp rc=1)
# → 其 inline tests 入下方 emit 专臂;crypto.ct(SHA-1 RFC 3174 向量)同 interp。
for m in parse message sse ws; do
    if "$CC" run "$ROOT/http/$m.ct" > "$T/std_$m.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS http/$m.ct (inline)"
    else
        fail=$((fail+1)); echo "  FAIL http/$m.ct (inline)"; sed -n '1,5p' "$T/std_$m.out"
    fi
done
# P6-A:frm 子层(router.ct 纯叶 + middleware.ct → router/enc;enc 纯面无
# W8052,解释臂直跑;emit 臂覆盖见 frm_route 夹具对拍)
# P6-B:中间件五件入列 —— cors/sechdr/limit/timeout 零 use 叶,csrf→crypto
# 纯叶(无 W8052);emit 臂覆盖见 frm_mw 夹具对拍
# P6-C:auth→crypto 叶 + body→json 叶(csrf 同款纯叶);emit 臂覆盖见
# frm_auth 夹具对拍
for m in frm/router frm/middleware frm/cors frm/csrf frm/sechdr frm/limit frm/timeout frm/auth frm/body; do
    mn=$(basename "$m")
    if "$CC" run "$ROOT/http/$m.ct" > "$T/std_$mn.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS http/$m.ct (inline)"
    else
        fail=$((fail+1)); echo "  FAIL http/$m.ct (inline)"; sed -n '1,5p' "$T/std_$mn.out"
    fi
done
if "$CC" run "$ROOT/std/crypto.ct" > "$T/std_crypto.out" 2>&1; then
    pass=$((pass+1)); echo "  PASS std/crypto.ct (inline, sha1+sha256)"
else
    fail=$((fail+1)); echo "  FAIL std/crypto.ct (inline)"; sed -n '1,5p' "$T/std_crypto.out"
fi

# ── corpus:主环 = 解释器 ──
for f in "$DIR"/corpus/*.ct; do
    name=$(basename "$f" .ct)
    if "$CC" run "$f" > "$T/$name.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS $name (interp)"
    else
        fail=$((fail+1)); echo "  FAIL $name (interp)"; sed -n '1,5p' "$T/$name.out"
    fi
done

# ── corpus:副臂 = emit 对拍((h) 回归哨;无 c_src,直链)──
if [ -x "$EMIT" ]; then
    for f in "$DIR"/corpus/*.ct; do
        name=$(basename "$f" .ct)
        if "$EMIT" run "$f" > "$T/$name.e.c" 2>"$T/$name.e.err" \
           && cc -O1 -w -o "$T/$name.e.bin" "$T/$name.e.c" 2>"$T/$name.e.cc.err" \
           && "$T/$name.e.bin" > "$T/$name.e.out" 2>&1; then
            pass=$((pass+1)); echo "  PASS $name (emit)"
        else
            fail=$((fail+1)); echo "  FAIL $name (emit)"; sed -n '1,5p' "$T/$name.e.out" "$T/$name.e.cc.err" "$T/$name.e.err" 2>/dev/null
        fi
    done
else
    echo "  [skip] emit 臂:缺 ctron-emit(先: compiler/native.sh)——不入计例,interp 臂已覆盖"
fi

# ── enc_fixtures(P4-B:压缩/HTTP 日期/query-form)──
# 双臂分工(结构性登记,非临时态):x_ 前缀 = 触 ctron_deflate 垫片 extern
# (&I64[] 视图 lane)——解释口径 extern 桥仅标量帧 "i:/s:"(eval_call.ct
# W4 E1)+ ctron-cc 宿主未链本垫片(dlsym 无符号)→ x_ 仅 emit 臂,链垫片
# + libminiz.a(缺席响亮失败并指路 vendor/deflate/build.sh,tests/net TLS
# 夹具同款口径);非 x_(纯 Ctron:协商矩阵/日期/form)双臂同跑,interp 臂
# 常设覆盖。压缩往返等价性 interp/emit 各自成立,emit 臂即对拍面。
for f in "$DIR"/enc_fixtures/*.ct; do
    name=$(basename "$f" .ct)
    case "$name" in
        x_*) continue ;;   # interp 无 lane extern 桥:emit 臂覆盖,不入计例
    esac
    if "$CC" run "$f" > "$T/$name.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS $name (interp)"
    else
        fail=$((fail+1)); echo "  FAIL $name (interp)"; sed -n '1,5p' "$T/$name.out"
    fi
done
if [ -x "$EMIT" ]; then
    for f in "$DIR"/enc_fixtures/*.ct; do
        name=$(basename "$f" .ct)
        MZCF=""
        MZLB=""
        case "$name" in
            x_*)
                if [ ! -f "$ROOT/vendor/deflate/build/lib/libminiz.a" ]; then
                    fail=$((fail+1)); echo "  FAIL $name (缺 vendor/deflate/build/lib/libminiz.a —— 先: sh vendor/deflate/build.sh)"
                    continue
                fi
                MZCF="-I$ROOT/vendor/deflate/miniz"
                MZLB="$ROOT/http/c_src/ctron_deflate.c $ROOT/vendor/deflate/build/lib/libminiz.a"
                ;;
        esac
        if "$EMIT" run "$f" > "$T/$name.e.c" 2>"$T/$name.e.err" \
           && cc -O1 -w -o "$T/$name.e.bin" "$T/$name.e.c" $MZCF $MZLB 2>"$T/$name.e.cc.err" \
           && "$T/$name.e.bin" > "$T/$name.e.out" 2>&1; then
            pass=$((pass+1)); echo "  PASS $name (emit)"
        else
            fail=$((fail+1)); echo "  FAIL $name (emit)"; sed -n '1,5p' "$T/$name.e.out" "$T/$name.e.cc.err" "$T/$name.e.err" 2>/dev/null
        fi
    done
fi

# ── http/client.ct inline tests(P4-C;emit 专臂,net/bind 子树 W8052)──
if [ -x "$EMIT" ]; then
    if "$EMIT" run "$ROOT/http/client.ct" > "$T/std_client.e.c" 2>"$T/std_client.e.err" \
       && cc -O1 -w -o "$T/std_client.e.bin" "$T/std_client.e.c" 2>"$T/std_client.e.cc.err" \
       && "$T/std_client.e.bin" > "$T/std_client.e.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS http/client.ct (inline, emit)"
    else
        fail=$((fail+1)); echo "  FAIL http/client.ct (inline, emit)"; sed -n '1,5p' "$T/std_client.e.out" "$T/std_client.e.cc.err" "$T/std_client.e.err" 2>/dev/null
    fi
fi

# ── P4-C 行为夹具:client_fixtures(客户端)与 sse_ws(SSE/WS)──
# 臂分工(结构性登记,enc_fixtures x_ 同口径):凡 use http.client 的夹具,
# 其 use 图必带 net/bind.ct —— 解释口径 W8052(rc=1)且垫片无绑定 → x_ 前缀
# = emit 专臂,cc 链 ctron_net.c(net 垫片;tests/net run.sh 同款)。双 RT 矩阵:
# 默认(裸线程)+ CTRON_RT=coro(补链 ctron_rt.c;服务端协程停车于 net 垫片)
# 各整跑、各计一例 —— tests/net 双矩阵口径在本目录的延伸。
# a_ 前缀 = 纯面(零 IO;interp + emit 双计),同 enc_fixtures a_。
for dir in client_fixtures sse_ws; do
    for f in "$DIR/$dir"/*.ct; do
        [ -f "$f" ] || continue
        name="${dir}_$(basename "$f" .ct)"
        case "$(basename "$f" .ct)" in
            x_*)
                if [ ! -x "$EMIT" ]; then
                    continue
                fi
                RTN=""
                RTSRC2="$ROOT/net/c_src/ctron_net.c"
                for mode in default coro; do
                    RTF=""
                    RTENV=""
                    if [ "$mode" = "coro" ]; then
                        RTF="$ROOT/net/c_src/ctron_rt.c"
                        RTENV="CTRON_RT=coro"
                    fi
                    if "$EMIT" run "$f" > "$T/$name.$mode.c" 2>"$T/$name.$mode.err" \
                       && cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/$name.$mode.bin" "$T/$name.$mode.c" "$RTSRC2" $RTF 2>"$T/$name.$mode.cc.err" \
                       && env $RTENV timeout 60 "$T/$name.$mode.bin" > "$T/$name.$mode.out" 2>&1; then
                        pass=$((pass+1)); echo "  PASS $name ($mode)"
                    else
                        fail=$((fail+1)); echo "  FAIL $name ($mode)"; sed -n '1,5p' "$T/$name.$mode.out" "$T/$name.$mode.cc.err" "$T/$name.$mode.err" 2>/dev/null
                    fi
                done
                ;;
            *)
                if "$CC" run "$f" > "$T/$name.out" 2>&1; then
                    pass=$((pass+1)); echo "  PASS $name (interp)"
                else
                    fail=$((fail+1)); echo "  FAIL $name (interp)"; sed -n '1,5p' "$T/$name.out"
                fi
                if [ -x "$EMIT" ]; then
                    if "$EMIT" run "$f" > "$T/$name.e.c" 2>"$T/$name.e.err" \
                       && cc -O1 -w -o "$T/$name.e.bin" "$T/$name.e.c" 2>"$T/$name.e.cc.err" \
                       && "$T/$name.e.bin" > "$T/$name.e.out" 2>&1; then
                        pass=$((pass+1)); echo "  PASS $name (emit)"
                    else
                        fail=$((fail+1)); echo "  FAIL $name (emit)"; sed -n '1,5p' "$T/$name.e.out" "$T/$name.e.cc.err" "$T/$name.e.err" 2>/dev/null
                    fi
                fi
                ;;
        esac
    done
done

# ── P6-A 行为夹具:frm_route(路由核 + 中间件链)──
# 臂分工(client_fixtures 同款):a_ 前缀 = 纯面(零 IO;use middleware 门面
# → router/enc 纯叶,无 W8052)interp + emit 双计;x_ = 触 net 垫片时钟
# (ctron_net_now_ns)→ emit 专臂,且为 env 门禁(CTRON_ROUTE_BENCH=1 启用,
# tests/http/bench/bench.sh 家族口径:本地/nightly 门禁,不入 CI 主环;
# 3 轮取最小,门 ns_per_hit ≤ 100;ns_per_reject 如实打印不裁 —— 全表扫描
# 拒绝面归因登记见 P6-A 报告)。
for f in "$DIR"/frm_route/a_*.ct; do
    [ -f "$f" ] || continue
    name="frm_route_$(basename "$f" .ct)"
    if "$CC" run "$f" > "$T/$name.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS $name (interp)"
    else
        fail=$((fail+1)); echo "  FAIL $name (interp)"; sed -n '1,5p' "$T/$name.out"
    fi
    if [ -x "$EMIT" ]; then
        if "$EMIT" run "$f" > "$T/$name.e.c" 2>"$T/$name.e.err" \
           && cc -O1 -w -o "$T/$name.e.bin" "$T/$name.e.c" 2>"$T/$name.e.cc.err" \
           && "$T/$name.e.bin" > "$T/$name.e.out" 2>&1; then
            pass=$((pass+1)); echo "  PASS $name (emit)"
        else
            fail=$((fail+1)); echo "  FAIL $name (emit)"; sed -n '1,5p' "$T/$name.e.out" "$T/$name.e.cc.err" "$T/$name.e.err" 2>/dev/null
        fi
    fi
done

# ── P6-B 行为夹具:frm_mw(公网中间件五件)──
# 臂分工(frm_route 同款):a_ 前缀 = 纯面(零 IO;cors/sechdr/limit/timeout
# 零 use 叶 + csrf→std.crypto 纯叶,无 W8052)interp + emit 双计。确定性:
# limit/timeout 时钟恒注入参数(tick 计数),零真钟零睡眠;CORS/CSRF/安全头
# 全请求-响应纯值面。
for f in "$DIR"/frm_mw/a_*.ct; do
    [ -f "$f" ] || continue
    name="frm_mw_$(basename "$f" .ct)"
    if "$CC" run "$f" > "$T/$name.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS $name (interp)"
    else
        fail=$((fail+1)); echo "  FAIL $name (interp)"; sed -n '1,5p' "$T/$name.out"
    fi
    if [ -x "$EMIT" ]; then
        if "$EMIT" run "$f" > "$T/$name.e.c" 2>"$T/$name.e.err" \
           && cc -O1 -w -o "$T/$name.e.bin" "$T/$name.e.c" 2>"$T/$name.e.cc.err" \
           && "$T/$name.e.bin" > "$T/$name.e.out" 2>&1; then
            pass=$((pass+1)); echo "  PASS $name (emit)"
        else
            fail=$((fail+1)); echo "  FAIL $name (emit)"; sed -n '1,5p' "$T/$name.e.out" "$T/$name.e.cc.err" "$T/$name.e.err" 2>/dev/null
        fi
    fi
done

# ── P6-C 行为夹具:frm_auth(认证三件套 + body 绑定)──
# 臂分工(frm_route/frm_mw 同款):a_ 前缀 = 纯面(零 IO;auth→crypto 叶 +
# body→json 叶 + guard 双叶并用皆纯叶,无 W8052)interp + emit 双计。
# 确定性:时钟/sid 熵源恒注入参数(auth.ct 头注④;limit.ct 先例),
# JWT 黄金向量离线预compute,零真钟零熵零外联。
for f in "$DIR"/frm_auth/a_*.ct; do
    [ -f "$f" ] || continue
    name="frm_auth_$(basename "$f" .ct)"
    if "$CC" run "$f" > "$T/$name.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS $name (interp)"
    else
        fail=$((fail+1)); echo "  FAIL $name (interp)"; sed -n '1,5p' "$T/$name.out"
    fi
    if [ -x "$EMIT" ]; then
        if "$EMIT" run "$f" > "$T/$name.e.c" 2>"$T/$name.e.err" \
           && cc -O1 -w -o "$T/$name.e.bin" "$T/$name.e.c" 2>"$T/$name.e.cc.err" \
           && "$T/$name.e.bin" > "$T/$name.e.out" 2>&1; then
            pass=$((pass+1)); echo "  PASS $name (emit)"
        else
            fail=$((fail+1)); echo "  FAIL $name (emit)"; sed -n '1,5p' "$T/$name.e.out" "$T/$name.e.cc.err" "$T/$name.e.err" 2>/dev/null
        fi
    fi
done

# ── P6-D 行为夹具:frm_static / frm_openapi / frm_html(静态+OpenAPI+转义)──
# 臂分工(frm_route 同款):a_ 前缀 = 纯面(零 IO;static→std.crypto 叶 +
# openapi/html 零 use 叶,无 W8052)interp + emit 双计。st_serve IO 薄胶
# 不入夹具(CWD 不定;P6-E todo_api e2e 接线)。快照夹具 = 路由表改 → 断言红。
for f in "$DIR"/frm_static/a_*.ct "$DIR"/frm_openapi/a_*.ct "$DIR"/frm_html/a_*.ct; do
    [ -f "$f" ] || continue
    name="p6d_$(basename "$(dirname "$f")")_$(basename "$f" .ct)"
    if "$CC" run "$f" > "$T/$name.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS $name (interp)"
    else
        fail=$((fail+1)); echo "  FAIL $name (interp)"; sed -n '1,5p' "$T/$name.out"
    fi
    if [ -x "$EMIT" ]; then
        if "$EMIT" run "$f" > "$T/$name.e.c" 2>"$T/$name.e.err" \
           && cc -O1 -w -o "$T/$name.e.bin" "$T/$name.e.c" 2>"$T/$name.e.cc.err" \
           && "$T/$name.e.bin" > "$T/$name.e.out" 2>&1; then
            pass=$((pass+1)); echo "  PASS $name (emit)"
        else
            fail=$((fail+1)); echo "  FAIL $name (emit)"; sed -n '1,5p' "$T/$name.e.out" "$T/$name.e.cc.err" "$T/$name.e.err" 2>/dev/null
        fi
    fi
done

if [ "${CTRON_ROUTE_BENCH:-}" = "1" ] && [ -x "$EMIT" ]; then
    B="frm_route_x_bench"
    if "$EMIT" run "$DIR/frm_route/x_bench.ct" > "$T/$B.c" 2>"$T/$B.err" \
       && cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/$B.bin" "$T/$B.c" "$ROOT/net/c_src/ctron_net.c" 2>"$T/$B.cc.err"; then
        MIN=""; MINR=""
        R=1
        while [ "$R" -le 3 ]; do
            OUT=$(env CTRON_ROUTE_BENCH_N="${CTRON_ROUTE_BENCH_N:-200000}" timeout 120 "$T/$B.bin") || { fail=$((fail+1)); echo "  FAIL $B (round $R 运行失败)"; break; }
            NS=$(printf '%s\n' "$OUT" | sed -n 's/^ns_per_hit=//p')
            NSR=$(printf '%s\n' "$OUT" | sed -n 's/^ns_per_reject=//p')
            if [ -z "$MIN" ] || [ "$NS" -lt "$MIN" ]; then MIN="$NS"; fi
            if [ -z "$MINR" ] || [ "$NSR" -lt "$MINR" ]; then MINR="$NSR"; fi
            R=$((R + 1))
        done
        if [ -n "$MIN" ]; then
            VERDICT=$(awk -v n="$MIN" 'BEGIN { print (n <= 100) ? "GREEN" : "RED" }')
            if [ "$VERDICT" = "GREEN" ]; then
                pass=$((pass+1)); echo "  PASS $B (bench: hit ${MIN}ns ≤100 | reject ${MINR}ns)"
            else
                fail=$((fail+1)); echo "  FAIL $B (bench: hit ${MIN}ns >100 门;reject ${MINR}ns —— 如实登记归因)"
            fi
        fi
    else
        fail=$((fail+1)); echo "  FAIL $B (构建失败)"; sed -n '1,5p' "$T/$B.err" "$T/$B.cc.err" 2>/dev/null
    fi
else
    echo "  [skip] frm_route x_bench:CTRON_ROUTE_BENCH=1 启用(bench 家族门禁,不入 CI 主环)"
fi

echo "http/run: pass=$pass fail=$fail"
# 空集口径:一例未跑与全败同罪
[ "$pass" -gt 0 ] || { echo "http/run: no cases ran"; exit 1; }
[ "$fail" = 0 ]
