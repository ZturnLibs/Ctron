// test_lex.c —— 词法器单元测试(锚定 §1 行为;C 版独立用例)。
// 失败即打印并计数,退出码非 0。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "lexer.h"
#include "token.h"

static int g_checks = 0, g_fails = 0;

#define CHECK(cond)                                                              \
    do {                                                                        \
        g_checks++;                                                             \
        if (!(cond)) {                                                          \
            g_fails++;                                                          \
            fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__);                \
        }                                                                       \
    } while (0)

#define CHECKM(cond, ...)                                                       \
    do {                                                                        \
        g_checks++;                                                             \
        if (!(cond)) {                                                          \
            g_fails++;                                                          \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);                \
            fprintf(stderr, __VA_ARGS__);                                       \
            fprintf(stderr, "\n");                                              \
        }                                                                       \
    } while (0)

// ---------- 渲染:记号流 → 空格分隔的稳定文本(测试期望串) ----------

typedef struct { char* d; size_t n, cap; } sb;

static void sb_putc(sb* b, char c) {
    if (b->n + 1 >= b->cap) {
        b->cap = b->cap ? b->cap * 2 : 256;
        b->d = (char*)realloc(b->d, b->cap);
        if (!b->d) abort();
    }
    b->d[b->n++] = c;
    b->d[b->n] = '\0';
}
static void sb_puts(sb* b, const char* s) { while (*s) sb_putc(b, *s++); }

static void render_esc(sb* b, const char* s) {
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        unsigned char c = *p;
        if (c == '\n') sb_puts(b, "\\n");
        else if (c == '\t') sb_puts(b, "\\t");
        else if (c == '\r') sb_puts(b, "\\r");
        else if (c == '\\') sb_puts(b, "\\\\");
        else if (c == '"') sb_puts(b, "\\\"");
        else if (c == '\0') sb_puts(b, "\\0");
        else if (c < 0x20) {
            char t[8];
            snprintf(t, sizeof t, "\\x%02X", c);
            sb_puts(b, t);
        }
        else sb_putc(b, (char)c);
    }
}

static void render_tok(sb* b, const ctron_token* t) {
    const char* n = ctron_tok_name(t->kind);
    switch (t->kind) {
    case TOK_INT:
    case TOK_FLOAT:
        sb_puts(b, n);
        sb_putc(b, '(');
        sb_puts(b, t->text);
        sb_putc(b, ',');
        sb_puts(b, ctron_suffix_name(t->suffix));
        sb_putc(b, ')');
        break;
    case TOK_IDENT:
        sb_puts(b, "Ident(");
        sb_puts(b, t->text);
        sb_putc(b, ')');
        break;
    case TOK_STR: {
        sb_puts(b, "Str[");
        for (size_t i = 0; i < t->parts.len; i++) {
            if (i) sb_puts(b, "|");
            if (t->parts.items[i].kind == PART_TEXT) sb_puts(b, "Text(");
            else sb_puts(b, "Interp(");
            render_esc(b, t->parts.items[i].s);
            sb_putc(b, ')');
        }
        sb_putc(b, ']');
        break;
    }
    default:
        sb_puts(b, n);
        break;
    }
}

static ctron_lex_result lex_str(const char* s) { return ctron_lex(s, strlen(s)); }

static void kinds_eq(const char* src, const char* expect) {
    ctron_lex_result r = lex_str(src);
    sb b = {0};
    for (size_t i = 0; i < r.ntoks; i++) {
        if (i) sb_putc(&b, ' ');
        render_tok(&b, &r.toks[i]);
    }
    CHECKM(strcmp(b.d, expect) == 0, "kinds_eq: [%s]\n  actual:   %s\n  expected: %s",
           src, b.d ? b.d : "(nil)", expect);
    free(b.d);
    ctron_lex_result_free(&r);
}

static void diag_eq(const char* src, int ndiags, const char* code, const char* msg) {
    ctron_lex_result r = lex_str(src);
    CHECKM((int)r.ndiags == ndiags, "diag count for [%s]: got %zu want %d", src, r.ndiags, ndiags);
    if ((int)r.ndiags == ndiags && ndiags > 0) {
        if (code) CHECKM(strcmp(r.diags[0].code, code) == 0, "code [%s]: %s", src, r.diags[0].code);
        if (msg) CHECKM(strcmp(r.diags[0].message, msg) == 0,
                        "msg [%s]: got '%s' want '%s'", src, r.diags[0].message, msg);
    }
    ctron_lex_result_free(&r);
}

