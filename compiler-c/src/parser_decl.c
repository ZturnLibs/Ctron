#include "parser_internal.h"

// parser_decl.c —— 文件与声明解析(fn/struct/enum/class/trait/impl/use/const/static)
// ============ 文件与声明 ============

cexpr* parse_expr(cparser* p) { return parse_expr_flags(p, 1); }

cvis parse_vis(cparser* p) {
    if (at_k(p, TOK_PUB)) {
        bump_tok(p);
        if (at_k(p, TOK_LPAREN) && tok_at(p, 1) == TOK_IDENT && tokp_at(p, 1)->text
            && strcmp(tokp_at(p, 1)->text, "pkg") == 0) {
            bump_tok(p);
            bump_tok(p);
            if (!eat_k(p, TOK_RPAREN)) err_here(p, "E1001", "pub(pkg) 缺少 )");
            return VIS_PUBPKG;
        }
        return VIS_PUB;
    }
    return VIS_PRIVATE;
}

void parse_attrs(cparser* p, alist* attrs, sv* derives) {
    for (;;) {
        if (at_k(p, TOK_HASH)) {
            bump_tok(p);
            if (!eat_k(p, TOK_LBRACKET)) err_here(p, "E1001", "#[ 缺少 [");
            cattr* a = alist_new(attrs, p->arena);
            a->name = NULL;
            if (peek_k(p) == TOK_IDENT) {
                a->name = dup_text(p, tokp_at(p, 0)->text);
                bump_tok(p);
            } else {
                err_here(p, "E1001", "预期注解名,实际 %s", tok_desc(p));
            }
            sv args = {0};
            if (eat_k(p, TOK_LPAREN)) {
                for (;;) {
                    if (at_k(p, TOK_RPAREN)) break;
                    sv_push(&args, dup_text(p, "")); // 占位,raw_arg 追加
                    // raw_arg:连续的 标识符/整数/点
                    cparser* q = p;
                    (void)q;
                    size_t idx = args.n - 1;
                    char tmp[512];
                    size_t tn = 0;
                    for (;;) {
                        if (peek_k(p) == TOK_IDENT) {
                            tn += (size_t)snprintf(tmp + tn, sizeof tmp - tn, "%s", tokp_at(p, 0)->text);
                            bump_tok(p);
                        } else if (peek_k(p) == TOK_INT) {
                            tn += (size_t)snprintf(tmp + tn, sizeof tmp - tn, "%s", tokp_at(p, 0)->text);
                            bump_tok(p);
                        } else if (peek_k(p) == TOK_DOT) {
                            tn += (size_t)snprintf(tmp + tn, sizeof tmp - tn, ".");
                            bump_tok(p);
                            continue;
                        } else break;
                        if (at_k(p, TOK_DOT)) continue;
                        break;
                    }
                    args.d[idx] = ctron_arena_strndup(p->arena, tmp, tn);
                    if (!eat_k(p, TOK_COMMA)) break;
                }
                if (!eat_k(p, TOK_RPAREN)) err_here(p, "E1001", "注解实参表缺少 )");
            }
            a->args = sv_done(&args, p->arena, &a->nargs);
            if (!eat_k(p, TOK_RBRACKET)) err_here(p, "E1001", "注解结束缺少 ]");
        } else if (at_k(p, TOK_AT)) {
            bump_tok(p);
            const char* name = NULL;
            if (peek_k(p) == TOK_IDENT) {
                name = tokp_at(p, 0)->text;
                bump_tok(p);
            } else {
                err_here(p, "E1001", "预期 derive,实际 %s", tok_desc(p));
            }
            if (name && strcmp(name, "derive") != 0)
                err_here(p, "E1001", "未知属性 @%s", name);
            if (!eat_k(p, TOK_LPAREN)) err_here(p, "E1001", "@derive 缺少 (");
            for (;;) {
                if (at_k(p, TOK_RPAREN)) break;
                sv seg = {0};
                size_t nseg;
                if (peek_k(p) == TOK_IDENT) {
                    sv_push(&seg, dup_text(p, tokp_at(p, 0)->text));
                    bump_tok(p);
                } else {
                    err_here(p, "E1001", "预期路径,实际 %s", tok_desc(p));
                }
                while (at_k(p, TOK_DOT)) {
                    bump_tok(p);
                    if (peek_k(p) == TOK_IDENT) {
                        sv_push(&seg, dup_text(p, tokp_at(p, 0)->text));
                        bump_tok(p);
                    } else {
                        err_here(p, "E1001", "路径段缺失,实际 %s", tok_desc(p));
                        break;
                    }
                }
                char** a = sv_done(&seg, p->arena, &nseg);
                // join 为 "a.b.c"
                size_t total = 1;
                for (size_t i = 0; i < nseg; i++) total += strlen(a[i]) + 1;
                char* j = (char*)ctron_arena_alloc(p->arena, total);
                j[0] = '\0';
                for (size_t i = 0; i < nseg; i++) {
                    if (i) strcat(j, ".");
                    strcat(j, a[i]);
                }
                sv_push(derives, j);
                if (!eat_k(p, TOK_COMMA)) break;
            }
            if (!eat_k(p, TOK_RPAREN)) err_here(p, "E1001", "@derive 缺少 )");
        } else {
            break;
        }
    }
}

