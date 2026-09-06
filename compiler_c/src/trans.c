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

// ================= 字符串缓冲 =================
typedef struct {
    char* d;
    size_t n, cap;
} sb;
static void sb_c(sb* b, char c) {
    if (b->n + 2 > b->cap) {
        b->cap = b->cap ? b->cap * 2 : 256;
        b->d = (char*)realloc(b->d, b->cap);
        if (!b->d) abort();
    }
    b->d[b->n++] = c;
    b->d[b->n] = '\0';
}
static void sb_s(sb* b, const char* s) {
    if (s)
        while (*s) sb_c(b, *s++);
}
static void sb_f(sb* b, const char* fmt, ...) {
    char tmp[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    sb_s(b, tmp);
}
static void sb_free(sb* b) { free(b->d); }

// ================= 类型模型 =================
#define MAX_FIELDS 32
#define MAX_VARIANTS 64
#define MAX_TYPES 64
// ================= 类型模型 =================
#define MAX_FIELDS 32
#define MAX_VARIANTS 64
#define MAX_TYPES 64
typedef struct tc tc;
typedef struct { int k; int bits, us; int ek, ebits, eus; const char* tname; int ek2, ebits2, eus2; const char* tname2; } ty;
enum { T_UNK, T_INT, T_FLT, T_BOOL, T_STR, T_ARR, T_STRUCT, T_ENUM, T_SUM, T_CLASS, T_BOX, T_RANGE, T_LIST, T_TRAIT, T_ERR }; // T_TRAIT:&Trait 形参标记;T_ERR:AnyError 链节点指针(ctron_anyerr*)(C10-i)
typedef struct sfield sfield;
typedef struct sdef sdef;
typedef struct evar evar;
typedef struct edef edef;
typedef struct cdef cdef;
struct sfield { char* name; ty t; };
struct sdef { char* name; sfield fields[MAX_FIELDS]; size_t n; };
struct evar { char* name; ty pty; int has_p; };
struct edef { char* name; evar variants[MAX_VARIANTS]; size_t n; };
struct cdef { char* name; sfield fields[MAX_FIELDS]; size_t n; };
static const char* ctype_of(ty t);
static const char* head_name(const cty* t);
static ty ty_int(int bits, int us) { ty t = {T_INT, bits, us, 0, 0, 0, NULL, 0, 0, 0, NULL}; t.tname = NULL; return t; }
static ty ty_flt(void) { ty t = {T_FLT, 64, 0, 0, 0, 0, NULL, 0, 0, 0, NULL}; t.tname = NULL; return t; }
static ty ty_bool(void) { ty t = {T_BOOL, 1, 0, 0, 0, 0, NULL, 0, 0, 0, NULL}; t.tname = NULL; return t; }
static ty ty_unk(void) { ty t = {T_UNK, 0, 0, 0, 0, 0, NULL, 0, 0, 0, NULL}; t.tname = NULL; return t; }
static const char* head_name(const cty* t) {
    if (!t || t->kind != TY_NAMED || t->npath == 0) return NULL;
    return t->path[0];
}
static ty ty_str(void) { ty t = {T_STR, 0, 0, 0, 0, 0, NULL, 0, 0, 0, NULL}; t.tname = NULL; return t; }
static ty ty_arr(ty elem) { ty t = {T_ARR, 0, 0, elem.k, elem.bits, elem.us, NULL, 0, 0, 0, NULL}; return t; }

static ty suff_ty(const char* s) {
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
static ty decl_ty_tc(tc* c, const cty* t);
static ty decl_ty(const cty* t);
static ty sum_ty_of(tc* c, const cty* t);
static ty e2_of(ty t) {
    ty e = ty_unk(); e.k = t.ek; e.bits = t.ebits; e.us = t.eus;
    if (t.ek == T_FLT) e = ty_flt();
    else if (t.ek == T_BOOL) e = ty_bool();
    else if (t.ek == T_STR) e = ty_str();
    else if (t.ek == T_STRUCT || t.ek == T_CLASS || t.ek == T_ENUM) e.tname = t.tname;
    return e;
}
static const char* dt_for_wl(const char* wl) {
    if (!strcmp(wl, "str")) return "const char*";
    if (!strcmp(wl, "f64")) return "double";
    if (!strcmp(wl, "b")) return "int";
    int ub = wl[0] == 'u';
    int bits = atoi(wl + 1);
    ty t = ty_int(bits, ub);
    return ctype_of(t);
}
static const char* ctype_of(ty t);
static const char* wlname(ty t);
static ty box_elem(ty t);
static const char* ewlname(ty t) { if (t.ek == T_STR) return "str"; if (t.ek == T_FLT) return "f64"; if (t.ek == T_BOOL) return "b"; ty e = ty_int(t.ebits, t.eus); return wlname(e); }
static const char* ctype_of(ty t) {
    if (t.k == T_ERR) return "ctron_anyerr*";
    if (t.k == T_SUM) return t.tname ? t.tname : "void";
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
static const char* wlname(ty t) {
    if (t.k == T_STR) return "str";
    if (t.k == T_FLT) return "f64";
    if (t.k == T_BOOL) return "b";
    static char buf[8];
    snprintf(buf, sizeof buf, "%c%d", t.us ? 'u' : 'i', t.bits);
    return buf;
}

// ================= 上下文 =================
#define MAX_FNS 256
#define MAX_VARS 128
#define MAX_SCOPES 64
typedef struct scope {
    struct { char* name; ty t; } vars[MAX_VARS];
    size_t n;
    struct scope* up;
} scope;

#define MAX_TYPES 64
#define MAX_FIELDS 32
#define MAX_VARIANTS 64

struct tc {
    sb head, body;
    sdef structs[MAX_TYPES];
    size_t nstructs;
    edef enums[MAX_TYPES];
    size_t nenums;
    cdef classes[MAX_TYPES];
    size_t nclasses;
    struct { char* type; char* trait; char* name; const cfn* f; const cprop* p; } methods[128];
    size_t n_methods;
    struct { char* type; char* trait; } impls[64];
    size_t n_impls;
    struct { char* trait; char* name; const cfn* f; const cprop* p; } defaults[64];
    size_t n_defaults;
    char* helpers[160];
    size_t n_helpers;
    char* arrs[32]; // 已用数组元素宽度(ctron_arr_<wl> typedef)
    size_t n_arrs;
    char* sums[64]; // 已用和类型 typedef 全文(ctron_opt_/ctron_res_/ctron_e_)
    size_t n_sums;
    char* traits[64];     // 已声明的 trait 名(&Trait 形参 → T_TRAIT 标记;C10-h)
    size_t n_traits;
    struct { const char* from; ty to; } subs[8]; // 单态化替换:trait 名 → 具体类型
    size_t n_subs;
    char* monos[64];      // 已例化的单态化 cname(去重)
    size_t n_monos;
    char* monoprotos[64]; // 单态化函数原型(进头部,含 ';')
    size_t n_monoprotos;
    sb s_sb;              // 单态化函数体(头部/m_sb 之后、body 之前)
    sb* out_sb;           // 当前函数体输出缓冲(默认 &body;单态化走临时缓冲)
    const cfile* srcf;    // 源文件(单态化按名找 decl)
    sb m_sb;              // 方法体/prop 访问器(发射到 fns 之后、main 之前)
    char* protos[160];    // 方法/prop/expect 原型
    size_t n_protos;
    char* cells[32];      // 已用 Atomic 元素宽度
    size_t n_cells;
    struct { char* name; ty t; char* init; } globals[32];
    size_t n_globals;
    struct { char* name; ty ret; int nparams; int is_void; ty pty[8]; } fns[MAX_FNS];
    size_t nfns;
    const ty* fn_ret; // 当前函数返回类型提示(?)
    const ty* want;   // 期望类型提示(None/Some/Ok/Err 构造推导)
    int in_main;      // 当前正在发射 fn main(? 早退 = exit 0,对齐 rt run_main)
    scope* sc;
    scope scopes[MAX_SCOPES];
    size_t n_scopes;
    char* err;
    ctron_arena* a;
    int tmpn;
    int in_test;
};

static ty decl_ty_tc(tc* c, const cty* t) {
    if (!t) return ty_unk();
    ty b = decl_ty(t);
    if (b.k != T_UNK) return b;
    ty sm = sum_ty_of(c, t); // T? / Option[T] / Result[T,E](须在 TY_NAMED 守卫前)
    if (sm.k == T_SUM) return sm;
    if (t->kind == TY_REF) return decl_ty_tc(c, t->sub); // 共享引用:表示不变
    if (t->kind == TY_NAMED && t->npath == 1 && !strcmp(t->path[0], "List") && t->nargs >= 1) {
        ty e2 = decl_ty_tc(c, t->args[0]);
        if (e2.k == T_UNK) return ty_unk();
        ty r = ty_unk(); r.k = T_LIST; r.ek = e2.k; r.ebits = e2.bits; r.eus = e2.us; r.tname = e2.tname;
        return r;
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
static void terr(tc* c, const char* fmt, ...) {
    if (c->err) return;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    c->err = ctron_arena_strndup(c->a, buf, strlen(buf));
}
static void use_arr(tc* c, const char* wl) {
    for (size_t i = 0; i < c->n_arrs; i++)
        if (!strcmp(c->arrs[i], wl)) return;
    if (c->n_arrs < 32) c->arrs[c->n_arrs++] = ctron_arena_strndup(c->a, wl, strlen(wl));
}
static void use_helper(tc* c, const char* name) {
    for (size_t i = 0; i < c->n_helpers; i++)
        if (!strcmp(c->helpers[i], name)) return;
    if (c->n_helpers < 160) c->helpers[c->n_helpers++] = ctron_arena_strndup(c->a, name, strlen(name));
}
static void add_fn(tc* c, const char* name, ty ret, int nparams, int is_void, const ty* ptys) {
    if (c->nfns >= MAX_FNS) return;
    c->fns[c->nfns].name = ctron_arena_strndup(c->a, name, strlen(name));
    c->fns[c->nfns].ret = ret;
    c->fns[c->nfns].nparams = nparams;
    c->fns[c->nfns].is_void = is_void;
    for (int i = 0; i < nparams && i < 8; i++) c->fns[c->nfns].pty[i] = ptys[i];
    c->nfns++;
}
static void use_proto(tc* c, const char* proto) {
    for (size_t i = 0; i < c->n_protos; i++)
        if (!strcmp(c->protos[i], proto)) return;
    if (c->n_protos < 160) c->protos[c->n_protos++] = ctron_arena_strndup(c->a, proto, strlen(proto));
}
static void use_sum(tc* c, const char* typedef_text) {
    for (size_t i = 0; i < c->n_sums; i++)
        if (!strcmp(c->sums[i], typedef_text)) return;
    if (c->n_sums < 64) c->sums[c->n_sums++] = ctron_arena_strndup(c->a, typedef_text, strlen(typedef_text));
}
static int fn_lookup(tc* c, const char* name, int* nparams, int* is_void, ty* ret, ty* ptys) {
    for (size_t i = 0; i < c->nfns; i++)
        if (!strcmp(c->fns[i].name, name)) {
            *nparams = c->fns[i].nparams;
            *is_void = c->fns[i].is_void;
            *ret = c->fns[i].ret;
            if (ptys) for (int i = 0; i < *nparams && i < 8; i++) ptys[i] = c->fns[i].pty[i];
            return 1;
        }
    return 0;
}
static void scope_push(tc* c) {
    if (c->n_scopes >= MAX_SCOPES) return;
    scope* s = &c->scopes[c->n_scopes++];
    memset(s, 0, sizeof *s);
    s->up = c->sc;
    c->sc = s;
}
static void scope_pop(tc* c) { if (c->sc) { c->sc = c->sc->up; if (c->n_scopes > 0) c->n_scopes--; } }
static int scope_find(tc* c, const char* name, ty* out) {
    for (scope* s = c->sc; s; s = s->up)
        for (size_t i = 0; i < s->n; i++)
            if (!strcmp(s->vars[i].name, name)) {
                *out = s->vars[i].t;
                return 1;
            }
    return 0;
}
static void scope_def(tc* c, const char* name, ty t) {
    if (!c->sc || c->sc->n >= MAX_VARS) return;
    c->sc->vars[c->sc->n].name = ctron_arena_strndup(c->a, name, strlen(name));
    c->sc->vars[c->sc->n].t = t;
    c->sc->n++;
}
static int is_reserved(const char* n) { return !strncmp(n, "ctron_", 6); }
static int is_void_helper(int iv) { return iv; }

// ---- 和类型(Option/Result)C10-d ----
static char* ty_mangle(tc* c, ty t) {
    char buf[96];
    if (t.k == T_INT) snprintf(buf, sizeof buf, "%c%d", t.us ? 'U' : 'I', t.bits);
    else if (t.k == T_FLT) snprintf(buf, sizeof buf, "F64");
    else if (t.k == T_BOOL) snprintf(buf, sizeof buf, "Bool");
    else if (t.k == T_STR) snprintf(buf, sizeof buf, "Str");
    else if (t.k == T_STRUCT || t.k == T_ENUM || t.k == T_CLASS) snprintf(buf, sizeof buf, "%s", t.tname ? t.tname : "?");
    else if (t.k == T_ERR) snprintf(buf, sizeof buf, "Err");
    else snprintf(buf, sizeof buf, "X");
    return ctron_arena_strndup(c->a, buf, strlen(buf));
}
// ================= 表达式(单次求值发射) =================
static ty emit_expr(tc* c, cexpr* e, sb* o);
static void emit_block(tc* c, cblock* b, sb* o);
static void emit_stmt(tc* c, cstmt* st, sb* o);
static void emit_value_to(tc* c, cexpr* ax, const char* rn, sb* o);
static void emit_if_assign(tc* c, cexpr* e, const char* rn, sb* o);
static int ensure_prop_accessor(tc* c, const char* type, const char* name, char* out_cname, ty* out_ret);
static const cfn* find_default_fn_for(tc* c, const char* type, const char* name, char** out_trait);
static int type_has_drop_m(tc* c, const char* type);
static int ensure_method_fn(tc* c, const char* type, const char* name, char* out_cname, ty* out_ret);
// C10-h:trait 参数单态化
static int fn_has_trait_param(tc* c, const cfn* F);
static const cfn* fn_named(const cfile* f, const char* nm);
static void ensure_mono_fn(tc* c, const cfn* F, const char* cname, const ty* argtys, const ty* atys, int nparams);
static void emit_fn(tc* c, const cfn* F, const char* cname);

static ty emit_expr(tc* c, cexpr* e, sb* o) {
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
        if (!e->m_is_name || !e->mname) { terr(c, "v1:成员访问不支持"); return ty_unk(); }
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
        if (e->bop == B_OR) { terr(c, "v1:|| 不存在(用 or2 德摩根)"); return ty_unk(); }
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
        for (size_t i = 0; i < e->nelems; i++) {
            if (i) sb_s(&elems, ", ");
            sb a1 = {0};
            ty it = emit_expr(c, e->elems[i], &a1);
            if (i == 0) et = it;
            else if (it.k != et.k || it.bits != et.bits || it.us != et.us)
                terr(c, "v1:数组字面量元素类型不一致");
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
            for (size_t i = 0; i < c->nclasses; i++)
                if (!strcmp(c->classes[i].name, tn)) { cds = (sdef*)&c->classes[i]; break; }
            for (size_t j = 0; cds && j < cds->n; j++) {
                if (j) sb_s(o, ", ");
                cexpr* fv = NULL;
                for (size_t i = 0; i < e->nfields; i++)
                    if (e->fields[i].name && !strcmp(e->fields[i].name, cds->fields[j].name)) fv = e->fields[i].value;
                if (fv) emit_expr(c, fv, o);
                else sb_s(o, "0");
            }
            sb_s(o, ")");
            ty r = ty_unk(); r.k = T_CLASS; r.tname = tn;
            return r;
        }
        sdef* sd = NULL;
        for (size_t i = 0; i < c->nstructs; i++)
            if (!strcmp(c->structs[i].name, tn)) { sd = &c->structs[i]; break; }
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
        // Option/Result 方法:or / expect(C10-d)
        if (cal && cal->kind == EX_MEMBER && cal->m_is_name && cal->mname) {
            sb rob2 = {0};
            ty rt4 = emit_expr(c, cal->obj, &rob2);
            if (c->err) { sb_free(&rob2); return ty_unk(); }
            if (rt4.k == T_SUM && (!strcmp(cal->mname, "or") || !strcmp(cal->mname, "expect")
                                  || !strcmp(cal->mname, "context")
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
    default:
        terr(c, "v1 不支持该表达式构造(kind %d|%s)", (int)e->kind,
             e->kind == EX_VOID ? "void字面量" : e->kind == EX_TUPLE ? "元组" : e->kind == EX_TYPEARGS ? "类型实参" : "其他");
        return ty_unk();
    }
}

// ================= 方法分发(C10-g②)=================
static const cfn* find_impl_method(tc* c, const char* type, const char* name) {
    for (size_t i = 0; i < c->n_methods; i++)
        if (!strcmp(c->methods[i].type, type) && !strcmp(c->methods[i].name, name) && c->methods[i].f)
            return c->methods[i].f;
    return NULL;
}
static const cprop* find_impl_prop(tc* c, const char* type, const char* name) {
    for (size_t i = 0; i < c->n_methods; i++)
        if (!strcmp(c->methods[i].type, type) && !strcmp(c->methods[i].name, name) && c->methods[i].p)
            return c->methods[i].p;
    return NULL;
}
static int type_impls_trait(tc* c, const char* type, const char* trait) {
    for (size_t i = 0; i < c->n_impls; i++)
        if (!strcmp(c->impls[i].type, type) && !strcmp(c->impls[i].trait, trait)) return 1;
    return 0;
}
// trait 默认方法:返回 (trait, 方法);类型实现了该 trait 即可用
static const cfn* find_default_fn_for(tc* c, const char* type, const char* name, char** out_trait) {
    for (size_t i = 0; i < c->n_defaults; i++)
        if (!strcmp(c->defaults[i].name, name) && type_impls_trait(c, type, c->defaults[i].trait)) {
            if (out_trait) *out_trait = c->defaults[i].trait;
            return c->defaults[i].f;
        }
    return NULL;
}
static const cprop* find_default_prop_for(tc* c, const char* type, const char* name, char** out_trait) {
    for (size_t i = 0; i < c->n_defaults; i++)
        if (!strcmp(c->defaults[i].name, name) && c->defaults[i].p && type_impls_trait(c, type, c->defaults[i].trait)) {
            if (out_trait) *out_trait = c->defaults[i].trait;
            return c->defaults[i].p;
        }
    return NULL;
}
static int find_class(tc* c, const char* n) {
    for (size_t i = 0; i < c->nclasses; i++)
        if (!strcmp(c->classes[i].name, n)) return 1;
    return 0;
}
static int type_has_drop_m(tc* c, const char* type) {
    for (size_t i = 0; i < c->n_methods; i++)
        if (!strcmp(c->methods[i].type, type) && !strcmp(c->methods[i].name, "drop")) return 1;
    return 0;
}

// 方法体发射:receiver self + 参数(self 为 class→指针;struct→值)
// 方法体发射:receiver self(class→指针;struct→值副本,对齐 rt call_method_body)
static void emit_method_fn(tc* c, const char* type, const cfn* F, const char* cname) {
    int is_class = find_class(c, type) != NULL;
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
static void emit_prop_accessor(tc* c, const char* type, const cprop* P, const char* cname) {
    int is_class = find_class(c, type) != NULL;
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
static int ensure_method_fn(tc* c, const char* type, const char* name, char* out_cname, ty* out_ret) {
    char cn[192];
    snprintf(cn, sizeof cn, "ctron_m_%s_%s", type, name);
    const cfn* mf = find_impl_method(c, type, name);
    const cfn* df = mf ? NULL : find_default_fn_for(c, type, name, NULL);
    const cfn* use = mf ? mf : df;
    if (!use) return 0;
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
static int ensure_prop_accessor(tc* c, const char* type, const char* name, char* out_cname, ty* out_ret) {
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
static int fn_has_trait_param(tc* c, const cfn* F) {
    if (!F) return 0;
    for (size_t j = 0; j < F->nparams; j++) {
        const cparam* p = &F->params[j];
        if (p->is_receiver || !p->ty) continue;
        if (decl_ty_tc(c, p->ty).k == T_TRAIT) return 1;
    }
    return 0;
}
static const cfn* fn_named(const cfile* f, const char* nm) {
    if (!f) return NULL;
    for (size_t i = 0; i < f->ndecls; i++) {
        const cdecl* d = &f->decls[i];
        if (d->kind == D_FN && d->fn_.name && !strcmp(d->fn_.name, nm)) return &d->fn_;
    }
    return NULL;
}
// argtys:模板形参(fn_lookup,含 T_TRAIT);atys:调用点实参实际类型。
static void ensure_mono_fn(tc* c, const cfn* F, const char* cname,
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
                       || ret.k == T_CLASS || ret.k == T_BOX) ? ctype_of(ret) : "void";
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

// ================= if 作为值(C10-f)=================
static void emit_if_assign(tc* c, cexpr* e, const char* rn, sb* o);
static void emit_if_assign(tc* c, cexpr* e, const char* rn, sb* o) {
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
static void emit_stmt(tc* c, cstmt* st, sb* o);
// 支持:字面量(int/bool/str)/通配/标识符绑定/单元变体/struct 模式(字段绑定+字面量);
// 位置:return / let / 语句。变体载荷(tuple variant)与 Option/Result → C10-d。
static void emit_value_to(tc* c, cexpr* ax, const char* rn, sb* o) {
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
    sb t2 = {0};
    emit_expr(c, ax, &t2);
    sb_f(o, "%s = %s;\n", rn, t2.d ? t2.d : "0");
    sb_free(&t2);
}

static ty emit_match(tc* c, cexpr* e, sb* o, int want_value, char** out_tmp) {
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
            sb_f(o, "%s %s = 0;\n", ctype_of(val_t), rn); // 结果变量在 match 块外(return/let 可见)
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
static void emit_if_stmt(tc* c, cexpr* e, sb* o) {
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

static void emit_stmt(tc* c, cstmt* st, sb* o) {
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
            if (!st->pat || st->pat->kind != PAT_IDENT || !st->pat->name) { terr(c, "v1:for 模式仅标识符"); return; }
            const char* var = st->pat->name;
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
            if (!st->pat || st->pat->kind != PAT_IDENT || !st->pat->name) { terr(c, "v1:for 模式仅标识符"); return; }
            const char* var = st->pat->name;
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
        if (!st->pat || st->pat->kind != PAT_IDENT || !st->pat->name) { terr(c, "v1:for 模式仅标识符"); return; }
        const char* var = st->pat->name;
        if (is_reserved(var)) { terr(c, "v1:标识符保留前缀 ctron_:%s", var); return; }
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

static void emit_block(tc* c, cblock* b, sb* o) {
    if (!b || c->err) return;
    for (size_t i = 0; i < b->nstmts; i++)
        emit_stmt(c, b->stmts[i], o);
    // Drop RAII(§6.4):作用域退出,按声明逆序调用 drop
    if (c->sc) {
        for (int di = (int)c->sc->n - 1; di >= 0; di--) {
            const char* vn = c->sc->vars[di].name;
            ty vt = c->sc->vars[di].t;
            if (vt.k == T_STRUCT && vt.tname && type_has_drop_m(c, vt.tname)) {
                char hn[192];
                snprintf(hn, sizeof hn, "ctron_m_%s_drop", vt.tname);
                use_proto(c, hn);
                sb_f(o, "ctron_m_%s_drop(%s);\n", vt.tname, vn);
            }
        }
    }
    if (b->tail) {
        if (b->tail->kind == EX_MATCH) { emit_match(c, b->tail, o, 0, NULL); return; }
        if (b->tail->kind == EX_IF) { emit_if_stmt(c, b->tail, o); return; }
        if (b->tail->kind == EX_OWN) {
            scope_push(c);
            emit_block(c, b->tail->obody, o);
            scope_pop(c);
            return;
        }
        sb r = {0};
        emit_expr(c, b->tail, &r);
        sb_f(o, "%s;\n", r.d ? r.d : "");
        sb_free(&r);
    }
}

// ================= 助手生成 =================
static void wbounds(char* lo, char* hi, size_t n, int bits, int us) {
    if (us) {
        snprintf(lo, n, "0");
        snprintf(hi, n, "%lluULL", bits >= 64 ? 18446744073709551615ULL : (1ULL << bits) - 1);
    } else if (bits >= 64) {
        snprintf(lo, n, "(-9223372036854775807LL - 1)");
        snprintf(hi, n, "9223372036854775807LL");
    } else {
        snprintf(lo, n, "%lldLL", -(1LL << (bits - 1)));
        snprintf(hi, n, "%lldLL", (1LL << (bits - 1)) - 1);
    }
}
static void emit_helper(tc* c, const char* name) {
    // name 形如 ctron_<fam>_<wl>;fam ∈ add/sub/mul/div/mod/neg/wadd/wsub/decl/print/assert_eq/assert_ne
    char fam[32], wl[16];
    if (!strcmp(name, "ctron_str_len")) { snprintf(fam, sizeof fam, "str_len"); wl[0] = 0; }
    else if (!strcmp(name, "ctron_str_char_len")) { snprintf(fam, sizeof fam, "str_char_len"); wl[0] = 0; }
    else if (!strcmp(name, "ctron_str_contains")) { snprintf(fam, sizeof fam, "str_contains"); wl[0] = 0; }
    else if (!strcmp(name, "ctron_str_cmp")) { snprintf(fam, sizeof fam, "str_cmp"); wl[0] = 0; }
    else if (!strcmp(name, "ctron_str_concat")) { snprintf(fam, sizeof fam, "str_concat"); wl[0] = 0; }
    else if (!strcmp(name, "ctron_str_slice")) { snprintf(fam, sizeof fam, "str_slice"); wl[0] = 0; }
    else if (!strcmp(name, "ctron_byte_at")) { snprintf(fam, sizeof fam, "byte_at"); wl[0] = 0; }
    else if (!strcmp(name, "ctron_byte_slice")) { snprintf(fam, sizeof fam, "byte_slice"); wl[0] = 0; }
    else if (!strcmp(name, "ctron_idx")) { snprintf(fam, sizeof fam, "idx"); wl[0] = 0; }
    else if (!strncmp(name, "ctron_assert_eq_", 16)) {
        snprintf(fam, sizeof fam, "assert_eq");
        snprintf(wl, sizeof wl, "%s", name + 16);
    } else if (!strncmp(name, "ctron_assert_ne_", 16)) {
        snprintf(fam, sizeof fam, "assert_ne");
        snprintf(wl, sizeof wl, "%s", name + 16);
    } else if (sscanf(name, "ctron_%31[^_]_%15s", fam, wl) != 2) {
        return;
    }
    int bits = 32, us = 0, isf = 0, isb = 0;
    int nowl = wl[0] == 0;
    if (!nowl && !strcmp(wl, "f64")) isf = 1;
    else if (!nowl && !strcmp(wl, "b")) isb = 1;
    else if (!nowl) { us = wl[0] == 'u'; bits = atoi(wl + 1); }
    char ct[32], lo[48], hi[48];
    if (!isf && !isb && !nowl && bits > 0) {
        ty t = ty_int(bits, us);
        snprintf(ct, sizeof ct, "%s", ctype_of(t));
        wbounds(lo, hi, sizeof lo, bits, us);
    }
    sb* o = &c->head;
    if (!strcmp(fam, "add") || !strcmp(fam, "sub") || !strcmp(fam, "mul")
        || !strcmp(fam, "div") || !strcmp(fam, "mod") || !strcmp(fam, "cadd")
        || !strcmp(fam, "csub") || !strcmp(fam, "cmul") || !strcmp(fam, "cdiv")
        || !strcmp(fam, "cmod") || !strcmp(fam, "iadd") || !strcmp(fam, "isub")
        || !strcmp(fam, "imul") || !strcmp(fam, "idiv") || !strcmp(fam, "imod")
        || !strcmp(fam, "madd") || !strcmp(fam, "msub") || !strcmp(fam, "mmul")
        || !strcmp(fam, "mdiv") || !strcmp(fam, "mmod")) {
        int is_c = 0;
        size_t fln = strlen(fam);
        // 复合族是 4 字符(cadd/iadd/madd…);普通二元 add/sub/mul/div/mod 是 3 字符,
        // 不能按首字母 'm' 判别(否则 mul/mod 被误当成员赋值族,base 掉进兜底 '%')。
        if (fln == 4 && fam[0] == 'c') is_c = 1;      // cadd… → "(assign)"
        else if (fln == 4 && fam[0] == 'i') is_c = 2; // iadd… → "(idx assign)"
        else if (fln == 4 && fam[0] == 'm') is_c = 3; // madd… → "(member assign)"
        const char* base = is_c ? fam + 1 : fam;
        const char* op = !strcmp(base, "add") ? "+" : !strcmp(base, "sub") ? "-" : !strcmp(base, "mul") ? "*"
                        : !strcmp(base, "div") ? "/" : "%";
        const char* zmsg = !strcmp(base, "div") ? "division by zero (/)" : "division by zero (%)";
        char omsg[48];
        if (is_c == 1) snprintf(omsg, sizeof omsg, "integer overflow (assign)");
        else if (is_c == 2) snprintf(omsg, sizeof omsg, "integer overflow (idx assign)");
        else if (is_c == 3) snprintf(omsg, sizeof omsg, "integer overflow (member assign)");
        else snprintf(omsg, sizeof omsg, "integer overflow (%s)", op);
        sb_f(o, "static %s %s(int64_t a, int64_t b) {\n", ct, name);
        if (!strcmp(base, "div") || !strcmp(base, "mod"))
            sb_f(o, "    if (b == 0) ctron_panic(\"%s\");\n", zmsg);
        sb_f(o, "    __int128 r = (__int128)a %s (__int128)b;\n", op);
        sb_f(o, "    if (r < (%s)(%s) || r > (%s)(%s)) ctron_panic(\"%s\");\n",
             ct, lo, ct, hi, omsg);
        sb_f(o, "    return (%s)r;\n}\n", ct);
        return;
    }
    if (!strcmp(fam, "neg")) {
        if (us) {
            sb_f(o, "static %s %s(int64_t a) { (void)a; ctron_panic(\"integer overflow (neg)\"); return (%s)0; }\n",
                 ct, name, ct);
        } else {
            sb_f(o, "static %s %s(int64_t a) {\n    __int128 r = -(__int128)a;\n", ct, name);
            sb_f(o, "    if (r < (%s)(%s) || r > (%s)(%s)) ctron_panic(\"integer overflow (neg)\");\n", ct, lo, ct, hi);
            sb_f(o, "    return (%s)r;\n}\n", ct);
        }
        return;
    }
    if (!strcmp(fam, "wadd") || !strcmp(fam, "wsub")) {
        const char* op = !strcmp(fam, "wadd") ? "+" : "-";
        sb_f(o, "static %s %s(int64_t a, int64_t b) {\n", ct, name);
        if (bits >= 64)
            sb_f(o, "    return (%s)(int64_t)((uint64_t)a %s (uint64_t)b);\n", ct, op);
        else {
            unsigned long long m = (1ULL << bits) - 1;
            sb_f(o, "    uint64_t r = ((uint64_t)a %s (uint64_t)b) & %lluULL;\n", op, m);
            if (us) sb_f(o, "    return (%s)r;\n", ct);
            else sb_f(o, "    return (%s)((int64_t)(r ^ %lluULL) - (int64_t)%lluULL);\n",
                      ct, (1ULL << (bits - 1)), (1ULL << (bits - 1)));
        }
        sb_f(o, "}\n");
        return;
    }
    if (!strcmp(fam, "str_len")) { sb_f(o, "static int64_t %s(const char* s) { return (%s)(s ? strlen(s) : 0); }\n", name, "int64_t"); return; }
    if (!strcmp(fam, "str_char_len")) {
        sb_f(o, "static int64_t %s(const char* s) { int64_t n = 0; const unsigned char* p = (const unsigned char*)(s ? s : \"\"); while (*p) { if ((*p & 0xC0) != 0x80) n++; p++; } return n; }\n", name);
        return;
    }
    if (!strcmp(fam, "str_contains")) {
        sb_f(o, "static int %s(const char* s, const char* x) { return strstr(s ? s : \"\", x ? x : \"\") != 0; }\n", name);
        return;
    }
    if (!strcmp(fam, "str_cmp")) {
        sb_f(o, "static int %s(const char* a, const char* b) { return strcmp(a ? a : \"\", b ? b : \"\"); }\n", name);
        return;
    }
    if (!strcmp(fam, "str_concat")) {
        sb_f(o, "static const char* %s(const char* a, const char* b) {\n"
              "    size_t n1 = a ? strlen(a) : 0, n2 = b ? strlen(b) : 0;\n"
              "    char* r = (char*)malloc(n1 + n2 + 1);\n"
              "    if (n1) memcpy(r, a, n1);\n"
              "    if (n2) memcpy(r + n1, b, n2);\n"
              "    r[n1 + n2] = 0;\n"
              "    return r;\n}\n", name);
        return;
    }
    if (!strcmp(fam, "str_slice")) {
        sb_f(o, "static const char* %s(const char* s, int64_t lo, int64_t hi, int incl) {\n"
              "    size_t len = s ? strlen(s) : 0;\n"
              "    int64_t l = lo < 0 ? 0 : lo;\n"
              "    int64_t h = incl ? hi + 1 : hi;\n"
              "    if (h > (int64_t)len) h = len;\n"
              "    if (l < (int64_t)len && ((unsigned char)s[l] & 0xC0) == 0x80) ctron_panic(\"invalid utf8 boundary (utf8)\");\n"
              "    if (h < (int64_t)len && ((unsigned char)s[h] & 0xC0) == 0x80) ctron_panic(\"invalid utf8 boundary (utf8)\");\n"
              "    if (l < 0 || h < l) ctron_panic(\"invalid utf8 boundary (utf8)\");\n"
              "    char* r = (char*)malloc((size_t)(h - l) + 1);\n"
              "    if (h > l) memcpy(r, s + l, (size_t)(h - l));\n"
              "    r[h - l] = 0;\n"
              "    return r;\n}\n", name);
        return;
    }
    if (!strcmp(fam, "byte_at")) {
        sb_f(o, "static int %s(const char* s, int64_t i) {\n"
              "    size_t len = s ? strlen(s) : 0;\n"
              "    if (i < 0 || (unsigned long long)i >= len) ctron_panic(\"index out of bounds\");\n"
              "    return (unsigned char)s[i];\n}\n", name);
        return;
    }
    if (!strcmp(fam, "byte_slice")) {
        sb_f(o, "static const char* %s(const char* s, int64_t a, int64_t b) {\n"
              "    size_t len = s ? strlen(s) : 0;\n"
              "    if (a < 0 || b > (int64_t)len || a > b) ctron_panic(\"byte_slice 越界\");\n"
              "    char* r = (char*)malloc((size_t)(b - a) + 1);\n"
              "    if (b > a) memcpy(r, s + a, (size_t)(b - a));\n"
              "    r[b - a] = 0;\n"
              "    return r;\n}\n", name);
        return;
    }
    if (!strcmp(fam, "fmt")) {
        if (!strcmp(wl, "i64") || !strcmp(wl, "str")) { /* 不会到这 */ }
        char pt[16] = "int64_t";
        if (us) snprintf(pt, sizeof pt, "uint64_t");
        if (!strcmp(wl, "i8") || !strcmp(wl, "i16") || !strcmp(wl, "i32") || !strcmp(wl, "i64"))
            sb_f(o, "static const char* %s(int64_t v) { char* r = (char*)malloc(32); snprintf(r, 32, \"%%lld\", (long long)v); return r; }\n", name);
        else if (us)
            sb_f(o, "static const char* %s(uint64_t v) { char* r = (char*)malloc(32); snprintf(r, 32, \"%%llu\", (unsigned long long)v); return r; }\n", name);
        return;
    }
    if (!strcmp(name, "ctron_fmt_f64")) {
        sb_f(o, "static const char* %s(double v) { char* r = (char*)malloc(64); if (v == (double)(long long)v) snprintf(r, 64, \"%%.1f\", v); else snprintf(r, 64, \"%%g\", v); return r; }\n", name);
        return;
    }
    if (!strcmp(name, "ctron_fmt_bool")) {
        sb_f(o, "static const char* %s(int v) { char* r = (char*)malloc(8); snprintf(r, 8, \"%%s\", v ? \"true\" : \"false\"); return r; }\n", name);
        return;
    }
    if (!strcmp(fam, "as")) {
        sb_f(o, "static %s %s(int64_t v) {\n", ct, name);
        if (us)
            sb_f(o, "    uint64_t r = (uint64_t)v & %lluULL;\n"
                "    return (%s)(int64_t)r;\n}\n", (1ULL << bits) - 1, ct);
        else if (bits < 64)
            sb_f(o, "    uint64_t r = (uint64_t)v & %lluULL;\n"
                "    return (%s)((int64_t)(r ^ %lluULL) - (int64_t)%lluULL);\n}\n",
                (1ULL << bits) - 1, ct, (1ULL << (bits - 1)), (1ULL << (bits - 1)));
        else
            sb_f(o, "    return (%s)v;\n}\n", ct);
        return;
    }
    if (!strcmp(fam, "idx")) {
        sb_f(o, "static int64_t %s(int64_t n, int64_t i) { if (i < 0 || i >= n) ctron_panic(\"index out of bounds\"); return i; }\n", name);
        return;
    }
    if (!strcmp(fam, "decl")) {
        sb_f(o, "static %s %s(int64_t v) {\n", ct, name);
        if (us)
            sb_f(o, "    if ((__int128)(uint64_t)v > (__int128)(%s)) ctron_panic(\"integer overflow (decl)\");\n", hi);
        else
            sb_f(o, "    if ((__int128)v < (__int128)(%s) || (__int128)v > (__int128)(%s)) ctron_panic(\"integer overflow (decl)\");\n",
                 lo, hi);
        sb_f(o, "    return (%s)v;\n}\n", ct);
        return;
    }
    if (!strcmp(fam, "print") && !strcmp(wl, "str")) {
        sb_f(o, "static void %s(const char* v) { printf(\"%%s\", v ? v : \"\"); }\n", name);
        return;
    }
    if (!strcmp(fam, "print")) {
        if (isf)
            sb_f(o, "static void %s(double v) { char b[64]; if (v == (double)(long long)v) snprintf(b, sizeof b, \"%%.1f\", v); else snprintf(b, sizeof b, \"%%g\", v); printf(\"%%s\", b); }\n", name);
        else if (isb)
            sb_f(o, "static void %s(int v) { printf(\"%%s\", v ? \"true\" : \"false\"); }\n", name);
        else if (us)
            sb_f(o, "static void %s(uint64_t v) { printf(\"%%llu\", (unsigned long long)v); }\n", name);
        else
            sb_f(o, "static void %s(int64_t v) { printf(\"%%lld\", (long long)v); }\n", name);
        return;
    }
    if (!strncmp(name, "ctron_box_", 10)) {
        char k[48];
        snprintf(k, sizeof k, "%s", name + 10);
        if (!strcmp(k, "I32"))
            sb_f(o, "static int32_t* %s(int64_t v) { int32_t* p = (int32_t*)malloc(sizeof(int32_t)); *p = (int32_t)v; return p; }\n", name);
        else if (!strcmp(k, "I64"))
            sb_f(o, "static int64_t* %s(int64_t v) { int64_t* p = (int64_t*)malloc(sizeof(int64_t)); *p = v; return p; }\n", name);
        else if (!strcmp(k, "F64"))
            sb_f(o, "static double* %s(double v) { double* p = (double*)malloc(sizeof(double)); *p = v; return p; }\n", name);
        else if (!strcmp(k, "Bool"))
            sb_f(o, "static int* %s(int v) { int* p = (int*)malloc(sizeof(int)); *p = v; return p; }\n", name);
        else
            sb_f(o, "static ctron_t_%s* %s(ctron_t_%s v) { ctron_t_%s* p = (ctron_t_%s*)malloc(sizeof(ctron_t_%s)); *p = v; return p; }\n", k, name, k, k, k, k);
        return;
    }
    if (!strncmp(name, "ctron_new_", 10)) {
        // class 构造:字段逐个赋值
        char cn[48];
        snprintf(cn, sizeof cn, "%s", name + 10);
        cdef* cd = NULL;
        for (size_t i = 0; i < c->nclasses; i++)
            if (!strcmp(c->classes[i].name, cn)) { cd = &c->classes[i]; break; }
        if (!cd) return;
        sb sig = {0};
        sb_f(&sig, "static ctron_c_%s* %s(", cn, name);
        sb asg = {0};
        for (size_t j = 0; j < cd->n; j++) {
            ty ft = cd->fields[j].t;
            if (j) sb_s(&sig, ", ");
            sb_f(&sig, "%s f_%s", ctype_of(ft), cd->fields[j].name);
            sb_f(&asg, "    p->%s = f_%s;\n", cd->fields[j].name, cd->fields[j].name);
        }
        sb_f(o, "%s) {\n    ctron_c_%s* p = (ctron_c_%s*)calloc(1, sizeof(ctron_c_%s));\n", sig.d ? sig.d : "", cn, cn, cn);
        sb_s(o, asg.d ? asg.d : "");
        sb_f(o, "    return p;\n}\n");
        sb_free(&sig);
        sb_free(&asg);
        return;
    }
    if (!strncmp(name, "ctron_arr_", 10)) {
        // name: ctron_arr_<wl>_lit
        char wl[16];
        snprintf(wl, sizeof wl, "%s", name + 10);
        wl[strlen(wl) - 4] = 0; // 去掉 _lit
        if (!strcmp(wl, "str"))
            sb_f(o, "static ctron_arr_str %s(int64_t n, const char** vals) { ctron_arr_str r; r.n = n; r.d = (const char**)calloc((size_t)(n ? n : 1), sizeof(const char*)); for (int64_t i = 0; i < n; i++) r.d[i] = vals[i] ? vals[i] : \"\"; return r; }\n", name);
        else if (!strncmp(wl, "f", 1)) {
            sb_f(o, "static ctron_arr_%s %s(int64_t n, double* vals) { ctron_arr_%s r; r.n = n; r.d = (double*)calloc((size_t)(n ? n : 1), sizeof(double)); for (int64_t i = 0; i < n; i++) r.d[i] = vals[i]; return r; }\n",
                 wl, name, wl);
        } else {
            int ub = wl[0] == 'u';
            int bits = atoi(wl + 1);
            ty t = ty_int(bits, ub);
            if (bits < 64)
                sb_f(o, "static ctron_arr_%s %s(int64_t n, int64_t* vals) { ctron_arr_%s r; r.n = n; r.d = (%s*)calloc((size_t)(n ? n : 1), sizeof(%s)); for (int64_t i = 0; i < n; i++) r.d[i] = (%s)vals[i]; return r; }\n",
                     wl, name, wl, ctype_of(t), ctype_of(t), ctype_of(t));
            else
                sb_f(o, "static ctron_arr_%s %s(int64_t n, uint64_t* vals) { ctron_arr_%s r; r.n = n; r.d = (%s*)calloc((size_t)(n ? n : 1), sizeof(%s)); for (int64_t i = 0; i < n; i++) r.d[i] = (%s)vals[i]; return r; }\n",
                     wl, name, wl, ctype_of(t), ctype_of(t), ctype_of(t));
        }
        return;
    }
    if (!strcmp(fam, "assert_eq") || !strcmp(fam, "assert_ne")) {
        int ne = !strcmp(fam, "assert_ne");
        const char* label = ne ? "assert_ne failed" : "assert_eq failed";
        const char* cmp = ne ? "!=" : "==";
        if (!strcmp(wl, "str")) {
            sb_f(o, "static void %s(const char* a, const char* b) { if (!(strcmp(a ? a : \"\", b ? b : \"\") %s 0)) { char m[256]; snprintf(m, sizeof m, \"%s: %%s != %%s\", a ? a : \"\", b ? b : \"\"); ctron_panic(m); } }\n",
                 name, ne ? "!=" : "==", label);
        } else if (isf) {
            sb_f(o, "static void %s(double a, double b) {\n", name);
            sb_f(o, "    char x[64], y[64];\n");
            sb_f(o, "    if (a == (double)(long long)a) snprintf(x, sizeof x, \"%%.1f\", a); else snprintf(x, sizeof x, \"%%g\", a);\n");
            sb_f(o, "    if (b == (double)(long long)b) snprintf(y, sizeof y, \"%%.1f\", b); else snprintf(y, sizeof y, \"%%g\", b);\n");
            sb_f(o, "    if (!(a %s b)) { char m[160]; snprintf(m, sizeof m, \"%s: %%s != %%s\", x, y); ctron_panic(m); }\n}\n",
                 cmp, label);
        } else if (isb) {
            sb_f(o, "static void %s(int a, int b) { if (!(a %s b)) ctron_panic(\"%s\"); }\n", name, cmp, label);
        } else {
            const char* pt = us ? "uint64_t" : "int64_t";
            sb_f(o, "static void %s(%s a, %s b) { if (!(a %s b)) { char m[128]; snprintf(m, sizeof m, \"%s: %%llu != %%llu\", (unsigned long long)a, (unsigned long long)b); ctron_panic(m); } }\n",
                 name, pt, pt, cmp, label);
        }
        return;
    }
}

// ================= 文件级 =================
static void collect_types(tc* c, const cfile* f) {
    for (size_t i = 0; i < f->ndecls; i++) {
        const cdecl* d = &f->decls[i];
        if (d->kind == D_STRUCT) {
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

static void collect_fns(tc* c, const cfile* f) {
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

static void emit_fn(tc* c, const cfn* F, const char* cname) {
    ty ret = decl_ty_tc(c, F->ret);
    c->fn_ret = &ret;
    const char* rct = (ret.k == T_FLT) ? "double" : (ret.k == T_BOOL) ? "int" : (ret.k == T_STR) ? "const char*" : (ret.k == T_INT) ? "int64_t" : (ret.k == T_STRUCT || ret.k == T_ENUM || ret.k == T_SUM || ret.k == T_LIST || ret.k == T_CLASS || ret.k == T_BOX) ? ctype_of(ret) : "void";
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
            if (fn_has_trait_param(&c, &d->fn_)) break; // 泛型原体:调用点按实参单态化(C10-h)
            char cn[256];
            snprintf(cn, sizeof cn, "ctron_user_%s", nm);
            int was_main = !strcmp(nm, "main");
            c.in_main = was_main;
            emit_fn(&c, &d->fn_, cn);
            c.in_main = 0;
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
        int has_fn_main = 0;
        for (size_t i = 0; i < f->ndecls; i++)
            if (f->decls[i].kind == D_FN && !strcmp(f->decls[i].fn_.name, "main")) has_fn_main = 1;
        if (ntests) {
            sb_f(&c.body, "int main(void) {\n    if (setjmp(ctron_panic_frame)) return 1;\n");
            for (size_t i = 0; i < f->ndecls; i++)
                if (f->decls[i].kind == D_TEST) sb_f(&c.body, "    ctron_test_%zu();\n", i);
            sb_s(&c.body, "    return 0;\n}\n");
        } else if (has_fn_main) {
            sb_s(&c.body, "int main(void) {\n    if (setjmp(ctron_panic_frame)) return 1;\n    return (int)ctron_user_main();\n}\n");
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
                "typedef struct ctron_anyerr ctron_anyerr;\n"
                "struct ctron_anyerr { char* message; ctron_anyerr* cause; char* trace; };\n"
                "static jmp_buf ctron_panic_frame;\n"
                "static _Noreturn void ctron_panic(const char* msg) {\n"
                "    fprintf(stderr, \"%s\\n\", msg);\n"
                "    longjmp(ctron_panic_frame, 1);\n"
                "}\n"
                "static void ctron_print_nl(void) { printf(\"\\n\"); }\n"
                "static void ctron_assert(int v) { if (!v) ctron_panic(\"assert failed\"); }\n");
        for (size_t i = 0; i < c.nstructs; i++) {
            sdef* sd = &c.structs[i];
            sb_f(h, "typedef struct {");
            for (size_t j = 0; j < sd->n; j++)
                sb_f(h, " %s %s;", ctype_of(sd->fields[j].t), sd->fields[j].name);
            sb_f(h, " } ctron_t_%s;\n", sd->name);
        }
        for (size_t i = 0; i < c.nclasses; i++) {
            cdef* cd = &c.classes[i];
            sb_f(h, "typedef struct {");
            for (size_t j = 0; j < cd->n; j++)
                sb_f(h, " %s %s;", ctype_of(cd->fields[j].t), cd->fields[j].name);
            sb_f(h, " } ctron_c_%s;\n", cd->name);
        }
        for (size_t i = 0; i < c.nenums; i++) {
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
        for (size_t i = 0; i < c.n_globals; i++)
            sb_f(h, "static %s %s = %s;\n", ctype_of(c.globals[i].t), c.globals[i].name,
                 c.globals[i].init ? c.globals[i].init : "0");
        for (size_t i = 0; i < c.n_sums; i++)
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
        for (size_t i = 0; i < c.n_helpers; i++)
            emit_helper(&c, c.helpers[i]);
        for (size_t i = 0; i < f->ndecls; i++) {
            const cdecl* d = &f->decls[i];
            if (d->kind != D_FN) continue;
            if (fn_has_trait_param(&c, &d->fn_)) continue; // 泛型(trait 形参)原型跳过
            ty ret = decl_ty_tc(&c, d->fn_.ret);
            const char* rct = (ret.k == T_FLT) ? "double" : (ret.k == T_BOOL) ? "int" : (ret.k == T_STR) ? "const char*" : (ret.k == T_INT) ? "int64_t" : (ret.k == T_STRUCT || ret.k == T_ENUM || ret.k == T_SUM || ret.k == T_LIST || ret.k == T_CLASS || ret.k == T_BOX) ? ctype_of(ret) : "void";
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
        ctron_arena_free(a);
        return res;
    }

    sb all = {0};
    sb_s(&all, c.head.d ? c.head.d : "");
    sb_s(&all, c.m_sb.d ? c.m_sb.d : "");
    sb_s(&all, c.s_sb.d ? c.s_sb.d : "");
    sb_s(&all, c.body.d ? c.body.d : "");
    res.code = all.d ? all.d : strdup("");
    sb_free(&c.head);
    sb_free(&c.body);
    sb_free(&c.s_sb);
    ctron_arena_free(a);
    return res;
}static ty box_elem(ty t) {
    ty e = ty_unk(); e.k = t.ek; e.bits = t.ebits; e.us = t.eus;
    if (t.ek == T_FLT) e = ty_flt();
    else if (t.ek == T_BOOL) e = ty_bool();
    else if (t.ek == T_STR) e = ty_str();
    else if (t.ek == T_STRUCT || t.ek == T_CLASS || t.ek == T_ENUM) e.tname = t.tname;
    return e;
}
static ty sum_ty_of(tc* c, const cty* t);
static ty sum_ty_of(tc* c, const cty* t) {
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

static ty decl_ty(const cty* t) {
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

