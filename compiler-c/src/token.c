// token.c —— 记号种类的稳定名(调试/测试渲染用)。
#include "token.h"

static const char* const TOK_NAMES[TOK_KIND_COUNT] = {
    "Int", "Float", "Str",
    "Ident",
    "Fn", "Let", "Var", "Const", "Static", "Comptime",
    "If", "Else", "Match", "While", "For", "In", "Return", "Break", "Continue",
    "Struct", "Class", "Enum", "Trait", "Impl", "Own", "Scope",
    "Test", "Use", "Pub", "Extern", "Prop",
    "True", "False", "Void", "Self",
    "Plus", "Minus", "Star", "Slash", "Percent",
    "WrapPlus", "WrapMinus",
    "PlusEq", "MinusEq", "StarEq", "SlashEq", "PercentEq",
    "EqEq", "NotEq", "Lt", "Gt", "LtEq", "GtEq", "Assign",
    "AndAnd", "OrOr", "Or",
    "DotDot", "DotDotEq", "Ellipsis", "Arrow", "FatArrow", "Question",
    "Dot", "Comma", "Colon",
    "LBracket", "RBracket", "LParen", "RParen",
    "LBrace", "RBrace", "Pipe", "Amp",
    "Hash", "At", "Underscore", "Bang",
    "Newline", "Eof",
};

const char* ctron_tok_name(ctron_tok_kind k) {
    if ((unsigned)k < TOK_KIND_COUNT) return TOK_NAMES[k];
    return "?";
}

static const char* const SUF_NAMES[] = {
    "none", "i8", "i16", "i32", "i64", "isize",
    "u8", "u16", "u32", "u64", "usize", "f32", "f64",
};

const char* ctron_suffix_name(ctron_suffix s) {
    if ((unsigned)s <= SUF_F64) return SUF_NAMES[s];
    return "?";
}
