// lexer.h —— 词法入口。规范 §1;换行显著性过滤见 §1.6。
#ifndef CTRON_LEXER_H
#define CTRON_LEXER_H

#include <stddef.h>
#include "token.h"

typedef struct {
    ctron_token* toks;  // 堆数组;随 ctron_lex_result_free 释放
    size_t ntoks;
    ctron_diag* diags;  // 堆数组;同上
    size_t ndiags;
    void* _arena;       // 内部:记号载荷字符串的 arena,由 free 统一释放
} ctron_lex_result;

/// 词法整个源文件(src 前 len 字节;源需为 UTF-8)。结果的生命周期到 free 为止。
ctron_lex_result ctron_lex(const char* src, size_t len);
void ctron_lex_result_free(ctron_lex_result* r);

#endif