/// 点分路径 → 段序列(arena)
sv parse_dotted_path_sv(cparser* p) {
    sv path = {0};
    if (peek_k(p) == TOK_IDENT) {
        sv_push(&path, dup_text(p, tokp_at(p, 0)->text));
        bump_tok(p);
    } else {
        err_here(p, "E1001", "预期路径,实际 %s", tok_desc(p));
        return path;
    }
    while (at_k(p, TOK_DOT)) {
        bump_tok(p);
        if (peek_k(p) == TOK_IDENT) {
            sv_push(&path, dup_text(p, tokp_at(p, 0)->text));
            bump_tok(p);
        } else {
            err_here(p, "E1001", "路径段缺失,实际 %s", tok_desc(p));
            break;
        }
    }
    return path;
}

char* expect_ident(cparser* p, const char* ctx) {
    if (peek_k(p) == TOK_IDENT) {
        char* s = dup_text(p, tokp_at(p, 0)->text);
        bump_tok(p);
        return s;
    }
    err_here(p, "E1001", "预期标识符(%s),实际 %s", ctx, tok_desc(p));
    return dup_text(p, "");
}

cfield* parse_field(cparser* p, cvis vis, fdlist* out) {
    cfield* f = fdlist_new(out, p->arena);
    f->vis = vis;
    f->is_var = 0;
    if (eat_k(p, TOK_LET)) { /* let 缺省 */ }
    else if (eat_k(p, TOK_VAR)) { f->is_var = 1; }
    f->name = expect_ident(p, "字段");
    if (!eat_k(p, TOK_COLON)) err_here(p, "E1001", "字段缺少 :");
    f->ty = parse_type(p);
    return f;
}

// ---------- 声明体循环共用 ----------

cfn* parse_fn(cparser* p, cattr** attrs, size_t nattrs, int top_level) {
    cvis vis = top_level ? parse_vis(p) : VIS_PRIVATE;
    int is_comptime = eat_k(p, TOK_COMPTIME);
    char* abi = NULL;
    if (at_k(p, TOK_EXTERN)) {
        bump_tok(p);
        if (peek_k(p) == TOK_STR) {
            const ctron_token* t = tokp_at(p, 0);
            // 取 Text 部件拼接(§9.6 ABI 串);无 Text 则空
            size_t n = 0;
            for (size_t i = 0; i < t->parts.len; i++)
                if (t->parts.items[i].kind == PART_TEXT) n += strlen(t->parts.items[i].s);
            char* buf = (char*)ctron_arena_alloc(p->arena, n + 1);
            buf[0] = '\0';
            for (size_t i = 0; i < t->parts.len; i++)
                if (t->parts.items[i].kind == PART_TEXT) strcat(buf, t->parts.items[i].s);
            abi = buf;
            bump_tok(p);
        } else {
            err_here(p, "E1001", "extern ABI 应为字符串,实际 %s", tok_desc(p));
        }
    }
    if (!eat_k(p, TOK_FN)) err_here(p, "E1001", "预期 fn,实际 %s", tok_desc(p));
    char* name = expect_ident(p, "函数名");
    // 类型参数
    tplist tps = {0};
    if (eat_k(p, TOK_LBRACKET)) {
        for (;;) {
            if (at_k(p, TOK_RBRACKET)) break;
            if (eat_k(p, TOK_COMPTIME)) {
                ctypeparam* tp = tplist_new(&tps, p->arena);
                tp->is_comptime = 1;
                tp->name = expect_ident(p, "comptime 参数");
                if (!eat_k(p, TOK_COLON)) err_here(p, "E1001", "comptime 参数缺少 :");
                parse_type(p);
                tp->bounds = NULL; tp->nbounds = 0;
            } else {
                ctypeparam* tp = tplist_new(&tps, p->arena);
                tp->is_comptime = 0;
                tp->name = expect_ident(p, "类型参数");
                sv bs = {0};
                if (eat_k(p, TOK_COLON)) {
                    for (;;) {
                        sv seg = parse_dotted_path_sv(p);
                        size_t nseg;
                        char** a = sv_done(&seg, p->arena, &nseg);
                        size_t total = 1;
                        for (size_t i = 0; i < nseg; i++) total += strlen(a[i]) + 1;
                        char* j = (char*)ctron_arena_alloc(p->arena, total);
                        j[0] = '\0';
                        for (size_t i = 0; i < nseg; i++) {
                            if (i) strcat(j, ".");
                            strcat(j, a[i]);
                        }
                        sv_push(&bs, j);
                        if (!eat_k(p, TOK_PLUS)) break;
                    }
                }
                tp->bounds = sv_done(&bs, p->arena, &tp->nbounds);
            }
            if (!eat_k(p, TOK_COMMA)) break;
        }
        if (!eat_k(p, TOK_RBRACKET)) err_here(p, "E1001", "类型参数表缺少 ]");
    }
    if (!eat_k(p, TOK_LPAREN)) err_here(p, "E1001", "参数表缺少 (");
    prlist prs = {0};
    int is_variadic = 0;
    for (;;) {
        if (at_k(p, TOK_RPAREN)) break;
        if (at_k(p, TOK_ELLIPSIS)) {
            // "..." 变参尾标(§9.6 v0.7):仅 extern 声明合法(否则 sem E4044)
            bump_tok(p);
            is_variadic = 1;
            break;
        }
        if (at_k(p, TOK_AMP) && tok_at(p, 1) == TOK_SELF) {
            bump_tok(p); bump_tok(p);
            cparam* pr = prlist_new(&prs, p->arena);
            pr->is_receiver = 1; pr->is_var = 0; pr->name = NULL; pr->ty = NULL;
        } else if (at_k(p, TOK_VAR) && tok_at(p, 1) == TOK_SELF) {
            bump_tok(p); bump_tok(p);
            cparam* pr = prlist_new(&prs, p->arena);
            pr->is_receiver = 1; pr->is_var = 1; pr->name = NULL; pr->ty = NULL;
        } else if (at_k(p, TOK_SELF)) {
            // 裸 self(v0.7 §4.7):类型可省(impl for 型),名字固定 self
            bump_tok(p);
            cparam* pr = prlist_new(&prs, p->arena);
            pr->is_receiver = 1; pr->is_var = 0;
            pr->name = dup_text(p, "self");
            pr->ty = eat_k(p, TOK_COLON) ? parse_type(p) : NULL;
        } else {
            cparam* pr = prlist_new(&prs, p->arena);
            pr->is_receiver = 0;
            pr->is_var = eat_k(p, TOK_VAR);
            pr->name = expect_ident(p, "参数");
            if (!eat_k(p, TOK_COLON)) err_here(p, "E1001", "参数缺少 :");
            pr->ty = parse_type(p);
        }
        if (!eat_k(p, TOK_COMMA)) break;
    }
    if (!eat_k(p, TOK_RPAREN)) err_here(p, "E1001", "参数表缺少 )");
    cty* ret = NULL;
    if (eat_k(p, TOK_ARROW)) ret = parse_type(p);
    cblock* body = NULL;
    if (at_k(p, TOK_LBRACE)) body = parse_block(p);

    cfn* f = (cfn*)ctron_arena_alloc(p->arena, sizeof(cfn));
    f->attrs = NULL; f->nattrs = 0;
    if (nattrs) {
        f->attrs = (cattr*)ctron_arena_alloc(p->arena, nattrs * sizeof(cattr));
        for (size_t i = 0; i < nattrs; i++) f->attrs[i] = *attrs[i];
    }
    f->nattrs = nattrs;
    f->vis = vis;
    f->is_comptime = is_comptime;
    f->abi = abi;
    f->variadic = is_variadic;
    f->name = name;
    f->type_params = tplist_done(&tps, p->arena, &f->ntype_params);
    f->params = prlist_done(&prs, p->arena, &f->nparams);
    f->ret = ret;
    f->body = body;
    return f;
}

