pub mod lexer;
pub mod token;

pub use lexer::lex;

pub fn version() -> &'static str {
    env!("CARGO_PKG_VERSION")
}
