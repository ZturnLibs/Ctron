// sem.c —— C3-a 语义检查(单文件,语法导向;见 sem.h 检查清单)。
#include "sem.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------- 诊断收集 ----------
typedef struct {
    ctron_arena* arena;
    ctron_diag* d;
    size_t n, cap;
} ck;

static void diag(ck* k, const char* code, const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (k->n == k->cap) {
        k->cap = k->cap ? k->cap * 2 : 16;
        k->d = (ctron_diag*)realloc(k->d, k->cap * sizeof(ctron_diag));
        if (!k->d) abort();
    }
    ctron_diag* e = &k->d[k->n++];
    e->code = code;
    e->message = ctron_arena_strndup(k->arena, buf, strlen(buf));
    e->span.line = 0;
    e->span.col = 0;
    e->span.start = 0;
    e->span.end = 0;
}

// ---------- 符号查询 ----------
typedef struct { const cfile* f; } sym;

static int has_attr(const cattr* a, size_t n, const char* name) {
    for (size_t i = 0; i < n; i++)
        if (a[i].name && strcmp(a[i].name, name) == 0) return 1;
    return 0;
}

static const cdecl* find_kind(const cfile* f, cdecl_kind kind, const char* name) {
    for (size_t i = 0; i < f->ndecls; i++) {
        const cdecl* d = &f->decls[i];
        if (d->kind != kind) continue;
        const char* nm = NULL;
        switch (kind) {
        case D_STRUCT: nm = d->strukt.name; break;
        case D_CLASS: nm = d->klass.name; break;
        case D_ENUM: nm = d->en.name; break;
        case D_TRAIT: nm = d->trait.name; break;
        case D_FN: nm = d->fn_.name; break;
        default: break;
        }
        if (nm && strcmp(nm, name) == 0) return d;
    }
    return NULL;
}
#define FIND(F, KIND, N) find_kind(F, KIND, N)
static const cdecl* find_class(const cfile* f, const char* n) { return FIND(f, D_CLASS, n); }
static const cdecl* find_struct(const cfile* f, const char* n) { return FIND(f, D_STRUCT, n); }
static const cdecl* find_enum(const cfile* f, const char* n) { return FIND(f, D_ENUM, n); }
static const cdecl* find_trait(const cfile* f, const char* n) { return FIND(f, D_TRAIT, n); }
static const cdecl* find_fn(const cfile* f, const char* n) { return FIND(f, D_FN, n); }

static const char* head_name(const cty* t); // 前置:type_has_member 先于定义使用

// 成员存在性:struct 字段 / class 字段·prop / impl prop(E2010 字段检查用)
static int type_has_member(const cfile* f, const char* ty, const char* m) {
    const cdecl* st = find_struct(f, ty);
    if (st)
        for (size_t i = 0; i < st->strukt.nfields; i++)
            if (st->strukt.fields[i].name && strcmp(st->strukt.fields[i].name, m) == 0) return 1;
    const cdecl* cl = find_class(f, ty);
    if (cl)
        for (size_t i = 0; i < cl->klass.nitems; i++) {
            const cclassitem* it = &cl->klass.items[i];
            if ((it->kind == CT_FIELD && it->f && it->f->name && strcmp(it->f->name, m) == 0)
                || (it->kind == CT_PROP && it->p && it->p->name && strcmp(it->p->name, m) == 0))
                return 1;
        }
    for (size_t i = 0; i < f->ndecls; i++) {
        const cdecl* d = &f->decls[i];
        if (d->kind != D_IMPL) continue;
        const char* fornm = head_name(d->impl.for_ty);
        if (!fornm || strcmp(fornm, ty) != 0) continue;
        for (size_t j = 0; j < d->impl.nitems; j++) {
            const cimplitem* it = &d->impl.items[j];
            if (it->kind == II_PROP && it->p && it->p->name && strcmp(it->p->name, m) == 0)
                return 1;
        }
    }
    return 0;
}

static const char* head_name(const cty* t) {
    if (!t || t->kind != TY_NAMED || t->npath == 0) return NULL;
    return t->path[0];
}

// ---------- 原语 / 前奏特殊 ----------
static int is_prim(const char* n) {
    static const char* const P[] = {"I8", "I16", "I32", "I64", "ISize", "U8", "U16", "U32",
                                    "U64", "USize", "F32", "F64", "Bool", "Str", "Void"};
    for (size_t i = 0; i < sizeof P / sizeof P[0]; i++)
        if (strcmp(n, P[i]) == 0) return 1;
    return 0;
}
static int is_send_special(const char* n) {
    static const char* const S[] = {"Mutex", "Atomic", "Global", "Sender", "Receiver",
                                    "Task", "String", "Box", "Never", "Option", "Result"};
    for (size_t i = 0; i < sizeof S / sizeof S[0]; i++)
        if (strcmp(n, S[i]) == 0) return 1;
    return 0;
}

// ---------- Send(§7.4):返回 1 = Send,0 = 可证明非 Send;未知保守 Send ----------
static int ty_send(const sym* s, const cty* t, int depth) {
    if (depth > 16) return 1;
    if (!t) return 1;
    switch (t->kind) {
    case TY_NAMED: {
        const char* n = head_name(t);
        if (!n) return 1;
        if (is_prim(n) || is_send_special(n)) return 1;
        const cdecl* c = find_class(s->f, n);
        if (c) {
            for (size_t i = 0; i < c->klass.nitems; i++) {
                const cclassitem* it = &c->klass.items[i];
                if (it->kind == CT_FIELD) {
                    if (it->f->is_var) return 0; // var 字段 → 非 Send(§7.4)
                    if (!ty_send(s, it->f->ty, depth + 1)) return 0;
                }
            }
            return 1;
        }
        const cdecl* st = find_struct(s->f, n);
        if (st) {
            for (size_t i = 0; i < st->strukt.nfields; i++)
                if (!ty_send(s, st->strukt.fields[i].ty, depth + 1)) return 0;
            return 1;
        }
        const cdecl* en = find_enum(s->f, n);
        if (en) {
            for (size_t i = 0; i < en->en.nvariants; i++) {
                const cvariant* v = &en->en.variants[i];
                for (size_t j = 0; j < v->ntys; j++)
                    if (!ty_send(s, v->tys[j], depth + 1)) return 0;
                for (size_t j = 0; j < v->nfields; j++)
                    if (!ty_send(s, v->fields[j].ty, depth + 1)) return 0;
            }
            return 1;
        }
        return 1; // 泛型参数/未知 → 保守 Send
    }
    case TY_REF: {
        if (t->sub && t->sub->kind == TY_NAMED) {
            const char* n = head_name(t->sub);
            if (n && find_trait(s->f, n)) return 0; // &Trait 特征对象恒非 Send
        }
        if (t->sub && t->sub->kind == TY_SLICE) return ty_send(s, t->sub->sub, depth + 1); // &T[]
        return ty_send(s, t->sub, depth + 1);
    }
    case TY_SLICE: return 0; // T[] 恒非 Send
    case TY_ARRAY: return t->elem ? ty_send(s, t->elem, depth + 1) : 1;
    case TY_OPT: return ty_send(s, t->sub, depth + 1);
    case TY_TUPLE:
        for (size_t i = 0; i < t->nelems; i++)
            if (!ty_send(s, t->elems[i], depth + 1)) return 0;
        return 1;
    default:
        return 1; // TY_FN / TY_SELF / TY_CVAL
    }
}

// ---------- 绑定环境 ----------
typedef struct bind {
    const char* name;
    cty* ty; // NULL = 未知(保守 Send)
    int depth;
    struct bind* next;
} bind;

static void bind_free(bind* b) {
    while (b) { bind* nx = b->next; free(b); b = nx; }
}
static void bind_push(bind** env, const char* name, cty* ty, int depth) {
    bind* b = (bind*)calloc(1, sizeof(bind));
    b->name = name;
    b->ty = ty;
    b->depth = depth;
    b->next = *env;
    *env = b;
}
static bind* bind_find(bind* env, const char* name) {
    for (bind* b = env; b; b = b->next)
        if (b->name && strcmp(b->name, name) == 0) return b;
    return NULL;
}

