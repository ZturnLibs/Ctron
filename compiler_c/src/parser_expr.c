#include "parser_internal.h"

// parser_expr.c —— 类型/表达式/块与语句/模式解析
// ============ 类型 ============

// [ 处内容为空
int bracket_content_is_empty(const cparser* p) {
    return tok_at(p, 1) == TOK_RBRACKET;
}
// [ 处内容恰为一个整数字面量
int bracket_content_is_single_int(const cparser* p) {
    return tok_at(p, 1) == TOK_INT && tok_at(p, 2) == TOK_RBRACKET;
}
// [ 配对 ] 之后是否紧跟 ( 或 {
int bracket_followed_by_call_or_lit(const cparser* p) {
    int depth = 0;
    size_t i = p->pos;
    while (i < p->ntoks) {
        ctron_tok_kind k = p->toks[i].kind;
        if (k == TOK_LBRACKET) depth++;
        else if (k == TOK_RBRACKET) {
            depth--;
            if (depth == 0) {
                if (i + 1 < p->ntoks) {
                    ctron_tok_kind nx = p->toks[i + 1].kind;
                    return nx == TOK_LPAREN || nx == TOK_LBRACE;
                }
                return 0;
            }
        } else if (k == TOK_EOF) return 0;
        i++;
    }
    return 0;
}

cty* parse_type(cparser* p) {
    p->depth++;
    if (p->depth > MAX_EXPR_DEPTH) {
        err_here(p, "E1001", "类型嵌套过深");
        p->depth--;
        return mk_type(p, TY_SELF);
    }
    cty* t = NULL;
    if (eat_k(p, TOK_AMP)) {
        t = mk_type(p, TY_REF);
        t->sub = parse_type(p);
    } else {
        // base
        switch (peek_k(p)) {
        case TOK_SELF:
            bump_tok(p);
            t = mk_type(p, TY_SELF);
            break;
        case TOK_INT:
        case TOK_FLOAT: {
            const char* raw = tokp_at(p, 0)->text ? tokp_at(p, 0)->text : "";
            bump_tok(p);
            t = mk_type(p, TY_CVAL);
            t->args = NULL; t->nargs = 0;
            char** pp = (char**)ctron_arena_alloc(p->arena, sizeof(char*));
            pp[0] = dup_text(p, raw);
            t->path = pp;
            t->npath = 1;
            break;
        }
        case TOK_FN: {
            bump_tok(p);
            t = mk_type(p, TY_FN);
            if (!eat_k(p, TOK_LPAREN)) err_here(p, "E1001", "函数类型缺少 (");
            tlist ts = {0};
            for (;;) {
                if (at_k(p, TOK_RPAREN)) break;
                tlist_push(&ts, parse_type(p));
                if (!eat_k(p, TOK_COMMA)) break;
            }
            if (!eat_k(p, TOK_RPAREN)) err_here(p, "E1001", "函数类型参数缺少 )");
            t->elems = tlist_done(&ts, p->arena, &t->nelems);
            t->fret = NULL;
            if (eat_k(p, TOK_ARROW)) t->fret = parse_type(p);
            break;
        }
        case TOK_LPAREN: {
            bump_tok(p);
            t = mk_type(p, TY_TUPLE);
            tlist ts = {0};
            for (;;) {
                if (at_k(p, TOK_RPAREN)) break;
                tlist_push(&ts, parse_type(p));
                if (!eat_k(p, TOK_COMMA)) break;
            }
            if (!eat_k(p, TOK_RPAREN)) err_here(p, "E1001", "元组类型缺少 )");
            t->elems = tlist_done(&ts, p->arena, &t->nelems);
            break;
        }
        case TOK_IDENT: {
            sv seg = parse_dotted_path_sv(p);
            size_t nseg;
            char** arr = sv_done(&seg, p->arena, &nseg);
            t = mk_type(p, TY_NAMED);
            t->path = arr;
            t->npath = nseg;
            t->args = NULL;
            t->nargs = 0;
            // [ ] 消歧:空 → 切片交后缀;单整型 → 定长数组;其余 → 类型实参
            if (at_k(p, TOK_LBRACKET) && !bracket_content_is_empty(p)) {
                if (bracket_content_is_single_int(p)) {
                    bump_tok(p);
                    cexpr* size = parse_expr(p);
                    if (!eat_k(p, TOK_RBRACKET)) err_here(p, "E1001", "定长数组缺少 ]");
                    cty* arr_t = mk_type(p, TY_ARRAY);
                    arr_t->elem = t;
                    arr_t->size = size;
                    t = arr_t;
                } else {
                    bump_tok(p);
                    tlist ts = {0};
                    for (;;) {
                        if (at_k(p, TOK_RBRACKET)) break;
                        tlist_push(&ts, parse_type(p));
                        if (!eat_k(p, TOK_COMMA)) break;
                    }
                    if (!eat_k(p, TOK_RBRACKET)) err_here(p, "E1001", "类型实参缺少 ]");
                    t->args = tlist_done(&ts, p->arena, &t->nargs);
                }
            }
            break;
        }
        default:
            err_here(p, "E1001", "预期类型,实际 %s", tok_desc(p));
            bump_tok(p);
            t = mk_type(p, TY_SELF);
            break;
        }
        // postfix
        for (;;) {
            if (at_k(p, TOK_LBRACKET)) {
                bump_tok(p);
                if (eat_k(p, TOK_RBRACKET)) {
                    cty* st = mk_type(p, TY_SLICE);
                    st->sub = t;
                    t = st;
                } else {
                    cexpr* size = NULL;
                    if (!at_k(p, TOK_RBRACKET)) size = parse_expr(p);
                    if (!eat_k(p, TOK_RBRACKET)) err_here(p, "E1001", "定长数组缺少 ]");
                    cty* at2 = mk_type(p, TY_ARRAY);
                    at2->elem = t;
                    at2->size = size;
                    t = at2;
                }
            } else if (at_k(p, TOK_QUESTION)) {
                bump_tok(p);
                cty* ot = mk_type(p, TY_OPT);
                ot->sub = t;
                t = ot;
            } else break;
        }
    }
    p->depth--;
    return t;
}

