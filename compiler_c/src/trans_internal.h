// trans_internal.h —— C→C 转译后端内部共享契约(模块拆分,原 trans.c 单体)。
// 模块:trans_core(sb/类型模型/上下文) trans_expr(表达式/方法/单态化)
//       trans_conc(并发) trans_stmt(if·match/语句) trans_rt(助手) trans(入口)。
// 语义契约 = rt.c 解释器;跨模块符号一律经本头声明(非 static)。
#ifndef CTRON_TRANS_INTERNAL_H
#define CTRON_TRANS_INTERNAL_H

#include "trans.h"
#include "parser.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ================= 字符串缓冲 =================
typedef struct {
    char* d;
    size_t n, cap;
} sb;

// ================= 类型模型 =================
#define MAX_FIELDS 32
#define MAX_VARIANTS 64
#define MAX_TYPES 64
#define MAX_FNS 256
#define MAX_VARS 128
#define MAX_SCOPES 64
typedef struct tc tc;
typedef struct { int k; int bits, us; int ek, ebits, eus; const char* tname; int ek2, ebits2, eus2; const char* tname2; } ty;
enum { T_UNK, T_INT, T_FLT, T_BOOL, T_STR, T_ARR, T_STRUCT, T_ENUM, T_SUM, T_CLASS, T_BOX, T_RANGE, T_LIST, T_TRAIT, T_ERR, T_SCOPE, T_TASK, T_CHAN, T_MUTEX, T_FNPTR, T_ARENA, T_TUP, T_SIMD };
typedef struct sfield sfield;
typedef struct sdef sdef;
typedef struct evar evar;
typedef struct edef edef;
typedef struct cdef cdef;
struct sfield { char* name; ty t; };
struct sdef { char* name; sfield fields[MAX_FIELDS]; size_t n; };
struct evar { char* name; ty pty; int has_p; };
struct edef { char* name; evar variants[MAX_VARIANTS]; size_t n; };
struct cdef { char* name; sfield fields[MAX_FIELDS]; size_t n; };
typedef struct scope {
    struct { char* name; ty t; } vars[MAX_VARS];
    size_t n;
    struct scope* up;
} scope;

// ================= 上下文 =================
struct tc {
    sb head, body;
    sdef structs[MAX_TYPES];
    size_t nstructs;
    edef enums[MAX_TYPES];
    size_t nenums;
    cdef classes[MAX_TYPES];
    size_t nclasses;
    struct { char* type; char* trait; char* name; const cfn* f; const cprop* p; } methods[128];
    size_t n_methods;
    struct { char* type; char* trait; } impls[64];
    size_t n_impls;
    struct { char* trait; char* name; const cfn* f; const cprop* p; } defaults[64];
    size_t n_defaults;
    char* helpers[160];
    size_t n_helpers;
    char* arrs[32]; // 已用数组元素宽度(ctron_arr_<wl> typedef)
    size_t n_arrs;
    char* sums[64]; // 已用和类型 typedef 全文(ctron_opt_/ctron_res_/ctron_e_)
    size_t n_sums;
    char* traits[64];     // 已声明的 trait 名(&Trait 形参 → T_TRAIT 标记;C10-h)
    size_t n_traits;
    struct { const char* from; ty to; } subs[8]; // 单态化替换:trait 名 → 具体类型
    size_t n_subs;
    char* monos[64];      // 已例化的单态化 cname(去重)
    size_t n_monos;
    char* monoprotos[64]; // 单态化函数原型(进头部,含 ';')
    size_t n_monoprotos;
    sb s_sb;              // 单态化函数体(头部/m_sb 之后、body 之前)
    sb* out_sb;           // 当前函数体输出缓冲(默认 &body;单态化走临时缓冲)
    const cfile* srcf;    // 源文件(单态化按名找 decl)
    sb m_sb;              // 方法体/prop 访问器(发射到 fns 之后、main 之前)
    sb ginit_sb;          // const/static 运行时初始化语句(ctron_ginit 函数体;main 序言调用)
    sb clo_sb;            // 闭包静态函数定义(s_sb 之后、body 之前;值位置闭包)
    char* protos[160];    // 方法/prop/expect 原型
    size_t n_protos;
    char* cells[32];      // 已用 Atomic 元素宽度
    size_t n_cells;
    struct { char* name; ty t; char* init; } globals[32];
    size_t n_globals;
    struct { char* name; ty ret; int nparams; int is_void; ty pty[8]; } fns[MAX_FNS];
    size_t nfns;
    const ty* fn_ret; // 当前函数返回类型提示(?)
    const ty* want;   // 期望类型提示(None/Some/Ok/Err 构造推导)
    int in_main;      // 当前正在发射 fn main(? 早退 = exit 0,对齐 rt run_main)
    scope* sc;
    scope scopes[MAX_SCOPES];
    size_t n_scopes;
    char* err;
    ctron_arena* a;
    int tmpn;
    int in_test;
};

