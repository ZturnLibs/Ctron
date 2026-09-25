#!/bin/sh
# todo_api e2e(P6-E)—— 自客户端(nc)打自服务端,全链断言:
#   健康/OpenAPI/静态页(200+ETag+304)/CORS 预检/JWT 拒未授权写/login 签发/
#   CRUD 全链(201→列表→204 删除)/限流 503/优雅停机(排空退出)
# 口径:仅回环;端口 env TODO_API_PORT(缺省 8091);发射臂构建;服务端串行
# (Connection: close);探针 = 单连接单请求(printf %b 发真实 CRLF)。
# RT 矩阵:默认;CTRON_TODO_RT=coro 时服务端链 ctron_rt 并以协程运行时启动。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
CC=cc
export CTRON_STDPATH="$ROOT/std"
PORT=${TODO_API_PORT:-8091}
KEY=${TODO_API_KEY:-s3cret-key}
pass=0; fail=0
T=$(mktemp -d /tmp/todoapi.XXXXXX)
SRV=""
cleanup() {
    [ -n "$SRV" ] && kill "$SRV" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
[ -x "$EMIT" ] || { echo "todo-e2e: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; }

ok()  { pass=$((pass+1)); echo "  ok  : $1"; }
bad() { fail=$((fail+1)); echo "  FAIL: $1"; }

# HTTP 探针:%b 解 \r\n 为真实 CRLF;输出落 $2
probe() {
    printf '%b' "$1" | timeout 10 nc 127.0.0.1 "$PORT" > "$2" 2>/dev/null
    return 0
}

# POST JSON 探针:$1=target $2=token(可空) $3=JSON 体 $4=输出
probe_post() {
    let cl=${#3}
    if [ -n "$2" ]; then
        probe "POST $1 HTTP/1.1\r\nHost: t\r\nAuthorization: Bearer $2\r\nContent-Type: application/json\r\nContent-Length: $cl\r\n\r\n$3" "$4"
    else
        probe "POST $1 HTTP/1.1\r\nHost: t\r\nContent-Type: application/json\r\nContent-Length: $cl\r\n\r\n$3" "$4"
    fi
}

# ── 构建 ──
if ! "$EMIT" run "$DIR/src/main.ct" > "$T/todo.c" 2>"$T/emit.err"; then
    echo "todo-e2e: emit 失败" >&2; head -3 "$T/emit.err"; exit 1
fi
RTS=""
RTFLAG=""
if [ "${CTRON_TODO_RT:-}" = "coro" ]; then
    RTS="$ROOT/net/c_src/ctron_rt.c"
    RTFLAG="CTRON_RT=coro"
fi
if ! $CC -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/todo.bin" "$T/todo.c" "$ROOT"/net/c_src/ctron_net.c $RTS 2>"$T/cc.err"; then
    echo "todo-e2e: cc 失败" >&2; head -3 "$T/cc.err"; exit 1
fi

start_srv() {
    env TODO_API_PORT="$PORT" TODO_API_KEY="$KEY" TODO_API_RLIMIT="$1" $RTFLAG "$T/todo.bin" > "$T/srv.log" 2>&1 &
    SRV=$!
    local i=0
    while [ "$i" -lt 25 ]; do
        printf 'GET /health HTTP/1.1\r\nHost: t\r\n\r\n' | timeout 3 nc 127.0.0.1 "$PORT" 2>/dev/null | grep -q "200 OK" && return 0
        sleep 0.2; i=$((i+1))
    done
    return 1
}

# ═══════════ 相位一:全链(阈值无限) ═══════════
start_srv 100000 || { echo "todo-e2e: 服务未就绪"; cat "$T/srv.log"; exit 1; }
ok "服务就绪(:$PORT)"

# 1) 健康
probe 'GET /health HTTP/1.1\r\nHost: t\r\n\r\n' "$T/r1"
grep -q "200 OK" "$T/r1" && grep -q '"ok":true' "$T/r1" && ok "健康端点 200 {ok:true}" || bad "健康端点"

# 2) 静态页 + ETag + 304
probe 'GET / HTTP/1.1\r\nHost: t\r\n\r\n' "$T/r2"
if grep -q "200 OK" "$T/r2" && grep -q "<h1>todo-api</h1>" "$T/r2"; then ok "静态页 200"; else bad "静态页 200"; fi

# 2.5) /metrics(P7-D Prometheus 文本面)
probe 'GET /metrics HTTP/1.1\r\nHost: t\r\n\r\n' "$T/rmx"
if grep -q "# TYPE http_total counter" "$T/rmx" && grep -qF 'http_total{code="200"}' "$T/rmx" && grep -q "todo_served" "$T/rmx"; then ok "/metrics Prometheus 文本(counter+gauge)"; else bad "/metrics"; fi
ETAG=$(grep -i "^ETag:" "$T/r2" | tr -d "\r" | cut -d" " -f2)
[ -n "$ETAG" ] && ok "ETag 在册" || bad "ETag 缺失"
probe "GET / HTTP/1.1\r\nHost: t\r\nIf-None-Match: $ETAG\r\n\r\n" "$T/r3"
grep -q "304 Not Modified" "$T/r3" && ok "If-None-Match 304" || bad "If-None-Match 304"

# 3) OpenAPI 快照要点
probe 'GET /openapi.json HTTP/1.1\r\nHost: t\r\n\r\n' "$T/r4"
grep -q '"openapi":"3.0.3"' "$T/r4" && grep -q '"/todos/{id}"' "$T/r4" && ok "OpenAPI 同源导出" || bad "OpenAPI 导出"

# 4) CORS 预检
probe 'OPTIONS /todos HTTP/1.1\r\nHost: t\r\nOrigin: http://x\r\nAccess-Control-Request-Method: POST\r\n\r\n' "$T/r5"
grep -q "204 No Content" "$T/r5" && grep -q "Access-Control-Allow-Origin: \*" "$T/r5" && ok "CORS 预检 204+ACAO" || bad "CORS 预检"

# 5) JWT 拒未授权写
B1='{"title":"no token"}'
probe_post "/todos" "" "$B1" "$T/r6"
grep -q "401 Unauthorized" "$T/r6" && ok "无令牌写 401" || bad "无令牌写应 401"

# 6) 登录签发
B2='{"uid":"demo"}'
probe_post "/login" "" "$B2" "$T/r7"
grep -q "200 OK" "$T/r7" && ok "login 200" || bad "login"
TOK=$(sed -n 's/.*"token":"\([^"]*\)".*/\1/p' "$T/r7")
[ -n "$TOK" ] && ok "JWT 签发在册" || bad "JWT 签发"

# 7) CRUD 全链(带令牌)
B3='{"title":"first item"}'
B4='{"title":"second item"}'
probe_post "/todos" "$TOK" "$B3" "$T/r8"
grep -q "201 Created" "$T/r8" && grep -q '"title":"first item"' "$T/r8" && ok "POST /todos 201" || bad "POST /todos"
probe_post "/todos" "$TOK" "$B4" "$T/r9"
grep -q "201 Created" "$T/r9" && ok "POST 第二条 201" || bad "POST 第二条"
probe 'GET /todos HTTP/1.1\r\nHost: t\r\n\r\n' "$T/r10"
grep -q "first item" "$T/r10" && grep -q "second item" "$T/r10" && ok "GET 列表两条" || bad "GET 列表"
probe "DELETE /todos/1 HTTP/1.1\r\nHost: t\r\nAuthorization: Bearer $TOK\r\n\r\n" "$T/r11"
cp "$T/r11" /tmp/r11b.cap 2>/dev/null
cp "$T/r11" /tmp/r11c.cap 2>/dev/null
grep -q "204 No Content" "$T/r11" && ok "DELETE 204" || bad "DELETE"
probe 'GET /todos HTTP/1.1\r\nHost: t\r\n\r\n' "$T/r12"
grep -q "first item" "$T/r12" && bad "删除后仍在" || ok "删除生效"
grep -q "second item" "$T/r12" && ok "幸存项在册" || bad "幸存项丢失"

# 8) 优雅停机(排空退出)
probe_post "/__shutdown" "" "" "$T/r13"
grep -q "200 OK" "$T/r13" && ok "停机指令 200" || bad "停机指令"
sleep 0.5
if ! kill -0 "$SRV" 2>/dev/null; then
    ok "服务排空退出"
# 20.5) 停机后拒绝(P7-G 演练:排空退出后新连接必拒)
if nc -z -w 2 127.0.0.1 "$PORT" 2>/dev/null; then bad "停机后端口应关闭"; else ok "停机后新连接拒绝"; fi
else
    kill "$SRV" 2>/dev/null; bad "服务未退出"
fi
grep -q "drained" "$T/srv.log" && ok "排空日志在册" || bad "排空日志"

# ═══════════ 相位二:限流 503(阈值 2) ═══════════
PORT=$((PORT+1))
start_srv 2 || { echo "todo-e2e: 限流相位服务未就绪"; exit 1; }
probe 'GET /health HTTP/1.1\r\nHost: t\r\n\r\n' "$T/l1"
probe 'GET /health HTTP/1.1\r\nHost: t\r\n\r\n' "$T/l2"
probe 'GET /health HTTP/1.1\r\nHost: t\r\n\r\n' "$T/l3"
probe 'GET /health HTTP/1.1\r\nHost: t\r\n\r\n' "$T/l4"
grep -q "503 Service Unavailable" "$T/l4" && ok "限流 503(阈值后拒绝)" || bad "限流 503"
kill "$SRV" 2>/dev/null

echo "todo-e2e: pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
