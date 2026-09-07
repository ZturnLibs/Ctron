//! 递归下降解析器:规范 §1.7 EBNF + §1.8 消歧 + §4 优先级。
//! 诊断码:E1001(语法错误)/ E3030(static var 恢复,§10.1)。

use crate::ast::*;
use crate::token::{Diagnostic, NumSuffix, Span, StrPart as TokStrPart, Tok, Token};

pub struct Parser {
    toks: Vec<Token>,
    pos: usize,
    diags: Vec<Diagnostic>,
    eof: Token,
    depth: u32,
}

const MAX_EXPR_DEPTH: u32 = 256;

pub fn parse_tokens(toks: Vec<Token>, mut diags: Vec<Diagnostic>) -> (File, Vec<Diagnostic>) {
    let mut p = Parser::new(toks);
    p.diags.append(&mut diags);
    let file = p.parse_file();
    (file, p.diags)
}

impl Parser {
    pub fn new(toks: Vec<Token>) -> Self {
        let eof = Token { tok: Tok::Eof, span: Span::new(0, 0, 0, 0) };
        Parser { toks, pos: 0, diags: Vec::new(), eof, depth: 0 }
    }

    // ---------- 游标原语 ----------

    fn tok_at(&self, n: usize) -> &Tok {
        self.toks.get(self.pos + n).map(|t| &t.tok).unwrap_or(&self.eof.tok)
    }
    fn peek(&self) -> &Tok { self.tok_at(0) }
    fn peek2(&self) -> &Tok { self.tok_at(1) }
    fn cur_span(&self) -> Span {
        self.toks.get(self.pos).map(|t| t.span).unwrap_or(self.eof.span)
    }
    fn bump(&mut self) -> Token {
        let t = self.toks.get(self.pos).cloned().unwrap_or_else(|| self.eof.clone());
        if self.pos < self.toks.len() { self.pos += 1; }
        t
    }
    fn at(&self, t: &Tok) -> bool { self.peek() == t }
    fn eat(&mut self, t: &Tok) -> bool { if self.at(t) { self.bump(); true } else { false } }
    fn err_here(&mut self, code: &'static str, msg: String) {
        let span = self.cur_span();
        self.diags.push(Diagnostic { code, message: msg, span });
    }
    fn expect(&mut self, t: &Tok, ctx: &str) -> Span {
        if self.at(t) { return self.bump().span; }
        let msg = format!("预期 {},实际 {:?} — {}", tok_display(t), self.peek(), ctx);
        self.err_here("E1001", msg);
        self.cur_span()
    }
    fn skip_newlines(&mut self) { while self.at(&Tok::Newline) { self.bump(); } }

    /// 循环停滞守卫:一轮未推进则报错并强制消费,保证任何错误路径都前进。
    fn ensure_progress(&mut self, before: usize) {
        if self.pos == before {
            self.err_here("E1001", "无法解析的语法元素".into());
            self.bump();
        }
    }

    /// 跳过换行后的第一个记号(不改游标)。
    fn lookahead_past_newlines(&self) -> &Tok {
        let mut i = self.pos;
        while matches!(self.toks.get(i).map(|t| &t.tok), Some(Tok::Newline)) { i += 1; }
        self.toks.get(i).map(|t| &t.tok).unwrap_or(&self.eof.tok)
    }

    // ---------- 文件与声明 ----------

    fn parse_file(&mut self) -> File {
        self.skip_newlines();
        let mut decls = Vec::new();
        loop {
            if self.at(&Tok::Eof) { break; }
            if self.at(&Tok::Newline) { self.bump(); continue; }
            let (attrs, derives) = self.parse_attrs();
            self.skip_newlines();
            if (!attrs.is_empty() || !derives.is_empty())
                && matches!(self.peek(), Tok::Hash | Tok::At | Tok::Eof)
            {
                self.err_here("E1001", "属性后缺少声明".into());
                continue;
            }
            if let Some(d) = self.parse_decl(attrs, derives) { decls.push(d); }
            self.skip_newlines();
        }
        File { decls }
    }

    fn parse_attrs(&mut self) -> (Vec<Attribute>, Vec<String>) {
        let (mut attrs, mut derives) = (Vec::new(), Vec::new());
        loop {
            if self.at(&Tok::Hash) {
                self.bump();
                self.expect(&Tok::LBracket, "#[");
                let name = match self.peek().clone() {
                    Tok::Ident(s) => { self.bump(); s }
                    other => {
                        self.err_here("E1001", format!("预期注解名,实际 {:?}", other));
                        String::new()
                    }
                };
                let mut args = Vec::new();
                if self.eat(&Tok::LParen) {
                    loop {
                        if self.at(&Tok::RParen) { break; }
                        args.push(self.raw_arg());
                        if !self.eat(&Tok::Comma) { break; }
                    }
                    self.expect(&Tok::RParen, "注解实参表");
                }
                self.expect(&Tok::RBracket, "注解结束");
                attrs.push(Attribute { name, args });
            } else if self.at(&Tok::At) {
                self.bump();
                let name = match self.peek().clone() {
                    Tok::Ident(s) => { self.bump(); s }
                    other => {
                        self.err_here("E1001", format!("预期 derive,实际 {:?}", other));
                        String::new()
                    }
                };
                if name != "derive" {
                    self.err_here("E1001", format!("未知属性 @{}", name));
                }
                self.expect(&Tok::LParen, "@derive(");
                loop {
                    if self.at(&Tok::RParen) { break; }
                    let path = self.parse_dotted_path();
                    derives.push(path.join("."));
                    if !self.eat(&Tok::Comma) { break; }
                }
                self.expect(&Tok::RParen, "@derive 结束");
            } else {
                break;
            }
        }
        (attrs, derives)
    }

    /// 原始实参文本(注解用;不支持嵌套括号内的复杂表达式)。
    fn raw_arg(&mut self) -> String {
        let mut out = String::new();
        loop {
            match self.peek().clone() {
                Tok::Ident(s) => { self.bump(); out.push_str(&s); }
                Tok::Int { text, .. } => { self.bump(); out.push_str(&text); }
                Tok::Dot => { self.bump(); out.push('.'); continue; }
                _ => break,
            }
            if self.at(&Tok::Dot) { continue; }
            break;
        }
        out
    }

    fn parse_dotted_path(&mut self) -> Vec<String> {
        let mut path = Vec::new();
        match self.peek().clone() {
            Tok::Ident(s) => { self.bump(); path.push(s); }
            other => {
                self.err_here("E1001", format!("预期路径,实际 {:?}", other));
                return path;
            }
        }
        while self.at(&Tok::Dot) {
            self.bump();
            match self.peek().clone() {
                Tok::Ident(s) => { self.bump(); path.push(s); }
                other => {
                    self.err_here("E1001", format!("路径段缺失,实际 {:?}", other));
                    break;
                }
            }
        }
        path
    }

    fn parse_vis(&mut self) -> Vis {
        if self.at(&Tok::Pub) {
            self.bump();
            if self.at(&Tok::LParen) && matches!(self.peek2(), Tok::Ident(s) if s == "pkg") {
                self.bump(); self.bump();
                self.expect(&Tok::RParen, "pub(pkg)");
                return Vis::PubPkg;
            }
            return Vis::Pub;
        }
        Vis::Private
    }

    fn parse_decl(&mut self, attrs: Vec<Attribute>, derives: Vec<String>) -> Option<Decl> {
        match self.peek().clone() {
            Tok::Use => Some(Decl::Use(self.parse_use())),
            Tok::Struct => Some(Decl::Struct(self.parse_struct(attrs, derives))),
            Tok::Class => Some(Decl::Class(self.parse_class(attrs, derives))),
            Tok::Enum => Some(Decl::Enum(self.parse_enum(attrs, derives))),
            Tok::Trait => Some(Decl::Trait(self.parse_trait(attrs))),
            Tok::Impl => Some(Decl::Impl(self.parse_impl())),
            Tok::Const => Some(Decl::Const(self.parse_const())),
            Tok::Static => Some(Decl::Static(self.parse_static())),
            Tok::Test => Some(Decl::Test(self.parse_test())),
            Tok::Fn | Tok::Pub | Tok::Comptime | Tok::Extern => Some(Decl::Fn(self.parse_fn(attrs, true))),
            other => {
                self.err_here("E1001", format!("顶层应为声明,实际 {:?}", other));
                self.bump();
                None
            }
        }
    }