// ============ 表达式 ============


cexpr* parse_expr_flags(cparser* p, int allow_struct) {
    return parse_or(p, allow_struct);
}

cexpr* parse_or(cparser* p, int allow_struct) {
    cexpr* lhs = parse_and(p, allow_struct);
    while (at_k(p, TOK_OR)) {
        bump_tok(p);
        cexpr* rhs = parse_and(p, allow_struct);
        cexpr* b = mk_expr(p, EX_BINARY);
        b->bop = B_OR;
        b->lhs = lhs;
        b->rhs = rhs;
        lhs = b;
    }
    return lhs;
}
cexpr* parse_and(cparser* p, int allow_struct) {
    cexpr* lhs = parse_compare(p, allow_struct);
    while (at_k(p, TOK_AND_AND)) {
        bump_tok(p);
        cexpr* rhs = parse_compare(p, allow_struct);
        cexpr* b = mk_expr(p, EX_BINARY);
        b->bop = B_AND;
        b->lhs = lhs;
        b->rhs = rhs;
        lhs = b;
    }
    return lhs;
}
cexpr* parse_compare(cparser* p, int allow_struct) {
    cexpr* lhs = parse_range(p, allow_struct);
    int chained_reported = 0;
    for (;;) {
        cbinop op;
        switch (peek_k(p)) {
        case TOK_EQ_EQ: op = B_EQ; break;
        case TOK_NOT_EQ: op = B_NE; break;
        case TOK_LT: op = B_LT; break;
        case TOK_GT: op = B_GT; break;
        case TOK_LT_EQ: op = B_LE; break;
        case TOK_GT_EQ: op = B_GE; break;
        default: return lhs;
        }
        bump_tok(p);
        cexpr* rhs = parse_range(p, allow_struct);
        cexpr* b = mk_expr(p, EX_BINARY);
        b->bop = op;
        b->lhs = lhs;
        b->rhs = rhs;
        lhs = b;
        // 比较不可链(§4.3)
        ctron_tok_kind nx = peek_k(p);
        if (nx == TOK_EQ_EQ || nx == TOK_NOT_EQ || nx == TOK_LT || nx == TOK_GT
            || nx == TOK_LT_EQ || nx == TOK_GT_EQ) {
            if (!chained_reported) {
                err_here(p, "E1001", "比较运算符不可链:写 a < b && b < c");
                chained_reported = 1;
            }
            continue;
        }
        return lhs;
    }
}
cexpr* parse_range(cparser* p, int allow_struct) {
    cexpr* from = parse_additive(p, allow_struct);
    if (at_k(p, TOK_DOT_DOT) || at_k(p, TOK_DOT_DOT_EQ)) {
        int inclusive = at_k(p, TOK_DOT_DOT_EQ);
        bump_tok(p);
        cexpr* to = parse_additive(p, allow_struct);
        cexpr* r = mk_expr(p, EX_RANGE);
        r->inclusive = inclusive;
        r->from = from;
        r->to = to;
        return r;
    }
    return from;
}
cexpr* parse_additive(cparser* p, int allow_struct) {
    cexpr* lhs = parse_multiplicative(p, allow_struct);
    for (;;) {
        cbinop op;
        switch (peek_k(p)) {
        case TOK_PLUS: op = B_ADD; break;
        case TOK_MINUS: op = B_SUB; break;
        case TOK_WRAP_PLUS: op = B_WADD; break;
        case TOK_WRAP_MINUS: op = B_WSUB; break;
        default: return lhs;
        }
        bump_tok(p);
        cexpr* rhs = parse_multiplicative(p, allow_struct);
        cexpr* b = mk_expr(p, EX_BINARY);
        b->bop = op;
        b->lhs = lhs;
        b->rhs = rhs;
        lhs = b;
    }
}
cexpr* parse_multiplicative(cparser* p, int allow_struct) {
    cexpr* lhs = parse_unary(p, allow_struct);
    for (;;) {
        cbinop op;
        switch (peek_k(p)) {
        case TOK_STAR: op = B_MUL; break;
        case TOK_SLASH: op = B_DIV; break;
        case TOK_PERCENT: op = B_MOD; break;
        default: return lhs;
        }
        bump_tok(p);
        cexpr* rhs = parse_unary(p, allow_struct);
        cexpr* b = mk_expr(p, EX_BINARY);
        b->bop = op;
        b->lhs = lhs;
        b->rhs = rhs;
        lhs = b;
    }
}
cexpr* parse_unary(cparser* p, int allow_struct) {
    if (eat_k(p, TOK_MINUS)) {
        cexpr* u = mk_expr(p, EX_UNARY);
        u->uop = UN_NEG;
        u->ux = parse_unary(p, allow_struct);
        return u;
    }
    if (eat_k(p, TOK_BANG)) {
        cexpr* u = mk_expr(p, EX_UNARY);
        u->uop = UN_NOT;
        u->ux = parse_unary(p, allow_struct);
        return u;
    }
    return parse_postfix(p, allow_struct);
}