// ---------- 类型构造 ----------
static cty* mk_named(ctron_arena* a, const char* name, cty** args, size_t nargs) {
    cty* t = (cty*)ctron_arena_alloc(a, sizeof(cty));
    t->kind = TY_NAMED;
    char** pp = (char**)ctron_arena_alloc(a, sizeof(char*));
    pp[0] = (char*)name; // 指向 AST arena 字符串(检查期内存活)
    t->path = pp;
    t->npath = 1;
    t->args = NULL;
    t->nargs = 0;
    if (nargs) {
        t->args = (cty**)ctron_arena_alloc(a, nargs * sizeof(cty*));
        memcpy(t->args, args, nargs * sizeof(cty*));
        t->nargs = nargs;
    }
    return t;
}

// ---------- 上下文 ----------
typedef struct ctx {
    ck* k;
    const sym* s;
    bind* env;
    int depth;
    int fn_no_spawn;
    int fn_pure;
    int fn_comptime;
    int fn_noalloc;   // #[no_alloc] / trait 契约 / bare 档
    int profile;      // sem_profile
    int in_own;
    const cty* fn_ret; // 当前函数返回类型(test 体为 NULL;? 语境检查用)
    int in_callee;     // >0 = 正在检查调用者表达式(成员作方法名,不作字段)
    char** arena_binds; // own 块内 arena 句柄活绑定(只移语义)
    size_t n_arena;
    char** moved;      // 已 move 的句柄名
    size_t n_moved;
    char** gc_local;   // own 块内“块生”GC 集合(into_gc 产物等,push 放行)
    size_t n_gc;
} ctx;

static int name_in(const char** a, size_t n, const char* name) {
    for (size_t i = 0; i < n; i++)
        if (strcmp(a[i], name) == 0) return 1;
    return 0;
}

// 分配效果摘要(may_alloc):fn 体含 GC 分配操作(含调用传递)。带递归栈防环。
typedef struct { const char** stack; size_t n; } estack;
static int expr_may_alloc(const cfile* f, const cdecl* fn, cexpr* e, estack* st);
static int stmt_may_alloc(const cfile* f, const cdecl* fn, cstmt* s, estack* st);
static int block_may_alloc(const cfile* f, const cdecl* fn, cblock* b, estack* st);

static int fn_is_noalloc(const cdecl* fn) {
    return fn && fn->kind == D_FN && has_attr(fn->fn_.attrs, fn->fn_.nattrs, "no_alloc");
}
static int on_estack(estack* st, const char* n) {
    for (size_t i = 0; i < st->n; i++)
        if (strcmp(st->stack[i], n) == 0) return 1;
    return 0;
}
static int call_target_is_alloc(const cfile* f, cexpr* callee, estack* st) {
    // 成员方法分类
    if (callee && callee->kind == EX_MEMBER && callee->m_is_name && callee->mname) {
        const char* m = callee->mname;
        if (strcmp(m, "to_string") == 0) return 1;
        if (strcmp(m, "push") == 0) return 1; // GC 集合增长
        if (strcmp(m, "into_gc") == 0) return 1;
        if (strcmp(m, "list") == 0 || strcmp(m, "array") == 0 || strcmp(m, "zeros") == 0) {
            const char* root = NULL;
            // 仅当接收者是 arena 时是允许路径;此处摘要模式保守:arena.* 不算分配
            cexpr* o = callee->obj;
            if (o && o->kind == EX_IDENT && strcmp(o->text, "arena") == 0) return 0;
            (void)root;
        }
        return 0;
    }
    // 泛型构造(Box/List/Map/String…)
    if (callee && callee->kind == EX_TYPEARGS && callee->obj && callee->obj->kind == EX_IDENT) {
        const char* n = callee->obj->text;
        if (strcmp(n, "Box") == 0 || strcmp(n, "List") == 0 || strcmp(n, "Map") == 0
            || strcmp(n, "String") == 0)
            return 1;
    }
    // 文件内用户函数调用
    if (callee && callee->kind == EX_IDENT) {
        const char* n = callee->text;
        const cdecl* d = find_fn(f, n);
        if (d && !fn_is_noalloc(d)) {
            if (on_estack(st, n)) return 1; // 环:保守分配
            if (st->n < 64) {
                st->stack[st->n++] = n;
                int r = block_may_alloc(f, d, d->fn_.body, st);
                st->n--;
                return r;
            }
            return 1;
        }
    }
    return 0;
}
static int expr_may_alloc(const cfile* f, const cdecl* fn, cexpr* e, estack* st) {
    if (!e) return 0;
    switch (e->kind) {
    case EX_STR: case EX_INT: case EX_FLOAT: case EX_BOOL: case EX_VOID: case EX_IDENT:
        return 0;
    case EX_TUPLE: case EX_ARRAY:
        for (size_t i = 0; i < e->nelems; i++)
            if (expr_may_alloc(f, fn, e->elems[i], st)) return 1;
        return 0;
    case EX_STRUCT: {
        if (e->npath > 0 && find_class(f, e->path[0])) return 1; // 类构造 = 分配
        for (size_t i = 0; i < e->nfields; i++)
            if (e->fields[i].value && expr_may_alloc(f, fn, e->fields[i].value, st)) return 1;
        return 0;
    }
    case EX_UNARY: return expr_may_alloc(f, fn, e->ux, st);
    case EX_BINARY: return expr_may_alloc(f, fn, e->lhs, st) || expr_may_alloc(f, fn, e->rhs, st);
    case EX_RANGE: return expr_may_alloc(f, fn, e->from, st) || expr_may_alloc(f, fn, e->to, st);
    case EX_CALL:
        if (call_target_is_alloc(f, e->callee, st)) return 1;
        if (expr_may_alloc(f, fn, e->callee, st)) return 1;
        for (size_t i = 0; i < e->nelems; i++)
            if (expr_may_alloc(f, fn, e->elems[i], st)) return 1;
        return 0;
    case EX_INDEX:
        return expr_may_alloc(f, fn, e->obj, st) || expr_may_alloc(f, fn, e->index, st);
    case EX_MEMBER: return expr_may_alloc(f, fn, e->obj, st);
    case EX_TYPEARGS: return expr_may_alloc(f, fn, e->obj, st);
    case EX_TRY: return expr_may_alloc(f, fn, e->obj, st);
    case EX_CLOSURE: return expr_may_alloc(f, fn, e->cbody, st);
    case EX_SCOPE: return block_may_alloc(f, fn, e->sbody, st);
    case EX_OWN: return block_may_alloc(f, fn, e->obody, st);
    case EX_IF:
        return expr_may_alloc(f, fn, e->cond, st)
            || block_may_alloc(f, fn, e->then_b, st)
            || (e->els && expr_may_alloc(f, fn, e->els, st));
    case EX_MATCH: {
        if (expr_may_alloc(f, fn, e->scrut, st)) return 1;
        for (size_t i = 0; i < e->narms; i++)
            if (expr_may_alloc(f, fn, e->arms[i].expr, st)) return 1;
        return 0;
    }
    case EX_BLOCK: return block_may_alloc(f, fn, e->block, st);
    default: return 0;
    }
}
static int stmt_may_alloc(const cfile* f, const cdecl* fn, cstmt* s, estack* st) {
    if (!s) return 0;
    switch (s->kind) {
    case ST_LET: return s->e ? expr_may_alloc(f, fn, s->e, st) : 0;
    case ST_RET: return s->e ? expr_may_alloc(f, fn, s->e, st) : 0;
    case ST_FOR:
        return (s->iter && expr_may_alloc(f, fn, s->iter, st))
            || block_may_alloc(f, fn, s->body, st);
    case ST_WHILE:
        return (s->e && expr_may_alloc(f, fn, s->e, st)) || block_may_alloc(f, fn, s->body, st);
    case ST_ASSIGN:
        return (s->target && expr_may_alloc(f, fn, s->target, st))
            || (s->value && expr_may_alloc(f, fn, s->value, st));
    case ST_EXPR: return s->e ? expr_may_alloc(f, fn, s->e, st) : 0;
    default: return 0;
    }
}
static int block_may_alloc(const cfile* f, const cdecl* fn, cblock* b, estack* st) {
    if (!b) return 0;
    for (size_t i = 0; i < b->nstmts; i++)
        if (stmt_may_alloc(f, fn, b->stmts[i], st)) return 1;
    return b->tail ? expr_may_alloc(f, fn, b->tail, st) : 0;
}