    fn parse_use(&mut self) -> UseDecl {
        self.bump(); // use
        let mut prefix = Vec::new();
        let mut group = false;
        loop {
            match self.peek().clone() {
                Tok::Ident(seg) => { self.bump(); prefix.push(seg); }
                _ => break,
            }
            if self.at(&Tok::Dot) && matches!(self.peek2(), Tok::LBrace) {
                self.bump(); // 消费组导入前的 Dot
                group = true;
                break;
            }
            if !self.eat(&Tok::Dot) { break; }
        }
        let mut imports = Vec::new();
        if group {
            self.expect(&Tok::LBrace, "use 组");
            loop {
                if self.at(&Tok::RBrace) { break; }
                let mut full = prefix.clone();
                let seg = self.parse_dotted_path();
                full.extend(seg);
                imports.push(full);
                if !self.eat(&Tok::Comma) { break; }
            }
            self.expect(&Tok::RBrace, "use 组结束");
        } else {
            imports.push(std::mem::take(&mut prefix));
        }
        UseDecl { imports }
    }

    fn parse_type_params(&mut self) -> Vec<TypeParam> {
        let mut out = Vec::new();
        if !self.eat(&Tok::LBracket) { return out; }
        loop {
            if self.at(&Tok::RBracket) { break; }
            if self.eat(&Tok::Comptime) {
                let name = self.expect_ident("comptime 参数");
                self.expect(&Tok::Colon, "comptime 参数");
                let ty = self.parse_type();
                out.push(TypeParam { name, bound: Vec::new(), is_comptime: true });
                let _ = ty;
            } else {
                let name = self.expect_ident("类型参数");
                let mut bound = Vec::new();
                if self.eat(&Tok::Colon) {
                    loop {
                        let b = self.parse_dotted_path();
                        bound.push(b.join("."));
                        if !self.eat(&Tok::Plus) { break; }
                    }
                }
                out.push(TypeParam { name, bound, is_comptime: false });
            }
            if !self.eat(&Tok::Comma) { break; }
        }
        self.expect(&Tok::RBracket, "类型参数表结束");
        out
    }

    fn expect_ident(&mut self, ctx: &str) -> String {
        match self.peek().clone() {
            Tok::Ident(s) => { self.bump(); s }
            other => {
                self.err_here("E1001", format!("预期标识符({}),实际 {:?}", ctx, other));
                String::new()
            }
        }
    }

    fn parse_field(&mut self, vis: Vis) -> Field {
        let mut is_var = false;
        if self.eat(&Tok::Let) {}
        else if self.eat(&Tok::Var) { is_var = true; }
        let name = self.expect_ident("字段");
        self.expect(&Tok::Colon, "字段");
        let ty = self.parse_type();
        Field { vis, is_var, name, ty }
    }

    fn parse_struct(&mut self, attrs: Vec<Attribute>, derives: Vec<String>) -> StructDecl {
        self.bump();
        let name = self.expect_ident("结构体");
        let type_params = self.parse_type_params();
        self.expect(&Tok::LBrace, "结构体体");
        let mut fields = Vec::new();
        loop {
            self.skip_newlines();
            if self.at(&Tok::RBrace) { self.bump(); break; }
            if self.at(&Tok::Eof) { self.err_here("E1001", "未闭合的结构体体".into()); break; }
            let before = self.pos;
            let vis = self.parse_vis();
            fields.push(self.parse_field(vis));
            self.ensure_progress(before);
            self.eat(&Tok::Comma);
        }
        StructDecl { attrs, derives, vis: Vis::Private, name, type_params, fields }
    }

    fn parse_enum(&mut self, attrs: Vec<Attribute>, derives: Vec<String>) -> EnumDecl {
        self.bump();
        let name = self.expect_ident("枚举");
        let type_params = self.parse_type_params();
        self.expect(&Tok::LBrace, "枚举体");
        let mut variants = Vec::new();
        loop {
            self.skip_newlines();
            if self.at(&Tok::RBrace) { self.bump(); break; }
            if self.at(&Tok::Eof) { self.err_here("E1001", "未闭合的枚举体".into()); break; }
            let before = self.pos;
            let vname = self.expect_ident("枚举变体");
            let kind = if self.eat(&Tok::LParen) {
                let mut tys = Vec::new();
                loop {
                    if self.at(&Tok::RParen) { break; }
                    tys.push(self.parse_type());
                    if !self.eat(&Tok::Comma) { break; }
                }
                self.expect(&Tok::RParen, "变体载荷");
                VariantKind::Tuple(tys)
            } else if self.eat(&Tok::LBrace) {
                let mut fields = Vec::new();
                loop {
                    self.skip_newlines();
                    if self.at(&Tok::RBrace) { self.bump(); break; }
                    if self.at(&Tok::Eof) { self.err_here("E1001", "未闭合的变体字段".into()); break; }
                    let before = self.pos;
                    fields.push(self.parse_field(Vis::Private));
                    self.ensure_progress(before);
                    self.eat(&Tok::Comma);
                }
                VariantKind::Struct(fields)
            } else {
                VariantKind::Unit
            };
            variants.push(Variant { name: vname, kind });
            self.ensure_progress(before);
            self.eat(&Tok::Comma);
        }
        EnumDecl { attrs, derives, vis: Vis::Private, name, type_params, variants }
    }

    fn parse_class(&mut self, attrs: Vec<Attribute>, _derives: Vec<String>) -> ClassDecl {
        self.bump();
        let name = self.expect_ident("类");
        let type_params = self.parse_type_params();
        self.expect(&Tok::LBrace, "类体");
        let mut items = Vec::new();
        loop {
            self.skip_newlines();
            if self.at(&Tok::RBrace) { self.bump(); break; }
            if self.at(&Tok::Eof) { self.err_here("E1001", "未闭合的类体".into()); break; }
            let mattrs = if self.at(&Tok::Hash) || self.at(&Tok::At) {
                let (mattrs, _) = self.parse_attrs();
                self.skip_newlines();
                mattrs
            } else { Vec::new() };
            let before = self.pos;
            let vis = self.parse_vis();
            if self.at(&Tok::Prop) {
                items.push(ClassItem::Prop(self.parse_prop_decl(mattrs, vis)));
            } else if self.at(&Tok::Fn) {
                let mut f = self.parse_fn(mattrs, false);
                f.vis = vis;
                items.push(ClassItem::Method(f));
            } else {
                items.push(ClassItem::Field(self.parse_field(vis)));
            }
            self.ensure_progress(before);
            self.eat(&Tok::Comma);
        }
        ClassDecl { attrs, vis: Vis::Private, name, type_params, items }
    }

    fn parse_prop_decl(&mut self, attrs: Vec<Attribute>, vis: Vis) -> PropDecl {
        let _ = attrs;
        self.bump(); // prop
        let name = self.expect_ident("属性");
        self.expect(&Tok::Colon, "属性");
        let ty = self.parse_type();
        let body = if self.at(&Tok::LBrace) { Some(self.parse_block()) } else { None };
        PropDecl { vis, name, ty, body }
    }

    fn parse_trait(&mut self, attrs: Vec<Attribute>) -> TraitDecl {
        self.bump();
        let name = self.expect_ident("trait");
        let type_params = self.parse_type_params();
        let mut supers = Vec::new();
        if self.eat(&Tok::Colon) {
            loop {
                let s = self.parse_dotted_path();
                supers.push(s.join("."));
                if !self.eat(&Tok::Plus) { break; }
            }
        }
        self.expect(&Tok::LBrace, "trait 体");
        let mut items = Vec::new();
        loop {
            self.skip_newlines();
            if self.at(&Tok::RBrace) { self.bump(); break; }
            if self.at(&Tok::Eof) { self.err_here("E1001", "未闭合的 trait 体".into()); break; }
            let mattrs = self.parse_attrs().0;
            self.skip_newlines();
            let before = self.pos;
            let vis = self.parse_vis();
            if self.at(&Tok::Prop) {
                let p = self.parse_prop_decl(mattrs, vis);
                if p.body.is_some() { items.push(TraitItem::PropImpl(p)); }
                else { items.push(TraitItem::PropSig(p)); }
            } else {
                let mut f = self.parse_fn(mattrs, false);
                f.vis = vis;
                // 无体方法保留 Method(FnDecl{body:None})——签名即契约,不得降格为属性
                items.push(TraitItem::Method(f));
            }
            self.ensure_progress(before);
        }
        TraitDecl { attrs, vis: Vis::Private, name, type_params, supers, items }
    }

