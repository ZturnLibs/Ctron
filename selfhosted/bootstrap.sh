#!/bin/sh
# bootstrap.sh —— Ctron 两级引导入口:C 宿主只建第一个原生 cc,其余全部跑在自举产物上。
#
# 用法: ./bootstrap.sh [--full]
#   阶段 0(首次引导):C 宿主种子编译"自举编译器模块" → 发射 C → gcc → 原生 cc(nc)
#   阶段 1(自举执行):以 nc 为 CTRON_SEED 重跑整个 ladder(--full 透传)
#
# 依赖:仅阶段 0 需要 compiler_c/build/ctronc;之后 C 宿主不再参与。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$DIR")
CHOST="$ROOT/compiler_c/build/ctronc"
T=$(mktemp -d /tmp/ctron_boot.XXXXXX)
trap 'rm -rf "$T"' EXIT

[ -x "$CHOST" ] || { echo "bootstrap: 缺少首次引导种子 $CHOST(先: make -C compiler_c)" >&2; exit 2; }

echo "== 阶段 0:首次引导(C 宿主建第一个原生 cc) =="
( cd "$DIR" && python3 tools/genmod.py "$DIR/cc.ct" "$T/boot_cc.ct" --trans ) || exit 2
( cd "$ROOT/compiler_c" && timeout 300 "$CHOST" run "$T/boot_cc.ct" > "$T/boot_cc.c" 2>&1 ) || exit 2
cc -O1 -w -o "$T/nc.bin" "$T/boot_cc.c" || { echo "bootstrap: nc 编译失败" >&2; exit 2; }
echo "原生 cc 就绪: $T/nc.bin(本次引导临时目录,随脚本退出清理)"

echo "== 阶段 1:自举产物做种子,重跑全部阶梯 =="
CTRON_BOOT=1 CTRON_SEED="$T/nc.bin" "$DIR/ladder.sh" ${1:-}
rc=$?
if [ "$rc" = 0 ]; then
    echo "bootstrap: 全部阶梯在自举二进制上通过 ✓"
else
    echo "bootstrap: 阶梯失败(rc=$rc)" >&2
fi
exit "$rc"
