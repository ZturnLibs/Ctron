// ast_show.c —— cfile → 确定性 Debug 文本(自举差分产物形态;C 版自定契约)。
// 风格取 Rust Debug 同族(变体名 + 字段名),便于将来与参照实现的语义对照。
#include "parser.h"
#include <stdio.h>

static void qstr(FILE* o, const char* s) {
    fputc('"', o);
    for (const unsigned char* p = (const unsigned char*)(s ? s : ""); *p; p++) {
        unsigned char c = *p;
        switch (c) {
        case '"': fputs("\\\"", o); break;
        case '\\': fputs("\\\\", o); break;
        case '\n': fputs("\\n", o); break;
        case '\t': fputs("\\t", o); break;
        case '\r': fputs("\\r", o); break;
        case '\0': fputs("\\0", o); break;
        default:
            if (c < 0x20) fprintf(o, "\\x%02X", c);
            else fputc((char)c, o);
        }
    }
    fputc('"', o);
}

static void vis_name(FILE* o, cvis v) {
    switch (v) {
    case VIS_PRIVATE: fputs("Private", o); break;
    case VIS_PUB: fputs("Pub", o); break;
    case VIS_PUBPKG: fputs("PubPkg", o); break;
    }
}

static void strlist(FILE* o, char** a, size_t n) {
    fputc('[', o);
    for (size_t i = 0; i < n; i++) {
        if (i) fputs(", ", o);
        fputc('"', o); fputs(a[i], o); fputc('"', o);
    }
    fputc(']', o);
}

static void attrlist(FILE* o, cattr* a, size_t n) {
    fputc('[', o);
    for (size_t i = 0; i < n; i++) {
        if (i) fputs(", ", o);
        fprintf(o, "Attribute { name: ");
        qstr(o, a[i].name);
        fputs(", args: ", o);
        strlist(o, a[i].args, a[i].nargs);
        fputc('}', o);
    }
    fputc(']', o);
}

static void show_type(FILE* o, cty* t);
static void show_expr(FILE* o, cexpr* e);
static void show_pat(FILE* o, cpat* p);

static void type_args_show(FILE* o, cty** args, size_t n) {
    fputc('[', o);
    for (size_t i = 0; i < n; i++) {
        if (i) fputs(", ", o);
        show_type(o, args[i]);
    }
    fputc(']', o);
}

static void show_type(FILE* o, cty* t) {
    if (!t) { fputs("(null)", o); return; }
    switch (t->kind) {
    case TY_NAMED:
        fputs("Named { path: ", o);
        strlist(o, t->path, t->npath);
        fputs(", args: ", o);
        type_args_show(o, t->args, t->nargs);
        fputc('}', o);
        break;
    case TY_REF: fputs("Ref(", o); show_type(o, t->sub); fputc(')', o); break;
    case TY_SLICE: fputs("Slice(", o); show_type(o, t->sub); fputc(')', o); break;
    case TY_ARRAY:
        fputs("Array { elem: ", o);
        show_type(o, t->elem);
        fputs(", size: ", o);
        if (t->size) show_expr(o, t->size);
        else fputs("None", o);
        fputc('}', o);
        break;
    case TY_OPT: fputs("Optional(", o); show_type(o, t->sub); fputc(')', o); break;
    case TY_TUPLE:
        fputs("Tuple(", o);
        for (size_t i = 0; i < t->nelems; i++) {
            if (i) fputs(", ", o);
            show_type(o, t->elems[i]);
        }
        fputc(')', o);
        break;
    case TY_FN:
        fputs("Fn { params: ", o);
        fputc('[', o);
        for (size_t i = 0; i < t->nelems; i++) {
            if (i) fputs(", ", o);
            show_type(o, t->elems[i]);
        }
        fputs("], ret: ", o);
        if (t->fret) show_type(o, t->fret);
        else fputs("None", o);
        fputc('}', o);
        break;
    case TY_SELF: fputs("SelfT", o); break;
    case TY_CVAL: fputs("ComptimeVal(", o); qstr(o, t->npath ? t->path[0] : ""); fputc(')', o); break;
    }
}

