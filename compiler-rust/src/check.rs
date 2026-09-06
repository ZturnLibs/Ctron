//! 语义检查:双向类型检查、Send 三检查点、分配效果、穷尽性、pure/caps/comptime。
//! 诊断码:E2010/E2020/E2030/E3010/E3020/E3031/E3040/E3060/E4010/E4020/E4030/E6010/E6020/W8020。

use crate::ast::{self, Expr};
use crate::sem::{self, DefId, DefKind, IntW, Profile, Sema, Symbol, Ty};
use crate::ast::BinOp;
use crate::token::{Diagnostic, Span};
use std::collections::{HashMap, HashSet};

pub fn parse_manifest(src: &str) -> sem::Manifest {
    let mut caps = HashMap::new();
    let mut budget = 100_000u64;
    let mut section = String::new();
    for line in src.lines() {
        let line = line.trim();
        if line.starts_with('[') && line.ends_with(']') {
            section = line[1..line.len() - 1].to_string();
            continue;
        }
        if let Some((k, v)) = line.split_once('=') {
            let k = k.trim().to_string();
            let v = v.trim().trim_matches('"').to_string();
            match section.as_str() {
                "caps" => caps.insert(k, v == "true"),
                "comptime" if k == "budget_ms" => {
                    budget = v.parse().unwrap_or(100_000) * 1000;
                    None
                }
                _ => None,
            };
        }
    }
    sem::Manifest { caps, comptime_budget_steps: budget }
}

pub fn check_src(src: &str, profile: Profile) -> Vec<Diagnostic> {
    check_package(&[("".to_string(), src.to_string())], None, profile)
        .into_iter().next().map(|(_, d)| d).unwrap_or_default()
}

pub fn check_package(
    files: &[(String, String)],
    manifest: Option<sem::Manifest>,
    profile: Profile,
) -> Vec<(String, Vec<Diagnostic>)> {
    let parsed: Vec<(String, ast::File)> = files.iter()
        .map(|(m, s)| (m.clone(), crate::parse_src(s).0)).collect();
    let (sema, mut per_module) = sem::build_package(files, manifest, profile);

    // 每模块的 fn id 表(body 检查用)
    let mut checked: HashSet<usize> = HashSet::new();
    // trait 方法 #[no_alloc] 契约表
    let mut tmap: HashMap<(String, String), bool> = HashMap::new();
    for pf in &parsed {
        for d in &pf.1.decls {
            if let ast::Decl::Trait(t) = d {
                for item in &t.items {
                    if let ast::TraitItem::Method(m) = item {
                        let na = m.attrs.iter().any(|a| a.name == "no_alloc");
                        tmap.insert((t.name.clone(), m.name.clone()), na);
                    }
                }
            }
        }
    }
    for (i, (mpath, ast_file)) in parsed.iter().enumerate() {
        let mut c = Checker::new(&sema, mpath.clone(), tmap.clone());
        for d in &ast_file.decls {
            c.check_top(d, &mut checked);
        }
        let diags = c.finish_caps();
        per_module[i].1.extend(diags);

        // W8010(§5.2):struct 含类引用字段 → 拷贝浅共享 lint(每 struct 一次)
        for (di, d) in sema.defs.iter().enumerate() {
            if d.kind != DefKind::Struct { continue; }
            let shallow: Vec<&str> = d.fields.iter().filter_map(|(_, t, _)| match t {
                Ty::Named { def, .. } if matches!(sema.defs[*def].kind, DefKind::Class) =>
                    Some(sema.defs[*def].name.as_str()),
                _ => None,
            }).collect();
            if !shallow.is_empty() {
                per_module[i].1.push(Diagnostic {
                    code: "W8010",
                    message: format!(
                        "struct `{}` 含类引用字段({}),拷贝为浅共享;需要深拷贝请显式克隆或改用 own",
                        d.name, shallow.join(", ")),
                    span: Span::new(1, 1, 0, 0),
                });
                let _ = di;
            }
        }
    }
    per_module
}

struct Local { ty: Ty }

pub struct Checker<'a> {
    sema: &'a Sema,
    moved: std::collections::HashSet<String>,   // E3050:own 块内已 move 的 arena 句柄
    handles: std::collections::HashSet<String>, // own 块内持有 arena 句柄的绑定
    subs: HashMap<u32, Ty>,
    scopes: Vec<HashMap<String, Local>>,
    diags: Vec<Diagnostic>,
    module: String,
    cur_ret: Ty,
    in_own: bool,
    no_alloc_ctx: bool,
    no_spawn_ctx: bool,
    pure_ctx: bool,
    comptime_ctx: bool,
    fresh: u32,
    depth: u32,
    caps_used: HashSet<String>,
    gc_alloc_fns: HashMap<usize, bool>,
    any_alloc_fns: HashMap<usize, bool>,
    trait_method_alloc_map: HashMap<(String, String), bool>,
}

impl<'a> Checker<'a> {
    pub fn new(sema: &'a Sema, module: String, trait_method_alloc_map: HashMap<(String, String), bool>) -> Self {
        let mut c = Checker {
            sema, subs: HashMap::new(), scopes: vec![HashMap::new()],
            diags: Vec::new(), module, cur_ret: Ty::Void, fresh: 0,
            moved: std::collections::HashSet::new(),
            handles: std::collections::HashSet::new(),
            in_own: false, no_alloc_ctx: false, no_spawn_ctx: false,
            pure_ctx: false, comptime_ctx: false, depth: 0,
            caps_used: HashSet::new(), gc_alloc_fns: HashMap::new(), any_alloc_fns: HashMap::new(),
            trait_method_alloc_map,
        };
        // 分配效果定点求解(全部用户 fn)
        let ids: Vec<usize> = (0..sema.fns.len()).collect();
        for id in ids { c.gc_alloc_fns.insert(id, false); c.any_alloc_fns.insert(id, false); }
        let mut changed = true;
        let mut round = 0;
        while changed && round < 32 {
            changed = false;
            round += 1;
            for id in 0..sema.fns.len() {
                let body = sema.fns[id].body.clone();
                let Some(body) = body else { continue };
                let g = c.scan_gc_alloc(&body, &mut HashSet::new());
                let a = c.scan_any_alloc(&body, &mut HashSet::new());
                if g && !c.gc_alloc_fns[&id] { c.gc_alloc_fns.insert(id, true); changed = true; }
                if a && !c.any_alloc_fns[&id] { c.any_alloc_fns.insert(id, true); changed = true; }
            }
        }
        c
    }

    fn fresh_var(&mut self) -> Ty { self.fresh += 1; Ty::Var(0x8000_0000 + self.fresh) }

    fn named(&mut self, name: &str, args: Vec<Ty>) -> Ty {
        // 原生标量名 → Ty 原生表示(避免 Named/原生双表示)
        if args.is_empty() {
            match name {
                "Str" => return Ty::Str, "String" => return Ty::String,
                "Bool" => return Ty::Bool, "Void" => return Ty::Void,
                "Never" => return Ty::Never,
                "I8" => return Ty::Int(IntW::W8), "I16" => return Ty::Int(IntW::W16),
                "I32" => return Ty::Int(IntW::W32), "I64" => return Ty::Int(IntW::W64),
                "ISize" => return Ty::Int(IntW::WSize),
                "U8" => return Ty::UInt(IntW::W8), "U16" => return Ty::UInt(IntW::W16),
                "U32" => return Ty::UInt(IntW::W32), "U64" => return Ty::UInt(IntW::W64),
                "USize" => return Ty::UInt(IntW::WSize),
                "F32" => return Ty::F32, "F64" => return Ty::F64,
                _ => {}
            }
        }
        match self.sema.def_by_name.get(name) {
            Some(&def) => Ty::Named { def, args },
            None => Ty::Err,
        }
    }

    pub fn finish_caps(mut self) -> Vec<Diagnostic> {
        // 能力审计(E4010):仅多文件包(有 manifest)执行
        if let Some(m) = &self.sema.manifest {
            for key in &self.caps_used {
                if !m.caps.get(key).copied().unwrap_or(false) {
                    self.diags.push(Diagnostic {
                        code: "E4010",
                        message: format!("能力使用超出 manifest 声明:{key}(缺声明;在 Ctron.toml [caps] 增加 {key} = true)"),
                        span: Span::new(1, 1, 0, 0),
                    });
                }
            }
        }
        self.diags
    }

    // ---------- 基础 ----------

    fn err(&mut self, code: &'static str, msg: String, span: Span) {
        self.diags.push(Diagnostic { code, message: msg, span });
    }

    fn resolve(&self, ty: &Ty) -> Ty {
        match ty {
            Ty::Var(v) => match self.subs.get(v) {
                Some(t) => self.resolve(t),
                None => ty.clone(),
            },
            other => other.clone(),
        }
    }

    fn unify(&mut self, a: &Ty, b: &Ty) -> bool {
        let a = self.resolve(a);
        let b = self.resolve(b);
        match (&a, &b) {
            (Ty::Var(x), Ty::Var(y)) if x == y => true,
            (Ty::Var(x), _) => { self.subs.insert(*x, b.clone()); true }
            (_, Ty::Var(x)) => { self.subs.insert(*x, a.clone()); true }
            (Ty::Err, _) | (_, Ty::Err) => true,
            (Ty::Named { def: d1, args: a1 }, Ty::Named { def: d2, args: a2 }) => {
                d1 == d2 && a1.len() == a2.len()
                    && a1.iter().zip(a2).all(|(x, y)| self.unify(x, y))
            }
            (Ty::Tuple(x), Ty::Tuple(y)) => x.len() == y.len() && x.iter().zip(y).all(|(u, v)| self.unify(u, v)),
            (Ty::MutSlice(x), Ty::MutSlice(y)) | (Ty::RoSlice(x), Ty::RoSlice(y)) => self.unify(x, y),
            (Ty::Ref(x), Ty::Ref(y)) => self.unify(x, y),
            (Ty::Array(x), Ty::Array(y)) => self.unify(x, y),
            (Ty::Optional(x), Ty::Optional(y)) => self.unify(x, y),
            (Ty::FnTy { params: p1, ret: r1 }, Ty::FnTy { params: p2, ret: r2 }) =>
                p1.len() == p2.len() && p1.iter().zip(p2).all(|(u, v)| self.unify(u, v)) && self.unify(r1, r2),
            (Ty::Range(x), Ty::Range(y)) => self.unify(x, y),
            (Ty::Simd(x), Ty::Simd(y)) => self.unify(x, y),
            (Ty::Int(x), Ty::Int(y)) => x == y,
            (Ty::UInt(x), Ty::UInt(y)) => x == y,
            _ => a == b,
        }
    }

    /// 数值宽容:跨宽度整数比较/算术按左侧;用于比较与算术运算的接受判定
    fn both_numeric(a: &Ty, b: &Ty) -> bool {
        matches!(a, Ty::Int(_) | Ty::UInt(_) | Ty::F32 | Ty::F64)
            && matches!(b, Ty::Int(_) | Ty::UInt(_) | Ty::F32 | Ty::F64)
    }

    fn is_int(&self, ty: &Ty) -> bool {
        matches!(self.resolve(ty), Ty::Int(_) | Ty::UInt(_))
    }

    fn is_gc_class_name(&self, name: &str) -> bool {
        self.sema.def_by_name.get(name)
            .map(|&d| matches!(self.sema.defs[d].kind, DefKind::Class))
            .unwrap_or(false)
    }


    fn is_gc_class(&self, ty: &Ty) -> bool {
        match self.resolve(ty) {
            Ty::Named { def, .. } => matches!(self.sema.defs[def].kind, DefKind::Class),
            _ => false,
        }
    }

    fn type_name(&self, ty: &Ty) -> String {
        match self.resolve(ty) {
            Ty::Named { def, .. } => self.sema.defs[def].name.clone(),
            Ty::Str => "Str".into(), Ty::String => "String".into(),
            Ty::Int(w) => match w { IntW::W8 => "I8".into(), IntW::W16 => "I16".into(), IntW::W32 => "I32".into(), IntW::W64 => "I64".into(), IntW::WSize => "ISize".into() },
            Ty::UInt(w) => match w { IntW::W8 => "U8".into(), IntW::W16 => "U16".into(), IntW::W32 => "U32".into(), IntW::W64 => "U64".into(), IntW::WSize => "USize".into() },
            Ty::F32 => "F32".into(), Ty::F64 => "F64".into(), Ty::Bool => "Bool".into(),
            Ty::Void => "Void".into(), Ty::MutSlice(e) => format!("{}[]", self.type_name(&e)),
            Ty::RoSlice(e) => format!("&{}[]", self.type_name(&e)),
            Ty::Optional(e) => format!("{}?", self.type_name(&e)),
            Ty::Ref(e) => format!("&{}", self.type_name(&e)),
            _ => "?".into(),
        }
    }

    // ---------- Send(§7.4) ----------

    fn is_send(&self, ty: &Ty) -> bool {
        let mut seen = HashSet::new();
        self.is_send_seen(ty, &mut seen)
    }

