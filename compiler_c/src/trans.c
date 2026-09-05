// trans.c —— C10-a:Ctron → C 转译后端(数值域骨架)。
// 语义契约 = rt.c 解释器(差分对象):
//   - 字面量:无后缀整型 = I32(raw 承载,生成码用 int64);浮点默认 F64(F32 注解同为 double,对齐 apply_decl);
//   - 算术:结果宽度取左操作数;中间值 128 位 + fits 检查;溢出 panic "integer overflow (<op>)";
//     除零 "division by zero (/)"/"(%)";无符号取负恒溢出(对齐 rt fits 的 x>=0 约束);
//   - 回绕 +% -%:二补截断(wrap_int);
//   - 入口 coerce:参数/let 注解按 fits 检查("…(decl)");返回值不 coerce(rt 不做);
//   - 比较:__int128 数值比较(规避 C 有符号/无符号混比陷阱,对齐 val_eq);
//   - test 块顺序执行;panic 长跳 main 帧 → stderr + exit 1(对齐 ctronc run/test 行为)。
// v1 拒绝域:Str/数组/struct/class/enum/match/闭包/Option/Result/?/GC/own/scope/tuple/const/static。
#include "trans.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ================= 字符串缓冲 =================
typedef struct {
    char* d;
    size_t n, cap;
} sb;
static void sb_c(sb* b, char c) {
    if (b->n + 2 > b->cap) {
        b->cap = b->cap ? b->cap * 2 : 256;
        b->d = (char*)realloc(b->d, b->cap);
        if (!b->d) abort();
    }
    b->d[b->n++] = c;
    b->d[b->n] = '\0';
}
static void sb_s(sb* b, const char* s) {
    if (s)
        while (*s) sb_c(b, *s++);
}
static void sb_f(sb* b, const char* fmt, ...) {
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    sb_s(b, tmp);
}
static void sb_free(sb* b) { free(b->d); }

// ================= 类型模型 =================
enum { T_UNK, T_INT, T_FLT, T_BOOL };
typedef struct {
    int k;
    int bits, us;
} ty;
static ty ty_int(int bits, int us) { ty t = {T_INT, bits, us}; return t; }
static ty ty_flt(void) { ty t = {T_FLT, 64, 0}; return t; }
static ty ty_bool(void) { ty t = {T_BOOL, 1, 0}; return t; }
static ty ty_unk(void) { ty t = {T_UNK, 0, 0}; return t; }

static ty suff_ty(const char* s) {
    int bits = 32, us = 0, isf = 0;
    if (s && *s) {
        if (!strcmp(s, "i8")) bits = 8;
        else if (!strcmp(s, "i16")) bits = 16;
        else if (!strcmp(s, "i32")) bits = 32;
        else if (!strcmp(s, "i64") || !strcmp(s, "isize")) bits = 64;
        else if (!strcmp(s, "u8")) { bits = 8; us = 1; }
        else if (!strcmp(s, "u16")) { bits = 16; us = 1; }
        else if (!strcmp(s, "u32")) { bits = 32; us = 1; }
        else if (!strcmp(s, "u64") || !strcmp(s, "usize")) { bits = 64; us = 1; }
        else if (!strcmp(s, "f32") || !strcmp(s, "f64")) isf = 1;
    }
    return isf ? ty_flt() : ty_int(bits, us);
}
static ty decl_ty(const cty* t) {
    if (!t || t->kind != TY_NAMED || t->npath != 1) return ty_unk();
    const char* n = t->path[0];
    if (!strcmp(n, "Bool")) return ty_bool();
    static const struct { const char* n; int bits, us; } IS[] = {
        {"I8", 8, 0}, {"I16", 16, 0}, {"I32", 32, 0}, {"I64", 64, 0}, {"ISize", 64, 0},
        {"U8", 8, 1}, {"U16", 16, 1}, {"U32", 32, 1}, {"U64", 64, 1}, {"USize", 64, 1},
    };
    for (size_t i = 0; i < sizeof IS / sizeof IS[0]; i++)
        if (!strcmp(n, IS[i].n)) return ty_int(IS[i].bits, IS[i].us);
    if (!strcmp(n, "F32") || !strcmp(n, "F64")) return ty_flt();
    return ty_unk();
}
static const char* ctype_of(ty t) {
    if (t.k == T_FLT) return "double";
    if (t.k == T_BOOL) return "int";
    if (t.us) {
        switch (t.bits) {
        case 8: return "uint8_t";
        case 16: return "uint16_t";
        case 32: return "uint32_t";
        default: return "uint64_t";
        }
    }
    switch (t.bits) {
    case 8: return "int8_t";
    case 16: return "int16_t";
    case 32: return "int32_t";
    default: return "int64_t";
    }
}
static const char* wlname(ty t) {
    if (t.k == T_FLT) return "f64";
    if (t.k == T_BOOL) return "b";
    static char buf[8];
    snprintf(buf, sizeof buf, "%c%d", t.us ? 'u' : 'i', t.bits);
    return buf;
}