cexpr* parse_postfix(cparser* p, int allow_struct) {
    cexpr* e = parse_primary(p, allow_struct);
    p->depth++;
    for (;;) {
        if (p->depth > MAX_EXPR_DEPTH) {
            err_here(p, "E1001", "表达式嵌套过深");
            break;
        }
        switch (peek_k(p)) {
        case TOK_LPAREN: {
            bump_tok(p);
            elist args = {0};
            for (;;) {
                if (at_k(p, TOK_RPAREN)) break;
                elist_push(&args, parse_expr(p));
                if (!eat_k(p, TOK_COMMA)) break;
            }
            if (!eat_k(p, TOK_RPAREN)) err_here(p, "E1001", "实参表缺少 )");
            cexpr* c = mk_expr(p, EX_CALL);
            c->callee = e;
            c->elems = elist_done(&args, p->arena, &c->nelems);
            e = c;
            break;
        }
        case TOK_LBRACKET: {
            if (bracket_followed_by_call_or_lit(p)) {
                // 类型实参后缀
                if (!eat_k(p, TOK_LBRACKET)) err_here(p, "E1001", "类型实参缺少 [");
                tlist ts = {0};
                for (;;) {
                    if (at_k(p, TOK_RBRACKET)) break;
                    tlist_push(&ts, parse_type(p));
                    if (!eat_k(p, TOK_COMMA)) break;
                }
                if (!eat_k(p, TOK_RBRACKET)) err_here(p, "E1001", "类型实参缺少 ]");
                cexpr* ta = mk_expr(p, EX_TYPEARGS);
                ta->obj = e;
                ta->targs = tlist_done(&ts, p->arena, &ta->ntargs);
                e = ta;
            } else {
                // 尝试索引;内容非合法单表达式(顶层逗号)→ 回退类型实参
                size_t save = p->pos;
                size_t dsave = p->ndiags;
                bump_tok(p); // [
                cexpr* idx = parse_expr(p);
                if (at_k(p, TOK_COMMA)) {
                    // 回退
                    p->pos = save;
                    p->ndiags = dsave;
                    if (!eat_k(p, TOK_LBRACKET)) err_here(p, "E1001", "类型实参缺少 [");
                    tlist ts = {0};
                    for (;;) {
                        if (at_k(p, TOK_RBRACKET)) break;
                        tlist_push(&ts, parse_type(p));
                        if (!eat_k(p, TOK_COMMA)) break;
                    }
                    if (!eat_k(p, TOK_RBRACKET)) err_here(p, "E1001", "类型实参缺少 ]");
                    cexpr* ta = mk_expr(p, EX_TYPEARGS);
                    ta->obj = e;
                    ta->targs = tlist_done(&ts, p->arena, &ta->ntargs);
                    e = ta;
                } else {
                    if (!eat_k(p, TOK_RBRACKET)) err_here(p, "E1001", "索引缺少 ]");
                    cexpr* ix = mk_expr(p, EX_INDEX);
                    ix->obj = e;
                    ix->index = idx;
                    e = ix;
                }
            }
            break;
        }
        case TOK_DOT: {
            bump_tok(p);
            cexpr* m = mk_expr(p, EX_MEMBER);
            m->obj = e;
            if (peek_k(p) == TOK_IDENT) {
                m->m_is_name = 1;
                m->mname = dup_text(p, tokp_at(p, 0)->text);
                bump_tok(p);
            } else if (peek_k(p) == TOK_INT) {
                m->m_is_name = 0;
                const char* txt = tokp_at(p, 0)->text ? tokp_at(p, 0)->text : "0";
                m->mtuple = (unsigned)strtoul(txt, NULL, 10);
                bump_tok(p);
            } else {
                err_here(p, "E1001", "预期成员名,实际 %s", tok_desc(p));
                m->m_is_name = 1;
                m->mname = dup_text(p, "");
            }
            e = m;
            break;
        }
        case TOK_QUESTION: {
            bump_tok(p);
            cexpr* tr = mk_expr(p, EX_TRY);
            tr->obj = e;
            e = tr;
            break;
        }
        default:
            goto postfix_done;
        }
    }
postfix_done:
    p->depth--;
    return e;
}

