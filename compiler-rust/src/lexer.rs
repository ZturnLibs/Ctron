//! 手写单遍词法器:规范 §1。换行显著性在 lex() 的过滤阶段处理(§1.6)。

use crate::token::{Diagnostic, Span, Tok, Token};

pub struct Lexer<'src> {
    src: &'src [u8],
    pos: usize,
    line: u32,
    col: u32,
    diags: Vec<Diagnostic>,
    /// 上一个已产出记号是否为 `.`:成员位置的 `or` 是方法名(x.or(默认),§5.2),按 Ident 产出
    prev_is_dot: bool,
}

impl<'src> Lexer<'src> {
    pub fn new(src: &'src str) -> Self {
        Lexer { src: src.as_bytes(), pos: 0, line: 1, col: 1, diags: Vec::new(), prev_is_dot: false }
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

    /// Some(t) = 实记号;None = 错误记号已吞(哨兵),由 lex() 主循环消化。
    /// prev_is_dot 仅按实记号更新:哨兵不得清除 `.` 状态(`x.;y` 中 y 仍是成员名)。
    pub fn next_token(&mut self) -> Option<Token> {
        let t = self.next_token_inner()?;
        self.prev_is_dot = t.tok == Tok::Dot;
        Some(t)
    }

    fn next_token_inner(&mut self) -> Option<Token> {
        self.skip_inline_trivia();
        let (start, line, col) = (self.pos, self.line, self.col);
        let Some(c) = self.peek() else {
            return Some(Token { tok: Tok::Eof, span: self.mark(start, line, col) });
        };
        if c == b'\n' {
            self.bump();
            return Some(Token { tok: Tok::Newline, span: self.mark(start, line, col) });
        }
        if c.is_ascii_alphabetic() || c == b'_' {
            return Some(self.lex_name(start, line, col));
        }
        if c.is_ascii_digit() {
            return Some(self.lex_number(start, line, col));
        }
        if c == b'"' {
            return Some(self.lex_string(start, line, col));
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
            // v0.7 修订二:预留字转正(原按 Ident 处理)
            "break" => Tok::Break, "continue" => Tok::Continue,
            "enum" => Tok::Enum, "trait" => Tok::Trait, "impl" => Tok::Impl,
            "own" => Tok::Own, "scope" => Tok::Scope, "test" => Tok::Test,
            "use" => Tok::Use, "pub" => Tok::Pub, "extern" => Tok::Extern, "prop" => Tok::Prop,
            "true" => Tok::True, "false" => Tok::False, "void" => Tok::Void,
            "self" => Tok::SelfKw,
            "_" => Tok::Underscore, // 通配(§1.5);锚定测试 keywords_and_idents
            // 保留运算符字(§1.3),但成员位置除外:x.or(默认)(§5.2)按 Ident 产出(锚定测试 newline_rules_1_6);
            // break/continue 等预留字按 Ident 处理,由 parser 拒绝
            "or" if !self.prev_is_dot => Tok::Or,
            _ => Tok::Ident(s),
        };
        Token { tok, span: self.mark(start, line, col) }
    }

    fn lex_number(&mut self, start: usize, line: u32, col: u32) -> Token {
        let radix = if self.peek() == Some(b'0') {
            // 进制前缀仅小写:`0XFF`/`0O`/`0B` 不复认,按 Int("0") + Ident 回落
            match self.peek2() {
                Some(b'x') => Some(16),
                Some(b'o') => Some(8),
                Some(b'b') => Some(1), // 1 占位:二进制,见下方统一改写
                _ => None,
            }
        } else { None };
        if let Some(r) = radix {
            self.bump(); self.bump();
            let r = if r == 1 { 2 } else { r };
            let digits_start = self.pos;
            while matches!(self.peek(), Some(c) if (c as char).is_digit(r as u32) || c == b'_') {
                self.bump();
            }
            if self.pos == digits_start {
                let sp = self.mark(start, line, col);
                self.err("E1001", "进制字面量缺少数字".into(), sp);
            }
            // 进制字面量不接受小数点/指数;直接进入后缀
            let text = self.src[start..self.pos].iter().map(|&b| b as char).collect();
            let suffix = self.take_suffix();
            return Token { tok: Tok::Int { text, suffix }, span: self.mark(start, line, col) };
        }
        // 十进制整数部分
        while matches!(self.peek(), Some(c) if c.is_ascii_digit() || c == b'_') { self.bump(); }
        let mut is_float = false;
        // 浮点:`.` 后跟数字才是(1..5 是 range;21.double() 是方法)
        if self.peek() == Some(b'.') && self.peek2().is_some_and(|c| c.is_ascii_digit()) {
            is_float = true;
            self.bump();
            while matches!(self.peek(), Some(c) if c.is_ascii_digit() || c == b'_') { self.bump(); }
        }
        // 指数:e/E [+-] 数字
        if matches!(self.peek(), Some(b'e') | Some(b'E')) {
            let sign = matches!(self.peek2(), Some(b'+') | Some(b'-'));
            let digit_after = if sign { self.peek3().is_some_and(|c| c.is_ascii_digit()) }
                              else { self.peek2().is_some_and(|c| c.is_ascii_digit()) };
            if digit_after {
                is_float = true;
                self.bump();
                if sign { self.bump(); }
                while matches!(self.peek(), Some(c) if c.is_ascii_digit() || c == b'_') { self.bump(); }
            }
        }
        let text = self.src[start..self.pos].iter().map(|&b| b as char).collect();
        let suffix = self.take_suffix();
        let tok = if is_float { Tok::Float { text, suffix } } else { Tok::Int { text, suffix } };
        Token { tok, span: self.mark(start, line, col) }
    }

    fn take_suffix(&mut self) -> crate::token::NumSuffix {
        use crate::token::NumSuffix::*;
        const SUFFIXES: [(&str, crate::token::NumSuffix); 12] = [
            ("i8", I8), ("i16", I16), ("i32", I32), ("i64", I64), ("isize", ISize),
            ("u8", U8), ("u16", U16), ("u32", U32), ("u64", U64), ("usize", USize),
            ("f32", F32), ("f64", F64),
        ];
        for (s, suff) in SUFFIXES {
            let n = s.len();
            // 后缀精确匹配(区分大小写):`255U8` 不匹配 u8,回落为 Int + Ident(U8)
            if self.src.len() >= self.pos + n
                && &self.src[self.pos..self.pos + n] == s.as_bytes() {
                // 后缀必须是完整词(后面不能紧跟标识符字符)
                let after = self.src.get(self.pos + n).copied();
                if !matches!(after, Some(c) if c.is_ascii_alphanumeric() || c == b'_') {
                    for _ in 0..n { self.bump(); }
                    return suff;
                }
            }
        }
        None
    }

    fn lex_string(&mut self, start: usize, line: u32, col: u32) -> Token {
        use crate::token::StrPart;
        self.bump(); // 开引号
        let mut parts: Vec<StrPart> = Vec::new();
        // 按字节收集、收尾统一 from_utf8_lossy:多字节 UTF-8 字符逐字节复制后重组,不会损坏(§1.4)
        let mut bytes: Vec<u8> = Vec::new();
        let tok = loop {
            match self.peek() {
                None | Some(b'\n') => {
                    if !bytes.is_empty() {
                        parts.push(StrPart::Text(String::from_utf8_lossy(&bytes).into_owned()));
                    }
                    let sp = self.mark(start, line, col);
                    self.err("E1001", "未终止的字符串(不允许跨行)".into(), sp);
                    break Tok::Str { parts };
                }
                Some(b'"') => {
                    self.bump();
                    if !bytes.is_empty() {
                        parts.push(StrPart::Text(String::from_utf8_lossy(&bytes).into_owned()));
                    }
                    break Tok::Str { parts };
                }
                Some(b'\\') => {
                    self.bump();
                    let Some(esc) = self.bump() else {
                        // 反斜杠后即文件尾:按未终止处理
                        if !bytes.is_empty() {
                            parts.push(StrPart::Text(String::from_utf8_lossy(&bytes).into_owned()));
                        }
                        let sp = self.mark(start, line, col);
                        self.err("E1001", "未终止的字符串(不允许跨行)".into(), sp);
                        break Tok::Str { parts };
                    };
                    match esc {
                        b'n' => bytes.push(b'\n'),
                        b't' => bytes.push(b'\t'),
                        b'r' => bytes.push(b'\r'),
                        b'0' => bytes.push(0),
                        b'\\' => bytes.push(b'\\'),
                        b'"' => bytes.push(b'"'),
                        b'{' => bytes.push(b'{'),
                        b'u' => {
                            // \u{HEX}
                            if self.peek() == Some(b'{') {
                                self.bump();
                                let mut hex = String::new();
                                while matches!(self.peek(), Some(c) if c.is_ascii_hexdigit()) {
                                    hex.push(self.bump().unwrap() as char);
                                }
                                if self.peek() == Some(b'}') && !hex.is_empty() {
                                    self.bump();
                                    // 解析溢出 u32 或非标量值(代理区)→ 非法 Unicode 转义,不再静默替换
                                    if let Some(ch) =
                                        u32::from_str_radix(&hex, 16).ok().and_then(char::from_u32)
                                    {
                                        let mut buf = [0u8; 4];
                                        bytes.extend_from_slice(ch.encode_utf8(&mut buf).as_bytes());
                                    } else {
                                        let sp = self.mark(start, line, col);
                                        self.err("E1001", "非法的 Unicode 转义".into(), sp);
                                    }
                                } else {
                                    let sp = self.mark(start, line, col);
                                    self.err("E1001", "\\u 转义缺少 {HEX}".into(), sp);
                                }
                            } else {
                                let sp = self.mark(start, line, col);
                                self.err("E1001", "\\u 转义缺少 {".into(), sp);
                            }
                        }
                        other => {
                            let sp = self.mark(start, line, col);
                            self.err("E1001", format!("非法转义 \\{}", other as char), sp);
                        }
                    }
                }
                Some(b'{') => {
                    // 插值:同一行内必须存在配对的 '}'(支持 {} 嵌套深度计数,§1.4);
                    // 字面 '{' 必须写 `\{`,行内无配对 '}' 的裸 '{' 报 E1001"未终止的插值",
                    // 不回退为字面(撤销 12945f3 的字面回退,规范所有者裁决 §1.4)
                    if !bytes.is_empty() {
                        parts.push(StrPart::Text(String::from_utf8_lossy(&bytes).into_owned()));
                        bytes.clear();
                    }
                    self.bump(); // {
                    let expr_start = self.pos;
                    let mut depth = 1usize;
                    let mut terminated = false;
                    while depth > 0 {
                        match self.peek() {
                            None | Some(b'\n') => { depth = 0; }
                            Some(b'"') => {
                                // 插值不得跨字符串配对:扫到 '"' 即未终止插值;
                                // 消耗它作为本串截断收尾引号,产出两个独立 Str(不融合、不续扫下一段)
                                self.bump();
                                depth = 0;
                            }
                            Some(b'{') => { depth += 1; self.bump(); }
                            Some(b'}') => {
                                depth -= 1;
                                // 收尾 '}' 不在此消耗:保证 raw 切片不含它,由下方统一消耗
                                if depth > 0 { self.bump(); } else { terminated = true; }
                            }
                            Some(_) => { self.bump(); }
                        }
                    }
                    if terminated {
                        let raw = String::from_utf8_lossy(&self.src[expr_start..self.pos]).into_owned();
                        self.bump(); // 收尾 '}'
                        parts.push(StrPart::Interp(raw));
                    } else {
                        // 行内无配对 '}':报 E1001 后字符串就此截断,只出这一条诊断
                        let sp = self.mark(start, line, col);
                        self.err("E1001", "未终止的插值".into(), sp);
                        break Tok::Str { parts };
                    }
                }
                Some(_) => {
                    // 源文件保证 UTF-8;多字节字符逐字节入缓冲,收尾 from_utf8_lossy 重组
                    bytes.push(self.bump().unwrap());
                }
            }
        };
        Token { tok, span: self.mark(start, line, col) }
    }

    fn lex_punct(&mut self, start: usize, line: u32, col: u32) -> Option<Token> {
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
        else if two(b'|', b'|') { self.bump(); self.bump(); Tok::OrOr } // v0.7:最长匹配优先于两个单 |(起始位零参闭包由 parser 角色分离,§4.0)
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
        if tok == Tok::Eof {
            // 错误记号(`;`/无法识别字符)已吞:返回哨兵 None,由 lex() 主循环继续取下一记号
            // (不再 `return self.next_token()` 尾递归——100k 连续 `;` 曾栈溢出)
            return None;
        }
        Some(Token { tok, span: self.mark(start, line, col) })
    }
}

/// 入口:词法整个源文件;换行显著性过滤见 filter_newlines(§1.6)。
pub fn lex(src: &str) -> (Vec<Token>, Vec<Diagnostic>) {
    let mut lx = Lexer::new(src);
    let mut raw = Vec::new();
    loop {
        match lx.next_token() {
            // 哨兵(错误记号已吞)由主循环消化:错误恢复是循环,非尾递归
            None => continue,
            Some(t) => {
                let eof = t.tok == Tok::Eof;
                raw.push(t);
                if eof { break }
            }
        }
    }
    let diags = lx.diags;
    (filter_newlines(raw), diags)
}

/// §1.6:行尾在延续集 → 换行无效;下一行以 `.` 或二元运算符开头 → 换行无效;连续换行折叠为一个。
/// 文件起始(第一个实记号之前)的 K 个前导换行保留为 K-1 个:首行不需要终止符
/// (锚定测试 comments_and_markers_are_trivia)。
fn filter_newlines(raw: Vec<Token>) -> Vec<Token> {
    // VecDeque 的 pop_front/drain(0..n) 均为 O(1)/一次性摊还:
    // 取代 Vec::remove(0) 逐次整体搬移的 O(n²)(50k 行源文件 61.5s → 亚秒)
    use std::collections::VecDeque;
    let mut rest: VecDeque<Token> = raw.into();
    let mut out: Vec<Token> = Vec::with_capacity(rest.len());
    let lead = rest.iter().take_while(|t| t.tok == Tok::Newline).count();
    out.extend(rest.drain(0..lead).skip(1));
    while let Some(t) = rest.pop_front() {
        if t.tok != Tok::Newline {
            out.push(t);
            continue;
        }
        // 收集连续换行,看向第一个实记号
        let mut next = rest.front().map(|x| x.tok.clone());
        while next.as_ref() == Some(&Tok::Newline) {
            rest.pop_front();
            next = rest.front().map(|x| x.tok.clone());
        }
        // 走到此处 out 必非空:前导换行已保留 K-1(K≥2)个,否则首个实记号已入 out
        let suppressed = matches!(out.last().map(|t| &t.tok), Some(prev) if line_end_continues(prev))
            || next.as_ref().is_some_and(continues_next_line);
        if !suppressed {
            out.push(t);
        }
    }
    out
}

fn line_end_continues(t: &Tok) -> bool {
    matches!(t, Tok::Comma | Tok::Assign | Tok::Arrow | Tok::FatArrow | Tok::AndAnd
        | Tok::OrOr | Tok::Or | Tok::DotDot | Tok::DotDotEq | Tok::Plus | Tok::Minus | Tok::Star
        | Tok::Slash | Tok::Percent | Tok::WrapPlus | Tok::WrapMinus | Tok::EqEq
        | Tok::NotEq | Tok::Lt | Tok::Gt | Tok::LtEq | Tok::GtEq | Tok::LParen
        | Tok::LBracket | Tok::LBrace | Tok::Pipe)
}

fn continues_next_line(t: &Tok) -> bool {
    matches!(t, Tok::Dot | Tok::Plus | Tok::Minus | Tok::Star | Tok::Slash
        | Tok::Percent | Tok::WrapPlus | Tok::WrapMinus | Tok::EqEq | Tok::NotEq
        | Tok::Lt | Tok::Gt | Tok::LtEq | Tok::GtEq | Tok::AndAnd | Tok::OrOr | Tok::Or
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

    #[test]
    fn hundred_k_semicolons_no_stack_overflow() {
        // 错误记号恢复必须由 lex() 主循环消化哨兵,而非 next_token 尾递归:
        // 100k 连续 `;` 在旧实现下递归 100k 层会栈溢出
        let src = ";".repeat(100_000);
        let (toks, diags) = lex(&src);
        assert_eq!(diags.len(), 100_000);
        assert!(diags.iter().all(|d| d.code == "E1001"));
        // lex 完成:除真实 Eof 外无记号产出
        assert_eq!(toks.len(), 1);
        assert_eq!(toks[0].tok, Tok::Eof);
    }

    #[test]
    fn number_radix_underscores_and_suffixes() {
        assert_eq!(kinds("255u8 0xFF 0o17 0b1010 1_000_000 5usize"),
            vec![Tok::Int { text: "255".into(), suffix: NumSuffix::U8 },
                 Tok::Int { text: "0xFF".into(), suffix: NumSuffix::None },
                 Tok::Int { text: "0o17".into(), suffix: NumSuffix::None },
                 Tok::Int { text: "0b1010".into(), suffix: NumSuffix::None },
                 Tok::Int { text: "1_000_000".into(), suffix: NumSuffix::None },
                 Tok::Int { text: "5".into(), suffix: NumSuffix::USize },
                 Tok::Eof]);
    }

    #[test]
    fn float_vs_range_disambiguation() {
        assert_eq!(kinds("1..5 0..=n 2.5 2.5f32 1e3"),
            vec![Tok::Int { text: "1".into(), suffix: NumSuffix::None }, Tok::DotDot,
                 Tok::Int { text: "5".into(), suffix: NumSuffix::None },
                 Tok::Int { text: "0".into(), suffix: NumSuffix::None }, Tok::DotDotEq,
                 Tok::Ident("n".into()),
                 Tok::Float { text: "2.5".into(), suffix: NumSuffix::None },
                 Tok::Float { text: "2.5".into(), suffix: NumSuffix::F32 },
                 Tok::Float { text: "1e3".into(), suffix: NumSuffix::None },
                 Tok::Eof]);
    }

    #[test]
    fn method_call_on_int_literal_is_not_float() {
        assert_eq!(kinds("21.double()"),
            vec![Tok::Int { text: "21".into(), suffix: NumSuffix::None }, Tok::Dot,
                 Tok::Ident("double".into()), Tok::LParen, Tok::RParen, Tok::Eof]);
    }

    #[test]
    fn string_escapes() {
        assert_eq!(kinds("\"a\\nb\" \"\\{\" \"\\u{4E2D}\" \"\\\\\" \"\\\"\""),
            vec![Tok::Str { parts: vec![StrPart::Text("a\nb".into())] },
                 Tok::Str { parts: vec![StrPart::Text("{".into())] },
                 Tok::Str { parts: vec![StrPart::Text("中".into())] },
                 Tok::Str { parts: vec![StrPart::Text("\\".into())] },
                 Tok::Str { parts: vec![StrPart::Text("\"".into())] },
                 Tok::Eof]);
    }

    #[test]
    fn string_interpolation_raw_parts() {
        assert_eq!(kinds("\"hi {name}\" \"len={xs.len} first={xs[0]}\" \"v={opt.or(0)}\""),
            vec![Tok::Str { parts: vec![StrPart::Text("hi ".into()), StrPart::Interp("name".into())] },
                 Tok::Str { parts: vec![StrPart::Text("len=".into()), StrPart::Interp("xs.len".into()),
                                       StrPart::Text(" first=".into()), StrPart::Interp("xs[0]".into())] },
                 Tok::Str { parts: vec![StrPart::Text("v=".into()), StrPart::Interp("opt.or(0)".into())] },
                 Tok::Eof]);
    }

    #[test]
    fn bare_brace_without_closing_reports_e1001() {
        // §1.4:字面 '{' 必须写 `\{`(锚定 tests/01d_strings.ct assert_eq("\{", "\{"))
        // 行内无配对 '}' 的裸 '{' 报 1 条 E1001"未终止的插值";同行有配对时仍是插值,行为不变
        let (_, diags) = lex("\"{\"");
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "E1001");
        assert_eq!(diags[0].message, "未终止的插值");
        let (_, diags) = lex("\"a{b\"");
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "E1001");
        assert_eq!(kinds("\"len={xs.len}\""),
            vec![Tok::Str { parts: vec![StrPart::Text("len=".into()), StrPart::Interp("xs.len".into())] },
                 Tok::Eof]);
    }

