#include "trans_internal.h"

// trans_stmt.c —— if/match 值发射 + 语句/块/函数体发射(C10-c/f)
// ================= if 作为值(C10-f)=================
void emit_if_assign(tc* c, cexpr* e, const char* rn, sb* o) {
    if (c->err || !e) return;
    sb cond = {0};
    emit_expr(c, e->cond, &cond);
    sb_f(o, "if (%s) {", cond.d ? cond.d : "0");
    sb_free(&cond);
    scope_push(c);
    if (e->then_b) {
        for (size_t i = 0; i < e->then_b->nstmts; i++) emit_stmt(c, e->then_b->stmts[i], o);
        if (e->then_b->tail) emit_value_to(c, e->then_b->tail, rn, o);
    }
    scope_pop(c);
    if (e->els && e->els->kind == EX_IF) {
        sb_s(o, " } else ");
        emit_if_assign(c, e->els, rn, o);
        return;
    }
    if (e->els && e->els->kind == EX_BLOCK) {
        sb_s(o, " } else {");
        scope_push(c);
        cblock* bl = e->els->block;
        for (size_t i = 0; bl && i < bl->nstmts; i++) emit_stmt(c, bl->stmts[i], o);
        if (bl && bl->tail) emit_value_to(c, bl->tail, rn, o);
        scope_pop(c);
        sb_s(o, "}\n");
        return;
    }
    sb_s(o, "}\n");
}

// ================= match(C10-c)=================
// 支持:字面量(int/bool/str)/通配/标识符绑定/单元变体/struct 模式(字段绑定+字面量);
// 位置:return / let / 语句。变体载荷(tuple variant)与 Option/Result → C10-d。
void emit_value_to(tc* c, cexpr* ax, const char* rn, sb* o) {
    if (c->err || !ax) return;
    if (ax->kind == EX_IF) {
        sb cond = {0};
        emit_expr(c, ax->cond, &cond);
        sb_f(o, "if (%s) {", cond.d ? cond.d : "0");
        sb_free(&cond);
        scope_push(c);
        if (ax->then_b) {
            for (size_t i = 0; i < ax->then_b->nstmts; i++) emit_stmt(c, ax->then_b->stmts[i], o);
            if (ax->then_b->tail) emit_value_to(c, ax->then_b->tail, rn, o);
        }
        scope_pop(c);
        if (ax->els && ax->els->kind == EX_BLOCK) {
            sb_s(o, " } else {");
            scope_push(c);
            cblock* bl = ax->els->block;
            for (size_t i = 0; bl && i < bl->nstmts; i++) emit_stmt(c, bl->stmts[i], o);
            if (bl && bl->tail) emit_value_to(c, bl->tail, rn, o);
            scope_pop(c);
            sb_s(o, "}\n");
        } else if (ax->els && ax->els->kind == EX_IF) {
            sb_s(o, " } else ");
            emit_value_to(c, ax->els, rn, o);
        } else {
            sb_s(o, "}\n");
        }
        return;
    }
    if (ax->kind == EX_BLOCK) {
        scope_push(c);
        cblock* bl = ax->block;
        for (size_t i = 0; bl && i < bl->nstmts; i++) emit_stmt(c, bl->stmts[i], o);
        if (bl && bl->tail) emit_value_to(c, bl->tail, rn, o);
        scope_pop(c);
        return;
    }
    if (ax->kind == EX_CALL && ax->callee && ax->callee->kind == EX_IDENT
        && ax->callee->text && !strcmp(ax->callee->text, "panic")) {
        // panic 为 Never:值位置只发射 panic 语句本身(不可达,dest 由 calloc/初始化器置零)
        sb t2 = {0};
        emit_expr(c, ax, &t2);
        sb_f(o, "%s;\n", t2.d ? t2.d : "");
        sb_free(&t2);
        return;
    }
    sb t2 = {0};
    emit_expr(c, ax, &t2);
    sb_f(o, "%s = %s;\n", rn, t2.d ? t2.d : "0");
    sb_free(&t2);
}

