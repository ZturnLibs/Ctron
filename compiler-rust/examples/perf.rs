//! R 线性能基线 harness(零依赖,手写计时)。
//! 运行:cargo run --release --example perf [-- quick]
//! 设计口径:docs/superpowers/plans/2026-09-07-r-perf-baseline.md
//! 五级流水(lex/parse/check/trans/interp)× 三类规模(冻结语料/合成单文件/多文件包)
//! + 解释执行微基准(fib/循环)+ build 端到端 + 峰值 RSS。

use std::path::PathBuf;
use std::time::Instant;

fn median_ms<F: FnMut()>(iters: usize, mut f: F) -> f64 {
    f(); // 预热
    let mut v = Vec::with_capacity(iters);
    for _ in 0..iters {
        let t = Instant::now();
        f();
        v.push(t.elapsed().as_secs_f64() * 1000.0);
    }
    v.sort_by(|a, b| a.partial_cmp(b).unwrap());
    v[v.len() / 2]
}

#[derive(Clone)]
struct Row {
    name: String,
    bytes: usize,
    lex: f64,
    parse: f64,
    check: f64,
    trans: f64,
    interp: Option<f64>,
}

fn corpus_rows(root: &PathBuf) -> Vec<(String, String)> {
    let mut out = Vec::new();
    let mut rd = std::fs::read_dir(root).expect("tests/ 根目录");
    let mut files: Vec<PathBuf> = rd
        .by_ref()
        .flatten()
        .map(|e| e.path())
        .filter(|p| p.is_file() && p.extension().is_some_and(|x| x == "ct"))
        .collect();
    files.sort();
    for f in files {
        let src = std::fs::read_to_string(&f).unwrap();
        out.push((f.file_name().unwrap().to_string_lossy().to_string(), src));
    }
    out
}

fn section_corpus(root: &PathBuf) {
    println!("## A. 冻结语料逐文件(release,中位数 ms)");
    println!();
    println!("| 文件 | KB | lex | parse | check | trans | interp |");
    println!("|---|---|---|---|---|---|---|");
    let files = corpus_rows(root);
    let mut rows: Vec<Row> = Vec::new();
    for (name, src) in &files {
        let bytes = src.len();
        let lex = median_ms(15, || {
            let _ = ctron::lex(src);
        });
        let parse = median_ms(15, || {
            let (toks, _) = ctron::lex(src);
            let _ = ctron::parser::Parser::new(toks).parse_file_public();
        });
        let check = median_ms(15, || {
            let _ = ctron::check_src(src, ctron::sem::Profile::Full);
        });
        let (ast, pdiags) = ctron::parse_src(src);
        let trans = if pdiags.is_empty() {
            median_ms(15, || {
                let _ = ctron::trans::Trans::new().trans_file(&ast);
            })
        } else { f64::NAN };
        let interp = if name.ends_with(".neg.ct") || name.ends_with(".lint.ct") {
            None
        } else {
            Some(median_ms(9, || {
                let _ = ctron::interp::run_test_file(src, ctron::sem::Profile::Full);
            }))
        };
        let fmt = |v: f64| if v.is_nan() { "—".to_string() } else { format!("{v:.2}") };
        println!(
            "| {name} | {:.1} | {} | {} | {} | {} | {} |",
            bytes as f64 / 1024.0,
            fmt(lex), fmt(parse), fmt(check), fmt(trans),
            interp.map(|v| format!("{v:.2}")).unwrap_or_else(|| "—".into()),
        );
        rows.push(Row { name: name.clone(), bytes, lex, parse, check, trans, interp });
    }

    // 聚合吞吐(排除 trans NaN)
    let ok: Vec<&Row> = rows.iter().filter(|r| !r.trans.is_nan()).collect();
    let sum = |f: &dyn Fn(&Row) -> f64| -> f64 { ok.iter().map(|r| f(r)).sum() };
    let kb: f64 = ok.iter().map(|r| r.bytes as f64 / 1024.0).sum();
    println!();
    println!(
        "聚合({} 个可转译文件,共 {:.0} KB):lex {:.1} MB/s | parse {:.1} MB/s | check {:.1} MB/s | trans {:.1} MB/s",
        ok.len(), kb,
        (kb / 1024.0) / (sum(&|r| r.lex) / 1000.0),
        (kb / 1024.0) / (sum(&|r| r.parse) / 1000.0),
        (kb / 1024.0) / (sum(&|r| r.check) / 1000.0),
        (kb / 1024.0) / (sum(&|r| r.trans) / 1000.0),
    );
    let mut by_check = rows.clone();
    by_check.sort_by(|a, b| b.check.partial_cmp(&a.check).unwrap());
    println!("check 最慢 Top5:{}", by_check.iter().take(5)
        .map(|r| format!("{}({:.1}ms)", r.name, r.check)).collect::<Vec<_>>().join(", "));
    println!();
}