// 摘要入口:fn 为顶层函数声明(D_FN)
static int fn_may_alloc_summary(const cfile* f, const char* name) {
    const cdecl* d = find_fn(f, name);
    if (!d || !d->fn_.body) return 0;
    estack st = {0};
    st.stack = NULL;
    // 用栈式小数组
    static const char* tmp[64];
    st.stack = tmp;
    int r = block_may_alloc(f, d, d->fn_.body, &st);
    return r;
}

// 前向
static void check_expr(ctx* c, cexpr* e);
static void check_block(ctx* c, cblock* b);

// ---------- v0.7 修订三:调用点推断诊断(E2060/E2061,最小面) ----------
static cty* derive_type(ctx* c, cexpr* e);
// 型参是否出现于类型节点(递归;保守:同名 Named 即命中)
static int ty_has_tp(const cty* t, const char* tp, int depth) {
    if (!t || depth > 8) return 0;
    if (t->kind == TY_NAMED && strcmp(head_name(t), tp) == 0) return 1;
    for (size_t i = 0; i < t->nargs; i++)
        if (ty_has_tp(t->args[i], tp, depth + 1)) return 1;
    if (ty_has_tp(t->sub, tp, depth + 1)) return 1;
    if (ty_has_tp(t->elem, tp, depth + 1)) return 1;
    for (size_t i = 0; i < t->nelems; i++)
        if (ty_has_tp(t->elems[i], tp, depth + 1)) return 1;
    if (ty_has_tp(t->fret, tp, depth + 1)) return 1;
    return 0;
}

// 泛型无 TypeArgs 调用的推断诊断:
//   E2060——某型参不出现在任何实参位参数型中(无法推断,要求显式);
//   E2061——同一型参在多个实参位推出不同类别(候选冲突)。
// 宽松口径:实参类型不可得(derive_type NULL)时跳过该候选。
static void check_call_infer(ctx* c, const cexpr* e, const char* name) {
    const cdecl* d = find_fn(c->s->f, name);
    if (!d || d->kind != D_FN) return;
    if (d->fn_.ntype_params == 0) return;
    if (e->callee && e->callee->kind == EX_TYPEARGS && e->callee->ntargs > 0) return; // 显式实参
    if (e->nelems != d->fn_.nparams) return; // 实参数不符(保守:交给其它检查)
    int reported = 0;
    for (size_t t = 0; t < d->fn_.ntype_params && !reported; t++) {
        const char* tp = d->fn_.type_params[t].name;
        int occurs = 0;
        const char* cand[8];
        size_t ncand = 0;
        for (size_t i = 0; i < d->fn_.nparams; i++) {
            if (!ty_has_tp(d->fn_.params[i].ty, tp, 0)) continue;
            occurs = 1;
            if (i >= e->nelems) continue;
            cty* at = derive_type(c, e->elems[i]);
            const char* an = head_name(at);
            if (!an) continue;
            int seen = 0;
            for (size_t q = 0; q < ncand; q++)
                if (strcmp(cand[q], an) == 0) { seen = 1; break; }
            if (!seen && ncand < 8) cand[ncand++] = an;
        }
        if (!occurs) {
            diag(c->k, "E2060", "无法推断类型实参:%s 型参 %s 不出现于实参位,请显式标注", name, tp);
            reported = 1;
        } else if (ncand >= 2) {
            diag(c->k, "E2061", "无法唯一推断类型实参:%s 型参 %s 候选冲突", name, tp);
            reported = 1;
        }
    }
}

static cty* derive_type(ctx* c, cexpr* e);

// ---------- 表达式根标识符(用于 arena move 与发送捕获) ----------
static const char* root_ident(cexpr* e) {
    for (;;) {
        if (!e) return NULL;
        switch (e->kind) {
        case EX_IDENT: return e->text;
        case EX_INDEX: e = e->obj; break;
        case EX_MEMBER: e = e->obj; break;
        case EX_TRY: e = e->obj; break;
        case EX_TYPEARGS: e = e->obj; break;
        case EX_CALL: e = e->callee; break;
        default: return NULL;
        }
    }
}

// 语法导向的局部类型推导(够 Send/穷尽/效果/条件类型检查即可)
static cty* derive_type(ctx* c, cexpr* e) {
    if (!e) return NULL;
    switch (e->kind) {
    case EX_STRUCT:
        if (e->npath >= 1) return mk_named(c->k->arena, e->path[0], NULL, 0);
        return NULL;
    case EX_CALL: {
        cexpr* cal = e->callee;
        if (cal && cal->kind == EX_TYPEARGS && cal->obj && cal->obj->kind == EX_IDENT) {
            return mk_named(c->k->arena, cal->obj->text, cal->targs, cal->ntargs);
        }
        if (cal && cal->kind == EX_IDENT) {
            const char* n = cal->text;
            if (strcmp(n, "Some") == 0 || strcmp(n, "None") == 0)
                return mk_named(c->k->arena, "Option", NULL, 0);
            if (strcmp(n, "Ok") == 0 || strcmp(n, "Err") == 0)
                return mk_named(c->k->arena, "Result", NULL, 0);
            const cdecl* d = find_fn(c->s->f, n);
            if (d) return d->fn_.ret; // 命名函数调用的返回类型
        }
        return NULL;
    }
    case EX_IDENT: {
        bind* b = bind_find(c->env, e->text);
        return b ? b->ty : NULL;
    }
    case EX_INT: {
        // 无后缀 → 默认 I32(期望类型自适应由类别检查放宽,不在此推断宽度)
        if (e->suffix && *e->suffix) {
            char u[8];
            size_t j = 0;
            for (const char* p = e->suffix; *p && j < 7; p++) u[j++] = (char)toupper((unsigned char)*p);
            u[j] = 0;
            if (!strcmp(u, "F32") || !strcmp(u, "F64")) return mk_named(c->k->arena, u, NULL, 0);
            if (u[0] == 'I' || u[0] == 'U') return mk_named(c->k->arena, u, NULL, 0);
        }
        return mk_named(c->k->arena, "I32", NULL, 0);
    }
    case EX_FLOAT: {
        if (e->suffix && !strcmp(e->suffix, "f32")) return mk_named(c->k->arena, "F32", NULL, 0);
        return mk_named(c->k->arena, "F64", NULL, 0);
    }
    case EX_STR: return mk_named(c->k->arena, "Str", NULL, 0);
    case EX_BOOL: return mk_named(c->k->arena, "Bool", NULL, 0);
    case EX_UNARY:
        if (e->uop == UN_NOT) return mk_named(c->k->arena, "Bool", NULL, 0);
        return derive_type(c, e->ux);
    case EX_BINARY: {
        switch (e->bop) {
        case B_OROR: case B_OR: case B_AND: case B_EQ: case B_NE: case B_LT: case B_GT: case B_LE: case B_GE:
            return mk_named(c->k->arena, "Bool", NULL, 0);
        default:
            return derive_type(c, e->lhs); // 算术沿左操作数
        }
    }
    case EX_TRY: return mk_named(c->k->arena, "Option", NULL, 0);
    default: return NULL;
    }
}

