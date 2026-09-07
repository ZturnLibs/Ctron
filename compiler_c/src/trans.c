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
#include "parser.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trans_internal.h"

// trans.c —— 文件级:声明收集/头部装配/转译入口
// ================= 文件级 =================
void collect_types(tc* c, const cfile* f) {
    for (size_t i = 0; i < f->ndecls; i++) {
        const cdecl* d = &f->decls[i];
        if (d->kind == D_STRUCT) {
            if (d->strukt.ntype_params > 0) continue; // 泛型 struct:字面量点按实例单态化(C10-p)
            if (c->nstructs >= MAX_TYPES) continue;
            sdef* sd = &c->structs[c->nstructs];
            memset(sd, 0, sizeof *sd);
            sd->name = d->strukt.name;
            for (size_t j = 0; j < d->strukt.nfields && sd->n < MAX_FIELDS; j++) {
                const cfield* fl = &d->strukt.fields[j];
                sd->fields[sd->n].name = fl->name;
                sd->fields[sd->n].t = decl_ty_tc(c, fl->ty);
                sd->n++;
            }
            c->nstructs++;
        } else if (d->kind == D_ENUM) {
            if (c->nenums >= MAX_TYPES) continue;
            edef* ed = &c->enums[c->nenums];
            memset(ed, 0, sizeof *ed);
            ed->name = d->en.name;
            for (size_t j = 0; j < d->en.nvariants && ed->n < MAX_VARIANTS; j++) {
                evar* ev = &ed->variants[ed->n];
                if (d->en.variants[j].kind == VK_UNIT) {
                    ev->name = d->en.variants[j].name;
                    ev->has_p = 0;
                } else if (d->en.variants[j].kind == VK_TUPLE && d->en.variants[j].ntys == 1) {
                    ev->name = d->en.variants[j].name;
                    ev->pty = decl_ty_tc(c, d->en.variants[j].tys[0]);
                    ev->has_p = 1;
                } else { terr(c, "v1:变体载荷不支持:%s", d->en.variants[j].name); continue; }
                ed->n++;
            }
            c->nenums++;
        } else if (d->kind == D_STATIC) {
            if (c->n_globals >= 32) continue;
            ty gt = decl_ty_tc(c, d->statik.ty);
            if (gt.k == T_UNK) { terr(c, "v1:static %s 类型不支持", d->statik.name); continue; }
            sb init = {0};
            if (d->statik.expr) emit_expr(c, d->statik.expr, &init);
            c->globals[c->n_globals].name = d->statik.name;
            c->globals[c->n_globals].t = gt;
            c->globals[c->n_globals].init = init.d ? ctron_arena_strndup(c->a, init.d, strlen(init.d)) : NULL;
            c->n_globals++;
            sb_free(&init);
        } else if (d->kind == D_TRAIT) {
            // trait 名登记(&Trait 形参解析; C10-h);默认方法/prop 体照旧
            if (d->trait.name && c->n_traits < 64)
                c->traits[c->n_traits++] = d->trait.name;
            for (size_t j = 0; j < d->trait.nitems && c->n_defaults < 64; j++) {
                const ctraititem* it = &d->trait.items[j];
                if (it->kind == TI_METHOD && it->m && it->m->body) {
                    c->defaults[c->n_defaults].trait = d->trait.name;
                    c->defaults[c->n_defaults].name = it->m->name;
                    c->defaults[c->n_defaults].f = it->m;
                    c->defaults[c->n_defaults].p = NULL;
                    c->n_defaults++;
                }
            }
        } else if (d->kind == D_IMPL) {
            // impl:方法/prop 记入方法表;impl 对记入 impls(默认体适用性)
            const char* tyh = head_name(d->impl.for_ty);
            const char* trh = head_name(d->impl.trait_ty);
            if (!tyh) continue;
            if (trh && c->n_impls < 64) {
                c->impls[c->n_impls].type = (char*)tyh;
                c->impls[c->n_impls].trait = (char*)trh;
                c->n_impls++;
            }
            for (size_t j = 0; j < d->impl.nitems && c->n_methods < 128; j++) {
                const cimplitem* it = &d->impl.items[j];
                if (it->kind == II_METHOD && it->m && it->m->body) {
                    c->methods[c->n_methods].type = (char*)tyh;
                    c->methods[c->n_methods].trait = (char*)trh;
                    c->methods[c->n_methods].name = it->m->name;
                    c->methods[c->n_methods].f = it->m;
                    c->methods[c->n_methods].p = NULL;
                    c->n_methods++;
                } else if (it->kind == II_PROP && it->p && it->p->body) {
                    c->methods[c->n_methods].type = (char*)tyh;
                    c->methods[c->n_methods].trait = (char*)trh;
                    c->methods[c->n_methods].name = it->p->name;
                    c->methods[c->n_methods].f = NULL;
                    c->methods[c->n_methods].p = it->p;
                    c->n_methods++;
                }
            }
        } else if (d->kind == D_CLASS) {
            if (d->klass.ntype_params > 0) continue; // 泛型 class:字面量点按实例单态化(C10-p)
            if (c->nclasses >= MAX_TYPES) continue;
            cdef* cd = &c->classes[c->nclasses];
            memset(cd, 0, sizeof *cd);
            cd->name = d->klass.name;
            int has_fn = 0;
            for (size_t j = 0; j < d->klass.nitems && cd->n < MAX_FIELDS; j++) {
                const cclassitem* it = &d->klass.items[j];
                if (it->kind == CT_FIELD && it->f) {
                    cd->fields[cd->n].name = it->f->name;
                    cd->fields[cd->n].t = decl_ty_tc(c, it->f->ty);
                    cd->n++;
                } else has_fn = 1;
            }
            if (has_fn) terr(c, "v1:class 方法/prop 不支持(C10-f):%s", d->klass.name);
            c->nclasses++;
        }
    }
}

