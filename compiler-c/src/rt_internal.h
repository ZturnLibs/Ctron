// rt_internal.h —— C4 解释器内部共享契约(模块拆分,原 rt.c 单体)。
// 模块:rt_core(值模型/缓冲/数值/环境/字符串/函数调用) rt_eval(域辅助/表达式)
//       rt_stmt(语句/块/入口)。
// 语义契约:panic/断言失败经 setjmp 长跳回 runner;字符串生命周期 = 单 arena。
#ifndef CTRON_RT_INTERNAL_H
#define CTRON_RT_INTERNAL_H

#include "rt.h"
#include "arena.h"
#include "parser.h"

#include <ctype.h>
#include <dirent.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ================= 值 =================
typedef enum { V_INT, V_FLOAT, V_BOOL, V_STR, V_VOID, V_RANGE, V_ARR, V_TAG, V_FN, V_CLOSURE,
                V_STRUCT, V_BOX, V_ERR, V_LIST, V_ATOM, V_TUPLE, V_TASK, V_CHAN, V_MUTEX, V_SCOPE,
                V_SIMD, V_NS } vkind;
typedef struct chan_t chan_t;
typedef struct mutex_t mutex_t;
typedef struct task_t task_t;
typedef struct scope_t scope_t;
typedef struct listnode listnode;
typedef struct atomcell atomcell;
typedef struct vfld vfld;
typedef struct errval errval;
typedef struct boxval boxval;

typedef struct val {
    vkind k;
    __int128 i; // 整数(raw 已归一到位宽;64 位无符号按非负)
    double f;
    int bits;
    int us;
    char* s;
    int64_t lo, hi;
    int inclusive;
    // 复合载荷
    struct val* items; // V_ARR 元素 / V_TAG 载荷
    size_t nitems;
    const char* tag;   // V_TAG(变体名:Some/None/Advance…)
    const cdecl* fnr;  // V_FN(命名函数值)
    const cexpr* clo;  // V_CLOSURE
    struct env* cap;   // V_CLOSURE 捕获环境
    int is_class;      // V_STRUCT
    const char* type;  // V_STRUCT 类型名
    vfld* flds;        // V_STRUCT 字段
    size_t nfld;
    boxval* bx;        // V_BOX
    errval* err;       // V_ERR
    listnode* lst;     // V_LIST
    atomcell* atom;    // V_ATOM
    chan_t* chan;      // V_CHAN(is_sender 存于 us)
    mutex_t* mtx;      // V_MUTEX
    task_t* task;      // V_TASK
    scope_t* scope;    // V_SCOPE
} val;

struct vfld { const char* name; val v; };
struct boxval { val inner; };
struct errval { char* msg; val cause; char* trace; };
struct listnode { struct val* items; size_t n; size_t cap; };
struct atomcell { __int128 v; int bits; int us; };
struct chan_t { int cap; struct listnode q; size_t head; };
struct mutex_t { val inner; };
struct task_t {
    val closure;
    jmp_buf jb;
    int panicked;
    char msg[512];
    int ran;
    val result;
    struct task_t* next;
};
struct scope_t { task_t* tasks; int cancelled; };


// ================= 上下文 =================
typedef struct bind { const char* name; val slot; struct bind* next; } bind;
typedef struct env { bind* head; struct env* up; } env;

typedef struct {
    const cfile* f;
    ctron_arena* a;
    env* top;
    val ret;
    int has_ret;
    jmp_buf jb;
    rt_status st;
    char msg[512];
    size_t tests_run, tests_total;
    val opt_result;
    int has_opt;
    scope_t* scope;
    char* out;
    size_t out_n, out_cap;
    val* consts;          // D_CONST 求值表(按 decl 顺序;const 初始化即 comptime)
    size_t nconsts;
    const char* err_head; // 当前函数 ? 的错误擦除目标(Result[.., E] 的 E 头;AnyError 时启用两段式)
    char* dom_title;      // stdweb.dom 最小锚(set_title/title 往返)
    unsigned long long steps;     // 表达式求值步计数(D2;CTRON_MAX_STEPS 护栏)
    unsigned long long max_steps; // 0 = 无限(C 默认,自举负载);N = 步上限,超限 panic
} rt;




// 解释器输出缓冲(rt 专用,与 trans 的 sb 独立)
typedef struct { char* d; size_t n, cap; } sb;

