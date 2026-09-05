// parser.c —— 递归下降解析器(规范 §1.7 EBNF + §1.8 消歧 + §4 优先级)。
// C 版独立实现,行为同源于共享规范与语料验收。诊断码:E1001 / E3030。
#include "parser.h"
#include "arena.h"
#include "lexer.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============ 列表基建 ============
// PT_LIST:指针子节点数组(元素为 arena 对象指针)。
// VLIST: 值子节点数组(元素为结构值;解析期逐个 arena 分配、收尾拷成连续数组)。
// SV:    char* 数组。

#define PT_LIST(T, SUF)                                                        \
    typedef struct { T** d; size_t n, cap; } SUF;                              \
    static __attribute__((unused)) void SUF##_push(SUF* v, T* p) {                                     \
        if (v->n == v->cap) {                                                  \
            v->cap = v->cap ? v->cap * 2 : 16;                                 \
            v->d = (T**)realloc(v->d, v->cap * sizeof(T*));                    \
            if (!v->d) abort();                                                \
        }                                                                      \
        v->d[v->n++] = p;                                                      \
    }                                                                          \
    static T** SUF##_done(SUF* v, ctron_arena* a, size_t* nout) {               \
        T** out = NULL;                                                        \
        if (v->n) {                                                            \
            out = (T**)ctron_arena_alloc(a, v->n * sizeof(T*));                \
            memcpy(out, v->d, v->n * sizeof(T*));                              \
        }                                                                      \
        *nout = v->n;                                                          \
        free(v->d);                                                            \
        v->d = NULL; v->n = v->cap = 0;                                        \
        return out;                                                            \
    }

#define VLIST(V, SUF)                                                          \
    typedef struct { V** d; size_t n, cap; } SUF;                              \
    static V* SUF##_new(SUF* v, ctron_arena* a) {                              \
        if (v->n == v->cap) {                                                  \
            v->cap = v->cap ? v->cap * 2 : 16;                                 \
            v->d = (V**)realloc(v->d, v->cap * sizeof(V*));                    \
            if (!v->d) abort();                                                \
        }                                                                      \
        V* p = (V*)ctron_arena_alloc(a, sizeof(V));                            \
        v->d[v->n++] = p;                                                      \
        return p;                                                              \
    }                                                                          \
    static __attribute__((unused)) V* SUF##_done(SUF* v, ctron_arena* a, size_t* nout) {               \
        V* out = NULL;                                                         \
        if (v->n) {                                                            \
            out = (V*)ctron_arena_alloc(a, v->n * sizeof(V));                  \
            for (size_t i = 0; i < v->n; i++) out[i] = *v->d[i];               \
        }                                                                      \
        *nout = v->n;                                                          \
        free(v->d);                                                            \
        v->d = NULL; v->n = v->cap = 0;                                        \
        return out;                                                            \
    }

typedef struct { char** d; size_t n, cap; } sv;
static void sv_push(sv* v, char* s) {
    if (v->n == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 16;
        v->d = (char**)realloc(v->d, v->cap * sizeof(char*));
        if (!v->d) abort();
    }
    v->d[v->n++] = s;
}
static char** sv_done(sv* v, ctron_arena* a, size_t* nout) {
    char** out = NULL;
    if (v->n) {
        out = (char**)ctron_arena_alloc(a, v->n * sizeof(char*));
        memcpy(out, v->d, v->n * sizeof(char*));
    }
    *nout = v->n;
    free(v->d);
    v->d = NULL; v->n = v->cap = 0;
    return out;
}

PT_LIST(cexpr, elist)
PT_LIST(cty, tlist)
PT_LIST(cpat, patlist)
PT_LIST(cstmt, slist)
VLIST(cattr, alist)
VLIST(ctypeparam, tplist)
VLIST(cfield, fdlist)
VLIST(cparam, prlist)
VLIST(cprop, proplist)
VLIST(cvariant, vlist)
VLIST(cclassitem, cilist)
VLIST(ctraititem, tilist)
VLIST(cimplitem, iilist)
VLIST(cfieldinit, filist)
VLIST(cclosureparam, cplist)
VLIST(cstructpatfield, spflist)
VLIST(cmatcharm, arm)
VLIST(cdecl, declist)