cexpr* parse_struct_lit(cparser* p, sv* path) {
    size_t npath;
    char** arr = sv_done(path, p->arena, &npath);
    if (!eat_k(p, TOK_LBRACE)) err_here(p, "E1001", "构造字面量缺少 {");
    cexpr* s = mk_expr(p, EX_STRUCT);
    s->path = arr;
    s->npath = npath;
    filist fs = {0};
    for (;;) {
        skip_newlines(p);
        if (at_k(p, TOK_RBRACE)) { bump_tok(p); break; }
        if (at_k(p, TOK_EOF)) { err_here(p, "E1001", "未闭合的构造字面量"); break; }
        size_t before = p->pos;
        cfieldinit* f = filist_new(&fs, p->arena);
        f->name = expect_ident(p, "字段初始化");
        f->value = eat_k(p, TOK_COLON) ? parse_expr(p) : NULL;
        ensure_progress(p, before);
        if (!eat_k(p, TOK_COMMA)) {
            skip_newlines(p);
            if (!at_k(p, TOK_RBRACE)) {
                err_here(p, "E1001", "构造字面量字段应以逗号分隔");
            }
        }
    }
    s->fields = filist_done(&fs, p->arena, &s->nfields);
    return s;
}

cexpr* parse_if(cparser* p) {
    bump_tok(p); // if
    cexpr* ie = mk_expr(p, EX_IF);
    ie->cond = parse_expr_flags(p, 0);
    ie->then_b = parse_block(p);
    ie->els = NULL;
    if (at_k(p, TOK_ELSE)) {
        bump_tok(p);
        if (at_k(p, TOK_IF)) ie->els = parse_if(p);
        else {
            cexpr* b = mk_expr(p, EX_BLOCK);
            b->block = parse_block(p);
            ie->els = b;
        }
    } else if (at_k(p, TOK_NEWLINE) && lookahead_past_newlines(p) == TOK_ELSE) {
        err_here(p, "E1001", "else 必须与 } 同行:`} else {`");
        skip_newlines(p);
        bump_tok(p); // else
        if (at_k(p, TOK_IF)) ie->els = parse_if(p);
        else {
            cexpr* b = mk_expr(p, EX_BLOCK);
            b->block = parse_block(p);
            ie->els = b;
        }
    }
    return ie;
}

cexpr* parse_match(cparser* p) {
    bump_tok(p); // match
    cexpr* m = mk_expr(p, EX_MATCH);
    m->scrut = parse_expr_flags(p, 0);
    if (!eat_k(p, TOK_LBRACE)) err_here(p, "E1001", "match 体缺少 {");
    arm arms = {0};
    for (;;) {
        skip_newlines(p);
        if (at_k(p, TOK_RBRACE)) { bump_tok(p); break; }
        if (at_k(p, TOK_EOF)) { err_here(p, "E1001", "未闭合的 match 体"); break; }
        cmatcharm* a = arm_new(&arms, p->arena);
        a->pat = parse_pattern(p);
        if (!eat_k(p, TOK_FAT_ARROW)) err_here(p, "E1001", "match 臂缺少 =>");
        a->expr = parse_expr(p);
        if (at_k(p, TOK_NEWLINE)) skip_newlines(p);
        else if (!at_k(p, TOK_RBRACE)) {
            err_here(p, "E1001", "match 臂后应为换行,实际 %s", tok_desc(p));
        }
    }
    m->arms = arm_done(&arms, p->arena, &m->narms);
    return m;
}

