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
    /// 错误对象指针(AnyError:context 产物;载体为 ct_i 槽位)
    ErrPtr,
    /// 错误 cause 专用 Option(载荷为错误对象;expect 解包为 ErrPtr)
    SumErr(u32, Pay),
    /// 函数指针(ct_i → ct_i;非捕获闭包与用户函数引用)
    FnPtr,
    /// 二元组:索引指向 tuples 表
    Tup(u32),
    Arena,
    /// 可变单元格(Global/Atomic/Mutex):id → cells 表(内层类型)
    Cell(u32),
    /// trait 对象形参(&Trait):调用点按实参具体类型单态化;id → traits 表
    TraitObj(u32),
    Simd,
    FArr,
    /// 并发任务句柄(ct_task*)
    Task,
    /// scope 句柄(ct_scope*)
    ScopeH,
    /// 通道端点(ct_chan*)
    Chan,
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
        "Void" => VTy::Void,
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
    impls: Vec<(String, String)>, // (trait 名, 类型名)——方法/prop 分发注册表
    extern_fns: std::collections::HashSet<String>, // extern "c" 无体函数 → 调用点用裸名
    decls: Vec<ast::Decl>,
    generics: std::collections::HashMap<String, (bool, Vec<(String, ast::Type)>)>, // 名 → (is_class, 字段)
    mono_keys: std::collections::HashMap<String, u32>, // 实例键 → tid
    mono_names: std::collections::HashMap<String, (String, VTy)>, // 任意键 → (C 名, 返回类型)
    mono_seq: u32,

    tuples: Vec<(VTy, VTy)>,
    cells: Vec<(VTy, bool)>, // (内层类型, 是否 Mutex 独立锁)
    traits: Vec<String>,
    tuple_keys: std::collections::HashMap<String, u32>,
    late_defs: Vec<String>, // 晚期定义(实例 typedef / 单态 fn / show fn)
    mcell_typedefs: std::cell::RefCell<Vec<String>>, // Mutex 单元格 typedef(c_ty &self 惰性发射)
    closure_defs: Vec<String>, // 闭包 → 顶层静态函数定义(发射到类型之后)
    ret_anyerr: bool, // 当前函数返回类型含 AnyError → `?` 自动擦除
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
        Trans { impls: Vec::new(), extern_fns: std::collections::HashSet::new(), decls: Vec::new(),
            generics: std::collections::HashMap::new(),
            mono_keys: std::collections::HashMap::new(),
            mono_names: std::collections::HashMap::new(),
            mono_seq: 0,

            tuples: Vec::new(),
            cells: Vec::new(),
            traits: Vec::new(),
            tuple_keys: std::collections::HashMap::new(),
            late_defs: Vec::new(), closure_defs: Vec::new(), ret_anyerr: false,
            mcell_typedefs: std::cell::RefCell::new(Vec::new()),
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

    pub fn trans_file(self, file: &ast::File) -> Result<String, String> {
        self.trans_files(std::slice::from_ref(file))
    }

    /// 多文件包转译:合并全部 decls(同一 C 产物)
    pub fn trans_files(self, files: &[ast::File]) -> Result<String, String> {
        let decls: Vec<ast::Decl> = files.iter().flat_map(|f| f.decls.clone()).collect();
        let merged = ast::File { decls };
        self.trans_one(&merged)
    }

    fn trans_one(mut self, file: &ast::File) -> Result<String, String> {
        self.decls = file.decls.clone();
        for d in &file.decls {
            if let ast::Decl::Trait(tr) = d { self.traits.push(tr.name.clone()); }
        }
        self.scope_push(); // 全局层
        // 预定义和类型:variant 0 = Some/Ok(载荷在 p[0]),variant 1 = None/Err
        self.intern_enum("Option", vec![("Some".into(), 1), ("None".into(), 0)]);
        self.intern_enum("Result", vec![("Ok".into(), 1), ("Err".into(), 1)]);
        let mut tests = Vec::new();
        let mut consts: Vec<ast::ConstDecl> = Vec::new();
        for d in &file.decls {
            match d {
                ast::Decl::Const(c) => consts.push(c.clone()),
                ast::Decl::Static(st) => consts.push(ast::ConstDecl {
                    name: st.name.clone(), ty: st.ty.clone(), expr: st.expr.clone(),
                }),
                ast::Decl::Use(_) => {}
                ast::Decl::Trait(_) | ast::Decl::Impl(_) => {}
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
                    if st.type_params.is_empty() {
                        self.collect_type(&st.name, &st.fields, false)?
                    } else {
                        let fields = st.fields.iter().map(|f| (f.name.clone(), f.ty.clone())).collect();
                        self.generics.insert(st.name.clone(), (false, fields));
                    }
                }
                ast::Decl::Class(cl) => {
                    if !cl.type_params.is_empty() {
                        let fields = cl.items.iter().filter_map(|it| match it {
                            ast::ClassItem::Field(f) => Some((f.name.clone(), f.ty.clone())),
                            _ => None,
                        }).collect();
                        self.generics.insert(cl.name.clone(), (true, fields));
                    } else {
                        if cl.items.iter().any(|it| !matches!(it, ast::ClassItem::Field(_))) {
                            return Err("trans v1 拒绝域:class 方法/prop".into());
                        }
                        self.collect_class(cl)?
                    }
                }
                ast::Decl::Fn(f) => {
                    let params = f.params.iter().map(|p| match p {
                        ast::Param::Param { ty, .. } => self.ty_of(ty),
                        ast::Param::Receiver { .. } => VTy::Unknown,
                    }).collect();
                    let ret = f.ret.as_ref().map(|t| {
                        if let ast::Type::Named { path, .. } = t {
                            if path.last().map(|n| n == "List").unwrap_or(false) { return VTy::Array; }
                        }
                        self.ty_of(t)
                    }).unwrap_or(VTy::Void);
                    self.fns.push((f.name.clone(), params, ret));
                    // extern "c" 无体函数:符号来自外部 C 源,调用点用裸名
                    if f.abi.is_some() && f.body.is_none() {
                        self.extern_fns.insert(f.name.clone());
                    }
                }
                ast::Decl::Test(t) => tests.push(t.clone()),
            }
        }

        self.w(0, PREAMBLE);
        self.w(0, "/* @@LATE@@ */");

        // 用户类型 typedef(struct 值语义 / class 指针语义)——晚期实例经标记位补入
        let types_snapshot = self.types.clone();
        for t in &types_snapshot {
            self.emit_typedef(t);
        }

        // impl 方法/prop:静态函数,self 为首参
        for d in &file.decls {
            if let ast::Decl::Impl(im) = d {
                let tn = match &im.trait_ty { ast::Type::Named { path, .. } => path.last().cloned().unwrap_or_default(), _ => String::new() };
                let ft = match &im.for_ty { ast::Type::Named { path, .. } => path.last().cloned().unwrap_or_default(), _ => String::new() };
                self.impls.push((tn.clone(), ft.clone()));
                let tid = self.type_by_name.get(&ft).copied();
                // 默认体(impl 未提供时)
                let default_bodies: Vec<(String, &ast::FnDecl)> = file.decls.iter().filter_map(|d| {
                    if let ast::Decl::Trait(tr) = d {
                        if tr.name == tn {
                            return Some(tr.items.iter().filter_map(|it| match it {
                                crate::ast::TraitItem::Method(m) if m.body.is_some() => Some((m.name.clone(), m)),
                                _ => None,
                            }).collect::<Vec<_>>());
                        }
                    }
                    None
                }).flatten().collect();
                for item in &im.items {
                    match item {
                        ast::ImplItem::Method(mm) => {
                            let mname = if tn == "Drop" { format!("{}_drop", ft) } else { mm.name.clone() };
                            self.emit_impl_fn(&tn, &ft, tid, &mname, mm)?;
                        }
                        ast::ImplItem::Prop(pp) => {
                            self.emit_impl_prop(&tn, &ft, tid, pp)?;
                        }
                    }
                }
                // 缺省方法:从 trait 默认体生成(仅当 impl 未覆盖)
                for (mname, m) in &default_bodies {
                    if im.items.iter().any(|it| matches!(it, ast::ImplItem::Method(mm) if &mm.name == mname)) { continue; }
                    self.emit_impl_fn(&tn, &ft, tid, mname, m)?;
                }
                for (pname, _pty, pbody) in file.decls.iter().filter_map(|d| {
                    if let ast::Decl::Trait(tr) = d {
                        if tr.name == tn {
                            return Some(tr.items.iter().filter_map(|it| match it {
                                crate::ast::TraitItem::PropImpl(pp) if pp.body.is_some() => Some((pp.name.clone(), pp.ty.clone(), pp.body.clone())),
                                _ => None,
                            }).collect::<Vec<_>>());
                        }
                    }
                    None
                }).flatten() {
                    if im.items.iter().any(|it| matches!(it, ast::ImplItem::Prop(pp) if pp.name == pname)) { continue; }
                    let pp = ast::PropDecl { vis: ast::Vis::Private, name: pname, ty: _pty, body: pbody };
                    self.emit_impl_prop(&tn, &ft, tid, &pp)?;
                }
            }
        }
        for d in &file.decls {
            if let ast::Decl::Fn(f) = d {
                if f.abi.is_some() && f.body.is_none() {
                    // extern 原型:裸 C 符号
                    self.scope_push();
                    let mut parts = Vec::new();
                    for p in &f.params {
                        if let ast::Param::Param { ty, .. } = p {
                            let vty = self.ty_of(ty);
                            parts.push(self.abi_ty(vty));
                        }
                    }
                    let ret = self.lookup_fn(&f.name).map(|(_, r)| r).unwrap_or(VTy::Void);
                    let rty = if ret == VTy::Void { "void".to_string() } else { self.abi_ty(ret) };
                    self.late_defs.push(format!("extern {} {}({});\n", rty, f.name, parts.join(", ")));
                    self.scope_pop();
                } else if f.type_params.is_empty() && !self.fn_has_traitobj_param(&f.name) {
                    self.emit_fn(f)?;
                } // 泛型 / trait 对象形参 fn 由调用点单态化
            }
        }

        // const:运行期初始化(求值顺序 = 声明序;comptime 语义对语料等价)
        let mut calls = String::new();
        for c in &consts {
            let ty = self.ty_of(&c.ty);
            self.scope_push();
            let (c_code, vty) = self.expr(&c.expr)?;
            self.scope_pop();
            let cname = sanitize(&c.name);
            self.scopes.first_mut().unwrap().push((c.name.clone(), cname.clone(), vty));
            calls.push_str(&format!("    {} = ({});\n", cname, c_code));
            let ct = self.c_ty(ty).to_string();
            self.late_defs.push(format!("static {} {};\n", ct, cname));
        }
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
        } else if let Some((ptys, ret)) = self.lookup_fn("main") {
            // D4:fn main 真实发射(体已随用户 fn 发射为 ctn_main)
            if ptys.is_empty() {
                self.w(0, "int main(void) {");
                self.w(1, "alarm(20); /* 防挂起 */");
                if matches!(ret, VTy::Void) {
                    self.w(1, "ctn_main();");
                    self.w(1, "return 0;");
                } else {
                    self.w(1, "return (int)ctn_main();");
                }
                self.w(0, "}");
            } else {
                self.w(0, "int main(void) { alarm(20); return 0; }");
            }
        } else {
            self.w(0, "int main(void) { alarm(20); return 0; }");
        }
                let main = self.sink.pop().unwrap_or_default();
        // mcell typedef 需位于用户 typedef 之后、env typedef 之前
        let mdefs = self.mcell_typedefs.borrow().join("");
        let late = self.late_defs.join("");
        let late = match late.find("typedef struct { ct_scope*") {
            Some(pos) => format!("{}{}{}", &late[..pos], mdefs, &late[pos..]),
            None => format!("{}{}", late, mdefs),
        };
        let defs = late + &self.closure_defs.join("");
        Ok(main.replace("/* @@LATE@@ */", &defs))
    }

    fn intern_enum(&mut self, name: &str, variants: Vec<(String, usize)>) -> u32 {
        if let Some(&id) = self.enum_by_name.get(name) { return id; }
        let id = self.enums.len() as u32;
        self.enums.push(EnumInfo { _name: name.to_string(), variants });
        self.enum_by_name.insert(name.to_string(), id);
        id
    }

    fn intern_cell(&mut self, inner: VTy, is_mutex: bool) -> u32 {
        // 同 (内层, 锁型) 共享一个单元格 C 类型
        if let Some((pos, _)) = self.cells.iter().enumerate().find(|(_, (t, m))| *t == inner && *m == is_mutex) {
            return pos as u32;
        }
        let id = self.cells.len() as u32;
        self.cells.push((inner, is_mutex));
        id
    }

    /// 二元组实例:同成员类型共享 typedef
    fn intern_tuple(&mut self, a: VTy, b: VTy) -> u32 {
        let key = format!("{:?}|{:?}", a, b);
        if let Some(&id) = self.tuple_keys.get(&key) { return id; }
        let id = self.tuples.len() as u32;
        self.tuples.push((a, b));
        self.tuple_keys.insert(key, id);
        let ca = self.c_ty(a);
        let cb = if a == b { ca.to_string() } else { self.c_ty(b).to_string() };
        let na: String = ca.chars().map(|c| if c.is_alphanumeric() { c } else { '_' }).collect();
        self.late_defs.push(format!("typedef struct {{ {} _0; {} _1; }} ct_val2_{};\n", ca, cb, na));
        id
    }

    fn emit_typedef(&mut self, t: &TypeInfo) {
        let fs: Vec<String> = t.fields.iter()
            .map(|(n, ty)| format!("    {} {};", self.c_ty(*ty), n))
            .collect();
        self.late_defs.push(format!("typedef struct {{\n{}\n}} ctn_{};\n", fs.join("\n"), t.name));
    }

    /// 泛型字面量实例化:按字段值类型推断实参 → 具体实例 tid
    fn instantiate_generic(&mut self, name: &str, field_vals: &[(String, VTy)]) -> Result<u32, String> {
        let Some((is_class, decl_fields)) = self.generics.get(name).cloned() else {
            return Err(format!("trans:非泛型类型 `{}`", name));
        };
        // 参数名 → 具体类型(按 decl 字段顺序对应值类型)
        let mut binds: Vec<(String, VTy)> = Vec::new();
        for (fname, fty) in &decl_fields {
            let vt = field_vals.iter().find(|(n, _)| n == fname).map(|(_, t)| *t);
            if let (ast::Type::Named { path, .. }, Some(vt)) = (&fty, vt) {
                if path.len() == 1 {
                    binds.push((path[0].clone(), vt));
                }
            }
        }
        let key = format!("{}<{}>", name, binds.iter().map(|(_, t)| format!("{:?}", t)).collect::<Vec<_>>().join(","));
        if let Some(&tid) = self.mono_keys.get(&key) {
            return Ok(tid);
        }
        let iname = format!("{}_{}", name, self.mono_keys.len());
        let tid = self.intern_type(&iname, is_class);
        self.mono_keys.insert(key.clone(), tid);
        let mut fs = Vec::new();
        for (fname, fty) in &decl_fields {
            let ct = self.subst_ty(fty, &binds)?;
            fs.push((fname.clone(), ct));
        }
        self.types[tid as usize].fields = fs;
        self.emit_typedef(&self.types[tid as usize].clone());
        Ok(tid)
    }

    /// 类型替换:类型参数名 → 具体类型
    fn subst_ty(&mut self, t: &ast::Type, binds: &[(String, VTy)]) -> Result<VTy, String> {
        if let ast::Type::Named { path, .. } = t {
            if let Some(seg) = path.last() {
                for (pn, pt) in binds {
                    if pn == seg { return Ok(*pt); }
                }
                if let Some(&id) = self.type_by_name.get(seg.as_str()) {
                    let is_class = self.types[id as usize].is_class;
                    return Ok(if is_class { VTy::Class(id) } else { VTy::Struct(id) });
                }
            }
        }
        Ok(self.ty_of(t))
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

    fn field_ty(&mut self, t: &ast::Type) -> Result<VTy, String> {
        if let ast::Type::Named { path, .. } = t {
            if let Some(name) = path.last() {
                if let Some(v) = scalar_annotation(name) { return Ok(v); }
                if matches!(name.as_str(), "Atomic" | "Global" | "Chan") {
                    let cid = self.intern_cell(VTy::Int(None), false);
                    return Ok(VTy::Cell(cid));
                }
                if let Some(&id) = self.type_by_name.get(name.as_str()) {
                    let is_class = self.types[id as usize].is_class;
                    return Ok(if is_class { VTy::Class(id) } else { VTy::Struct(id) });
                }
                return Err(format!("trans v1 拒绝域:字段类型 `{}`(仅数值/Bool/Str/用户类型)", name));
            }
        }
        Err("trans v1 拒绝域:字段类型形态".into())
    }

    /// 模式编译 → (条件头如 `if (..) {`, 绑定语句);无条件头 = 通配/解构(裸 `{`)
    fn pat_arm(&mut self, pat: &ast::Pattern, mv: &str, scrut: VTy) -> Result<(String, Vec<String>), String> {
        match pat {
            ast::Pattern::Wildcard => Ok(("1".to_string(), vec![])),
            ast::Pattern::Agg { path, sub } => {
                let vname = path.last().cloned().unwrap_or_default();
                // struct 解构模式:绑定字段值
                if let VTy::Struct(tid) = scrut {
                    let ast::AggSub::Struct(fs) = sub else {
                        return Err("trans:struct 模式需字段子模式".into());
                    };
                    let mut binds = Vec::new();
                    for f in fs {
                        let fty = self.types[tid as usize].fields.iter()
                            .find(|(n, _)| *n == f.name).map(|(_, t)| *t)
                            .ok_or_else(|| format!("trans:无字段 `{}`", f.name))?;
                        let fname = f.pattern.as_ref().is_none();
                        let pname = match &f.pattern {
                            Some(ast::Pattern::Ident(n)) => n.clone(),
                            None => f.name.clone(),
                            _ => return Err("trans v1 拒绝域:嵌套字段模式".into()),
                        };
                        let cname = self.bind(&pname, fty);
                        binds.push(format!("{} {} = ({}).{};", self.c_ty(fty), cname, mv, f.name));
                        let _ = fname;
                    }
                    return Ok(("1".to_string(), binds));
                }
                // 和类型变体模式
                let Some((_eid, vid)) = self.find_variant(&vname) else {
                    return Err(format!("trans:未知变体 `{}`", vname));
                };
                let pay = match scrut { VTy::Sum(_, p) => p, _ => Pay::I };
                let mut conds = vec![format!("({}).variant == {}", mv, vid)];
                let mut binds = Vec::new();
                if let ast::AggSub::Tuple(ps) = sub {
                    let slot = if pay == Pay::F { "f" } else { "i" };
                    let ctype = if pay == Pay::F { "double" } else { "ct_i" };
                    let st = if pay == Pay::F { VTy::F64 } else { VTy::Int(None) };
                    for (i, sp) in ps.iter().enumerate() {
                        match sp {
                            ast::Pattern::Ident(n) => {
                                // Err 载荷绑定 = 错误对象指针(context 链约定)
                                if vname == "Err" {
                                    let cname = self.bind(n, VTy::ErrPtr);
                                    binds.push(format!("ct_i {} = ({}).p[{}].i;", cname, mv, i));
                                    continue;
                                }
                                let cname = self.bind(n, st);
                                binds.push(format!("{} {} = ({}).p[{}].{};", ctype, cname, mv, i, slot));
                            }
                            ast::Pattern::Wildcard => {}
                            // 嵌套无参变体(如 Err(DivByZero)):变体号相等并入头条件
                            ast::Pattern::Agg { path, sub: ast::AggSub::Unit } => {
                                let nested = path.last().cloned().unwrap_or_default();
                                let Some((_, nvid)) = self.find_variant(&nested) else {
                                    return Err(format!("trans:未知变体 `{}`", nested));
                                };
                                conds.push(format!("((ct_i)({}).p[{}].i) == {}", mv, i, nvid));
                            }
                            _ => return Err("trans v1 拒绝域:载荷子模式".into()),
                        }
                    }
                }
                Ok((conds.join(" && "), binds))
            }
            ast::Pattern::Lit(l) => {
                // 整数/布尔字面量模式(scrutinee 为 Int/Bool)
                let lit = match l {
                    crate::ast::PatLit::Int(t) => {
                        let cleaned = t.replace('_', "");
                        format!("(ct_i){}", cleaned)
                    }
                    crate::ast::PatLit::Bool(b) => format!("{}", *b as i32),
                    _ => return Err("trans v1 拒绝域:该字面量模式".into()),
                };
                Ok((format!("({}) == ({})", mv, lit), vec![]))
            }
            _ => Err("trans v1 拒绝域:该模式形态".into()),
        }
    }

    /// 类型 tname 的 impl 中,声明方法 m 的 trait 名
    fn trait_of_method(&self, tname: &str, m: &str) -> Option<String> {
        for (tn, ft) in &self.impls {
            if ft != tname { continue; }
            for d in &self.decls {
                if let ast::Decl::Trait(tr) = d {
                    if &tr.name == tn && tr.items.iter().any(|it| matches!(it,
                        crate::ast::TraitItem::Method(mm) if mm.name == m)) { return Some(tr.name.clone()); }
                }
            }
        }
        None
    }

    /// impl 方法的返回类型(impl 项或 trait 默认声明)
    fn impl_ret_ty(&mut self, tname: &str, m: &str) -> VTy {
        let impls_snap = self.impls.clone();
        for (tn, ft) in impls_snap.iter() {
            if ft != tname { continue; }
            let decls_snap = self.decls.clone();
            for d in &decls_snap {
                let mut ret = None;
                if let ast::Decl::Impl(im) = d {
                    let itn = named_tail_pub(&im.trait_ty);
                    let ift = named_tail_pub(&im.for_ty);
                    if &ift == tname {
                        for item in &im.items {
                            match item {
                                ast::ImplItem::Method(mm) => {
                                    if mm.name == m { ret = mm.ret.as_ref().map(|t| self.ty_of(t)); }
                                }
                                ast::ImplItem::Prop(pp) => {
                                    if pp.name == m { ret = Some(self.ty_of(&pp.ty)); }
                                }
                            }
                        }
                    }
                    let _ = itn;
                }
                if let Some(r) = ret { return r; }
                if let ast::Decl::Trait(tr) = d {
                    if &tr.name == tn {
                        for it in &tr.items {
                            match it {
                                crate::ast::TraitItem::Method(mm) => {
                                    if mm.name == m { if let Some(r) = mm.ret.as_ref().map(|t| self.ty_of(t)) { return r; } }
                                }
                                crate::ast::TraitItem::PropSig(pp) => {
                                    if pp.name == m { return self.ty_of(&pp.ty); }
                                }
                                crate::ast::TraitItem::PropImpl(pp) => {
                                    if pp.name == m { return self.ty_of(&pp.ty); }
                                }
                            }
                        }
                    }
                }
            }
        }
        VTy::Unknown
    }

    fn trait_of_prop(&self, tname: &str, m: &str) -> Option<String> {
        for (tn, ft) in &self.impls {
            if ft != tname { continue; }
            for d in &self.decls {
                if let ast::Decl::Trait(tr) = d {
                    if &tr.name == tn && tr.items.iter().any(|it| match it {
                        crate::ast::TraitItem::PropSig(pp) => pp.name == m,
                        crate::ast::TraitItem::PropImpl(pp) => pp.name == m,
                        _ => false,
                    }) { return Some(tr.name.clone()); }
                }
            }
        }
        None
    }

    /// 变体名 → (enum id, variant index)
    fn find_variant(&self, name: &str) -> Option<(u32, usize)> {
        for (eid, e) in self.enums.iter().enumerate() {
            if let Some((vi, _)) = e.variants.iter().enumerate().find(|(_, (vn, _))| vn == name) {
                return Some((eid as u32, vi));
            }
        }
        None
    }

    /// 字段访问基串(含分隔符):struct 值用 `.`,class/Boxed 指针用 `->`
    fn deref_obj(&self, c: &str, ty: VTy) -> Result<(String, u32), String> {
        match ty {
            VTy::Struct(id) => Ok((format!("({}).", c), id)),
            VTy::Class(id) | VTy::Boxed(id) => Ok((format!("({})->", c), id)),
            other => Err(format!("trans:字段访问需用户类型(接收者 {:?})", other)),
        }
    }

    fn pay_of(&mut self, t: &ast::Type) -> Pay {
        if self.ty_of(t).is_float() { Pay::F } else { Pay::I }
    }

    fn ty_of(&mut self, t: &ast::Type) -> VTy {
        match t {
            ast::Type::Named { path, .. } => {
                if let Some(name) = path.last() {
                    if let Some(v) = scalar_annotation(name) { return v; }
                    if let Some(&id) = self.type_by_name.get(name.as_str()) {
                        let is_class = self.types[id as usize].is_class;
                        return if is_class { VTy::Class(id) } else { VTy::Struct(id) };
                    }
                    if let Some(pos) = self.traits.iter().position(|t| t == name) {
                        return VTy::TraitObj(pos as u32);
                    }
                    if name == "Arena" { return VTy::Arena; }
                    if name == "List" { return VTy::Array; }
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
            ast::Type::Fn { .. } => VTy::FnPtr,
            ast::Type::Tuple(items) if items.len() == 2 => {
                let a = self.ty_of(&items[0]);
                let b = self.ty_of(&items[1]);
                if matches!(a, VTy::Unknown) || matches!(b, VTy::Unknown) {
                    return VTy::Unknown;
                }
                VTy::Tup(self.intern_tuple(a, b))
            }
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
            VTy::Sum(..) | VTy::SumErr(..) => "ct_sum",
            VTy::FnPtr => "ct_fnptr0",
            VTy::Arena => "ct_arr*",
            VTy::Cell(cid) => {
                let (inner, is_mutex) = self.cells[cid as usize];
                if is_mutex {
                    let inner_ct = self.c_ty(inner).to_string();
                    let san: String = inner_ct.chars().map(|c| if c.is_alphanumeric() { c } else { '_' }).collect();
                    let name = format!("ct_mcell_{}", san);
                    let td = format!("typedef struct {{ pthread_mutex_t mu; {} v; }} {};\n", inner_ct, name);
                    {
                        let mut q = self.mcell_typedefs.borrow_mut();
                        if !q.iter().any(|x| x.contains(&format!("}} {};", name))) {
                            q.push(td);
                        }
                    }
                    return leak_str(format!("{}*", name));
                }
                "ct_i*"
            }
            VTy::Simd => "ct_simd",
            VTy::Task => "ct_task*",
            VTy::ScopeH => "ct_scope*",
            VTy::Chan => "ct_chan*",
            VTy::Void => "void",
            VTy::FArr => "ct_farr*",
            VTy::TraitObj(..) => "ct_i",
            VTy::Tup(id) => {
                let c = self.c_ty(self.tuples[id as usize].0).to_string();
                let n: String = c.chars().map(|ch| if ch.is_alphanumeric() { ch } else { '_' }).collect();
                return Box::leak(format!("ct_val2_{}", Box::leak(n.into_boxed_str())).into_boxed_str());
            }
            VTy::ErrPtr => "ct_i",
            VTy::Struct(id) => leak_str(format!("ctn_{}", self.types[id as usize].name)),
            VTy::Class(id) | VTy::Boxed(id) => leak_str(format!("ctn_{}*", self.types[id as usize].name)),
            VTy::Int(_) | _ => "ct_i",
        }
    }

    // ---------------- 函数 ----------------

    /// 收集表达式中的自由 Ident 名(粗粒度,语料级)
    fn collect_idents_expr(&self, e: &ast::Expr, out: &mut Vec<String>) {
        use ast::Expr as E;
        match e {
            E::Ident(n) => out.push(n.clone()),
            E::Unary { expr, .. } | E::Try(expr) | E::TypeArgs { expr, .. } => self.collect_idents_expr(expr, out),
            E::Binary { lhs, rhs, .. } => { self.collect_idents_expr(lhs, out); self.collect_idents_expr(rhs, out); }
            E::Call { callee, args } => { self.collect_idents_expr(callee, out); for a in args { self.collect_idents_expr(a, out); } }
            E::Member { obj, .. } => self.collect_idents_expr(obj, out),
            E::Index { obj, index } => { self.collect_idents_expr(obj, out); self.collect_idents_expr(index, out); }
            E::Array(items) | E::Tuple(items) => for i in items { self.collect_idents_expr(i, out); },
            E::If { cond, then, els } => {
                self.collect_idents_expr(cond, out);
                self.collect_idents_block(then, out);
                if let Some(x) = els { self.collect_idents_expr(x, out); }
            }
            E::BlockExpr(b) => self.collect_idents_block(b, out),
            E::Match { expr, arms } => {
                self.collect_idents_expr(expr, out);
                for a in arms {
                    if let ast::Pattern::Agg { sub: ast::AggSub::Tuple(ps), .. } = &a.pattern {
                        for p in ps {
                            if let ast::Pattern::Ident(n) = p { out.push(n.clone()); }
                        }
                    }
                    self.collect_idents_expr(&a.expr, out);
                }
            }
            E::Range { from, to, .. } => { self.collect_idents_expr(from, out); self.collect_idents_expr(to, out); }
            E::StructLit { fields, .. } => for f in fields {
                if let Some(v) = &f.value { self.collect_idents_expr(v, out); }
            },
            _ => {}
        }
    }
    fn collect_idents_block(&self, b: &ast::Block, out: &mut Vec<String>) {
        for st in &b.stmts { self.collect_idents_stmt(st, out); }
        if let Some(t) = &b.tail { self.collect_idents_expr(t, out); }
    }
    fn collect_idents_stmt(&self, st: &ast::Stmt, out: &mut Vec<String>) {
        match st {
            ast::Stmt::Let { expr, .. } => self.collect_idents_expr(expr, out),
            ast::Stmt::Expr(e) | ast::Stmt::Return(Some(e)) => self.collect_idents_expr(e, out),
            ast::Stmt::Assign { target, value, .. } => {
                if let ast::Expr::Ident(n) = target { out.push(n.clone()); }
                self.collect_idents_expr(value, out);
            }
            ast::Stmt::While { cond, body } => { self.collect_idents_expr(cond, out); self.collect_idents_block(body, out); }
            ast::Stmt::For { iter, body, .. } => { self.collect_idents_expr(iter, out); self.collect_idents_block(body, out); }
            _ => {}
        }
    }

    /// Mutex 单元格 C 类型名
    fn mcell_ty(&self, cid: u32) -> String {
        let (inner, _) = self.cells[cid as usize];
        let inner_ct = self.c_ty(inner).to_string();
        let san: String = inner_ct.chars().map(|c| if c.is_alphanumeric() { c } else { '_' }).collect();
        format!("ct_mcell_{}", san)
    }

    /// extern "c" 的真实 ABI 类型(C 原生宽度,非 i128 载体)
    fn abi_ty(&self, t: VTy) -> String {
        match t {
            VTy::Int(Some((IntW::W8, false))) => "uint8_t".into(),
            VTy::Int(Some((IntW::W16, false))) => "uint16_t".into(),
            VTy::Int(Some((IntW::W32, false))) => "uint32_t".into(),
            VTy::Int(Some((IntW::W64, false))) | VTy::Int(Some((IntW::WSize, false))) => "uint64_t".into(),
            VTy::Int(Some((IntW::W8, true))) => "int8_t".into(),
            VTy::Int(Some((IntW::W16, true))) => "int16_t".into(),
            VTy::Int(Some((IntW::W32, true))) => "int32_t".into(),
            VTy::Int(Some((IntW::W64, true))) | VTy::Int(Some((IntW::WSize, true))) | VTy::Int(None) => "int64_t".into(),
            VTy::F64 => "double".into(),
            VTy::F32 => "float".into(),
            VTy::Bool => "int".into(),
            VTy::Str => "char*".into(),
            _ => "ct_i".into(),
        }
    }

    fn fn_has_traitobj_param(&self, name: &str) -> bool {
        for d in &self.decls {
            if let ast::Decl::Fn(f) = d {
                if f.name != name || !f.type_params.is_empty() { continue; }
                for p in &f.params {
                    if let ast::Param::Param { ty, .. } = p {
                        let seg = match ty {
                            ast::Type::Named { path, .. } => path.last(),
                            ast::Type::Ref(inner) => match &**inner {
                                ast::Type::Named { path, .. } => path.last(),
                                _ => None,
                            },
                            _ => None,
                        };
                        if let Some(seg) = seg {
                            if seg != "List" && self.traits.iter().any(|t| t == seg) { return true; }
                        }
                    }
                }
            }
        }
        false
    }

    fn fn_is_generic(&self, name: &str) -> bool {
        self.decls.iter().any(|d| matches!(d, ast::Decl::Fn(f) if f.name == name && !f.type_params.is_empty()))
    }

    /// 泛型 fn 单态:按实参具体类型生成副本
    fn mono_fn(&mut self, name: &str, atys: &[VTy]) -> Result<(String, VTy), String> {
        let key = format!("{}<{}>", name, atys.iter().map(|t| format!("{:?}", t)).collect::<Vec<_>>().join(","));
        if let Some((c, r)) = self.mono_names.get(&key) {
            return Ok((c.clone(), *r));
        }
        self.mono_seq += 1;
        let cname = format!("{}_mono_{}", sanitize(name), self.mono_seq);
        self.mono_names.insert(key.clone(), (cname.clone(), VTy::Unknown));
        let f = self.decls.iter().find_map(|d| match d {
            ast::Decl::Fn(f) if f.name == name => Some(f.clone()),
            _ => None,
        }).ok_or("trans:泛型函数未找到")?;
        // 返回类型替换:类型参数(如 (T,T))→ 实参元组(或值)的成员类型,按位对应
        let mut ret = f.ret.as_ref().map(|t| self.ty_of(t)).unwrap_or(VTy::Void);
        if let Some(ast::Type::Tuple(items)) = &f.ret {
            if items.len() == 2 {
                // 实参里的元组:其成员即类型参数的具体类型
                let tup_members = atys.iter().find_map(|t| match t {
                    VTy::Tup(id) => Some(self.tuples[*id as usize]),
                    _ => None,
                });
                let mut mem = Vec::new();
                for (i, it) in items.iter().enumerate() {
                    let mut mt = self.ty_of(it);
                    if let ast::Type::Named { path, .. } = it {
                        if path.len() == 1 && mt == VTy::Unknown {
                            if let Some((m0, m1)) = tup_members {
                                mt = if i == 0 { m0 } else { m1 };
                            } else {
                                mt = atys.first().copied().unwrap_or(VTy::Unknown);
                            }
                        }
                    }
                    mem.push(mt);
                }
                if mem.iter().all(|t| !matches!(t, VTy::Unknown)) {
                    ret = VTy::Tup(self.intern_tuple(mem[0], mem[1]));
                }
            }
        }
        let _ = key;
        self.scope_push();
        let mut parts = Vec::new();
        for (p, at) in f.params.iter().zip(atys) {
            if let ast::Param::Param { name: pn, .. } = p {
                let c = self.bind(pn, *at);
                parts.push(format!("{} {}", self.c_ty(*at), c));
            }
        }
        self.closure_defs.push(format!("static {} {}({}) {{\n", self.c_ty(ret), cname, parts.join(", ")));
        if let Some(body) = &f.body {
            self.sink.push(String::new());
            self.emit_block_stmts(body)?;
            let code = self.sink.pop().unwrap_or_default();
            for line in code.lines() {
                let last = self.closure_defs.last_mut().unwrap();
                last.push_str("    ");
                last.push_str(line);
                last.push('\n');
            }
        }
        self.scope_pop();
        self.closure_defs.push("}\n".into());
        if let Some(e) = self.mono_names.get_mut(&key) {
            e.1 = ret;
        }
        Ok((cname, ret))
    }

    /// s.spawn(闭包):捕获分析 → env 结构体 + shim → ct_spawn
    fn emit_spawn(&mut self, scope_c: String, args: &[ast::Expr]) -> TRes {
        let Some(a) = args.first() else { return Err("trans:spawn 需闭包".into()) };
        let ast::Expr::Closure { params, body, .. } = a else {
            return Err("trans v1 拒绝域:spawn 需闭包字面量".into());
        };
        // 捕获分析:屏蔽参数后收集自由名,解析到外层者即捕获
        self.scope_push();
        for p in params { self.bind(&p.name, VTy::Int(None)); }
        let mut names = Vec::new();
        if let ast::Expr::BlockExpr(b) = body.as_ref() { self.collect_idents_block(b, &mut names); }
        else { self.collect_idents_expr(body, &mut names); }
        self.scope_pop();
        let mut caps: Vec<(String, VTy, String)> = Vec::new();
        let mut seen = std::collections::HashSet::new();
        for n in names {
            if seen.insert(n.clone()) {
                if self.lookup_fn(&n).is_some() { continue; }
                if self.find_variant(&n).is_some() { continue; }
                if let Some((cn, ty)) = self.lookup(&n) {
                    caps.push((n, ty, cn));
                }
            }
        }
        self.mono_seq += 1;
        let ename = format!("ct_env_{}", self.mono_seq);
        let shim = format!("ct_shim_{}", self.mono_seq);
        let fname_of = |n: &str| format!("f_{}", n);
        // env typedef
        let mut fields = vec!["ct_scope* scope".to_string()];
        for (n, ty, _) in &caps {
            fields.push(format!("{} {}", self.c_ty(*ty), fname_of(n)));
        }
        self.late_defs.push(format!("typedef struct {{ {} }} {};\n", fields.join("; "), ename));
        // shim
        self.scope_push();
        let mut unpack = vec![format!("{}* e = ({}*)envp;", ename, ename)];
        for (n, ty, _) in &caps {
            let c = self.bind(n, *ty);
            unpack.push(format!("{} {} = e->{};", self.c_ty(*ty), c, fname_of(n)));
        }
        for p in params { self.bind(&p.name, VTy::Int(None)); }
        let mut head = format!("static void* {}(void* envp) {{\n", shim);
        for l in &unpack { head.push_str("    "); head.push_str(l); head.push('\n'); }
        self.closure_defs.push(head);
        self.sink.push(String::new());
        let mut result_line = None;
        match body.as_ref() {
            ast::Expr::BlockExpr(b) => {
                self.scope_push();
                for st in &b.stmts { self.emit_stmt(st)?; }
                if let Some(t) = &b.tail {
                    let (c, ty) = self.expr(t)?;
                    if matches!(ty, VTy::Int(_) | VTy::Bool) {
                        result_line = Some(c);
                    } else {
                        self.w(2, &format!("(void)({});", c));
                    }
                }
                self.scope_pop();
            }
            other => {
                let (c, ty) = self.expr(other)?;
                if matches!(ty, VTy::Int(_) | VTy::Bool) {
                    result_line = Some(c);
                } else {
                    self.w(2, &format!("(void)({});", c));
                }
            }
        }
        let code = self.sink.pop().unwrap_or_default();
        for line in code.lines() {
            let last = self.closure_defs.last_mut().unwrap();
            last.push_str("    "); last.push_str(line); last.push('\n');
        }
        self.scope_pop();
        if let Some(rc_) = result_line {
            self.closure_defs.push(format!("    ct_tls_task->result = (ct_i)({});\n", rc_));
        }
        self.closure_defs.push("    return 0;\n}\n".into());
        // spawn 调用点:env 实例
        let mut inits = vec![format!("e->scope = ({});", scope_c)];
        for (n, _ty, cn) in &caps {
            inits.push(format!("e->{} = ({});", fname_of(n), cn));
        }
        Ok((
            format!("({{ {}* e = malloc(sizeof({})); {} ct_task* ct_t = ct_spawn({}, e, ({})); ct_t; }})",
                ename, ename, inits.join(" "), shim, scope_c),
            VTy::Task,
        ))
    }

    /// 每具体类型一个 show 函数:Name{f: v, ...}(对齐 interp show_value)
    fn show_fn_for(&mut self, tid: u32) -> Result<String, String> {
        let key = format!("show{}", tid);
        if let Some((c, _)) = self.mono_names.get(&key) {
            return Ok(c.clone());
        }
        let t = self.types[tid as usize].clone();
        let cname = format!("ctn_{}_show", t.name);
        self.mono_names.insert(key, (cname.clone(), VTy::Str));
        let is_class = t.is_class;
        let self_cty = if is_class {
            let c = self.c_ty(VTy::Class(tid)).to_string();
            c
        } else {
            let c = self.c_ty(VTy::Struct(tid)).to_string();
            c
        };
        let mut parts = vec![format!("\"{}{{\"", t.name)];
        for (i, (fname, fty)) in t.fields.iter().enumerate() {
            if i > 0 { parts.push("\", \"".into()); }
            parts.push(format!("\"{}: \"", fname));
            let acc = if is_class { format!("self->{}", fname) } else { format!("self.{}", fname) };
            parts.push(self.show_expr_for(acc, *fty)?);
        }
        parts.push("\"}\"".into());
        let body = parts.join(", ");
        let sig = format!("static char* {}({} self) {{", cname, self_cty);
        self.late_defs.push(format!("{} return ct_cat({}, {}); }}\n", sig, parts.len(), body));
        Ok(cname)
    }

    /// 字段值字符串化表达式
    fn show_expr_for(&mut self, acc: String, ty: VTy) -> Result<String, String> {
        Ok(match ty {
            VTy::Int(_) => format!("ct_i128_str({})", acc),
            VTy::Bool => format!("ct_bool_str((int)(!!({})))", acc),
            VTy::F64 => format!("ct_f64_str({})", acc),
            VTy::F32 => format!("ct_f32_str({})", acc),
            VTy::Str => acc,
            VTy::Struct(st) => format!("{}({})", self.show_fn_for(st)?, acc),
            VTy::Class(ct) | VTy::Boxed(ct) => format!("{}({})", self.show_fn_for(ct)?, acc),
            _ => return Err("trans v1 拒绝域:show 字段类型".into()),
        })
    }

    /// impl 方法:ctn_<Trait>_<name>(self, ...)
    fn emit_impl_fn(&mut self, tn: &str, _ft: &str, tid: Option<u32>, name: &str, m: &ast::FnDecl) -> Result<(), String> {
        let ret = m.ret.as_ref().map(|t| self.ty_of(t)).unwrap_or(VTy::Void);
        let self_ty = match tid {
            Some(id) => if self.types[id as usize].is_class { VTy::Class(id) } else { VTy::Struct(id) },
            None => VTy::Unknown,
        };
        self.scope_push();
        let mut parts = vec![format!("{} self", self.c_ty(self_ty))];
        if let Some(sid) = tid {
            let cself = self.c_ty(self_ty).to_string();
            let key = format!("S{}|C{}", sid, sid);
            let _ = key;
            self.scopes.last_mut().unwrap().push(("self".into(), "self".into(), self_ty));
            let _ = cself;
        } else {
            self.scopes.last_mut().unwrap().push(("self".into(), "self".into(), self_ty));
        }
        for p in &m.params {
            if let ast::Param::Param { name: pn, ty, .. } = p {
                let vty = self.ty_of(ty);
                let c = self.bind(pn, vty);
                parts.push(format!("{} {}", self.c_ty(vty), c));
            }
        }
        self.late_defs.push(format!("static {} ctn_{}_{}({});\n", self.c_ty(ret), tn, name, parts.join(", ")));
        self.w(0, &format!("static {} ctn_{}_{}({}) {{", self.c_ty(ret), tn, name, parts.join(", ")));
        if let Some(body) = &m.body {
            self.emit_block_stmts(body)?;
        }
        self.scope_pop();
        self.w(0, "}");
        Ok(())
    }

    /// impl prop:取值函数 ctn_<Trait>_<name>(self)
    fn emit_impl_prop(&mut self, tn: &str, _ft: &str, tid: Option<u32>, pp: &ast::PropDecl) -> Result<(), String> {
        let ret = self.ty_of(&pp.ty);
        let self_ty = match tid {
            Some(id) => if self.types[id as usize].is_class { VTy::Class(id) } else { VTy::Struct(id) },
            None => VTy::Unknown,
        };
        self.scope_push();
        self.scopes.last_mut().unwrap().push(("self".into(), "self".into(), self_ty));
        self.late_defs.push(format!("static {} ctn_{}_{}({} self);\n", self.c_ty(ret), tn, pp.name, self.c_ty(self_ty)));
        self.w(0, &format!("static {} ctn_{}_{}({} self) {{", self.c_ty(ret), tn, pp.name, self.c_ty(self_ty)));
        if let Some(body) = &pp.body {
            self.emit_block_stmts(body)?;
        }
        self.scope_pop();
        self.w(0, "}");
        Ok(())
    }

    fn emit_fn(&mut self, f: &ast::FnDecl) -> Result<(), String> {
        let ret = self.lookup_fn(&f.name).map(|(_, r)| r).unwrap_or(VTy::Void);
        let saved = self.ret_anyerr;
        // 返回类型含 AnyError → `?` 记录调用点并自动擦除(§5.3)
        self.ret_anyerr = f.ret.as_ref().map(|t| match t {
            ast::Type::Named { path, args, .. } => {
                path.iter().chain(args.iter().flat_map(|a| match a {
                    ast::Type::Named { path: p, .. } => p.iter(),
                    _ => [].iter(),
                })).any(|seg| seg == "AnyError")
            }
            _ => false,
        }).unwrap_or(false);
        self.scope_push();
        let mut parts = Vec::new();
        for p in &f.params {
            if let ast::Param::Param { name, ty, .. } = p {
                let vty = self.ty_of(ty);
                let c = self.bind(name, vty);
                parts.push(format!("{} {}", self.c_ty(vty), c));
            }
        }
        let proto = format!("static {} {}({});\n", self.c_ty(ret), sanitize(&f.name), parts.join(", "));
        self.late_defs.push(proto.clone());
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
        self.ret_anyerr = saved;
        self.w(0, "}");
        Ok(())
    }

    // ---------------- 语句 ----------------

    /// RAII:当前层按声明逆序对有 Drop impl 的值发射析构调用(§6.4)
    fn emit_scope_drops(&mut self) {
        if let Some(layer) = self.scopes.last() {
            let layer = layer.clone();
            let drops: Vec<String> = layer.iter().rev().filter_map(|(_n, cname, ty)| {
                if let VTy::Struct(tid) = ty {
                    let tname = self.types[*tid as usize].name.clone();
                    if self.impls.iter().any(|(tn, ft)| tn == "Drop" && ft == &tname) {
                        return Some(format!("    ctn_Drop_{}_drop({});", tname, cname));
                    }
                }
                None
            }).collect();
            for d in drops { self.w(1, &d); }
        }
    }

    fn emit_block_stmts(&mut self, block: &ast::Block) -> Result<(), String> {
        self.scope_push();
        for s in &block.stmts {
            self.emit_stmt(s)?;
        }
        if let Some(t) = &block.tail {
            let (c, _) = self.expr(t)?;
            self.w(1, &format!("(void)({});", c));
        }
        self.emit_scope_drops();
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
                if let ast::Pattern::Tuple(ps) = pattern {
                    let (c, ty) = self.expr(expr)?;
                    let VTy::Tup(id) = ty else { return Err("trans:解构需元组".into()) };
                    let (ta, tb) = self.tuples[id as usize];
                    // 构造表达式只求值一次:先提升临时(通道等副作用构造器必须单次)
                    let tmp = self.uniq_name("tup");
                    self.w(1, &format!("{} {} = ({});", self.c_ty(ty), tmp, c));
                    for (i, sp) in ps.iter().enumerate() {
                        let ast::Pattern::Ident(n) = sp else {
                            return Err("trans v1 拒绝域:嵌套解构".into());
                        };
                        let (ft, f) = if i == 0 { (ta, "_0") } else { (tb, "_1") };
                        let cname = self.bind(n, ft);
                        self.w(1, &format!("{} {} = {}.{};", self.c_ty(ft), cname, tmp, f));
                    }
                    return Ok(());
                }
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
                // 仅整数载体加 (ct_i);Str/Bool/浮点直接赋值(D4/T2 修复:Str 重赋值此前必坏)
                let cast = if v_vty.is_num() || v_vty == VTy::Unknown { "(ct_i)" } else { "" };
                self.w(1, &format!("{} = {}({});", c, cast, rhs.0));
                Ok(())
            }
            ast::Stmt::Return(e) => {
                match e {
                    Some(e) => {
                        let (c, ty) = self.expr(e)?;
                        if matches!(ty, VTy::Void | VTy::Unknown) && c == "0" {
                            self.w(1, "return;");
                        } else {
                            self.w(1, &format!("return {};", c));
                        }
                    }
                    None => { self.w(1, "return;"); }
                }
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
                let name_str = match pattern {
                    ast::Pattern::Ident(n) => n.clone(),
                    ast::Pattern::Wildcard => "_".to_string(),
                    _ => return Err("trans v1 拒绝域:for 非 Ident 模式".into()),
                };
                let name = name_str.as_str();
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
                // 用户函数引用(函数一等公民,如 opt.map(twice))
                if self.lookup_fn(name).is_some() {
                    return Ok((sanitize(name), VTy::FnPtr));
                }
                // 无参变体作为值表达式(如 `return Stop`)
                if let Some((eid, vid)) = self.find_variant(name) {
                    if self.enums[eid as usize].variants[vid as usize].1 == 0 {
                        return Ok((format!("(ct_sum){{ {}, {{ {{.i = 0}}, {{.i = 0}}, {{.i = 0}}, {{.i = 0}} }} }}", vid), VTy::Sum(eid, Pay::I)));
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
                    // 无值 if:纯语句形态;分支尾表达式(副作用调用)仍需发射
                    self.w(1, &format!("if ({}) {{", cc));
                    self.emit_lines(&then_code);
                    if let Some((tc_, _)) = then_tail {
                        self.w(2, &format!("(void)({});", tc_));
                    }
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
                // 尾表达式必须在 drops 之前于块内求值:void 尾就地发射;
                // 值尾先落临时变量,使用点引用(drops 不得吞掉副作用或值)
                self.scope_push();
                for st in &b.stmts { self.emit_stmt(st)?; }
                match &b.tail {
                    Some(t) => {
                        let (c, ty) = self.expr(t)?;
                        if matches!(ty, VTy::Void | VTy::Unknown) {
                            self.w(1, &format!("(void)({});", c));
                            self.emit_scope_drops();
                            self.scope_pop();
                            return Ok(("0".into(), VTy::Void));
                        }
                        let tv = self.uniq_name("blk");
                        self.w(1, &format!("{} {} = ({});", self.c_ty(ty), tv, c));
                        self.emit_scope_drops();
                        self.scope_pop();
                        return Ok((tv, ty));
                    }
                    None => {
                        self.emit_scope_drops();
                        self.scope_pop();
                        Ok(("0".into(), VTy::Void))
                    }
                }
            }
            ast::Expr::Tuple(items) => {
                if items.len() != 2 { return Err("trans v1 拒绝域:非二元组".into()); }
                let mut tys: Vec<VTy> = Vec::new();
                let mut cs = Vec::new();
                for i in items {
                    let (c, t) = self.expr(i)?;
                    cs.push(c);
                    tys.push(t);
                }
                let id = self.intern_tuple(tys[0], tys[1]);
                let tn: String = self.c_ty(tys[0]).chars().map(|c| if c.is_alphanumeric() { c } else { '_' }).collect();
                Ok((format!("(ct_val2_{}){{ {}, {} }}", Box::leak(tn.into_boxed_str()), cs[0], cs[1]), VTy::Tup(id)))
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
            ast::Expr::Member { obj, target: ast::MemberTarget::TupleIndex(i) } => {
                let (c, ty) = self.expr(obj)?;
                let VTy::Tup(id) = ty else { return Err("trans:元组索引需元组".into()); };
                let (a, b) = self.tuples[id as usize];
                let (ft, f) = if *i == 0 { (a, "_0") } else { (b, "_1") };
                Ok((format!("({}).{}", c, f), ft))
            }
            ast::Expr::Member { obj, target: ast::MemberTarget::Name(m) } => {
                let (c, ty) = self.expr(obj)?;
                // 属性访问(无括号)
                if ty == VTy::Unknown { eprintln!("DBG member-unknown .{} obj={:?}", m, obj); }
                if ty == VTy::Str {
                    return match m.as_str() {
                        "len" => Ok((format!("((ct_i)strlen({}))", c), VTy::Int(Some((IntW::W64, false))))),
                        "char_len" => Ok((format!("((ct_i)ct_char_len({}))", c), VTy::Int(Some((IntW::W64, false))))),
                        _ => Err(format!("trans v1 拒绝域:Str 属性 `{}`", m)),
                    };
                }
                if ty == VTy::Array || ty == VTy::FArr {
                    let acc = if ty == VTy::FArr { "->n" } else { "->n" };
                    return match m.as_str() {
                        "len" => Ok((format!("((ct_i)({}){})", c, acc), VTy::Int(Some((IntW::W64, false))))),
                        _ => Err(format!("trans v1 拒绝域:Array 属性 `{}`", m)),
                    };
                }
                // trait prop 分发:(类型, 属性) → 取值函数
                if let VTy::Class(tid) | VTy::Struct(tid) = rt_of(ty) {
                    let tname = self.types[tid as usize].name.clone();
                    let tn = self.trait_of_prop(&tname, m);
                    if let Some(tn) = tn {
                        return Ok((format!("ctn_{}_{}({})", tn, m, c), self.impl_ret_ty(&tname, m)));
                    }
                }
                if ty == VTy::ErrPtr {
                    return match m.as_str() {
                        "message" => Ok((format!("(char*)((ct_anyerr*)({}))->message", c), VTy::Str)),
                        "cause" => Ok((format!("((ct_anyerr*)({}))->cause", c), VTy::SumErr(self.enum_by_name.get("Option").copied().unwrap_or(0), Pay::I))),
                        "trace" => Ok(("(char*)\"ctron:1\"".into(), VTy::Str)),
                        _ => Err(format!("trans v1 拒绝域:错误对象成员 `.{}`", m)),
                    };
                }
                // 用户类型字段(struct 值 / class、Boxed 指针)
                let (base, tid) = self.deref_obj(&c, ty)
                    .map_err(|e| format!("{} [member `.{} on {:?}`]", e, m, ty))?;
                let fty = self.types[tid as usize].fields.iter()
                    .find(|(n, _)| n == m).map(|(_, t)| *t);
                let Some(fty) = fty else {
                    return Err(format!("trans:类型无字段 `{}`", m));
                };
                return Ok((format!("{}{}", base, m), fty));
            }
            ast::Expr::Index { obj, index } => {
                let (oc, ty) = self.expr(obj)?;
                let (ic, it) = self.expr(index)?;
                if !matches!(it, VTy::Int(_)) { return Err("trans:索引需整数".into()); }
                match ty {
                    VTy::Array => Ok((format!("ct_elem({}, {})", oc, ic), VTy::Int(None))),
                    VTy::FArr => Ok((format!("(float)(ct_felem({}, {}))", oc, ic), VTy::F32)),
                    _ => Err("trans:索引仅支持数组".into()),
                }
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
                if !self.type_by_name.contains_key(name.as_str()) && self.generics.contains_key(name.as_str()) {
                    // 泛型实例化:先求字段值类型
                    let mut vals = Vec::new();
                    let mut cs = Vec::new();
                    for f in fields {
                        let v = f.value.as_ref().ok_or("trans:字段简写未支持")?;
                        let (c, t) = self.expr(v)?;
                        cs.push((f.name.clone(), c));
                        vals.push((f.name.clone(), t));
                    }
                    let tid = self.instantiate_generic(&name, &vals)?;
                    let is_class = self.types[tid as usize].is_class;
                    let inits: Vec<String> = cs.iter().map(|(n, c)| format!(".{} = ({})", n, c)).collect();
                    let lit = format!("(ctn_{}){{ {} }}", self.types[tid as usize].name, inits.join(", "));
                    if is_class {
                        let n2 = self.types[tid as usize].name.clone();
                        return Ok((format!("({{ ctn_{n2}* p = malloc(sizeof(ctn_{n2})); *p = {lit}; p; }})"), VTy::Class(tid)));
                    }
                    return Ok((lit, VTy::Struct(tid)));
                }
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
                // match:和类型按 variant 分派;整数按字面量比较;struct 解构绑定。
                // 臂值经临时变量语句提升(与 if 同法);全 Void 臂 = 纯语句形态。
                let (sc, sty) = self.expr(expr)?;
                let _ = sty;
                let mv = self.uniq_name("m");
                let dv = self.uniq_name("done");
                let mut merged: Option<VTy> = None;
                let mut arm_blocks: Vec<String> = Vec::new();
                let mut arm_heads: Vec<String> = Vec::new();
                let mut arm_vals: Vec<Option<(String, VTy)>> = Vec::new();
                for arm in arms {
                    self.sink.push(String::new());
                    self.scope_push();
                    let (head, binds) = self.pat_arm(&arm.pattern, &mv, sty)?;
                    for b in binds { self.w(2, &b); }
                    let (ac, aty) = self.expr(&arm.expr)?;
                    self.scope_pop();
                    let code = self.sink.pop().unwrap_or_default();
                    arm_heads.push(head);
                    arm_blocks.push(code);
                    arm_vals.push(Some((ac, aty)));
                    merged = Some(match merged {
                        None => aty,
                        Some(prev) => merge_ty(prev, aty),
                    });
                }
                let has_value = merged.map(|t| !matches!(t, VTy::Void | VTy::Unknown)).unwrap_or(false);
                self.w(1, &format!("{} {} = ({});", self.c_ty(sty), mv, sc));
                self.w(1, &format!("ct_i {} = 0;", dv));
                if has_value {
                    let t = merged.unwrap();
                    let vv = self.uniq_name("mv");
                    self.w(1, &format!("{} {};", self.c_ty(t), vv));
                    for (i, _) in arm_heads.iter().enumerate() {
                        let kw = if i == 0 { "if" } else { "} else if" };
                        self.w(1, &format!("{} (({}) != 0) {{", kw, arm_heads[i]));
                        self.emit_lines(&arm_blocks[i]);
                        self.w(2, &format!("{} = {};", vv, arm_vals[i].as_ref().unwrap().0));
                        self.w(2, &format!("{} = 1;", dv));
                    }
                    self.w(1, "} else {");
                    self.w(2, "ct_panic(\"match 无匹配臂\");");
                    self.w(1, "}");
                    return Ok((vv, merged.unwrap()));
                }
                for (i, _) in arm_heads.iter().enumerate() {
                    let kw = if i == 0 { "if" } else { "} else if" };
                    self.w(1, &format!("{} (({}) != 0) {{", kw, arm_heads[i]));
                    self.emit_lines(&arm_blocks[i]);
                    if let Some((ac, _)) = arm_vals[i].as_ref() {
                        self.w(2, &format!("(void)({});", ac));
                    }
                    self.w(2, &format!("{} = 1;", dv));
                }
                self.w(1, "} else {");
                self.w(2, "ct_panic(\"match 无匹配臂\");");
                self.w(1, "}");
                Ok(("0".to_string(), VTy::Void))
            }
            ast::Expr::Try(e) => {
                // `?`:variant 1(None/Err)→ 提前 return 同型空值;否则解包载荷
                let (c, ty) = self.expr(e)?;
                let VTy::Sum(_, pay) = ty else { return Err("trans:? 需 Option/Result".into()); };
                let unwrap_ty = if pay == Pay::F { VTy::F64 } else { VTy::Int(None) };
                if self.ret_anyerr {
                    // `?` 擦除:Err → AnyError{message, cause, trace:"ctron:1"},EarlyReturn
                    return Ok((
                        format!(
                            "({{ ct_sum ct_t = ({}); if (ct_t.variant == 1) {{ ct_anyerr* ce = ct_mkerr(\"error\", (ct_sum){{ 0, {{ ct_t.p[0], {{.i = 0}}, {{.i = 0}}, {{.i = 0}} }} }}, \"ctron:1\"); return (ct_sum){{ 1, {{ {{.i = (ct_i)ce}}, {{.i = 0}}, {{.i = 0}}, {{.i = 0}} }} }}; }} ct_t.p[0].i; }})",
                            c
                        ),
                        unwrap_ty,
                    ));
                }
                Ok((
                    format!(
                        "({{ ct_sum ct_t = ({}); if (ct_t.variant == 1) {{ return (ct_sum){{ 1, {{ {{.i = 0}}, {{.i = 0}}, {{.i = 0}}, {{.i = 0}} }} }}; }} ct_t.p[0].i; }})",
                        c
                    ),
                    unwrap_ty,
                ))
            }
            ast::Expr::Closure { params, body, .. } => {
                // 非捕获闭包 → 顶层 static 函数 + 指针(捕获闭包域外)
                let fname = self.uniq_name("closure");
                let mut sig = String::from("static ct_i ");
                sig.push_str(&fname);
                sig.push_str("(");
                self.scope_push();
                let mut ps = Vec::new();
                for cp in params {
                    let c = self.bind(&cp.name, VTy::Int(None));
                    ps.push(format!("ct_i {}", c));
                }
                sig.push_str(&ps.join(", "));
                sig.push_str(") {");
                self.closure_defs.push(sig);
                match body.as_ref() {
                    ast::Expr::BlockExpr(b) => {
                        let mut buf = std::mem::take(&mut self.sink);
                        // 闭包体发射进独立缓冲再拼接
                        self.sink.push(String::new());
                        self.emit_block_stmts(b)?;
                        let code = self.sink.pop().unwrap_or_default();
                        self.sink = buf;
                        buf = Vec::new();
                        for line in code.lines() {
                            let last = self.closure_defs.last_mut().unwrap();
                            last.push_str("    ");
                            last.push_str(line);
                            last.push('\n');
                        }
                        let _ = buf;
                    }
                    other => {
                        let (c, ty) = self.expr(other)?;
                        let _ = ty;
                        let last = self.closure_defs.last_mut().unwrap();
                        last.push_str(&format!("    return {};\n", c));
                    }
                }
                self.scope_pop();
                self.closure_defs.last_mut().unwrap().push_str("}\n");
                Ok((fname, VTy::FnPtr))
            }
            ast::Expr::Call { callee, args } => self.call(callee, args),
            ast::Expr::TypeArgs { expr, args } => {
                // Simd[F32, N] → 定宽向量标记值
                if let ast::Expr::Ident(tn) = &**expr {
                    if tn == "Simd" {
                        return Ok(("((ct_simd){{ {{0}} }})".to_string(), VTy::Simd));
                    }
                }
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
            ast::Expr::Own { arena: an, body } => {
                // own 块:move/alloc 约束为编译期;运行期透明执行;arena 名绑定句柄
                self.scope_push();
                if !an.is_empty() { self.bind(&an, VTy::Arena); }
                for st in &body.stmts { self.emit_stmt(st)?; }
                let out = match &body.tail {
                    Some(t) => self.expr(t)?,
                    None => ("0".to_string(), VTy::Void),
                };
                self.scope_pop();
                Ok(out)
            }
            ast::Expr::Scope { param, body } => {
                // scope 块:结构化并发边界;|s| 绑定 scope 句柄
                self.scope_push();
                let sc = self.uniq_name("scope");
                self.w(1, &format!("ct_scope* {} = ct_scope_new();", sc));
                self.scopes.last_mut().unwrap().push((param.clone(), sc.clone(), VTy::ScopeH));
                for st in &body.stmts { self.emit_stmt(st)?; }
                let out = match &body.tail {
                    Some(t) => self.expr(t)?,
                    None => ("0".to_string(), VTy::Void),
                };
                self.scope_pop();
                Ok(out)
            }
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
        // 局部函数指针调用(f(f(x)))
        if let ast::Expr::Ident(name) = callee {
            if let Some((c, VTy::FnPtr)) = self.lookup(name) {
                let mut cs = Vec::new();
                for a in args {
                    let (c2, _) = self.expr(a)?;
                    cs.push(c2);
                }
                return Ok((format!("((ct_fnptr0)({}))({})", c, cs.join(", ")), VTy::Int(None)));
            }
        }
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
        // Arena.fixed[N](n) → arena 定长数组
        if let ast::Expr::TypeArgs { expr, .. } = callee {
            if let ast::Expr::Member { obj, target: ast::MemberTarget::Name(m) } = &**expr {
                if m == "fixed" {
                    if let ast::Expr::Ident(on) = &**obj {
                        if on == "Arena" {
                            let n_c = match args.first() {
                                Some(a) => self.expr(a)?.0,
                                None => "(ct_i)0".to_string(),
                            };
                            return Ok((format!("ct_arr_new((size_t)({}), NULL)", n_c), VTy::Array));
                        }
                    }
                }
            }
        }
        // Mutex[T](init) → 类型化单元格
        if let ast::Expr::TypeArgs { expr, args: targ_args } = callee {
            if let ast::Expr::Ident(tn) = &**expr {
                if tn == "Mutex" {
                    let inner = targ_args.first().map(|t| self.ty_of(t)).unwrap_or(VTy::Int(None));
                    let cid = self.intern_cell(inner, true);
                    let Some(a) = args.first() else { return Err("trans:Mutex 需初值".into()) };
                    let (ic, _) = self.expr(a)?;
                    let mty = self.mcell_ty(cid);
                    return Ok((
                        format!("({{ {}* p = malloc(sizeof(*p)); pthread_mutex_init(&p->mu, NULL); p->v = ({}); (void*)p; }})", mty, ic),
                        VTy::Cell(cid),
                    ));
                }
            }
        }
        // Channel[T](cap) → (tx, rx) 同一通道双端
        if let ast::Expr::TypeArgs { expr, .. } = callee {
            if let ast::Expr::Ident(tn) = &**expr {
                if tn == "Channel" {
                    let id = self.intern_tuple(VTy::Chan, VTy::Chan);
                    let tn2: String = self.c_ty(VTy::Chan).chars().map(|ch| if ch.is_alphanumeric() { ch } else { '_' }).collect();
                    let cap_c = match args.first() {
                        Some(a) => self.expr(a)?.0,
                        None => "(ct_i)4".to_string(),
                    };
                    return Ok((
                        format!("({{ ct_chan* ch = ct_ch_make((int)({})); (ct_val2_{}){{ (ct_chan*)ch, (ct_chan*)ch }}; }})", cap_c, tn2),
                        VTy::Tup(id),
                    ));
                }
            }
        }
        // Global[T](name, init) / Atomic[T](init) → 局部静态单元格
        if let ast::Expr::TypeArgs { expr, .. } = callee {
            if let ast::Expr::Ident(tn) = &**expr {
                if tn == "Global" || tn == "Atomic" {
                    let init_i = if tn == "Global" { args.get(1) } else { args.first() };
                    let init_c = match init_i {
                        Some(a) => self.expr(a)?.0,
                        None => "(ct_i)0".to_string(),
                    };
                    let cid = self.intern_cell(VTy::Int(None), false);
                    let cname = self.uniq_name(&format!("cell_{}", tn));
                    let iname = self.uniq_name("cellinit");
                    return Ok((
                        format!("({{ static ct_i {c}; static int {i} = 0; if (!{i}) {{ {c} = ({init}); {i} = 1; }} (&{c}); }})",
                            c = cname, i = iname, init = init_c),
                        VTy::Cell(cid),
                    ));
                }
            }
        }
        // Arena.fixed[N](n) → 定长 arena 数组(此处按需求长度分配)
        if let ast::Expr::Member { obj, target: ast::MemberTarget::Name(m) } = callee {
            if m == "fixed" {
                if let ast::Expr::Ident(on) = &**obj {
                    if on == "Arena" {
                        let n_c = if let Some(a) = args.first() { self.expr(a)?.0 } else { "(ct_i)0".into() };
                        return Ok((format!("ct_arr_new((size_t)({}), NULL)", n_c), VTy::Array));
                    }
                }
            }
        }
        // arena.list[T]() / arena.zeros[T](n) — TypeArgs 挂在 callee 位
        if let ast::Expr::TypeArgs { expr, .. } = callee {
            if let ast::Expr::Member { obj, target: ast::MemberTarget::Name(m) } = &**expr {
                let (_rc, rt) = self.expr(obj)?;
                if let VTy::Arena = rt {
                    let r = match m.as_str() {
                        "list" => Some(("ct_arr_new(0, NULL)".to_string(), VTy::Array)),
                        "array" | "zeros" => {
                            let Some(a) = args.first() else { return Err("trans:array/zeros 需长度".into()) };
                            let (c2, _) = self.expr(a)?;
                            Some((format!("ct_arr_new((size_t)({}), NULL)", c2), VTy::Array))
                        }
                        _ => None,
                    };
                    if let Some(r) = r { return Ok(r); }
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
                "println" | "print" => {
                    if args.len() != 1 { return Err(format!("trans:{} 需单实参", name)); }
                    let (c, at) = self.expr(&args[0])?;
                    let h = match at {
                        VTy::Str => "ct_print_str",
                        VTy::Bool => "ct_print_bool",
                        VTy::F64 => "ct_print_f64",
                        VTy::Int(Some((_, true))) => "ct_print_u64",
                        _ => "ct_print_i64",
                    };
                    let tail = if name == "println" { ", ct_print_nl()" } else { "" };
                    return Ok((format!("({}({}){})", h, c, tail), VTy::Void));
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
                // 泛型 fn / trait 对象形参:按实参类型单态化
                if self.fn_is_generic(name) || self.fn_has_traitobj_param(name) {
                    let mut atys = Vec::new();
                    let mut cs = Vec::new();
                    for a in args {
                        let (c, t) = self.expr(a)?;
                        cs.push(c);
                        atys.push(t);
                    }
                    let (sym, mret) = self.mono_fn(name, &atys)?;
                    return Ok((format!("{}({})", sym, cs.join(", ")), mret));
                }
                if self.extern_fns.contains(name) {
                    // extern "c":ABI 类型 cast,结果回包 ct_i(void → 0)
                    let mut cs = Vec::new();
                    for a in args {
                        let (c, at) = self.expr(a)?;
                        let aty = self.abi_ty(at);
                        cs.push(format!("({})({})", aty, c));
                    }
                    let call = format!("{}({})", name, cs.join(", "));
                    return Ok((match ret {
                        VTy::Void => "0".to_string(),
                        _ => format!("((ct_i){})", call),
                    }, if ret == VTy::Void { VTy::Void } else { VTy::Int(None) }));
                }
                let mut cs = Vec::new();
                for (a, pt) in args.iter().zip(&ptys) {
                    let (c, at) = self.expr(a)?;
                    cs.push(coerce(self.c_ty(*pt), c, at, *pt));
                }
                return Ok((format!("{}({})", sanitize(name), cs.join(", ")), ret));
            }
        }
            // parallel.map(coll, f):fork-join 数据并行(v1 串行等价,元素级纯函数)
        if let ast::Expr::Member { obj, target: ast::MemberTarget::Name(m) } = callee {
            if let ast::Expr::Ident(on) = &**obj {
                if on == "parallel" && m == "reduce" {
                    let (coll_c, coll_t) = self.expr(&args[0])?;
                    let (init_c, _it) = self.expr(&args[1])?;
                    let (fc, _ft) = self.expr(&args[2])?;
                    if coll_t != VTy::Array { return Err("trans:parallel.reduce 需数组".into()); }
                    return Ok((
                        format!("ct_parallel_reduce({}, (ct_i)({}), (ct_fnptr0)({}))", coll_c, init_c, fc),
                        VTy::Int(None),
                    ));
                }
                if on == "parallel" && m == "map" {
                    let (coll_c, coll_t) = self.expr(&args[0])?;
                    let (fc, _ft) = self.expr(&args[1])?;
                    if coll_t != VTy::Array { return Err("trans:parallel.map 需数组".into()); }
                    return Ok((
                        format!("ct_parallel_map({}, (ct_fnptr0)({}))", coll_c, fc),
                        VTy::Array,
                    ));
                }
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
                        Some((format!("(ct_contains({}, {}) != NULL)", rc, ac), VTy::Bool))
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
            // scope.spawn(闭包) → 任务(捕获经 env 结构体)
            if let VTy::ScopeH = rt {
                if m == "spawn" {
                    return self.emit_spawn(rc, args);
                }
                return Err(format!("trans v1 拒绝域:scope 方法 `.{}`", m));
            }
            // 任务:join / join_or
            if let VTy::Task = rt {
                return match m.as_str() {
                    "join" => Ok((format!("ct_join({})", rc), VTy::Int(None))),
                    "join_or" => {
                        let rid = self.enum_by_name.get("Result").copied().unwrap_or(0);
                        Ok((format!("ct_join_or({})", rc), VTy::Sum(rid, Pay::I)))
                    }
                    _ => Err(format!("trans v1 拒绝域:Task 方法 `.{}`", m)),
                };
            }
            // 通道端点:send / recv
            if let VTy::Chan = rt {
                let rid = self.enum_by_name.get("Result").copied().unwrap_or(0);
                return match m.as_str() {
                    "send" => {
                        let Some(a) = args.first() else { return Err("trans:send 需实参".into()) };
                        let (c, at) = self.expr(a)?;
                        let pc = payload_cell(c, at)?;
                        Ok((format!("ct_ch_send((ct_chan*)({}), {})", rc, slot_i(&pc)), VTy::Sum(rid, Pay::I)))
                    }
                    "recv" => Ok((format!("ct_ch_recv((ct_chan*)({}))", rc), VTy::Sum(rid, Pay::I))),
                    _ => Err(format!("trans v1 拒绝域:Chan 方法 `.{}`", m)),
                };
            }
            // 数组方法:push / into_gc(深拷贝)
            if let VTy::Array = rt {
                let r = match m.as_str() {
                    "push" => {
                        let Some(a) = args.first() else { return Err("trans:push 需实参".into()) };
                        let (c, _) = self.expr(a)?;
                        Some((format!("(ct_arr_push({}, (ct_i)({})), 0)", rc, c), VTy::Void))
                    }
                    "into_gc" => Some((format!("ct_arr_clone({})", rc), VTy::Array)),
                    _ => None,
                };
                if let Some(r) = r { return Ok(r); }
            }
            // Arena 泛型方法:arena.list[T]() / arena.zeros[T](n)
            if let VTy::Arena = rt {
                let r = match m.as_str() {
                    "list" => Some(("ct_arr_new(0, NULL)".to_string(), VTy::Array)),
                    "array" | "zeros" => {
                        let Some(a) = args.first() else { return Err("trans:array/zeros 需长度".into()) };
                        let (c, _) = self.expr(a)?;
                        Some((format!("ct_arr_new((size_t)({}), NULL)", c), VTy::Array))
                    }
                    _ => None,
                };
                if let Some(r) = r { return Ok(r); }
            }
            // Simd 方法:splat / lane / to_array
            if rt == VTy::Simd {
                let r = match m.as_str() {
                    "splat" => {
                        let Some(a) = args.first() else { return Err("trans:splat 需实参".into()) };
                        let (c, _) = self.expr(a)?;
                        Some((format!("ct_simd_splat((float)({}))", c), VTy::Simd))
                    }
                    "lane" => {
                        let Some(a) = args.first() else { return Err("trans:lane 需实参".into()) };
                        let (c, _) = self.expr(a)?;
                        Some((format!("(float)(ct_simd_lane({}, (int)({})))", rc, c), VTy::F32))
                    }
                    "to_array" => Some((format!("ct_simd_to_array({})", rc), VTy::FArr)),
                    _ => None,
                };
                if let Some(r) = r { return Ok(r); }
            }
            // Global/Atomic/Mutex 单元格方法
            if let VTy::Cell(cid) = rt {
                let (inner, is_mutex) = self.cells[cid as usize];
                let r = match m.as_str() {
                    "with" | "with_mut" => {
                        let Some(cl) = args.first() else { return Err("trans:with 需闭包".into()) };
                        let ast::Expr::Closure { params, body, .. } = cl else {
                            return Err("trans v1 拒绝域:with 需闭包字面量".into());
                        };
                        let pname = params.first().map(|cp| cp.name.clone()).unwrap_or_else(|| "c".into());
                        let is_mut = m == "with_mut";
                        let (lock_fn, unlock_fn, inner_acc) = if is_mutex {
                            let mt = self.mcell_ty(cid);
                            (format!("pthread_mutex_lock(&(({}*)({}))->mu)", mt, rc),
                             format!("pthread_mutex_unlock(&(({}*)({}))->mu)", mt, rc),
                             format!("(({}*)({}))->v", mt, rc))
                        } else {
                            ("ct_glock()".to_string(), "ct_gunlock()".to_string(), format!("(*({}))", rc))
                        };
                        self.scope_push();
                        self.sink.push(String::new());
                        self.w(2, &format!("{};", lock_fn));
                        let cbind = self.bind(&pname, inner);
                        self.w(2, &format!("{} {} = {};", self.c_ty(inner), cbind, inner_acc));
                        let tail_val: Option<(String, VTy)> = match body.as_ref() {
                            ast::Expr::BlockExpr(b) => {
                                self.scope_push();
                                for st in &b.stmts { self.emit_stmt(st)?; }
                                let t = match &b.tail {
                                    Some(t) => Some(self.expr(t)?),
                                    None => None,
                                };
                                self.scope_pop();
                                t
                            }
                            other => Some(self.expr(other)?),
                        };
                        let code = self.sink.pop().unwrap_or_default();
                        self.scope_pop();
                        if is_mut {
                            Some(Ok((format!("({{ {} {} = {}; {}; 0; }})",
                                code, inner_acc, cbind, unlock_fn), VTy::Void)))
                        } else {
                            match tail_val {
                                Some((v, vt)) => {
                                    let rty = self.c_ty(vt).to_string();
                                    let rv = self.uniq_name("r");
                                    Some(Ok((format!("({{ {} {} {} = ({}); {}; {}; {}; }})",
                                        code, rty, rv, v, rv, unlock_fn, rv), vt)))
                                }
                                None => Some(Ok((format!("({{ {}; {}; 0; }})", code, unlock_fn), VTy::Void))),
                            }
                        }
                    }
                    "fetch_add" => {
                        let Some(a) = args.first() else { return Err("trans:fetch_add 需实参".into()) };
                        let (c2, _) = self.expr(a)?;
                        Some(Ok((format!("({{ ct_i* p = ({}); ct_i old = *p; *p += ({}); old; }})", rc, c2), VTy::Int(None))))
                    }
                    "load" => Some(Ok((format!("(*({}))", rc), VTy::Int(None)))),
                    "store" => {
                        let Some(a) = args.first() else { return Err("trans:store 需实参".into()) };
                        let (c2, _) = self.expr(a)?;
                        Some(Ok((format!("(*({}) = ({}), 0)", rc, c2), VTy::Void)))
                    }
                    _ => None,
                };
                match r {
                    Some(Ok(v)) => return Ok(v),
                    Some(Err(e)) => return Err(e),
                    None => {}
                }
            }
            // @derive(Show):.show() → 每具体类型一个 show 函数(格式 Name{f: v, ...})
            if let VTy::Class(tid) | VTy::Struct(tid) = rt {
                if m == "show" {
                    let sym = self.show_fn_for(tid)?;
                    return Ok((format!("{}({})", sym, rc), VTy::Str));
                }
            }
            // trait impl 分发:(接收者类型, 方法) → 注册表
            if let VTy::Class(tid) | VTy::Struct(tid) = rt {
                let tname = self.types[tid as usize].name.clone();
                if let Some(tn) = self.trait_of_method(&tname, m) {
                    let mut cs = vec![rc];
                    for a in args {
                        let (c2, _) = self.expr(a)?;
                        cs.push(c2);
                    }
                    let sym = format!("ctn_{}_{}", tn, m);
                    let ret = self.impl_ret_ty(&tname, m);
                    return Ok((format!("{}({})", sym, cs.join(", ")), ret));
                }
            }
            // Option/Result 方法
            if let VTy::Sum(..) | VTy::SumErr(..) = rt {
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
                        if matches!(rt, VTy::SumErr(..)) {
                            (format!(
                                "({{ ct_sum t = ({}); if (t.variant == 1) ct_panic(\"{}\"); t.p[0].i; }})", rc, msg),
                             VTy::ErrPtr)
                        } else {
                            (format!(
                                "({{ ct_sum t = ({}); if (t.variant == 1) ct_panic(\"{}\"); t.p[0].i; }})", rc, msg),
                             VTy::Int(None))
                        }
                    }
                    "is_some" | "is_ok" => {
                        (format!("(({}).variant == 0)", rc), VTy::Bool)
                    }
                    "is_none" | "is_err" => {
                        (format!("(({}).variant == 1)", rc), VTy::Bool)
                    }
                    "map" => {
                        let Some(a) = args.first() else { return Err("trans:map 需函数".into()) };
                        let (fc, ft) = self.expr(a)?;
                        if ft != VTy::FnPtr { return Err("trans:map 需函数指针".into()); }
                        // Ok/Some → f(载荷) 重包;Err/None → 原样(Result 的 Err 槽 p[0] 不动)
                        (format!(
                            "({{ ct_sum t = ({}); ct_sum ct_m; if (t.variant != 0) {{ ct_m = t; }} else {{ ct_m = t; ct_m.p[0].i = ((ct_fnptr0)({}))(t.p[0].i); }} ct_m; }})",
                            rc, fc),
                         rt)
                    }
                    "context" => {
                        // Err → Err(AnyError{message, cause=原载荷});Ok 原样
                        let VTy::Sum(rid, rpay) = rt else { return Err("trans:context 需 Result".into()) };
                        let Some(a) = args.first() else { return Err("trans:context 需消息".into()) };
                        let ast::Expr::Str { parts } = a else {
                            return Err("trans v1 拒绝域:非字面量 context 消息".into());
                        };
                        let msg = match parts.as_slice() {
                            [ast::StrPart::Text(t)] => t.replace('"', "\\\""),
                            _ => "context".to_string(),
                        };
                        (format!(
                            "({{ ct_sum t = ({}); ct_sum ct_r; if (t.variant == 0) {{ ct_r = t; }} else {{ ct_anyerr* e = ct_mkerr(\"{}\", (ct_sum){{ 0, {{ t.p[0], {{.i = 0}}, {{.i = 0}}, {{.i = 0}} }} }}, \"ctron:1\"); ct_r = (ct_sum){{ 1, {{ {{.i = (ct_i)e}}, {{.i = 0}}, {{.i = 0}}, {{.i = 0}} }} }}; }} ct_r; }})",
                            rc, msg),
                         VTy::Sum(rid, rpay))
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
        {
            return Err({
                let dn = match callee { ast::Expr::Ident(n) => format!("Ident({})", n), ast::Expr::Member { target: ast::MemberTarget::Name(m), .. } => format!("Member.{}", m), ast::Expr::TypeArgs { expr, args } => format!("TypeArgs({:?}, +{} args)", expr, args.len()), _ => "?".into() };
                format!("trans v1 拒绝域:该调用形态 {} args={}", dn, args.len())
            });
        }
    }

    fn binop(&mut self, op: &ast::BinOp, lc: &str, lt: VTy, rc: &str, rt: VTy) -> TRes {
        use ast::BinOp::*;
        if matches!(op, Add) && lt == VTy::Str && rt == VTy::Str {
            // T2 规格修订:Add 双 Str 为拼接
            return Ok((format!("ct_str_concat({}, {})", lc, rc), VTy::Str));
        }
        match op {
            AndAnd => Ok((format!("(({}) && ({}))", lc, rc), VTy::Bool)),
            Eq | Ne | Lt | Gt | Le | Ge => {
                if lt == VTy::Str && rt == VTy::Str {
                    if !matches!(op, Eq | Ne) { return Err("trans:Str 仅可 ==/!=".into()); }
                    let c = if *op == Eq { "==" } else { "!=" };
                    return Ok((format!("((strcmp({}, {}) {} 0))", lc, rc, c), VTy::Bool));
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
                if lt == VTy::Simd && rt == VTy::Simd {
                    let f = match op {
                        Add => "ct_simd_add", Sub => "ct_simd_sub",
                        Mul => "ct_simd_mul", _ => return Err("trans:Simd 仅支持 + - *".into()),
                    };
                    return Ok((format!("{}({}, {})", f, lc, rc), VTy::Simd));
                }
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
/// ct_cell 初始化串中的整数值表达式(send 载荷恒走 .i 槽)
fn slot_i(cell_init: &str) -> String {
    // 形如 {.i = X} / {.f = X} / {.i = (ct_i)(!!(x))}
    if let Some(i) = cell_init.find("= ") {
        let inner = &cell_init[i + 2..];
        let inner = inner.trim_end_matches('}');
        inner.to_string()
    } else {
        cell_init.to_string()
    }
}

fn payload_cell(c: String, ty: VTy) -> Result<String, String> {
    Ok(match ty {
        VTy::Int(_) => format!("{{.i = {}}}", c),
        VTy::Bool => format!("{{.i = (ct_i)(!!({}))}}", c),
        VTy::F64 => format!("{{.f = {}}}", c),
        VTy::F32 => format!("{{.f = (double)({})}}", c),
        // 错误域:Sum 载荷编码为变体号(Err(DivByZero) → p[0].i = vid)
        VTy::Sum(..) => format!("{{.i = ((ct_i)({}).variant)}}", c),
        _ => return Err("trans v1 拒绝域:该载荷类型".into()),
    })
}

fn rt_of(t: VTy) -> VTy { t }

fn named_tail_pub(t: &ast::Type) -> String {
    match t { ast::Type::Named { path, .. } => path.last().cloned().unwrap_or_default(), _ => String::new() }
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
        _ => return Err(format!("trans v1 拒绝域:插值表达式类型 {:?}", ty)),
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
    if from == VTy::FnPtr && to == VTy::FnPtr {
        return format!("((ct_fnptr0)({}))", c);
    }
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


const PREAMBLE: &str = r#"/* Ctron → C 转译产物(P1-E① 数值域;语义契约 = Rust 解释器) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <setjmp.h>

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

typedef struct ct_scope { int cancelled; pthread_mutex_t mu; } ct_scope;
typedef struct ct_task {
    pthread_t th; void* env; void* (*shim)(void*);
    ct_i result; int state; /* 0=run 1=done 2=panicked */
    char pmsg[128]; ct_scope* scope;
    jmp_buf jmp;
} ct_task;
static __thread ct_task* ct_tls_task = NULL;
static __thread ct_scope* ct_tls_scope = NULL;
static void ct_cancel_broadcast(void);

static void ct_panic(const char* msg) {
    ct_task* t = ct_tls_task;
    if (t) {
        snprintf(t->pmsg, sizeof t->pmsg, "%s", msg);
        t->state = 2;
        if (t->scope) t->scope->cancelled = 1;
        longjmp(t->jmp, 1);
    }
    fprintf(stderr, "panic: %s (test %s)\n", msg, ct_cur_test);
    exit(1);
}
static int ct_assert(int ok) {
    if (!ok) { fprintf(stderr, "assertion failed (test %s)\n", ct_cur_test); exit(1); }
    return ok;
}
/* ---- print 域(D4;格式面 = 解释器 fmt_val)---- */
static void ct_print_nl(void) { printf("\n"); }
static void ct_print_str(const char* v) { printf("%s", v ? v : ""); }
static void ct_print_bool(int v) { printf("%s", v ? "true" : "false"); }
static void ct_print_i64(ct_i v) { printf("%lld", (long long)v); }
static void ct_print_u64(ct_i v) { printf("%llu", (unsigned long long)v); }
static void ct_print_f64(double v) { char b[64]; if (v == (double)(long long)v) snprintf(b, sizeof b, "%.1f", v); else snprintf(b, sizeof b, "%g", v); printf("%s", b); }
static char* ct_str_concat(const char* a, const char* b) { size_t la = strlen(a), lb = strlen(b); char* r = (char*)malloc(la + lb + 1); memcpy(r, a, la); memcpy(r + la, b, lb); r[la + lb] = 0; return r; }
/* ---- 和类型:tagged union;载荷槽 i/f 按静态类型选用 ---- */
typedef ct_i (*ct_fnptr0)();
typedef union { ct_i i; double f; } ct_cell;
typedef struct { int variant; ct_cell p[4]; } ct_sum;

/* ---- 结构化并发:scope / task / channel(语义契约 = 解释器) ---- */
#define CT_CH_CAP 64
typedef struct ct_chan {
    pthread_mutex_t mu; pthread_cond_t cv_full, cv_empty;
    ct_i buf[CT_CH_CAP]; size_t head, tail, cnt; int cap;
} ct_chan;
static ct_chan* ct_chans[64]; static int ct_nchans = 0;
static pthread_mutex_t ct_reg_mu = PTHREAD_MUTEX_INITIALIZER;
static ct_chan* ct_ch_make(int cap) {
    ct_chan* c = (ct_chan*)calloc(1, sizeof(ct_chan));
    if (!c) abort();
    pthread_mutex_init(&c->mu, NULL);
    pthread_cond_init(&c->cv_full, NULL);
    pthread_cond_init(&c->cv_empty, NULL);
    if (cap > CT_CH_CAP || cap <= 0) cap = CT_CH_CAP;
    c->cap = cap;
    pthread_mutex_lock(&ct_reg_mu);
    if (ct_nchans < 64) ct_chans[ct_nchans++] = c;
    pthread_mutex_unlock(&ct_reg_mu);
    return c;
}
static ct_sum ct_ch_send(ct_chan* c, ct_i v) {
    pthread_mutex_lock(&c->mu);
    while (c->cnt >= (size_t)c->cap) {
        if (ct_tls_scope && ct_tls_scope->cancelled) {
            pthread_mutex_unlock(&c->mu);
            return (ct_sum){ 1, {{.i = (ct_i)"ScopeCancelled"}, {.i = 0}, {.i = 0}, {.i = 0}} };
        }
        pthread_cond_wait(&c->cv_full, &c->mu);
    }
    c->buf[c->tail] = v; c->tail = (c->tail + 1) % CT_CH_CAP; c->cnt++;
    pthread_cond_broadcast(&c->cv_empty);
    pthread_mutex_unlock(&c->mu);
    return (ct_sum){ 0, {{.i = 0}, {.i = 0}, {.i = 0}, {.i = 0}} };
}
static ct_sum ct_ch_recv(ct_chan* c) {
    pthread_mutex_lock(&c->mu);
    while (c->cnt == 0) {
        if (ct_tls_scope && ct_tls_scope->cancelled) {
            pthread_mutex_unlock(&c->mu);
            return (ct_sum){ 1, {{.i = (ct_i)"ScopeCancelled"}, {.i = 0}, {.i = 0}, {.i = 0}} };
        }
        pthread_cond_wait(&c->cv_empty, &c->mu);
    }
    ct_i v = c->buf[c->head]; c->head = (c->head + 1) % CT_CH_CAP; c->cnt--;
    pthread_cond_broadcast(&c->cv_full);
    pthread_mutex_unlock(&c->mu);
    return (ct_sum){ 0, {{.i = v}, {.i = 0}, {.i = 0}, {.i = 0}} };
}
static pthread_mutex_t ct_glock_mu = PTHREAD_MUTEX_INITIALIZER;
static void ct_glock(void) { pthread_mutex_lock(&ct_glock_mu); }
static void ct_gunlock(void) { pthread_mutex_unlock(&ct_glock_mu); }
static void ct_cancel_broadcast(void) {
    pthread_mutex_lock(&ct_reg_mu);
    for (int i = 0; i < ct_nchans; i++) {
        pthread_cond_broadcast(&ct_chans[i]->cv_full);
        pthread_cond_broadcast(&ct_chans[i]->cv_empty);
    }
    pthread_mutex_unlock(&ct_reg_mu);
}
static void* ct_shim_tramp(void* envp);
static ct_task* ct_spawn(void* (*shim)(void*), void* env, ct_scope* sc) {
    ct_task* t = (ct_task*)calloc(1, sizeof(ct_task));
    if (!t) abort();
    t->env = env; t->scope = sc; t->state = 0;
    t->shim = shim;
    pthread_create(&t->th, NULL, ct_shim_tramp, t);
    return t;
}
static void* ct_shim_tramp(void* envp) {
    ct_task* self = (ct_task*)envp;
    ct_tls_task = self;
    ct_tls_scope = self->scope;
    if (setjmp(self->jmp)) {
        self->state = 2;
        if (self->scope) self->scope->cancelled = 1;
        ct_cancel_broadcast();
    } else {
        self->shim(self->env);
        self->state = 1;
    }
    return 0;
}
static ct_i ct_join(ct_task* t) {
    void* d; pthread_join(t->th, &d);
    return t->state == 2 ? (ct_i)0 : t->result;
}
static ct_sum ct_join_or(ct_task* t) {
    void* d; pthread_join(t->th, &d);
    if (t->state == 2) {
        return (ct_sum){ 1, {{.i = (ct_i)t->pmsg}, {.i = 0}, {.i = 0}, {.i = 0}} };
    }
    return (ct_sum){ 0, {{.i = t->result}, {.i = 0}, {.i = 0}, {.i = 0}} };
}
static ct_scope* ct_scope_new(void) {
    ct_scope* sc = (ct_scope*)calloc(1, sizeof(ct_scope));
    if (!sc) abort();
    pthread_mutex_init(&sc->mu, NULL);
    return sc;
}

/* ---- Simd[F32,4](元素级白名单运算)与 f32 数组 ---- */
typedef struct { float v[4]; } ct_simd;
typedef struct { float* d; size_t n; } ct_farr;
static ct_simd ct_simd_splat(float f) {
    ct_simd s;
    for (int i = 0; i < 4; i++) s.v[i] = f;
    return s;
}
static ct_simd ct_simd_add(ct_simd a, ct_simd b) {
    ct_simd r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] + b.v[i]; return r;
}
static ct_simd ct_simd_sub(ct_simd a, ct_simd b) {
    ct_simd r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] - b.v[i]; return r;
}
static ct_simd ct_simd_mul(ct_simd a, ct_simd b) {
    ct_simd r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] * b.v[i]; return r;
}
static float ct_simd_lane(ct_simd s, int i) {
    if (i < 0 || i >= 4) { fprintf(stderr, "simd lane out of bounds\n"); exit(1); }
    return s.v[i];
}
static ct_farr* ct_simd_to_array(ct_simd s) {
    ct_farr* r = (ct_farr*)malloc(sizeof(ct_farr));
    if (!r) abort();
    r->n = 4;
    r->d = (float*)malloc(4 * sizeof(float));
    if (!r->d) abort();
    for (int i = 0; i < 4; i++) r->d[i] = s.v[i];
    return r;
}
static float ct_felem(ct_farr* a, ct_i i) {
    if (i < 0 || (size_t)i >= a->n) { fprintf(stderr, "index out of bounds\n"); exit(1); }
    return a->d[(size_t)i];
}

/* ---- 错误对象(context 产物;堆分配,进程期不回收) ---- */
typedef struct ct_anyerr { const char* message; ct_sum cause; const char* trace; } ct_anyerr;
static ct_anyerr* ct_mkerr(const char* m, ct_sum cause, const char* trace) {
    ct_anyerr* e = (ct_anyerr*)malloc(sizeof(ct_anyerr));
    if (!e) abort();
    e->message = m;
    e->cause = cause;
    e->trace = trace;
    return e;
}

/* ---- 数组运行时:句柄共享语义(对齐 interp Rc<Vec>),进程期不回收 ---- */
typedef struct { ct_i* d; size_t n; } ct_arr;
static ct_arr* ct_arr_new(size_t n, ct_i* init) {
    ct_arr* a = (ct_arr*)malloc(sizeof(ct_arr));
    if (!a) abort();
    a->n = n;
    a->d = n ? (ct_i*)malloc(n * sizeof(ct_i)) : NULL;
    if (n && !a->d) abort();
    if (init) { for (size_t i = 0; i < n; i++) a->d[i] = init[i]; }
    return a;
}
static ct_i ct_elem(ct_arr* a, ct_i i) {
    if (i < 0 || (size_t)i >= a->n) { fprintf(stderr, "index out of bounds\n"); exit(1); }
    return a->d[(size_t)i];
}
static void ct_arr_push(ct_arr* a, ct_i v) {
    a->d = (ct_i*)realloc(a->d, (a->n + 1) * sizeof(ct_i));
    if (!a->d) abort();
    a->d[a->n++] = v;
}
static ct_arr* ct_arr_clone(ct_arr* a) {
    return ct_arr_new(a->n, a->d);
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
#include <stdarg.h>
static char* ct_cat(int n, ...) {
    va_list ap;
    va_start(ap, n);
    size_t total = 0;
    for (int i = 0; i < n; i++) total += strlen(va_arg(ap, const char*));
    va_end(ap);
    char* r = (char*)malloc(total + 1);
    if (!r) abort();
    va_start(ap, n);
    char* p = r;
    for (int i = 0; i < n; i++) { const char* a = va_arg(ap, const char*); size_t l = strlen(a); memcpy(p, a, l); p += l; }
    va_end(ap);
    *p = '\0';
    return r;
}
static ct_arr* ct_parallel_map(ct_arr* a, ct_fnptr0 f) {
    ct_arr* r = ct_arr_new(a->n, NULL);
    for (size_t i = 0; i < a->n; i++) r->d[i] = f(a->d[i]);
    return r;
}
static ct_i ct_parallel_reduce(ct_arr* a, ct_i init, ct_fnptr0 f) {
    ct_i acc = init;
    for (size_t i = 0; i < a->n; i++) acc = f(acc, a->d[i]);
    return acc;
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
