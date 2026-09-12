//! 记号模型:规范 §1.3(关键字)/§1.4(字面量)/§1.5(运算符与标点)。

#[derive(Debug, Clone, PartialEq)]
pub enum StrPart {
    /// 已解码的字面文本(转义已展开)
    Text(String),
    /// 插值表达式的原始源文本(未解析,parser 阶段子解析)
    Interp(String),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NumSuffix {
    None, I8, I16, I32, I64, ISize, U8, U16, U32, U64, USize, F32, F64,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Tok {
    // 字面量
    Int { text: String, suffix: NumSuffix },
    Float { text: String, suffix: NumSuffix },
    Str { parts: Vec<StrPart> },
    // 名字与关键字(§1.3 唯一权威清单)
    Ident(String),
    Fn, Let, Var, Const, Static, Comptime, If, Else, Match, While, For, In,
    Return, Struct, Class, Enum, Trait, Impl, Own, Scope, Test, Use, Pub, Extern,
    Prop, True, False, Void, SelfKw,
    // 运算符与标点(§1.5)
    Plus, Minus, Star, Slash, Percent, WrapPlus, WrapMinus,
    PlusEq, MinusEq, StarEq, SlashEq, PercentEq,
    EqEq, NotEq, Lt, Gt, LtEq, GtEq, Assign,
    AndAnd, OrOr, Or,     // OrOr = 逻辑或(v0.7);or = 取默认中缀(保留运算符字)
    DotDot, DotDotEq, Arrow, FatArrow, Question, Dot, Comma, Colon,
    LBracket, RBracket, LParen, RParen, LBrace, RBrace, Pipe, Amp,
    Hash, At, Underscore, Bang,
    // 结构
    Newline, Eof,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Span {
    pub line: u32,   // 1-based
    pub col: u32,    // 1-based
    pub start: usize, // 字节偏移
    pub end: usize,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Token {
    pub tok: Tok,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Diagnostic {
    pub code: &'static str, // 词法器只用 "E1001"
    pub message: String,
    pub span: Span,
}

impl Span {
    pub fn new(line: u32, col: u32, start: usize, end: usize) -> Span {
        Span { line, col, start, end }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn token_kinds_are_constructible() {
        let t = Token { tok: Tok::Int { text: "255".into(), suffix: NumSuffix::U8 }, span: Span::new(1, 1, 0, 5) };
        assert_eq!(t.tok, Tok::Int { text: "255".into(), suffix: NumSuffix::U8 });
        let d = Diagnostic { code: "E1001", message: "禁用的标点 ;".into(), span: t.span };
        assert_eq!(d.code, "E1001");
    }
}
