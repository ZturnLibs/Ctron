#!/bin/sh
# ci.sh —— 全量验证门禁(W1 起委派 ctron gate;九步编排已迁仓库根 ctron.ctcl。
# 前置:宿主 seed 已构建(make -C compiler-c);gcc/python3 可用;fmt 对拍需 cargo。)
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec sh "$DIR/ctron" gate
