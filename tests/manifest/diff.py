#!/usr/bin/env python3
"""diff.py —— CTCL 三线黄金对拍(Python / C / Rust)。

语料:tests/manifest/positive/*.ctcl + negative 中期望码全部落在三线作用域
(E5042/E5043/E5044/E5045/E5047/E5049/E5050)的用例 + 仓库全部 Ctron.ctcl。

对拍维度:
  1. 诊断码序列(文件序,逐字节一致)
  2. 语义字段(name/version/caps/budget;caps 按集合、缺省=空)
  3. 规范形态(Python vs Rust;两侧均过滤注释行——注释所有权渲染待 L2 移植)

二进制缺失时对应臂 SKIP(诚实降级,不算失败);spec §9。
"""
import os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import ctcl_check as C  # noqa: E402

SCOPED = {"E5042", "E5043", "E5044", "E5045", "E5047", "E5049", "E5050"}
CBIN = os.path.join(ROOT, "compiler-c", "build", "ctronc")
RBIN_CANDIDATES = [
    os.path.join(ROOT, "compiler-rust", "target", "debug", "ctron"),
    os.path.join(ROOT, "compiler-rust", "target", "release", "ctron"),
]


def expected_codes(text):
    codes = []
    for line in text.split("\n"):
        s = line.strip()
        if s.startswith("// expect-error:"):
            codes.extend(c.strip() for c in s[len("// expect-error:"):].split(",") if c.strip())
    return codes


def python_arm(text):
    blocks, ds, tail = C.parse_manifest(text)
    ds = C.validate(blocks, ds)
    codes = [f'{d["code"]} {d["msg"]}' for d in ds if d["code"].startswith("E")]
    fields = {}
    for b in blocks:
        if b["name"] == "pkg":
            for f in b["fields"]:
                if f["k"] == "name" and isinstance(f["v"], str):
                    fields["name"] = f["v"]
                elif f["k"] == "version" and isinstance(f["v"], str):
                    fields["version"] = f["v"]
                elif f["k"] == "caps" and isinstance(f["v"], list):
                    members = C.SCHEMA["pkg"]["keys"]["caps"].get("members", [])
                    kept = [x for x in f["v"] if x in members]  # 仅被接受成员计入语义字段(镜像 C/Rust 存储)
                    if kept:
                        fields["caps"] = ",".join(sorted(kept))
        elif b["name"] == "comptime":
            for f in b["fields"]:
                if f["k"] == "budget_ms" and isinstance(f["v"], int):
                    fields["budget"] = str(f["v"])
    canon = C.render(blocks, tail)
    return codes, fields, canon


def parse_cli(out):
    parts = out.split("---")
    codes = [l.strip() for l in parts[0].split("\n") if l.strip()]
    fields = {}
    if len(parts) > 1:
        for line in parts[1].split("\n"):
            if ":" in line:
                k, v = line.split(":", 1)
                if k.strip():
                    fields[k.strip()] = v.strip()
    canon = parts[2] if len(parts) > 2 else None
    if canon is not None:
        canon = canon.replace("\n", "", 1)  # 仅剥协议分隔符的一个换行,保留规范形态自身的前导空行
    return codes, fields, canon


def run_arm(label, cmd, path):
    r = subprocess.run(cmd + [path], capture_output=True, text=True)
    codes, fields, canon = parse_cli(r.stdout)
    return codes, fields, canon


def norm(fields):
    d = dict(fields)
    d["caps"] = tuple(sorted(d["caps"].split(","))) if "caps" in d else ()
    return d


def main():
    cbin = CBIN if os.path.exists(CBIN) else None
    rbin = next((p for p in RBIN_CANDIDATES if os.path.exists(p)), None)
    if not cbin:
        print("SKIP:C 宿主未构建(make -C compiler-c)")
    if not rbin:
        print("SKIP:Rust 二进制未构建(cargo build)")

    mdir = os.path.join(ROOT, "tests", "manifest")
    files = []
    for n in sorted(os.listdir(os.path.join(mdir, "positive"))):
        files.append(("positive/" + n, os.path.join(mdir, "positive", n)))
    for n in sorted(os.listdir(os.path.join(mdir, "negative"))):
        p = os.path.join(mdir, "negative", n)
        files.append(("negative/" + n, p))
    for root, dirs, names in os.walk(ROOT):
        dirs[:] = [d for d in dirs if d not in (".git", "target", "__pycache__", "manifest")]
        for n in sorted(names):
            if n == "Ctron.ctcl":
                files.append((os.path.relpath(os.path.join(root, n), ROOT),
                              os.path.join(root, n)))

    # 第四线语料同步:扁平拷贝 → tests/manifest/all(自举线 ctcl_chk 扫描该目录)
    import shutil
    all_dir = os.path.join(mdir, "all")
    if os.path.exists(all_dir):
        shutil.rmtree(all_dir)
    os.makedirs(all_dir)
    flat_of = {}
    for label, path in files:
        flat = label.replace("/", "__")
        shutil.copy(path, os.path.join(all_dir, flat))
        flat_of[label] = flat

    sh_sections = {}
    if cbin:
        sh = subprocess.run([cbin, "run", "../selfhosted/ctcl_chk.ct"],
                            cwd=os.path.join(ROOT, "compiler-c"), capture_output=True, text=True)
        cur = None
        for line in sh.stdout.split("\n"):
            if line.startswith("== "):
                cur = line[3:].strip()
                sh_sections[cur] = []
            elif cur is not None and line.strip():
                sh_sections[cur].append(line.strip())

    bad = skip = 0
    total = 0
    for label, path in files:
        text = open(path, encoding="utf-8").read()
        total += 1
        pc, pf, pcanon = python_arm(text)
        arms = {"py": (pc, norm(pf), pcanon)}
        if cbin:
            cc, cf, _ = run_arm("c", [cbin, "manifest"], path)
            arms["c"] = (cc, norm(cf), None)
        if rbin:
            rc, rf, rcanon = run_arm("r", [rbin, "manifest"], path)
            arms["r"] = (rc, norm(rf), rcanon)
        flat = flat_of[label]
        if cbin and flat in sh_sections:
            arms["sh"] = (sh_sections[flat], None, None)
        errs = []
        base = arms["py"]
        for a, (codes, fields, canon) in arms.items():
            if codes != base[0]:
                errs.append("%s码序列 %s≠%s" % (a, codes, base[0]))
            if fields is not None and base[1] is not None and fields != base[1]:
                errs.append("%s字段 %s≠%s" % (a, fields, base[1]))
        if rbin and arms["r"][2] != base[2]:
            errs.append("r规范形态 ≠ py规范形态")
        status = "PASS" if not errs else "FAIL"
        if errs:
            bad += 1
        print("[%s] %s%s" % (status, label, ("  " + "; ".join(errs)) if errs else ""))

    if os.path.exists(all_dir):
        shutil.rmtree(all_dir)
    print("\n四线对拍:%d 例(全量 E 码 + 消息全文)%s" %
          (total, "ALL GREEN" if not bad else "%d 例失败" % bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