// ---------- E2010(保守子集):条件须 Bool;let 字面量类别与注解冲突 ----------
// 类别:0 未知(不判) / 1 数值 / 2 Bool / 3 Str 系
static int prim_cat(const char* n) {
    if (!n) return 0;
    if (!strcmp(n, "Bool")) return 2;
    if (!strcmp(n, "Str") || !strcmp(n, "String")) return 3;
    static const char* const NUM[] = {"I8", "I16", "I32", "I64", "ISize", "U8", "U16",
                                      "U32", "U64", "USize", "F32", "F64"};
    for (size_t i = 0; i < sizeof NUM / sizeof NUM[0]; i++)
        if (!strcmp(n, NUM[i])) return 1;
    return 0;
}
// cond 非空且可证明非 Bool 时报告;返回是否报告
static int check_cond_bool(ctx* c, cexpr* cond, const char* what) {
    cty* t = derive_type(c, cond);
    const char* hn = head_name(t);
    if (!hn) return 0; // 推不出 → 不报告(保守)
    int cat = prim_cat(hn);
    if (cat == 2 || cat == 0) return 0;
    diag(c->k, "E2010", "%s 条件应为 Bool,实际 %s", what, hn);
    return 1;
}
static const char* cat_name(int cat) {
    return cat == 1 ? "数值" : cat == 2 ? "Bool" : "Str";
}
// ---------- W8040:遮蔽前奏符号(名单对齐 Rust sem.rs register_prelude) ----------
static int is_prelude_name(const char* n) {
    static const char* const P[] = {
        "I8", "I16", "I32", "I64", "ISize", "U8", "U16", "U32", "U64", "USize",
        "F32", "F64", "Bool", "Str", "String", "Void", "Never", "TaskPanic",
        "Channel", "List", "Map", "Set", "Box", "StringBuilder", "Atomic", "Global",
        "Mutex", "Sender", "Receiver", "Task", "Scope", "Arena", "Region", "Pool",
        "ArenaList", "Simd", "AnyError", "Parallel", "Path", "Bytes",
        "Option", "Result", "Some", "None", "Ok", "Err",
        "Error", "Show", "Eq", "Drop", "Clone", "Hash", "Iter", "Cap",
        "Clock", "Fs", "Net", "Log",
    };
    for (size_t i = 0; i < sizeof P / sizeof P[0]; i++)
        if (strcmp(n, P[i]) == 0) return 1;
    return 0;
}
static void check_shadow_prelude(ctx* c, const char* name) {
    if (name && is_prelude_name(name))
        diag(c->k, "W8040", "遮蔽前奏符号(shadow):%s", name);
}
// opt/res 接收类型判定:Option/Result/T? → 1;可证明不是 → 0;未知 → -1(不判)
static int is_optres_ty(const sym* s, const cty* t) {
    if (!t) return -1;
    if (t->kind == TY_OPT) return 1;
    const char* hn = head_name(t);
    if (!hn) return -1;
    if (!strcmp(hn, "Option") || !strcmp(hn, "Result")) return 1;
    // 原语/本文件具名类型 → 可证明非 Option/Result;其余(泛型形参等) → 未知
    if (prim_cat(hn) || find_struct(s->f, hn) || find_class(s->f, hn) || find_enum(s->f, hn))
        return 0;
    return -1;
}

static const char* const OPTION_VARS[] = {"Some", "None"};
static const char* const RESULT_VARS[] = {"Ok", "Err"};

// ---------- 枚举穷尽 ----------
typedef struct { const char* const* names; size_t n; int is_static; } evlist;

static int enum_variants(const cfile* f, const cty* t, evlist* out) {
    if (!t) return 0;
    if (t->kind == TY_OPT) {
        out->names = OPTION_VARS; out->n = 2; out->is_static = 1; return 1;
    }
    if (t->kind == TY_NAMED) {
        const char* n = head_name(t);
        if (!n) return 0;
        if (strcmp(n, "Option") == 0) {
            out->names = OPTION_VARS; out->n = 2; out->is_static = 1; return 1;
        }
        if (strcmp(n, "Result") == 0) {
            out->names = RESULT_VARS; out->n = 2; out->is_static = 1; return 1;
        }
        const cdecl* en = find_enum(f, n);
        if (en) {
            out->n = en->en.nvariants;
            out->is_static = 0;
            out->names = NULL;
            if (en->en.nvariants) {
                out->names = (const char* const*)calloc(en->en.nvariants, sizeof(char*));
                for (size_t i = 0; i < en->en.nvariants; i++)
                    ((char**)out->names)[i] = en->en.variants[i].name;
            }
            return 1;
        }
    }
    return 0;
}

static void check_match_exhaustive(ctx* c, cexpr* m) {
    evlist ev = {0};
    if (!enum_variants(c->s->f, derive_type(c, m->scrut), &ev) || ev.n == 0) return;
    int* covered = (int*)calloc(ev.n, sizeof(int));
    int whole = 0;
    for (size_t i = 0; i < m->narms && !whole; i++) {
        cpat* p = m->arms[i].pat;
        if (p->kind == PAT_WILD || p->kind == PAT_IDENT) whole = 1;
        else if (p->kind == PAT_AGG && p->npath > 0) {
            for (size_t j = 0; j < ev.n; j++)
                if (strcmp(p->path[0], ev.names[j]) == 0) covered[j] = 1;
        }
    }
    if (!whole) {
        int missing = 0;
        for (size_t j = 0; j < ev.n; j++)
            if (!covered[j]) missing++;
        if (missing) {
            char buf[256] = {0};
            size_t o = 0;
            for (size_t j = 0; j < ev.n; j++)
                if (!covered[j]) o += (size_t)snprintf(buf + o, sizeof buf - o, "%s%s",
                                                       o ? " " : "", ev.names[j]);
            diag(c->k, "E2030", "match 不穷尽(not exhaustive):缺变体 %s", buf);
        }
    }
    free(covered);
    if (!ev.is_static && ev.names) free((void*)ev.names);
}

// ---------- spawn 闭包捕获的 Send 检查(E3010) ----------
typedef struct { const char** names; size_t n, cap; bind* env; } capset;

static void cap_add(capset* cs, const char* name) {
    if (name_in(cs->names, cs->n, name)) return;
    // 闭包引用外层已绑定名 → 捕获(env 快照于 spawn 调用点)
    if (!bind_find(cs->env, name)) return;
    if (cs->n == cs->cap) {
        cs->cap = cs->cap ? cs->cap * 2 : 8;
        cs->names = (const char**)realloc(cs->names, cs->cap * sizeof(char*));
        if (!cs->names) abort();
    }
    cs->names[cs->n++] = name;
}
static void cap_walk(capset* cs, cexpr* e) {
    if (!e) return;
    switch (e->kind) {
    case EX_IDENT: cap_add(cs, e->text); return;
    case EX_STR: case EX_INT: case EX_FLOAT: case EX_BOOL: case EX_VOID: return;
    case EX_TUPLE: case EX_ARRAY:
        for (size_t i = 0; i < e->nelems; i++) cap_walk(cs, e->elems[i]);
        return;
    case EX_STRUCT:
        for (size_t i = 0; i < e->nfields; i++)
            if (e->fields[i].value) cap_walk(cs, e->fields[i].value);
        return;
    case EX_UNARY: cap_walk(cs, e->ux); return;
    case EX_BINARY: cap_walk(cs, e->lhs); cap_walk(cs, e->rhs); return;
    case EX_RANGE: cap_walk(cs, e->from); cap_walk(cs, e->to); return;
    case EX_CALL:
        cap_walk(cs, e->callee);
        for (size_t i = 0; i < e->nelems; i++) cap_walk(cs, e->elems[i]);
        return;
    case EX_INDEX: cap_walk(cs, e->obj); cap_walk(cs, e->index); return;
    case EX_MEMBER: cap_walk(cs, e->obj); return;
    case EX_TYPEARGS: cap_walk(cs, e->obj); return;
    case EX_TRY: cap_walk(cs, e->obj); return;
    case EX_CLOSURE:
        cap_walk(cs, e->cbody);
        return;
    case EX_SCOPE: cap_walk(cs, (cexpr*)e->sbody); return;
    case EX_OWN: cap_walk(cs, (cexpr*)e->obody); return;
    case EX_IF: cap_walk(cs, e->cond); cap_walk(cs, (cexpr*)e->then_b);
        if (e->els) cap_walk(cs, e->els); return;
    case EX_MATCH:
        cap_walk(cs, e->scrut);
        for (size_t i = 0; i < e->narms; i++) cap_walk(cs, e->arms[i].expr);
        return;
    case EX_BLOCK: {
        cblock* b = e->block;
        for (size_t i = 0; i < b->nstmts; i++) {
            cstmt* st = b->stmts[i];
            switch (st->kind) {
            case ST_LET: cap_walk(cs, st->e); break;
            case ST_RET: if (st->e) cap_walk(cs, st->e); break;
            case ST_FOR: cap_walk(cs, st->iter); cap_walk(cs, (cexpr*)st->body); break;
            case ST_WHILE: cap_walk(cs, st->e); cap_walk(cs, (cexpr*)st->body); break;
            case ST_ASSIGN: cap_walk(cs, st->target); cap_walk(cs, st->value); break;
            case ST_EXPR: cap_walk(cs, st->e); break;
            }
        }
        if (b->tail) cap_walk(cs, b->tail);
        return;
    }
    default: return;
    }
}

