// rt.c —— C4-a 解释器:数值/逻辑/字符串/范围子集的行为/panic 语料执行器。
// 支持:整宽全集/浮点/布尔/Str;检查算术(溢出 panic)、+%/回绕、除法/取模;
//       比较;&& 短路;if/else 值、块尾值、while/for-range、跨块遮蔽;
//       let/var + 类型自适应;顶层函数/递归/UFCS、.as[T]()、断言内建、插值串。
// panic/断言失败经 setjmp 长跳回 runner;字符串生命周期 = 单 arena。
#include "rt.h"
#include "arena.h"
#include "parser.h"

#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ================= 值 =================
typedef enum { V_INT, V_FLOAT, V_BOOL, V_STR, V_VOID, V_RANGE, V_ARR, V_TAG, V_FN, V_CLOSURE,
                V_STRUCT, V_BOX, V_ERR, V_LIST, V_ATOM, V_TUPLE, V_TASK, V_CHAN, V_MUTEX, V_SCOPE } vkind;
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
struct errval { char* msg; val cause; };
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

static val v_int(__int128 x, int bits, int us) { val v = {0}; v.k = V_INT; v.i = x; v.bits = bits; v.us = us; return v; }
static val v_flt(double f) { val v = {0}; v.k = V_FLOAT; v.f = f; return v; }
static val v_bool(int b) { val v = {0}; v.k = V_BOOL; v.i = b; return v; }
static val v_void(void) { val v = {0}; v.k = V_VOID; return v; }
static val v_rng(int64_t lo, int64_t hi, int incl) { val v = {0}; v.k = V_RANGE; v.lo = lo; v.hi = hi; v.inclusive = incl; return v; }
static val v_arr(val* items, size_t n) { val v = {0}; v.k = V_ARR; v.items = items; v.nitems = n; return v; }
static val v_tag(const char* tag, val* items, size_t n) { val v = {0}; v.k = V_TAG; v.tag = tag; v.items = items; v.nitems = n; return v; }
static val v_fn(const cdecl* d) { val v = {0}; v.k = V_FN; v.fnr = d; return v; }
static val v_obj(const char* type, int is_class, vfld* flds, size_t nf) {
    val v = {0}; v.k = V_STRUCT; v.type = type; v.is_class = is_class; v.flds = flds; v.nfld = nf; return v;
}
static val v_closure(const cexpr* ce, struct env* cap) { val v = {0}; v.k = V_CLOSURE; v.clo = ce; v.cap = cap; return v; }

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
} rt;

static void rt_abort(rt* R, rt_status st, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(R->msg, sizeof R->msg, fmt, ap);
    va_end(ap);
    R->st = st;
    longjmp(R->jb, 1);
}

static void rt_puts(rt* R, const char* s) {
    size_t n = s ? strlen(s) : 0;
    if (!n) return;
    if (R->out_n + n + 1 > R->out_cap) {
        R->out_cap = (R->out_cap ? R->out_cap * 2 : 256);
        while (R->out_n + n + 1 > R->out_cap) R->out_cap *= 2;
        R->out = (char*)realloc(R->out, R->out_cap);
        if (!R->out) abort();
    }
    memcpy(R->out + R->out_n, s, n);
    R->out_n += n;
    R->out[R->out_n] = 0;
}

static val v_box(rt* R, val inner) {
    val v = {0};
    v.k = V_BOX;
    boxval* n = (boxval*)ctron_arena_alloc(R->a, sizeof(boxval));
    n->inner = inner;
    v.bx = n;
    return v;
}
static val clone_val(rt* R, val v);

static val v_str_own(rt* R, const char* s) { val v = {0}; v.k = V_STR; v.s = ctron_arena_strndup(R->a, s, strlen(s)); return v; }

static val v_err(rt* R, const char* msg, val cause) {
    val v = {0};
    v.k = V_ERR;
    errval* e = (errval*)ctron_arena_alloc(R->a, sizeof(errval));
    e->msg = ctron_arena_strndup(R->a, msg, strlen(msg));
    e->cause = cause;
    v.err = e;
    return v;
}

// ================= 字符串缓冲 / 格式化 =================
static char* astr(rt* R, const char* s) { return ctron_arena_strndup(R->a, s, strlen(s)); }

typedef struct { char* d; size_t n, cap; } sb;
static void sb_c(sb* b, char c) {
    if (b->n + 2 > b->cap) { b->cap = b->cap ? b->cap * 2 : 64; b->d = (char*)realloc(b->d, b->cap); if (!b->d) abort(); }
    b->d[b->n++] = c;
    b->d[b->n] = '\0';
}
static void sb_s(sb* b, const char* s) { if (s) while (*s) sb_c(b, *s++); }

static void fmt_int(char out[80], __int128 x, int us) {
    if (!us) { snprintf(out, 80, "%lld", (long long)x); return; }
    unsigned __int128 u = (unsigned __int128)x;
    char t[80]; int n = 0;
    if (u == 0) t[n++] = '0';
    while (u && n < 78) { t[n++] = (char)('0' + (int)(u % 10)); u /= 10; }
    for (int i = 0; i < n; i++) out[i] = t[n - 1 - i];
    out[n] = 0;
}
static void fmt_val(rt* R, val v, sb* b) {
    (void)R;
    char buf[80];
    switch (v.k) {
    case V_INT: fmt_int(buf, v.i, v.us); sb_s(b, buf); break;
    case V_FLOAT:
        if (v.f == (double)(long long)v.f) snprintf(buf, sizeof buf, "%.1f", v.f);
        else snprintf(buf, sizeof buf, "%g", v.f);
        sb_s(b, buf);
        break;
    case V_BOOL: sb_s(b, v.i ? "true" : "false"); break;
    case V_STR: sb_s(b, v.s ? v.s : ""); break;
    case V_VOID: break;
    default: sb_s(b, "<range>"); break;
    }
}

// ================= 数值辅助 =================
static int fits(__int128 x, int bits, int us) {
    if (bits >= 64) return us ? x >= 0 : x >= -((__int128)1 << 63) && x <= ((__int128)1 << 63) - 1;
    if (us) return x >= 0 && x < ((__int128)1 << bits);
    __int128 hi = ((__int128)1 << (bits - 1)) - 1;
    return x >= -hi - 1 && x <= hi;
}
static val ck_int(rt* R, __int128 x, int bits, int us, const char* op) {
    if (!fits(x, bits, us)) rt_abort(R, RT_PANIC, "integer overflow (%s)", op);
    return v_int(x, bits, us);
}
static val wrap_int(__int128 x, int bits, int us) {
    if (bits >= 64) return v_int(x, bits, us);
    unsigned __int128 m = (((unsigned __int128)1) << bits) - 1;
    __int128 r = (__int128)((unsigned __int128)x & m);
    if (!us) { __int128 h = ((__int128)1) << (bits - 1); if (r >= h) r -= ((__int128)1) << bits; }
    return v_int(r, bits, us);
}

static __int128 parse_int(const char* t) {
    const char* p = t;
    int rad = 10;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { rad = 16; p += 2; }
    else if (p[0] == '0' && (p[1] == 'o' || p[1] == 'O')) { rad = 8; p += 2; }
    else if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) { rad = 2; p += 2; }
    unsigned __int128 u = 0;
    for (; *p; p++) {
        if (*p == '_') continue;
        int d = -1;
        if (isdigit((unsigned char)*p)) d = *p - '0';
        else if (*p >= 'a' && *p <= 'f') d = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'F') d = *p - 'A' + 10;
        if (d < 0 || d >= rad) continue;
        u = u * (unsigned)rad + (unsigned)d;
    }
    return (__int128)u;
}
static double parse_flt(const char* t) {
    char buf[160];
    size_t n = 0;
    for (const char* p = t; *p && n < 150; p++)
        if (*p != '_') buf[n++] = *p;
    buf[n] = 0;
    return atof(buf);
}
static void suff_type(const char* s, int* bits, int* us, int* isf) {
    *bits = 32; *us = 0; *isf = 0;
    if (!s || !*s) return;
    if (!strcmp(s, "i8")) *bits = 8;
    else if (!strcmp(s, "i16")) *bits = 16;
    else if (!strcmp(s, "i32")) *bits = 32;
    else if (!strcmp(s, "i64") || !strcmp(s, "isize")) *bits = 64;
    else if (!strcmp(s, "u8")) { *bits = 8; *us = 1; }
    else if (!strcmp(s, "u16")) { *bits = 16; *us = 1; }
    else if (!strcmp(s, "u32")) { *bits = 32; *us = 1; }
    else if (!strcmp(s, "u64") || !strcmp(s, "usize")) { *bits = 64; *us = 1; }
    else if (!strcmp(s, "f32") || !strcmp(s, "f64")) *isf = 1;
}
static void lower_suf(const char* n, char out[8]) {
    size_t k = 0;
    out[k++] = (char)tolower((unsigned char)n[0]);
    for (const char* p = n + 1; *p && k < 7; p++) out[k++] = (char)tolower((unsigned char)*p);
    out[k] = 0;
}
static int decl_num(const char* n, int* bits, int* us, int* isf) {
    if (!n) return 0;
    char c0 = n[0];
    if (c0 != 'I' && c0 != 'U' && c0 != 'F') return 0;
    char s[8];
    lower_suf(n, s);
    suff_type(s, bits, us, isf);
    return 1;
}
static const char* head_nm(const cty* t) {
    if (!t || t->kind != TY_NAMED || t->npath == 0) return NULL;
    return t->path[0];
}

