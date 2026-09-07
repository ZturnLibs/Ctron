#include "trans_internal.h"

// trans_core.c —— 字符串缓冲/类型模型/上下文与作用域(sb/ty/tc)
// ================= 字符串缓冲 =================
void sb_c(sb* b, char c) {
    if (b->n + 2 > b->cap) {
        b->cap = b->cap ? b->cap * 2 : 256;
        b->d = (char*)realloc(b->d, b->cap);
        if (!b->d) abort();
    }
    b->d[b->n++] = c;
    b->d[b->n] = '\0';
}
void sb_s(sb* b, const char* s) {
    if (s)
        while (*s) sb_c(b, *s++);
}
void sb_f(sb* b, const char* fmt, ...) {
    char tmp[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    sb_s(b, tmp);
}
void sb_free(sb* b) { free(b->d); }


// ================= 类型模型 =================
const char* head_name(const cty* t) {
    if (!t || t->kind != TY_NAMED || t->npath == 0) return NULL;
    return t->path[0];
}
ty ty_arr(ty elem) { ty t = {T_ARR, 0, 0, elem.k, elem.bits, elem.us, NULL, 0, 0, 0, NULL}; return t; }

ty suff_ty(const char* s) {
    int bits = 32, us = 0, isf = 0;
    if (s && *s) {
        if (!strcmp(s, "i8")) bits = 8;
        else if (!strcmp(s, "i16")) bits = 16;
        else if (!strcmp(s, "i32")) bits = 32;
        else if (!strcmp(s, "i64") || !strcmp(s, "isize")) bits = 64;
        else if (!strcmp(s, "u8")) { bits = 8; us = 1; }
        else if (!strcmp(s, "u16")) { bits = 16; us = 1; }
        else if (!strcmp(s, "u32")) { bits = 32; us = 1; }
        else if (!strcmp(s, "u64") || !strcmp(s, "usize")) { bits = 64; us = 1; }
        else if (!strcmp(s, "f32") || !strcmp(s, "f64")) isf = 1;
    }
    return isf ? ty_flt() : ty_int(bits, us);
}
ty e2_of(ty t) {
    ty e = ty_unk(); e.k = t.ek; e.bits = t.ebits; e.us = t.eus;
    if (t.ek == T_FLT) e = ty_flt();
    else if (t.ek == T_BOOL) e = ty_bool();
    else if (t.ek == T_STR) e = ty_str();
    else if (t.ek == T_STRUCT || t.ek == T_CLASS || t.ek == T_ENUM) e.tname = t.tname;
    return e;
}
const char* dt_for_wl(const char* wl) {
    if (!strcmp(wl, "str")) return "const char*";
    if (!strcmp(wl, "f64")) return "double";
    if (!strcmp(wl, "b")) return "int";
    int ub = wl[0] == 'u';
    int bits = atoi(wl + 1);
    ty t = ty_int(bits, ub);
    return ctype_of(t);
}
const char* ewlname(ty t) { if (t.ek == T_STR) return "str"; if (t.ek == T_FLT) return "f64"; if (t.ek == T_BOOL) return "b"; ty e = ty_int(t.ebits, t.eus); return wlname(e); }
const char* ctype_of(ty t) {
    if (t.k == T_FNPTR) return "ctron_fnptr";
    if (t.k == T_ARENA) return "void*"; // bare 档 arena 句柄:v1 无状态不回收
    if (t.k == T_SCOPE) return "ctron_scope*";
    if (t.k == T_TASK) { static char tb[96]; char mk[48];
        if (t.ek == T_FLT) snprintf(mk, sizeof mk, "F64");
        else if (t.ek == T_BOOL) snprintf(mk, sizeof mk, "Bool");
        else if (t.ek == T_STR) snprintf(mk, sizeof mk, "Str");
        else if (t.ek == T_STRUCT || t.ek == T_ENUM || t.ek == T_CLASS) snprintf(mk, sizeof mk, "%s", t.tname ? t.tname : "?");
        else snprintf(mk, sizeof mk, "%c%d", t.eus ? 'U' : 'I', t.ebits ? t.ebits : 32);
        snprintf(tb, sizeof tb, "ctron_task_%s*", mk); return tb; }
    if (t.k == T_CHAN || t.k == T_MUTEX) {
        static char cb[96]; char mk[48];
        if (t.ek == T_FLT) snprintf(mk, sizeof mk, "f64");
        else if (t.ek == T_BOOL) snprintf(mk, sizeof mk, "b");
        else if (t.ek == T_STR) snprintf(mk, sizeof mk, "str");
        else if (t.ek == T_STRUCT || t.ek == T_ENUM || t.ek == T_CLASS) snprintf(mk, sizeof mk, "%s", t.tname ? t.tname : "?");
        else snprintf(mk, sizeof mk, "%c%d", t.eus ? 'u' : 'i', t.ebits ? t.ebits : 32);
        snprintf(cb, sizeof cb, "%s_%s*", t.k == T_CHAN ? "ctron_chan" : "ctron_mutex", mk);
        return cb; }
    if (t.k == T_ERR) return "ctron_anyerr*";
    if (t.k == T_SUM) return t.tname ? t.tname : "void";
    if (t.k == T_TUP) return t.tname ? t.tname : "void"; // 元组:完整 typedef 名(tup_ty 生成)
    if (t.k == T_SIMD) { static char sd[64]; snprintf(sd, sizeof sd, "ctron_simd_f64_%d", (int)t.bits); return sd; }
    if (t.k == T_RANGE) return "ctron_rng";
    if (t.k == T_LIST) { static char cl[96]; snprintf(cl, sizeof cl, "ctron_list_%s", ewlname(t)); return cl; }
    if (t.k == T_CLASS) { static char cb1[96]; snprintf(cb1, sizeof cb1, "ctron_c_%s*", t.tname ? t.tname : "?"); return cb1; }
    if (t.k == T_BOX) { static char cb2[128]; ty e = box_elem(t); snprintf(cb2, sizeof cb2, "%s*", ctype_of(e)); return cb2; }
    if (t.k == T_STRUCT) { static char sb1[96]; snprintf(sb1, sizeof sb1, "ctron_t_%s", t.tname ? t.tname : "?"); return sb1; }
    if (t.k == T_ENUM) { static char sb2[96]; snprintf(sb2, sizeof sb2, "ctron_e_%s", t.tname ? t.tname : "?"); return sb2; }
    if (t.k == T_STR) return "const char*";
    if (t.k == T_ARR) {
        static char buf[48];
        snprintf(buf, sizeof buf, "ctron_arr_%s", ewlname(t));
        return buf;
    }
    if (t.k == T_FLT) return "double";
    if (t.k == T_BOOL) return "int";
    if (t.us) {
        switch (t.bits) {
        case 8: return "uint8_t";
        case 16: return "uint16_t";
        case 32: return "uint32_t";
        default: return "uint64_t";
        }
    }
    switch (t.bits) {
    case 8: return "int8_t";
    case 16: return "int16_t";
    case 32: return "int32_t";
    default: return "int64_t";
    }
}
const char* wlname(ty t) {
    if (t.k == T_STR) return "str";
    if (t.k == T_FLT) return "f64";
    if (t.k == T_BOOL) return "b";
    static char buf[8];
    snprintf(buf, sizeof buf, "%c%d", t.us ? 'u' : 'i', t.bits);
    return buf;
}

ty decl_ty_tc(tc* c, const cty* t) {
    if (!t) return ty_unk();
    ty b = decl_ty(t);
    if (b.k != T_UNK) return b;
    ty sm = sum_ty_of(c, t); // T? / Option[T] / Result[T,E](须在 TY_NAMED 守卫前)
    if (sm.k == T_SUM) return sm;
    if (t->kind == TY_FN) return ty_fnptr(); // fn(A) -> B:无原型函数指针(int64 统一 ABI)
    if (t->kind == TY_REF) return decl_ty_tc(c, t->sub); // 共享引用:表示不变
    if (t->kind == TY_TUPLE && t->nelems == 2) { // (A, B):subs 活跃时即单态化后的具体元组
        ty a = decl_ty_tc(c, t->elems[0]);
        ty b = decl_ty_tc(c, t->elems[1]);
        if (a.k == T_UNK || b.k == T_UNK) return ty_unk();
        return tup_ty(c, a, b);
    }
    if (t->kind == TY_NAMED && t->npath == 1 && !strcmp(t->path[0], "List") && t->nargs >= 1) {
        ty e2 = decl_ty_tc(c, t->args[0]);
        if (e2.k == T_UNK) return ty_unk();
        ty r = ty_unk(); r.k = T_LIST; r.ek = e2.k; r.ebits = e2.bits; r.eus = e2.us; r.tname = e2.tname;
        return r;
    }
    if (t->kind == TY_NAMED && t->npath == 1 && t->nargs == 0 && !strcmp(t->path[0], "Arena"))
        return ty_arena(); // bare 档 arena 句柄
    if (t->kind == TY_NAMED && t->npath == 1 && t->nargs >= 1
        && (!strcmp(t->path[0], "Atomic") || !strcmp(t->path[0], "Global") || !strcmp(t->path[0], "Mutex"))) {
        ty inner = decl_ty_tc(c, t->args[0]);
        if (inner.k == T_UNK) return ty_unk();
        rt_mutex_type(c, inner); // typedef 同步发射(collect 阶段无表达式点)
        return holder_ty(T_MUTEX, inner); // 共享单元格:字段/绑定按指针表示
    }
    if (t->kind == TY_SLICE) {
        ty e = decl_ty_tc(c, t->sub);
        return e.k == T_UNK ? ty_unk() : ty_arr(e);
    }
    if (t->kind == TY_ARRAY) {
        ty e = decl_ty_tc(c, t->elem);
        return e.k == T_UNK ? ty_unk() : ty_arr(e); // 维度不校验(对齐 rt)
    }
    if (!t || t->kind != TY_NAMED || t->npath != 1) return ty_unk();
    const char* n = t->path[0];
    // C10-h:单态化替换(内层优先:逆序查)—— 只在 TY_NAMED 单名守卫内、prelude(decl_ty)之后
    for (size_t si = c->n_subs; si > 0; si--)
        if (c->subs[si - 1].from && !strcmp(c->subs[si - 1].from, n)) return c->subs[si - 1].to;
    for (size_t i = 0; i < c->nstructs; i++)
        if (!strcmp(c->structs[i].name, n)) {
            ty r = ty_unk(); r.k = T_STRUCT; r.bits = (int)i; r.tname = c->structs[i].name; return r;
        }
    for (size_t i = 0; i < c->nenums; i++)
        if (!strcmp(c->enums[i].name, n)) {
            ty r = ty_unk(); r.k = T_ENUM; r.bits = (int)i; r.tname = c->enums[i].name; return r;
        }
    for (size_t i = 0; i < c->nclasses; i++)
        if (!strcmp(c->classes[i].name, n)) {
            ty r = ty_unk(); r.k = T_CLASS; r.bits = (int)i; r.tname = c->classes[i].name; return r;
        }
    // trait 名:&Trait 形参标记(未替换时)。只出现在需要按实参例化的位置。
    for (size_t ti = 0; ti < c->n_traits; ti++)
        if (!strcmp(c->traits[ti], n)) {
            ty r = ty_unk(); r.k = T_TRAIT; r.tname = c->traits[ti]; return r;
        }
    return ty_unk();
}
void terr(tc* c, const char* fmt, ...) {
    if (c->err) return;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    c->err = ctron_arena_strndup(c->a, buf, strlen(buf));
}
void use_arr(tc* c, const char* wl) {
    for (size_t i = 0; i < c->n_arrs; i++)
        if (!strcmp(c->arrs[i], wl)) return;
    if (c->n_arrs < 32) c->arrs[c->n_arrs++] = ctron_arena_strndup(c->a, wl, strlen(wl));
}
void use_helper(tc* c, const char* name) {
    for (size_t i = 0; i < c->n_helpers; i++)
        if (!strcmp(c->helpers[i], name)) return;
    if (c->n_helpers < 160) c->helpers[c->n_helpers++] = ctron_arena_strndup(c->a, name, strlen(name));
}
void add_fn(tc* c, const char* name, ty ret, int nparams, int is_void, const ty* ptys) {
    if (c->nfns >= MAX_FNS) return;
    c->fns[c->nfns].name = ctron_arena_strndup(c->a, name, strlen(name));
    c->fns[c->nfns].ret = ret;
    c->fns[c->nfns].nparams = nparams;
    c->fns[c->nfns].is_void = is_void;
    for (int i = 0; i < nparams && i < 8; i++) c->fns[c->nfns].pty[i] = ptys[i];
    c->nfns++;
}
void use_proto(tc* c, const char* proto) {
    for (size_t i = 0; i < c->n_protos; i++)
        if (!strcmp(c->protos[i], proto)) return;
    if (c->n_protos < 160) c->protos[c->n_protos++] = ctron_arena_strndup(c->a, proto, strlen(proto));
}
void use_sum(tc* c, const char* typedef_text) {
    for (size_t i = 0; i < c->n_sums; i++)
        if (!strcmp(c->sums[i], typedef_text)) return;
    if (c->n_sums < 64) c->sums[c->n_sums++] = ctron_arena_strndup(c->a, typedef_text, strlen(typedef_text));
}
int fn_lookup(tc* c, const char* name, int* nparams, int* is_void, ty* ret, ty* ptys) {
    for (size_t i = 0; i < c->nfns; i++)
        if (!strcmp(c->fns[i].name, name)) {
            *nparams = c->fns[i].nparams;
            *is_void = c->fns[i].is_void;
            *ret = c->fns[i].ret;
            if (ptys) for (int j = 0; j < *nparams && j < 8; j++) ptys[j] = c->fns[i].pty[j]; // j:勿遮蔽外层匹配下标
            return 1;
        }
    return 0;
}
void scope_push(tc* c) {
    if (c->n_scopes >= MAX_SCOPES) return;
    scope* s = &c->scopes[c->n_scopes++];
    memset(s, 0, sizeof *s);
    s->up = c->sc;
    c->sc = s;
}
void scope_pop(tc* c) { if (c->sc) { c->sc = c->sc->up; if (c->n_scopes > 0) c->n_scopes--; } }
int scope_find(tc* c, const char* name, ty* out) {
    for (scope* s = c->sc; s; s = s->up)
        for (size_t i = 0; i < s->n; i++)
            if (!strcmp(s->vars[i].name, name)) {
                *out = s->vars[i].t;
                return 1;
            }
    return 0;
}
void scope_def(tc* c, const char* name, ty t) {
    if (!c->sc || c->sc->n >= MAX_VARS) return;
    c->sc->vars[c->sc->n].name = ctron_arena_strndup(c->a, name, strlen(name));
    c->sc->vars[c->sc->n].t = t;
    c->sc->n++;
}
int is_reserved(const char* n) { return !strncmp(n, "ctron_", 6); }
int is_void_helper(int iv) { return iv; }

// ---- 元组(C10-p):二元组值语义,C 结构 tag-free ----
ty tup_ty(tc* c, ty a, ty b) {
    // v1:元素限标量域(struct 载荷元组的字段 tname 无处承载,诚实拒绝)
    if ((a.k != T_INT && a.k != T_FLT && a.k != T_BOOL && a.k != T_STR)
        || (b.k != T_INT && b.k != T_FLT && b.k != T_BOOL && b.k != T_STR))
        return ty_unk();
    char* m0 = ty_mangle(c, a);
    char* m1 = ty_mangle(c, b);
    char name[128];
    snprintf(name, sizeof name, "ctron_tup_%.40s_%.40s", m0 ? m0 : "X", m1 ? m1 : "X");
    char def[256];
    snprintf(def, sizeof def, "typedef struct { %s e0; %s e1; } %s;\n", ctype_of(a), ctype_of(b), name);
    use_sum(c, def);
    ty r = ty_unk();
    r.k = T_TUP;
    r.tname = ctron_arena_strndup(c->a, name, strlen(name));
    r.ek = a.k; r.ebits = a.bits; r.eus = a.us;
    r.ek2 = b.k; r.ebits2 = b.bits; r.eus2 = b.us;
    return r;
}
ty tup_elem(ty t, int ix) { // T_TUP 元素类型(标量域)
    ty e = ty_unk();
    int k = ix == 0 ? t.ek : t.ek2;
    int b2 = ix == 0 ? t.ebits : t.ebits2;
    int u2 = ix == 0 ? t.eus : t.eus2;
    e.k = k; e.bits = b2; e.us = u2;
    if (k == T_FLT) e = ty_flt();
    else if (k == T_BOOL) e = ty_bool();
    else if (k == T_STR) e = ty_str();
    return e;
}

// ---- 和类型(Option/Result)C10-d ----
char* ty_mangle(tc* c, ty t) {
    char buf[96];
    if (t.k == T_INT) snprintf(buf, sizeof buf, "%c%d", t.us ? 'U' : 'I', t.bits);
    else if (t.k == T_FLT) snprintf(buf, sizeof buf, "F64");
    else if (t.k == T_BOOL) snprintf(buf, sizeof buf, "Bool");
    else if (t.k == T_STR) snprintf(buf, sizeof buf, "Str");
    else if (t.k == T_STRUCT || t.k == T_ENUM || t.k == T_CLASS) snprintf(buf, sizeof buf, "%s", t.tname ? t.tname : "?");
    else if (t.k == T_ERR) snprintf(buf, sizeof buf, "Err");
    else if (t.k == T_TUP) snprintf(buf, sizeof buf, "%s", t.tname ? t.tname + (strncmp(t.tname, "ctron_tup_", 10) == 0 ? 10 : 0) : "Tup");
    else snprintf(buf, sizeof buf, "X");
    return ctron_arena_strndup(c->a, buf, strlen(buf));
}
