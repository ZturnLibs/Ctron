#!/usr/bin/env python3
"""bench.py —— C 版 vs Rust 版性能对比框架(2026-09-07 基准的固化)。

四段基准:
  1) 解释器端到端:tools/bench/*.ct(test 块形式,双方 CLI 均可运行),
     批量调用计时(目标 ~1.5s/批,5 批取中位)。noop 档给出进程启动基线。
  2) 编译器前端:tests/*.ct 批量 parse/check + selfhosted/sem_chk.ct 单大文件。
  3) 转译发射:34 语料批量 trans(注意:进程启动主导,趋势参考)。
  4) 原生路径:tools/bench/nfib.ct、narr.ct 双后端 trans → cc -O2 → 执行
     (exit 0 一致性门控后计时)。

一致性门控:所有计时前先验证双方对同一程序 exit 0(Rust run 仅执行 test 块,
故基准统一为 test 块形式;见 docs/c-rust-divergences.md 差异 D1)。

用法:
  python3 tools/bench.py            # 全量
  python3 tools/bench.py --quick    # 减轮次(冒烟)

前置: compiler-c/build/ctronc (make -C compiler-c)
      compiler-rust/target/release/ctron (cargo build --release --manifest-path compiler-rust/Cargo.toml)
      cc (原生段)
"""
import glob
import os
import statistics
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
C = os.path.join(REPO, "compiler-c/build/ctronc")
R = os.path.join(REPO, "compiler-rust/target/release/ctron")
BENCH = os.path.join(REPO, "tools/bench")
BIG = os.path.join(REPO, "selfhosted/sem_chk.ct")
QUICK = "--quick" in sys.argv
ROUNDS = 3 if QUICK else 5
NATIVE_RUNS = 5 if QUICK else 11


def sh(cmd, **kw):
    return subprocess.run(cmd, **kw)


def batch_ms(cmd, n):
    t0 = time.perf_counter()
    for _ in range(n):
        sh(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return (time.perf_counter() - t0) / n * 1000.0


def gate(path):
    """一致性门控:双方对同一 test 块程序 exit 0。"""
    rc = sh([C, "test", path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode
    rr = sh([R, "run", path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode
    return rc == 0 and rr == 0


def interp_bench(name):
    path = os.path.join(BENCH, f"{name}.ct")
    if not gate(path):
        print(f"{name:12s}  门控失败(两侧输出/退出码不一致)——跳过")
        return
    t1 = batch_ms([C, "test", path], 3)
    t2 = batch_ms([R, "run", path], 3)
    n1 = max(3, min(400, int(1500 / max(t1, 0.05))))
    n2 = max(3, min(400, int(1500 / max(t2, 0.05))))
    c = statistics.median([batch_ms([C, "test", path], n1) for _ in range(ROUNDS)])
    r = statistics.median([batch_ms([R, "run", path], n2) for _ in range(ROUNDS)])
    tag = "C 快 %.1fx" % (r / c) if r > c else ("Rust 快 %.1fx" % (c / r) if c > r else "持平")
    print(f"{name:12s}  C: {c:8.3f} ms/次   Rust: {r:8.3f} ms/次   {tag}")


def stage_batch(cmdfn, files, label, impl):
    ts = []
    for _ in range(ROUNDS):
        t0 = time.perf_counter()
        for f in files:
            sh(cmdfn(f), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        ts.append((time.perf_counter() - t0) * 1000)
    m = statistics.median(ts)
    print(f"  {label:6s}[{impl:4s}] 中位 {m:8.1f} ms/轮 ({len(files)} 文件, {m/len(files):6.2f} ms/文件)")


def big_file(label, cmd, n=None):
    n = n or (7 if QUICK else 15)
    ts = []
    for _ in range(n):
        t0 = time.perf_counter()
        sh(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        ts.append((time.perf_counter() - t0) * 1000)
    print(f"  {label:12s} 中位 {statistics.median(ts):7.2f} ms")


def native_bench(name):
    path = os.path.join(BENCH, f"{name}.ct")
    cc_path, rc_path = f"/tmp/ctb_{name}_c.c", f"/tmp/ctb_{name}_r.c"
    cbin, rbin = f"/tmp/ctb_{name}_c", f"/tmp/ctb_{name}_r"
    ok = True
    for binp, out in [(C, cc_path), (R, rc_path)]:
        if sh([binp, "trans", path, "-o", out], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode != 0:
            ok = False
    if ok:
        for src, out in [(cc_path, cbin), (rc_path, rbin)]:
            if sh(["cc", "-O2", "-w", "-std=c11", src, "-o", out], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode != 0:
                ok = False
    if not ok:
        print(f"{name:12s}  构建失败——跳过")
        return
    e1 = sh([cbin], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode
    e2 = sh([rbin], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode
    if e1 != 0 or e2 != 0:
        print(f"{name:12s}  门控失败(exit {e1}/{e2})——跳过")
        return
    ts = []
    for b in (cbin, rbin):
        vals = []
        for _ in range(NATIVE_RUNS):
            t0 = time.perf_counter()
            sh([b], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            vals.append((time.perf_counter() - t0) * 1000)
        ts.append(statistics.median(vals))
    tag = "C 快 %.2fx" % (ts[1] / ts[0]) if ts[1] > ts[0] else "Rust 快 %.2fx" % (ts[0] / ts[1])
    print(f"{name:12s}  C后端: {ts[0]:7.2f} ms   Rust后端: {ts[1]:7.2f} ms   {tag}")


def main():
    for b, hint in [(C, "make -C compiler-c"), (R, "cargo build --release --manifest-path compiler-rust/Cargo.toml")]:
        if not os.access(b, os.X_OK):
            print(f"缺少 {b}(先: {hint})")
            return 2
    print(f"== 解释器端到端(test 块;含进程启动;{ROUNDS} 批中位) ==")
    for b in ["bench_noop", "bench_fib", "bench_str", "bench_arr", "bench_call"]:
        interp_bench(b)

    print(f"\n== 编译器前端批量(tests/*.ct;{ROUNDS} 轮中位;进程启动主导,趋势参考) ==")
    files = sorted(glob.glob(os.path.join(REPO, "tests/*.ct")))
    for impl, binp in [("C", C), ("Rust", R)]:
        print(f"-- {impl} --")
        stage_batch(lambda f: [binp, "parse", f], files, "parse", impl)
        stage_batch(lambda f: [binp, "check", f], files, "check", impl)

    print(f"\n== 单大文件前端(sem_chk.ct 4086 行) ==")
    print("-- C --")
    big_file("parse", [C, "parse", BIG])
    big_file("check", [C, "check", BIG])
    print("-- Rust --")
    big_file("parse", [R, "parse", BIG])
    big_file("check", [R, "check", BIG])

    print(f"\n== 原生路径(trans → cc -O2 → 执行;{NATIVE_RUNS} 次中位) ==")
    for b in ["nfib", "narr"]:
        native_bench(b)

    print("\n注: 解释器负载受 Rust 2M 表达式步上限约束(基准已按 ~50 万步 sizing);")
    print("    字符串基准为 O(n²) 拼接模式(规格惯例是插值,故意选它暴露分配器差异);")
    print("    语义/能力差异清单见 docs/c-rust-divergences.md。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
