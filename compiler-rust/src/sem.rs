//! 语义模型:类型表示、类型/符号注册表、prelude、包级名字解析与声明收集。
//! 诊断:E2020(未解析/不可见)、E5010(孤儿规则)、E5020(循环依赖)。

use crate::ast;
use crate::token::Diagnostic;
use std::collections::HashMap;

pub type DefId = usize;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum IntW { W8, W16, W32, W64, WSize }

#[derive(Debug, Clone, PartialEq)]
pub enum Ty {
    Err,
    Void, Never, Bool, Str, String,
    Int(IntW), UInt(IntW), F32, F64,
    Simd(Box<Ty>),
    Named { def: DefId, args: Vec<Ty> },
    Tuple(Vec<Ty>),
    MutSlice(Box<Ty>),
    RoSlice(Box<Ty>),
    Ref(Box<Ty>),
    Array(Box<Ty>),
    Optional(Box<Ty>),
    FnTy { params: Vec<Ty>, ret: Box<Ty> },
    Range(Box<Ty>),
    Ctor { def: DefId, args: Vec<Ty> },
    Var(u32),
    ComptimeVal(String),
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum DefKind { Struct, Class, Enum, Trait, Prelude }

#[derive(Debug, Clone)]
pub struct FnSig { pub params: Vec<Ty>, pub ret: Ty }

#[derive(Debug, Clone)]
pub struct TypeDef {
    pub name: String,
    pub kind: DefKind,
    pub cap: bool,
    pub params: Vec<Ty>,                    // 类型参数变量(按声明序)
    pub fields: Vec<(String, Ty, bool)>,
    pub variants: Vec<(String, Vec<Ty>)>,
    pub props: Vec<(String, Ty)>,
    pub methods: Vec<(String, FnSig)>,   // trait 方法签名
}

#[derive(Debug, Clone)]
pub struct FnDef {
    pub module: String,
    pub name: String,
    pub vis: ast::Vis,
    pub type_params: Vec<String>,
    /// 与 type_params 同序的统一变量 id(声明期分配;v0.7 修订三:调用点推断用)
    pub tparam_vars: Vec<u32>,
    pub params: Vec<(String, Ty)>,
    pub ret: Ty,
    pub is_comptime: bool,
    pub no_alloc: bool,
    pub no_spawn: bool,
    pub pure: bool,
    pub body: Option<ast::Block>,
}

#[derive(Debug, Clone)]
pub struct ImplEntry {
    pub module: String,
    pub trait_name: String,
    pub for_type: String,
}

#[derive(Debug, Clone)]
pub enum Symbol {
    Fn(usize),
    Const { ty: Ty },
    Static { ty: Ty },
    Type(DefId),
    Module(String),
    Variant { def: DefId, idx: usize },
}

#[derive(Debug, Clone)]
pub struct SemModule {
    pub path: String,
    pub file_idx: usize,
    pub symbols: HashMap<String, Symbol>,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Profile { Full, Web, Bare }

impl Profile {
    pub fn from_str(s: &str) -> Profile {
        match s { "web" => Profile::Web, "bare" => Profile::Bare, _ => Profile::Full }
    }
}

pub struct Manifest {
    pub caps: HashMap<String, bool>,
    #[allow(dead_code)]
    pub comptime_budget_steps: u64,
}

pub struct Sema {
    pub defs: Vec<TypeDef>,
    pub def_by_name: HashMap<String, DefId>,
    pub cap_key_by_def: HashMap<DefId, String>,
    pub fns: Vec<FnDef>,
    pub impls: Vec<ImplEntry>,
    pub mods: Vec<SemModule>,
    pub mod_by_path: HashMap<String, usize>,
    pub manifest: Option<Manifest>,
    pub profile: Profile,
    pub var_next: u32,
    pub runtime_fns: HashMap<String, RuntimeFnSig>,
}

impl Sema {
    /// 公开的不可变类型降级(interp 用)
    pub fn lower_ty_pub(&self, t: &ast::Type) -> Ty {
        let empty = HashMap::new();
        self.lower_ty(t, &empty)
    }