    fn is_send_seen(&self, ty: &Ty, seen: &mut HashSet<DefId>) -> bool {
        // 递归类型(Node? / 链表字段)防爆栈:def 只访问一次
        match self.resolve(ty) {
            Ty::Named { def, .. } if !seen.insert(def) => return true,
            _ => {}
        }
        match self.resolve(ty) {
            Ty::Err | Ty::Void | Ty::Never | Ty::Bool | Ty::Str | Ty::String
            | Ty::Int(_) | Ty::UInt(_) | Ty::F32 | Ty::F64 | Ty::Range(_) | Ty::ComptimeVal(_) => true,
            Ty::Simd(_) => true,
            Ty::MutSlice(_) => false,
            Ty::RoSlice(e) => self.is_send_seen(&e, seen),
            Ty::Ref(e) => {
                // &Trait 恒非 Send(v0.4);&T 共享只读视图按 T
                let e2 = self.resolve(e.as_ref());
                match &e2 {
                    Ty::Named { def, .. } if self.sema.defs[*def].kind == DefKind::Trait =>
                        self.sema.defs[*def].name == "AnyError",
                    _ => self.is_send_seen(&e2, seen),
                }
            }
            Ty::Array(e) | Ty::Optional(e) => self.is_send_seen(&e, seen),
            Ty::Tuple(items) => items.iter().all(|t| self.is_send_seen(t, seen)),
            Ty::Named { def, args } => {
                let d = &self.sema.defs[def];
                match d.name.as_str() {
                    "Mutex" | "Atomic" | "Global" => true,
                    "Option" | "Result" | "Box" | "List" | "Sender" | "Receiver" | "Task" =>
                        args.iter().all(|a| self.is_send_seen(a, seen)),
                    _ => match d.kind {
                        DefKind::Class => d.fields.iter().all(|(_, t, is_var)| !is_var && self.is_send_seen(t, seen)),
                        DefKind::Struct => d.fields.iter().all(|(_, t, _)| self.is_send_seen(t, seen)),
                        DefKind::Enum => d.variants.iter().all(|(_, ps)| ps.iter().all(|t| self.is_send_seen(t, seen))),
                        DefKind::Trait => false, // &Trait 已在上层处理;裸 trait 名按非 Send
                        DefKind::Prelude => true,
                    },
                }
            }
            Ty::FnTy { .. } | Ty::Ctor { .. } => false,
            Ty::Var(_) => true, // 未定变量保守放行(避免误报)
        }
    }

    // ---------- 分配效果 ----------

    fn fn_gc_alloc(&self, id: usize) -> bool { self.gc_alloc_fns.get(&id).copied().unwrap_or(false) }
    fn fn_any_alloc(&self, id: usize) -> bool { self.any_alloc_fns.get(&id).copied().unwrap_or(false) }

    fn scan_gc_alloc(&self, block: &ast::Block, seen: &mut HashSet<String>) -> bool {
        self.scan_block_gc(&block.stmts, block.tail.as_deref(), seen)
    }

    fn scan_block_gc(&self, stmts: &[ast::Stmt], tail: Option<&ast::Expr>, seen: &mut HashSet<String>) -> bool {
        stmts.iter().any(|s| self.scan_stmt_gc(s, seen))
            || tail.map(|e| self.scan_expr_gc(e, seen)).unwrap_or(false)
    }

    fn scan_stmts_gc(&self, stmts: &[ast::Stmt], tail: Option<&ast::Expr>, seen: &mut HashSet<String>) -> bool {
        stmts.iter().any(|s| self.scan_stmt_gc(s, seen))
            || tail.map(|e| self.scan_expr_gc(e, seen)).unwrap_or(false)
    }

    fn scan_stmts_any(&self, stmts: &[ast::Stmt], tail: Option<&ast::Expr>, seen: &mut HashSet<String>) -> bool {
        stmts.iter().any(|s| self.scan_stmt_any(s, seen))
            || tail.map(|e| self.scan_expr_any(e, seen)).unwrap_or(false)
    }

    fn scan_stmt_gc(&self, s: &ast::Stmt, seen: &mut HashSet<String>) -> bool {
        match s {
            ast::Stmt::Let { expr, .. } => self.scan_expr_gc(expr, seen),
            ast::Stmt::Return(e) => e.as_ref().map(|e| self.scan_expr_gc(e, seen)).unwrap_or(false),
            ast::Stmt::For { iter, body, .. } => self.scan_expr_gc(iter, seen) || self.scan_stmts_gc(body.stmts.as_slice(), body.tail.as_deref(), seen),
            ast::Stmt::While { cond, body } => self.scan_expr_gc(cond, seen) || self.scan_stmts_gc(body.stmts.as_slice(), body.tail.as_deref(), seen),
            ast::Stmt::Assign { value, .. } => self.scan_expr_gc(value, seen),
            ast::Stmt::Expr(e) => self.scan_expr_gc(e, seen),
        }
    }

    fn scan_expr_gc(&self, e: &ast::Expr, seen: &mut HashSet<String>) -> bool {
        use ast::Expr::*;
        match e {
            Str { parts } => parts.iter().any(|p| matches!(p, ast::StrPart::Interp(_))),
            StructLit { path, .. } => {
                // 仅类(引用类型)构造是 GC 分配;值 struct 是栈/内联(§6.1)
                path.last().map(|n| self.is_gc_class_name(n)).unwrap_or(false)
            }
            Unary { expr, .. } | Try(expr) => self.scan_expr_gc(expr, seen),
            Binary { lhs, rhs, .. } | Range { from: lhs, to: rhs, .. } =>
                self.scan_expr_gc(lhs, seen) || self.scan_expr_gc(rhs, seen),
            Call { callee, args } => {
                if self.scan_expr_gc(callee, seen) || args.iter().any(|a| self.scan_expr_gc(a, seen)) { return true; }
                // 直接调用用户 fn → 传播
                if let Ident(name) = &**callee {
                    if let Some(Symbol::Fn(id)) = self.lookup_fn(name) {
                        if seen.contains(name) { return false; }
                        seen.insert(name.clone());
                        let r = self.fn_gc_alloc(id);
                        seen.remove(name);
                        return r;
                    }
                }
                // to_string / 构造类调用
                if let Expr::Member { target: ast::MemberTarget::Name(m), .. } = &**callee {
                    if m == "to_string" { return true; }
                }
                if let Expr::TypeArgs { expr, .. } = &**callee {
                    if let Expr::Ident(n) = &**expr {
                        if matches!(n.as_str(), "Box" | "String" | "StringBuilder" | "List" | "Map" | "Set") { return true; }
                    }
                }
                false
            }
            Index { obj, index } => self.scan_expr_gc(obj, seen) || self.scan_expr_gc(index, seen),
            Member { obj, .. } => self.scan_expr_gc(obj, seen),
            TypeArgs { expr, .. } => self.scan_expr_gc(expr, seen),
            If { cond, then, els } => self.scan_expr_gc(cond, seen)
                || self.scan_block_gc(then.stmts.as_slice(), then.tail.as_deref(), seen)
                || els.as_ref().map(|e| self.scan_expr_gc(e, seen)).unwrap_or(false),
            Match { expr, arms } => self.scan_expr_gc(expr, seen)
                || arms.iter().any(|a| self.scan_expr_gc(&a.expr, seen)),
            Closure { body, .. } => self.scan_expr_gc(body, seen),
            Scope { body, .. } => self.scan_block_gc(body.stmts.as_slice(), body.tail.as_deref(), seen),
            BlockExpr(b) => self.scan_block_gc(b.stmts.as_slice(), b.tail.as_deref(), seen),
            _ => false,
        }
    }

    fn scan_any_alloc(&self, block: &ast::Block, seen: &mut HashSet<String>) -> bool {
        self.scan_gc_alloc(block, seen)
            || block.stmts.iter().any(|s| self.scan_stmt_any(s, seen))
            || block.tail.as_ref().map(|e| self.scan_expr_any(e, seen)).unwrap_or(false)
    }

    fn scan_stmt_any(&self, s: &ast::Stmt, seen: &mut HashSet<String>) -> bool {
        match s {
            ast::Stmt::Let { expr, .. } => self.scan_expr_any(expr, seen),
            ast::Stmt::Return(e) => e.as_ref().map(|e| self.scan_expr_any(e, seen)).unwrap_or(false),
            ast::Stmt::For { iter, body, .. } => self.scan_expr_any(iter, seen) || self.scan_stmts_any(body.stmts.as_slice(), body.tail.as_deref(), seen),
            ast::Stmt::While { cond, body } => self.scan_expr_any(cond, seen) || self.scan_stmts_any(body.stmts.as_slice(), body.tail.as_deref(), seen),
            ast::Stmt::Assign { value, .. } => self.scan_expr_any(value, seen),
            ast::Stmt::Expr(e) => self.scan_expr_any(e, seen),
        }
    }

    fn scan_expr_any(&self, e: &ast::Expr, seen: &mut HashSet<String>) -> bool {
        if self.scan_expr_gc(e, seen) { return true; }
        use ast::Expr::*;
        match e {
            Call { callee, args } => {
                if args.iter().any(|a| self.scan_expr_any(a, seen)) { return true; }
                if let Expr::Member { obj, target: ast::MemberTarget::Name(m), .. } = &**callee {
                    // 拒绝表按接收者判定:ArenaList 是 arena 后备,不算分配(§6.6)
                    if matches!(m.as_str(), "push" | "pop" | "insert" | "remove") {
                        // 接收者是 arena.xxx 成员链(ArenaList)→ 不算分配
                        let recv_is_arena = matches!(obj.as_ref(),
                            Expr::Member { obj: recv_obj, .. }
                                if matches!(**recv_obj, Expr::Ident(ref n) if n == "arena"));
                        if !recv_is_arena { return true; }
                    }
                }
                if let Ident(name) = &**callee {
                    if let Some(Symbol::Fn(id)) = self.lookup_fn(name) {
                        if seen.contains(name) { return false; }
                        seen.insert(name.clone());
                        let r = self.fn_any_alloc(id);
                        seen.remove(name);
                        return r;
                    }
                }
                false
            }
            Unary { expr, .. } | Try(expr) => self.scan_expr_any(expr, seen),
            Binary { lhs, rhs, .. } | Range { from: lhs, to: rhs, .. } =>
                self.scan_expr_any(lhs, seen) || self.scan_expr_any(rhs, seen),
            Index { obj, index } => self.scan_expr_any(obj, seen) || self.scan_expr_any(index, seen),
            Member { obj, .. } => self.scan_expr_any(obj, seen),
            TypeArgs { expr, .. } => self.scan_expr_any(expr, seen),
            If { cond, then, els } => self.scan_expr_any(cond, seen)
                || self.scan_block_any(then.stmts.as_slice(), then.tail.as_deref(), seen)
                || els.as_ref().map(|x| self.scan_expr_any(x, seen)).unwrap_or(false),
            Match { expr, arms } => self.scan_expr_any(expr, seen)
                || arms.iter().any(|a| self.scan_expr_any(&a.expr, seen)),
            Closure { body, .. } => self.scan_expr_any(body, seen),
            Scope { body, .. } => self.scan_block_any(body.stmts.as_slice(), body.tail.as_deref(), seen),
            BlockExpr(b) => self.scan_block_any(b.stmts.as_slice(), b.tail.as_deref(), seen),
            _ => false,
        }
    }

    fn scan_block_any(&self, stmts: &[ast::Stmt], tail: Option<&ast::Expr>, seen: &mut HashSet<String>) -> bool {
        self.scan_stmts_gc(stmts, tail, seen) || self.scan_stmts_any(stmts, tail, seen)
    }


    fn lookup_fn(&self, name: &str) -> Option<Symbol> {
        for scope in self.scopes.iter().rev() {
            if scope.contains_key(name) { return None; } // 被局部遮蔽 → 不是顶层 fn
        }
        self.sema.mods.iter().find(|m| m.path == self.module)
            .and_then(|m| m.symbols.get(name)).cloned()
    }

    fn lookup_local(&self, name: &str) -> Option<Ty> {
        for scope in self.scopes.iter().rev() {
            if let Some(l) = scope.get(name) { return Some(self.resolve(&l.ty)); }
        }
        None
    }

    fn lookup_type(&self, name: &str) -> Option<DefId> {
        self.sema.def_by_name.get(name).copied()
    }

    fn module_static_ty(&self, name: &str) -> Ty {
        self.sema.mods.iter().find(|m| m.path == self.module)
            .and_then(|m| m.symbols.get(name))
            .and_then(|s| match s { Symbol::Static { ty } => Some(ty.clone()), _ => None })
            .unwrap_or(Ty::Err)
    }

    // ---------- 顶层 ----------