// ================= 上下文 =================
#define MAX_FNS 256
#define MAX_VARS 128
#define MAX_SCOPES 64
typedef struct scope {
    struct { char* name; ty t; } vars[MAX_VARS];
    size_t n;
    struct scope* up;
} scope;

typedef struct {
    sb head, body;
    char* helpers[160];
    size_t n_helpers;
    struct { char* name; ty ret; int nparams; int is_void; } fns[MAX_FNS];
    size_t nfns;
    scope* sc;
    scope scopes[MAX_SCOPES];
    size_t n_scopes;
    char* err;
    ctron_arena* a;
    int tmpn;
    int in_test;
} tc;

static void terr(tc* c, const char* fmt, ...) {
    if (c->err) return;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    c->err = ctron_arena_strndup(c->a, buf, strlen(buf));
}
static void use_helper(tc* c, const char* name) {
    for (size_t i = 0; i < c->n_helpers; i++)
        if (!strcmp(c->helpers[i], name)) return;
    if (c->n_helpers < 160) c->helpers[c->n_helpers++] = ctron_arena_strndup(c->a, name, strlen(name));
}
static void add_fn(tc* c, const char* name, ty ret, int nparams, int is_void) {
    if (c->nfns >= MAX_FNS) return;
    c->fns[c->nfns].name = ctron_arena_strndup(c->a, name, strlen(name));
    c->fns[c->nfns].ret = ret;
    c->fns[c->nfns].nparams = nparams;
    c->fns[c->nfns].is_void = is_void;
    c->nfns++;
}
static int fn_lookup(tc* c, const char* name, int* nparams, int* is_void, ty* ret) {
    for (size_t i = 0; i < c->nfns; i++)
        if (!strcmp(c->fns[i].name, name)) {
            *nparams = c->fns[i].nparams;
            *is_void = c->fns[i].is_void;
            *ret = c->fns[i].ret;
            return 1;
        }
    return 0;
}
static void scope_push(tc* c) {
    if (c->n_scopes >= MAX_SCOPES) return;
    scope* s = &c->scopes[c->n_scopes++];
    memset(s, 0, sizeof *s);
    s->up = c->sc;
    c->sc = s;
}
static void scope_pop(tc* c) { c->sc = c->sc->up; }
static int scope_find(tc* c, const char* name, ty* out) {
    for (scope* s = c->sc; s; s = s->up)
        for (size_t i = 0; i < s->n; i++)
            if (!strcmp(s->vars[i].name, name)) {
                *out = s->vars[i].t;
                return 1;
            }
    return 0;
}
static void scope_def(tc* c, const char* name, ty t) {
    if (!c->sc || c->sc->n >= MAX_VARS) return;
    c->sc->vars[c->sc->n].name = ctron_arena_strndup(c->a, name, strlen(name));
    c->sc->vars[c->sc->n].t = t;
    c->sc->n++;
}
static int is_reserved(const char* n) { return !strncmp(n, "ctron_", 6); }

// ================= 表达式(单次求值发射) =================
static ty emit_expr(tc* c, cexpr* e, sb* o);
static void emit_block(tc* c, cblock* b, sb* o);

