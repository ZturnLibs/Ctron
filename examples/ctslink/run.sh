#!/bin/sh
# ctslink 门与冒烟 —— 三段,状态如实印:
#   [1] 语义门(interp;硬门):check + 裸跑(web_todo README 同款两行)
#       sh compiler/ctc.sh check examples/ctslink/src/main.ct
#       sh compiler/ctc.sh examples/ctslink/src/main.ct
#   [2] 原生臂发射+链接(门外语义,web_todo 同款路径):ctron-emit run
#       src/serve_net.ct → cc(链接 net c_src + db c_src:ctron_dbpg/
#       ctron_dbredis/ctron_entropy——uuid 真熵垫片宿主)
#   [3] loopback 冒烟:curl 全链 register→login→create→redirect→stats→delete
#       (CSRF cookie-jar 双提交;零 env = 内存 store + 内存计数;env
#       CTRON_REDIS_URL/CTRON_PG_DSN 设了才走 Redis/PG——PG 仅启动 DDL 应用,
#       v1 行存储恒内存,README 差异表)
# 原生臂现况:框架面两卡在册(报告/README「原生臂现况」):① net.ct 门面
# E4046(bind.ct Box 形参 v0.9 拒收;serve_net 已以消费侧 &T extern 直 declare
# 绕开)② emit 泛型 struct 载荷限定(Router{state:struct};web_todo serve_net
# 同红)。[2]/[3] 被卡时印 BLOCKED 行退出 0(框架侧问题不作本例门),[1] 失败
# 退出 1。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
CC=cc
PORT=${CTS_LINK_PORT:-8093}
pass=0; fail=0
T=$(mktemp -d /tmp/ctslink.XXXXXX)
SRV=""
cleanup() {
    [ -n "$SRV" ] && kill "$SRV" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

ok()  { pass=$((pass+1)); echo "  ok  : $1"; }
bad() { fail=$((fail+1)); echo "  FAIL: $1"; }

echo "== [1] 语义门(interp)=="
if sh "$ROOT/compiler/ctc.sh" check "$DIR/src/main.ct" > "$T/chk.log" 2>&1; then
    ok "check main.ct ($(tail -1 "$T/chk.log"))"
else
    bad "check main.ct"; head -5 "$T/chk.log"; exit 1
fi
if sh "$ROOT/compiler/ctc.sh" check "$DIR/src/serve_net.ct" > "$T/chk2.log" 2>&1; then
    ok "check serve_net.ct($(tail -1 "$T/chk2.log"))"
else
    bad "check serve_net.ct"; head -5 "$T/chk2.log"; exit 1
fi
echo "  -- 裸跑 test_call 语义门(mw 会话族 eval 期 CPU 重,分钟级)--"
if sh "$ROOT/compiler/ctc.sh" "$DIR/src/main.ct" > "$T/run.log" 2>&1; then
    ok "裸跑四组语义门全绿"
else
    bad "裸跑语义门"; head -20 "$T/run.log"; exit 1
fi

echo "== [2] 原生臂发射+链接 =="
if [ ! -x "$EMIT" ]; then
    echo "  BLOCKED(tree): 缺 compiler/bin/ctron-emit(先 compiler/native.sh);web_todo README 原生臂同径"
    echo "ctslink: pass=$pass fail=$fail(原生臂跳过)"
    exit 0
fi
if "$EMIT" run "$DIR/src/serve_net.ct" > "$T/sl.c" 2>"$T/emit.err"; then
    ok "emit serve_net.ct → C"
else
    echo "  BLOCKED(tree): emit 失败:$(head -1 "$T/emit.err")"
    echo "  (web_todo serve_net emit 同红实锚:泛型 struct 载荷限定 Router{state:struct};框架侧 emit 载荷面扩位挂志向)"
    echo "ctslink: pass=$pass fail=$fail(原生臂跳过)"
    exit 0
fi
if $CC -O1 -w -pthread -I"$ROOT/lib/net/c_src" -o "$T/sl.bin" "$T/sl.c" \
      "$ROOT"/net/c_src/ctron_net.c "$ROOT"/db/c_src/ctron_dbpg.c \
      "$ROOT"/db/c_src/ctron_dbredis.c "$ROOT"/db/c_src/ctron_entropy.c 2>"$T/cc.err"; then
    ok "cc 链接(net c_src + db c_src 三件)"
else
    echo "  BLOCKED(tree): cc 失败:$(head -2 "$T/cc.err")"
    echo "ctslink: pass=$pass fail=$fail(原生臂跳过)"
    exit 0
fi

echo "== [3] loopback 冒烟(:$PORT;内存 store)=="
env CTS_LINK_PORT="$PORT" "$T/sl.bin" > "$T/srv.log" 2>&1 &
SRV=$!
i=0
while [ "$i" -lt 50 ]; do
    curl -sf "http://127.0.0.1:$PORT/healthz" > /dev/null 2>&1 && break
    sleep 0.2; i=$((i+1))
done
if ! curl -sf "http://127.0.0.1:$PORT/healthz" > /dev/null 2>&1; then
    bad "服务未就绪"; head -5 "$T/srv.log"; exit 1
fi
ok "healthz 200 ok"
B="http://127.0.0.1:$PORT"
CJ="$T/jar"

# 注册页(seed csrf cookie)
code=$(curl -s -o /dev/null -w '%{http_code}' -c "$CJ" "$B/register")
[ "$code" = "200" ] && ok "GET /register 200" || bad "GET /register($code)"
TOK=$(grep -F 'csrf' "$CJ" | awk '{print $7}')
[ -n "$TOK" ] && ok "csrf 令牌 seed 在册" || bad "csrf 令牌缺失"

# 注册 → 302 /login
code=$(curl -s -o /dev/null -w '%{http_code}' -b "$CJ" -c "$CJ" \
    --data-urlencode "name=demo" --data-urlencode "pass=passw0rd" \
    --data-urlencode "csrf=$TOK" "$B/register")
[ "$code" = "302" ] && ok "POST /register 302" || bad "POST /register($code)"

# 登录 → 302 / + sid
code=$(curl -s -o /dev/null -w '%{http_code}' -b "$CJ" -c "$CJ" \
    --data-urlencode "name=demo" --data-urlencode "pass=passw0rd" \
    --data-urlencode "csrf=$TOK" "$B/login")
[ "$code" = "302" ] && ok "POST /login 302(会话签发)" || bad "POST /login($code)"

# 建链 → 302 / + flash "created <code>"
code=$(curl -s -o /dev/null -w '%{http_code}' -b "$CJ" -c "$CJ" \
    --data-urlencode "url=http://example.com/a" \
    --data-urlencode "csrf=$TOK" "$B/links")
[ "$code" = "302" ] && ok "POST /links 302" || bad "POST /links($code)"
FV=$(grep -F 'web_flash' "$CJ" | awk '{print $7}')
CODE=$(printf '%s' "$FV" | tail -c 7)
case "$CODE" in
    [a-z][a-z][a-z][a-z][a-z][a-z][a-z]) ok "code 7 位 [a-z0-9] 在册($CODE)" ;;
    *) bad "code 提取失败(flash=$FV)"; exit 1 ;;
