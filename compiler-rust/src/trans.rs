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
    Str,
    Range,
    /// 数组/切片句柄(ct_arr*):赋值共享后备,对齐 interp Rc 语义
    Array,
    /// 用户 struct(值语义)——索引指向 Trans::types
    Struct(u32),
    /// 用户 class(引用语义)——ctn_<Name>* 指针
    Class(u32),
    /// Box[T](v)——内层 struct 的堆指针
    Boxed(u32),
    /// 和类型(tagged union):预定义 Option/Result 与用户 enum;载荷槽类别
    Sum(u32, Pay),
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
        "Str" | "String" => VTy::Str,
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

#[derive(Clone, Copy, PartialEq)]
#[derive(Debug)]
enum Pay { I, F }

#[derive(Clone)]
struct EnumInfo {
    _name: String,
    variants: Vec<(String, usize)>, // (变体名, 元组载荷数)
}

#[derive(Clone)]
struct TypeInfo {
    name: String,
    is_class: bool,
    fields: Vec<(String, VTy)>, // (字段名, 类型)
}

pub struct Trans {
    sink: Vec<String>, // 缓冲栈:顶层缓冲即最终产物
    scopes: Vec<Vec<(String, String, VTy)>>, // 作用域栈:(名, C 名, 类型)
    uniq: u32,
    fns: Vec<(String, Vec<VTy>, VTy)>,
    types: Vec<TypeInfo>,
    type_by_name: std::collections::HashMap<String, u32>,
    enums: Vec<EnumInfo>,
    enum_by_name: std::collections::HashMap<String, u32>,
}