static ty emit_expr(tc* c, cexpr* e, sb* o) {
    if (c->err) return ty_unk();
    if (!e) return ty_unk();
    switch (e->kind) {
    case EX_INT: {
        ty t = suff_ty(e->suffix);
        if (t.k == T_FLT) {
            sb_f(o, "(double)(%s)", e->text && *e->text ? e->text : "0");
            return t;
        }
        sb_f(o, "(int64_t)(%s)", e->text && *e->text ? e->text : "0");
        return t;
    }
    case EX_FLOAT:
        sb_f(o, "(double)(%s)", e->text && *e->text ? e->text : "0");
        return ty_flt();
    case EX_BOOL:
        sb_f(o, "%d", e->bval ? 1 : 0);
        return ty_bool();
    case EX_IDENT: {
        ty t;
        if (scope_find(c, e->text, &t)) {
            sb_s(o, e->text);
            return t;
        }
        terr(c, "v1 未解析名称:%s", e->text);
        return ty_unk();
    }
    case EX_UNARY: {
        sb t2 = {0};
        ty x = emit_expr(c, e->ux, &t2);
        if (c->err) { sb_free(&t2); return x; }
        if (e->uop == UN_NOT) {
            if (x.k != T_BOOL && x.k != T_UNK) terr(c, "v1:一元 ! 需 Bool");
            sb_f(o, "(!(%s))", t2.d ? t2.d : "0");
            sb_free(&t2);
            return ty_bool();
        }
        if (x.k == T_FLT) {
            sb_f(o, "(-(%s))", t2.d ? t2.d : "0.0");
            sb_free(&t2);
            return ty_flt();
        }
        if (x.k != T_INT) { terr(c, "v1:一元 - 需数值"); sb_free(&t2); return x; }
        char h[64];
        snprintf(h, sizeof h, "ctron_neg_%s", wlname(x));
        use_helper(c, h);
        sb_f(o, "%s(%s)", h, t2.d ? t2.d : "0");
        sb_free(&t2);
        return x;
    }
    case EX_BINARY: {
        if (e->bop == B_AND) {
            sb l = {0}, r = {0};
            ty lt = emit_expr(c, e->lhs, &l);
            ty rt = emit_expr(c, e->rhs, &r);
            if (c->err) { sb_free(&l); sb_free(&r); return ty_unk(); }
            if ((lt.k != T_BOOL && lt.k != T_UNK) || (rt.k != T_BOOL && rt.k != T_UNK))
                terr(c, "v1:&& 需 Bool");
            sb_f(o, "((%s) && (%s))", l.d ? l.d : "0", r.d ? r.d : "0");
            sb_free(&l);
            sb_free(&r);
            return ty_bool();
        }
        if (e->bop == B_OR) { terr(c, "v1:|| 不存在(用 or2 德摩根)"); return ty_unk(); }
        sb l = {0}, r = {0};
        ty lt = emit_expr(c, e->lhs, &l);
        ty rt = emit_expr(c, e->rhs, &r);
        if (c->err) { sb_free(&l); sb_free(&r); return ty_unk(); }
        if (e->bop >= B_EQ && e->bop <= B_GE) {
            const char* op = e->bop == B_EQ ? "==" : e->bop == B_NE ? "!="
                            : e->bop == B_LT ? "<" : e->bop == B_GT ? ">" : e->bop == B_LE ? "<=" : ">=";
            if (lt.k == T_INT && rt.k == T_INT)
                sb_f(o, "((__int128)(%s) %s (__int128)(%s))", l.d ? l.d : "0", op, r.d ? r.d : "0");
            else
                sb_f(o, "((%s) %s (%s))", l.d ? l.d : "0", op, r.d ? r.d : "0");
            sb_free(&l);
            sb_free(&r);
            return ty_bool();
        }
        if (lt.k == T_FLT || rt.k == T_FLT) {
            if (e->bop == B_WADD || e->bop == B_WSUB) terr(c, "v1:回绕算符需整型");
            const char* op = e->bop == B_ADD ? "+" : e->bop == B_SUB ? "-" : e->bop == B_MUL ? "*"
                            : e->bop == B_DIV ? "/" : "%";
            sb_f(o, "((%s) %s (%s))", l.d ? l.d : "0.0", op, r.d ? r.d : "0.0");
            sb_free(&l);
            sb_free(&r);
            return ty_flt();
        }
        if (lt.k != T_INT || rt.k != T_INT) { terr(c, "v1:算术需数值"); sb_free(&l); sb_free(&r); return ty_unk(); }
        if (e->bop == B_WADD || e->bop == B_WSUB) {
            char h[64];
            snprintf(h, sizeof h, "ctron_w%s_%s", e->bop == B_WADD ? "add" : "sub", wlname(lt));
            use_helper(c, h);
            sb_f(o, "%s(%s, %s)", h, l.d ? l.d : "0", r.d ? r.d : "0");
            sb_free(&l);
            sb_free(&r);
            return lt;
        }
        char h[64];
        snprintf(h, sizeof h, "ctron_%s_%s",
                 e->bop == B_ADD ? "add" : e->bop == B_SUB ? "sub" : e->bop == B_MUL ? "mul"
                 : e->bop == B_DIV ? "div" : "mod", wlname(lt));
        use_helper(c, h);
        sb_f(o, "%s(%s, %s)", h, l.d ? l.d : "0", r.d ? r.d : "0");
        sb_free(&l);
        sb_free(&r);
        return lt;
    }
    case EX_CALL: {
        cexpr* cal = e->callee;
        const char* nm = (cal && cal->kind == EX_IDENT) ? cal->text : NULL;
        if (!nm) { terr(c, "v1 仅支持具名函数调用"); return ty_unk(); }
        sb args = {0};
        for (size_t i = 0; i < e->nelems; i++) {
            if (i) sb_s(&args, ", ");
            sb a1 = {0};
            emit_expr(c, e->elems[i], &a1);
            sb_s(&args, a1.d ? a1.d : "0");
            sb_free(&a1);
        }
        if (c->err) { sb_free(&args); return ty_unk(); }
        if (!strcmp(nm, "panic")) {
            if (e->nelems != 1 || e->elems[0]->kind != EX_STR)
                terr(c, "v1:panic 需单个字面量实参");
            else {
                sb_f(o, "ctron_panic(\"");
                const ctron_str_part* sp = e->elems[0]->sparts;
                for (size_t i = 0; sp && i < e->elems[0]->nsparts; i++)
                    if (sp[i].kind == PART_TEXT && sp[i].s)
                        for (const char* p = sp[i].s; *p; p++) {
                            if (*p == '"' || *p == '\\') sb_c(o, '\\');
                            sb_c(o, *p);
                        }
                sb_s(o, "\")");
            }
            sb_free(&args);
            return ty_unk();
        }
        if (!strcmp(nm, "assert")) {
            if (e->nelems != 1) terr(c, "v1:assert 参数");
            else sb_f(o, "ctron_assert(%s)", args.d ? args.d : "0");
            sb_free(&args);
            return ty_bool();
        }
        if (!strcmp(nm, "assert_eq") || !strcmp(nm, "assert_ne")) {
            if (e->nelems != 2) { terr(c, "v1:assert_eq/ne 参数"); sb_free(&args); return ty_unk(); }
            sb a1 = {0}, a2 = {0};
            ty t1 = emit_expr(c, e->elems[0], &a1);
            emit_expr(c, e->elems[1], &a2);
            char h[64];
            snprintf(h, sizeof h, "ctron_%s_%s", nm, wlname(t1));
            use_helper(c, h);
            sb_f(o, "%s(%s, %s)", h, a1.d ? a1.d : "0", a2.d ? a2.d : "0");
            sb_free(&a1);
            sb_free(&a2);
            sb_free(&args);
            return ty_bool();
        }
        if (!strcmp(nm, "print") || !strcmp(nm, "println")) {
            if (e->nelems != 1) { terr(c, "v1:%s 参数", nm); sb_free(&args); return ty_unk(); }
            sb a1 = {0};
            ty t1 = emit_expr(c, e->elems[0], &a1);
            char h[64];
            snprintf(h, sizeof h, "ctron_print_%s", wlname(t1));
            use_helper(c, h);
            if (!strcmp(nm, "println"))
                sb_f(o, "(%s(%s), ctron_print_nl())", h, a1.d ? a1.d : "0");
            else
                sb_f(o, "%s(%s)", h, a1.d ? a1.d : "0");
            sb_free(&a1);
            sb_free(&args);
            return ty_unk();
        }
        int nparams, is_void;
        ty ret;
        if (!fn_lookup(c, nm, &nparams, &is_void, &ret)) {
            terr(c, "v1 未解析函数:%s", nm);
            sb_free(&args);
            return ty_unk();
        }
        if ((int)e->nelems != nparams) terr(c, "v1:参数个数 %s", nm);
        char cn[256];
        snprintf(cn, sizeof cn, "ctron_user_%s", nm);
        sb_f(o, "%s(%s)", cn, args.d ? args.d : "");
        sb_free(&args);
        return is_void ? ty_unk() : ret;
    }
    default:
        terr(c, "v1 不支持该表达式构造(kind %d)", (int)e->kind);
        return ty_unk();
    }
}

