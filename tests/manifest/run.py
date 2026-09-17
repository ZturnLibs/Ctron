#!/usr/bin/env python3
"""run.py —— CTCL 黄金语料 runner。

positive/:判定 E=0 且 fmt 幂等(fmt(fmt(x)) == fmt(x))。
negative/:文件头 `// expect-error: E5042, …` 标注期望诊断码序列(文件序,精确相等);
          同时断言文件**判定失败**(负例必须真的失败,防语料腐化)。
三线符合性:本 runner 为 Python 参考实现口径;C/Rust/自举适配后,ci.sh 以同一语料
对拍三线解析输出(规范 §9)。
"""
import os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import ctcl_check as C  # noqa: E402


def expected_codes(text):
    codes = []
    for line in text.split("\n"):
        s = line.strip()
        if s.startswith("// expect-error:"):
            codes.extend(c.strip() for c in s[len("// expect-error:"):].split(",") if c.strip())
    return codes


def main():
    base = os.path.dirname(os.path.abspath(__file__))
    bad = 0

    pos_dir = os.path.join(base, "positive")
    for name in sorted(os.listdir(pos_dir)):
        if not name.endswith(".ctcl"):
            continue
        path = os.path.join(pos_dir, name)
        text = open(path, encoding="utf-8").read()
        blocks, ds, tail = C.parse_manifest(text)
        ds = C.validate(blocks, ds)
        es = [d for d in ds if d["code"].startswith("E")]
        c1 = C.render(blocks, tail)
        b2, d2, t2 = C.parse_manifest(c1)
        idem = (not d2) and C.render(b2, t2) == c1
        ok = not es and idem
        print("[%s] positive/%s%s" % ("PASS" if ok else "FAIL", name,
              "" if idem else "  ← fmt 不幂等"))
        for d in ds:
            print("      %s L%d: %s" % (d["code"], d["line"], d["msg"]))
        if not ok:
            bad += 1

    neg_dir = os.path.join(base, "negative")
    for name in sorted(os.listdir(neg_dir)):
        if not name.endswith(".ctcl"):
            continue
        path = os.path.join(neg_dir, name)
        text = open(path, encoding="utf-8").read()
        want = expected_codes(text)
        blocks, ds, _ = C.parse_manifest(text)
        ds = C.validate(blocks, ds)
        got = [d["code"] for d in ds if d["code"].startswith("E")]
        ok = (got == want) and want
        print("[%s] negative/%s  got=%s want=%s" % ("PASS" if ok else "FAIL", name, got, want))
        for d in ds:
            print("      %s L%d: %s" % (d["code"], d["line"], d["msg"]))
        if not ok:
            bad += 1

    print("\n语料:%s" % ("ALL GREEN" if not bad else "%d 例失败" % bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