    #[test]
    fn unterminated_string_reports_e1001() {
        let (_, diags) = lex("\"abc");
        assert_eq!(diags[0].code, "E1001");
        let (_, diags) = lex("\"a\\q\""); // 非法转义
        assert_eq!(diags[0].code, "E1001");
    }

    #[test]
    fn newline_rules_1_6() {
        // 首点式:换行被吞,是一个表达式
        assert_eq!(kinds("opt\n    .map(f)\n    .or(0)"),
            vec![Tok::Ident("opt".into()), Tok::Dot, Tok::Ident("map".into()),
                 Tok::LParen, Tok::Ident("f".into()), Tok::RParen,
                 Tok::Dot, Tok::Ident("or".into()), Tok::LParen, Tok::Int { text: "0".into(), suffix: NumSuffix::None }, Tok::RParen,
                 Tok::Eof]);
        // 行尾运算符:换行被吞
        assert_eq!(kinds("let x = 1 +\n    2"),
            vec![Tok::Let, Tok::Ident("x".into()), Tok::Assign,
                 Tok::Int { text: "1".into(), suffix: NumSuffix::None }, Tok::Plus,
                 Tok::Int { text: "2".into(), suffix: NumSuffix::None }, Tok::Eof]);
        // 普通语句:换行保留;连续空行折叠为一个
        assert_eq!(kinds("let a = 1\n\n\nlet b = 2"),
            vec![Tok::Let, Tok::Ident("a".into()), Tok::Assign,
                 Tok::Int { text: "1".into(), suffix: NumSuffix::None }, Tok::Newline,
                 Tok::Let, Tok::Ident("b".into()), Tok::Assign,
                 Tok::Int { text: "2".into(), suffix: NumSuffix::None }, Tok::Eof]);
        // } else { 同行:行尾是 },换行语义留给 parser;此处验证 token 序列
        assert_eq!(kinds("} else {"),
            vec![Tok::RBrace, Tok::Else, Tok::LBrace, Tok::Eof]);
    }

