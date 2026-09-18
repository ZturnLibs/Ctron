#include "trans_internal.h"

// trans_expr.c —— 表达式发射/内建与调用/方法与 trait 分发/泛型单态化(C10-g/h/p)
// ================= 表达式(单次求值发射) =================
// C10-h:trait 参数单态化
// C10-k:并发运行时(scope/spawn/join/Channel/Mutex;eager 内联模型)

// 泛型类型形参绑定:按“声明类型 ↔ 实参类型”结构统一(C10-p)
// 匹配:单名 Named 命中形参名即绑定;二元组按元素递归(元组→元组)。保守:不一致不覆盖首绑。
static void unify_gen(tc* c, const cty* t, ty a, char** names, ty* vals, size_t np) {
    if (!t) return;
    if (t->kind == TY_NAMED && t->npath == 1 && t->nargs == 0) {
        for (size_t j = 0; j < np; j++)
            if (names[j] && !strcmp(names[j], t->path[0])) {
                if (vals[j].k == T_UNK && a.k != T_UNK) vals[j] = a;
                return;
            }
        return;
    }
    if (t->kind == TY_TUPLE && t->nelems == 2 && a.k == T_TUP) {
        unify_gen(c, t->elems[0], tup_elem(a, 0), names, vals, np);
        unify_gen(c, t->elems[1], tup_elem(a, 1), names, vals, np);
        return;
    }
}

// Simd[E, N](C10-q):E 统一 double 承载(对齐 rt v_flt);N 编译期定长
static const char* simd_typedef(tc* c, int n) {
    char name[48], def[160];
    snprintf(name, sizeof name, "ctron_simd_f64_%d", n);
    snprintf(def, sizeof def, "typedef struct { double d[%d]; } %s;\n", n, name);
    use_sum(c, def);
    return ctron_arena_strndup(c->a, name, strlen(name));
}
static ty simd_ty(int n) {
    ty r = ty_unk();
    r.k = T_SIMD;
    r.bits = n;
    r.ek = T_FLT;
    return r;
}