    /// 不可变类型降级(检查期;未知类型名 → Ty::Err)
    pub fn lower_ty(&self, t: &ast::Type, params: &HashMap<String, Ty>) -> Ty {
        match t {
            ast::Type::Named { path, args } => {
                if path.len() == 1 && args.is_empty() {
                    if let Some(v) = params.get(&path[0]) { return v.clone(); }
                    // 原生标量名 → Ty 原生表示(检查器按 Ty 比较一致性)
                    match path[0].as_str() {
                        "I8" => return Ty::Int(IntW::W8), "I16" => return Ty::Int(IntW::W16),
                        "I32" => return Ty::Int(IntW::W32), "I64" => return Ty::Int(IntW::W64),
                        "ISize" => return Ty::Int(IntW::WSize),
                        "U8" => return Ty::UInt(IntW::W8), "U16" => return Ty::UInt(IntW::W16),
                        "U32" => return Ty::UInt(IntW::W32), "U64" => return Ty::UInt(IntW::W64),
                        "USize" => return Ty::UInt(IntW::WSize),
                        "F32" => return Ty::F32, "F64" => return Ty::F64,
                        "Bool" => return Ty::Bool, "Str" => return Ty::Str,
                        "String" => return Ty::String, "Void" => return Ty::Void,
                        "Never" => return Ty::Never,
                        _ => {}
                    }
                }
                match self.def_by_name.get(&path.join(".")) {
                    Some(&def) => Ty::Named { def, args: args.iter().map(|a| self.lower_ty(a, params)).collect() },
                    None => Ty::Err,
                }
            }
            ast::Type::Ref(inner) => Ty::Ref(Box::new(self.lower_ty(inner, params))),
            ast::Type::Slice(inner) => Ty::MutSlice(Box::new(self.lower_ty(inner, params))),
            ast::Type::Array { elem, .. } => Ty::Array(Box::new(self.lower_ty(elem, params))),
            ast::Type::Optional(inner) => {
                match self.def_by_name.get("Option") {
                    Some(&d) => Ty::Named { def: d, args: vec![self.lower_ty(inner, params)] },
                    None => Ty::Err,
                }
            }
            ast::Type::Tuple(tys) => Ty::Tuple(tys.iter().map(|t| self.lower_ty(t, params)).collect()),
            ast::Type::Fn { params: ps, ret } => Ty::FnTy {
                params: ps.iter().map(|t| self.lower_ty(t, params)).collect(),
                ret: Box::new(match ret { Some(r) => self.lower_ty(r, params), None => Ty::Void }),
            },
            ast::Type::SelfT => Ty::Err,
            ast::Type::ComptimeVal(text) => Ty::ComptimeVal(text.clone()),
        }
    }

    pub fn new_var(&mut self) -> Ty { let v = self.var_next; self.var_next += 1; Ty::Var(v) }

    pub fn def_id(&mut self, def: TypeDef) -> DefId {
        let id = self.defs.len();
        self.def_by_name.insert(def.name.clone(), id);
        self.defs.push(def);
        id
    }

    pub fn intern_type_name(&mut self, name: &str) -> DefId {
        if let Some(&id) = self.def_by_name.get(name) { return id; }
        self.def_id(TypeDef {
            name: name.into(), kind: DefKind::Prelude, cap: false, params: vec![],
            fields: vec![], variants: vec![], props: vec![], methods: vec![],
        })
    }

