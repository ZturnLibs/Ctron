use std::process::ExitCode;

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().collect();
    match args.get(1).map(String::as_str) {
        Some("version") => {
            println!("ctron {}", ctron::version());
            ExitCode::SUCCESS
        }
        Some("lex") => {
            let Some(path) = args.get(2) else {
                eprintln!("usage: ctron lex <file>");
                return ExitCode::from(2);
            };
            let Ok(src) = std::fs::read_to_string(path) else {
                eprintln!("无法读取 {path}");
                return ExitCode::from(2);
            };
            let (toks, diags) = ctron::lex(&src);
            for d in &diags {
                println!("{path}:{}:{} {}: {}", d.span.line, d.span.col, d.code, d.message);
            }
            println!("{} tokens, {} diagnostics", toks.len(), diags.len());
            if diags.is_empty() { ExitCode::SUCCESS } else { ExitCode::FAILURE }
        }
        Some("parse") => {
            let path = match args.get(2) {
                Some(p) if p != "--ast" => p.clone(),
                _ => {
                    eprintln!("usage: ctron parse <file> [--ast]");
                    return ExitCode::from(2);
                }
            };
            let show_ast = args.iter().any(|a| a == "--ast");
            let Ok(src) = std::fs::read_to_string(&path) else {
                eprintln!("无法读取 {path}");
                return ExitCode::from(2);
            };
            let (file, diags) = ctron::parse_src(&src);
            for d in &diags {
                println!("{path}:{}:{} {}: {}", d.span.line, d.span.col, d.code, d.message);
            }
            if show_ast {
                println!("{file:#?}");
            } else {
                println!("{} decls, {} diagnostics", file.decls.len(), diags.len());
            }
            if diags.is_empty() { ExitCode::SUCCESS } else { ExitCode::FAILURE }
        }
        Some("trans") => {
            let mut path = String::new();
            let mut out_path: Option<String> = None;
            let mut with: Vec<String> = Vec::new();
            let mut a = args.iter().skip(2);
            while let Some(arg) = a.next() {
                if arg == "-o" { out_path = a.next().cloned(); }
                else if arg == "--with" { if let Some(w) = a.next() { with.push(w.clone()); } }
                else if !arg.starts_with('-') && path.is_empty() { path = arg.clone(); }
            }
            let Ok(src) = std::fs::read_to_string(&path) else {
                eprintln!("无法读取 {path}");
                return ExitCode::from(2);
            };
            let (mut file, diags) = ctron::parse_src(&src);
            for w in &with {
                let Ok(ws) = std::fs::read_to_string(w) else {
                    eprintln!("无法读取 {w}");
                    return ExitCode::from(2);
                };
                let (wf, wd) = ctron::parse_src(&ws);
                for d in &wd {
                    eprintln!("{w}:{}:{} {}: {}", d.span.line, d.span.col, d.code, d.message);
                }
                if !wd.is_empty() { return ExitCode::from(1); }
                file.decls.extend(wf.decls);
            }
            for d in &diags {
                eprintln!("{path}:{}:{} {}: {}", d.span.line, d.span.col, d.code, d.message);
            }
            if !diags.is_empty() { return ExitCode::from(1); }
            match ctron::trans::Trans::new().trans_files(std::slice::from_ref(&file)) {
                Ok(c_code) => {
                    match out_path {
                        Some(p) => { let _ = std::fs::write(&p, c_code); println!("{p}"); }
                        None => print!("{c_code}"),
                    }
                    ExitCode::SUCCESS
                }
                Err(reason) => {
                    eprintln!("{path}: {reason}");
                    ExitCode::from(1)
                }
            }
        }
        Some("run") => {
            // 解释器运行:执行全部 test 块
            let path = match args.get(2) {
                Some(p) if !p.starts_with('-') => p.clone(),
                _ => { eprintln!("usage: ctron run <file>"); return ExitCode::from(2); }
            };
            let Ok(src) = std::fs::read_to_string(&path) else {
                eprintln!("无法读取 {path}");
                return ExitCode::from(2);
            };
            let profile = if path.contains("bare") { ctron::sem::Profile::Bare }
                else if path.contains("web") { ctron::sem::Profile::Web }
                else { ctron::sem::Profile::Full };
            // 尊重 //@ panic: 标记:文件声明了 panic 语义 → 预期某个 test panic 且消息含标记
            let panic_marker = src.lines()
                .find(|l| l.trim_start().starts_with("//@ panic:"))
                .map(|l| l.trim_start_matches("//@ panic:").trim().to_string());
            // D1:有 fn main → 运行 main(对齐 C 版 run);否则执行全部 test 块
            let is_main = ctron::parse_src(&src).0.decls.iter().any(|d| {
                matches!(d, ctron::ast::Decl::Fn(f) if f.name == "main")
            });
            if is_main {
                return match ctron::run_main_file(&src, profile) {
                    Ok(code) if code == 0 => ExitCode::SUCCESS,
                    Ok(code) => ExitCode::from(code as u8),
                    Err(m) => { eprintln!("{}", m); ExitCode::from(1) }
                };
            }
            let results = ctron::run_test_file(&src, profile);
            let mut failed = 0usize;
            let panic_ok = match &panic_marker {
                Some(marker) => results.iter().any(|(_, r)| matches!(r, Err(m) if m.contains(marker))),
                None => false,
            };
            for (name, r) in &results {
                let is_expected_panic = panic_marker.is_some()
                    && matches!(r, Err(m) if m.contains(panic_marker.as_ref().unwrap()));
                match r {
                    Ok(()) => println!("ok   {name}"),
                    Err(m) if is_expected_panic => println!("panic-ok {name}: {m}"),
                    Err(m) => { println!("FAIL {name}: {m}"); failed += 1; }
                }
            }
            if let Some(marker) = &panic_marker {
                if !panic_ok {
                    println!("FAIL 预期 panic(含 \"{marker}\")但未发生");
                    failed += 1;
                }
            }
            if failed > 0 { eprintln!("{failed} 个测试失败"); ExitCode::from(1) } else { ExitCode::SUCCESS }
        }
        Some("test") => {
            // R-P2c:ctron test <file|pkg目录> [--filter pat] [--format=json] [--deterministic]
            let mut path = String::new();
            let mut filter: Option<String> = None;
            let mut json = false;
            let mut a = args.iter().skip(2);
            while let Some(arg) = a.next() {
                if arg == "--filter" { filter = a.next().cloned(); }
                else if arg == "--format=json" { json = true; }
                else if arg == "--format" { json = a.next().map(|v| v == "json").unwrap_or(false); }
                // --deterministic:interp 本征顺序执行(任务按 spawn 序号串行),接受占位;
                // 真调度冻结随原生后端并发落地
                else if arg == "--deterministic" {}
                else if !arg.starts_with('-') && path.is_empty() { path = arg.clone(); }
            }
            if path.is_empty() {
                eprintln!("usage: ctron test <file|pkg目录> [--filter pat] [--format=json] [--deterministic]");
                return ExitCode::from(2);
            }
            let src = if std::path::Path::new(&path).is_dir() {
                // pkg 模式:src/*.ct 按文件名序拼接为单源(v0 不去重跨文件 test 名)
                let dir = std::path::Path::new(&path);
                let src_dir = if dir.join("src").is_dir() { dir.join("src") } else { dir.to_path_buf() };
                let mut parts: Vec<String> = Vec::new();
                match std::fs::read_dir(&src_dir) {
                    Ok(es) => {
                        let mut cts: Vec<std::path::PathBuf> = es.flatten().map(|e| e.path())
                            .filter(|p| p.extension().is_some_and(|x| x == "ct")).collect();
                        cts.sort();
                        for c in &cts {
                            match std::fs::read_to_string(c) {
                                Ok(s) => parts.push(s),
                                Err(e) => { eprintln!("无法读取 {}: {e}", c.display()); return ExitCode::from(2); }
                            }
                        }
                    }
                    Err(e) => { eprintln!("无法读取目录 {}: {e}", src_dir.display()); return ExitCode::from(2); }
                }
                parts.join("\n")
            } else {
                match std::fs::read_to_string(&path) {
                    Ok(s) => s,
                    Err(_) => { eprintln!("无法读取 {path}"); return ExitCode::from(2); }
                }
            };
            let profile = if path.contains("bare") { ctron::sem::Profile::Bare }
                else if path.contains("web") { ctron::sem::Profile::Web }
                else { ctron::sem::Profile::Full };
            let report = ctron::testing::test_report(&src, profile, filter.as_deref());
            if !report.diags.is_empty() {
                for d in &report.diags {
                    println!("{path}:{}:{} {}: {}", d.span.line, d.span.col, d.code, d.message);
                }
                return ExitCode::from(1);
            }
            if json {
                let total = report.results.len();
                println!("{{");
                println!("  \"total\": {total},");
                println!("  \"passed\": {},", report.passed());
                println!("  \"failed\": {},", report.failed());
                println!("  \"results\": [");
                for (i, r) in report.results.iter().enumerate() {
                    let comma = if i + 1 < total { "," } else { "" };
                    let name = ctron::testing::json_escape(&r.name);
                    match &r.message {
                        Some(m) => println!("    {{\"name\": \"{name}\", \"status\": \"{}\", \"message\": \"{}\"}}{comma}",
                            r.status.as_str(), ctron::testing::json_escape(m)),
                        None => println!("    {{\"name\": \"{name}\", \"status\": \"{}\"}}{comma}", r.status.as_str()),
                    }
                }
                println!("  ]");
                println!("}}");
            } else {
                for r in &report.results {
                    match (&r.status, &r.message) {
                        (ctron::testing::TestStatus::Pass, _) => println!("ok   {}", r.name),
                        (ctron::testing::TestStatus::PanicOk, Some(m)) => println!("panic-ok {}: {m}", r.name),
                        (ctron::testing::TestStatus::PanicOk, None) => println!("panic-ok {}", r.name),
                        (ctron::testing::TestStatus::Fail, Some(m)) => println!("FAIL {}: {m}", r.name),
                        (ctron::testing::TestStatus::Fail, None) => println!("FAIL {}", r.name),
                    }
                }
                let total = report.results.len();
                println!("{} passed, {} failed ({} total)", report.passed(), report.failed(), total);
            }
            if report.failed() > 0 {
                eprintln!("{} 个测试失败", report.failed());
                return ExitCode::from(1);
            }
            ExitCode::SUCCESS
        }
        Some("fmt") => {
            // R-P2d:ctron fmt <file|pkg目录> [-w] [--check];默认打印格式化结果到 stdout
            let mut path = String::new();
            let mut write_in_place = false;
            let mut check = false;
            let mut a = args.iter().skip(2);
            while let Some(arg) = a.next() {
                if arg == "-w" || arg == "--write" { write_in_place = true; }
                else if arg == "--check" { check = true; }
                else if !arg.starts_with('-') && path.is_empty() { path = arg.clone(); }
            }
            if path.is_empty() {
                eprintln!("usage: ctron fmt <file|pkg目录> [-w] [--check]");
                return ExitCode::from(2);
            }
            // 目标文件集:单文件,或 pkg 目录的 src/*.ct(无 src 则目录直下)
            let mut targets: Vec<std::path::PathBuf> = Vec::new();
            let p = std::path::Path::new(&path);
            if p.is_dir() {
                let src_dir = if p.join("src").is_dir() { p.join("src") } else { p.to_path_buf() };
                match std::fs::read_dir(&src_dir) {
                    Ok(es) => {
                        for e in es.flatten() {
                            let ep = e.path();
                            if ep.extension().is_some_and(|x| x == "ct") { targets.push(ep); }
                        }
                    }
                    Err(e) => { eprintln!("无法读取目录 {}: {e}", src_dir.display()); return ExitCode::from(2); }
                }
                targets.sort();
            } else {
                targets.push(p.to_path_buf());
            }
            let mut unformatted = 0usize;
            let mut errors = 0usize;
            for t in &targets {
                let Ok(src) = std::fs::read_to_string(t) else {
                    eprintln!("无法读取 {}", t.display()); errors += 1; continue;
                };
                match ctron::fmt::fmt_src(&src) {
                    Ok(formatted) => {
                        if check {
                            if formatted != src {
                                println!("{}", t.display());
                                unformatted += 1;
                            }
                        } else if write_in_place {
                            if formatted != src {
                                if let Err(e) = std::fs::write(t, &formatted) {
                                    eprintln!("写入失败 {}: {e}", t.display()); errors += 1; continue;
                                }
                            }
                            println!("{}", t.display());
                        } else {
                            print!("{formatted}");
                        }
                    }
                    Err(msg) => {
                        eprintln!("{}: {msg}", t.display()); errors += 1;
                    }
                }
            }
            if errors > 0 { return ExitCode::from(1); }
            if check {
                if unformatted > 0 {
                    eprintln!("{unformatted} 个文件待格式化");
                    return ExitCode::from(1);
                }
                return ExitCode::SUCCESS;
            }
            ExitCode::SUCCESS
        }
        Some("build") => {
            // 转译 + cc:一行得到原生二进制
            let mut path = String::new();
            let mut out_bin = String::from("a.out");
            let mut with: Vec<String> = Vec::new();
            let mut a = args.iter().skip(2);
            while let Some(arg) = a.next() {
                if arg == "-o" { out_bin = a.next().cloned().unwrap_or(out_bin); }
                else if arg == "--with" { if let Some(w) = a.next() { with.push(w.clone()); } }
                else if !arg.starts_with('-') && path.is_empty() { path = arg.clone(); }
            }
            let Ok(src) = std::fs::read_to_string(&path) else {
                eprintln!("无法读取 {path}");
                return ExitCode::from(2);
            };
            let (mut file, diags) = ctron::parse_src(&src);
            for d in &diags {
                eprintln!("{path}:{}:{} {}: {}", d.span.line, d.span.col, d.code, d.message);
            }
            if !diags.is_empty() { return ExitCode::from(1); }
            for w in &with {
                let Ok(ws) = std::fs::read_to_string(w) else {
                    eprintln!("无法读取 {w}");
                    return ExitCode::from(2);
                };
                let (wf, wd) = ctron::parse_src(&ws);
                for d in &wd {
                    eprintln!("{w}:{}:{} {}: {}", d.span.line, d.span.col, d.code, d.message);
                }
                if !wd.is_empty() { return ExitCode::from(1); }
                file.decls.extend(wf.decls);
            }
            match ctron::trans::Trans::new().trans_files(std::slice::from_ref(&file)) {
                Ok(c_code) => {
                    let c_path = std::env::temp_dir().join("ctron_build.c");
                    std::fs::write(&c_path, c_code).unwrap();
                    let mut cmd = std::process::Command::new("cc");
                    cmd.args(["-O1", "-w", "-std=gnu11"]).arg(&c_path).arg("-o").arg(&out_bin);
                    // 同目录 c_src/*.c 自动链接(FFI)
                    if let Some(dir) = std::path::Path::new(&path).parent() {
                        let cs = if dir.join("c_src").is_dir() { dir.join("c_src") }
                            else if let Some(pp) = dir.parent() { pp.join("c_src") }
                            else { dir.join("c_src") };
                                        if cs.is_dir() {
                            if let Ok(es) = std::fs::read_dir(&cs) {
                                for e in es.flatten() {
                                    if e.path().extension().is_some_and(|x| x == "c") { cmd.arg(e.path()); }
                                }
                            }
                        }
                    }
                    let st = cmd.status();
                    if st.is_err() || !st.as_ref().map(|s| s.success()).unwrap_or(false) {
                        eprintln!("cc 编译失败: {st:?}");
                    }
                    match st {
                        Ok(st) if st.success() => { println!("{out_bin}"); ExitCode::SUCCESS }
                        _ => ExitCode::from(1),
                    }
                }
                Err(reason) => { eprintln!("{path}: {reason}"); ExitCode::from(1) }
            }
        }
        Some("check") => {
            let path = match args.get(2) {
                Some(p) if !p.starts_with("--") => p.clone(),
                _ => {
                    eprintln!("usage: ctron check <file|pkg目录> [--profile bare|web|full]");
                    return ExitCode::from(2);
                }
            };
            // 包级检查:<目录>(含 Ctron.toml + src/*.ct)
            if std::path::Path::new(&path).is_dir() {
                let dir = std::path::Path::new(&path);
                let toml = std::fs::read_to_string(dir.join("Ctron.toml")).unwrap_or_default();
                let manifest = ctron::check::parse_manifest(&toml);
                let pkg = toml.lines().find_map(|l| l.trim().strip_prefix("name = "))
                    .map(|s| s.trim_matches('"').to_string())
                    .unwrap_or_else(|| dir.file_name().map(|n| n.to_string_lossy().to_string()).unwrap_or_default());
                let src_dir = if dir.join("src").is_dir() { dir.join("src") } else { dir.to_path_buf() };
                let mut files: Vec<(String, String)> = Vec::new();
                let mut cts: Vec<std::path::PathBuf> = std::fs::read_dir(&src_dir).unwrap()
                    .flatten().map(|e| e.path())
                    .filter(|p| p.extension().is_some_and(|x| x == "ct")).collect();
                cts.sort();
                for c in &cts {
                    let stem = c.file_stem().unwrap().to_string_lossy().to_string();
                    files.push((format!("{pkg}.{stem}"), std::fs::read_to_string(c).unwrap()));
                }
                let profile = if args.iter().any(|a| a == "--profile") {
                    match args.iter().position(|a| a == "--profile").and_then(|i| args.get(i + 1)) {
                        Some(p) if p == "bare" => ctron::sem::Profile::Bare,
                        Some(p) if p == "web" => ctron::sem::Profile::Web,
                        _ => ctron::sem::Profile::Full,
                    }
                } else { ctron::sem::Profile::Full };
                let result = ctron::check::check_package(&files, Some(manifest), profile);
                let mut err_count = 0usize;
                for (mpath, diags) in &result {
                    for d in diags {
                        println!("{mpath}:{}:{} {}: {}", d.span.line, d.span.col, d.code, d.message);
                        if d.code.starts_with('E') { err_count += 1; }
                    }
                }
                if err_count == 0 {
                    println!("0 errors({} 文件)", files.len());
                    return ExitCode::SUCCESS;
                }
                eprintln!("{err_count} 个错误");
                return ExitCode::from(1);
            }
            let profile = args.iter().position(|a| a == "--profile")
                .and_then(|i| args.get(i + 1))
                .map(|p| match p.as_str() { "bare" => ctron::sem::Profile::Bare, "web" => ctron::sem::Profile::Web, _ => ctron::sem::Profile::Full })
                .unwrap_or(ctron::sem::Profile::Full);
            let Ok(src) = std::fs::read_to_string(&path) else {
                eprintln!("无法读取 {path}");
                return ExitCode::from(2);
            };
            let diags = ctron::check_src(&src, profile);
            for d in &diags {
                println!("{path}:{}:{} {}: {}", d.span.line, d.span.col, d.code, d.message);
            }
            println!("{} diagnostics", diags.len());
            if diags.is_empty() { ExitCode::SUCCESS } else { ExitCode::FAILURE }
        }
        _ => {
            eprintln!("usage: ctron <version|lex|parse|check|run|test|build|trans|fmt> [args]");
            ExitCode::from(2)
        }
    }
}