void collect_fns(tc* c, const cfile* f) {
    for (size_t i = 0; i < f->ndecls; i++) {
        const cdecl* d = &f->decls[i];
        if (d->kind != D_FN) continue;
        if (d->fn_.abi) { terr(c, "v1 不支持 extern:%s", d->fn_.name); continue; }
        ty ret = decl_ty_tc(c, d->fn_.ret);
        ty ptys[8];
        for (size_t j = 0; j < d->fn_.nparams && j < 8; j++) ptys[j] = decl_ty_tc(c, d->fn_.params[j].ty);
        add_fn(c, d->fn_.name, ret, (int)d->fn_.nparams, ret.k == T_UNK, ptys);
    }
}

void emit_fn(tc* c, const cfn* F, const char* cname) {
    ty ret = decl_ty_tc(c, F->ret);
    c->fn_ret = &ret;
    const char* rct = (ret.k == T_FLT) ? "double" : (ret.k == T_BOOL) ? "int" : (ret.k == T_STR) ? "const char*" : (ret.k == T_INT) ? "int64_t" : (ret.k == T_STRUCT || ret.k == T_ENUM || ret.k == T_SUM || ret.k == T_LIST || ret.k == T_CLASS || ret.k == T_BOX || ret.k == T_TUP) ? ctype_of(ret) : "void";
    sb* B = c->out_sb ? c->out_sb : &c->body;
    int saved_it = c->in_test; // in_test 泄漏会让方法/例化函数体裸 return
    c->in_test = 0;
    sb_f(B, "static %s %s(", rct, cname);
    scope_push(c);
    for (size_t i = 0; i < F->nparams; i++) {
        const cparam* p = &F->params[i];
        if (p->is_receiver) { terr(c, "v1 不支持 receiver"); return; }
        ty pt = decl_ty_tc(c, p->ty);
        if (pt.k == T_UNK) { terr(c, "v1:参数 %s 需类型注解", p->name ? p->name : "?"); return; }
        if (pt.k == T_ARR) use_arr(c, ewlname(pt)); // 数组参数 typedef 注册
        if (!p->name) { terr(c, "v1:参数缺名"); return; }
        if (i) sb_s(B, ", ");
        sb_f(B, "%s ctron_p_%s", ctype_of(pt), p->name);
    }
    sb_s(B, ") {\n");
    for (size_t i = 0; i < F->nparams; i++) {
        const cparam* p = &F->params[i];
        ty pt = decl_ty_tc(c, p->ty);
        if (!p->name) continue;
        if (pt.k == T_INT) {
            char h[64];
            snprintf(h, sizeof h, "ctron_decl_%s", wlname(pt));
            use_helper(c, h);
            sb_f(B, "    %s %s = %s(ctron_p_%s);\n", ctype_of(pt), p->name, h, p->name);
            scope_def(c, p->name, pt);
        } else {
            sb_f(B, "    %s %s = ctron_p_%s;\n", ctype_of(pt), p->name, p->name);
            scope_def(c, p->name, pt);
        }
    }
    if (F->body) emit_block(c, F->body, B);
    scope_pop(c);
    sb_s(B, "}\n");
    c->in_test = saved_it;
    c->fn_ret = NULL;
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
    c.srcf = f;
    c.out_sb = &c.body;

    size_t ntests = 0;
    for (size_t i = 0; i < f->ndecls; i++)
        if (f->decls[i].kind == D_TEST) ntests++;

    collect_types(&c, f);
    collect_fns(&c, f);
    if (!c.err && c.nfns == 0 && ntests == 0) terr(&c, "v1:文件没有可转译的函数");

    for (size_t i = 0; i < f->ndecls && !c.err; i++) {
        const cdecl* d = &f->decls[i];
        switch (d->kind) {
        case D_FN: {
            const char* nm = d->fn_.name;
            if (is_reserved(nm)) { terr(&c, "v1:标识符保留前缀 ctron_:%s", nm); break; }
            if (fn_is_generic(&d->fn_) || fn_has_trait_param(&c, &d->fn_)) break; // 泛型原体:调用点按实参单态化(C10-h/p)
            char cn[256];
            snprintf(cn, sizeof cn, "ctron_user_%s", nm);
            int was_main = !strcmp(nm, "main");
            c.in_main = was_main;
            emit_fn(&c, &d->fn_, cn);
            c.in_main = 0;
            break;
        }
        case D_CONST: {
            // const 声明:字面量初始化静态发射;非常量(comptime fn 调用)→ ctron_ginit 运行时初始化
            const char* nm2 = d->konst.name;
            if (!nm2 || is_reserved(nm2)) { terr(&c, "v1:const 标识符:%s", nm2 ? nm2 : "?"); break; }
            ty gt = decl_ty_tc(&c, d->konst.ty);
            if (gt.k == T_UNK) { terr(&c, "v1:const %s 类型不支持", nm2); break; }
            cexpr* ie = d->konst.expr;
            int literal = ie && (ie->kind == EX_INT || ie->kind == EX_FLOAT || ie->kind == EX_BOOL || ie->kind == EX_STR);
            sb init2 = {0};
            if (ie) {
                const ty* sw3 = c.want;
                c.want = &gt;
                emit_expr(&c, ie, &init2);
                c.want = sw3;
            }
            if (c.err) { sb_free(&init2); break; }
            if (c.n_globals < 32) {
                c.globals[c.n_globals].name = d->konst.name;
                c.globals[c.n_globals].t = gt;
                c.globals[c.n_globals].init = init2.d && literal
                    ? ctron_arena_strndup(c.a, init2.d, strlen(init2.d)) : NULL;
                c.n_globals++;
            }
            if (!literal)
                sb_f(&c.ginit_sb, "    %s = %s;\n", nm2, init2.d ? init2.d : "0");
            sb_free(&init2);
            break;
        }
        case D_TEST: {
            char cn[64];
            snprintf(cn, sizeof cn, "ctron_test_%zu", i);
            sb_f(&c.body, "static void %s(void) {\n", cn);
            c.sc = NULL;
            scope_push(&c);
            c.in_test = 1;
            c.fn_ret = NULL;
            emit_block(&c, d->test.body, &c.body);
            c.in_test = 0;
            scope_pop(&c);
            sb_s(&c.body, "}\n");
            break;
        }
        case D_USE: break;
        case D_STRUCT:
        case D_CLASS:
        case D_ENUM:
        case D_STATIC:
        case D_TRAIT:
        case D_IMPL:
            break; // typedef/全局由头部装配统一发射(collect_types 已收集)
        default:
            terr(&c, "v1 不支持该声明(kind %d)", (int)d->kind);
            break;
        }
    }

    if (!c.err) {
        int has_ginit = c.ginit_sb.d && c.ginit_sb.n > 0;
        if (has_ginit) // const 运行时初始化(原型在头部,先于 main/测试体)
            sb_f(&c.m_sb, "static void ctron_ginit(void) {\n%s}\n", c.ginit_sb.d);
        int has_fn_main = 0;
        for (size_t i = 0; i < f->ndecls; i++)
            if (f->decls[i].kind == D_FN && !strcmp(f->decls[i].fn_.name, "main")) has_fn_main = 1;
        if (ntests) {
            sb_f(&c.body, "int main(void) {\n    if (setjmp(ctron_panic_frame)) return 1;\n");
            if (has_ginit) sb_s(&c.body, "    ctron_ginit();\n");
            for (size_t i = 0; i < f->ndecls; i++)
                if (f->decls[i].kind == D_TEST) sb_f(&c.body, "    ctron_test_%zu();\n", i);
            sb_s(&c.body, "    return 0;\n}\n");
        } else if (has_fn_main) {
            sb_f(&c.body, "int main(void) {\n    if (setjmp(ctron_panic_frame)) return 1;\n%s    return (int)ctron_user_main();\n}\n",
                 has_ginit ? "    ctron_ginit();\n" : "");
        } else {
            terr(&c, "v1:文件既无 fn main 也无 test 块");
        }
    }

    if (!c.err) {
        sb* h = &c.head;
        sb_s(h, "/* 由 ctronc trans(C10-a/b)生成;语义对齐 rt.c 解释器 */\n"
                "#include <stdio.h>\n#include <stdint.h>\n#include <stdlib.h>\n"
                "#include <string.h>\n#include <setjmp.h>\n\n"
                "typedef struct { const char** d; int64_t n; } ctron_arr_str;\n"
                "typedef struct { const char** d; int64_t n; int64_t cap; } ctron_list_str;\n"
                "typedef struct { int64_t lo, hi; int incl; } ctron_rng;\n"
                "typedef int64_t (*ctron_fnptr)();\n" // 无原型函数指针:任意实参数调用点经显式 cast 合法调用
                "typedef struct ctron_anyerr ctron_anyerr;\n"
                "struct ctron_anyerr { char* message; ctron_anyerr* cause; char* trace; };\n"
                "typedef struct { int cancelled; } ctron_scope;\n"
                "typedef struct { int panicked; char msg[256]; } ctron_task_base;\n"
                "typedef struct ctron_task_ctx ctron_task_ctx;\n"
                "struct ctron_task_ctx { ctron_task_base* rec; jmp_buf jb; };\n"
                "static jmp_buf ctron_panic_frame;\n"
                "static ctron_task_ctx* ctron_task_cur;\n"
                "static ctron_scope* ctron_scope_cur;\n"
                "static _Noreturn void ctron_panic(const char* msg) {\n"
                "    if (ctron_task_cur) { snprintf(ctron_task_cur->rec->msg, sizeof ctron_task_cur->rec->msg, \"%s\", msg); longjmp(ctron_task_cur->jb, 1); }\n"
                "    fprintf(stderr, \"%s\\n\", msg);\n"
                "    longjmp(ctron_panic_frame, 1);\n"
                "}\n"
                "static void ctron_print_nl(void) { printf(\"\\n\"); }\n"
                "static int ctron_assert(int v) { if (!v) ctron_panic(\"assert failed\"); return 1; }\n");
        for (size_t i = 0; i < c.nclasses; i++) { // class typedef:引用语义(sums 的指针字段依赖其前置)
            cdef* cd = &c.classes[i];
            sb_f(h, "typedef struct {");
            for (size_t j = 0; j < cd->n; j++)
                sb_f(h, " %s %s;", ctype_of(cd->fields[j].t), cd->fields[j].name);
            sb_f(h, " } ctron_c_%s;\n", cd->name);
        }
        for (size_t i = 0; i < c.nenums; i++) { // enum typedef + 变体号
            edef* ed = &c.enums[i];
            int has_payload = 0;
            for (size_t j = 0; j < ed->n; j++)
                if (ed->variants[j].has_p) has_payload = 1;
            if (has_payload) {
                sb_f(h, "typedef struct { int tag; union {");
                for (size_t j = 0; j < ed->n; j++)
                    if (ed->variants[j].has_p)
                        sb_f(h, " %s u_%s;", ctype_of(ed->variants[j].pty), ed->variants[j].name);
                sb_f(h, " } as; } ctron_e_%s;\n", ed->name);
            } else {
                sb_f(h, "typedef struct { int tag; } ctron_e_%s;\n", ed->name);
            }
            for (size_t j = 0; j < ed->n; j++)
                sb_f(h, "#define CTRON_%s_%s %lld\n", ed->name, ed->variants[j].name, (long long)j);
        }
        for (size_t i = 0; i < c.n_sums; i++) // 和类型/运行时单元(引用 class 指针/enum 值)
            sb_f(h, "%s\n", c.sums[i]);
        for (size_t i = 0; i < c.n_arrs; i++) {
            const char* wl = c.arrs[i];
            if (!strcmp(wl, "str")) continue; // str typedef 已在头部
            const char* dt = dt_for_wl(wl);
            sb_f(h, "typedef struct { %s* d; int64_t n; } ctron_arr_%s;\n", dt, wl);
            sb_f(h, "typedef struct { %s* d; int64_t n; int64_t cap; } ctron_list_%s;\n", dt, wl);
            sb_f(h, "static void ctron_list_%s_push(ctron_list_%s* l, %s v) { if (l->n == l->cap) { l->cap = l->cap ? l->cap * 2 : 4; %s* nd = (%s*)realloc(l->d, (size_t)l->cap * sizeof(%s)); l->d = nd; } l->d[l->n++] = v; }\n",
                 wl, wl, dt, dt, dt, dt);
            sb_f(h, "static ctron_list_%s ctron_list_%s_clone(ctron_list_%s l) { ctron_list_%s r; r.n = l.n; r.cap = l.n ? l.n : 4; %s* nd = (%s*)malloc((size_t)r.cap * sizeof(%s)); for (int64_t i = 0; i < l.n; i++) nd[i] = l.d[i]; r.d = nd; return r; }\n",
                 wl, wl, wl, wl, dt, dt, dt);
        }
        for (size_t i = 0; i < c.nstructs; i++) { // struct typedef:值语义(字段可含共享单元格指针)
            sdef* sd = &c.structs[i];
            sb_f(h, "typedef struct {");
            for (size_t j = 0; j < sd->n; j++)
                sb_f(h, " %s %s;", ctype_of(sd->fields[j].t), sd->fields[j].name);
            sb_f(h, " } ctron_t_%s;\n", sd->name);
        }
        for (size_t i = 0; i < c.n_globals; i++)
            sb_f(h, "static %s %s = %s;\n", ctype_of(c.globals[i].t), c.globals[i].name,
                 c.globals[i].init ? c.globals[i].init : "0");
        for (size_t i = 0; i < c.n_helpers; i++)
            emit_helper(&c, c.helpers[i]);
        for (size_t i = 0; i < f->ndecls; i++) {
            const cdecl* d = &f->decls[i];
            if (d->kind != D_FN) continue;
            if (fn_is_generic(&d->fn_) || fn_has_trait_param(&c, &d->fn_)) continue; // 泛型原体:原型由单态化点提供
            ty ret = decl_ty_tc(&c, d->fn_.ret);
            const char* rct = (ret.k == T_FLT) ? "double" : (ret.k == T_BOOL) ? "int" : (ret.k == T_STR) ? "const char*" : (ret.k == T_INT) ? "int64_t" : (ret.k == T_STRUCT || ret.k == T_ENUM || ret.k == T_SUM || ret.k == T_LIST || ret.k == T_CLASS || ret.k == T_BOX || ret.k == T_TUP) ? ctype_of(ret) : "void";
            sb_f(h, "static %s ctron_user_%s(", rct, d->fn_.name);
            for (size_t j = 0; j < d->fn_.nparams; j++) {
                ty pt = decl_ty_tc(&c, d->fn_.params[j].ty);
                if (j) sb_s(h, ", ");
                sb_f(h, "%s ctron_p_%s", ctype_of(pt),
                     d->fn_.params[j].name ? d->fn_.params[j].name : "_");
            }
            sb_s(h, ");\n");
        }
        for (size_t i = 0; i < c.n_monoprotos; i++) // C10-h 单态化原型
            sb_f(h, "%s\n", c.monoprotos[i]);
        for (size_t i = 0; i < f->ndecls; i++)
            if (f->decls[i].kind == D_TEST) sb_f(h, "static void ctron_test_%zu(void);\n", i);
        sb_s(h, "\n");
    }

    if (c.err) {
        res.err = strdup(c.err);
        sb_free(&c.head);
        sb_free(&c.body);
        sb_free(&c.s_sb);
        sb_free(&c.clo_sb);
        sb_free(&c.ginit_sb);
        ctron_arena_free(a);
        return res;
    }

    sb all = {0};
    sb_s(&all, c.head.d ? c.head.d : "");
    sb_s(&all, c.m_sb.d ? c.m_sb.d : "");
    sb_s(&all, c.s_sb.d ? c.s_sb.d : "");
    sb_s(&all, c.clo_sb.d ? c.clo_sb.d : "");
    sb_s(&all, c.body.d ? c.body.d : "");
    res.code = all.d ? all.d : strdup("");
    sb_free(&c.head);
    sb_free(&c.body);
    sb_free(&c.s_sb);
    sb_free(&c.clo_sb);
    sb_free(&c.ginit_sb);
    ctron_arena_free(a);
    return res;
}