// 构造器(内联;定义唯一)
static inline ty ty_unk(void) { ty t = {T_UNK, 0, 0, 0, 0, 0, NULL, 0, 0, 0, NULL}; t.tname = NULL; return t; }
static inline ty ty_int(int bits, int us) { ty t = {T_INT, bits, us, 0, 0, 0, NULL, 0, 0, 0, NULL}; t.tname = NULL; return t; }
static inline ty ty_flt(void) { ty t = {T_FLT, 64, 0, 0, 0, 0, NULL, 0, 0, 0, NULL}; t.tname = NULL; return t; }
static inline ty ty_bool(void) { ty t = {T_BOOL, 1, 0, 0, 0, 0, NULL, 0, 0, 0, NULL}; t.tname = NULL; return t; }
static inline ty ty_str(void) { ty t = {T_STR, 0, 0, 0, 0, 0, NULL, 0, 0, 0, NULL}; t.tname = NULL; return t; }
static inline ty ty_fnptr(void) { ty t = {T_FNPTR, 0, 0, 0, 0, 0, NULL, 0, 0, 0, NULL}; t.tname = NULL; return t; }
static inline ty ty_arena(void) { ty t = {T_ARENA, 0, 0, 0, 0, 0, NULL, 0, 0, 0, NULL}; t.tname = NULL; return t; }
ty ty_arr(ty elem);

