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

#include "parser_internal.h"

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