// ================= 环境 =================
static void env_push(rt* R) {
    env* e = (env*)ctron_arena_alloc(R->a, sizeof(env));
    e->up = R->top;
    R->top = e;
}
static void env_pop(rt* R) { if (R->top) R->top = R->top->up; }
static bind* env_find(rt* R, const char* name) {
    for (env* e = R->top; e; e = e->up)
        for (bind* b = e->head; b; b = b->next)
            if (strcmp(b->name, name) == 0) return b;
    return NULL;
}
static void env_let(rt* R, const char* name, val v) {
    bind* b = (bind*)ctron_arena_alloc(R->a, sizeof(bind));
    b->name = name;
    b->slot = (v.k == V_STRUCT && !v.is_class) ? clone_val(R, v) : v;
    b->next = R->top->head;
    R->top->head = b;
}

// ================= 前向 =================
static val eval_expr(rt* R, cexpr* e);
static val eval_block(rt* R, cblock* b);
static void eval_stmt(rt* R, cstmt* st);
static val call_decl(rt* R, const cdecl* fn, cexpr** args, size_t n);
static val interp_raw(rt* R, const char* raw);

// 值按声明数值类型适配(其余原样)
static val apply_decl(rt* R, val v, const cty* ty) {
    const char* n = ty ? head_nm(ty) : NULL;
    if (!n) return v;
    if (!strcmp(n, "Bool") || !strcmp(n, "Str")) return v;
    int bits, us, isf;
    if (decl_num(n, &bits, &us, &isf)) {
        if (isf) return v.k == V_INT ? v_flt((double)v.i) : v_flt(v.f);
        if (v.k == V_INT) return ck_int(R, v.i, bits, us, "decl");
        if (v.k == V_FLOAT) return ck_int(R, (__int128)v.f, bits, us, "decl");
    }
    return v;
}

// .as[T]() 显式转换(截断/回绕)
static val as_conv(rt* R, val v, const char* n) {
    int bits, us, isf;
    if (!decl_num(n, &bits, &us, &isf))
        rt_abort(R, RT_ERROR, "as 目标类型不支持: %s", n);
    if (isf) {
        double d = v.k == V_INT ? (double)v.i : v.f;
        if (v.k == V_FLOAT && bits == 32) d = (double)(float)d;
        return v_flt(d);
    }
    if (v.k == V_FLOAT) return wrap_int((__int128)v.f, bits, us);
    return wrap_int(v.i, bits, us);
}

static int truthy(val v) { return v.k == V_BOOL ? v.i != 0 : v.k == V_INT ? v.i != 0 : v.k == V_FLOAT ? v.f != 0 : 1; }

// 值拷贝语义:struct 值深拷贝;class/Box 共享
static val clone_val(rt* R, val v) {
    if (v.k == V_STRUCT && !v.is_class) {
        vfld* fs = NULL;
        if (v.nfld) {
            fs = (vfld*)ctron_arena_alloc(R->a, v.nfld * sizeof(vfld));
            for (size_t i = 0; i < v.nfld; i++) {
                fs[i].name = v.flds[i].name;
                fs[i].v = clone_val(R, v.flds[i].v);
            }
        }
        return v_obj(v.type, 0, fs, v.nfld);
    }
    if (v.k == V_ARR) {
        val* items = NULL;
        if (v.nitems) {
            items = (val*)ctron_arena_alloc(R->a, v.nitems * sizeof(val));
            for (size_t i = 0; i < v.nitems; i++) items[i] = clone_val(R, v.items[i]);
        }
        return v_arr(items, v.nitems);
    }
    return v;
}

static int tag_is_some(const char* t) { return t && (!strcmp(t, "Some") || !strcmp(t, "Ok")); }
static int tag_is_none(const char* t) { return t && (!strcmp(t, "None") || !strcmp(t, "Err")); }

// ---- 类实例:impl/trait 方法与 prop 解析(按实例类名) ----
static const cdecl* find_kind(const cfile* f, cdecl_kind kd, const char* name);
static const cfn* cls_method(const rt* R, const char* cls, const char* m) {
    for (size_t i = 0; i < R->f->ndecls; i++) {
        const cdecl* d = &R->f->decls[i];
        if (d->kind != D_IMPL) continue;
        const char* fornm = head_nm(d->impl.for_ty);
        if (!fornm || strcmp(fornm, cls) != 0) continue;
        const char* tn = head_nm(d->impl.trait_ty);
        const cdecl* tr = tn ? find_kind(R->f, D_TRAIT, tn) : NULL;
        for (size_t j = 0; j < d->impl.nitems; j++) {
            const cimplitem* it = &d->impl.items[j];
            if (it->kind == II_METHOD && it->m->name && strcmp(it->m->name, m) == 0) return it->m;
        }
        if (tr) {
            for (size_t j = 0; j < tr->trait.nitems; j++) {
                const ctraititem* it = &tr->trait.items[j];
                if (it->kind == TI_METHOD && it->m->body && it->m->name && strcmp(it->m->name, m) == 0)
                    return it->m; // trait 默认方法体
            }
        }
    }
    return NULL;
}
static const cprop* cls_prop(const rt* R, const char* cls, const char* m) {
    for (size_t i = 0; i < R->f->ndecls; i++) {
        const cdecl* d = &R->f->decls[i];
        if (d->kind != D_IMPL) continue;
        const char* fornm = head_nm(d->impl.for_ty);
        if (!fornm || strcmp(fornm, cls) != 0) continue;
        for (size_t j = 0; j < d->impl.nitems; j++) {
            const cimplitem* it = &d->impl.items[j];
            if (it->kind == II_PROP && it->p->name && strcmp(it->p->name, m) == 0 && it->p->body)
                return it->p;
        }
    }
    return NULL;
}
// 以 self=实例调用方法体
static val call_method_body(rt* R, const cfn* F, val self, cexpr** args, size_t nargs) {
    // 参数:receiver(&self)→ self;其余按名
    size_t named = 0;
    for (size_t i = 0; i < F->nparams; i++) if (!F->params[i].is_receiver) named++;
    if (named != nargs) rt_abort(R, RT_ERROR, "方法参数个数: %s", F->name ? F->name : "?");
    env_push(R);
    for (size_t i = 0; i < F->nparams; i++) {
        const cparam* pr = &F->params[i];
        if (pr->is_receiver) env_let(R, "self", self);
    }
    size_t ai = 0;
    for (size_t i = 0; i < F->nparams; i++) {
        const cparam* pr = &F->params[i];
        if (pr->is_receiver) continue;
        val a = eval_expr(R, args[ai++]);
        a = apply_decl(R, a, pr->ty);
        env_let(R, pr->name, a);
    }
    int sr = R->has_ret;
    val srv = R->ret;
    R->has_ret = 0;
    val body = eval_block(R, F->body);
    val res = R->has_ret ? R->ret : body;
    R->has_ret = sr;
    R->ret = srv;
    env_pop(R);
    return res;
}
static val call_prop_body(rt* R, const cprop* P, val self) {
    env_push(R);
    env_let(R, "self", self);
    int sr = R->has_ret;
    val srv = R->ret;
    R->has_ret = 0;
    val body = eval_block(R, P->body);
    val res = R->has_ret ? R->ret : body;
    R->has_ret = sr;
    R->ret = srv;
    env_pop(R);
    return res;
}


static val v_list(rt* R) {
    val v = {0};
    v.k = V_LIST;
    listnode* ln = (listnode*)ctron_arena_alloc(R->a, sizeof(listnode));
    v.lst = ln;
    return v;
}
static val v_atom(rt* R, __int128 x, int bits, int us) {
    val v = {0};
    v.k = V_ATOM;
    atomcell* c = (atomcell*)ctron_arena_alloc(R->a, sizeof(atomcell));
    c->v = x;
    c->bits = bits;
    c->us = us;
    v.atom = c;
    return v;
}
static void list_push(rt* R, listnode* ln, val item) {
    if (ln->n == ln->cap) {
        size_t nc = ln->cap ? ln->cap * 2 : 4;
        val* ni = (val*)ctron_arena_alloc(R->a, nc * sizeof(val));
        for (size_t i = 0; i < ln->n; i++) ni[i] = ln->items[i];
        ln->items = ni;
        ln->cap = nc;
    }
    ln->items[ln->n++] = item;
}
static val list_clone_deep(rt* R, const listnode* ln) {
    val v = v_list(R);
    for (size_t i = 0; i < ln->n; i++) list_push(R, v.lst, clone_val(R, ln->items[i]));
    return v;
}


static val invoke_val1(rt* R, val fnv, val a);

// ---------------- 并发原语 ----------------
static val v_tuple(rt* R, val a, val b) {
    val* it = (val*)ctron_arena_alloc(R->a, 2 * sizeof(val));
    it[0] = a;
    it[1] = b;
    val v = {0};
    v.k = V_TUPLE;
    v.items = it;
    v.nitems = 2;
    return v;
}
static val v_chan(rt* R, int cap) {
    val v = {0};
    v.k = V_CHAN;
    chan_t* c = (chan_t*)ctron_arena_alloc(R->a, sizeof(chan_t));
    c->cap = cap;
    v.chan = c;
    v.us = 1; // 发送端
    return v;
}
static val v_mutex(rt* R, val inner) {
    val v = {0};
    v.k = V_MUTEX;
    mutex_t* m = (mutex_t*)ctron_arena_alloc(R->a, sizeof(mutex_t));
    m->inner = inner;
    v.mtx = m;
    return v;
}
static val v_scope(rt* R) {
    val v = {0};
    v.k = V_SCOPE;
    scope_t* sc = (scope_t*)ctron_arena_alloc(R->a, sizeof(scope_t));
    v.scope = sc;
    return v;
}
static val v_task(rt* R, task_t* t) {
    (void)R;
    val v = {0};
    v.k = V_TASK;
    v.task = t;
    return v;
}
static void scope_add_task(rt* R, scope_t* sc, task_t* t) {
    (void)R;
    t->next = sc->tasks;
    sc->tasks = t;
}

