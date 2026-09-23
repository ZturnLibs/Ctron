#!/bin/sh
# ctecho run.sh —— 构建(emit+cc 链 net 垫片)+ 后台服务 + 3 客户端冒烟 + kill
# 口径:逐探针回显逐字比对(不等即 FAIL);nc -w 2 空闲超时收口(macOS nc
# 无 OpenBSD -N 半关旗,服务端读客户端 FIN 或 handler 超时皆收敛)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "ctecho: 缺 compiler/bin/ctron-emit(先: compiler/native.sh)" >&2; exit 2; }
export CTRON_STDPATH="$ROOT/std"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$EMIT" run "$DIR/src/main.ct" > "$T/ctecho.c" || exit 1
# P2-D:并链 ctron_rt.c(协程模式需要;默认环境 active()=0 仍走 P1 pthread 面,
# 源码零改动同形不变)。-pthread = rt 依赖;-I 供 ctron_net.c 的 ctron_rt.h。
cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/ctecho" "$T/ctecho.c" "$ROOT/net/c_src/ctron_net.c" "$ROOT/net/c_src/ctron_rt.c" || exit 1
PORT=${CTECHO_PORT:-$(( (RANDOM % 20000) + 30000 ))}
CTECHO_PORT=$PORT "$T/ctecho" > "$T/srv.log" 2>&1 & SRV=$!
sleep 0.5
fail=0
for i in 1 2 3; do
    got=$(printf "hello$i" | nc -w 2 127.0.0.1 "$PORT")
    if [ "$got" = "hello$i" ]; then
        echo "  probe$i ok"
    else
        fail=1; echo "  probe$i FAIL got=[$got]"
    fi
done
kill "$SRV" 2>/dev/null
wait "$SRV" 2>/dev/null
if [ "$fail" = 0 ]; then
    echo "ctecho smoke done (port $PORT, 3/3 echo)"
    exit 0
fi
echo "ctecho smoke FAILED (port $PORT)" >&2
cat "$T/srv.log" >&2
exit 1
