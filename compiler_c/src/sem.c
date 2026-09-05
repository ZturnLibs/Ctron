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
    int in_own;
    char** arena_binds; // own 块内 arena 句柄活绑定
    size_t n_arena;
    char** moved; // 已 move 的句柄名
    size_t n_moved;
} ctx;

static int name_in(const char** a, size_t n, const char* name) {
    for (size_t i = 0; i < n; i++)
        if (strcmp(a[i], name) == 0) return 1;
    return 0;
}

// 前向
static void check_expr(ctx* c, cexpr* e);
static void check_block(ctx* c, cblock* b);
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

// 语法导向的局部类型推导(够 Send/穷尽/效果检查即可)
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
        }
        return NULL;
    }
    case EX_IDENT: {
        bind* b = bind_find(c->env, e->text);
        return b ? b->ty : NULL;
    }
    case EX_STR: return mk_named(c->k->arena, "Str", NULL, 0);
    case EX_BOOL: return mk_named(c->k->arena, "Bool", NULL, 0);
    case EX_TRY: return mk_named(c->k->arena, "Option", NULL, 0);
    default: return NULL;
    }
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

static void check_block(ctx* c, cblock* b) {
    if (!b) return;
    for (size_t i = 0; i < b->nstmts; i++) {
        cstmt* st = b->stmts[i];
        switch (st->kind) {
        case ST_LET: {
            // 单标识符模式 → 绑定
            if (st->pat && st->pat->kind == PAT_IDENT && st->pat->name) {
                cty* ty = st->ty ? st->ty : derive_type(c, st->e);
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
            if (st->body) check_block(c, st->body);
            break;
        case ST_WHILE:
            if (st->e) check_expr(c, st->e);
            if (st->body) check_block(c, st->body);
            break;
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
        for (size_t i = 0; i < e->nfields; i++)
            if (e->fields[i].value) check_expr(c, e->fields[i].value);
        return;
    case EX_UNARY: check_expr(c, e->ux); return;
    case EX_BINARY: check_expr(c, e->lhs); check_expr(c, e->rhs); return;
    case EX_RANGE: check_expr(c, e->from); check_expr(c, e->to); return;
    case EX_CALL: {
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
        // spawn:no_spawn 上下文 E4030;否则闭包捕获 Send E3010
        if (e->callee && e->callee->kind == EX_MEMBER && e->callee->m_is_name
            && e->callee->mname && strcmp(e->callee->mname, "spawn") == 0) {
            check_spawn_send(c, e);
        }
        if (e->callee) check_expr(c, e->callee);
        for (size_t i = 0; i < e->nelems; i++) check_expr(c, e->elems[i]);
        return;
    }
    case EX_INDEX: check_expr(c, e->obj); check_expr(c, e->index); return;
    case EX_MEMBER: check_expr(c, e->obj); return;
    case EX_TYPEARGS: check_expr(c, e->obj); return;
    case EX_TRY: check_expr(c, e->obj); return;
    case EX_CLOSURE: {
        // 闭包参数进入环境(参数深于调用处)
        c->depth++;
        for (size_t i = 0; i < e->ncparams; i++)
            if (e->cparams[i].name)
                bind_push(&c->env, e->cparams[i].name, e->cparams[i].ty, c->depth);
        check_expr(c, e->cbody);
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
        c->arena_binds = NULL;
        c->n_arena = 0;
        c->moved = NULL;
        c->n_moved = 0;
        c->depth++;
        check_block(c, e->obody);
        free(c->arena_binds);
        free(c->moved);
        c->arena_binds = save_arena;
        c->n_arena = save_na;
        c->moved = save_moved;
        c->n_moved = save_nm;
        c->in_own = 0;
        return;
    }
    case EX_IF:
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
static void check_fn(ctx* c, const cfn* f) {
    ctx sub;
    sub = *c;
    sub.env = NULL;
    sub.depth = 1;
    sub.in_own = 0;
    sub.arena_binds = NULL;
    sub.n_arena = 0;
    sub.moved = NULL;
    sub.n_moved = 0;
    sub.fn_pure = has_attr(f->attrs, f->nattrs, "pure");
    sub.fn_comptime = f->is_comptime;
    sub.fn_no_spawn = has_attr(f->attrs, f->nattrs, "no_spawn");
    for (size_t i = 0; i < f->nparams; i++) {
        const cparam* pr = &f->params[i];
        if (!pr->is_receiver && pr->name) bind_push(&sub.env, pr->name, pr->ty, 1);
    }
    if (f->body) check_block(&sub, f->body);
    bind_free(sub.env);
}

// ---------- 顶层 ----------
ctron_sem_result ctron_sem_check(const cfile* f, ctron_arena* arena) {
    ck k = {0};
    k.arena = arena;
    sym s = {f};
    ctx c = {0};
    c.k = &k;
    c.s = &s;
    c.depth = 1;

    // 顶层 fn 体
    for (size_t i = 0; i < f->ndecls; i++) {
        const cdecl* d = &f->decls[i];
        switch (d->kind) {
        case D_FN:
            check_fn(&c, &d->fn_);
            break;
        case D_CLASS:
            for (size_t j = 0; j < d->klass.nitems; j++) {
                const cclassitem* it = &d->klass.items[j];
                if (it->kind == CT_METHOD && it->m->body) check_fn(&c, it->m);
            }
            break;
        case D_IMPL:
            for (size_t j = 0; j < d->impl.nitems; j++) {
                const cimplitem* it = &d->impl.items[j];
                if (it->kind == II_METHOD && it->m->body) check_fn(&c, it->m);
            }
            break;
        case D_TRAIT:
            for (size_t j = 0; j < d->trait.nitems; j++) {
                const ctraititem* it = &d->trait.items[j];
                if (it->kind == TI_METHOD && it->m->body) check_fn(&c, it->m);
            }
            break;
        case D_TEST: {
            ctx tc = c;
            tc.env = NULL;
            tc.depth = 1;
            check_block(&tc, d->test.body);
            bind_free(tc.env);
            break;
        }
        case D_STATIC:
            // E3031:非 Send 类型不可作静态存储
            if (!ty_send(&s, d->statik.ty, 0))
                diag(&k, "E3031", "static 存储非 Send(Send):%s", d->statik.name);
            break;
        case D_STRUCT:
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