// ---------- 合成规模 ----------

fn synth_src(n_fns: usize) -> String {
    let mut s = String::new();
    s.push_str("// 合成基准文件(自动生成,零诊断校验由 harness 断言)\n");
    s.push_str("enum Op {\n    Add\n    Sub\n}\n\nstruct Pt {\n    var x: I32\n    var y: I32\n}\n\n");
    s.push_str("fn pick[T](a: T, b: T, c: Bool) -> T {\n    if c { return a }\n    return b\n}\n\n");
    s.push_str("fn apply1(f: fn(I32) -> I32, x: I32) -> I32 {\n    return f(x)\n}\n\n");
    s.push_str("fn add1(x: I32) -> I32 {\n    return x + 1\n}\n\n");
    for i in 0..n_fns {
        if i % 10 == 9 {
            s.push_str(&format!(
                "fn f_{i}(o: Op) -> I32 {{\n    let v = match o {{\n        Add => 10\n        Sub => 20\n    }}\n    return v.add1()\n}}\n\n"
            ));
        } else {
            s.push_str(&format!(
"fn f_{i}(a: I32, b: I32) -> I32 {{\n    var acc: I32 = 0\n    for k in 0..8 {{\n        if k % 2 == 0 {{\n            acc += a * k + b\n        }} else {{\n            acc -= b - k\n        }}\n    }}\n    let s = \"iter {{acc}} at {{a}}\"\n    let n = s.len\n    match n % 3 {{\n        0 => {{ acc += n }}\n        _ => {{ acc *= 2 }}\n    }}\n    let p = Pt {{ x: acc, y: n }}\n    acc += p.x + p.y\n    acc += apply1(|x| x * 2, acc)\n    return acc\n}}\n\n"
            ));
        }
    }
    s
}

fn section_scale(quick: bool) {
    println!("## B. 合成单文件规模(release,中位数 ms;行吞吐 = 行数/秒)");
    println!();
    println!("| 行数 | KB | lex | parse | check | check 行/s | trans | trans 行/s |");
    println!("|---|---|---|---|---|---|---|---|");
    let sizes: Vec<usize> = if quick { vec![1_000, 5_000] } else { vec![1_000, 5_000, 10_000, 50_000] };
    let tmp = std::env::temp_dir();
    for lines in sizes {
        let n_fns = lines / 22;
        let src = synth_src(n_fns);
        let actual_lines = src.lines().count();
        let path = tmp.join(format!("ctron_synth_{lines}.ct"));
        std::fs::write(&path, &src).unwrap();
        let iters = if lines >= 50_000 { 3 } else { 5 };

        let lex = median_ms(iters, || { let _ = ctron::lex(&src); });
        let parse = median_ms(iters, || {
            let (toks, _) = ctron::lex(&src);
            let _ = ctron::parser::Parser::new(toks).parse_file_public();
        });
        let check = median_ms(iters, || {
            let d = ctron::check_src(&src, ctron::sem::Profile::Full);
            assert!(d.is_empty(), "合成文件必须零诊断: {:?}", d.first().map(|x| x.code));
        });
        let (ast, pd) = ctron::parse_src(&src);
        assert!(pd.is_empty());
        let trans = median_ms(iters, || {
            let _ = ctron::trans::Trans::new().trans_file(&ast).expect("trans ok");
        });
        let kb = src.len() as f64 / 1024.0;
        println!(
            "| {actual_lines} | {kb:.0} | {lex:.1} | {parse:.1} | {check:.1} | {:.0} | {trans:.1} | {:.0} |",
            actual_lines as f64 / (check / 1000.0),
            actual_lines as f64 / (trans / 1000.0),
        );
        let _ = std::fs::remove_file(&path);
    }
    println!();
}