cexpr* parse_scope(cparser* p) {
    bump_tok(p); // scope
    cexpr* s = mk_expr(p, EX_SCOPE);
    if (!eat_k(p, TOK_LBRACE)) err_here(p, "E1001", "scope 块缺少 {");
    if (!eat_k(p, TOK_PIPE)) err_here(p, "E1001", "scope 参数缺少 |");
    s->sparam = expect_ident(p, "scope 参数");
    if (!eat_k(p, TOK_PIPE)) err_here(p, "E1001", "scope 参数缺少 |");
    s->sbody = parse_block_after_lbrace(p);
    return s;
}

cexpr* parse_own(cparser* p) {
    bump_tok(p); // own
    cexpr* o = mk_expr(p, EX_OWN);
    if (!eat_k(p, TOK_LPAREN)) err_here(p, "E1001", "own 块缺少 (");
    o->arena_name = expect_ident(p, "arena 名");
    if (!eat_k(p, TOK_RPAREN)) err_here(p, "E1001", "own 块缺少 )");
    o->obody = parse_block(p);
    return o;
}

cexpr* parse_closure(cparser* p) {
    if (!eat_k(p, TOK_PIPE)) err_here(p, "E1001", "闭包缺少 |");
    cexpr* c = mk_expr(p, EX_CLOSURE);
    cplist cps = {0};
    if (!at_k(p, TOK_PIPE)) {
        for (;;) {
            cclosureparam* cp = cplist_new(&cps, p->arena);
            cp->is_var = eat_k(p, TOK_VAR);
            cp->name = expect_ident(p, "闭包参数");
            cp->ty = eat_k(p, TOK_COLON) ? parse_type(p) : NULL;
            if (!eat_k(p, TOK_COMMA)) break;
        }
    }
    if (!eat_k(p, TOK_PIPE)) err_here(p, "E1001", "闭包参数结束缺少 |");
    c->cret = NULL;
    if (eat_k(p, TOK_ARROW)) c->cret = parse_type(p);
    c->cbody = parse_expr(p);
    c->cparams = cplist_done(&cps, p->arena, &c->ncparams);
    return c;
}

cexpr* parse_primary(cparser* p, int allow_struct) {
    p->depth++;
    if (p->depth > MAX_EXPR_DEPTH) {
        err_here(p, "E1001", "表达式嵌套过深");
        p->depth--;
        return mk_expr(p, EX_VOID);
    }
    cexpr* e = parse_primary_inner(p, allow_struct);
    p->depth--;
    return e;
}

