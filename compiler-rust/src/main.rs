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
            eprintln!("usage: ctron <version|lex|parse|check> [args]");
            ExitCode::from(2)
        }
    }
}