// ---------- 用例(锚点语义来自 §1 与共享语料验收) ----------

static void test_keywords_and_idents(void) {
    kinds_eq("fn let var struct extern\n",
             "Fn Let Var Struct Extern Newline Eof");
    kinds_eq("order org own", "Ident(order) Ident(org) Own Eof");
    kinds_eq("self selfish", "Self Ident(selfish) Eof");
    kinds_eq("_ _x", "Underscore Ident(_x) Eof");
}

static void test_operators_longest_match(void) {
    kinds_eq("+% += .. ..= -> => == != <= >= &&",
             "WrapPlus PlusEq DotDot DotDotEq Arrow FatArrow EqEq NotEq LtEq GtEq AndAnd Eof");
    kinds_eq("+ = < . , : # @ | & ! ?",
             "Plus Assign Lt Dot Comma Colon Hash At Pipe Amp Bang Question Eof");
}

static void test_comments_and_markers_are_trivia(void) {
    kinds_eq("// 普通注释\n//@ fail: E2030\n/// doc\nlet",
             "Newline Newline Let Eof");
}

static void test_banned_punctuation_is_e1001(void) {
    diag_eq("let a = 1;", 1, "E1001", "禁用的标点 ;");
    diag_eq("a :: b", 1, "E1001", "禁用的标点 ::");
}

static void test_hundred_k_semicolons_no_stack_overflow(void) {
    size_t n = 100000;
    char* src = (char*)malloc(n + 1);
    for (size_t i = 0; i < n; i++) src[i] = ';';
    src[n] = '\0';
    ctron_lex_result r = ctron_lex(src, n);
    CHECKM(r.ndiags == n, "diags: %zu want %zu", r.ndiags, n);
    CHECKM(r.ntoks == 1 && r.toks[0].kind == TOK_EOF, "toks: %zu", r.ntoks);
    ctron_lex_result_free(&r);
    free(src);
}

static void test_number_radix_underscores_and_suffixes(void) {
    kinds_eq("255u8 0xFF 0o17 0b1010 1_000_000 5usize",
             "Int(255,u8) Int(0xFF,none) Int(0o17,none) Int(0b1010,none) "
             "Int(1_000_000,none) Int(5,usize) Eof");
}

static void test_float_vs_range_disambiguation(void) {
    kinds_eq("1..5 0..=n 2.5 2.5f32 1e3",
             "Int(1,none) DotDot Int(5,none) Int(0,none) DotDotEq Ident(n) "
             "Float(2.5,none) Float(2.5,f32) Float(1e3,none) Eof");
}

static void test_method_call_on_int_literal_is_not_float(void) {
    kinds_eq("21.double()", "Int(21,none) Dot Ident(double) LParen RParen Eof");
}

static void test_string_escapes(void) {
    kinds_eq("\"a\\nb\" \"\\{\" \"\\u{4E2D}\" \"\\\\\" \"\\\"\"",
             "Str[Text(a\\nb)] Str[Text({)] Str[Text(\xE4\xB8\xAD)] Str[Text(\\\\)] Str[Text(\\\")] Eof");
}

static void test_string_interpolation_raw_parts(void) {
    kinds_eq("\"hi {name}\" \"len={xs.len} first={xs[0]}\" \"v={opt.or(0)}\"",
             "Str[Text(hi )|Interp(name)] "
             "Str[Text(len=)|Interp(xs.len)|Text( first=)|Interp(xs[0])] "
             "Str[Text(v=)|Interp(opt.or(0))] Eof");
}

static void test_bare_brace_without_closing_reports_e1001(void) {
    diag_eq("\"{\"", 1, "E1001", "未终止的插值");
    diag_eq("\"a{b\"", 1, "E1001", "未终止的插值");
    kinds_eq("\"len={xs.len}\"",
             "Str[Text(len=)|Interp(xs.len)] Eof");
}

