//! 纯数据 AST:P1-B「核心接口」骨架逐字落地(字段即契约;全部 pub,
//! 全部 derive(Debug, Clone, PartialEq);无方法无 impl)。Task 2-4 的解析器按此构造。

#[derive(Debug, Clone, PartialEq)]
pub struct File { pub decls: Vec<Decl> }
#[derive(Debug, Clone, PartialEq)]
pub struct Attribute { pub name: String, pub args: Vec<String> }   // #[no_alloc] / @derive(Json) 的展开
#[derive(Debug, Clone, PartialEq)]
pub enum Decl { Use(UseDecl), Struct(StructDecl), Class(ClassDecl), Enum(EnumDecl),
    Trait(TraitDecl), Impl(ImplDecl), Fn(FnDecl), Const(ConstDecl), Static(StaticDecl), Test(TestDecl) }
#[derive(Debug, Clone, PartialEq)]
pub struct UseDecl { pub imports: Vec<Vec<String>> }               // 组导入已拆为全路径
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
    Int { text: String, suffix: String }, Float { text: String, suffix: String },
    Str { parts: Vec<StrPart> }, Bool(bool), Void,
    Ident(String), Tuple(Vec<Expr>), Array(Vec<Expr>),
    StructLit { path: Vec<String>, type_args: Vec<Type>, fields: Vec<StructField> },
    Unary { op: UnOp, expr: Box<Expr> },
    Binary { op: BinOp, lhs: Box<Expr>, rhs: Box<Expr> },
    Range { inclusive: bool, from: Box<Expr>, to: Box<Expr> },
    Call { callee: Box<Expr>, args: Vec<Expr> },
    Index { obj: Box<Expr>, index: Box<Expr> },
    Member { obj: Box<Expr>, target: MemberTarget },     // Name(String) | TupleIndex(u32)
    TypeArgs { expr: Box<Expr>, args: Vec<Type> },
    Try(Box<Expr>),
    Closure { params: Vec<ClosureParam>, ret: Option<Box<Type>>, body: Box<Expr> },
    Scope { param: String, body: Block },
    Own { arena: String, body: Block },
    If { cond: Box<Expr>, then: Block, els: Option<Box<Expr>> },   // els: If 或 BlockExpr
    Match { expr: Box<Expr>, arms: Vec<MatchArm> },
    BlockExpr(Block),
}
#[derive(Debug, Clone, PartialEq)]
pub struct StructField { pub name: String, pub value: Option<Expr> } // 简写 = None
#[derive(Debug, Clone, PartialEq)]
pub struct MatchArm { pub pattern: Pattern, pub expr: Expr }
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
    Let { is_var: bool, pattern: Pattern, ty: Option<Type>, expr: Expr },
    Return(Option<Expr>),
    For { pattern: Pattern, iter: Expr, body: Block },
    While { cond: Expr, body: Block },
    Assign { target: Expr, op: AssignOp, value: Expr },
    Expr(Expr),
    // v0.7 修订二:语句级、无值、绑同函数体最近循环(§4 提案)
    Break,
    Continue,
}
#[derive(Debug, Clone, PartialEq)]
pub enum AssignOp { Eq, AddEq, SubEq, MulEq, DivEq, ModEq }

#[derive(Debug, Clone, PartialEq)]
pub enum Pattern {
    Ident(String), Wildcard, Lit(PatLit), Tuple(Vec<Pattern>),
    Agg { path: Vec<String>, sub: AggSub },
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