    fn check_top(&mut self, d: &ast::Decl, checked: &mut HashSet<usize>) {
        match d {
            ast::Decl::Fn(fun) => {
                if let Some(Symbol::Fn(id)) = self.sema.mods.iter()
                    .find(|m| m.path == self.module)
                    .and_then(|m| m.symbols.get(&fun.name)).cloned()
                {
                    if checked.insert(id) { self.check_user_fn(id, fun); }
                }
            }
            ast::Decl::Test(t) => {
                self.scopes.push(HashMap::new());
                self.check_block(&t.body);
                self.scopes.pop();
            }
            ast::Decl::Const(c) => {
                self.check_const(c);
            }
            ast::Decl::Static(s) => {
                let ty = self.module_static_ty(&s.name);
                let ty = self.resolve(&ty);
                if !self.is_send(&ty) {
                    self.err("E3031",
                        format!("非 Send 类型不可作为静态存储:{} 含 var 字段(§7.4);可变全局唯一路径是 Global[T]", s.name),
                        Span::new(1, 1, 0, 0));
                }
            }
            ast::Decl::Impl(im) => self.check_impl(im),
            _ => {}
        }
    }

    fn check_user_fn(&mut self, id: usize, fun: &ast::FnDecl) {
        let def = &self.sema.fns[id];
        // v1 限制:带 trait bound 的泛型函数体内不做 trait 方法解析(show 等),
        // 留给单态化后检查;此处仅检查签名。
        let has_bound_param = fun.type_params.iter()
            .any(|tp| tp.bound.iter().any(|b| b == "Show" || b == "Eq" || b == "Hash" || b == "Clone" || b == "Iter"));
        if has_bound_param {
            self.no_alloc_ctx = def.no_alloc || self.sema.profile == Profile::Bare;
            if self.no_alloc_ctx && self.any_alloc_fns.get(&id).copied().unwrap_or(false) {
                self.err("E3040", format!("`{}` 声明/语境要求无分配,但函数体含分配", def.name), Span::new(1, 1, 0, 0));
            }
            self.no_alloc_ctx = false;
            return;
        }
        let param_tys: Vec<Ty> = def.params.iter().map(|(_, t)| t.clone()).collect();
        let ret = def.ret.clone();
        self.cur_ret = ret;
        self.no_alloc_ctx = def.no_alloc || self.sema.profile == Profile::Bare;
        self.no_spawn_ctx = def.no_spawn;
        self.pure_ctx = def.pure;
        self.comptime_ctx = def.is_comptime;
        // 能力使用:参数引用 Cap trait
        for ty in &param_tys {
            self.note_cap_use(ty);
        }
        self.scopes.push(HashMap::new());
        for ((name, ty), _) in def.params.iter().zip(param_tys.iter()) {
            self.scopes.last_mut().unwrap().insert(name.clone(), Local { ty: ty.clone() });
        }
        if let Some(body) = &def.body {
            self.check_block(body);
        }
        self.scopes.pop();
        let _ = fun;
        // #[no_alloc] / bare:函数体不得有任何分配
        if self.no_alloc_ctx && self.any_alloc_fns.get(&id).copied().unwrap_or(false) {
            self.err("E3040",
                format!("`{}` 声明/语境要求无分配(no allocation),但函数体含 GC/String/集合分配", def.name),
                Span::new(1, 1, 0, 0));
        }
        self.no_alloc_ctx = false;
        self.no_spawn_ctx = false;
        self.pure_ctx = false;
        self.comptime_ctx = false;
    }

    fn note_cap_use(&mut self, ty: &Ty) {
        match self.resolve(ty) {
            Ty::Ref(inner) => self.note_cap_use(&inner),
            Ty::Named { def, .. } => {
                if let Some(key) = self.sema.cap_key_by_def.get(&def).cloned() {
                    self.caps_used.insert(key);
                }
            }
            _ => {}
        }
    }

    fn check_const(&mut self, c: &ast::ConstDecl) {
        // comptime 求值(预算)——自递归不终止 → E6010
        if let Expr::Call { callee, args } = &c.expr {
            if let Expr::Ident(name) = &**callee {
                if let Some(Symbol::Fn(id)) = self.lookup_fn_global(name) {
                    if self.sema.fns[id].is_comptime {
                        let budget = self.sema.manifest.as_ref()
                            .map(|m| m.comptime_budget_steps).unwrap_or(100_000);
                        let argvals: Vec<i64> = args.iter().filter_map(|e| self.const_int(e)).collect();
                        let mut steps = 0u64;
                        let mut depth = 0u32;
                        if self.eval_comptime(name, &argvals, budget, &mut steps, &mut depth).is_err() {
                            self.err("E6010",
                                format!("comptime 预算超限(comptime budget exceeded):`{}` 求值步数越界", c.name),
                                Span::new(1, 1, 0, 0));
                        }
                    }
                }
            }
        }
    }

    fn lookup_fn_global(&self, name: &str) -> Option<Symbol> {
        self.sema.mods.iter().find(|m| m.path == self.module)
            .and_then(|m| m.symbols.get(name)).cloned()
    }

    fn const_int(&self, e: &ast::Expr) -> Option<i64> {
        if let Expr::Int { text, .. } = e { text.parse().ok() } else { None }
    }

    fn check_impl(&mut self, im: &ast::ImplDecl) {
        let trait_name = match &im.trait_ty {
            ast::Type::Named { path, .. } => path.last().cloned().unwrap_or_default(),
            _ => String::new(),
        };
        let im_for_type = crate::sem::named_tail(&im.for_ty);
        for item in &im.items {
            if let ast::ImplItem::Method(m) = item {
                let contract_no_alloc = self.trait_method_no_alloc(&trait_name, &m.name);
                if std::env::var("CTRON_DEBUG").is_ok() { eprintln!("[dbg] impl {}.{} contract={}", trait_name, m.name, contract_no_alloc); }
                let saved = self.no_alloc_ctx;
                if contract_no_alloc { self.no_alloc_ctx = true; }
                if let Some(body) = &m.body {
                    let alloc = self.scan_any_alloc(body, &mut HashSet::new());
                    if contract_no_alloc && alloc {
                        self.err("E3040",
                            format!("`{}.{}` 实现违反 trait 的 #[no_alloc] 契约:函数体含分配(allocation)", trait_name, m.name),
                            Span::new(1, 1, 0, 0));
                    }
                    self.scopes.push(HashMap::new());
                    // self 绑定:&self → &for_ty;var self → for_ty
                    let for_named = self.named(&im_for_type, vec![]);
                    let self_ty = if m.params.iter().any(|p| matches!(p, ast::Param::Receiver { is_var: true })) {
                        for_named.clone()
                    } else {
                        Ty::Ref(Box::new(for_named.clone()))
                    };
                    self.scopes.last_mut().unwrap().insert("self".into(), Local { ty: self_ty });
                    for p in &m.params {
                        if let ast::Param::Param { name, ty, .. } = p {
                            let t = self.lower_local_ty(ty);
                            self.scopes.last_mut().unwrap().insert(name.clone(), Local { ty: t });
                        }
                    }
                    self.cur_ret = match &m.ret { Some(t) => self.lower_local_ty(t), None => Ty::Void };
                    self.check_block(body);
                    self.scopes.pop();
                    self.no_alloc_ctx = saved;
                }
            }
        }
    }

    fn trait_method_no_alloc(&self, trait_name: &str, method: &str) -> bool {
        // 从本包文件中查 trait 方法的 #[no_alloc](经符号表定位 trait AST 不可行,直接查 defs 不存方法;用 impl 关系不可知 → 查 trait 方法缓存)
        self.trait_method_alloc_map.get(&(trait_name.to_string(), method.to_string())).copied().unwrap_or(false)
    }

    // ---------- comptime 解释器(E6010 预算) ----------

    fn eval_comptime(&mut self, name: &str, args: &[i64], budget: u64, steps: &mut u64, depth: &mut u32) -> Result<i64, ()> {
        *steps += 1;
        *depth += 1;
        if *steps > budget || *depth > 128 { return Err(()); }
        let r = self.eval_comptime_inner(name, args, budget, steps, depth);
        *depth -= 1;
        r
    }

    fn eval_comptime_inner(&mut self, name: &str, args: &[i64], budget: u64, steps: &mut u64, depth: &mut u32) -> Result<i64, ()> {
        let (body, params): (ast::Block, Vec<String>) = {
            let Some(Symbol::Fn(id)) = self.lookup_fn_global(name) else { return Err(()) };
            let f = &self.sema.fns[id];
            if !f.is_comptime { return Err(()); }
            let Some(body) = f.body.clone() else { return Err(()) };
            (body, f.params.iter().map(|(n, _)| n.clone()).collect())
        };
        let mut locals: HashMap<String, i64> = HashMap::new();
        for (i, p) in params.iter().enumerate() {
            if let Some(v) = args.get(i) { locals.insert(p.clone(), *v); }
        }
        // 块求值:Some(v) = 早返回值;None = 顺序执行到尾且无值
        match self.eval_block_comptime(&body, &mut locals, budget, steps, depth)? {
            Some(v) => Ok(v),
            None => Err(()),
        }
    }

    fn eval_block_comptime(&mut self, block: &ast::Block, locals: &mut HashMap<String, i64>, budget: u64, steps: &mut u64, depth: &mut u32) -> Result<Option<i64>, ()> {
        for s in &block.stmts {
            match s {
                ast::Stmt::Let { pattern, expr, .. } => {
                    let v = self.eval_expr_comptime(expr, locals, budget, steps, depth)?;
                    if let ast::Pattern::Ident(n) = pattern { locals.insert(n.clone(), v); }
                }
                ast::Stmt::Return(e) => {
                    let v = match e { Some(e) => self.eval_expr_comptime(e, locals, budget, steps, depth)?, None => return Err(()) };
                    return Ok(Some(v));   // 早返回
                }
                ast::Stmt::Expr(e) => {
                    // 语句位置的 if:分支块的早返回(Some)必须传播
                    if let Expr::If { cond, then, els } = e {
                        let c = self.eval_expr_comptime(cond, locals, budget, steps, depth)?;
                        let branch: Option<&ast::Block> = if c != 0 {
                            Some(then)
                        } else {
                            match els.as_deref() {
                                Some(Expr::BlockExpr(b)) => Some(b),
                                Some(Expr::If { .. }) => None,   // else-if 链在 comptime 语料中不出现
                                _ => None,
                            }
                        };
                        if let Some(b) = branch {
                            if let Some(v) = self.eval_block_comptime(b, locals, budget, steps, depth)? {
                                return Ok(Some(v));
                            }
                        }
                    } else {
                        self.eval_expr_comptime(e, locals, budget, steps, depth)?;
                    }
                }
                _ => {}
            }
        }
        match &block.tail {
            Some(tail) => Ok(Some(self.eval_expr_comptime(tail, locals, budget, steps, depth)?)),
            None => Ok(None),
        }
    }

    fn eval_expr_comptime(&mut self, e: &ast::Expr, locals: &mut HashMap<String, i64>, budget: u64, steps: &mut u64, depth: &mut u32) -> Result<i64, ()> {
        match e {
            Expr::Int { text, .. } => text.parse().map_err(|_| ()),
            Expr::Unary { op: ast::UnOp::Neg, expr } => Ok(-self.eval_expr_comptime(expr, locals, budget, steps, depth)?),
            Expr::Binary { op, lhs, rhs } => {
                let a = self.eval_expr_comptime(lhs, locals, budget, steps, depth)?;
                let b = self.eval_expr_comptime(rhs, locals, budget, steps, depth)?;
                Ok(match op {
                    ast::BinOp::Add => a.wrapping_add(b),
                    ast::BinOp::Sub => a.wrapping_sub(b),
                    ast::BinOp::Mul => a.wrapping_mul(b),
                    ast::BinOp::Lt => (a < b) as i64,
                    ast::BinOp::Gt => (a > b) as i64,
                    ast::BinOp::Le => (a <= b) as i64,
                    ast::BinOp::Ge => (a >= b) as i64,
                    ast::BinOp::Eq => (a == b) as i64,
                    ast::BinOp::Ne => (a != b) as i64,
                    _ => return Err(()),
                })
            }
            Expr::If { cond, then, els } => {
                let c = self.eval_expr_comptime(cond, locals, budget, steps, depth)?;
                if c != 0 {
                    self.eval_block_comptime(then, locals, budget, steps, depth)?.ok_or(())
                } else if let Some(els) = els {
                    self.eval_expr_comptime(els, locals, budget, steps, depth)
                } else { Err(()) }
            }
            Expr::BlockExpr(b) => {
                self.eval_block_comptime(b, locals, budget, steps, depth)?.ok_or(())
            }
            Expr::Call { callee, args } => {
                if let Expr::Ident(name) = &**callee {
                    let mut vals = Vec::new();
                    for a in args.iter() {
                        vals.push(self.eval_expr_comptime(a, locals, budget, steps, depth)?);
                    }
                    self.eval_comptime(name, &vals, budget, steps, depth)
                } else { Err(()) }
            }
            Expr::Ident(n) => locals.get(n).copied().ok_or(()),
            _ => Err(()),
        }
    }

}

// ---------- 块与语句 ----------

impl<'a> Checker<'a> {
    fn check_block(&mut self, block: &ast::Block) -> Ty {
        self.scopes.push(HashMap::new());
        let mut ty = Ty::Void;
        for s in &block.stmts {
            self.check_stmt(s);
        }
        if let Some(tail) = &block.tail {
            ty = self.expr(tail, None);
        }
        self.scopes.pop();
        ty
    }

