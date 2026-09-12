//! Ctron 解释器(CVM):执行行为/panic 语料。
//! 值模型 Rc 化;任务为急切执行 + 阻塞挂起 + 取消重跑(§7 语义的语料级实现)。

use crate::ast::{self, Expr};
use crate::sem::{self, DefId, DefKind, IntW, Sema, Symbol, Ty};
use crate::token::Span;
use std::cell::{Cell, RefCell};
use std::collections::{HashMap, HashSet};
use std::rc::Rc;



#[derive(Clone)]
pub enum Value {
    Void,
    Int(i64),
    /// 宽度标记整数(let 注解/字面量后缀/as 转换产生,§3.6 溢出与回绕语义)
    IntW(IntW, i64),
    UInt(u64),
    UIntW(IntW, u64),
    F64(f64),
    F32(f32),
    Bool(bool),
    Str(Rc<String>),
    Enum { def: DefId, variant: usize, payload: Vec<Value> },
    Struct { def: DefId, fields: Rc<RefCell<Vec<(String, Value)>>> },
    Class { def: DefId, fields: Rc<RefCell<Vec<(String, Value)>>> },
    Boxed(Rc<Value>),
    Tuple(Vec<Value>),
    Array(Rc<RefCell<Vec<Value>>>),
    Range { from: i64, to: i64, inclusive: bool },
    Closure { params: Vec<ast::ClosureParam>, body: Rc<Expr>, env: Rc<Env> },
    FnRef { id: usize },
    Simd(Vec<f64>),
    Task(u32),
    Chan { id: u32, sender: bool },
    MutexInst(Rc<RefCell<Value>>),
    Atomic(Rc<Cell<i64>>),
    GlobalRef(Rc<Cell<i64>>),
    Parallel,
    Arena,
    AnyError { message: Rc<String>, cause: Option<Rc<Value>>, trace: Rc<String> },
}


pub struct Local { pub value: Value }

pub struct Env {
    vars: RefCell<HashMap<String, Local>>,
    parent: Option<Rc<Env>>,
}

impl Env {
    pub fn new() -> Rc<Env> { Rc::new(Env { vars: RefCell::new(HashMap::new()), parent: None }) }
    pub fn child(parent: &Rc<Env>) -> Rc<Env> {
        Rc::new(Env { vars: RefCell::new(HashMap::new()), parent: Some(parent.clone()) })
    }
    pub fn define(&self, name: String, l: Local) { self.vars.borrow_mut().insert(name, l); }
    /// 就近更新既有绑定(含外层作用域);找不到时返回 false
    pub fn assign(&self, name: &str, l: Local) -> bool {
        if self.vars.borrow().contains_key(name) {
            self.vars.borrow_mut().insert(name.to_string(), l);
            return true;
        }
        self.parent.as_ref().map(|p| p.assign(name, l)).unwrap_or(false)
    }
    pub fn get(&self, name: &str) -> Option<Local> {
        if let Some(l) = self.vars.borrow().get(name) { return Some(Local { value: l.value.clone() }); }
        self.parent.as_ref().and_then(|p| p.get(name))
    }
}

#[derive(Clone, Copy, PartialEq)]
pub enum TaskStatus { Pending, Completed, Panicked, Blocked }

pub struct Task {
    pub closure: Rc<Expr>,
    pub env: Rc<Env>,
    pub status: TaskStatus,
    pub result: Option<Value>,
    pub panic_msg: Option<String>,
}

pub struct ChannelState {
    pub queue: std::collections::VecDeque<Value>,
    capacity: usize,
}

pub type EvalResult = Result<Value, Flow>;

struct MatchCtx { matched: bool }

pub enum Flow {
    None,
    Panic(String),
    Blocked,
    EarlyReturn(Value),
    // v0.7 修订二:绑最近 enclosing 循环,由 For/While 执行器捕获
    Break,
    Continue,
}

pub struct Interp<'a> {
    pub sema: &'a Sema,
    pub module: String,
    pub file: Rc<ast::File>,
    pub globals: Rc<Env>,
    pub consts: RefCell<HashMap<String, Value>>,
    pub statics: RefCell<HashMap<String, Value>>,
    pub channels: RefCell<HashMap<u32, Rc<RefCell<ChannelState>>>>,
    pub tasks: RefCell<HashMap<u32, Rc<RefCell<Task>>>>,
    pub task_next: Cell<u32>,
    pub chan_next: Cell<u32>,
    pub atomics: RefCell<HashMap<String, Rc<Cell<i64>>>>,
    pub globals_rt: RefCell<HashMap<String, Rc<Cell<i64>>>>,
    pub cancelled_scopes: RefCell<HashSet<u32>>,
    pub expr_depth: Cell<u32>,
    pub steps: Cell<u64>,
    pub in_own: bool,
    pub interp_cache: RefCell<HashMap<String, ast::Expr>>,
    pub cur_ret_e_name: RefCell<Option<String>>,
}

impl<'a> Interp<'a> {
    pub fn new(sema: &'a Sema, module: String, file: Rc<ast::File>) -> Self {
        Interp {
            sema, module, file, globals: Env::new(),
            consts: RefCell::new(HashMap::new()), statics: RefCell::new(HashMap::new()),
            channels: RefCell::new(HashMap::new()), tasks: RefCell::new(HashMap::new()),
            task_next: Cell::new(0), chan_next: Cell::new(0),
            atomics: RefCell::new(HashMap::new()), globals_rt: RefCell::new(HashMap::new()),
            cancelled_scopes: RefCell::new(HashSet::new()),
            expr_depth: Cell::new(0), steps: Cell::new(0), in_own: false,
            interp_cache: RefCell::new(HashMap::new()),
            cur_ret_e_name: RefCell::new(None),
        }
    }

    fn err(&mut self, code: &'static str, msg: String, _span: Span) {
        // interp 层的诊断转为 panic(运行时无诊断队列)
        eprintln!("[ctron:{}] {}: {}", self.module, code, msg);
    }

    fn fresh_task(&self) -> u32 { let v = self.task_next.get(); self.task_next.set(v + 1); v }
    fn fresh_chan(&self) -> u32 { let v = self.chan_next.get(); self.chan_next.set(v + 1); v }

    // ---------- 顶层 ----------

    pub fn run_tests(&mut self, file: &ast::File) -> Vec<(String, Result<(), String>)> {
        register_sema_names(self.sema);
        let mut results = Vec::new();
        self.eval_globals(file);
        for d in &file.decls {
            if let ast::Decl::Test(t) = d {
                let env = Env::child(&self.globals);
                let r = match self.check_block(&t.body, &env) {
                    Ok(_) | Err(Flow::EarlyReturn(_)) => Ok(()),
                    Err(Flow::Panic(m)) => Err(m),
                    Err(_) => Err("异常控制流".into()),
                };
                results.push((t.name.clone(), r));
            }
        }
        // R-P2c 发现面:零参 fn test_* 与 test 块同一机制执行(返回值忽略)
        for d in &file.decls {
            if let ast::Decl::Fn(f) = d {
                if f.name.starts_with("test_") && f.params.is_empty() {
                    if let Some(body) = &f.body {
                        let env = Env::child(&self.globals);
                        let r = match self.check_block(body, &env) {
                            Ok(_) | Err(Flow::EarlyReturn(_)) => Ok(()),
                            Err(Flow::Panic(m)) => Err(m),
                            Err(_) => Err("异常控制流".into()),
                        };
                        results.push((f.name.clone(), r));
                    }
                }
            }
        }
        results
    }

    fn eval_globals(&mut self, file: &ast::File) {
        for d in &file.decls {
            match d {
                ast::Decl::Const(c) => {
                    let env = Env::child(&self.globals);
                    if let Ok(v) = self.expr(&c.expr, &env) {
                        self.consts.borrow_mut().insert(c.name.clone(), v);
                    }
                }
                ast::Decl::Static(s) => {
                    let env = Env::child(&self.globals);
                    if let Ok(v) = self.expr(&s.expr, &env) {
                        self.statics.borrow_mut().insert(s.name.clone(), v);
                    }
                }
                _ => {}
            }
        }
    }

    /// 运行 fn main(D1;对齐 C 版 ctron_rt_run_main):
    /// const/static 预求值后执行 main 体,EarlyReturn 值 → 退出码;panic → Err。
    pub fn run_main(&mut self) -> Result<i32, String> {
        let body = self.file.decls.iter().find_map(|d| match d {
            ast::Decl::Fn(f) if f.name == "main" => f.body.clone(),
            _ => None,
        });
        let Some(body) = body else { return Err("缺少 fn main".into()) };
        let env = Env::child(&self.globals);
        match self.check_block(&body, &env) {
            Ok(_) => Ok(0),
            Err(Flow::EarlyReturn(v)) => Ok(exit_code_of(v)),
            Err(Flow::Panic(m)) => Err(m),
            Err(_) => Err("异常控制流".into()),
        }
    }

    // ---------- 块与语句 ----------

    fn check_block(&mut self, block: &ast::Block, env: &Rc<Env>) -> EvalResult {
        let child = Env::child(env);
        let mut drop_stack: Vec<(Value, DefId)> = Vec::new();
        let r = self.exec_stmts(&block.stmts, &child, &mut drop_stack);
        let out = match r {
            Ok(()) => match block.tail.as_deref() {
                Some(t) => self.expr(t, &child),
                None => Ok(Value::Void),
            },
            Err(f) => Err(f),
        };
        for (v, def) in drop_stack.iter().rev() { self.run_drop(v, *def); }
        out
    }

    fn exec_stmts(&mut self, stmts: &[ast::Stmt], env: &Rc<Env>, drops: &mut Vec<(Value, DefId)>) -> Result<(), Flow> {
        for s in stmts { self.exec_stmt(s, env, drops)?; }
        Ok(())
    }

    fn lower_local_ty(&mut self, t: &ast::Type) -> Ty {
        self.sema.lower_ty_pub(t)
    }