    fn parse_impl(&mut self) -> ImplDecl {
        self.bump();
        let type_params = self.parse_type_params();
        let trait_ty = self.parse_type();
        self.expect(&Tok::For, "impl");
        let for_ty = self.parse_type();
        self.expect(&Tok::LBrace, "impl 体");
        let mut items = Vec::new();
        loop {
            self.skip_newlines();
            if self.at(&Tok::RBrace) { self.bump(); break; }
            if self.at(&Tok::Eof) { self.err_here("E1001", "未闭合的 impl 体".into()); break; }
            let mattrs = self.parse_attrs().0;
            self.skip_newlines();
            let vis = self.parse_vis();
            if self.at(&Tok::Prop) {
                items.push(ImplItem::Prop(self.parse_prop_decl(mattrs, vis)));
            } else {
                let mut f = self.parse_fn(mattrs, false);
                f.vis = vis;
                items.push(ImplItem::Method(f));
            }
        }
        ImplDecl { type_params, trait_ty, for_ty, items }
    }

    fn parse_fn(&mut self, attrs: Vec<Attribute>, top_level: bool) -> FnDecl {
        let vis = if top_level { self.parse_vis() } else { Vis::Private };
        let is_comptime = self.eat(&Tok::Comptime);
        let abi = if self.at(&Tok::Extern) {
            self.bump();
            match self.peek().clone() {
                Tok::Str { .. } => {
                    let s = self.take_str_lit();
                    Some(s.iter().filter_map(|p| match p {
                        crate::ast::StrPart::Text(t) => Some(t.clone()),
                        _ => None,
                    }).collect::<Vec<_>>().join(""))
                }
                _ => {
                    self.expect(&Tok::Str { parts: Vec::new() }, "extern ABI 字符串");
                    None
                }
            }
        } else {
            None
        };
        self.expect(&Tok::Fn, "函数");
        let name = self.expect_ident("函数名");
        let type_params = self.parse_type_params();
        self.expect(&Tok::LParen, "参数表");
        let mut params = Vec::new();
        loop {
            if self.at(&Tok::RParen) { break; }
            if self.at(&Tok::Amp) && matches!(self.peek2(), Tok::SelfKw) {
                self.bump(); self.bump();
                params.push(Param::Receiver { is_var: false });
            } else if self.at(&Tok::Var) && matches!(self.peek2(), Tok::SelfKw) {
                self.bump(); self.bump();
                params.push(Param::Receiver { is_var: true });
            } else {
                let is_var = self.eat(&Tok::Var);
                let pname = self.expect_ident("参数");
                self.expect(&Tok::Colon, "参数");
                let ty = self.parse_type();
                params.push(Param::Param { is_var, name: pname, ty });
            }
            if !self.eat(&Tok::Comma) { break; }
        }
        self.expect(&Tok::RParen, "参数表结束");
        let ret = if self.eat(&Tok::Arrow) { Some(self.parse_type()) } else { None };
        let body = if self.at(&Tok::LBrace) { Some(self.parse_block()) } else { None };
        FnDecl { attrs, vis, is_comptime, abi, name, type_params, params, ret, body }
    }

    fn take_str_lit(&mut self) -> Vec<crate::ast::StrPart> {
        match self.peek().clone() {
            Tok::Str { parts } => {
                self.bump();
                parts.into_iter().map(|p| match p {
                    TokStrPart::Text(t) => crate::ast::StrPart::Text(t),
                    TokStrPart::Interp(t) => crate::ast::StrPart::Interp(t),
                }).collect()
            }
            _ => Vec::new(),
        }
    }

    fn parse_const(&mut self) -> ConstDecl {
        self.bump();
        let name = self.expect_ident("常量");
        self.expect(&Tok::Colon, "常量");
        let ty = self.parse_type();
        self.expect(&Tok::Assign, "常量");
        let expr = self.parse_expr();
        ConstDecl { name, ty, expr }
    }

    fn parse_static(&mut self) -> StaticDecl {
        self.bump(); // static
        let was_var = if self.at(&Tok::Var) {
            self.err_here("E3030", "static var 不存在;用 static let 或 Global[T]".into());
            self.bump();
            true
        } else {
            self.expect(&Tok::Let, "static 声明");
            false
        };
        let name = self.expect_ident("静态");
        self.expect(&Tok::Colon, "静态");
        let ty = self.parse_type();
        self.expect(&Tok::Assign, "静态");
        let expr = self.parse_expr();
        StaticDecl { name, ty, expr, was_var }
    }

    fn parse_test(&mut self) -> TestDecl {
        self.bump(); // test
        let name = match self.peek().clone() {
            Tok::Str { parts } => {
                self.bump();
                parts.into_iter().map(|p| match p {
                    TokStrPart::Text(t) => t,
                    TokStrPart::Interp(t) => t,
                }).collect::<Vec<_>>().join("")
            }
            other => {
                self.err_here("E1001", format!("预期测试名字符串,实际 {:?}", other));
                String::new()
            }
        };
        let body = self.parse_block();
        TestDecl { name, body }
    }

    // ---------- 类型 ----------

    pub fn parse_type(&mut self) -> Type {
        self.depth += 1;
        if self.depth > MAX_EXPR_DEPTH {
            self.err_here("E1001", "类型嵌套过深".into());
            self.depth -= 1;
            return Type::SelfT;
        }
        let t = self.parse_type_inner();
        self.depth -= 1;
        t
    }

    fn parse_type_inner(&mut self) -> Type {
        if self.eat(&Tok::Amp) {
            return Type::Ref(Box::new(self.parse_type()));
        }
        let base = self.parse_type_base();
        self.parse_type_postfix(base)
    }

    fn parse_type_base(&mut self) -> Type {
        match self.peek().clone() {
            Tok::SelfKw => { self.bump(); Type::SelfT }
            Tok::Int { text, .. } | Tok::Float { text, .. } => {
                self.bump();
                Type::ComptimeVal(text)
            }
            Tok::Fn => {
                self.bump();
                self.expect(&Tok::LParen, "函数类型");
                let mut params = Vec::new();
                loop {
                    if self.at(&Tok::RParen) { break; }
                    params.push(self.parse_type());
                    if !self.eat(&Tok::Comma) { break; }
                }
                self.expect(&Tok::RParen, "函数类型参数");
                let ret = if self.eat(&Tok::Arrow) { Some(Box::new(self.parse_type())) } else { None };
                Type::Fn { params, ret }
            }
            Tok::LParen => {
                self.bump();
                let mut tys = Vec::new();
                loop {
                    if self.at(&Tok::RParen) { break; }
                    tys.push(self.parse_type());
                    if !self.eat(&Tok::Comma) { break; }
                }
                self.expect(&Tok::RParen, "元组类型");
                Type::Tuple(tys)
            }
            Tok::Ident(_) => {
                let path = self.parse_dotted_path();
                let mut ty = Type::Named { path, args: Vec::new() };
                // 类型位置 [ ] 消歧(§1.8 裁决):空 → 切片(交后缀);
                // 单个整数字面量 → 定长数组 Type [ Expr ];其余 → 泛型类型实参。
                // 残余歧义:T[SIZE](comptime 常量标识符)按泛型解析。
                if self.at(&Tok::LBracket) && !self.bracket_content_is_empty() {
                    if self.bracket_content_is_single_int() {
                        self.bump();
                        let size = self.parse_expr();
                        self.expect(&Tok::RBracket, "定长数组");
                        ty = Type::Array { elem: Box::new(ty), size: Some(size) };
                    } else {
                        self.bump();
                        let mut args = Vec::new();
                        loop {
                            if self.at(&Tok::RBracket) { break; }
                            args.push(self.parse_type());
                            if !self.eat(&Tok::Comma) { break; }
                        }
                        self.expect(&Tok::RBracket, "类型实参");
                        if let Type::Named { args: ref mut old, .. } = ty {
                            *old = args;
                        }
                    }
                }
                ty
            }
            other => {
                self.err_here("E1001", format!("预期类型,实际 {:?}", other));
                self.bump();
                Type::SelfT
            }
        }
    }

