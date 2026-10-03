#!/bin/sh
# rt_reactor_smoke —— P2-B 冒烟驱动:reactor(kqueue/io_uring/epoll/poll)+ wait_fd。
# build(cc -O1 -pthread,只链 ctron_rt.c + main.c,不链 ctron_net.c)
# + run ×(worker=2,4 × 后端臂)+ 断言;退出码即结果。
# 不入 tests/net/run.sh 主环:纯 C 冒烟目录(无 src/main.ct)被主环守卫跳过,由本 run.sh 承载。
# T51 §9.5 后端矩阵(双后端同形):同一 main.c 在各后端整跑一遍 ——
#   darwin = kqueue 单臂;linux 族 = epoll 点名 + io_uring 点名双臂(运行时
#   探测胜出者受 env 压制点名);内核 <5.5(无 NODROP,io_uring 契约不容)
#   ⇒ io_uring 臂显式登记跳过(环境探测先行,不静默、不硬炸);其他平台 =
#   poll 单臂。io_uring 点名而环境不给(内核自称支持却被沙箱拦)→ rt abort
#   响亮,属环境问题立案。
set -eu
cd "$(dirname "$0")"

CC=${CC:-cc}
OUT=${TMPDIR:-/tmp}/rt_reactor_smoke_app.$$
trap 'rm -f "$OUT"' EXIT INT TERM

echo "== build: $CC -O1 -pthread c_src/ctron_rt.c src/main.c =="
"$CC" -O1 -pthread -Wall -I c_src -o "$OUT" c_src/ctron_rt.c src/main.c

# 后端臂按平台定夺(T51):每臂 = CTRON_RT_REACTOR 点名(" " = auto 单臂)
case "$(uname)" in
    Darwin)
        ARMS=" "                              # kqueue 单臂
        ;;
    Linux)
        # 探测门:kernel ≥5.5(NODROP/POLL_REMOVE 同代)才开 io_uring 臂
        K=$(uname -r | sed 's/^\([0-9][0-9]*\)\.\([0-9][0-9]*\).*/\1.\2/')
        KMAJ=${K%%.*}; KMIN=${K##*.}
        if [ $((KMAJ * 100 + KMIN)) -ge 505 ]; then
            ARMS="epoll io_uring"             # 双后端同形矩阵
        else
            echo "== skip io_uring arm: kernel $K < 5.5(NODROP 探测门不过,epoll 单臂)"
            ARMS="epoll"
        fi
        ;;
    *)
        ARMS=" "                              # poll 回退单臂
        ;;
esac

for RB in $ARMS; do
    for W in 2 4; do
        if [ "$RB" = " " ]; then
            case "$(uname)" in
                Darwin) EXP="kqueue" ;;
                Linux)  EXP="uring-any" ;;   # auto:io_uring 自检过则 uring,否则 epoll
                *)      EXP="poll" ;;
            esac
            echo "== run (CTRON_RT=coro, CTRON_RT_WORKERS=$W, backend=auto, expect=$EXP) =="
            CTRON_RT=coro CTRON_RT_WORKERS=$W CTRON_SMOKE_EXPECT_REACTOR="$EXP" "$OUT"
        else
            # io_uring 点名臂:内核自检可合法回退 epoll(rt 侧响亮登记),
            # 断言口径同为 uring-any;epoll 点名臂保持严格
            case "$RB" in
                io_uring) EXP="uring-any" ;;
                *)        EXP="$RB" ;;
            esac
            echo "== run (CTRON_RT=coro, CTRON_RT_WORKERS=$W, CTRON_RT_REACTOR=$RB, expect=$EXP) =="
            CTRON_RT=coro CTRON_RT_WORKERS=$W CTRON_RT_REACTOR="$RB" \
                CTRON_SMOKE_EXPECT_REACTOR="$EXP" "$OUT"
        fi
        rc=$?
        if [ "$rc" -ne 0 ]; then
            echo "SMOKE FAIL: backend=$RB workers=$W rc=$rc"
            exit "$rc"
        fi
    done
done
echo "== SMOKE GREEN =="