static void check_spawn_send(ctx* c, cexpr* call) {
    // 捕获外层闭包或本体内自由绑定 → Send 校验
    if (c->fn_no_spawn) {
        diag(c->k, "E4030", "no_spawn 上下文中的 spawn");
        return;
    }
    if (call->nelems == 0) return;
    cexpr* f = call->elems[0];
    if (!f || f->kind != EX_CLOSURE) return;
    capset cs = {0};
    cs.env = c->env;
    cap_walk(&cs, f);
    int reported = 0;
    for (size_t i = 0; i < cs.n && !reported; i++) {
        bind* b = bind_find(c->env, cs.names[i]);
        if (b && b->ty && !ty_send(c->s, b->ty, 0)) {
            diag(c->k, "E3010", "spawn 捕获非 Send(Send):%s", cs.names[i]);
            reported = 1;
        }
    }
    free(cs.names);
}

static const char* expr_root_name(cexpr* e) { return root_ident(e); }

// 成员调用的接收者是否 arena(arena.list/zeros/array/Arena.fixed 放行)
static int member_is_arena_op(cexpr* callee) {
    if (!callee || callee->kind != EX_MEMBER || !callee->m_is_name || !callee->mname) return 0;
    const char* m = callee->mname;
    if (!(strcmp(m, "zeros") == 0 || strcmp(m, "array") == 0 || strcmp(m, "list") == 0
          || strcmp(m, "fixed") == 0 || strcmp(m, "new") == 0)) return 0;
    const char* root = expr_root_name(callee->obj);
    return root && (strcmp(root, "arena") == 0 || strcmp(root, "Arena") == 0);
}

// E3040 分类:受限语境(own / #[no_alloc] / bare)内该调用是否为分配
static int call_alloc_in_ctx(ctx* c, cexpr* call) {
    cexpr* cal = call->callee;
    if (!cal) return 0;
    if (cal->kind == EX_MEMBER && cal->m_is_name && cal->mname) {
        const char* m = cal->mname;
        if (strcmp(m, "into_gc") == 0) return c->in_own ? 0 : 1;
        if (strcmp(m, "to_string") == 0) return 1;
        if (strcmp(m, "push") == 0) {
            if (c->in_own) {
                const char* root = expr_root_name(cal->obj);
                if (root && (name_in((const char**)c->arena_binds, c->n_arena, root)
                             || name_in((const char**)c->gc_local, c->n_gc, root)))
                    return 0;
                return 1;
            }
            return 1;
        }
        return member_is_arena_op(cal) ? 0 : 0;
    }
    if (cal->kind == EX_TYPEARGS && cal->obj && cal->obj->kind == EX_IDENT) {
        const char* n = cal->obj->text;
        if (strcmp(n, "Box") == 0 || strcmp(n, "List") == 0 || strcmp(n, "Map") == 0
            || strcmp(n, "String") == 0)
            return 1;
        return 0;
    }
    if (cal->kind == EX_IDENT) {
        const char* n = cal->text;
        const cdecl* d = find_fn(c->s->f, n);
        if (d && d->kind == D_FN && !fn_is_noalloc(d) && fn_may_alloc_summary(c->s->f, n)) return 1;
        return 0;
    }
    return 0;
}

static void check_alloc_ctx(ctx* c, const char* kind) {
    if (c->in_own) diag(c->k, "E3040", "own 块内 GC 分配(allocation):%s", kind);
    else diag(c->k, "E3040", "no_alloc 上下文 GC 分配(allocation):%s", kind);
}

// ---------- 能力调用(pure/comptime)判定 ----------
// 接收者类型的语法形态:&Trait(声明 trait)→ 能力调用
static int receiver_is_capability_trait(ctx* c, cexpr* callee) {
    if (!callee || callee->kind != EX_MEMBER) return 0;
    const char* root = root_ident(callee->obj);
    if (!root) return 0;
    bind* b = bind_find(c->env, root);
    if (!b || !b->ty || b->ty->kind != TY_REF || !b->ty->sub) return 0;
    const char* n = head_name(b->ty->sub);
    return n && find_trait(c->s->f, n) ? 1 : 0;
}

// ---------- 表达式 ----------
static void check_expr(ctx* c, cexpr* e);

static int brk_depth = 0; // v0.7 修订二:E2070 循环深度(sem 单遍单线程)
static int brk_outer = 0;        // v0.7:E2072 —— 闭包外层是否存在循环
static int fn_has_drop_local = 0; // v0.7:E2071 —— 本函数含注解 Drop 局部(保守函数级口径)
// v0.7:E2071 —— 注解类型是否带用户 Drop impl(镜像 rt type_has_drop)
static int sem_has_drop_impl(const sym* s, const char* ty) {
    if (!ty || !s || !s->f) return 0;
    for (size_t i = 0; i < s->f->ndecls; i++) {
        const cdecl* d = &s->f->decls[i];
        if (d->kind != D_IMPL) continue;
        const char* tr = head_name(d->impl.trait_ty);
        const char* fo = head_name(d->impl.for_ty);
        if (tr && fo && strcmp(tr, "Drop") == 0 && strcmp(fo, ty) == 0) return 1;
    }
    return 0;
}