    fn check_stmt(&mut self, s: &ast::Stmt) {
        match s {
            ast::Stmt::Let { pattern, ty: ann, expr, .. } => {
                let hinted = ann.as_ref().map(|t| self.lower_local_ty(t));
                let ety = self.expr(expr, hinted.as_ref());
                // E3050(§6.4):own 块内 arena 句柄仅移动——拷贝绑定即 move 源
                if self.in_own {
                    if self.expr_is_arena_handle_init(expr) {
                        if let ast::Pattern::Ident(n) = pattern { self.handles.insert(n.clone()); }
                    } else if let ast::Expr::Ident(src) = expr {
                        if self.handles.contains(src) && !self.moved.contains(src) {
                            self.moved.insert(src.clone());
                            if let ast::Pattern::Ident(n) = pattern { self.handles.insert(n.clone()); }
                        }
                    }
                }
                let bind_ty = hinted.unwrap_or(ety);
                let binds = self.check_pattern(pattern, Some(&bind_ty));
                self.scopes.last_mut().unwrap().extend(binds);
            }
            ast::Stmt::Return(e) => {
                let hint = if matches!(self.cur_ret, Ty::Err) { None } else { Some(self.cur_ret.clone()) };
                match e {
                    Some(e) => { self.expr(e, hint.as_ref()); }
                    None => { let _ = hint; }
                }
            }
            ast::Stmt::For { pattern, iter, body } => {
                let ity = self.expr(iter, None);
                let elem = match self.resolve(&ity) {
                    Ty::Range(e) | Ty::MutSlice(e) | Ty::RoSlice(e) | Ty::Array(e) => *e,
                    Ty::Named { def, args } => {
                        let name = &self.sema.defs[def].name;
                        if (name == "List" || name == "Simd") && !args.is_empty() { args[0].clone() }
                        else { Ty::Err }
                    }
                    _ => Ty::Err,
                };
                self.scopes.push(HashMap::new());
                let binds = self.check_pattern(pattern, Some(&elem));
                self.scopes.last_mut().unwrap().extend(binds);
                self.check_block(body);
                self.scopes.pop();
            }
            ast::Stmt::While { cond, body } => {
                let cty = self.expr(cond, None);
                if !matches!(self.resolve(&cty), Ty::Bool | Ty::Err) {
                    self.err("E2010", format!("while 条件应为 Bool,实际 {}", self.type_name(&cty)), Span::new(1, 1, 0, 0));
                }
                self.check_block(body);
            }
            ast::Stmt::Assign { target, op: _, value } => {
                let tty = self.expr(target, None);
                if self.in_own && self.root_is_gc_class(target, &tty) {
                    self.err("E3060",
                        "own 块内对 GC 值可变借用/赋值(mutable access to GC value in own block);改为 arena 值或移出块".into(),
                        Span::new(1, 1, 0, 0));
                }
                self.expr(value, Some(&tty));
            }
            ast::Stmt::Expr(e) => {
                let ty = self.expr(e, None);
                if std::env::var("CTRON_DEBUG").is_ok() { eprintln!("[dbg] stmt expr ty={:?} src={:?}", self.type_name(&ty), e); }
                // W8020:Result/Option 结果被丢弃
                if let Ty::Named { def, .. } = self.resolve(&ty) {
                    if matches!(self.sema.defs[def].name.as_str(), "Result" | "Option") {
                        self.err("W8020", "must-use 结果被丢弃:Result/Option 应处理或 `let _ =` 显式丢弃".into(), Span::new(1, 1, 0, 0));
                    }
                }
            }
        }
    }

    fn root_is_gc_class(&mut self, target: &ast::Expr, _tty: &Ty) -> bool {
        // 找到成员/索引链的根标识符,再判定其绑定类型是否为 GC 类
        fn root_name(e: &ast::Expr) -> Option<String> {
            match e {
                Expr::Ident(n) => Some(n.clone()),
                Expr::Member { obj, .. } | Expr::Index { obj, .. } => root_name(obj),
                _ => None,
            }
        }
        let Some(n) = root_name(target) else { return false };
        let Some(t) = self.lookup_local(&n) else { return false };
        self.is_gc_class(&t)
    }

    fn lower_local_ty(&mut self, t: &ast::Type) -> Ty {
        let params = HashMap::new();
        self.sema.lower_ty(t, &params)
    }

    // ---------- 模式 ----------

    fn check_pattern(&mut self, p: &ast::Pattern, hint: Option<&Ty>) -> HashMap<String, Local> {
        self.check_pattern_cov(p, hint).0
    }

    fn check_pattern_cov(&mut self, p: &ast::Pattern, hint: Option<&Ty>) -> (HashMap<String, Local>, HashSet<String>) {
        let mut out = HashMap::new();
        let mut cov = HashSet::new();
        self.check_pattern_into(p, hint, &mut out, &mut cov);
        (out, cov)
    }

    fn check_pattern_into(&mut self, p: &ast::Pattern, hint: Option<&Ty>, out: &mut HashMap<String, Local>, cov: &mut HashSet<String>) {
        match p {
            ast::Pattern::Ident(n) => {
                out.insert(n.clone(), Local { ty: hint.cloned().unwrap_or(Ty::Err) });
            }
            ast::Pattern::Wildcard => {}
            ast::Pattern::Lit(_) => {}
            ast::Pattern::Tuple(ps) => {
                let elems: Vec<Ty> = match hint.map(|h| self.resolve(h)) {
                    Some(Ty::Tuple(tys)) => tys,
                    _ => vec![],
                };
                for (i, sub) in ps.iter().enumerate() {
                    self.check_pattern_into(sub, elems.get(i), out, cov);
                }
            }
            ast::Pattern::Agg { path, sub } => {
                let name = path.last().cloned().unwrap_or_default();
                // 先从 hint(scrutinee)的枚举定义解析变体载荷
                let mut handled = false;
                if let Some(h) = hint.map(|h| self.resolve(h)) {
                    if let Ty::Named { def, args: hargs } = &h {
                        if std::env::var("CTRON_DEBUG").is_ok() { eprintln!("[dbg] agg def={} hargs={:?}", self.sema.defs[*def].name, hargs); }
                        if !self.sema.defs[*def].variants.is_empty() {
                            cov.insert(name.clone());
                            handled = true;
                            // 类型参数位对齐一致化:param_vars[i] ↔ hargs[i]
                            for (pt, ha) in self.sema.defs[*def].params.iter().zip(hargs.iter()) {
                                let _ = self.unify(pt, ha);
                                if std::env::var("CTRON_DEBUG").is_ok() { eprintln!("[dbg] align {:?} <- {:?} (def={})", pt, ha, self.sema.defs[*def].name); }
                            }
                            let payload: Vec<Ty> = self.sema.defs[*def].variants.iter()
                                .find(|(vn, _)| vn == &name)
                                .map(|(_, ps)| ps.iter().map(|pt| {
                                    // 载荷中的类型参数变量按 def.params 位置映射到实例实参
                                    if let Ty::Var(_) = pt {
                                        if let Some(pos) = self.sema.defs[*def].params.iter().position(|pv| pv == pt) {
                                            if let Some(ha) = hargs.get(pos) { return ha.clone(); }
                                        }
                                    }
                                    resolve_vars(pt, &self.subs)
                                }).collect())
                                .unwrap_or_default();
                            if let ast::AggSub::Tuple(ps) = sub {
                                for (i, sp) in ps.iter().enumerate() {
                                    if std::env::var("CTRON_DEBUG").is_ok() {
                                        eprintln!("[dbg] agg {} payload[{}]={:?} sub={:?} subs={:?} pt={:?}", name, i, payload.get(i), sp, self.subs, pt_of_payload(payload.as_slice(), i));
                                    }
                                    self.check_pattern_into(sp, payload.get(i), out, cov);
                                }
                            }
                        }
                    }
                }
                if !handled {
                    // 具名类型(结构体/类模式)
                    if let Some(def) = self.lookup_type(&name) {
                        cov.insert(name.clone());
                        if matches!(self.sema.defs[def].kind, DefKind::Class | DefKind::Struct) {
                            if let ast::AggSub::Struct(_) = sub {
                                let fields: Vec<(String, Ty)> = self.sema.defs[def].fields.iter()
                                    .map(|(n, t, _)| (n.clone(), t.clone())).collect();
                                for (fname, fty) in fields {
                                    if let Some(sp) = find_field_pat(sub, &fname) {
                                        match &sp.pattern {
                                            Some(bp) => self.check_pattern_into(bp, Some(&fty), out, cov),
                                            None => { out.insert(sp.name.clone(), Local { ty: fty }); }
                                        }
                                    }
                                }
                                return;
                            }
                        }
                    } else if path.len() == 1 {
                        // 无载荷变体(约定:PascalCase)
                        cov.insert(name.clone());
                        return;
                    }
                    cov.insert(name.clone());
                }
            }
        }
    }

    // ---------- 表达式 ----------

    fn expr(&mut self, e: &ast::Expr, hint: Option<&Ty>) -> Ty {
        self.depth += 1;
        if self.depth > 256 {
            self.err("E1001", "表达式嵌套过深(语义检查)".into(), Span::new(1, 1, 0, 0));
            self.depth -= 1;
            return Ty::Err;
        }
        let t = self.expr_inner(e, hint);
        self.depth -= 1;
        t
    }