cexpr* parse_primary_inner(cparser* p, int allow_struct) {
    const ctron_token* t = tokp_at(p, 0);
    switch (t->kind) {
    case TOK_INT: {
        cexpr* e = mk_expr(p, EX_INT);
        e->text = dup_text(p, t->text);
        e->suffix = dup_text(p, suffix_str(t->suffix));
        bump_tok(p);
        return e;
    }
    case TOK_FLOAT: {
        cexpr* e = mk_expr(p, EX_FLOAT);
        e->text = dup_text(p, t->text);
        e->suffix = dup_text(p, suffix_str(t->suffix));
        bump_tok(p);
        return e;
    }
    case TOK_STR: {
        cexpr* e = mk_expr(p, EX_STR);
        ast_str_parts(p, &t->parts, &e->sparts, &e->nsparts);
        bump_tok(p);
        return e;
    }
    case TOK_TRUE: {
        bump_tok(p);
        cexpr* x = mk_expr(p, EX_BOOL);
        x->bval = 1;
        return x;
    }
    case TOK_FALSE: {
        bump_tok(p);
        cexpr* x = mk_expr(p, EX_BOOL);
        x->bval = 0;
        return x;
    }
    case TOK_VOID:
        bump_tok(p);
        return mk_expr(p, EX_VOID);
    case TOK_SELF: {
        bump_tok(p);
        cexpr* x = mk_expr(p, EX_IDENT);
        x->text = dup_text(p, "self");
        return x;
    }
    case TOK_IDENT: {
        char* nm = dup_text(p, t->text);
        bump_tok(p);
        if (allow_struct && at_k(p, TOK_LBRACE)) {
            sv path = {0};
            sv_push(&path, nm);
            return parse_struct_lit(p, &path);
        }
        cexpr* x = mk_expr(p, EX_IDENT);
        x->text = nm;
        return x;
    }
    case TOK_LPAREN: {
        bump_tok(p);
        if (at_k(p, TOK_RPAREN)) { bump_tok(p); return mk_expr(p, EX_TUPLE); }
        elist items = {0};
        elist_push(&items, parse_expr(p));
        int is_tuple = 0;
        while (eat_k(p, TOK_COMMA)) {
            if (at_k(p, TOK_RPAREN)) break;
            elist_push(&items, parse_expr(p));
            is_tuple = 1;
        }
        if (!eat_k(p, TOK_RPAREN)) err_here(p, "E1001", "括号/元组缺少 )");
        if (is_tuple) {
            cexpr* tp = mk_expr(p, EX_TUPLE);
            tp->elems = elist_done(&items, p->arena, &tp->nelems);
            return tp;
        }
        cexpr* single = items.d[0];
        free(items.d);
        return single;
    }
    case TOK_LBRACKET: {
        bump_tok(p);
        elist items = {0};
        for (;;) {
            if (at_k(p, TOK_RBRACKET)) break;
            elist_push(&items, parse_expr(p));
            if (!eat_k(p, TOK_COMMA)) break;
        }
        if (!eat_k(p, TOK_RBRACKET)) err_here(p, "E1001", "数组字面量缺少 ]");
        cexpr* ar = mk_expr(p, EX_ARRAY);
        ar->elems = elist_done(&items, p->arena, &ar->nelems);
        return ar;
    }
    case TOK_LBRACE: {
        cexpr* b = mk_expr(p, EX_BLOCK);
        b->block = parse_block(p);
        return b;
    }
    case TOK_IF: return parse_if(p);
    case TOK_MATCH: return parse_match(p);
    case TOK_SCOPE: return parse_scope(p);
    case TOK_OWN: return parse_own(p);
    case TOK_PIPE: return parse_closure(p);
    default:
        err_here(p, "E1001", "意外的记号 %s 在表达式位置", tok_desc(p));
        bump_tok(p);
        return mk_expr(p, EX_VOID);
    }
}

// ============ 块与语句 ============