// prop 声明(共享 trait/class/impl 循环)
void parse_prop(cparser* p, cattr** attrs, size_t nattrs, cvis vis, proplist* out) {
    (void)attrs; (void)nattrs;
    bump_tok(p); // prop
    cprop* pr = proplist_new(out, p->arena);
    pr->vis = vis;
    pr->name = expect_ident(p, "属性");
    if (!eat_k(p, TOK_COLON)) err_here(p, "E1001", "属性缺少 :");
    pr->ty = parse_type(p);
    if (at_k(p, TOK_LBRACE)) pr->body = parse_block(p);
    else pr->body = NULL;
}

// use 声明
void parse_use(cparser* p, cuse* u) {
    bump_tok(p); // use
    sv prefix = {0};
    int group = 0;
    for (;;) {
        if (peek_k(p) == TOK_IDENT) {
            sv_push(&prefix, dup_text(p, tokp_at(p, 0)->text));
            bump_tok(p);
        } else break;
        if (at_k(p, TOK_DOT) && tok_at(p, 1) == TOK_LBRACE) {
            bump_tok(p);
            group = 1;
            break;
        }
        if (!eat_k(p, TOK_DOT)) break;
    }
    iv imports = {0};
    if (group) {
        if (!eat_k(p, TOK_LBRACE)) err_here(p, "E1001", "use 组缺少 {");
        for (;;) {
            if (at_k(p, TOK_RBRACE)) break;
            cimport* imp = (cimport*)ctron_arena_alloc(p->arena, sizeof(cimport));
            // prefix + 段
            sv full = {0};
            for (size_t i = 0; i < prefix.n; i++) sv_push(&full, prefix.d[i]);
            sv seg = parse_dotted_path_sv(p);
            for (size_t i = 0; i < seg.n; i++) sv_push(&full, seg.d[i]);
            free(seg.d);
            imp->segs = sv_done(&full, p->arena, &imp->nsegs);
            if (imports.n == imports.cap) {
                imports.cap = imports.cap ? imports.cap * 2 : 8;
                imports.d = (cimport**)realloc(imports.d, imports.cap * sizeof(cimport*));
                if (!imports.d) abort();
            }
            imports.d[imports.n++] = imp;
            if (!eat_k(p, TOK_COMMA)) break;
        }
        if (!eat_k(p, TOK_RBRACE)) err_here(p, "E1001", "use 组缺少 }");
        u->nimports = imports.n;
        u->imports = NULL;
        if (imports.n) {
            u->imports = (cimport*)ctron_arena_alloc(p->arena, imports.n * sizeof(cimport));
            for (size_t i = 0; i < imports.n; i++) u->imports[i] = *imports.d[i];
        }
        free(imports.d);
    } else {
        cimport* imp = (cimport*)ctron_arena_alloc(p->arena, sizeof(cimport));
        imp->segs = sv_done(&prefix, p->arena, &imp->nsegs);
        u->imports = imp;
        u->nimports = 1;
    }
}

