#include "parser_internal.h"

// parser_core.c —— 列表基建/游标/AST 构造辅助
void sv_push(sv* v, char* s) {
    if (v->n == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 16;
        v->d = (char**)realloc(v->d, v->cap * sizeof(char*));
        if (!v->d) abort();
    }
    v->d[v->n++] = s;
}
char** sv_done(sv* v, ctron_arena* a, size_t* nout) {
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
ctron_tok_kind tok_at(const cparser* p, size_t n) {
    if (p->pos + n < p->ntoks) return p->toks[p->pos + n].kind;
    return TOK_EOF;
}
const ctron_token* tokp_at(const cparser* p, size_t n) {
    if (p->pos + n < p->ntoks) return &p->toks[p->pos + n];
    return &p->eof_tok;
}
ctron_tok_kind peek_k(const cparser* p) { return tok_at(p, 0); }
ctron_token bump_tok(cparser* p) {
    const ctron_token* t = tokp_at(p, 0);
    if (p->pos < p->ntoks) p->pos++;
    return *t;
}
int at_k(const cparser* p, ctron_tok_kind k) { return peek_k(p) == k; }
int eat_k(cparser* p, ctron_tok_kind k) {
    if (at_k(p, k)) { bump_tok(p); return 1; }
    return 0;
}
void skip_newlines(cparser* p) { while (at_k(p, TOK_NEWLINE)) bump_tok(p); }
void diag_push(cparser* p, ctron_diag d) {
    if (p->ndiags == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 16;
        p->diags = (ctron_diag*)realloc(p->diags, p->cap * sizeof(ctron_diag));
        if (!p->diags) abort();
    }
    p->diags[p->ndiags++] = d;
}
void err_here(cparser* p, const char* code, const char* fmt, ...) {
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
const char* tok_display_name(ctron_tok_kind k) {
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
const char* tok_desc(cparser* p) {
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
ctron_tok_kind lookahead_past_newlines(const cparser* p) {
    size_t i = p->pos;
    while (i < p->ntoks && p->toks[i].kind == TOK_NEWLINE) i++;
    if (i < p->ntoks) return p->toks[i].kind;
    return TOK_EOF;
}
void ensure_progress(cparser* p, size_t before) {
    if (p->pos == before) {
        err_here(p, "E1001", "无法解析的语法元素");
        bump_tok(p);
    }
}

// ============ 构造辅助 ============

char* dup_text(cparser* p, const char* s) {
    if (!s) return NULL;
    return ctron_arena_strndup(p->arena, s, strlen(s));
}

cexpr* mk_expr(cparser* p, cexpr_kind k) {
    cexpr* e = (cexpr*)ctron_arena_alloc(p->arena, sizeof(cexpr));
    e->kind = k;
    return e;
}
cty* mk_type(cparser* p, cty_kind k) {
    cty* t = (cty*)ctron_arena_alloc(p->arena, sizeof(cty));
    t->kind = k;
    return t;
}
cblock* mk_block(cparser* p) {
    return (cblock*)ctron_arena_alloc(p->arena, sizeof(cblock));
}
cpat* cpat_wild(cparser* p) {
    cpat* w = (cpat*)ctron_arena_alloc(p->arena, sizeof(cpat));
    w->kind = PAT_WILD;
    return w;
}

const char* suffix_str(ctron_suffix s) {
    if (s == SUF_NONE) return "";
    return ctron_suffix_name(s); // 小写名,如 "u8"
}

// Str 部件拷贝到 AST(arena)
void ast_str_parts(cparser* p, const ctron_str_parts* src, ctron_str_part** out, size_t* nout) {
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
