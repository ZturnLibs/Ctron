pub mod ast;
pub mod check;
pub mod interp;
pub mod trans;
pub mod sem;
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

/// 单文件语义检查
/// 运行单个文件的全部 test 块;返回 (测试名, 结果)
pub fn run_test_file(src: &str, profile: sem::Profile) -> Vec<(String, Result<(), String>)> {
    interp::run_test_file(src, profile)
}

/// 运行 fn main(D1):返回退出码;解析/运行失败 → Err
pub fn run_main_file(src: &str, profile: sem::Profile) -> Result<i32, String> {
    interp::run_main_file(src, profile)
}

pub fn check_src(src: &str, profile: sem::Profile) -> Vec<token::Diagnostic> {
    check::check_src(src, profile)
}

/// 多文件包语义检查
pub fn check_package(files: &[(String, String)], manifest: Option<sem::Manifest>, profile: sem::Profile)
    -> Vec<(String, Vec<token::Diagnostic>)> {
    check::check_package(files, manifest, profile)
}
