//! ctron fmt(R-P2d):token 流重排,不动 AST(roadmap 钉死的载体层)。
//!
//! 布局权威是**原始源码间隙**而非过滤流:§1.6 过滤器会滤除 `{`/`,` 之后的换行,
//! 过滤流丢失块的多行性;原始间隙的自由换行数(扣除注释占据的换行)统一驱动:
//!   - 换行:free ≥ 1 → 换行;free ≥ 2 → 换行 + 空行(至多 1 行,折叠);
//!   - 链断行:Dot 前自由换行 → 换行 + 相对缩进(重词法化时被 §1.6 滤除,流不变);
//!   - 块多行性:匹配大括号之间的原始间隙含换行 → 拆行布局;否则内联(`{ x }`/`{}`)。
//! 注释从 token span 间隙回收逐字回贴;字面量按 span 源切片输出。
//! 验收 = 幂等 + AST 等价 + 行为矩阵(tests/fmt_suite.rs);规范文本见 docs/fmt-spec.md。

use crate::token::{Tok, Token};

#[derive(Debug, Clone)]
struct Comment {
    start: usize,
    /// 注释文本终点(不含行尾换行;换行留给统一的间隙计数)
    end: usize,
    text: String,
}

/// token 流序列化器;词法诊断非空时为 Err(fmt 只服务合法语法面)
pub fn fmt_src(src: &str) -> Result<String, String> {
    let (toks, diags) = crate::lex(src);
    if !diags.is_empty() {
        let mut msg = String::new();
        for d in &diags {
            msg.push_str(&format!("{}:{} {}: {}\n", d.span.line, d.span.col, d.code, d.message));
        }
        return Err(msg);
    }
    let comments = extract_comments(src, &toks);
    let partner = match_braces(&toks);
    let mut em = Emitter {
        src,
        toks: &toks,
        comments,
        partner,
        out: String::new(),
        indent: 0,
        at_line_start: true,
        line_indent_extra: 0,
        last_end: 0,
        prev_real: Vec::new(),
        pipe_open: false,
        sign_unary: false,
    };
    em.run();
    // 规范:无前导空行,文件以单个换行结束
    let mut out = em.out.trim_start_matches('\n').trim_end_matches(['\n', ' ', '\t']).to_string();
    if !out.is_empty() { out.push('\n'); }
    Ok(out)
}

fn extract_comments(src: &str, toks: &[Token]) -> Vec<Comment> {
    let bytes = src.as_bytes();
    let mut out = Vec::new();
    let mut gaps: Vec<(usize, usize)> = Vec::new();
    let mut prev_end = 0usize;
    for t in toks {
        if t.span.start > prev_end { gaps.push((prev_end, t.span.start)); }
        prev_end = t.span.end.max(prev_end);
    }
    if prev_end < bytes.len() { gaps.push((prev_end, bytes.len())); }
    for (gs, ge) in gaps {
        let mut i = gs;
        while i + 1 < ge {
            if bytes[i] == b'/' && bytes[i + 1] == b'/' {
                let mut j = i + 2;
                while j < ge && bytes[j] != b'\n' { j += 1; }
                let text_end = j;
                out.push(Comment { start: i, end: text_end, text: src[i..text_end].trim_end().to_string() });
                i = text_end.max(i + 2);
            } else {
                i += 1;
            }
        }
    }
    out
}

/// partner[i] = 与 i 配对的大括号下标(未配对为 None)
fn match_braces(toks: &[Token]) -> Vec<Option<usize>> {
    let mut partner: Vec<Option<usize>> = vec![None; toks.len()];
    let mut stack: Vec<usize> = Vec::new();
    for (i, t) in toks.iter().enumerate() {
        match t.tok {
            Tok::LBrace => stack.push(i),
            Tok::RBrace => {
                if let Some(open) = stack.pop() {
                    partner[open] = Some(i);
                    partner[i] = Some(open);
                }
            }
            _ => {}
        }
    }
    partner
}

fn operand_end(t: Option<&Tok>) -> bool {
    match t {
        None => false,
        Some(t) => matches!(t, Tok::Int { .. } | Tok::Float { .. } | Tok::Str { .. }
            | Tok::Ident(_) | Tok::RParen | Tok::RBracket | Tok::Question
            | Tok::True | Tok::False | Tok::Underscore | Tok::RBrace | Tok::SelfKw),
    }
}

