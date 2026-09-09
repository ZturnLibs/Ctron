#!/usr/bin/env python3
"""suite.py —— 用 tests/ 一致性测试集验证编译出的 Ctron 编译器(bin/ctron-cc)。

标记语义(tests/README.md §1/§2):
  *.ct        行为:编译干净 + test 块全过(rc=0)
  *.neg.ct    负例:编译失败,诊断含全部 //@ fail: 码(有 //@ msg: 时须含其子串)
  *.lint.ct   lint:诊断含全部 //@ warn: 码
  *.panic.ct  编译干净,运行期失败且输出含 //@ panic: 子串
跳过:roadmap/(红=规范锚)、modules/(多文件包)、//@ target: 非 full。
对照:C 参考宿主(compiler-c/build/ctronc)同口径跑一遍,输出双方记分与分歧清单。
"""
import re, subprocess, sys, glob, os

MARKER_RE = re.compile(r"^//@\s*(\w+)\s*:\s*(.+?)\s*$")
CATS = [("behavior", "*.ct"), ("neg", "*.neg.ct"), ("lint", "*.lint.ct"), ("panic", "*.panic.ct")]

def markers(path):
    mk = {}
    for line in open(path, encoding="utf-8"):
        m = MARKER_RE.match(line.strip())
        if m:
            mk.setdefault(m.group(1), []).append(m.group(2))
    return mk

def run(binpath, path, sub):
    try:
        p = subprocess.run([binpath, sub, path], capture_output=True, text=True, timeout=20, cwd=ROOT)
        return p.returncode, p.stdout + p.stderr
    except subprocess.TimeoutExpired:
        return None, "TIMEOUT"

def host_probe(path, kind, mk):
    """C 宿主双阶段协议:check(编译)先行,neg/lint 只看 check,behavior/panic 再 test。"""
    rc, out = run(HOST, path, "check")
    if kind == "neg":
        return rc, out
    if kind == "lint":
        return 0, out if all(c in out for c in mk.get("warn", [])) else out
    if rc != 0:
        return rc, out
    return run(HOST, path, "test")

def verdict(kind, mk, rc, out):
    """返回 (通过?, 原因)"""
    if rc is None:
        return False, "超时"
    if kind == "behavior":
        return (rc == 0), ("" if rc == 0 else f"rc={rc}: {first_line(out)}")
    if kind == "neg":
        if rc == 0:
            return False, "未拦截(编译通过)"
        missing = [c for c in mk.get("fail", []) if c not in out]
        if missing:
            return False, "缺诊断码 " + ",".join(missing)
        for msg in mk.get("msg", []):
            if msg not in out:
                return False, f"消息缺子串「{msg}」"
        return True, ""
    if kind == "lint":
        missing = [c for c in mk.get("warn", []) if c not in out]
        if missing:
            return False, "缺警告 " + ",".join(missing) + ("" if rc else "(且文件通过)")
        return True, ""
    if kind == "panic":
        diag = re.search(r"^[EW]\d{4}:", out, re.M)
        if diag:
            return False, f"编译期拦截({diag.group(0)})预期运行期"
        if rc is not None and rc != 0 and any(s in out for s in mk.get("panic", [])):
            return True, ""
        return False, f"rc={rc} 输出无 panic 子串: {first_line(out)}"
    return False, "?"

def first_line(s):
    s = s.strip().replace("\n", " ⏎ ")
    return (s[:90] + "…") if len(s) > 90 else s

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CC = os.environ.get("CTRON_CC", os.path.join(ROOT, "compiler/bin/ctron-cc"))
HOST = os.path.join(ROOT, "compiler-c/build/ctronc")
TESTS = os.path.join(ROOT, "tests")

if not os.path.exists(CC):
    sys.exit("suite: 缺少 bin/ctron-cc(先: compiler/native.sh)")

skip = {"target": 0}
score = {k: [0, 0] for k, _ in CATS}          # kind -> [cc_pass, total]
hscore = {k: [0, 0] for k, _ in CATS}         # kind -> [host_pass, total]
gaps, disagree, bothfail = [], [], []

