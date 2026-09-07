#!/usr/bin/env python3
"""Ctron 测试集元检查。

在编译器存在之前,先保证测试集自身的格式、标记与错误码引用一致。
规则来源:tests/README.md §1–§4。测试集变更后运行:python3 tests/meta_check.py
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).parent

# 与 README §4 同步的错误码注册表
ERROR_CODES = {
    "E1001": "解析错误(通用语法违规;含比较不可链)",
    "E2010": "类型不匹配",
    "E2020": "未解析的名称",
    "E2030": "match 不穷尽",
    "E3010": "spawn 捕获了非 Send 值",
    "E3020": "channel 收发非 Send 类型",
    "E3030": "static var 不存在",
    "E3031": "非 Send 类型作为全局/静态存储",
    "E3040": "no_alloc 上下文中出现 GC/String 分配",
    "E3050": "own 块内 move/borrow 违规",
    "E3060": "own 块内对 GC 值可变借用",
    "E3070": "闭包可变捕获未显式 Mutex[T] 包装(R 线 R-P3a)",
    "E4010": "能力使用超出 manifest 声明",
    "E4020": "#[pure] 函数含副作用",
    "E4030": "#[no_spawn] 上下文 spawn",
    "E5010": "trait 孤儿规则违规",
    "E5020": "循环依赖",
    "E6010": "comptime 预算超限",
    "E6020": "comptime 副作用/不确定",
    "W8010": "struct 含可变类引用字段(浅共享)",
    "W8020": "must-use 结果被丢弃",
}

MARKER_RE = re.compile(r"^//@\s*(\w+)\s*:\s*(.+?)\s*$")
CODE_RE = re.compile(r"^[EW]\d{4}$")
TARGETS = {"full", "web", "bare"}


def kind_of(path: Path) -> str | None:
    name = path.name
    if name.endswith(".neg.ct"):
        return "neg"
    if name.endswith(".lint.ct"):
        return "lint"
    if name.endswith(".panic.ct"):
        return "panic"
    if name.endswith(".ct"):
        return "behavior"
    return None


def check_file(path: Path) -> list[str]:
    errors = []
    kind = kind_of(path)
    if kind is None:
        return errors

    markers: dict[str, list[str]] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        m = MARKER_RE.match(line.strip())
        if not m:
            continue
        key, value = m.group(1), m.group(2)
        markers.setdefault(key, []).append(value)

    has_test_block = 'test "' in path.read_text(encoding="utf-8")

    # 多文件用例(tests/modules/<case>/):类型由标记决定,源码文件跳过
    in_modules = "modules" in path.relative_to(ROOT).parts
    if in_modules:
        if not (path.parent.parent / "Ctron.toml").exists():
            errors.append("modules 用例缺少 Ctron.toml(项目根)")
        if markers.get("fail"):
            kind = "neg"
        elif markers.get("warn"):
            kind = "lint"
        elif markers.get("panic"):
            kind = "panic"
        elif has_test_block:
            kind = "behavior"
        else:
            return errors          # 普通源码文件(如 circular/src/b.ct)

    # 未知标记键
    for key in markers:
        if key not in {"fail", "msg", "warn", "panic", "target"}:
            errors.append(f"未知标记键: //@ {key}:")
    # msg 只配 fail
    if "msg" in markers and "fail" not in markers:
        errors.append("//@ msg: 只能与 //@ fail: 同用")
    # 错误码必须已注册
    for key in ("fail", "warn"):
        for code in markers.get(key, []):
            if not CODE_RE.match(code):
                errors.append(f"错误码格式非法: {code}")
            elif code not in ERROR_CODES:
                errors.append(f"错误码未注册(先加 README §4 与本脚本注册表): {code}")
    # target/profile 取值
    for key in ("target", "profile"):
        for value in markers.get(key, []):
            if value not in TARGETS:
                errors.append(f"{key} 取值非法: {value}(应为 {sorted(TARGETS)})")

    if kind == "neg":
        if not markers.get("fail"):
            errors.append("neg 测试必须列出至少一个 //@ fail: 错误码")
        for banned in ("warn", "panic"):
            if banned in markers:
                errors.append(f"neg 测试不得使用 //@ {banned}:")
    elif kind == "lint":
        if not markers.get("warn"):
            errors.append("lint 测试必须列出至少一个 //@ warn: 警告码")
        if "fail" in markers or "panic" in markers:
            errors.append("lint 测试不得 fail/panic(应编译通过)")
        if not has_test_block:
            errors.append("lint 测试须含至少一个 test 块(应可编译可运行)")
    elif kind == "panic":
        if not markers.get("panic"):
            errors.append("panic 测试必须有 //@ panic: 标记")
        if "fail" in markers or "warn" in markers:
            errors.append("panic 测试不得 fail/warn(应编译通过)")
        if not has_test_block:
            errors.append("panic 测试须含至少一个 test 块")
    else:  # behavior
        for banned in ("fail", "warn", "panic"):
            if banned in markers:
                errors.append(
                    f"行为测试不得使用 //@ {banned}:(请拆分为对应后缀的文件)"
                )
        if not has_test_block:
            errors.append("行为测试须含至少一个 test 块")

    return errors


def main() -> int:
    all_files = sorted(p for p in ROOT.rglob("*.ct") if p.is_file())
    stats: dict[str, int] = {}
    failures = 0

    for path in all_files:
        kind = kind_of(path) or "other"
        stats[kind] = stats.get(kind, 0) + 1
        rel = path.relative_to(ROOT)
        for err in check_file(path):
            failures += 1
            print(f"[FAIL] {rel}: {err}")

    print()
    print(f"测试文件: {len(all_files)} 个 " + " ".join(f"{k}={v}" for k, v in sorted(stats.items())))
    if failures:
        print(f"元检查失败: {failures} 处问题")
        return 1
    print("元检查通过:标记、命名约定与错误码引用全部一致")
    return 0


if __name__ == "__main__":
    sys.exit(main())