    fn parse_type_postfix(&mut self, mut ty: Type) -> Type {
        loop {
            if self.at(&Tok::LBracket) {
                self.bump();
                if self.eat(&Tok::RBracket) {
                    ty = Type::Slice(Box::new(ty));
                } else {
                    let size = if self.at(&Tok::RBracket) { None } else { Some(self.parse_expr()) };
                    self.expect(&Tok::RBracket, "定长数组");
                    ty = Type::Array { elem: Box::new(ty), size };
                }
            } else if self.at(&Tok::Question) {
                self.bump();
                ty = Type::Optional(Box::new(ty));
            } else {
                break;
            }
        }
        ty
    }

    /// `[` 处内容是否为空(`[]`)。
    fn bracket_content_is_empty(&self) -> bool {
        matches!(self.peek2(), Tok::RBracket)
    }

    /// `[` 处内容是否恰为一个整数字面量。
    fn bracket_content_is_single_int(&self) -> bool {
        matches!(self.peek2(), Tok::Int { .. }) && matches!(self.tok_at(2), Tok::RBracket)
    }

    /// `[` 处的快速判定:配对 `]` 之后是否紧跟 `(` 或 `{`(§1.8)。
    fn bracket_followed_by_call_or_lit(&self) -> bool {
        // 仅 ]( 触发(泛型实参 + 构造调用,如 List[I32](…));]/{ 不触发:
        // x[expr] { 是下标后跟块(T1;Pair[T] { } 形态双版语法皆不存在)
        let mut depth = 0i32;
        let mut i = self.pos;
        while i < self.toks.len() {
            match &self.toks[i].tok {
                Tok::LBracket => depth += 1,
                Tok::RBracket => {
                    depth -= 1;
                    if depth == 0 {
                        return matches!(self.toks.get(i + 1).map(|t| &t.tok), Some(Tok::LParen));
                    }
                }
                Tok::Eof => return false,
                _ => {}
            }
            i += 1;
        }
        false
    }

    // ---------- 表达式 ----------

    pub fn parse_expr(&mut self) -> Expr { self.parse_expr_flags(true) }

    fn parse_expr_flags(&mut self, allow_struct: bool) -> Expr { self.parse_or(allow_struct) }

    fn parse_or(&mut self, allow_struct: bool) -> Expr {
        let mut lhs = self.parse_and(allow_struct);
        while self.at(&Tok::Or) {
            self.bump();
            let rhs = self.parse_and(allow_struct);
            lhs = Expr::Binary { op: BinOp::Or, lhs: Box::new(lhs), rhs: Box::new(rhs) };
        }
        lhs
    }

    fn parse_and(&mut self, allow_struct: bool) -> Expr {
        let mut lhs = self.parse_compare(allow_struct);
        while self.at(&Tok::AndAnd) {
            self.bump();
            let rhs = self.parse_compare(allow_struct);
            lhs = Expr::Binary { op: BinOp::AndAnd, lhs: Box::new(lhs), rhs: Box::new(rhs) };
        }
        lhs
    }

    fn parse_compare(&mut self, allow_struct: bool) -> Expr {
        let mut lhs = self.parse_range(allow_struct);
        let mut chained_reported = false;
        loop {
            let op = match self.peek() {
                Tok::EqEq => BinOp::Eq, Tok::NotEq => BinOp::Ne,
                Tok::Lt => BinOp::Lt, Tok::Gt => BinOp::Gt,
                Tok::LtEq => BinOp::Le, Tok::GtEq => BinOp::Ge,
                _ => break,
            };
            self.bump();
            let rhs = self.parse_range(allow_struct);
            lhs = Expr::Binary { op, lhs: Box::new(lhs), rhs: Box::new(rhs) };
            // 比较不可链(§4.3)
            if matches!(self.peek(), Tok::EqEq | Tok::NotEq | Tok::Lt | Tok::Gt | Tok::LtEq | Tok::GtEq) {
                if !chained_reported {
                    self.err_here("E1001", "比较运算符不可链:写 a < b && b < c".into());
                    chained_reported = true;
                }
                continue; // 消费并继续(恢复),只报一次
            }
            break;
        }
        lhs
    }

    fn parse_range(&mut self, allow_struct: bool) -> Expr {
        let from = self.parse_additive(allow_struct);
        if self.at(&Tok::DotDot) || self.at(&Tok::DotDotEq) {
            let inclusive = self.at(&Tok::DotDotEq);
            self.bump();
            let to = self.parse_additive(allow_struct);
            return Expr::Range { inclusive, from: Box::new(from), to: Box::new(to) };
        }
        from
    }

    fn parse_additive(&mut self, allow_struct: bool) -> Expr {
        let mut lhs = self.parse_multiplicative(allow_struct);
        loop {
            let op = match self.peek() {
                Tok::Plus => BinOp::Add, Tok::Minus => BinOp::Sub,
                Tok::WrapPlus => BinOp::WrapAdd, Tok::WrapMinus => BinOp::WrapSub,
                _ => break,
            };
            self.bump();
            let rhs = self.parse_multiplicative(allow_struct);
            lhs = Expr::Binary { op, lhs: Box::new(lhs), rhs: Box::new(rhs) };
        }
        lhs
    }

    fn parse_multiplicative(&mut self, allow_struct: bool) -> Expr {
        let mut lhs = self.parse_unary(allow_struct);
        loop {
            let op = match self.peek() {
                Tok::Star => BinOp::Mul, Tok::Slash => BinOp::Div, Tok::Percent => BinOp::Mod,
                _ => break,
            };
            self.bump();
            let rhs = self.parse_unary(allow_struct);
            lhs = Expr::Binary { op, lhs: Box::new(lhs), rhs: Box::new(rhs) };
        }
        lhs
    }

    fn parse_unary(&mut self, allow_struct: bool) -> Expr {
        if self.eat(&Tok::Minus) {
            let e = self.parse_unary(allow_struct);
            return Expr::Unary { op: UnOp::Neg, expr: Box::new(e) };
        }
        if self.eat(&Tok::Bang) {
            let e = self.parse_unary(allow_struct);
            return Expr::Unary { op: UnOp::Not, expr: Box::new(e) };
        }
        self.parse_postfix(allow_struct)
    }

    fn parse_postfix(&mut self, allow_struct: bool) -> Expr {
        let e = self.parse_primary(allow_struct);
        self.depth += 1;
        let r = self.parse_postfix_loop(e, allow_struct);
        self.depth -= 1;
        r
    }

    fn parse_postfix_loop(&mut self, mut e: Expr, _allow_struct: bool) -> Expr {
        loop {
            // 恢复路径守卫:超限立即返回,不再构造 Call 包裹(否则与 parse_expr 互递归无界)
            if self.depth > MAX_EXPR_DEPTH {
                self.err_here("E1001", "表达式嵌套过深".into());
                return e;
            }
            match self.peek().clone() {
                Tok::LParen => {
                    self.bump();
                    let mut args = Vec::new();
                    loop {
                        if self.at(&Tok::RParen) { break; }
                        args.push(self.parse_expr());
                        if !self.eat(&Tok::Comma) { break; }
                    }
                    self.expect(&Tok::RParen, "实参表");
                    e = Expr::Call { callee: Box::new(e), args };
                }
                Tok::LBracket => {
                    if self.bracket_followed_by_call_or_lit() {
                        e = self.parse_typeargs_suffix(e);
                    } else {
                        // 尝试索引;内容非合法单表达式(如顶层逗号)→ 回退类型实参(§1.8)
                        let save = self.pos;
                        let dlen = self.diags.len();
                        self.bump();
                        let idx = self.parse_expr();
                        if self.at(&Tok::Comma) {
                            self.pos = save;
                            self.diags.truncate(dlen);
                            e = self.parse_typeargs_suffix(e);
                        } else {
                            self.expect(&Tok::RBracket, "索引");
                            e = Expr::Index { obj: Box::new(e), index: Box::new(idx) };
                        }
                    }
                }
                Tok::Dot => {
                    self.bump();
                    let target = match self.peek().clone() {
                        Tok::Ident(name) => { self.bump(); MemberTarget::Name(name) }
                        Tok::Int { text, .. } => {
                            self.bump();
                            MemberTarget::TupleIndex(text.parse().unwrap_or(0))
                        }
                        other => {
                            self.err_here("E1001", format!("预期成员名,实际 {:?}", other));
                            MemberTarget::Name(String::new())
                        }
                    };
                    e = Expr::Member { obj: Box::new(e), target };
                }
                Tok::Question => { self.bump(); e = Expr::Try(Box::new(e)); }
                _ => break,
            }
        }
        e
    }

