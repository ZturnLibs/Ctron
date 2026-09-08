// pkg.h —— C3-c 模块级检查(按包目录;含 Ctron.toml)。
#ifndef CTRON_PKG_H
#define CTRON_PKG_H

#include <stddef.h>

typedef struct {
    char* rel;   // 所属源文件,如 "src/main.ct"
    char* code;  // E5010/E5020/E2020/E4010/E6010
    char* msg;   // 含语料要求的英文子串
} pkg_diag;

typedef struct {
    pkg_diag* d;
    size_t n;
} pkg_res;

/// 对包目录(含 Ctron.toml)做模块级检查;结果为堆字符串,调用 ctron_pkg_res_free。
pkg_res ctron_pkg_check(const char* root);
void ctron_pkg_res_free(pkg_res* r);

#endif
