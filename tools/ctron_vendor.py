#!/usr/bin/env python3
"""ctron vendor 原型 —— std 模块 use 闭集自动拷贝 + 钉版清单(09-21 spec §5.4)。

用法:
    python3 tools/ctron_vendor.py std.str,std.csv OUT_DIR
    python3 tools/ctron_vendor.py str,csv OUT_DIR        # 前缀可省

行为:
  1. 以给定模块为根 BFS 解析 `use std.<名>.{...}` 闭集(仅 T1 纯核心单文件形态;
     域包目录/缺失即 fail-closed 报错退出)。
  2. 逐模块拷贝 std/<名>.ct → OUT_DIR/<名>.ct(与 examples vendored 布局一致,
     消费方 CTRON_STDPATH=OUT_DIR 即用)。
  3. 生成 OUT_DIR/VENDOR.lock:每行 `<名> sha256:<摘要>`,字节级钉版
     (与闭源分发泳道 digest 形态同构;升级 = 重跑本工具后 lock diff 可见)。
"""

import argparse
import hashlib
import os
import re
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STD = os.path.join(ROOT, "std")
USE_RE = re.compile(r"^use std\.([a-z_]+(?:\.[a-z_]+)*)\.\{")


def fail(msg: str) -> "None":
    print(f"ctron vendor: {msg}", file=sys.stderr)
    sys.exit(1)


def closure(roots: "list[str]") -> "list[str]":
    seen: "list[str]" = []
    stack = list(reversed(roots))
    while stack:
        m = stack.pop()
        if m in seen:
            continue
        path = os.path.join(STD, m + ".ct")
        if not os.path.isfile(path):
            fail(f"std/{m}.ct 不存在(域包目录形态与缺失均不在 vendor 原型范围)")
        with open(path, encoding="utf-8") as fh:
            body = fh.read()
        if 'extern "c"' in body or "#[link" in body:
            fail(f"{m} 为 T2/T3 域包(C 边界面)——vendored 闭集仅限 T1 纯核心,"
                 f"C 运行时与链接口径须随包整体携带,不适用本工具")
        seen.append(m)
        for line in body.splitlines():
            hit = USE_RE.match(line)
            if hit:
                dep = hit.group(1)
                if "." in dep:
                    fail(f"{m} 依赖域包子模块 std.{dep}(T2/T3 形态)——"
                         f"vendored 闭集仅限 T1 纯核心")
                if dep not in seen:
                    stack.append(dep)
    return seen


def main() -> None:
    ap = argparse.ArgumentParser(description="std 模块 use 闭集拷贝 + 钉版清单")
    ap.add_argument("modules", help="逗号分隔模块名(如 std.str,std.csv)")
    ap.add_argument("out", help="输出目录(vendored std 根)")
    args = ap.parse_args()

    roots = [m.split(".")[-1] for m in args.modules.split(",") if m.strip()]
    if not roots:
        fail("模块清单为空")
    os.makedirs(args.out, exist_ok=True)

    lines = []
    for m in closure(roots):
        src = os.path.join(STD, m + ".ct")
        shutil.copyfile(src, os.path.join(args.out, m + ".ct"))
        digest = hashlib.sha256(open(src, "rb").read()).hexdigest()
        lines.append(f"{m} sha256:{digest}")

    lock = os.path.join(args.out, "VENDOR.lock")
    with open(lock, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")
    print(f"ctron vendor: {len(lines)} 模块 → {args.out}(钉版 VENDOR.lock)")


if __name__ == "__main__":
    main()