    fn parse_typeargs_suffix(&mut self, e: Expr) -> Expr {
        self.expect(&Tok::LBracket, "类型实参");
        let mut args = Vec::new();
        loop {
            if self.at(&Tok::RBracket) { break; }
            args.push(self.parse_type());
            if !self.eat(&Tok::Comma) { break; }
        }
        self.expect(&Tok::RBracket, "类型实参结束");
        Expr::TypeArgs { expr: Box::new(e), args }
    }

    fn parse_primary(&mut self, allow_struct: bool) -> Expr {
        self.depth += 1;
        if self.depth > MAX_EXPR_DEPTH {
            self.err_here("E1001", "表达式嵌套过深".into());
            self.depth -= 1;
            return Expr::Void;
        }
        let e = self.parse_primary_inner(allow_struct);
        self.depth -= 1;
        e
    }

    fn parse_primary_inner(&mut self, allow_struct: bool) -> Expr {
        match self.peek().clone() {
            Tok::Int { text, suffix } => { self.bump(); Expr::Int { text, suffix: suffix_name(&suffix) } }
            Tok::Float { text, suffix } => { self.bump(); Expr::Float { text, suffix: suffix_name(&suffix) } }
            Tok::Str { parts } => {
                self.bump();
                Expr::Str { parts: parts.into_iter().map(|p| match p {
                    TokStrPart::Text(t) => StrPart::Text(t),
                    TokStrPart::Interp(t) => StrPart::Interp(t),
                }).collect() }
            }
            Tok::True => { self.bump(); Expr::Bool(true) }
            Tok::False => { self.bump(); Expr::Bool(false) }
            Tok::Void => { self.bump(); Expr::Void }
            Tok::SelfKw => { self.bump(); Expr::Ident("self".into()) }
            Tok::Ident(name) => {
                self.bump();
                if allow_struct && self.at(&Tok::LBrace) {
                    return self.parse_struct_lit(vec![name]);
                }
                Expr::Ident(name)
            }
            Tok::LParen => {
                self.bump();
                if self.at(&Tok::RParen) { self.bump(); return Expr::Tuple(vec![]); }
                let mut items = vec![self.parse_expr()];
                let mut is_tuple = false;
                while self.eat(&Tok::Comma) {
                    if self.at(&Tok::RParen) { break; }
                    items.push(self.parse_expr());
                    is_tuple = true;
                }
                self.expect(&Tok::RParen, "括号/元组");
                if is_tuple { Expr::Tuple(items) } else { items.remove(0) }
            }
            Tok::LBracket => {
                self.bump();
                let mut items = Vec::new();
                loop {
                    if self.at(&Tok::RBracket) { break; }
                    items.push(self.parse_expr());
                    if !self.eat(&Tok::Comma) { break; }
                }
                self.expect(&Tok::RBracket, "数组字面量");
                Expr::Array(items)
            }
            Tok::LBrace => Expr::BlockExpr(self.parse_block()),
            Tok::If => self.parse_if(),
            Tok::Match => self.parse_match(),
            Tok::Scope => self.parse_scope(),
            Tok::Own => self.parse_own(),
            Tok::Pipe => self.parse_closure(),
            other => {
                self.err_here("E1001", format!("意外的记号 {:?} 在表达式位置", other));
                self.bump();
                Expr::Void
            }
        }
    }

    fn parse_struct_lit(&mut self, path: Vec<String>) -> Expr {
        self.expect(&Tok::LBrace, "构造字面量");
        let mut fields = Vec::new();
        loop {
            self.skip_newlines();
            if self.at(&Tok::RBrace) { self.bump(); break; }
            if self.at(&Tok::Eof) { self.err_here("E1001", "未闭合的构造字面量".into()); break; }
            let before = self.pos;
            let name = self.expect_ident("字段初始化");
            let value = if self.eat(&Tok::Colon) { Some(self.parse_expr()) } else { None };
            fields.push(StructField { name, value });
            self.ensure_progress(before);
            if !self.eat(&Tok::Comma) {
                self.skip_newlines();
                if !self.at(&Tok::RBrace) {
                    self.err_here("E1001", "构造字面量字段应以逗号分隔".into());
                }
            }
        }
        Expr::StructLit { path, type_args: Vec::new(), fields }
    }

    fn parse_if(&mut self) -> Expr {
        self.bump(); // if
        let cond = self.parse_expr_flags(false);
        let then = self.parse_block();
        let mut els = None;
        if self.at(&Tok::Else) {
            self.bump();
            els = Some(Box::new(self.parse_else_branch()));
        } else if self.at(&Tok::Newline) && matches!(self.lookahead_past_newlines(), Tok::Else) {
            self.err_here("E1001", "else 必须与 } 同行:`} else {`".into());
            self.skip_newlines();
            self.bump(); // else
            els = Some(Box::new(self.parse_else_branch()));
        }
        Expr::If { cond: Box::new(cond), then, els }
    }

    fn parse_else_branch(&mut self) -> Expr {
        if self.at(&Tok::If) { self.parse_if() } else { Expr::BlockExpr(self.parse_block()) }
    }

    fn parse_match(&mut self) -> Expr {
        self.bump(); // match
        let scrutinee = self.parse_expr_flags(false);
        self.expect(&Tok::LBrace, "match 体");
        let mut arms = Vec::new();
        loop {
            self.skip_newlines();
            if self.at(&Tok::RBrace) { self.bump(); break; }
            if self.at(&Tok::Eof) { self.err_here("E1001", "未闭合的 match 体".into()); break; }
            let pattern = self.parse_pattern();
            self.expect(&Tok::FatArrow, "match 臂");
            let body = self.parse_expr();
            arms.push(MatchArm { pattern, expr: body });
            if self.at(&Tok::Newline) { self.skip_newlines(); }
            else if !self.at(&Tok::RBrace) {
                self.err_here("E1001", format!("match 臂后应为换行,实际 {:?}", self.peek()));
            }
        }
        Expr::Match { expr: Box::new(scrutinee), arms }
    }

    fn parse_scope(&mut self) -> Expr {
        self.bump(); // scope
        self.expect(&Tok::LBrace, "scope 块");
        self.expect(&Tok::Pipe, "scope 参数");
        let param = self.expect_ident("scope 参数");
        self.expect(&Tok::Pipe, "scope 参数");
        let body = self.parse_block_after_lbrace();
        Expr::Scope { param, body }
    }

    fn parse_own(&mut self) -> Expr {
        self.bump(); // own
        self.expect(&Tok::LParen, "own 块");
        let arena = self.expect_ident("arena 名");
        self.expect(&Tok::RParen, "own 块");
        let body = self.parse_block();
        Expr::Own { arena, body }
    }

    fn parse_closure(&mut self) -> Expr {
        self.expect(&Tok::Pipe, "闭包");
        let mut params = Vec::new();
        if !self.at(&Tok::Pipe) {
            loop {
                let is_var = self.eat(&Tok::Var);
                let name = self.expect_ident("闭包参数");
                let ty = if self.eat(&Tok::Colon) { Some(self.parse_type()) } else { None };
                params.push(ClosureParam { is_var, name, ty });
                if !self.eat(&Tok::Comma) { break; }
            }
        }
        self.expect(&Tok::Pipe, "闭包参数结束");
        let ret = if self.eat(&Tok::Arrow) { Some(Box::new(self.parse_type())) } else { None };
        let body = Box::new(self.parse_expr());
        Expr::Closure { params, ret, body }
    }

    // ---------- 块与语句 ----------

    pub fn parse_block(&mut self) -> Block {
        self.expect(&Tok::LBrace, "块");
        self.parse_block_after_lbrace()
    }