static void str_parts_show(FILE* o, ctron_str_part* sp, size_t n) {
    fputc('[', o);
    for (size_t i = 0; i < n; i++) {
        if (i) fputs(", ", o);
        fputs(sp[i].kind == PART_TEXT ? "Text(" : "Interp(", o);
        qstr(o, sp[i].s);
        fputc(')', o);
    }
    fputc(']', o);
}

static const char* binop_name(cbinop op) {
    switch (op) {
    case B_OR: return "Or"; case B_AND: return "AndAnd";
    case B_EQ: return "Eq"; case B_NE: return "Ne";
    case B_LT: return "Lt"; case B_GT: return "Gt";
    case B_LE: return "Le"; case B_GE: return "Ge";
    case B_ADD: return "Add"; case B_SUB: return "Sub";
    case B_WADD: return "WrapAdd"; case B_WSUB: return "WrapSub";
    case B_MUL: return "Mul"; case B_DIV: return "Div"; case B_MOD: return "Mod";
    }
    return "?";
}

static void show_block(FILE* o, cblock* b);
static void show_arms(FILE* o, cmatcharm* arms, size_t n) {
    fputc('[', o);
    for (size_t i = 0; i < n; i++) {
        if (i) fputs(", ", o);
        fprintf(o, "MatchArm { pattern: ");
        show_pat(o, arms[i].pat);
        fputs(", expr: ", o);
        show_expr(o, arms[i].expr);
        fputc('}', o);
    }
    fputc(']', o);
}

