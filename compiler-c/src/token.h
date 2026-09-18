// token.h —— 记号模型(规范 §1.3 关键字/§1.4 字面量/§1.5 运算符与标点)。
// C 版独立实现,语义与 docs/spec v0.5 对齐。
#ifndef CTRON_TOKEN_H
#define CTRON_TOKEN_H

#include <stddef.h>
#include <stdint.h>

/// 数值后缀(区分大小写、仅小写;§1.4)
typedef enum {
    SUF_NONE = 0,
    SUF_I8, SUF_I16, SUF_I32, SUF_I64, SUF_ISIZE,
    SUF_U8, SUF_U16, SUF_U32, SUF_U64, SUF_USIZE,
    SUF_F32, SUF_F64,
} ctron_suffix;

/// 字符串部件(§1.4):已解码文本 或 插值原始源文本(解析期再子解析)
typedef enum { PART_TEXT = 0, PART_INTERP } ctron_part_kind;

typedef struct {
    ctron_part_kind kind;
    const char* s; // arena, NUL 结尾
} ctron_str_part;

typedef struct {
    ctron_str_part* items; // arena 数组;可为空
    size_t len;
} ctron_str_parts;

typedef struct {
    uint32_t line; // 1-based
    uint32_t col;  // 1-based
    size_t start;  // 字节偏移
    size_t end;
} ctron_span;

typedef enum {
    // 字面量
    TOK_INT, TOK_FLOAT, TOK_STR,
    // 名字与关键字(§1.3 唯一权威清单)
    TOK_IDENT,
    TOK_FN, TOK_LET, TOK_VAR, TOK_CONST, TOK_STATIC, TOK_COMPTIME,
    TOK_IF, TOK_ELSE, TOK_MATCH, TOK_WHILE, TOK_FOR, TOK_IN, TOK_RETURN,
    TOK_BREAK, TOK_CONTINUE,
    TOK_STRUCT, TOK_CLASS, TOK_ENUM, TOK_TRAIT, TOK_IMPL, TOK_OWN, TOK_SCOPE,
    TOK_TEST, TOK_USE, TOK_PUB, TOK_EXTERN, TOK_PROP,
    TOK_TRUE, TOK_FALSE, TOK_VOID, TOK_SELF,
    // 运算符与标点(§1.5)
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_WRAP_PLUS, TOK_WRAP_MINUS,
    TOK_PLUS_EQ, TOK_MINUS_EQ, TOK_STAR_EQ, TOK_SLASH_EQ, TOK_PERCENT_EQ,
    TOK_EQ_EQ, TOK_NOT_EQ, TOK_LT, TOK_GT, TOK_LT_EQ, TOK_GT_EQ, TOK_ASSIGN,
    TOK_AND_AND, TOK_OR_OR, TOK_OR, // or = 取默认中缀(保留运算符字);OR_OR = 逻辑或(v0.7)
    TOK_DOT_DOT, TOK_DOT_DOT_EQ, TOK_ELLIPSIS, TOK_ARROW, TOK_FAT_ARROW, TOK_QUESTION,
    TOK_DOT, TOK_COMMA, TOK_COLON,
    TOK_LBRACKET, TOK_RBRACKET, TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE, TOK_PIPE, TOK_AMP,
    TOK_HASH, TOK_AT, TOK_UNDERSCORE, TOK_BANG,
    // 结构
    TOK_NEWLINE, TOK_EOF,
    TOK_KIND_COUNT
} ctron_tok_kind;

typedef struct {
    ctron_tok_kind kind;
    ctron_suffix suffix;   // TOK_INT / TOK_FLOAT
    const char* text;      // TOK_INT/TOK_FLOAT/TOK_IDENT 的原文(arena)
    ctron_str_parts parts; // TOK_STR
    ctron_span span;
} ctron_token;

typedef struct {
    const char* code;    // "E1001"(静态)
    const char* message; // arena
    ctron_span span;
} ctron_diag;

const char* ctron_tok_name(ctron_tok_kind k);
const char* ctron_suffix_name(ctron_suffix s);

#endif
