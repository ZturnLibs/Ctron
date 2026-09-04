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
        _ => {
            eprintln!("usage: ctron <version|lex> [args]");
            ExitCode::from(2)
        }
    }
}