static void show_expr(FILE* o, cexpr* e) {
    if (!e) { fputs("(null)", o); return; }
    switch (e->kind) {
    case EX_INT: fprintf(o, "Int { text: "); qstr(o, e->text);
        fprintf(o, ", suffix: "); qstr(o, e->suffix); fputc('}', o); break;
    case EX_FLOAT: fprintf(o, "Float { text: "); qstr(o, e->text);
        fprintf(o, ", suffix: "); qstr(o, e->suffix); fputc('}', o); break;
    case EX_STR: fprintf(o, "Str { parts: "); str_parts_show(o, e->sparts, e->nsparts);
        fputc('}', o); break;
    case EX_BOOL: fprintf(o, "Bool(%s)", e->bval ? "true" : "false"); break;
    case EX_VOID: fputs("Void", o); break;
    case EX_IDENT: fprintf(o, "Ident("); qstr(o, e->text); fputc(')', o); break;
    case EX_TUPLE:
        fputs("Tuple(", o);
        for (size_t i = 0; i < e->nelems; i++) {
            if (i) fputs(", ", o);
            show_expr(o, e->elems[i]);
        }
        fputc(')', o);
        break;
    case EX_ARRAY:
        fputs("Array(", o);
        for (size_t i = 0; i < e->nelems; i++) {
            if (i) fputs(", ", o);
            show_expr(o, e->elems[i]);
        }
        fputc(')', o);
        break;
    case EX_STRUCT:
        fputs("StructLit { path: ", o);
        strlist(o, e->path, e->npath);
        fputs(", fields: ", o);
        fputc('[', o);
        for (size_t i = 0; i < e->nfields; i++) {
            if (i) fputs(", ", o);
            fprintf(o, "StructField { name: ");
            qstr(o, e->fields[i].name);
            fputs(", value: ", o);
            if (e->fields[i].value) show_expr(o, e->fields[i].value);
            else fputs("None", o);
            fputc('}', o);
        }
        fputs("] }", o);
        break;
    case EX_UNARY:
        fprintf(o, "Unary { op: %s, expr: ", e->uop == UN_NEG ? "Neg" : "Not");
        show_expr(o, e->ux);
        fputc('}', o);
        break;
    case EX_BINARY:
        fprintf(o, "Binary { op: %s, lhs: ", binop_name(e->bop));
        show_expr(o, e->lhs);
        fputs(", rhs: ", o);
        show_expr(o, e->rhs);
        fputc('}', o);
        break;
    case EX_RANGE:
        fprintf(o, "Range { inclusive: %s, from: ", e->inclusive ? "true" : "false");
        show_expr(o, e->from);
        fputs(", to: ", o);
        show_expr(o, e->to);
        fputc('}', o);
        break;
    case EX_CALL:
        fputs("Call { callee: ", o);
        show_expr(o, e->callee);
        fputs(", args: ", o);
        fputc('[', o);
        for (size_t i = 0; i < e->nelems; i++) {
            if (i) fputs(", ", o);
            show_expr(o, e->elems[i]);
        }
        fputs("] }", o);
        break;
    case EX_INDEX:
        fputs("Index { obj: ", o);
        show_expr(o, e->obj);
        fputs(", index: ", o);
        show_expr(o, e->index);
        fputc('}', o);
        break;
    case EX_MEMBER:
        fputs("Member { obj: ", o);
        show_expr(o, e->obj);
        fputs(", target: ", o);
        if (e->m_is_name) { fprintf(o, "Name("); qstr(o, e->mname); fputc(')', o); }
        else fprintf(o, "TupleIndex(%u)", e->mtuple);
        fputc('}', o);
        break;
    case EX_TYPEARGS:
        fputs("TypeArgs { expr: ", o);
        show_expr(o, e->obj);
        fputs(", args: ", o);
        type_args_show(o, e->targs, e->ntargs);
        fputc('}', o);
        break;
    case EX_TRY:
        fputs("Try(", o);
        show_expr(o, e->obj);
        fputc(')', o);
        break;
    case EX_CLOSURE:
        fputs("Closure { params: ", o);
        fputc('[', o);
        for (size_t i = 0; i < e->ncparams; i++) {
            if (i) fputs(", ", o);
            fprintf(o, "ClosureParam { is_var: %s, name: ", e->cparams[i].is_var ? "true" : "false");
            qstr(o, e->cparams[i].name);
            fputs(", ty: ", o);
            if (e->cparams[i].ty) show_type(o, e->cparams[i].ty);
            else fputs("None", o);
            fputc('}', o);
        }
        fputs("], ret: ", o);
        if (e->cret) show_type(o, e->cret);
        else fputs("None", o);
        fputs(", body: ", o);
        show_expr(o, e->cbody);
        fputc('}', o);
        break;
    case EX_SCOPE:
        fprintf(o, "Scope { param: "); qstr(o, e->sparam);
        fputs(", body: ", o);
        show_block(o, e->sbody);
        fputc('}', o);
        break;
    case EX_OWN:
        fprintf(o, "Own { arena: "); qstr(o, e->arena_name);
        fputs(", body: ", o);
        show_block(o, e->obody);
        fputc('}', o);
        break;
    case EX_IF:
        fputs("If { cond: ", o);
        show_expr(o, e->cond);
        fputs(", then: ", o);
        show_block(o, e->then_b);
        fputs(", els: ", o);
        if (e->els) show_expr(o, e->els);
        else fputs("None", o);
        fputc('}', o);
        break;
    case EX_MATCH:
        fputs("Match { expr: ", o);
        show_expr(o, e->scrut);
        fputs(", arms: ", o);
        show_arms(o, e->arms, e->narms);
        fputc('}', o);
        break;
    case EX_BLOCK:
        fputs("BlockExpr(", o);
        show_block(o, e->block);
        fputc(')', o);
        break;
    }
}

static void show_stmt(FILE* o, cstmt* s) {
    if (!s) { fputs("(null)", o); return; }
    switch (s->kind) {
    case ST_LET:
        fprintf(o, "Let { is_var: %s, pattern: ", s->is_var ? "true" : "false");
        show_pat(o, s->pat);
        fputs(", ty: ", o);
        if (s->ty) show_type(o, s->ty);
        else fputs("None", o);
        fputs(", expr: ", o);
        show_expr(o, s->e);
        fputc('}', o);
        break;
    case ST_RET:
        fputs("Return(", o);
        if (s->e) show_expr(o, s->e);
        else fputs("None", o);
        fputc(')', o);
        break;
    case ST_FOR:
        fputs("For { pattern: ", o);
        show_pat(o, s->pat);
        fputs(", iter: ", o);
        show_expr(o, s->iter);
        fputs(", body: ", o);
        show_block(o, s->body);
        fputc('}', o);
        break;
    case ST_BREAK:
        fputs("Break", o);
        break;
    case ST_CONTINUE:
        fputs("Continue", o);
        break;
    case ST_WHILE:
        fputs("While { cond: ", o);
        show_expr(o, s->e);
        fputs(", body: ", o);
        show_block(o, s->body);
        fputc('}', o);
        break;
    case ST_ASSIGN: {
        static const char* const aop[] = {"Eq", "AddEq", "SubEq", "MulEq", "DivEq", "ModEq"};
        fprintf(o, "Assign { target: ");
        show_expr(o, s->target);
        fprintf(o, ", op: %s, value: ", aop[s->aop]);
        show_expr(o, s->value);
        fputc('}', o);
        break;
    }
    case ST_EXPR:
        fputs("Expr(", o);
        show_expr(o, s->e);
        fputc(')', o);
        break;
    }
}

