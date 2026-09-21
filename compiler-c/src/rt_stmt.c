#include "rt_internal.h"

// rt_stmt.c —— 语句/块求值 + 入口(test 块 / main / runner)(C4)
// ================= 语句 =================

void bind_value(rt* R, cpat* p, val v) {
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

void eval_let(rt* R, cstmt* st) {
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

void eval_stmt(rt* R, cstmt* st) {
    if (!st) return;
    switch (st->kind) {
    case ST_LET: eval_let(R, st); break;
    case ST_RET: R->ret = st->e ? eval_expr(R, st->e) : v_void(); R->has_ret = 1; break;
    case ST_BREAK: R->has_brk = 1; break;
    case ST_CONTINUE: R->has_cont = 1; break;
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
                size_t lim = cont->k == V_LIST ? cont->lst->n : cont->nitems;
                if (idx < 0 || (unsigned long long)idx >= lim)
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
                if (R->has_brk) { R->has_brk = 0; break; }
                R->has_cont = 0;
            }
        } else if (it.k == V_ARR || it.k == V_TUPLE) {
            for (size_t i = 0; i < it.nitems; i++) {
                if (iv) iv->slot = it.items[i];
                (void)eval_block(R, st->body);
                if (R->has_ret) break;
                if (R->has_brk) { R->has_brk = 0; break; }
                R->has_cont = 0;
            }
        } else {
            env_pop(R);
            rt_abort(R, RT_ERROR, "for 需要 range 或数组");
        }
        env_pop(R);
        break;
    }
    case ST_WHILE: {
        R->has_brk = 0;
        R->has_cont = 0;
        while (!R->has_ret && !R->has_brk) {
            val c = eval_expr(R, st->e);
            if (!truthy(c)) break;
            (void)eval_block(R, st->body);
            R->has_cont = 0;
        }
        R->has_brk = 0;
        R->has_cont = 0;
        break;
    }
    }
}

// ================= 块 =================
// Drop impl 探测与作用域退出逆序 drop(RAII,§6.4)
int type_has_drop(const rt* R, const char* ty) {
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
static void drop_frame(rt* R, env* f) {
    for (bind* b = f->head; b; b = b->next) {
        if (b->slot.k != V_STRUCT || !type_has_drop(R, b->slot.type)) continue;
        const cfn* F = cls_method(R, b->slot.type, "drop");
        if (F && F->body) (void)call_method_body(R, F, b->slot, NULL, 0);
    }
    // bind 节点入复用链(env_let/env_pop 的 free-list;bump arena 无逐对象回收,
    // 帧销毁即节点可复用——循环体逐轮重绑 let/var 的分配主源)
    for (bind* b = f->head; b; ) {
        bind* nx = b->next;
        b->next = R->bind_free;
        R->bind_free = b;
        b = nx;
    }
    f->head = NULL; // 展开后清空:嵌套 panic 不重复展开(幂等)
}

void drop_scope(rt* R) {
    if (R->top) drop_frame(R, R->top);
}

// P1-A2 镜像(§6.4):panic 沿帧链自内向外逆序展开 Drop
void rt_panic_unwind(rt* R) {
    for (env* f = R->top; f; f = f->up) drop_frame(R, f);
}

val eval_block(rt* R, cblock* b) {
    val tail = v_void();
    if (!b) return tail;
    env_push(R);
    for (size_t i = 0; i < b->nstmts && !R->has_ret && !R->has_brk && !R->has_cont; i++)
        eval_stmt(R, b->stmts[i]);
    // v0.7 修订二:has_brk/has_cont 同样终止块内后继语句与尾表达式;
    // drop_scope 照跑(RAII 不受控制流影响)
    if (!R->has_ret && !R->has_brk && !R->has_cont && b->tail) tail = eval_expr(R, b->tail);
    drop_scope(R); // 本帧声明逆序 drop
    env_pop(R);
    return R->has_ret ? R->ret : tail;
}

// ================= 入口:运行 test 块 =================
rt_run ctron_rt_run(const cfile* f) {
    rt_run out = {0};
    ctron_arena* arena = ctron_arena_new();
    rt R = {0};
    R.max_steps = rt_env_steps();
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

    eval_consts(&R);
    for (size_t i = 0; i < f->ndecls; i++) {
        const cdecl* d = &f->decls[i];
        if (d->kind != D_TEST) continue;
        R.tests_run++;
        R.top = NULL;
        R.has_ret = 0;
        (void)eval_block(&R, d->test.body);
    }
    out.tests_run = R.tests_run;
    out.out = R.out ? strdup(R.out) : NULL; // test 块内 println 输出此前被丢弃
    ctron_arena_free(arena);
    return out;
}

rt_run ctron_rt_run_main(const cfile* f) {
    ctron_arena* arena = ctron_arena_new();
    rt R = {0};
    R.max_steps = rt_env_steps();
    R.f = f;
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
    eval_consts(&R);
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