    fn expr_inner(&mut self, e: &ast::Expr, hint: Option<&Ty>) -> Ty {
        match e {
            Expr::Int { suffix, .. } => {
                if let Some(Ty::Int(w)) = hint.map(|h| self.resolve(h)) { return Ty::Int(w); }
                if let Some(Ty::UInt(w)) = hint.map(|h| self.resolve(h)) { return Ty::UInt(w); }
                match suffix.as_str() {
                    "i8" => Ty::Int(IntW::W8), "i16" => Ty::Int(IntW::W16),
                    "i32" => Ty::Int(IntW::W32), "i64" => Ty::Int(IntW::W64),
                    "isize" => Ty::Int(IntW::WSize),
                    "u8" => Ty::UInt(IntW::W8), "u16" => Ty::UInt(IntW::W16),
                    "u32" => Ty::UInt(IntW::W32), "u64" => Ty::UInt(IntW::W64),
                    "usize" => Ty::UInt(IntW::WSize),
                    _ => Ty::Int(IntW::W32),
                }
            }
            Expr::Float { suffix, .. } => {
                if let Some(h) = hint { if matches!(self.resolve(h), Ty::F32 | Ty::F64) { return self.resolve(h); } }
                match suffix.as_str() { "f32" => Ty::F32, _ => Ty::F64 }
            }
            Expr::Str { parts } => {
                if self.in_own && parts.iter().any(|p| matches!(p, ast::StrPart::Interp(_))) {
                    self.err("E3040", "own 块内插值串构造 String(allocation in own block)".into(), Span::new(1, 1, 0, 0));
                }
                Ty::Str
            }
            Expr::Bool(_) => Ty::Bool,
            Expr::Void => Ty::Void,
            Expr::Ident(name) => self.check_ident(name, hint),
            Expr::Tuple(items) => Ty::Tuple(items.iter().map(|i| self.expr(i, None)).collect()),
            Expr::Array(items) => {
                let mut elem = Ty::Err;
                for (i, item) in items.iter().enumerate() {
                    if i == 0 { elem = self.expr(item, None); }
                    else { let e2 = self.expr(item, Some(&elem)); let _ = e2; }
                }
                Ty::Array(Box::new(elem))
            }
            Expr::StructLit { path, fields, .. } => self.check_struct_lit(path, fields),
            Expr::Unary { op, expr } => {
                let t = self.expr(expr, hint);
                match op {
                    ast::UnOp::Neg => match self.resolve(&t) {
                        Ty::Int(_) | Ty::UInt(_) | Ty::F32 | Ty::F64 | Ty::Err => t,
                        other => { self.err("E2010", format!("一元 - 需要数值,实际 {}", self.type_name(&other)), Span::new(1, 1, 0, 0)); Ty::Err }
                    },
                    ast::UnOp::Not => match self.resolve(&t) {
                        Ty::Bool | Ty::Err => Ty::Bool,
                        other => { self.err("E2010", format!("一元 ! 需要 Bool,实际 {}", self.type_name(&other)), Span::new(1, 1, 0, 0)); Ty::Err }
                    },
                }
            }
            Expr::Binary { op, lhs, rhs } => self.check_binary(op, lhs, rhs, hint),
            Expr::Range { inclusive: _, from, to } => {
                let f = self.expr(from, None);
                self.expr(to, Some(&f));
                Ty::Range(Box::new(f))
            }
            Expr::Call { callee, args } => self.check_call(callee, args, hint),
            Expr::Index { obj, index } => {
                let ot = self.expr(obj, None);
                self.expr(index, None);
                match self.resolve(&ot) {
                    Ty::MutSlice(e) | Ty::RoSlice(e) | Ty::Array(e) => *e,
                    Ty::Named { def, args } => {
                        if self.sema.defs[def].name == "List" { args.first().cloned().unwrap_or(Ty::Err) }
                        else { Ty::Err }
                    }
                    _ => Ty::Err,
                }
            }
            Expr::Member { obj, target } => self.check_member(obj, target),
            Expr::TypeArgs { expr, args } => {
                // 类型实参挂在前缀上:泛型类型构造器(Simd[F32,4])或泛型方法(arena.array[I32])
                match &**expr {
                    Expr::Ident(name) => {
                        if name == "Simd" {
                            let elem = args.first().map(|t| self.lower_local_ty(t)).unwrap_or(Ty::Err);
                            Ty::Simd(Box::new(elem))
                        } else if name == "Box" {
                            // Box[T](v) 的值直接表现为 T(访问自动解引用)
                            args.first().map(|t| self.lower_local_ty(t)).unwrap_or(Ty::Err)
                        } else if let Some(def) = self.lookup_type(name) {
                            let targs = args.iter().map(|t| self.lower_local_ty(t)).collect();
                            Ty::Ctor { def, args: targs }
                        } else { Ty::Err }
                    }
                    _ => {
                        // 泛型方法:交给 Call 处理;此处仅求 receiver 类型并以 Err 占位
                        let _ = args;
                        self.expr(expr, None);
                        Ty::Err
                    }
                }
            }
            Expr::Try(e) => {
                // §5.3 前置:所在函数必须返回 Result/Option
                let ret_ok = match self.resolve(&self.cur_ret.clone()) {
                    Ty::Named { def, .. } => {
                        matches!(self.sema.defs[def].name.as_str(), "Result" | "Option")
                    }
                    _ => false,
                };
                if !ret_ok {
                    self.err("E2010", "`?` 只能用于返回 Result/Option 的函数(§5.3)".into(), Span::new(1, 1, 0, 0));
                    let t = self.expr(e, None);
                    return match self.resolve(&t) {
                        Ty::Optional(inner) => *inner,
                        Ty::Named { ref args, .. } => args.first().cloned().unwrap_or(Ty::Err),
                        _ => Ty::Err,
                    };
                }
                let t = self.expr(e, None);
                match self.resolve(&t) {
                    Ty::Named { def, ref args } => {
                        let name = self.sema.defs[def].name.clone();
                        if name == "Result" {
                            // E 兼容:同型或目标为 AnyError 擦除
                            let target_e = self.current_ret_error();
                            if let Some(te) = target_e {
                                if let Some(Ty::Named { def: e_def, .. }) = args.get(1) {
                                    let e_name = self.sema.defs.get(*e_def).map(|d| d.name.clone()).unwrap_or_default();
                                    let allowed = args.get(1) == Some(&te) || e_name == "AnyError"
                                        || self.sema.impls.iter().any(|im| &im.trait_name == "Error" && &im.for_type == e_def_name_of(self.sema, *e_def).as_str());
                                    if !allowed {
                                        self.err("E2010", format!("`?` 错误类型不匹配:{} vs {}", e_name, self.type_name(&te)), Span::new(1, 1, 0, 0));
                                    }
                                }
                            }
                            args.first().cloned().unwrap_or(Ty::Err)
                        } else if name == "Option" {
                            args.first().cloned().unwrap_or(Ty::Err)
                        } else {
                            self.err("E2010", "`?` 只能用于 Result/Option".into(), Span::new(1, 1, 0, 0));
                            Ty::Err
                        }
                    }
                    _ => { self.err("E2010", "`?` 只能用于 Result/Option".into(), Span::new(1, 1, 0, 0)); Ty::Err }
                }
            }
            Expr::Closure { params, ret: _, body } => {
                // 有期望 fn 类型 → 参数按其类型;否则参数 Err(体仍检查)
                let mut binds = HashMap::new();
                let mut param_tys = Vec::new();
                let fn_hint = hint.and_then(|h| match self.resolve(h) {
                    Ty::FnTy { params, ret } => Some((params, *ret)),
                    _ => None,
                });
                for (i, p) in params.iter().enumerate() {
                    let ty = p.ty.as_ref().map(|t| self.lower_local_ty(t))
                        .or_else(|| fn_hint.as_ref().and_then(|(ps, _)| ps.get(i).cloned()))
                        .unwrap_or(Ty::Err);
                    param_tys.push(ty.clone());
                    binds.insert(p.name.clone(), Local { ty });
                }
                let body_hint = fn_hint.as_ref().map(|(_, r)| r.clone());
                self.scopes.push(binds);
                let bt = self.expr(body, body_hint.as_ref());
                self.scopes.pop();
                Ty::FnTy { params: param_tys, ret: Box::new(bt) }
            }
            Expr::Scope { param, body } => {
                let scope_ty = self.named("Scope", vec![]);
                self.scopes.push(HashMap::from([(param.clone(), Local { ty: scope_ty })]));
                let t = self.check_block(body);
                self.scopes.pop();
                t
            }
            Expr::Own { arena, body } => {
                let saved_own = self.in_own;
                let saved_na = self.no_alloc_ctx;
                let saved_moved = std::mem::take(&mut self.moved);
                let saved_handles = std::mem::take(&mut self.handles);
                self.in_own = true;
                let arena_ty = self.named("Arena", vec![]);
                self.scopes.push(HashMap::from([(arena.clone(), Local { ty: arena_ty })]));
                self.check_block(body);
                self.scopes.pop();
                self.in_own = saved_own;
                self.no_alloc_ctx = saved_na;
                self.moved = saved_moved;
                self.handles = saved_handles;
                Ty::Void
            }
            Expr::If { cond, then, els } => {
                let c = self.expr(cond, None);
                if !matches!(self.resolve(&c), Ty::Bool | Ty::Err) {
                    self.err("E2010", format!("if 条件应为 Bool,实际 {}", self.type_name(&c)), Span::new(1, 1, 0, 0));
                }
                let t1 = self.check_block(then);
                let t2 = els.as_ref().map(|e| self.expr(e, Some(&t1)));
                match t2 {
                    Some(t2) => { let _ = self.unify(&t1, &t2); t1 }
                    None => Ty::Void,
                }
            }
            Expr::Match { expr: scrut, arms } => {
                let st = self.expr(scrut, None);
                let mut arm_tys = Vec::new();
                let mut covered: HashSet<String> = HashSet::new();
                let mut has_wildcard = false;
                for arm in arms {
                    self.scopes.push(HashMap::new());
                    let (binds, cov) = self.check_pattern_cov(&arm.pattern, Some(&st));
                    self.scopes.last_mut().unwrap().extend(binds);
                    covered.extend(cov);
                    if matches!(arm.pattern, ast::Pattern::Wildcard | ast::Pattern::Ident(_)) { has_wildcard = true; }
                    let at = self.expr(&arm.expr, None);
                    self.scopes.pop();
                    arm_tys.push(at);
                }
                // 穷尽性(§4.6):enum 需全覆盖或有通配/绑定
                if let Ty::Named { def, .. } = self.resolve(&st) {
                    if matches!(self.sema.defs[def].kind, DefKind::Enum) {
                        let all: Vec<String> = self.sema.defs[def].variants.iter().map(|(n, _)| n.clone()).collect();
                        let missing: Vec<String> = all.into_iter().filter(|v| !covered.contains(v)).collect();
                        if !missing.is_empty() && !has_wildcard {
                            self.err("E2030",
                                format!("match 不穷尽(not exhaustive):缺少变体 {}", missing.join(", ")),
                                Span::new(1, 1, 0, 0));
                        }
                    }
                }
                arm_tys.first().cloned().unwrap_or(Ty::Void)
            }
            Expr::BlockExpr(b) => self.check_block(b),
        }
    }

    fn current_ret_error(&self) -> Option<Ty> {
        if let Ty::Named { def, args } = self.resolve(&self.cur_ret.clone()) {
            if self.sema.defs[def].name == "Result" { return args.get(1).cloned(); }
        }
        None
    }

    /// own 块内:表达式是否创建 arena 句柄(arena.array/zeros/list)
    fn expr_is_arena_handle_init(&self, e: &ast::Expr) -> bool {
        let inner = match e {
            ast::Expr::TypeArgs { expr, .. } => Some(&**expr),
            ast::Expr::Call { callee, .. } => match &**callee {
                ast::Expr::TypeArgs { expr, .. } => Some(&**expr),
                ast::Expr::Member { obj, .. } => Some(&**obj),
                _ => None,
            },
            ast::Expr::Member { obj, .. } => Some(&**obj),
            _ => None,
        };
        match inner {
            Some(ast::Expr::Member { obj, target: ast::MemberTarget::Name(m) }) => {
                if !matches!(m.as_str(), "array" | "zeros" | "list" | "fixed") { return false; }
                matches!(&**obj, ast::Expr::Ident(a) if a == "arena")
            }
            _ => false,
        }
    }

    fn check_ident(&mut self, name: &str, _hint: Option<&Ty>) -> Ty {
        if self.moved.contains(name) {
            self.err("E3050", format!("arena 句柄 `{}` 已 move,不可再使用(use after move)", name), Span::new(1, 1, 0, 0));
        }
        if let Some(t) = self.lookup_local(name) { return t; }
        match self.lookup_fn_global(name) {
            Some(Symbol::Fn(id)) => {
                let f = &self.sema.fns[id];
                Ty::FnTy { params: f.params.iter().map(|(_, t)| t.clone()).collect(), ret: Box::new(f.ret.clone()) }
            }
            Some(Symbol::Const { ty }) => ty,
            Some(Symbol::Static { ty }) => ty,
            Some(Symbol::Variant { def, idx }) => self.variant_value(def, idx),
            Some(Symbol::Module(_)) => Ty::Err, // 模块名作为值:仅用于成员调用,在 call 处理
            Some(Symbol::Type(def)) => Ty::Named { def, args: vec![] }, // 类型名作为关联调用接收者
            None => {
                // prelude 枚举变体值(Some/None/Ok/Err)
                match name {
                    "Some" => {
                        let a = self.fresh_var();
                        let d = self.sema.def_by_name["Option"];
                        return Ty::Ctor { def: d, args: vec![a] };
                    }
                    "None" => {
                        let a = self.fresh_var();
                        let d = self.sema.def_by_name["Option"];
                        return Ty::Named { def: d, args: vec![a] };
                    }
                    "Ok" | "Err" => {
                        let a = self.fresh_var();
                        let e = self.fresh_var();
                        let d = self.sema.def_by_name["Result"];
                        return if name == "Ok" { Ty::Ctor { def: d, args: vec![a, e] } } else { Ty::Ctor { def: d, args: vec![e, a] } };
                    }
                    _ => {}
                }
                // 已知类型名作为关联调用接收者(Arena.fixed 等)
                if let Some(def) = self.sema.def_by_name.get(name) {
                    return Ty::Named { def: *def, args: vec![] };
                }
                self.err("E2020", format!("未解析的名称 `{}`", name), Span::new(1, 1, 0, 0));
                Ty::Err
            }
        }
    }

    fn variant_value(&mut self, def: DefId, idx: usize) -> Ty {
        let payload = self.sema.defs[def].variants.get(idx).map(|(_, ps)| ps.clone()).unwrap_or_default();
        let mut args = Vec::new();
        for _ in 0..payload.len() { args.push(self.fresh_var()); }
        if payload.is_empty() { Ty::Named { def, args } } else { Ty::Ctor { def, args } }
    }

    fn check_binary(&mut self, op: &ast::BinOp, lhs: &ast::Expr, rhs: &ast::Expr, _hint: Option<&Ty>) -> Ty {
        use ast::BinOp::*;
        if *op == BinOp::Or {
            // Option/Result 取默认中缀
            let lt = self.expr(lhs, None);
            let def_ok = match self.resolve(&lt) {
                Ty::Named { def, args } => {
                    let n = self.sema.defs[def].name.clone();
                    if n == "Option" || n == "Result" {
                        let default = args.first().cloned().unwrap_or(Ty::Err);
                        self.expr(rhs, Some(&default));
                        return default;
                    }
                    false
                }
                _ => false,
            };
            if !def_ok {
                self.err("E2010", "`or` 只能用于 Option/Result".into(), Span::new(1, 1, 0, 0));
            }
            self.expr(rhs, None);
            return Ty::Err;
        }
        let lt = self.expr(lhs, None);
        let rt = self.expr(rhs, Some(&lt));
        let lt = self.resolve(&lt);
        let rt = self.resolve(&rt);
        match op {
            AndAnd => {
                if !matches!(lt, Ty::Bool | Ty::Err) || !matches!(rt, Ty::Bool | Ty::Err) {
                    self.err("E2010", "&& 需要 Bool".into(), Span::new(1, 1, 0, 0));
                }
                Ty::Bool
            }
            Eq | Ne | Lt | Gt | Le | Ge => {
                if !(Self::both_numeric(&lt, &rt) || matches!(lt, Ty::Bool | Ty::Err) || matches!(rt, Ty::Bool | Ty::Err)
                    || matches!(lt, Ty::Str | Ty::String | Ty::Err) || matches!(rt, Ty::Str | Ty::String | Ty::Err)
                    || self.unify(&lt, &rt)) {
                    self.err("E2010", format!("比较类型不匹配({:?}):{} vs {}", op, self.type_name(&lt), self.type_name(&rt)), Span::new(1, 1, 0, 0));
                }
                Ty::Bool
            }
            Add | Sub | Mul | Div | Mod | WrapAdd | WrapSub => {
                if std::env::var("CTRON_DEBUG").is_ok() { eprintln!("arith lt={:?} rt={:?}", lt, rt); }
                let simd_pair = matches!(&lt, Ty::Simd(_)) && matches!(&rt, Ty::Simd(_));
                if simd_pair || Self::both_numeric(&lt, &rt) || lt == Ty::Err || rt == Ty::Err { lt }
                else {
                    self.err("E2010", format!("算术需要数值,实际 {} 与 {}", self.type_name(&lt), self.type_name(&rt)), Span::new(1, 1, 0, 0));
                    Ty::Err
                }
            }
            Or => Ty::Err,
        }
    }

