// lexer.c —— 手写单遍词法器(规范 §1)。C 版独立实现。
// 错误恢复为循环(吞错误记号后由主循环续取),非递归:100k 连续 ';' 不栈溢出。
#include "lexer.h"
#include "arena.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------- 堆动态数组(记号/诊断/字节缓冲;重分配移动数组但不移动 arena 字符串) ----------

typedef struct { ctron_token* d; size_t n, cap; } tvec;
typedef struct { ctron_diag* d; size_t n, cap; } dvec;
typedef struct { unsigned char* d; size_t n, cap; } bvec;

static void tvec_push(tvec* v, ctron_token t) {
    if (v->n == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 64;
        v->d = (ctron_token*)realloc(v->d, v->cap * sizeof(*v->d));
        if (!v->d) abort();
    }
    v->d[v->n++] = t;
}
static void dvec_push(dvec* v, ctron_diag d) {
    if (v->n == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 16;
        v->d = (ctron_diag*)realloc(v->d, v->cap * sizeof(*v->d));
        if (!v->d) abort();
    }
    v->d[v->n++] = d;
}
static void bvec_reset(bvec* v) { v->n = 0; }
static void bvec_push(bvec* v, unsigned char c) {
    if (v->n == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 64;
        v->d = (unsigned char*)realloc(v->d, v->cap);
        if (!v->d) abort();
    }
    v->d[v->n++] = c;
}
// ---------- 词法器 ----------

typedef struct {
    const char* src;
    size_t len;
    size_t pos;
    uint32_t line, col;
    int prev_is_dot; // 上一实记号是否为 '.'(成员位置的 or 是方法名,§5.2)
    ctron_arena* arena;
    dvec diags;
} lexer;

static unsigned char peek(lexer* lx) { return lx->pos < lx->len ? (unsigned char)lx->src[lx->pos] : 0; }
static unsigned char peek2(lexer* lx) { return lx->pos + 1 < lx->len ? (unsigned char)lx->src[lx->pos + 1] : 0; }
static unsigned char peek3(lexer* lx) { return lx->pos + 2 < lx->len ? (unsigned char)lx->src[lx->pos + 2] : 0; }

static unsigned char bump(lexer* lx) {
    if (lx->pos >= lx->len) return 0;
    unsigned char c = (unsigned char)lx->src[lx->pos++];
    if (c == '\n') { lx->line++; lx->col = 1; } else { lx->col++; }
    return c;
}

static ctron_span mark(lexer* lx, size_t start, uint32_t line, uint32_t col) {
    ctron_span s = { line, col, start, lx->pos };
    return s;
}

static void lx_err(lexer* lx, ctron_span sp, const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    ctron_diag d;
    d.code = "E1001";
    d.message = ctron_arena_strndup(lx->arena, buf, strlen(buf));
    d.span = sp;
    dvec_push(&lx->diags, d);
}

static int is_ident_start(unsigned char c) { return isalpha(c) || c == '_'; }
static int is_ident_cont(unsigned char c) { return isalnum(c) || c == '_'; }
static int is_digit_c(unsigned char c) { return isdigit(c); }

/// 行内空白与注释(含 /// 与 //@)是 trivia;换行不在此处理(它是记号)。
static void skip_inline_trivia(lexer* lx) {
    for (;;) {
        unsigned char c = peek(lx);
        if (c == ' ' || c == '\t' || c == '\r') { bump(lx); }
        else if (c == '/' && peek2(lx) == '/') {
            while (lx->pos < lx->len && peek(lx) != '\n') bump(lx);
        }
        else break;
    }
}

// ---------- 名字与关键字 ----------