static void show_block(FILE* o, cblock* b) {
    if (!b) { fputs("(null)", o); return; }
    fputs("Block { stmts: [", o);
    for (size_t i = 0; i < b->nstmts; i++) {
        if (i) fputs(", ", o);
        show_stmt(o, b->stmts[i]);
    }
    fputs("], tail: ", o);
    if (b->tail) show_expr(o, b->tail);
    else fputs("None", o);
    fputc('}', o);
}

static void show_pat(FILE* o, cpat* p) {
    if (!p) { fputs("(null)", o); return; }
    switch (p->kind) {
    case PAT_IDENT: fprintf(o, "Ident("); qstr(o, p->name); fputc(')', o); break;
    case PAT_WILD: fputs("Wildcard", o); break;
    case PAT_LIT:
        switch (p->lkind) {
        case PLIT_INT: fputs("Lit(Int(", o); qstr(o, p->name); fputs("))", o); break;
        case PLIT_FLOAT: fputs("Lit(Float(", o); qstr(o, p->name); fputs("))", o); break;
        case PLIT_STR: fputs("Lit(Str(", o); qstr(o, p->name); fputs("))", o); break;
        case PLIT_BOOL: fprintf(o, "Lit(Bool(%s))", p->lb ? "true" : "false"); break;
        }
        break;
    case PAT_TUPLE:
        fputs("Tuple(", o);
        for (size_t i = 0; i < p->nelems; i++) {
            if (i) fputs(", ", o);
            show_pat(o, p->elems[i]);
        }
        fputc(')', o);
        break;
    case PAT_AGG:
        fputs("Agg { path: ", o);
        strlist(o, p->path, p->npath);
        fputs(", sub: ", o);
        switch (p->agg) {
        case AG_UNIT: fputs("Unit", o); break;
        case AG_TUPLE:
            fputs("Tuple(", o);
            for (size_t i = 0; i < p->nelems; i++) {
                if (i) fputs(", ", o);
                show_pat(o, p->elems[i]);
            }
            fputc(')', o);
            break;
        case AG_STRUCT:
            fputs("Struct([", o);
            for (size_t i = 0; i < p->nsfields; i++) {
                if (i) fputs(", ", o);
                fprintf(o, "StructPatField { name: ");
                qstr(o, p->sfields[i].name);
                fputs(", pattern: ", o);
                if (p->sfields[i].pat) show_pat(o, p->sfields[i].pat);
                else fputs("None", o);
                fputc('}', o);
            }
            fputs("])", o);
            break;
        }
        fputc('}', o);
        break;
    }
}

static void show_field(FILE* o, cfield* f) {
    fprintf(o, "Field { vis: ");
    vis_name(o, f->vis);
    fprintf(o, ", is_var: %s, name: ", f->is_var ? "true" : "false");
    qstr(o, f->name);
    fputs(", ty: ", o);
    show_type(o, f->ty);
    fputc('}', o);
}

static void show_typeparam(FILE* o, ctypeparam* tp) {
    fprintf(o, "TypeParam { name: ");
    qstr(o, tp->name);
    fputs(", bound: ", o);
    strlist(o, tp->bounds, tp->nbounds);
    fprintf(o, ", is_comptime: %s }", tp->is_comptime ? "true" : "false");
}