// 运行任务体:任务级捕获 panic
static void run_task(rt* R, task_t* t) {
    if (t->ran) return;
    t->ran = 1;
    jmp_buf saved;
    memcpy(saved, R->jb, sizeof saved);
    if (setjmp(R->jb) == 0) {
        // 以闭包捕获环境执行
        const cexpr* c = t->closure.clo;
        env* saved_top = R->top;
        env* te = (env*)ctron_arena_alloc(R->a, sizeof(env));
        te->head = NULL;
        te->up = t->closure.cap;
        R->top = te;
        int sr = R->has_ret;
        val srv = R->ret;
        R->has_ret = 0;
        if (c->ncparams >= 1 && c->cparams[0].name) env_let(R, c->cparams[0].name, v_void());
        val res = eval_expr(R, c->cbody);
        t->result = R->has_ret ? R->ret : res;
        R->has_ret = sr;
        R->ret = srv;
        R->top = saved_top;
    } else {
        t->panicked = 1;
        snprintf(t->msg, sizeof t->msg, "%s", R->msg);
    }
    memcpy(R->jb, saved, sizeof saved);
}

// 运行一个尚未执行的任务(返回是否运行过)
static int run_next_task(rt* R) {
    if (!R->scope) return 0;
    for (task_t* t = R->scope->tasks; t; t = t->next) {
        if (!t->ran) {
            run_task(R, t);
            return 1;
        }
    }
    return 0;
}

static val chan_send(rt* R, val sender, val v) {
    chan_t* c = sender.chan;
    for (int guard = 0; guard < 8; guard++) {
        if (c->q.n < (size_t)c->cap) {
            list_push(R, &c->q, v);
            val* one = (val*)ctron_arena_alloc(R->a, sizeof(val));
            one[0] = v_void();
            return v_tag("Ok", one, 1);
        }
        if (R->scope && R->scope->cancelled) {
            val* one = (val*)ctron_arena_alloc(R->a, sizeof(val));
            one[0] = v_tag("ScopeCancelled", NULL, 0);
            return v_tag("Err", one, 1);
        }
        if (!run_next_task(R)) {
            // 无其他任务可推进:若已取消则 Err,否则视为死锁(语料不出现)
            val* one = (val*)ctron_arena_alloc(R->a, sizeof(val));
            one[0] = v_tag("ScopeCancelled", NULL, 0);
            return v_tag("Err", one, 1);
        }
    }
    val* one = (val*)ctron_arena_alloc(R->a, sizeof(val));
    one[0] = v_tag("ScopeCancelled", NULL, 0);
    return v_tag("Err", one, 1);
}

static val chan_recv(rt* R, val recv) {
    chan_t* c = recv.chan;
    for (int guard = 0; guard < 8; guard++) {
        if (c->head < c->q.n) {
            val out = c->q.items[c->head++];
            val* one = (val*)ctron_arena_alloc(R->a, sizeof(val));
            one[0] = out;
            return v_tag("Ok", one, 1);
        }
        if (R->scope && R->scope->cancelled) {
            val* one = (val*)ctron_arena_alloc(R->a, sizeof(val));
            one[0] = v_tag("ScopeCancelled", NULL, 0);
            return v_tag("Err", one, 1);
        }
        if (!run_next_task(R)) {
            val* one = (val*)ctron_arena_alloc(R->a, sizeof(val));
            one[0] = v_tag("ScopeCancelled", NULL, 0);
            return v_tag("Err", one, 1);
        }
    }
    val* one = (val*)ctron_arena_alloc(R->a, sizeof(val));
    one[0] = v_tag("ScopeCancelled", NULL, 0);
    return v_tag("Err", one, 1);
}

static val mutex_with(rt* R, val mx, val f, int mut) {
    if (!mut) return invoke_val1(R, f, mx.mtx->inner); // with:只读
    // with_mut:参数绑定副本,闭包体写回存储(整值/类共享皆可)
    if (f.k != V_CLOSURE || !f.clo) rt_abort(R, RT_ERROR, "with_mut 需闭包");
    const cexpr* c = f.clo;
    env* saved = R->top;
    env* ce = (env*)ctron_arena_alloc(R->a, sizeof(env));
    ce->head = NULL;
    ce->up = f.cap;
    R->top = ce;
    const char* pn = c->ncparams >= 1 && c->cparams[0].name ? c->cparams[0].name : "it";
    env_let(R, pn, mx.mtx->inner);
    int sr = R->has_ret;
    val srv = R->ret;
    R->has_ret = 0;
    val res = eval_expr(R, c->cbody);
    if (R->has_ret) res = R->ret;
    bind* bb = env_find(R, pn);
    if (bb) mx.mtx->inner = bb->slot; // 写回
    R->has_ret = sr;
    R->ret = srv;
    R->top = saved;
    return res;
}

static int val_eq(rt* R, val a, val b) {
    (void)R;
    if (a.k == V_INT && b.k == V_INT) return a.i == b.i;
    if (a.k == V_FLOAT && b.k == V_FLOAT) return a.f == b.f;
    if (a.k == V_INT && b.k == V_FLOAT) return (double)a.i == b.f;
    if (a.k == V_FLOAT && b.k == V_INT) return a.f == (double)b.i;
    if (a.k == V_BOOL && b.k == V_BOOL) return a.i == b.i;
    if (a.k == V_STR && b.k == V_STR) return strcmp(a.s ? a.s : "", b.s ? b.s : "") == 0;
    return 0;
}

// ================= 语句 =================
static void bind_value(rt* R, cpat* p, val v);

static void bind_value(rt* R, cpat* p, val v) {
    if (!p) return;
    switch (p->kind) {
    case PAT_IDENT: env_let(R, p->name, v); break;
    case PAT_WILD: break;
    case PAT_TUPLE:
        if (v.k != V_TUPLE && v.k != V_ARR) break;
        for (size_t i = 0; i < p->nelems && i < v.nitems; i++)
            bind_value(R, p->elems[i], v.items[i]);
        break;
    default: break;
    }
}

static void eval_let(rt* R, cstmt* st) {
    if (!st->pat) return;
    if (st->pat->kind == PAT_TUPLE) {
        val v = st->e ? eval_expr(R, st->e) : v_void();
        bind_value(R, st->pat, v);
        return;
    }
    if (st->pat->kind != PAT_IDENT || !st->pat->name) {
        if (st->e) (void)eval_expr(R, st->e);
        return;
    }
    val v = st->e ? eval_expr(R, st->e) : v_void();
    v = apply_decl(R, v, st->ty);
    env_let(R, st->pat->name, v);
}

