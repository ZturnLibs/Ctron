#!/bin/sh
# ci.sh —— 一条命令全量验证门禁(自举编译器线)
# 前置:宿主 seed 已构建(make -C compiler-c);gcc/python3 可用;[9/9] fmt 对拍需 cargo(Rust 参考臂)。
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

echo "[1/9] 解析器/测试集自检"
python3 tests/meta_check.py
python3 tests/manifest/run.py
python3 tests/manifest/diff.py
python3 tools/ctcl_check.py --selftest

echo "[2/9] 模块拼接"
sh "$DIR/compiler/build.sh"

echo "[3/9] 自举出原生四件套(ctron-cc/chk/emit/fmt;冒烟与测试集的原生臂前置)"
sh "$DIR/compiler/native.sh"

echo "[4/9] 验收冒烟(--full,含并发/枚举/fn 值发射/自举固定点/fmt 金样)+ tests/ 一致性测试集(对照 C 参考宿主)+ rt 纯 C 冒烟(coro 调度器与 reactor 后端矩阵)"
sh "$DIR/compiler/test/smoke.sh" --full
python3 "$DIR/compiler/test/suite.py"

# T51 §9.5:rt 纯 C 冒烟(coro 调度器 + reactor wait_fd 全契约)。秒级、
# 零依赖、仅回环;reactor 后端双臂同形(linux 族 = epoll 点名 + io_uring
# 点名,后者内核自检不过时响亮登记回退;darwin = kqueue 单臂)。
sh "$DIR/tests/net/rt_core_smoke/run.sh"
sh "$DIR/tests/net/rt_reactor_smoke/run.sh"

echo "[5/9] ctron 驱动冒烟(run/check/build/fmt 契约/help/无 cc 路径)"
sh "$DIR/tests/dist/ctron_smoke.sh"

echo "[5.5/9] 插件沙箱阶梯(T52:derive+lint 五锚,interp/emit 双臂+确定性)"
sh "$DIR/tests/plugins/run.sh"

echo "[6/9] 性能基线冒烟(发射一致性 + 后端加速比)"
sh "$DIR/compiler/bench.sh" 2>&1 | tail -12

echo "[7/9] FFI 边界微基准(跨 C-ABI)+ GC 性能登记(§9.4;digest 硬门+比值 WARN)"
sh "$DIR/compiler/test/bench_ffi.sh" 2>&1 | tail -16
sh "$DIR/tests/gc/bench.sh" 2>&1 | tail -3

# T31 事故回归探针(2026-10-05):发射确定性双环境——编译器在 CTRON_GC=1 下自身
# 被 GC 化,发射产物必须与 off 逐字一致(s/N 槽注册+盒类 bump 的常驻守门哨)
PT=$(mktemp -d)
CTRON_STDPATH="$DIR/lib/std" CTRON_GC=1 "$DIR/compiler/bin/ctron-emit" run "$DIR/compiler/build/cc_run.ct" > "$PT/emit_gc1.c" 2>/dev/null
env -u CTRON_GC CTRON_STDPATH="$DIR/lib/std" "$DIR/compiler/bin/ctron-emit" run "$DIR/compiler/build/cc_run.ct" > "$PT/emit_off.c" 2>/dev/null
if ! diff -q "$PT/emit_gc1.c" "$PT/emit_off.c" > /dev/null 2>&1; then
    echo "[FAIL] 发射确定性双环境探针分歧(编译器 GC 化输出漂移;归 GC 泳道)" >&2
    rm -rf "$PT"
    exit 1
fi
rm -rf "$PT"
echo "  ok  : 发射确定性双环境探针(GC=1/off 大语料发射逐字一致)"
sh "$DIR/tests/lang/bench/bench.sh" 2>&1 | tail -3

echo "[8/9] GUI 阶梯(S1–S9 headless:布局桥/事件/绑定竖切/命令缓冲断言/FreeType 中文)"
# W5 门禁挂载(2026-09-19)。headless 断言无显示依赖,xvfb 仅未来窗口冒烟所需。
# Linux 首跑 vendored 构建(GLFW)需 X11/GL 开发头:缺头时显式 skip 并指路(非静默假绿),
# 包名待 Linux CI 首验后钉死并回填 ci.yml(登记 MVP 阶梯 W5)。
RUN_GUI=1
if [ "$(uname)" = "Linux" ]; then
    if [ ! -f /usr/include/X11/Xlib.h ] || [ ! -f /usr/include/GL/gl.h ]; then
        RUN_GUI=0
    fi
fi
if [ "$RUN_GUI" = "1" ]; then
    sh "$DIR/tests/gui/run.sh"
else
    echo "[skip] GUI 阶梯:Linux 缺 X11/GL 开发头——apt install libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev libgl-dev 后重跑即接入(W5 环境前置)"
fi

echo "[9/9] fmt 三宿主对拍(Rust 参考/C 宿主/自举;R-P2d 移植验收)"
sh "$DIR/tests/fmt/parity.sh"

echo "CI: 全部通过 ✓"