fn tok_text(t: &Tok) -> &'static str {
    match t {
        Tok::Fn => "fn", Tok::Let => "let", Tok::Var => "var", Tok::Const => "const",
        Tok::Static => "static", Tok::Comptime => "comptime", Tok::If => "if", Tok::Else => "else",
        Tok::Match => "match", Tok::While => "while", Tok::For => "for", Tok::In => "in",
        Tok::Break => "break", Tok::Continue => "continue",
        Tok::Return => "return", Tok::Struct => "struct", Tok::Class => "class", Tok::Enum => "enum",
        Tok::Trait => "trait", Tok::Impl => "impl", Tok::Own => "own", Tok::Scope => "scope",
        Tok::Test => "test", Tok::Use => "use", Tok::Pub => "pub", Tok::Extern => "extern",
        Tok::Prop => "prop", Tok::True => "true", Tok::False => "false", Tok::Void => "void",
        Tok::SelfKw => "self",
        Tok::Plus => "+", Tok::Minus => "-", Tok::Star => "*", Tok::Slash => "/", Tok::Percent => "%",
        Tok::WrapPlus => "+%", Tok::WrapMinus => "-%",
        Tok::PlusEq => "+=", Tok::MinusEq => "-=", Tok::StarEq => "*=", Tok::SlashEq => "/=",
        Tok::PercentEq => "%=",
        Tok::EqEq => "==", Tok::NotEq => "!=", Tok::Lt => "<", Tok::Gt => ">",
        Tok::LtEq => "<=", Tok::GtEq => ">=", Tok::Assign => "=",
        Tok::AndAnd => "&&", Tok::OrOr => "||", Tok::Or => "or",
        Tok::DotDot => "..", Tok::DotDotEq => "..=", Tok::Arrow => "->", Tok::FatArrow => "=>",
        Tok::Question => "?", Tok::Dot => ".", Tok::Comma => ",", Tok::Colon => ":",
        Tok::LBracket => "[", Tok::RBracket => "]", Tok::LParen => "(", Tok::RParen => ")",
        Tok::LBrace => "{", Tok::RBrace => "}", Tok::Pipe => "|", Tok::Amp => "&",
        Tok::Hash => "#", Tok::At => "@", Tok::Underscore => "_", Tok::Bang => "!",
        _ => "",
    }
}

struct Emitter<'a> {
    src: &'a str,
    toks: &'a [Token],
    comments: Vec<Comment>,
    partner: Vec<Option<usize>>,
    out: String,
    indent: usize,
    at_line_start: bool,
    /// 行内相对缩进(链断行),换行时清零
    line_indent_extra: usize,
    /// 上一个已发射内容的终点(实记号 span.end 或注释 end;行尾换行不计入)
    last_end: usize,
    /// 实记号历史栈尾(一元判定)
    prev_real: Vec<Tok>,
    pipe_open: bool,
    /// 上一个 +/- 是否按一元发射(决定其后紧贴)
    sign_unary: bool,
}

impl<'a> Emitter<'a> {
    fn ind_str(&self) -> String {
        "    ".repeat(self.indent + self.line_indent_extra)
    }

    fn write(&mut self, s: &str) {
        if self.at_line_start {
            self.out.push_str(&self.ind_str());
            self.at_line_start = false;
        }
        self.out.push_str(s);
    }

    fn write_sep(&mut self, s: &str, space_before: bool) {
        if !self.at_line_start && space_before {
            self.out.push(' ');
        }
        self.write(s);
    }

    fn newline(&mut self, blank: bool) {
        self.out.push('\n');
        if blank { self.out.push('\n'); }
        self.at_line_start = true;
        self.line_indent_extra = 0;
    }

    /// [from, to) 内自由换行数(扣除注释占据的换行;注释 end 不含行尾换行)
    fn free_newlines(&self, from: usize, to: usize) -> usize {
        if to <= from { return 0; }
        let bytes = self.src.as_bytes();
        let mut count = 0usize;
        for i in from..to.min(bytes.len()) {
            if bytes[i] != b'\n' { continue; }
            if self.comments.iter().any(|c| i >= c.start && i < c.end) { continue; }
            count += 1;
        }
        count
    }

    fn prev(&self) -> Option<&Tok> { self.prev_real.last() }

    fn block_multiline(&self, open_idx: usize) -> bool {
        match self.partner[open_idx] {
            Some(close) => {
                let (s, e) = (self.toks[open_idx].span.end, self.toks[close].span.start);
                self.src.as_bytes()[s..e].contains(&b'\n')
            }
            None => false,
        }
    }

