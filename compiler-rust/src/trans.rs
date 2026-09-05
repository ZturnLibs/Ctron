//! trans.rs —— P1-E①:Ctron → C 转译后端(数值域骨架)。
//!
//! 语义契约 = 本 crate 的解释器(语义差分对象,与 C 版 C10-a 对 rt.c 的关系同构):
//!   - 整数统一以 `__int128` 承载(消除 C 符号/宽度混合陷阱);宽度只体现在运算检查;
//!   - 无后缀整型 = 带标记 I64(检查算术,i64 边界);后缀/let 注解产生宽度标记(§3.6);
//!   - 算术结果宽度取左操作数中任一带宽度者(左优先);检查算术溢出 panic "integer overflow";
//!     除零 "division by zero";`+%`/`-%` 二补回绕到宽度;
//!   - let 注解 coerce 只重标宽度不检查(对齐 interp coerce_literal);
//!   - 比较:i128 数值精确比较(跨宽度/跨符号,对齐 interp compare_values);
//!   - 一元负号:无符号恒溢出;有符号 = 0 - x 按宽度检查;
//!   - if 作为值:语句提升——if 语句先行发射到输出流,使用点引用临时变量
//!     (语句顺序与解释器求值顺序一致);
//!   - test 块 = 独立 C 函数按序执行;panic → stderr "panic: <msg>" + exit 1;
//!     assert/assert_eq/panic 内建与解释器同文案;
//!   - 遮蔽:每个绑定发唯一 C 名(C 不允许同块重声明,解释器允许遮蔽)。
//! v1 拒绝域:Str/数组/struct/class/enum/trait/impl/match/闭包/Option/Result/?/GC/own/
//!   scope/tuple/const/static/use/Member/Index/TypeArgs。

use crate::ast;
use crate::sem::IntW;

// ---------------- 值类型格(镜像 interp 的宽度标记模型) ----------------

#[derive(Clone, Copy, PartialEq, Debug)]
enum VTy {
    Unknown,
    Void,
    Bool,
    F64,
    F32,
    /// None = 无标记(i64 检查算术);Some((宽, 无符号)) = 宽度标记
    Int(Option<(IntW, bool)>),
}

impl VTy {
    fn is_num(&self) -> bool {
        matches!(self, VTy::Int(_) | VTy::F32 | VTy::F64)
    }
    fn is_float(&self) -> bool {
        matches!(self, VTy::F32 | VTy::F64)
    }
}

fn wbits(w: IntW) -> i32 {
    match w {
        IntW::W8 => 8,
        IntW::W16 => 16,
        IntW::W32 => 32,
        IntW::W64 | IntW::WSize => 64,
    }
}

fn scalar_annotation(name: &str) -> Option<VTy> {
    Some(match name {
        "I8" => VTy::Int(Some((IntW::W8, true))),
        "I16" => VTy::Int(Some((IntW::W16, true))),
        "I32" => VTy::Int(Some((IntW::W32, true))),
        "I64" | "ISize" => VTy::Int(Some((IntW::W64, true))),
        "U8" => VTy::Int(Some((IntW::W8, false))),
        "U16" => VTy::Int(Some((IntW::W16, false))),
        "U32" => VTy::Int(Some((IntW::W32, false))),
        "U64" | "USize" => VTy::Int(Some((IntW::W64, false))),
        "F64" => VTy::F64,
        "F32" => VTy::F32,
        "Bool" => VTy::Bool,
        _ => return None,
    })
}

fn suffix_ty(suffix: &str) -> VTy {
    match suffix {
        "u8" => VTy::Int(Some((IntW::W8, false))),
        "u16" => VTy::Int(Some((IntW::W16, false))),
        "u32" => VTy::Int(Some((IntW::W32, false))),
        "u64" | "usize" => VTy::Int(Some((IntW::W64, false))),
        "i8" => VTy::Int(Some((IntW::W8, true))),
        "i16" => VTy::Int(Some((IntW::W16, true))),
        "i32" => VTy::Int(Some((IntW::W32, true))),
        "i64" | "isize" => VTy::Int(Some((IntW::W64, true))),
        "f32" => VTy::F32,
        "f64" => VTy::F64,
        _ => VTy::Int(Some((IntW::W64, true))), // 无后缀 = 带标记 I64
    }
}

// ---------------- 发射器 ----------------