// ================= 语句 =================
// if 语句(if/else-if 链/else;作为表达式值 = v1 不支持)
static void emit_if_stmt(tc* c, cexpr* e, sb* o) {
    if (c->err || !e) return;
    sb cond = {0};
    ty ct = emit_expr(c, e->cond, &cond);
    if (c->err) { sb_free(&cond); return; }
    if (ct.k != T_BOOL && ct.k != T_UNK) terr(c, "v1:if 条件需 Bool");
    sb_f(o, "if (%s) {", cond.d ? cond.d : "0");
    sb_free(&cond);
    scope_push(c);
    emit_block(c, e->then_b, o);
    scope_pop(c);
    if (e->els && e->els->kind == EX_IF) {
        sb_s(o, " else ");
        emit_if_stmt(c, e->els, o);
        return;
    }
    if (e->els && e->els->kind == EX_BLOCK) {
        sb_s(o, " else {");
        scope_push(c);
        emit_block(c, e->els->block, o);
        scope_pop(c);
        sb_s(o, "}\n");
        return;
    }
    if (e->els) { terr(c, "v1:else 分支构造不支持"); return; }
    sb_s(o, "}\n");
}

static void emit_stmt(tc* c, cstmt* st, sb* o) {
    if (c->err || !st) return;
    switch (st->kind) {
    case ST_LET: {
        if (!st->pat || st->pat->kind != PAT_IDENT || !st->pat->name) {
            terr(c, "v1:let 仅支持标识符模式");
            return;
        }
        const char* name = st->pat->name;
        if (is_reserved(name)) { terr(c, "v1:标识符保留前缀 ctron_:%s", name); return; }
        if (!st->e) { terr(c, "v1:let 缺初值"); return; }
        ty ann = decl_ty(st->ty);
        sb rhs = {0};
        ty it = emit_expr(c, st->e, &rhs);
        if (c->err) { sb_free(&rhs); return; }
        ty t = (ann.k != T_UNK) ? ann : it;
        if (t.k == T_UNK) { terr(c, "v1:无法推导 %s 的类型(补类型注解)", name); sb_free(&rhs); return; }
        if (ann.k == T_INT) {
            char h[64];
            snprintf(h, sizeof h, "ctron_decl_%s", wlname(ann));
            use_helper(c, h);
            sb_f(o, "%s %s = %s(%s);\n", ctype_of(ann), name, h, rhs.d ? rhs.d : "0");
        } else {
            sb_f(o, "%s %s = %s;\n", ctype_of(t), name, rhs.d ? rhs.d : "0");
        }
        sb_free(&rhs);
        scope_def(c, name, t);
        return;
    }
    case ST_ASSIGN: {
        if (!st->target || st->target->kind != EX_IDENT) { terr(c, "v1:赋值目标仅支持标识符"); return; }
        const char* name = st->target->text;
        ty t;
        if (!scope_find(c, name, &t)) { terr(c, "v1:未解析赋值目标 %s", name); return; }
        sb rhs = {0};
        emit_expr(c, st->value, &rhs);
        if (c->err) { sb_free(&rhs); return; }
        if (st->aop == A_EQ) {
            if (t.k == T_INT) {
                char h[64];
                snprintf(h, sizeof h, "ctron_decl_%s", wlname(t));
                use_helper(c, h);
                sb_f(o, "%s = %s(%s);\n", name, h, rhs.d ? rhs.d : "0");
            } else {
                sb_f(o, "%s = %s;\n", name, rhs.d ? rhs.d : "0");
            }
            sb_free(&rhs);
            return;
        }
        if (t.k != T_INT) { terr(c, "v1:复合赋值需整型"); sb_free(&rhs); return; }
        const char* fam = st->aop == A_ADDEQ ? "cadd" : st->aop == A_SUBEQ ? "csub"
                        : st->aop == A_MULEQ ? "cmul" : st->aop == A_DIVEQ ? "cdiv" : "cmod";
        char h[64];
        snprintf(h, sizeof h, "ctron_%s_%s", fam, wlname(t));
        use_helper(c, h);
        sb_f(o, "%s = %s(%s, %s);\n", name, h, name, rhs.d ? rhs.d : "0");
        sb_free(&rhs);
        return;
    }
    case ST_RET:
        if (c->in_test) { terr(c, "v1:test 块内不支持 return"); return; }
        if (st->e) {
            sb r = {0};
            emit_expr(c, st->e, &r);
            sb_f(o, "return %s;\n", r.d ? r.d : "0");
            sb_free(&r);
        } else sb_s(o, "return 0;\n");
        return;
    case ST_EXPR:
        if (st->e && st->e->kind == EX_IF) {
            emit_if_stmt(c, st->e, o);
            return;
        }
        if (st->e && st->e->kind == EX_MATCH) { terr(c, "v1 不支持 match"); return; }
        {
            sb r = {0};
            emit_expr(c, st->e, &r);
            sb_f(o, "%s;\n", r.d ? r.d : "");
            sb_free(&r);
        }
        return;
    case ST_WHILE: {
        sb cond = {0};
        emit_expr(c, st->e, &cond);
        sb_f(o, "while (%s) {", cond.d ? cond.d : "0");
        sb_free(&cond);
        scope_push(c);
        emit_block(c, st->body, o);
        scope_pop(c);
        sb_s(o, "}\n");
        return;
    }
    case ST_FOR: {
        if (!st->iter || st->iter->kind != EX_RANGE) { terr(c, "v1:for 仅支持 range"); return; }
        if (!st->pat || st->pat->kind != PAT_IDENT || !st->pat->name) { terr(c, "v1:for 模式仅标识符"); return; }
        const char* var = st->pat->name;
        if (is_reserved(var)) { terr(c, "v1:标识符保留前缀 ctron_:%s", var); return; }
        char lo[32], hi[32];
        snprintf(lo, sizeof lo, "ctron_rng_%d", c->tmpn);
        snprintf(hi, sizeof hi, "ctron_rng_%d", c->tmpn + 1);
        c->tmpn += 2;
        sb a = {0}, b = {0};
        emit_expr(c, st->iter->from, &a);
        emit_expr(c, st->iter->to, &b);
        sb_f(o, "{ int64_t %s = %s; int64_t %s = %s; for (int64_t %s = %s; %s %s %s; %s++) {",
             lo, a.d ? a.d : "0", hi, b.d ? b.d : "0",
             var, lo, var, st->iter->inclusive ? "<=" : "<", hi, var);
        sb_free(&a);
        sb_free(&b);
        scope_push(c);
        scope_def(c, var, ty_int(64, 0));
        emit_block(c, st->body, o);
        scope_pop(c);
        sb_s(o, "}}\n");
        return;
    }
    }
}