    fn run(&mut self) {
        for i in 0..self.toks.len() {
            let span = self.toks[i].span;
            let tok = self.toks[i].tok.clone();
            if tok == Tok::Newline || tok == Tok::Eof {
                // 过滤流中的换行不是布局权威(原始间隙统一处理);Eof 只负责尾部注释
                if tok == Tok::Eof { self.emit_comments_before(span.start); }
                continue;
            }
            self.emit_comments_before(span.start);
            if tok != Tok::Else && tok != Tok::Dot {
                self.apply_gap_break(span.start);
            }
            match tok {
                Tok::LBrace => {
                    let ml = self.block_multiline(i);
                    let space = self.needs_space(&tok);
                    self.write_sep("{", space);
                    if ml {
                        self.indent += 1;
                        self.newline(false);
                    }
                    self.push_real(tok, span.end);
                }
                Tok::RBrace => {
                    let ml = self.partner[i].map(|o| self.block_multiline(o)).unwrap_or(false);
                    // 前隙换行已由 apply_gap_break 统一落下;此处只负责降缩进
                    if ml { self.indent = self.indent.saturating_sub(1); }
                    let space = !ml && self.needs_space(&tok);
                    self.write_sep("}", space);
                    self.push_real(tok, span.end);
                }
                Tok::Else => {
                    // `} else` 权威同行:前隙换行一律吞掉
                    self.write_sep("else", true);
                    self.push_real(tok, span.end);
                }
                Tok::Pipe => {
                    if self.pipe_open {
                        self.write_sep("|", false); // 闭包参数收尾紧贴
                        self.pipe_open = false;
                    } else {
                        let tight = matches!(self.prev(), Some(Tok::Comma) | Some(Tok::LParen)
                            | Some(Tok::Ident(_)) | Some(Tok::RParen) | Some(Tok::LBracket));
                        self.write_sep("|", !tight);
                        self.pipe_open = true;
                    }
                    self.push_real(tok, span.end);
                }
                Tok::Dot => {
                    // 链断行(R4 v1 定版):自由换行 → 换行 + 相对缩进 1 级,空行折叠沿用 R1;
                    // 补插换行重词法化时被 §1.6 滤除,流不变、天然幂等。
                    // 此前统一间隙换行先行,本分支不可达,规范形态退化为同缩进——本次按 spec 定版。
                    let free = self.free_newlines(self.last_end, span.start);
                    if free >= 1 {
                        if !self.at_line_start {
                            self.newline(free >= 2);
                        } else if free >= 2 {
                            self.out.push('\n');
                        }
                        self.line_indent_extra = 1;
                    }
                    self.write_sep(".", false);
                    self.push_real(tok, span.end);
                }
                Tok::Minus | Tok::Plus => {
                    self.sign_unary = !operand_end(self.prev());
                    let space = self.needs_space(&tok);
                    self.write_sep(tok_text(&tok), space);
                    self.push_real(tok, span.end);
                }
                _ => {
                    let text: String = match &tok {
                        Tok::Int { .. } | Tok::Float { .. } | Tok::Str { .. } =>
                            self.src[span.start..span.end].to_string(),
                        Tok::Ident(s) => s.clone(),
                        t => tok_text(t).to_string(),
                    };
                    let space = self.needs_space(&tok);
                    self.write_sep(&text, space);
                    self.push_real(tok, span.end);
                }
            }
        }
    }

    /// 统一间隙换行:free ≥ 1 换行,free ≥ 2 加空行(至多一行);
    /// 行首状态(前一内容已自终止)时只补空行差额
    fn apply_gap_break(&mut self, cur_start: usize) {
        let free = self.free_newlines(self.last_end, cur_start);
        if free >= 1 {
            if !self.at_line_start {
                self.newline(free >= 2);
            } else if free >= 2 {
                self.out.push('\n');
            }
        }
    }

    fn push_real(&mut self, tok: Tok, end: usize) {
        self.prev_real.push(tok);
        if self.prev_real.len() > 4 { self.prev_real.remove(0); }
        self.last_end = end;
    }

    /// 空格表:是否在当前记号前补一个空格(行首缩进由 write 处理)
    fn needs_space(&self, cur: &Tok) -> bool {
        use Tok::*;
        let Some(prev) = self.prev() else { return false };
        // cur 侧紧贴
        if matches!(cur, Comma | Colon | RParen | RBracket | Dot | Question) { return false; }
        if matches!(cur, RBrace) && matches!(prev, LBrace) { return false; } // 空块 {}
        if matches!(cur, LBracket) && matches!(prev, Ident(_) | RParen | RBracket | Question | Hash) { return false; }
        if matches!(cur, LParen) && matches!(prev, Ident(_) | RParen | RBracket | Question | SelfKw) { return false; }
        // 前缀 ! & 的紧贴只在其右侧(prev 侧);左侧恒空格,防 `return!(...)`/`&&!b`
        // prev 侧紧贴
        if matches!(prev, LParen | LBracket | Dot | At | Hash | Question) { return false; }
        if matches!(prev, Bang | Amp) { return false; }
        if matches!(prev, Minus | Plus) && self.sign_unary { return false; }
        if matches!(prev, Pipe) && self.pipe_open { return false; } // 闭包参数起点
        // range 无空格
        if matches!(prev, DotDot | DotDotEq) || matches!(cur, DotDot | DotDotEq) { return false; }
        true
    }

    /// 发射位于 `before` 之前的待处理注释:
    /// 前置自由换行先落(含空行折叠);独占行注释自终止(落行尾换行),行尾注释挂当前行
    fn emit_comments_before(&mut self, before: usize) {
        loop {
            let Some(pos) = self.comments.iter().position(|c| c.start < before) else { break };
            let c = self.comments.remove(pos);
            let pre_free = self.free_newlines(self.last_end, c.start);
            if pre_free >= 1 {
                // 独占行注释(前置有换行)
                if !self.at_line_start {
                    self.newline(pre_free >= 2);
                } else if pre_free >= 2 {
                    self.out.push('\n');
                }
                self.write(&c.text);
                self.out.push('\n');
                self.at_line_start = true;
            } else {
                // 行尾注释:挂当前行
                if !self.at_line_start { self.out.push(' '); }
                self.write(&c.text);
            }
            self.last_end = c.end;
        }
    }
}