type TRes = Result<(String, VTy), String>; // Ok((C 表达式, 类型)) / Err(域外拒绝)

pub struct Trans {
    sink: Vec<String>, // 缓冲栈:顶层缓冲即最终产物
    scopes: Vec<Vec<(String, String, VTy)>>, // 作用域栈:(名, C 名, 类型)
    uniq: u32,
    fns: Vec<(String, Vec<VTy>, VTy)>,
}

impl Trans {
    pub fn new() -> Self {
        Trans { sink: vec![String::new()], scopes: Vec::new(), uniq: 0, fns: Vec::new() }
    }

    fn uniq_name(&mut self, base: &str) -> String {
        self.uniq += 1;
        format!("ct_{}_{}", base, self.uniq)
    }

    fn w(&mut self, ind: usize, s: &str) {
        let buf = self.sink.last_mut().unwrap();
        for _ in 0..ind { buf.push_str("    "); }
        buf.push_str(s);
        buf.push('\n');
    }

    fn emit_lines(&mut self, code: &str) {
        for line in code.lines() {
            if line.is_empty() { continue; }
            self.w(2, line);
        }
    }

    fn scope_push(&mut self) { self.scopes.push(Vec::new()); }
    fn scope_pop(&mut self) { self.scopes.pop(); }

    fn bind(&mut self, name: &str, ty: VTy) -> String {
        let c = self.uniq_name(&sanitize(name));
        self.scopes.last_mut().unwrap().push((name.to_string(), c.clone(), ty));
        c
    }

    fn lookup(&self, name: &str) -> Option<(String, VTy)> {
        for layer in self.scopes.iter().rev() {
            for (n, c, t) in layer.iter().rev() {
                if n == name { return Some((c.clone(), *t)); }
            }
        }
        None
    }

    fn lookup_fn(&self, name: &str) -> Option<(Vec<VTy>, VTy)> {
        self.fns.iter().find(|(n, _, _)| n == name).map(|(_, ps, r)| (ps.clone(), *r))
    }

    // ---------------- 文件 ----------------

    pub fn trans_file(mut self, file: &ast::File) -> Result<String, String> {
        let mut tests = Vec::new();
        for d in &file.decls {
            match d {
                ast::Decl::Fn(f) => {
                    let params = f.params.iter().map(|p| match p {
                        ast::Param::Param { ty, .. } => self.ty_of(ty),
                        ast::Param::Receiver { .. } => VTy::Unknown,
                    }).collect();
                    let ret = f.ret.as_ref().map(|t| self.ty_of(t)).unwrap_or(VTy::Void);
                    self.fns.push((f.name.clone(), params, ret));
                }
                ast::Decl::Test(t) => tests.push(t.clone()),
                other => return Err(format!(
                    "trans v1 拒绝域:声明 `{}`(仅支持 fn/test)",
                    decl_name(other)
                )),
            }
        }

        self.w(0, PREAMBLE);

        for d in &file.decls {
            if let ast::Decl::Fn(f) = d {
                self.emit_fn(f)?;
            }
        }

        let mut calls = String::new();
        for (i, t) in tests.iter().enumerate() {
            let fname = format!("ct_test_{}", i);
            self.w(0, &format!("static void {}(void) {{", fname));
            self.scope_push();
            let esc = t.name.replace('\\', "\\\\").replace('"', "\\\"");
            self.w(1, &format!("ct_cur_test = \"{}\";", esc));
            self.emit_block_stmts(&t.body)?;
            self.scope_pop();
            self.w(0, "}");
            calls.push_str(&format!("    {}();\n", fname));
        }
        if !tests.is_empty() {
            self.w(0, "int main(void) {");
            self.w(1, "alarm(20); /* 防挂起 */");
            self.w(1, &calls);
            self.w(1, "return 0;");
            self.w(0, "}");
        } else {
            self.w(0, "int main(void) { alarm(20); return 0; }");
        }
        Ok(self.sink.into_iter().next_back().unwrap_or_default())
    }

    fn ty_of(&self, t: &ast::Type) -> VTy {
        if let ast::Type::Named { path, .. } = t {
            if let Some(name) = path.last() {
                if let Some(v) = scalar_annotation(name) { return v; }
            }
        }
        VTy::Unknown
    }

