//! 手写单遍词法器:规范 §1。换行显著性在 lex() 的过滤阶段处理(§1.6)。

use crate::token::{Diagnostic, Span, Tok, Token};

pub struct Lexer<'src> {
    src: &'src [u8],
    pos: usize,
    line: u32,
    col: u32,
    diags: Vec<Diagnostic>,
}

impl<'src> Lexer<'src> {
    pub fn new(src: &'src str) -> Self {
        Lexer { src: src.as_bytes(), pos: 0, line: 1, col: 1, diags: Vec::new() }
    }

    fn peek(&self) -> Option<u8> { self.src.get(self.pos).copied() }
    fn peek2(&self) -> Option<u8> { self.src.get(self.pos + 1).copied() }
    fn peek3(&self) -> Option<u8> { self.src.get(self.pos + 2).copied() }

    fn bump(&mut self) -> Option<u8> {
        let c = self.peek()?;
        self.pos += 1;
        if c == b'\n' { self.line += 1; self.col = 1; } else { self.col += 1; }
        Some(c)
    }

    fn mark(&self, start: usize, line: u32, col: u32) -> Span {
        Span { line, col, start, end: self.pos }
    }

    fn err(&mut self, code: &'static str, msg: String, span: Span) {
        self.diags.push(Diagnostic { code, message: msg, span });
    }

    /// 跳过行内空白与注释(含 /// 与 //@);换行不在此处理(它是记号)。
    fn skip_inline_trivia(&mut self) {
        loop {
            match self.peek() {
                Some(b' ') | Some(b'\t') | Some(b'\r') => { self.bump(); }
                Some(b'/') if self.peek2() == Some(b'/') => {
                    while let Some(c) = self.peek() {
                        if c == b'\n' { break }
                        self.bump();
                    }
                }
                _ => break,
            }
        }
    }

    pub fn next_token(&mut self) -> Token {
        self.skip_inline_trivia();
        let (start, line, col) = (self.pos, self.line, self.col);
        let Some(c) = self.peek() else {
            return Token { tok: Tok::Eof, span: self.mark(start, line, col) };
        };
        if c == b'\n' {
            self.bump();
            return Token { tok: Tok::Newline, span: self.mark(start, line, col) };
        }
        if c.is_ascii_alphabetic() || c == b'_' {
            return self.lex_name(start, line, col);
        }
        if c.is_ascii_digit() {
            return self.lex_number(start, line, col);
        }
        if c == b'"' {
            return self.lex_string(start, line, col);
        }
        self.lex_punct(start, line, col)
    }

    fn lex_name(&mut self, start: usize, line: u32, col: u32) -> Token {
        while matches!(self.peek(), Some(c) if c.is_ascii_alphanumeric() || c == b'_') {
            self.bump();
        }
        let s = std::str::from_utf8(&self.src[start..self.pos]).unwrap_or("").to_string();
        let tok = match s.as_str() {
            "fn" => Tok::Fn, "let" => Tok::Let, "var" => Tok::Var,
            "const" => Tok::Const, "static" => Tok::Static, "comptime" => Tok::Comptime,
            "if" => Tok::If, "else" => Tok::Else, "match" => Tok::Match,
            "while" => Tok::While, "for" => Tok::For, "in" => Tok::In,
            "return" => Tok::Return, "struct" => Tok::Struct, "class" => Tok::Class,
            "enum" => Tok::Enum, "trait" => Tok::Trait, "impl" => Tok::Impl,
            "own" => Tok::Own, "scope" => Tok::Scope, "test" => Tok::Test,
            "use" => Tok::Use, "pub" => Tok::Pub, "extern" => Tok::Extern, "prop" => Tok::Prop,
            "true" => Tok::True, "false" => Tok::False, "void" => Tok::Void,
            "self" => Tok::SelfKw,
            "_" => Tok::Underscore, // 通配(§1.5);锚定测试 keywords_and_idents
            "or" => Tok::Or, // 保留运算符字(§1.3);break/continue 等预留字按 Ident 处理,由 parser 拒绝
            _ => Tok::Ident(s),
        };
        Token { tok, span: self.mark(start, line, col) }
    }

    fn lex_number(&mut self, start: usize, line: u32, col: u32) -> Token {
        // Task 4 完成数字;此处先消费数字字符避免误判标点
        while matches!(self.peek(), Some(c) if c.is_ascii_digit()) { self.bump(); }
        let text = std::str::from_utf8(&self.src[start..self.pos]).unwrap_or("").to_string();
        Token { tok: Tok::Int { text, suffix: crate::token::NumSuffix::None }, span: self.mark(start, line, col) }
    }