static void emit_block(tc* c, cblock* b, sb* o) {
    if (!b || c->err) return;
    for (size_t i = 0; i < b->nstmts; i++)
        emit_stmt(c, b->stmts[i], o);
    if (b->tail) {
        sb r = {0};
        emit_expr(c, b->tail, &r);
        sb_f(o, "%s;\n", r.d ? r.d : "");
        sb_free(&r);
    }
}

// ================= 助手生成 =================
static void wbounds(char* lo, char* hi, size_t n, int bits, int us) {
    if (us) {
        snprintf(lo, n, "0");
        snprintf(hi, n, "%lluULL", bits >= 64 ? 18446744073709551615ULL : (1ULL << bits) - 1);
    } else if (bits >= 64) {
        snprintf(lo, n, "(-9223372036854775807LL - 1)");
        snprintf(hi, n, "9223372036854775807LL");
    } else {
        snprintf(lo, n, "%lldLL", -(1LL << (bits - 1)));
        snprintf(hi, n, "%lldLL", (1LL << (bits - 1)) - 1);
    }
}
static void emit_helper(tc* c, const char* name) {
    // name 形如 ctron_<fam>_<wl>;fam ∈ add/sub/mul/div/mod/neg/wadd/wsub/decl/print/assert_eq/assert_ne
    char fam[32], wl[16];
    if (!strncmp(name, "ctron_assert_eq_", 16)) {
        snprintf(fam, sizeof fam, "assert_eq");
        snprintf(wl, sizeof wl, "%s", name + 16);
    } else if (!strncmp(name, "ctron_assert_ne_", 16)) {
        snprintf(fam, sizeof fam, "assert_ne");
        snprintf(wl, sizeof wl, "%s", name + 16);
    } else if (sscanf(name, "ctron_%31[^_]_%15s", fam, wl) != 2) {
        return;
    }
    int bits = 32, us = 0, isf = 0, isb = 0;
    if (!strcmp(wl, "f64")) isf = 1;
    else if (!strcmp(wl, "b")) isb = 1;
    else { us = wl[0] == 'u'; bits = atoi(wl + 1); }
    char ct[32], lo[48], hi[48];
    if (!isf && !isb) {
        ty t = ty_int(bits, us);
        snprintf(ct, sizeof ct, "%s", ctype_of(t));
        wbounds(lo, hi, sizeof lo, bits, us);
    }
    sb* o = &c->head;
    if (!strcmp(fam, "add") || !strcmp(fam, "sub") || !strcmp(fam, "mul")
        || !strcmp(fam, "div") || !strcmp(fam, "mod") || !strcmp(fam, "cadd")
        || !strcmp(fam, "csub") || !strcmp(fam, "cmul") || !strcmp(fam, "cdiv")
        || !strcmp(fam, "cmod")) {
        int is_c = fam[0] == 'c';
        const char* base = is_c ? fam + 1 : fam;
        const char* op = !strcmp(base, "add") ? "+" : !strcmp(base, "sub") ? "-" : !strcmp(base, "mul") ? "*"
                        : !strcmp(base, "div") ? "/" : "%";
        const char* zmsg = !strcmp(base, "div") ? "division by zero (/)" : "division by zero (%)";
        char omsg[48];
        if (is_c) snprintf(omsg, sizeof omsg, "integer overflow (assign)");
        else snprintf(omsg, sizeof omsg, "integer overflow (%s)", op);
        sb_f(o, "static %s %s(int64_t a, int64_t b) {\n", ct, name);
        if (!strcmp(base, "div") || !strcmp(base, "mod"))
            sb_f(o, "    if (b == 0) ctron_panic(\"%s\");\n", zmsg);
        sb_f(o, "    __int128 r = (__int128)a %s (__int128)b;\n", op);
        sb_f(o, "    if (r < (%s)(%s) || r > (%s)(%s)) ctron_panic(\"%s\");\n",
             ct, lo, ct, hi, omsg);
        sb_f(o, "    return (%s)r;\n}\n", ct);
        return;
    }
    if (!strcmp(fam, "neg")) {
        if (us) {
            sb_f(o, "static %s %s(int64_t a) { (void)a; ctron_panic(\"integer overflow (neg)\"); return (%s)0; }\n",
                 ct, name, ct);
        } else {
            sb_f(o, "static %s %s(int64_t a) {\n    __int128 r = -(__int128)a;\n", ct, name);
            sb_f(o, "    if (r < (%s)(%s) || r > (%s)(%s)) ctron_panic(\"integer overflow (neg)\");\n", ct, lo, ct, hi);
            sb_f(o, "    return (%s)r;\n}\n", ct);
        }
        return;
    }
    if (!strcmp(fam, "wadd") || !strcmp(fam, "wsub")) {
        const char* op = !strcmp(fam, "wadd") ? "+" : "-";
        sb_f(o, "static %s %s(int64_t a, int64_t b) {\n", ct, name);
        if (bits >= 64)
            sb_f(o, "    return (%s)(int64_t)((uint64_t)a %s (uint64_t)b);\n", ct, op);
        else {
            unsigned long long m = (1ULL << bits) - 1;
            sb_f(o, "    uint64_t r = ((uint64_t)a %s (uint64_t)b) & %lluULL;\n", op, m);
            if (us) sb_f(o, "    return (%s)r;\n", ct);
            else sb_f(o, "    return (%s)((int64_t)(r ^ %lluULL) - (int64_t)%lluULL);\n",
                      ct, (1ULL << (bits - 1)), (1ULL << (bits - 1)));
        }
        sb_f(o, "}\n");
        return;
    }
    if (!strcmp(fam, "decl")) {
        sb_f(o, "static %s %s(int64_t v) {\n", ct, name);
        if (us)
            sb_f(o, "    if (v < 0 || (__int128)v >= (__int128)(%s)) ctron_panic(\"integer overflow (decl)\");\n", hi);
        else
            sb_f(o, "    if ((__int128)v < (__int128)(%s) || (__int128)v > (__int128)(%s)) ctron_panic(\"integer overflow (decl)\");\n",
                 lo, hi);
        sb_f(o, "    return (%s)v;\n}\n", ct);
        return;
    }
    if (!strcmp(fam, "print")) {
        if (isf)
            sb_f(o, "static void %s(double v) { char b[64]; if (v == (double)(long long)v) snprintf(b, sizeof b, \"%%.1f\", v); else snprintf(b, sizeof b, \"%%g\", v); printf(\"%%s\", b); }\n", name);
        else if (isb)
            sb_f(o, "static void %s(int v) { printf(\"%%s\", v ? \"true\" : \"false\"); }\n", name);
        else if (us)
            sb_f(o, "static void %s(uint64_t v) { printf(\"%%llu\", (unsigned long long)v); }\n", name);
        else
            sb_f(o, "static void %s(int64_t v) { printf(\"%%lld\", (long long)v); }\n", name);
        return;
    }
    if (!strcmp(fam, "assert_eq") || !strcmp(fam, "assert_ne")) {
        int ne = !strcmp(fam, "assert_ne");
        const char* label = ne ? "assert_ne failed" : "assert_eq failed";
        const char* cmp = ne ? "!=" : "==";
        if (isf) {
            sb_f(o, "static void %s(double a, double b) {\n", name);
            sb_f(o, "    char x[64], y[64];\n");
            sb_f(o, "    if (a == (double)(long long)a) snprintf(x, sizeof x, \"%%.1f\", a); else snprintf(x, sizeof x, \"%%g\", a);\n");
            sb_f(o, "    if (b == (double)(long long)b) snprintf(y, sizeof y, \"%%.1f\", b); else snprintf(y, sizeof y, \"%%g\", b);\n");
            sb_f(o, "    if (!(a %s b)) { char m[160]; snprintf(m, sizeof m, \"%s: %%s != %%s\", x, y); ctron_panic(m); }\n}\n",
                 cmp, label);
        } else if (isb) {
            sb_f(o, "static void %s(int a, int b) { if (!(a %s b)) ctron_panic(\"%s\"); }\n", name, cmp, label);
        } else {
            const char* pt = us ? "uint64_t" : "int64_t";
            sb_f(o, "static void %s(%s a, %s b) { if (!(a %s b)) { char m[128]; snprintf(m, sizeof m, \"%s: %%llu != %%llu\", (unsigned long long)a, (unsigned long long)b); ctron_panic(m); } }\n",
                 name, pt, pt, cmp, label);
        }
        return;
    }
}