typedef struct { cimport** d; size_t n, cap; } iv;

// ============ 游标 ============

typedef struct {
    const ctron_token* toks;
    size_t ntoks;
    size_t pos;
    ctron_arena* arena;
    ctron_diag* diags; // 堆;消息在 arena
    size_t ndiags, cap;
    uint32_t depth;
    ctron_token eof_tok; // 越界哨兵
} cparser;

enum { MAX_EXPR_DEPTH = 256 };

static ctron_tok_kind tok_at(const cparser* p, size_t n) {
    if (p->pos + n < p->ntoks) return p->toks[p->pos + n].kind;
    return TOK_EOF;
}
static const ctron_token* tokp_at(const cparser* p, size_t n) {
    if (p->pos + n < p->ntoks) return &p->toks[p->pos + n];
    return &p->eof_tok;
}
static ctron_tok_kind peek_k(const cparser* p) { return tok_at(p, 0); }
static ctron_token bump_tok(cparser* p) {
    const ctron_token* t = tokp_at(p, 0);
    if (p->pos < p->ntoks) p->pos++;
    return *t;
}
static int at_k(const cparser* p, ctron_tok_kind k) { return peek_k(p) == k; }
static int eat_k(cparser* p, ctron_tok_kind k) {
    if (at_k(p, k)) { bump_tok(p); return 1; }
    return 0;
}
static void skip_newlines(cparser* p) { while (at_k(p, TOK_NEWLINE)) bump_tok(p); }

static void diag_push(cparser* p, ctron_diag d) {
    if (p->ndiags == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 16;
        p->diags = (ctron_diag*)realloc(p->diags, p->cap * sizeof(ctron_diag));
        if (!p->diags) abort();
    }
    p->diags[p->ndiags++] = d;
}

static void err_here(cparser* p, const char* code, const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    ctron_diag d;
    d.code = code;
    d.message = ctron_arena_strndup(p->arena, buf, strlen(buf));
    d.span = tokp_at(p, 0)->span;
    diag_push(p, d);
}

// 标记显示(中文;近似 Rust tok_display)
static const char* tok_display_name(ctron_tok_kind k) {
    switch (k) {
    case TOK_INT: return "整数字面量";
    case TOK_FLOAT: return "浮点字面量";
    case TOK_STR: return "字符串";
    case TOK_IDENT: return "标识符";
    case TOK_FN: return "fn"; case TOK_LET: return "let"; case TOK_VAR: return "var";
    case TOK_CONST: return "const"; case TOK_STATIC: return "static";
    case TOK_COMPTIME: return "comptime"; case TOK_IF: return "if";
    case TOK_ELSE: return "else"; case TOK_MATCH: return "match";
    case TOK_WHILE: return "while"; case TOK_FOR: return "for"; case TOK_IN: return "in";
    case TOK_RETURN: return "return"; case TOK_STRUCT: return "struct";
    case TOK_CLASS: return "class"; case TOK_ENUM: return "enum";
    case TOK_TRAIT: return "trait"; case TOK_IMPL: return "impl";
    case TOK_OWN: return "own"; case TOK_SCOPE: return "scope"; case TOK_TEST: return "test";
    case TOK_USE: return "use"; case TOK_PUB: return "pub"; case TOK_EXTERN: return "extern";
    case TOK_PROP: return "prop"; case TOK_TRUE: return "true"; case TOK_FALSE: return "false";
    case TOK_VOID: return "void"; case TOK_SELF: return "self";
    case TOK_PLUS: return "+"; case TOK_MINUS: return "-"; case TOK_STAR: return "*";
    case TOK_SLASH: return "/"; case TOK_PERCENT: return "%";
    case TOK_WRAP_PLUS: return "+%"; case TOK_WRAP_MINUS: return "-%";
    case TOK_PLUS_EQ: return "+="; case TOK_MINUS_EQ: return "-=";
    case TOK_STAR_EQ: return "*="; case TOK_SLASH_EQ: return "/=";
    case TOK_PERCENT_EQ: return "%="; case TOK_EQ_EQ: return "==";
    case TOK_NOT_EQ: return "!="; case TOK_LT: return "<"; case TOK_GT: return ">";
    case TOK_LT_EQ: return "<="; case TOK_GT_EQ: return ">="; case TOK_ASSIGN: return "=";
    case TOK_AND_AND: return "&&"; case TOK_OR: return "or";
    case TOK_DOT_DOT: return ".."; case TOK_DOT_DOT_EQ: return "..=";
    case TOK_ARROW: return "->"; case TOK_FAT_ARROW: return "=>";
    case TOK_QUESTION: return "?"; case TOK_DOT: return ".";
    case TOK_COMMA: return ","; case TOK_COLON: return ":";
    case TOK_LBRACKET: return "["; case TOK_RBRACKET: return "]";
    case TOK_LPAREN: return "("; case TOK_RPAREN: return ")";
    case TOK_LBRACE: return "{"; case TOK_RBRACE: return "}";
    case TOK_PIPE: return "|"; case TOK_AMP: return "&"; case TOK_HASH: return "#";
    case TOK_AT: return "@"; case TOK_UNDERSCORE: return "_"; case TOK_BANG: return "!";
    case TOK_NEWLINE: return "换行"; case TOK_EOF: return "文件尾";
    default: return "?";
    }
}