    fn c_ty(&self, t: VTy) -> &'static str {
        match t {
            VTy::F64 => "double",
            VTy::F32 => "float",
            VTy::Bool => "int",
            VTy::Int(_) => "ct_i",
            _ => "ct_i",
        }
    }

    // ---------------- 函数 ----------------

    fn emit_fn(&mut self, f: &ast::FnDecl) -> Result<(), String> {
        let ret = self.lookup_fn(&f.name).map(|(_, r)| r).unwrap_or(VTy::Void);
        self.scope_push();
        let mut parts = Vec::new();
        for p in &f.params {
            if let ast::Param::Param { name, ty, .. } = p {
                let vty = self.ty_of(ty);
                let c = self.bind(name, vty);
                parts.push(format!("{} {}", self.c_ty(vty), c));
            }
        }
        self.w(0, &format!(
            "static {} {}({}) {{",
            self.c_ty(ret),
            sanitize(&f.name),
            parts.join(", ")
        ));
        if let Some(body) = &f.body {
            self.emit_block_stmts(body)?;
        }
        self.scope_pop();
        self.w(0, "}");
        Ok(())
    }

    // ---------------- 语句 ----------------

    fn emit_block_stmts(&mut self, block: &ast::Block) -> Result<(), String> {
        self.scope_push();
        for s in &block.stmts {
            self.emit_stmt(s)?;
        }
        if let Some(t) = &block.tail {
            let (c, _) = self.expr(t)?;
            self.w(1, &format!("(void)({});", c));
        }
        self.scope_pop();
        Ok(())
    }

    /// 分支体:语句发射到当前缓冲,返回尾值(若有)
    fn emit_branch(&mut self, block: &ast::Block) -> Result<Option<(String, VTy)>, String> {
        self.scope_push();
        for s in &block.stmts { self.emit_stmt(s)?; }
        let tail = match &block.tail {
            Some(t) => Some(self.expr(t)?),
            None => None,
        };
        self.scope_pop();
        Ok(tail)
    }

    fn emit_stmt(&mut self, s: &ast::Stmt) -> Result<(), String> {
        match s {
            ast::Stmt::Let { pattern, ty: ann, expr, .. } => {
                if !matches!(pattern, ast::Pattern::Ident(_) | ast::Pattern::Wildcard) {
                    return Err("trans v1 拒绝域:非 Ident 绑定模式".into());
                }
                let (c, vty) = self.expr(expr)?;
                let vty = match ann {
                    Some(t) => match self.ty_of(t) {
                        VTy::Unknown => vty,
                        ann_ty => retag(vty, ann_ty),
                    },
                    None => vty,
                };
                match pattern {
                    ast::Pattern::Wildcard => {
                        self.w(1, &format!("(void)({});", c));
                    }
                    ast::Pattern::Ident(name) => {
                        let cname = self.bind(name, vty);
                        self.w(1, &format!("{} {} = {};", self.c_ty(vty), cname, c));
                    }
                    _ => unreachable!(),
                }
                Ok(())
            }
            ast::Stmt::Assign { target, op, value } => {
                let ast::Expr::Ident(name) = target else {
                    return Err("trans v1 拒绝域:赋值目标(仅 Ident)".into());
                };
                let Some((c, vty)) = self.lookup(name) else {
                    return Err(format!("trans:未绑定变量 `{}`", name));
                };
                let (v, v_vty) = self.expr(value)?;
                let rhs = match op {
                    ast::AssignOp::Eq => (v, v_vty),
                    other => {
                        let bin = match other {
                            ast::AssignOp::AddEq => ast::BinOp::Add,
                            ast::AssignOp::SubEq => ast::BinOp::Sub,
                            ast::AssignOp::MulEq => ast::BinOp::Mul,
                            ast::AssignOp::DivEq => ast::BinOp::Div,
                            _ => ast::BinOp::Mod,
                        };
                        self.binop(&bin, &format!("({})", c), vty, &v, v_vty)?
                    }
                };
                self.w(1, &format!("{} = (ct_i)({});", c, rhs.0));
                Ok(())
            }
            ast::Stmt::Return(e) => {
                let c = match e { Some(e) => self.expr(e)?.0, None => String::new() };
                self.w(1, &format!("return {};", c));
                Ok(())
            }
            ast::Stmt::Expr(e) => {
                let (c, _) = self.expr(e)?;
                self.w(1, &format!("(void)({});", c));
                Ok(())
            }
            ast::Stmt::While { cond, body } => {
                let (c, ty) = self.expr(cond)?;
                if !matches!(ty, VTy::Bool | VTy::Unknown) {
                    return Err("trans:while 条件需为 Bool".into());
                }
                self.w(1, &format!("while ({}) {{", c));
                self.emit_block_stmts(body)?;
                self.w(1, "}");
                Ok(())
            }
            ast::Stmt::For { pattern, iter, body } => {
                let ast::Pattern::Ident(name) = pattern else {
                    return Err("trans v1 拒绝域:for 非 Ident 模式".into());
                };
                let ast::Expr::Range { inclusive, from, to } = iter else {
                    return Err("trans v1 拒绝域:for 仅支持 range".into());
                };
                let (fc, ft) = self.expr(from.as_ref())?;
                if !matches!(ft, VTy::Int(_)) { return Err("trans:range 端点需整数".into()); }
                let (tcc, _) = self.expr(to.as_ref())?;
                let end = self.uniq_name("end");
                let it = self.uniq_name("it");
                self.w(1, &format!("{{ ct_i {} = (ct_i)({});", end, tcc));
                let cmp = if *inclusive { "<=" } else { "<" };
                self.w(1, &format!(
                    "for (ct_i {} = {}; {} {} {}; {}++) {{",
                    it, fc, it, cmp, end, it
                ));
                self.scope_push();
                let c = self.bind(name, VTy::Int(Some((IntW::W64, true))));
                self.w(1, &format!("ct_i {} = {};", c, it));
                self.emit_block_stmts(body)?;
                self.scope_pop();
                self.w(1, "}");
                self.w(1, "}");
                Ok(())
            }
        }
    }

    // ---------------- 表达式 ----------------

    fn expr(&mut self, e: &ast::Expr) -> TRes {
        match e {
            ast::Expr::Int { text, suffix } => {
                let cleaned = text.replace('_', "");
                Ok((format!("((ct_i){})", cleaned), suffix_ty(suffix)))
            }
            ast::Expr::Float { text, suffix } => {
                let cleaned = text.replace('_', "");
                let f32lit = suffix == "f32";
                Ok((
                    if f32lit { format!("({}f)", cleaned) } else { format!("({})", cleaned) },
                    if f32lit { VTy::F32 } else { VTy::F64 },
                ))
            }
            ast::Expr::Bool(b) => Ok(((*b as i32).to_string(), VTy::Bool)),
            ast::Expr::Void => Ok(("0".into(), VTy::Void)),
            ast::Expr::Ident(name) => {
                let Some((c, ty)) = self.lookup(name) else {
                    return Err(format!("trans:未绑定标识符 `{}`", name));
                };
                Ok((c, ty))
            }
            ast::Expr::Unary { op, expr } => {
                let (c, ty) = self.expr(expr)?;
                match op {
                    ast::UnOp::Not => Ok((format!("(!({}))", c), VTy::Bool)),
                    ast::UnOp::Neg => match ty {
                        VTy::Int(Some((_w, false))) => Err(
                            "trans:无符号取负恒溢出(域外)".into(),
                        ),
                        VTy::Int(Some((w, true))) => Ok((
                            format!("ct_sub(((ct_i)0), ({}), {}, 0)", c, wbits(w)),
                            VTy::Int(Some((w, true))),
                        )),
                        VTy::Int(None) => Ok((
                            format!("ct_sub(((ct_i)0), ({}), 64, 0)", c),
                            VTy::Int(None),
                        )),
                        VTy::F64 => Ok((format!("(-({}))", c), VTy::F64)),
                        VTy::F32 => Ok((format!("(-({}))", c), VTy::F32)),
                        _ => Err("trans:取负需数值".into()),
                    },
                }
            }
            ast::Expr::Binary { op, lhs, rhs } => {
                let (lc, lt) = self.expr(lhs)?;
                let (rc, rt) = self.expr(rhs)?;
                self.binop(op, &lc, lt, &rc, rt)
            }
            ast::Expr::If { cond, then, els } => {
                let (cc, ct) = self.expr(cond)?;
                if !matches!(ct, VTy::Bool | VTy::Unknown) {
                    return Err("trans:if 条件需为 Bool".into());
                }
                // 分支各自在独立子缓冲中生成;值经临时变量在使用点引用(语句提升)
                self.sink.push(String::new());
                let then_tail = self.emit_branch(then)?;
                let then_code = self.sink.pop().unwrap_or_default();
                let mut els_code = String::new();
                let els_tail = match els {
                    Some(e) => {
                        let blk = match &**e {
                            ast::Expr::BlockExpr(b) => (*b).clone(),
                            other => ast::Block { stmts: vec![], tail: Some(Box::new(other.clone())) },
                        };
                        self.sink.push(String::new());
                        let t = self.emit_branch(&blk)?;
                        els_code = self.sink.pop().unwrap_or_default();
                        t
                    }
                    None => None,
                };
                let t_ty = then_tail.as_ref().map(|(_, t)| *t).unwrap_or(VTy::Void);
                let e_ty = els_tail.as_ref().map(|(_, t)| *t).unwrap_or(VTy::Void);
                let merged = if els.is_some() { merge_ty(t_ty, e_ty) } else { VTy::Void };

                if merged.is_num() || merged == VTy::Bool {
                    let tv = self.uniq_name("ifv");
                    let (tc_, _) = then_tail.unwrap();
                    let (ec, _) = els_tail.unwrap();
                    self.w(1, &format!("{} {};", self.c_ty(merged), tv));
                    self.w(1, &format!("if ({}) {{", cc));
                    self.emit_lines(&then_code);
                    self.w(2, &format!("{} = {};", tv, tc_));
                    self.w(1, "} else {");
                    self.emit_lines(&els_code);
                    self.w(2, &format!("{} = {};", tv, ec));
                    self.w(1, "}");
                    Ok((tv, merged))
                } else {
                    // 无值 if:纯语句形态
                    self.w(1, &format!("if ({}) {{", cc));
                    self.emit_lines(&then_code);
                    if let Some((ec, _)) = els_tail {
                        self.w(1, "} else {");
                        self.emit_lines(&els_code);
                        self.w(2, &format!("(void)({});", ec));
                    }
                    self.w(1, "}");
                    Ok(("0".into(), VTy::Void))
                }
            }
            ast::Expr::BlockExpr(b) => {
                self.scope_push();
                for s in &b.stmts { self.emit_stmt(s)?; }
                let out = match &b.tail {
                    Some(t) => self.expr(t)?,
                    None => ("0".to_string(), VTy::Void),
                };
                self.scope_pop();
                Ok(out)
            }
            ast::Expr::Call { callee, args } => self.call(callee, args),
            ast::Expr::TypeArgs { expr, args } => {
                // as[T]() 显式转换(§3.6 截断语义)
                if let ast::Expr::Member { obj, target: ast::MemberTarget::Name(m) } = &**expr {
                    if m == "as" {
                        let targ = args.iter().filter_map(|t| match t {
                            ast::Type::Named { path, .. } => path.last().cloned(),
                            _ => None,
                        }).next().unwrap_or_default();
                        let (c, ty) = self.expr(obj)?;
                        if !ty.is_num() { return Err("trans:as 需数值".into()); }
                        return self.emit_as(&c, ty, &targ);
                    }
                }
                Err("trans v1 拒绝域:泛型实参".into())
            }
            ast::Expr::Range { .. } => Err("trans v1 拒绝域:range 作为值".into()),
            other => Err(format!(
                "trans v1 拒绝域:{}",
                match other {
                    ast::Expr::Str { .. } => "字符串",
                    ast::Expr::Array(_) | ast::Expr::Index { .. } => "数组",
                    ast::Expr::StructLit { .. } => "结构体字面量",
                    ast::Expr::Member { .. } => "成员访问",
                    ast::Expr::TypeArgs { .. } => "泛型实参/as",
                    ast::Expr::Try(_) => "? 错误传播",
                    ast::Expr::Closure { .. } => "闭包",
                    ast::Expr::Match { .. } => "match",
                    ast::Expr::Own { .. } => "own 块",
                    ast::Expr::Scope { .. } => "scope 块",
                    ast::Expr::Tuple(_) => "元组",
                    _ => "该表达式形态",
                }
            )),
        }
    }

    /// as[T]():int→int 截断/符号扩展;浮点→整 数截断;int→浮点 精确
    fn emit_as(&mut self, c: &str, from: VTy, targ: &str) -> TRes {
        let target = scalar_annotation(targ).ok_or_else(|| format!("trans:as 目标 `{}` 未支持", targ))?;
        match (from, target) {
            (VTy::Int(_), VTy::Int(w)) => {
                let (bits, us) = match w {
                    Some(x) => (wbits(x.0), (!x.1) as i32),
                    None => (64, 0),
                };
                Ok((format!("ct_as_ii({}, {}, {})", c, bits, us), VTy::Int(w)))
            }
            (VTy::Int(_), VTy::F64) => Ok((format!("((double)({}))", c), VTy::F64)),
            (VTy::Int(_), VTy::F32) => Ok((format!("((float)({}))", c), VTy::F32)),
            (VTy::F32 | VTy::F64, VTy::Int(w)) => {
                let (bits, us) = match w {
                    Some(x) => (wbits(x.0), (!x.1) as i32),
                    None => (64, 0),
                };
                Ok((format!("ct_as_fi({}, {}, {})", c, bits, us), VTy::Int(w)))
            }
            (VTy::F64, VTy::F32) => Ok((format!("((float)({}))", c), VTy::F32)),
            (VTy::F32, VTy::F64) => Ok((format!("((double)({}))", c), VTy::F64)),
            _ => Err("trans:as 转换类型不支持".into()),
        }
    }

    fn call(&mut self, callee: &ast::Expr, args: &[ast::Expr]) -> TRes {
        // x.as[T]() — as 的 TypeArgs 挂在 callee 位置
        if let ast::Expr::TypeArgs { expr, args: targ_args } = callee {
            if let ast::Expr::Member { obj, target: ast::MemberTarget::Name(m) } = &**expr {
                if m == "as" {
                    if !args.is_empty() { return Err("trans:as 不接受调用实参".into()); }
                    let targ = targ_args.iter().filter_map(|t| match t {
                        ast::Type::Named { path, .. } => path.last().cloned(),
                        _ => None,
                    }).next().unwrap_or_default();
                    let (c, ty) = self.expr(obj)?;
                    if !ty.is_num() { return Err("trans:as 需数值".into()); }
                    return self.emit_as(&c, ty, &targ);
                }
            }
        }
        if let ast::Expr::Ident(name) = callee {
            match name.as_str() {
                "assert" => {
                    let Some(a) = args.first() else { return Err("trans:assert 需实参".into()) };
                    let (c, at) = self.expr(a)?;
                    if !matches!(at, VTy::Bool | VTy::Unknown) {
                        return Err("trans:assert 需 Bool".into());
                    }
                    return Ok((format!("(ct_assert({}), 0)", c), VTy::Void));
                }
                "assert_eq" | "assert_ne" => {
                    if args.len() < 2 { return Err("trans:assert_eq 需两实参".into()); }
                    let (a, at) = self.expr(&args[0])?;
                    let (b, bt) = self.expr(&args[1])?;
                    let ok = if at.is_float() || bt.is_float() {
                        // 浮点:C == 语义(与解释器 values_equal 的数值比较对齐,NaN 不在语料)
                        format!("(({}) == ({}))", a, b)
                    } else if matches!(at, VTy::Bool) && matches!(bt, VTy::Bool) {
                        format!("((!!({})) == (!!({})))", a, b)
                    } else {
                        // 整数:i128 精确比较
                        let cmp = if name == "assert_eq" { "==" } else { "!=" };
                        format!("(({}) {} ({}))", a, cmp, b)
                    };
                    return Ok((format!("(ct_assert({}), 0)", ok), VTy::Void));
                }
                "panic" => {
                    if let Some(ast::Expr::Str { parts }) = args.first() {
                        if let [ast::StrPart::Text(t)] = parts.as_slice() {
                            let esc = t.replace('\\', "\\\\").replace('"', "\\\"");
                            return Ok((format!("(ct_panic(\"{}\"), 0)", esc), VTy::Void));
                        }
                    }
                    return Err("trans v1 拒绝域:非字面量 panic 消息".into());
                }
                _ => {}
            }
            if let Some((ptys, ret)) = self.lookup_fn(name) {
                if args.len() != ptys.len() {
                    return Err(format!("trans:函数 `{}` 实参数不符", name));
                }
                let mut cs = Vec::new();
                for (a, pt) in args.iter().zip(&ptys) {
                    let (c, at) = self.expr(a)?;
                    cs.push(coerce(self.c_ty(*pt), c, at, *pt));
                }
                return Ok((format!("{}({})", sanitize(name), cs.join(", ")), ret));
            }
        }
        Err("trans v1 拒绝域:该调用形态(仅内建/数值函数)".into())
    }

    fn binop(&mut self, op: &ast::BinOp, lc: &str, lt: VTy, rc: &str, rt: VTy) -> TRes {
        use ast::BinOp::*;
        match op {
            AndAnd => Ok((format!("(({}) && ({}))", lc, rc), VTy::Bool)),
            Eq | Ne | Lt | Gt | Le | Ge => {
                if lt.is_num() && rt.is_num() {
                    let c = match op {
                        Eq => "==", Ne => "!=", Lt => "<", Gt => ">", Le => "<=", _ => ">=",
                    };
                    Ok((format!("(({}) {} ({}))", lc, c, rc), VTy::Bool))
                } else if lt == VTy::Bool && rt == VTy::Bool {
                    let c = match op { Eq => "==", Ne => "!=", _ => return Err("trans:Bool 仅可 ==/!=".into()) };
                    Ok((format!("((!!({})) {} (!!({})))", lc, c, rc), VTy::Bool))
                } else {
                    Err("trans:比较需数值/Bool".into())
                }
            }
            Add | Sub | Mul | Div | Mod => {
                let (VTy::Int(lw), VTy::Int(rw)) = (lt, rt) else {
                    if lt.is_float() && rt.is_float() {
                        if float_kind(lt) != float_kind(rt) {
                            return Err("trans:F32/F64 混算".into());
                        }
                        let c = arith_char(op);
                        return Ok((format!("(({}) {} ({}))", lc, c, rc), lt));
                    }
                    return Err("trans:算术需数值".into());
                };
                let (w, bits, us) = merge_width(lw, rw);
                let fn_name = match op {
                    Add => "ct_add", Sub => "ct_sub", Mul => "ct_mul",
                    Div => "ct_div", _ => "ct_mod",
                };
                Ok((format!("{}({}, {}, {}, {})", fn_name, lc, rc, bits, us), VTy::Int(w)))
            }
            WrapAdd | WrapSub => {
                let (VTy::Int(lw), VTy::Int(rw)) = (lt, rt) else {
                    return Err("trans:回绕算术需整数".into());
                };
                let (w, bits, us) = merge_width(lw, rw);
                let f = if *op == WrapAdd { "ct_wadd" } else { "ct_wsub" };
                Ok((format!("{}({}, {}, {}, {})", f, lc, rc, bits, us), VTy::Int(w)))
            }
            Or => Err("trans:`or` 仅用于 Option(数值域外)".into()),
        }
    }
}

