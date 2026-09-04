use std::process::ExitCode;

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().collect();
    match args.get(1).map(String::as_str) {
        Some("version") => {
            println!("ctron {}", ctron::version());
            ExitCode::SUCCESS
        }
        _ => {
            eprintln!("usage: ctron <version|lex> [args]");
            ExitCode::from(2)
        }
    }
}