static ctron_tok_kind keyword_of(const char* s) {
    struct { const char* kw; ctron_tok_kind k; } map[] = {
        {"fn", TOK_FN}, {"let", TOK_LET}, {"var", TOK_VAR},
        {"const", TOK_CONST}, {"static", TOK_STATIC}, {"comptime", TOK_COMPTIME},
        {"if", TOK_IF}, {"else", TOK_ELSE}, {"match", TOK_MATCH},
        {"while", TOK_WHILE}, {"for", TOK_FOR}, {"in", TOK_IN},
        {"break", TOK_BREAK}, {"continue", TOK_CONTINUE},
        {"return", TOK_RETURN}, {"struct", TOK_STRUCT}, {"class", TOK_CLASS},
        {"enum", TOK_ENUM}, {"trait", TOK_TRAIT}, {"impl", TOK_IMPL},
        {"own", TOK_OWN}, {"scope", TOK_SCOPE}, {"test", TOK_TEST},
        {"use", TOK_USE}, {"pub", TOK_PUB}, {"extern", TOK_EXTERN},
        {"prop", TOK_PROP},
        {"true", TOK_TRUE}, {"false", TOK_FALSE}, {"void", TOK_VOID},
        {"self", TOK_SELF},
    };
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
        if (strcmp(s, map[i].kw) == 0) return map[i].k;
    return TOK_KIND_COUNT; // 非关键字
}

static int lex_name(lexer* lx, size_t start, uint32_t line, uint32_t col, ctron_token* out) {
    while (lx->pos < lx->len && is_ident_cont(peek(lx))) bump(lx);
    size_t n = lx->pos - start;
    char* text = ctron_arena_strndup(lx->arena, lx->src + start, n);
    ctron_tok_kind k = TOK_KIND_COUNT;
    if (n == 1 && text[0] == '_') k = TOK_UNDERSCORE; // 通配(§1.5)
    else {
        k = keyword_of(text);
        if (k == TOK_KIND_COUNT) {
            // 保留运算符字(§1.3);成员位置的 or 除外:x.or(默认)(§5.2)
            if (strcmp(text, "or") == 0 && !lx->prev_is_dot) k = TOK_OR;
            else k = TOK_IDENT;
        }
    }
    out->kind = k;
    out->suffix = SUF_NONE;
    out->text = text;
    out->parts.items = NULL;
    out->parts.len = 0;
    out->span = mark(lx, start, line, col);
    return 1;
}

// ---------- 数字字面量 ----------

static int digit_in(unsigned char c, int radix) {
    if (c == '_') return 1;
    int v = -1;
    if (isdigit(c)) v = c - '0';
    else if (radix == 16 && isalpha(c)) v = tolower(c) - 'a' + 10;
    return v >= 0 && v < radix;
}

