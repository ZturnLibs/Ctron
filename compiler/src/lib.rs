pub mod ast;
pub mod lexer;
pub mod parser;
pub mod token;

pub use lexer::lex;
pub use parser::parse_tokens;

pub fn version() -> &'static str {
    env!("CARGO_PKG_VERSION")
}

/// 词法 + 解析一步到位;词法诊断与解析诊断按序合并。
pub fn parse_src(src: &str) -> (ast::File, Vec<token::Diagnostic>) {
    let (toks, mut diags) = lex(src);
    let (file, mut pdiags) = parse_tokens(toks, Vec::new());
    diags.append(&mut pdiags);
    (file, diags)
}