static void eval_stmt(rt* R, cstmt* st) {
    if (!st) return;
    switch (st->kind) {
    case ST_LET: eval_let(R, st); break;
    case ST_RET: R->ret = st->e ? eval_expr(R, st->e) : v_void(); R->has_ret = 1; break;
    case ST_EXPR: (void)eval_expr(R, st->e); break;
    case ST_ASSIGN: {
        // 索引目标:arr[i] = v / arr[i] op= v(写透共享后备)
        if (st->target && st->target->kind == EX_INDEX
            && st->target->obj && st->target->obj->kind == EX_IDENT) {
            bind* b = env_find(R, st->target->obj->text);
            if (!b) rt_abort(R, RT_ERROR, "未知绑定 %s", st->target->obj->text);
            val* cont = &b->slot;
            val* elem = NULL;
            if (cont->k == V_ARR || cont->k == V_LIST) {
                val ix = eval_expr(R, st->target->index);
                long long idx = (long long)ix.i;
                if (idx < 0 || (unsigned long long)idx >= cont->nitems
                    || (cont->k == V_LIST && (size_t)idx >= cont->lst->n))
                    rt_abort(R, RT_PANIC, "index out of bounds");
                if (cont->k == V_ARR) elem = &cont->items[idx];
                else elem = &cont->lst->items[idx];
            } else rt_abort(R, RT_ERROR, "索引赋值目标非数组/列表");
            val r = eval_expr(R, st->value);
            if (st->aop == A_EQ) {
                if (elem->k == V_INT && r.k == V_INT) r = ck_int(R, r.i, elem->bits, elem->us, "=");
                else if (elem->k == V_INT && r.k == V_FLOAT) r = ck_int(R, (__int128)r.f, elem->bits, elem->us, "=");
                *elem = r;
                break;
            }
            val l = *elem;
            __int128 x = 0;
            switch (st->aop) {
            case A_ADDEQ: x = l.i + r.i; break;
            case A_SUBEQ: x = l.i - r.i; break;
            case A_MULEQ: x = l.i * r.i; break;
            case A_DIVEQ:
                if (r.i == 0) rt_abort(R, RT_PANIC, "division by zero (/=)");
                x = l.i / r.i;
                break;
            case A_MODEQ:
                if (r.i == 0) rt_abort(R, RT_PANIC, "division by zero (%=)");
                x = l.i % r.i;
                break;
            default: rt_abort(R, RT_ERROR, "索引赋值运算符"); break;
            }
            *elem = ck_int(R, x, l.bits, l.us, "idx assign");
            break;
        }
        // 成员目标:b.x = v / b.x op= v
        if (st->target && st->target->kind == EX_MEMBER && st->target->m_is_name
            && st->target->obj && st->target->obj->kind == EX_IDENT) {
            bind* b = env_find(R, st->target->obj->text);
            if (!b) rt_abort(R, RT_ERROR, "未知绑定 %s", st->target->obj->text);
            val* objv = &b->slot;
            val target = *objv;
            if (target.k == V_BOX && target.bx) { target = target.bx->inner; objv = &target; }
            if (target.k != V_STRUCT)
                rt_abort(R, RT_ERROR, "成员赋值目标需为 struct/class: %s", st->target->mname);
            vfld* f = NULL;
            for (size_t i = 0; i < target.nfld; i++)
                if (strcmp(target.flds[i].name, st->target->mname) == 0) { f = &target.flds[i]; break; }
            if (!f) rt_abort(R, RT_ERROR, "字段不存在: %s", st->target->mname);
            val r = eval_expr(R, st->value);
            if (st->aop == A_EQ) {
                if (f->v.k == V_INT && r.k == V_INT) r = ck_int(R, r.i, f->v.bits, f->v.us, "=");
                else if (f->v.k == V_INT && r.k == V_FLOAT) r = ck_int(R, (__int128)r.f, f->v.bits, f->v.us, "=");
                f->v = (r.k == V_STRUCT && !r.is_class) ? clone_val(R, r) : r;
                if (st->target->obj->kind == EX_IDENT) {
                    // 写回(对象本身是绑定内槽,直接改已生效;仅 struct 值独立副本)
                }
                break;
            }
            val l = f->v;
            __int128 x = 0;
            switch (st->aop) {
            case A_ADDEQ: x = l.i + r.i; break;
            case A_SUBEQ: x = l.i - r.i; break;
            case A_MULEQ: x = l.i * r.i; break;
            case A_DIVEQ:
                if (r.i == 0) rt_abort(R, RT_PANIC, "division by zero (/=)");
                x = l.i / r.i;
                break;
            case A_MODEQ:
                if (r.i == 0) rt_abort(R, RT_PANIC, "division by zero (%=)");
                x = l.i % r.i;
                break;
            default: rt_abort(R, RT_ERROR, "赋值运算符"); break;
            }
            f->v = ck_int(R, x, l.bits, l.us, "member assign");
            break;
        }
        if (st->target && st->target->kind == EX_IDENT) {
            bind* b = env_find(R, st->target->text);
            if (!b) rt_abort(R, RT_ERROR, "未知绑定 %s", st->target->text);
            val l = b->slot;
            val r = eval_expr(R, st->value);
            if (st->aop == A_EQ) {
                if (l.k == V_INT && r.k == V_INT) r = ck_int(R, r.i, l.bits, l.us, "=");
                else if (l.k == V_INT && r.k == V_FLOAT) r = ck_int(R, (__int128)r.f, l.bits, l.us, "=");
                b->slot = (r.k == V_STRUCT && !r.is_class) ? clone_val(R, r) : r;
                break;
            }
            __int128 x;
            int wrap = 0;
            switch (st->aop) {
            case A_ADDEQ: x = l.i + r.i; break;
            case A_SUBEQ: x = l.i - r.i; break;
            case A_MULEQ: x = l.i * r.i; break;
            case A_DIVEQ:
                if (r.i == 0) rt_abort(R, RT_PANIC, "division by zero (/=)");
                x = l.i / r.i;
                break;
            case A_MODEQ:
                if (r.i == 0) rt_abort(R, RT_PANIC, "division by zero (%=)");
                x = l.i % r.i;
                break;
            default: rt_abort(R, RT_ERROR, "赋值运算符"); return;
            }
            (void)wrap;
            b->slot = ck_int(R, x, l.bits, l.us, "assign");
        } else {
            if (st->target) (void)eval_expr(R, st->target);
            if (st->value) (void)eval_expr(R, st->value);
        }
        break;
    }
    case ST_FOR: {
        if (!st->iter) rt_abort(R, RT_ERROR, "for 缺迭代");
        int wild = st->pat && st->pat->kind == PAT_WILD;
        if (st->pat && st->pat->kind != PAT_IDENT && st->pat->kind != PAT_WILD)
            rt_abort(R, RT_ERROR, "for 模式不支持");
        val it = eval_expr(R, st->iter);
        env_push(R);
        bind* iv = NULL;
        if (!wild) {
            env_let(R, st->pat->name, v_int(0, 32, 0));
            iv = env_find(R, st->pat->name);
        }
        if (it.k == V_RANGE) {
            for (int64_t cur = it.lo; (it.inclusive ? cur <= it.hi : cur < it.hi); cur++) {
                if (iv) iv->slot = v_int(cur, 32, 0);
                (void)eval_block(R, st->body);
                if (R->has_ret) break;
            }
        } else if (it.k == V_ARR || it.k == V_TUPLE) {
            for (size_t i = 0; i < it.nitems; i++) {
                if (iv) iv->slot = it.items[i];
                (void)eval_block(R, st->body);
                if (R->has_ret) break;
            }
        } else {
            env_pop(R);
            rt_abort(R, RT_ERROR, "for 需要 range 或数组");
        }
        env_pop(R);
        break;
    }
    case ST_WHILE: {
        while (!R->has_ret) {
            val c = eval_expr(R, st->e);
            if (!truthy(c)) break;
            (void)eval_block(R, st->body);
        }
        break;
    }
    }
}

// ================= 块 =================
// Drop impl 探测与作用域退出逆序 drop(RAII,§6.4)
static int type_has_drop(const rt* R, const char* ty) {
    if (!ty) return 0;
    for (size_t i = 0; i < R->f->ndecls; i++) {
        const cdecl* d = &R->f->decls[i];
        if (d->kind != D_IMPL) continue;
        const char* tn = head_nm(d->impl.trait_ty);
        const char* fn2 = head_nm(d->impl.for_ty);
        if (tn && fn2 && strcmp(tn, "Drop") == 0 && strcmp(fn2, ty) == 0) return 1;
    }
    return 0;
}
static void drop_scope(rt* R) {
    if (!R->top) return;
    for (bind* b = R->top->head; b; b = b->next) {
        if (b->slot.k != V_STRUCT || !type_has_drop(R, b->slot.type)) continue;
        const cfn* F = cls_method(R, b->slot.type, "drop");
        if (F && F->body) (void)call_method_body(R, F, b->slot, NULL, 0);
    }
}

static val eval_block(rt* R, cblock* b) {
    val tail = v_void();
    if (!b) return tail;
    env_push(R);
    for (size_t i = 0; i < b->nstmts && !R->has_ret; i++)
        eval_stmt(R, b->stmts[i]);
    if (!R->has_ret && b->tail) tail = eval_expr(R, b->tail);
    drop_scope(R); // 本帧声明逆序 drop
    env_pop(R);
    return R->has_ret ? R->ret : tail;
}

// ================= 字符串(文本+插值) =================
static val interp_raw(rt* R, const char* raw) {
    size_t n = strlen(raw);
    char* tmp = (char*)malloc(n + 64);
    snprintf(tmp, n + 64, "fn __interp() { return %s }", raw);
    ctron_parse_result pr = ctron_parse_src(tmp, strlen(tmp));
    free(tmp);
    val out = v_void();
    if (pr.file && pr.file->ndecls >= 1 && pr.file->decls[0].kind == D_FN) {
        cblock* body = pr.file->decls[0].fn_.body;
        if (body) {
            int sr = R->has_ret;
            val rv = R->ret;
            R->has_ret = 0;
            out = eval_block(R, body);
            if (R->has_ret) out = R->ret;
            R->has_ret = sr;
            R->ret = rv;
        }
    }
    ctron_parse_result_free(&pr);
    return out;
}

static val str_expr(rt* R, cexpr* e) {
    sb b = {0};
    for (size_t i = 0; i < e->nsparts; i++) {
        const ctron_str_part* p = &e->sparts[i];
        if (p->kind == PART_TEXT) sb_s(&b, p->s ? p->s : "");
        else {
            val v = interp_raw(R, p->s);
            sb tmp = {0};
            fmt_val(R, v, &tmp);
            sb_s(&b, tmp.d ? tmp.d : "");
            free(tmp.d);
        }
    }
    char* s = b.d ? b.d : astr(R, "");
    val out = v_bool(0);
    out.k = V_STR;
    out.s = astr(R, s);
    free(b.d);
    return out;
}

// ================= 函数与断言 =================
static const cdecl* find_kind(const cfile* f, cdecl_kind kd, const char* name) {
    if (!name) return NULL;
    for (size_t i = 0; i < f->ndecls; i++) {
        const cdecl* d = &f->decls[i];
        if (d->kind != kd) continue;
        const char* nm = kd == D_STRUCT ? d->strukt.name
                        : kd == D_CLASS ? d->klass.name
                        : kd == D_TRAIT ? d->trait.name
                        : kd == D_ENUM ? d->en.name : NULL;
        if (nm && strcmp(nm, name) == 0) return d;
    }
    return NULL;
}
static const cdecl* file_fn(const rt* R, const char* name) {
    for (size_t i = 0; i < R->f->ndecls; i++) {
        const cdecl* d = &R->f->decls[i];
        if (d->kind == D_FN && strcmp(d->fn_.name, name) == 0) return d;
    }
    return NULL;
}