// ================= 文件级 =================
static void collect_fns(tc* c, const cfile* f) {
    for (size_t i = 0; i < f->ndecls; i++) {
        const cdecl* d = &f->decls[i];
        if (d->kind != D_FN) continue;
        if (d->fn_.abi) { terr(c, "v1 不支持 extern:%s", d->fn_.name); continue; }
        ty ret = decl_ty(d->fn_.ret);
        add_fn(c, d->fn_.name, ret, (int)d->fn_.nparams, ret.k == T_UNK);
    }
}

static void emit_fn(tc* c, const cfn* F, const char* cname) {
    ty ret = decl_ty(F->ret);
    const char* rct = (ret.k == T_FLT) ? "double" : (ret.k == T_BOOL) ? "int" : (ret.k == T_INT) ? "int64_t" : "void";
    sb_f(&c->body, "static %s %s(", rct, cname);
    scope_push(c);
    for (size_t i = 0; i < F->nparams; i++) {
        const cparam* p = &F->params[i];
        if (p->is_receiver) { terr(c, "v1 不支持 receiver"); return; }
        ty pt = decl_ty(p->ty);
        if (pt.k == T_UNK) { terr(c, "v1:参数 %s 需类型注解", p->name ? p->name : "?"); return; }
        if (!p->name) { terr(c, "v1:参数缺名"); return; }
        if (i) sb_s(&c->body, ", ");
        sb_f(&c->body, "%s ctron_p_%s", pt.k == T_FLT ? "double" : pt.k == T_BOOL ? "int" : "int64_t", p->name);
    }
    sb_s(&c->body, ") {\n");
    for (size_t i = 0; i < F->nparams; i++) {
        const cparam* p = &F->params[i];
        ty pt = decl_ty(p->ty);
        if (!p->name) continue;
        if (pt.k == T_INT) {
            char h[64];
            snprintf(h, sizeof h, "ctron_decl_%s", wlname(pt));
            use_helper(c, h);
            sb_f(&c->body, "    %s %s = %s(ctron_p_%s);\n", ctype_of(pt), p->name, h, p->name);
            scope_def(c, p->name, pt);
        } else {
            scope_def(c, p->name, pt);
        }
    }
    if (F->body) emit_block(c, F->body, &c->body);
    scope_pop(c);
    sb_s(&c->body, "}\n");
}