static void check_block(ctx* c, cblock* b) {
    if (!b) return;
    for (size_t i = 0; i < b->nstmts; i++) {
        cstmt* st = b->stmts[i];
        switch (st->kind) {
        case ST_LET: {
            // 单标识符模式 → 绑定
            if (st->pat && st->pat->kind == PAT_IDENT && st->pat->name) {
                cty* ty = st->ty ? st->ty : derive_type(c, st->e);
                // v0.7 E2071(保守函数级):注解带 Drop impl → 本函数内 break/continue 一律拒绝
                if (st->ty && st->ty->kind == TY_NAMED && st->ty->npath == 1
                    && sem_has_drop_impl(c->s, st->ty->path[0]))
                    fn_has_drop_local = 1;
                // E2010(保守):注解原语类别 vs 初值类别(字面量直接归类;其余推导,推得出才判)
                if (st->ty) {
                    const char* ann = (st->ty->kind == TY_NAMED && st->ty->npath == 1) ? st->ty->path[0] : NULL;
                    int acat = prim_cat(ann);
                    if (acat && st->e) {
                        int lcat = 0;
                        const char* lname = NULL;
                        if (st->e->kind == EX_STR) lcat = 3;
                        else if (st->e->kind == EX_BOOL) lcat = 2;
                        else if (st->e->kind == EX_INT || st->e->kind == EX_FLOAT) lcat = 1;
                        else {
                            lname = head_name(derive_type(c, st->e));
                            lcat = prim_cat(lname);
                        }
                        if (lcat && lcat != acat)
                            diag(c->k, "E2010", "类型不匹配:期望 %s,实得 %s", ann,
                                 lname ? lname : cat_name(lcat));
                    }
                }
                check_shadow_prelude(c, st->pat->name); // W8040
                int handled_move = 0;
                // own 块 move 语义:let x = <arena 句柄> 是 move(源失效,别名接管)
                if (c->in_own && st->e && st->e->kind == EX_IDENT) {
                    const char* src = st->e->text;
                    size_t ai = c->n_arena;
                    for (size_t j = 0; j < c->n_arena; j++)
                        if (strcmp(c->arena_binds[j], src) == 0) { ai = j; break; }
                    if (ai < c->n_arena && strcmp(c->arena_binds[ai], src) == 0) {
                        // 源失效:moved 加入源;arena_binds 移除源、加入别名
                        char** nm = (char**)realloc(c->moved, (c->n_moved + 1) * sizeof(char*));
                        c->moved = nm;
                        c->moved[c->n_moved++] = (char*)src;
                        for (size_t j = ai; j + 1 < c->n_arena; j++) c->arena_binds[j] = c->arena_binds[j + 1];
                        c->n_arena--;
                        char** ab = (char**)realloc(c->arena_binds, (c->n_arena + 1) * sizeof(char*));
                        c->arena_binds = ab;
                        c->arena_binds[c->n_arena++] = (char*)st->pat->name;
                        handled_move = 1;
                    } else if (name_in((const char**)c->moved, c->n_moved, src)) {
                        diag(c->k, "E3050", "use-after-move(moved):arena 句柄 %s", src);
                        handled_move = 1;
                    }
                }
                if (!handled_move && st->e) {
                    if (c->in_own && st->e->kind == EX_CALL) {
                        const char* root = root_ident(st->e);
                        if (root && strcmp(root, "arena") == 0
                            && !name_in((const char**)c->arena_binds, c->n_arena, st->pat->name)) {
                            char** ab = (char**)realloc(c->arena_binds, (c->n_arena + 1) * sizeof(char*));
                            c->arena_binds = ab;
                            c->arena_binds[c->n_arena++] = (char*)st->pat->name;
                        }
                        if (st->e->callee && st->e->callee->kind == EX_MEMBER
                            && st->e->callee->m_is_name && st->e->callee->mname
                            && strcmp(st->e->callee->mname, "into_gc") == 0
                            && !name_in((const char**)c->gc_local, c->n_gc, st->pat->name)) {
                            char** g = (char**)realloc(c->gc_local, (c->n_gc + 1) * sizeof(char*));
                            c->gc_local = g;
                            c->gc_local[c->n_gc++] = (char*)st->pat->name;
                        }
                    }
                    check_expr(c, st->e);
                }
                bind_push(&c->env, st->pat->name, ty, c->depth);
            } else {
                if (st->e) check_expr(c, st->e);
            }
            break;
        }
        case ST_RET:
            if (st->e) check_expr(c, st->e);
            break;
        case ST_FOR:
            if (st->iter) check_expr(c, st->iter);
            brk_depth++;
            if (st->body) check_block(c, st->body);
            brk_depth--;
            break;
        case ST_WHILE:
            if (st->e) {
                check_cond_bool(c, st->e, "while");
                check_expr(c, st->e);
            }
            brk_depth++;
            if (st->body) check_block(c, st->body);
            brk_depth--;
            break;
        case ST_BREAK:
        case ST_CONTINUE: {
            const char* bkw = (st->kind == ST_BREAK ? "break" : "continue");
            if (brk_depth == 0) {
                if (brk_outer)
                    diag(c->k, "E2072", "%s 不得穿越闭包边界(闭包体是独立函数,不可 break/continue 外层循环)", bkw);
                else
                    diag(c->k, "E2070", "%s 出现在循环外(绑定同函数体最近循环)", bkw);
            } else if (fn_has_drop_local) {
                diag(c->k, "E2071", "%s 需越过带 Drop 局部的作用域(v1 静态拒绝;将 Drop 局部移入内层块或重构循环)", bkw);
            }
            break;
        }
        case ST_ASSIGN: {
            // own 块内对类值成员的可变写 → E3060
            if (c->in_own && st->target && st->target->kind == EX_MEMBER) {
                const char* root = root_ident(st->target);
                if (root) {
                    bind* b = bind_find(c->env, root);
                    const char* hn = b && b->ty ? head_name(b->ty) : NULL;
                    if (hn && find_class(c->s->f, hn)) {
                        diag(c->k, "E3060", "own 块内对 GC 值可变写(mutable):%s", root);
                    }
                }
            }
            if (st->target) check_expr(c, st->target);
            if (st->value) check_expr(c, st->value);
            break;
        }
        case ST_EXPR: {
            // W8020:丢弃 Result/Option 返回值
            cexpr* e = st->e;
            if (e && e->kind == EX_CALL) {
                const char* fnname = root_ident(e);
                if (fnname) {
                    const cdecl* d = find_fn(c->s->f, fnname);
                    if (d) {
                        cty* rt = d->fn_.ret;
                        if (rt && ((rt->kind == TY_OPT)
                                   || (rt->kind == TY_NAMED && (strcmp(head_name(rt), "Option") == 0
                                                                || strcmp(head_name(rt), "Result") == 0)))) {
                            diag(c->k, "W8020", "结果被丢弃(must-use):%s 返回 Option/Result", fnname);
                        }
                    }
                }
            }
            if (st->e) check_expr(c, st->e);
            break;
        }
        }
    }
    if (b->tail) check_expr(c, b->tail);
}