esac

# 公开跳转 ×2(302 + 真址;计数 +2)
r1=$(curl -s -o /dev/null -w '%{http_code} %{redirect_url}' "$B/$CODE")
[ "$r1" = "302 http://example.com/a" ] && ok "公开跳转 302 → 真址" || bad "公开跳转($r1)"
curl -s -o /dev/null "$B/$CODE"
# 仪表盘含条目
if curl -s -b "$CJ" "$B/" | grep -qF "$CODE"; then ok "仪表盘列出短链"; else bad "仪表盘缺条目"; fi
# 统计(属主;点击 2)
st=$(curl -s -b "$CJ" "$B/links/$CODE/stats")
if printf '%s' "$st" | grep -qF '<p class="num">2</p>'; then ok "统计页点击计数 2"; else bad "统计页计数(缺 num=2)"; fi
# 删除 → 跳转转 404
code=$(curl -s -o /dev/null -w '%{http_code}' -b "$CJ" \
    --data-urlencode "csrf=$TOK" "$B/links/$CODE/delete")
[ "$code" = "302" ] && ok "POST delete 302" || bad "POST delete($code)"
code=$(curl -s -o /dev/null -w '%{http_code}' "$B/$CODE")
[ "$code" = "404" ] && ok "删除后跳转 404" || bad "删除后跳转($code)"

# 优雅停机
code=$(curl -s -o /dev/null -w '%{http_code}' -X POST "$B/__shutdown")
[ "$code" = "200" ] && ok "POST /__shutdown 200" || bad "停机($code)"
sleep 0.5
if kill -0 "$SRV" 2>/dev/null; then kill "$SRV" 2>/dev/null; bad "服务未退出"; else ok "服务排空退出"; fi

echo "ctslink: pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
