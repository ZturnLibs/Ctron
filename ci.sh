#!/bin/sh
# ci.sh —— 一条命令全量验证门禁(自举编译器线)
# 前置:宿主 seed 已构建(make -C compiler-c);gcc/python3 可用。
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

echo "[1/5] 解析器/测试集自检"
python3 tests/meta_check.py

echo "[2/5] 模块拼接"
sh "$DIR/compiler/build.sh"

echo "[3/5] 验收冒烟(--full,含并发/枚举/fn 值发射与自举固定点)"
sh "$DIR/compiler/test/smoke.sh" --full

echo "[4/5] 原生二进制重建 + tests/ 一致性测试集(对照 C 参考宿主)"
sh "$DIR/compiler/native.sh"
python3 "$DIR/compiler/test/suite.py"

echo "[5/5] 性能基线冒烟(发射一致性 + 后端加速比)"
sh "$DIR/compiler/bench.sh" 2>&1 | tail -12

echo "CI: 全部通过 ✓"
