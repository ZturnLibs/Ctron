// parser.h —— C 版解析入口(§1.7 全语法)。诊断码:E1001 / E3030。
#ifndef CTRON_PARSER_H
#define CTRON_PARSER_H

#include <stddef.h>
#include <stdio.h>
#include "ast.h"
#include "token.h"

typedef struct {
    cfile* file;
    ctron_diag* diags; // 堆数组;随 ctron_parse_result_free 释放
    size_t ndiags;
    void* _arena;      // 内部 AST arena
} ctron_parse_result;

/// 词法 + 解析一步到位;词法诊断与解析诊断按序合并。
ctron_parse_result ctron_parse_src(const char* src, size_t len);
void ctron_parse_result_free(ctron_parse_result* r);

/// 把 AST 打印为确定性文本(Debug 形态;自举差分产物契约由 C 版自定)。
void ctron_file_show(const cfile* f, FILE* out);

#endif