fn section_package() {
    println!("## C. 多文件包规模(100 模块链式依赖,check_package)");
    let n = 100;
    let mut files: Vec<(String, String)> = Vec::new();
    for i in 0..n {
        let src = if i == 0 {
            "pub fn g(x: I32) -> I32 {\n    return x + 1\n}\n".to_string()
        } else {
            format!(
                "use perfchain.m_{}.g\n\npub fn g(x: I32) -> I32 {{\n    return g(x) + 1\n}}\n",
                i - 1
            )
        };
        files.push((format!("perfchain.m_{i}"), src));
    }
    let manifest_ctcl = "pkg {\n    manifest_version = 1\n    name = \"perfchain\"\n    version = \"0.1.0\"\n}\n";
    let ms = median_ms(5, || {
        let (manifest, _) = ctron::check::parse_manifest(manifest_ctcl);
        let d = ctron::check_package(&files, Some(manifest), ctron::sem::Profile::Full);
        let n_diags: usize = d.iter().map(|(_, v)| v.len()).sum();
        assert!(n_diags == 0, "合成包必须零诊断");
    });
    println!("100 模块(每模块 1 个 pub fn,链式 use)check_package 中位数:{ms:.1} ms");
    println!();
}

fn section_interp(root: &PathBuf) {
    println!("## D. 解释执行微基准(release;树遍历解释器支配成本)");
    let fib = "fn fib(n: I32) -> I32 {\n    if n < 2 { return n }\n    return fib(n - 1) + fib(n - 2)\n}\n\ntest \"fib25\" {\n    assert_eq(fib(25), 75025)\n}\n";
    let loop1m = "test \"loop1m\" {\n    var acc: I64 = 0\n    var i: I32 = 0\n    while i < 1_000_000 {\n        acc += i\n        i += 1\n    }\n    assert_eq(acc, 499999500000)\n}\n";
    for (name, src) in [("递归 fib(25)(~24 万次调用)", fib), ("100 万次 while 循环(每次 2 语句)", loop1m)] {
        let ms = median_ms(3, || {
            let r = ctron::interp::run_test_file(src, ctron::sem::Profile::Full);
            assert!(r.iter().all(|(_, x)| x.is_ok()), "{name} 运行失败");
        });
        println!("{name}:{ms:.0} ms");
    }
    // 真实语料的解释执行总量
    let files = corpus_rows(root);
    let mut total = 0.0f64;
    let mut n = 0;
    for (name, src) in &files {
        if name.ends_with(".neg.ct") || name.ends_with(".lint.ct") { continue }
        let ms = median_ms(5, || {
            let _ = ctron::interp::run_test_file(src, ctron::sem::Profile::Full);
        });
        total += ms;
        n += 1;
    }
    println!("冻结语料全部行为/panic 文件解释执行合计({n} 文件):{total:.0} ms");
    println!();
}