/// 返回 (宽度标记, bits, us);us 语义 = 1 无符号(VTy 的 bool = 有符号,注意取反)
fn merge_width(lw: Option<(IntW, bool)>, rw: Option<(IntW, bool)>) -> (Option<(IntW, bool)>, i32, i32) {
    match (lw, rw) {
        (Some(x), _) => (Some(x), wbits(x.0), (!x.1) as i32),
        (None, Some(x)) => (Some(x), wbits(x.0), (!x.1) as i32),
        (None, None) => (None, 64, 0),
    }
}

fn float_kind(t: VTy) -> u8 {
    if t == VTy::F32 { 1 } else { 2 }
}

fn arith_char(op: &ast::BinOp) -> char {
    use ast::BinOp::*;
    match op {
        Add => '+', Sub => '-', Mul => '*', Div => '/', _ => '%',
    }
}

fn retag(cur: VTy, ann: VTy) -> VTy {
    // let 注解 coerce:重标宽度不检查(对齐 interp coerce_literal)
    match (cur, ann) {
        (VTy::Int(_), VTy::Int(w)) => VTy::Int(w),
        (VTy::F64, VTy::F32) => VTy::F32,
        (VTy::F32, VTy::F64) => VTy::F64,
        _ => ann,
    }
}

fn merge_ty(a: VTy, b: VTy) -> VTy {
    if a == b { return a; }
    match (a, b) {
        (VTy::Int(_), VTy::Int(_)) => VTy::Int(None),
        _ => VTy::Unknown,
    }
}