ty emit_match(tc* c, cexpr* e, sb* o, int want_value, char** out_tmp) {
    if (c->err) return ty_unk();
    sb sc = {0};
    ty st = emit_expr(c, e->scrut, &sc);
    if (c->err) { sb_free(&sc); return ty_unk(); }
    int n = c->tmpn++;
    char vn[32], rn[32];
    snprintf(vn, sizeof vn, "ctron_m%d", n);
    snprintf(rn, sizeof rn, "ctron_mr%d", n);
    ty val_t = ty_unk();
    // 先干跑各臂求值,取值类型(表达式纯,双发射安全)
    if (want_value && e->narms > 0) {
        sb t0 = {0};
        scope_push(c);
        if (e->arms[0].expr->kind == EX_BLOCK) {
            // 块臂:尾表达式类型
            cblock* bl = e->arms[0].expr->block;
            for (size_t i = 0; bl && i < bl->nstmts; i++) { sb d = {0}; emit_stmt(c, bl->stmts[i], &d); sb_free(&d); }
            if (bl && bl->tail) val_t = emit_expr(c, bl->tail, &t0);
        } else val_t = emit_expr(c, e->arms[0].expr, &t0);
        scope_pop(c);
        sb_free(&t0);
        if (val_t.k != T_UNK) {
            sb_f(o, "%s %s = {0};\n", ctype_of(val_t), rn); // 结果变量在 match 块外(return/let 可见)
        } else want_value = 0;
    }
    if (want_value) want_value = 1;

    sb_f(o, "{ %s %s = %s;\n", ctype_of(st), vn, sc.d ? sc.d : "0");
    sb_free(&sc);
    int emitted_catch = 0;
    const char* pbind_name = NULL;
    ty pbind_ty = ty_unk();
    const char* pbind_acc = NULL;
    for (size_t i = 0; i < e->narms && !c->err; i++) {
        cpat* p = e->arms[i].pat;
        sb cond = {0};
        int need_cond = 1;
        if (p->kind == PAT_WILD || p->kind == PAT_IDENT) need_cond = 0;
        else if (p->kind == PAT_LIT) {
            if (p->lkind == PLIT_INT) sb_f(&cond, "((__int128)%s == (__int128)(int64_t)(%s))", vn, p->name);
            else if (p->lkind == PLIT_BOOL) sb_f(&cond, "(%s == %d)", vn, p->lb ? 1 : 0);
            else if (p->lkind == PLIT_STR) { use_helper(c, "ctron_str_cmp"); sb_f(&cond, "(ctron_str_cmp(%s, \"%s\") == 0)", vn, p->name ? p->name : ""); }
            else { terr(c, "v1:浮点字面量模式不支持"); sb_free(&cond); return ty_unk(); }
        } else if (p->kind == PAT_AGG && p->agg == AG_TUPLE && p->npath > 0
                   && (st.k == T_SUM || st.k == T_ENUM)) {
            // 载荷变体:Some(x) / Ok(_) / Err(e) / Err(DivByZero) / Advance(v)
            const char* p0 = p->path[0];
            const char* good = NULL; const char* mem = NULL;
            ty pt = ty_unk();
            if (st.k == T_SUM) {
                int is_opt2 = !strncmp(st.tname, "ctron_opt_", 10);
                if (is_opt2 && !strcmp(p0, "Some")) { good = "CTRON_OPT_SOME"; mem = "some"; }
                else if (!is_opt2 && !strcmp(p0, "Ok")) { good = "CTRON_RES_OK"; mem = "ok"; }
                else if (!is_opt2 && !strcmp(p0, "Err")) { good = "CTRON_RES_ERR"; mem = "err"; }
                else { terr(c, "v1:和类型变体不符:%s", p0); sb_free(&cond); return ty_unk(); }
                if (!strcmp(p0, "Err")) {
                    pt = ty_unk(); pt.k = st.ek2; pt.bits = st.ebits2; pt.us = st.eus2;
                    pt.tname = st.tname2;
                } else {
                    pt = ty_unk(); pt.k = st.ek; pt.bits = st.ebits; pt.us = st.eus;
                    if (st.ek == T_FLT) pt = ty_flt();
                    if (st.ek == T_BOOL) pt = ty_bool();
                    if (st.ek == T_STR) pt = ty_str();
                }
                sb_f(&cond, "(%s.tag == %s)", vn, good);
            } else {
                edef* ed = &c->enums[st.bits];
                evar* ev = NULL;
                for (size_t j = 0; j < ed->n; j++)
                    if (!strcmp(ed->variants[j].name, p0)) { ev = &ed->variants[j]; break; }
                if (!ev) { terr(c, "v1:未知变体:%s", p0); sb_free(&cond); return ty_unk(); }
                pt = ev->pty;
                sb_f(&cond, "(%s.tag == CTRON_%s_%s)", vn, ed->name, p0);
                mem = NULL;
            }
            // 载荷子模式
            if (p->nelems >= 1 && p->elems[0]) {
                cpat* sub = p->elems[0];
                char acc[192];
                if (st.k == T_SUM) snprintf(acc, sizeof acc, "%s.as.%s", vn, mem);
                else snprintf(acc, sizeof acc, "%s.as.u_%s", vn, p0);
                if (sub->kind == PAT_WILD) { /* 无条件 */ }
                else if (sub->kind == PAT_IDENT) {
                    pbind_name = sub->name;
                    pbind_ty = pt;
                    pbind_acc = ctron_arena_strndup(c->a, acc, strlen(acc));
                } else if (sub->kind == PAT_AGG && sub->agg == AG_UNIT && pt.k == T_ENUM) {
                    sb_f(&cond, " && %s.tag == CTRON_%s_%s", acc, pt.tname, sub->path[0]);
                } else if (sub->kind == PAT_LIT && sub->lkind == PLIT_INT) {
                    sb_f(&cond, " && (%s == (int64_t)(%s))", acc, sub->name);
                } else { terr(c, "v1:载荷子模式不支持"); sb_free(&cond); return ty_unk(); }
            }
            if (p->nelems > 1) { terr(c, "v1:多载荷变体不支持"); sb_free(&cond); return ty_unk(); }
        } else if (p->kind == PAT_AGG && p->agg == AG_UNIT && p->npath > 0 && st.k == T_SUM) {
            // None(和类型单元模式)
            const char* p0 = p->path[0];
            int is_opt = !strncmp(st.tname, "ctron_opt_", 10);
            if (is_opt && !strcmp(p0, "None")) sb_f(&cond, "(%s.tag == CTRON_OPT_NONE)", vn);
            else { terr(c, "v1:和类型单元模式不支持:%s", p0); sb_free(&cond); return ty_unk(); }
        } else if (p->kind == PAT_AGG && p->agg == AG_UNIT && p->npath > 0 && st.k == T_ENUM) {
            edef* ed = &c->enums[st.bits];
            sb_f(&cond, "(%s.tag == CTRON_%s_%s)", vn, ed->name, p->path[0]);
        } else if (p->kind == PAT_AGG && p->agg == AG_STRUCT && st.k == T_STRUCT) {
            for (size_t j = 0; j < p->nsfields; j++) {
                const cstructpatfield* f = &p->sfields[j];
                if (!f->pat) continue; // 绑定字段 → 条件外处理
                if (f->pat->kind == PAT_LIT && f->pat->lkind == PLIT_INT) {
                    sb_f(&cond, "%s(%s.%s == (int64_t)(%s))", j ? "" : "", vn, f->name, f->pat->name);
                } else { terr(c, "v1:struct 模式字段子模式不支持"); sb_free(&cond); return ty_unk(); }
            }
        } else {
            terr(c, "v1:该模式不支持(kind %d agg %d path %s scrut %d/%s)", (int)p->kind, (int)p->agg,
                 p->npath > 0 ? p->path[0] : "-", (int)st.k, st.tname ? st.tname : "-");
            sb_free(&cond);
            return ty_unk();
        }

        if (need_cond) {
            if (i > 0 && !emitted_catch) sb_s(o, " else ");
            sb_f(o, "if (%s) ", cond.d ? cond.d : "1");
        } else if (i > 0) {
            sb_s(o, " else ");
        }
        sb_free(&cond);
        sb_s(o, "{\n    ");
        scope_push(c);
        // 绑定
        if (p->kind == PAT_IDENT && p->name) {
            scope_def(c, p->name, st);
            sb_f(o, "%s %s = %s;\n    ", ctype_of(st), p->name, vn);
        }
        if (pbind_name) {
            scope_def(c, pbind_name, pbind_ty);
            sb_f(o, "%s %s = %s;\n    ", ctype_of(pbind_ty), pbind_name, pbind_acc);
            pbind_name = NULL;
        }
        if (p->kind == PAT_AGG && p->agg == AG_STRUCT && st.k == T_STRUCT) {
            sdef* sd = &c->structs[st.bits];
            for (size_t j = 0; j < p->nsfields; j++) {
                const cstructpatfield* f = &p->sfields[j];
                if (f->pat) continue;
                for (size_t k2 = 0; k2 < sd->n; k2++)
                    if (!strcmp(sd->fields[k2].name, f->name)) {
                        scope_def(c, f->name, sd->fields[k2].t);
                        sb_f(o, "%s %s = %s.%s;\n    ", ctype_of(sd->fields[k2].t), f->name, vn, f->name);
                    }
            }
        }
        // 臂体
        if (want_value) {
            emit_value_to(c, e->arms[i].expr, rn, o);
        } else {
            cexpr* ax = e->arms[i].expr;
            if (ax->kind == EX_BLOCK) {
                emit_block(c, ax->block, o);
            } else {
                sb t2 = {0};
                emit_expr(c, ax, &t2);
                sb_f(o, "%s;\n    ", t2.d ? t2.d : "");
                sb_free(&t2);
            }
        }
        scope_pop(c);
        sb_s(o, "}\n");
        if (p->kind == PAT_WILD || p->kind == PAT_IDENT) emitted_catch = 1;
    }
    if (!emitted_catch && !c->err)
        sb_f(o, "    else ctron_panic(\"match 无匹配臂\");\n");
    if (want_value && out_tmp) *out_tmp = ctron_arena_strndup(c->a, rn, strlen(rn));
    sb_s(o, "}\n");
    return val_t;
}