// 标签构造名:Some/None/Ok/Err + 本文件枚举变体
static int is_variant(const rt* R, const char* name) {
    if (!name) return 0;
    if (!strcmp(name, "Some") || !strcmp(name, "None") || !strcmp(name, "Ok") || !strcmp(name, "Err")) return 1;
    for (size_t i = 0; i < R->f->ndecls; i++) {
        const cdecl* d = &R->f->decls[i];
        if (d->kind != D_ENUM) continue;
        for (size_t j = 0; j < d->en.nvariants; j++)
            if (strcmp(d->en.variants[j].name, name) == 0) return 1;
    }
    return 0;
}

// 以既有实参值调用具名函数(用于 UFCS 与 map 路径)
static val call_decl_vals(rt* R, const cdecl* fn, val* args, size_t n) {
    const cfn* F = &fn->fn_;
    if (F->nparams != n) rt_abort(R, RT_ERROR, "参数个数: %s 期望 %zu 实得 %zu", F->name, F->nparams, n);
    env_push(R);
    for (size_t i = 0; i < n; i++) {
        val a = args[i];
        a = apply_decl(R, a, F->params[i].ty);
        env_let(R, F->params[i].name, a);
    }
    int sr = R->has_ret;
    val srv = R->ret;
    R->has_ret = 0;
    val body = eval_block(R, F->body);
    val res = R->has_ret ? R->ret : body;
    R->has_ret = sr;
    R->ret = srv;
    env_pop(R);
    return res;
}

// 多/单实参调用函数值(命名函数或闭包)
static val invoke_vals(rt* R, val fnv, val* args, size_t n) {
    if (fnv.k == V_FN) return call_decl_vals(R, fnv.fnr, args, n);
    if (fnv.k == V_CLOSURE) {
        const cexpr* c = fnv.clo;
        if (n != c->ncparams) rt_abort(R, RT_ERROR, "闭包参数个数");
        env* saved = R->top;
        env* callee_env = (env*)ctron_arena_alloc(R->a, sizeof(env));
        callee_env->head = NULL;
        callee_env->up = fnv.cap;
        R->top = callee_env;
        for (size_t i = 0; i < n; i++)
            if (c->cparams[i].name) env_let(R, c->cparams[i].name, args[i]);
        val r = eval_expr(R, c->cbody);
        R->top = saved;
        return r;
    }
    rt_abort(R, RT_ERROR, "调用目标非函数值");
    return v_void();
}
static val invoke_val1(rt* R, val fnv, val a) {
    val args[1];
    args[0] = a;
    return invoke_vals(R, fnv, args, 1);
}

// 选项/结果方法内建(map/or/expect/context/is_some);命中置 R->opt_result
static int option_builtin(rt* R, val recv, const char* m, cexpr* call) {
    if (recv.k != V_TAG) return 0;
    int is_t = tag_is_some(recv.tag) || tag_is_none(recv.tag);
    if (!is_t) return 0;
    int some = tag_is_some(recv.tag);
    if (!strcmp(m, "or")) {
        if (call->nelems != 1) rt_abort(R, RT_ERROR, "or 实参");
        if (some && recv.nitems == 1) { R->opt_result = recv.items[0]; R->has_opt = 1; return 1; }
        R->opt_result = eval_expr(R, call->elems[0]);
        R->has_opt = 1;
        return 1;
    }
    if (!strcmp(m, "map")) {
        if (call->nelems != 1) rt_abort(R, RT_ERROR, "map 实参");
        val f = eval_expr(R, call->elems[0]);
        if (!some) { R->opt_result = recv; R->has_opt = 1; return 1; }
        if (recv.nitems != 1) rt_abort(R, RT_ERROR, "map 载荷");
        val r = invoke_val1(R, f, recv.items[0]);
        val* one = (val*)ctron_arena_alloc(R->a, sizeof(val));
        one[0] = r;
        R->opt_result = v_tag(some && !strcmp(recv.tag, "Some") ? "Some" : "Ok", one, 1);
        R->has_opt = 1;
        return 1;
    }
    if (!strcmp(m, "expect")) {
        if (some && recv.nitems == 1) { R->opt_result = recv.items[0]; R->has_opt = 1; return 1; }
        char msg[128] = "expect failed";
        if (call->nelems == 1) {
            val mv = eval_expr(R, call->elems[0]);
            if (mv.k == V_STR && mv.s) snprintf(msg, sizeof msg, "expect failed: %s", mv.s);
        }
        rt_abort(R, RT_PANIC, "%s", msg);
    }
    if (!strcmp(m, "is_some") || !strcmp(m, "is_ok")) {
        R->opt_result = v_bool(some);
        R->has_opt = 1;
        return 1;
    }
    if (!strcmp(m, "context")) {
        // Result 专用:Err 包成错误链;Ok 原样
        if (strcmp(recv.tag, "Err") == 0) {
            val msg = call->nelems == 1 ? eval_expr(R, call->elems[0]) : v_bool(0);
            const char* txt = (msg.k == V_STR && msg.s) ? msg.s : "";
            val payload = recv.nitems >= 1 ? recv.items[0] : v_void();
            val* cause_payload = (val*)ctron_arena_alloc(R->a, sizeof(val));
            cause_payload[0] = payload;
            val* one = (val*)ctron_arena_alloc(R->a, sizeof(val));
            one[0] = v_err(R, txt, v_tag("Some", cause_payload, 1));
            R->opt_result = v_tag("Err", one, 1);
            R->has_opt = 1;
            return 1;
        }
        R->opt_result = recv;
        R->has_opt = 1;
        return 1;
    }
    return 0;
}

static int pat_bind(rt* R, cpat* p, val s) {
    if (!p) return 0;
    switch (p->kind) {
    case PAT_WILD: return 1;
    case PAT_IDENT: env_let(R, p->name, s); return 1;
    case PAT_LIT: {
        if (s.k == V_INT && p->lkind == PLIT_INT) return s.i == parse_int(p->name);
        if (s.k == V_STR && p->lkind == PLIT_STR) return s.s && p->name && strcmp(s.s, p->name) == 0;
        if (s.k == V_BOOL && p->lkind == PLIT_BOOL) return s.i == (p->lb ? 1 : 0);
        return 0;
    }
    case PAT_TUPLE: return 0; // 本域不涉
    case PAT_AGG: {
        if (p->agg == AG_STRUCT) {
            if (s.k == V_BOX && s.bx) s = s.bx->inner;
            if (s.k != V_STRUCT) return 0;
            for (size_t i = 0; i < p->nsfields; i++) {
                const cstructpatfield* f = &p->sfields[i];
                val fv = v_void();
                int found = 0;
                for (size_t j = 0; j < s.nfld; j++)
                    if (strcmp(s.flds[j].name, f->name) == 0) { fv = s.flds[j].v; found = 1; break; }
                if (!found) return 0;
                if (!f->pat) env_let(R, f->name, fv);
                else if (!pat_bind(R, f->pat, fv)) return 0;
            }
            return 1;
        }
        if (s.k != V_TAG || !p->path || p->npath == 0 || strcmp(p->path[0], s.tag) != 0) return 0;
        if (p->agg == AG_UNIT) return s.nitems == 0;
        if (p->agg == AG_TUPLE) {
            if (p->nelems != s.nitems) return 0;
            for (size_t i = 0; i < p->nelems; i++)
                if (!pat_bind(R, p->elems[i], s.items[i])) return 0;
            return 1;
        }
        return 0;
    }
    default: return 0;
    }
}

static val call_decl(rt* R, const cdecl* fn, cexpr** args, size_t n) {
    const cfn* F = &fn->fn_;
    if (F->nparams != n) rt_abort(R, RT_ERROR, "参数个数: %s 期望 %zu 实得 %zu",
                                   F->name, F->nparams, n);
    env_push(R);
    for (size_t i = 0; i < n; i++) {
        val a = eval_expr(R, args[i]);
        const cparam* pr = &F->params[i];
        a = apply_decl(R, a, pr->ty);
        env_let(R, pr->name, a);
    }
    int save_ret = R->has_ret;
    val save_retv = R->ret;
    R->has_ret = 0;
    val body = eval_block(R, F->body);
    val res = R->has_ret ? R->ret : body;
    R->has_ret = save_ret;
    R->ret = save_retv;
    env_pop(R);
    return res;
}

static void assert_fail(rt* R, const char* what) { rt_abort(R, RT_ASSERT_FAIL, "%s", what); }

