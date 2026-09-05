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
            let mut a = args.iter().skip(2);
            while let Some(arg) = a.next() {
                if arg == "-o" { out_path = a.next().cloned(); }
                else if !arg.starts_with('-') && path.is_empty() { path = arg.clone(); }
            }
            let Ok(src) = std::fs::read_to_string(&path) else {
                eprintln!("无法读取 {path}");
                return ExitCode::from(2);
            };
            let (file, diags) = ctron::parse_src(&src);
            for d in &diags {
                eprintln!("{path}:{}:{} {}: {}", d.span.line, d.span.col, d.code, d.message);
            }
            if !diags.is_empty() { return ExitCode::from(1); }
            match ctron::trans::Trans::new().trans_file(&file) {
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
        Some("check") => {
            let path = match args.get(2) {
                Some(p) if !p.starts_with("--") => p.clone(),
                _ => {
                    eprintln!("usage: ctron check <file> [--profile bare|web|full]");
                    return ExitCode::from(2);
                }
            };
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