// ================= 函数原型(全量;节内私有助手亦经此声明) =================
void add_fn(tc* c, const char* name, ty ret, int nparams, int is_void, const ty* ptys);
ty box_elem(ty t);
void chan_mk(char* b, size_t n, ty e);
void collect_fns(tc* c, const cfile* f);
void collect_types(tc* c, const cfile* f);
ty ctron_payload_ty(ty t);
ctron_trans_result ctron_trans_file(const cfile* f);
void ctron_trans_result_free(ctron_trans_result* r);
const char* ctype_of(ty t);
ty decl_ty(const cty* t);
ty decl_ty_tc(tc* c, const cty* t);
const char* dt_for_wl(const char* wl);
ty e2_of(ty t);
ty emit_atomic_call(tc* c, cexpr* e, ty mt, const char* rn, sb* o);
void emit_block(tc* c, cblock* b, sb* o);
ty emit_chan_call(tc* c, cexpr* e, const char* rn, sb* o);
void emit_effect_expr(tc* c, cexpr* t, sb* o);
ty emit_expr(tc* c, cexpr* e, sb* o);
void emit_fn(tc* c, const cfn* F, const char* cname);
void emit_helper(tc* c, const char* name);
void emit_if_assign(tc* c, cexpr* e, const char* rn, sb* o);
void emit_if_stmt(tc* c, cexpr* e, sb* o);
void emit_inline_closure_body(tc* c, const cexpr* clo, const char* dest, sb* o);
ty emit_match(tc* c, cexpr* e, sb* o, int want_value, char** out_tmp);
void emit_method_fn(tc* c, const char* type, const cfn* F, const char* cname);
ty emit_mutex_call(tc* c, cexpr* e, ty mt, const char* rn, sb* o);
ty emit_parallel_map(tc* c, cexpr* e, sb* o);
ty emit_parallel_reduce(tc* c, cexpr* e, sb* o);
void emit_prop_accessor(tc* c, const char* type, const cprop* P, const char* cname);
ty emit_scope_expr(tc* c, cexpr* e, sb* o);
ty emit_spawn_call(tc* c, cexpr* e, sb* o);
void emit_stmt(tc* c, cstmt* st, sb* o);
void emit_tail_to(tc* c, cexpr* t, const char* dest, sb* o);
ty emit_task_call(tc* c, cexpr* e, const char* rn, sb* o);
void emit_value_to(tc* c, cexpr* ax, const char* rn, sb* o);
int ensure_method_fn(tc* c, const char* type, const char* name, char* out_cname, ty* out_ret);
int ensure_prop_accessor(tc* c, const char* type, const char* name, char* out_cname, ty* out_ret);
const char* ewlname(ty t);
int find_class(tc* c, const char* n);
const cfn* find_default_fn_for(tc* c, const char* type, const char* name, char** out_trait);
const cprop* find_default_prop_for(tc* c, const char* type, const char* name, char** out_trait);
const cfn* find_impl_method(tc* c, const char* type, const char* name);
const cprop* find_impl_prop(tc* c, const char* type, const char* name);
int fn_has_trait_param(tc* c, const cfn* F);
int fn_lookup(tc* c, const char* name, int* nparams, int* is_void, ty* ret, ty* ptys);
const cfn* fn_named(const cfile* f, const char* nm);
const char* head_name(const cty* t);
ty holder_ty(int k, ty inner);
int is_reserved(const char* n);
int is_void_helper(int iv);
ty probe_closure_ret(tc* c, const cexpr* clo);
ty probe_val_ty(tc* c, cexpr* t);
void rt_chan_type(tc* c, ty elem);
void rt_mutex_type(tc* c, ty inner);
void rt_task_type(tc* c, ty r);
void sb_c(sb* b, char c);
void sb_f(sb* b, const char* fmt, ...);
void sb_free(sb* b);
void sb_s(sb* b, const char* s);
void scope_def(tc* c, const char* name, ty t);
int scope_find(tc* c, const char* name, ty* out);
void scope_pop(tc* c);
void scope_push(tc* c);
ty suff_ty(const char* s);
ty sum_ty_of(tc* c, const cty* t);
void task_mk(char* b, size_t n, ty r);
void terr(tc* c, const char* fmt, ...);
ty ty_arr(ty elem);
char* ty_mangle(tc* c, ty t);
int type_has_drop_m(tc* c, const char* type);
int type_impls_trait(tc* c, const char* type, const char* trait);
void use_arr(tc* c, const char* wl);
void use_helper(tc* c, const char* name);
void use_proto(tc* c, const char* proto);
void use_sum(tc* c, const char* typedef_text);
void wbounds(char* lo, char* hi, size_t n, int bits, int us);
const char* wlname(ty t);

void ensure_mono_fn(tc* c, const cfn* F, const char* cname, const ty* argtys, const ty* atys, int nparams);

// ---- 泛型单态化/元组/derive(Show)(C10-p)----
ty tup_ty(tc* c, ty a, ty b);                       // 二元组 typedef(T_TUP;ek/ek2 承载元素)
ty tup_elem(ty t, int ix);                          // T_TUP 元素类型
int fn_is_generic(const cfn* F);                    // fn 带类型形参([T])
void ensure_mono_generic(tc* c, const cfn* F, const char* cname,
                         char** pnames, ty* pvals, size_t np); // 泛型 fn 调用点例化(subs 驱动)
int ensure_derived_show(tc* c, const char* type, char* out_cname, ty* out_ret, int depth); // derive(Show)/字段可显示合成

#endif