    fn check_struct_lit(&mut self, path: &[String], fields: &[ast::StructField]) -> Ty {
        let name = path.last().cloned().unwrap_or_default();
        let Some(def) = self.lookup_type(&name) else {
            self.err("E2020", format!("未解析的类型 `{}`", name), Span::new(1, 1, 0, 0));
            return Ty::Err;
        };
        let param_vars: Vec<u32> = self.sema.defs[def].fields.iter()
            .flat_map(|(_, t, _)| collect_vars(t))
            .collect();
        for f in fields {
            let fty = self.sema.defs[def].fields.iter()
                .find(|(n, _, _)| n == &f.name).map(|(_, t, _)| t.clone());
            match fty {
                Some(fty) => {
                    let hint = resolve_vars(&fty, &self.subs);
                    let got = self.expr(f.value.as_ref().unwrap_or(&ast::Expr::Void), Some(&hint));
                    let _ = self.unify(&hint, &got);
                }
                None => {
                    self.err("E2010", format!("`{}` 无字段 `{}`", name, f.name), Span::new(1, 1, 0, 0));
                }
            }
        }
        let args = param_vars.iter().map(|&v| self.resolve(&Ty::Var(v))).collect();
        if self.in_own && matches!(self.sema.defs[def].kind, DefKind::Class) {
            self.err("E3040", "own 块内构造 GC 类实例(allocation in own block)".into(), Span::new(1, 1, 0, 0));
        }
        Ty::Named { def, args }
    }

    // ---------- 调用 ----------

    fn check_call(&mut self, callee: &ast::Expr, args: &[ast::Expr], hint: Option<&Ty>) -> Ty {
        match callee {
            Expr::TypeArgs { expr, args: type_args } => {
                // 泛型方法(arena.array[I32](8))
                if let Expr::Member { obj, target: ast::MemberTarget::Name(m) } = &**expr {
                    let ot = self.expr(obj, None);
                    let targs: Vec<Ty> = type_args.iter().map(|t| self.lower_local_ty(t)).collect();
                    let oname = match self.resolve(&ot) { Ty::Named { def, .. } => self.sema.defs[def].name.clone(), _ => String::new() };
                    if oname == "Arena" {
                        let t = targs.first().cloned().unwrap_or(Ty::Err);
                        return match m.as_str() {
                            "array" | "zeros" => Ty::MutSlice(Box::new(t)),
                            "list" => self.named("ArenaList", vec![t]),
                            _ => { self.err("E2020", format!("Arena 无方法 `{}`", m), Span::new(1, 1, 0, 0)); Ty::Err }
                        };
                    }
                }
                // 泛型类型构造器调用(Box[Point](p))
                if let Expr::Ident(name) = &**expr {
                    if name == "Arena" {
                        // Arena.fixed(n) 等关联构造
                        for a in args { self.expr(a, None); }
                        return self.named("Arena", vec![]);
                    }
                    if let Some(def) = self.lookup_type(name) {
                        let targs: Vec<Ty> = type_args.iter().map(|t| self.lower_local_ty(t)).collect();
                        let dn = self.sema.defs[def].name.clone();
                        if dn == "Box" {
                            // Box[T](v):值表现为 T(成员访问自动解引用)
                            if let Some(a) = args.first() { self.expr(a, Some(&targs[0])); }
                            return targs.first().cloned().unwrap_or(Ty::Err);
                        }
                        for a in args { self.expr(a, None); }
                        return match dn.as_str() {
                            "Channel" => {
                                let elem = targs.first().cloned().unwrap_or(Ty::Err);
                                if !self.is_send(&elem) {
                                    self.err("E3020", format!("channel 元素类型必须 Send:`{}` 非 Send(§7.4)", self.type_name(&elem)), Span::new(1, 1, 0, 0));
                                }
                                let s = self.named("Sender", vec![elem.clone()]);
                                let r = self.named("Receiver", vec![elem]);
                                Ty::Tuple(vec![s, r])
                            }
                            _ => Ty::Named { def, args: targs },
                        };
                    }
                }
                Ty::Err
            }
            Expr::Member { obj, target: ast::MemberTarget::Name(m) } => {
                self.check_method_call(obj, m, args, hint)
            }
            Expr::Ident(name) => self.check_ident_call(name, args, hint),
            _ => {
                let ct = self.expr(callee, None);
                match self.resolve(&ct) {
                    Ty::FnTy { params, ret } => {
                        for (p, a) in params.iter().zip(args) { self.expr(a, Some(p)); }
                        *ret
                    }
                    Ty::Ctor { def, args: targs } => {
                        for a in args { self.expr(a, None); }
                        if self.sema.defs[def].name == "Channel" {
                            let elem = targs.first().cloned().unwrap_or(Ty::Err);
                            if !self.is_send(&elem) {
                                self.err("E3020", format!("channel 元素类型必须 Send:`{}` 非 Send(§7.4)", self.type_name(&elem)), Span::new(1, 1, 0, 0));
                            }
                            let s = self.named("Sender", vec![elem.clone()]);
                            let r = self.named("Receiver", vec![elem]);
                            Ty::Tuple(vec![s, r])
                        } else {
                            Ty::Named { def, args: targs }
                        }
                    }
                    _ => { for a in args { self.expr(a, None); } Ty::Err }
                }
            }
        }
    }

    fn check_ident_call(&mut self, name: &str, args: &[ast::Expr], hint: Option<&Ty>) -> Ty {
        // 本地绑定的函数值(参数为 fn 类型 / let 绑定的闭包)
        if let Some(t) = self.lookup_local(name) {
            if let Ty::FnTy { params, ret } = self.resolve(&t) {
                for (p, a) in params.iter().zip(args) { self.expr(a, Some(p)); }
                return *ret;
            }
        }
        self.check_ident_call_global(name, args, hint)
    }

    fn check_ident_call_global(&mut self, name: &str, args: &[ast::Expr], hint: Option<&Ty>) -> Ty {
        // prelude 枚举变体构造器(Some/None/Ok/Err)
        match name {
            "Some" => {
                let a = self.fresh_var();
                let d = self.sema.def_by_name["Option"];
                if let Some(h) = hint { let _ = self.unify(&Ty::Ctor { def: d, args: vec![a.clone()] }, h); }
                return Ty::Named { def: d, args: vec![a] };
            }
            "None" => {
                let a = self.fresh_var();
                let d = self.sema.def_by_name["Option"];
                if let Some(h) = hint { let _ = self.unify(&Ty::Named { def: d, args: vec![a.clone()] }, h); }
                return Ty::Named { def: d, args: vec![a] };
            }
            "Ok" | "Err" => {
                let a = self.fresh_var();
                let e = self.fresh_var();
                let d = self.sema.def_by_name["Result"];
                let ctor = if name == "Ok" { Ty::Ctor { def: d, args: vec![a.clone(), e.clone()] } } else { Ty::Ctor { def: d, args: vec![e.clone(), a.clone()] } };
                if let Some(h) = hint { let _ = self.unify(&ctor, h); }
                let (first, second) = if name == "Ok" { (a, e) } else { (e, a) };
                let _ = second;
                if let Some(a0) = args.first() { self.expr(a0, Some(&first)); }
                return Ty::Named { def: d, args: vec![first, second] };
            }
            _ => {}
        }
        match name {
            "assert" => { self.expr(&args[0], Some(&Ty::Bool)); Ty::Void }
            "assert_eq" | "assert_ne" => {
                let a = self.expr(&args[0], hint);
                let mut b_hint = a.clone();
                if !self.is_int(&b_hint) && !matches!(self.resolve(&b_hint), Ty::F32 | Ty::F64) { b_hint = Ty::Err; }
                let b = self.expr(args.get(1).unwrap_or(&ast::Expr::Void), Some(&b_hint));
                let _ = self.unify(&a, &b);
                Ty::Void
            }
            "panic" => { if let Some(a) = args.first() { self.expr(a, Some(&Ty::Str)); } Ty::Never }
            _ => {
                let Some(sym) = self.lookup_fn_global(name) else {
                    self.err("E2020", format!("未解析的名称 `{}`", name), Span::new(1, 1, 0, 0));
                    for a in args { self.expr(a, None); }
                    return Ty::Err;
                };
                match sym {
                    Symbol::Fn(id) => {
                        if self.in_own && self.fn_gc_alloc(id) {
                            self.err("E3040", format!("own 块内调用含 GC 分配的函数 `{}`(allocation in own block);改为 arena 或移出", name), Span::new(1, 1, 0, 0));
                        }
                        let f = &self.sema.fns[id];
                        if f.is_comptime && self.is_comptime_call_self(name) { /* 自递归在预算期处理 */ }
                        let ps: Vec<Ty> = f.params.iter().map(|(_, t)| t.clone()).collect();
                        let ret = f.ret.clone();
                        self.check_args_against(&ps, &ret, args, hint);
                        ret
                    }
                    Symbol::Variant { def, idx } => {
                        let payload = self.sema.defs[def].variants.get(idx).map(|(_, ps)| ps.clone()).unwrap_or_default();
                        let args_n: Vec<Ty> = payload.iter().map(|_| self.fresh_var()).collect();
                        // 用 hint 约束
                        if let Some(h) = hint { let cand = Ty::Named { def, args: args_n.clone() }; let _ = self.unify(&cand, h); }
                        for (_, a) in payload.iter().zip(args) {
                            self.expr(a, None);
                        }
                        Ty::Named { def, args: args_n }
                    }
                    _ => { for a in args { self.expr(a, None); } Ty::Err }
                }
            }
        }
    }

    fn is_comptime_call_self(&self, _name: &str) -> bool { false }

    fn check_args_against(&mut self, params: &[Ty], ret: &Ty, args: &[ast::Expr], hint: Option<&Ty>) -> Ty {
        let ret = self.resolve(ret);
        let ret = match (&ret, hint) {
            (Ty::Named { .. }, Some(h)) => { let h = self.resolve(h); if matches!(h, Ty::Named { .. }) { let _ = self.unify(&ret, &h); h } else { ret } }
            _ => ret,
        };
        for (p, a) in params.iter().zip(args) {
            self.expr(a, Some(p));
        }
        ret
    }

    // ---------- 成员与方法 ----------