static void test_unterminated_string_reports_e1001(void) {
    diag_eq("\"abc", 1, "E1001", "未终止的字符串(不允许跨行)");
    diag_eq("\"a\\q\"", 1, "E1001", "非法转义 \\q");
}

static void test_newline_rules_1_6(void) {
    kinds_eq("opt\n    .map(f)\n    .or(0)",
             "Ident(opt) Dot Ident(map) LParen Ident(f) RParen "
             "Dot Ident(or) LParen Int(0,none) RParen Eof");
    kinds_eq("let x = 1 +\n    2",
             "Let Ident(x) Assign Int(1,none) Plus Int(2,none) Eof");
    kinds_eq("let a = 1\n\n\nlet b = 2",
             "Let Ident(a) Assign Int(1,none) Newline "
             "Let Ident(b) Assign Int(2,none) Eof");
    kinds_eq("} else {", "RBrace Else LBrace Eof");
}

static void test_backslash_eof_reports_e1001(void) {
    diag_eq("\"a\\", 1, "E1001", "未终止的字符串(不允许跨行)");
}

static void test_numeric_suffix_is_case_sensitive(void) {
    kinds_eq("255U8", "Int(255,none) Ident(U8) Eof");
    diag_eq("255U8", 0, NULL, NULL);
    kinds_eq("255u8", "Int(255,u8) Eof");
}

static void test_uppercase_radix_prefix_is_not_recognized(void) {
    kinds_eq("0XFF", "Int(0,none) Ident(XFF) Eof");
    kinds_eq("0xFF 0o17 0b1010",
             "Int(0xFF,none) Int(0o17,none) Int(0b1010,none) Eof");
}

static void test_unicode_escape_overflow_reports_e1001(void) {
    diag_eq("\"\\u{FFFFFFFFF}\"", 1, "E1001", "非法的 Unicode 转义");
}

static void test_interpolation_does_not_pair_across_strings(void) {
    ctron_lex_result r = lex_str("f(\"x{y\",\"z}\")");
    CHECKM(r.ndiags == 1, "ndiags %zu", r.ndiags);
    CHECKM(r.ndiags == 1 && strcmp(r.diags[0].message, "未终止的插值") == 0,
            "msg: %s", r.diags[0].message);
    CHECKM(r.ntoks == 7, "ntoks %zu", r.ntoks);
    if (r.ntoks == 7) {
        CHECK(r.toks[0].kind == TOK_IDENT);
        CHECK(r.toks[1].kind == TOK_LPAREN);
        CHECK(r.toks[2].kind == TOK_STR && r.toks[2].parts.len == 1 &&
              r.toks[2].parts.items[0].kind == PART_TEXT &&
              strcmp(r.toks[2].parts.items[0].s, "x") == 0);
        CHECK(r.toks[3].kind == TOK_COMMA);
        CHECK(r.toks[4].kind == TOK_STR && r.toks[4].parts.len == 1 &&
              r.toks[4].parts.items[0].kind == PART_TEXT &&
              strcmp(r.toks[4].parts.items[0].s, "z}") == 0);
        CHECK(r.toks[5].kind == TOK_RPAREN);
        CHECK(r.toks[6].kind == TOK_EOF);
    }
    ctron_lex_result_free(&r);
}

typedef void (*tfn)(void);
static const tfn TESTS[] = {
    test_keywords_and_idents,
    test_operators_longest_match,
    test_comments_and_markers_are_trivia,
    test_banned_punctuation_is_e1001,
    test_hundred_k_semicolons_no_stack_overflow,
    test_number_radix_underscores_and_suffixes,
    test_float_vs_range_disambiguation,
    test_method_call_on_int_literal_is_not_float,
    test_string_escapes,
    test_string_interpolation_raw_parts,
    test_bare_brace_without_closing_reports_e1001,
    test_unterminated_string_reports_e1001,
    test_newline_rules_1_6,
    test_backslash_eof_reports_e1001,
    test_numeric_suffix_is_case_sensitive,
    test_uppercase_radix_prefix_is_not_recognized,
    test_unicode_escape_overflow_reports_e1001,
    test_interpolation_does_not_pair_across_strings,
};

int main(void) {
    for (size_t i = 0; i < sizeof TESTS / sizeof TESTS[0]; i++) TESTS[i]();
    printf("test_lex: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