// ================= 跨模块原型 =================
val apply_decl(rt* R, val v, const cty* ty);
val as_conv(rt* R, val v, const char* n);
void assert_fail(rt* R, const char* what);
char* astr(rt* R, const char* s);
void bind_value(rt* R, cpat* p, val v);
val call_decl(rt* R, const cdecl* fn, cexpr** args, size_t n);
val call_decl_vals(rt* R, const cdecl* fn, val* args, size_t n);
val call_method_body(rt* R, const cfn* F, val self, cexpr** args, size_t nargs);
val call_prop_body(rt* R, const cprop* P, val self);
val chan_recv(rt* R, val recv);
val chan_send(rt* R, val sender, val v);
val ck_int(rt* R, __int128 x, int bits, int us, const char* op);
val clone_val(rt* R, val v);
const cfn* cls_method(const rt* R, const char* cls, const char* m);
const cprop* cls_prop(const rt* R, const char* cls, const char* m);
rt_run ctron_rt_run(const cfile* f);
void ctron_rt_run_free(rt_run* r);
rt_run ctron_rt_run_main(const cfile* f);
int decl_num(const char* n, int* bits, int* us, int* isf);
void drop_scope(rt* R);
bind* env_find(rt* R, const char* name);
void env_let(rt* R, const char* name, val v);
void env_pop(rt* R);
void env_push(rt* R);
const char* err_head_of(const cty* t);
val eval_block(rt* R, cblock* b);
void eval_consts(rt* R);
val eval_expr(rt* R, cexpr* e);
void eval_let(rt* R, cstmt* st);
void eval_stmt(rt* R, cstmt* st);
const cdecl* file_fn(const rt* R, const char* name);
const cdecl* find_kind(const cfile* f, cdecl_kind kd, const char* name);
int fits(__int128 x, int bits, int us);
void fmt_int(char out[80], __int128 x, int us);
void fmt_val(rt* R, val v, sb* b);
const char* head_nm(const cty* t);
val interp_raw(rt* R, const char* raw);
val invoke_val1(rt* R, val fnv, val a);
val invoke_vals(rt* R, val fnv, val* args, size_t n);
int is_variant(const rt* R, const char* name);
val list_clone_deep(rt* R, const listnode* ln);
void list_push(rt* R, listnode* ln, val item);
void lower_suf(const char* n, char out[8]);
val mutex_with(rt* R, val mx, val f, int mut);
int option_builtin(rt* R, val recv, const char* m, cexpr* call);
double parse_flt(const char* t);
__int128 parse_int(const char* t);
int pat_bind(rt* R, cpat* p, val s);
void rt_abort(rt* R, rt_status st, const char* fmt, ...);
unsigned long long rt_env_steps(void); // CTRON_MAX_STEPS(D2;0/未设 = 无限)
void rt_puts(rt* R, const char* s);
int run_next_task(rt* R);
void run_task(rt* R, task_t* t);
void rsb_c(sb* b, char c);
void rsb_s(sb* b, const char* s);
void scope_add_task(rt* R, scope_t* sc, task_t* t);
val str_expr(rt* R, cexpr* e);
void suff_type(const char* s, int* bits, int* us, int* isf);
int tag_is_none(const char* t);
int tag_is_some(const char* t);
int truthy(val v);
int type_has_drop(const rt* R, const char* ty);
val v_arr(val* items, size_t n);
val v_atom(rt* R, __int128 x, int bits, int us);
val v_bool(int b);
val v_box(rt* R, val inner);
val v_chan(rt* R, int cap);
val v_closure(const cexpr* ce, struct env* cap);
val v_err_t(rt* R, const char* msg, val cause, const char* trace);
val v_flt(double f);
val v_fn(const cdecl* d);
val v_int(__int128 x, int bits, int us);
val v_list(rt* R);
val v_mutex(rt* R, val inner);
val v_ns(const char* name);
val v_obj(const char* type, int is_class, vfld* flds, size_t nf);
val v_rng(int64_t lo, int64_t hi, int incl);
val v_scope(rt* R);
val v_str_own(rt* R, const char* s);
val v_tag(const char* tag, val* items, size_t n);
val v_task(rt* R, task_t* t);
val v_tuple(rt* R, val a, val b);
val v_void(void);
int val_eq(rt* R, val a, val b);
val wrap_int(__int128 x, int bits, int us);

#endif
