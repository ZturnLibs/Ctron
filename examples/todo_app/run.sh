#!/bin/sh
# todo_app e2e —— 浏览器形态多用户 todo 管理应用全链验收:
#   相位一:首页分流/注册(重名/短密拒)/登录(错密拒,成则 Set-Cookie sid)/
#           未登录 /app 302/空态/加两条/翻一条(勾选面)/删一条/多用户隔离(bob
#           看不到 alice 条目)/登出
#   相位二:同 data 目录重启 → 再登录 → 条目与 done 态保持(持久化)
# 口径:仅回环;TODO_APP_PORT(缺省 8092);发射臂构建;服务端串行
# (Connection: close);探针 = 单连接单请求(printf %b 发真实 CRLF)。
# 红账(2026-09-26):原生臂暂被编译器在册发射缺陷阻断(主分发预算家族,
# 见 docs/c-rust-divergences.md「todo_app 复现族」节)——emit 失败即本缺陷,
# 非应用代码;四模块 check 0E + ctc test 全绿为当前语义门,发射修复后本脚本
# 即为完整验收门(两相位含重启持久化)。可用 CTRON_EMIT= 指向健康二进制。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT=${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}
CC=cc
export CTRON_STDPATH="$ROOT/std"
PORT=${TODO_APP_PORT:-8092}
pass=0; fail=0
T=$(mktemp -d /tmp/todoapp.XXXXXX)
SRV=""
cleanup() {
    [ -n "$SRV" ] && kill "$SRV" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
[ -x "$EMIT" ] || { echo "todo-app-e2e: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; }

ok()  { pass=$((pass+1)); echo "  ok  : $1"; }
bad() { fail=$((fail+1)); echo "  FAIL: $1"; }

# HTTP 探针:%b 解 \r\n 为真实 CRLF;输出落 $2
probe() {
    printf '%b' "$1" | timeout 10 nc 127.0.0.1 "$PORT" > "$2" 2>/dev/null
    return 0
}

# POST 表单探针:$1=target $2=cookie(可空) $3=表单体 $4=输出
probe_form() {
    local cl=${#3}
    local ck=""
    if [ -n "$2" ]; then ck="Cookie: sid=$2\r\n"; fi
    probe "POST $1 HTTP/1.1\r\nHost: t\r\n${ck}Content-Type: application/x-www-form-urlencoded\r\nContent-Length: $cl\r\n\r\n$3" "$4"
}

# ── 构建 ──
mkdir -p "$T/data"
if ! "$EMIT" run "$DIR/src/main.ct" > "$T/app.c" 2>"$T/emit.err"; then
    echo "todo-app-e2e: emit 失败" >&2; head -3 "$T/emit.err"; exit 1
fi
if ! $CC -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/app.bin" "$T/app.c" "$ROOT"/net/c_src/ctron_net.c 2>"$T/cc.err"; then
    echo "todo-app-e2e: cc 失败" >&2; head -5 "$T/cc.err"; exit 1
fi

start_srv() {
    env TODO_APP_PORT="$PORT" TODO_APP_DATA="$T/data" TODO_APP_ITERS="${TODO_APP_ITERS:-200}" "$T/app.bin" > "$T/srv.log" 2>&1 &
    SRV=$!
    local i=0
    while [ "$i" -lt 25 ]; do
        printf 'GET /health HTTP/1.1\r\nHost: t\r\n\r\n' | timeout 3 nc 127.0.0.1 "$PORT" 2>/dev/null | grep -q "200 OK" && return 0
        sleep 0.2; i=$((i+1))
    done
    return 1
}

# ═══════════ 相位一:全链 ═══════════
start_srv || { echo "todo-app-e2e: 服务未就绪"; cat "$T/srv.log"; exit 1; }
ok "服务就绪(:$PORT)"

# 1) 健康
probe 'GET /health HTTP/1.1\r\nHost: t\r\n\r\n' "$T/r1"
grep -q "200 OK" "$T/r1" && grep -q '"ok":true' "$T/r1" && ok "健康端点 200 {ok:true}" || bad "健康端点"

# 2) 首页分流(未登录 → /login)
probe 'GET / HTTP/1.1\r\nHost: t\r\n\r\n' "$T/r2"
grep -q "303" "$T/r2" && grep -q "Location: /login" "$T/r2" && ok "未登录首页 303 → /login" || bad "首页分流"

# 3) 登录页
probe 'GET /login HTTP/1.1\r\nHost: t\r\n\r\n' "$T/r3"
grep -q "200 OK" "$T/r3" && grep -q 'type="password"' "$T/r3" && ok "登录页 200(密码框在册)" || bad "登录页"

# 4) 注册 alice → 303 /login
probe_form "/register" "" "uid=alice&pass=secret1" "$T/r4"
grep -q "303" "$T/r4" && grep -q "Location: /login" "$T/r4" && ok "注册 alice 303 → /login" || bad "注册 alice"

# 5) 重复注册拒(页内横幅)
probe_form "/register" "" "uid=alice&pass=secret1" "$T/r5"
grep -q "200 OK" "$T/r5" && grep -q "用户名已存在" "$T/r5" && ok "重复注册 200+横幅" || bad "重复注册拒"

# 6) 短密码拒
probe_form "/register" "" "uid=carl&pass=abc" "$T/r6"
grep -q "200 OK" "$T/r6" && grep -q "密码至少 6 位" "$T/r6" && ok "短密码 200+横幅" || bad "短密码拒"

# 7) 错密登录拒
probe_form "/login" "" "uid=alice&pass=wrong99" "$T/r7"
grep -q "200 OK" "$T/r7" && grep -q "用户名或密码不对" "$T/r7" && ok "错密登录 200+横幅" || bad "错密登录拒"

# 8) 正确登录 → 303 + Set-Cookie sid
probe_form "/login" "" "uid=alice&pass=secret1" "$T/r8"
grep -q "303" "$T/r8" && grep -q "Set-Cookie: sid=" "$T/r8" && ok "登录 303 + Set-Cookie sid" || bad "登录签发"
ALICE_SID=$(grep -i "^Set-Cookie: sid=" "$T/r8" | head -1 | sed 's/^Set-Cookie: sid=\([^;]*\).*/\1/' | tr -d "\r")
[ -n "$ALICE_SID" ] && ok "sid 抠取在册" || bad "sid 抠取"

# 9) 未登录 /app → 303 /login
probe 'GET /app HTTP/1.1\r\nHost: t\r\n\r\n' "$T/r9"
grep -q "303" "$T/r9" && grep -q "Location: /login" "$T/r9" && ok "未登录 /app 303" || bad "/app 门卫"

# 10) alice /app 空态
probe "GET /app HTTP/1.1\r\nHost: t\r\nCookie: sid=$ALICE_SID\r\n\r\n" "$T/r10"
grep -q "200 OK" "$T/r10" && grep -q "暂无条目" "$T/r10" && grep -q "alice" "$T/r10" && ok "alice /app 空态在册" || bad "/app 空态"

# 11) 加两条
probe_form "/app/add" "$ALICE_SID" "title=alpha" "$T/r11"
grep -q "303" "$T/r11" && ok "加 alpha 303" || bad "加 alpha"
probe_form "/app/add" "$ALICE_SID" "title=beta" "$T/r12"
grep -q "303" "$T/r12" && ok "加 beta 303" || bad "加 beta"

# 12) 列表双现
probe "GET /app HTTP/1.1\r\nHost: t\r\nCookie: sid=$ALICE_SID\r\n\r\n" "$T/r13"
grep -q "alpha" "$T/r13" && grep -q "beta" "$T/r13" && ok "列表 alpha+beta 双现" || bad "列表双现"

# 13) 翻转 id=1 → done 面
probe_form "/app/toggle/1" "$ALICE_SID" "" "$T/r14"
grep -q "303" "$T/r14" && ok "toggle 303" || bad "toggle"
probe "GET /app HTTP/1.1\r\nHost: t\r\nCookie: sid=$ALICE_SID\r\n\r\n" "$T/r15"
grep -q 'class="done"' "$T/r15" && grep -q "&#9745;" "$T/r15" && ok "alpha done 面在册" || bad "done 面"

# 14) 删 id=2
probe_form "/app/delete/2" "$ALICE_SID" "" "$T/r16"
grep -q "303" "$T/r16" && ok "delete 303" || bad "delete"
probe "GET /app HTTP/1.1\r\nHost: t\r\nCookie: sid=$ALICE_SID\r\n\r\n" "$T/r17"
grep -q "alpha" "$T/r17" && ! grep -q "beta" "$T/r17" && ok "删 beta 生效,alpha 幸存" || bad "删 beta"

# 15) 多用户隔离:bob 注册登录,看不到 alpha
probe_form "/register" "" "uid=bob&pass=bobpass1" "$T/r18"
grep -q "303" "$T/r18" && ok "注册 bob 303" || bad "注册 bob"
probe_form "/login" "" "uid=bob&pass=bobpass1" "$T/r19"
BOB_SID=$(grep -i "^Set-Cookie: sid=" "$T/r19" | head -1 | sed 's/^Set-Cookie: sid=\([^;]*\).*/\1/' | tr -d "\r")
[ -n "$BOB_SID" ] && ok "bob sid 在册" || bad "bob sid"
probe "GET /app HTTP/1.1\r\nHost: t\r\nCookie: sid=$BOB_SID\r\n\r\n" "$T/r20"
grep -q "暂无条目" "$T/r20" && ! grep -q "alpha" "$T/r20" && ok "bob 空态(隔离生效)" || bad "多用户隔离"

# 16) 登出
probe_form "/logout" "$ALICE_SID" "" "$T/r21"
grep -q "303" "$T/r21" && grep -q "Max-Age=0" "$T/r21" && ok "登出 303 + 清 cookie" || bad "登出"

# 17) metrics(todo_users gauge)
probe 'GET /metrics HTTP/1.1\r\nHost: t\r\n\r\n' "$T/r22"
grep -q 'todo_users' "$T/r22" && grep -q 'todo_items' "$T/r22" && ok "/metrics gauge 在册" || bad "/metrics"

# ═══════════ 相位二:同 data 重启持久化 ═══════════
PORT=$((PORT+1))
kill "$SRV" 2>/dev/null
SRV=""
sleep 0.5
start_srv || { echo "todo-app-e2e: 相位二服务未就绪"; exit 1; }
probe_form "/login" "" "uid=alice&pass=secret1" "$T/r23"
ALICE_SID2=$(grep -i "^Set-Cookie: sid=" "$T/r23" | head -1 | sed 's/^Set-Cookie: sid=\([^;]*\).*/\1/' | tr -d "\r")
[ -n "$ALICE_SID2" ] && ok "重启后 alice 再登录" || bad "重启后登录"
probe "GET /app HTTP/1.1\r\nHost: t\r\nCookie: sid=$ALICE_SID2\r\n\r\n" "$T/r24"
if grep -q "alpha" "$T/r24" && grep -q 'class="done"' "$T/r24" && ! grep -q "beta" "$T/r24"; then
    ok "持久化:alpha 在册且 done 态保持,beta 仍缺"
else
    bad "持久化"
fi
kill "$SRV" 2>/dev/null
SRV=""

echo "todo-app-e2e: pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