fn section_e2e(root: &PathBuf, quick: bool) {
    println!("## E. build 端到端与峰值内存(CLI 子进程)");
    let manifest = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let rel_bin = manifest.join("target/release/ctron");
    let dbg_bin = manifest.join("target/debug/ctron");
    // 预算口径(§8.2):10 万行全量 debug 构建 < 5s —— 用 10k/45k 合成实测外推
    let mut cases: Vec<(&str, PathBuf)> = vec![
        ("06f_parallel.ct(并发,真实语料)", root.join("06f_parallel.ct")),
        ("合成 1k 行", std::env::temp_dir().join("ctron_synth_e2e_1k.ct")),
    ];
    let synth1k = synth_src(1_000 / 22);
    std::fs::write(std::env::temp_dir().join("ctron_synth_e2e_1k.ct"), &synth1k).unwrap();
    if !quick {
        for lines in [10_000usize, 45_000] {
            let p = std::env::temp_dir().join(format!("ctron_synth_e2e_{lines}.ct"));
            std::fs::write(&p, synth_src(lines / 22)).unwrap();
            cases.push((Box::leak(format!("合成 {lines} 行").into_boxed_str()), p));
        }
    }
    for (name, f) in &cases {
        let out = std::env::temp_dir().join("ctron_perf_e2e_bin");
        let t = Instant::now();
        let r = std::process::Command::new(&rel_bin)
            .args(["build", f.to_str().unwrap(), "-o", out.to_str().unwrap()])
            .output();
        let dt = t.elapsed().as_secs_f64() * 1000.0;
        match r {
            Ok(o) if o.status.success() => {
                let sz = std::fs::metadata(&out).map(|m| m.len()).unwrap_or(0);
                println!("{name}: build 总耗时 {dt:.0} ms,产物 {} KB", sz / 1024);
            }
            Ok(o) => println!("{name}: build 失败 rc={:?} {}", o.status.code(), String::from_utf8_lossy(&o.stderr)),
            Err(e) => println!("{name}: 启动失败 {e}"),
        }
    }
    // trans 与 cc 分解(合成 1k)
    let c_path = std::env::temp_dir().join("ctron_perf_e2e.c");
    let t = Instant::now();
    let _ = std::process::Command::new(&rel_bin)
        .args(["trans", std::env::temp_dir().join("ctron_synth_e2e_1k.ct").to_str().unwrap(), "-o", c_path.to_str().unwrap()])
        .output().unwrap();
    let trans_ms = t.elapsed().as_secs_f64() * 1000.0;
    let t = Instant::now();
    let cc = std::process::Command::new("cc")
        .args(["-O0", "-w", "-std=gnu11", c_path.to_str().unwrap(), "-o", std::env::temp_dir().join("ctron_perf_cc_bin").to_str().unwrap()])
        .output().unwrap();
    let cc_ms = t.elapsed().as_secs_f64() * 1000.0;
    let c_lines = std::fs::read_to_string(&c_path).unwrap().lines().count();
    println!("trans(含进程启动){trans_ms:.0} ms → C {c_lines} 行 → cc -O0 {cc_ms:.0} ms(cc rc={})", cc.status.code().unwrap_or(-1));
    // debug 对照(合成 1k 的 check)
    let t = Instant::now();
    let _ = std::process::Command::new(&dbg_bin)
        .args(["check", std::env::temp_dir().join("ctron_synth_e2e_1k.ct").to_str().unwrap()])
        .output().unwrap();
    let dbg_ms = t.elapsed().as_secs_f64() * 1000.0;
    let t = Instant::now();
    let _ = std::process::Command::new(&rel_bin)
        .args(["check", std::env::temp_dir().join("ctron_synth_e2e_1k.ct").to_str().unwrap()])
        .output().unwrap();
    let rel_ms = t.elapsed().as_secs_f64() * 1000.0;
    println!("debug/release 对照(合成 1k check,含进程启动):debug {dbg_ms:.0} ms vs release {rel_ms:.0} ms");
    // 峰值 RSS(check 50k)
    if !quick {
        let big = std::env::temp_dir().join("ctron_synth_50000.ct");
        // section_scale 已写并被删;重建一份
        let n_fns = 50_000 / 22;
        let src = synth_src(n_fns);
        std::fs::write(&big, &src).unwrap();
        if let Ok(o) = std::process::Command::new("/usr/bin/time")
            .args(["-l", rel_bin.to_str().unwrap(), "check", big.to_str().unwrap()])
            .output()
        {
            let err = String::from_utf8_lossy(&o.stderr);
            for line in err.lines() {
                if line.contains("maximum resident set size") {
                    println!("check 50k 行峰值 RSS:{}", line.trim());
                }
            }
        }
        let _ = std::fs::remove_file(&big);
    }
    println!();
}

fn main() {
    // 基准解除 D2 步数护栏(默认 2_000_000 会截断 fib(25)/百万循环;语料已知会终止)
    std::env::set_var("CTRON_MAX_STEPS", "0");
    let quick = std::env::args().any(|a| a == "quick");
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../tests");
    println!("# Ctron Rust 线性能基线(release,中位数)\n");
    if std::env::args().any(|a| a == "e2e") {
        section_e2e(&root, quick);
        return;
    }
    section_corpus(&root);
    section_scale(quick);
    section_package();
    section_interp(&root);
    section_e2e(&root, quick);
}
