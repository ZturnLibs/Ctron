#!/bin/sh
# ci.sh —— 一条命令全量验证门禁(自举编译器线)
# 前置:宿主 seed 已构建(make -C compiler-c);gcc/python3 可用。
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

echo "[1/7] 解析器/测试集自检"
python3 tests/meta_check.py
python3 tests/manifest/run.py
python3 tests/manifest/diff.py
python3 tools/ctcl_check.py --selftest

echo "[2/7] 模块拼接"
sh "$DIR/compiler/build.sh"

echo "[3/7] 验收冒烟(--full,含并发/枚举/fn 值发射与自举固定点)"
sh "$DIR/compiler/test/smoke.sh" --full

echo "[4/7] 原生二进制重建 + tests/ 一致性测试集(对照 C 参考宿主)"
sh "$DIR/compiler/native.sh"
python3 "$DIR/compiler/test/suite.py"

echo "[5/7] ctc 驱动冒烟(run/check/build/help/无 cc 路径)"
sh "$DIR/tests/dist/ctc_smoke.sh"

echo "[6/7] 性能基线冒烟(发射一致性 + 后端加速比)"
sh "$DIR/compiler/bench.sh" 2>&1 | tail -12

echo "[7/7] FFI 边界微基准(跨 C-ABI)"
sh "$DIR/compiler/test/bench_ffi.sh" 2>&1 | tail -16

echo "CI: 全部通过 ✓"