ty emit_expr(tc* c, cexpr* e, sb* o) {
    if (c->err) return ty_unk();
    if (!e) return ty_unk();
    switch (e->kind) {
    case EX_INT: {
        ty t = suff_ty(e->suffix);
        if (t.k == T_FLT) {
            sb_f(o, "(double)(%s)", e->text && *e->text ? e->text : "0");
            return t;
        }
        {
            // 去 _ 分隔 + 进制归一(0x/0o/0b → 十进制发出;超 int64 以 ULL 承载)
            char digits[64];
            size_t di = 0;
            const char* p = e->text && *e->text ? e->text : "0";
            int base = 10;
            if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
            else if (p[0] == '0' && (p[1] == 'o' || p[1] == 'O')) { base = 8; p += 2; }
            else if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) { base = 2; p += 2; }
            for (; *p && di < 63; p++)
                if (*p != '_') digits[di++] = *p;
            digits[di] = 0;
            unsigned long long uv = strtoull(digits, NULL, base);
            if (uv > 9223372036854775807ULL)
                sb_f(o, "(int64_t)(uint64_t)(%lluULL)", uv);
            else
                sb_f(o, "(int64_t)(%llu)", uv);
        }
        return t;
    }
    case EX_STR: {
        int pure_text = 1;
        for (size_t i = 0; i < e->nsparts; i++)
            if (e->sparts[i].kind != PART_TEXT) pure_text = 0;
        if (pure_text) {
            sb_s(o, "\"");
            for (size_t i = 0; i < e->nsparts; i++) {
                const char* p = e->sparts[i].s ? e->sparts[i].s : "";
                for (; *p; p++) {
                    if (*p == '"' || *p == '\\') sb_c(o, '\\');
                    if (*p == '\n') { sb_s(o, "\\n"); continue; }
                    sb_c(o, *p);
                }
            }
            sb_s(o, "\"");
            return ty_str();
        }
        // 插值:逐部件 emit,格式化为串后 concat 链(rt fmt_val 语义)
        sb chain = {0};
        int nseg = 0;
        for (size_t i = 0; i < e->nsparts && !c->err; i++) {
            const ctron_str_part* sp = &e->sparts[i];
            sb seg = {0};
            if (sp->kind == PART_TEXT) {
                sb_s(&seg, "\"");
                const char* p = sp->s ? sp->s : "";
                for (; *p; p++) {
                    if (*p == '"' || *p == '\\') sb_c(&seg, '\\');
                    sb_c(&seg, *p);
                }
                sb_s(&seg, "\"");
            } else {
                // 片段:解析为表达式(镜像 rt interp_raw 的片段解析)
                char* tmp = (char*)malloc(strlen(sp->s ? sp->s : "") + 64);
                snprintf(tmp, strlen(sp->s ? sp->s : "") + 64, "fn __ip() { return %s }", sp->s ? sp->s : "0");
                ctron_parse_result ip = ctron_parse_src(tmp, strlen(tmp));
                free(tmp);
                cexpr* px = NULL;
                if (ip.file && ip.file->ndecls >= 1 && ip.file->decls[0].kind == D_FN
                    && ip.file->decls[0].fn_.body) {
                    cblock* pb = ip.file->decls[0].fn_.body;
                    if (pb->tail) px = pb->tail;
                    else if (pb->nstmts >= 1 && pb->stmts[pb->nstmts - 1]->kind == ST_RET)
                        px = pb->stmts[pb->nstmts - 1]->e; // return <片段>
                }
                if (!px) { terr(c, "v1:插值片段解析失败"); ctron_parse_result_free(&ip); sb_free(&seg); break; }
                sb v = {0};
                ty vt = emit_expr(c, px, &v);
                if (vt.k == T_INT) { use_helper(c, "ctron_fmt_i64"); sb_f(&seg, "ctron_fmt_i64(%s)", v.d ? v.d : "0"); }
                else if (vt.k == T_FLT) { use_helper(c, "ctron_fmt_f64"); sb_f(&seg, "ctron_fmt_f64(%s)", v.d ? v.d : "0.0"); }
                else if (vt.k == T_BOOL) { use_helper(c, "ctron_fmt_bool"); sb_f(&seg, "ctron_fmt_bool(%s)", v.d ? v.d : "0"); }
                else if (vt.k == T_STR) sb_s(&seg, v.d ? v.d : "\"\"");
                else terr(c, "v1:插值片段类型不支持(kind %d 片段 %s)", vt.k, sp->s ? sp->s : "?");
                sb_free(&v);
                ctron_parse_result_free(&ip);
            }
            if (c->err) { sb_free(&seg); break; }
            if (nseg == 0) {
                sb_s(&chain, seg.d ? seg.d : "");
            } else {
                use_helper(c, "ctron_str_concat");
                sb nc = {0};
                sb_f(&nc, "ctron_str_concat(%s, %s)", chain.d ? chain.d : "", seg.d ? seg.d : "");
                sb_free(&chain);
                chain = nc;
            }
            nseg++;
            nseg++;
            sb_free(&seg);
        }
        if (c->err) { sb_free(&chain); return ty_unk(); }
        if (nseg == 0) { sb_s(o, "\"\""); sb_free(&chain); return ty_str(); }
        sb_s(o, chain.d ? chain.d : "\"\"");
        sb_free(&chain);
        return ty_str();
    }
    case EX_FLOAT:
        sb_f(o, "(double)(%s)", e->text && *e->text ? e->text : "0");
        return ty_flt();
    case EX_BOOL:
        sb_f(o, "%d", e->bval ? 1 : 0);
        return ty_bool();
    case EX_VOID:
        sb_s(o, "0");
        return ty_unk();
    case EX_TUPLE: {
        // 二元组字面量(C10-p):typedef 结构按实例生成;struct 载荷 v1 拒绝(tup_ty)
        if (e->nelems != 2) { terr(c, "v1:仅支持二元组字面量"); return ty_unk(); }
        sb t0 = {0}, t1 = {0};
        ty a = emit_expr(c, e->elems[0], &t0);
        ty b = emit_expr(c, e->elems[1], &t1);
        if (c->err) { sb_free(&t0); sb_free(&t1); return ty_unk(); }
        ty r = tup_ty(c, a, b);
        if (r.k != T_TUP) { terr(c, "v1:元组元素类型不支持"); sb_free(&t0); sb_free(&t1); return ty_unk(); }
        sb_f(o, "(%s){ .e0 = %s, .e1 = %s }", r.tname ? r.tname : "0", t0.d ? t0.d : "0", t1.d ? t1.d : "0");
        sb_free(&t0);
        sb_free(&t1);
        return r;
    }
    case EX_IDENT: {
        ty t;
        if (scope_find(c, e->text, &t)) {
            sb_s(o, e->text);
            return t;
        }
        for (size_t gi = 0; gi < c->n_globals; gi++)
            if (!strcmp(c->globals[gi].name, e->text)) {
                sb_s(o, e->text);
                return c->globals[gi].t;
            }
        // 函数引用(函数一等公民,如 opt.map(twice);泛型/trait 形参原体除外)
        const cfn* rf = fn_named(c->srcf, e->text);
        if (rf && !fn_has_trait_param(c, rf) && !fn_is_generic(rf)) {
            sb_f(o, "(ctron_fnptr)ctron_user_%s", e->text);
            return ty_fnptr();
        }
        // None(期望 Option)
        if (!strcmp(e->text, "None") && c->want && c->want->k == T_SUM
            && !strncmp(c->want->tname, "ctron_opt_", 10)) {
            sb_f(o, "(%s){ .tag = CTRON_OPT_NONE }", c->want->tname);
            return *c->want;
        }
        // 裸变体(用户 enum 单元变体;须全文件唯一)
        for (size_t i = 0; i < c->nenums; i++) {
            edef* ed = &c->enums[i];
            for (size_t j = 0; j < ed->n; j++)
                if (!strcmp(ed->variants[j].name, e->text)) {
                    sb_f(o, "(ctron_e_%s){ .tag = CTRON_%s_%s }", ed->name, ed->name, e->text);
                    ty r = ty_unk(); r.k = T_ENUM; r.tname = ed->name;
                    return r;
                }
        }
        terr(c, "v1 未解析名称:%s", e->text);
        return ty_unk();
    }
    case EX_MEMBER: {
        if (!e->m_is_name) { // 元组下标 .0/.1(C10-p;对齐 rt TupleIndex 语义)
            sb tb = {0};
            ty tt = emit_expr(c, e->obj, &tb);
            if (c->err) { sb_free(&tb); return ty_unk(); }
            if (tt.k != T_TUP || e->mtuple > 1) { terr(c, "v1:元组下标目标不支持"); sb_free(&tb); return ty_unk(); }
            sb_f(o, "(%s).e%u", tb.d ? tb.d : "0", e->mtuple);
            sb_free(&tb);
            return tup_elem(tt, (int)e->mtuple);
        }
        const char* m = e->mname;
        sb ob = {0};
        ty ot = emit_expr(c, e->obj, &ob);
        if (c->err) { sb_free(&ob); return ty_unk(); }
        const char* ov = ob.d ? ob.d : "0";
        if (ot.k == T_STRUCT || ot.k == T_CLASS) {
            // 字段
            if (ot.k == T_STRUCT) {
                sdef* sd = &c->structs[ot.bits];
                for (size_t i = 0; i < sd->n; i++)
                    if (!strcmp(sd->fields[i].name, m)) {
                        sb_f(o, "(%s).%s", ov, m);
                        ty r = sd->fields[i].t;
                        sb_free(&ob);
                        return r;
                    }
            } else {
                cdef* cd = &c->classes[ot.bits];
                for (size_t i = 0; i < cd->n; i++)
                    if (!strcmp(cd->fields[i].name, m)) {
                        sb_f(o, "(%s)->%s", ov, m);
                        ty r = cd->fields[i].t;
                        sb_free(&ob);
                        return r;
                    }
            }
            // prop 访问器(impl prop / trait 默认 prop);ensure 内部去重
            char pn[192];
            ty pret;
            if (ensure_prop_accessor(c, ot.tname, m, pn, &pret)) {
                sb_f(o, "%s(%s)", pn, ov);
                sb_free(&ob);
                return pret;
            }
            terr(c, "v1:%s 无字段/prop %s", ot.tname, m);
            sb_free(&ob);
            return ty_unk();
        }
        if (ot.k == T_CLASS) {
            cdef* cd = &c->classes[ot.bits];
            for (size_t i = 0; i < cd->n; i++)
                if (!strcmp(cd->fields[i].name, m)) {
                    sb_f(o, "(%s)->%s", ov, m);
                    ty r = cd->fields[i].t;
                    sb_free(&ob);
                    return r;
                }
            terr(c, "v1:class %s 无字段 %s", cd->name, m);
            sb_free(&ob);
            return ty_unk();
        }
        if (ot.k == T_BOX) {
            ty et = box_elem(ot);
            if (et.k == T_STRUCT || et.k == T_CLASS) {
                for (size_t i = 0; i < c->nstructs; i++)
                    if (!strcmp(c->structs[i].name, et.tname ? et.tname : "")) {
                        sdef* sd = &c->structs[i];
                        for (size_t k2 = 0; k2 < sd->n; k2++)
                            if (!strcmp(sd->fields[k2].name, m)) {
                                sb_f(o, "(%s)->%s", ov, m);
                                ty r = sd->fields[k2].t;
                                sb_free(&ob);
                                return r;
                            }
                    }
            }
        }
        if ((ot.k == T_ARR || ot.k == T_LIST) && !strcmp(m, "len")) {
            sb_f(o, "((%s).n)", ov);
            sb_free(&ob);
            return ty_int(64, 0);
        }
        if (ot.k == T_STR) {
            if (!strcmp(m, "len")) { use_helper(c, "ctron_str_len"); sb_f(o, "ctron_str_len(%s)", ov); sb_free(&ob); return ty_int(64, 0); }
            if (!strcmp(m, "char_len")) { use_helper(c, "ctron_str_char_len"); sb_f(o, "ctron_str_char_len(%s)", ov); sb_free(&ob); return ty_int(64, 0); }
            if (!strcmp(m, "to_string")) { sb_s(o, ov); sb_free(&ob); return ty_str(); }
        }
        if (ot.k == T_ERR) {
            // AnyError 链节点属性(C10-i):message/cause/trace(rt EX_MEMBER V_ERR 语义)
            if (!strcmp(m, "message")) { sb_f(o, "(%s)->message", ov); sb_free(&ob); return ty_str(); }
            if (!strcmp(m, "trace")) { sb_f(o, "(%s)->trace", ov); sb_free(&ob); return ty_str(); }
            if (!strcmp(m, "cause")) {
                use_sum(c, "typedef struct { int tag; union { ctron_anyerr* some; } as; } ctron_opt_err;\n"
                           "#define CTRON_OPT_NONE 0\n#define CTRON_OPT_SOME 1\n");
                sb_f(o, "(ctron_opt_err){ .tag = (%s)->cause ? CTRON_OPT_SOME : CTRON_OPT_NONE, .as.some = (%s)->cause }", ov, ov);
                ty r = ty_unk(); r.k = T_SUM; r.tname = "ctron_opt_err"; r.ek = T_ERR;
                sb_free(&ob);
                return r;
            }
            terr(c, "v1:错误链属性不支持: %s", m);
            sb_free(&ob);
            return ty_unk();
        }
        terr(c, "v1:成员 .%s 不支持(目标类型 %d)", m, (int)ot.k);
        sb_free(&ob);
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
        if (e->bop == B_OROR) {
            // v0.7 修订一:|| 直映 C(C 原生短路);镜像 && 括号化
            sb l2 = {0}, r2 = {0};
            emit_expr(c, e->lhs, &l2);
            emit_expr(c, e->rhs, &r2);
            sb_f(o, "((%s) || (%s))", l2.d ? l2.d : "0", r2.d ? r2.d : "0");
            sb_free(&l2);
            sb_free(&r2);
            return ty_unk();
        }
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
        // B_OR = or 取默认(§4.4;run 解释执行支持,直映发射 v1 未接)
        if (e->bop == B_OR) { terr(c, "v1:or 取默认未接直映发射(§4.4);用 run 解释执行"); return ty_unk(); }
        sb l = {0}, r = {0};
        ty lt = emit_expr(c, e->lhs, &l);
        ty rt = emit_expr(c, e->rhs, &r);
        if (c->err) { sb_free(&l); sb_free(&r); return ty_unk(); }
        if (e->bop >= B_EQ && e->bop <= B_GE) {
            const char* op = e->bop == B_EQ ? "==" : e->bop == B_NE ? "!="
                            : e->bop == B_LT ? "<" : e->bop == B_GT ? ">" : e->bop == B_LE ? "<=" : ">=";
            if (lt.k == T_STR && rt.k == T_STR) {
                use_helper(c, "ctron_str_cmp");
                const char* c2 = !strcmp(op, "==") ? "==" : !strcmp(op, "!=") ? "!="
                                : !strcmp(op, "<") ? "<" : !strcmp(op, ">") ? ">" : !strcmp(op, "<=") ? "<=" : ">=";
                sb_f(o, "(ctron_str_cmp(%s, %s) %s 0)", l.d ? l.d : "0", r.d ? r.d : "0", c2);
            } else if (lt.k == T_INT && rt.k == T_INT)
                sb_f(o, "((__int128)(%s) %s (__int128)(%s))", l.d ? l.d : "0", op, r.d ? r.d : "0");
            else
                sb_f(o, "((%s) %s (%s))", l.d ? l.d : "0", op, r.d ? r.d : "0");
            sb_free(&l);
            sb_free(&r);
            return ty_bool();
        }
        if ((lt.k == T_STR || rt.k == T_STR) && e->bop == B_ADD) {
            if (lt.k != T_STR || rt.k != T_STR) { terr(c, "v1:Str 拼接需两侧 Str"); sb_free(&l); sb_free(&r); return ty_unk(); }
            use_helper(c, "ctron_str_concat");
            sb_f(o, "ctron_str_concat(%s, %s)", l.d ? l.d : "\"\"", r.d ? r.d : "\"\"");
            sb_free(&l);
            sb_free(&r);
            return ty_str();
        }
        if (lt.k == T_SIMD || rt.k == T_SIMD) {
            // Simd 元素级白名单运算(§9.5:+ - * /;两侧同长,对齐 rt)
            if (lt.k != T_SIMD || rt.k != T_SIMD) { terr(c, "v1:Simd 运算需两侧同为 Simd"); sb_free(&l); sb_free(&r); return ty_unk(); }
            if (lt.bits != rt.bits) { terr(c, "v1:Simd 元素数不一致"); sb_free(&l); sb_free(&r); return ty_unk(); }
            if (e->bop != B_ADD && e->bop != B_SUB && e->bop != B_MUL && e->bop != B_DIV) {
                terr(c, "v1:Simd 元素级运算不支持该算符");
                sb_free(&l); sb_free(&r);
                return ty_unk();
            }
            int n = (int)lt.bits;
            const char* tdn = simd_typedef(c, n);
            const char* op = e->bop == B_ADD ? "+" : e->bop == B_SUB ? "-" : e->bop == B_MUL ? "*" : "/";
            int u = c->tmpn++;
            sb_f(o, "({ %s ctron_sa%d = %s; %s ctron_sb%d = %s; %s ctron_sr%d; "
                    "for (int i = 0; i < %d; i++) ctron_sr%d.d[i] = ctron_sa%d.d[i] %s ctron_sb%d.d[i]; "
                    "ctron_sr%d; })",
                 tdn, u, l.d ? l.d : "0", tdn, u, r.d ? r.d : "0", tdn, u,
                 n, u, u, op, u, u);
            sb_free(&l);
            sb_free(&r);
            return simd_ty(n);
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
    case EX_ARRAY: {
        if (e->nelems == 0) { terr(c, "v1:空数组字面量需注解推导(v1 拒绝)"); return ty_unk(); }
        sb elems = {0};
        ty et = ty_unk();
        if (c->want && c->want->k == T_ARR) et = e2_of(*c->want); // 注解元素类型优先(对齐 rt coerce)
        for (size_t i = 0; i < e->nelems; i++) {
            if (i) sb_s(&elems, ", ");
            sb a1 = {0};
            ty it = emit_expr(c, e->elems[i], &a1);
            if (et.k == T_UNK) et = it;
            else if (it.k != T_UNK && it.k != et.k)
                terr(c, "v1:数组字面量元素类型不一致"); // 注解优先:宽度/符号差异经 decl 助手自适应
            sb_s(&elems, a1.d ? a1.d : "0");
            sb_free(&a1);
        }
        if (c->err) { sb_free(&elems); return ty_unk(); }
        if (et.k != T_INT && et.k != T_FLT && et.k != T_BOOL && et.k != T_STR) {
            terr(c, "v1:数组元素类型不支持");
            sb_free(&elems);
            return ty_unk();
        }
        char wl[16];
        if (et.k == T_STR) snprintf(wl, sizeof wl, "str");
        else snprintf(wl, sizeof wl, "%s", wlname(et));
        use_arr(c, wl);
        char h[64];
        snprintf(h, sizeof h, "ctron_arr_%s_lit", wl);
        use_helper(c, h);

        const char* lit_t = et.k == T_STR ? "const char*"
                          : et.k == T_FLT ? "double"
                          : (et.us && et.bits >= 64) ? "uint64_t" : "int64_t";
        sb_f(o, "%s(%lld, (%s[]){%s})", h, (long long)e->nelems, lit_t, elems.d ? elems.d : "");
        sb_free(&elems);
        return ty_arr(et);
    }
    case EX_STRUCT: {
        if (e->npath == 0) { terr(c, "v1:构造缺类型名"); return ty_unk(); }
        const char* tn = e->path[0];
        // 泛型 struct/class 字面量:按字段实参推导类型实参 → 注册单态化实例(C10-p)
        const cstruct* gs = NULL;
        for (size_t i = 0; i < c->srcf->ndecls; i++) {
            const cdecl* d = &c->srcf->decls[i];
            if (d->kind == D_STRUCT && d->strukt.ntype_params > 0 && d->strukt.name
                && !strcmp(d->strukt.name, tn)) { gs = &d->strukt; break; }
        }
        const cclass* gc = NULL;
        for (size_t i = 0; !gs && i < c->srcf->ndecls; i++) {
            const cdecl* d = &c->srcf->decls[i];
            if (d->kind == D_CLASS && d->klass.ntype_params > 0 && d->klass.name
                && !strcmp(d->klass.name, tn)) { gc = &d->klass; break; }
        }
        if (gs || gc) {
            size_t np = gs ? gs->ntype_params : gc->ntype_params;
            if (np == 0 || np > 4) { terr(c, "v1:泛型类型实参数:%s", tn); return ty_unk(); }
            char* pnames[4] = {0};
            ty pvals[4];
            memset(pvals, 0, sizeof pvals);
            for (size_t j = 0; j < np; j++)
                pnames[j] = gs ? gs->type_params[j].name : gc->type_params[j].name;
            size_t nfields = gs ? gs->nfields : 0;
            size_t citems = gc ? gc->nitems : 0;
            // 字段实参求值(单次);同时按“泛型字段声明 ↔ 实参类型”绑定类型形参
            sb fvals[MAX_FIELDS];
            memset(fvals, 0, sizeof fvals);
            size_t nf = 0;
            for (size_t j = 0; j < (gc ? citems : nfields) && j < MAX_FIELDS; j++) {
                const cfield* fl = gs ? &gs->fields[j]
                    : (gc->items[j].kind == CT_FIELD ? gc->items[j].f : NULL);
                if (gc && !fl) continue;
                cexpr* fv = NULL;
                for (size_t i2 = 0; i2 < e->nfields; i2++)
                    if (e->fields[i2].name && fl->name && !strcmp(e->fields[i2].name, fl->name)) fv = e->fields[i2].value;
                const ty* sw4 = c->want;
                c->want = NULL;
                ty at = emit_expr(c, fv, &fvals[nf]); // 缺字段初值:v1 要求全字段(构造字面量诚实拒绝)
                c->want = sw4;
                if (c->err) break;
                unify_gen(c, fl->ty, at, pnames, pvals, np);
                nf++;
            }
            if (c->err) { for (size_t j = 0; j < MAX_FIELDS; j++) sb_free(&fvals[j]); return ty_unk(); }
            // 单态化名:<Tn>__<M1>_…<Mn>;未绑定形参 → 拒绝
            sb mn = {0};
            sb_f(&mn, "%s", tn);
            int unbound = 0;
            for (size_t j = 0; j < np; j++) {
                if (pvals[j].k == T_UNK) { terr(c, "v1:泛型实参 %s 无法推导:%s", pnames[j] ? pnames[j] : "?", tn); unbound = 1; break; }
                char* mk = ty_mangle(c, pvals[j]);
                sb_f(&mn, "__%.30s", mk ? mk : "?");
            }
            if (unbound) { sb_free(&mn); for (size_t j = 0; j < MAX_FIELDS; j++) sb_free(&fvals[j]); return ty_unk(); }
            const char* mname = mn.d ? mn.d : tn;
            if (gs) {
                // 泛型 struct:实例注册进 structs 表(typedef/字段访问/drop 检查统一走既有路径)
                int sidx = -1;
                for (size_t i = 0; i < c->nstructs; i++)
                    if (!strcmp(c->structs[i].name, mname)) { sidx = (int)i; break; }
                if (sidx < 0 && c->nstructs < MAX_TYPES) {
                    size_t saved = c->n_subs;
                    for (size_t j = 0; j < np && c->n_subs < 8; j++) {
                        c->subs[c->n_subs].from = pnames[j];
                        c->subs[c->n_subs].to = pvals[j];
                        c->n_subs++;
                    }
                    sdef* sd = &c->structs[c->nstructs];
                    memset(sd, 0, sizeof *sd);
                    sd->name = ctron_arena_strndup(c->a, mname, strlen(mname));
                    for (size_t j = 0; j < gs->nfields && sd->n < MAX_FIELDS; j++) {
                        sd->fields[sd->n].name = gs->fields[j].name;
                        sd->fields[sd->n].t = decl_ty_tc(c, gs->fields[j].ty); // subs 活跃:解析具体字段类型
                        sd->n++;
                    }
                    c->nstructs++;
                    c->n_subs = saved;
                }
                sb_f(o, "(ctron_t_%s){", mname);
                size_t fi = 0;
                for (size_t j = 0; j < gs->nfields; j++) {
                    if (j) sb_s(o, ", ");
                    sb_s(o, fvals[fi].d ? fvals[fi].d : "0");
                    fi++;
                }
                sb_s(o, "}");
                ty r = ty_unk();
                r.k = T_STRUCT;
                r.tname = ctron_arena_strndup(c->a, mname, strlen(mname));
                for (size_t i = 0; i < c->nstructs; i++) // 字段访问按索引取 sdef
                    if (!strcmp(c->structs[i].name, mname)) { r.bits = (int)i; break; }
                for (size_t j = 0; j < MAX_FIELDS; j++) sb_free(&fvals[j]);
                sb_free(&mn);
                return r;
            }
            // 泛型 class:实例注册进 classes 表 + malloc 构造助手
            int cidx = -1;
            for (size_t i = 0; i < c->nclasses; i++)
                if (!strcmp(c->classes[i].name, mname)) { cidx = (int)i; break; }
            if (cidx < 0 && c->nclasses < MAX_TYPES) {
                size_t saved = c->n_subs;
                for (size_t j = 0; j < np && c->n_subs < 8; j++) {
                    c->subs[c->n_subs].from = pnames[j];
                    c->subs[c->n_subs].to = pvals[j];
                    c->n_subs++;
                }
                cdef* cd = &c->classes[c->nclasses];
                memset(cd, 0, sizeof *cd);
                cd->name = ctron_arena_strndup(c->a, mname, strlen(mname));
                for (size_t j = 0; j < gc->nitems && cd->n < MAX_FIELDS; j++) {
                    if (gc->items[j].kind != CT_FIELD || !gc->items[j].f) continue;
                    cd->fields[cd->n].name = gc->items[j].f->name;
                    cd->fields[cd->n].t = decl_ty_tc(c, gc->items[j].f->ty);
                    cd->n++;
                }
                c->nclasses++;
                c->n_subs = saved;
            }
            char nh[96];
            snprintf(nh, sizeof nh, "ctron_new_%s", mname);
            use_helper(c, nh);
            sb_f(o, "ctron_new_%s(", mname);
            for (size_t j = 0; j < nf; j++) {
                if (j) sb_s(o, ", ");
                sb_s(o, fvals[j].d ? fvals[j].d : "0");
            }
            sb_s(o, ")");
            ty rc = ty_unk();
            rc.k = T_CLASS;
            rc.tname = ctron_arena_strndup(c->a, mname, strlen(mname));
            for (size_t i = 0; i < c->nclasses; i++) // 字段访问按索引取 cdef
                if (!strcmp(c->classes[i].name, mname)) { rc.bits = (int)i; break; }
            for (size_t j = 0; j < MAX_FIELDS; j++) sb_free(&fvals[j]);
            sb_free(&mn);
            return rc;
        }
        int is_cls = 0;
        for (size_t i = 0; i < c->nclasses; i++)
            if (!strcmp(c->classes[i].name, tn)) is_cls = 1;
        if (is_cls) {
            // class:malloc + 字段赋值(引用语义)
            char nh[96];
            snprintf(nh, sizeof nh, "ctron_new_%s", tn);
            use_helper(c, nh);
            sb_f(o, "ctron_new_%s(", tn);
            sdef* cds = NULL;
            int cdidx = 0;
            for (size_t i = 0; i < c->nclasses; i++)
                if (!strcmp(c->classes[i].name, tn)) { cds = (sdef*)&c->classes[i]; cdidx = (int)i; break; }
            for (size_t j = 0; cds && j < cds->n; j++) {
                if (j) sb_s(o, ", ");
                cexpr* fv = NULL;
                for (size_t i = 0; i < e->nfields; i++)
                    if (e->fields[i].name && !strcmp(e->fields[i].name, cds->fields[j].name)) fv = e->fields[i].value;
                if (fv) emit_expr(c, fv, o);
                else sb_s(o, "0");
            }
            sb_s(o, ")");
            ty r = ty_unk(); r.k = T_CLASS; r.tname = tn; r.bits = cdidx;
            return r;
        }
        sdef* sd = NULL;
        int sdidx = 0;
        for (size_t i = 0; i < c->nstructs; i++)
            if (!strcmp(c->structs[i].name, tn)) { sd = &c->structs[i]; sdidx = (int)i; break; }
        if (!sd) { terr(c, "v1:构造目标需 struct:%s", tn); return ty_unk(); }
        sb_f(o, "(ctron_t_%s){", tn);
        for (size_t j = 0; j < sd->n; j++) {
            if (j) sb_s(o, ", ");
            cexpr* fv = NULL;
            for (size_t i = 0; i < e->nfields; i++)
                if (e->fields[i].name && !strcmp(e->fields[i].name, sd->fields[j].name)) fv = e->fields[i].value;
            if (fv) emit_expr(c, fv, o);
            else sb_s(o, "0");
        }
        sb_s(o, "}");
        ty r = ty_unk();
        r.k = T_STRUCT;
        r.tname = sd->name;
        r.bits = sdidx;
        return r;
    }
    case EX_INDEX: {
        // v1:obj 仅标识符(数组);str 索引走 byte_at
        if (!e->obj || e->obj->kind != EX_IDENT) { terr(c, "v1:索引目标仅标识符"); return ty_unk(); }
        ty t;
        if (!scope_find(c, e->obj->text, &t)) { terr(c, "v1 未解析名称:%s", e->obj->text); return ty_unk(); }
        if (t.k != T_ARR && t.k != T_LIST) { terr(c, "v1:索引目标需数组/列表"); return ty_unk(); }
        use_helper(c, "ctron_idx");
        sb ix = {0};
        emit_expr(c, e->index, &ix);
        if (c->err) { sb_free(&ix); return ty_unk(); }
        sb_f(o, "%s.d[ctron_idx(%s.n, %s)]", e->obj->text, e->obj->text, ix.d ? ix.d : "0");
        sb_free(&ix);
        ty e2 = ty_int(t.ebits, t.eus);
        if (t.ek == T_FLT) e2 = ty_flt();
        if (t.ek == T_BOOL) e2 = ty_bool();
        if (t.ek == T_STR) e2 = ty_str();
        return e2;
    }
    case EX_RANGE: {
        // range 作为值(let r = 0..4;for i in r)
        sb a = {0}, b = {0};
        emit_expr(c, e->from, &a);
        emit_expr(c, e->to, &b);
        sb_f(o, "(ctron_rng){ .lo = (int64_t)(%s), .hi = (int64_t)(%s), .incl = %d }",
             a.d ? a.d : "0", b.d ? b.d : "0", e->inclusive ? 1 : 0);
        sb_free(&a);
        sb_free(&b);
        ty r = ty_unk(); r.k = T_RANGE;
        return r;
    }
    case EX_CALL: {
        cexpr* cal = e->callee;
        // 函数值调用(fn 类型形参 / 闭包值;int64 统一 ABI,无原型)
        if (cal && cal->kind == EX_IDENT) {
            ty ft;
            if (scope_find(c, cal->text, &ft) && ft.k == T_FNPTR) {
                sb_f(o, "((ctron_fnptr)(%s))(", cal->text);
                for (size_t i = 0; i < e->nelems; i++) {
                    if (i) sb_s(o, ", ");
                    sb a1 = {0};
                    emit_expr(c, e->elems[i], &a1);
                    sb_f(o, "(int64_t)(%s)", a1.d ? a1.d : "0");
                    sb_free(&a1);
                }
                sb_s(o, ")");
                return ty_int(64, 0);
            }
        }
        // C10-k:并发成员调用(scope.spawn / task.join·join_or / chan.send·recv / mutex.with·with_mut)
        if (cal && cal->kind == EX_MEMBER && cal->m_is_name && cal->mname
            && cal->obj && cal->obj->kind == EX_IDENT) {
            ty rt;
            if (scope_find(c, cal->obj->text, &rt)) {
                if (rt.k == T_SCOPE && !strcmp(cal->mname, "spawn"))
                    return emit_spawn_call(c, e, o);
                if (rt.k == T_TASK && (!strcmp(cal->mname, "join") || !strcmp(cal->mname, "join_or")))
                    return emit_task_call(c, e, cal->obj->text, o);
                if (rt.k == T_CHAN && (!strcmp(cal->mname, "send") || !strcmp(cal->mname, "recv")))
                    return emit_chan_call(c, e, cal->obj->text, o);
                if (rt.k == T_MUTEX && (!strcmp(cal->mname, "with") || !strcmp(cal->mname, "with_mut")))
                    return emit_mutex_call(c, e, rt, cal->obj->text, o);
                if (rt.k == T_MUTEX && cal->mname
                    && (!strcmp(cal->mname, "load") || !strcmp(cal->mname, "store") || !strcmp(cal->mname, "fetch_add")))
                    return emit_atomic_call(c, e, rt, cal->obj->text, o);
            }
        }
        // C10-m:parallel 命名空间(§7.7;解释器为序贯形态,v1 同构发射)
        if (cal && cal->kind == EX_MEMBER && cal->m_is_name && cal->mname
            && cal->obj && cal->obj->kind == EX_IDENT && !strcmp(cal->obj->text, "parallel")) {
            if (!strcmp(cal->mname, "map") && e->nelems == 2) return emit_parallel_map(c, e, o);
            if (!strcmp(cal->mname, "reduce") && e->nelems == 3) return emit_parallel_reduce(c, e, o);
            terr(c, "v1:parallel.%s 需 map(切片, fn)/reduce(切片, 初值, fn)", cal->mname ? cal->mname : "?");
            return ty_unk();
        }
        // C10-n:bare 档 arena(§5.6):Arena.fixed(n) 无状态句柄;arena.zeros[T](n) 零数组(值等价 rt)
        if (cal && cal->kind == EX_MEMBER && cal->m_is_name && cal->mname
            && cal->obj && cal->obj->kind == EX_IDENT && !strcmp(cal->obj->text, "Arena")
            && !strcmp(cal->mname, "fixed")) {
            sb_f(o, "(void*)0");
            return ty_arena();
        }
        if (cal && cal->kind == EX_TYPEARGS && cal->ntargs == 1
            && cal->obj && cal->obj->kind == EX_MEMBER && cal->obj->m_is_name && cal->obj->mname
            && cal->obj->obj && cal->obj->obj->kind == EX_IDENT) {
            ty at2;
            if (scope_find(c, cal->obj->obj->text, &at2) && at2.k == T_ARENA
                && !strcmp(cal->obj->mname, "zeros")) {
                if (e->nelems != 1) { terr(c, "v1:zeros 实参"); return ty_unk(); }
                ty el = decl_ty_tc(c, cal->targs[0]);
                if (el.k == T_UNK) { terr(c, "v1:zeros 元素类型不支持"); return ty_unk(); }
                const char* wl = (el.k == T_STR) ? "str" : (el.k == T_FLT) ? "f64" : (el.k == T_BOOL) ? "b" : wlname(el);
                use_arr(c, wl);
                sb nb = {0};
                emit_expr(c, e->elems[0], &nb);
                int zn = c->tmpn++;
                sb_f(o, "({ ctron_arr_%s ctron_zr%d; ctron_zr%d.n = %s; ctron_zr%d.d = (%s*)calloc((size_t)(ctron_zr%d.n ? ctron_zr%d.n : 1), sizeof(%s)); ctron_zr%d; })",
                     wl, zn, zn, nb.d ? nb.d : "0", zn, ctype_of(el), zn, zn, ctype_of(el), zn);
                sb_free(&nb);
                return ty_arr(el);
            }
        }
        // C10-m:stdweb.dom 最小锚(§9.2;set_title/title,对齐 rt dom_title)
        if (cal && cal->kind == EX_MEMBER && cal->m_is_name && cal->mname
            && cal->obj && cal->obj->kind == EX_IDENT && !strcmp(cal->obj->text, "dom")) {
            if (!strcmp(cal->mname, "set_title") && e->nelems == 1) {
                sb a1 = {0};
                emit_expr(c, e->elems[0], &a1);
                use_sum(c, "static char* ctron_dom_title = NULL;\n"
                           "static void ctron_dom_set_title(const char* s) { ctron_dom_title = strdup(s ? s : \"\"); }\n"
                           "static const char* ctron_dom_title_fn(void) { return ctron_dom_title ? ctron_dom_title : \"\"; }\n");
                sb_f(o, "ctron_dom_set_title(%s)", a1.d ? a1.d : "0");
                sb_free(&a1);
                return ty_unk();
            }
            if (!strcmp(cal->mname, "title") && e->nelems == 0) {
                use_sum(c, "static char* ctron_dom_title = NULL;\n"
                           "static void ctron_dom_set_title(const char* s) { ctron_dom_title = strdup(s ? s : \"\"); }\n"
                           "static const char* ctron_dom_title_fn(void) { return ctron_dom_title ? ctron_dom_title : \"\"; }\n");
                sb_s(o, "ctron_dom_title_fn()");
                return ty_str();
            }
            terr(c, "v1:dom.%s 仅支持 set_title/title", cal->mname ? cal->mname : "?");
            return ty_unk();
        }
        // C10-k:Channel/Mutex/Global 类型构造(Channel 须经元组解构 let (tx, rx))
        if (cal && cal->kind == EX_TYPEARGS && cal->ntargs == 1
            && cal->obj && cal->obj->kind == EX_IDENT) {
            const char* cn0 = cal->obj->text;
            if (!strcmp(cn0, "Channel")) {
                terr(c, "v1:Channel 需元组解构 let (tx, rx) = Channel[T](cap)");
                return ty_unk();
            }
            if (!strcmp(cn0, "Mutex") || !strcmp(cn0, "Global") || !strcmp(cn0, "Atomic")) {
                ty inner = decl_ty_tc(c, cal->targs[0]);
                if (inner.k == T_UNK) { terr(c, "v1:%s 载荷类型不支持", cn0); return ty_unk(); }
                if (!strcmp(cn0, "Atomic") && e->nelems != 1) { terr(c, "v1:Atomic 实参"); return ty_unk(); }
                if (e->nelems < 1 || e->nelems > 2) { terr(c, "v1:%s 实参", cn0); return ty_unk(); }
                rt_mutex_type(c, inner);
                char mw[64];
                chan_mk(mw, sizeof mw, inner);
                if (e->nelems == 2) { // Global[T]("name", init):名称仅编译期(rt 语义)
                    sb d0 = {0};
                    emit_expr(c, e->elems[0], &d0);
                    sb_free(&d0);
                }
                sb init = {0};
                emit_expr(c, e->elems[e->nelems - 1], &init);
                sb_f(o, "ctron_mutex_%s_new(%s)", mw, init.d && *init.d ? init.d : "0");
                sb_free(&init);
                if (c->err) return ty_unk();
                return holder_ty(T_MUTEX, inner);
            }
        }
        // C10-q:Simd[E, N].splat(v) —— 定长向量构造
        if (cal && cal->kind == EX_MEMBER && cal->m_is_name && cal->mname
            && !strcmp(cal->mname, "splat") && cal->obj && cal->obj->kind == EX_TYPEARGS
            && cal->obj->obj && cal->obj->obj->kind == EX_IDENT
            && !strcmp(cal->obj->obj->text, "Simd")) {
            int n = 0;
            for (size_t ti = 0; ti < cal->obj->ntargs; ti++) {
                const cty* ta = cal->obj->targs[ti];
                if (ta->kind == TY_CVAL && ta->npath == 1) n = atoi(ta->path[0]);
            }
            if (n <= 0 || n > 64) { terr(c, "v1:Simd 宽度非法"); return ty_unk(); }
            if (e->nelems != 1) { terr(c, "v1:splat 实参"); return ty_unk(); }
            simd_typedef(c, n); // 注册向量 typedef(值形由助手内部构造)
            char hn[48];
            snprintf(hn, sizeof hn, "ctron_simd_splat_%d", n);
            use_helper(c, hn);
            sb v = {0};
            emit_expr(c, e->elems[0], &v);
            if (c->err) { sb_free(&v); return ty_unk(); }
            sb_f(o, "%s((double)(%s))", hn, v.d ? v.d : "0");
            sb_free(&v);
            return simd_ty(n);
        }
        // Option/Result 方法:or / expect(C10-d)
        if (cal && cal->kind == EX_MEMBER && cal->m_is_name && cal->mname) {
            sb rob2 = {0};
            ty rt4 = emit_expr(c, cal->obj, &rob2);
            if (c->err) { sb_free(&rob2); return ty_unk(); }
            if (rt4.k == T_MUTEX && cal->mname
                && (!strcmp(cal->mname, "load") || !strcmp(cal->mname, "store") || !strcmp(cal->mname, "fetch_add")))
                return emit_atomic_call(c, e, rt4, rob2.d ? rob2.d : "0", o);
            if (rt4.k == T_MUTEX && cal->mname
                && (!strcmp(cal->mname, "with") || !strcmp(cal->mname, "with_mut")))
                return emit_mutex_call(c, e, rt4, rob2.d ? rob2.d : "0", o);
            if (rt4.k == T_SUM && (!strcmp(cal->mname, "or") || !strcmp(cal->mname, "expect")
                                  || !strcmp(cal->mname, "context")
                                  || !strcmp(cal->mname, "map")
                                  || !strcmp(cal->mname, "is_some") || !strcmp(cal->mname, "is_ok"))) {
                int is_opt2 = !strncmp(rt4.tname, "ctron_opt_", 10);
                const char* good = is_opt2 ? "CTRON_OPT_SOME" : "CTRON_RES_OK";
                const char* mem = is_opt2 ? "some" : "ok";
                ty vt = ty_unk(); vt.k = rt4.ek; vt.bits = rt4.ebits; vt.us = rt4.eus;
                if (rt4.ek == T_FLT) vt = ty_flt();
                if (rt4.ek == T_BOOL) vt = ty_bool();
                if (rt4.ek == T_STR) vt = ty_str();
                if (!strcmp(cal->mname, "is_some") || !strcmp(cal->mname, "is_ok")) {
                    if (e->nelems != 0) { terr(c, "v1:%s 实参", cal->mname); sb_free(&rob2); return ty_unk(); }
                    sb_f(o, "((%s).tag == %s)", rob2.d ? rob2.d : "0", good);
                    sb_free(&rob2);
                    return ty_bool();
                }
                if (!strcmp(cal->mname, "context")) {
                    // Result 专用:Err 包成错误链;Ok 原样(rt option_builtin context 语义)
                    if (e->nelems != 1) { terr(c, "v1:context 实参"); sb_free(&rob2); return ty_unk(); }
                    sb a1 = {0};
                    emit_expr(c, e->elems[0], &a1);
                    if (c->err || is_opt2) {
                        if (!is_opt2) { sb_free(&a1); sb_free(&rob2); return ty_unk(); }
                        sb_s(o, rob2.d ? rob2.d : "0"); // Option:无效果(rt:原样)
                        sb_free(&a1); sb_free(&rob2);
                        return rt4;
                    }
                    if (c->err) { sb_free(&a1); sb_free(&rob2); return ty_unk(); }
                    // ok 载荷 mangle/ctype(与 sum_ty_of 同源)
                    ty okty = ty_unk(); okty.k = rt4.ek; okty.bits = rt4.ebits; okty.us = rt4.eus;
                    if (rt4.ek == T_FLT) okty = ty_flt();
                    else if (rt4.ek == T_BOOL) okty = ty_bool();
                    else if (rt4.ek == T_STR) okty = ty_str();
                    char* okm = ty_mangle(c, okty);
                    const char* okct = ctype_of(okty);
                    int src_chain = rt4.ek2 == T_ERR;
                    const char* srcm = src_chain ? "Err" : (rt4.tname2 ? rt4.tname2 : "?");
                    if (!strcmp(srcm, "?")) { terr(c, "v1:context 错误载荷类型未知"); sb_free(&a1); sb_free(&rob2); return ty_unk(); }
                    char outn[96];
                    snprintf(outn, sizeof outn, "ctron_res_%s_Err", okm ? okm : "?");
                    char def[600];
                    snprintf(def, sizeof def,
                        "typedef struct { int tag; union { %s ok; ctron_anyerr* err; } as; } %s;\n"
                        "#define CTRON_RES_OK 0\n#define CTRON_RES_ERR 1\n", okct, outn);
                    use_sum(c, def);
                    char hn[120];
                    snprintf(hn, sizeof hn, "ctron_ctxres_%s_%s", okm ? okm : "?", srcm);
                    char hbody[1024];
                    if (src_chain)
                        snprintf(hbody, sizeof hbody,
                            "static %s %s(%s v, const char* msg) {\n"
                            "    %s r;\n"
                            "    if (v.tag == CTRON_RES_OK) { r.tag = CTRON_RES_OK; r.as.ok = v.as.ok; return r; }\n"
                            "    ctron_anyerr* n = (ctron_anyerr*)calloc(1, sizeof(ctron_anyerr));\n"
                            "    n->message = strdup(msg ? msg : \"\");\n"
                            "    n->cause = v.as.err;\n"
                            "    n->trace = strdup(v.as.err && v.as.err->trace ? v.as.err->trace : \"main:1\");\n"
                            "    r.tag = CTRON_RES_ERR; r.as.err = n; return r;\n}\n",
                            outn, hn, ctype_of(rt4), outn);
                    else
                        snprintf(hbody, sizeof hbody,
                            "static %s %s(%s v, const char* msg) {\n"
                            "    %s r;\n"
                            "    if (v.tag == CTRON_RES_OK) { r.tag = CTRON_RES_OK; r.as.ok = v.as.ok; return r; }\n"
                            "    ctron_anyerr* leaf = (ctron_anyerr*)calloc(1, sizeof(ctron_anyerr));\n"
                            "    leaf->message = strdup(\"\");\n"
                            "    ctron_anyerr* n = (ctron_anyerr*)calloc(1, sizeof(ctron_anyerr));\n"
                            "    n->message = strdup(msg ? msg : \"\");\n"
                            "    n->cause = leaf;\n"
                            "    n->trace = strdup(\"main:1\");\n"
                            "    r.tag = CTRON_RES_ERR; r.as.err = n; return r;\n}\n",
                            outn, hn, ctype_of(rt4), outn);
                    use_sum(c, hbody);
                    sb_f(o, "%s(%s, %s)", hn, rob2.d ? rob2.d : "0", a1.d ? a1.d : "\"\"");
                    sb_free(&a1); sb_free(&rob2);
                    ty r = ty_unk(); r.k = T_SUM;
                    r.tname = ctron_arena_strndup(c->a, outn, strlen(outn));
                    r.ek = rt4.ek; r.ebits = rt4.ebits; r.eus = rt4.eus;
                    r.ek2 = T_ERR; r.ebits2 = 0; r.eus2 = 0; r.tname2 = NULL;
                    return r;
                }
                if (!strcmp(cal->mname, "map")) {
                    // Ok/Some → f(载荷) 重包;Err/None → 原样(对齐 rt/解释器语义)
                    if (e->nelems != 1) { terr(c, "v1:map 实参"); sb_free(&rob2); return ty_unk(); }
                    sb a1 = {0};
                    ty ft = emit_expr(c, e->elems[0], &a1);
                    if (c->err || ft.k != T_FNPTR) {
                        if (!c->err) terr(c, "v1:map 需函数值");
                        sb_free(&a1); sb_free(&rob2);
                        return ty_unk();
                    }
                    char hn[128];
                    snprintf(hn, sizeof hn, "ctron_map_%.100s", rt4.tname ? rt4.tname + strlen("ctron_") : "X");
                    char ht[512];
                    snprintf(ht, sizeof ht,
                        "static %s %s(%s v, ctron_fnptr f) { if (v.tag == %s) v.as.%s = (%s)f((int64_t)v.as.%s); return v; }\n",
                        ctype_of(rt4), hn, ctype_of(rt4), good, mem, ctype_of(vt), mem);
                    use_sum(c, ht);
                    sb_f(o, "%s(%s, %s)", hn, rob2.d ? rob2.d : "0", a1.d ? a1.d : "0");
                    sb_free(&a1); sb_free(&rob2);
                    return rt4;
                }
                if (!strcmp(cal->mname, "or")) {
                    if (e->nelems != 1) { terr(c, "v1:or 实参"); sb_free(&rob2); return ty_unk(); }
                    sb a1 = {0};
                    emit_expr(c, e->elems[0], &a1);
                    sb_f(o, "(%s.tag == %s ? (%s.as.%s) : (%s))", rob2.d ? rob2.d : "0", good,
                         rob2.d ? rob2.d : "0", mem, a1.d ? a1.d : "0");
                    sb_free(&a1); sb_free(&rob2);
                    return vt;
                }
                if (e->nelems != 1) { terr(c, "v1:expect 实参"); sb_free(&rob2); return ty_unk(); }
                sb a1 = {0}, a2 = {0};
                emit_expr(c, e->elems[0], &a1);
                emit_expr(c, e->elems[0], &a2);
                char hname[96];
                snprintf(hname, sizeof hname, "ctron_expect_%.48s", rt4.tname ? rt4.tname + strlen("ctron_") : "X");
                use_helper(c, hname);
                char htext[512];
                snprintf(htext, sizeof htext,
                    "static %s %s(%s v, const char* msg) { if (v.tag != %s) ctron_panic(msg); return v.as.%s; }\n",
                    ctype_of(vt), hname, ctype_of(rt4), good, mem);
                use_sum(c, htext);
                sb_f(o, "%s(%s, %s)", hname, rob2.d ? rob2.d : "0", a1.d ? a1.d : "0");
                sb_free(&a1); sb_free(&a2); sb_free(&rob2);
                return vt;
            }
            sb_free(&rob2);
        }
        // Str 成员方法:contains/slice/to_string(rt 内建;必须在具名函数门之前)
        if (cal && cal->kind == EX_MEMBER && cal->m_is_name && cal->mname) {
            sb rob = {0};
            ty rt3 = emit_expr(c, cal->obj, &rob);
            if (c->err) { sb_free(&rob); return ty_unk(); }
            if (rt3.k == T_STR && !strcmp(cal->mname, "contains")) {
                if (e->nelems != 1) { terr(c, "v1:contains 实参"); sb_free(&rob); return ty_unk(); }
                sb a1 = {0};
                emit_expr(c, e->elems[0], &a1);
                use_helper(c, "ctron_str_contains");
                sb_f(o, "ctron_str_contains(%s, %s)", rob.d ? rob.d : "\"\"", a1.d ? a1.d : "\"\"");
                sb_free(&a1); sb_free(&rob);
                return ty_bool();
            }
            if (rt3.k == T_STR && !strcmp(cal->mname, "slice")) {
                if (e->nelems != 1 || e->elems[0]->kind != EX_RANGE) { terr(c, "v1:slice 需 range"); sb_free(&rob); return ty_unk(); }
                sb a1 = {0}, a2 = {0};
                emit_expr(c, e->elems[0]->from, &a1);
                emit_expr(c, e->elems[0]->to, &a2);
                use_helper(c, "ctron_str_slice");
                sb_f(o, "ctron_str_slice(%s, %s, %s, %d)", rob.d ? rob.d : "\"\"",
                     a1.d ? a1.d : "0", a2.d ? a2.d : "0", e->elems[0]->inclusive ? 1 : 0);
                sb_free(&a1); sb_free(&a2); sb_free(&rob);
                return ty_str();
            }
            if (rt3.k == T_STR && !strcmp(cal->mname, "to_string")) {
                sb_s(o, rob.d ? rob.d : "\"\"");
                sb_free(&rob);
                return ty_str();
            }
            sb_free(&rob);
        }
        // Simd 成员方法:lane(i)/to_array()(C10-q;对齐 rt recv V_SIMD 语义)
        if (cal && cal->kind == EX_MEMBER && cal->m_is_name && cal->mname) {
            sb rob5 = {0};
            ty rt8 = emit_expr(c, cal->obj, &rob5);
            if (c->err) { sb_free(&rob5); return ty_unk(); }
            if (rt8.k == T_SIMD && (!strcmp(cal->mname, "lane") || !strcmp(cal->mname, "to_array"))) {
                int n = (int)rt8.bits;
                const char* tdn = simd_typedef(c, n);
                if (!strcmp(cal->mname, "lane")) {
                    if (e->nelems != 1) { terr(c, "v1:lane 实参"); sb_free(&rob5); return ty_unk(); }
                    sb ix = {0};
                    emit_expr(c, e->elems[0], &ix);
                    if (c->err) { sb_free(&ix); sb_free(&rob5); return ty_unk(); }
                    int u = c->tmpn++;
                    sb_f(o, "({ %s ctron_sv%d = %s; int64_t ctron_si%d = (int64_t)(%s); "
                            "if (ctron_si%d < 0 || ctron_si%d >= %d) ctron_panic(\"index out of bounds\"); "
                            "ctron_sv%d.d[ctron_si%d]; })",
                         tdn, u, rob5.d ? rob5.d : "0", u, ix.d ? ix.d : "0", u, u, n, u, u);
                    sb_free(&ix);
                    sb_free(&rob5);
                    return ty_flt();
                }
                if (e->nelems != 0) { terr(c, "v1:to_array 实参"); sb_free(&rob5); return ty_unk(); }
                char hn[48];
                snprintf(hn, sizeof hn, "ctron_simd_toarr_%d", n);
                use_helper(c, hn);
                sb_f(o, "%s(%s)", hn, rob5.d ? rob5.d : "0");
                sb_free(&rob5);
                use_arr(c, "f64");
                return ty_arr(ty_flt());
            }
            sb_free(&rob5);
        }
        // .as[T]() 显式转换(rt as_conv:int 回绕截断/浮点)
        if (cal && cal->kind == EX_TYPEARGS && cal->obj && cal->obj->kind == EX_MEMBER
            && cal->obj->m_is_name && cal->obj->mname && strcmp(cal->obj->mname, "as") == 0
            && cal->ntargs == 1 && e->nelems == 0) {
            ty tt = decl_ty_tc(c, cal->targs[0]);
            sb v = {0};
            emit_expr(c, cal->obj->obj, &v);
            if (tt.k == T_FLT) sb_f(o, "((double)(%s))", v.d ? v.d : "0");
            else if (tt.k == T_INT) {
                char hn[64];
                snprintf(hn, sizeof hn, "ctron_as_%s", wlname(tt));
                use_helper(c, hn);
                sb_f(o, "%s(%s)", hn, v.d ? v.d : "0");
            } else { terr(c, "v1:as 目标类型不支持"); }
            sb_free(&v);
            return tt;
        }
        // Box[T](v):显式堆分配,自动解引用访问
        if (cal && cal->kind == EX_TYPEARGS && cal->obj && cal->obj->kind == EX_IDENT
            && strcmp(cal->obj->text, "Box") == 0 && cal->ntargs == 1 && e->nelems == 1) {
            ty et = decl_ty_tc(c, cal->targs[0]);
            if (et.k == T_UNK) { terr(c, "v1:Box 元素类型不支持"); return ty_unk(); }
            char* k = ty_mangle(c, et);
            
            { char hn[96]; snprintf(hn, sizeof hn, "ctron_box_%s", k); use_helper(c, hn); }
            sb a1 = {0};
            emit_expr(c, e->elems[0], &a1);
            char hn2[96];
            snprintf(hn2, sizeof hn2, "ctron_box_%s", k);
            sb_f(o, "%s(%s)", hn2, a1.d ? a1.d : "0");
            sb_free(&a1);
            ty r = ty_unk(); r.k = T_BOX; r.ek = et.k; r.ebits = et.bits; r.eus = et.us; r.tname = et.tname;
            return r;
        }
        // List 方法:push / into_gc(C10-g)
        if (cal && cal->kind == EX_MEMBER && cal->m_is_name && cal->mname) {
            sb rob4 = {0};
            ty rt6 = emit_expr(c, cal->obj, &rob4);
            if (c->err) { sb_free(&rob4); return ty_unk(); }
            if (rt6.k == T_LIST && !strcmp(cal->mname, "push")) {
                if (!cal->obj || cal->obj->kind != EX_IDENT) { terr(c, "v1:push 接收者需为局部列表"); sb_free(&rob4); return ty_unk(); }
                if (e->nelems != 1) { terr(c, "v1:push 实参"); sb_free(&rob4); return ty_unk(); }
                char wl[16];
                snprintf(wl, sizeof wl, "%s", ewlname(rt6));
                use_arr(c, wl);
                sb a1 = {0};
                const ty* sw2 = c->want;
                c->want = NULL;
                emit_expr(c, e->elems[0], &a1);
                c->want = sw2;
                char hn[96];
                snprintf(hn, sizeof hn, "ctron_list_%s_push", wl);
                use_helper(c, hn);
                sb_f(o, "ctron_list_%s_push(&%s, %s)", wl, cal->obj->text, a1.d ? a1.d : "0");
                sb_free(&a1); sb_free(&rob4);
                return ty_unk();
            }
            if (rt6.k == T_LIST && !strcmp(cal->mname, "into_gc")) {
                if (e->nelems != 0) { terr(c, "v1:into_gc 实参"); sb_free(&rob4); return ty_unk(); }
                char wl[16];
                snprintf(wl, sizeof wl, "%s", ewlname(rt6));
                use_arr(c, wl);
                char hn[96];
                snprintf(hn, sizeof hn, "ctron_list_%s_clone", wl);
                use_helper(c, hn);
                sb_f(o, "ctron_list_%s_clone(%s)", wl, rob4.d ? rob4.d : "0");
                sb_free(&rob4);
                ty r = ty_unk(); r.k = T_LIST; r.ek = rt6.ek; r.ebits = rt6.ebits; r.eus = rt6.eus; r.tname = rt6.tname;
                return r;
            }
            sb_free(&rob4);
        }
        // impl/trait 方法分发(C10-g②):recv 为 struct/class/enum 用户类型
        // v1:接收者仅支持标识符(表达式安全,单次求值)
        if (cal && cal->kind == EX_MEMBER && cal->m_is_name && cal->mname
            && cal->obj && cal->obj->kind == EX_IDENT) {
            ty rt7;
            if (!scope_find(c, cal->obj->text, &rt7)) { /* 落到 UFCS/错误 */ }
            else if (rt7.k == T_STRUCT || rt7.k == T_CLASS || (rt7.k == T_ENUM && rt7.tname)) {
                char cnx[192];
                ty mret;
                if (ensure_method_fn(c, rt7.tname, cal->mname, cnx, &mret)) {
                    sb_f(o, "%s(%s", cnx, cal->obj->text);
                    for (size_t i = 0; i < e->nelems; i++) {
                        sb a1 = {0};
                        emit_expr(c, e->elems[i], &a1);
                        sb_f(o, ", %s", a1.d ? a1.d : "0");
                        sb_free(&a1);
                    }
                    sb_s(o, ")");
                    return mret.k == T_UNK ? ty_unk() : mret;
                }
            }
        }
        // UFCS:自由函数作方法(obj.method(args) → method(obj, args))(rt 同语义)
        if (cal && cal->kind == EX_MEMBER && cal->m_is_name && cal->mname) {
            int un, iv;
            ty uret, uargtys[8];
            if (fn_lookup(c, cal->mname, &un, &iv, &uret, uargtys) && un == (int)e->nelems + 1) {
                sb rob3 = {0};
                ty rt5 = emit_expr(c, cal->obj, &rob3);
                (void)rt5;
                sb uargs = {0};
                sb_s(&uargs, rob3.d ? rob3.d : "0");
                for (size_t i = 0; i < e->nelems; i++) {
                    sb a1 = {0};
                    const ty* sw = c->want;
                    if (i < 8) c->want = &uargtys[i + 1];
                    emit_expr(c, e->elems[i], &a1);
                    c->want = sw;
                    sb_f(&uargs, ", %s", a1.d ? a1.d : "0");
                    sb_free(&a1);
                }
                char cn2[256];
                snprintf(cn2, sizeof cn2, "ctron_user_%s", cal->mname);
                sb_f(o, "%s(%s)", cn2, uargs.d ? uargs.d : "");
                sb_free(&uargs); sb_free(&rob3);
                return is_void_helper(iv) ? ty_unk() : uret;
            }
        }
        // List 容器:List[T]() / arena.list[T]()
        if (cal && cal->kind == EX_TYPEARGS && cal->obj && cal->ntargs == 1) {
            int is_list_ctor = 0;
            if (cal->obj->kind == EX_IDENT && !strcmp(cal->obj->text, "List")) is_list_ctor = 1;
            if (cal->obj->kind == EX_MEMBER && cal->obj->m_is_name
                && !strcmp(cal->obj->mname, "list") && cal->obj->obj
                && cal->obj->obj->kind == EX_IDENT && !strcmp(cal->obj->obj->text, "arena")) is_list_ctor = 1;
            if (is_list_ctor && e->nelems == 0) {
                ty et = decl_ty_tc(c, cal->targs[0]);
                if (et.k == T_UNK) { terr(c, "v1:List 元素类型不支持"); return ty_unk(); }
                char wl[16];
                if (et.k == T_STR) snprintf(wl, sizeof wl, "str");
                else snprintf(wl, sizeof wl, "%s", wlname(et));
                use_arr(c, wl);
                sb_f(o, "(ctron_list_%s){0}", wl);
                ty r = ty_unk(); r.k = T_LIST; r.ek = et.k; r.ebits = et.bits; r.eus = et.us; r.tname = et.tname;
                return r;
            }
        }
        const char* nm = (cal && cal->kind == EX_IDENT) ? cal->text : NULL;
        if (!nm) {
            if (cal && cal->kind == EX_MEMBER && cal->m_is_name)
                terr(c, "v1 仅支持具名函数调用(成员方法:%s)", cal->mname);
            else
                terr(c, "v1 仅支持具名函数调用");
            return ty_unk();
        }
        // 用户函数参数类型提示(提前查表)
        int nparams = 0, is_void = 0;
        ty fret, argtys[8];
        int is_user_fn = fn_lookup(c, nm, &nparams, &is_void, &fret, argtys);
        // C10-p:泛型 fn 单态化 —— 带类型形参([T])的 fn 由调用点按实参例化
        const cfn* GF = fn_named(c->srcf, nm);
        if (GF && GF->ntype_params > 0) {
            if ((int)e->nelems != (int)GF->nparams) { terr(c, "v1:泛型参数个数 %s", nm); return ty_unk(); }
            size_t np = GF->ntype_params > 4 ? 4 : GF->ntype_params;
            char* pnames[4] = {0};
            ty pvals[4];
            memset(pvals, 0, sizeof pvals);
            for (size_t j = 0; j < np; j++) pnames[j] = GF->type_params[j].name;
            sb abuf[8];
            memset(abuf, 0, sizeof abuf);
            ty atys[8];
            memset(atys, 0, sizeof atys);
            for (size_t i = 0; i < e->nelems && i < 8 && !c->err; i++) {
                const ty* sw5 = c->want;
                c->want = NULL;
                atys[i] = emit_expr(c, e->elems[i], &abuf[i]);
                c->want = sw5;
            }
            if (!c->err) {
                for (size_t k = 0; k < GF->nparams && k < 8; k++)
                    unify_gen(c, GF->params[k].ty, atys[k], pnames, pvals, np);
                sb mn = {0};
                sb_f(&mn, "ctron_user_%s", nm);
                int unbound = 0;
                for (size_t j = 0; j < np; j++) {
                    if (pvals[j].k == T_UNK) { terr(c, "v1:泛型实参 %s 无法推导:%s", pnames[j] ? pnames[j] : "?", nm); unbound = 1; break; }
                    char* mk = ty_mangle(c, pvals[j]);
                    sb_f(&mn, "__%.40s", mk ? mk : "?");
                }
                if (!unbound) {
                    // 返回类型在 subs 活跃下解析((T,T) → 具体元组)
                    size_t saved = c->n_subs;
                    for (size_t j = 0; j < np && c->n_subs < 8; j++) {
                        c->subs[c->n_subs].from = pnames[j];
                        c->subs[c->n_subs].to = pvals[j];
                        c->n_subs++;
                    }
                    ty gret = decl_ty_tc(c, GF->ret);
                    ensure_mono_generic(c, GF, mn.d ? mn.d : "", pnames, pvals, np);
                    if (!c->err) {
                        sb_f(o, "%s(", mn.d ? mn.d : "");
                        for (size_t i = 0; i < e->nelems && i < 8; i++) {
                            if (i) sb_s(o, ", ");
                            sb_s(o, abuf[i].d ? abuf[i].d : "0");
                        }
                        sb_s(o, ")");
                    }
                    if (!c->err && gret.k != T_UNK) fret = gret; // 单态化返回类型
                    is_void = gret.k == T_UNK;
                    c->n_subs = saved;
                }
                sb_free(&mn);
            }
            for (size_t i = 0; i < e->nelems && i < 8; i++) sb_free(&abuf[i]);
            return c->err ? ty_unk() : (is_void ? ty_unk() : fret);
        }
        // extern "c" FFI:ABI 宽度 cast + 裸 C 符号调用(P1-E⑰ 同构)
        int is_ext = 0;
        for (size_t xi = 0; xi < c->n_externs; xi++)
            if (!strcmp(c->externs[xi], nm)) { is_ext = 1; break; }
        if (is_ext) {
            if ((int)e->nelems != nparams) { terr(c, "v1:参数个数 %s", nm); return ty_unk(); }
            sb ea = {0};
            for (size_t i = 0; i < e->nelems && i < 8; i++) {
                if (i) sb_s(&ea, ", ");
                sb a1 = {0};
                const ty* sw7 = c->want;
                if (i < 8) c->want = &argtys[i];
                emit_expr(c, e->elems[i], &a1);
                c->want = sw7;
                if (c->err) { sb_free(&a1); sb_free(&ea); return ty_unk(); }
                sb_f(&ea, "(%s)(%s)", abi_ty(argtys[i]), a1.d ? a1.d : "0");
                sb_free(&a1);
            }
            if (is_void_helper(is_void)) {
                sb_f(o, "%s(%s)", nm, ea.d ? ea.d : "");
                sb_free(&ea);
                return ty_unk();
            }
            sb_f(o, "(int64_t)(%s(%s))", nm, ea.d ? ea.d : "");
            sb_free(&ea);
            return ty_int(64, 0);
        }
        // C10-h:trait 参数单态化 —— fn 带 &Trait 形参 → 按实参具体类型例化后调用
        if (is_user_fn && (int)e->nelems == nparams) {
            int ntrait = 0;
            for (int i = 0; i < nparams && i < 8; i++)
                if (argtys[i].k == T_TRAIT) { ntrait = 1; break; }
            if (ntrait) {
                sb abuf[8];
                memset(abuf, 0, sizeof abuf);
                ty atys[8];
                memset(atys, 0, sizeof atys);
                for (size_t i = 0; i < e->nelems && i < 8 && !c->err; i++) {
                    const ty* saved_w = c->want;
                    if (argtys[i].k != T_TRAIT) c->want = &argtys[i];
                    atys[i] = emit_expr(c, e->elems[i], &abuf[i]);
                    c->want = saved_w;
                }
                if (!c->err) {
                    sb mname = {0};
                    sb_f(&mname, "ctron_user_%s", nm);
                    int bad = 0;
                    for (int i = 0; i < nparams && i < 8; i++)
                        if (argtys[i].k == T_TRAIT) {
                            if (atys[i].k == T_TRAIT || atys[i].k == T_UNK) {
                                terr(c, "v1:trait 形参 %s 实参类型无法单态化", nm);
                                bad = 1; break;
                            }
                            char* mk = ty_mangle(c, atys[i]);
                            sb_f(&mname, "__%s", mk ? mk : "?");
                        }
                    if (!bad) {
                        const cfn* mf = fn_named(c->srcf, nm);
                        if (!mf) terr(c, "v1 单态化目标缺失:%s", nm);
                        else ensure_mono_fn(c, mf, mname.d ? mname.d : "", argtys, atys, nparams);
                        if (!c->err) {
                            sb_f(o, "%s(", mname.d ? mname.d : "");
                            for (size_t i = 0; i < e->nelems && i < 8; i++) {
                                if (i) sb_s(o, ", ");
                                sb_s(o, abuf[i].d ? abuf[i].d : "0");
                            }
                            sb_s(o, ")");
                        }
                    }
                    sb_free(&mname);
                }
                for (size_t i = 0; i < e->nelems && i < 8; i++) sb_free(&abuf[i]);
                return c->err ? ty_unk() : (is_void ? ty_unk() : fret);
            }
        }
        sb args = {0};
        for (size_t i = 0; i < e->nelems; i++) {
            if (i) sb_s(&args, ", ");
            const ty* saved_w = c->want;
            if (is_user_fn && i < 8) c->want = &argtys[i];
            sb a1 = {0};
            emit_expr(c, e->elems[i], &a1);
            c->want = saved_w;
            sb_s(&args, a1.d ? a1.d : "0");
            sb_free(&a1);
        }
        if (c->err) { sb_free(&args); return ty_unk(); }
        if (!strcmp(nm, "byte_at")) {
            if (e->nelems != 2) { terr(c, "v1:byte_at 实参"); sb_free(&args); return ty_unk(); }
            sb a1 = {0}, a2 = {0};
            emit_expr(c, e->elems[0], &a1);
            emit_expr(c, e->elems[1], &a2);
            use_helper(c, "ctron_byte_at");
            sb_f(o, "ctron_byte_at(%s, %s)", a1.d ? a1.d : "0", a2.d ? a2.d : "0");
            sb_free(&a1); sb_free(&a2); sb_free(&args);
            return ty_int(32, 0);
        }
        if (!strcmp(nm, "byte_slice")) {
            if (e->nelems != 3) { terr(c, "v1:byte_slice 实参"); sb_free(&args); return ty_unk(); }
            sb a1 = {0}, a2 = {0}, a3 = {0};
            emit_expr(c, e->elems[0], &a1);
            emit_expr(c, e->elems[1], &a2);
            emit_expr(c, e->elems[2], &a3);
            use_helper(c, "ctron_byte_slice");
            sb_f(o, "ctron_byte_slice(%s, %s, %s)", a1.d ? a1.d : "0", a2.d ? a2.d : "0", a3.d ? a3.d : "0");
            sb_free(&a1); sb_free(&a2); sb_free(&a3); sb_free(&args);
            return ty_str();
        }
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
        // 用户枚举载荷变体构造:Advance(x)
        for (size_t ei = 0; ei < c->nenums; ei++) {
            edef* ed = &c->enums[ei];
            for (size_t j = 0; j < ed->n; j++) {
                if (!strcmp(ed->variants[j].name, nm) && ed->variants[j].has_p) {
                    const ty* saved_w = c->want;
                    c->want = &ed->variants[j].pty;
                    sb a1 = {0};
                    emit_expr(c, e->nelems == 1 ? e->elems[0] : NULL, &a1);
                    c->want = saved_w;
                    sb_f(o, "(ctron_e_%s){ .tag = CTRON_%s_%s, .as.u_%s = %s }", ed->name, ed->name, nm, nm,
                         a1.d ? a1.d : "0");
                    sb_free(&a1);
                    ty r = ty_unk(); r.k = T_ENUM; r.tname = ed->name;
                    return r;
                }
            }
        }
        // 和类型构造:Some/Ok/Err(期望类型推导;Some 缺失时从载荷推导)
        if (!strcmp(nm, "Some") || !strcmp(nm, "Ok") || !strcmp(nm, "Err")) {
            const ty* w = c->want;
            char wbuf[96];
            if ((!w || w->k != T_SUM) && !strcmp(nm, "Some") && e->nelems == 1) {
                // 从载荷推导 opt<T>
                const ty* sw = c->want;
                c->want = NULL;
                sb p1 = {0};
                ty pt = emit_expr(c, e->elems[0], &p1);
                c->want = sw;
                if (pt.k == T_UNK) { terr(c, "v1:Some 载荷类型未知"); sb_free(&p1); sb_free(&args); return ty_unk(); }
                char* k = ty_mangle(c, pt);
                snprintf(wbuf, sizeof wbuf, "ctron_opt_%s", k);
                char def[512];
                snprintf(def, sizeof def,
                    "typedef struct { int tag; union { %s some; } as; } %s;\n"
                    "#define CTRON_OPT_NONE 0\n#define CTRON_OPT_SOME 1\n", ctype_of(pt), wbuf);
                use_sum(c, def);
                static ty derived; // 单线程发射器
                derived = ty_unk(); derived.k = T_SUM;
                derived.ek = pt.k; derived.ebits = pt.bits; derived.eus = pt.us;
                derived.tname = ctron_arena_strndup(c->a, wbuf, strlen(wbuf));
                sb_free(&p1);
                sb_f(o, "(%s){ .tag = CTRON_OPT_SOME, .as.some = %s }", wbuf, args.d ? args.d : "0");
                sb_free(&args);
                return derived;
            }
            if (!w || w->k != T_SUM) { terr(c, "v1:%s 需期望类型(注解/返回类型)", nm); sb_free(&args); return ty_unk(); }
            int is_opt = !strncmp(w->tname, "ctron_opt_", 10);
            if (!strcmp(nm, "Some") && is_opt)
                sb_f(o, "(%s){ .tag = CTRON_OPT_SOME, .as.some = %s }", w->tname, args.d ? args.d : "0");
            else if (!strcmp(nm, "Ok") && !is_opt)
                sb_f(o, "(%s){ .tag = CTRON_RES_OK, .as.ok = %s }", w->tname, args.d ? args.d : "0");
            else if (!strcmp(nm, "Err") && !is_opt)
                sb_f(o, "(%s){ .tag = CTRON_RES_ERR, .as.err = %s }", w->tname, args.d ? args.d : "0");
            else { terr(c, "v1:%s 与期望和类型不符", nm); sb_free(&args); return ty_unk(); }
            sb_free(&args);
            return *w;
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
        if (!is_user_fn) {
            terr(c, "v1 未解析函数:%s", nm);
            sb_free(&args);
            return ty_unk();
        }
        if ((int)e->nelems != nparams) terr(c, "v1:参数个数 %s", nm);
        char cn[256];
        snprintf(cn, sizeof cn, "ctron_user_%s", nm);
        sb_f(o, "%s(%s)", cn, args.d ? args.d : "");
        sb_free(&args);
        return is_void ? ty_unk() : fret;
    }
    case EX_MATCH:
        terr(c, "v1:match 仅支持 return/let/语句位置");
        return ty_unk();
    case EX_SCOPE:
        return emit_scope_expr(c, e, o);
    case EX_CLOSURE: {
        // 值位置闭包(非捕获)→ 顶层 static 函数 + 函数指针(int64 统一 ABI)
        if (e->ncparams > 8) { terr(c, "v1:闭包参数过多"); return ty_unk(); }
        if (!e->cbody) { terr(c, "v1:闭包缺体"); return ty_unk(); }
        char fn[64];
        snprintf(fn, sizeof fn, "ctron_clo%d", c->tmpn++);
        sb fb = {0};
        scope_push(c);
        sb_f(&fb, "static int64_t %s(", fn);
        for (size_t i = 0; i < e->ncparams; i++) {
            if (i) sb_s(&fb, ", ");
            sb_f(&fb, "int64_t %s", e->cparams[i].name ? e->cparams[i].name : "_"); // 独立函数作用域:形参即源名(EX_IDENT 直发文本)
            scope_def(c, e->cparams[i].name, ty_int(64, 0));
        }
        sb_s(&fb, ") {\n    int64_t ctron_rv = 0;\n");
        const cexpr* b = e->cbody;
        if (b->kind == EX_BLOCK && b->block) {
            for (size_t i = 0; i < b->block->nstmts && !c->err; i++) emit_stmt(c, b->block->stmts[i], &fb);
            if (b->block->tail && !c->err) emit_tail_to(c, b->block->tail, "ctron_rv", &fb);
        } else if (!c->err) {
            emit_tail_to(c, (cexpr*)b, "ctron_rv", &fb);
        }
        scope_pop(c);
        if (!c->err) sb_f(&fb, "    return ctron_rv;\n}\n");
        if (c->err) { sb_free(&fb); return ty_unk(); }
        sb_s(&c->clo_sb, fb.d ? fb.d : "");
        sb_free(&fb);
        sb_f(o, "(ctron_fnptr)%s", fn);
        return ty_fnptr();
    }
    case EX_OWN:
        terr(c, "v1:own 块需语句位置");
        return ty_unk();
    default:
        terr(c, "v1 不支持该表达式构造(kind %d|%s)", (int)e->kind,
             e->kind == EX_VOID ? "void字面量" : e->kind == EX_TUPLE ? "元组" : e->kind == EX_TYPEARGS ? "类型实参" : "其他");
        return ty_unk();
    }
}

// ================= 方法分发(C10-g②)=================
const cfn* find_impl_method(tc* c, const char* type, const char* name) {
    for (size_t i = 0; i < c->n_methods; i++)
        if (!strcmp(c->methods[i].type, type) && !strcmp(c->methods[i].name, name) && c->methods[i].f)
            return c->methods[i].f;
    return NULL;
}
const cprop* find_impl_prop(tc* c, const char* type, const char* name) {
    for (size_t i = 0; i < c->n_methods; i++)
        if (!strcmp(c->methods[i].type, type) && !strcmp(c->methods[i].name, name) && c->methods[i].p)
            return c->methods[i].p;
    return NULL;
}
int type_impls_trait(tc* c, const char* type, const char* trait) {
    for (size_t i = 0; i < c->n_impls; i++)
        if (!strcmp(c->impls[i].type, type) && !strcmp(c->impls[i].trait, trait)) return 1;
    return 0;
}
// trait 默认方法:返回 (trait, 方法);类型实现了该 trait 即可用
const cfn* find_default_fn_for(tc* c, const char* type, const char* name, char** out_trait) {
    for (size_t i = 0; i < c->n_defaults; i++)
        if (!strcmp(c->defaults[i].name, name) && type_impls_trait(c, type, c->defaults[i].trait)) {
            if (out_trait) *out_trait = c->defaults[i].trait;
            return c->defaults[i].f;
        }
    return NULL;
}
const cprop* find_default_prop_for(tc* c, const char* type, const char* name, char** out_trait) {
    for (size_t i = 0; i < c->n_defaults; i++)
        if (!strcmp(c->defaults[i].name, name) && c->defaults[i].p && type_impls_trait(c, type, c->defaults[i].trait)) {
            if (out_trait) *out_trait = c->defaults[i].trait;
            return c->defaults[i].p;
        }
    return NULL;
}
int find_class(tc* c, const char* n) {
    for (size_t i = 0; i < c->nclasses; i++)
        if (!strcmp(c->classes[i].name, n)) return 1;
    return 0;
}
int type_has_drop_m(tc* c, const char* type) {
    for (size_t i = 0; i < c->n_methods; i++)
        if (!strcmp(c->methods[i].type, type) && !strcmp(c->methods[i].name, "drop")) return 1;
    return 0;
}

// 方法体发射:receiver self + 参数(self 为 class→指针;struct→值)
// 方法体发射:receiver self(class→指针;struct→值副本,对齐 rt call_method_body)
void emit_method_fn(tc* c, const char* type, const cfn* F, const char* cname) {
    int is_class = find_class(c, type);
    ty ret = decl_ty_tc(c, F->ret);
    const ty* saved_fr = c->fn_ret;
    c->fn_ret = &ret;
    const char* rct = ctype_of(ret);
    if (ret.k == T_UNK) rct = "void";
    char sct[128];
    if (is_class) snprintf(sct, sizeof sct, "ctron_c_%s*", type);
    else snprintf(sct, sizeof sct, "ctron_t_%s", type);
    sb_f(&c->m_sb, "static %s %s(%s self", rct, cname, sct);
    for (size_t i = 0; i < F->nparams; i++) {
        const cparam* p = &F->params[i];
        if (p->is_receiver || !p->name) continue;
        ty pt = decl_ty_tc(c, p->ty);
        if (pt.k == T_UNK) { terr(c, "v1:方法参数 %s 需类型注解", p->name); return; }
        sb_f(&c->m_sb, ", %s ctron_p_%s", ctype_of(pt), p->name);
    }
    sb_f(&c->m_sb, ") {");
    scope_push(c);
    int saved_it = c->in_test;
    c->in_test = 0;
    ty selft = ty_unk();
    selft.k = is_class ? T_CLASS : T_STRUCT;
    selft.tname = type;
    scope_def(c, "self", selft);
    for (size_t i = 0; i < F->nparams; i++) {
        const cparam* p = &F->params[i];
        if (p->is_receiver || !p->name) continue;
        ty pt = decl_ty_tc(c, p->ty);
        if (pt.k == T_INT) {
            char h[64];
            snprintf(h, sizeof h, "ctron_decl_%s", wlname(pt));
            use_helper(c, h);
            sb_f(&c->m_sb, "    %s %s = %s(ctron_p_%s);", ctype_of(pt), p->name, h, p->name);
        } else {
            sb_f(&c->m_sb, "    %s %s = ctron_p_%s;", ctype_of(pt), p->name, p->name);
        }
        scope_def(c, p->name, pt);
    }
    if (F->body) emit_block(c, F->body, &c->m_sb);
    scope_pop(c);
    c->fn_ret = saved_fr;
    c->in_test = saved_it;
    sb_f(&c->m_sb, "}");
}

// prop 访问器发射
void emit_prop_accessor(tc* c, const char* type, const cprop* P, const char* cname) {
    int is_class = find_class(c, type);
    ty ret = decl_ty_tc(c, P->ty);
    const ty* saved_fr2 = c->fn_ret;
    c->fn_ret = &ret;
    const char* rct = ctype_of(ret);
    if (ret.k == T_UNK) rct = "void";
    char sct[128];
    if (is_class) snprintf(sct, sizeof sct, "ctron_c_%s*", type);
    else snprintf(sct, sizeof sct, "ctron_t_%s", type);
    sb_f(&c->m_sb, "static %s %s(%s self) {", rct, cname, sct);
    scope_push(c);
    int saved_it2 = c->in_test;
    c->in_test = 0;
    ty selft = ty_unk();
    selft.k = is_class ? T_CLASS : T_STRUCT;
    selft.tname = type;
    scope_def(c, "self", selft);
    if (P->body) emit_block(c, P->body, &c->m_sb);
    scope_pop(c);
    c->fn_ret = saved_fr2;
    c->in_test = saved_it2;
    sb_f(&c->m_sb, "}");
}

// 惰性确保方法/prop 已发射;返回发射名
int ensure_method_fn(tc* c, const char* type, const char* name, char* out_cname, ty* out_ret) {
    char cn[192];
    snprintf(cn, sizeof cn, "ctron_m_%s_%s", type, name);
    const cfn* mf = find_impl_method(c, type, name);
    const cfn* df = mf ? NULL : find_default_fn_for(c, type, name, NULL);
    const cfn* use = mf ? mf : df;
    if (!use) {
        // derive(Show)/结构化合成(C10-p):无 impl/默认时的 show 落点
        if (!strcmp(name, "show") && ensure_derived_show(c, type, out_cname, out_ret, 0)) return 1;
        return 0;
    }
    for (size_t i = 0; i < c->n_protos; i++)
        if (!strcmp(c->protos[i], cn)) {
            snprintf(out_cname, 192, "%s", cn);
            *out_ret = decl_ty_tc(c, use->ret);
            return 1;
        }
    use_proto(c, cn);
    snprintf(out_cname, 192, "%s", cn);
    *out_ret = decl_ty_tc(c, use->ret);
    emit_method_fn(c, type, use, cn);
    return 1;
}
int ensure_prop_accessor(tc* c, const char* type, const char* name, char* out_cname, ty* out_ret) {
    char cn[192];
    snprintf(cn, sizeof cn, "ctron_prop_%s_%s", type, name);
    const cprop* use = NULL;
    for (size_t i = 0; i < c->n_protos; i++)
        if (!strcmp(c->protos[i], cn)) {
            snprintf(out_cname, 192, "%s", cn);
            const cprop* mp0 = find_impl_prop(c, type, name);
            use = mp0 ? mp0 : find_default_prop_for(c, type, name, NULL);
            if (use) { *out_ret = decl_ty_tc(c, use->ty); return 1; }
            return 1;
        }
    const cprop* mp = find_impl_prop(c, type, name);
    const cprop* dp = mp ? NULL : find_default_prop_for(c, type, name, NULL);
    use = mp ? mp : dp;
    if (!use) return 0;
    use_proto(c, cn);
    snprintf(out_cname, 192, "%s", cn);
    *out_ret = decl_ty_tc(c, use->ty);
    emit_prop_accessor(c, type, use, cn);
    return 1;
}

// ================= trait 参数单态化(C10-h)=================
// fn 带 &Trait 形参时为“泛型”;原体/原型不发射,调用点按实参具体类型例化。
int fn_has_trait_param(tc* c, const cfn* F) {
    if (!F) return 0;
    for (size_t j = 0; j < F->nparams; j++) {
        const cparam* p = &F->params[j];
        if (p->is_receiver || !p->ty) continue;
        if (decl_ty_tc(c, p->ty).k == T_TRAIT) return 1;
    }
    return 0;
}
const cfn* fn_named(const cfile* f, const char* nm) {
    if (!f) return NULL;
    for (size_t i = 0; i < f->ndecls; i++) {
        const cdecl* d = &f->decls[i];
        if (d->kind == D_FN && d->fn_.name && !strcmp(d->fn_.name, nm)) return &d->fn_;
    }
    return NULL;
}
// argtys:模板形参(fn_lookup,含 T_TRAIT);atys:调用点实参实际类型。
void ensure_mono_fn(tc* c, const cfn* F, const char* cname,
                           const ty* argtys, const ty* atys, int nparams) {
    if (c->err) return;
    for (size_t i = 0; i < c->n_monos; i++)
        if (!strcmp(c->monos[i], cname)) return; // 已例化:复用
    if (c->n_monos >= 64) { terr(c, "v1:单态化函数数量超限"); return; }
    c->monos[c->n_monos++] = ctron_arena_strndup(c->a, cname, strlen(cname));

    // subs:trait 形参名 → 实参具体类型(发射期间生效;嵌套例化由内层覆盖)
    size_t saved_subs = c->n_subs;
    for (int j = 0; j < nparams && j < 8; j++) {
        if (argtys[j].k == T_TRAIT && c->n_subs < 8) {
            c->subs[c->n_subs].from = argtys[j].tname;
            c->subs[c->n_subs].to = atys[j];
            c->n_subs++;
        }
    }
    // 原型(具体签名,进头部;含 ';')
    sb proto = {0};
    ty ret = decl_ty_tc(c, F->ret);
    const char* rct = (ret.k == T_FLT) ? "double" : (ret.k == T_BOOL) ? "int"
                    : (ret.k == T_STR) ? "const char*" : (ret.k == T_INT) ? "int64_t"
                    : (ret.k == T_STRUCT || ret.k == T_ENUM || ret.k == T_SUM || ret.k == T_LIST
                       || ret.k == T_CLASS || ret.k == T_BOX || ret.k == T_TUP) ? ctype_of(ret) : "void";
    sb_f(&proto, "static %s %s(", rct, cname);
    int firstp = 1;
    for (size_t j = 0; j < F->nparams; j++) {
        const cparam* p = &F->params[j];
        if (p->is_receiver) continue;
        ty pt = decl_ty_tc(c, p->ty);
        if (pt.k == T_UNK) { terr(c, "v1:单态化参数 %s 类型未知", p->name ? p->name : "?"); sb_free(&proto); c->n_subs = saved_subs; return; }
        if (!firstp) sb_s(&proto, ", ");
        firstp = 0;
        sb_f(&proto, "%s ctron_p_%s", ctype_of(pt), p->name ? p->name : "_");
    }
    sb_s(&proto, ");");
    if (c->n_monoprotos < 64)
        c->monoprotos[c->n_monoprotos++] =
            ctron_arena_strndup(c->a, proto.d ? proto.d : "", proto.d ? strlen(proto.d) : 0);
    sb_free(&proto);

    // 函数体走临时缓冲(直接写 body/m_sb 会把 ensure 嵌套体插进未闭合函数中间)
    sb tmp = {0};
    sb* saved_out = c->out_sb;
    c->out_sb = &tmp;
    emit_fn(c, F, cname);
    c->out_sb = saved_out;
    c->n_subs = saved_subs;
    if (tmp.d) sb_s(&c->s_sb, tmp.d);
    sb_free(&tmp);
}

// ================= 泛型 fn 单态化(C10-p)=================
int fn_is_generic(const cfn* F) { return F && F->ntype_params > 0; }

// 调用点例化:subs 已由调用方压入(参数/返回类型在替换下解析),原型+体复用 ensure_mono_fn;
// subs 的恢复由调用方负责(嵌套例化依赖内层覆盖)。
void ensure_mono_generic(tc* c, const cfn* F, const char* cname,
                         char** pnames, ty* pvals, size_t np) {
    (void)pnames;
    (void)pvals;
    (void)np;
    ensure_mono_fn(c, F, cname, NULL, NULL, 0);
}

// derive(Show)/结构化合成(C10-p):字段全可显示的 struct 合成 show 方法
// (显式 @derive(Show) 与 bound 满足的结构可显示性同走此路径;对齐 rt derive 逐字段格式面)
static int struct_showable(tc* c, const char* type, int depth) {
    if (!type || depth > 6) return 0;
    sdef* sd = NULL;
    for (size_t i = 0; i < c->nstructs; i++)
        if (!strcmp(c->structs[i].name, type)) { sd = &c->structs[i]; break; }
    if (!sd) return 0;
    for (size_t j = 0; j < sd->n; j++) {
        ty ft = sd->fields[j].t;
        if (ft.k == T_INT || ft.k == T_FLT || ft.k == T_BOOL || ft.k == T_STR) continue;
        if (ft.k == T_STRUCT && ft.tname && struct_showable(c, ft.tname, depth + 1)) continue;
        return 0;
    }
    return 1;
}
int ensure_derived_show(tc* c, const char* type, char* out_cname, ty* out_ret, int depth) {
    if (depth > 6) return 0;
    sdef* sd = NULL;
    for (size_t i = 0; i < c->nstructs; i++)
        if (!strcmp(c->structs[i].name, type)) { sd = &c->structs[i]; break; }
    if (!sd || !struct_showable(c, type, 0)) return 0;
    char cn[192];
    snprintf(cn, sizeof cn, "ctron_m_%s_show", type);
    for (size_t i = 0; i < c->n_protos; i++)
        if (!strcmp(c->protos[i], cn)) {
            snprintf(out_cname, 192, "%s", cn);
            *out_ret = ty_str();
            return 1;
        }
    use_proto(c, cn);
    use_helper(c, "ctron_str_concat");
    // 串接链:"<Type> {" + " k=<val>"*n + " }"(rt derive(Show) 同族格式)
    sb cur = {0};
    sb_f(&cur, "\"%.80s {\"", type);
    for (size_t j = 0; j < sd->n; j++) {
        const char* fn2 = sd->fields[j].name;
        ty ft = sd->fields[j].t;
        sb v = {0};
        if (ft.k == T_INT) { use_helper(c, "ctron_fmt_i64"); sb_f(&v, "ctron_fmt_i64(self.%s)", fn2); }
        else if (ft.k == T_BOOL) { use_helper(c, "ctron_fmt_bool"); sb_f(&v, "ctron_fmt_bool(self.%s)", fn2); }
        else if (ft.k == T_FLT) { use_helper(c, "ctron_fmt_f64"); sb_f(&v, "ctron_fmt_f64(self.%s)", fn2); }
        else if (ft.k == T_STR) sb_f(&v, "self.%s", fn2);
        else if (ft.k == T_STRUCT && ft.tname) {
            char sub[192];
            ty srt;
            if (!ensure_derived_show(c, ft.tname, sub, &srt, depth + 1)) { sb_free(&v); sb_free(&cur); return 0; }
            sb_f(&v, "%s(self.%s)", sub, fn2);
        }
        if (v.n == 0) { sb_free(&v); sb_free(&cur); return 0; }
        sb nxt = {0};
        sb_f(&nxt, "ctron_str_concat(%s, \" %s=\")", cur.d ? cur.d : "\"\"", fn2);
        sb v2 = {0};
        sb_f(&v2, "ctron_str_concat(%s, %s)", nxt.d ? nxt.d : "\"\"", v.d ? v.d : "\"\"");
        sb_free(&nxt);
        sb_free(&v);
        sb_free(&cur);
        cur = v2;
    }
    sb fin = {0};
    sb_f(&fin, "ctron_str_concat(%s, \" }\")", cur.d ? cur.d : "\"\"");
    sb_free(&cur);
    sb_f(&c->m_sb, "static const char* %s(ctron_t_%s self) {\n    return %s;\n}\n", cn, type, fin.d ? fin.d : "\"\"");
    sb_free(&fin);
    snprintf(out_cname, 192, "%s", cn);
    *out_ret = ty_str();
    return 1;
}