    fn lex_string(&mut self, start: usize, line: u32, col: u32) -> Token {
        self.bump(); // 开引号
        let mut text = String::new();
        let tok = loop {
            match self.peek() {
                None | Some(b'\n') => {
                    let sp = self.mark(start, line, col);
                    self.err("E1001", "未终止的字符串(不允许跨行)".into(), sp);
                    break Tok::Str { parts: vec![crate::token::StrPart::Text(text)] };
                }
                Some(b'"') => { self.bump(); break Tok::Str { parts: vec![crate::token::StrPart::Text(text)] }; }
                Some(_) => { text.push(self.bump().unwrap() as char); }
            }
        };
        Token { tok, span: self.mark(start, line, col) }
    }

    fn lex_punct(&mut self, start: usize, line: u32, col: u32) -> Token {
        // 闭包只捕获拷贝(src 引用与 pos),避免与 self.bump() 的可变借用冲突
        let (src0, pos0) = (self.src, self.pos);
        let two = |a: u8, b: u8| src0.len() > pos0 + 1 && src0[pos0] == a && src0[pos0 + 1] == b;
        let three = |a: u8, b: u8, c: u8| {
            src0.len() > pos0 + 2 && src0[pos0] == a && src0[pos0 + 1] == b && src0[pos0 + 2] == c
        };
        let tok = if two(b'+', b'%') { self.bump(); self.bump(); Tok::WrapPlus }
        else if two(b'-', b'%') { self.bump(); self.bump(); Tok::WrapMinus }
        else if two(b'+', b'=') { self.bump(); self.bump(); Tok::PlusEq }
        else if two(b'-', b'=') { self.bump(); self.bump(); Tok::MinusEq }
        else if two(b'*', b'=') { self.bump(); self.bump(); Tok::StarEq }
        else if two(b'/', b'=') { self.bump(); self.bump(); Tok::SlashEq }
        else if two(b'%', b'=') { self.bump(); self.bump(); Tok::PercentEq }
        else if two(b'=', b'=') { self.bump(); self.bump(); Tok::EqEq }
        else if two(b'!', b'=') { self.bump(); self.bump(); Tok::NotEq }
        else if two(b'<', b'=') { self.bump(); self.bump(); Tok::LtEq }
        else if two(b'>', b'=') { self.bump(); self.bump(); Tok::GtEq }
        else if two(b'&', b'&') { self.bump(); self.bump(); Tok::AndAnd }
        else if three(b'.', b'.', b'=') { self.bump(); self.bump(); self.bump(); Tok::DotDotEq } // 最长匹配 ..=(§1.5)
        else if two(b'.', b'.') { self.bump(); self.bump(); Tok::DotDot }
        else if two(b'-', b'>') { self.bump(); self.bump(); Tok::Arrow }
        else if two(b'=', b'>') { self.bump(); self.bump(); Tok::FatArrow }
        else if two(b':', b':') {
            self.bump(); self.bump();
            let sp = self.mark(start, line, col);
            self.err("E1001", "禁用的标点 ::".into(), sp);
            Tok::Colon // 恢复为单个冒号继续
        }
        else {
            let c = self.bump().unwrap();
            match c {
                b'+' => Tok::Plus, b'-' => Tok::Minus, b'*' => Tok::Star,
                b'/' => Tok::Slash, b'%' => Tok::Percent, b'=' => Tok::Assign,
                b'<' => Tok::Lt, b'>' => Tok::Gt, b'!' => Tok::Bang,
                b'?' => Tok::Question, b'.' => Tok::Dot, b',' => Tok::Comma,
                b':' => Tok::Colon, b'[' => Tok::LBracket, b']' => Tok::RBracket,
                b'(' => Tok::LParen, b')' => Tok::RParen, b'{' => Tok::LBrace,
                b'}' => Tok::RBrace, b'|' => Tok::Pipe, b'&' => Tok::Amp,
                b'#' => Tok::Hash, b'@' => Tok::At,
                b';' => {
                    let sp = self.mark(start, line, col);
                    self.err("E1001", "禁用的标点 ;".into(), sp);
                    Tok::Eof // 丢弃该记号:调用方循环重取
                }
                _ => {
                    let sp = self.mark(start, line, col);
                    self.err("E1001", format!("无法识别的字符 '{}'", c as char), sp);
                    Tok::Eof
                }
            }
        };
        if tok == Tok::Eof && self.pos < self.src.len() {
            return self.next_token(); // 错误记号被吞,继续
        }
        Token { tok, span: self.mark(start, line, col) }
    }
}

/// 入口:词法整个源文件;换行显著性过滤见 filter_newlines(§1.6)。
pub fn lex(src: &str) -> (Vec<Token>, Vec<Diagnostic>) {
    let mut lx = Lexer::new(src);
    let mut raw = Vec::new();
    loop {
        let t = lx.next_token();
        let eof = t.tok == Tok::Eof;
        raw.push(t);
        if eof { break }
    }
    let diags = lx.diags;
    (filter_newlines(raw), diags)
}