static void show_ctypeparams(FILE* o, ctypeparam* tp, size_t n) {
    fputc('[', o);
    for (size_t i = 0; i < n; i++) {
        if (i) fputs(", ", o);
        show_typeparam(o, &tp[i]);
    }
    fputc(']', o);
}

static void show_fn(FILE* o, cfn* f, const char* tag) {
    fprintf(o, "%s { attrs: ", tag);
    attrlist(o, f->attrs, f->nattrs);
    fprintf(o, ", vis: ");
    vis_name(o, f->vis);
    fprintf(o, ", is_comptime: %s, abi: ", f->is_comptime ? "true" : "false");
    if (f->abi) { qstr(o, f->abi); } else fputs("None", o);
    fputs(", name: ", o);
    qstr(o, f->name);
    fputs(", type_params: ", o);
    show_ctypeparams(o, f->type_params, f->ntype_params);
    fputs(", params: [", o);
    for (size_t i = 0; i < f->nparams; i++) {
        if (i) fputs(", ", o);
        cparam* pr = &f->params[i];
        if (pr->is_receiver) {
            fprintf(o, "Receiver { is_var: %s }", pr->is_var ? "true" : "false");
        } else {
            fprintf(o, "Param { is_var: %s, name: ", pr->is_var ? "true" : "false");
            qstr(o, pr->name);
            fputs(", ty: ", o);
            show_type(o, pr->ty);
            fputc('}', o);
        }
    }
    fputs("], ret: ", o);
    if (f->ret) show_type(o, f->ret);
    else fputs("None", o);
    fputs(", body: ", o);
    if (f->body) show_block(o, f->body);
    else fputs("None", o);
    fputc('}', o);
}

static void show_prop(FILE* o, cprop* p, const char* tag) {
    fprintf(o, "%s { vis: ", tag);
    vis_name(o, p->vis);
    fputs(", name: ", o);
    qstr(o, p->name);
    fputs(", ty: ", o);
    show_type(o, p->ty);
    fputs(", body: ", o);
    if (p->body) show_block(o, p->body);
    else fputs("None", o);
    fputc('}', o);
}

