// ast.h —— C 版纯数据 AST(字段即契约;arena 分配;无方法)。
// 语义与 docs/spec v0.5 §1.7 一致;C 版独立结构,不与 Rust 版共享代码。
#ifndef CTRON_AST_H
#define CTRON_AST_H

#include <stddef.h>
#include "arena.h"
#include "token.h"

typedef struct cexpr cexpr;
typedef struct cblock cblock;
typedef struct cfn cfn;

typedef enum { VIS_PRIVATE = 0, VIS_PUB, VIS_PUBPKG } cvis;

typedef struct {
    char* name;
    char** args;   // #[name(...)] 原始实参文本;arena 数组,可为空
    size_t nargs;
} cattr;

typedef struct {
    char** segs;   // 一个完整导入路径(组导入已拆为全路径)
    size_t nsegs;
    char* alias;   // 别名(NULL = 无别名;spec 2026-09-22 §3)
} cimport;

// ---------- 类型 ----------

typedef enum {
    TY_NAMED,      // path + args(可为空)
    TY_REF,        // &T
    TY_SLICE,      // T[]
    TY_ARRAY,      // T[N](size 可能缺失)
    TY_OPT,        // T?
    TY_TUPLE,      // (A, B)
    TY_FN,         // fn(A) -> B
    TY_SELF,
    TY_CVAL,       // 泛型值实参(如 Simd[F32, 4] 的 4)
} cty_kind;

typedef struct cty {
    cty_kind kind;
    char** path;
    size_t npath;
    struct cty** args;
    size_t nargs;
    struct cty* sub;         // REF/SLICE/OPT
    struct cty* elem;        // ARRAY
    cexpr* size;             // ARRAY 维度(可能 NULL)
    struct cty** elems;      // TUPLE / FN 参数
    size_t nelems;
    struct cty* fret;        // FN 返回
} cty;

// ---------- 表达式 ----------

typedef enum {
    EX_INT, EX_FLOAT, EX_STR, EX_BOOL, EX_VOID,
    EX_IDENT, EX_TUPLE, EX_ARRAY,
    EX_STRUCT,          // 构造字面量:path + fields
    EX_UNARY, EX_BINARY, EX_RANGE,
    EX_CALL, EX_INDEX, EX_MEMBER,
    EX_TYPEARGS,        // expr[Type, ...]
    EX_TRY,
    EX_CLOSURE,
    EX_SCOPE,           // scope { |s| ... }
    EX_OWN,             // own (arena) { ... }
    EX_IF,
    EX_MATCH,
    EX_BLOCK,
} cexpr_kind;

typedef enum { UN_NEG = 0, UN_NOT } cunop;
typedef enum {
    B_OROR, B_OR, B_AND, B_EQ, B_NE, B_LT, B_GT, B_LE, B_GE,
    B_ADD, B_SUB, B_WADD, B_WSUB, B_MUL, B_DIV, B_MOD,
} cbinop;

typedef struct { char* name; cexpr* value; } cfieldinit; // value = NULL 为简写

typedef struct {
    int is_var;
    char* name;
    cty* ty;             // 可为 NULL
} cclosureparam;


typedef struct cexpr {
    cexpr_kind kind;
    // 字面量载荷
    char* text;          // INT/FLOAT 原文;IDENT 名;VOID 不用
    char* suffix;        // INT/FLOAT 后缀("" 或小写名)
    int bval;            // BOOL
    ctron_str_part* sparts; // STR 部件
    size_t nsparts;
    // 列表
    struct cexpr** elems;  // TUPLE/ARRAY; CALL 的实参
    size_t nelems;
    char** path;           // STRUCT/IDENT 多段? STRUCT 用;MEMBER 的 obj 链不在此
    size_t npath;
    struct cty** targs;    // STRUCT/EX_TYPEARGS 的类型实参
    size_t ntargs;
    cfieldinit* fields;    // STRUCT
    size_t nfields;
    // 一元/二元/range
    cunop uop;
    cexpr* ux;
    cbinop bop;
    cexpr* lhs;
    cexpr* rhs;
    int inclusive;         // RANGE
    cexpr* from;
    cexpr* to;
    cexpr* callee;         // CALL
    cexpr* obj;            // INDEX obj / MEMBER obj / TYPEARGS 基 / TRY 操作数
    cexpr* index;          // INDEX
    int m_is_name;         // MEMBER
    char* mname;           // MEMBER(名字)
    unsigned mtuple;       // MEMBER(元组下标)
    // closure
    cclosureparam* cparams;
    size_t ncparams;
    cty* cret;             // 可为 NULL
    cexpr* cbody;
    // scope / own
    char* sparam;
    cblock* sbody;
    char* arena_name;
    cblock* obody;
    // if
    cexpr* cond;
    cblock* then_b;
    cexpr* els;            // 可为 NULL;If 或 BlockExpr
    // match
    cexpr* scrut;
    struct cmatcharm* arms;
    size_t narms;
    // block expr
    cblock* block;
} cexpr;

typedef struct cpat cpat;

typedef struct cmatcharm { cpat* pat; cexpr* expr; } cmatcharm;

// ---------- 块与语句 ----------

typedef enum { ST_LET, ST_RET, ST_FOR, ST_WHILE, ST_ASSIGN, ST_EXPR, ST_BREAK, ST_CONTINUE } cstmt_kind;
typedef enum { A_EQ, A_ADDEQ, A_SUBEQ, A_MULEQ, A_DIVEQ, A_MODEQ } cassignop;

