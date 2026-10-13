#!/bin/sh
# bootstrap.sh —— Ctron 三级引导入口:C 宿主只建第一个原生 cc,其余全部跑在自举产物上。
#
# 用法: ./bootstrap.sh [--full]
#   阶段 0(首次引导):C 宿主种子编译"自举编译器模块" → 发射 C → gcc → 原生 cc(nc)
#   阶段 1(自举执行):以 nc 为 CTRON_SEED 重跑整个 ladder(--full 透传)
#   阶段 2(驱动器级,W4 波3 批4):以 nc 为现工具链自建原生用户驱动器 bin/ctron
#     (compiler/driver/src/main.ct → emit → 剥模板包装臂 → 链 proc 垫片 → 冒烟)
#
# 依赖:仅阶段 0 需要 compiler-c/build/ctronc;之后 C 宿主不再参与。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$DIR")
CHOST="$ROOT/compiler-c/build/ctronc"
T=$(mktemp -d /tmp/ctron_boot.XXXXXX)
trap 'rm -rf "$T"' EXIT

[ -x "$CHOST" ] || { echo "bootstrap: 缺少首次引导种子 $CHOST(先: make -C compiler-c)" >&2; exit 2; }

echo "== 阶段 0:首次引导(C 宿主建第一个原生 cc) =="
( cd "$DIR" && python3 tools/genmod.py "$DIR/cc.ct" "$T/boot_cc.ct" --trans ) || exit 2
( cd "$ROOT/compiler-c" && timeout 300 "$CHOST" run "$T/boot_cc.ct" > "$T/boot_cc.c" 2>&1 ) || exit 2
cc -O1 -w -o "$T/nc.bin" "$T/boot_cc.c" || { echo "bootstrap: nc 编译失败" >&2; exit 2; }
echo "原生 cc 就绪: $T/nc.bin(本次引导临时目录,随脚本退出清理)"

echo "== 阶段 1:自举产物做种子,重跑全部阶梯 =="
CTRON_BOOT=1 CTRON_SEED="$T/nc.bin" "$DIR/ladder.sh" ${1:-}
rc=$?

echo "== 阶段 2:驱动器级(现工具链自建原生用户驱动器) =="
# 与编译器同梯(Ctron 用 Ctron 构建 Ctron):nc 为解释器跑发射管线(ctc.sh emit
# 同构:build.sh 拼接 + ANCHOR 注入 + std 根注入 + 暂存发射),native.sh 驱动链
# 同款守卫+剥臂(发射模板 C main 包装 --version/run 两臂,不剥则驱动器自带面不可达),
# 链 proc 垫片(use proc 委派族缺符号)。冒烟:--version rc0 有输出 + --help rc0。
# 委派族对拍归 tests/drv_parity(dev 布局),本级不重复。
if [ "$rc" = 0 ]; then
    drv_ok=0
    sh "$ROOT/compiler/build.sh" || drv_ok=1
    if [ "$drv_ok" = 0 ]; then
        sed -e "s|ANCHORINPUT|$ROOT/compiler/driver/src/main.ct|" \
            -e "s|ANCHORLANG|zh|" \
            "$ROOT/compiler/build/cc_emit.ct" > "$T/emit_drv.ct"
        CTRON_STDPATH="$ROOT/lib/std" timeout 600 "$T/nc.bin" run "$T/emit_drv.ct" > "$T/ctron_drv.c.wip"
        drv_rc=$?
        if [ "$drv_rc" = 0 ]; then
            mv "$T/ctron_drv.c.wip" "$T/ctron_drv.c"
            DRV_A='if (argc >= 2 && !strcmp(argv[1], "--version")) { puts(ctron_version); return 0; }'
            DRV_B='if (argc >= 3 && strcmp(argv[1], "run") == 0) { ctron_cli_input = argv[2]; ctron_argv_off = 3; }'
            if grep -qF "$DRV_A" "$T/ctron_drv.c" && grep -qF "$DRV_B" "$T/ctron_drv.c" \
               && sed -e "s|if (argc >= 2 && !strcmp(argv\[1\], \"--version\")) { puts(ctron_version); return 0; }||" \
                      -e "s|if (argc >= 3 && strcmp(argv\[1\], \"run\") == 0) { ctron_cli_input = argv\[2\]; ctron_argv_off = 3; }||" \
                      "$T/ctron_drv.c" > "$T/ctron_drv.s" \
               && mv "$T/ctron_drv.s" "$T/ctron_drv.c" \
               && cc -O2 -w -o "$T/ctron_drv.bin" "$T/ctron_drv.c" "$ROOT/lib/proc/c_src/ctron_proc.c"; then
                V_OUT=$("$T/ctron_drv.bin" --version) && [ -n "$V_OUT" ] \
                    && "$T/ctron_drv.bin" --help 2>&1 | grep -q "用法" && drv_ok=0 || drv_ok=1
            else
                drv_ok=1
            fi
        else
            drv_ok=1
        fi
    fi
    if [ "$drv_ok" = 0 ]; then
        echo "bootstrap: 驱动器级通过(现工具链自建 bin/ctron;--version/--help 冒烟绿)"
    else
        echo "bootstrap: 驱动器级失败(自建驱动器构建或冒烟不过)" >&2
        rc=1
    fi
else
    echo "bootstrap: 阶梯失败,驱动器级跳过(rc=$rc)" >&2
fi

if [ "$rc" = 0 ]; then
    echo "bootstrap: 全部阶梯在自举二进制上通过 ✓"
else
    echo "bootstrap: 阶梯失败(rc=$rc)" >&2
fi
exit "$rc"