static void show_decl(FILE* o, cdecl* d) {
    switch (d->kind) {
    case D_USE: {
        fprintf(o, "Use { imports: [");
        for (size_t i = 0; i < d->use.nimports; i++) {
            if (i) fputs(", ", o);
            strlist(o, d->use.imports[i].segs, d->use.imports[i].nsegs);
        }
        fputs("] }", o);
        break;
    }
    case D_STRUCT: {
        cstruct* s = &d->strukt;
        fprintf(o, "Struct { attrs: ");
        attrlist(o, s->attrs, s->nattrs);
        fputs(", derives: ", o);
        strlist(o, s->derives, s->nderives);
        fputs(", name: ", o);
        qstr(o, s->name);
        fputs(", type_params: ", o);
        show_ctypeparams(o, s->type_params, s->ntype_params);
        fputs(", fields: [", o);
        for (size_t i = 0; i < s->nfields; i++) {
            if (i) fputs(", ", o);
            show_field(o, &s->fields[i]);
        }
        fputs("] }", o);
        break;
    }
    case D_CLASS: {
        cclass* c = &d->klass;
        fprintf(o, "Class { attrs: ");
        attrlist(o, c->attrs, c->nattrs);
        fputs(", name: ", o);
        qstr(o, c->name);
        fputs(", type_params: ", o);
        show_ctypeparams(o, c->type_params, c->ntype_params);
        fputs(", items: [", o);
        for (size_t i = 0; i < c->nitems; i++) {
            if (i) fputs(", ", o);
            cclassitem* it = &c->items[i];
            switch (it->kind) {
            case CT_FIELD: show_field(o, it->f); break;
            case CT_METHOD: show_fn(o, it->m, "Method"); break;
            case CT_PROP: show_prop(o, it->p, "Prop"); break;
            }
        }
        fputs("] }", o);
        break;
    }
    case D_ENUM: {
        cenum* e = &d->en;
        fprintf(o, "Enum { attrs: ");
        attrlist(o, e->attrs, e->nattrs);
        fputs(", derives: ", o);
        strlist(o, e->derives, e->nderives);
        fputs(", name: ", o);
        qstr(o, e->name);
        fputs(", type_params: ", o);
        show_ctypeparams(o, e->type_params, e->ntype_params);
        fputs(", variants: [", o);
        for (size_t i = 0; i < e->nvariants; i++) {
            if (i) fputs(", ", o);
            cvariant* v = &e->variants[i];
            fprintf(o, "Variant { name: ");
            qstr(o, v->name);
            fputs(", kind: ", o);
            if (v->kind == VK_UNIT) fputs("Unit", o);
            else if (v->kind == VK_TUPLE) {
                fputs("Tuple(", o);
                for (size_t j = 0; j < v->ntys; j++) {
                    if (j) fputs(", ", o);
                    show_type(o, v->tys[j]);
                }
                fputc(')', o);
            } else {
                fputs("Struct([", o);
                for (size_t j = 0; j < v->nfields; j++) {
                    if (j) fputs(", ", o);
                    show_field(o, &v->fields[j]);
                }
                fputs("])", o);
            }
            fputc('}', o);
        }
        fputs("] }", o);
        break;
    }
    case D_TRAIT: {
        ctrait* t = &d->trait;
        fprintf(o, "Trait { attrs: ");
        attrlist(o, t->attrs, t->nattrs);
        fputs(", name: ", o);
        qstr(o, t->name);
        fputs(", type_params: ", o);
        show_ctypeparams(o, t->type_params, t->ntype_params);
        fputs(", supers: ", o);
        strlist(o, t->supers, t->nsupers);
        fputs(", items: [", o);
        for (size_t i = 0; i < t->nitems; i++) {
            if (i) fputs(", ", o);
            ctraititem* it = &t->items[i];
            switch (it->kind) {
            case TI_METHOD: show_fn(o, it->m, "Method"); break;
            case TI_PROPSIG: show_prop(o, it->p, "PropSig"); break;
            case TI_PROPIMPL: show_prop(o, it->p, "PropImpl"); break;
            }
        }
        fputs("] }", o);
        break;
    }
    case D_IMPL: {
        cimpl* im = &d->impl;
        fprintf(o, "Impl { type_params: ");
        show_ctypeparams(o, im->type_params, im->ntype_params);
        fputs(", trait_ty: ", o);
        show_type(o, im->trait_ty);
        fputs(", for_ty: ", o);
        show_type(o, im->for_ty);
        fputs(", items: [", o);
        for (size_t i = 0; i < im->nitems; i++) {
            if (i) fputs(", ", o);
            cimplitem* it = &im->items[i];
            if (it->kind == II_METHOD) show_fn(o, it->m, "Method");
            else show_prop(o, it->p, "Prop");
        }
        fputs("] }", o);
        break;
    }
    case D_FN: show_fn(o, &d->fn_, "Fn"); break;
    case D_CONST: {
        cconst* c = &d->konst;
        fprintf(o, "Const { name: ");
        qstr(o, c->name);
        fputs(", ty: ", o);
        show_type(o, c->ty);
        fputs(", expr: ", o);
        show_expr(o, c->expr);
        fputc('}', o);
        break;
    }
    case D_STATIC: {
        cstatic* s = &d->statik;
        fprintf(o, "Static { name: ");
        qstr(o, s->name);
        fprintf(o, ", was_var: %s, ty: ", s->was_var ? "true" : "false");
        show_type(o, s->ty);
        fputs(", expr: ", o);
        show_expr(o, s->expr);
        fputc('}', o);
        break;
    }
    case D_TEST: {
        ctest* t = &d->test;
        fprintf(o, "Test { name: ");
        qstr(o, t->name);
        fputs(", body: ", o);
        show_block(o, t->body);
        fputc('}', o);
        break;
    }
    }
}

void ctron_file_show(const cfile* f, FILE* out) {
    fputs("File { decls: [", out);
    for (size_t i = 0; i < f->ndecls; i++) {
        if (i) fputs(",\n  ", out);
        else fputs("\n  ", out);
        show_decl(out, &f->decls[i]);
    }
    fputs("\n] }\n", out);
}