static void check_expr(ctx* c, cexpr* e) {
    if (!e) return;
    // arena 句柄 moved 后使用(E3050;仅 EX_IDENT 层报告,避免递归重复)
    if (c->in_own && e->kind == EX_IDENT
        && name_in((const char**)c->moved, c->n_moved, e->text))
        diag(c->k, "E3050", "use-after-move(moved):arena 句柄 %s", e->text);
    switch (e->kind) {
    case EX_STR: case EX_INT: case EX_FLOAT: case EX_BOOL: case EX_VOID:
        return;
    case EX_IDENT: return;
    case EX_TUPLE: case EX_ARRAY:
        for (size_t i = 0; i < e->nelems; i++) check_expr(c, e->elems[i]);
        return;
    case EX_STRUCT:
        if ((c->in_own || c->fn_noalloc) && e->npath > 0 && find_class(c->s->f, e->path[0])) {
            check_alloc_ctx(c, "类构造");
        }
        for (size_t i = 0; i < e->nfields; i++)
            if (e->fields[i].value) check_expr(c, e->fields[i].value);
        return;
    case EX_UNARY: {
        cty* t = derive_type(c, e->ux);
        const char* hn = head_name(t);
        if (hn) {
            int cat = prim_cat(hn);
            if (e->uop == UN_NEG && cat && cat != 1)
                diag(c->k, "E2010", "一元 - 需要数值,实际 %s", hn);
            if (e->uop == UN_NOT && cat && cat != 2)
                diag(c->k, "E2010", "一元 ! 需要 Bool,实际 %s", hn);
        }
        check_expr(c, e->ux);
        return;
    }
    case EX_BINARY: {
        if (e->bop == B_AND || e->bop == B_OR || e->bop == B_OROR) {
            cty* lt = derive_type(c, e->lhs);
            cty* rt = derive_type(c, e->rhs);
            int lc = prim_cat(head_name(lt)), rc = prim_cat(head_name(rt));
            // v0.7 修订一:|| 与 && 同口径(仅已知非 Bool 标量类别报错)
            if ((e->bop == B_AND || e->bop == B_OROR) && ((lc && lc != 2) || (rc && rc != 2)))
                diag(c->k, "E2010", "%s 需要 Bool", e->bop == B_AND ? "&&" : "||");
            check_expr(c, e->lhs);
            check_expr(c, e->rhs);
            return;
        }
        cty* lt = derive_type(c, e->lhs);
        cty* rt = derive_type(c, e->rhs);
        const char* ln = head_name(lt);
        const char* rn = head_name(rt);
        int lc = prim_cat(ln), rc = prim_cat(rn);
        if (e->bop >= B_EQ && e->bop <= B_GE) {
            // 比较:两侧已知时,同类(数值/Bool/Str)或同名类型方可(Rust 同规则)
            if (ln && rn && lc && rc
                && !(lc == rc || (lc == 2 || rc == 2) || (lc == 3 || rc == 3)
                     || (lc == 0 || rc == 0) || !strcmp(ln, rn)))
                diag(c->k, "E2010", "比较类型不匹配:%s vs %s", ln, rn);
        } else {
            // 算术/回绕:两侧已知时需均为数值;Add 亦接纳双 Str(拼接,T2 规格修订 2026-09-08)
            if (ln && rn && lc && rc && !(lc == 1 && rc == 1)
                && !(e->bop == B_ADD && lc == 3 && rc == 3))
                diag(c->k, "E2010", "算术需要数值,实际 %s 与 %s", ln, rn);
        }
        check_expr(c, e->lhs);
        check_expr(c, e->rhs);
        return;
    }
    case EX_RANGE: check_expr(c, e->from); check_expr(c, e->to); return;
    case EX_CALL: {
        // 分配语境(E3040):own 块 / #[no_alloc] / bare
        if ((c->in_own || c->fn_noalloc) && call_alloc_in_ctx(c, e)) {
            const char* root = expr_root_name(e->callee);
            check_alloc_ctx(c, root ? root : "调用");
            return;
        }
        // 泛型无 TypeArgs 调用的推断诊断(v0.7 修订三)
        if (e->callee && ((e->callee->kind == EX_IDENT && e->callee->text)
            || (e->callee->kind == EX_TYPEARGS && e->callee->obj
                && e->callee->obj->kind == EX_IDENT && e->callee->obj->text))) {
            const char* cname = (e->callee->kind == EX_IDENT)
                ? e->callee->text : e->callee->obj->text;
            check_call_infer(c, e, cname);
        }
        // Channel[T](cap):元素须 Send(E3020)
        if (e->callee && e->callee->kind == EX_TYPEARGS && e->callee->obj
            && e->callee->obj->kind == EX_IDENT
            && strcmp(e->callee->obj->text, "Channel") == 0
            && e->callee->ntargs > 0) {
            if (!ty_send(c->s, e->callee->targs[0], 0))
                diag(c->k, "E3020", "channel 元素非 Send(Send):Channel 元素类型");
        }
        // 能力调用:pure/comptime 函数内通过 &Trait 接收者调用
        if ((c->fn_pure || c->fn_comptime) && receiver_is_capability_trait(c, e->callee)) {
            if (c->fn_pure) diag(c->k, "E4020", "pure 函数含能力调用(pure)");
            else diag(c->k, "E6020", "comptime 函数含能力调用(comptime)");
        }
        // E4042(§9.6 v0.6):捕获闭包实参传入 extern "c" 的 fn 指针形参——
        // C 回调无 env 槽;仅裸 fn 名(无捕获)可作 C-ABI 回调(镜像自举 ext_cb_target)
        if (e->callee && e->callee->kind == EX_IDENT && e->callee->text) {
            const cdecl* extd = find_kind(c->s->f, D_FN, e->callee->text);
            if (extd && extd->kind == D_FN && extd->fn_.abi && extd->fn_.body == NULL) {
                int hasfnty = 0;
                for (size_t pi = 0; pi < extd->fn_.nparams; pi++) {
                    const cparam* pp = &extd->fn_.params[pi];
                    if (pp->ty && pp->ty->kind == TY_FN) hasfnty = 1;
                }
                if (hasfnty) {
                    for (size_t ai = 0; ai < e->nelems; ai++) {
                        if (e->elems[ai] && e->elems[ai]->kind == EX_CLOSURE) {
                            diag(c->k, "E4042", "捕获闭包不可作 C-ABI 回调实参(无 env 槽):%s",
                                 e->callee->text);
                            break;
                        }
                    }
                }
            }
        }
        // or:只能用于 Option/Result(§4)
        if (e->callee && e->callee->kind == EX_MEMBER && e->callee->m_is_name
            && e->callee->mname && strcmp(e->callee->mname, "or") == 0
            && is_optres_ty(c->s, derive_type(c, e->callee->obj)) == 0)
            diag(c->k, "E2010", "`or` 只能用于 Option/Result");
        // spawn:no_spawn 上下文 E4030;否则闭包捕获 Send E3010
        if (e->callee && e->callee->kind == EX_MEMBER && e->callee->m_is_name
            && e->callee->mname && strcmp(e->callee->mname, "spawn") == 0) {
            check_spawn_send(c, e);
        }
        if (e->callee) {
            c->in_callee++;
            check_expr(c, e->callee);
            c->in_callee--;
        }
        for (size_t i = 0; i < e->nelems; i++) check_expr(c, e->elems[i]);
        return;
    }
    case EX_INDEX: check_expr(c, e->obj); check_expr(c, e->index); return;
    case EX_MEMBER: {
        check_expr(c, e->obj);
        // 字段存在性(保守):接收者为本文件具名 struct/class 且成员既非字段也非 prop;调用者位置跳过
        if (e->m_is_name && e->mname && !c->in_callee) {
            cty* t = derive_type(c, e->obj);
            const char* hn = head_name(t);
            if (hn && !is_prim(hn) && (find_struct(c->s->f, hn) || find_class(c->s->f, hn))
                && !type_has_member(c->s->f, hn, e->mname))
                diag(c->k, "E2010", "`%s` 无字段 `%s`", hn, e->mname);
        }
        return;
    }
    case EX_TYPEARGS: check_expr(c, e->obj); return;
    case EX_TRY: {
        // §5.3:操作数须 Result/Option;所在函数须返回 Result/Option(可证明时)
        int op_ok = is_optres_ty(c->s, derive_type(c, e->obj));
        if (op_ok == 0)
            diag(c->k, "E2010", "`?` 只能用于 Result/Option");
        int fn_ok = is_optres_ty(c->s, c->fn_ret);
        if (fn_ok == 0)
            diag(c->k, "E2010", "`?` 只能用于返回 Result/Option 的函数(§5.3)");
        check_expr(c, e->obj);
        return;
    }
    case EX_CLOSURE: {
        // 闭包参数进入环境(参数深于调用处)
        c->depth++;
        for (size_t i = 0; i < e->ncparams; i++)
            if (e->cparams[i].name)
                bind_push(&c->env, e->cparams[i].name, e->cparams[i].ty, c->depth);
        {
            // v0.7 E2072:闭包体是独立函数边界——深度清零,外层循环存在性穿透
            int sd = brk_depth;
            int so = brk_outer;
            if (sd > 0) so = 1;
            brk_depth = 0;
            brk_outer = so;
            check_expr(c, e->cbody);
            brk_depth = sd;
            brk_outer = so;
        }
        return;
    }
    case EX_SCOPE: {
        c->depth++;
        if (e->sparam) bind_push(&c->env, e->sparam, NULL, c->depth);
        check_block(c, e->sbody);
        return;
    }
    case EX_OWN: {
        c->in_own = 1;
        char** save_arena = c->arena_binds;
        size_t save_na = c->n_arena;
        char** save_moved = c->moved;
        size_t save_nm = c->n_moved;
        char** save_gc = c->gc_local;
        size_t save_ng = c->n_gc;
        c->arena_binds = NULL;
        c->n_arena = 0;
        c->moved = NULL;
        c->n_moved = 0;
        c->gc_local = NULL;
        c->n_gc = 0;
        c->depth++;
        check_block(c, e->obody);
        free(c->arena_binds);
        free(c->moved);
        free(c->gc_local);
        c->arena_binds = save_arena;
        c->n_arena = save_na;
        c->moved = save_moved;
        c->n_moved = save_nm;
        c->gc_local = save_gc;
        c->n_gc = save_ng;
        c->in_own = 0;
        return;
    }
    case EX_IF:
        check_cond_bool(c, e->cond, "if");
        check_expr(c, e->cond);
        check_block(c, e->then_b);
        if (e->els) check_expr(c, e->els);
        return;
    case EX_MATCH:
        check_match_exhaustive(c, e);
        check_expr(c, e->scrut);
        for (size_t i = 0; i < e->narms; i++)
            if (e->arms[i].expr) check_expr(c, e->arms[i].expr);
        return;
    case EX_BLOCK:
        check_block(c, e->block);
        return;
    default:
        return;
    }
}

// ---------- 函数体驱动 ----------
// 方法所属 trait 的同名方法带 #[no_alloc] → 实现体受契约约束(E3040)
static int impl_method_noalloc_contract(const sym* s, const cdecl* impl, const char* mname) {
    if (!impl || impl->kind != D_IMPL || !mname) return 0;
    const char* tn = head_name(impl->impl.trait_ty);
    const cdecl* tr = tn ? find_trait(s->f, tn) : NULL;
    if (!tr) return 0;
    for (size_t i = 0; i < tr->trait.nitems; i++) {
        const ctraititem* it = &tr->trait.items[i];
        if (it->kind == TI_METHOD && it->m->name && strcmp(it->m->name, mname) == 0)
            return has_attr(it->m->attrs, it->m->nattrs, "no_alloc");
    }
    return 0;
}