    fn check_member(&mut self, obj: &ast::Expr, target: &ast::MemberTarget) -> Ty {
        // 模块命名空间成员(dom.set_title)在 Call 中处理;此处仅当属性/未应用方法
        if let Expr::Ident(name) = obj {
            if let Some(Symbol::Module(_)) = self.lookup_fn_global(name) {
                return Ty::Err;
            }
        }
        let ot0 = self.expr(obj, None);
        let mut ot = if let Ty::Ref(inner) = self.resolve(&ot0) {
            let i = self.resolve(&inner);
            if !matches!(i, Ty::Err) { i } else { ot0 }
        } else { self.resolve(&ot0) };
        // 已绑定推断变量 → 取其绑定类型(可多级)
        for _ in 0..8 {
            if !matches!(ot, Ty::Var(_)) { break; }
            let r = self.resolve(&ot);
            if r == ot { break; }
            ot = r;
        }
        // Named{原生标量名} 归一为原生 Ty(统一分发)
        if let Ty::Named { def, args } = &ot {
            if args.is_empty() {
                let n = self.sema.defs[*def].name.as_str();
                let prim = match n {
                    "Str" => Some(Ty::Str), "String" => Some(Ty::String), "Bool" => Some(Ty::Bool),
                    "Void" => Some(Ty::Void), "Never" => Some(Ty::Never),
                    "I8" => Some(Ty::Int(IntW::W8)), "I16" => Some(Ty::Int(IntW::W16)),
                    "I32" => Some(Ty::Int(IntW::W32)), "I64" => Some(Ty::Int(IntW::W64)),
                    "ISize" => Some(Ty::Int(IntW::WSize)),
                    "U8" => Some(Ty::UInt(IntW::W8)), "U16" => Some(Ty::UInt(IntW::W16)),
                    "U32" => Some(Ty::UInt(IntW::W32)), "U64" => Some(Ty::UInt(IntW::W64)),
                    "USize" => Some(Ty::UInt(IntW::WSize)),
                    "F32" => Some(Ty::F32), "F64" => Some(Ty::F64),
                    _ => None,
                };
                if let Some(pt) = prim { ot = pt; }
            }
        }
        let tn = member_name(target);
        match &ot {
            Ty::Str => match tn.as_str() {
                "len" | "char_len" => Ty::UInt(IntW::WSize),
                _ => { self.err("E2020", format!("Str 无属性 `{}`", tn), Span::new(1, 1, 0, 0)); Ty::Err }
            },
            Ty::String => match tn.as_str() {
                "len" => Ty::UInt(IntW::WSize),
                _ => { self.err("E2020", format!("String 无属性 `{}`", tn), Span::new(1, 1, 0, 0)); Ty::Err }
            },
            Ty::MutSlice(e) | Ty::RoSlice(e) | Ty::Array(e) => match tn.as_str() {
                "len" => Ty::UInt(IntW::WSize),
                _ => { let _ = e; self.err("E2020", format!("无属性 `{}`", tn), Span::new(1, 1, 0, 0)); Ty::Err }
            },
            Ty::Optional(_inner) => match tn.as_str() {
                "is_some" | "is_none" => self.named("Bool", vec![]),
                _ => { self.err("E2020", format!("Option 无属性 `{}`", tn), Span::new(1, 1, 0, 0)); Ty::Err }
            },
            Ty::Tuple(items) => match target {
                ast::MemberTarget::TupleIndex(i) => items.get(*i as usize).cloned().unwrap_or(Ty::Err),
                _ => { self.err("E2020", "元组无该成员".into(), Span::new(1, 1, 0, 0)); Ty::Err }
            },
            Ty::Simd(_) => Ty::Err, // splat/lane/to_array 是方法,在 Call 中
            Ty::Named { def, args } => self.check_named_member(*def, args, target),
            Ty::Err => Ty::Err,
            other => { self.err("E2020", format!("{} 无成员 `{}`", self.type_name(other), tn), Span::new(1, 1, 0, 0)); Ty::Err }
        }
    }

    fn check_named_member(&mut self, def: DefId, _args: &[Ty], target: &ast::MemberTarget) -> Ty {
        let d = &self.sema.defs[def];
        let tn = target_name(target);
        // 属性
        if let Some((_, pty)) = d.props.iter().find(|(n, _)| n == &tn) {
            return pty.clone();
        }
        match d.name.as_str() {
            "Option" | "Result" => match tn.as_str() {
                "is_some" | "is_none" | "is_ok" | "is_err" => self.named("Bool", vec![]),
                _ => { self.err("E2020", format!("{} 无属性 `{}`(方法请直接调用)", d.name, tn), Span::new(1, 1, 0, 0)); Ty::Err }
            },
            "AnyError" => match tn.as_str() {
                "message" | "trace" => self.named("Str", vec![]),
                 "cause" => {
                    let err_t = self.named("Error", vec![]);
                    let opt = self.named("Option", vec![]);
                    match opt { Ty::Named { def, .. } => Ty::Named { def, args: vec![Ty::Ref(Box::new(err_t))] }, _ => Ty::Err }
                }
                _ => { self.err("E2020", format!("AnyError 无属性 `{}`", tn), Span::new(1, 1, 0, 0)); Ty::Err }
            },
            "List" | "ArenaList" => match tn.as_str() {
                "len" => Ty::UInt(IntW::WSize),
                _ => { self.err("E2020", format!("{} 无属性 `{}`", d.name, tn), Span::new(1, 1, 0, 0)); Ty::Err }
            },
            _ => {
                if d.kind == DefKind::Class || d.kind == DefKind::Struct {
                    if let Some((_, fty, _)) = d.fields.iter().find(|(n, _, _)| n == &tn) {
                        return fty.clone();
                    }
                    // trait impl 带来的属性(FakeEnv.name 来自 Named trait)
                    for im in &self.sema.impls {
                        if im.for_type == d.name {
                            if let Some(tdef) = self.lookup_type(&im.trait_name) {
                                if let Some((_, pty)) = self.sema.defs[tdef].props.iter().find(|(n, _)| n == &tn) {
                                    return pty.clone();
                                }
                            }
                        }
                    }
                }
                if d.kind == DefKind::Trait {
                    // trait 对象属性(Error.message 等)
                    if let Some((_, pty)) = d.props.iter().find(|(n, _)| n == &tn) {
                        return pty.clone();
                    }
                }
                self.err("E2020", format!("`{}` 无属性 `{}`", d.name, tn), Span::new(1, 1, 0, 0));
                Ty::Err
            }
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn check_method_call(&mut self, obj: &ast::Expr, m: &str, args: &[ast::Expr], hint: Option<&Ty>) -> Ty {
        // 纯度/comptime/spawn 检查钩子
        let ot_raw = self.expr(obj, None);
        let ot_raw = self.resolve(&ot_raw);
        let ot = match &ot_raw {
            Ty::Ref(inner) => {
                let i = self.resolve(inner);
                if !matches!(i, Ty::Err) { i } else { ot_raw.clone() }
            }
            other => other.clone(),
        };
        // Cap trait 接收者调用:解引用前后都查(&Clock.now() 与 Clock 值)
        let cap_hit = |c: &Checker, t: &Ty| {
            let r = c.resolve(t);
            let r = match &r { Ty::Ref(i) => c.resolve(i), other => other.clone() };
            match &r { Ty::Named { def, .. } => c.sema.defs[*def].cap, _ => false }
        };
        if self.pure_ctx && cap_hit(self, &ot_raw) {
            self.err("E4020", "`#[pure]` 函数含副作用:能力(capability)调用即 I/O".into(), Span::new(1, 1, 0, 0));
        }
        if self.comptime_ctx && cap_hit(self, &ot_raw) {
            self.err("E6020", "comptime 函数为纯语义:不允许能力调用/副作用".into(), Span::new(1, 1, 0, 0));
        }
        // spawn:Scope.spawn → 捕获分析
        if m == "spawn" {
            if let Ty::Named { def, .. } = &ot {
                if self.sema.defs[*def].name == "Scope" {
                    if self.no_spawn_ctx {
                        self.err("E4030", "#[no_spawn] 语境中出现 spawn(§8.3)".into(), Span::new(1, 1, 0, 0));
                    }
                    let Some(ast::Expr::Closure { body, .. }) = args.first() else {
                        return self.named("Task", vec![Ty::Err]);
                    };
                    // 捕获分析:闭包体内引用、但绑定于外层作用域的名字
                    let outer: HashSet<String> = self.scopes.iter().flat_map(|sc| sc.keys().cloned()).collect();
                    let mut local: HashSet<String> = HashSet::new();
                    let mut caps: Vec<String> = Vec::new();
                    collect_captures(body, &mut local, &outer, &mut caps);
                    for cname in &caps {
                        let cty = self.lookup_local(cname);
                        if let Some(cty) = cty {
                            if !self.is_send(&cty) {
                                self.err("E3010",
                                    format!("闭包捕获非 Send 值 `{cname}`:类型 {} 不能跨任务(§7.4);加 Mutex 或改传值", self.type_name(&cty)),
                                    Span::new(1, 1, 0, 0));
                            }
                        }
                    }
                    let ret = self.expr(body, None);
                    return self.named("Task", vec![ret]);
                }
            }
        }
        // 纯度 / comptime:Cap trait 接收者的方法调用
        if let Ty::Ref(inner) = &ot {
            if let Ty::Named { def, .. } = self.resolve(inner) {
                if self.sema.defs[def].cap {
                    if self.pure_ctx {
                        self.err("E4020", "`#[pure]` 函数含副作用:能力(capability)调用即 I/O".into(), Span::new(1, 1, 0, 0));
                    }
                    if self.comptime_ctx {
                        self.err("E6020", "comptime 函数为纯语义:不允许能力调用/副作用".into(), Span::new(1, 1, 0, 0));
                    }
                }
            }
        }
        // 模块命名空间成员调用(stdweb.dom.title 等)
        if let Expr::Ident(name) = obj {
            if let Some(Symbol::Module(mp)) = self.lookup_fn_global(name) {
                if mp == "stdweb.dom" {
                    return match m {
                        "set_title" => { if let Some(a) = args.first() { self.expr(a, Some(&Ty::Str)); } Ty::Void }
                        "title" => self.named("Str", vec![]),
                        _ => { self.err("E2020", format!("stdweb.dom 无函数 `{}`", m), Span::new(1, 1, 0, 0)); Ty::Err }
                    };
                }
            }
        }
        // 方法分发
        match &ot {
            Ty::Optional(inner) => match m {
                "is_some" | "is_none" => self.named("Bool", vec![]),
                "or" => { self.expr(&args[0], Some(inner)); self.resolve(inner) }
                "expect" => { self.expr(&args[0], Some(&Ty::Str)); self.resolve(inner) }
                "map" => {
                    let f = self.expr(&args[0], None);
                    let r = match self.resolve(&f) { Ty::FnTy { ret, .. } => *ret, _ => Ty::Err };
                    Ty::Optional(Box::new(r))
                }
                _ => { self.err("E2020", format!("Option 无方法 `{}`", m), Span::new(1, 1, 0, 0)); Ty::Err }
            },
            Ty::Str => match m {
                "contains" => { self.expr(&args[0], Some(&Ty::Str)); self.named("Bool", vec![]) }
                "to_string" => {
                    for a in args { self.expr(a, None); }
                    if self.in_own {
                        self.err("E3040", "own 块内构造 String(GC allocation in own block)".into(), Span::new(1, 1, 0, 0));
                    }
                    self.named("String", vec![])
                }
                "slice" => { if let Some(a) = args.first() { let at = self.expr(a, None); let _ = at; } self.named("Str", vec![]) }
                "iter" => { self.named("Str", vec![]) }
                _ => { self.err("E2020", format!("Str 无方法 `{}`", m), Span::new(1, 1, 0, 0)); Ty::Err }
            },
            Ty::String => match m {
                "to_string" => { for a in args { self.expr(a, None); } self.named("String", vec![]) }
                _ => { self.err("E2020", format!("String 无方法 `{}`", m), Span::new(1, 1, 0, 0)); Ty::Err }
            },
            Ty::Simd(elem) => match m {
                "splat" => { self.expr(&args[0], Some(elem)); Ty::Simd(elem.clone()) }
                "lane" => { self.expr(&args[0], None); (**elem).clone() }
                "to_array" => { Ty::Array(Box::new((**elem).clone())) }
                _ => { self.err("E2020", format!("Simd 无方法 `{}`", m), Span::new(1, 1, 0, 0)); Ty::Err }
            },
            Ty::Named { def, args: targs } => {
                let dname = self.sema.defs[*def].name.clone();
                if dname == "Arena" {
                    return match m {
                        "fixed" => { for a in args { self.expr(a, None); } self.named("Arena", vec![]) }
                        _ => { self.err("E2020", format!("Arena 无关联函数 `{}`", m), Span::new(1, 1, 0, 0)); Ty::Err }
                    };
                }
                if std::env::var("CTRON_DEBUG").is_ok() { eprintln!("[dbg] method recv={} m={}", dname, m); }
                // 属性式零参方法(prop)不在此;方法按名分发
                match dname.as_str() {
                    "Option" | "Result" => {
                        let v = targs.first().cloned().unwrap_or(Ty::Err);
                        match m {
                            "is_some" | "is_none" | "is_ok" | "is_err" => self.named("Bool", vec![]),
                            "or" => { self.expr(&args[0], Some(&v)); v }
                            "expect" => { self.expr(&args[0], Some(&Ty::Str)); v }
                            "context" if dname == "Result" => {
                                self.expr(&args[0], Some(&Ty::Str));
                                let t = targs.first().cloned().unwrap_or(Ty::Err);
                                let any_err = self.named("AnyError", vec![]);
                                if std::env::var("CTRON_DEBUG").is_ok() { eprintln!("[dbg] context -> Result[{:?}, {:?}]", t, any_err); }
                                self.named("Result", vec![t, any_err])
                            }
                            "map" => {
                                let fty = self.expr(&args[0], None);
                                let (ps, r) = match self.resolve(&fty) {
                                    Ty::FnTy { params, ret } => (params, *ret),
                                    _ => (vec![], Ty::Err),
                                };
                                let _ = ps;
                                if dname == "Option" { Ty::Optional(Box::new(r)) } else {
                                    let e = targs.get(1).cloned().unwrap_or(Ty::Err);
                                    self.named("Result", vec![r, e])
                                }
                            }
                            _ => { self.err("E2020", format!("{} 无方法 `{}`", dname, m), Span::new(1, 1, 0, 0)); Ty::Err }
                        }
                    }
                    "List" => match m {
                        "push" => { self.expr(&args[0], Some(&targs.first().cloned().unwrap_or(Ty::Err))); Ty::Void }
                        "pop" => Ty::Optional(Box::new(targs.first().cloned().unwrap_or(Ty::Err))),
                        _ => { self.err("E2020", format!("List 无方法 `{}`", m), Span::new(1, 1, 0, 0)); Ty::Err }
                    },
                    "ArenaList" => match m {
                        "push" => { self.expr(&args[0], Some(&targs.first().cloned().unwrap_or(Ty::Err))); Ty::Void }
                        "into_gc" => self.named("List", vec![targs.first().cloned().unwrap_or(Ty::Err)]),
                        _ => { self.err("E2020", format!("ArenaList 无方法 `{}`", m), Span::new(1, 1, 0, 0)); Ty::Err }
                    },
                    "Atomic" => match m {
                        "load" => targs.first().cloned().unwrap_or(Ty::Err),
                        "store" => { self.expr(&args[0], Some(&targs.first().cloned().unwrap_or(Ty::Err))); Ty::Void }
                        "fetch_add" => { self.expr(&args[0], None); targs.first().cloned().unwrap_or(Ty::Err) }
                        _ => { self.err("E2020", format!("Atomic 无方法 `{}`", m), Span::new(1, 1, 0, 0)); Ty::Err }
                    },
                    "Mutex" | "Global" => match m {
                        "with" | "with_mut" => {
                            let fty = self.expr(&args[0], None);
                            match self.resolve(&fty) {
                                Ty::FnTy { ret, .. } => *ret,
                                _ => Ty::Err,
                            }
                        }
                        _ => { self.err("E2020", format!("{} 无方法 `{}`", dname, m), Span::new(1, 1, 0, 0)); Ty::Err }
                    },
                    "Sender" => match m {
                        "send" => {
                            self.expr(&args[0], Some(&targs.first().cloned().unwrap_or(Ty::Err)));
                            let v = self.named("Void", vec![]);
                            let tp = self.named("TaskPanic", vec![]);
                            self.named("Result", vec![v, tp])
                        }
                        _ => { self.err("E2020", format!("Sender 无方法 `{}`", m), Span::new(1, 1, 0, 0)); Ty::Err }
                    },
                    "Receiver" => match m {
                        "recv" => {
                            let tp = self.named("TaskPanic", vec![]);
                            self.named("Result", vec![targs.first().cloned().unwrap_or(Ty::Err), tp])
                        }
                        _ => { self.err("E2020", format!("Receiver 无方法 `{}`", m), Span::new(1, 1, 0, 0)); Ty::Err }
                    },
                    "Task" => match m {
                        "join" => targs.first().cloned().unwrap_or(Ty::Err),
                        "join_or" => {
                            let tp = self.named("TaskPanic", vec![]);
                            self.named("Result", vec![targs.first().cloned().unwrap_or(Ty::Err), tp])
                        }
                        _ => { self.err("E2020", format!("Task 无方法 `{}`", m), Span::new(1, 1, 0, 0)); Ty::Err }
                    },
                    "Parallel" => match m {
                        "map" => {
                            let coll = self.expr(&args[0], None);
                            let elem = match self.resolve(&coll) {
                                Ty::MutSlice(e) | Ty::RoSlice(e) | Ty::Array(e) => *e,
                                _ => Ty::Err,
                            };
                            let f = self.expr(&args[1], None);
                            let out = match self.resolve(&f) {
                                Ty::FnTy { ret, .. } => *ret,
                                _ => Ty::Err,
                            };
                            let _ = elem;
                            self.named("List", vec![out])
                        }
                        "reduce" => {
                            let init = self.expr(&args[1], None);
                            self.expr(&args[2], None);
                            init
                        }
                        _ => { self.err("E2020", format!("Parallel 无方法 `{}`", m), Span::new(1, 1, 0, 0)); Ty::Err }
                    },
                    "AnyError" => { self.err("E2020", format!("AnyError 无方法 `{}`", m), Span::new(1, 1, 0, 0)); Ty::Err }
                    _ => {
                        // 类固有方法 / trait 方法(含 &Trait 对象)
                        self.check_user_method(*def, &targs.clone(), m, args, hint)
                    }
                }
            }
            _ => {
                // UFCS:自由函数首参匹配接收者
                if let Some(Symbol::Fn(id)) = self.lookup_fn_global(m) {
                    let f = &self.sema.fns[id];
                    if let Some((_, first)) = f.params.first() {
                        let first = self.resolve(first);
                        let recv_ok = self.unify(&ot, &first)
                            || matches!(first, Ty::Ref(ref inner) if {
                                let i = self.resolve(inner);
                                let o = self.resolve(&ot);
                                self.unify(&i, &o) || matches!(o, Ty::Int(_) | Ty::UInt(_) | Ty::F32 | Ty::F64) && matches!(i, Ty::Int(_) | Ty::UInt(_) | Ty::F32 | Ty::F64)
                            });
                        if recv_ok {
                            for (fp, a) in f.params.iter().zip(args.iter()) {
                                self.expr(a, Some(&fp.1));
                            }
                            return f.ret.clone();
                        }
                    }
                }
                self.err("E2020", format!("{} 无方法 `{}`", self.type_name(&ot), m), Span::new(1, 1, 0, 0));
                Ty::Err
            }
        }
    }

    fn check_user_method(&mut self, def: DefId, targs: &[Ty], m: &str, args: &[ast::Expr], _hint: Option<&Ty>) -> Ty {
        let d = &self.sema.defs[def];
        // trait 对象/具名 trait:方法签名在 def.methods
        if d.kind == DefKind::Trait || self.is_trait_name(def) {
            if let Some((_, sig)) = d.methods.iter().find(|(n, _)| n == m) {
                let ret = sig.ret.clone();
                for (p, a) in sig.params.iter().zip(args.iter().skip(0)) {
                    self.expr(a, Some(p));
                }
                return ret;
            }
        }
        // trait 方法经 impl 表
        let iname = d.name.clone();
        for im in &self.sema.impls {
            if im.for_type == iname {
                    if let Some(Symbol::Type(tdef)) = self.lookup_fn_global(&im.trait_name) {
                    if let Some((_, sig)) = self.sema.defs[tdef].methods.iter().find(|(n, _)| n == m) {
                        for (p, a) in sig.params.iter().zip(args.iter()) {
                            self.expr(a, Some(p));
                        }
                        return sig.ret.clone();
                    }
                }
            }
        }
        // 用户自由函数 UFCS
        if let Some(Symbol::Fn(id)) = self.lookup_fn_global(m) {
            let f = &self.sema.fns[id];
            if let Some((_, first)) = f.params.first() {
                if self.unify(first, &self.resolve(&Ty::Named { def, args: targs.to_vec() })) || matches!(first, Ty::Ref(_)) {
                    for (p, a) in f.params.iter().zip(args.iter()) {
                        self.expr(a, Some(&p.1));
                    }
                    return f.ret.clone();
                }
            }
        }
        self.err("E2020", format!("`{}` 无方法 `{}`", d.name, m), Span::new(1, 1, 0, 0));
        for a in args { self.expr(a, None); }
        Ty::Err
    }

    fn is_trait_name(&self, def: DefId) -> bool {
        matches!(self.sema.defs[def].kind, DefKind::Trait)
    }
}

// ---------- 捕获分析 ----------



fn bind_pattern(p: &ast::Pattern, bound: &mut HashSet<String>) {
    match p {
        ast::Pattern::Ident(n) => { bound.insert(n.clone()); }
        ast::Pattern::Tuple(ps) => ps.iter().for_each(|p| bind_pattern(p, bound)),
        ast::Pattern::Agg { sub, .. } => {
            if let ast::AggSub::Tuple(ps) = sub { ps.iter().for_each(|p| bind_pattern(p, bound)); }
            if let ast::AggSub::Struct(fs) = sub {
                fs.iter().for_each(|f| if let Some(pt) = &f.pattern { bind_pattern(pt, bound) });
            }
        }
        _ => {}
    }
}



// ---------- 小工具 ----------


fn e_def_name_of(sema: &Sema, def: DefId) -> String { sema.defs[def].name.clone() }

fn member_name(t: &ast::MemberTarget) -> String {
    match t { ast::MemberTarget::Name(n) => n.clone(), ast::MemberTarget::TupleIndex(i) => i.to_string() }
}

fn target_name(t: &ast::MemberTarget) -> String { member_name(t) }

fn find_field_pat<'p>(sub: &'p ast::AggSub, name: &str) -> Option<&'p ast::StructPatField> {
    if let ast::AggSub::Struct(fs) = sub { fs.iter().find(|f| f.name == name) } else { None }
}

