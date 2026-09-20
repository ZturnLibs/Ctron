#!/bin/sh
# rt_reactor_smoke —— P2-B 冒烟驱动:reactor(kqueue/epoll/poll 回退)+ wait_fd。
# build(cc -O1 -pthread,只链 ctron_rt.c + main.c,不链 ctron_net.c)
# + run ×(worker=2,4,CTRON_RT_WORKERS 注入)+ 断言;退出码即结果。
# 不入 tests/net/run.sh 主环:纯 C 冒烟目录(无 src/main.ct)被主环守卫跳过,由本 run.sh 承载。
set -eu
cd "$(dirname "$0")"

CC=${CC:-cc}
OUT=${TMPDIR:-/tmp}/rt_reactor_smoke_app.$$
trap 'rm -f "$OUT"' EXIT INT TERM

echo "== build: $CC -O1 -pthread c_src/ctron_rt.c src/main.c =="
"$CC" -O1 -pthread -Wall -I c_src -o "$OUT" c_src/ctron_rt.c src/main.c

for W in 2 4; do
    echo "== run (CTRON_RT=coro, CTRON_RT_WORKERS=$W) =="
    CTRON_RT=coro CTRON_RT_WORKERS=$W "$OUT"
    rc=$?
    if [ "$rc" -ne 0 ]; then
        echo "SMOKE FAIL: workers=$W rc=$rc"
        exit "$rc"
    fi
done
echo "== SMOKE GREEN =="
