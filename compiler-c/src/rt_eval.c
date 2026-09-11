#include "rt_internal.h"
#include <time.h>

// rt_eval.c —— 域辅助(和类型/数组/类/并发)+ 表达式求值(C4-i)
// ================= 前向 =================

// 值按声明数值类型适配(其余原样)
val apply_decl(rt* R, val v, const cty* ty) {
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
val as_conv(rt* R, val v, const char* n) {
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

int truthy(val v) { return v.k == V_BOOL ? v.i != 0 : v.k == V_INT ? v.i != 0 : v.k == V_FLOAT ? v.f != 0 : 1; }

// 值拷贝语义:struct 值深拷贝;class/Box 共享
val clone_val(rt* R, val v) {
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

int tag_is_some(const char* t) { return t && (!strcmp(t, "Some") || !strcmp(t, "Ok")); }
int tag_is_none(const char* t) { return t && (!strcmp(t, "None") || !strcmp(t, "Err")); }

// ---- 类实例:impl/trait 方法与 prop 解析(按实例类名) ----
const cfn* cls_method(const rt* R, const char* cls, const char* m) {
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
const cprop* cls_prop(const rt* R, const char* cls, const char* m) {
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
val call_method_body(rt* R, const cfn* F, val self, cexpr** args, size_t nargs) {
    // 参数:receiver(&self)→ self;其余按名
    size_t named = 0;
    for (size_t i = 0; i < F->nparams; i++) if (!F->params[i].is_receiver) named++;
    if (named != nargs) rt_abort(R, RT_ERROR, "方法参数个数: %s", F->name ? F->name : "?");
    env_push(R);
    const char* saved_eh = R->err_head;
    R->err_head = err_head_of(F->ret);
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
    R->err_head = saved_eh;
    env_pop(R);
    return res;
}
val call_prop_body(rt* R, const cprop* P, val self) {
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


val v_list(rt* R) {
    val v = {0};
    v.k = V_LIST;
    listnode* ln = (listnode*)ctron_arena_alloc(R->a, sizeof(listnode));
    v.lst = ln;
    return v;
}
val v_atom(rt* R, __int128 x, int bits, int us) {
    val v = {0};
    v.k = V_ATOM;
    atomcell* c = (atomcell*)ctron_arena_alloc(R->a, sizeof(atomcell));
    c->v = x;
    c->bits = bits;
    c->us = us;
    v.atom = c;
    return v;
}
void list_push(rt* R, listnode* ln, val item) {
    if (ln->n == ln->cap) {
        size_t nc = ln->cap ? ln->cap * 2 : 4;
        val* ni = (val*)ctron_arena_alloc(R->a, nc * sizeof(val));
        for (size_t i = 0; i < ln->n; i++) ni[i] = ln->items[i];
        ln->items = ni;
        ln->cap = nc;
    }
    ln->items[ln->n++] = item;
}
val list_clone_deep(rt* R, const listnode* ln) {
    val v = v_list(R);
    for (size_t i = 0; i < ln->n; i++) list_push(R, v.lst, clone_val(R, ln->items[i]));
    return v;
}



// ---------------- 并发原语 ----------------
val v_tuple(rt* R, val a, val b) {
    val* it = (val*)ctron_arena_alloc(R->a, 2 * sizeof(val));
    it[0] = a;
    it[1] = b;
    val v = {0};
    v.k = V_TUPLE;
    v.items = it;
    v.nitems = 2;
    return v;
}
val v_chan(rt* R, int cap) {
    val v = {0};
    v.k = V_CHAN;
    chan_t* c = (chan_t*)ctron_arena_alloc(R->a, sizeof(chan_t));
    c->cap = cap;
    v.chan = c;
    v.us = 1; // 发送端
    return v;
}
val v_mutex(rt* R, val inner) {
    val v = {0};
    v.k = V_MUTEX;
    mutex_t* m = (mutex_t*)ctron_arena_alloc(R->a, sizeof(mutex_t));
    m->inner = inner;
    v.mtx = m;
    return v;
}
val v_scope(rt* R) {
    val v = {0};
    v.k = V_SCOPE;
    scope_t* sc = (scope_t*)ctron_arena_alloc(R->a, sizeof(scope_t));
    v.scope = sc;
    return v;
}
val v_task(rt* R, task_t* t) {
    (void)R;
    val v = {0};
    v.k = V_TASK;
    v.task = t;
    return v;
}
void scope_add_task(rt* R, scope_t* sc, task_t* t) {
    (void)R;
    t->next = sc->tasks;
    sc->tasks = t;
}

// 运行任务体:任务级捕获 panic
void run_task(rt* R, task_t* t) {
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
int run_next_task(rt* R) {
    if (!R->scope) return 0;
    for (task_t* t = R->scope->tasks; t; t = t->next) {
        if (!t->ran) {
            run_task(R, t);
            return 1;
        }
    }
    return 0;
}

val chan_send(rt* R, val sender, val v) {
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

val chan_recv(rt* R, val recv) {
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

val mutex_with(rt* R, val mx, val f, int mut) {
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

int val_eq(rt* R, val a, val b) {
    (void)R;
    if (a.k == V_INT && b.k == V_INT) return a.i == b.i;
    if (a.k == V_FLOAT && b.k == V_FLOAT) return a.f == b.f;
    if (a.k == V_INT && b.k == V_FLOAT) return (double)a.i == b.f;
    if (a.k == V_FLOAT && b.k == V_INT) return a.f == (double)b.i;
    if (a.k == V_BOOL && b.k == V_BOOL) return a.i == b.i;
    if (a.k == V_STR && b.k == V_STR) return strcmp(a.s ? a.s : "", b.s ? b.s : "") == 0;
    return 0;
}

// ================= 表达式 =================
val eval_expr(rt* R, cexpr* e) {
    if (!e) return v_void();
    if (R->max_steps && ++R->steps > R->max_steps)
        rt_abort(R, RT_PANIC, "instruction limit exceeded (可能的无限循环)"); // D2:CTRON_MAX_STEPS
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
        if (R->consts) { // const(初始化已预求值,即 comptime)
            size_t ci = 0;
            for (size_t i = 0; i < R->f->ndecls; i++) {
                const cdecl* cd = &R->f->decls[i];
                if (cd->kind != D_CONST) continue;
                if (strcmp(cd->konst.name, e->text) == 0) return R->consts[ci];
                ci++;
            }
        }
        if (is_variant(R, e->text)) return v_tag(e->text, NULL, 0); // 裸变体值(如 None)
        if (!strcmp(e->text, "parallel") || !strcmp(e->text, "dom"))
            return v_ns(e->text); // 内建命名空间(§7.7/§9.2)
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
        if (e->bop == B_OR) {
            // or 中缀取默认(§4.4):Some/Ok(1 载荷)取载荷,否则取默认;与 .or(默认) 语义唯一
            val l = eval_expr(R, e->lhs);
            if (l.k == V_TAG && tag_is_some(l.tag) && l.nitems == 1) return l.items[0];
            return eval_expr(R, e->rhs);
        }
        val l = eval_expr(R, e->lhs);
        val r = eval_expr(R, e->rhs);
        if (l.k == V_SIMD || r.k == V_SIMD) {
            // 元素级白名单运算(§9.5):+ - * /,两侧同长 Simd
            if (l.k != V_SIMD || r.k != V_SIMD) rt_abort(R, RT_ERROR, "Simd 运算需两侧同为 Simd");
            if (l.nitems != r.nitems) rt_abort(R, RT_ERROR, "Simd 元素数不一致");
            if (e->bop != B_ADD && e->bop != B_SUB && e->bop != B_MUL && e->bop != B_DIV)
                rt_abort(R, RT_ERROR, "Simd 元素级运算不支持该算符");
            val* it = (val*)ctron_arena_alloc(R->a, l.nitems * sizeof(val));
            for (size_t i = 0; i < l.nitems; i++) {
                double a = l.items[i].k == V_FLOAT ? l.items[i].f : (double)l.items[i].i;
                double b2 = r.items[i].k == V_FLOAT ? r.items[i].f : (double)r.items[i].i;
                it[i] = v_flt(e->bop == B_ADD ? a + b2 : e->bop == B_SUB ? a - b2
                              : e->bop == B_MUL ? a * b2 : a / b2);
            }
            val sv = {0};
            sv.k = V_SIMD;
            sv.items = it;
            sv.nitems = l.nitems;
            return sv;
        }
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
    case EX_TUPLE: {
        val* it = NULL;
        if (e->nelems) {
            it = (val*)ctron_arena_alloc(R->a, e->nelems * sizeof(val));
            for (size_t i = 0; i < e->nelems; i++) it[i] = eval_expr(R, e->elems[i]);
        }
        val v = {0};
        v.k = V_TUPLE;
        v.items = it;
        v.nitems = e->nelems;
        return v;
    }
    case EX_TYPEARGS:
        return eval_expr(R, e->obj); // 类型实参后缀对值透明(如 Simd[F32,4] 的基)
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
        if (!e->m_is_name) {
            // 元组下标 .0/.1/…
            if (o.k != V_TUPLE) rt_abort(R, RT_ERROR, "元组下标目标不支持");
            if (e->mtuple >= o.nitems) rt_abort(R, RT_PANIC, "index out of bounds");
            return o.items[e->mtuple];
        }
        if (!e->mname) rt_abort(R, RT_ERROR, "属性访问不支持");
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
            if (strcmp(m, "trace") == 0) {
                // 两段式位置链:自环向上找最近一次 ? 记录(§5.4)
                const errval* e2 = o.err;
                for (int d2 = 0; e2 && d2 < 8; d2++) {
                    if (e2->trace) return v_str_own(R, e2->trace);
                    if (e2->cause.k == V_TAG && e2->cause.nitems == 1 && e2->cause.items[0].k == V_ERR)
                        e2 = e2->cause.items[0].err;
                    else break;
                }
                return v_str_own(R, "");
            }
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
        if (v.k == V_TAG && strcmp(v.tag, "Err") == 0) {
            // 两段式擦除:函数错误类型为 AnyError 时,? 记录调用点并自动转换(§5.3)
            if (R->err_head && strcmp(R->err_head, "AnyError") == 0) {
                val inner = v.nitems >= 1 ? v.items[0] : v_void();
                val errv;
                if (inner.k != V_ERR) {
                    sb b = {0};
                    fmt_val(R, inner, &b);
                    val* cause_payload = (val*)ctron_arena_alloc(R->a, sizeof(val));
                    cause_payload[0] = inner;
                    errv = v_err_t(R, b.d ? b.d : "", v_tag("Some", cause_payload, 1), "main:1");
                    free(b.d);
                } else {
                    errv = inner; // 已是 AnyError,原样传播
                }
                val* one = (val*)ctron_arena_alloc(R->a, sizeof(val));
                one[0] = errv;
                R->ret = v_tag("Err", one, 1); // 自动擦除为 Err(AnyError) 并传播
                R->has_ret = 1;
                return R->ret;
            }
            R->ret = v;
            R->has_ret = 1;
            return v;
        }
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
            && strcmp(cal->obj->text, "List") == 0) {
            return v_list(R); // GC List[T]()
        }
        // Simd[E, N].method(args) —— splat(其余 lane/to_array 走成员路径)
        if (cal && cal->kind == EX_MEMBER && cal->m_is_name && cal->mname && cal->obj
            && cal->obj->kind == EX_TYPEARGS && cal->obj->obj && cal->obj->obj->kind == EX_IDENT
            && strcmp(cal->obj->obj->text, "Simd") == 0) {
            if (strcmp(cal->mname, "splat") != 0) rt_abort(R, RT_ERROR, "Simd 方法不支持: %s", cal->mname);
            size_t n = 0;
            for (size_t ti = 0; ti < cal->obj->ntargs; ti++) {
                const cty* ta = cal->obj->targs[ti];
                if (ta->kind == TY_CVAL && ta->npath == 1) n = (size_t)strtoul(ta->path[0], NULL, 10);
            }
            if (n == 0 || n > 64) rt_abort(R, RT_ERROR, "Simd 宽度非法");
            if (e->nelems != 1) rt_abort(R, RT_ERROR, "splat 实参");
            val x = eval_expr(R, e->elems[0]);
            double d = x.k == V_FLOAT ? x.f : (double)x.i;
            val* it = (val*)ctron_arena_alloc(R->a, n * sizeof(val));
            for (size_t i = 0; i < n; i++) it[i] = v_flt(d);
            val sv = {0};
            sv.k = V_SIMD;
            sv.items = it;
            sv.nitems = n;
            return sv;
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
            if (!strcmp(nm, "byte_at")) {
                if (e->nelems != 2) rt_abort(R, RT_ERROR, "byte_at 实参");
                val sv = eval_expr(R, e->elems[0]);
                val iv = eval_expr(R, e->elems[1]);
                if (sv.k != V_STR) rt_abort(R, RT_ERROR, "byte_at 目标需 Str");
                long long i = (long long)iv.i;
                if (i < 0 || !sv.s || (unsigned long long)i >= strlen(sv.s))
                    rt_abort(R, RT_PANIC, "index out of bounds");
                return v_int((unsigned char)sv.s[i], 32, 0);
            }
            if (!strcmp(nm, "byte_slice")) {
                if (e->nelems != 3) rt_abort(R, RT_ERROR, "byte_slice 实参");
                val sv = eval_expr(R, e->elems[0]);
                val av = eval_expr(R, e->elems[1]);
                val bv = eval_expr(R, e->elems[2]);
                if (sv.k != V_STR) rt_abort(R, RT_ERROR, "byte_slice 目标需 Str");
                long long a = (long long)av.i, b = (long long)bv.i;
                long long len = sv.s ? (long long)strlen(sv.s) : 0;
                if (a < 0 || b > len || a > b) rt_abort(R, RT_PANIC, "byte_slice 越界");
                val o = {0};
                o.k = V_STR;
                o.s = ctron_arena_strndup(R->a, sv.s + a, (size_t)(b - a));
                return o;
            }
            if (!strcmp(nm, "read_dir")) {
                if (e->nelems != 1) rt_abort(R, RT_ERROR, "read_dir 实参");
                val pv = eval_expr(R, e->elems[0]);
                const char* path = (pv.k == V_STR && pv.s) ? pv.s : "";
                DIR* dd = opendir(path);
                if (!dd) {
                    val* none = NULL;
                    return v_tag("None", none, 0);
                }
                size_t cap = 256, len = 0;
                char* buf = (char*)malloc(cap);
                if (!buf) abort();
                buf[0] = 0;
                struct dirent* de;
                while ((de = readdir(dd)) != NULL) {
                    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
                    size_t bl = strlen(de->d_name);
                    if (len + bl + 2 > cap) {
                        while (len + bl + 2 > cap) cap *= 2;
                        char* nb2 = (char*)realloc(buf, cap);
                        if (!nb2) abort();
                        buf = nb2;
                    }
                    memcpy(buf + len, de->d_name, bl);
                    len += bl;
                    buf[len++] = '\n';
                }
                closedir(dd);
                buf[len] = 0;
                char* ar = ctron_arena_strndup(R->a, buf, len);
                free(buf);
                val* one = (val*)ctron_arena_alloc(R->a, sizeof(val));
                one[0] = v_str_own(R, ar);
                return v_tag("Some", one, 1);
            }
            if (!strcmp(nm, "ctron_entry")) {
                val o = {0};
                o.k = V_STR;
                o.s = "";
                return o;
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
            if (!strcmp(nm, "fs_exists")) {
                if (e->nelems != 1) rt_abort(R, RT_ERROR, "fs_exists 实参");
                val pv = eval_expr(R, e->elems[0]);
                const char* path = (pv.k == V_STR && pv.s) ? pv.s : "";
                FILE* f = fopen(path, "rb");
                if (!f) return v_bool(0);
                fclose(f);
                return v_bool(1);
            }
            if (!strcmp(nm, "fs_write")) {
                if (e->nelems != 2) rt_abort(R, RT_ERROR, "fs_write 实参");
                val pv = eval_expr(R, e->elems[0]);
                val cv = eval_expr(R, e->elems[1]);
                const char* path = (pv.k == V_STR && pv.s) ? pv.s : "";
                const char* data = (cv.k == V_STR && cv.s) ? cv.s : "";
                FILE* f = fopen(path, "wb");
                if (!f) return v_bool(0);
                size_t dn = strlen(data);
                size_t w = fwrite(data, 1, dn, f);
                fclose(f);
                return v_bool(w == dn);
            }
            if (!strcmp(nm, "fs_delete")) {
                if (e->nelems != 1) rt_abort(R, RT_ERROR, "fs_delete 实参");
                val pv = eval_expr(R, e->elems[0]);
                const char* path = (pv.k == V_STR && pv.s) ? pv.s : "";
                return v_bool(remove(path) == 0);
            }
            if (!strcmp(nm, "now_ms_text")) {
                // now_ms 的求值面伴生:返回毫秒的规范十进制文本(cc D 域 vD 包装)
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                long long ms = (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
                char buf[32];
                snprintf(buf, sizeof(buf), "%lld", ms);
                return v_str_own(R, ctron_arena_strndup(R, buf, strlen(buf)));
            }
            if (!strcmp(nm, "now_ms")) {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                double ms = (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
                return v_flt(ms);
            }
            if (!strcmp(nm, "utf8_enc")) {
                if (e->nelems != 1) rt_abort(R, RT_ERROR, "utf8_enc 实参");
                val cv = eval_expr(R, e->elems[0]);
                long long cp = (long long)cv.i;
                if (cp < 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = 0xFFFD;
                unsigned char b[5]; int un = 0;
                if (cp < 0x80) { b[un++] = (unsigned char)cp; }
                else if (cp < 0x800) { b[un++] = (unsigned char)(0xC0 | (cp >> 6)); b[un++] = (unsigned char)(0x80 | (cp & 0x3F)); }
                else if (cp < 0x10000) { b[un++] = (unsigned char)(0xE0 | (cp >> 12)); b[un++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F)); b[un++] = (unsigned char)(0x80 | (cp & 0x3F)); }
                else { b[un++] = (unsigned char)(0xF0 | (cp >> 18)); b[un++] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F)); b[un++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F)); b[un++] = (unsigned char)(0x80 | (cp & 0x3F)); }
                b[un] = 0;
                char* ar = ctron_arena_strndup(R->a, (char*)b, (size_t)un);
                return v_str_own(R, ar);
            }
            if (!strcmp(nm, "read_line")) {
                // stdin 读一行(去尾部 \n/\r);EOF 返回空串。LSP/管道程序用。
                size_t cap = 256, n = 0;
                char* buf = (char*)malloc(cap);
                if (!buf) abort();
                int c;
                while ((c = fgetc(stdin)) != EOF) {
                    if (c == '\n') break;
                    if (n + 2 > cap) { cap *= 2; buf = (char*)realloc(buf, cap); if (!buf) abort(); }
                    buf[n++] = (char)c;
                }
                if (c == EOF && n == 0) { free(buf); return v_str_own(R, ""); }
                if (n > 0 && buf[n - 1] == '\r') n--;
                buf[n] = 0;
                char* ar = ctron_arena_strndup(R->a, buf, n);
                free(buf);
                return v_str_own(R, ar);
            }
            if (!strcmp(nm, "read_bytes")) {
                // stdin 恰好读 n 字节(LSP Content-Length 体);不足则返回已读部分
                if (e->nelems != 1) rt_abort(R, RT_ERROR, "read_bytes 实参");
                val nv = eval_expr(R, e->elems[0]);
                long long want = nv.k == V_INT ? (long long)nv.i : 0;
                if (want < 0) rt_abort(R, RT_PANIC, "read_bytes 负长度");
                size_t cap = (size_t)want + 1;
                char* buf = (char*)malloc(cap);
                if (!buf) abort();
                size_t got = 0;
                while (got < (size_t)want) {
                    size_t k = fread(buf + got, 1, (size_t)want - got, stdin);
                    if (k == 0) break;
                    got += k;
                }
                buf[got] = 0;
                char* ar = ctron_arena_strndup(R->a, buf, got);
                free(buf);
                return v_str_own(R, ar);
            }
            if (!strcmp(nm, "flush_out")) {
                // 把 print/println 的缓冲立即写到真实 stdout(LSP 每帧后调用)
                if (R->out && R->out_n) {
                    fwrite(R->out, 1, R->out_n, stdout);
                    fflush(stdout);
                    R->out_n = 0;
                    R->out[0] = 0;
                }
                return v_void();
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
                    rsb_s(&m, want ? "assert_eq failed: " : "assert_ne failed: ");
                    fmt_val(R, a, &m);
                    rsb_s(&m, " != ");
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
            if (cal->mname && strcmp(cal->mname, "contains") == 0 && recv.k == V_STR) {
                if (e->nelems != 1) rt_abort(R, RT_ERROR, "contains 实参");
                val nv = eval_expr(R, e->elems[0]);
                if (nv.k != V_STR) rt_abort(R, RT_ERROR, "contains 需 Str");
                return v_bool(strstr(recv.s ? recv.s : "", nv.s ? nv.s : "") != NULL);
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
                rsb_s(&b, recv.type ? recv.type : "?");
                rsb_s(&b, "(");
                for (size_t i = 0; i < recv.nfld; i++) {
                    if (i) rsb_s(&b, ",");
                    rsb_s(&b, recv.flds[i].name);
                    rsb_s(&b, "=");
                    fmt_val(R, recv.flds[i].v, &b);
                }
                rsb_s(&b, ")");
                val out = v_void();
                out.k = V_STR;
                out.s = astr(R, b.d ? b.d : "");
                free(b.d);
                return out;
            }
            if (recv.k == V_NS && cal->mname) {
                // 内建命名空间:parallel(§7.7 数据并行,解释器序贯执行)/ stdweb.dom(§9.2 最小锚)
                const char* ns = recv.tag;
                if (!strcmp(ns, "parallel") && !strcmp(cal->mname, "map")) {
                    if (e->nelems != 2) rt_abort(R, RT_ERROR, "parallel.map 实参");
                    val arrv = eval_expr(R, e->elems[0]);
                    val fv = eval_expr(R, e->elems[1]);
                    if (arrv.k != V_ARR) rt_abort(R, RT_ERROR, "parallel.map 需切片");
                    val* out = (val*)ctron_arena_alloc(R->a, (arrv.nitems ? arrv.nitems : 1) * sizeof(val));
                    for (size_t i = 0; i < arrv.nitems; i++) out[i] = invoke_val1(R, fv, arrv.items[i]);
                    return v_arr(out, arrv.nitems);
                }
                if (!strcmp(ns, "parallel") && !strcmp(cal->mname, "reduce")) {
                    if (e->nelems != 3) rt_abort(R, RT_ERROR, "parallel.reduce 实参");
                    val arrv = eval_expr(R, e->elems[0]);
                    val acc = eval_expr(R, e->elems[1]);
                    val fv = eval_expr(R, e->elems[2]);
                    if (arrv.k != V_ARR) rt_abort(R, RT_ERROR, "parallel.reduce 需切片");
                    for (size_t i = 0; i < arrv.nitems; i++) {
                        val pair[2];
                        pair[0] = acc;
                        pair[1] = arrv.items[i];
                        acc = invoke_vals(R, fv, pair, 2);
                    }
                    return acc;
                }
                if (!strcmp(ns, "dom") && !strcmp(cal->mname, "set_title")) {
                    if (e->nelems != 1) rt_abort(R, RT_ERROR, "set_title 实参");
                    val tv = eval_expr(R, e->elems[0]);
                    if (tv.k != V_STR) rt_abort(R, RT_ERROR, "set_title 需 Str");
                    R->dom_title = astr(R, tv.s ? tv.s : "");
                    return v_void();
                }
                if (!strcmp(ns, "dom") && !strcmp(cal->mname, "title")) {
                    if (e->nelems != 0) rt_abort(R, RT_ERROR, "title 实参");
                    return v_str_own(R, R->dom_title ? R->dom_title : "");
                }
                rt_abort(R, RT_ERROR, "命名空间不支持: %s.%s", ns, cal->mname);
            }
            if (recv.k == V_SIMD && cal->mname) {
                if (!strcmp(cal->mname, "lane")) {
                    if (e->nelems != 1) rt_abort(R, RT_ERROR, "lane 实参");
                    val iv = eval_expr(R, e->elems[0]);
                    long long li = (long long)iv.i;
                    if (li < 0 || (unsigned long long)li >= recv.nitems) rt_abort(R, RT_PANIC, "index out of bounds");
                    return recv.items[li];
                }
                if (!strcmp(cal->mname, "to_array")) {
                    if (e->nelems != 0) rt_abort(R, RT_ERROR, "to_array 实参");
                    val* it = (val*)ctron_arena_alloc(R->a, (recv.nitems ? recv.nitems : 1) * sizeof(val));
                    for (size_t i = 0; i < recv.nitems; i++) it[i] = recv.items[i];
                    return v_arr(it, recv.nitems);
                }
                rt_abort(R, RT_ERROR, "Simd 方法不支持: %s", cal->mname);
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

// const 初始化即 comptime(§8):运行 test/main 前预求值全部 D_CONST
void eval_consts(rt* R) {
    size_t n = 0;
    for (size_t i = 0; i < R->f->ndecls; i++)
        if (R->f->decls[i].kind == D_CONST) n++;
    if (!n) return;
    R->consts = (val*)ctron_arena_alloc(R->a, n * sizeof(val));
    R->nconsts = n;
    env* saved = R->top;
    R->top = NULL;
    env_push(R);
    size_t ci = 0;
    for (size_t i = 0; i < R->f->ndecls; i++) {
        const cdecl* d = &R->f->decls[i];
        if (d->kind != D_CONST) continue;
        val v = d->konst.expr ? eval_expr(R, d->konst.expr) : v_void();
        R->consts[ci++] = apply_decl(R, v, d->konst.ty);
    }
    R->top = saved;
}
