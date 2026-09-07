#include "trans_internal.h"

// trans_conc.c —— 并发运行时发射(spawn/join/Channel/Mutex/Atomic/scope/parallel/own)
// ================= 并发运行时(C10-k)=================
// 模型:scope/spawn/join/join_or/Channel/Mutex 代码生成。任务体在 spawn 点内联
// eager 执行(任务级 panic 捕获于每-任务 ctron_task_ctx,栈式 ctron_task_cur),
// task 记录供 join/join_or 读取结果。Channel 为有界队列:满/空时若当前作用域已
// 取消则 Err(ScopeCancelled),否则立即 Err(eager 模型下无等待任务可推进)。
// 与 rt 的差异仅在任务何时真正执行 —— 对确定性协作语料观察等价(见 HANDOFF)。
ty holder_ty(int k, ty inner) {
    ty h = ty_unk(); h.k = k;
    h.ek = inner.k; h.ebits = inner.bits; h.eus = inner.us;
    if (inner.k == T_STRUCT || inner.k == T_ENUM || inner.k == T_CLASS) h.tname = inner.tname;
    return h;
}
ty ctron_payload_ty(ty t) { // 容器(T_CHAN/T_TASK/T_MUTEX)载荷类型
    ty e = ty_unk(); e.k = t.ek; e.bits = t.ebits; e.us = t.eus;
    if (t.ek == T_FLT) e = ty_flt();
    else if (t.ek == T_BOOL) e = ty_bool();
    else if (t.ek == T_STR) e = ty_str();
    else if (t.ek == T_ERR) e.k = T_ERR;
    else if (t.ek == T_STRUCT || t.ek == T_ENUM || t.ek == T_CLASS) e.tname = t.tname;
    return e;
}
void task_mk(char* b, size_t n, ty r) {
    if (r.k == T_UNK) snprintf(b, n, "void");
    else if (r.k == T_FLT) snprintf(b, n, "F64");
    else if (r.k == T_BOOL) snprintf(b, n, "Bool");
    else if (r.k == T_STR) snprintf(b, n, "Str");
    else if (r.k == T_STRUCT || r.k == T_ENUM || r.k == T_CLASS) snprintf(b, n, "%s", r.tname ? r.tname : "?");
    else snprintf(b, n, "%c%d", r.us ? 'U' : 'I', r.bits ? r.bits : 32);
}
void chan_mk(char* b, size_t n, ty e) {
    if (e.k == T_FLT) snprintf(b, n, "f64");
    else if (e.k == T_BOOL) snprintf(b, n, "b");
    else if (e.k == T_STR) snprintf(b, n, "str");
    else if (e.k == T_STRUCT || e.k == T_ENUM || e.k == T_CLASS) snprintf(b, n, "%s", e.tname ? e.tname : "?");
    else snprintf(b, n, "%c%d", e.us ? 'u' : 'i', e.bits ? e.bits : 32);
}
void rt_chan_type(tc* c, ty elem) {
    if (elem.k == T_UNK) return;
    char wl[64];
    chan_mk(wl, sizeof wl, elem);
    const char* ect = ctype_of(elem); // 载荷 C 类型
    sb t = {0};
    sb_f(&t, "typedef struct { int64_t cap, head, n; %s* d; } ctron_chan_%s;\n", ect, wl);
    sb_f(&t, "typedef struct { int tag; union { %s ok; int32_t err; } as; } ctron_res_%s_ChanE;\n", ect, wl);
    sb_s(&t, "#define CTRON_RES_OK 0\n#define CTRON_RES_ERR 1\n");
    sb_f(&t, "static ctron_chan_%s* ctron_chan_%s_new(int64_t cap) { ctron_chan_%s* cn = (ctron_chan_%s*)calloc(1, sizeof(ctron_chan_%s)); cn->cap = cap > 0 ? cap : 1; cn->d = (%s*)calloc((size_t)cn->cap, sizeof(%s)); return cn; }\n", wl, wl, wl, wl, wl, ect, ect);
    sb_f(&t, "static ctron_res_%s_ChanE ctron_chan_%s_send(ctron_chan_%s* cn, %s v) { ctron_res_%s_ChanE r; if (cn->n < cn->cap) { cn->d[(cn->head + cn->n) %% cn->cap] = v; cn->n++; r.tag = CTRON_RES_OK; r.as.ok = v; return r; } if (ctron_scope_cur && ctron_scope_cur->cancelled) { r.tag = CTRON_RES_ERR; r.as.err = 0; return r; } r.tag = CTRON_RES_ERR; r.as.err = 0; return r; }\n", wl, wl, wl, ect, wl);
    sb_f(&t, "static ctron_res_%s_ChanE ctron_chan_%s_recv(ctron_chan_%s* cn) { ctron_res_%s_ChanE r; if (cn->n > 0) { %s o = cn->d[cn->head]; cn->head = (cn->head + 1) %% cn->cap; cn->n--; r.tag = CTRON_RES_OK; r.as.ok = o; return r; } if (ctron_scope_cur && ctron_scope_cur->cancelled) { r.tag = CTRON_RES_ERR; r.as.err = 0; return r; } r.tag = CTRON_RES_ERR; r.as.err = 0; return r; }\n", wl, wl, wl, wl, ect, wl);
    use_sum(c, t.d ? t.d : "");
    sb_free(&t);
}
void rt_mutex_type(tc* c, ty inner) {
    if (inner.k == T_UNK) return;
    char m[64];
    chan_mk(m, sizeof m, inner);
    const char* ict = ctype_of(inner);
    sb t = {0};
    sb_f(&t, "typedef struct { %s v; } ctron_mutex_%s;\n", ict, m);
    sb_f(&t, "static ctron_mutex_%s* ctron_mutex_%s_new(%s v) { ctron_mutex_%s* p = (ctron_mutex_%s*)calloc(1, sizeof(ctron_mutex_%s)); p->v = v; return p; }\n", m, m, ict, m, m, m);
    sb_f(&t, "static %s ctron_mutex_%s_load(ctron_mutex_%s* p) { return p->v; }\n", ict, m, m);
    sb_f(&t, "static void ctron_mutex_%s_store(ctron_mutex_%s* p, %s v) { p->v = v; }\n", m, m, ict);
    sb_f(&t, "static %s ctron_mutex_%s_fetch_add(ctron_mutex_%s* p, %s d) { %s o = p->v; p->v = (%s)((__int128)p->v + (__int128)d); return o; }\n", ict, m, m, ict, ict, ict);
    use_sum(c, t.d ? t.d : "");
    sb_free(&t);
}
void rt_task_type(tc* c, ty r) {
    char mk[64];
    task_mk(mk, sizeof mk, r);
    const char* rct = r.k == T_UNK ? NULL : ctype_of(r);
    sb t = {0};
    if (r.k == T_UNK) {
        sb_f(&t, "typedef struct { int panicked; char msg[256]; } ctron_task_void;\n");
        sb_s(&t, "static void ctron_join_void(ctron_task_void* t) { if (t->panicked) ctron_panic(t->msg); }\n");
        sb_f(&t, "typedef struct { int tag; union { int64_t ok; int32_t err; } as; } ctron_res_void_TP;\n#define CTRON_RES_OK 0\n#define CTRON_RES_ERR 1\n");
        sb_f(&t, "static ctron_res_void_TP ctron_join_or_void(ctron_task_void* t) { ctron_res_void_TP r; if (t->panicked) { if (ctron_scope_cur) ctron_scope_cur->cancelled = 1; r.tag = CTRON_RES_ERR; r.as.err = 0; return r; } r.tag = CTRON_RES_OK; r.as.ok = 0; return r; }\n");
    } else {
        sb_f(&t, "typedef struct { int panicked; char msg[256]; %s result; } ctron_task_%s;\n", rct, mk);
        sb_f(&t, "static %s ctron_join_%s(ctron_task_%s* t) { if (t->panicked) ctron_panic(t->msg); return t->result; }\n", rct, mk, mk);
        sb_f(&t, "typedef struct { int tag; union { %s ok; int32_t err; } as; } ctron_res_%s_TP;\n#define CTRON_RES_OK 0\n#define CTRON_RES_ERR 1\n", rct, mk);
        sb_f(&t, "static ctron_res_%s_TP ctron_join_or_%s(ctron_task_%s* t) { ctron_res_%s_TP r; if (t->panicked) { if (ctron_scope_cur) ctron_scope_cur->cancelled = 1; r.tag = CTRON_RES_ERR; r.as.err = 0; return r; } r.tag = CTRON_RES_OK; r.as.ok = t->result; return r; }\n", mk, mk, mk, mk);
    }
    use_sum(c, t.d ? t.d : "");
    sb_free(&t);
}
// 尾表达式仅求值副作用(void 任务/with_mut 块尾/scope 无值尾)
void emit_effect_expr(tc* c, cexpr* t, sb* o) {
    if (!t || c->err) return;
    if (t->kind == EX_MATCH) { emit_match(c, t, o, 0, NULL); return; }
    if (t->kind == EX_IF) { emit_if_stmt(c, t, o); return; }
    if (t->kind == EX_BLOCK) { scope_push(c); sb_s(o, "{\n"); emit_block(c, t->block, o); scope_pop(c); sb_s(o, "}\n"); return; }
    if (t->kind == EX_OWN) { scope_push(c); emit_block(c, t->obody, o); scope_pop(c); return; }
    sb x = {0};
    ty tt = emit_expr(c, t, &x);
    (void)tt;
    if (!c->err) sb_f(o, "%s;\n", x.d && *x.d ? x.d : "0");
    sb_free(&x);
}
// 尾表达式求值并赋给 dest(match/if/块值走语句提升;其余为表达式)
void emit_tail_to(tc* c, cexpr* t, const char* dest, sb* o) {
    if (!t || c->err) return;
    if (t->kind == EX_MATCH) {
        char* rn = NULL;
        ty mt = emit_match(c, t, o, 1, &rn);
        (void)mt;
        if (rn && !c->err) sb_f(o, "%s = %s;\n", dest, rn);
        return;
    }
    emit_value_to(c, t, dest, o);
}
// 内联发射闭包体(调用点已预先绑好 cparams 同名 Ctron 变量)。
// dest 非空:块尾/体值赋给 dest;dest 空:仅求值副作用(void 任务 / with_mut)。
void emit_inline_closure_body(tc* c, const cexpr* clo, const char* dest, sb* o) {
    if (!clo || !clo->cbody || c->err) return;
    scope_push(c);
    sb_s(o, "{\n");
    const cexpr* b = clo->cbody;
    if (b->kind == EX_BLOCK && b->block) {
        cblock* bl = b->block;
        for (size_t i = 0; i < bl->nstmts && !c->err; i++) emit_stmt(c, bl->stmts[i], o);
        if (bl->tail && !c->err) {
            if (dest) emit_tail_to(c, bl->tail, dest, o);
            else emit_effect_expr(c, bl->tail, o);
        }
    } else if (dest) {
        emit_tail_to(c, b, dest, o);
    } else {
        emit_effect_expr(c, b, o);
    }
    scope_pop(c);
    sb_s(o, "}\n");
}
// 值位置表达式的类型探针(块/if/match 尾;普通表达式走真实发射到哑缓冲)
ty probe_val_ty(tc* c, cexpr* t) {
    if (!t || c->err) return ty_unk();
    if (t->kind == EX_BLOCK) {
        if (!t->block) return ty_unk();
        scope_push(c);
        for (size_t i = 0; i < t->block->nstmts; i++) { sb d = {0}; emit_stmt(c, t->block->stmts[i], &d); sb_free(&d); if (c->err) break; }
        ty r = c->err ? ty_unk() : probe_val_ty(c, t->block->tail);
        scope_pop(c);
        return r;
    }
    if (t->kind == EX_IF) {
        if (!t->then_b) return ty_unk();
        scope_push(c);
        for (size_t i = 0; i < t->then_b->nstmts; i++) { sb d = {0}; emit_stmt(c, t->then_b->stmts[i], &d); sb_free(&d); if (c->err) break; }
        ty r = c->err ? ty_unk() : probe_val_ty(c, t->then_b->tail);
        scope_pop(c);
        return r;
    }
    if (t->kind == EX_MATCH) {
        if (!t->narms) return ty_unk();
        cexpr* ax = t->arms[0].expr;
        if (ax && ax->kind == EX_BLOCK && ax->block) {
            scope_push(c);
            for (size_t i = 0; i < ax->block->nstmts; i++) { sb d = {0}; emit_stmt(c, ax->block->stmts[i], &d); sb_free(&d); if (c->err) break; }
            ty r = c->err ? ty_unk() : probe_val_ty(c, ax->block->tail);
            scope_pop(c);
            return r;
        }
        return probe_val_ty(c, ax);
    }
    if (t->kind == EX_CLOSURE) return ty_fnptr(); // 值位置闭包:类型恒函数指针(避免干跑重复发射)
    sb d = {0};
    ty r = emit_expr(c, t, &d);
    sb_free(&d);
    return r;
}
// 闭包返回值类型探针(cret 注解优先;否则块尾/体表达式类型;无尾 = void 任务)
ty probe_closure_ret(tc* c, const cexpr* clo) {
    if (!clo || c->err) return ty_unk();
    if (clo->cret) {
        ty rt = decl_ty_tc(c, clo->cret);
        if (rt.k != T_UNK) return rt;
    }
    if (!clo->cbody) return ty_unk();
    scope_push(c);
    ty r = ty_unk();
    const cexpr* b = clo->cbody;
    if (b->kind == EX_BLOCK && b->block) {
        cblock* bl = b->block;
        if (bl->tail) {
            for (size_t i = 0; i < bl->nstmts && !c->err; i++) { sb d = {0}; emit_stmt(c, bl->stmts[i], &d); sb_free(&d); }
            if (!c->err) r = probe_val_ty(c, bl->tail);
        }
    } else {
        r = probe_val_ty(c, b);
    }
    scope_pop(c);
    return r;
}
// 任务记录(heaped)+ 内联 eager 体;产生 ctype = ctron_task_<mk>* 的 GNU 值
ty emit_spawn_call(tc* c, cexpr* e, sb* o) {
    if (e->nelems != 1 || !e->elems[0] || e->elems[0]->kind != EX_CLOSURE) {
        terr(c, "v1:spawn 需单个闭包实参");
        return ty_unk();
    }
    const cexpr* clo = e->elems[0];
    if (clo->ncparams > 0) { terr(c, "v1:spawn 闭包参数不支持"); return ty_unk(); }
    ty ret = probe_closure_ret(c, clo);
    if (c->err) return ty_unk();
    if (ret.k != T_UNK && ret.k != T_INT && ret.k != T_FLT && ret.k != T_BOOL && ret.k != T_STR
        && ret.k != T_STRUCT && ret.k != T_CLASS && ret.k != T_ENUM && ret.k != T_SUM && ret.k != T_ERR) {
        terr(c, "v1:任务返回值类型不支持(kind %d)", ret.k);
        return ty_unk();
    }
    char mk[64];
    task_mk(mk, sizeof mk, ret);
    rt_task_type(c, ret);
    int n = c->tmpn++;
    char tn[40], cx[56], pv[56];
    snprintf(tn, sizeof tn, "ctron_tt%d", n);
    snprintf(cx, sizeof cx, "%s_cx", tn);
    snprintf(pv, sizeof pv, "%s_p", tn);
    sb_f(o, "({ ctron_task_%s* %s = (ctron_task_%s*)calloc(1, sizeof(ctron_task_%s));\n", mk, tn, mk, mk);
    sb_f(o, "  ctron_task_ctx %s; ctron_task_ctx* %s = ctron_task_cur;\n", cx, pv);
    sb_f(o, "  %s.rec = (ctron_task_base*)%s; ctron_task_cur = &%s;\n", cx, tn, cx);
    sb_f(o, "  if (setjmp(%s.jb) == 0) {\n", cx);
    if (ret.k != T_UNK) {
        char dest[80];
        snprintf(dest, sizeof dest, "%s->result", tn);
        emit_inline_closure_body(c, clo, dest, o);
    } else {
        emit_inline_closure_body(c, clo, NULL, o);
    }
    if (!c->err) sb_f(o, "  } else { %s->panicked = 1; }\n  ctron_task_cur = %s;\n  %s; })", tn, pv, tn);
    return c->err ? ty_unk() : holder_ty(T_TASK, ret);
}
ty emit_task_call(tc* c, cexpr* e, const char* rn, sb* o) {
    ty ht;
    if (!scope_find(c, rn, &ht) || ht.k != T_TASK) { terr(c, "v1:join 目标需任务"); return ty_unk(); }
    ty ret = ctron_payload_ty(ht);
    const char* m = e->callee ? e->callee->mname : NULL;
    rt_task_type(c, ret);
    char mk[64];
    task_mk(mk, sizeof mk, ret);
    if (m && !strcmp(m, "join")) {
        if (e->nelems != 0) { terr(c, "v1:join 实参"); return ty_unk(); }
        sb_f(o, "ctron_join_%s(%s)", mk, rn);
        return ret;
    }
    if (m && !strcmp(m, "join_or")) {
        if (e->nelems != 0) { terr(c, "v1:join_or 实参"); return ty_unk(); }
        sb_f(o, "ctron_join_or_%s(%s)", mk, rn);
        char nm[96];
        snprintf(nm, sizeof nm, "ctron_res_%s_TP", mk);
        ty sum = ty_unk(); sum.k = T_SUM;
        sum.tname = ctron_arena_strndup(c->a, nm, strlen(nm));
        sum.ek = ret.k; sum.ebits = ret.bits; sum.eus = ret.us;
        if (ret.k == T_STRUCT || ret.k == T_ENUM || ret.k == T_CLASS) sum.tname = sum.tname;
        sum.ek2 = T_INT; sum.ebits2 = 32; sum.eus2 = 0; sum.tname2 = NULL;
        return sum;
    }
    terr(c, "v1:任务方法不支持:%s", m ? m : "?");
    return ty_unk();
}
ty emit_chan_call(tc* c, cexpr* e, const char* rn, sb* o) {
    ty ct;
    if (!scope_find(c, rn, &ct) || ct.k != T_CHAN) { terr(c, "v1:通道方法目标需 Channel"); return ty_unk(); }
    ty elem = ctron_payload_ty(ct);
    if (elem.k == T_UNK) { terr(c, "v1:通道载荷类型未知"); return ty_unk(); }
    const char* m = e->callee ? e->callee->mname : NULL;
    rt_chan_type(c, elem);
    char wl[64];
    chan_mk(wl, sizeof wl, elem);
    if (m && !strcmp(m, "send")) {
        if (e->nelems != 1) { terr(c, "v1:send 实参"); return ty_unk(); }
        sb a = {0};
        emit_expr(c, e->elems[0], &a);
        sb_f(o, "ctron_chan_%s_send(%s, %s)", wl, rn, a.d && *a.d ? a.d : "0");
        sb_free(&a);
    } else if (m && !strcmp(m, "recv")) {
        if (e->nelems != 0) { terr(c, "v1:recv 实参"); return ty_unk(); }
        sb_f(o, "ctron_chan_%s_recv(%s)", wl, rn);
    } else {
        terr(c, "v1:通道方法不支持:%s", m ? m : "?");
        return ty_unk();
    }
    if (c->err) return ty_unk();
    char nm[96];
    snprintf(nm, sizeof nm, "ctron_res_%s_ChanE", wl);
    ty sum = ty_unk(); sum.k = T_SUM;
    sum.tname = ctron_arena_strndup(c->a, nm, strlen(nm));
    sum.ek = elem.k; sum.ebits = elem.bits; sum.eus = elem.us;
    if (elem.k == T_STRUCT || elem.k == T_ENUM || elem.k == T_CLASS) sum.tname = sum.tname;
    sum.ek2 = T_INT; sum.ebits2 = 32; sum.eus2 = 0; sum.tname2 = NULL;
    return sum;
}
// Mutex[T].with(f) / .with_mut(|var it| …):参数复制入 + 闭包体内联 + (with_mut)复制出
ty emit_mutex_call(tc* c, cexpr* e, ty mt, const char* rn, sb* o) {
    if (mt.k != T_MUTEX) { terr(c, "v1:with 目标需 Mutex/Global"); return ty_unk(); }
    ty inner = ctron_payload_ty(mt);
    if (inner.k == T_UNK) { terr(c, "v1:Mutex 载荷类型未知"); return ty_unk(); }
    const char* m = e->callee ? e->callee->mname : NULL;
    int mut = m && !strcmp(m, "with_mut");
    if (!mut && (!m || strcmp(m, "with"))) { terr(c, "v1:Mutex 方法不支持:%s", m ? m : "?"); return ty_unk(); }
    if (e->nelems != 1 || !e->elems[0] || e->elems[0]->kind != EX_CLOSURE) {
        terr(c, "v1:%s 需单个闭包实参", m ? m : "with");
        return ty_unk();
    }
    const cexpr* clo = e->elems[0];
    if (clo->ncparams > 1) { terr(c, "v1:%s 闭包参数过多", m ? m : "with"); return ty_unk(); }
    const char* pn = (clo->ncparams >= 1 && clo->cparams[0].name) ? clo->cparams[0].name : "it";
    rt_mutex_type(c, inner);
    char mw[64];
    chan_mk(mw, sizeof mw, inner);
    int n = c->tmpn++;
    char mx[48];
    snprintf(mx, sizeof mx, "ctron_mx%d", n);
    sb_f(o, "({ ctron_mutex_%s* %s = %s;\n", mw, mx, rn);
    sb_f(o, "  %s %s = %s->v;\n", ctype_of(inner), pn, mx);
    scope_push(c);
    scope_def(c, pn, inner);
    ty rty = ty_unk();
    if (!mut) {
        rty = probe_closure_ret(c, clo);
        if (c->err) { scope_pop(c); return ty_unk(); }
        if (rty.k == T_UNK) { scope_pop(c); terr(c, "v1:with 返回类型无法推导"); return ty_unk(); }
        char wv[48];
        snprintf(wv, sizeof wv, "ctron_wv%d", n);
        sb_f(o, "  %s %s = 0;\n", ctype_of(rty), wv);
        emit_inline_closure_body(c, clo, wv, o);
        scope_pop(c);
        if (!c->err) sb_f(o, "  %s; })", wv);
        return c->err ? ty_unk() : rty;
    }
    emit_inline_closure_body(c, clo, NULL, o);
    scope_pop(c);
    if (!c->err) sb_f(o, "  %s->v = %s;\n  0; })", mx, pn);
    return ty_unk();
}
// Atomic 单元格方法:load / store / fetch_add(对齐 rt V_ATOM 语义;与 with/with_mut 共用 ctron_mutex_* 单元格)
ty emit_atomic_call(tc* c, cexpr* e, ty mt, const char* rn, sb* o) {
    if (mt.k != T_MUTEX) { terr(c, "v1:原子目标需 Atomic/Global"); return ty_unk(); }
    ty inner = ctron_payload_ty(mt);
    if (inner.k == T_UNK) { terr(c, "v1:原子载荷类型未知"); return ty_unk(); }
    const char* m = e->callee ? e->callee->mname : NULL;
    rt_mutex_type(c, inner);
    char mw[64];
    chan_mk(mw, sizeof mw, inner);
    if (!strcmp(m, "load")) {
        if (e->nelems != 0) { terr(c, "v1:load 实参"); return ty_unk(); }
        sb_f(o, "ctron_mutex_%s_load(%s)", mw, rn);
        return inner;
    }
    if (!strcmp(m, "store")) {
        if (e->nelems != 1) { terr(c, "v1:store 实参"); return ty_unk(); }
        sb a1 = {0};
        emit_expr(c, e->elems[0], &a1);
        sb_f(o, "ctron_mutex_%s_store(%s, %s)", mw, rn, a1.d ? a1.d : "0");
        sb_free(&a1);
        return ty_unk();
    }
    // fetch_add:返回旧值;加法 __int128 承载后回写(截断语义对齐 rt)
    if (e->nelems != 1) { terr(c, "v1:fetch_add 实参"); return ty_unk(); }
    sb a1 = {0};
    emit_expr(c, e->elems[0], &a1);
    sb_f(o, "ctron_mutex_%s_fetch_add(%s, %s)", mw, rn, a1.d ? a1.d : "0");
    sb_free(&a1);
    return inner;
}
// parallel.map(切片, fn):序贯形态(对齐 rt);单次求值经语句表达式局部变量
ty emit_parallel_map(tc* c, cexpr* e, sb* o) {
    ty at = ty_unk(), ft = ty_unk();
    sb ab = {0}, fb = {0};
    at = emit_expr(c, e->elems[0], &ab);
    ft = emit_expr(c, e->elems[1], &fb);
    if (c->err) { sb_free(&ab); sb_free(&fb); return ty_unk(); }
    if (at.k != T_ARR || ft.k != T_FNPTR) {
        terr(c, "v1:parallel.map 需(切片, fn)");
        sb_free(&ab); sb_free(&fb);
        return ty_unk();
    }
    ty el = e2_of(at);
    use_arr(c, wlname(el));
    const char* awl = wlname(el);
    int n = c->tmpn++;
    char pa[40], pf[40];
    snprintf(pa, sizeof pa, "ctron_pma%d", n);
    snprintf(pf, sizeof pf, "ctron_pmf%d", n);
    sb_f(o, "({ ctron_arr_%s %s = %s; ctron_fnptr %s = %s;\n", awl, pa, ab.d ? ab.d : "{0}", pf, fb.d ? fb.d : "0");
    sb_f(o, "  ctron_arr_%s ctron_pmr%d; ctron_pmr%d.n = %s.n; ctron_pmr%d.d = (%s*)calloc((size_t)(%s.n ? %s.n : 1), sizeof(%s));\n",
         awl, n, n, pa, n, ctype_of(el), pa, pa, ctype_of(el));
    sb_f(o, "  for (int64_t ctron_pi = 0; ctron_pi < %s.n; ctron_pi++) ctron_pmr%d.d[ctron_pi] = (%s)((ctron_fnptr)(%s))((int64_t)(%s.d[ctron_pi]));\n",
         pa, n, ctype_of(el), pf, pa);
    sb_f(o, "  ctron_pmr%d; })", n);
    sb_free(&ab); sb_free(&fb);
    return ty_arr(el);
}
// parallel.reduce(切片, 初值, fn):序贯折叠(对齐 rt)
ty emit_parallel_reduce(tc* c, cexpr* e, sb* o) {
    ty at = ty_unk(), it = ty_unk(), ft = ty_unk();
    sb ab = {0}, ib = {0}, fb = {0};
    at = emit_expr(c, e->elems[0], &ab);
    it = emit_expr(c, e->elems[1], &ib);
    ft = emit_expr(c, e->elems[2], &fb);
    if (c->err) { sb_free(&ab); sb_free(&ib); sb_free(&fb); return ty_unk(); }
    if (at.k != T_ARR || ft.k != T_FNPTR) {
        terr(c, "v1:parallel.reduce 需(切片, 初值, fn)");
        sb_free(&ab); sb_free(&ib); sb_free(&fb);
        return ty_unk();
    }
    ty el = e2_of(at);
    use_arr(c, wlname(el));
    const char* awl = wlname(el);
    int n = c->tmpn++;
    char pa[40], pf[40], ac[40];
    snprintf(pa, sizeof pa, "ctron_pra%d", n);
    snprintf(pf, sizeof pf, "ctron_prf%d", n);
    snprintf(ac, sizeof ac, "ctron_prc%d", n);
    sb_f(o, "({ ctron_arr_%s %s = %s; int64_t %s = (int64_t)(%s); ctron_fnptr %s = %s;\n",
         awl, pa, ab.d ? ab.d : "{0}", ac, ib.d ? ib.d : "0", pf, fb.d ? fb.d : "0");
    sb_f(o, "  for (int64_t ctron_pi = 0; ctron_pi < %s.n; ctron_pi++) %s = ((ctron_fnptr)(%s))(%s, (int64_t)(%s.d[ctron_pi]));\n",
         pa, ac, pf, ac, pa);
    sb_f(o, "  %s; })", ac);
    sb_free(&ab); sb_free(&ib); sb_free(&fb);
    return ty_int(64, 0);
}
// scope { |s| … }:值 = 块尾(GNU 语句表达式;块尾匹配/if 走语句提升)
ty emit_scope_expr(tc* c, cexpr* e, sb* o) {
    if (!e || !e->sbody) { terr(c, "v1:scope 缺体"); return ty_unk(); }
    ty vty = ty_unk();
    {
        scope_push(c);
        ty st = ty_unk(); st.k = T_SCOPE;
        if (e->sparam) scope_def(c, e->sparam, st);
        for (size_t i = 0; i < e->sbody->nstmts; i++) { sb d = {0}; emit_stmt(c, e->sbody->stmts[i], &d); sb_free(&d); if (c->err) break; }
        if (!c->err && e->sbody->tail) vty = probe_val_ty(c, e->sbody->tail);
        scope_pop(c);
    }
    if (c->err) return ty_unk();
    int n = c->tmpn++;
    char sn[48], vs[48];
    snprintf(sn, sizeof sn, "ctron_sc%d", n);
    snprintf(vs, sizeof vs, "ctron_sv%d", n);
    sb_f(o, "({ ctron_scope %s; memset(&%s, 0, sizeof %s);\n", sn, sn, sn);
    sb_f(o, "  ctron_scope* ctron_sp%d = ctron_scope_cur; ctron_scope_cur = &%s;\n", n, sn);
    scope_push(c);
    ty st = ty_unk(); st.k = T_SCOPE;
    if (e->sparam) {
        scope_def(c, e->sparam, st);
        sb_f(o, "  ctron_scope* %s = &%s;\n", e->sparam, sn);
    }
    int have_val = (e->sbody->tail && vty.k != T_UNK);
    // sv 声明在内层块之外:语句表达式尾部读取发生在块闭合后(同层可见)
    if (have_val) sb_f(o, "  %s %s = {0};\n", ctype_of(vty), vs);
    sb_s(o, "  {\n");
    for (size_t i = 0; i < e->sbody->nstmts && !c->err; i++) emit_stmt(c, e->sbody->stmts[i], o);
    if (!c->err && have_val) {
        emit_tail_to(c, e->sbody->tail, vs, o);
    } else if (!c->err && e->sbody->tail) {
        emit_effect_expr(c, e->sbody->tail, o);
    }
    scope_pop(c);
    sb_s(o, "  }\n");
    sb_f(o, "  ctron_scope_cur = ctron_sp%d;\n", n);
    if (have_val) sb_f(o, "  %s; })", vs);
    else sb_f(o, "  0; })");
    return c->err ? ty_unk() : vty;
}