    pub fn is_prelude_name(&self, name: &str) -> bool {
        self.def_by_name.get(name).map(|&id| self.defs[id].kind == DefKind::Prelude).unwrap_or(false)
    }
}

fn prelude_def(name: &str, cap: bool, kind: DefKind) -> TypeDef {
    TypeDef { name: name.into(), kind, cap, params: vec![], fields: vec![], variants: vec![], props: vec![], methods: vec![] }
}

fn register_prelude(sema: &mut Sema) {
    for n in ["I8", "I16", "I32", "I64", "ISize", "U8", "U16", "U32", "U64", "USize",
              "F32", "F64", "Bool", "Str", "String", "Void", "Never", "TaskPanic"] {
        sema.def_id(prelude_def(n, false, DefKind::Prelude));
    }
    for n in ["Channel", "List", "Map", "Set", "Box", "StringBuilder",
              "Atomic", "Global", "Mutex", "Sender", "Receiver", "Task", "Scope",
              "Arena", "Region", "Pool", "ArenaList", "Simd", "AnyError", "Parallel",
              "Path", "Bytes"] {
        sema.def_id(prelude_def(n, false, DefKind::Prelude));
    }
    {
        let t = sema.new_var();
        sema.def_id(TypeDef { name: "Option".into(), kind: DefKind::Prelude, cap: false, params: vec![t.clone()],
            fields: vec![], variants: vec![("Some".into(), vec![t]), ("None".into(), vec![])],
            props: vec![], methods: vec![] });
        let t = sema.new_var();
        let e = sema.new_var();
        sema.def_id(TypeDef { name: "Result".into(), kind: DefKind::Prelude, cap: false, params: vec![t.clone(), e.clone()],
            fields: vec![], variants: vec![("Ok".into(), vec![t]), ("Err".into(), vec![e])],
            props: vec![], methods: vec![] });
    }
    sema.def_id(TypeDef {
        name: "Error".into(), kind: DefKind::Trait, cap: false, params: vec![],
        fields: vec![], variants: vec![],
        props: vec![
            ("message".into(), Ty::Str),
            ("trace".into(), Ty::Str),
        ],
        methods: vec![],
    });
    for n in ["Show", "Eq", "Drop", "Clone", "Hash", "Iter"] {
        sema.def_id(prelude_def(n, false, DefKind::Trait));
    }
    sema.def_id(prelude_def("Cap", true, DefKind::Trait));
    for (n, key) in [("Clock", "time"), ("Fs", "fs"), ("Net", "net"), ("Log", "log")] {
        let id = sema.def_id(prelude_def(n, true, DefKind::Trait));
        sema.cap_key_by_def.insert(id, key.into());
    }
}

// ---------- AST 类型 → Ty ----------

pub struct Lower<'a> {
    pub sema: &'a mut Sema,
    pub params: HashMap<String, Ty>,
}

impl<'a> Lower<'a> {
    pub fn lower(&mut self, t: &ast::Type) -> Ty {
        match t {
            ast::Type::Named { path, args } => {
                if path.len() == 1 && args.is_empty() {
                    if let Some(v) = self.params.get(&path[0]) { return v.clone(); }
                    // 原生标量名 → Ty 原生表示
                    match path[0].as_str() {
                        "I8" => return Ty::Int(IntW::W8), "I16" => return Ty::Int(IntW::W16),
                        "I32" => return Ty::Int(IntW::W32), "I64" => return Ty::Int(IntW::W64),
                        "ISize" => return Ty::Int(IntW::WSize),
                        "U8" => return Ty::UInt(IntW::W8), "U16" => return Ty::UInt(IntW::W16),
                        "U32" => return Ty::UInt(IntW::W32), "U64" => return Ty::UInt(IntW::W64),
                        "USize" => return Ty::UInt(IntW::WSize),
                        "F32" => return Ty::F32, "F64" => return Ty::F64,
                        "Bool" => return Ty::Bool, "Str" => return Ty::Str,
                        "String" => return Ty::String, "Void" => return Ty::Void,
                        "Never" => return Ty::Never,
                        _ => {}
                    }
                }
                let def = self.sema.intern_type_name(&path.join("."));
                let args = args.iter().map(|a| self.lower(a)).collect();
                Ty::Named { def, args }
            }
            ast::Type::Ref(inner) => Ty::Ref(Box::new(self.lower(inner))),
            ast::Type::Slice(inner) => Ty::MutSlice(Box::new(self.lower(inner))),
            ast::Type::Array { elem, .. } => Ty::Array(Box::new(self.lower(elem))),
            ast::Type::Optional(inner) => {
                match self.sema.def_by_name.get("Option") {
                    Some(&d) => Ty::Named { def: d, args: vec![self.lower(inner)] },
                    None => Ty::Err,
                }
            }
            ast::Type::Tuple(tys) => Ty::Tuple(tys.iter().map(|t| self.lower(t)).collect()),
            ast::Type::Fn { params, ret } => Ty::FnTy {
                params: params.iter().map(|t| self.lower(t)).collect(),
                ret: Box::new(match ret { Some(r) => self.lower(r), None => Ty::Void }),
            },
            ast::Type::SelfT => Ty::Err,
            ast::Type::ComptimeVal(text) => Ty::ComptimeVal(text.clone()),
        }
    }
}

// ---------- 包构建 ----------

pub struct ParsedFile {
    pub module: String,
    pub file_idx: usize,
    pub ast: ast::File,
}

pub fn build_package(
    files: &[(String, String)],
    manifest: Option<Manifest>,
    profile: Profile,
) -> (Sema, Vec<(String, Vec<Diagnostic>)>) {
    let mut sema = Sema {
        defs: Vec::new(), def_by_name: HashMap::new(), cap_key_by_def: HashMap::new(),
        fns: Vec::new(), impls: Vec::new(), mods: Vec::new(), mod_by_path: HashMap::new(),
        manifest, profile, var_next: 0,
        runtime_fns: HashMap::new(),
    };
    register_prelude(&mut sema);

    let mut per_module: Vec<(String, Vec<Diagnostic>)> = Vec::new();
    let mut parsed: Vec<ParsedFile> = Vec::new();
    for (mpath, src) in files {
        let (ast_file, diags) = crate::parse_src(src);
        let file_idx = parsed.len();
        parsed.push(ParsedFile { module: mpath.clone(), file_idx, ast: ast_file });
        per_module.push((mpath.clone(), diags));
    }

    // 模块壳(供导入解析寻址)
    for pf in &parsed {
        sema.mod_by_path.insert(pf.module.clone(), sema.mods.len());
        sema.mods.push(SemModule { path: pf.module.clone(), file_idx: pf.file_idx, symbols: HashMap::new() });
    }

    // 循环依赖(E5020)
    {
        let mut edges: HashMap<String, Vec<String>> = HashMap::new();
        for pf in &parsed {
            let mut targets = Vec::new();
            collect_use_targets(&pf.ast, &mut targets);
            edges.insert(pf.module.clone(), targets);
        }
        let mut state: HashMap<String, u8> = HashMap::new();
        for (mpath, _) in files {
            let mut stack = Vec::new();
            dfs_cycles(mpath, &edges, &mut state, &mut stack);
            for cyc in &stack {
                if let Some(idx) = per_module.iter().position(|(m, _)| m == cyc) {
                    per_module[idx].1.push(Diagnostic {
                        code: "E5020",
                        message: "循环依赖(circular import):模块间 use 形成环".into(),
                        span: crate::token::Span::new(1, 1, 0, 0),
                    });
                }
            }
        }
    }

    // 第一遍:本模块声明 → 符号表 + fns/impls + 孤儿规则
    let mut mod_syms: Vec<HashMap<String, Symbol>> = vec![HashMap::new(); parsed.len()];
    // 预建空类型壳(前向引用可解析),待填列表
    let mut pending_fills: Vec<DefFill> = Vec::new();
    for pf in &parsed {
        let mut lower = Lower { sema: &mut sema, params: HashMap::new() };
        pre_register_type_shells(pf, &mut lower, &mut mod_syms[pf.file_idx]);
        collect_own_decls(pf, &mut lower, &mut mod_syms[pf.file_idx], &mut pending_fills, &mut per_module[pf.file_idx].1);
    }
    // 第二遍:填充字段/变体(此时全部类型名已注册,前向引用可解析)
    for fill in pending_fills {
        fill_def_fields(&mut sema, fill);
    }

    // 第二遍:导入解析(跨模块可见性 E2020)
    for pf in &parsed {
        let imports = collect_imports(&pf.ast);
        for p in &imports {
            let Some((sym_name, target)) = p.split_last() else { continue };
            let target_syms = sema.mod_by_path.get(&target.join("."))
                .and_then(|&idx| mod_syms.get(idx))
                .cloned()
                .unwrap_or_default();
            let mut import_diags = Vec::new();
            resolve_import(&sema, &target_syms, target, sym_name, &mut mod_syms[pf.file_idx], &mut import_diags);
            per_module[pf.file_idx].1.extend(import_diags);
        }
    }

    for pf in &parsed {
        sema.mods[pf.file_idx].symbols = std::mem::take(&mut mod_syms[pf.file_idx]);
    }

    (sema, per_module)
}

fn dfs_cycles(node: &str, edges: &HashMap<String, Vec<String>>, state: &mut HashMap<String, u8>, stack: &mut Vec<String>) {
    match state.get(node).copied() {
        Some(0) => { stack.push(node.to_string()); return; }
        Some(_) => return,
        None => {}
    }
    state.insert(node.to_string(), 0);
    if let Some(targets) = edges.get(node) {
        for t in targets.clone() {
            if edges.contains_key(&t) { dfs_cycles(&t, edges, state, stack); }
        }
    }
    state.insert(node.to_string(), 1);
}

fn fill_def_fields(sema: &mut Sema, fill: DefFill) {
    let d = &mut sema.defs[fill.def];
    d.fields = fill.fields;
    d.variants = fill.variants;
}

/// 预建类型壳:全部类型名先注册(前向引用可解析),字段/变体延后填充
fn pre_register_type_shells(pf: &ParsedFile, lower: &mut Lower, syms: &mut HashMap<String, Symbol>) {
    for d in &pf.ast.decls {
        let (name, kind, derives, type_params): (String, DefKind, Vec<String>, &Vec<ast::TypeParam>) = match d {
            ast::Decl::Struct(s) => (s.name.clone(), DefKind::Struct, s.derives.clone(), &s.type_params),
            ast::Decl::Class(c) => (c.name.clone(), DefKind::Class, vec![], &c.type_params),
            ast::Decl::Enum(e) => (e.name.clone(), DefKind::Enum, e.derives.clone(), &e.type_params),
            _ => continue,
        };
        let (param_vars, pvmap) = take_param_vars(lower, type_params);
        drop(pvmap);
        let id = lower.sema.def_id(TypeDef { name: name.clone(), kind, cap: false,
            params: param_vars, fields: vec![], variants: vec![], props: vec![], methods: vec![] });
        for der in &derives {
            lower.sema.impls.push(ImplEntry { module: "@derive".into(), trait_name: der.clone(), for_type: name.clone() });
        }
        syms.insert(name, Symbol::Type(id));
    }
}

fn collect_use_targets(file: &ast::File, out: &mut Vec<String>) {
    for d in &file.decls {
        if let ast::Decl::Use(u) = d {
            for p in &u.imports {
                if p.len() >= 2 { out.push(p[..p.len() - 1].join(".")); }
            }
        }
    }
}

fn collect_imports(file: &ast::File) -> Vec<Vec<String>> {
    let mut out = Vec::new();
    for d in &file.decls {
        if let ast::Decl::Use(u) = d {
            out.extend(u.imports.iter().cloned());
        }
    }
    out
}

struct DefFill {
    def: DefId,
    fields: Vec<(String, Ty, bool)>,
    variants: Vec<(String, Vec<Ty>)>,
}

#[allow(clippy::too_many_arguments)]
fn collect_own_decls(
    pf: &ParsedFile,
    lower: &mut Lower,
    syms: &mut HashMap<String, Symbol>,
    fills: &mut Vec<DefFill>,
    diags: &mut Vec<Diagnostic>,
) {
    for d in &pf.ast.decls {
        match d {
            ast::Decl::Struct(s) => {
                let (param_vars, pvmap) = take_param_vars(lower, &s.type_params);
                let id = lower.sema.def_id(TypeDef { name: s.name.clone(), kind: DefKind::Struct, cap: false,
                    params: param_vars, fields: vec![], variants: vec![], props: vec![], methods: vec![] });
                for d in &s.derives { lower.sema.impls.push(ImplEntry { module: "@derive".into(), trait_name: d.clone(), for_type: s.name.clone() }); }
                let fields = s.fields.iter().map(|fl| (fl.name.clone(), lower.lower(&fl.ty), fl.is_var)).collect();
                fills.push(DefFill { def: id, fields, variants: vec![] });
                drop(pvmap);
                syms.insert(s.name.clone(), Symbol::Type(id));
            }
            ast::Decl::Class(c) => {
                let params = bind_params(lower, &c.type_params);
                let mut fields = Vec::new();
                for item in &c.items {
                    if let ast::ClassItem::Field(fl) = item {
                        fields.push((fl.name.clone(), lower.lower(&fl.ty), fl.is_var));
                    }
                }
                drop(params);
                let id = def_id_user_params(lower.sema, &c.name, DefKind::Class, &[], vec![], fields, vec![]);
                syms.insert(c.name.clone(), Symbol::Type(id));
            }
            ast::Decl::Enum(e) => {
                let (param_vars, pvmap) = take_param_vars(lower, &e.type_params);
                let mut variants = Vec::new();
                for v in &e.variants {
                    match &v.kind {
                        ast::VariantKind::Unit => variants.push((v.name.clone(), vec![])),
                        ast::VariantKind::Tuple(tys) => variants.push((v.name.clone(), tys.iter().map(|t| lower.lower(t)).collect())),
                        ast::VariantKind::Struct(fs) => variants.push((v.name.clone(), fs.iter().map(|fl| lower.lower(&fl.ty)).collect())),
                    }
                }
                drop(pvmap);
                let id = lower.sema.def_id(TypeDef { name: e.name.clone(), kind: DefKind::Enum, cap: false,
                    params: param_vars, fields: vec![], variants, props: vec![], methods: vec![] });
                for d in &e.derives { lower.sema.impls.push(ImplEntry { module: "@derive".into(), trait_name: d.clone(), for_type: e.name.clone() }); }
                syms.insert(e.name.clone(), Symbol::Type(id));
                for (i, v) in e.variants.iter().enumerate() {
                    syms.insert(v.name.clone(), Symbol::Variant { def: id, idx: i });
                }
            }
            ast::Decl::Trait(t) => {
                let cap = t.supers.iter().any(|s| s == "Cap");
                let mut props = Vec::new();
                let mut methods = Vec::new();
                let saved = lower.params.clone();
                let _ = bind_params(lower, &t.type_params);
                for item in &t.items {
                    match item {
                        ast::TraitItem::PropSig(p) | ast::TraitItem::PropImpl(p) => props.push((p.name.clone(), lower.lower(&p.ty))),
                        ast::TraitItem::Method(m) => {
                            let mut params = Vec::new();
                            for p in &m.params {
                                if let ast::Param::Param { ty, .. } = p { params.push(lower.lower(ty)); }
                            }
                            let ret = match &m.ret { Some(r) => lower.lower(r), None => Ty::Void };
                            methods.push((m.name.clone(), FnSig { params, ret }));
                        }
                    }
                }
                lower.params = saved;
                let (param_vars, _) = take_param_vars(lower, &t.type_params);
                let id = lower.sema.def_id(TypeDef {
                    name: t.name.clone(), kind: DefKind::Trait, cap, params: param_vars,
                    fields: vec![], variants: vec![], props, methods,
                });
                syms.insert(t.name.clone(), Symbol::Type(id));
            }
            ast::Decl::Fn(fun) => {
                let id = lower.sema.fns.len();
                let def = lower_fn_def(lower, pf, fun);
                lower.sema.fns.push(def);
                syms.entry(fun.name.clone()).or_insert(Symbol::Fn(id));
            }
            ast::Decl::Const(c) => {
                let ty = lower.lower(&c.ty);
                syms.insert(c.name.clone(), Symbol::Const { ty });
            }
            ast::Decl::Static(s) => {
                let ty = lower.lower(&s.ty);
                syms.insert(s.name.clone(), Symbol::Static { ty });
            }
            ast::Decl::Impl(im) => {
                let trait_name = named_tail(&im.trait_ty);
                let for_type = named_tail(&im.for_ty);
                let trait_local = syms.contains_key(&trait_name) && !lower.sema.is_prelude_name(&trait_name);
                let for_local = syms.contains_key(&for_type) && !lower.sema.is_prelude_name(&for_type);
                if !trait_local && !for_local {
                    diags.push(Diagnostic {
                        code: "E5010",
                        message: format!("孤儿规则(orphan rule)违规:trait `{}` 与类型 `{}` 均非本包定义;在包内定义新类型或等待 trait 演进", trait_name, for_type),
                        span: crate::token::Span::new(1, 1, 0, 0),
                    });
                }
                lower.sema.impls.push(ImplEntry { module: pf.module.clone(), trait_name, for_type });
            }
            _ => {}
        }
    }
}

pub fn named_tail(t: &ast::Type) -> String {
    match t {
        ast::Type::Named { path, .. } => path.last().cloned().unwrap_or_default(),
        _ => String::new(),
    }
}

#[allow(clippy::too_many_arguments)]
fn def_id_user_params(sema: &mut Sema, name: &str, kind: DefKind, derives: &[String], _params: Vec<Ty>,
               fields: Vec<(String, Ty, bool)>, variants: Vec<(String, Vec<Ty>)>) -> DefId {
    for d in derives {
        sema.impls.push(ImplEntry { module: "@derive".into(), trait_name: d.clone(), for_type: name.into() });
    }
    sema.def_id(TypeDef { name: name.into(), kind, cap: false, params: vec![], fields, variants, props: vec![], methods: vec![] })
}

/// 按声明序创建类型参数变量
fn take_param_vars(lower: &mut Lower, tps: &[ast::TypeParam]) -> (Vec<Ty>, HashMap<String, Ty>) {
    let mut ordered = Vec::new();
    let mut map = HashMap::new();
    for tp in tps {
        if tp.is_comptime { continue; }
        let v = lower.sema.new_var();
        lower.params.insert(tp.name.clone(), v.clone());
        map.insert(tp.name.clone(), v.clone());
        ordered.push(v);
    }
    (ordered, map)
}

fn bind_params(lower: &mut Lower, tps: &[ast::TypeParam]) -> HashMap<String, Ty> {
    let mut m = HashMap::new();
    for tp in tps {
        if !tp.is_comptime {
            let v = lower.sema.new_var();
            lower.params.insert(tp.name.clone(), v.clone());
            m.insert(tp.name.clone(), v);
        }
    }
    m
}

fn lower_fn_def(lower: &mut Lower, pf: &ParsedFile, m: &ast::FnDecl) -> FnDef {
    let saved = lower.params.clone();
    let tps = bind_params(lower, &m.type_params);
    // v0.7 修订三:TPar 名→Var 按声明序成对保存(推断求解与显式实参对位用)
    let mut tparam_names = Vec::new();
    let mut tparam_vars = Vec::new();
    for tp in &m.type_params {
        if tp.is_comptime { continue; }
        if let Some(Ty::Var(v)) = tps.get(&tp.name) {
            tparam_names.push(tp.name.clone());
            tparam_vars.push(*v);
        }
    }
    let mut params = Vec::new();
    for p in &m.params {
        if let ast::Param::Param { name, ty, .. } = p {
            params.push((name.clone(), lower.lower(ty)));
        }
    }
    let ret = match &m.ret { Some(r) => lower.lower(r), None => Ty::Void };
    lower.params = saved;
    FnDef {
        module: pf.module.clone(), name: m.name.clone(), vis: m.vis.clone(),
        type_params: tparam_names, tparam_vars, params, ret,
        is_comptime: m.is_comptime,
        no_alloc: m.attrs.iter().any(|a| a.name == "no_alloc"),
        no_spawn: m.attrs.iter().any(|a| a.name == "no_spawn"),
        pure: m.attrs.iter().any(|a| a.name == "pure"),
        body: m.body.clone(),
    }
}

fn resolve_import(
    sema: &Sema,
    target_syms: &HashMap<String, Symbol>,
    target: &[String],
    sym_name: &str,
    syms: &mut HashMap<String, Symbol>,
    diags: &mut Vec<Diagnostic>,
) {
    let head = target.first().map(String::as_str).unwrap_or("");
    match head {
        "std" | "stdweb" => match sym_name {
            "Fs" | "Clock" | "Net" | "Log" => {
                if let Some(&id) = sema.def_by_name.get(sym_name) {
                    syms.insert(sym_name.to_string(), Symbol::Type(id));
                }
            }
            "parallel" => {
                let def = sema.def_by_name.get("Parallel").copied().unwrap_or(usize::MAX);
                syms.insert("parallel".to_string(), Symbol::Const { ty: Ty::Named { def, args: vec![] } });
            }
            other => {
                syms.insert(other.to_string(), Symbol::Module(format!("{}.{}", target.join("."), other)));
            }
        },
        _ => {
            // 本包模块
            if !target_syms.is_empty() {
                let found = target_syms.get(sym_name).cloned();
                match found {
                    Some(sym) => {
                        let visible = match &sym {
                            Symbol::Fn(id) => !matches!(sema.fns[*id].vis, ast::Vis::Private),
                            _ => true,
                        };
                        if visible {
                            syms.insert(sym_name.to_string(), sym);
                        } else {
                            diags.push(Diagnostic {
                                code: "E2020",
                                message: format!("`{}` 未导出(模块私有):pub 或 pub(pkg) 方可跨模块使用", sym_name),
                                span: crate::token::Span::new(1, 1, 0, 0),
                            });
                        }
                    }
                    None => {
                        diags.push(Diagnostic {
                            code: "E2020",
                            message: format!("未解析的名称 `{}`(模块 {} 中不存在)", sym_name, target.join(".")),
                            span: crate::token::Span::new(1, 1, 0, 0),
                        });
                    }
                }
            }
        }
    }
}


/// 公开 prelude 注册(供 interp 使用)
pub fn register_prelude_pub(sema: &mut Sema) { register_prelude(sema); }

/// 简版声明收集(interp 运行时用:只注册 fn 名/type/const/static 值)
pub fn collect_decls_simple(sema: &mut Sema, file: &ast::File, _src: &str) {
    for d in &file.decls {
        match d {
            ast::Decl::Struct(s) => {
                let id = sema.intern_type_name(&s.name);
                let _ = id;
            }
            ast::Decl::Class(c) => { sema.intern_type_name(&c.name); }
            ast::Decl::Enum(e) => {
                let def = sema.intern_type_name(&e.name);
                let variants: Vec<(String, Vec<Ty>)> = Vec::new();
                let _ = variants;
                if let Some(td) = sema.defs.get_mut(def) {
                    for (i, v) in e.variants.iter().enumerate() {
                        td.variants.push((v.name.clone(), vec![]));
                        let _ = i;
                    }
                }
            }
            ast::Decl::Fn(f) => {
                let param_tys: Vec<Ty> = f.params.iter()
                    .filter_map(|p| match p {
                        ast::Param::Param { ty, .. } => Some(sema.lower_ty_pub(ty)),
                        _ => None,
                    }).collect();
                let ret = match &f.ret {
                    Some(r) => sema.lower_ty_pub(r),
                    None => Ty::Void,
                };
                sema.runtime_fns.entry(f.name.clone()).or_insert_with(|| RuntimeFnSig {
                    params: param_tys, ret, is_comptime: f.is_comptime,
                });
            }
            _ => {}
        }
    }
}

#[derive(Debug, Clone)]
pub struct RuntimeFnSig {
    pub params: Vec<Ty>,
    pub ret: Ty,
    pub is_comptime: bool,
}