for kind, pat in CATS:
    for path in sorted(glob.glob(os.path.join(TESTS, pat))):
        base = os.path.basename(path)
        if kind == "behavior" and re.search(r"\.(neg|lint|panic)\.ct$", base):
            continue
        mk = markers(path)
        if mk.get("target") and mk["target"][0] != "full":
            skip["target"] += 1
            continue
        rc, out = run(CC, path, "run")
        ok, why = verdict(kind, mk, rc, out)
        score[kind][1] += 1
        if ok:
            score[kind][0] += 1
        hrc, hout = host_probe(path, kind, mk)
        hok, _ = verdict(kind, mk, hrc, hout)
        hscore[kind][1] += 1
        if hok:
            hscore[kind][0] += 1
        tag = f"[{kind}] {base}"
        if ok and hok:
            continue
        if ok and not hok:
            disagree.append(f"{tag} — 自举 cc 通过而宿主未过(宿主: {first_line(hout)})")
        elif not ok and hok:
            gaps.append(f"{tag} — {why}")
        else:
            bothfail.append(f"{tag} — cc: {why} | 宿主: {first_line(hout)}")

# ---------- modules/(多文件包,tests/README §6)----------
# 入口:src/main.ct;无 main.ct 时取带 //@ 标记的文件。
# 自举侧:bin/ctron-cc run <entry>(加载器常开:use 解析/可见性/合并/循环检测)。
# 宿主侧:ctronc pkg <dir>(包检查 oracle,仅 check 面;宿主无包运行口径,行为用例不对齐运行)。
print(f"\n== modules/ 多文件包(§6)==")
mpass, hpass, mnotes = [0, 0], [0, 0], []
for case in sorted(glob.glob(os.path.join(TESTS, "modules", "*"))):
    if not os.path.isdir(case):
        continue
    src = os.path.join(case, "src")
    entry = os.path.join(src, "main.ct")
    if not os.path.exists(entry):
        marked = [f for f in sorted(glob.glob(os.path.join(src, "*.ct"))) if markers(f)]
        if not marked:
            mnotes.append(f"{os.path.basename(case)}: 无入口且无标记文件(跳过)")
            continue
        entry = marked[0]
    mk = markers(entry)
    kind = "neg" if mk.get("fail") else ("lint" if mk.get("warn") else ("panic" if mk.get("panic") else "behavior"))
    name = os.path.basename(case)
    mpass[1] += 1
    hpass[1] += 1
    rc, out = run(CC, entry, "run")
    ok, why = verdict(kind, mk, rc, out)
    if ok:
        mpass[0] += 1
    else:
        mnotes.append(f"[{kind}] {name} — 自举: {why}")
    hrc, hout = run(HOST, case, "pkg")
    if kind == "behavior":
        hok = hrc == 0
    else:
        hok = hrc != 0 and all(c in hout for c in mk.get("fail", []))
    if hok:
        hpass[0] += 1
    else:
        mnotes.append(f"[{kind}] {name} — 宿主pkg: rc={hrc} {first_line(hout)}")
print(f"自举 {mpass[0]}/{mpass[1]} | 宿主pkg {hpass[0]}/{hpass[1]}")
for n in mnotes:
    print("  " + n)

total = sum(v[1] for v in score.values())
print(f"== tests/ 一致性测试集 × bin/ctron-cc(对照 C 参考宿主)==")
print(f"{'类别':<10}{'自举cc':<12}{'C 宿主':<12}")
for kind, _ in CATS:
    c, h = score[kind], hscore[kind]
    print(f"{kind:<10}{c[0]}/{c[1]:<11}{h[0]}/{h[1]:<11}")
print(f"{'合计':<10}{sum(v[0] for v in score.values())}/{total:<11}{sum(v[0] for v in hscore.values())}/{total}")
print(f"(跳过: roadmap/ 全目录=规范锚、modules/ 見独立小节;target 非 full {skip['target']} 件)")
if gaps:
    print(f"\n-- 自举 cc 未过而宿主通过(能力缺口,{len(gaps)})--")
    for g in gaps: print("  " + g)
if disagree:
    print(f"\n-- 分歧:cc 通过而宿主未过({len(disagree)})--")
    for d in disagree: print("  " + d)
if bothfail:
    print(f"\n-- 双方均未过(语言面共性缺口,{len(bothfail)})--")
    for b in bothfail: print("  " + b)
