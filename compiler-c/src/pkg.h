// pkg.h —— C3-c 模块级检查(按包目录;含 Ctron.ctcl)与 CTCL 单文件检查。
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

// —— CTCL 单文件检查(manifest 子命令;三线黄金对拍 §9)——
typedef struct {
    char* name;
    char* version;
    char** caps;
    size_t ncaps;
    int has_comptime;
    int budget_ok;
    long budget_ms;
    pkg_res diags;
} ctron_manifest;

ctron_manifest ctron_manifest_check(const char* ctcl_path);
void ctron_manifest_free(ctron_manifest* m);
void ctron_pkg_res_free(pkg_res* r);

#endif