/// 读数字(区分大小写、仅小写;§1.4)。文本切片不含后缀;span 含后缀。
static int lex_number(lexer* lx, size_t start, uint32_t line, uint32_t col, ctron_token* out) {
    int radix = 0;
    if (peek(lx) == '0') {
        unsigned char p2 = peek2(lx);
        if (p2 == 'x') radix = 16; // 大写前缀不复认:0XFF → Int("0") + Ident("XFF")
        else if (p2 == 'o') radix = 8;
        else if (p2 == 'b') radix = 2;
    }
    int is_float = 0;
    size_t text_end;
    if (radix) {
        bump(lx); bump(lx); // 进制前缀
        size_t ds = lx->pos;
        while (lx->pos < lx->len && digit_in(peek(lx), radix)) bump(lx);
        if (lx->pos == ds) {
            ctron_span sp = mark(lx, start, line, col);
            lx_err(lx, sp, "进制字面量缺少数字");
        }
        text_end = lx->pos;
    } else {
        while (lx->pos < lx->len && (is_digit_c(peek(lx)) || peek(lx) == '_')) bump(lx);
        // 浮点:'.' 后跟数字才是(1..5 是 range;21.double() 是方法)
        if (peek(lx) == '.' && is_digit_c(peek2(lx))) {
            is_float = 1;
            bump(lx);
            while (lx->pos < lx->len && (is_digit_c(peek(lx)) || peek(lx) == '_')) bump(lx);
        }
        // 指数:e/E [+-] 数字
        unsigned char c = peek(lx);
        if (c == 'e' || c == 'E') {
            int sign = (peek2(lx) == '+' || peek2(lx) == '-');
            int digit_after = sign ? is_digit_c(peek3(lx)) : is_digit_c(peek2(lx));
            if (digit_after) {
                is_float = 1;
                bump(lx);
                if (sign) bump(lx);
                while (lx->pos < lx->len && (is_digit_c(peek(lx)) || peek(lx) == '_')) bump(lx);
            }
        }
        text_end = lx->pos;
    }
    char* text = ctron_arena_strndup(lx->arena, lx->src + start, text_end - start);
    // 后缀(小写精确匹配;后缀后不得紧跟标识符字符:`255U8` 回落为 Int+Ident)
    static const struct { const char* s; ctron_suffix sfx; } SUFS[] = {
        {"i8", SUF_I8}, {"i16", SUF_I16}, {"i32", SUF_I32}, {"i64", SUF_I64},
        {"isize", SUF_ISIZE}, {"u8", SUF_U8}, {"u16", SUF_U16}, {"u32", SUF_U32},
        {"u64", SUF_U64}, {"usize", SUF_USIZE}, {"f32", SUF_F32}, {"f64", SUF_F64},
    };
    ctron_suffix sfx = SUF_NONE;
    for (size_t i = 0; i < sizeof SUFS / sizeof SUFS[0]; i++) {
        size_t n = strlen(SUFS[i].s);
        if (lx->pos + n <= lx->len && memcmp(lx->src + lx->pos, SUFS[i].s, n) == 0) {
            unsigned char after = lx->pos + n < lx->len ? (unsigned char)lx->src[lx->pos + n] : 0;
            if (!(isalnum(after) || after == '_')) {
                for (size_t j = 0; j < n; j++) bump(lx);
                sfx = SUFS[i].sfx;
                break;
            }
        }
    }
    out->kind = is_float ? TOK_FLOAT : TOK_INT;
    out->suffix = sfx;
    out->text = text;
    out->parts.items = NULL;
    out->parts.len = 0;
    out->span = mark(lx, start, line, col);
    return 1;
}

// ---------- Unicode 标量 → UTF-8 ----------