    fn exec_stmt(&mut self, s: &ast::Stmt, env: &Rc<Env>, drops: &mut Vec<(Value, DefId)>) -> Result<(), Flow> {
        match s {
            ast::Stmt::Let { pattern, ty: ann, expr, .. } => {
                let v = self.expr(expr, env)?;
                // 注解类型强制(§3.6 字面量自适应)
                let v = if let Some(ann_ty) = ann {
                    let target = self.lower_local_ty(ann_ty);
                    self.coerce_literal(&v, &target)
                } else { v };
                let v = self.copy_if_value(&v);
                if let Some(dd) = self.def_has_drop(&v) { drops.push((v.clone(), dd)); }
                self.scopes_push_bind(pattern, &v, env);
                Ok(())
            }
            ast::Stmt::Return(e) => {
                let v = match e { Some(e) => self.expr(e, env)?, None => Value::Void };
                Err(Flow::EarlyReturn(v))
            }
            // v0.7 修订二:向最近 enclosing 循环传播(For/While 执行器捕获)
            ast::Stmt::Break => Err(Flow::Break),
            ast::Stmt::Continue => Err(Flow::Continue),
            ast::Stmt::For { pattern, iter, body } => {
                let it = self.expr(iter, env)?;
                let items: Vec<Value> = match &it {
                    Value::Range { from, to, inclusive } => (*from..(*to + *inclusive as i64)).map(Value::Int).collect(),
                    Value::Array(arr) => arr.borrow().clone(),
                    Value::Str(s) => s.chars().map(|c| Value::Str(Rc::new(c.to_string()))).collect(),
                    _ => vec![],
                };
                for item in items {
                    self.scopes_push_bind(pattern, &item, env);
                    let mut sub = Vec::new();
                    let r = self.exec_stmts(body.stmts.as_slice(), env, &mut sub);
                    // break/continue(及既有 EarlyReturn/panic 路径)也先跑本轮 drop
                    for (v, def) in sub.iter().rev() { self.run_drop(v, *def); }
                    match r {
                        Err(Flow::Break) => break,
                        Err(Flow::Continue) => continue,
                        Err(e) => return Err(e),
                        Ok(()) => {}
                    }
                    if let Some(t) = &body.tail { self.expr(t, env)?; }
                }
                Ok(())
            }
            ast::Stmt::While { cond, body } => {
                'outer: loop {
                    let c = self.expr(cond, env)?;
                    if !truthy(&c) { break; }
                    let mut sub = Vec::new();
                    let r = self.exec_stmts(body.stmts.as_slice(), env, &mut sub);
                    for (v, def) in sub.iter().rev() { self.run_drop(v, *def); }
                    match r {
                        Err(Flow::Break) => break 'outer,
                        Err(Flow::Continue) => continue,
                        Err(e) => return Err(e),
                        Ok(()) => {}
                    }
                    if let Some(t) = &body.tail { self.expr(t, env)?; }
                }
                Ok(())
            }
            ast::Stmt::Assign { target, op, value } => {
                let v = self.expr(value, env)?;
                self.assign_target(target, env, op, v)?;
                Ok(())
            }
            ast::Stmt::Expr(e) => { self.expr(e, env)?; Ok(()) }
        }
    }

    fn scopes_push_bind(&mut self, pattern: &ast::Pattern, value: &Value, env: &Rc<Env>) {
        let mut binds = HashMap::new();
        self.bind_pattern(pattern, value, &mut binds);
        for (n, l) in binds { env.define(n, l); }
    }

    fn bind_pattern(&mut self, p: &ast::Pattern, v: &Value, out: &mut HashMap<String, Local>) {
        let v = self.copy_if_value(v);
        match p {
            ast::Pattern::Ident(n) => { out.insert(n.clone(), Local { value: v }); }
            ast::Pattern::Wildcard => {}
            ast::Pattern::Lit(_) => {}
            ast::Pattern::Tuple(ps) => {
                if let Value::Tuple(items) = &v {
                    for (i, sp) in ps.iter().enumerate() {
                        if let Some(item) = items.get(i) { self.bind_pattern(sp, item, out); }
                    }
                }
            }
            ast::Pattern::Agg { sub, .. } => match sub {
                ast::AggSub::Tuple(ps) => {
                    if let Value::Enum { payload, .. } = &v {
                        for (i, sp) in ps.iter().enumerate() {
                            if let Some(pv) = payload.get(i) { self.bind_pattern(sp, pv, out); }
                        }
                    }
                }
                ast::AggSub::Struct(fs) => {
                    for f in fs {
                        let fv = self.member(&v, &f.name).ok();
                        match (&f.pattern, fv) {
                            (Some(bp), Some(fv)) => self.bind_pattern(bp, &fv, out),
                            (None, Some(fv)) => { out.insert(f.name.clone(), Local { value: fv }); }
                            _ => {}
                        }
                    }
                }
                ast::AggSub::Unit => {}
            },
        }
    }

    fn assign_target(&mut self, target: &ast::Expr, env: &Rc<Env>, op: &ast::AssignOp, v: Value) -> Result<(), Flow> {
        match target {
            Expr::Ident(n) => {
                let cur = env.get(n).map(|l| l.value).unwrap_or(Value::Void);
                let nv = apply_assign_op(op, &cur, &v)?;
                if !env.assign(n, Local { value: nv.clone() }) {
                    env.define(n.clone(), Local { value: nv });
                }
                Ok(())
            }
            Expr::Member { obj, target: mt } => {
                let recv = self.expr(obj, env)?;
                let name = member_name(mt);
                let cur = self.member(&recv, &name)?;
                let nv = apply_assign_op(op, &cur, &v)?;
                self.set_member(&recv, &name, nv);
                Ok(())
            }
            Expr::Index { obj, index } => {
                let o = self.expr(obj, env)?;
                let i = self.expr(index, env)?;
                let idx = runtime_index_usize(&o, &i);
                if let Value::Array(arr) = &o {
                    let mut b = arr.borrow_mut();
                    if idx >= b.len() { return Err(Flow::Panic("index out of bounds".into())); }
                    match op {
                        ast::AssignOp::Eq => b[idx] = v,
                        other => {
                            let cur = b[idx].clone();
                            b[idx] = apply_assign_op(other, &cur, &v)?;
                        }
                    }
                }
                Ok(())
            }
            _ => Ok(()),
        }
    }

    // ---------- 表达式 ----------

    fn expr(&mut self, e: &ast::Expr, env: &Rc<Env>) -> EvalResult {
        // 步数上限:防无限循环(默认 2_000_000;CTRON_MAX_STEPS 覆盖,0 = 无限;D2)
        static MAX_STEPS: std::sync::OnceLock<u64> = std::sync::OnceLock::new();
        let max_steps = *MAX_STEPS.get_or_init(|| {
            std::env::var("CTRON_MAX_STEPS").ok().and_then(|v| v.parse::<u64>().ok()).unwrap_or(2_000_000)
        });
        self.steps.set(self.steps.get() + 1);
        if max_steps > 0 && self.steps.get() > max_steps {
            return Err(Flow::Panic("instruction limit exceeded (可能的无限循环)".into()));
        }
        self.expr_depth.set(self.expr_depth.get() + 1);
        if self.expr_depth.get() > 256 {
            self.expr_depth.set(self.expr_depth.get() - 1);
            return Err(Flow::Panic("表达式嵌套过深".into()));
        }
        let r = self.expr_inner(e, env);
        self.expr_depth.set(self.expr_depth.get() - 1);
        r
    }

    fn expr_inner(&mut self, e: &ast::Expr, env: &Rc<Env>) -> EvalResult {
        match e {
            Expr::Int { text, suffix } => {
                let cleaned = text.replace('_', "");
                let v = parse_int(&cleaned).unwrap_or(0);
                if !suffix.is_empty() {
                    return Ok(match suffix.as_str() {
                        "u8" => Value::UIntW(IntW::W8, v as u64),
                        "u16" => Value::UIntW(IntW::W16, v as u64),
                        "u32" => Value::UIntW(IntW::W32, v as u64),
                        "u64" | "usize" => Value::UInt(v as u64),
                        "i8" => Value::IntW(IntW::W8, v),
                        "i16" => Value::IntW(IntW::W16, v),
                        "i32" => Value::IntW(IntW::W32, v),
                        "i64" | "isize" => Value::Int(v),
                        _ => Value::Int(v),
                    });
                }
                Ok(Value::Int(v))
            }
            Expr::Float { text, suffix } => {
                let cleaned = text.replace('_', "");
                let v: f64 = cleaned.parse().unwrap_or(0.0);
                if suffix == "F32" { return Ok(Value::F32(v as f32)); }
                Ok(Value::F64(v))
            }
            Expr::Str { parts } => {
                let mut out = String::new();
                for p in parts {
                    match p {
                        ast::StrPart::Text(t) => out.push_str(t),
                        ast::StrPart::Interp(src) => {
                            let v = self.eval_interp(src, env)?;
                            out.push_str(&to_display(&v));
                        }
                    }
                }
                Ok(Value::Str(Rc::new(out)))
            }
            Expr::Bool(b) => Ok(Value::Bool(*b)),
            Expr::Void => Ok(Value::Void),
            Expr::Ident(name) => self.eval_ident(name, env),
            Expr::Tuple(items) => {
                let mut vs = Vec::new();
                for i in items { vs.push(self.expr(i, env)?); }
                Ok(Value::Tuple(vs))
            }
            Expr::Array(items) => {
                let mut vs = Vec::new();
                for i in items { vs.push(self.expr(i, env)?); }
                Ok(Value::Array(Rc::new(RefCell::new(vs))))
            }
            Expr::StructLit { path, fields, .. } => {
                let name = path.last().cloned().unwrap_or_default();
                let Some(def) = self.sema.def_by_name.get(&name).copied() else {
                    return Err(Flow::Panic(format!("未解析的类型 `{}`", name)));
                };
                let mut fv = Vec::new();
                for f in fields {
                    let v = self.expr(f.value.as_ref().unwrap_or(&ast::Expr::Void), env)?;
                    fv.push((f.name.clone(), v));
                }
                Ok(Value::Struct { def, fields: Rc::new(RefCell::new(fv)) })
            }
            Expr::Unary { op, expr } => {
                let v = self.expr(expr, env)?;
                match op {
                    ast::UnOp::Neg => Ok(match v {
                        Value::Int(i) => Value::Int(-i),
                        Value::UInt(u) => Value::UInt(u.wrapping_neg()),
                        Value::F64(f) => Value::F64(-f),
                        Value::F32(f) => Value::F32(-f),
                        other => other,
                    }),
                    ast::UnOp::Not => Ok(Value::Bool(!truthy(&v))),
                }
            }
            Expr::Binary { op, lhs, rhs } => {
                let a = self.expr(lhs, env)?;
                self.eval_binop(op, &a, rhs, env)
            }
            Expr::Range { inclusive, from, to } => {
                let Value::Int(f) = self.expr(from, env)? else { return Ok(Value::Void) };
                let Value::Int(t) = self.expr(to, env)? else { return Ok(Value::Void) };
                Ok(Value::Range { from: f, to: t, inclusive: *inclusive })
            }
            Expr::Call { callee, args } => self.eval_call(callee, args, env),
            Expr::Index { obj, index } => {
                let o = self.expr(obj, env)?;
                let i = self.expr(index, env)?;
                runtime_index(&o, &i)
            }
            Expr::Member { obj, target } => {
                let o = self.expr(obj, env)?;
                self.member(&o, &member_name(target))
            }
            Expr::TypeArgs { expr, args } => {
                // 非调用位置的泛型类型表达式:Simd[F32, N] → 定宽标记值
                if let Expr::Ident(ty_name) = &**expr {
                    if ty_name == "Simd" {
                        let n = args.get(1).and_then(|t| match t {
                            ast::Type::ComptimeVal(s) => s.trim().parse::<usize>().ok(),
                            ast::Type::Named { path, .. } => path.last().and_then(|s| s.trim().parse::<usize>().ok()),
                            _ => None,
                        }).unwrap_or(4);
                        return Ok(Value::Simd(vec![0.0; n.max(1)]));
                    }
                }
                self.expr(expr, env)
            }
            Expr::Try(e) => {
                let v = self.expr(e, env)?;
                match v {
                    Value::Enum { def, variant, mut payload } => {
                        let vname = self.variant_name(def, variant);
                        match vname.as_str() {
                            "Some" | "Ok" => Ok(payload.pop().unwrap_or(Value::Void)),
                            _ => {
                                let inner_err = payload.pop().unwrap_or(Value::Void);
                                let is_err_variant = self.variant_name(def, variant) == "Err";
                                if self.cur_ret_e_name.borrow().as_deref() == Some("AnyError")
                                    && is_err_variant
                                    && !matches!(inner_err, Value::AnyError { .. })
                                {
                                    let tr = Rc::new(format!("{}:1", self.module));
                                    let m = to_display(&inner_err);
                                    Ok(Value::AnyError { message: Rc::new(m), cause: Some(Rc::new(inner_err)), trace: tr })
                                } else {
                                    Err(Flow::EarlyReturn(Value::Enum { def, variant, payload: vec![inner_err] }))
                                }
                            }
                        }
                    }
                    other => Err(Flow::Panic(format!("`?` 作用于非 Result/Option:{}", to_display(&other)))),
                }
            }
            Expr::Closure { params, body, .. } => Ok(Value::Closure {
                params: params.clone(), body: Rc::new((**body).clone()), env: env.clone(),
            }),
            // (call_fn_value 的 ast-args 包装在下方 call_fn_value_ast)
            Expr::Scope { param, body } => {
                let sid = self.fresh_scope();
                let env2 = Env::child(env);
                env2.define(param.clone(), Local { value: Value::Void });
                let r = self.exec_scope_block(body, &env2, sid);
                self.cancelled_scopes.borrow_mut().insert(sid);
                r
            }
            Expr::Own { arena, body } => {
                let saved = self.in_own;
                self.in_own = true;
                let arena_env = Env::child(env);
                arena_env.define(arena.clone(), Local { value: Value::Arena });
                let r = self.check_block(body, &arena_env);
                self.in_own = saved;
                r
            }
            Expr::If { cond, then, els } => {
                let c = self.expr(cond, env)?;
                if truthy(&c) {
                    self.check_block(then, env)
                } else if let Some(e2) = els {
                    self.expr(e2, env)
                } else { Ok(Value::Void) }
            }
            Expr::Match { expr: scrut, arms } => {
                let sv = self.expr(scrut, env)?;
                for arm in arms {
                    let arm_env = Env::child(env);
                    let mut binds = HashMap::new();
                    if self.try_match(&arm.pattern, &sv, &arm_env, &mut binds) {
                        for (n, l) in binds { arm_env.define(n, l); }
                        return self.expr(&arm.expr, &arm_env);
                    }
                }
                Err(Flow::Panic("match 无匹配臂".into()))
            }
            Expr::BlockExpr(b) => self.check_block(b, env),
        }
    }

    fn fresh_scope(&self) -> u32 {
        thread_local! { static N: Cell<u32> = Cell::new(0); }
        let v = N.get() + 1; N.set(v); v
    }

    fn exec_scope_block(&mut self, body: &ast::Block, env: &Rc<Env>, sid: u32) -> EvalResult {
        self.cancelled_scopes.borrow_mut().insert(sid);
        self.check_block(body, env)
    }

    // ---------- 调用 ----------

    fn eval_call(&mut self, callee: &ast::Expr, args: &[ast::Expr], env: &Rc<Env>) -> EvalResult {
        match callee {
            Expr::Ident(name) => self.call_ident(name, args, env),
            Expr::Member { obj, target } => {
                let m = member_name(target);
                if let Expr::Ident(on) = &**obj {
                    if let Some(Symbol::Module(mp)) = self.module_symbol(on) {
                        if mp == "stdweb.dom" {
                            return match m.as_str() {
                                "set_title" => { for a in args { self.expr(a, env)?; } Ok(Value::Void) }
                                "title" => Ok(Value::Str(Rc::new("Ctron".into()))),
                                _ => Err(Flow::Panic(format!("stdweb.dom 无函数 `{}`", m))),
                            };
                        }
                    }
                }
                self.eval_method(obj, &m, args, env)
            }
            Expr::TypeArgs { expr: inner, args: type_args_ast } => {
                // 1) inner = Ident(类型名) → 泛型类型构造器:Atomic[I32](0), Box[T](v), Channel[T](cap)
                if let Expr::Ident(ty_name) = &**inner {
                    let mut vals = Vec::new();
                    for a in args { vals.push(self.expr(a, env)?); }
                    return match ty_name.as_str() {
                        "List" => Ok(Value::Array(Rc::new(RefCell::new(Vec::new())))), // List[T]() 空表(D1)
                        "Atomic" => Ok(Value::Atomic(Rc::new(Cell::new(
                            vals.first().and_then(|v| if let Value::Int(i) = v { Some(*i) } else { None }).unwrap_or(0)
                        )))),
                        "Mutex" => Ok(Value::MutexInst(Rc::new(RefCell::new(
                            vals.first().cloned().unwrap_or(Value::Void)
                        )))),
                        "Global" => Ok(Value::GlobalRef(Rc::new(Cell::new(
                            vals.get(1).and_then(|v| if let Value::Int(i) = v { Some(*i) } else { None }).unwrap_or(0)
                        )))),
                        "Box" => Ok(Value::Boxed(Rc::new(vals.first().cloned().unwrap_or(Value::Void)))),
                        "Simd" => {
                            // Simd[F32, N] → 定宽标记(长度即 N),后续 .splat() 填充
                            let n = type_args_ast.get(1).and_then(|t| match t {
                                ast::Type::ComptimeVal(s) => s.trim().parse::<usize>().ok(),
                                ast::Type::Named { path, .. } => path.last().and_then(|s| s.trim().parse::<usize>().ok()),
                                _ => None,
                            }).unwrap_or(4);
                            Ok(Value::Simd(vec![0.0; n.max(1)]))
                        }
                        "Channel" => {
                            let ch_id = self.fresh_chan();
                            let cap = vals.first().and_then(|v| if let Value::Int(i) = v { Some(*i as usize) } else { Some(16) }).unwrap_or(16);
                            self.channels.borrow_mut().insert(ch_id, Rc::new(RefCell::new(ChannelState {
                                queue: std::collections::VecDeque::new(), capacity: cap,
                            })));
                            let sender = Value::Chan { id: ch_id, sender: true };
                            let receiver = Value::Chan { id: ch_id, sender: false };
                            Ok(Value::Tuple(vec![sender, receiver]))
                        }
                        _ => Ok(Value::Void),
                    };
                }
                // 2) inner = Member(泛型方法):arena.list[I32](), x.as[I8]() 等
                if let Expr::Member { obj: recv_expr, target: ast::MemberTarget::Name(method) } = &**inner {
                    let recv = self.expr(recv_expr, env)?;
                    // `as` 方法:数值转换(截断语义,§3.6)
                    if method == "as" {
                        let targ = type_args_ast.iter().filter_map(|t| match t {
                            ast::Type::Named { path, .. } => path.last().cloned(),
                            _ => None,
                        }).next().unwrap_or_default();
                        return Ok(convert_as(&recv, &targ));
                    }
                    // Arena 泛型方法
                    if matches!(recv, Value::Arena) {
                        return match method.as_str() {
                            "list" => Ok(Value::Array(Rc::new(RefCell::new(vec![])))),
                            "array" | "zeros" => {
                                let n = if let Some(Some(a)) = args.first().map(|a| self.expr(a, env).ok()) {
                                    match a { Value::Int(n) => n as usize, _ => 0 }
                                } else { 0 };
                                Ok(Value::Array(Rc::new(RefCell::new(vec![Value::Int(0); n]))))
                            }
                            _ => Err(Flow::Panic(format!("Arena 无方法 `{}`", method))),
                        };
                    }
                }
                // 3) 常规调用:先求值 inner(可能是链式成员),再分发
                let f = self.expr(inner, env)?;
                self.call_fn_value_ast(&f, args, env)
            }
            _ => {
                let f = self.expr(callee, env)?;
                self.call_fn_value_ast(&f, args, env)
            }
        }
    }

    fn module_symbol(&self, name: &str) -> Option<Symbol> {
        self.sema.mods.iter().find(|m| m.path == self.module)
            .and_then(|m| m.symbols.get(name)).cloned()
    }

    fn call_ident(&mut self, name: &str, args: &[ast::Expr], env: &Rc<Env>) -> EvalResult {
        if let Some(l) = env.get(name) {
            if matches!(l.value, Value::Closure { .. } | Value::FnRef { .. }) {
                return self.call_fn_value_ast(&l.value, args, env);
            }
        }
        match name {
            "assert" => {
                let c = self.expr(&args[0], env)?;
                if !truthy(&c) { return Err(Flow::Panic("assertion failed".into())); }
                return Ok(Value::Void);
            }
            "assert_eq" | "assert_ne" => {
                let a = self.expr(&args[0], env)?;
                let b = self.expr(&args[1], env)?;
                let same = values_equal(&a, &b);
                if (name == "assert_eq") != same {
                    return Err(Flow::Panic(format!("assert failed: {} vs {}", to_display(&a), to_display(&b))));
                }
                return Ok(Value::Void);
            }
            "panic" => {
                let msg = if let Some(a) = args.first() { to_display(&self.expr(a, env)?) } else { "panic".into() };
                return Err(Flow::Panic(msg));
            }
            _ => {}
        }
        // prelude 枚举变体构造器
        match name {
            "Some" => {
                let d = self.sema.def_by_name.get("Option").copied().unwrap_or(0);
                let mut payload = Vec::new();
                for a in args { payload.push(self.expr(a, env)?); }
                return Ok(Value::Enum { def: d, variant: 0, payload });
            }
            "None" => {
                let d = self.sema.def_by_name.get("Option").copied().unwrap_or(0);
                return Ok(Value::Enum { def: d, variant: 1, payload: vec![] });
            }
            "Ok" => {
                let d = self.sema.def_by_name.get("Result").copied().unwrap_or(0);
                let mut payload = Vec::new();
                for a in args { payload.push(self.expr(a, env)?); }
                return Ok(Value::Enum { def: d, variant: 0, payload });
            }
            "Err" => {
                let d = self.sema.def_by_name.get("Result").copied().unwrap_or(0);
                let mut payload = Vec::new();
                for a in args { payload.push(self.expr(a, env)?); }
                return Ok(Value::Enum { def: d, variant: 1, payload });
            }
            _ => {}
        }
        // print 内建(D1;格式面 = fmt_val:浮点整值 %.1f 否则 %g)
        if name == "println" || name == "print" {
            if args.len() != 1 { return Err(Flow::Panic(format!("{} 需单实参", name))); }
            let v = self.expr(&args[0], env)?;
            let mut out = String::new();
            fmt_value(&v, &mut out);
            if name == "println" { out.push('\n'); }
            print!("{}", out);
            return Ok(Value::Void);
        }
        // I/O 内建族(D1;语义镜像 C rt read_file/read_line/read_bytes/flush_out)
        if name == "read_file" {
            if args.len() != 1 { return Err(Flow::Panic("read_file 实参".into())); }
            let pv = self.expr(&args[0], env)?;
            let path = match &pv { Value::Str(s) => s.clone(), _ => Rc::new(String::new()) };
            let d = self.sema.def_by_name.get("Option").copied().unwrap_or(0);
            return match std::fs::read(&*path) {
                Ok(bytes) => Ok(Value::Enum {
                    def: d, variant: 0,
                    payload: vec![Value::Str(Rc::new(String::from_utf8_lossy(&bytes).to_string()))],
                }),
                Err(_) => Ok(Value::Enum { def: d, variant: 1, payload: vec![] }),
            };
        }
        if name == "read_line" {
            if !args.is_empty() { return Err(Flow::Panic("read_line 实参".into())); }
            let mut line = String::new();
            match std::io::stdin().read_line(&mut line) {
                Ok(0) => {}
                Ok(_) => { if line.ends_with('\n') { line.pop(); if line.ends_with('\r') { line.pop(); } } }
                Err(e) => return Err(Flow::Panic(format!("read_line: {}", e))),
            }
            return Ok(Value::Str(Rc::new(line)));
        }
        if name == "read_bytes" {
            if args.len() != 1 { return Err(Flow::Panic("read_bytes 实参".into())); }
            let nv = self.expr(&args[0], env)?;
            let want = match nv { Value::Int(i) | Value::IntW(_, i) => i, Value::UInt(u) | Value::UIntW(_, u) => u as i64, _ => 0 };
            if want < 0 { return Err(Flow::Panic("read_bytes 负长度".into())); }
            use std::io::Read;
            let mut buf = vec![0u8; want as usize];
            let mut got = 0usize;
            while got < buf.len() {
                match std::io::stdin().read(&mut buf[got..]) {
                    Ok(0) => break,
                    Ok(k) => got += k,
                    Err(e) => return Err(Flow::Panic(format!("read_bytes: {}", e))),
                }
            }
            buf.truncate(got);
            return Ok(Value::Str(Rc::new(String::from_utf8_lossy(&buf).to_string())));
        }
        if name == "flush_out" {
            use std::io::Write;
            let _ = std::io::stdout().flush();
            return Ok(Value::Void);
        }
        // 字节访问内建(D1;语义镜像 C rt byte_at/byte_slice)
        if name == "byte_at" {
            if args.len() != 2 { return Err(Flow::Panic("byte_at 实参".into())); }
            let sv = self.expr(&args[0], env)?;
            let iv = self.expr(&args[1], env)?;
            let s = match &sv { Value::Str(s) => s.clone(), _ => return Err(Flow::Panic("byte_at 目标需 Str".into())) };
            let i = match iv { Value::Int(i) | Value::IntW(_, i) => i, Value::UInt(u) | Value::UIntW(_, u) => u as i64, _ => 0 };
            let b = s.as_bytes();
            if i < 0 || (i as usize) >= b.len() { return Err(Flow::Panic("index out of bounds".into())); }
            return Ok(Value::Int(b[i as usize] as i64));
        }
        if name == "byte_slice" {
            if args.len() != 3 { return Err(Flow::Panic("byte_slice 实参".into())); }
            let sv = self.expr(&args[0], env)?;
            let av = self.expr(&args[1], env)?;
            let bv = self.expr(&args[2], env)?;
            let s = match &sv { Value::Str(s) => s.clone(), _ => return Err(Flow::Panic("byte_slice 目标需 Str".into())) };
            let a = match av { Value::Int(i) => i, Value::IntW(_, i) => i, _ => 0 };
            let b = match bv { Value::Int(i) | Value::IntW(_, i) => i, Value::UInt(u) | Value::UIntW(_, u) => u as i64, _ => 0 };
            let len = s.len() as i64;
            if a < 0 || b > len || a > b { return Err(Flow::Panic(format!("byte_slice 越界 (DBG a={} b={} len={})", a, b, len))); }
            return Ok(Value::Str(Rc::new(s[(a as usize)..(b as usize)].to_string())));
        }
        if let Some(sym) = self.module_symbol(name) {
            if let Symbol::Variant { def, idx } = sym {
                let mut payload = Vec::new();
                for a in args { payload.push(self.expr(a, env)?); }
                return Ok(Value::Enum { def, variant: idx, payload });
            }
            if let Symbol::Fn(id) = sym {
                let mut vals = Vec::new();
                for a in args { vals.push(self.expr(a, env)?); }
                return self.call_user_fn(id, &vals, env);
            }
        }
        Err(Flow::Panic(format!("未解析的调用 `{}`", name)))
    }

    fn eval_method(&mut self, obj: &ast::Expr, m: &str, args: &[ast::Expr], env: &Rc<Env>) -> EvalResult {
        let mut o = self.expr(obj, env)?;
        // Box 自动解引用(§3.3):字段/方法访问穿透
        if let Value::Boxed(inner) = &o {
            o = (**inner).clone();
        }

        // 标量 to_string(D1;fmt_val 同格式;method 位与属性位双覆盖)
        if m == "to_string" && args.is_empty() {
            let mut out = String::new();
            fmt_value(&o, &mut out);
            return Ok(Value::Str(Rc::new(out)));
        }

        if m == "spawn" {
            if let Some(ast::Expr::Closure { body, .. }) = args.first() {
                let tid = self.fresh_task();
                let task = Task {
                    closure: Rc::new((**body).clone()), env: env.clone(),
                    status: TaskStatus::Pending, result: None, panic_msg: None,
                };
                self.tasks.borrow_mut().insert(tid, Rc::new(RefCell::new(task)));
                self.run_task(tid);
                return Ok(Value::Task(tid));
            }
            return Err(Flow::Panic("spawn 需要闭包".into()));
        }

        match &o {
            Value::Task(tid) => {
                let tid = *tid;
                return match m {
                    "join" => {
                        self.resume_task(tid);
                        match self.task_status(tid) {
                            TaskStatus::Completed => Ok(self.task_result(tid).unwrap_or(Value::Void)),
                            TaskStatus::Panicked => Err(Flow::Panic(self.task_panic(tid).unwrap_or_default())),
                            _ => Ok(Value::Void),
                        }
                    }
                    "join_or" => {
                        self.resume_task(tid);
                        match self.task_status(tid) {
                            TaskStatus::Panicked => {
                                let msg = self.task_panic(tid).unwrap_or_else(|| "task panicked".into());
                                Ok(self.make_result(Value::Str(Rc::new(msg)), 1))
                            }
                            _ => Ok(self.make_result(self.task_result(tid).unwrap_or(Value::Void), 0)),
                        }
                    }
                    _ => Err(Flow::Panic(format!("Task 无方法 `{}`", m))),
                };
            }
            Value::Chan { id, sender: true } => {
                let chan_id = *id;
                let v = self.expr(&args[0], env)?;
                let room = {
                    let channels = self.channels.borrow();
                    channels.get(&chan_id).map(|c| c.borrow().queue.len() < c.borrow().capacity).unwrap_or(false)
                };
                let cancelled = !self.cancelled_scopes.borrow().is_empty();
                if room {
                    if let Some(ch) = self.channels.borrow().get(&chan_id) {
                        ch.borrow_mut().queue.push_back(v);
                    }
                    return Ok(self.make_result(Value::Void, 0));
                }
                if cancelled {
                    return Ok(self.make_result(Value::Str(Rc::new("ScopeCancelled".into())), 1));
                }
                return Err(Flow::Blocked);
            }
            Value::Chan { id, sender: false } => {
                let chan_id = *id;
                let v = {
                    let channels = self.channels.borrow();
                    channels.get(&chan_id).and_then(|c| c.borrow_mut().queue.pop_front())
                };
                match v {
                    Some(v) => return Ok(self.make_result(v, 0)),
                    None => {
                        if self.cancelled_scopes.borrow().is_empty() {
                            return Err(Flow::Blocked);
                        }
                        return Ok(self.make_result(Value::Str(Rc::new("ScopeCancelled".into())), 1));
                    }
                }
            }
            Value::MutexInst(inner) => return match m {
                "with" | "with_mut" => {
                    let f = self.expr(&args[0], env)?;
                    let cur = inner.borrow().clone();
                    match f {
                        Value::Closure { params, body, env: fenv } => {
                            let cenv = Env::child(&fenv);
                            if let Some(p) = params.first() { cenv.define(p.name.clone(), Local { value: cur }); }
                            match self.expr(&body, &cenv) {
                                Ok(v) => {
                                    if m == "with_mut" {
                                        if let Some(p) = params.first() {
                                            if let Some(l) = cenv.get(&p.name) {
                                                *inner.borrow_mut() = l.value;
                                            }
                                        }
                                    }
                                    Ok(v)
                                }
                                other => other,
                            }
                        }
                        _ => Err(Flow::Panic("with 需要闭包".into())),
                    }
                }
                _ => Err(Flow::Panic(format!("Mutex 无方法 `{}`", m))),
            },
            Value::Atomic(cell) => return match m {
                "load" => Ok(Value::Int(cell.get())),
                "store" => {
                    let v = self.expr(&args[0], env)?;
                    if let Value::Int(i) = v { cell.set(i); }
                    Ok(Value::Void)
                }
                "fetch_add" => {
                    let v = self.expr(&args[0], env)?;
                    if let Value::Int(d) = v {
                        let old = cell.get(); cell.set(old + d); Ok(Value::Int(old))
                    } else { Err(Flow::Panic("fetch_add 需要整数".into())) }
                }
                _ => Err(Flow::Panic(format!("Atomic 无方法 `{}`", m))),
            },
            Value::GlobalRef(cell) => return match m {
                "with" | "with_mut" => {
                    let f = self.expr(&args[0], env)?;
                    let cur = Value::Int(cell.get());
                    match f {
                        Value::Closure { params, body, env: fenv } => {
                            let cenv = Env::child(&fenv);
                            let pname = params.first().map(|p| p.name.clone());
                            if let Some(p) = params.first() { cenv.define(p.name.clone(), Local { value: cur }); }
                            match self.expr(&body, &cenv) {
                                Ok(v) => {
                                    if m == "with_mut" {
                                        // 回写参数终值(体可能为 void 块)
                                        if let Some(pn) = pname {
                                            if let Some(l) = cenv.get(&pn) {
                                                if let Value::Int(nv) = l.value { cell.set(nv); }
                                            }
                                        }
                                    }
                                    Ok(v)
                                }
                                other => other,
                            }
                        }
                        _ => Err(Flow::Panic("Global 需要闭包".into())),
                    }
                }
                _ => Err(Flow::Panic(format!("Global 无方法 `{}`", m))),
            },
            Value::Arena => return match m {
                "fixed" => { for a in args { self.expr(a, env)?; } Ok(Value::Arena) }
                _ => Err(Flow::Panic(format!("Arena 无关联函数 `{}`", m))),
            },
            Value::Parallel => return match m {
                "map" => {
                    let coll = self.expr(&args[0], env)?;
                    let f = self.expr(&args[1], env)?;
                    let elems = match &coll { Value::Array(arr) => arr.borrow().clone(), _ => vec![] };
                    let mut out = Vec::new();
                    for e in elems { out.push(self.call_fn_value(&f, &[e], env)?); }
                    Ok(Value::Array(Rc::new(RefCell::new(out))))
                }
                "reduce" => {
                    let coll = self.expr(&args[0], env)?;
                    let mut acc = self.expr(&args[1], env)?;
                    let f = self.expr(&args[2], env)?;
                    let elems = match &coll { Value::Array(arr) => arr.borrow().clone(), _ => vec![] };
                    for e in elems { acc = self.call_fn_value(&f, &[acc, e], env)?; }
                    Ok(acc)
                }
                _ => Err(Flow::Panic(format!("Parallel 无方法 `{}`", m))),
            },
            _ => {}
        }

        // 非阻塞成员方法(含错误级联的 Err 接收者)
        match &o {
            Value::Array(arr) => return match m {
                "push" => {
                    let v = self.expr(&args[0], env)?;
                    arr.borrow_mut().push(v);
                    Ok(Value::Void)
                }
                "pop" => Ok(arr.borrow_mut().pop().unwrap_or(Value::Void)),
                "len" => Ok(Value::Int(arr.borrow().len() as i64)),
                "into_gc" => {
                    // 深拷贝到新的 GC 数组(interp 中即新建独立 Rc)
                    let items = arr.borrow().clone();
                    Ok(Value::Array(Rc::new(RefCell::new(items))))
                }
                _ => Err(Flow::Panic(format!("Array 无方法 `{}`", m))),
            },
            Value::Str(s) => {
                let s = s.clone();
                return match m {
                    "len" => Ok(Value::Int(s.len() as i64)),
                    "char_len" => Ok(Value::Int(s.chars().count() as i64)),
                    "contains" => {
                        let a = self.expr(&args[0], env)?;
                        Ok(Value::Bool(s.contains(&to_display(&a))))
                    }
                    "slice" => {
                        let r = self.expr(&args[0], env)?;
                        if let Value::Range { from, to, .. } = r {
                            let b = s.as_bytes();
                            let (f, t) = (from as usize, to as usize);
                            if f > b.len() || t > b.len() || !s.is_char_boundary(f) || !s.is_char_boundary(t) {
                                return Err(Flow::Panic("invalid utf8 boundary".into()));
                            }
                            Ok(Value::Str(Rc::new(s[f..t].to_string())))
                        } else { Err(Flow::Panic("slice 需要 range".into())) }
                    }
                    "to_string" => { for a in args { self.expr(a, env)?; } Ok(Value::Str(Rc::new(s.to_string()))) }
                    "iter" => Ok(Value::Void),
                    _ => Err(Flow::Panic(format!("Str 无方法 `{}`", m))),
                };
            }
            Value::Simd(items) => {
                let items = items.clone();
                return match m {
                    "splat" => {
                        let v = self.expr(&args[0], env)?;
                        let f = match v { Value::F32(f) => f as f64, Value::F64(f) => f, Value::Int(i) => i as f64, _ => 0.0 };
                        let n = items.len().max(1);
                        Ok(Value::Simd(vec![f; n]))
                    }
                    "lane" => {
                        let i = self.expr(&args[0], env)?;
                        let Value::Int(i) = i else { return Err(Flow::Panic("lane 需要整数".into())) };
                        Ok(Value::F32(items.get(i as usize).copied().unwrap_or(0.0) as f32))
                    }
                    "to_array" => Ok(Value::Array(Rc::new(RefCell::new(
                        items.iter().map(|f| Value::F32(*f as f32)).collect())))),
                    _ => Err(Flow::Panic(format!("Simd 无方法 `{}`", m))),
                };
            }
            Value::Arena => return match m {
                "fixed" => { for a in args { self.expr(a, env)?; } Ok(Value::Arena) }
                _ => Err(Flow::Panic(format!("Arena 无关联函数 `{}`", m))),
            },
            Value::Parallel => return match m {
                "map" => {
                    let coll = self.expr(&args[0], env)?;
                    let f = self.expr(&args[1], env)?;
                    let elems = match &coll { Value::Array(arr) => arr.borrow().clone(), _ => vec![] };
                    let mut out = Vec::new();
                    for e in elems { out.push(self.call_fn_value(&f, &[e], env)?); }
                    Ok(Value::Array(Rc::new(RefCell::new(out))))
                }
                "reduce" => {
                    let coll = self.expr(&args[0], env)?;
                    let mut acc = self.expr(&args[1], env)?;
                    let f = self.expr(&args[2], env)?;
                    let elems = match &coll { Value::Array(arr) => arr.borrow().clone(), _ => vec![] };
                    for e in elems { acc = self.call_fn_value(&f, &[acc, e], env)?; }
                    Ok(acc)
                }
                _ => Err(Flow::Panic(format!("Parallel 无方法 `{}`", m))),
            },
            Value::Enum { def, variant, payload } => {
                let dname = self.def_name(*def);
                let vname = self.variant_name(*def, *variant);
                let is_ok_some = vname == "Ok" || vname == "Some";
                let value = payload.last().cloned().unwrap_or(Value::Void);
                if dname == "AnyError" {
                    return Err(Flow::Panic(format!("AnyError 无方法 `{}`", m)));
                }
                if dname == "Result" || dname == "Option" {
                    return match m {
                        "is_some" | "is_ok" => Ok(Value::Bool(is_ok_some)),
                        "is_none" | "is_err" => Ok(Value::Bool(!is_ok_some)),
                        "or" => {
                            let d = self.expr(&args[0], env)?;
                            Ok(if is_ok_some { value } else { d })
                        }
                        "expect" => {
                            let msg = to_display(&self.expr(&args[0], env)?);
                            if is_ok_some { Ok(value) } else { Err(Flow::Panic(msg)) }
                        }
                        "map" => {
                            let f = self.expr(&args[0], env)?;
                            if is_ok_some {
                                let v = self.call_fn_value(&f, &[value.clone()], env)?;
                                if dname == "Option" {
                                    Ok(Value::Enum { def: *def, variant: 0, payload: vec![v] })
                                } else {
                                    let e = payload.first().cloned().unwrap_or(Value::Void);
                                    Ok(Value::Enum { def: *def, variant: 0, payload: vec![v, e] })
                                }
                            } else {
                                Ok(Value::Enum { def: *def, variant: *variant, payload: payload.clone() })
                            }
                        }
                        "context" if dname == "Result" => {
                            let cm = to_display(&self.expr(&args[0], env)?);
                            if is_ok_some {
                                Ok(Value::Enum { def: *def, variant: *variant, payload: payload.clone() })
                            } else {
                                let cause = Rc::new(value.clone());
                                let tr = Rc::new(format!("{}:1", self.module));
                                Ok(Value::Enum { def: *def, variant: 1, payload: vec![Value::AnyError {
                                    message: Rc::new(cm), cause: Some(cause), trace: tr,
                                }] })
                            }
                        }
                        _ => Err(Flow::Panic(format!("{} 无方法 `{}`", dname, m))),
                    };
                }
                // trait 方法(impl 表)
                for im in &self.sema.impls {
                    if im.for_type != dname { continue; }
                    if self.trait_has_method(&im.trait_name, m).is_some() {
                        let mut vals = vec![o.clone()];
                        for a in args { vals.push(self.expr(a, env)?); }
                        return self.call_impl_method(&im.trait_name, &im.for_type, m, &vals, env);
                    }
                }
            }
            Value::Class { def, .. } | Value::Struct { def, .. } => {
                let dname = self.def_name(*def);
                for im in &self.sema.impls {
                    if im.for_type != dname { continue; }
                    if self.trait_has_method(&im.trait_name, m).is_some() {
                        let mut vals = vec![o.clone()];
                        for a in args { vals.push(self.expr(a, env)?); }
                        return self.call_impl_method(&im.trait_name, &im.for_type, m, &vals, env);
                    }
                }
                if let Some(Symbol::Fn(id)) = self.module_symbol(m) {
                    let mut vals = vec![o.clone()];
                    for a in args { vals.push(self.expr(a, env)?); }
                    return self.call_user_fn(id, &vals, env);
                }
                // @derive(Show):.show() 综合为格式化字符串(格式不钉死)
                if m == "show" {
                    return Ok(Value::Str(Rc::new(self.show_value(&o))));
                }
            }
            _ => {}
        }

        // 用户自由函数 UFCS
        if let Some(Symbol::Fn(id)) = self.module_symbol(m) {
            let mut vals = vec![o.clone()];
            for a in args { vals.push(self.expr(a, env)?); }
            return self.call_user_fn(id, &vals, env);
        }

        Err(Flow::Panic(format!("{} 无方法 `{}`", to_display(&o), m)))
    }

    // ---------- trait / impl 方法 ----------

    fn trait_has_method(&self, trait_name: &str, m: &str) -> Option<()> {
        // trait 方法的体在 AST;此处查 AST 中的 trait 声明
        if let Some(t) = self.file.decls.iter().find_map(|d| match d {
            ast::Decl::Trait(t) if t.name == trait_name => Some(t),
            _ => None,
        }) {
            if t.items.iter().any(|it| matches!(it,
                crate::ast::TraitItem::Method(mm) if mm.name == m))
            {
                return Some(());
            }
        }
        None
    }

    /// 调用 impl 方法:方法体来自本文件的 ast::ImplDecl
    fn call_impl_method(&mut self, trait_name: &str, for_type: &str, m: &str, vals: &[Value], env: &Rc<Env>) -> EvalResult {
        let dname = for_type.to_string();
        // self 值 = 第一个实参
        let self_val = vals.first().cloned().unwrap_or(Value::Void);
        let mut body: Option<ast::Block> = None;
        let mut param_names: Vec<String> = Vec::new();
        let mut found = false;
        for d in &self.file.decls {
            if let ast::Decl::Impl(im) = d {
                let tn = named_tail(&im.trait_ty);
                let ft = named_tail(&im.for_ty);
                if tn == trait_name && ft == for_type {
                    for item in &im.items {
                        if let ast::ImplItem::Method(mm) = item {
                            if mm.name == m {
                                found = true;
                                body = mm.body.clone();
                                for p in &mm.params {
                                    if let ast::Param::Param { name, .. } = p {
                                        param_names.push(name.clone());
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        if !found || body.is_none() {
            // 兜底: trait 默认体
            return self.call_trait_default(&trait_name, &dname, m, vals, env);
        }
        let saved_module = self.module.clone();
        let saved_ret = self.cur_ret_e_name.borrow().clone();
        self.module = self.module.clone();
        let fenv = Env::child(&self.globals);
        fenv.define("self".into(), Local { value: self_val });
        for (i, n) in param_names.iter().enumerate() {
            if let Some(v) = vals.get(i + 1) {
                let v = self.copy_if_value(v);
                fenv.define(n.clone(), Local { value: v });
            }
        }
        let r = self.check_block(&body.unwrap(), &fenv);
        self.module = saved_module;
        *self.cur_ret_e_name.borrow_mut() = saved_ret;
        match r {
            Ok(v) => Ok(v),
            Err(Flow::EarlyReturn(v)) => Ok(v),
            Err(Flow::Panic(m)) => Err(Flow::Panic(m)),
            Err(Flow::Blocked) => Err(Flow::Blocked),
            Err(Flow::None) => Ok(Value::Void),
            Err(Flow::Break | Flow::Continue) => Err(Flow::Panic("控制流越界: break/continue 穿越调用边界".into())),
        }
    }

    /// trait 默认方法体(03d describe)
    fn call_trait_default(&mut self, trait_name: &str, for_type: &str, m: &str, vals: &[Value], env: &Rc<Env>) -> EvalResult {
        let _ = env;
        let mut body: Option<ast::Block> = None;
        let mut self_ty: Option<ast::Type> = None;
        let mut param_names: Vec<String> = Vec::new();
        for d in &self.file.decls {
            if let ast::Decl::Trait(t) = d {
                if t.name != *trait_name { continue; }
                for item in &t.items {
                    if let crate::ast::TraitItem::Method(mm) = item {
                        if mm.name == *m {
                            body = mm.body.clone();
                            for p in &mm.params {
                                match p {
                                    ast::Param::Receiver { .. } => {}
                                    ast::Param::Param { name, .. } => param_names.push(name.clone()),
                                }
                            }
                        }
                    }
                }
                self_ty = Some(ast::Type::Named { path: vec![for_type.to_string()], args: vec![] });
            }
        }
        let Some(body) = body else {
            return Err(Flow::Panic(format!("`{}.{}` 无实现可调用", trait_name, m)));
        };
        let saved_module = self.module.clone();
        self.module = self.module.clone();
        let fenv = Env::child(&self.globals);
        fenv.define("self".into(), Local { value: vals.first().cloned().unwrap_or(Value::Void) });
        for (i, n) in param_names.iter().enumerate() {
            if let Some(v) = vals.get(i + 1) {
                fenv.define(n.clone(), Local { value: v.clone() });
            }
        }
        let _ = self_ty;
        let r = self.check_block(&body, &fenv);
        self.module = saved_module;
        r
    }

    // ---------- fn 值 / 用户函数 ----------

    fn call_fn_value(&mut self, f: &Value, args: &[Value], env: &Rc<Env>) -> EvalResult {
        match f {
            Value::Closure { params, body, env: fenv } => {
                let cenv = Env::child(fenv);
                for (i, p) in params.iter().enumerate() {
                    let v = args.get(i).cloned().unwrap_or(Value::Void);
                    cenv.define(p.name.clone(), Local { value: v });
                }
                match &**body {
                    Expr::BlockExpr(b) => self.check_block(b, &cenv),
                    other => self.expr(other, &cenv),
                }
            }
            Value::FnRef { id } => {
                let vals: Vec<Value> = args.to_vec();
                self.call_user_fn(*id, &vals, env)
            }
            _ => Err(Flow::Panic("调用非函数值".into())),
        }
    }

    fn call_user_fn(&mut self, id: usize, vals: &[Value], _env: &Rc<Env>) -> EvalResult {
        let (module, ret, body, param_names) = {
            let Some(f) = self.sema.fns.get(id) else {
                return Err(Flow::Panic("fn 未定义".into()));
            };
            (f.module.clone(), f.ret.clone(), f.body.clone(),
             f.params.iter().map(|(n, _)| n.clone()).collect::<Vec<_>>())
        };
        let Some(body) = body else {
            return Err(Flow::Panic("外部声明不可调用".into()));
        };
        // 返回类型 E 名(AnyError 擦除判定)
        if let sem::Ty::Named { def, .. } = &ret {
            *self.cur_ret_e_name.borrow_mut() = Some(self.def_name(*def));
        } else {
            *self.cur_ret_e_name.borrow_mut() = None;
        }
        let saved_module = self.module.clone();
        self.module = module;
        let fenv = Env::child(&self.globals);
        for (i, n) in param_names.iter().enumerate() {
            let v = vals.get(i).cloned().unwrap_or(Value::Void);
            let v = self.copy_if_value(&v);
            fenv.define(n.clone(), Local { value: v });
        }
        let r = self.check_block(&body, &fenv);
        self.module = saved_module;
        match r {
            Ok(v) => Ok(v),
            Err(Flow::EarlyReturn(v)) => Ok(v),
            Err(Flow::Panic(m)) => Err(Flow::Panic(m)),
            Err(Flow::Blocked) => Err(Flow::Blocked),
            Err(Flow::None) => Ok(Value::Void),
            Err(Flow::Break | Flow::Continue) => Err(Flow::Panic("控制流越界: break/continue 穿越调用边界".into())),
        }
    }

    // ---------- 任务 ----------

    fn run_task(&mut self, tid: u32) {
        let Some(task) = self.tasks.borrow().get(&tid).cloned() else { return };
        if task.borrow().status != TaskStatus::Pending { return; }
        let (body, env) = (task.borrow().closure.clone(), task.borrow().env.clone());
        let r = match &*body {
            Expr::Closure { body: inner, .. } => match &**inner {
                Expr::BlockExpr(b) => self.check_block(b, &env),
                other => self.expr(other, &env),
            },
            other => self.expr(other, &env),
        };
        let Some(t) = self.tasks.borrow().get(&tid).cloned() else { return };
        let mut tb = t.borrow_mut();
        match r {
            Ok(v) => { tb.status = TaskStatus::Completed; tb.result = Some(v); }
            Err(Flow::Panic(m)) => {
                tb.status = TaskStatus::Panicked;
                tb.panic_msg = Some(m);
                self.cancelled_scopes.borrow_mut().insert(u32::MAX - tid);
            }
            Err(Flow::Blocked) => { tb.status = TaskStatus::Blocked; }
            Err(_) => { tb.status = TaskStatus::Completed; tb.result = Some(Value::Void); }
        }
    }

    fn resume_task(&mut self, tid: u32) {
        let status = self.tasks.borrow().get(&tid).map(|t| t.borrow().status).unwrap_or(TaskStatus::Completed);
        if status == TaskStatus::Blocked {
            // 取消态重跑:阻塞的 send 解析 Err(§7.2 取消传播)
            let task = self.tasks.borrow().get(&tid).cloned().unwrap();
            let (body, env) = (task.borrow().closure.clone(), task.borrow().env.clone());
            let r = match &*body {
                Expr::Closure { body: inner, .. } => match &**inner {
                    Expr::BlockExpr(b) => self.check_block(b, &env),
                    other => self.expr(other, &env),
                },
                other => self.expr(other, &env),
            };
            let Some(t) = self.tasks.borrow().get(&tid).cloned() else { return };
            let mut tb = t.borrow_mut();
            match r {
                Ok(v) => { tb.status = TaskStatus::Completed; tb.result = Some(v); }
                Err(Flow::Panic(m)) => { tb.status = TaskStatus::Panicked; tb.panic_msg = Some(m); }
                Err(Flow::EarlyReturn(v)) => { tb.status = TaskStatus::Completed; tb.result = Some(v); }
                Err(Flow::Blocked) => { tb.status = TaskStatus::Blocked; }
                Err(Flow::None) => { tb.status = TaskStatus::Completed; tb.result = Some(Value::Void); }
                // break/continue 不得穿越任务边界(检查面 E2072 把关;防御性按 panic 收敛)
                Err(Flow::Break | Flow::Continue) => {
                    tb.status = TaskStatus::Panicked;
                    tb.panic_msg = Some("控制流越界: break/continue 穿越任务边界".into());
                }
            }
        } else if status == TaskStatus::Pending {
            self.run_task(tid);
        }
    }

    fn task_status(&self, tid: u32) -> TaskStatus {
        self.tasks.borrow().get(&tid).map(|t| t.borrow().status).unwrap_or(TaskStatus::Completed)
    }
    fn task_result(&self, tid: u32) -> Option<Value> {
        self.tasks.borrow().get(&tid).and_then(|t| t.borrow().result.clone())
    }
    fn task_panic(&self, tid: u32) -> Option<String> {
        self.tasks.borrow().get(&tid).and_then(|t| t.borrow().panic_msg.clone())
    }

    fn make_result(&mut self, v: Value, variant: usize) -> Value {
        let d = self.sema.def_by_name.get("Result").copied().unwrap_or(0);
        Value::Enum { def: d, variant, payload: vec![v] }
    }

    // ---------- 成员 ----------

    fn member(&mut self, o: &Value, name: &str) -> EvalResult {
        // Box 自动解引用(§3.3)
        if let Value::Boxed(inner) = o {
            return self.member(inner, name);
        }
        match o {
            Value::Str(s) => match name {
                "len" => Ok(Value::Int(s.len() as i64)),
                "char_len" => Ok(Value::Int(s.chars().count() as i64)),
                "to_string" => Ok(o.clone()),
                _ => Err(Flow::Panic(format!("Str 无属性 `{}`", name))),
            },
            // 标量 to_string(D1;fmt_val 同格式)
            Value::Int(_) | Value::IntW(..) | Value::UInt(_) | Value::UIntW(..)
            | Value::Bool(_) | Value::F64(_) | Value::F32(_) => match name {
                "to_string" => {
                    let mut out = String::new();
                    fmt_value(o, &mut out);
                    Ok(Value::Str(Rc::new(out)))
                }
                _ => Err(Flow::Panic(format!("无属性 `{}`", name))),
            },
            Value::Array(arr) => match name {
                "len" => Ok(Value::Int(arr.borrow().len() as i64)),
                _ => Err(Flow::Panic(format!("无属性 `{}`", name))),
            },
            Value::Tuple(items) => {
                // .0 .1 等
                if let Ok(idx) = name.parse::<usize>() {
                    match items.get(idx) {
                        Some(v) => Ok(v.clone()),
                        None => Err(Flow::Panic(format!("元组索引 {} 越界", idx))),
                    }
                } else {
                    Err(Flow::Panic(format!("元组无成员 `{}`", name)))
                }
            }
            Value::Simd(items) => match name {
                "len" => Ok(Value::Int(items.len() as i64)),
                _ => Err(Flow::Panic(format!("Simd 无属性 `{}`", name))),
            },
            Value::Class { def, fields } | Value::Struct { def, fields } => {
                let dname = self.def_name(*def);
                if let Some((_, v)) = fields.borrow().iter().find(|(n, _)| n == name) {
                    return Ok(v.clone());
                }
                // derive(Error) 枚举外:trait props(Error.message 等)经 impl 查
                for im in &self.sema.impls {
                    if im.for_type != dname { continue; }
                    if let Some(tdef) = self.type_def_id(&im.trait_name) {
                        if matches!(self.sema.defs[tdef].kind, DefKind::Trait) {
                            if let Some((_, pty)) = self.sema.defs[tdef].props.iter().find(|(n, _)| n == name) {
                                let _ = pty;
                                return self.call_trait_prop(tdef, name, &dname, o);
                            }
                        }
                    }
                }
                Err(Flow::Panic(format!("`{}` 无属性 `{}`", dname, name)))
            }
            Value::Enum { def, variant, payload } => {
                let dname = self.def_name(*def);
                // @derive(Error) → message/trace 委托显示;cause 空链
                if self.sema.impls.iter().any(|im| im.for_type == dname && im.trait_name == "Error") {
                    return match name {
                        "message" => Ok(Value::Str(Rc::new(to_display(&Value::Enum { def: *def, variant: *variant, payload: payload.clone() })))),
                        "cause" | "trace" => Ok(Value::Str(Rc::new(String::new()))),
                        _ => Err(Flow::Panic(format!("`{}` 无属性 `{}`", dname, name))),
                    };
                }
                Err(Flow::Panic(format!("`{}` 无属性 `{}`", dname, name)))
            }
            Value::AnyError { message, cause, trace } => match name {
                "message" => Ok(Value::Str(message.clone())),
                "cause" => {
                    let d = self.sema.def_by_name.get("Option").copied().unwrap_or(0);
                    match cause {
                        Some(c) => Ok(Value::Enum { def: d, variant: 0, payload: vec![(**c).clone()] }),
                        None => Ok(Value::Enum { def: d, variant: 1, payload: vec![] }),
                    }
                }
                "trace" => Ok(Value::Str(trace.clone())),
                _ => Err(Flow::Panic(format!("AnyError 无属性 `{}`", name))),
            },
            _ => Err(Flow::Panic(format!("{} 无成员 `{}`", to_display(o), name))),
        }
    }

    fn call_trait_prop(&mut self, tdef: DefId, name: &str, for_type: &str, self_val: &Value) -> EvalResult {
        // prop 体优先在 impl 声明,其次 trait 声明默认体
        let tname = self.def_name(tdef);
        let mut prop_body: Option<ast::Block> = None;
        for d in &self.file.decls {
            if let ast::Decl::Impl(im) = d {
                if named_tail(&im.for_ty) != for_type || named_tail(&im.trait_ty) != tname { continue; }
                for item in &im.items {
                    if let ast::ImplItem::Prop(p) = item {
                        if p.name == *name { prop_body = p.body.clone(); break; }
                    }
                }
            }
        }
        if prop_body.is_none() {
            prop_body = self.file.decls.iter().find_map(|d| {
                if let ast::Decl::Trait(t) = d {
                    if t.name == tname {
                        for item in &t.items {
                            if let crate::ast::TraitItem::PropImpl(p) = item {
                                if p.name == *name { return p.body.clone(); }
                            }
                        }
                    }
                }
                None
            });
        }
        if let Some(body) = prop_body {
            let saved = self.module.clone();
            let fenv = Env::child(&self.globals);
            fenv.define("self".into(), Local { value: self_val.clone() });
            let r = self.check_block(&body, &fenv);
            self.module = saved;
            return r;
        }
        Err(Flow::Panic(format!("trait `{}` 无属性 `{}` 的体", tname, name)))
    }

    fn set_member(&mut self, o: &Value, name: &str, v: Value) {
        if let Value::Class { fields, .. } | Value::Struct { fields, .. } = o {
            let mut b = fields.borrow_mut();
            if let Some(slot) = b.iter_mut().find(|(n, _)| n == name) { slot.1 = v; }
        }
    }

    /// @derive(Show) 综合格式化(格式不钉死,只要求信息完整)
    fn show_value(&mut self, v: &Value) -> String {
        match v {
            Value::Struct { def, fields } | Value::Class { def, fields } => {
                let name = self.def_name(*def);
                let fs: Vec<String> = fields.borrow().iter()
                    .map(|(n, fv)| format!("{}: {}", n, self.show_value(fv))).collect();
                format!("{}{{{}}}", name, fs.join(", "))
            }
            Value::Enum { def, variant, payload } => {
                let dname = self.def_name(*def);
                let vname = self.variant_name(*def, *variant);
                if payload.is_empty() { format!("{}::{}", dname, vname) }
                else {
                    let ps: Vec<String> = payload.iter().map(|p| self.show_value(p)).collect();
                    format!("{}::{}({})", dname, vname, ps.join(", "))
                }
            }
            Value::Boxed(inner) => self.show_value(inner),
            _ => to_display(v),
        }
    }

    // ---------- 值辅助 ----------

    fn copy_if_value(&mut self, v: &Value) -> Value {
        match v {
            Value::Struct { def, fields } => Value::Struct {
                def: *def,
                fields: Rc::new(RefCell::new(fields.borrow().clone())),
            },
            other => other.clone(),
        }
    }

    fn def_has_drop(&self, v: &Value) -> Option<DefId> {
        match v {
            Value::Struct { def, .. } | Value::Class { def, .. } => {
                let name = self.def_name(*def);
                if self.sema.impls.iter().any(|im| im.trait_name == "Drop" && im.for_type == name) {
                    Some(*def)
                } else { None }
            }
            _ => None,
        }
    }

    fn run_drop(&mut self, v: &Value, def: DefId) {
        let dname = self.def_name(def);
        // Drop 方法体在本文件 impl 中
        let drop_body = self.file.decls.iter().find_map(|d| {
            if let ast::Decl::Impl(im) = d {
                let ft = named_tail(&im.for_ty);
                let tn = named_tail(&im.trait_ty);
                if ft == dname && tn == "Drop" {
                    for item in &im.items {
                        if let ast::ImplItem::Method(mm) = item {
                            if mm.name == "drop" { return mm.body.clone(); }
                        }
                    }
                }
            }
            None
        });
        if let Some(body) = drop_body {
            let fenv = Env::child(&self.globals);
            fenv.define("self".into(), Local { value: v.clone() });
            let _ = self.check_block(&body, &fenv);
        }
    }

    fn eval_interp(&mut self, src: &str, env: &Rc<Env>) -> EvalResult {
        let cached = self.interp_cache.borrow().get(src).cloned();
        if let Some(e) = cached {
            return self.expr(&e, env);
        }
        let (tokens, _) = crate::lex(src);
        let mut p = crate::parser::Parser::new(tokens);
        let e = p.parse_expr_public();
        self.interp_cache.borrow_mut().insert(src.to_string(), e.clone());
        self.expr(&e, env)
    }

    fn try_match(&mut self, p: &ast::Pattern, v: &Value, env: &Rc<Env>, out: &mut HashMap<String, Local>) -> bool {
        let mut m = MatchCtx { matched: true };
        self.match_pattern_into(p, v, &mut m, out, env);
        m.matched
    }

    fn match_pattern_into(&mut self, p: &ast::Pattern, v: &Value, m: &mut MatchCtx, out: &mut HashMap<String, Local>, env: &Rc<Env>) {
        if !m.matched { return; }
        let v = self.copy_if_value(v);
        match p {
            ast::Pattern::Ident(n) => { out.insert(n.clone(), Local { value: v }); }
            ast::Pattern::Wildcard => {}
            ast::Pattern::Lit(l) => {
                let ok = match (l, &v) {
                    (ast::PatLit::Int(t), Value::Int(i)) => text_int_eq(t, *i),
                    (ast::PatLit::Float(t), Value::F64(f)) => t.parse::<f64>().map(|x| x == *f).unwrap_or(false),
                    (ast::PatLit::Str(s), Value::Str(vs)) => s == &**vs,
                    (ast::PatLit::Bool(b), Value::Bool(vb)) => b == vb,
                    _ => false,
                };
                if !ok { m.matched = false; }
            }
            ast::Pattern::Tuple(ps) => match &v {
                Value::Tuple(items) if items.len() == ps.len() => {
                    for (sp, item) in ps.iter().zip(items) {
                        self.match_pattern_into(sp, item, m, out, env);
                    }
                }
                _ => m.matched = false,
            },
            ast::Pattern::Agg { path, sub } => {
                let name = path.last().cloned().unwrap_or_default();
                match &v {
                    Value::Enum { def, variant, payload } => {
                        let vname = self.variant_name(*def, *variant);
                        if vname != name {
                            m.matched = false;
                            return;
                        }
                        match sub {
                            ast::AggSub::Tuple(ps) => {
                                for (i, sp) in ps.iter().enumerate() {
                                    match payload.get(i) {
                                        Some(pv) => self.match_pattern_into(sp, pv, m, out, env),
                                        None => m.matched = false,
                                    }
                                }
                            }
                            ast::AggSub::Struct(fs) => {
                                let field_names = self.sema.defs.get(*def)
                                    .and_then(|d| d.variants.get(*variant))
                                    .map(|_| ());
                                let _ = field_names;
                                // v0.5 变体 struct 字段无名:载荷按序
                                for (i, f) in fs.iter().enumerate() {
                                    match payload.get(i) {
                                        Some(pv) => match &f.pattern {
                                            Some(bp) => self.match_pattern_into(bp, pv, m, out, env),
                                            None => { out.insert(f.name.clone(), Local { value: pv.clone() }); }
                                        },
                                        None => m.matched = false,
                                    }
                                }
                            }
                            ast::AggSub::Unit => {}
                        }
                    }
                    Value::Struct { def, fields } => {
                        let dname = self.def_name(*def);
                        if dname != name { m.matched = false; return; }
                        if let ast::AggSub::Struct(fs) = sub {
                            for f in fs {
                                let fv = fields.borrow().iter().find(|(n, _)| n == &f.name).map(|(_, v)| v.clone());
                                match (&f.pattern, fv) {
                                    (Some(bp), Some(fv)) => self.match_pattern_into(bp, &fv, m, out, env),
                                    (None, Some(fv)) => { out.insert(f.name.clone(), Local { value: fv }); }
                                    _ => m.matched = false,
                                }
                            }
                        } else { m.matched = false; }
                    }
                    _ => m.matched = false,
                }
            }
        }
    }

    // ---------- 小工具 ----------

    fn variant_name(&self, def: DefId, idx: usize) -> String {
        self.sema.defs.get(def)
            .and_then(|d| d.variants.get(idx)).map(|(n, _)| n.clone()).unwrap_or_default()
    }

    fn def_name(&self, def: DefId) -> String {
        self.sema.defs.get(def).map(|d| d.name.clone()).unwrap_or_default()
    }

    fn type_def_id(&self, name: &str) -> Option<DefId> {
        self.sema.def_by_name.get(name).copied()
    }

}

fn named_tail(t: &ast::Type) -> String {
    match t {
        ast::Type::Named { path, .. } => path.last().cloned().unwrap_or_default(),
        _ => String::new(),
    }
}

fn member_name(t: &ast::MemberTarget) -> String {
    match t { ast::MemberTarget::Name(n) => n.clone(), ast::MemberTarget::TupleIndex(i) => i.to_string() }
}

fn text_int_eq(t: &str, i: i64) -> bool {
    parse_int(&t.replace('_', "")).map(|x| x == i).unwrap_or(false)
}

fn values_equal(a: &Value, b: &Value) -> bool {
    // 跨宽度数值比较
    if compare_values(a, b) == Some(0) {
        if matches!((a, b), (Value::Int(_) | Value::IntW(..) | Value::UInt(_) | Value::UIntW(..) | Value::F64(_) | Value::F32(_), Value::Int(_) | Value::IntW(..) | Value::UInt(_) | Value::UIntW(..) | Value::F64(_) | Value::F32(_))) {
            return true;
        }
    }
    match (a, b) {
        (Value::Bool(x), Value::Bool(y)) => x == y,
        (Value::Str(x), Value::Str(y)) => x == y,
        (Value::Enum { variant: v1, payload: p1, .. }, Value::Enum { variant: v2, payload: p2, .. }) => {
            v1 == v2 && p1.len() == p2.len() && p1.iter().zip(p2).all(|(x, y)| values_equal(x, y))
        }
        (Value::Tuple(x), Value::Tuple(y)) => x.len() == y.len() && x.iter().zip(y).all(|(u, v)| values_equal(u, v)),
        _ => false,
    }
}

fn runtime_index(o: &Value, i: &Value) -> Result<Value, Flow> {
    let idx_of = |i: &Value| -> Option<usize> { int_i64(i).map(|n| n as usize) };
    match (o, i) {
        (Value::Array(arr), _) => {
            let Some(idx) = idx_of(i) else { return Ok(Value::Void) };
            let b = arr.borrow();
            if idx >= b.len() { return Err(Flow::Panic("index out of bounds".into())); }
            Ok(b[idx].clone())
        }
        (Value::Simd(items), _) => {
            let Some(idx) = idx_of(i) else { return Ok(Value::Void) };
            Ok(Value::F32(items.get(idx).copied().unwrap_or(0.0) as f32))
        }
        // 其余目标(Str/Int/…)不可索引:panic 逐字对齐 C rt(T2 后一致性收口;
        // 原静默返回 Void 曾把 ccmod 双种子分歧藏进 cc.ct sem 域)
        _ => return Err(Flow::Panic("索引目标非数组".into())),
    }
}

fn runtime_index_usize(_o: &Value, i: &Value) -> usize {
    match i {
        Value::Int(n) | Value::IntW(_, n) => *n as usize,
        Value::UInt(n) | Value::UIntW(_, n) => *n as usize,
        _ => 0,
    }
}

fn parse_int(cleaned: &str) -> Option<i64> {
    let (radix, digits) = if let Some(r) = cleaned.strip_prefix("0x") { (16, r) }
        else if let Some(r) = cleaned.strip_prefix("0o") { (8, r) }
        else if let Some(r) = cleaned.strip_prefix("0b") { (2, r) }
        else { (10, cleaned) };
    let digits = digits.replace('_', "");
    let signed = digits.starts_with('-');
    let digits = digits.trim_start_matches('-').to_string();
    match u64::from_str_radix(&digits, radix) {
        Ok(u) => if signed { Some(-(u as i64)) } else { Some(u as i64) },
        Err(_) => None,
    }
}

pub fn truthy(v: &Value) -> bool {
    match v { Value::Bool(b) => *b, _ => true }
}

pub fn to_display(v: &Value) -> String {
    match v {
        Value::Void => "void".into(),
        Value::Int(i) | Value::IntW(_, i) => i.to_string(),
        Value::UInt(u) | Value::UIntW(_, u) => u.to_string(),
        Value::F64(f) => fmt_float(*f),
        Value::F32(f) => fmt_float(*f as f64),
        Value::Bool(b) => b.to_string(),
        Value::Str(s) => s.to_string(),
        Value::Enum { variant, payload, .. } => {
            let vn = variant.to_string();
            if payload.is_empty() { vn }
            else { format!("{}({})", vn, payload.iter().map(|p| to_display(p)).collect::<Vec<_>>().join(", ")) }
        }
        Value::Tuple(items) => format!("({})", items.iter().map(|p| to_display(p)).collect::<Vec<_>>().join(", ")),
        _ => "<value>".into(),
    }
}

thread_local! {
    static SEMA_NAME: RefCell<HashMap<DefId, String>> = RefCell::new(HashMap::new());
}

pub fn register_sema_names(sema: &Sema) {
    SEMA_NAME.with(|c| {
        let mut b = c.borrow_mut();
        b.clear();
        for (i, d) in sema.defs.iter().enumerate() {
            b.insert(i, d.name.clone());
        }
    });
}

fn fmt_float(f: f64) -> String {
    if f == f.trunc() && f.abs() < 1e15 { format!("{:.1}", f) } else { format!("{}", f) }
}

fn apply_assign_op(op: &ast::AssignOp, cur: &Value, v: &Value) -> Result<Value, Flow> {
    use ast::AssignOp::*;
    if *op == Eq { return Ok(v.clone()); }
    // 宽度标记值:复合赋值与二元算术同语义(§3.6)
    if let Some(bin) = match op {
        AddEq => Some(ast::BinOp::Add), SubEq => Some(ast::BinOp::Sub),
        MulEq => Some(ast::BinOp::Mul), DivEq => Some(ast::BinOp::Div),
        ModEq => Some(ast::BinOp::Mod), _ => None,
    } {
        if let Some(r) = tagged_bin(cur, v, &bin) { return r; }
    }
    match (op, cur, v) {
        (AddEq, Value::Int(x), Value::Int(y)) => Ok(Value::Int(x + y)),
        (SubEq, Value::Int(x), Value::Int(y)) => Ok(Value::Int(x - y)),
        (MulEq, Value::Int(x), Value::Int(y)) => Ok(Value::Int(x * y)),
        (DivEq, Value::Int(x), Value::Int(y)) => {
            if *y == 0 { Err(Flow::Panic("division by zero".into())) } else { Ok(Value::Int(x / y)) }
        }
        (ModEq, Value::Int(x), Value::Int(y)) => {
            if *y == 0 { Err(Flow::Panic("division by zero".into())) } else { Ok(Value::Int(x % y)) }
        }
        (AddEq, Value::UInt(x), Value::UInt(y)) => Ok(Value::UInt(x + y)),
        (SubEq, Value::UInt(x), Value::UInt(y)) => Ok(Value::UInt(x - y)),
        (MulEq, Value::UInt(x), Value::UInt(y)) => Ok(Value::UInt(x * y)),
        (AddEq, Value::F64(x), Value::F64(y)) => Ok(Value::F64(x + y)),
        (SubEq, Value::F64(x), Value::F64(y)) => Ok(Value::F64(x - y)),
        (MulEq, Value::F64(x), Value::F64(y)) => Ok(Value::F64(x * y)),
        _ => Err(Flow::Panic("算术类型错误".into())),
    }
}

impl<'a> Interp<'a> {
    /// 以 AST 实参调用函数值/闭包
    fn call_fn_value_ast(&mut self, f: &Value, args: &[ast::Expr], env: &Rc<Env>) -> EvalResult {
        let mut vals = Vec::new();
        for a in args { vals.push(self.expr(a, env)?); }
        self.call_fn_value(f, &vals, env)
    }
}

impl<'a> Interp<'a> {
    fn eval_ident(&mut self, name: &str, env: &Rc<Env>) -> EvalResult {
        if let Some(l) = env.get(name) { return Ok(l.value); }
        // 常量 / 静态
        if let Some(v) = self.consts.borrow().get(name) { return Ok(v.clone()); }
        if let Some(v) = self.statics.borrow().get(name) { return Ok(v.clone()); }
        // prelude 值
        if name == "parallel" { return Ok(Value::Parallel); }
        // prelude 枚举变体值(Some/None/Ok/Err)
        match name {
            "Some" => {
                let d = self.sema.def_by_name.get("Option").copied().unwrap_or(0);
                return Ok(Value::Enum { def: d, variant: 0, payload: vec![Value::Void] }); // 需要 hint 提供类型
            }
            "None" => {
                let d = self.sema.def_by_name.get("Option").copied().unwrap_or(0);
                return Ok(Value::Enum { def: d, variant: 1, payload: vec![] });
            }
            "Ok" => {
                let d = self.sema.def_by_name.get("Result").copied().unwrap_or(0);
                return Ok(Value::Enum { def: d, variant: 0, payload: vec![Value::Void] }); // 需要 hint 提供类型
            }
            "Err" => {
                let d = self.sema.def_by_name.get("Result").copied().unwrap_or(0);
                return Ok(Value::Enum { def: d, variant: 1, payload: vec![Value::Void] }); // 需要 hint 提供类型
            }
            _ => {}
        }
        // 用户枚举的单元变体值(Bad, DivByZero, Stop, Red 等)
        if let Some(Symbol::Variant { def, idx }) = self.module_symbol(name) {
            return Ok(Value::Enum { def, variant: idx, payload: vec![] });
        }
        // 用户 fn
        if let Some(Symbol::Fn(id)) = self.module_symbol(name) { return Ok(Value::FnRef { id }); }
        // 类型名作为关联调用接收者(Arena.fixed / Simd.splat 等)
        if let Some(&_def) = self.sema.def_by_name.get(name) {
            return Ok(Value::Arena);
        }
        Err(Flow::Panic(format!("未解析的名称 `{}`", name)))
    }

    fn eval_binop(&mut self, op: &ast::BinOp, a: &Value, rhs: &ast::Expr, env: &Rc<Env>) -> EvalResult {
        use ast::BinOp::*;
        // Option/Result 取默认中缀(对齐 C rt:Some/Ok 且恰 1 载荷 → 取载荷;其余一律取默认)
        if *op == Or {
            let unwrapped = match a {
                Value::Enum { variant: 0, payload, .. } if payload.len() == 1 => Some(payload[0].clone()),
                _ => None,
            };
            if let Some(v) = unwrapped { return Ok(v); }
            return self.expr(rhs, env);
        }
        // && 短路:LHS 假时不求值 RHS(对齐 C rt;守卫型 RHS 可含越界等副作用,T1 关联)
        if *op == AndAnd {
            if !truthy(a) { return Ok(Value::Bool(false)); }
            let b = self.expr(rhs, env)?;
            return Ok(Value::Bool(truthy(&b)));
        }
        // || 短路:LHS 真时不求值 RHS(v0.7 修订一)
        if *op == OrOr {
            if truthy(a) { return Ok(Value::Bool(true)); }
            let b = self.expr(rhs, env)?;
            return Ok(Value::Bool(truthy(&b)));
        }
        let b = self.expr(rhs, env)?;
        let simd_pair = matches!(a, Value::Simd(_)) && matches!(b, Value::Simd(_));
        if simd_pair {
            let (va, vb) = if let (Value::Simd(va), Value::Simd(vb)) = (a, b) {
                (va.clone(), vb.clone())
            } else { (vec![], vec![]) };
            let out: Vec<f64> = va.iter().zip(vb.iter()).map(|(x, y)| match op {
                Add => x + y, Sub => x - y, Mul => x * y,
                Div => if *y == 0.0 { f64::NAN } else { x / y },
                _ => 0.0,
            }).collect();
            return Ok(Value::Simd(out));
        }
        match op {
            AndAnd => Ok(Value::Bool(truthy(a) && truthy(&b))),
            OrOr => Ok(Value::Bool(truthy(a) || truthy(&b))),
            Eq | Ne | Lt | Gt | Le | Ge => {
                let ord = compare_values(a, &b);
                let r = match op {
                    Eq => ord == Some(0),
                    Ne => ord != Some(0),
                    Lt => ord == Some(-1),
                    Gt => ord == Some(1),
                    Le => ord != Some(1),
                    Ge => ord != Some(-1),
                    _ => false,
                };
                if ord.is_none() {
                    self.err("E2010", format!("比较类型不匹配({:?}):{} vs {}", op, to_display(a), to_display(&b)), Span::new(1, 1, 0, 0));
                }
                Ok(Value::Bool(r))
            }
            Add | Sub | Mul | Div | Mod | WrapAdd | WrapSub => {
                let b_clone = b.clone();
                if let Some(r) = tagged_bin(a, &b_clone, op) { return r; }
                match (a, &b_clone) {
                    (Value::Int(x), Value::Int(y)) => Ok(Value::Int(match op {
                        Add => checked_add_i64(*x, *y)?,
                        Sub => checked_sub_i64(*x, *y)?,
                        Mul => checked_mul_i64(*x, *y)?,
                        Div => { if *y == 0 { return Err(Flow::Panic("division by zero".into())); } x / y }
                        Mod => { if *y == 0 { return Err(Flow::Panic("division by zero".into())); } x % y }
                        WrapAdd => x.wrapping_add(*y),
                        WrapSub => x.wrapping_sub(*y),
                        _ => 0,
                    })),
                    (Value::UInt(x), Value::UInt(y)) => Ok(Value::UInt(match op {
                        Add => { let r = x.checked_add(*y).ok_or_else(|| Flow::Panic("integer overflow".into()))?; r }
                        Sub => { let r = x.checked_sub(*y).ok_or_else(|| Flow::Panic("integer overflow".into()))?; r }
                        Mul => { let r = x.checked_mul(*y).ok_or_else(|| Flow::Panic("integer overflow".into()))?; r }
                        Div => { if *y == 0 { return Err(Flow::Panic("division by zero".into())); } x / y }
                        Mod => { if *y == 0 { return Err(Flow::Panic("division by zero".into())); } x % y }
                        WrapAdd => x.wrapping_add(*y),
                        WrapSub => x.wrapping_sub(*y),
                        _ => 0,
                    })),
                    (Value::F64(x), Value::F64(y)) => Ok(Value::F64(match op {
                        Add | WrapAdd => x + y, Sub | WrapSub => x - y, Mul => x * y,
                        Div => if *y == 0.0 { f64::NAN } else { x / y },
                        Mod => x % y, _ => 0.0,
                    })),
                    (Value::F32(x), Value::F32(y)) => Ok(Value::F32(match op {
                        Add | WrapAdd => x + y, Sub | WrapSub => x - y, Mul => x * y,
                        Div => if *y == 0.0 { f32::NAN } else { x / y },
                        Mod => x % y, _ => 0.0,
                    })),
                    // 混合 UInt/Int:i128 中介 + 左侧类型承载(对齐 C rt ck_int)
                    (Value::UInt(x), Value::Int(y)) => Ok(Value::UInt(match op {
                        Add => i128::checked_add(*x as i128, *y as i128).and_then(|v| u64::try_from(v).ok()).ok_or_else(|| Flow::Panic("integer overflow".into()))?,
                        Sub => i128::checked_sub(*x as i128, *y as i128).and_then(|v| u64::try_from(v).ok()).ok_or_else(|| Flow::Panic("integer overflow".into()))?,
                        Mul => i128::checked_mul(*x as i128, *y as i128).and_then(|v| u64::try_from(v).ok()).ok_or_else(|| Flow::Panic("integer overflow".into()))?,
                        Div => { if *y == 0 { return Err(Flow::Panic("division by zero".into())); } ((*x as i128) / (*y as i128)) as u64 }
                        Mod => { if *y == 0 { return Err(Flow::Panic("division by zero".into())); } ((*x as i128) % (*y as i128)) as u64 }
                        WrapAdd => x.wrapping_add_signed(*y),
                        WrapSub => x.wrapping_sub(*y as u64),
                        _ => 0,
                    })),
                    (Value::Int(x), Value::UInt(y)) => Ok(Value::Int(match op {
                        Add => i128::checked_add(*x as i128, *y as i128).and_then(|v| i64::try_from(v).ok()).ok_or_else(|| Flow::Panic("integer overflow".into()))?,
                        Sub => i128::checked_sub(*x as i128, *y as i128).and_then(|v| i64::try_from(v).ok()).ok_or_else(|| Flow::Panic("integer overflow".into()))?,
                        Mul => i128::checked_mul(*x as i128, *y as i128).and_then(|v| i64::try_from(v).ok()).ok_or_else(|| Flow::Panic("integer overflow".into()))?,
                        Div => { if *y == 0 { return Err(Flow::Panic("division by zero".into())); } ((*x as i128) / (*y as i128)) as i64 }
                        Mod => { if *y == 0 { return Err(Flow::Panic("division by zero".into())); } ((*x as i128) % (*y as i128)) as i64 }
                        WrapAdd => x.wrapping_add(*y as i64),
                        WrapSub => x.wrapping_sub(*y as i64),
                        _ => 0,
                    })),
                    (Value::Str(x), Value::Str(y)) if matches!(op, crate::ast::BinOp::Add) => {
                        // T2 规格修订:Add 双 Str 为拼接(对齐 C rt)
                        Ok(Value::Str(Rc::new(format!("{}{}", x, y))))
                    }
                    _ => {
                        let msg = format!("算术需要数值,实际 {} 与 {}", to_display(a), to_display(&b_clone));
                        return Err(Flow::Panic(msg));
                    }
                }
            }
            Or => Ok(a.clone()),
        }
    }
}

fn checked_add_i64(x: i64, y: i64) -> Result<i64, Flow> {
    x.checked_add(y).ok_or_else(|| Flow::Panic("integer overflow".into()))
}
fn checked_sub_i64(x: i64, y: i64) -> Result<i64, Flow> {
    x.checked_sub(y).ok_or_else(|| Flow::Panic("integer overflow".into()))
}
fn checked_mul_i64(x: i64, y: i64) -> Result<i64, Flow> {
    x.checked_mul(y).ok_or_else(|| Flow::Panic("integer overflow".into()))
}

// ---- 宽度整数视图(带标记与不带标记统一读取) ----

fn int_i64(v: &Value) -> Option<i64> {
    match v {
        Value::Int(x) | Value::IntW(_, x) => Some(*x),
        Value::UInt(x) | Value::UIntW(_, x) => Some(*x as i64),
        _ => None,
    }
}

fn int_u64(v: &Value) -> Option<u64> {
    match v {
        Value::Int(x) | Value::IntW(_, x) => Some(*x as u64),
        Value::UInt(x) | Value::UIntW(_, x) => Some(*x),
        _ => None,
    }
}

fn int_width_of(v: &Value) -> Option<(IntW, bool)> {
    match v {
        Value::IntW(w, _) => Some((*w, true)),
        Value::UIntW(w, _) => Some((*w, false)),
        _ => None,
    }
}

const OVERFLOW_MSG: &str = "integer overflow";

/// 带宽度标记的算术(§3.6):默认检查(溢出 panic),Wrap* 回绕到目标宽度
fn tagged_bin(a: &Value, b: &Value, op: &ast::BinOp) -> Option<Result<Value, Flow>> {
    use ast::BinOp::*;
    if !matches!(op, Add | Sub | Mul | Div | Mod | WrapAdd | WrapSub) { return None; }
    let (w, signed) = match (int_width_of(a), int_width_of(b)) {
        (Some(t), _) => t,
        (None, Some(t)) => t,
        (None, None) => return None,
    };
    let wrapping = matches!(op, WrapAdd | WrapSub);
    let div0 = || Flow::Panic("division by zero".into());
    if !signed {
        let (x, y) = (int_u64(a)?, int_u64(b)?);
        let r: u64 = match op {
            Add => match x.checked_add(y) { Some(r) => r, None => return Some(Err(Flow::Panic(OVERFLOW_MSG.into()))) },
            Sub => match x.checked_sub(y) { Some(r) => r, None => return Some(Err(Flow::Panic(OVERFLOW_MSG.into()))) },
            Mul => match x.checked_mul(y) { Some(r) => r, None => return Some(Err(Flow::Panic(OVERFLOW_MSG.into()))) },
            WrapAdd => x.wrapping_add(y),
            WrapSub => x.wrapping_sub(y),
            Div => { if y == 0 { return Some(Err(div0())); } x / y }
            Mod => { if y == 0 { return Some(Err(div0())); } x % y }
            _ => return None,
        };
        let r = if wrapping && !matches!(w, IntW::W64 | IntW::WSize) {
            match w { IntW::W8 => (r as u8) as u64, IntW::W16 => (r as u16) as u64, IntW::W32 => (r as u32) as u64, _ => r }
        } else if !wrapping {
            let max: u64 = match w { IntW::W8 => u8::MAX as u64, IntW::W16 => u16::MAX as u64, IntW::W32 => u32::MAX as u64, _ => u64::MAX };
            if r > max { return Some(Err(Flow::Panic(OVERFLOW_MSG.into()))); }
            r
        } else { r };
        return Some(Ok(Value::UIntW(w, r)));
    }
    let (x, y) = (int_i64(a)?, int_i64(b)?);
    let r: i64 = match op {
        Add => match x.checked_add(y) { Some(r) => r, None => return Some(Err(Flow::Panic(OVERFLOW_MSG.into()))) },
        Sub => match x.checked_sub(y) { Some(r) => r, None => return Some(Err(Flow::Panic(OVERFLOW_MSG.into()))) },
        Mul => match x.checked_mul(y) { Some(r) => r, None => return Some(Err(Flow::Panic(OVERFLOW_MSG.into()))) },
        WrapAdd => x.wrapping_add(y),
        WrapSub => x.wrapping_sub(y),
        Div => { if y == 0 { return Some(Err(div0())); } x / y }
        Mod => { if y == 0 { return Some(Err(div0())); } x % y }
        _ => return None,
    };
    let r = if wrapping && !matches!(w, IntW::W64 | IntW::WSize) {
        match w { IntW::W8 => (r as i8) as i64, IntW::W16 => (r as i16) as i64, IntW::W32 => (r as i32) as i64, _ => r }
    } else if !wrapping {
        let (min, max) = match w {
            IntW::W8 => (i8::MIN as i64, i8::MAX as i64),
            IntW::W16 => (i16::MIN as i64, i16::MAX as i64),
            IntW::W32 => (i32::MIN as i64, i32::MAX as i64),
            _ => (i64::MIN, i64::MAX),
        };
        if r < min || r > max { return Some(Err(Flow::Panic(OVERFLOW_MSG.into()))); }
        r
    } else { r };
    Some(Ok(Value::IntW(w, r)))
}

/// `as[T]()` 显式转换(§3.6 截断语义),结果携带目标宽度
fn convert_as(v: &Value, targ: &str) -> Value {
    let w_signed = match targ {
        "I8" => Some((IntW::W8, true)), "I16" => Some((IntW::W16, true)),
        "I32" => Some((IntW::W32, true)), "I64" | "ISize" => Some((IntW::W64, true)),
        "U8" => Some((IntW::W8, false)), "U16" => Some((IntW::W16, false)),
        "U32" => Some((IntW::W32, false)), "U64" | "USize" => Some((IntW::W64, false)),
        _ => None,
    };
    if let Some((w, signed)) = w_signed {
        if signed {
            let x: i64 = match v {
                Value::Int(i) | Value::IntW(_, i) => *i,
                Value::UInt(u) | Value::UIntW(_, u) => *u as i64,
                Value::F64(f) => *f as i64,
                Value::F32(f) => *f as i64,
                _ => return v.clone(),
            };
            return Value::IntW(w, match w {
                IntW::W8 => (x as i8) as i64, IntW::W16 => (x as i16) as i64,
                IntW::W32 => (x as i32) as i64, _ => x,
            });
        }
        let x: u64 = match v {
            Value::Int(i) | Value::IntW(_, i) => *i as u64,
            Value::UInt(u) | Value::UIntW(_, u) => *u,
            Value::F64(f) => *f as u64,
            Value::F32(f) => *f as u64,
            _ => return v.clone(),
        };
        return Value::UIntW(w, match w {
            IntW::W8 => (x as u8) as u64, IntW::W16 => (x as u16) as u64,
            IntW::W32 => (x as u32) as u64, _ => x,
        });
    }
    match (v, targ) {
        (_, "F64") => match v {
            Value::F32(f) => Value::F64(*f as f64),
            Value::F64(f) => Value::F64(*f),
            other => int_i64(other).map(|i| Value::F64(i as f64)).unwrap_or_else(|| v.clone()),
        },
        (_, "F32") => match v {
            Value::F64(f) => Value::F32(*f as f32),
            Value::F32(f) => Value::F32(*f),
            other => int_i64(other).map(|i| Value::F32(i as f32)).unwrap_or_else(|| v.clone()),
        },
        _ => v.clone(),
    }
}

fn compare_values(a: &Value, b: &Value) -> Option<i8> {
    let lt = |x: f64, y: f64| if x < y { -1 } else if x > y { 1 } else { 0 };
    // 整数族(含宽度标记)统一精确比较:符号 + 绝对值位型
    let int_cmp = |a: &Value, b: &Value| -> Option<i8> {
        let sgn = |v: &Value| -> Option<(bool, u64)> {
            match v {
                Value::Int(x) | Value::IntW(_, x) => Some((*x < 0, x.unsigned_abs())),
                Value::UInt(x) | Value::UIntW(_, x) => Some((false, *x)),
                _ => None,
            }
        };
        let (an, am) = sgn(a)?;
        let (bn, bm) = sgn(b)?;
        Some(match (an, bn) {
            (true, true) => match bm.cmp(&am) { std::cmp::Ordering::Less => -1, std::cmp::Ordering::Greater => 1, std::cmp::Ordering::Equal => 0 },
            (true, false) => -1,
            (false, true) => 1,
            (false, false) => match am.cmp(&bm) { std::cmp::Ordering::Less => -1, std::cmp::Ordering::Greater => 1, std::cmp::Ordering::Equal => 0 },
        })
    };
    if let Some(r) = int_cmp(a, b) { return Some(r); }
    match (a, b) {
        (Value::F64(x), Value::F64(y)) => Some(lt(*x, *y)),
        (Value::F32(x), Value::F32(y)) => Some(lt(*x as f64, *y as f64)),
        (Value::F64(x), Value::F32(y)) => Some(lt(*x, *y as f64)),
        (Value::F32(x), Value::F64(y)) => Some(lt(*x as f64, *y)),
        (Value::Str(x), Value::Str(y)) => Some(match x.as_str() { s if s < y.as_str() => -1, s if s > y.as_str() => 1, _ => 0 }),
        _ => None,
    }
}


/// 便捷入口:解析 + 构建 + 运行单文件的全部 test 块
pub fn run_test_file(src: &str, profile: crate::sem::Profile) -> Vec<(String, Result<(), String>)> {
    let files = vec![("".to_string(), src.to_string())];
    let (sema, _) = sem::build_package(&files, None, profile);
    let (tokens, _) = crate::lex(src);
    let ast_file = crate::parser::Parser::new(tokens).parse_file_public();
    let file = Rc::new(ast_file);
    let mut interp = Interp::new(&sema, String::new(), file.clone());
    interp.run_tests(&file)
}

/// 运行 fn main(D1):解析诊断非空 → Err(诊断文本);panic → Err(消息)。
pub fn run_main_file(src: &str, profile: crate::sem::Profile) -> Result<i32, String> {
    let (file, diags) = crate::parse_src(src);
    if !diags.is_empty() {
        let mut msg = String::new();
        for d in &diags {
            msg.push_str(&format!("{}:{} {} {}\n", d.span.line, d.span.col, d.code, d.message));
        }
        return Err(msg);
    }
    let has_main = file.decls.iter().any(|d| matches!(d, ast::Decl::Fn(f) if f.name == "main"));
    if !has_main { return Err("缺少 fn main".into()); }
    let (sema, _) = sem::build_package(&vec![("".to_string(), src.to_string())], None, profile);
    let ast_file = Rc::new(file);
    let mut interp = Interp::new(&sema, String::new(), ast_file.clone());
    interp.eval_globals(&ast_file);
    interp.run_main()
}

// fmt_val 同族:浮点整值 %.1f 否则 %g(对齐 C rt fmt_val/C10 print 域)
fn fmt_value(v: &Value, out: &mut String) {
    match v {
        Value::Void => {}
        Value::Int(i) => out.push_str(&i.to_string()),
        Value::IntW(_, i) => out.push_str(&i.to_string()),
        Value::UInt(u) => out.push_str(&u.to_string()),
        Value::UIntW(_, u) => out.push_str(&u.to_string()),
        Value::F64(f) => {
            if *f == (*f as i64) as f64 { out.push_str(&format!("{:.1}", f)); }
            else { out.push_str(&format!("{}", f)); }
        }
        Value::F32(f) => {
            if *f == (*f as i64) as f32 { out.push_str(&format!("{:.1}", f)); }
            else { out.push_str(&format!("{}", f)); }
        }
        Value::Bool(b) => out.push_str(if *b { "true" } else { "false" }),
        Value::Str(s) => out.push_str(s),
        _ => out.push_str("<value>"),
    }
}

fn exit_code_of(v: Value) -> i32 {
    match v {
        Value::Int(i) => i as i32,
        Value::IntW(_, i) => i as i32,
        Value::UInt(u) => u as i32,
        Value::UIntW(_, u) => u as i32,
        _ => 0,
    }
}



impl<'a> Interp<'a> {
    /// 字面量类型强制:根据注解类型调整数值运行时表示
    fn coerce_literal(&self, v: &Value, target: &Ty) -> Value {
        // Named 原生标量名 → 原生 Ty
        let target = match target {
            Ty::Named { def, args } if args.is_empty() => {
                let n = self.sema.defs.get(*def).map(|d| d.name.as_str()).unwrap_or("");
                match n {
                    "I8" => Ty::Int(IntW::W8), "I16" => Ty::Int(IntW::W16),
                    "I32" => Ty::Int(IntW::W32), "I64" => Ty::Int(IntW::W64),
                    "ISize" => Ty::Int(IntW::WSize),
                    "U8" => Ty::UInt(IntW::W8), "U16" => Ty::UInt(IntW::W16),
                    "U32" => Ty::UInt(IntW::W32), "U64" => Ty::UInt(IntW::W64),
                    "USize" => Ty::UInt(IntW::WSize),
                    "F32" => Ty::F32, "F64" => Ty::F64,
                    "Str" => Ty::Str, "Bool" => Ty::Bool,
                    _ => target.clone(),
                }
            }
            _ => target.clone(),
        };
        match (v, target) {
            // 注解/后缀 → 宽度标记值(§3.6:溢出即 panic 的宽度依据)
            (Value::Int(i), Ty::Int(w)) => Value::IntW(w, *i),
            (Value::UInt(u), Ty::Int(w)) => Value::IntW(w, *u as i64),
            (Value::IntW(_, i), Ty::Int(w)) => Value::IntW(w, *i),
            (Value::UIntW(_, u), Ty::Int(w)) => Value::IntW(w, *u as i64),
            (Value::Int(i), Ty::UInt(w)) => Value::UIntW(w, *i as u64),
            (Value::UInt(u), Ty::UInt(w)) => Value::UIntW(w, *u),
            (Value::IntW(_, i), Ty::UInt(w)) => Value::UIntW(w, *i as u64),
            (Value::UIntW(_, u), Ty::UInt(w)) => Value::UIntW(w, *u),
            (Value::Int(i), Ty::F64) => Value::F64(*i as f64),
            (Value::Int(i), Ty::F32) => Value::F32(*i as f32),
            (Value::F64(f), Ty::F32) => Value::F32(*f as f32),
            (Value::F32(f), Ty::F64) => Value::F64(*f as f64),
            (Value::F64(f), Ty::Int(_)) => Value::Int(*f as i64),
            _ => v.clone(),
        }
    }
}