cblock* parse_block_after_lbrace(cparser* p) {
    skip_newlines(p);
    cblock* b = mk_block(p);
    slist ss = {0};
    b->tail = NULL;
    for (;;) {
        skip_newlines(p);
        if (at_k(p, TOK_RBRACE)) { bump_tok(p); break; }
        if (at_k(p, TOK_EOF)) { err_here(p, "E1001", "未闭合的块"); break; }
        ctron_tok_kind k = peek_k(p);
        if (k == TOK_LET || k == TOK_VAR) {
            size_t before = p->pos;
            cstmt* st = (cstmt*)ctron_arena_alloc(p->arena, sizeof(cstmt));
            slist_push(&ss, st);
            st->kind = ST_LET;
            st->is_var = (k == TOK_VAR);
            bump_tok(p);
            st->pat = parse_pattern(p);
            st->ty = eat_k(p, TOK_COLON) ? parse_type(p) : NULL;
            if (!eat_k(p, TOK_ASSIGN)) err_here(p, "E1001", "绑定缺少 =");
            st->e = parse_expr(p);
            ensure_progress(p, before);
            // require_stmt_end
            if (at_k(p, TOK_NEWLINE)) skip_newlines(p);
            else if (at_k(p, TOK_RBRACE) || at_k(p, TOK_EOF)) { /* ok */ }
            else err_here(p, "E1001", "语句后应为换行,实际 %s", tok_desc(p));
        } else if (k == TOK_RETURN) {
            bump_tok(p);
            cstmt* st = (cstmt*)ctron_arena_alloc(p->arena, sizeof(cstmt));
            slist_push(&ss, st);
            st->kind = ST_RET;
            if (at_k(p, TOK_NEWLINE) || at_k(p, TOK_RBRACE)) st->e = NULL;
            else st->e = parse_expr(p);
            if (at_k(p, TOK_NEWLINE)) skip_newlines(p);
            else if (at_k(p, TOK_RBRACE) || at_k(p, TOK_EOF)) { /* ok */ }
            else err_here(p, "E1001", "语句后应为换行,实际 %s", tok_desc(p));
        } else if (k == TOK_FOR) {
            bump_tok(p);
            cstmt* st = (cstmt*)ctron_arena_alloc(p->arena, sizeof(cstmt));
            slist_push(&ss, st);
            st->kind = ST_FOR;
            st->pat = parse_pattern(p);
            if (!eat_k(p, TOK_IN)) err_here(p, "E1001", "for 缺少 in");
            st->iter = parse_expr_flags(p, 0);
            st->body = parse_block(p);
            if (at_k(p, TOK_NEWLINE)) skip_newlines(p);
            else if (at_k(p, TOK_RBRACE) || at_k(p, TOK_EOF)) { /* ok */ }
            else err_here(p, "E1001", "语句后应为换行,实际 %s", tok_desc(p));
        } else if (k == TOK_WHILE) {
            bump_tok(p);
            cstmt* st = (cstmt*)ctron_arena_alloc(p->arena, sizeof(cstmt));
            slist_push(&ss, st);
            st->kind = ST_WHILE;
            st->e = parse_expr_flags(p, 0); // cond
            st->body = parse_block(p);
            if (at_k(p, TOK_NEWLINE)) skip_newlines(p);
            else if (at_k(p, TOK_RBRACE) || at_k(p, TOK_EOF)) { /* ok */ }
            else err_here(p, "E1001", "语句后应为换行,实际 %s", tok_desc(p));
        } else {
            cexpr* e = parse_expr(p);
            cassignop aop;
            int is_assign = 1;
            switch (peek_k(p)) {
            case TOK_ASSIGN: aop = A_EQ; break;
            case TOK_PLUS_EQ: aop = A_ADDEQ; break;
            case TOK_MINUS_EQ: aop = A_SUBEQ; break;
            case TOK_STAR_EQ: aop = A_MULEQ; break;
            case TOK_SLASH_EQ: aop = A_DIVEQ; break;
            case TOK_PERCENT_EQ: aop = A_MODEQ; break;
            default: is_assign = 0; aop = A_EQ; break;
            }
            if (is_assign) {
                bump_tok(p);
                if (!(e->kind == EX_IDENT || e->kind == EX_MEMBER || e->kind == EX_INDEX)) {
                    err_here(p, "E1001", "无效的赋值目标");
                }
                cstmt* st = (cstmt*)ctron_arena_alloc(p->arena, sizeof(cstmt));
            slist_push(&ss, st);
                st->kind = ST_ASSIGN;
                st->target = e;
                st->aop = aop;
                st->value = parse_expr(p);
                if (at_k(p, TOK_NEWLINE)) skip_newlines(p);
                else if (at_k(p, TOK_RBRACE) || at_k(p, TOK_EOF)) { /* ok */ }
                else err_here(p, "E1001", "语句后应为换行,实际 %s", tok_desc(p));
            } else {
                // 尾表达式判定:其后(可跨换行)是 } 或 EOF → tail
                ctron_tok_kind nx = lookahead_past_newlines(p);
                if (nx == TOK_RBRACE) {
                    skip_newlines(p);
                    b->tail = e;
                    bump_tok(p);
                    break;
                } else if (nx == TOK_EOF) {
                    err_here(p, "E1001", "未闭合的块");
                    b->tail = e;
                    break;
                } else {
                    cstmt* st = (cstmt*)ctron_arena_alloc(p->arena, sizeof(cstmt));
            slist_push(&ss, st);
                    st->kind = ST_EXPR;
                    st->e = e;
                    if (at_k(p, TOK_NEWLINE)) skip_newlines(p);
                    else err_here(p, "E1001", "语句后应为换行,实际 %s", tok_desc(p));
                }
            }
        }
    }
    b->stmts = slist_done(&ss, p->arena, &b->nstmts);
    return b;
}

cblock* parse_block(cparser* p) {
    if (!eat_k(p, TOK_LBRACE)) err_here(p, "E1001", "块缺少 {");
    return parse_block_after_lbrace(p);
}

// ============ 模式 ============

