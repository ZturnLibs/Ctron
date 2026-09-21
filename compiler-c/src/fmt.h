// fmt.h —— ctron fmt(R-P2d 双宿主移植):token 流重排,不动 AST。
// 规范唯一权威:docs/fmt-spec.md;参考实现:compiler-rust/src/fmt.rs。
// 词法复用 ctron_lex(过滤流 + 字节 span,与 Rust crate::lex 同构);
// 词法诊断非空时 out=NULL、ndiags>0(fmt 只服务合法语法面,规范 R8)。
#ifndef CTRON_FMT_H
#define CTRON_FMT_H

#include <stddef.h>

typedef struct {
    unsigned line, col; // 1-based
    char* code;         // strdup("E1001")
    char* message;      // strdup
} ctron_fmt_diag;

typedef struct {
    char* out;               // 规范格式输出(malloc,NUL 结尾);诊断非空为 NULL
    ctron_fmt_diag* diags;   // 词法诊断(深拷贝)
    size_t ndiags;
} ctron_fmt_result;

ctron_fmt_result ctron_fmt_src(const char* src, size_t len);
void ctron_fmt_result_free(ctron_fmt_result* r);

#endif