// 记号说明:如 标识符("x")/整数字面量("1")
static const char* tok_desc(cparser* p) {
    const ctron_token* t = tokp_at(p, 0);
    static char buf[128];
    const char* n = tok_display_name(t->kind);
    switch (t->kind) {
    case TOK_IDENT:
    case TOK_INT:
    case TOK_FLOAT:
        snprintf(buf, sizeof buf, "%s(%s)", n, t->text ? t->text : "");
        break;
    default:
        snprintf(buf, sizeof buf, "%s", n);
        break;
    }
    return buf;
}

/// 停在首个非换行记号(不改游标),返回其种类。
static ctron_tok_kind lookahead_past_newlines(const cparser* p) {
    size_t i = p->pos;
    while (i < p->ntoks && p->toks[i].kind == TOK_NEWLINE) i++;
    if (i < p->ntoks) return p->toks[i].kind;
    return TOK_EOF;
}

/// 循环停滞守卫:一轮未推进则报错并强制消费。
static void ensure_progress(cparser* p, size_t before) {
    if (p->pos == before) {
        err_here(p, "E1001", "无法解析的语法元素");
        bump_tok(p);
    }
}

// ============ 构造辅助 ============

static char* dup_text(cparser* p, const char* s) {
    if (!s) return NULL;
    return ctron_arena_strndup(p->arena, s, strlen(s));
}

static cexpr* mk_expr(cparser* p, cexpr_kind k) {
    cexpr* e = (cexpr*)ctron_arena_alloc(p->arena, sizeof(cexpr));
    e->kind = k;
    return e;
}
static cty* mk_type(cparser* p, cty_kind k) {
    cty* t = (cty*)ctron_arena_alloc(p->arena, sizeof(cty));
    t->kind = k;
    return t;
}
static cblock* mk_block(cparser* p) {
    return (cblock*)ctron_arena_alloc(p->arena, sizeof(cblock));
}
static cpat* cpat_wild(cparser* p) {
    cpat* w = (cpat*)ctron_arena_alloc(p->arena, sizeof(cpat));
    w->kind = PAT_WILD;
    return w;
}

static const char* suffix_str(ctron_suffix s) {
    if (s == SUF_NONE) return "";
    return ctron_suffix_name(s); // 小写名,如 "u8"
}

// Str 部件拷贝到 AST(arena)
static void ast_str_parts(cparser* p, const ctron_str_parts* src, ctron_str_part** out, size_t* nout) {
    *nout = src->len;
    *out = NULL;
    if (!src->len) return;
    ctron_str_part* arr = (ctron_str_part*)ctron_arena_alloc(p->arena, src->len * sizeof(ctron_str_part));
    for (size_t i = 0; i < src->len; i++) {
        arr[i].kind = src->items[i].kind;
        arr[i].s = dup_text(p, src->items[i].s);
    }
    *out = arr;
}