typedef struct cstmt {
    cstmt_kind kind;
    int is_var;              // LET
    cpat* pat;               // LET/FOR
    cty* ty;                 // LET 类型标注(可 NULL)
    cexpr* e;                // LET 初值 / RET 表达式(NULL=裸 return)
    cexpr* iter;             // FOR
    cblock* body;            // FOR/WHILE
    cexpr* target;           // ASSIGN
    cassignop aop;           // ASSIGN
    cexpr* value;            // ASSIGN
} cstmt;

struct cblock {
    cstmt** stmts;
    size_t nstmts;
    cexpr* tail;             // 块尾表达式(可 NULL)
};

// ---------- 模式 ----------

typedef enum { PAT_IDENT, PAT_WILD, PAT_LIT, PAT_TUPLE, PAT_AGG } cpat_kind;
typedef enum { PLIT_INT, PLIT_FLOAT, PLIT_STR, PLIT_BOOL } cpatlit_kind;
typedef enum { AG_UNIT, AG_TUPLE, AG_STRUCT } cagg_kind;

typedef struct { char* name; cpat* pat; } cstructpatfield; // pat=NULL 为简写

struct cpat {
    cpat_kind kind;
    char* name;              // IDENT / LIT 原文
    cpatlit_kind lkind;      // LIT
    int lb;                  // LIT bool
    struct cpat** elems;     // TUPLE;AG 的元组载荷
    size_t nelems;
    char** path;             // AGG 路径
    size_t npath;
    cagg_kind agg;           // AGG
    cstructpatfield* sfields; // AG_STRUCT
    size_t nsfields;
};

// ---------- 声明 ----------

typedef struct {
    cvis vis;
    int is_var;              // 缺省 let/let = 不可变;var = 可变
    char* name;
    cty* ty;
} cfield;

typedef struct {
    int is_comptime;
    char* name;
    char** bounds;           // bound 路径(join 为 "." 或段?取整串)
    size_t nbounds;
} ctypeparam;

typedef struct {
    cvis vis;
    char* name;
    cty* ty;
    cblock* body;            // prop { ... }:实现;NULL = 仅签名
} cprop;

typedef struct {
    int is_receiver;         // &self / var self
    int is_var;              // receiver 的 var 标志;具名参数的 var
    char* name;              // 具名参数名
    cty* ty;
} cparam;

struct cfn {
    cattr* attrs;
    size_t nattrs;
    cvis vis;
    int is_comptime;
    char* abi;               // extern "c" 的 "c"(可 NULL)
    int variadic;            // 形参表尾 "..."(§9.6 v0.7)
    char* name;
    ctypeparam* type_params;
    size_t ntype_params;
    cparam* params;
    size_t nparams;
    cty* ret;                // 可 NULL
    cblock* body;            // 可 NULL(extern 声明/trait 签名)
};

typedef enum { CT_FIELD, CT_METHOD, CT_PROP } cclassitem_kind;
typedef struct {
    cclassitem_kind kind;
    cfield* f;
    cfn* m;
    cprop* p;
} cclassitem;

typedef enum { TI_METHOD, TI_PROPSIG, TI_PROPIMPL } ctraititem_kind;
typedef struct {
    ctraititem_kind kind;
    cfn* m;
    cprop* p;
} ctraititem;

typedef enum { II_METHOD, II_PROP } cimplitem_kind;
typedef struct {
    cimplitem_kind kind;
    cfn* m;
    cprop* p;
} cimplitem;

typedef struct {
    cimport* imports;
    size_t nimports;
} cuse;

typedef struct {
    cattr* attrs;
    size_t nattrs;
    char** derives;          // @derive 列表
    size_t nderives;
    char* name;
    ctypeparam* type_params;
    size_t ntype_params;
    cfield* fields;
    size_t nfields;
} cstruct;

typedef struct {
    cattr* attrs;
    size_t nattrs;
    char* name;
    ctypeparam* type_params;
    size_t ntype_params;
    cclassitem* items;
    size_t nitems;
} cclass;

typedef enum { VK_UNIT, VK_TUPLE, VK_STRUCT } cvariant_kind;
typedef struct {
    char* name;
    cvariant_kind kind;
    cty** tys;               // VK_TUPLE
    size_t ntys;
    cfield* fields;          // VK_STRUCT
    size_t nfields;
} cvariant;

typedef struct {
    cattr* attrs;
    size_t nattrs;
    char** derives;
    size_t nderives;
    char* name;
    ctypeparam* type_params;
    size_t ntype_params;
    cvariant* variants;
    size_t nvariants;
} cenum;

typedef struct {
    cattr* attrs;
    size_t nattrs;
    char* name;
    ctypeparam* type_params;
    size_t ntype_params;
    char** supers;           // 超 trait 完整路径
    size_t nsupers;
    ctraititem* items;
    size_t nitems;
} ctrait;

typedef struct {
    ctypeparam* type_params;
    size_t ntype_params;
    cty* trait_ty;
    cty* for_ty;
    cimplitem* items;
    size_t nitems;
} cimpl;

typedef struct {
    char* name;
    cty* ty;
    cexpr* expr;
} cconst;

typedef struct {
    char* name;
    cty* ty;
    cexpr* expr;
    int was_var;             // static var 恢复(仅出现于诊断路径)
} cstatic;

typedef struct {
    char* name;
    cblock* body;
} ctest;

typedef enum {
    D_USE, D_STRUCT, D_CLASS, D_ENUM, D_TRAIT, D_IMPL,
    D_FN, D_CONST, D_STATIC, D_TEST,
} cdecl_kind;

typedef struct cdecl {
    cdecl_kind kind;
    cuse use;
    cstruct strukt;
    cclass klass;
    cenum en;
    ctrait trait;
    cimpl impl;
    cfn fn_;
    cconst konst;
    cstatic statik;
    ctest test;
} cdecl;

typedef struct {
    cdecl* decls;
    size_t ndecls;
} cfile;

#endif