    #[test]
    fn backslash_eof_reports_e1001() {
        // 反斜杠后即文件尾:按未终止字符串处理(锚定该路径)
        let (_, diags) = lex("\"a\\");
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "E1001");
    }

    #[test]
    fn numeric_suffix_is_case_sensitive() {
        // 大写后缀不复认:255U8 回落为 Int("255") + Ident("U8"),无诊断
        assert_eq!(kinds("255U8"),
            vec![Tok::Int { text: "255".into(), suffix: NumSuffix::None },
                 Tok::Ident("U8".into()), Tok::Eof]);
        let (_, diags) = lex("255U8");
        assert!(diags.is_empty());
        // 小写后缀不受影响
        assert_eq!(kinds("255u8"),
            vec![Tok::Int { text: "255".into(), suffix: NumSuffix::U8 }, Tok::Eof]);
    }

    #[test]
    fn uppercase_radix_prefix_is_not_recognized() {
        // 大写进制前缀不复认:0XFF → Int("0") + Ident("XFF")
        assert_eq!(kinds("0XFF"),
            vec![Tok::Int { text: "0".into(), suffix: NumSuffix::None },
                 Tok::Ident("XFF".into()), Tok::Eof]);
        // 小写前缀不受影响
        assert_eq!(kinds("0xFF 0o17 0b1010"),
            vec![Tok::Int { text: "0xFF".into(), suffix: NumSuffix::None },
                 Tok::Int { text: "0o17".into(), suffix: NumSuffix::None },
                 Tok::Int { text: "0b1010".into(), suffix: NumSuffix::None },
                 Tok::Eof]);
    }

    #[test]
    fn unicode_escape_overflow_reports_e1001() {
        // \u{FFFFFFFFF} 超出 u32:恰 1 条 E1001"非法的 Unicode 转义",不再静默替换
        let (_, diags) = lex("\"\\u{FFFFFFFFF}\"");
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "E1001");
        assert_eq!(diags[0].message, "非法的 Unicode 转义");
    }

    #[test]
    fn interpolation_does_not_pair_across_strings() {
        // 跨串配对:插值扫描遇 '"' 视为未终止 → 恰 1 条 E1001,产出两个独立 Str(不再融合为一串)
        let (toks, diags) = lex("f(\"x{y\",\"z}\")");
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "E1001");
        assert_eq!(diags[0].message, "未终止的插值");
        assert_eq!(toks.into_iter().map(|t| t.tok).collect::<Vec<_>>(),
            vec![Tok::Ident("f".into()), Tok::LParen,
                 Tok::Str { parts: vec![StrPart::Text("x".into())] },
                 Tok::Comma,
                 Tok::Str { parts: vec![StrPart::Text("z}".into())] },
                 Tok::RParen, Tok::Eof]);
    }
}
