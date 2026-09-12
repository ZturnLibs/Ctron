// parser_internal.h —— 解析器内部共享契约(模块拆分,原 parser.c 单体)。
// 模块:parser_core(列表/游标/构造辅助) parser_decl(文件与声明)
//       parser_expr(类型/表达式/块与语句/模式) parser(入口)。
// 诊断契约:E1001 / E3030;错误恢复全部带停滞守卫(ensure_progress)。
#ifndef CTRON_PARSER_INTERNAL_H
#define CTRON_PARSER_INTERNAL_H

#include "parser.h"

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




// 标记显示(中文;近似 Rust tok_display)

// 记号说明:如 标识符("x")/整数字面量("1")

/// 停在首个非换行记号(不改游标),返回其种类。

/// 循环停滞守卫:一轮未推进则报错并强制消费。
// Str 部件拷贝到 AST(arena)

// ================= 跨模块原型 =================
void ast_str_parts(cparser* p, const ctron_str_parts* src, ctron_str_part** out, size_t* nout);
int at_k(const cparser* p, ctron_tok_kind k);
int bracket_content_is_empty(const cparser* p);
int bracket_content_is_single_int(const cparser* p);
int bracket_followed_by_call_or_lit(const cparser* p);
ctron_token bump_tok(cparser* p);
cpat* cpat_wild(cparser* p);
void ctron_parse_result_free(ctron_parse_result* r);
ctron_parse_result ctron_parse_src(const char* src, size_t len);
void diag_push(cparser* p, ctron_diag d);
char* dup_text(cparser* p, const char* s);
int eat_k(cparser* p, ctron_tok_kind k);
void ensure_progress(cparser* p, size_t before);
void err_here(cparser* p, const char* code, const char* fmt, ...);
char* expect_ident(cparser* p, const char* ctx);
ctron_tok_kind lookahead_past_newlines(const cparser* p);
cblock* mk_block(cparser* p);
cexpr* mk_expr(cparser* p, cexpr_kind k);
cty* mk_type(cparser* p, cty_kind k);
cexpr* parse_additive(cparser* p, int allow_struct);
cexpr* parse_and(cparser* p, int allow_struct);
void parse_attrs(cparser* p, alist* attrs, sv* derives);
cblock* parse_block(cparser* p);
cblock* parse_block_after_lbrace(cparser* p);
cexpr* parse_closure(cparser* p);
cexpr* parse_compare(cparser* p, int allow_struct);
int parse_decl(cparser* p, cattr** attrs, size_t nattrs, sv* derives, cdecl* out);
sv parse_dotted_path_sv(cparser* p);
cexpr* parse_expr(cparser* p);
cexpr* parse_expr_flags(cparser* p, int allow_struct);
cfield* parse_field(cparser* p, cvis vis, fdlist* out);
int parse_file(cparser* p, cfile* f);
cfn* parse_fn(cparser* p, cattr** attrs, size_t nattrs, int top_level);
cexpr* parse_if(cparser* p);
cexpr* parse_match(cparser* p);
cexpr* parse_multiplicative(cparser* p, int allow_struct);
cexpr* parse_or(cparser* p, int allow_struct);
cexpr* parse_oror(cparser* p, int allow_struct);
cexpr* parse_own(cparser* p);
cpat* parse_pattern(cparser* p);
cexpr* parse_postfix(cparser* p, int allow_struct);
cexpr* parse_primary(cparser* p, int allow_struct);
cexpr* parse_primary_inner(cparser* p, int allow_struct);
void parse_prop(cparser* p, cattr** attrs, size_t nattrs, cvis vis, proplist* out);
cexpr* parse_range(cparser* p, int allow_struct);
cexpr* parse_scope(cparser* p);
cexpr* parse_struct_lit(cparser* p, sv* path);
cty* parse_type(cparser* p);
cexpr* parse_unary(cparser* p, int allow_struct);
void parse_use(cparser* p, cuse* u);
cvis parse_vis(cparser* p);
ctron_tok_kind peek_k(const cparser* p);
void skip_newlines(cparser* p);
const char* suffix_str(ctron_suffix s);
char** sv_done(sv* v, ctron_arena* a, size_t* nout);
void sv_push(sv* v, char* s);
ctron_tok_kind tok_at(const cparser* p, size_t n);
const char* tok_desc(cparser* p);
const char* tok_display_name(ctron_tok_kind k);
const ctron_token* tokp_at(const cparser* p, size_t n);

#endif
