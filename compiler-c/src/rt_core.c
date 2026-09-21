#include "rt_internal.h"
#include <stdlib.h>
#include "arena.h"

// rt_core.c —— 值构造/缓冲/数值辅助/环境 + 字符串插值 + 函数调用与断言域(C4)
val v_int(__int128 x, int bits, int us) { val v = {0}; v.k = V_INT; v.i = x; v.bits = bits; v.us = us; return v; }
val v_flt(double f) { val v = {0}; v.k = V_FLOAT; v.f = f; return v; }
val v_bool(int b) { val v = {0}; v.k = V_BOOL; v.i = b; return v; }
val v_void(void) { val v = {0}; v.k = V_VOID; return v; }
val v_rng(int64_t lo, int64_t hi, int incl) { val v = {0}; v.k = V_RANGE; v.lo = lo; v.hi = hi; v.inclusive = incl; return v; }
val v_arr(val* items, size_t n) { val v = {0}; v.k = V_ARR; v.items = items; v.nitems = n; return v; }
val v_tag(const char* tag, val* items, size_t n) { val v = {0}; v.k = V_TAG; v.tag = tag; v.items = items; v.nitems = n; return v; }
val v_fn(const cdecl* d) { val v = {0}; v.k = V_FN; v.fnr = d; return v; }
val v_obj(const char* type, int is_class, vfld* flds, size_t nf) {
    val v = {0}; v.k = V_STRUCT; v.type = type; v.is_class = is_class; v.flds = flds; v.nfld = nf; return v;
}
val v_closure(const cexpr* ce, struct env* cap) {
    for (struct env* q = cap; q; q = q->up) q->captured = 1; // 捕获链免于帧复用
    val v = {0};
    v.k = V_CLOSURE;
    v.clo = ce;
    v.cap = cap;
    return v;
}
val v_ns(const char* name) { val v = {0}; v.k = V_NS; v.tag = name; return v; }
unsigned long long rt_env_steps(void) {
    // D2:CTRON_MAX_STEPS 步上限(N = 上限;0/未设/非法 = 无限)。默认无限支撑自举负载。
    const char* s = getenv("CTRON_MAX_STEPS");
    if (!s || !*s) return 0;
    unsigned long long v = 0;
    for (const char* q = s; *q; q++) {
        if (*q < '0' || *q > '9') return 0;
        if (v > 1844674407370955161ULL) return 18446744073709551615ULL;
        v = v * 10 + (unsigned long long)(*q - '0');
    }
    return v;
}
void rt_abort(rt* R, rt_status st, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(R->msg, sizeof R->msg, fmt, ap);
    va_end(ap);
    R->st = st;
    if (st == RT_PANIC) rt_panic_unwind(R); // §6.4 panic 展开保证 Drop 执行(P1-A2 镜像)
    longjmp(R->jb, 1);
}
void rt_puts(rt* R, const char* s) {  // 缓冲输出;LSP 经 flush_out 主动落盘
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
val v_box(rt* R, val inner) {
    val v = {0};
    v.k = V_BOX;
    boxval* n = (boxval*)ctron_arena_alloc(R->a, sizeof(boxval));
    n->inner = inner;
    v.bx = n;
    return v;
}
val v_str_own(rt* R, const char* s) { val v = {0}; v.k = V_STR; v.s = ctron_arena_strndup(R->a, s, strlen(s)); return v; }
val v_err_t(rt* R, const char* msg, val cause, const char* trace) {
    val v = {0};
    v.k = V_ERR;
    errval* e = (errval*)ctron_arena_alloc(R->a, sizeof(errval));
    e->msg = ctron_arena_strndup(R->a, msg, strlen(msg));
    e->cause = cause;
    e->trace = trace ? ctron_arena_strndup(R->a, trace, strlen(trace)) : NULL;
    v.err = e;
    return v;
}

// ================= 字符串缓冲 / 格式化 =================
char* astr(rt* R, const char* s) { return ctron_arena_strndup(R->a, s, strlen(s)); }

void rsb_c(sb* b, char c) {
    if (b->n + 2 > b->cap) { b->cap = b->cap ? b->cap * 2 : 64; b->d = (char*)realloc(b->d, b->cap); if (!b->d) abort(); }
    b->d[b->n++] = c;
    b->d[b->n] = '\0';
}
void rsb_s(sb* b, const char* s) { if (s) while (*s) rsb_c(b, *s++); }

void fmt_int(char out[80], __int128 x, int us) {
    if (!us) { snprintf(out, 80, "%lld", (long long)x); return; }
    unsigned __int128 u = (unsigned __int128)x;
    char t[80]; int n = 0;
    if (u == 0) t[n++] = '0';
    while (u && n < 78) { t[n++] = (char)('0' + (int)(u % 10)); u /= 10; }
    for (int i = 0; i < n; i++) out[i] = t[n - 1 - i];
    out[n] = 0;
}
void fmt_val(rt* R, val v, sb* b) {
    (void)R;
    char buf[80];
    switch (v.k) {
    case V_INT: fmt_int(buf, v.i, v.us); rsb_s(b, buf); break;
    case V_FLOAT:
        if (v.f == (double)(long long)v.f) snprintf(buf, sizeof buf, "%.1f", v.f);
        else snprintf(buf, sizeof buf, "%g", v.f);
        rsb_s(b, buf);
        break;
    case V_BOOL: rsb_s(b, v.i ? "true" : "false"); break;
    case V_STR: rsb_s(b, v.s ? v.s : ""); break;
    case V_TAG: rsb_s(b, v.tag ? v.tag : ""); break;
    case V_VOID: break;
    default: rsb_s(b, "<value>"); break;
    }
}

// ================= 数值辅助 =================
int fits(__int128 x, int bits, int us) {
    if (bits >= 64) {
        if (us) return x >= 0 && x <= ((__int128)1 << 64) - 1; // P1-B:U64 上界(2^64 溢出可观察)
        return x >= -((__int128)1 << 63) && x <= ((__int128)1 << 63) - 1;
    }
    if (us) return x >= 0 && x < ((__int128)1 << bits);
    __int128 hi = ((__int128)1 << (bits - 1)) - 1;
    return x >= -hi - 1 && x <= hi;
}
val ck_int(rt* R, __int128 x, int bits, int us, const char* op) {
    if (!fits(x, bits, us)) rt_abort(R, RT_PANIC, "integer overflow (%s)", op);
    return v_int(x, bits, us);
}
val wrap_int(__int128 x, int bits, int us) {
    if (bits >= 64) return v_int(x, bits, us);
    unsigned __int128 m = (((unsigned __int128)1) << bits) - 1;
    __int128 r = (__int128)((unsigned __int128)x & m);
    if (!us) { __int128 h = ((__int128)1) << (bits - 1); if (r >= h) r -= ((__int128)1) << bits; }
    return v_int(r, bits, us);
}

__int128 parse_int(const char* t) {
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
double parse_flt(const char* t) {
    char buf[160];
    size_t n = 0;
    for (const char* p = t; *p && n < 150; p++)
        if (*p != '_') buf[n++] = *p;
    buf[n] = 0;
    return atof(buf);
}
void suff_type(const char* s, int* bits, int* us, int* isf) {
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
void lower_suf(const char* n, char out[8]) {
    size_t k = 0;
    out[k++] = (char)tolower((unsigned char)n[0]);
    for (const char* p = n + 1; *p && k < 7; p++) out[k++] = (char)tolower((unsigned char)*p);
    out[k] = 0;
}
int decl_num(const char* n, int* bits, int* us, int* isf) {
    if (!n) return 0;
    char c0 = n[0];
    if (c0 != 'I' && c0 != 'U' && c0 != 'F') return 0;
    char s[8];
    lower_suf(n, s);
    suff_type(s, bits, us, isf);
    return 1;
}
const char* head_nm(const cty* t) {
    if (!t || t->kind != TY_NAMED || t->npath == 0) return NULL;
    return t->path[0];
}
// ? 擦除目标:Result[T, E] 的 E 头;AnyError 本身 ⇒ 两段式转换(§5.3/§5.4)
const char* err_head_of(const cty* t) {
    if (!t || t->kind != TY_NAMED || t->npath == 0) return NULL;
    if (!strcmp(t->path[0], "AnyError")) return "AnyError";
    if (!strcmp(t->path[0], "Result") && t->nargs >= 2) return head_nm(t->args[1]);
    return NULL;
}

// ================= 环境 =================
void env_push(rt* R) {
    env* e = R->env_free;
    if (e) {
        R->env_free = e->up; // 复用链也走 up 域
    } else {
        e = (env*)ctron_arena_alloc(R->a, sizeof(env));
    }
    e->head = NULL;
    e->captured = 0;
    e->up = R->top;
    R->top = e;
}
void env_pop(rt* R) {
    if (!R->top) return;
    env* e = R->top;
    env* up = e->up;
    if (!e->captured) {
        // 未被闭包捕获:bind 节点与帧体一并入复用链(调用帧占解释形态需求大头,
        // 见 docs/linux-seed-memory-evidence.md 归因榜)
        bind* b = e->head;
        while (b) {
            bind* nx = b->next; // 先存后改:头插复用链会覆写 next,原序遍历依赖它
            b->next = R->bind_free;
            R->bind_free = b;
            b = nx;
        }
        e->head = NULL;
        e->up = R->env_free;
        R->env_free = e;
    }
    R->top = up;
}
bind* env_find(rt* R, const char* name) {
    for (env* e = R->top; e; e = e->up)
        for (bind* b = e->head; b; b = b->next)
            if (strcmp(b->name, name) == 0) return b;
    return NULL;
}
void env_let(rt* R, const char* name, val v) {
    // 同帧同名 → 原地覆写(循环体逐轮重绑 let/var 时零分配;可见性语义与头插一致:
    // env_find 本就返回最新绑定,覆写即最新)
    for (bind* ex = R->top->head; ex; ex = ex->next) {
        if (ex->name == name || strcmp(ex->name, name) == 0) {
            ex->slot = (v.k == V_STRUCT && !v.is_class) ? clone_val(R, v) : v;
            return;
        }
    }
    bind* b = R->bind_free;
    if (b) {
        R->bind_free = b->next;
    } else {
        b = (bind*)ctron_arena_alloc(R->a, sizeof(bind));
    }
    b->name = name;
    b->slot = (v.k == V_STRUCT && !v.is_class) ? clone_val(R, v) : v;
    b->next = R->top->head;
    R->top->head = b;
}

// ================= 字符串(文本+插值) =================
val interp_raw(rt* R, const char* raw) {
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

val str_expr(rt* R, cexpr* e) {
    // 纯文本字面量(单 TEXT 部件):直接别名解析期 NUL 常量,零分配。
    // 字面量求值(比较用 "Enum"/"i"/类型码等)是解释形态分配的最大来源;
    // parts[0].s 生命周期 = 解析 arena = 全程序,且字符串不可变,别名安全。
    if (e->nsparts == 1 && e->sparts[0].kind == PART_TEXT && e->sparts[0].s) {
        val out = {0};
        out.k = V_STR;
        out.s = e->sparts[0].s;
        return out;
    }
    sb b = {0};
    for (size_t i = 0; i < e->nsparts; i++) {
        const ctron_str_part* p = &e->sparts[i];
        if (p->kind == PART_TEXT) rsb_s(&b, p->s ? p->s : "");
        else {
            val v = interp_raw(R, p->s);
            sb tmp = {0};
            fmt_val(R, v, &tmp);
            rsb_s(&b, tmp.d ? tmp.d : "");
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
const cdecl* find_kind(const cfile* f, cdecl_kind kd, const char* name) {
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
const cdecl* file_fn(const rt* R, const char* name) {
    for (size_t i = 0; i < R->f->ndecls; i++) {
        const cdecl* d = &R->f->decls[i];
        if (d->kind == D_FN && strcmp(d->fn_.name, name) == 0) return d;
    }
    return NULL;
}

// 标签构造名:Some/None/Ok/Err + 本文件枚举变体
int is_variant(const rt* R, const char* name) {
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
val call_decl_vals(rt* R, const cdecl* fn, val* args, size_t n) {
    const cfn* F = &fn->fn_;
    if (F->nparams != n) rt_abort(R, RT_ERROR, "参数个数: %s 期望 %zu 实得 %zu", F->name, F->nparams, n);
    env_push(R);
    const char* saved_eh = R->err_head;
    R->err_head = err_head_of(F->ret);
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
    R->err_head = saved_eh;
    env_pop(R);
    return res;
}

// 多/单实参调用函数值(命名函数或闭包)
val invoke_vals(rt* R, val fnv, val* args, size_t n) {
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
val invoke_val1(rt* R, val fnv, val a) {
    val args[1];
    args[0] = a;
    return invoke_vals(R, fnv, args, 1);
}

// 选项/结果方法内建(map/or/expect/context/is_some);命中置 R->opt_result
int option_builtin(rt* R, val recv, const char* m, cexpr* call) {
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
        // Result 专用:Err 包成错误链;Ok 原样。context 物化 AnyError,trace 继承传播链(§5.4)
        if (strcmp(recv.tag, "Err") == 0) {
            val msg = call->nelems == 1 ? eval_expr(R, call->elems[0]) : v_bool(0);
            const char* txt = (msg.k == V_STR && msg.s) ? msg.s : "";
            val payload = recv.nitems >= 1 ? recv.items[0] : v_void();
            const char* tr = (payload.k == V_ERR && payload.err && payload.err->trace) ? payload.err->trace : "main:1";
            val* cause_payload = (val*)ctron_arena_alloc(R->a, sizeof(val));
            cause_payload[0] = payload;
            val* one = (val*)ctron_arena_alloc(R->a, sizeof(val));
            one[0] = v_err_t(R, txt, v_tag("Some", cause_payload, 1), tr);
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

int pat_bind(rt* R, cpat* p, val s) {
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

void ctron_fn_restore(const char* prev);
val call_decl(rt* R, const cdecl* fn, cexpr** args, size_t n) {
    const cfn* F = &fn->fn_;
    const char* saved_fn = NULL;
    {
        static int fat = -1;
        if (fat < 0) fat = getenv("CTRON_FN_TRACE") ? 1 : 0;
        if (fat) {
            saved_fn = ctron_fn_enter(F->name);
        }
    }
    if (F->nparams != n) rt_abort(R, RT_ERROR, "参数个数: %s 期望 %zu 实得 %zu",
                                   F->name, F->nparams, n);
    /* 实参先在调用方环境求值(§4 调用语义:形参不得遮蔽调用方同名局部)。
       否则 or3(b == 34, …) 中实参表达式会读到形参 b 的中间值。 */
    val* vals = NULL;
    if (n) {
        vals = (val*)malloc(n * sizeof(val));
        if (!vals) abort();
        for (size_t i = 0; i < n; i++) vals[i] = eval_expr(R, args[i]);
    }
    env_push(R);
    const char* saved_eh = R->err_head;
    R->err_head = err_head_of(F->ret);
    for (size_t i = 0; i < n; i++) {
        val a = vals[i];
        const cparam* pr = &F->params[i];
        a = apply_decl(R, a, pr->ty);
        env_let(R, pr->name, a);
    }
    if (vals) free(vals);
    int save_ret = R->has_ret;
    val save_retv = R->ret;
    R->has_ret = 0;
    val body = eval_block(R, F->body);
    val res = R->has_ret ? R->ret : body;
    R->has_ret = save_ret;
    R->ret = save_retv;
    R->err_head = saved_eh;
    env_pop(R);
    if (saved_fn) ctron_fn_restore(saved_fn);
    return res;
}

void assert_fail(rt* R, const char* what) { rt_abort(R, RT_ASSERT_FAIL, "%s", what); }