cpat* parse_pattern(cparser* p) {
    const ctron_token* t = tokp_at(p, 0);
    switch (t->kind) {
    case TOK_UNDERSCORE:
        bump_tok(p);
        return cpat_wild(p);
    case TOK_INT: {
        bump_tok(p);
        cpat* pt = (cpat*)ctron_arena_alloc(p->arena, sizeof(cpat));
        pt->kind = PAT_LIT;
        pt->lkind = PLIT_INT;
        pt->name = dup_text(p, t->text);
        return pt;
    }
    case TOK_FLOAT: {
        bump_tok(p);
        cpat* pt = (cpat*)ctron_arena_alloc(p->arena, sizeof(cpat));
        pt->kind = PAT_LIT;
        pt->lkind = PLIT_FLOAT;
        pt->name = dup_text(p, t->text);
        return pt;
    }
    case TOK_STR: {
        bump_tok(p);
        cpat* pt = (cpat*)ctron_arena_alloc(p->arena, sizeof(cpat));
        pt->kind = PAT_LIT;
        pt->lkind = PLIT_STR;
        size_t n = 0;
        for (size_t i = 0; i < t->parts.len; i++) n += strlen(t->parts.items[i].s);
        char* buf = (char*)ctron_arena_alloc(p->arena, n + 1);
        buf[0] = '\0';
        for (size_t i = 0; i < t->parts.len; i++) strcat(buf, t->parts.items[i].s);
        pt->name = buf;
        return pt;
    }
    case TOK_TRUE:
    case TOK_FALSE: {
        bump_tok(p);
        cpat* pt = (cpat*)ctron_arena_alloc(p->arena, sizeof(cpat));
        pt->kind = PAT_LIT;
        pt->lkind = PLIT_BOOL;
        pt->lb = (t->kind == TOK_TRUE);
        return pt;
    }
    case TOK_LPAREN: {
        bump_tok(p);
        patlist ps = {0};
        for (;;) {
            if (at_k(p, TOK_RPAREN)) break;
            patlist_push(&ps, parse_pattern(p));
            if (!eat_k(p, TOK_COMMA)) break;
        }
        if (!eat_k(p, TOK_RPAREN)) err_here(p, "E1001", "元组模式缺少 )");
        if (ps.n == 1) {
            cpat* single = ps.d[0];
            free(ps.d);
            return single;
        }
        cpat* pt = (cpat*)ctron_arena_alloc(p->arena, sizeof(cpat));
        pt->kind = PAT_TUPLE;
        pt->elems = patlist_done(&ps, p->arena, &pt->nelems);
        return pt;
    }
    case TOK_IDENT: {
        sv path = parse_dotted_path_sv(p);
        size_t npath;
        char** arr = sv_done(&path, p->arena, &npath);
        if (at_k(p, TOK_LPAREN)) {
            bump_tok(p);
            patlist ps = {0};
            for (;;) {
                if (at_k(p, TOK_RPAREN)) break;
                patlist_push(&ps, parse_pattern(p));
                if (!eat_k(p, TOK_COMMA)) break;
            }
            if (!eat_k(p, TOK_RPAREN)) err_here(p, "E1001", "变体模式缺少 )");
            cpat* pt = (cpat*)ctron_arena_alloc(p->arena, sizeof(cpat));
            pt->kind = PAT_AGG;
            pt->path = arr;
            pt->npath = npath;
            pt->agg = AG_TUPLE;
            pt->elems = patlist_done(&ps, p->arena, &pt->nelems);
            return pt;
        } else if (at_k(p, TOK_LBRACE)) {
            bump_tok(p);
            cpat* pt = (cpat*)ctron_arena_alloc(p->arena, sizeof(cpat));
            pt->kind = PAT_AGG;
            pt->path = arr;
            pt->npath = npath;
            pt->agg = AG_STRUCT;
            spflist flds = {0};
            for (;;) {
                skip_newlines(p);
                if (at_k(p, TOK_RBRACE)) { bump_tok(p); break; }
                if (at_k(p, TOK_EOF)) { err_here(p, "E1001", "未闭合的结构模式"); break; }
                size_t before = p->pos;
                cstructpatfield* f = spflist_new(&flds, p->arena);
                f->name = expect_ident(p, "结构模式字段");
                f->pat = eat_k(p, TOK_COLON) ? parse_pattern(p) : NULL;
                ensure_progress(p, before);
                if (!eat_k(p, TOK_COMMA)) {
                    skip_newlines(p);
                    if (!at_k(p, TOK_RBRACE)) {
                        err_here(p, "E1001", "结构模式字段应以逗号分隔");
                    }
                }
            }
            pt->sfields = spflist_done(&flds, p->arena, &pt->nsfields);
            return pt;
        } else if (npath > 1) {
            cpat* pt = (cpat*)ctron_arena_alloc(p->arena, sizeof(cpat));
            pt->kind = PAT_AGG;
            pt->path = arr;
            pt->npath = npath;
            pt->agg = AG_UNIT;
            return pt;
        } else if (npath == 1 && arr[0] && arr[0][0] && isupper((unsigned char)arr[0][0])) {
            // PascalCase 单段 → 无载荷变体(命名约定,§1.3)
            cpat* pt = (cpat*)ctron_arena_alloc(p->arena, sizeof(cpat));
            pt->kind = PAT_AGG;
            pt->path = arr;
            pt->npath = npath;
            pt->agg = AG_UNIT;
            return pt;
        } else {
            cpat* pt = (cpat*)ctron_arena_alloc(p->arena, sizeof(cpat));
            pt->kind = PAT_IDENT;
            pt->name = arr ? arr[0] : dup_text(p, "");
            return pt;
        }
    }
    default:
        err_here(p, "E1001", "意外记号 %s 在模式位置", tok_desc(p));
        bump_tok(p);
        return cpat_wild(p);
    }
}