// 顶层声明分发(attr/derive 已收集)
int parse_decl(cparser* p, cattr** attrs, size_t nattrs, sv* derives, cdecl* out) {
    switch (peek_k(p)) {
    case TOK_USE: {
        out->kind = D_USE;
        parse_use(p, &out->use);
        return 1;
    }
    case TOK_STRUCT: {
        bump_tok(p);
        cstruct* s = &out->strukt;
        s->name = expect_ident(p, "结构体");
        tplist tps = {0};
        if (eat_k(p, TOK_LBRACKET)) {
            // 复用:类型参数块解析放行(与 fn 一致)
            for (;;) {
                if (at_k(p, TOK_RBRACKET)) break;
                if (eat_k(p, TOK_COMPTIME)) {
                    ctypeparam* tp = tplist_new(&tps, p->arena);
                    tp->is_comptime = 1;
                    tp->name = expect_ident(p, "comptime 参数");
                    if (!eat_k(p, TOK_COLON)) err_here(p, "E1001", "comptime 参数缺少 :");
                    parse_type(p);
                } else {
                    ctypeparam* tp = tplist_new(&tps, p->arena);
                    tp->is_comptime = 0;
                    tp->name = expect_ident(p, "类型参数");
                    sv bs = {0};
                    if (eat_k(p, TOK_COLON)) {
                        for (;;) {
                            sv seg = parse_dotted_path_sv(p);
                            size_t nseg;
                            char** a = sv_done(&seg, p->arena, &nseg);
                            size_t total = 1;
                            for (size_t i = 0; i < nseg; i++) total += strlen(a[i]) + 1;
                            char* j = (char*)ctron_arena_alloc(p->arena, total);
                            j[0] = '\0';
                            for (size_t i = 0; i < nseg; i++) {
                                if (i) strcat(j, ".");
                                strcat(j, a[i]);
                            }
                            sv_push(&bs, j);
                            if (!eat_k(p, TOK_PLUS)) break;
                        }
                    }
                    tp->bounds = sv_done(&bs, p->arena, &tp->nbounds);
                }
                if (!eat_k(p, TOK_COMMA)) break;
            }
            if (!eat_k(p, TOK_RBRACKET)) err_here(p, "E1001", "类型参数表缺少 ]");
        }
        s->type_params = tplist_done(&tps, p->arena, &s->ntype_params);
        s->attrs = NULL; s->nattrs = 0;
        if (nattrs) {
            s->attrs = (cattr*)ctron_arena_alloc(p->arena, nattrs * sizeof(cattr));
            for (size_t i = 0; i < nattrs; i++) s->attrs[i] = *attrs[i];
        }
        s->nattrs = nattrs;
        s->derives = sv_done(derives, p->arena, &s->nderives);
        if (!eat_k(p, TOK_LBRACE)) err_here(p, "E1001", "结构体缺少 {");
        fdlist fs = {0};
        for (;;) {
            skip_newlines(p);
            if (at_k(p, TOK_RBRACE)) { bump_tok(p); break; }
            if (at_k(p, TOK_EOF)) { err_here(p, "E1001", "未闭合的结构体体"); break; }
            size_t before = p->pos;
            cvis v = parse_vis(p);
            parse_field(p, v, &fs);
            ensure_progress(p, before);
            eat_k(p, TOK_COMMA);
        }
        s->fields = fdlist_done(&fs, p->arena, &s->nfields);
        out->kind = D_STRUCT;
        return 1;
    }
    case TOK_CLASS: {
        bump_tok(p);
        cclass* c = &out->klass;
        c->name = expect_ident(p, "类");
        tplist tps = {0};
        if (eat_k(p, TOK_LBRACKET)) {
            for (;;) {
                if (at_k(p, TOK_RBRACKET)) break;
                if (eat_k(p, TOK_COMPTIME)) {
                    ctypeparam* tp = tplist_new(&tps, p->arena);
                    tp->is_comptime = 1;
                    tp->name = expect_ident(p, "comptime 参数");
                    if (!eat_k(p, TOK_COLON)) err_here(p, "E1001", "comptime 参数缺少 :");
                    parse_type(p);
                } else {
                    ctypeparam* tp = tplist_new(&tps, p->arena);
                    tp->is_comptime = 0;
                    tp->name = expect_ident(p, "类型参数");
                    sv bs = {0};
                    if (eat_k(p, TOK_COLON)) {
                        for (;;) {
                            sv seg = parse_dotted_path_sv(p);
                            size_t nseg;
                            char** a = sv_done(&seg, p->arena, &nseg);
                            size_t total = 1;
                            for (size_t i = 0; i < nseg; i++) total += strlen(a[i]) + 1;
                            char* j = (char*)ctron_arena_alloc(p->arena, total);
                            j[0] = '\0';
                            for (size_t i = 0; i < nseg; i++) {
                                if (i) strcat(j, ".");
                                strcat(j, a[i]);
                            }
                            sv_push(&bs, j);
                            if (!eat_k(p, TOK_PLUS)) break;
                        }
                    }
                    tp->bounds = sv_done(&bs, p->arena, &tp->nbounds);
                }
                if (!eat_k(p, TOK_COMMA)) break;
            }
            if (!eat_k(p, TOK_RBRACKET)) err_here(p, "E1001", "类型参数表缺少 ]");
        }
        c->type_params = tplist_done(&tps, p->arena, &c->ntype_params);
        c->attrs = NULL; c->nattrs = 0;
        if (nattrs) {
            c->attrs = (cattr*)ctron_arena_alloc(p->arena, nattrs * sizeof(cattr));
            for (size_t i = 0; i < nattrs; i++) c->attrs[i] = *attrs[i];
        }
        c->nattrs = nattrs;
        if (!eat_k(p, TOK_LBRACE)) err_here(p, "E1001", "类体缺少 {");
        cilist items = {0};
        for (;;) {
            skip_newlines(p);
            if (at_k(p, TOK_RBRACE)) { bump_tok(p); break; }
            if (at_k(p, TOK_EOF)) { err_here(p, "E1001", "未闭合的类体"); break; }
            cattr** mattrs = NULL;
            size_t nmattrs = 0;
            if (at_k(p, TOK_HASH) || at_k(p, TOK_AT)) {
                alist ma = {0};
                sv md = {0};
                parse_attrs(p, &ma, &md);
                skip_newlines(p);
                nmattrs = ma.n;
                if (nmattrs) {
                    mattrs = (cattr**)ma.d;
                    // 保持数组有效(ma.d 由 alist_new 分配,稍后由 done 释放;需拷贝为独立数组)
                    mattrs = (cattr**)ctron_arena_alloc(p->arena, nmattrs * sizeof(cattr*));
                    for (size_t i = 0; i < nmattrs; i++) mattrs[i] = ma.d[i];
                }
                free(ma.d);
                free(md.d);
            }
            size_t before = p->pos;
            cvis v = parse_vis(p);
            cclassitem* it = cilist_new(&items, p->arena);
            if (at_k(p, TOK_PROP)) {
                proplist pl = {0};
                parse_prop(p, mattrs, nmattrs, v, &pl);
                it->kind = CT_PROP;
                it->p = pl.d[0];
            } else if (at_k(p, TOK_FN)) {
                cfn* f = parse_fn(p, mattrs, nmattrs, 0);
                f->vis = v;
                it->kind = CT_METHOD;
                it->m = f;
            } else {
                fdlist fs = {0};
                parse_field(p, v, &fs);
                it->kind = CT_FIELD;
                it->f = fs.d[0];
            }
            ensure_progress(p, before);
            eat_k(p, TOK_COMMA);
        }
        c->items = cilist_done(&items, p->arena, &c->nitems);
        out->kind = D_CLASS;
        return 1;
    }
    case TOK_ENUM: {
        bump_tok(p);
        cenum* e = &out->en;
        e->name = expect_ident(p, "枚举");
        tplist tps = {0};
        if (eat_k(p, TOK_LBRACKET)) {
            for (;;) {
                if (at_k(p, TOK_RBRACKET)) break;
                if (eat_k(p, TOK_COMPTIME)) {
                    ctypeparam* tp = tplist_new(&tps, p->arena);
                    tp->is_comptime = 1;
                    tp->name = expect_ident(p, "comptime 参数");
                    if (!eat_k(p, TOK_COLON)) err_here(p, "E1001", "comptime 参数缺少 :");
                    parse_type(p);
                } else {
                    ctypeparam* tp = tplist_new(&tps, p->arena);
                    tp->is_comptime = 0;
                    tp->name = expect_ident(p, "类型参数");
                    sv bs = {0};
                    if (eat_k(p, TOK_COLON)) {
                        for (;;) {
                            sv seg = parse_dotted_path_sv(p);
                            size_t nseg;
                            char** a = sv_done(&seg, p->arena, &nseg);
                            size_t total = 1;
                            for (size_t i = 0; i < nseg; i++) total += strlen(a[i]) + 1;
                            char* j = (char*)ctron_arena_alloc(p->arena, total);
                            j[0] = '\0';
                            for (size_t i = 0; i < nseg; i++) {
                                if (i) strcat(j, ".");
                                strcat(j, a[i]);
                            }
                            sv_push(&bs, j);
                            if (!eat_k(p, TOK_PLUS)) break;
                        }
                    }
                    tp->bounds = sv_done(&bs, p->arena, &tp->nbounds);
                }
                if (!eat_k(p, TOK_COMMA)) break;
            }
            if (!eat_k(p, TOK_RBRACKET)) err_here(p, "E1001", "类型参数表缺少 ]");
        }
        e->type_params = tplist_done(&tps, p->arena, &e->ntype_params);
        e->attrs = NULL; e->nattrs = 0;
        if (nattrs) {
            e->attrs = (cattr*)ctron_arena_alloc(p->arena, nattrs * sizeof(cattr));
            for (size_t i = 0; i < nattrs; i++) e->attrs[i] = *attrs[i];
        }
        e->nattrs = nattrs;
        e->derives = sv_done(derives, p->arena, &e->nderives);
        if (!eat_k(p, TOK_LBRACE)) err_here(p, "E1001", "枚举体缺少 {");
        vlist vs = {0};
        for (;;) {
            skip_newlines(p);
            if (at_k(p, TOK_RBRACE)) { bump_tok(p); break; }
            if (at_k(p, TOK_EOF)) { err_here(p, "E1001", "未闭合的枚举体"); break; }
            size_t before = p->pos;
            cvariant* v = vlist_new(&vs, p->arena);
            v->name = expect_ident(p, "枚举变体");
            if (eat_k(p, TOK_LPAREN)) {
                v->kind = VK_TUPLE;
                tlist ts = {0};
                for (;;) {
                    if (at_k(p, TOK_RPAREN)) break;
                    tlist_push(&ts, parse_type(p));
                    if (!eat_k(p, TOK_COMMA)) break;
                }
                if (!eat_k(p, TOK_RPAREN)) err_here(p, "E1001", "变体载荷缺少 )");
                v->tys = tlist_done(&ts, p->arena, &v->ntys);
            } else if (eat_k(p, TOK_LBRACE)) {
                v->kind = VK_STRUCT;
                fdlist fs = {0};
                for (;;) {
                    skip_newlines(p);
                    if (at_k(p, TOK_RBRACE)) { bump_tok(p); break; }
                    if (at_k(p, TOK_EOF)) { err_here(p, "E1001", "未闭合的变体字段"); break; }
                    size_t b = p->pos;
                    parse_field(p, VIS_PRIVATE, &fs);
                    ensure_progress(p, b);
                    eat_k(p, TOK_COMMA);
                }
                v->fields = fdlist_done(&fs, p->arena, &v->nfields);
            } else {
                v->kind = VK_UNIT;
            }
            ensure_progress(p, before);
            eat_k(p, TOK_COMMA);
        }
        e->variants = vlist_done(&vs, p->arena, &e->nvariants);
        out->kind = D_ENUM;
        return 1;
    }
    case TOK_TRAIT: {
        bump_tok(p);
        ctrait* t = &out->trait;
        t->name = expect_ident(p, "trait");
        tplist tps = {0};
        if (eat_k(p, TOK_LBRACKET)) {
            for (;;) {
                if (at_k(p, TOK_RBRACKET)) break;
                if (eat_k(p, TOK_COMPTIME)) {
                    ctypeparam* tp = tplist_new(&tps, p->arena);
                    tp->is_comptime = 1;
                    tp->name = expect_ident(p, "comptime 参数");
                    if (!eat_k(p, TOK_COLON)) err_here(p, "E1001", "comptime 参数缺少 :");
                    parse_type(p);
                } else {
                    ctypeparam* tp = tplist_new(&tps, p->arena);
                    tp->is_comptime = 0;
                    tp->name = expect_ident(p, "类型参数");
                    sv bs = {0};
                    if (eat_k(p, TOK_COLON)) {
                        for (;;) {
                            sv seg = parse_dotted_path_sv(p);
                            size_t nseg;
                            char** a = sv_done(&seg, p->arena, &nseg);
                            size_t total = 1;
                            for (size_t i = 0; i < nseg; i++) total += strlen(a[i]) + 1;
                            char* j = (char*)ctron_arena_alloc(p->arena, total);
                            j[0] = '\0';
                            for (size_t i = 0; i < nseg; i++) {
                                if (i) strcat(j, ".");
                                strcat(j, a[i]);
                            }
                            sv_push(&bs, j);
                            if (!eat_k(p, TOK_PLUS)) break;
                        }
                    }
                    tp->bounds = sv_done(&bs, p->arena, &tp->nbounds);
                }
                if (!eat_k(p, TOK_COMMA)) break;
            }
            if (!eat_k(p, TOK_RBRACKET)) err_here(p, "E1001", "类型参数表缺少 ]");
        }
        t->type_params = tplist_done(&tps, p->arena, &t->ntype_params);
        t->attrs = NULL; t->nattrs = 0;
        if (nattrs) {
            t->attrs = (cattr*)ctron_arena_alloc(p->arena, nattrs * sizeof(cattr));
            for (size_t i = 0; i < nattrs; i++) t->attrs[i] = *attrs[i];
        }
        t->nattrs = nattrs;
        sv sups = {0};
        if (eat_k(p, TOK_COLON)) {
            for (;;) {
                sv seg = parse_dotted_path_sv(p);
                size_t nseg;
                char** a = sv_done(&seg, p->arena, &nseg);
                size_t total = 1;
                for (size_t i = 0; i < nseg; i++) total += strlen(a[i]) + 1;
                char* j = (char*)ctron_arena_alloc(p->arena, total);
                j[0] = '\0';
                for (size_t i = 0; i < nseg; i++) {
                    if (i) strcat(j, ".");
                    strcat(j, a[i]);
                }
                sv_push(&sups, j);
                if (!eat_k(p, TOK_PLUS)) break;
            }
        }
        t->supers = sv_done(&sups, p->arena, &t->nsupers);
        if (!eat_k(p, TOK_LBRACE)) err_here(p, "E1001", "trait 体缺少 {");
        tilist its = {0};
        for (;;) {
            skip_newlines(p);
            if (at_k(p, TOK_RBRACE)) { bump_tok(p); break; }
            if (at_k(p, TOK_EOF)) { err_here(p, "E1001", "未闭合的 trait 体"); break; }
            alist ma = {0};
            sv md = {0};
            parse_attrs(p, &ma, &md);
            skip_newlines(p);
            cattr** mattrs = NULL;
            size_t nmattrs = ma.n;
            if (nmattrs) {
                mattrs = (cattr**)ctron_arena_alloc(p->arena, nmattrs * sizeof(cattr*));
                for (size_t i = 0; i < nmattrs; i++) mattrs[i] = ma.d[i];
            }
            free(ma.d);
            free(md.d);
            size_t before = p->pos;
            cvis v = parse_vis(p);
            ctraititem* it = tilist_new(&its, p->arena);
            if (at_k(p, TOK_PROP)) {
                proplist pl = {0};
                parse_prop(p, mattrs, nmattrs, v, &pl);
                cprop* pr = pl.d[0];
                it->kind = pr->body ? TI_PROPIMPL : TI_PROPSIG;
                it->p = pr;
            } else {
                cfn* f = parse_fn(p, mattrs, nmattrs, 0);
                f->vis = v;
                it->kind = TI_METHOD;
                it->m = f;
            }
            ensure_progress(p, before);
        }
        t->items = tilist_done(&its, p->arena, &t->nitems);
        out->kind = D_TRAIT;
        return 1;
    }
    case TOK_IMPL: {
        bump_tok(p);
        cimpl* im = &out->impl;
        tplist tps = {0};
        if (eat_k(p, TOK_LBRACKET)) {
            for (;;) {
                if (at_k(p, TOK_RBRACKET)) break;
                if (eat_k(p, TOK_COMPTIME)) {
                    ctypeparam* tp = tplist_new(&tps, p->arena);
                    tp->is_comptime = 1;
                    tp->name = expect_ident(p, "comptime 参数");
                    if (!eat_k(p, TOK_COLON)) err_here(p, "E1001", "comptime 参数缺少 :");
                    parse_type(p);
                } else {
                    ctypeparam* tp = tplist_new(&tps, p->arena);
                    tp->is_comptime = 0;
                    tp->name = expect_ident(p, "类型参数");
                    sv bs = {0};
                    if (eat_k(p, TOK_COLON)) {
                        for (;;) {
                            sv seg = parse_dotted_path_sv(p);
                            size_t nseg;
                            char** a = sv_done(&seg, p->arena, &nseg);
                            size_t total = 1;
                            for (size_t i = 0; i < nseg; i++) total += strlen(a[i]) + 1;
                            char* j = (char*)ctron_arena_alloc(p->arena, total);
                            j[0] = '\0';
                            for (size_t i = 0; i < nseg; i++) {
                                if (i) strcat(j, ".");
                                strcat(j, a[i]);
                            }
                            sv_push(&bs, j);
                            if (!eat_k(p, TOK_PLUS)) break;
                        }
                    }
                    tp->bounds = sv_done(&bs, p->arena, &tp->nbounds);
                }
                if (!eat_k(p, TOK_COMMA)) break;
            }
            if (!eat_k(p, TOK_RBRACKET)) err_here(p, "E1001", "类型参数表缺少 ]");
        }
        im->type_params = tplist_done(&tps, p->arena, &im->ntype_params);
        im->trait_ty = parse_type(p);
        if (!eat_k(p, TOK_FOR)) err_here(p, "E1001", "impl 缺少 for");
        im->for_ty = parse_type(p);
        if (!eat_k(p, TOK_LBRACE)) err_here(p, "E1001", "impl 体缺少 {");
        iilist its = {0};
        for (;;) {
            skip_newlines(p);
            if (at_k(p, TOK_RBRACE)) { bump_tok(p); break; }
            if (at_k(p, TOK_EOF)) { err_here(p, "E1001", "未闭合的 impl 体"); break; }
            alist ma = {0};
            sv md = {0};
            parse_attrs(p, &ma, &md);
            skip_newlines(p);
            cattr** mattrs = NULL;
            size_t nmattrs = ma.n;
            if (nmattrs) {
                mattrs = (cattr**)ctron_arena_alloc(p->arena, nmattrs * sizeof(cattr*));
                for (size_t i = 0; i < nmattrs; i++) mattrs[i] = ma.d[i];
            }
            free(ma.d);
            free(md.d);
            cvis v = parse_vis(p);
            cimplitem* it = iilist_new(&its, p->arena);
            if (at_k(p, TOK_PROP)) {
                proplist pl = {0};
                parse_prop(p, mattrs, nmattrs, v, &pl);
                it->kind = II_PROP;
                it->p = pl.d[0];
            } else {
                cfn* f = parse_fn(p, mattrs, nmattrs, 0);
                f->vis = v;
                it->kind = II_METHOD;
                it->m = f;
            }
        }
        im->items = iilist_done(&its, p->arena, &im->nitems);
        out->kind = D_IMPL;
        return 1;
    }
    case TOK_CONST: {
        bump_tok(p);
        cconst* c = &out->konst;
        c->name = expect_ident(p, "常量");
        if (!eat_k(p, TOK_COLON)) err_here(p, "E1001", "常量缺少 :");
        c->ty = parse_type(p);
        if (!eat_k(p, TOK_ASSIGN)) err_here(p, "E1001", "常量缺少 =");
        c->expr = parse_expr(p);
        out->kind = D_CONST;
        return 1;
    }
    case TOK_STATIC: {
        bump_tok(p);
        cstatic* s = &out->statik;
        if (at_k(p, TOK_VAR)) {
            err_here(p, "E3030", "static var 不存在;用 static let 或 Global[T]");
            bump_tok(p);
            s->was_var = 1;
        } else {
            if (!eat_k(p, TOK_LET)) err_here(p, "E1001", "static 声明应为 static let");
            s->was_var = 0;
        }
        s->name = expect_ident(p, "静态");
        if (!eat_k(p, TOK_COLON)) err_here(p, "E1001", "静态缺少 :");
        s->ty = parse_type(p);
        if (!eat_k(p, TOK_ASSIGN)) err_here(p, "E1001", "静态缺少 =");
        s->expr = parse_expr(p);
        out->kind = D_STATIC;
        return 1;
    }
    case TOK_TEST: {
        bump_tok(p);
        ctest* t = &out->test;
        t->name = NULL;
        if (peek_k(p) == TOK_STR) {
            const ctron_token* tk = tokp_at(p, 0);
            size_t n = 0;
            for (size_t i = 0; i < tk->parts.len; i++) n += strlen(tk->parts.items[i].s);
            char* buf = (char*)ctron_arena_alloc(p->arena, n + 1);
            buf[0] = '\0';
            for (size_t i = 0; i < tk->parts.len; i++) strcat(buf, tk->parts.items[i].s);
            t->name = buf;
            bump_tok(p);
        } else {
            err_here(p, "E1001", "预期测试名字符串,实际 %s", tok_desc(p));
            t->name = dup_text(p, "");
        }
        t->body = parse_block(p);
        out->kind = D_TEST;
        return 1;
    }
    case TOK_FN:
    case TOK_PUB:
    case TOK_COMPTIME:
    case TOK_EXTERN: {
        cfn* f = parse_fn(p, attrs, nattrs, 1);
        out->kind = D_FN;
        out->fn_ = *f;
        return 1;
    }
    default:
        err_here(p, "E1001", "顶层应为声明,实际 %s", tok_desc(p));
        bump_tok(p);
        return 0;
    }
}