fn coerce(c_ty: &str, c: String, from: VTy, to: VTy) -> String {
    match (from, to) {
        (VTy::F64, VTy::F32) => format!("(float)({})", c),
        (VTy::F32, VTy::F64) => format!("(double)({})", c),
        (VTy::Int(_), VTy::Int(_)) | (VTy::Int(_), VTy::Unknown) if c_ty == "ct_i" => c,
        _ => c,
    }
}

fn sanitize(name: &str) -> String {
    format!("ctn_{}", name)
}

fn decl_name(d: &ast::Decl) -> &str {
    match d {
        ast::Decl::Use(_) => "use",
        ast::Decl::Struct(_) => "struct",
        ast::Decl::Class(_) => "class",
        ast::Decl::Enum(_) => "enum",
        ast::Decl::Trait(_) => "trait",
        ast::Decl::Impl(_) => "impl",
        ast::Decl::Const(_) => "const",
        ast::Decl::Static(_) => "static",
        _ => "?",
    }
}

const PREAMBLE: &str = r#"/* Ctron → C 转译产物(P1-E① 数值域;语义契约 = Rust 解释器) */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

typedef __int128 ct_i;
static const char* ct_cur_test = "";

static void ct_panic(const char* msg) {
    fprintf(stderr, "panic: %s (test %s)\n", msg, ct_cur_test);
    exit(1);
}
static int ct_assert(int ok) {
    if (!ok) { fprintf(stderr, "assertion failed (test %s)\n", ct_cur_test); exit(1); }
    return ok;
}
/* 宽度检查算术(bits ≤ 64;us = 无符号)。结果宽度 = 调用点左操作数宽度。 */
static ct_i ct_add(ct_i a, ct_i b, int bits, int us) {
    ct_i r = a + b;
    ct_i lo = us ? 0 : -(((ct_i)1) << (bits - 1));
    ct_i hi = us ? ((((ct_i)1) << bits) - 1) : ((((ct_i)1) << (bits - 1)) - 1);
    if (r < lo || r > hi) ct_panic("integer overflow");
    return r;
}
static ct_i ct_sub(ct_i a, ct_i b, int bits, int us) {
    ct_i r = a - b;
    ct_i lo = us ? 0 : -(((ct_i)1) << (bits - 1));
    ct_i hi = us ? ((((ct_i)1) << bits) - 1) : ((((ct_i)1) << (bits - 1)) - 1);
    if (r < lo || r > hi) ct_panic("integer overflow");
    return r;
}
static ct_i ct_mul(ct_i a, ct_i b, int bits, int us) {
    ct_i r = a * b;
    ct_i lo = us ? 0 : -(((ct_i)1) << (bits - 1));
    ct_i hi = us ? ((((ct_i)1) << bits) - 1) : ((((ct_i)1) << (bits - 1)) - 1);
    if (r < lo || r > hi) ct_panic("integer overflow");
    return r;
}
static ct_i ct_div(ct_i a, ct_i b, int bits, int us) {
    if (b == 0) ct_panic("division by zero");
    (void)bits; (void)us;
    return a / b;
}
static ct_i ct_mod(ct_i a, ct_i b, int bits, int us) {
    if (b == 0) ct_panic("division by zero");
    (void)bits; (void)us;
    return a % b;
}
/* as[T]() 显式转换:int→int 截断(§3.6);浮点→整数截断 */
static ct_i ct_as_ii(ct_i a, int bits, int us) {
    ct_i m = ((((ct_i)1) << bits) - 1);
    ct_i t = a & m;
    if (!us && t >= (((ct_i)1) << (bits - 1))) t -= (((ct_i)1) << bits);
    return t;
}
static ct_i ct_as_fi(double d, int bits, int us) {
    ct_i t = (ct_i)d; /* C 浮点→整数转换 = 向零截断 */
    ct_i m = ((((ct_i)1) << bits) - 1);
    t = t & m;
    if (!us && t >= (((ct_i)1) << (bits - 1))) t -= (((ct_i)1) << bits);
    return t;
}
/* 回绕:二补截断到宽度(§3.6 +%/-%) */
static ct_i ct_wadd(ct_i a, ct_i b, int bits, int us) {
    ct_i m = ((((ct_i)1) << bits) - 1);
    ct_i t = (a + b) & m;
    if (!us && t >= (((ct_i)1) << (bits - 1))) t -= (((ct_i)1) << bits);
    return t;
}
static ct_i ct_wsub(ct_i a, ct_i b, int bits, int us) {
    ct_i m = ((((ct_i)1) << bits) - 1);
    ct_i t = (a - b) & m;
    if (!us && t >= (((ct_i)1) << (bits - 1))) t -= (((ct_i)1) << bits);
    return t;
}
"#;
