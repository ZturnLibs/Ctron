//! 纯数据 AST:P1-B「核心接口」骨架逐字落地(字段即契约;全部 pub,
//! 全部 derive(Debug, Clone, PartialEq);无方法无 impl)。Task 2-4 的解析器按此构造。

use std::cell::RefCell;
use crate::token::Span;

// ---- span 侧表(P2-D 批①:Expr/Stmt 节点 id → Span;parse 期填充,check 期读)----
// id=0 哨兵:合成节点(无源位);span_of(0) 回落 1:1。
// reset 挂调用方(单文件入口/check_package),不在 parser 内——多文件包逐文件
// reset 会洗掉先解析文件的 span 表。
pub const SYNTHETIC_ID: u32 = 0;

thread_local! {
    static SPANS: RefCell<Vec<Span>> = RefCell::new(Vec::new());
}

pub fn spans_reset() {
    SPANS.with(|s| {
        let mut v = s.borrow_mut();
        v.clear();
        v.push(Span::new(1, 1, 0, 0)); // 槽 0 = 哨兵
    });
}

/// 分配节点 id 并记 span(构造序 = id 序)。
pub fn nid(span: Span) -> u32 {
    SPANS.with(|s| {
        let mut v = s.borrow_mut();
        v.push(span);
        (v.len() - 1) as u32
    })
}

pub fn span_of(id: u32) -> Span {
    SPANS.with(|s| s.borrow().get(id as usize).copied().unwrap_or_else(|| Span::new(1, 1, 0, 0)))
}

#[derive(Debug, Clone, PartialEq)]
pub struct File { pub decls: Vec<Decl> }
#[derive(Debug, Clone, PartialEq)]
pub struct Attribute { pub name: String, pub args: Vec<String> }   // #[no_alloc] / @derive(Json) 的展开
#[derive(Debug, Clone, PartialEq)]
pub enum Decl { Use(UseDecl), Struct(StructDecl), Class(ClassDecl), Enum(EnumDecl),
    Trait(TraitDecl), Impl(ImplDecl), Fn(FnDecl), Const(ConstDecl), Static(StaticDecl), Test(TestDecl) }
#[derive(Debug, Clone, PartialEq)]
pub struct ImportItem { pub segs: Vec<String>, pub alias: Option<String> }   // 别名:Some= Sym as Alias
#[derive(Debug, Clone, PartialEq)]
pub struct UseDecl { pub imports: Vec<ImportItem> }                // 组导入已拆为全路径
#[derive(Debug, Clone, PartialEq)]
pub struct StructDecl { pub attrs: Vec<Attribute>, pub derives: Vec<String>, pub vis: Vis,
    pub name: String, pub type_params: Vec<TypeParam>, pub fields: Vec<Field> }
#[derive(Debug, Clone, PartialEq)]
pub struct ClassDecl { pub attrs: Vec<Attribute>, pub vis: Vis, pub name: String,
    pub type_params: Vec<TypeParam>, pub items: Vec<ClassItem> }
#[derive(Debug, Clone, PartialEq)]
pub enum ClassItem { Field(Field), Method(FnDecl), Prop(PropDecl) }
#[derive(Debug, Clone, PartialEq)]
pub struct EnumDecl { pub attrs: Vec<Attribute>, pub derives: Vec<String>, pub vis: Vis,
    pub name: String, pub type_params: Vec<TypeParam>, pub variants: Vec<Variant> }
#[derive(Debug, Clone, PartialEq)]
pub enum VariantKind { Unit, Tuple(Vec<Type>), Struct(Vec<Field>) }
#[derive(Debug, Clone, PartialEq)]
pub struct Variant { pub name: String, pub kind: VariantKind }
#[derive(Debug, Clone, PartialEq)]
pub struct TraitDecl { pub attrs: Vec<Attribute>, pub vis: Vis, pub name: String,
    pub type_params: Vec<TypeParam>, pub supers: Vec<String>, pub items: Vec<TraitItem> }
#[derive(Debug, Clone, PartialEq)]
pub enum TraitItem { Method(FnDecl), PropSig(PropDecl), PropImpl(PropDecl) }
#[derive(Debug, Clone, PartialEq)]
pub struct PropDecl { pub vis: Vis, pub name: String, pub ty: Type, pub body: Option<Block> }
#[derive(Debug, Clone, PartialEq)]
pub struct ImplDecl { pub type_params: Vec<TypeParam>, pub trait_ty: Type,
    pub for_ty: Type, pub items: Vec<ImplItem> }
#[derive(Debug, Clone, PartialEq)]
pub enum ImplItem { Method(FnDecl), Prop(PropDecl) }
#[derive(Debug, Clone, PartialEq)]
pub struct FnDecl { pub attrs: Vec<Attribute>, pub vis: Vis, pub is_comptime: bool,
    pub abi: Option<String>, pub name: String, pub type_params: Vec<TypeParam>,
    pub params: Vec<Param>, pub ret: Option<Type>, pub body: Option<Block> }
#[derive(Debug, Clone, PartialEq)]
pub enum Param { Receiver { is_var: bool }, Param { is_var: bool, name: String, ty: Type } }
#[derive(Debug, Clone, PartialEq)]
pub struct ConstDecl { pub name: String, pub ty: Type, pub expr: Expr }
#[derive(Debug, Clone, PartialEq)]
pub struct StaticDecl { pub name: String, pub ty: Type, pub expr: Expr, pub was_var: bool }
#[derive(Debug, Clone, PartialEq)]
pub struct TestDecl { pub name: String, pub body: Block }
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Vis { Private, Pub, PubPkg }
#[derive(Debug, Clone, PartialEq)]
pub struct TypeParam { pub name: String, pub bound: Vec<String>, pub is_comptime: bool }
#[derive(Debug, Clone, PartialEq)]
pub struct Field { pub vis: Vis, pub is_var: bool, pub name: String, pub ty: Type }