static void check_fn(ctx* c, const cfn* f, int no_alloc_contract) {
    ctx sub;
    sub = *c;
    sub.env = NULL;
    sub.depth = 1;
    sub.in_own = 0;
    sub.arena_binds = NULL;
    sub.n_arena = 0;
    sub.moved = NULL;
    sub.n_moved = 0;
    sub.gc_local = NULL;
    sub.n_gc = 0;
    sub.fn_pure = has_attr(f->attrs, f->nattrs, "pure");
    sub.fn_comptime = f->is_comptime;
    sub.fn_no_spawn = has_attr(f->attrs, f->nattrs, "no_spawn");
    sub.fn_ret = f->ret;
    sub.fn_noalloc = has_attr(f->attrs, f->nattrs, "no_alloc") || no_alloc_contract
                     || (c->profile == SEM_BARE);
    for (size_t i = 0; i < f->nparams; i++) {
        const cparam* pr = &f->params[i];
        if (!pr->is_receiver && pr->name) {
            check_shadow_prelude(&sub, pr->name); // W8040
            bind_push(&sub.env, pr->name, pr->ty, 1);
        }
    }
    fn_has_drop_local = 0;
    brk_outer = 0;
    if (f->body) check_block(&sub, f->body);
    bind_free(sub.env);
}

// ---------- 顶层 ----------
// ---------- FFI 诊断面(§9.6 v0.6/v0.7/v0.8;与自举线 sem_main 对齐) ----------
// C-ABI 类型治理:容器/能力类型不得跨 extern 边界(镜像自举 ext_nonabi_ty);
// Option[Str] 放行(NULL↔None 编组面,v0.8);fn 类型 = C 函数指针,合法
static int ffi_nonabi(const cty* t, int depth) {
    if (!t || depth > 8) return 0;
    if (t->kind == TY_NAMED) {
        const char* h = t->npath > 0 ? t->path[t->npath - 1] : "";
        if (strcmp(h, "Option") == 0) {
            if (t->nargs == 1 && t->args[0]->kind == TY_NAMED
                && t->args[0]->npath > 0
                && strcmp(t->args[0]->path[t->args[0]->npath - 1], "Str") == 0)
                return 0;
            return 1;
        }
        if (strcmp(h, "List") == 0 || strcmp(h, "Atomic") == 0
            || strcmp(h, "Result") == 0 || strcmp(h, "Box") == 0
            || strcmp(h, "Mutex") == 0 || strcmp(h, "Channel") == 0
            || strcmp(h, "Global") == 0) return 1;
        return 0;
    }
    if (t->kind == TY_FN) return 0;
    if (t->kind == TY_REF || t->kind == TY_SLICE || t->kind == TY_OPT)
        return ffi_nonabi(t->sub, depth + 1);
    if (t->kind == TY_ARRAY)
        return ffi_nonabi(t->elem, depth + 1);
    return 0;
}

static void check_ffi_decls(ck* k, const cfile* f) {
    for (size_t i = 0; i < f->ndecls; i++) {
        const cdecl* d = &f->decls[i];
        if (d->kind == D_FN) {
            const cfn* fn = &d->fn_;
            int is_ext = fn->abi && !fn->body;
            if (is_ext && !has_attr(fn->attrs, fn->nattrs, "trusted"))
                diag(k, "W8050", "extern 未标记 #[trusted](信任边界):%s", fn->name);
            if (!is_ext && has_attr(fn->attrs, fn->nattrs, "trusted"))
                diag(k, "E4040", "#[trusted] 用于非 extern 声明:%s", fn->name);
            if (has_attr(fn->attrs, fn->nattrs, "repr"))
                diag(k, "E4041", "#[repr(c)] 用于非 struct 声明:%s", fn->name);
            if (fn->variadic && !is_ext)
                diag(k, "E4044", "变参形参(...)仅限 extern 声明:%s", fn->name);
            if (is_ext) {
                for (size_t pi = 0; pi < fn->nparams; pi++) {
                    const cparam* pp = &fn->params[pi];
                    if (pp->ty && ffi_nonabi(pp->ty, 0))
                        diag(k, "W8052", "extern 形参非 C-ABI 类型(§9.6):%s.%s",
                             fn->name, pp->name ? pp->name : "?");
                }
                if (fn->ret && ffi_nonabi(fn->ret, 0))
                    diag(k, "W8052", "extern 返回非 C-ABI 类型(§9.6):%s", fn->name);
            }
        } else if (d->kind == D_STRUCT) {
            if (has_attr(d->strukt.attrs, d->strukt.nattrs, "repr")) {
                for (size_t fi = 0; fi < d->strukt.nfields; fi++) {
                    const cfield* fl = &d->strukt.fields[fi];
                    if (fl->ty && ffi_nonabi(fl->ty, 0))
                        diag(k, "W8051", "repr(c) struct 含非 C-ABI 字段(§9.6):%s.%s",
                             d->strukt.name, fl->name);
                }
            }
        } else if (d->kind == D_ENUM) {
            if (has_attr(d->en.attrs, d->en.nattrs, "repr"))
                diag(k, "E4041", "#[repr(c)] 用于非 struct 声明(enum):%s", d->en.name);
        }
    }
}

ctron_sem_result ctron_sem_check_mode(const cfile* f, ctron_arena* arena, int profile) {
    ck k = {0};
    k.arena = arena;
    sym s = {f};
    ctx c = {0};
    c.k = &k;
    c.s = &s;
    c.depth = 1;
    c.profile = profile;

    // FFI 诊断面(§9.6;与自举线对齐)
    check_ffi_decls(&k, f);

    // 顶层 fn 体
    for (size_t i = 0; i < f->ndecls; i++) {
        const cdecl* d = &f->decls[i];
        switch (d->kind) {
        case D_FN:
            check_shadow_prelude(&c, d->fn_.name); // W8040
            check_fn(&c, &d->fn_, 0);
            break;
        case D_CLASS:
            for (size_t j = 0; j < d->klass.nitems; j++) {
                const cclassitem* it = &d->klass.items[j];
                if (it->kind == CT_METHOD && it->m->body) check_fn(&c, it->m, 0);
            }
            break;
        case D_IMPL:
            for (size_t j = 0; j < d->impl.nitems; j++) {
                const cimplitem* it = &d->impl.items[j];
                if (it->kind == II_METHOD && it->m->body)
                    check_fn(&c, it->m, impl_method_noalloc_contract(&s, d, it->m->name));
            }
            break;
        case D_TRAIT:
            for (size_t j = 0; j < d->trait.nitems; j++) {
                const ctraititem* it = &d->trait.items[j];
                if (it->kind == TI_METHOD && it->m->body) check_fn(&c, it->m, 0);
            }
            break;
        case D_TEST: {
            ctx tc = c;
            tc.env = NULL;
            tc.depth = 1;
            tc.fn_noalloc = (profile == SEM_BARE);
            fn_has_drop_local = 0;
            brk_outer = 0;
            check_block(&tc, d->test.body);
            bind_free(tc.env);
            break;
        }
        case D_STATIC:
            // E3031:非 Send 类型不可作静态存储
            if (!ty_send(&s, d->statik.ty, 0))
                diag(&k, "E3031", "static 存储非 Send(Send):%s", d->statik.name);
            break;
        case D_CONST:
            check_shadow_prelude(&c, d->konst.name); // W8040
            break;
        case D_STRUCT:
            check_shadow_prelude(&c, d->strukt.name); // W8040
            // W8010:struct 含类引用字段 → 拷贝浅共享
            for (size_t j = 0; j < d->strukt.nfields; j++) {
                const cfield* fl = &d->strukt.fields[j];
                const char* hn = head_name(fl->ty);
                if (hn && find_class(f, hn)) {
                    diag(&k, "W8010", "struct 含类引用字段(浅拷贝):%s.%s", d->strukt.name, fl->name);
                    break;
                }
            }
            break;
        default:
            break;
        }
    }

    ctron_sem_result r;
    r.diags = k.d;
    r.ndiags = k.n;
    return r;
}

ctron_sem_result ctron_sem_check(const cfile* f, ctron_arena* arena) {
    return ctron_sem_check_mode(f, arena, SEM_FULL);
}
