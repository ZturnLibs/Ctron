#!/usr/bin/env python3
"""Ctron Rust 版全量测试战役:61 文件 × 三条路径(解释器/检查/原生)"""
import subprocess, sys, re, os
from pathlib import Path

ROOT = Path("/Users/zyj/ZturnLibs/Ctron")
CT = ROOT / "compiler-rust/target/debug/ctron"
TESTS = ROOT / "tests"
TMP = Path("/tmp/ctron_campaign")
os.makedirs(TMP, exist_ok=True)

files = sorted(p for p in TESTS.rglob("*.ct") if "modules" not in p.parts and "roadmap" not in p.parts)
rows = []
for f in files:
    name = f.name
    src = f.read_text()
    kind = ("neg" if name.endswith(".neg.ct") else
            "lint" if name.endswith(".lint.ct") else
            "panic" if name.endswith(".panic.ct") else "behavior")
    marker = None
    m = re.search(r"//@\s*(?:fail|panic):\s*(\S+)", src)
    if m: marker = m.group(1)
    w = re.search(r"//@\s*warn:\s*(\S+)", src)
    rows.append((f, name, kind, marker, (w.group(1) if w else None)))

def ctron(*args, timeout=30):
    try:
        r = subprocess.run([str(CT), *args], capture_output=True, text=True, timeout=timeout, cwd=str(ROOT))
        return r.returncode, r.stdout, r.stderr
    except subprocess.TimeoutExpired:
        return -99, "", "TIMEOUT"

ok = fail = 0
details = []
for f, name, kind, marker, warn in rows:
    # —— 解释器路径(behavior/panic/web) ——
    interp = "—"
    if kind in ("behavior", "panic"):
        rc, out, err = ctron("run", str(f))
        if kind == "behavior":
            interp = "PASS" if rc == 0 else f"FAIL({err.strip()[:60] or out.strip()[:60]})"
        else:
            # ctron run 尊重 //@ panic: 标记:预期 panic 输出 "panic-ok" 且 rc=0
            interp = "PASS" if "panic-ok" in out else f"FAIL(rc={rc})"
    elif kind == "neg":
        prof = ["--profile", "bare"] if name.startswith("08_bare") else []
        rc, out, err = ctron("check", str(f), *prof)
        codes = re.findall(r"[EW]\d{4}", err + out)
        interp = "PASS" if (marker in codes) else f"FAIL(want {marker}, got {codes or '无'})"
    elif kind == "lint":
        rc, out, err = ctron("check", str(f))
        codes = re.findall(r"[EW]\d{4}", err + out)
        interp = "PASS" if (warn in codes) else f"FAIL(want {warn}, got {codes or '无'})"

    # —— 原生路径(behavior/panic;neg/lint 是编译期用例不适用) ——
    native = "—"
    if kind in ("behavior", "panic") and not name.startswith("10_web"):
        stem = name.replace(".", "_")
        c_path = TMP / f"{stem}.c"
        rc, out, err = ctron("trans", str(f), "-o", str(c_path))
        if rc == 0:
            cc = subprocess.run(["cc", "-O1", "-w", "-std=gnu11", str(c_path), "-o", str(TMP / stem)],
                                capture_output=True, text=True)
            if cc.returncode != 0:
                native = "CC-FAIL"
            else:
                try:
                    run = subprocess.run([str(TMP / stem)], capture_output=True, text=True, timeout=20)
                    if kind == "behavior":
                        native = "PASS" if run.returncode == 0 else f"FAIL({run.stderr.strip()[:50]})"
                    else:
                        hit = marker and (marker in run.stderr)
                        native = "PASS" if (run.returncode != 0 and hit) else f"FAIL(rc={run.returncode})"
                except subprocess.TimeoutExpired:
                    native = "TIMEOUT"
        else:
            native = f"TRANS-FAIL({err.strip()[:60]})"

    good = (interp in ("PASS", "—")) and (native in ("PASS", "—"))
    ok, fail = (ok + 1, fail) if good else (ok, fail + 1)
    mark = "✓" if good else "✗"
    rows_out = f"{mark} {name:34} interp={interp:20} native={native}"
    print(rows_out)
    if not good:
        details.append(rows_out)

print(f"\n=== 结果:{ok} 通过 / {fail} 失败 / 共 {len(rows)} ===")
sys.exit(1 if fail else 0)