    fn parse_block_after_lbrace(&mut self) -> Block {
        self.skip_newlines();
        let mut stmts = Vec::new();
        let mut tail = None;
        loop {
            self.skip_newlines();
            if self.at(&Tok::RBrace) { self.bump(); break; }
            if self.at(&Tok::Eof) { self.err_here("E1001", "未闭合的块".into()); break; }
            match self.peek().clone() {
                Tok::Let | Tok::Var => {
                    let before = self.pos;
                    let is_var = self.at(&Tok::Var);
                    self.bump();
                    let pattern = self.parse_pattern();
                    let ty = if self.eat(&Tok::Colon) { Some(self.parse_type()) } else { None };
                    self.expect(&Tok::Assign, "绑定");
                    let expr = self.parse_expr();
                    stmts.push(Stmt::Let { is_var, pattern, ty, expr });
                    self.ensure_progress(before);
                    self.require_stmt_end();
                }
                Tok::Return => {
                    self.bump();
                    let e = if self.at(&Tok::Newline) || self.at(&Tok::RBrace) { None } else { Some(self.parse_expr()) };
                    stmts.push(Stmt::Return(e));
                    self.require_stmt_end();
                }
                Tok::For => {
                    self.bump();
                    let pattern = self.parse_pattern();
                    self.expect(&Tok::In, "for");
                    let iter = self.parse_expr_flags(false);
                    let body = self.parse_block();
                    stmts.push(Stmt::For { pattern, iter, body });
                    self.require_stmt_end();
                }
                Tok::While => {
                    self.bump();
                    let cond = self.parse_expr_flags(false);
                    let body = self.parse_block();
                    stmts.push(Stmt::While { cond, body });
                    self.require_stmt_end();
                }
                _ => {
                    let e = self.parse_expr();
                    let aop = match self.peek() {
                        Tok::Assign => Some(AssignOp::Eq), Tok::PlusEq => Some(AssignOp::AddEq),
                        Tok::MinusEq => Some(AssignOp::SubEq), Tok::StarEq => Some(AssignOp::MulEq),
                        Tok::SlashEq => Some(AssignOp::DivEq), Tok::PercentEq => Some(AssignOp::ModEq),
                        _ => None,
                    };
                    if let Some(op) = aop {
                        self.bump();
                        if !matches!(e, Expr::Ident(_) | Expr::Member { .. } | Expr::Index { .. }) {
                            self.err_here("E1001", "无效的赋值目标".into());
                        }
                        let value = self.parse_expr();
                        stmts.push(Stmt::Assign { target: e, op, value });
                        self.require_stmt_end();
                    } else {
                        // 尾表达式判定:其后(可跨换行)是 } 或 EOF → tail(块值,优先)
                        match self.lookahead_past_newlines() {
                            Tok::RBrace => {
                                self.skip_newlines();
                                tail = Some(Box::new(e));
                                self.bump();
                                break;
                            }
                            Tok::Eof => {
                                self.err_here("E1001", "未闭合的块".into());
                                tail = Some(Box::new(e));
                                break;
                            }
                            _ => {
                                stmts.push(Stmt::Expr(e));
                                if self.at(&Tok::Newline) { self.skip_newlines(); }
                                else {
                                    self.err_here("E1001", format!("语句后应为换行,实际 {:?}", self.peek()));
                                }
                            }
                        }
                    }
                }
            }
        }
        Block { stmts, tail }
    }

    fn require_stmt_end(&mut self) {
        if self.at(&Tok::Newline) { self.skip_newlines(); }
        else if self.at(&Tok::RBrace) || self.at(&Tok::Eof) { /* 闭合交给循环头 */ }
        else {
            self.err_here("E1001", format!("语句后应为换行,实际 {:?}", self.peek()));
        }
    }

    // ---------- 模式 ----------

    pub fn parse_pattern(&mut self) -> Pattern {
        match self.peek().clone() {
            Tok::Underscore => { self.bump(); Pattern::Wildcard }
            Tok::Int { text, .. } => { self.bump(); Pattern::Lit(PatLit::Int(text)) }
            Tok::Float { text, .. } => { self.bump(); Pattern::Lit(PatLit::Float(text)) }
            Tok::Str { parts } => {
                self.bump();
                let s = parts.into_iter().map(|p| match p {
                    TokStrPart::Text(t) => t,
                    TokStrPart::Interp(t) => t,
                }).collect::<Vec<_>>().join("");
                Pattern::Lit(PatLit::Str(s))
            }
            Tok::True => { self.bump(); Pattern::Lit(PatLit::Bool(true)) }
            Tok::False => { self.bump(); Pattern::Lit(PatLit::Bool(false)) }
            Tok::LParen => {
                self.bump();
                let mut ps = Vec::new();
                loop {
                    if self.at(&Tok::RParen) { break; }
                    ps.push(self.parse_pattern());
                    if !self.eat(&Tok::Comma) { break; }
                }
                self.expect(&Tok::RParen, "元组模式");
                if ps.len() == 1 { ps.remove(0) } else { Pattern::Tuple(ps) }
            }
            Tok::Ident(_) => {
                let mut path = self.parse_dotted_path();
                if self.at(&Tok::LParen) {
                    self.bump();
                    let mut ps = Vec::new();
                    loop {
                        if self.at(&Tok::RParen) { break; }
                        ps.push(self.parse_pattern());
                        if !self.eat(&Tok::Comma) { break; }
                    }
                    self.expect(&Tok::RParen, "变体模式");
                    Pattern::Agg { path, sub: AggSub::Tuple(ps) }
                } else if self.at(&Tok::LBrace) {
                    self.bump();
                    let mut fields = Vec::new();
                    loop {
                        self.skip_newlines();
                        if self.at(&Tok::RBrace) { self.bump(); break; }
                        let before = self.pos;
                        let fname = self.expect_ident("结构模式字段");
                        let pat = if self.eat(&Tok::Colon) { Some(self.parse_pattern()) } else { None };
                        fields.push(StructPatField { name: fname, pattern: pat });
                        self.ensure_progress(before);
                        if !self.eat(&Tok::Comma) {
                            self.skip_newlines();
                            if !self.at(&Tok::RBrace) {
                                self.err_here("E1001", "结构模式字段应以逗号分隔".into());
                            }
                        }
                    }
                    Pattern::Agg { path, sub: AggSub::Struct(fields) }
                } else if path.len() > 1 {
                    Pattern::Agg { path, sub: AggSub::Unit }
                } else if path[0].chars().next().map(|c| c.is_ascii_uppercase()).unwrap_or(false) {
                    // PascalCase 单段 → 无载荷变体(命名约定,§1.3)
                    Pattern::Agg { path, sub: AggSub::Unit }
                } else {
                    Pattern::Ident(path.swap_remove(0))
                }
            }
            other => {
                self.err_here("E1001", format!("意外记号 {:?} 在模式位置", other));
                self.bump();
                Pattern::Wildcard
            }
        }
    }
}

fn suffix_name(s: &NumSuffix) -> String {
    // C-AST v1 契约:后缀保留源码小写原文(§1.4)
    match s {
        NumSuffix::None => String::new(),
        NumSuffix::I8 => "i8".into(), NumSuffix::I16 => "i16".into(),
        NumSuffix::I32 => "i32".into(), NumSuffix::I64 => "i64".into(),
        NumSuffix::ISize => "isize".into(),
        NumSuffix::U8 => "u8".into(), NumSuffix::U16 => "u16".into(),
        NumSuffix::U32 => "u32".into(), NumSuffix::U64 => "u64".into(),
        NumSuffix::USize => "usize".into(),
        NumSuffix::F32 => "f32".into(), NumSuffix::F64 => "f64".into(),
    }
}