static void utf8_encode(unsigned int cp, bvec* out) {
    if (cp < 0x80) { bvec_push(out, (unsigned char)cp); }
    else if (cp < 0x800) {
        bvec_push(out, (unsigned char)(0xC0 | (cp >> 6)));
        bvec_push(out, (unsigned char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        bvec_push(out, (unsigned char)(0xE0 | (cp >> 12)));
        bvec_push(out, (unsigned char)(0x80 | ((cp >> 6) & 0x3F)));
        bvec_push(out, (unsigned char)(0x80 | (cp & 0x3F)));
    } else {
        bvec_push(out, (unsigned char)(0xF0 | (cp >> 18)));
        bvec_push(out, (unsigned char)(0x80 | ((cp >> 12) & 0x3F)));
        bvec_push(out, (unsigned char)(0x80 | ((cp >> 6) & 0x3F)));
        bvec_push(out, (unsigned char)(0x80 | (cp & 0x3F)));
    }
}

/// 合法标量:≤ U+10FFFF 且不在代理区。
static int valid_scalar(unsigned int cp) {
    return cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF);
}

// ---------- 字符串字面量 ----------

typedef struct { ctron_str_part* d; size_t n, cap; } pvec;

static void pvec_push(pvec* v, ctron_str_part p) {
    if (v->n == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 8;
        v->d = (ctron_str_part*)realloc(v->d, v->cap * sizeof(*v->d));
        if (!v->d) abort();
    }
    v->d[v->n++] = p;
}

/// 把 bvec 当前内容落为 Text 部件并清空缓冲。
static void flush_text(lexer* lx, pvec* parts, bvec* bytes) {
    if (bytes->n == 0) return;
    ctron_str_part p;
    p.kind = PART_TEXT;
    p.s = ctron_arena_strndup(lx->arena, bytes->d, bytes->n);
    pvec_push(parts, p);
    bvec_reset(bytes);
}

static int lex_string(lexer* lx, size_t start, uint32_t line, uint32_t col, ctron_token* out,
                      bvec* bytes) {
    bump(lx); // 开引号
    pvec parts = {0};
    int done = 0;
    for (;;) {
        unsigned char c = peek(lx);
        if (lx->pos >= lx->len || c == '\n') {
            flush_text(lx, &parts, bytes);
            ctron_span sp = mark(lx, start, line, col);
            lx_err(lx, sp, "未终止的字符串(不允许跨行)");
            done = 1;
            break;
        }
        if (c == '"') {
            bump(lx);
            flush_text(lx, &parts, bytes);
            done = 1;
            break;
        }
        if (c == '\\') {
            bump(lx);
            if (lx->pos >= lx->len) {
                flush_text(lx, &parts, bytes);
                ctron_span sp = mark(lx, start, line, col);
                lx_err(lx, sp, "未终止的字符串(不允许跨行)");
                done = 1;
                break;
            }
            unsigned char esc = bump(lx);
            switch (esc) {
            case 'n': bvec_push(bytes, '\n'); break;
            case 't': bvec_push(bytes, '\t'); break;
            case 'r': bvec_push(bytes, '\r'); break;
            case '0': bvec_push(bytes, 0); break;
            case '\\': bvec_push(bytes, '\\'); break;
            case '"': bvec_push(bytes, '"'); break;
            case '{': bvec_push(bytes, '{'); break;
            case 'u': {
                if (peek(lx) == '{') {
                    bump(lx);
                    unsigned int cp = 0;
                    int overflow = 0, ndig = 0;
                    while (isxdigit(peek(lx))) {
                        unsigned char h = (unsigned char)tolower(bump(lx));
                        int v = isdigit(h) ? h - '0' : h - 'a' + 10;
                        if (cp > (0x10FFFFu - (unsigned)v) / 16u) overflow = 1;
                        cp = cp * 16u + (unsigned)v;
                        ndig++;
                    }
                    if (peek(lx) == '}' && ndig > 0) {
                        bump(lx);
                        if (overflow || !valid_scalar(cp)) {
                            ctron_span sp = mark(lx, start, line, col);
                            lx_err(lx, sp, "非法的 Unicode 转义");
                        } else {
                            utf8_encode(cp, bytes);
                        }
                    } else {
                        ctron_span sp = mark(lx, start, line, col);
                        lx_err(lx, sp, "\\u 转义缺少 {HEX}");
                    }
                } else {
                    ctron_span sp = mark(lx, start, line, col);
                    lx_err(lx, sp, "\\u 转义缺少 {");
                }
                break;
            }
            default: {
                ctron_span sp = mark(lx, start, line, col);
                lx_err(lx, sp, "非法转义 \\%c", esc);
                break;
            }
            }
            continue;
        }
        if (c == '{') {
            // 插值:同一行内须有配对 '}'(嵌套深度计数);字面 '{' 必须写 `\{`。
            // 行内无配对 '}' → E1001"未终止的插值";遇 '"' 视为未终止(不跨串配对)。
            flush_text(lx, &parts, bytes);
            bump(lx); // {
            size_t expr_start = lx->pos;
            int depth = 1, terminated = 0;
            while (depth > 0) {
                if (lx->pos >= lx->len || peek(lx) == '\n') { depth = 0; }
                else if (peek(lx) == '"') {
                    bump(lx); // 消耗它作为本串截断收尾引号
                    depth = 0;
                }
                else if (peek(lx) == '{') { depth++; bump(lx); }
                else if (peek(lx) == '}') {
                    depth--;
                    if (depth > 0) bump(lx); else terminated = 1;
                }
                else bump(lx);
            }
            if (terminated) {
                ctron_str_part p;
                p.kind = PART_INTERP;
                p.s = ctron_arena_strndup(lx->arena, lx->src + expr_start, lx->pos - expr_start);
                pvec_push(&parts, p);
                bump(lx); // 收尾 '}'
            } else {
                ctron_span sp = mark(lx, start, line, col);
                lx_err(lx, sp, "未终止的插值");
                done = 1;
                break;
            }
            continue;
        }
        // 多字节 UTF-8 逐字节入缓冲,不校验(源保证 UTF-8;§1.1)
        bvec_push(bytes, bump(lx));
    }
    // 落盘到 arena
    if (parts.n > 0) {
        ctron_str_part* arr = (ctron_str_part*)ctron_arena_alloc(lx->arena, parts.n * sizeof(ctron_str_part));
        memcpy(arr, parts.d, parts.n * sizeof(ctron_str_part));
        out->parts.items = arr;
    } else {
        out->parts.items = NULL;
    }
    out->parts.len = parts.n;
    free(parts.d);
    (void)done;
    out->kind = TOK_STR;
    out->suffix = SUF_NONE;
    out->text = NULL;
    out->span = mark(lx, start, line, col);
    return 1;
}

// ---------- 标点与运算符(最长匹配;§1.5) ----------

/// 返回 1 = 产出实记号;0 = 错误记号已吞(哨兵),由主循环续取。
static int lex_punct(lexer* lx, size_t start, uint32_t line, uint32_t col, ctron_token* out) {
    size_t len = lx->len;
    unsigned char a0 = peek(lx), b0 = peek2(lx), c0 = peek3(lx);
    ctron_tok_kind k;
    int ntake = 1;
#define TWO(x, y) (a0 == x && b0 == y)
#define THREE(x, y, z) (a0 == x && b0 == y && c0 == z)
    if (TWO('+', '%')) { k = TOK_WRAP_PLUS; ntake = 2; }
    else if (TWO('-', '%')) { k = TOK_WRAP_MINUS; ntake = 2; }
    else if (TWO('+', '=')) { k = TOK_PLUS_EQ; ntake = 2; }
    else if (TWO('-', '=')) { k = TOK_MINUS_EQ; ntake = 2; }
    else if (TWO('*', '=')) { k = TOK_STAR_EQ; ntake = 2; }
    else if (TWO('/', '=')) { k = TOK_SLASH_EQ; ntake = 2; }
    else if (TWO('%', '=')) { k = TOK_PERCENT_EQ; ntake = 2; }
    else if (TWO('=', '=')) { k = TOK_EQ_EQ; ntake = 2; }
    else if (TWO('!', '=')) { k = TOK_NOT_EQ; ntake = 2; }
    else if (TWO('<', '=')) { k = TOK_LT_EQ; ntake = 2; }
    else if (TWO('>', '=')) { k = TOK_GT_EQ; ntake = 2; }
    else if (TWO('&', '&')) { k = TOK_AND_AND; ntake = 2; }
    else if (THREE('.', '.', '=')) { k = TOK_DOT_DOT_EQ; ntake = 3; } // 最长匹配 ..=
    else if (TWO('.', '.')) { k = TOK_DOT_DOT; ntake = 2; }
    else if (TWO('-', '>')) { k = TOK_ARROW; ntake = 2; }
    else if (TWO('=', '>')) { k = TOK_FAT_ARROW; ntake = 2; }
    else if (TWO(':', ':')) {
        for (int i = 0; i < 2; i++) bump(lx);
        ctron_span sp = mark(lx, start, line, col);
        lx_err(lx, sp, "禁用的标点 ::");
        k = TOK_COLON; // 恢复为单个冒号继续
        ntake = 0;     // 已消费
        goto emit;
    } else {
        unsigned char c = a0;
        switch (c) {
        case '+': k = TOK_PLUS; break;
        case '-': k = TOK_MINUS; break;
        case '*': k = TOK_STAR; break;
        case '/': k = TOK_SLASH; break;
        case '%': k = TOK_PERCENT; break;
        case '=': k = TOK_ASSIGN; break;
        case '<': k = TOK_LT; break;
        case '>': k = TOK_GT; break;
        case '!': k = TOK_BANG; break;
        case '?': k = TOK_QUESTION; break;
        case '.': k = TOK_DOT; break;
        case ',': k = TOK_COMMA; break;
        case ':': k = TOK_COLON; break;
        case '[': k = TOK_LBRACKET; break;
        case ']': k = TOK_RBRACKET; break;
        case '(': k = TOK_LPAREN; break;
        case ')': k = TOK_RPAREN; break;
        case '{': k = TOK_LBRACE; break;
        case '}': k = TOK_RBRACE; break;
        case '|': k = TOK_PIPE; break;
        case '&': k = TOK_AMP; break;
        case '#': k = TOK_HASH; break;
        case '@': k = TOK_AT; break;
        case ';': {
            bump(lx);
            ctron_span sp = mark(lx, start, line, col);
            lx_err(lx, sp, "禁用的标点 ;");
            return 0; // 丢弃该记号:主循环续取
        }
        default: {
            bump(lx);
            ctron_span sp = mark(lx, start, line, col);
            lx_err(lx, sp, "无法识别的字符 '%c'", c);
            return 0;
        }
        }
        ntake = 1; // 未在分支内 bump,由下方统一消费
    }
    (void)len;
    for (int i = 0; i < ntake; i++) bump(lx);
emit:
    out->kind = k;
    out->suffix = SUF_NONE;
    out->text = NULL;
    out->parts.items = NULL;
    out->parts.len = 0;
    out->span = mark(lx, start, line, col);
    return 1;
#undef TWO
#undef THREE
}

/// 单记号扫描:1 = 实记号;0 = 错误记号已吞(哨兵,不更新 prev_is_dot)。
static int lexer_next(lexer* lx, ctron_token* out, bvec* bytes) {
    skip_inline_trivia(lx);
    size_t start = lx->pos;
    uint32_t line = lx->line, col = lx->col;
    unsigned char c = peek(lx);
    int real = 0;
    if (lx->pos >= lx->len) {
        out->kind = TOK_EOF; out->suffix = SUF_NONE; out->text = NULL;
        out->parts.items = NULL; out->parts.len = 0;
        out->span = mark(lx, start, line, col);
        real = 1;
    } else if (c == '\n') {
        bump(lx);
        out->kind = TOK_NEWLINE; out->suffix = SUF_NONE; out->text = NULL;
        out->parts.items = NULL; out->parts.len = 0;
        out->span = mark(lx, start, line, col);
        real = 1;
    } else if (is_ident_start(c)) {
        real = lex_name(lx, start, line, col, out);
    } else if (is_digit_c(c)) {
        real = lex_number(lx, start, line, col, out);
    } else if (c == '"') {
        real = lex_string(lx, start, line, col, out, bytes);
    } else {
        real = lex_punct(lx, start, line, col, out);
    }
    if (real) lx->prev_is_dot = (out->kind == TOK_DOT);
    return real;
}

// ---------- §1.6 换行显著性过滤 ----------

static int line_end_continues(ctron_tok_kind k) {
    switch (k) {
    case TOK_COMMA: case TOK_ASSIGN: case TOK_ARROW: case TOK_FAT_ARROW:
    case TOK_AND_AND: case TOK_OR: case TOK_DOT_DOT: case TOK_DOT_DOT_EQ:
    case TOK_PLUS: case TOK_MINUS: case TOK_STAR: case TOK_SLASH: case TOK_PERCENT:
    case TOK_WRAP_PLUS: case TOK_WRAP_MINUS: case TOK_EQ_EQ: case TOK_NOT_EQ:
    case TOK_LT: case TOK_GT: case TOK_LT_EQ: case TOK_GT_EQ:
    case TOK_LPAREN: case TOK_LBRACKET: case TOK_LBRACE: case TOK_PIPE:
        return 1;
    default: return 0;
    }
}

static int continues_next_line(ctron_tok_kind k) {
    switch (k) {
    case TOK_DOT: case TOK_PLUS: case TOK_MINUS: case TOK_STAR: case TOK_SLASH:
    case TOK_PERCENT: case TOK_WRAP_PLUS: case TOK_WRAP_MINUS: case TOK_EQ_EQ:
    case TOK_NOT_EQ: case TOK_LT: case TOK_GT: case TOK_LT_EQ: case TOK_GT_EQ:
    case TOK_AND_AND: case TOK_OR: case TOK_DOT_DOT: case TOK_DOT_DOT_EQ:
        return 1;
    default: return 0;
    }
}

/// 行尾在延续集 → 换行无效;下一行以 '.' 或二元运算符开头 → 换行无效;
/// 连续换行折叠为一个;文件起始的前导换行保留 K-1 个。
static ctron_token* filter_newlines(ctron_token* raw, size_t nraw, size_t* nout) {
    ctron_token* out = (ctron_token*)malloc((nraw + 1) * sizeof(ctron_token));
    if (!out) abort();
    size_t o = 0, i = 0;
    size_t lead = 0;
    while (i < nraw && raw[i].kind == TOK_NEWLINE) { lead++; i++; }
    for (size_t j = 1; j < lead; j++) out[o++] = raw[j]; // 保留 K-1 个
    while (i < nraw) {
        if (raw[i].kind != TOK_NEWLINE) {
            out[o++] = raw[i++];
            continue;
        }
        // 收集连续换行,看向其后第一个实记号
        size_t run_end = i;
        while (run_end < nraw && raw[run_end].kind == TOK_NEWLINE) run_end++;
        ctron_tok_kind next = run_end < nraw ? raw[run_end].kind : TOK_EOF;
        // 此处 o > 0:前导换行已特殊处理,首个实记号必已入 out
        int suppressed = line_end_continues(out[o - 1].kind) || continues_next_line(next);
        if (!suppressed) out[o++] = raw[i];
        i = run_end;
    }
    *nout = o;
    free(raw);
    return out;
}

// ---------- 入口 ----------

ctron_lex_result ctron_lex(const char* src, size_t len) {
    lexer lx;
    memset(&lx, 0, sizeof lx);
    lx.src = src;
    lx.len = len;
    lx.line = 1;
    lx.col = 1;
    lx.arena = ctron_arena_new();
    bvec bytes = {0};
    tvec raw = {0};
    for (;;) {
        ctron_token t;
        if (lexer_next(&lx, &t, &bytes)) {
            int eof = (t.kind == TOK_EOF);
            tvec_push(&raw, t);
            if (eof) break;
        }
    }
    free(bytes.d);
    size_t ntoks;
    ctron_token* toks = filter_newlines(raw.d, raw.n, &ntoks);
    ctron_lex_result r;
    r.toks = toks;
    r.ntoks = ntoks;
    r.diags = lx.diags.d;
    r.ndiags = lx.diags.n;
    r._arena = lx.arena;
    return r;
}

void ctron_lex_result_free(ctron_lex_result* r) {
    if (!r) return;
    free(r->toks);
    free(r->diags);
    ctron_arena_free((ctron_arena*)r->_arena);
    r->toks = NULL;
    r->diags = NULL;
    r->ntoks = r->ndiags = 0;
    r->_arena = NULL;
}