ty box_elem(ty t) {
    ty e = ty_unk(); e.k = t.ek; e.bits = t.ebits; e.us = t.eus;
    if (t.ek == T_FLT) e = ty_flt();
    else if (t.ek == T_BOOL) e = ty_bool();
    else if (t.ek == T_STR) e = ty_str();
    else if (t.ek == T_STRUCT || t.ek == T_CLASS || t.ek == T_ENUM) e.tname = t.tname;
    return e;
}
ty sum_ty_of(tc* c, const cty* t) {
    if (!t) return ty_unk();
    char def[512], name[96];
    if (t->kind == TY_OPT) {
        ty e = decl_ty_tc(c, t->sub);
        if (e.k == T_UNK) return ty_unk();
        char* k = ty_mangle(c, e);
        snprintf(name, sizeof name, "ctron_opt_%s", k);
        snprintf(def, sizeof def,
            "typedef struct { int tag; union { %s some; } as; } %s;\n"
            "#define CTRON_OPT_NONE 0\n"
            "#define CTRON_OPT_SOME 1\n", ctype_of(e), name);
        use_sum(c, def);
        ty r = ty_unk(); r.k = T_SUM; r.tname = ctron_arena_strndup(c->a, name, strlen(name));
        r.ek = e.k; r.ebits = e.bits; r.eus = e.us; r.tname = r.tname;
        return r;
    }
    if (t->kind == TY_NAMED && t->npath == 1 && !strcmp(t->path[0], "Option") && t->nargs >= 1) {
        ty e = decl_ty_tc(c, t->args[0]);
        if (e.k == T_UNK) return ty_unk();
        char* k = ty_mangle(c, e);
        char name[96], def[512];
        snprintf(name, sizeof name, "ctron_opt_%s", k);
        snprintf(def, sizeof def,
            "typedef struct { int tag; union { %s some; } as; } %s;\n"
            "#define CTRON_OPT_NONE 0\n"
            "#define CTRON_OPT_SOME 1\n", ctype_of(e), name);
        use_sum(c, def);
        ty r = ty_unk(); r.k = T_SUM; r.tname = ctron_arena_strndup(c->a, name, strlen(name));
        r.ek = e.k; r.ebits = e.bits; r.eus = e.us;
        return r;
    }
    if (t->kind == TY_NAMED && t->npath == 1 && !strcmp(t->path[0], "Result") && t->nargs >= 2) {
        ty a = decl_ty_tc(c, t->args[0]);
        ty b = decl_ty_tc(c, t->args[1]);
        if (a.k == T_UNK || b.k == T_UNK) return ty_unk();
        char* k1 = ty_mangle(c, a);
        char* k2 = ty_mangle(c, b);
        snprintf(name, sizeof name, "ctron_res_%s_%s", k1, k2);
        snprintf(def, sizeof def,
            "typedef struct { int tag; union { %s ok; %s err; } as; } %s;\n"
            "#define CTRON_RES_OK 0\n"
            "#define CTRON_RES_ERR 1\n", ctype_of(a), ctype_of(b), name);
        use_sum(c, def);
        ty r = ty_unk(); r.k = T_SUM; r.tname = ctron_arena_strndup(c->a, name, strlen(name));
        r.ek = a.k; r.ebits = a.bits; r.eus = a.us; // Ok 载荷
        r.ek2 = b.k; r.ebits2 = b.bits; r.eus2 = b.us;
        r.tname2 = (b.k == T_STRUCT || b.k == T_ENUM) ? b.tname : NULL;
        return r;
    }
    return ty_unk();
}

ty decl_ty(const cty* t) {
    if (!t) return ty_unk();
    if (t->kind == TY_SLICE) {
        ty e = decl_ty(t->sub);
        return e.k == T_UNK ? ty_unk() : ty_arr(e);
    }
    if (t->kind == TY_ARRAY) {
        ty e = decl_ty(t->elem);
        return e.k == T_UNK ? ty_unk() : ty_arr(e); // 维度不校验(对齐 rt)
    }
    if (t->kind != TY_NAMED || t->npath != 1) return ty_unk();
    const char* n = t->path[0];
    if (!strcmp(n, "AnyError")) { ty r = ty_unk(); r.k = T_ERR; return r; } // 错误链节点(C10-i)
    if (!strcmp(n, "Str")) return ty_str();
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