fn tok_display(t: &Tok) -> &'static str {
    match t {
        Tok::Int { .. } => "整数字面量", Tok::Float { .. } => "浮点字面量", Tok::Str { .. } => "字符串",
        Tok::Ident(_) => "标识符", Tok::Fn => "fn", Tok::Let => "let", Tok::Var => "var",
        Tok::Const => "const", Tok::Static => "static", Tok::Comptime => "comptime",
        Tok::If => "if", Tok::Else => "else", Tok::Match => "match", Tok::While => "while",
        Tok::For => "for", Tok::In => "in", Tok::Return => "return", Tok::Struct => "struct",
        Tok::Class => "class", Tok::Enum => "enum", Tok::Trait => "trait", Tok::Impl => "impl",
        Tok::Own => "own", Tok::Scope => "scope", Tok::Test => "test", Tok::Use => "use",
        Tok::Pub => "pub", Tok::Extern => "extern", Tok::Prop => "prop", Tok::True => "true",
        Tok::False => "false", Tok::Void => "void", Tok::SelfKw => "self",
        Tok::Plus => "+", Tok::Minus => "-", Tok::Star => "*", Tok::Slash => "/",
        Tok::Percent => "%", Tok::WrapPlus => "+%", Tok::WrapMinus => "-%",
        Tok::PlusEq => "+=", Tok::MinusEq => "-=", Tok::StarEq => "*=", Tok::SlashEq => "/=",
        Tok::PercentEq => "%=", Tok::EqEq => "==", Tok::NotEq => "!=", Tok::Lt => "<",
        Tok::Gt => ">", Tok::LtEq => "<=", Tok::GtEq => ">=", Tok::Assign => "=",
        Tok::AndAnd => "&&", Tok::Or => "or", Tok::DotDot => "..", Tok::DotDotEq => "..=",
        Tok::Arrow => "->", Tok::FatArrow => "=>", Tok::Question => "?", Tok::Dot => ".",
        Tok::Comma => ",", Tok::Colon => ":", Tok::LBracket => "[", Tok::RBracket => "]",
        Tok::LParen => "(", Tok::RParen => ")", Tok::LBrace => "{", Tok::RBrace => "}",
        Tok::Pipe => "|", Tok::Amp => "&", Tok::Hash => "#", Tok::At => "@",
        Tok::Underscore => "_", Tok::Bang => "!", Tok::Newline => "换行", Tok::Eof => "文件尾",
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn expr(src: &str) -> (Expr, Vec<Diagnostic>) {
        let (toks, ld) = crate::lex(src);
        let mut p = Parser::new(toks);
        let e = p.parse_expr();
        p.diags.extend(ld);
        (e, p.diags)
    }
    fn file(src: &str) -> (File, Vec<Diagnostic>) { crate::parse_src(src) }
    fn block(src: &str) -> (Block, Vec<Diagnostic>) {
        let (toks, ld) = crate::lex(src);
        let mut p = Parser::new(toks);
        let b = p.parse_block();
        p.diags.extend(ld);
        (b, p.diags)
    }
    fn codes(d: &[Diagnostic]) -> Vec<&'static str> { d.iter().map(|x| x.code).collect() }

    // ---------- 表达式层 ----------

    #[test]
    fn precedence_mul_binds_tighter() {
        let (e, d) = expr("2 + 3 * 4");
        assert!(d.is_empty());
        match e {
            Expr::Binary { op: BinOp::Add, rhs, .. } =>
                assert!(matches!(*rhs, Expr::Binary { op: BinOp::Mul, .. })),
            other => panic!("{:?}", other),
        }
    }

    #[test]
    fn comparison_chaining_is_e1001() {
        let (_, d) = expr("x < y < z");
        assert_eq!(codes(&d), vec!["E1001"]);
    }

    #[test]
    fn ufcs_chain_with_or_method() {
        let (e, d) = expr("opt.map(f).or(0)");
        assert!(d.is_empty());
        let s = format!("{:?}", e);
        assert!(s.contains(r#"Name("map")"#) && s.contains(r#"Name("or")"#));
    }

    #[test]
    fn generic_instantiation_then_call() {
        let (e, d) = expr("Channel[I32](4)");
        assert!(d.is_empty());
        match e {
            Expr::Call { callee, .. } => assert!(matches!(*callee, Expr::TypeArgs { .. })),
            other => panic!("{:?}", other),
        }
    }

    #[test]
    fn index_then_member() {
        let (e, d) = expr("xs[i].len");
        assert!(d.is_empty());
        match e {
            Expr::Member { obj, .. } => assert!(matches!(*obj, Expr::Index { .. })),
            other => panic!("{:?}", other),
        }
    }

    #[test]
    fn simd_typeargs_via_index_fallback() {
        // ] 后是 . 不满足快速路径;索引内容含顶层逗号 → 回退类型实参(§1.8)
        let (e, d) = expr("Simd[F32, 4].splat(v)");
        assert!(d.is_empty());
        let s = format!("{:?}", e);
        assert!(s.contains("TypeArgs"), "{}", s);
    }

    #[test]
    fn or_infix_vs_or_member() {
        let (e, d) = expr("x or y");
        assert!(d.is_empty());
        assert!(matches!(e, Expr::Binary { op: BinOp::Or, .. }));
        let (e, d) = expr("x.or(0)");
        assert!(d.is_empty());
        let s = format!("{:?}", e);
        assert!(s.contains(r#"Name("or")"#) && !s.contains("Binary"));
    }

    #[test]
    fn closure_var_param_and_block_body() {
        let (e, d) = expr("|var a| { a }");
        assert!(d.is_empty());
        match e {
            Expr::Closure { params, .. } => {
                assert_eq!(params.len(), 1);
                assert!(params[0].is_var);
            }
            other => panic!("{:?}", other),
        }
    }

    #[test]
    fn scope_and_own() {
        let (e, d) = expr("scope { |s| t.join() }");
        assert!(d.is_empty());
        assert!(matches!(e, Expr::Scope { ref param, .. } if param == "s"));
        let (e, d) = expr("own (arena) { }");
        assert!(d.is_empty());
        assert!(matches!(e, Expr::Own { ref arena, .. } if arena == "arena"));
    }

    #[test]
    fn interp_str_parts() {
        let (e, d) = expr("\"hi {name}\"");
        assert!(d.is_empty());
        match e {
            Expr::Str { parts } => {
                assert_eq!(parts, vec![StrPart::Text("hi ".into()), StrPart::Interp("name".into())]);
            }
            other => panic!("{:?}", other),
        }
    }

    // ---------- 声明层 ----------

    #[test]
    fn static_var_recovers_with_e3030() {
        let (f, d) = file("static var COUNTER: I32 = 0");
        assert_eq!(codes(&d), vec!["E3030"]);
        match &f.decls[0] {
            Decl::Static(s) => assert!(s.was_var),
            other => panic!("{:?}", other),
        }
    }

    #[test]
    fn use_group_expands_to_full_paths() {
        let (f, d) = file("use std.net.{TcpListener, Request}");
        assert!(d.is_empty());
        match &f.decls[0] {
            Decl::Use(u) => {
                let want: Vec<Vec<String>> = vec![
                    vec!["std".into(), "net".into(), "TcpListener".into()],
                    vec!["std".into(), "net".into(), "Request".into()],
                ];
                assert_eq!(u.imports, want);
            }
            other => panic!("{:?}", other),
        }
    }

    #[test]
    fn pub_pkg_visibility() {
        let (f, d) = file("pub(pkg) fn triple(x: I32) -> I32 { return x * 3 }");
        assert!(d.is_empty());
        match &f.decls[0] {
            Decl::Fn(fun) => assert_eq!(fun.vis, Vis::PubPkg),
            other => panic!("{:?}", other),
        }
    }

    #[test]
    fn extern_decl_has_no_body() {
        let (f, d) = file("#[trusted]\nextern \"c\" fn ctron_add(a: I64, b: I64) -> I64");
        assert!(d.is_empty(), "{:?}", d);
        match &f.decls[0] {
            Decl::Fn(fun) => {
                assert_eq!(fun.abi.as_deref(), Some("c"));
                assert!(fun.body.is_none());
                assert_eq!(fun.attrs.len(), 1);
            }
            other => panic!("{:?}", other),
        }
    }

    #[test]
    fn trait_supertraits() {
        let (f, d) = file("trait Env: Clock + Named { }");
        assert!(d.is_empty());
        match &f.decls[0] {
            Decl::Trait(t) => {
                let want: Vec<String> = vec!["Clock".into(), "Named".into()];
                assert_eq!(t.supers, want);
            }
            other => panic!("{:?}", other),
        }
    }

    #[test]
    fn derives_collected() {
        let (f, d) = file("@derive(Show, Eq)\nstruct Pixel {\n    let x: I32\n}");
        assert!(d.is_empty(), "{:?}", d);
        match &f.decls[0] {
            Decl::Struct(s) => {
                let want: Vec<String> = vec!["Show".into(), "Eq".into()];
                assert_eq!(s.derives, want);
            }
            other => panic!("{:?}", other),
        }
    }

    // ---------- 语句与模式 ----------

    #[test]
    fn let_tuple_pattern() {
        let (b, d) = block("{ let (a, b) = pair }");
        assert!(d.is_empty());
        match &b.stmts[0] {
            Stmt::Let { pattern: Pattern::Tuple(ps), .. } => assert_eq!(ps.len(), 2),
            other => panic!("{:?}", other),
        }
    }

    #[test]
    fn match_struct_pattern_shorthand() {
        let (b, d) = block("{ match p { Pt { x, y } => x } }");
        assert!(d.is_empty());
        match b.tail.as_deref() {
            Some(Expr::Match { arms, .. }) => match &arms[0].pattern {
                Pattern::Agg { sub: AggSub::Struct(fs), .. } => assert_eq!(fs.len(), 2),
                other => panic!("{:?}", other),
            },
            other => panic!("{:?}", other),
        }
    }

    #[test]
    fn assignment_targets() {
        let (b, d) = block("{ b.x = 10 }");
        assert!(d.is_empty());
        assert!(matches!(&b.stmts[0], Stmt::Assign { op: AssignOp::Eq, .. }));
        let (b, d) = block("{ x += 1 }");
        assert!(d.is_empty());
        assert!(matches!(&b.stmts[0], Stmt::Assign { op: AssignOp::AddEq, .. }));
    }

    #[test]
    fn else_must_be_same_line() {
        let (_, d) = file("fn f() {\n    if a {\n    }\n    else {\n    }\n}");
        assert!(d.iter().any(|x| x.code == "E1001" && x.message.contains("else")));
        let (_, d) = file("fn f() {\n    if a {\n    } else {\n    }\n}");
        assert!(d.is_empty(), "{:?}", d);
    }

    #[test]
    fn for_wildcard_and_return_some() {
        let (b, d) = block("{ for _ in 0..4 { } }");
        assert!(d.is_empty());
        assert!(matches!(&b.stmts[0], Stmt::For { pattern: Pattern::Wildcard, .. }));
        let (b, d) = block("{ return Ok(v) }");
        assert!(d.is_empty());
        assert!(matches!(&b.stmts[0], Stmt::Return(Some(_))));
    }

    #[test]
    fn int_ident_adjacency_is_rejected() {
        // 255U8 词法回落:Int+Ident 相邻 → 语句层 E1001(P1-A 下游契约③)
        let (_, d) = block("{ let m: U8 = 255u8 }");
        assert!(d.is_empty());
        let (_, d) = block("{ let m = 255 U8 }");
        assert!(d.iter().any(|x| x.code == "E1001"));
    }
}

#[cfg(test)]
mod final_review_pins {
    use super::*;
    use crate::ast::*;

    #[test]
    fn type_position_typeargs_unconditional() {
        // B-1 回归:类型位置 [ ] 无条件为类型实参(曾静默错析为定长数组)
        let (f, d) = crate::parse_src("fn f() -> Atomic[I32] { return x }");
        assert!(d.is_empty(), "{:?}", d);
        match &f.decls[0] {
            Decl::Fn(fun) => match &fun.ret {
                Some(Type::Named { args, .. }) => assert_eq!(args.len(), 1),
                other => panic!("ret 应为 Named 带实参,实际 {:?}", other),
            },
            other => panic!("{:?}", other),
        }
        let (f, d) = crate::parse_src("fn g(m: Map[Str, I32]) -> Void { return void }");
        assert!(d.is_empty(), "{:?}", d);
        match &f.decls[0] {
            Decl::Fn(fun) => match &fun.params[0] {
                Param::Param { ty: Type::Named { args, .. }, .. } => assert_eq!(args.len(), 2),
                other => panic!("参数应为 Named 带两实参,实际 {:?}", other),
            },
            other => panic!("{:?}", other),
        }
    }

    #[test]
    fn trait_method_sig_keeps_fn_decl() {
        // B-2 回归:trait 无体方法保留 Method(FnDecl),不得降格为 PropSig
        let (f, d) = crate::parse_src("trait Clock: Cap {\n    fn now(&self) -> U64\n}");
        assert!(d.is_empty(), "{:?}", d);
        match &f.decls[0] {
            Decl::Trait(t) => match &t.items[0] {
                TraitItem::Method(fun) => {
                    assert_eq!(fun.name, "now");
                    assert!(fun.body.is_none());
                    assert_eq!(fun.params.len(), 1);
                }
                other => panic!("应为 Method,实际 {:?}", other),
            },
            other => panic!("{:?}", other),
        }
    }

    #[test]
    fn pub_prop_parses() {
        // B-3 回归:prop 支持前置 Visibility
        let (f, d) = crate::parse_src("class Box2 {\n    pub prop size: I64 {\n        return 1\n    }\n}");
        assert!(d.is_empty(), "{:?}", d);
        match &f.decls[0] {
            Decl::Class(c) => match &c.items[0] {
                ClassItem::Prop(p) => {
                    assert_eq!(p.vis, Vis::Pub);
                    assert!(p.body.is_some());
                }
                other => panic!("{:?}", other),
            },
            other => panic!("{:?}", other),
        }
    }

    #[test]
    fn deep_nesting_capped() {
        // 恢复路径盲区回归:2000 个连续 ( 在默认测试线程上必须正常退出且产出诊断
        // (曾因 postfix 的 LParen 恢复分支与 parse_expr 互递归而栈溢出 abort)
        let src = format!("fn f() -> I32 {{ return {}1{} }}", "(".repeat(2000), ")".repeat(2000));
        let (_, d) = crate::parse_src(&src);
        assert!(!d.is_empty());
        assert!(d.iter().any(|x| x.message.contains("嵌套过深")));
        let nested = format!("{}I64{}", "(".repeat(1500), ")".repeat(1500));
        let src = format!("fn g(x: {}) -> Void {{ return void }}", nested);
        let (_, d) = crate::parse_src(&src);
        assert!(!d.is_empty());
        assert!(d.iter().any(|x| x.message.contains("嵌套过深")));
    }
}

#[cfg(test)]
mod rework_pins {
    use super::*;
    use crate::ast::*;

    #[test]
    fn trait_pub_prop_parses() {
        // B-3 收尾回归:trait 内 pub prop(曾漏改 trait 循环)
        let (f, d) = crate::parse_src("trait T {\n    pub prop size: I64\n}");
        assert!(d.is_empty(), "{:?}", d);
        match &f.decls[0] {
            Decl::Trait(t) => match &t.items[0] {
                TraitItem::PropSig(p) => assert_eq!(p.vis, Vis::Pub),
                other => panic!("{:?}", other),
            },
            other => panic!("{:?}", other),
        }
    }

    #[test]
    fn fixed_array_type_survives_disambiguation() {
        // B-1 对称回归:I32[3] 必须是定长数组而非泛型实参
        let (f, d) = crate::parse_src("fn f() -> Void {\n    var buf: I32[3] = [1, 2, 3]\n    return void\n}");
        assert!(d.is_empty(), "{:?}", d);
        let (f, d) = crate::parse_src("static let BUF: I32[3] = zero()");
        assert!(d.is_empty(), "{:?}", d);
        match &f.decls[0] {
            Decl::Static(s) => match &s.ty {
                Type::Array { elem, size: Some(_) } => {
                    assert!(matches!(**elem, Type::Named { ref path, .. } if path == &vec!["I32".to_string()]));
                }
                other => panic!("应为定长数组,实际 {:?}", other),
            },
            other => panic!("{:?}", other),
        }
    }

    #[test]
    fn slice_and_generic_types_still_work() {
        // 切片与泛型不回归
        let (f, d) = crate::parse_src("fn f(xs: I32[]) { return void }");
        assert!(d.is_empty(), "{:?}", d);
        let (f, d) = crate::parse_src("fn g(m: Map[Str, I32]) { return void }");
        assert!(d.is_empty(), "{:?}", d);
        let _ = f;
    }
}

impl Parser {
    /// 供解释器:解析单个表达式(插值源文本)
    pub fn parse_expr_public(&mut self) -> crate::ast::Expr {
        self.parse_expr()
    }
}

impl Parser {
    /// 公开文件解析(interp 用)
    pub fn parse_file_public(&mut self) -> crate::ast::File {
        self.parse_file()
    }
}