// ============ 文件与声明 ============

static cfn* parse_fn(cparser* p, cattr** attrs, size_t nattrs, int top_level);
static cexpr* parse_expr_flags(cparser* p, int allow_struct);
static cexpr* parse_expr(cparser* p) { return parse_expr_flags(p, 1); }
static cty* parse_type(cparser* p);
static cblock* parse_block(cparser* p);
static cblock* parse_block_after_lbrace(cparser* p);
static cpat* parse_pattern(cparser* p);

static cvis parse_vis(cparser* p) {
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

static void parse_attrs(cparser* p, alist* attrs, sv* derives) {
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
static sv parse_dotted_path_sv(cparser* p) {
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

static char* expect_ident(cparser* p, const char* ctx) {
    if (peek_k(p) == TOK_IDENT) {
        char* s = dup_text(p, tokp_at(p, 0)->text);
        bump_tok(p);
        return s;
    }
    err_here(p, "E1001", "预期标识符(%s),实际 %s", ctx, tok_desc(p));
    return dup_text(p, "");
}

static cfield* parse_field(cparser* p, cvis vis, fdlist* out) {
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

static cfn* parse_fn(cparser* p, cattr** attrs, size_t nattrs, int top_level) {
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
    for (;;) {
        if (at_k(p, TOK_RPAREN)) break;
        if (at_k(p, TOK_AMP) && tok_at(p, 1) == TOK_SELF) {
            bump_tok(p); bump_tok(p);
            cparam* pr = prlist_new(&prs, p->arena);
            pr->is_receiver = 1; pr->is_var = 0; pr->name = NULL; pr->ty = NULL;
        } else if (at_k(p, TOK_VAR) && tok_at(p, 1) == TOK_SELF) {
            bump_tok(p); bump_tok(p);
            cparam* pr = prlist_new(&prs, p->arena);
            pr->is_receiver = 1; pr->is_var = 1; pr->name = NULL; pr->ty = NULL;
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
    f->name = name;
    f->type_params = tplist_done(&tps, p->arena, &f->ntype_params);
    f->params = prlist_done(&prs, p->arena, &f->nparams);
    f->ret = ret;
    f->body = body;
    return f;
}

// prop 声明(共享 trait/class/impl 循环)
static void parse_prop(cparser* p, cattr** attrs, size_t nattrs, cvis vis, proplist* out) {
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
static void parse_use(cparser* p, cuse* u) {
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
static int parse_decl(cparser* p, cattr** attrs, size_t nattrs, sv* derives, cdecl* out) {
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

static int parse_file(cparser* p, cfile* f) {
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

// ============ 类型 ============

// [ 处内容为空
static int bracket_content_is_empty(const cparser* p) {
    return tok_at(p, 1) == TOK_RBRACKET;
}
// [ 处内容恰为一个整数字面量
static int bracket_content_is_single_int(const cparser* p) {
    return tok_at(p, 1) == TOK_INT && tok_at(p, 2) == TOK_RBRACKET;
}
// [ 配对 ] 之后是否紧跟 ( 或 {
static int bracket_followed_by_call_or_lit(const cparser* p) {
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

static cty* parse_type(cparser* p) {
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

static cexpr* parse_primary_inner(cparser* p, int allow_struct);
static cexpr* parse_primary(cparser* p, int allow_struct);
static cexpr* parse_postfix(cparser* p, int allow_struct);
static cexpr* parse_unary(cparser* p, int allow_struct);
static cexpr* parse_multiplicative(cparser* p, int allow_struct);
static cexpr* parse_additive(cparser* p, int allow_struct);
static cexpr* parse_range(cparser* p, int allow_struct);
static cexpr* parse_compare(cparser* p, int allow_struct);
static cexpr* parse_and(cparser* p, int allow_struct);
static cexpr* parse_or(cparser* p, int allow_struct);

static cexpr* parse_expr_flags(cparser* p, int allow_struct) {
    return parse_or(p, allow_struct);
}

static cexpr* parse_or(cparser* p, int allow_struct) {
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
static cexpr* parse_and(cparser* p, int allow_struct) {
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
static cexpr* parse_compare(cparser* p, int allow_struct) {
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
static cexpr* parse_range(cparser* p, int allow_struct) {
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
static cexpr* parse_additive(cparser* p, int allow_struct) {
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
static cexpr* parse_multiplicative(cparser* p, int allow_struct) {
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
static cexpr* parse_unary(cparser* p, int allow_struct) {
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

static cexpr* parse_postfix(cparser* p, int allow_struct) {
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

static cexpr* parse_struct_lit(cparser* p, sv* path) {
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

static cexpr* parse_if(cparser* p) {
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

static cexpr* parse_match(cparser* p) {
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

static cexpr* parse_scope(cparser* p) {
    bump_tok(p); // scope
    cexpr* s = mk_expr(p, EX_SCOPE);
    if (!eat_k(p, TOK_LBRACE)) err_here(p, "E1001", "scope 块缺少 {");
    if (!eat_k(p, TOK_PIPE)) err_here(p, "E1001", "scope 参数缺少 |");
    s->sparam = expect_ident(p, "scope 参数");
    if (!eat_k(p, TOK_PIPE)) err_here(p, "E1001", "scope 参数缺少 |");
    s->sbody = parse_block_after_lbrace(p);
    return s;
}

static cexpr* parse_own(cparser* p) {
    bump_tok(p); // own
    cexpr* o = mk_expr(p, EX_OWN);
    if (!eat_k(p, TOK_LPAREN)) err_here(p, "E1001", "own 块缺少 (");
    o->arena_name = expect_ident(p, "arena 名");
    if (!eat_k(p, TOK_RPAREN)) err_here(p, "E1001", "own 块缺少 )");
    o->obody = parse_block(p);
    return o;
}

static cexpr* parse_closure(cparser* p) {
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

static cexpr* parse_primary(cparser* p, int allow_struct) {
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

static cexpr* parse_primary_inner(cparser* p, int allow_struct) {
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

static cblock* parse_block_after_lbrace(cparser* p) {
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

static cblock* parse_block(cparser* p) {
    if (!eat_k(p, TOK_LBRACE)) err_here(p, "E1001", "块缺少 {");
    return parse_block_after_lbrace(p);
}

// ============ 模式 ============

static cpat* parse_pattern(cparser* p) {
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

// ============ 入口 ============

ctron_parse_result ctron_parse_src(const char* src, size_t len) {
    ctron_parse_result res;
    memset(&res, 0, sizeof res);
    ctron_lex_result lr = ctron_lex(src, len);

    cparser p;
    memset(&p, 0, sizeof p);
    p.toks = lr.toks;
    p.ntoks = lr.ntoks;
    p.arena = ctron_arena_new();
    // 词法诊断(消息拷贝进解析 arena;与 Rust 一致:词法在前)
    for (size_t i = 0; i < lr.ndiags; i++) {
        ctron_diag d = lr.diags[i];
        d.message = ctron_arena_strndup(p.arena, d.message, strlen(d.message));
        diag_push(&p, d);
    }

    cfile* f = (cfile*)ctron_arena_alloc(p.arena, sizeof(cfile));
    parse_file(&p, f);

    // 记号/词法 arena 在解析完成后才释放(记号载荷文本在解析期被读取)
    ctron_lex_result_free(&lr);

    res.file = f;
    res.diags = p.diags;
    res.ndiags = p.ndiags;
    res._arena = p.arena;
    return res;
}

void ctron_parse_result_free(ctron_parse_result* r) {
    if (!r) return;
    free(r->diags);
    ctron_arena_free((ctron_arena*)r->_arena);
    r->file = NULL;
    r->diags = NULL;
    r->ndiags = 0;
    r->_arena = NULL;
}