/// §1.6:行尾在延续集 → 换行无效;下一行以 `.` 或二元运算符开头 → 换行无效;连续换行折叠为一个。
/// 文件起始(第一个实记号之前)的 K 个前导换行保留为 K-1 个:首行不需要终止符
/// (锚定测试 comments_and_markers_are_trivia)。
fn filter_newlines(mut raw: Vec<Token>) -> Vec<Token> {
    let mut out: Vec<Token> = Vec::with_capacity(raw.len());
    let lead = raw.iter().take_while(|t| t.tok == Tok::Newline).count();
    out.extend(raw.drain(0..lead).skip(1));
    while !raw.is_empty() {
        let t = raw.remove(0);
        if t.tok != Tok::Newline {
            out.push(t);
            continue;
        }
        // 收集连续换行,看向第一个实记号
        let mut next = raw.first().map(|x| x.tok.clone());
        while next.as_ref() == Some(&Tok::Newline) {
            raw.remove(0);
            next = raw.first().map(|x| x.tok.clone());
        }
        let suppressed = match out.last() {
            Some(prev) => line_end_continues(&prev.tok),
            None => true, // 文件开头
        } || next.as_ref().is_some_and(continues_next_line);
        if !suppressed {
            out.push(t);
        }
    }
    out
}

fn line_end_continues(t: &Tok) -> bool {
    matches!(t, Tok::Comma | Tok::Assign | Tok::Arrow | Tok::FatArrow | Tok::AndAnd
        | Tok::Or | Tok::DotDot | Tok::DotDotEq | Tok::Plus | Tok::Minus | Tok::Star
        | Tok::Slash | Tok::Percent | Tok::WrapPlus | Tok::WrapMinus | Tok::EqEq
        | Tok::NotEq | Tok::Lt | Tok::Gt | Tok::LtEq | Tok::GtEq | Tok::LParen
        | Tok::LBracket | Tok::LBrace | Tok::Pipe | Tok::Question)
}

fn continues_next_line(t: &Tok) -> bool {
    matches!(t, Tok::Dot | Tok::Plus | Tok::Minus | Tok::Star | Tok::Slash
        | Tok::Percent | Tok::WrapPlus | Tok::WrapMinus | Tok::EqEq | Tok::NotEq
        | Tok::Lt | Tok::Gt | Tok::LtEq | Tok::GtEq | Tok::AndAnd | Tok::Or
        | Tok::DotDot | Tok::DotDotEq)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::token::{NumSuffix, StrPart, Tok};

    fn kinds(src: &str) -> Vec<Tok> {
        lex(src).0.into_iter().map(|t| t.tok).collect()
    }

    #[test]
    fn keywords_and_idents() {
        assert_eq!(kinds("fn let var struct extern\n"), vec![Tok::Fn, Tok::Let, Tok::Var, Tok::Struct, Tok::Extern, Tok::Newline, Tok::Eof]);
        assert_eq!(kinds("order org own"), vec![Tok::Ident("order".into()), Tok::Ident("org".into()), Tok::Own, Tok::Eof]);
        assert_eq!(kinds("self selfish"), vec![Tok::SelfKw, Tok::Ident("selfish".into()), Tok::Eof]);
        assert_eq!(kinds("_ _x"), vec![Tok::Underscore, Tok::Ident("_x".into()), Tok::Eof]);
    }

    #[test]
    fn operators_longest_match() {
        assert_eq!(kinds("+% += .. ..= -> => == != <= >= &&"),
            vec![Tok::WrapPlus, Tok::PlusEq, Tok::DotDot, Tok::DotDotEq, Tok::Arrow,
                 Tok::FatArrow, Tok::EqEq, Tok::NotEq, Tok::LtEq, Tok::GtEq, Tok::AndAnd, Tok::Eof]);
        assert_eq!(kinds("+ = < . , : # @ | & ! ?"),
            vec![Tok::Plus, Tok::Assign, Tok::Lt, Tok::Dot, Tok::Comma, Tok::Colon,
                 Tok::Hash, Tok::At, Tok::Pipe, Tok::Amp, Tok::Bang, Tok::Question, Tok::Eof]);
    }

    #[test]
    fn comments_and_markers_are_trivia() {
        assert_eq!(kinds("// 普通注释\n//@ fail: E2030\n/// doc\nlet"),
            vec![Tok::Newline, Tok::Newline, Tok::Let, Tok::Eof]);
    }

    #[test]
    fn banned_punctuation_is_e1001() {
        let (_, diags) = lex("let a = 1;");
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "E1001");
        assert!(diags[0].message.contains(';'));
        let (_, diags) = lex("a :: b");
        assert_eq!(diags[0].code, "E1001");
    }
}