void ctron_trans_result_free(ctron_trans_result* r) {
    if (!r) return;
    free(r->code);
    free(r->err);
    r->code = NULL;
    r->err = NULL;
}

ctron_trans_result ctron_trans_file(const cfile* f) {
    ctron_trans_result res = {0};
    ctron_arena* a = ctron_arena_new();
    tc c = {0};
    c.a = a;

    size_t ntests = 0;
    for (size_t i = 0; i < f->ndecls; i++)
        if (f->decls[i].kind == D_TEST) ntests++;

    collect_fns(&c, f);
    if (!c.err && c.nfns == 0 && ntests == 0) terr(&c, "v1:文件没有可转译的函数");

    for (size_t i = 0; i < f->ndecls && !c.err; i++) {
        const cdecl* d = &f->decls[i];
        switch (d->kind) {
        case D_FN: {
            const char* nm = d->fn_.name;
            if (is_reserved(nm)) { terr(&c, "v1:标识符保留前缀 ctron_:%s", nm); break; }
            char cn[256];
            snprintf(cn, sizeof cn, "ctron_user_%s", nm);
            emit_fn(&c, &d->fn_, cn);
            break;
        }
        case D_TEST: {
            char cn[64];
            snprintf(cn, sizeof cn, "ctron_test_%zu", i);
            sb_f(&c.body, "static void %s(void) {\n", cn);
            c.sc = NULL;
            scope_push(&c);
            c.in_test = 1;
            emit_block(&c, d->test.body, &c.body);
            c.in_test = 0;
            scope_pop(&c);
            sb_s(&c.body, "}\n");
            break;
        }
        case D_USE: break;
        default:
            terr(&c, "v1 不支持该声明(kind %d)", (int)d->kind);
            break;
        }
    }

    if (!c.err) {
        int has_fn_main = 0;
        for (size_t i = 0; i < f->ndecls; i++)
            if (f->decls[i].kind == D_FN && !strcmp(f->decls[i].fn_.name, "main")) has_fn_main = 1;
        if (ntests) {
            sb_f(&c.body, "int main(void) {\n    if (setjmp(ctron_panic_frame)) return 1;\n");
            for (size_t i = 0; i < f->ndecls; i++)
                if (f->decls[i].kind == D_TEST) sb_f(&c.body, "    ctron_test_%zu();\n", i);
            sb_s(&c.body, "    return 0;\n}\n");
        } else if (has_fn_main) {
            sb_s(&c.body, "int main(void) {\n    if (setjmp(ctron_panic_frame)) return 1;\n    return (int)ctron_user_main();\n}\n");
        } else {
            terr(&c, "v1:文件既无 fn main 也无 test 块");
        }
    }

    if (!c.err) {
        sb* h = &c.head;
        sb_s(h, "/* 由 ctronc trans(C10-a)生成;语义对齐 rt.c 解释器 */\n"
                "#include <stdio.h>\n#include <stdint.h>\n#include <stdlib.h>\n"
                "#include <string.h>\n#include <setjmp.h>\n\n"
                "static jmp_buf ctron_panic_frame;\n"
                "static _Noreturn void ctron_panic(const char* msg) {\n"
                "    fprintf(stderr, \"%s\\n\", msg);\n"
                "    longjmp(ctron_panic_frame, 1);\n"
                "}\n"
                "static void ctron_print_nl(void) { printf(\"\\n\"); }\n"
                "static void ctron_assert(int v) { if (!v) ctron_panic(\"assert failed\"); }\n");
        for (size_t i = 0; i < c.n_helpers; i++)
            emit_helper(&c, c.helpers[i]);
        for (size_t i = 0; i < f->ndecls; i++) {
            const cdecl* d = &f->decls[i];
            if (d->kind != D_FN) continue;
            ty ret = decl_ty(d->fn_.ret);
            const char* rct = (ret.k == T_FLT) ? "double" : (ret.k == T_BOOL) ? "int" : (ret.k == T_INT) ? "int64_t" : "void";
            sb_f(h, "static %s ctron_user_%s(", rct, d->fn_.name);
            for (size_t j = 0; j < d->fn_.nparams; j++) {
                ty pt = decl_ty(d->fn_.params[j].ty);
                if (j) sb_s(h, ", ");
                sb_f(h, "%s ctron_p_%s", pt.k == T_FLT ? "double" : pt.k == T_BOOL ? "int" : "int64_t",
                     d->fn_.params[j].name ? d->fn_.params[j].name : "_");
            }
            sb_s(h, ");\n");
        }
        for (size_t i = 0; i < f->ndecls; i++)
            if (f->decls[i].kind == D_TEST) sb_f(h, "static void ctron_test_%zu(void);\n", i);
        sb_s(h, "\n");
    }

    if (c.err) {
        res.err = strdup(c.err);
        sb_free(&c.head);
        sb_free(&c.body);
        ctron_arena_free(a);
        return res;
    }

    sb all = {0};
    sb_s(&all, c.head.d ? c.head.d : "");
    sb_s(&all, c.body.d ? c.body.d : "");
    res.code = all.d ? all.d : strdup("");
    sb_free(&c.head);
    sb_free(&c.body);
    ctron_arena_free(a);
    return res;
}