impl Trans {
    pub fn new() -> Self {
        Trans {
            sink: vec![String::new()], scopes: Vec::new(), uniq: 0,
            fns: Vec::new(), types: Vec::new(),
            type_by_name: std::collections::HashMap::new(),
            enums: Vec::new(),
            enum_by_name: std::collections::HashMap::new(),
        }
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
        // 预定义和类型:variant 0 = Some/Ok(载荷在 p[0]),variant 1 = None/Err
        self.intern_enum("Option", vec![("Some".into(), 1), ("None".into(), 0)]);
        self.intern_enum("Result", vec![("Ok".into(), 1), ("Err".into(), 1)]);
        let mut tests = Vec::new();
        for d in &file.decls {
            match d {
                ast::Decl::Enum(en) => {
                    if !en.type_params.is_empty() {
                        return Err("trans v1 拒绝域:泛型 enum".into());
                    }
                    let variants: Vec<(String, usize)> = en.variants.iter().map(|v| {
                        let arity = match &v.kind {
                            ast::VariantKind::Unit => 0,
                            ast::VariantKind::Tuple(ts) => ts.len(),
                            _ => 0,
                        };
                        (v.name.clone(), arity)
                    }).collect();
                    self.intern_enum(&en.name, variants);
                }
                ast::Decl::Struct(st) => {
                    if !st.type_params.is_empty() {
                        return Err("trans v1 拒绝域:泛型 struct(单态化未实现)".into());
                    }
                    self.collect_type(&st.name, &st.fields, false)?
                }
                ast::Decl::Class(cl) => {
                    if !cl.type_params.is_empty() {
                        return Err("trans v1 拒绝域:泛型 class".into());
                    }
                    if cl.items.iter().any(|it| !matches!(it, ast::ClassItem::Field(_))) {
                        return Err("trans v1 拒绝域:class 方法/prop".into());
                    }
                    self.collect_class(cl)?
                }
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

        // 用户类型 typedef(struct 值语义 / class 指针语义)
        let types_snapshot = self.types.clone();
        for t in &types_snapshot {
            let fs: Vec<String> = t.fields.iter()
                .map(|(n, ty)| format!("    {} {};", self.c_ty(*ty), n))
                .collect();
            self.w(0, &format!("typedef struct {{\n{}\n}} ctn_{};", fs.join("\n"), t.name));
        }

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

    fn intern_enum(&mut self, name: &str, variants: Vec<(String, usize)>) -> u32 {
        if let Some(&id) = self.enum_by_name.get(name) { return id; }
        let id = self.enums.len() as u32;
        self.enums.push(EnumInfo { _name: name.to_string(), variants });
        self.enum_by_name.insert(name.to_string(), id);
        id
    }

    fn intern_type(&mut self, name: &str, is_class: bool) -> u32 {
        if let Some(&id) = self.type_by_name.get(name) { return id; }
        let id = self.types.len() as u32;
        self.types.push(TypeInfo { name: name.to_string(), is_class, fields: Vec::new() });
        self.type_by_name.insert(name.to_string(), id);
        id
    }

    fn collect_type(&mut self, name: &str, fields: &[ast::Field], is_class: bool) -> Result<(), String> {
        let id = self.intern_type(name, is_class);
        let _ = id;
        let mut fs = Vec::new();
        for f in fields {
            let ty = self.field_ty(&f.ty)?;
            fs.push((f.name.clone(), ty));
        }
        self.types[id as usize].fields = fs;
        Ok(())
    }

    fn collect_class(&mut self, cl: &ast::ClassDecl) -> Result<(), String> {
        let fields: Vec<ast::Field> = cl.items.iter().filter_map(|it| match it {
            ast::ClassItem::Field(f) => Some(f.clone()),
            _ => None,
        }).collect();
        self.collect_type(&cl.name, &fields, true)
    }

    fn field_ty(&self, t: &ast::Type) -> Result<VTy, String> {
        if let ast::Type::Named { path, .. } = t {
            if let Some(name) = path.last() {
                if let Some(v) = scalar_annotation(name) { return Ok(v); }
                if let Some(&id) = self.type_by_name.get(name.as_str()) {
                    let is_class = self.types[id as usize].is_class;
                    return Ok(if is_class { VTy::Class(id) } else { VTy::Struct(id) });
                }
                return Err(format!("trans v1 拒绝域:字段类型 `{}`(仅数值/Bool/Str/用户类型)", name));
            }
        }
        Err("trans v1 拒绝域:字段类型形态".into())
    }

    /// 模式编译:(变体号, 载荷绑定语句);-1 = 通配臂
    /// 变体名 → (enum id, variant index)
    fn find_variant(&self, name: &str) -> Option<(u32, usize)> {
        for (eid, e) in self.enums.iter().enumerate() {
            if let Some((vi, _)) = e.variants.iter().enumerate().find(|(_, (vn, _))| vn == name) {
                return Some((eid as u32, vi));
            }
        }
        None
    }

    fn pat_arm(&mut self, pat: &ast::Pattern, mv: &str, pay: Pay) -> Result<(i32, Vec<String>), String> {
        let ast::Pattern::Agg { path, sub } = pat else {
            return Err("trans v1 拒绝域:match 模式(仅变体/通配)".into());
        };
        let vname = path.last().cloned().unwrap_or_default();
        let Some(eid) = self.enum_by_name.values().cloned().reduce(|a, _| a) else {
            return Err("trans:无和类型".into());
        };
        let _ = eid;
        // 在全部枚举里找变体定义
        let mut found: Option<(i32, usize)> = None;
        for e in &self.enums {
            if let Some((vi, (_vn, arity))) = e.variants.iter().enumerate()
                .find(|(_, (vn, _))| *vn == vname)
            {
                found = Some((vi as i32, *arity));
            }
        }
        let Some((vid, arity)) = found else {
            return Err(format!("trans:未知变体 `{}`", vname));
        };
        let mut binds = Vec::new();
        match sub {
            ast::AggSub::Unit => {}
            ast::AggSub::Tuple(ps) => {
                for (i, sp) in ps.iter().enumerate() {
                    match sp {
                        ast::Pattern::Ident(n) => {
                            let slot_ty = if pay == Pay::F { VTy::F64 } else { VTy::Int(None) };
                            let slot = if pay == Pay::F { "f" } else { "i" };
                            let ctype = if pay == Pay::F { "double" } else { "ct_i" };
                            let cname = self.bind(n, slot_ty);
                            binds.push(format!("{} {} = {}.p[{}].{};", ctype, cname, mv, i, slot));
                        }
                        ast::Pattern::Wildcard => {}
                        _ => return Err("trans v1 拒绝域:载荷子模式".into()),
                    }
                }
            }
            _ => return Err("trans v1 拒绝域:struct 载荷模式".into()),
        }
        let _ = arity;
        Ok((vid, binds))
    }

    /// 字段访问基串(含分隔符):struct 值用 `.`,class/Boxed 指针用 `->`
    fn deref_obj(&self, c: &str, ty: VTy) -> Result<(String, u32), String> {
        match ty {
            VTy::Struct(id) => Ok((format!("({}).", c), id)),
            VTy::Class(id) | VTy::Boxed(id) => Ok((format!("({})->", c), id)),
            _ => Err("trans:字段访问需用户类型".into()),
        }
    }

    fn pay_of(&self, t: &ast::Type) -> Pay {
        if self.ty_of(t).is_float() { Pay::F } else { Pay::I }
    }

    fn ty_of(&self, t: &ast::Type) -> VTy {
        match t {
            ast::Type::Named { path, .. } => {
                if let Some(name) = path.last() {
                    if let Some(v) = scalar_annotation(name) { return v; }
                    if let Some(&id) = self.type_by_name.get(name.as_str()) {
                        let is_class = self.types[id as usize].is_class;
                        return if is_class { VTy::Class(id) } else { VTy::Struct(id) };
                    }
                    if let Some(&id) = self.enum_by_name.get(name.as_str()) {
                        // 载荷类别取第一个类型实参(Option[F64] → F;无实参 → I)
                        let pay = if let ast::Type::Named { args, .. } = t {
                            args.first().map(|a| {
                                if self.ty_of(a).is_float() { Pay::F } else { Pay::I }
                            }).unwrap_or(Pay::I)
                        } else { Pay::I };
                        return VTy::Sum(id, pay);
                    }
                }
                VTy::Unknown
            }
            // T[N] 定长 / T[] 视图 / &T[] 只读视图:统一数组句柄(interp 同为 Value::Array)
            ast::Type::Slice(_) | ast::Type::Array { .. } => VTy::Array,
            ast::Type::Ref(inner) => self.ty_of(inner),
            ast::Type::Optional(inner) => {
                let id = self.enum_by_name.get("Option").copied().unwrap_or(0);
                VTy::Sum(id, self.pay_of(inner))
            }
            _ => VTy::Unknown,
        }
    }

    fn c_ty(&self, t: VTy) -> &'static str {
        match t {
            VTy::F64 => "double",
            VTy::F32 => "float",
            VTy::Bool => "int",
            VTy::Str => "char*",
            VTy::Range => "ct_range",
            VTy::Array => "ct_arr*",
            VTy::Sum(..) => "ct_sum",
            VTy::Struct(id) => leak_str(format!("ctn_{}", self.types[id as usize].name)),
            VTy::Class(id) | VTy::Boxed(id) => leak_str(format!("ctn_{}*", self.types[id as usize].name)),
            VTy::Int(_) | _ => "ct_i",
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
                // 字段/索引赋值
                if let ast::Expr::Member { obj, target: mt } = target {
                    let name = match mt {
                        ast::MemberTarget::Name(n) => n.clone(),
                        _ => return Err("trans v1 拒绝域:元组索引赋值".into()),
                    };
                    let (oc, oty) = self.expr(obj)?;
                    let (base, _tid) = self.deref_obj(&oc, oty)?;
                    let (v, fvty) = self.expr(value)?;
                    let lhs = format!("{}{}", base, name);
                    let _ = oty;
                    let _ = fvty;
                    return match op {
                        ast::AssignOp::Eq => {
                            self.w(1, &format!("{} = {};", lhs, v));
                            Ok(())
                        }
                        other => {
                            let bin = assign_binop(other);
                            let (cc, _) = self.binop(&bin, &format!("({})", lhs), VTy::Int(None), &v, VTy::Int(None))?;
                            self.w(1, &format!("{} = {};", lhs, cc));
                            Ok(())
                        }
                    };
                }
                if let ast::Expr::Index { obj, index } = target {
                    let (oc, oty) = self.expr(obj)?;
                    if oty != VTy::Array { return Err("trans:索引赋值仅支持数组".into()); }
                    let (ic, ity) = self.expr(index)?;
                    if !matches!(ity, VTy::Int(_)) { return Err("trans:索引需整数".into()); }
                    let (v, _) = self.expr(value)?;
                    return match op {
                        ast::AssignOp::Eq => {
                            self.w(1, &format!("ct_elem_store({}, {}, {});", oc, ic, v));
                            Ok(())
                        }
                        other => {
                            let bin = assign_binop(other);
                            let (cc, _) = self.binop(&bin, &format!("ct_elem({}, {})", oc, ic), VTy::Int(None), &v, VTy::Int(None))?;
                            self.w(1, &format!("ct_elem_store({}, {}, {});", oc, ic, cc));
                            Ok(())
                        }
                    };
                }
                let ast::Expr::Ident(name) = target else {
                    return Err("trans v1 拒绝域:赋值目标".into());
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
                // 三种迭代:range 字面量 / Range 类型的值 / 数组(Range/数组值只求值一次)
                enum IterKind { RangeLit, RangeVal, Arr }
                let (kind, lo_c, hi_c, incl, tmp, init) = match iter {
                    ast::Expr::Range { inclusive, from, to } => {
                        let (fc, ft) = self.expr(from.as_ref())?;
                        if !matches!(ft, VTy::Int(_)) { return Err("trans:range 端点需整数".into()); }
                        let (tcc, _) = self.expr(to.as_ref())?;
                        (IterKind::RangeLit, fc, tcc, *inclusive, String::new(), String::new())
                    }
                    other => {
                        let (c, ty) = self.expr(other)?;
                        match ty {
                            VTy::Range => {
                                let t = self.uniq_name("rng");
                                (IterKind::RangeVal, String::new(), String::new(), false, t, c)
                            }
                            VTy::Array => {
                                let t = self.uniq_name("arr");
                                (IterKind::Arr, String::new(), String::new(), false, t, c)
                            }
                            _ => return Err("trans v1 拒绝域:for 仅支持 range/数组".into()),
                        }
                    }
                };
                let end = self.uniq_name("end");
                let it = self.uniq_name("it");
                match kind {
                    IterKind::RangeLit => {
                        self.w(1, &format!("{{ ct_i {} = (ct_i)({});", end, hi_c));
                        let cmp = if incl { "<=" } else { "<" };
                        self.w(1, &format!(
                            "for (ct_i {} = {}; {} {} {}; {}++) {{", it, lo_c, it, cmp, end, it));
                    }
                    IterKind::RangeVal => {
                        self.w(1, &format!("{{ ct_range {} = {};", tmp, init));
                        let cmp = if incl { "<=" } else { "<" };
                        self.w(1, &format!(
                            "for (ct_i {} = (ct_i)({}.lo); {} {} (ct_i)({}.hi); {}++) {{",
                            it, tmp, it, cmp, tmp, it));
                    }
                    IterKind::Arr => {
                        self.w(1, &format!("{{ ct_arr* {} = {};", tmp, init));
                        self.w(1, &format!(
                            "for (size_t {} = 0; {} < {}->n; {}++) {{", it, it, tmp, it));
                    }
                }
                self.scope_push();
                let c = self.bind(name, VTy::Int(Some((IntW::W64, true))));
                match kind {
                    IterKind::Arr => self.w(1, &format!("ct_i {} = {}->d[{}];", c, tmp, it)),
                    _ => self.w(1, &format!("ct_i {} = {};", c, it)),
                }
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
                if let Some((c, ty)) = self.lookup(name) {
                    return Ok((c, ty));
                }
                // 无参变体作为值表达式(如 `return Stop`)
                if let Some((eid, vid)) = self.find_variant(name) {
                    if self.enums[eid as usize].variants[vid as usize].1 == 0 {
                        return Ok((format!("(ct_sum){{ {}, {{ 0, 0, 0, 0 }} }}", vid), VTy::Sum(eid, Pay::I)));
                    }
                }
                Err(format!("trans:未绑定标识符 `{}`", name))
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

                if merged.is_num() || merged == VTy::Bool || merged == VTy::Str || merged == VTy::Range {
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
            ast::Expr::Str { parts } => {
                // 串插值:各段求值 → 字符串化 → ct_concat 链(进程期分配,v1 不回收)
                let mut acc: Option<String> = None;
                for p in parts {
                    let (c, _ty) = match p {
                        ast::StrPart::Text(t) => (format!("(char*)\"{}\"", c_escape(t)), VTy::Str),
                        ast::StrPart::Interp(src) => {
                            let (tokens, _) = crate::lex(src);
                            let mut parser = crate::parser::Parser::new(tokens);
                            let e = parser.parse_expr_public();
                            let (c, ty) = self.expr(&e)?;
                            (display_wrap(&c, ty)?, VTy::Str)
                        }
                    };
                    acc = Some(match acc {
                        Some(prev) => format!("ct_concat({}, {})", prev, c),
                        None => c,
                    });
                }
                match acc {
                    Some(c) => Ok((c, VTy::Str)),
                    None => Ok(("(char*)\"\"".into(), VTy::Str)),
                }
            }
            ast::Expr::Range { inclusive, from, to } => {
                let (fc, ft) = self.expr(from)?;
                if !matches!(ft, VTy::Int(_)) { return Err("trans:range 端点需整数".into()); }
                let (tcc, _) = self.expr(to)?;
                Ok((
                    format!("(ct_range){{ {}, {}, {} }}", fc, tcc, if *inclusive { 1 } else { 0 }),
                    VTy::Range,
                ))
            }
            ast::Expr::Member { obj, target: ast::MemberTarget::Name(m) } => {
                let (c, ty) = self.expr(obj)?;
                // 属性访问(无括号)
                if ty == VTy::Str {
                    return match m.as_str() {
                        "len" => Ok((format!("((ct_i)strlen({}))", c), VTy::Int(Some((IntW::W64, false))))),
                        "char_len" => Ok((format!("((ct_i)ct_char_len({}))", c), VTy::Int(Some((IntW::W64, false))))),
                        _ => Err(format!("trans v1 拒绝域:Str 属性 `{}`", m)),
                    };
                }
                if ty == VTy::Array {
                    return match m.as_str() {
                        "len" => Ok((format!("((ct_i)({})->n)", c), VTy::Int(Some((IntW::W64, false))))),
                        _ => Err(format!("trans v1 拒绝域:Array 属性 `{}`", m)),
                    };
                }
                // 用户类型字段(struct 值 / class、Boxed 指针)
                let (base, tid) = self.deref_obj(&c, ty)?;
                let fty = self.types[tid as usize].fields.iter()
                    .find(|(n, _)| n == m).map(|(_, t)| *t);
                let Some(fty) = fty else {
                    return Err(format!("trans:类型无字段 `{}`", m));
                };
                return Ok((format!("{}{}", base, m), fty));
            }
            ast::Expr::Index { obj, index } => {
                let (oc, ty) = self.expr(obj)?;
                if ty != VTy::Array { return Err("trans:索引仅支持数组".into()); }
                let (ic, it) = self.expr(index)?;
                if !matches!(it, VTy::Int(_)) { return Err("trans:索引需整数".into()); }
                Ok((format!("ct_elem({}, {})", oc, ic), VTy::Int(None)))
            }
            ast::Expr::Array(items) => {
                let mut parts = Vec::new();
                for i in items {
                    let (c, _) = self.expr(i)?;
                    parts.push(c);
                }
                let n = parts.len();
                let inits = if parts.is_empty() { "NULL".to_string() }
                    else { format!("(ct_i[]){{{}}}", parts.join(", ")) };
                Ok((format!("ct_arr_new({}, {})", n, inits), VTy::Array))
            }
            ast::Expr::StructLit { path, fields, .. } => {
                let name = path.last().cloned().unwrap_or_default();
                let Some(&tid) = self.type_by_name.get(name.as_str()) else {
                    return Err(format!("trans:未解析类型 `{}`", name));
                };
                let is_class = self.types[tid as usize].is_class;
                let mut inits = Vec::new();
                for f in fields {
                    let v = f.value.as_ref().ok_or("trans:字段简写未支持")?;
                    let (c, _) = self.expr(v)?;
                    inits.push(format!(".{} = ({})", f.name, c));
                }
                let lit = format!("(ctn_{}){{ {} }}", name, inits.join(", "));
                if is_class {
                    // class 引用语义:字面量即堆分配
                    Ok((format!("({{ ctn_{name}* p = malloc(sizeof(ctn_{name})); *p = {lit}; p; }})", name = name, lit = lit), VTy::Class(tid)))
                } else {
                    Ok((lit, VTy::Struct(tid)))
                }
            }
            ast::Expr::Match { expr, arms } => {
                // 语句形态 match:variant 分派 + 载荷绑定;无匹配臂 panic(对齐 interp)
                let (sc, sty) = self.expr(expr)?;
                let VTy::Sum(_, pay) = sty else { return Err("trans:match 需和类型".into()); };
                let mv = self.uniq_name("m");
                let dv = self.uniq_name("done");
                self.w(1, &format!("{{ ct_sum {} = ({}); int {} = 0;", mv, sc, dv));
                for arm in arms {
                    let (vid, binds) = self.pat_arm(&arm.pattern, &mv, pay)?;
                    self.scope_push();
                    let cond = if vid == -1 { String::new() } else { format!("if ({}.variant == {}) {{", mv, vid) };
                    if vid != -1 {
                        self.w(2, &cond);
                        for b in binds {
                            self.w(3, &b);
                        }
                    } else {
                        // 通配臂放最后,直接开块
                        self.w(2, "{");
                    }
                    let (ac, aty) = self.expr(&arm.expr)?;
                    let _ = aty;
                    self.w(3, &format!("(void)({});", ac));
                    self.w(3, &format!("{} = 1;", dv));
                    self.w(2, "}");
                    self.scope_pop();
                }
                self.w(2, &format!("if (!{}) ct_panic(\"match 无匹配臂\");", dv));
                self.w(1, "}");
                Ok(("0".to_string(), VTy::Void))
            }
            ast::Expr::Try(e) => {
                // `?`:variant 1(None/Err)→ 提前 return 同型空值;否则解包载荷
                let (c, ty) = self.expr(e)?;
                let VTy::Sum(_, pay) = ty else { return Err("trans:? 需 Option/Result".into()); };
                let _ = ty;
                Ok((
                    format!(
                        "({{ ct_sum ct_t = ({}); if (ct_t.variant == 1) {{ return (ct_sum){{ 1, {{ {{.i = 0}}, {{.i = 0}}, {{.i = 0}}, {{.i = 0}} }} }}; }} ct_t.p[0].i; }})",
                        c
                    ),
                    if pay == Pay::F { VTy::F64 } else { VTy::Int(None) },
                ))
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
        // 和类型变体构造器:Some/None/Ok/Err/用户 enum 变体
        if let ast::Expr::Ident(name) = callee {
            if let Some(&eid) = self.enum_by_name.get("*prelude*") { let _ = eid; }
            let mut hit: Option<(u32, usize, usize)> = None;
            for (eid, e) in self.enums.iter().enumerate() {
                if let Some((vi, (_vn, arity))) = e.variants.iter().enumerate().find(|(_, (vn, _))| vn == name) {
                    hit = Some((eid as u32, vi, *arity));
                    break;
                }
            }
            if let Some((eid, vi, arity)) = hit {
                if args.len() != arity {
                    return Err(format!("trans:变体 `{}` 需 {} 个载荷", name, arity));
                }
                let mut ps = Vec::new();
                for a in args.iter() {
                    let (c, at) = self.expr(a)?;
                    ps.push(payload_cell(c, at)?);
                }
                let fields: Vec<String> = (0..4).map(|i| {
                    ps.get(i).cloned().unwrap_or_else(|| "{.i = 0}".into())
                }).collect();
                return Ok((format!("(ct_sum){{ {}, {{ {} }} }}", vi, fields.join(", ")), VTy::Sum(eid, Pay::I)));
            }
        }
        // Box[T](v) — 堆装箱
        if let ast::Expr::TypeArgs { expr, .. } = callee {
            if let ast::Expr::Ident(tn) = &**expr {
                if tn == "Box" {
                    let Some(a) = args.first() else { return Err("trans:Box 需实参".into()) };
                    let (vc, vty) = self.expr(a)?;
                    let tid = match vty {
                        VTy::Struct(id) => id,
                        _ => return Err("trans v1 拒绝域:Box 仅支持用户 struct".into()),
                    };
                    let name = self.types[tid as usize].name.clone();
                    return Ok((
                        format!("({{ ctn_{n}* p = malloc(sizeof(ctn_{n})); *p = {v}; p; }})", n = name, v = vc),
                        VTy::Boxed(tid),
                    ));
                }
            }
        }
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
                    let ok = if at == VTy::Str && bt == VTy::Str {
                        format!("(strcmp({}, {}) == 0)", a, b)
                    } else if at.is_float() || bt.is_float() {
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
        // 方法调用:接收者.方法(实参) — Str 方法,其次 UFCS 用户函数
        if let ast::Expr::Member { obj, target: ast::MemberTarget::Name(m) } = callee {
            let (rc, rt) = self.expr(obj)?;
            if rt == VTy::Str {
                let r = match m.as_str() {
                    "len" => Some((format!("((ct_i)strlen({}))", rc), VTy::Int(Some((IntW::W64, false))))),
                    "char_len" => Some((format!("((ct_i)ct_char_len({}))", rc), VTy::Int(Some((IntW::W64, false))))),
                    "contains" => {
                        let Some(a) = args.first() else { return Err("trans:contains 需实参".into()) };
                        let (ac, at) = self.expr(a)?;
                        if at != VTy::Str { return Err("trans:contains 需 Str".into()); }
                        Some((format!("(ct_i)(ct_contains({}, {}) != NULL)", rc, ac), VTy::Int(Some((IntW::W64, true)))))
                    }
                    "slice" => {
                        let Some(a) = args.first() else { return Err("trans:slice 需实参".into()) };
                        let ast::Expr::Range { from, to, .. } = a else {
                            return Err("trans:slice 需 range 实参".into());
                        };
                        let (fc, _) = self.expr(from)?;
                        let (tcc, _) = self.expr(to)?;
                        Some((format!("ct_slice({}, (long)({}), (long)({}))", rc, fc, tcc), VTy::Str))
                    }
                    "to_string" => Some((format!("ct_dup({})", rc), VTy::Str)),
                    _ => None,
                };
                if let Some(r) = r { return Ok(r); }
            }
            // Option/Result 方法
            if let VTy::Sum(_, _) = rt {
                let r = match m.as_str() {
                    "or" => {
                        let Some(a) = args.first() else { return Err("trans:or 需默认值".into()) };
                        let (dc, dty) = self.expr(a)?;
                        if !dty.is_num() { return Err("trans:or 默认值需数值".into()); }
                        let slot = if dty.is_float() { "f" } else { "i" };
                        let cast = if dty == VTy::F32 { "(float)" } else { "" };
                        (format!(
                            "({{ ct_sum t = ({}); (t.variant == 0) ? {}t.p[0].{} : {}{}; }})",
                            rc, cast, slot, cast, dc),
                         dty)
                    }
                    "expect" => {
                        let Some(a) = args.first() else { return Err("trans:expect 需消息".into()) };
                        let ast::Expr::Str { parts } = a else {
                            return Err("trans v1 拒绝域:非字面量 expect 消息".into());
                        };
                        let msg = match parts.as_slice() {
                            [ast::StrPart::Text(t)] => t.replace('"', "\\\""),
                            _ => "expect failed".to_string(),
                        };
                        (format!(
                            "({{ ct_sum t = ({}); if (t.variant == 1) ct_panic(\"{}\"); t.p[0].i; }})", rc, msg),
                         VTy::Int(None))
                    }
                    "is_some" | "is_ok" => {
                        (format!("(ct_i)(({}).variant == 0)", rc), VTy::Int(Some((IntW::W64, true))))
                    }
                    "is_none" | "is_err" => {
                        (format!("(ct_i)(({}).variant == 1)", rc), VTy::Int(Some((IntW::W64, true))))
                    }
                    _ => return Err(format!("trans v1 拒绝域:和类型方法 `.{}`", m)),
                };
                return Ok(r);
            }
            // UFCS:用户自由函数以接收者为首参
            if let Some((ptys, ret)) = self.lookup_fn(m) {
                if args.len() == ptys.len().saturating_sub(1) || args.len() + 1 == ptys.len() {
                    let mut cs = vec![rc];
                    for a in args {
                        let (c, _) = self.expr(a)?;
                        cs.push(c);
                    }
                    if cs.len() == ptys.len() {
                        let mut final_cs = Vec::new();
                        for (c, pt) in cs.iter().zip(&ptys) {
                            final_cs.push(coerce(self.c_ty(*pt), c.clone(), VTy::Unknown, *pt));
                        }
                        return Ok((format!("{}({})", sanitize(m), final_cs.join(", ")), ret));
                    }
                }
            }
        }
        Err("trans v1 拒绝域:该调用形态(仅内建/数值函数)".into())
    }

    fn binop(&mut self, op: &ast::BinOp, lc: &str, lt: VTy, rc: &str, rt: VTy) -> TRes {
        use ast::BinOp::*;
        match op {
            AndAnd => Ok((format!("(({}) && ({}))", lc, rc), VTy::Bool)),
            Eq | Ne | Lt | Gt | Le | Ge => {
                if lt == VTy::Str && rt == VTy::Str {
                    if !matches!(op, Eq | Ne) { return Err("trans:Str 仅可 ==/!=".into()); }
                    let c = if *op == Eq { "==" } else { "!=" };
                    return Ok((format!("((strcmp({}, {}) {}) 0)", lc, rc, c), VTy::Bool));
                }
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
fn assign_binop(op: &ast::AssignOp) -> ast::BinOp {
    match op {
        ast::AssignOp::AddEq => ast::BinOp::Add,
        ast::AssignOp::SubEq => ast::BinOp::Sub,
        ast::AssignOp::MulEq => ast::BinOp::Mul,
        ast::AssignOp::DivEq => ast::BinOp::Div,
        _ => ast::BinOp::Mod,
    }
}

/// 变体载荷 → ct_cell 初始化(v1:数值/Bool)
fn payload_cell(c: String, ty: VTy) -> Result<String, String> {
    Ok(match ty {
        VTy::Int(_) => format!("{{.i = {}}}", c),
        VTy::Bool => format!("{{.i = (ct_i)(!!({}))}}", c),
        VTy::F64 => format!("{{.f = {}}}", c),
        VTy::F32 => format!("{{.f = (double)({})}}", c),
        _ => return Err("trans v1 拒绝域:该载荷类型".into()),
    })
}

/// intern 成 &'static str(编译器进程一次性,不做回收)
fn leak_str(s: String) -> &'static str {
    Box::leak(s.into_boxed_str())
}

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

/// 插值段字符串化(对齐 interp to_display 的分派)
fn display_wrap(c: &str, ty: VTy) -> Result<String, String> {
    Ok(match ty {
        VTy::Str => c.to_string(),
        VTy::Bool => format!("ct_bool_str((int)(!!({})))", c),
        VTy::F64 => format!("ct_f64_str({})", c),
        VTy::F32 => format!("ct_f32_str({})", c),
        VTy::Int(_) => format!("ct_i128_str({})", c),
        _ => return Err("trans v1 拒绝域:插值表达式类型".into()),
    })
}

/// 解码后的 Ctron 字符串文本 → C 字符串字面量转义(非 ASCII 原样 UTF-8 字节)
fn c_escape(t: &str) -> String {
    // 字节级转义:非 ASCII 字节原样保留(UTF-8 字节流直通 C 字面量)
    let mut out: Vec<u8> = Vec::new();
    for &b in t.as_bytes() {
        match b {
            b'\\' => out.extend_from_slice(b"\\\\"),
            b'"' => out.extend_from_slice(b"\\\""),
            b'\n' => out.extend_from_slice(b"\\n"),
            b'\t' => out.extend_from_slice(b"\\t"),
            b'\r' => out.extend_from_slice(b"\\r"),
            0x00..=0x1F | 0x7F => out.extend_from_slice(format!("\\{:03o}", b).as_bytes()),
            _ => out.push(b),
        }
    }
    String::from_utf8(out).expect("UTF-8")
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
#include <string.h>
#include <signal.h>
#include <unistd.h>

typedef __int128 ct_i;
typedef struct { ct_i lo; ct_i hi; int incl; } ct_range;

static char* ct_f32_str(float f) {
    static char bufs[8][64];
    static int rot = 0;
    rot = (rot + 1) % 8;
    double d = (double)f;
    if (d == (double)(long long)d && d < 1e15 && d > -1e15) snprintf(bufs[rot], 64, "%.1f", d);
    else snprintf(bufs[rot], 64, "%.9g", d);
    return bufs[rot];
}
static const char* ct_cur_test = "";

static void ct_panic(const char* msg) {
    fprintf(stderr, "panic: %s (test %s)\n", msg, ct_cur_test);
    exit(1);
}
static int ct_assert(int ok) {
    if (!ok) { fprintf(stderr, "assertion failed (test %s)\n", ct_cur_test); exit(1); }
    return ok;
}
/* ---- 和类型:tagged union;载荷槽 i/f 按静态类型选用 ---- */
typedef union { ct_i i; double f; } ct_cell;
typedef struct { int variant; ct_cell p[4]; } ct_sum;

/* ---- 数组运行时:句柄共享语义(对齐 interp Rc<Vec>),进程期不回收 ---- */
typedef struct { ct_i* d; size_t n; } ct_arr;
static ct_arr* ct_arr_new(size_t n, ct_i* init) {
    ct_arr* a = (ct_arr*)malloc(sizeof(ct_arr));
    if (!a) abort();
    a->n = n;
    a->d = n ? (ct_i*)malloc(n * sizeof(ct_i)) : NULL;
    if (n && !a->d) abort();
    for (size_t i = 0; i < n; i++) a->d[i] = init[i];
    return a;
}
static ct_i ct_elem(ct_arr* a, ct_i i) {
    if (i < 0 || (size_t)i >= a->n) { fprintf(stderr, "index out of bounds\n"); exit(1); }
    return a->d[(size_t)i];
}
static void ct_elem_store(ct_arr* a, ct_i i, ct_i v) {
    if (i < 0 || (size_t)i >= a->n) { fprintf(stderr, "index out of bounds\n"); exit(1); }
    a->d[(size_t)i] = v;
}

/* ---- Str 运行时(进程期分配,v1 不回收;参考后端契约) ---- */
static char* ct_concat(const char* a, const char* b) {
    size_t la = strlen(a), lb = strlen(b);
    char* r = (char*)malloc(la + lb + 1);
    if (!r) abort();
    memcpy(r, a, la); memcpy(r + la, b, lb + 1);
    return r;
}
static char* ct_dup(const char* s) {
    char* r = (char*)malloc(strlen(s) + 1);
    if (!r) abort();
    strcpy(r, s);
    return r;
}
static size_t ct_char_len(const char* s) {
    size_t n = 0;
    for (const unsigned char* p = (const unsigned char*)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}
static int ct_contains(const char* s, const char* sub) { return strstr(s, sub) != NULL; }
static int ct_is_boundary(const char* s, long i) {
    if (i < 0) return 0;
    unsigned char c = (unsigned char)s[i];
    return c == 0 || (c & 0xC0) != 0x80;
}
static char* ct_slice(const char* s, long f, long t) {
    if (t < f) ct_panic("invalid utf8 boundary");
    if (!ct_is_boundary(s, f) || !ct_is_boundary(s, t)) ct_panic("invalid utf8 boundary");
    char* r = (char*)malloc((size_t)(t - f) + 1);
    if (!r) abort();
    memcpy(r, s + f, (size_t)(t - f));
    r[t - f] = '\0';
    return r;
}
static char* ct_i128_str(ct_i v) {
    static char bufs[8][48];
    static int rot = 0;
    rot = (rot + 1) % 8;
    char* p = bufs[rot] + 47;
    *p = '\0';
    int neg = v < 0;
    unsigned __int128 u = neg ? (unsigned __int128)(-v) : (unsigned __int128)v;
    do { *--p = (char)('0' + (char)(u % 10)); u /= 10; } while (u);
    if (neg) *--p = '-';
    return p;
}
static char* ct_bool_str(int b) { return b ? (char*)"true" : (char*)"false"; }
static char* ct_f64_str(double d) {
    static char bufs[8][64];
    static int rot = 0;
    rot = (rot + 1) % 8;
    if (d == (double)(long long)d && d < 1e15 && d > -1e15) snprintf(bufs[rot], 64, "%.1f", d);
    else snprintf(bufs[rot], 64, "%.17g", d);
    return bufs[rot];
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
