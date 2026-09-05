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
typedef enum { V_INT, V_FLOAT, V_BOOL, V_STR, V_VOID, V_RANGE } vkind;

typedef struct {
    vkind k;
    __int128 i; // 整数(raw 已归一到位宽;64 位无符号按非负)
    double f;
    int bits;
    int us;
    char* s;
    int64_t lo, hi;
    int inclusive;
} val;

static val v_int(__int128 x, int bits, int us) { val v = {0}; v.k = V_INT; v.i = x; v.bits = bits; v.us = us; return v; }
static val v_flt(double f) { val v = {0}; v.k = V_FLOAT; v.f = f; return v; }
static val v_bool(int b) { val v = {0}; v.k = V_BOOL; v.i = b; return v; }
static val v_void(void) { val v = {0}; v.k = V_VOID; return v; }
static val v_rng(int64_t lo, int64_t hi, int incl) { val v = {0}; v.k = V_RANGE; v.lo = lo; v.hi = hi; v.inclusive = incl; return v; }

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
} rt;

static void rt_abort(rt* R, rt_status st, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(R->msg, sizeof R->msg, fmt, ap);
    va_end(ap);
    R->st = st;
    longjmp(R->jb, 1);
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
    b->slot = v;
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
static void eval_let(rt* R, cstmt* st) {
    if (!st->pat || st->pat->kind != PAT_IDENT || !st->pat->name) {
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
        if (st->target && st->target->kind == EX_IDENT) {
            bind* b = env_find(R, st->target->text);
            if (!b) rt_abort(R, RT_ERROR, "未知绑定 %s", st->target->text);
            val l = b->slot;
            val r = eval_expr(R, st->value);
            if (st->aop == A_EQ) {
                if (l.k == V_INT && r.k == V_INT) r = ck_int(R, r.i, l.bits, l.us, "=");
                else if (l.k == V_INT && r.k == V_FLOAT) r = ck_int(R, (__int128)r.f, l.bits, l.us, "=");
                b->slot = r;
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
        if (!st->pat || st->pat->kind != PAT_IDENT || !st->pat->name || !st->iter)
            rt_abort(R, RT_ERROR, "for 模式不支持");
        val rg = eval_expr(R, st->iter);
        if (rg.k != V_RANGE) rt_abort(R, RT_ERROR, "for 需要 range");
        env_push(R);
        env_let(R, st->pat->name, v_int(rg.lo, 32, 0));
        bind* iv = env_find(R, st->pat->name);
        for (int64_t cur = rg.lo; (rg.inclusive ? cur <= rg.hi : cur < rg.hi); cur++) {
            iv->slot = v_int(cur, 32, 0);
            (void)eval_block(R, st->body);
            if (R->has_ret) break;
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
static val eval_block(rt* R, cblock* b) {
    val tail = v_void();
    if (!b) return tail;
    env_push(R);
    for (size_t i = 0; i < b->nstmts && !R->has_ret; i++)
        eval_stmt(R, b->stmts[i]);
    if (!R->has_ret && b->tail) tail = eval_expr(R, b->tail);
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
static const cdecl* file_fn(const rt* R, const char* name) {
    for (size_t i = 0; i < R->f->ndecls; i++) {
        const cdecl* d = &R->f->decls[i];
        if (d->kind == D_FN && strcmp(d->fn_.name, name) == 0) return d;
    }
    return NULL;
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
        if (!b) rt_abort(R, RT_ERROR, "未解析名称: %s", e->text);
        return b->slot;
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
    case EX_BLOCK: return eval_block(R, e->block);
    case EX_CALL: {
        cexpr* cal = e->callee;
        // .as[T]()
        if (cal && cal->kind == EX_TYPEARGS && cal->obj && cal->obj->kind == EX_MEMBER
            && cal->obj->m_is_name && cal->obj->mname && strcmp(cal->obj->mname, "as") == 0) {
            val v = eval_expr(R, cal->obj->obj);
            const char* tn = cal->ntargs > 0 ? head_nm(cal->targs[0]) : NULL;
            return as_conv(R, v, tn);
        }
        if (cal && cal->kind == EX_IDENT) {
            const char* nm = cal->text;
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
            const cdecl* fn = file_fn(R, nm);
            if (!fn) rt_abort(R, RT_ERROR, "未知函数: %s", nm);
            return call_decl(R, fn, e->elems, e->nelems);
        }
        // UFCS / 成员方法(把接收者作为首参)
        if (cal && cal->kind == EX_MEMBER && cal->m_is_name && cal->mname) {
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

void ctron_rt_run_free(rt_run* r) {
    if (!r) return;
    free((void*)r->msg);
    r->msg = NULL;
}