// ================= 语句 =================
// if 语句(if/else-if 链/else;作为表达式值 = v1 不支持)
void emit_if_stmt(tc* c, cexpr* e, sb* o) {
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

void emit_stmt(tc* c, cstmt* st, sb* o) {
    if (c->err || !st) return;
    switch (st->kind) {
    case ST_LET: {
        if (st->pat && st->pat->kind == PAT_WILD) {
            // let _ = expr:求值(保留 panic 副作用)后丢弃
            const ty* saved_w = c->want;
            c->want = NULL;
            sb d = {0};
            if (st->e) emit_expr(c, st->e, &d);
            c->want = saved_w;
            sb_f(o, "(void)(%s);\n", d.d ? d.d : "0");
            sb_free(&d);
            return;
        }
        if (st->pat && st->pat->kind == PAT_TUPLE) {
            // 元组解构:Channel[T](cap) 双端(eager 同指针)或一般二元组(C10-p)
            int is_chan = st->e && st->e->kind == EX_CALL && st->e->callee
                && st->e->callee->kind == EX_TYPEARGS && st->e->callee->ntargs == 1
                && st->e->callee->obj && st->e->callee->obj->kind == EX_IDENT
                && !strcmp(st->e->callee->obj->text, "Channel") && st->e->nelems == 1;
            if (!is_chan && st->pat->nelems == 2 && st->pat->elems && st->pat->elems[0]
                && st->pat->elems[0]->kind == PAT_IDENT && st->pat->elems[0]->name
                && st->pat->elems[1] && st->pat->elems[1]->kind == PAT_IDENT
                && st->pat->elems[1]->name) {
                // 一般二元组解构:RHS 单次求值进临时,再按元素绑定
                if (is_reserved(st->pat->elems[0]->name) || is_reserved(st->pat->elems[1]->name)) {
                    terr(c, "v1:标识符保留前缀 ctron_");
                    return;
                }
                sb rhs = {0};
                ty tt = emit_expr(c, st->e, &rhs);
                if (c->err) { sb_free(&rhs); return; }
                if (tt.k != T_TUP) { terr(c, "v1:解构目标需二元组"); sb_free(&rhs); return; }
                int n5 = c->tmpn++;
                char tn2[32];
                snprintf(tn2, sizeof tn2, "ctron_dt%d", n5);
                sb_f(o, "%s %s = %s;\n", ctype_of(tt), tn2, rhs.d ? rhs.d : "0");
                sb_free(&rhs);
                for (int ix = 0; ix < 2; ix++) {
                    ty et = tup_elem(tt, ix);
                    const char* nm2 = st->pat->elems[ix]->name;
                    sb_f(o, "%s %s = %s.e%d;\n", ctype_of(et), nm2, tn2, ix);
                    scope_def(c, nm2, et);
                }
                return;
            }
            if (st->pat->nelems != 2 || !st->pat->elems || !st->pat->elems[0] || !st->pat->elems[1]
                || st->pat->elems[0]->kind != PAT_IDENT || !st->pat->elems[0]->name
                || st->pat->elems[1]->kind != PAT_IDENT || !st->pat->elems[1]->name) {
                terr(c, "v1:元组解构仅支持 (a, b) 双标识符");
                return;
            }
            if (!is_chan) {
                terr(c, "v1:元组解构 RHS 仅支持 Channel[T](cap)");
                return;
            }
            ty elem = decl_ty_tc(c, st->e->callee->targs[0]);
            if (elem.k == T_UNK) { terr(c, "v1:Channel 载荷类型不支持"); return; }
            rt_chan_type(c, elem);
            char wl[64];
            chan_mk(wl, sizeof wl, elem);
            if (is_reserved(st->pat->elems[0]->name) || is_reserved(st->pat->elems[1]->name)) {
                terr(c, "v1:标识符保留前缀 ctron_");
                return;
            }
            sb cap = {0};
            emit_expr(c, st->e->elems[0], &cap);
            if (c->err) { sb_free(&cap); return; }
            char tmp[40];
            snprintf(tmp, sizeof tmp, "ctron_cn%d", c->tmpn++);
            sb_f(o, "ctron_chan_%s* %s = ctron_chan_%s_new((int64_t)(%s));\n", wl, tmp, wl, cap.d && *cap.d ? cap.d : "0");
            sb_free(&cap);
            sb_f(o, "ctron_chan_%s* %s = %s;\n", wl, st->pat->elems[0]->name, tmp);
            sb_f(o, "ctron_chan_%s* %s = %s;\n", wl, st->pat->elems[1]->name, tmp);
            scope_def(c, st->pat->elems[0]->name, holder_ty(T_CHAN, elem));
            scope_def(c, st->pat->elems[1]->name, holder_ty(T_CHAN, elem));
            return;
        }
        if (!st->pat || st->pat->kind != PAT_IDENT || !st->pat->name) {
            terr(c, "v1:let 仅支持标识符/通配模式");
            return;
        }
        const char* name = st->pat->name;
        if (is_reserved(name)) { terr(c, "v1:标识符保留前缀 ctron_:%s", name); return; }
        if (!st->e) { terr(c, "v1:let 缺初值"); return; }
        ty ann = decl_ty_tc(c, st->ty);
        const ty* saved_let_want = c->want;
        c->want = (ann.k != T_UNK) ? &ann : NULL;
        if (st->e->kind == EX_TRY) {
            // let x = expr? —— None/Err 提前 return(rt 语义)
            sb op = {0};
            ty ot = emit_expr(c, st->e->obj, &op);
            c->want = saved_let_want;
            if (c->err) { sb_free(&op); return; }
            if (ot.k != T_SUM) { terr(c, "v1:? 需 Option/Result"); sb_free(&op); return; }
            int is_opt = !strncmp(ot.tname, "ctron_opt_", 10);
            const char* bad = is_opt ? "CTRON_OPT_NONE" : "CTRON_RES_ERR";
            const char* mem = is_opt ? "some" : "ok";
            ty vt = ty_unk(); vt.k = ot.ek; vt.bits = ot.ebits; vt.us = ot.eus;
            if (ot.ek == T_FLT) vt = ty_flt();
            if (ot.ek == T_BOOL) vt = ty_bool();
            if (ot.ek == T_STR) vt = ty_str();
            int n2 = c->tmpn++;
            char tn[32];
            snprintf(tn, sizeof tn, "ctron_t%d", n2);
            sb_f(o, "%s %s = %s;\n", ctype_of(ot), tn, op.d ? op.d : "0");
            int ret_diff_sum = (c->fn_ret && c->fn_ret->k == T_SUM && strcmp(c->fn_ret->tname, ot.tname) != 0);
            if (c->in_test) sb_f(o, "if (%s.tag == %s) return;\n", tn, bad);
            else if (c->in_main) sb_f(o, "if (%s.tag == %s) exit(0);\n", tn, bad);
            else if (c->fn_ret && c->fn_ret->k == T_SUM && !ret_diff_sum)
                sb_f(o, "if (%s.tag == %s) return %s;\n", tn, bad, tn);
            else if (c->fn_ret && c->fn_ret->k == T_SUM && ret_diff_sum && is_opt)
                sb_f(o, "if (%s.tag == %s) return (%s){ .tag = CTRON_OPT_NONE };\n", tn, bad, c->fn_ret->tname);
            else if (c->fn_ret && c->fn_ret->k == T_SUM && !is_opt
                     && c->fn_ret->ek2 == T_ERR && ot.ek2 != T_ERR && ot.tname2) {
                // 两段式擦除:fn 错误目标为 AnyError,? 操作数错误为具体类型 → 物化链后传播(rt §5.3)
                char efn[96];
                snprintf(efn, sizeof efn, "ctron_erase_e_%s", ot.tname2);
                char ebody[512];
                snprintf(ebody, sizeof ebody,
                    "static ctron_anyerr* %s(ctron_e_%s v) {\n"
                    "    ctron_anyerr* n = (ctron_anyerr*)calloc(1, sizeof(ctron_anyerr));\n"
                    "    n->message = strdup(\"\");\n"
                    "    n->cause = (ctron_anyerr*)calloc(1, sizeof(ctron_anyerr));\n"
                    "    n->trace = strdup(\"main:1\");\n"
                    "    (void)v; return n;\n}\n", efn, ot.tname2);
                use_sum(c, ebody);
                sb_f(o, "if (%s.tag == %s) return (%s){ .tag = CTRON_RES_ERR, .as.err = %s(%s.as.err) };\n",
                     tn, bad, c->fn_ret->tname, efn, tn);
            } else { terr(c, "v1:? 早退类型与函数返回类型不符"); sb_free(&op); return; }
            sb_f(o, "%s %s = %s.as.%s;\n", ctype_of(vt), name, tn, mem);
            sb_free(&op);
            scope_def(c, name, vt);
            return;
        }
        if (st->e->kind == EX_MATCH) {
            char* rn = NULL;
            ty mt = emit_match(c, st->e, o, 1, &rn);
            ty t2 = (ann.k != T_UNK) ? ann : mt;
            if (t2.k == T_UNK) { terr(c, "v1:无法推导 %s 的类型 @L%d", name, __LINE__); return; }
            sb_f(o, "%s %s = %s;\n", ctype_of(t2), name, rn ? rn : "0");
            scope_def(c, name, t2);
            return;
        }
        if (st->e->kind == EX_IF || st->e->kind == EX_BLOCK) {
            // let x = if/块 值:先声明(类型从 then 尾干跑推导),再 if/else 链赋值
            const ty* saved_w2 = c->want;
            c->want = NULL;
            ty vt = ty_unk();
            {
                sb d0 = {0};
                scope_push(c);
                if (st->e->kind == EX_IF && st->e->then_b) {
                    for (size_t i = 0; i < st->e->then_b->nstmts; i++) emit_stmt(c, st->e->then_b->stmts[i], &d0);
                    if (st->e->then_b->tail) vt = emit_expr(c, st->e->then_b->tail, &d0);
                } else if (st->e->kind == EX_BLOCK && st->e->block) {
                    for (size_t i = 0; i < st->e->block->nstmts; i++) emit_stmt(c, st->e->block->stmts[i], &d0);
                    if (st->e->block->tail) vt = emit_expr(c, st->e->block->tail, &d0);
                }
                scope_pop(c);
                sb_free(&d0);
            }
            c->want = saved_w2;
            if (vt.k == T_UNK) { terr(c, "v1:无法推导 %s 的类型 @L%d", name, __LINE__); return; }
            int n3 = c->tmpn++;
            char rn[32];
            snprintf(rn, sizeof rn, "ctron_v%d", n3);
            sb_f(o, "%s %s = 0;\n", ctype_of(vt), rn);
            if (st->e->kind == EX_IF) emit_if_assign(c, st->e, rn, o);
            else emit_value_to(c, st->e, rn, o);
            sb_f(o, "%s %s = %s;\n", ctype_of(vt), name, rn);
            scope_def(c, name, vt);
            return;
        }
        sb rhs = {0};
        ty it = emit_expr(c, st->e, &rhs);
        if (c->err) { sb_free(&rhs); return; }
        ty t = (ann.k != T_UNK) ? ann : it;
        if (t.k == T_UNK) { terr(c, "v1:无法推导 %s 的类型(补类型注解) @L%d", name, __LINE__); sb_free(&rhs); return; }
        // 初始化器先于绑定求值(对齐 rt eval_let;C 的名字在声明符末即入栈,须走临时)
        int n4 = c->tmpn++;
        char nv[32];
        snprintf(nv, sizeof nv, "ctron_nv%d", n4);
        if (ann.k == T_INT) {
            char h[64];
            snprintf(h, sizeof h, "ctron_decl_%s", wlname(ann));
            use_helper(c, h);
            sb_f(o, "%s %s = %s(%s);\n", ctype_of(ann), nv, h, rhs.d ? rhs.d : "0");
            sb_f(o, "%s %s = %s;\n", ctype_of(ann), name, nv);
        } else {
            sb_f(o, "%s %s = %s;\n", ctype_of(t), nv, rhs.d ? rhs.d : "0");
            sb_f(o, "%s %s = %s;\n", ctype_of(t), name, nv);
        }
        sb_free(&rhs);
        c->want = saved_let_want;
        scope_def(c, name, t);
        return;
    }
    case ST_ASSIGN: {
        // 成员赋值:p.x = v / p.x op= v(rt:"="/"member assign")
        if (st->target && st->target->kind == EX_MEMBER && st->target->m_is_name
            && st->target->obj && st->target->obj->kind == EX_IDENT) {
            const char* on = st->target->obj->text;
            const char* fn2 = st->target->mname;
            ty t;
            if (!scope_find(c, on, &t)) { terr(c, "v1 未解析名称:%s", on); return; }
            if (t.k != T_STRUCT && t.k != T_CLASS && t.k != T_BOX) { terr(c, "v1:成员赋值目标需 struct/class"); return; }
            sdef* sd = NULL;
            sfield* fl = NULL;
            if (t.k == T_STRUCT) {
                sd = &c->structs[t.bits];
                for (size_t i = 0; i < sd->n; i++)
                    if (!strcmp(sd->fields[i].name, fn2)) { fl = &sd->fields[i]; break; }
            } else if (t.k == T_CLASS) {
                cdef* cd = &c->classes[t.bits];
                for (size_t i = 0; i < cd->n; i++)
                    if (!strcmp(cd->fields[i].name, fn2)) { fl = (sfield*)&cd->fields[i]; break; }
            } else {
                ty et = box_elem(t);
                if (et.k == T_STRUCT) {
                    sd = &c->structs[et.bits];
                    for (size_t i = 0; i < sd->n; i++)
                        if (!strcmp(sd->fields[i].name, fn2)) { fl = &sd->fields[i]; break; }
                }
            }
            if (!fl) { terr(c, "v1:成员赋值字段不存在:%s", fn2); return; }
            sb rhs = {0};
            emit_expr(c, st->value, &rhs);
            if (c->err) { sb_free(&rhs); return; }
            const char* arrow = (t.k == T_CLASS || t.k == T_BOX) ? "->" : ".";
            if (st->aop == A_EQ) {
                if (fl->t.k == T_INT) {
                    char h[64];
                    snprintf(h, sizeof h, "ctron_decl_%s", wlname(fl->t));
                    use_helper(c, h);
                    sb_f(o, "%s%s%s = %s(%s);\n", on, arrow, fn2, h, rhs.d ? rhs.d : "0");
                } else {
                    sb_f(o, "%s%s%s = %s;\n", on, arrow, fn2, rhs.d ? rhs.d : "0");
                }
            } else {
                if (fl->t.k != T_INT) { terr(c, "v1:复合成员赋值需整型"); sb_free(&rhs); return; }
                const char* fam = st->aop == A_ADDEQ ? "madd" : st->aop == A_SUBEQ ? "msub"
                                : st->aop == A_MULEQ ? "mmul" : st->aop == A_DIVEQ ? "mdiv" : "mmod";
                char h[64];
                snprintf(h, sizeof h, "ctron_%s_%s", fam, wlname(fl->t));
                use_helper(c, h);
                sb_f(o, "%s%s%s = %s(%s%s%s, %s);\n", on, arrow, fn2, h, on, arrow, fn2, rhs.d ? rhs.d : "0");
            }
            sb_free(&rhs);
            return;
        }
        // 索引赋值:arr[i] = v / arr[i] op= v(rt:"="/"idx assign")
        if (st->target && st->target->kind == EX_INDEX && st->target->obj
            && st->target->obj->kind == EX_IDENT) {
            const char* on = st->target->obj->text;
            ty t;
            if (!scope_find(c, on, &t)) { terr(c, "v1 未解析名称:%s", on); return; }
            if (t.k != T_ARR) { terr(c, "v1:索引赋值目标需数组"); return; }
            use_helper(c, "ctron_idx");
            sb ix = {0}, rhs = {0};
            emit_expr(c, st->target->index, &ix);
            emit_expr(c, st->value, &rhs);
            if (c->err) { sb_free(&ix); sb_free(&rhs); return; }
            ty e2 = ty_int(t.ebits, t.eus);
            if (t.ek == T_FLT) e2 = ty_flt();
            if (t.ek == T_BOOL) e2 = ty_bool();
            if (t.ek == T_STR) e2 = ty_str();
            if (st->aop == A_EQ) {
                if (e2.k == T_INT) {
                    char h[64];
                    snprintf(h, sizeof h, "ctron_decl_%s", wlname(e2));
                    use_helper(c, h);
                    sb_f(o, "%s.d[ctron_idx(%s.n, %s)] = %s(%s);\n", on, on, ix.d ? ix.d : "0", h, rhs.d ? rhs.d : "0");
                } else {
                    sb_f(o, "%s.d[ctron_idx(%s.n, %s)] = %s;\n", on, on, ix.d ? ix.d : "0", rhs.d ? rhs.d : "0");
                }
            } else {
                if (e2.k != T_INT) { terr(c, "v1:复合索引赋值需整型"); sb_free(&ix); sb_free(&rhs); return; }
                const char* fam = st->aop == A_ADDEQ ? "iadd" : st->aop == A_SUBEQ ? "isub"
                                : st->aop == A_MULEQ ? "imul" : st->aop == A_DIVEQ ? "idiv" : "imod";
                char h[64];
                snprintf(h, sizeof h, "ctron_%s_%s", fam, wlname(e2));
                use_helper(c, h);
                sb_f(o, "%s.d[ctron_idx(%s.n, %s)] = %s(%s.d[ctron_idx(%s.n, %s)], %s);\n",
                     on, on, ix.d ? ix.d : "0", h, on, on, ix.d ? ix.d : "0", rhs.d ? rhs.d : "0");
            }
            sb_free(&ix);
            sb_free(&rhs);
            return;
        }
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
        if (c->in_test) {
            // rt:has_ret 置位 → 跳过余下语句,测试仍算通过(void return 等价)
            if (st->e) { sb d = {0}; emit_expr(c, st->e, &d); sb_free(&d); }
            sb_s(o, "return;\n");
            return;
        }
        {
            const ty* saved_rw = c->want;
            c->want = c->fn_ret;
            if (st->e && st->e->kind == EX_TRY) {
                // return expr? —— None/Err 原样传播
                sb op = {0};
                ty ot = emit_expr(c, st->e->obj, &op);
                c->want = saved_rw;
                if (c->err) { sb_free(&op); return; }
                if (ot.k != T_SUM) { terr(c, "v1:? 需 Option/Result"); sb_free(&op); return; }
                int is_opt = !strncmp(ot.tname, "ctron_opt_", 10);
                const char* bad = is_opt ? "CTRON_OPT_NONE" : "CTRON_RES_ERR";
                const char* mem = is_opt ? "some" : "ok";
                int n2 = c->tmpn++;
                if (c->in_main) {
                    sb_f(o, "{ %s ctron_t%d = %s;\n", ctype_of(ot), n2, op.d ? op.d : "0");
                    sb_f(o, "    if (ctron_t%d.tag == %s) exit(0);\n}\n", n2, bad);
                } else if (c->fn_ret && c->fn_ret->k == T_SUM && strcmp(c->fn_ret->tname, ot.tname) != 0
                           && c->fn_ret->ek2 == T_ERR && ot.ek2 != T_ERR && ot.tname2) {
                    // return expr? —— fn 错误目标 AnyError,操作数错误为具体类型:物化链后返回
                    char efn[96];
                    snprintf(efn, sizeof efn, "ctron_erase_e_%s", ot.tname2);
                    char ebody[512];
                    snprintf(ebody, sizeof ebody,
                        "static ctron_anyerr* %s(ctron_e_%s v) {\n"
                        "    ctron_anyerr* n = (ctron_anyerr*)calloc(1, sizeof(ctron_anyerr));\n"
                        "    n->message = strdup(\"\");\n"
                        "    n->cause = (ctron_anyerr*)calloc(1, sizeof(ctron_anyerr));\n"
                        "    n->trace = strdup(\"main:1\");\n"
                        "    (void)v; return n;\n}\n", efn, ot.tname2);
                    use_sum(c, ebody);
                    sb_f(o, "{ %s ctron_t%d = %s;\n", ctype_of(ot), n2, op.d ? op.d : "0");
                    sb_f(o, "    if (ctron_t%d.tag == CTRON_RES_ERR) return (%s){ .tag = CTRON_RES_ERR, .as.err = %s(ctron_t%d.as.err) };\n",
                         n2, c->fn_ret->tname, efn, n2);
                    sb_f(o, "    return (%s){ .tag = CTRON_RES_OK, .as.ok = ctron_t%d.as.ok };\n}\n",
                         c->fn_ret->tname, n2);
                } else if (c->fn_ret && c->fn_ret->k == T_SUM) {
                    sb_f(o, "{ %s ctron_t%d = %s;\n", ctype_of(ot), n2, op.d ? op.d : "0");
                    sb_f(o, "    if (ctron_t%d.tag == %s) return ctron_t%d;\n", n2, bad, n2);
                    sb_f(o, "    return ctron_t%d.as.%s;\n}\n", n2, mem);
                } else {
                    terr(c, "v1:? 早退类型与函数返回类型不符");
                }
                sb_free(&op);
                c->want = saved_rw;
                return;
            }
        }
        if (st->e && st->e->kind == EX_MATCH) {
            char* rn = NULL;
            emit_match(c, st->e, o, 1, &rn);
            if (rn) sb_f(o, "return %s;\n", rn);
            return;
        }
        if (!c->fn_ret || c->fn_ret->k == T_UNK) {
            // void 函数:求值副作用后裸返回(rt:返回值被丢弃)
            if (st->e && st->e->kind != EX_VOID) {
                sb r = {0};
                emit_expr(c, st->e, &r);
                sb_f(o, "(void)(%s);\n", r.d ? r.d : "0");
                sb_free(&r);
            }
            sb_s(o, "return;\n");
            return;
        }
        if (st->e) {
            sb r = {0};
            emit_expr(c, st->e, &r);
            sb_f(o, "return %s;\n", r.d ? r.d : "0");
            sb_free(&r);
        } else sb_s(o, "return 0;\n");
        return;
    case ST_EXPR:
        if (st->e && st->e->kind == EX_OWN) {
            // own 块:语句位透明执行(arena 句柄域 → C10-f+)
            scope_push(c);
            emit_block(c, st->e->obody, o);
            scope_pop(c);
            return;
        }
        if (st->e && st->e->kind == EX_IF) {
            emit_if_stmt(c, st->e, o);
            return;
        }
        if (st->e && st->e->kind == EX_MATCH) {
            emit_match(c, st->e, o, 0, NULL);
            return;
        }
        if (st->e && st->e->kind == EX_BLOCK) {
            scope_push(c);
            sb_s(o, "{\n");
            emit_block(c, st->e->block, o);
            scope_pop(c);
            sb_s(o, "}\n");
            return;
        }
        {
            sb r = {0};
            emit_expr(c, st->e, &r);
            sb_f(o, "%s;\n", r.d ? r.d : "");
            sb_free(&r);
        }
        return;
    case ST_BREAK:
        sb_s(o, "break;\n");
        return;
    case ST_CONTINUE:
        sb_s(o, "continue;\n");
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
        // for x in <range 值>(let r = 0..4; for i in r)
        if (st->iter && st->iter->kind != EX_RANGE) {
            if (!st->pat || (st->pat->kind != PAT_IDENT && st->pat->kind != PAT_WILD) || (st->pat->kind == PAT_IDENT && !st->pat->name)) { terr(c, "v1:for 模式仅标识符/通配"); return; }
            const char* var = st->pat->kind == PAT_WILD ? "_" : st->pat->name;
            sb it0 = {0};
            ty it_t = emit_expr(c, st->iter, &it0);
            if (c->err) { sb_free(&it0); return; }
            if (it_t.k == T_RANGE) {
                char iname[32];
                snprintf(iname, sizeof iname, "ctron_it_%d", c->tmpn++);
                sb_f(o, "{ ctron_rng %s = %s; for (int64_t %s = %s.lo; %s.incl ? %s <= %s.hi : %s < %s.hi; %s++) {",
                     iname, it0.d ? it0.d : "0", var, iname,
                     iname, var, iname, var, iname, var);
                sb_free(&it0);
                scope_push(c);
                scope_def(c, var, ty_int(64, 0));
                emit_block(c, st->body, o);
                scope_pop(c);
                sb_s(o, "}}\n");
                return;
            }
            if (it_t.k != T_ARR) { terr(c, "v1:for 需要 range 或数组"); sb_free(&it0); return; }
            {
                char iname[32];
                snprintf(iname, sizeof iname, "ctron_it_%d", c->tmpn++);
                sb_f(o, "{ %s %s = %s; for (int64_t ctron_i = 0; ctron_i < %s.n; ctron_i++) {",
                     ctype_of(it_t), iname, it0.d ? it0.d : "0", iname);
                sb_free(&it0);
                scope_push(c);
                scope_def(c, var, e2_of(it_t));
                sb_f(o, " %s %s = %s.d[ctron_i];", ctype_of(e2_of(it_t)), var, iname);
                emit_block(c, st->body, o);
                scope_pop(c);
                sb_s(o, "}}\n");
                return;
            }
        }
        // for x in <数组>
        if (st->iter && st->iter->kind != EX_RANGE) {
            if (!st->pat || (st->pat->kind != PAT_IDENT && st->pat->kind != PAT_WILD) || (st->pat->kind == PAT_IDENT && !st->pat->name)) { terr(c, "v1:for 模式仅标识符/通配"); return; }
            const char* var = st->pat->kind == PAT_WILD ? "_" : st->pat->name;
            if (is_reserved(var)) { terr(c, "v1:标识符保留前缀 ctron_:%s", var); return; }
            sb it = {0};
            ty it_t = emit_expr(c, st->iter, &it);
            if (c->err) { sb_free(&it); return; }
            if (it_t.k != T_ARR) { terr(c, "v1:for 需要 range 或数组"); sb_free(&it); return; }
            ty e2 = ty_int(it_t.ebits, it_t.eus);
            if (it_t.ek == T_FLT) e2 = ty_flt();
            if (it_t.ek == T_BOOL) e2 = ty_bool();
            if (it_t.ek == T_STR) e2 = ty_str();
            char iname[32];
            snprintf(iname, sizeof iname, "ctron_it_%d", c->tmpn++);
            sb_f(o, "{ %s %s = %s; for (int64_t ctron_i = 0; ctron_i < %s.n; ctron_i++) {",
                 ctype_of(it_t), iname, it.d ? it.d : "0", iname);
            sb_free(&it);
            scope_push(c);
            scope_def(c, var, e2);
            sb_f(o, " %s %s = %s.d[ctron_i];", ctype_of(e2), var, iname);
            emit_block(c, st->body, o);
            scope_pop(c);
            sb_s(o, "}}\n");
            return;
        }
        if (!st->iter || st->iter->kind != EX_RANGE) { terr(c, "v1:for 仅支持 range"); return; }
        if (!st->pat || (st->pat->kind != PAT_IDENT && st->pat->kind != PAT_WILD) || (st->pat->kind == PAT_IDENT && !st->pat->name)) { terr(c, "v1:for 模式仅标识符/通配"); return; }
        const char* var = st->pat->kind == PAT_WILD ? "_" : st->pat->name;
        if (st->pat->kind != PAT_WILD && is_reserved(var)) { terr(c, "v1:标识符保留前缀 ctron_:%s", var); return; }
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

void emit_block(tc* c, cblock* b, sb* o) {
    if (!b || c->err) return;
    for (size_t i = 0; i < b->nstmts; i++)
        emit_stmt(c, b->stmts[i], o);
    if (b->tail) {
        if (b->tail->kind == EX_MATCH) { emit_match(c, b->tail, o, 0, NULL); }
        else if (b->tail->kind == EX_IF) { emit_if_stmt(c, b->tail, o); }
        else if (b->tail->kind == EX_OWN) {
            scope_push(c);
            emit_block(c, b->tail->obody, o);
            scope_pop(c);
        } else {
            sb r = {0};
            emit_expr(c, b->tail, &r);
            sb_f(o, "%s;\n", r.d ? r.d : "");
            sb_free(&r);
        }
    }
    // Drop RAII(§6.4):作用域退出,按声明逆序调用 drop
    if (c->sc) {
        for (int di = (int)c->sc->n - 1; di >= 0; di--) {
            const char* vn = c->sc->vars[di].name;
            if (!strcmp(vn, "self")) continue; // receiver 由调用方 drop(方法帧不重复)
            ty vt = c->sc->vars[di].t;
            if (vt.k == T_STRUCT && vt.tname && type_has_drop_m(c, vt.tname)) {
                char hn[192];
                ty drt;
                if (ensure_method_fn(c, vt.tname, "drop", hn, &drt)) // 原型+方法体确保发射
                    sb_f(o, "%s(%s);\n", hn, vn);
            }
        }
    }
}