// ================= 表达式 =================
static val eval_expr(rt* R, cexpr* e) {
    if (!e) return v_void();
    switch (e->kind) {
    case EX_INT: {
        int bits = 32, us = 0, isf = 0;
        suff_type(e->suffix, &bits, &us, &isf);
        if (isf) return v_flt(parse_flt(e->text));
        return v_int(parse_int(e->text), bits, us);
    }
    case EX_FLOAT: return v_flt(parse_flt(e->text));
    case EX_STR: return str_expr(R, e);
    case EX_BOOL: return v_bool(e->bval);
    case EX_VOID: return v_void();
    case EX_IDENT: {
        bind* b = env_find(R, e->text);
        if (b) return b->slot;
        const cdecl* d = file_fn(R, e->text);
        if (d) return v_fn(d); // 一等函数值(如 map(twice))
        for (size_t i = 0; i < R->f->ndecls; i++) {
            const cdecl* sd = &R->f->decls[i];
            if (sd->kind == D_STATIC && sd->statik.name && strcmp(sd->statik.name, e->text) == 0)
                return eval_expr(R, sd->statik.expr); // static let 只读全局
        }
        if (is_variant(R, e->text)) return v_tag(e->text, NULL, 0); // 裸变体值(如 None)
        rt_abort(R, RT_ERROR, "未解析名称: %s", e->text);
    }
    case EX_UNARY: {
        val x = eval_expr(R, e->ux);
        if (e->uop == UN_NOT) return v_bool(!truthy(x));
        if (x.k == V_INT) return ck_int(R, -x.i, x.bits, x.us, "neg");
        if (x.k == V_FLOAT) return v_flt(-x.f);
        return x;
    }
    case EX_BINARY: {
        if (e->bop == B_AND) {
            val l = eval_expr(R, e->lhs);
            return truthy(l) ? v_bool(truthy(eval_expr(R, e->rhs))) : v_bool(0);
        }
        val l = eval_expr(R, e->lhs);
        val r = eval_expr(R, e->rhs);
        switch (e->bop) {
        case B_ADD:
            if (l.k == V_STR && r.k == V_STR) {
                size_t a1 = l.s ? strlen(l.s) : 0, b1 = r.s ? strlen(r.s) : 0;
                char* c2 = (char*)ctron_arena_alloc(R->a, a1 + b1 + 1);
                if (a1) memcpy(c2, l.s, a1);
                if (b1) memcpy(c2 + a1, r.s, b1);
                c2[a1 + b1] = 0;
                val o = {0};
                o.k = V_STR;
                o.s = c2;
                return o;
            }
            if (l.k == V_FLOAT || r.k == V_FLOAT) return v_flt((l.k == V_FLOAT ? l.f : (double)l.i) + (r.k == V_FLOAT ? r.f : (double)r.i));
            return ck_int(R, l.i + r.i, l.bits, l.us, "+");
        case B_SUB:
            if (l.k == V_FLOAT || r.k == V_FLOAT) return v_flt((l.k == V_FLOAT ? l.f : (double)l.i) - (r.k == V_FLOAT ? r.f : (double)r.i));
            return ck_int(R, l.i - r.i, l.bits, l.us, "-");
        case B_MUL:
            if (l.k == V_FLOAT || r.k == V_FLOAT) return v_flt((l.k == V_FLOAT ? l.f : (double)l.i) * (r.k == V_FLOAT ? r.f : (double)r.i));
            return ck_int(R, l.i * r.i, l.bits, l.us, "*");
        case B_DIV:
            if (l.k == V_FLOAT || r.k == V_FLOAT) {
                double rd = r.k == V_FLOAT ? r.f : (double)r.i;
                if (rd == 0.0) return v_flt(l.k == V_FLOAT ? l.f / 0.0 : (double)l.i / 0.0);
                return v_flt((l.k == V_FLOAT ? l.f : (double)l.i) / rd);
            }
            if (r.i == 0) rt_abort(R, RT_PANIC, "division by zero (/)");
            return ck_int(R, l.i / r.i, l.bits, l.us, "/");
        case B_MOD:
            if (r.i == 0) rt_abort(R, RT_PANIC, "division by zero (%)");
            return ck_int(R, l.i % r.i, l.bits, l.us, "%");
        case B_WADD: return wrap_int(l.i + r.i, l.bits, l.us);
        case B_WSUB: return wrap_int(l.i - r.i, l.bits, l.us);
        case B_EQ: return v_bool(val_eq(R, l, r));
        case B_NE: return v_bool(!val_eq(R, l, r));
        case B_LT: case B_GT: case B_LE: case B_GE: {
            int c;
            if (l.k == V_STR && r.k == V_STR) c = strcmp(l.s, r.s);
            else if (l.k == V_FLOAT || r.k == V_FLOAT) {
                double a = l.k == V_FLOAT ? l.f : (double)l.i, b2 = r.k == V_FLOAT ? r.f : (double)r.i;
                c = a < b2 ? -1 : a > b2 ? 1 : 0;
            } else c = l.i < r.i ? -1 : l.i > r.i ? 1 : 0;
            int t = e->bop == B_LT ? c < 0 : e->bop == B_GT ? c > 0 : e->bop == B_LE ? c <= 0 : c >= 0;
            return v_bool(t);
        }
        default:
            rt_abort(R, RT_ERROR, "二元运算符不支持");
        }
        break;
    }
    case EX_RANGE: {
        val a = eval_expr(R, e->from);
        val b = eval_expr(R, e->to);
        return v_rng((int64_t)a.i, (int64_t)b.i, e->inclusive);
    }
    case EX_IF: {
        val c = eval_expr(R, e->cond);
        if (truthy(c)) return eval_block(R, e->then_b);
        if (e->els) {
            if (e->els->kind == EX_BLOCK) return eval_block(R, e->els->block);
            return eval_expr(R, e->els);
        }
        return v_void();
    }
    case EX_ARRAY: {
        val* arr = (val*)ctron_arena_alloc(R->a, (e->nelems ? e->nelems : 0) * sizeof(val));
        for (size_t i = 0; i < e->nelems; i++) arr[i] = eval_expr(R, e->elems[i]);
        return v_arr(arr, e->nelems);
    }
    case EX_STRUCT: {
        if (e->npath == 0) rt_abort(R, RT_ERROR, "构造缺少类型名");
        const char* tn = e->path[0];
        int is_class = find_kind(R->f, D_CLASS, tn) != NULL;
        vfld* fs = NULL;
        if (e->nfields) {
            fs = (vfld*)ctron_arena_alloc(R->a, e->nfields * sizeof(vfld));
            for (size_t i = 0; i < e->nfields; i++) {
                fs[i].name = e->fields[i].name;
                fs[i].v = e->fields[i].value ? eval_expr(R, e->fields[i].value) : v_void();
            }
        }
        return v_obj(tn, is_class, fs, e->nfields);
    }
    case EX_INDEX: {
        val o = eval_expr(R, e->obj);
        val ix = eval_expr(R, e->index);
        if (o.k == V_LIST) {
            long long li = (long long)ix.i;
            if (li < 0 || (unsigned long long)li >= o.lst->n) rt_abort(R, RT_PANIC, "index out of bounds");
            return o.lst->items[li];
        }
        if (o.k != V_ARR) rt_abort(R, RT_ERROR, "索引目标非数组");
        long long i = (long long)ix.i;
        if (i < 0 || (unsigned long long)i >= o.nitems) rt_abort(R, RT_PANIC, "index out of bounds");
        return o.items[i];
    }
    case EX_MEMBER: {
        val o = eval_expr(R, e->obj);
        if (o.k == V_BOX && o.bx) o = o.bx->inner;
        if (!e->m_is_name || !e->mname) rt_abort(R, RT_ERROR, "属性访问不支持");
        const char* m = e->mname;
        if (strcmp(m, "len") == 0) {
            if (o.k == V_ARR) return v_int(o.nitems, 32, 0);
            if (o.k == V_STR) return v_int(o.s ? (__int128)strlen(o.s) : 0, 32, 0);
            if (o.k == V_LIST) return v_int(o.lst->n, 32, 0);
            rt_abort(R, RT_ERROR, ".len 目标类型不支持");
        }
        if (strcmp(m, "char_len") == 0) {
            if (o.k == V_STR) {
                int64_t n = 0;
                const unsigned char* p = (const unsigned char*)(o.s ? o.s : "");
                while (*p) { if ((*p & 0xC0) != 0x80) n++; p++; }
                return v_int(n, 32, 0);
            }
            rt_abort(R, RT_ERROR, ".char_len 目标类型不支持");
        }
        if (o.k == V_ERR) {
            if (strcmp(m, "message") == 0) return v_str_own(R, o.err->msg);
            if (strcmp(m, "cause") == 0) return o.err->cause;
            rt_abort(R, RT_ERROR, "错误属性不支持: %s", m);
        }
        if (o.k == V_STRUCT) {
            for (size_t i = 0; i < o.nfld; i++)
                if (strcmp(o.flds[i].name, m) == 0) return o.flds[i].v;
            if (o.is_class) {
                const cprop* P = cls_prop(R, o.type, m);
                if (P) return call_prop_body(R, P, o);
            }
            rt_abort(R, RT_ERROR, "字段/属性不存在: %s", m);
        }
        rt_abort(R, RT_ERROR, "属性访问不支持: %s", m);
    }
    case EX_TRY: {
        val v = eval_expr(R, e->obj);
        if (v.k == V_TAG && strcmp(v.tag, "None") == 0) {
            R->ret = v; // 提前返回 None
            R->has_ret = 1;
            return v;
        }
        if (v.k == V_TAG && (strcmp(v.tag, "Some") == 0 || strcmp(v.tag, "Ok") == 0)
            && v.nitems == 1) return v.items[0];
        if (v.k == V_TAG && strcmp(v.tag, "Err") == 0) { R->ret = v; R->has_ret = 1; return v; }
        return v;
    }
    case EX_CLOSURE:
        return v_closure(e, R->top);
    case EX_MATCH: {
        val sc = eval_expr(R, e->scrut);
        for (size_t i = 0; i < e->narms; i++) {
            env_push(R);
            if (pat_bind(R, e->arms[i].pat, sc)) {
                val r = eval_expr(R, e->arms[i].expr);
                env_pop(R);
                return r;
            }
            env_pop(R);
        }
        rt_abort(R, RT_ERROR, "match 无匹配臂");
    }
    case EX_OWN: return eval_block(R, e->obody);
    case EX_SCOPE: {
        val sv = v_scope(R);
        scope_t* saved = R->scope;
        R->scope = sv.scope;
        env_push(R);
        if (e->sparam) env_let(R, e->sparam, sv);
        int sr = R->has_ret;
        val srv = R->ret;
        R->has_ret = 0;
        val body = eval_block(R, e->sbody);
        val res = R->has_ret ? R->ret : body;
        R->has_ret = sr;
        R->ret = srv;
        // 作用域退出:收尾所有任务(正常已 join;残留应可推进)
        for (int guard = 0; guard < 64; guard++)
            if (!run_next_task(R)) break;
        env_pop(R);
        R->scope = saved;
        return res;
    }
    case EX_BLOCK: return eval_block(R, e->block);
    case EX_CALL: {
        cexpr* cal = e->callee;
        if (cal && cal->kind == EX_MEMBER && cal->obj && cal->obj->kind == EX_IDENT
            && strcmp(cal->obj->text, "Arena") == 0 && cal->mname
            && strcmp(cal->mname, "fixed") == 0) {
            // Arena.fixed(n): arena 句柄(无状态)
            val h = {0};
            h.k = V_ARR; // 占位;真正零数组经 arena.zeros 现建
            (void)e;
            val av = {0};
            av.k = V_ARR;
            av.items = NULL;
            av.nitems = 0;
            return av;
        }
        // .as[T]()
        if (cal && cal->kind == EX_TYPEARGS && cal->obj && cal->obj->kind == EX_MEMBER
            && cal->obj->m_is_name && cal->obj->mname && strcmp(cal->obj->mname, "as") == 0) {
            val v = eval_expr(R, cal->obj->obj);
            const char* tn = cal->ntargs > 0 ? head_nm(cal->targs[0]) : NULL;
            return as_conv(R, v, tn);
        }
        // Box[T](v)
        if (cal && cal->kind == EX_TYPEARGS && cal->obj && cal->obj->kind == EX_IDENT
            && strcmp(cal->obj->text, "Box") == 0) {
            if (e->nelems != 1) rt_abort(R, RT_ERROR, "Box 实参");
            return v_box(R, eval_expr(R, e->elems[0]));
        }
        if (cal && cal->kind == EX_TYPEARGS && cal->obj && cal->obj->kind == EX_IDENT
            && strcmp(cal->obj->text, "Atomic") == 0) {
            if (e->nelems != 1) rt_abort(R, RT_ERROR, "Atomic 实参");
            val v = eval_expr(R, e->elems[0]);
            if (v.k != V_INT) rt_abort(R, RT_ERROR, "Atomic 初始值");
            return v_atom(R, v.i, v.bits, v.us);
        }
        if (cal && cal->kind == EX_TYPEARGS && cal->obj && cal->obj->kind == EX_IDENT
            && strcmp(cal->obj->text, "Channel") == 0) {
            if (e->nelems != 1) rt_abort(R, RT_ERROR, "Channel 实参");
            val capv = eval_expr(R, e->elems[0]);
            int cap = (int)capv.i;
            val snd = v_chan(R, cap);
            val rcv = snd;
            rcv.us = 0;
            return v_tuple(R, snd, rcv);
        }
        if (cal && cal->kind == EX_TYPEARGS && cal->obj && cal->obj->kind == EX_IDENT
            && (strcmp(cal->obj->text, "Mutex") == 0 || strcmp(cal->obj->text, "Global") == 0)) {
            if (e->nelems < 1 || e->nelems > 2) rt_abort(R, RT_ERROR, "%s 实参", cal->obj->text);
            if (strcmp(cal->obj->text, "Global") == 0 && e->nelems == 2 && e->elems[0])
                (void)eval_expr(R, e->elems[0]); // 名称仅编译期
            val init = eval_expr(R, e->elems[e->nelems - 1]);
            return v_mutex(R, init);
        }
        if (cal && cal->kind == EX_TYPEARGS && cal->obj && cal->obj->kind == EX_MEMBER
            && cal->obj->mname && cal->obj->obj && cal->obj->obj->kind == EX_IDENT
            && strcmp(cal->obj->obj->text, "arena") == 0) {
            const char* am = cal->obj->mname;
            if (strcmp(am, "list") == 0) return v_list(R);
            if (strcmp(am, "zeros") == 0 || strcmp(am, "array") == 0) {
                if (e->nelems != 1) rt_abort(R, RT_ERROR, "%s 实参", am);
                val nv = eval_expr(R, e->elems[0]);
                long long n = (long long)nv.i;
                if (n < 0 || n > (1 << 20)) rt_abort(R, RT_ERROR, "数组过大");
                val* arr = (val*)ctron_arena_alloc(R->a, (size_t)n * sizeof(val));
                for (long long i = 0; i < n; i++) arr[i] = v_int(0, 32, 0);
                return v_arr(arr, (size_t)n);
            }
            rt_abort(R, RT_ERROR, "arena 方法不支持: %s", am);
        }
        if (cal && cal->kind == EX_IDENT) {
            const char* nm = cal->text;
            // 绑定为函数值(fn 类型参数)→ 按值调用
            {
                bind* fb = env_find(R, nm);
                if (fb && (fb->slot.k == V_FN || fb->slot.k == V_CLOSURE)) {
                    val* av = NULL;
                    if (e->nelems) {
                        av = (val*)ctron_arena_alloc(R->a, e->nelems * sizeof(val));
                        for (size_t i = 0; i < e->nelems; i++) av[i] = eval_expr(R, e->elems[i]);
                    }
                    return invoke_vals(R, fb->slot, av, e->nelems);
                }
            }
            if (!strcmp(nm, "panic")) {
                if (e->nelems != 1) rt_abort(R, RT_ERROR, "panic 实参");
                val mv = eval_expr(R, e->elems[0]);
                rt_abort(R, RT_PANIC, "%s", (mv.k == V_STR && mv.s) ? mv.s : "panic");
            }
            if (!strcmp(nm, "print") || !strcmp(nm, "println")) {
                if (e->nelems != 1) rt_abort(R, RT_ERROR, "%s 实参", nm);
                sb b = {0};
                val pv = eval_expr(R, e->elems[0]);
                fmt_val(R, pv, &b);
                rt_puts(R, b.d ? b.d : "");
                if (!strcmp(nm, "println")) rt_puts(R, "\n");
                free(b.d);
                return v_void();
            }
            if (!strcmp(nm, "read_file")) {
                if (e->nelems != 1) rt_abort(R, RT_ERROR, "read_file 实参");
                val pv = eval_expr(R, e->elems[0]);
                const char* path = (pv.k == V_STR && pv.s) ? pv.s : "";
                FILE* f = fopen(path, "rb");
                if (!f) {
                    val* none = NULL;
                    return v_tag("None", none, 0);
                }
                fseek(f, 0, SEEK_END);
                long sz = ftell(f);
                fseek(f, 0, SEEK_SET);
                char* buf = (char*)malloc((size_t)sz + 1);
                size_t got = fread(buf, 1, (size_t)sz, f);
                fclose(f);
                buf[got] = 0;
                char* ar = ctron_arena_strndup(R->a, buf, got);
                free(buf);
                val* one = (val*)ctron_arena_alloc(R->a, sizeof(val));
                one[0] = v_str_own(R, ar);
                return v_tag("Some", one, 1);
            }
            if (!strcmp(nm, "assert")) {
                if (e->nelems != 1) rt_abort(R, RT_ERROR, "assert 参数");
                val c = eval_expr(R, e->elems[0]);
                if (!truthy(c)) assert_fail(R, "assert failed");
                return v_void();
            }
            if (!strcmp(nm, "assert_eq") || !strcmp(nm, "assert_ne")) {
                if (e->nelems != 2) rt_abort(R, RT_ERROR, "assert_eq/ne 参数");
                val a = eval_expr(R, e->elems[0]);
                val b = eval_expr(R, e->elems[1]);
                int eq = val_eq(R, a, b);
                int want = !strcmp(nm, "assert_eq");
                if (eq != want) {
                    sb m = {0};
                    sb_s(&m, want ? "assert_eq failed: " : "assert_ne failed: ");
                    fmt_val(R, a, &m);
                    sb_s(&m, " != ");
                    fmt_val(R, b, &m);
                    rt_abort(R, RT_ASSERT_FAIL, "%s", m.d ? m.d : "");
                }
                return v_void();
            }
            if (is_variant(R, nm)) {
                val* items = NULL;
                if (e->nelems) {
                    items = (val*)ctron_arena_alloc(R->a, e->nelems * sizeof(val));
                    for (size_t i = 0; i < e->nelems; i++) items[i] = eval_expr(R, e->elems[i]);
                }
                return v_tag(nm, items, e->nelems);
            }
            const cdecl* fn = file_fn(R, nm);
            if (!fn) rt_abort(R, RT_ERROR, "未知函数: %s", nm);
            return call_decl(R, fn, e->elems, e->nelems);
        }
        // 成员方法:选项内建(map/or/expect)优先,否则 UFCS(接收者作首参)
        if (cal && cal->kind == EX_MEMBER && cal->m_is_name && cal->mname) {
            val recv = eval_expr(R, cal->obj);
            R->has_opt = 0;
            if (option_builtin(R, recv, cal->mname, e)) {
                val r = R->opt_result;
                R->has_opt = 0;
                return r;
            }
            if (recv.k == V_STR && cal->mname && strcmp(cal->mname, "slice") == 0) {
                if (e->nelems != 1) rt_abort(R, RT_ERROR, "slice 实参");
                val rg = eval_expr(R, e->elems[0]);
                if (rg.k != V_RANGE) rt_abort(R, RT_ERROR, "slice 需 range");
                size_t len = recv.s ? strlen(recv.s) : 0;
                long long lo = rg.lo < 0 ? 0 : rg.lo;
                long long hi = rg.inclusive ? rg.hi + 1 : rg.hi;
                if (hi > (long long)len) hi = len;
                // utf8 边界检查
                if (lo < (long long)len && ((unsigned char)recv.s[lo] & 0xC0) == 0x80)
                    rt_abort(R, RT_PANIC, "invalid utf8 boundary (utf8)");
                if (hi < (long long)len && ((unsigned char)recv.s[hi] & 0xC0) == 0x80)
                    rt_abort(R, RT_PANIC, "invalid utf8 boundary (utf8)");
                if (lo < 0 || hi < lo) rt_abort(R, RT_PANIC, "invalid utf8 boundary (utf8)");
                val out = {0};
                out.k = V_STR;
                out.s = ctron_arena_strndup(R->a, recv.s + lo, (size_t)(hi - lo));
                return out;
            }
            if (cal->mname && strcmp(cal->mname, "to_string") == 0
                && (recv.k == V_STR || recv.k == V_INT || recv.k == V_BOOL || recv.k == V_FLOAT)) {
                sb b = {0};
                fmt_val(R, recv, &b);
                val out = v_void();
                out.k = V_STR;
                out.s = astr(R, b.d ? b.d : "");
                free(b.d);
                return out;
            }
            if (recv.k == V_STRUCT && cal->mname && strcmp(cal->mname, "show") == 0) {
                // @derive(Show) 的格式化(语料只断言非空)
                sb b = {0};
                sb_s(&b, recv.type ? recv.type : "?");
                sb_s(&b, "(");
                for (size_t i = 0; i < recv.nfld; i++) {
                    if (i) sb_s(&b, ",");
                    sb_s(&b, recv.flds[i].name);
                    sb_s(&b, "=");
                    fmt_val(R, recv.flds[i].v, &b);
                }
                sb_s(&b, ")");
                val out = v_void();
                out.k = V_STR;
                out.s = astr(R, b.d ? b.d : "");
                free(b.d);
                return out;
            }
            if (recv.k == V_LIST && cal->mname) {
                if (strcmp(cal->mname, "push") == 0) {
                    if (e->nelems != 1) rt_abort(R, RT_ERROR, "push 实参");
                    list_push(R, recv.lst, eval_expr(R, e->elems[0]));
                    return v_void();
                }
                if (strcmp(cal->mname, "into_gc") == 0) return list_clone_deep(R, recv.lst);
                rt_abort(R, RT_ERROR, "列表方法不支持: %s", cal->mname);
            }
            if (recv.k == V_ATOM && cal->mname) {
                if (strcmp(cal->mname, "load") == 0) {
                    if (e->nelems != 0) rt_abort(R, RT_ERROR, "load 参数");
                    return v_int(recv.atom->v, recv.atom->bits, recv.atom->us);
                }
                if (strcmp(cal->mname, "store") == 0) {
                    if (e->nelems != 1) rt_abort(R, RT_ERROR, "store 实参");
                    val x = eval_expr(R, e->elems[0]);
                    recv.atom->v = x.k == V_INT ? x.i : (__int128)x.f;
                    return v_void();
                }
                if (strcmp(cal->mname, "fetch_add") == 0) {
                    if (e->nelems != 1) rt_abort(R, RT_ERROR, "fetch_add 实参");
                    val x = eval_expr(R, e->elems[0]);
                    __int128 old = recv.atom->v;
                    recv.atom->v = old + (x.k == V_INT ? x.i : 0);
                    return v_int(old, recv.atom->bits, recv.atom->us);
                }
                rt_abort(R, RT_ERROR, "原子方法不支持: %s", cal->mname);
            }
            if (recv.k == V_SCOPE && cal->mname && strcmp(cal->mname, "spawn") == 0) {
                if (e->nelems != 1) rt_abort(R, RT_ERROR, "spawn 实参");
                val f = eval_expr(R, e->elems[0]);
                if (f.k != V_CLOSURE && f.k != V_FN) rt_abort(R, RT_ERROR, "spawn 需闭包");
                task_t* t = (task_t*)ctron_arena_alloc(R->a, sizeof(task_t));
                t->closure = f;
                scope_add_task(R, recv.scope, t);
                return v_task(R, t);
            }
            if (recv.k == V_TASK && cal->mname) {
                task_t* t = recv.task;
                if (strcmp(cal->mname, "join") == 0) {
                    run_task(R, t);
                    if (t->panicked) rt_abort(R, RT_PANIC, "%s", t->msg);
                    return t->result;
                }
                if (strcmp(cal->mname, "join_or") == 0) {
                    run_task(R, t);
                    val* one = (val*)ctron_arena_alloc(R->a, sizeof(val));
                    if (t->panicked) {
                        if (R->scope) R->scope->cancelled = 1; // 兄弟失败 → 取消作用域
                        one[0] = v_tag("TaskPanic", NULL, 0);
                        return v_tag("Err", one, 1);
                    }
                    one[0] = t->result;
                    return v_tag("Ok", one, 1);
                }
                rt_abort(R, RT_ERROR, "任务方法不支持: %s", cal->mname);
            }
            if (recv.k == V_CHAN && cal->mname) {
                if (strcmp(cal->mname, "send") == 0) {
                    if (!recv.us) rt_abort(R, RT_ERROR, "接收端不能 send");
                    if (e->nelems != 1) rt_abort(R, RT_ERROR, "send 实参");
                    return chan_send(R, recv, eval_expr(R, e->elems[0]));
                }
                if (strcmp(cal->mname, "recv") == 0) {
                    if (recv.us) rt_abort(R, RT_ERROR, "发送端不能 recv");
                    if (e->nelems != 0) rt_abort(R, RT_ERROR, "recv 实参");
                    return chan_recv(R, recv);
                }
                rt_abort(R, RT_ERROR, "通道方法不支持: %s", cal->mname);
            }
            if (recv.k == V_MUTEX && cal->mname) {
                if (e->nelems != 1) rt_abort(R, RT_ERROR, "%s 实参", cal->mname);
                val f = eval_expr(R, e->elems[0]);
                if (!strcmp(cal->mname, "with") || !strcmp(cal->mname, "with_mut"))
                    return mutex_with(R, recv, f, !strcmp(cal->mname, "with_mut"));
                rt_abort(R, RT_ERROR, "互斥方法不支持: %s", cal->mname);
            }
            if (recv.k == V_STRUCT && recv.is_class && cal->mname) {
                const cfn* F = cls_method(R, recv.type, cal->mname);
                if (F) return call_method_body(R, F, recv, e->elems, e->nelems);
                rt_abort(R, RT_ERROR, "未知方法/UFCS: %s", cal->mname);
            }
            cexpr** arg2 = (cexpr**)ctron_arena_alloc(R->a, (e->nelems + 1) * sizeof(cexpr*));
            arg2[0] = cal->obj;
            for (size_t i = 0; i < e->nelems; i++) arg2[i + 1] = e->elems[i];
            const cdecl* fn = file_fn(R, cal->mname);
            if (!fn) rt_abort(R, RT_ERROR, "未知方法/UFCS: %s", cal->mname);
            return call_decl(R, fn, arg2, e->nelems + 1);
        }
        rt_abort(R, RT_ERROR, "调用形式不支持");
        break;
    }
    default:
        rt_abort(R, RT_ERROR, "表达式不支持(kind %d)", (int)e->kind);
    }
    return v_void();
}