#[derive(Debug, Clone, PartialEq)]
pub enum Expr {
    Int { text: String, suffix: String, id: u32 }, Float { text: String, suffix: String, id: u32 },
    Str { parts: Vec<StrPart>, id: u32 }, Bool(bool, u32), Void(u32),
    Ident(String, u32), Tuple(Vec<Expr>, u32), Array(Vec<Expr>, u32),
    StructLit { path: Vec<String>, type_args: Vec<Type>, fields: Vec<StructField>, id: u32 },
    Unary { op: UnOp, expr: Box<Expr>, id: u32 },
    Binary { op: BinOp, lhs: Box<Expr>, rhs: Box<Expr>, id: u32 },
    Range { inclusive: bool, from: Box<Expr>, to: Box<Expr>, id: u32 },
    Call { callee: Box<Expr>, args: Vec<Expr>, id: u32 },
    /// W5:repeat 字面量 `[elem] ** N`(1009 裁决:专节点非结合)
    Repeat { elem: Box<Expr>, count: Box<Expr>, id: u32 },
    Index { obj: Box<Expr>, index: Box<Expr>, id: u32 },
    Member { obj: Box<Expr>, target: MemberTarget, id: u32 },     // Name(String) | TupleIndex(u32)
    TypeArgs { expr: Box<Expr>, args: Vec<Type>, id: u32 },
    Try(Box<Expr>, u32),
    Closure { params: Vec<ClosureParam>, ret: Option<Box<Type>>, body: Box<Expr>, id: u32 },
    Scope { param: String, body: Block, id: u32 },
    Own { arena: String, body: Block, id: u32 },
    If { cond: Box<Expr>, then: Block, els: Option<Box<Expr>>, id: u32 },   // els: If 或 BlockExpr
    Match { expr: Box<Expr>, arms: Vec<MatchArm>, id: u32 },
    BlockExpr(Block, u32),
}
#[derive(Debug, Clone, PartialEq)]
pub struct StructField { pub name: String, pub value: Option<Expr> } // 简写 = None
#[derive(Debug, Clone, PartialEq)]
pub struct MatchArm { pub pattern: Pattern, pub guard: Option<Expr>, pub expr: Expr } // guard:模式守卫(R-P3c)
#[derive(Debug, Clone, PartialEq)]
pub enum MemberTarget { Name(String), TupleIndex(u32) }
#[derive(Debug, Clone, PartialEq)]
pub enum UnOp { Neg, Not }
#[derive(Debug, Clone, PartialEq)]
pub enum BinOp { OrOr, Or, AndAnd, Eq, Ne, Lt, Gt, Le, Ge, Add, Sub, WrapAdd, WrapSub, Mul, Div, Mod }
#[derive(Debug, Clone, PartialEq)]
pub struct ClosureParam { pub is_var: bool, pub name: String, pub ty: Option<Type> }
#[derive(Debug, Clone, PartialEq)]
pub enum StrPart { Text(String), Interp(String) }

#[derive(Debug, Clone, PartialEq)]
pub struct Block { pub stmts: Vec<Stmt>, pub tail: Option<Box<Expr>> }
#[derive(Debug, Clone, PartialEq)]
pub enum Stmt {
    Let { is_var: bool, pattern: Pattern, ty: Option<Type>, expr: Expr, id: u32 },
    Return(Option<Expr>, u32),
    For { pattern: Pattern, iter: Expr, body: Block, id: u32 },
    While { cond: Expr, body: Block, id: u32 },
    Assign { target: Expr, op: AssignOp, value: Expr, id: u32 },
    Expr(Expr, u32),
    // v0.7 修订二:语句级、无值、绑同函数体最近循环(§4 提案)
    Break(u32),
    Continue(u32),
}
#[derive(Debug, Clone, PartialEq)]
pub enum AssignOp { Eq, AddEq, SubEq, MulEq, DivEq, ModEq }

#[derive(Debug, Clone, PartialEq)]
pub enum Pattern {
    Ident(String), Wildcard, Lit(PatLit), Tuple(Vec<Pattern>),
    Agg { path: Vec<String>, sub: AggSub },
    /// 或模式(R-P3c):替身依序试配
    Or(Vec<Pattern>),
}
#[derive(Debug, Clone, PartialEq)]
pub enum PatLit { Int(String), Float(String), Str(String), Bool(bool) }
#[derive(Debug, Clone, PartialEq)]
pub enum AggSub { Unit, Tuple(Vec<Pattern>), Struct(Vec<StructPatField>) }
#[derive(Debug, Clone, PartialEq)]
pub struct StructPatField { pub name: String, pub pattern: Option<Pattern> } // 简写 = None

#[derive(Debug, Clone, PartialEq)]
pub enum Type {
    Named { path: Vec<String>, args: Vec<Type> }, Ref(Box<Type>), Slice(Box<Type>),
    Array { elem: Box<Type>, size: Option<Expr> }, Optional(Box<Type>),
    Tuple(Vec<Type>), Fn { params: Vec<Type>, ret: Option<Box<Type>> }, SelfT,
    ComptimeVal(String),   // 泛型值实参(如 Simd[F32, 4] 的 4,§6.4)
}