int parse_file(cparser* p, cfile* f) {
    skip_newlines(p);
    declist ds = {0};
    for (;;) {
        if (at_k(p, TOK_EOF)) break;
        if (at_k(p, TOK_NEWLINE)) { bump_tok(p); continue; }
        alist attrs = {0};
        sv derives = {0};
        parse_attrs(p, &attrs, &derives);
        skip_newlines(p);
        if ((attrs.n != 0 || derives.n != 0)
            && (at_k(p, TOK_HASH) || at_k(p, TOK_AT) || at_k(p, TOK_EOF))) {
            err_here(p, "E1001", "属性后缺少声明");
            free(attrs.d);
            free(derives.d);
            continue;
        }
        // attrs → 数组
        cattr** aarr = NULL;
        size_t narr = attrs.n;
        if (narr) {
            aarr = (cattr**)ctron_arena_alloc(p->arena, narr * sizeof(cattr*));
            for (size_t i = 0; i < narr; i++) aarr[i] = attrs.d[i];
        }
        free(attrs.d);
        cdecl* d = (cdecl*)ctron_arena_alloc(p->arena, sizeof(cdecl));
        memset(d, 0, sizeof(cdecl));
        if (parse_decl(p, aarr, narr, &derives, d)) {
            cdecl* slot = declist_new(&ds, p->arena);
            *slot = *d;
        }
        free(derives.d);
        skip_newlines(p);
    }
    f->decls = declist_done(&ds, p->arena, &f->ndecls);
    return 1;
}