// ================= 入口:运行 test 块 =================
rt_run ctron_rt_run(const cfile* f) {
    rt_run out = {0};
    ctron_arena* arena = ctron_arena_new();
    rt R = {0};
    R.f = f;
    R.a = arena;
    out.st = RT_OK;

    size_t ntest = 0;
    for (size_t i = 0; i < f->ndecls; i++)
        if (f->decls[i].kind == D_TEST) ntest++;
    out.tests_total = ntest;

    if (setjmp(R.jb)) {
        out.st = R.st;
        out.msg = strdup(R.msg);
        out.tests_run = R.tests_run;
        ctron_arena_free(arena);
        return out;
    }

    for (size_t i = 0; i < f->ndecls; i++) {
        const cdecl* d = &f->decls[i];
        if (d->kind != D_TEST) continue;
        R.tests_run++;
        R.top = NULL;
        R.has_ret = 0;
        (void)eval_block(&R, d->test.body);
    }
    out.tests_run = R.tests_run;
    ctron_arena_free(arena);
    return out;
}

rt_run ctron_rt_run_main(const cfile* f) {
    ctron_arena* arena = ctron_arena_new();
    rt R = {0};
    R.a = arena;
    rt_run out = {0};
    out.st = RT_OK;
    const cfn* main = NULL;
    for (size_t i = 0; i < f->ndecls; i++) {
        const cdecl* d = &f->decls[i];
        if (d->kind == D_FN && strcmp(d->fn_.name, "main") == 0) { main = &d->fn_; break; }
    }
    if (!main) {
        out.st = RT_ERROR;
        out.msg = strdup("缺少 fn main");
        ctron_arena_free(arena);
        return out;
    }
    if (setjmp(R.jb)) {
        out.st = R.st;
        out.msg = strdup(R.msg);
        out.out = R.out ? R.out : NULL;
        out.exit_code = 1;
        ctron_arena_free(arena);
        return out;
    }
    env_push(&R);
    int sr = R.has_ret;
    val srv = R.ret;
    R.has_ret = 0;
    (void)eval_block(&R, main->body);
    val res = R.has_ret ? R.ret : srv;
    R.has_ret = sr;
    R.ret = srv;
    env_pop(&R);
    if (res.k == V_INT) out.exit_code = (long)res.i;
    out.out = R.out ? R.out : NULL;
    ctron_arena_free(arena);
    return out;
}

void ctron_rt_run_free(rt_run* r) {
    if (!r) return;
    free((void*)r->msg);
    free(r->out);
    r->msg = NULL;
    r->out = NULL;
}