fn collect_vars(t: &Ty) -> Vec<u32> { let mut v = Vec::new(); collect_vars_impl(t, &mut v); v }

fn collect_vars_impl(t: &Ty, out: &mut Vec<u32>) {
    match t {
        Ty::Var(v) => out.push(*v),
        Ty::Named { args, .. } | Ty::Tuple(args) => args.iter().for_each(|a| collect_vars_impl(a, out)),
        Ty::MutSlice(a) | Ty::RoSlice(a) | Ty::Ref(a) | Ty::Array(a) | Ty::Optional(a)
        | Ty::Simd(a) | Ty::Range(a) => collect_vars_impl(a, out),
        Ty::FnTy { params, ret } => { params.iter().for_each(|a| collect_vars_impl(a, out)); collect_vars_impl(ret, out); }
        _ => {}
    }
}

fn pt_of_payload(payload: &[Ty], i: usize) -> Option<Ty> { payload.get(i).cloned() }

fn resolve_vars(t: &Ty, subs: &HashMap<u32, Ty>) -> Ty {
    match t {
            Ty::Var(v) => subs.get(v).cloned().unwrap_or(t.clone()),
            other => other.clone(),
        }
    }

/// spawn 捕获分析:收集闭包体内引用、且绑定于外层作用域的名字。
fn collect_captures(e: &ast::Expr, local: &mut HashSet<String>, outer: &HashSet<String>, caps: &mut Vec<String>) {
    use ast::Expr::*;
    match e {
        Ident(n) => {
            if !local.contains(n) && outer.contains(n) && !caps.contains(n) {
                caps.push(n.clone());
            }
        }
        Unary { expr, .. } | Try(expr) => collect_captures(expr, local, outer, caps),
        Binary { lhs, rhs, .. } | Range { from: lhs, to: rhs, .. } => {
            collect_captures(lhs, local, outer, caps);
            collect_captures(rhs, local, outer, caps);
        }
        Call { callee, args } => {
            collect_captures(callee, local, outer, caps);
            args.iter().for_each(|a| collect_captures(a, local, outer, caps));
        }
        Index { obj, index } => { collect_captures(obj, local, outer, caps); collect_captures(index, local, outer, caps); }
        Member { obj, .. } => collect_captures(obj, local, outer, caps),
        TypeArgs { expr, .. } => collect_captures(expr, local, outer, caps),
        Tuple(items) | Array(items) => items.iter().for_each(|i| collect_captures(i, local, outer, caps)),
        StructLit { fields, .. } => fields.iter().for_each(|f| if let Some(v) = &f.value { collect_captures(v, local, outer, caps) }),
        If { cond, then, els } => {
            collect_captures(cond, local, outer, caps);
            let mut l2 = local.clone();
            collect_captures_block(then, &mut l2, outer, caps);
            if let Some(e2) = els { collect_captures(e2, local, outer, caps); }
        }
        Match { expr, arms } => {
            collect_captures(expr, local, outer, caps);
            for a in arms {
                let mut l2 = local.clone();
                bind_pattern(&a.pattern, &mut l2);
                collect_captures(&a.expr, &mut l2, outer, caps);
            }
        }
        Closure { params, body, .. } => {
            let mut l2 = local.clone();
            for p in params { l2.insert(p.name.clone()); }
            collect_captures(body, &mut l2, outer, caps);
        }
        Scope { param, body } => {
            let mut l2 = local.clone();
            l2.insert(param.clone());
            collect_captures_block(body, &mut l2, outer, caps);
        }
        Own { body, .. } => collect_captures_block(body, local, outer, caps),
        BlockExpr(b) => collect_captures_block(b, local, outer, caps),
        _ => {}
    }
}

fn collect_captures_block(b: &ast::Block, local: &mut HashSet<String>, outer: &HashSet<String>, caps: &mut Vec<String>) {
    for s in &b.stmts {
        match s {
            ast::Stmt::Let { pattern, expr, .. } => {
                collect_captures(expr, local, outer, caps);
                bind_pattern(pattern, local);
            }
            ast::Stmt::Return(e) => if let Some(e) = e { collect_captures(e, local, outer, caps); }
            ast::Stmt::For { pattern, iter, body } => {
                collect_captures(iter, local, outer, caps);
                let mut l2 = local.clone();
                bind_pattern(pattern, &mut l2);
                collect_captures_block(body, &mut l2, outer, caps);
            }
            ast::Stmt::While { cond, body } => { collect_captures(cond, local, outer, caps); collect_captures_block(body, local, outer, caps); }
            ast::Stmt::Assign { target, value, .. } => { collect_captures(target, local, outer, caps); collect_captures(value, local, outer, caps); }
            ast::Stmt::Expr(e) => collect_captures(e, local, outer, caps),
        }
    }
    if let Some(t) = &b.tail { collect_captures(t, local, outer, caps); }
}
