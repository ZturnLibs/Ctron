// suite_rt.c —— C4 执行验收:对“已支持”行为/panic 语料逐文件运行 test 块。
// C4-i 起 34 个可运行语料全量纳入(允许表 = 全集);新增域收敛时在此登记。
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "rt.h"

typedef struct { const char* name; int panic; char panic_msg[64]; } expect;

static const char* const SUPPORTED[] = {
    "01_basics.ct",
    "01_overflow.panic.ct",
    "02d_divzero.panic.ct",
    "03b_numeric_widths.ct",
    "04b_logic.ct",
    "01d_strings.ct",
    "01e_multiline_chain.ct",
    "02b_option_propagation.ct",
    "02e_match_patterns.ct",
    "03_values_refs.ct",
    "03c_str_string.ct",
    "02_option_result.ct",
    "03d_props_traits.ct",
    "05_own.ct",
    "05g_into_gc_isolation.ct",
    "05i_deep_cause.ct",
    "06g_noalloc_trait.ct",
    "05d_drop.ct",
    "05b_panic_join.ct",
    "06_concurrency.ct",
    "06e_cancel.ct",
    "03e_generics_types.ct",
    "06d_globals.ct",
    "07_capabilities.ct",
    "00_doctest.ct",
    "03f_slices.ct",
    "03g_fn_types.ct",
    "03h_utf8_boundary.panic.ct",
    // C4-i deferred 域收敛
    "04_generics_comptime.ct",
    "06f_parallel.ct",
    "08_bare.ct",
    "09_simd.ct",
    "10_trace.ct",
    "10_web_dom.ct",
};

static int supported(const char* n) {
    for (size_t i = 0; i < sizeof SUPPORTED / sizeof SUPPORTED[0]; i++)
        if (strcmp(SUPPORTED[i], n) == 0) return 1;
    return 0;
}

static char* read_file_str(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

// //@ panic: <子串>
static void panic_substr(const char* src, char out[64]) {
    out[0] = 0;
    const char* p = src;
    while ((p = strstr(p, "//@")) != NULL) {
        p += 3;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "panic:", 6) == 0) {
            p += 6;
            while (*p == ' ' || *p == '\t') p++;
            const char* v = p;
            while (*p && *p != '\n') p++;
            size_t n = (size_t)(p - v);
            if (n > 63) n = 63;
            memcpy(out, v, n);
            out[n] = 0;
            return;
        }
    }
}

int main(int argc, char** argv) {
    const char* root = argc > 1 ? argv[1] : "../tests";
    DIR* d = opendir(root);
    if (!d) { fprintf(stderr, "suite_rt: 无法打开 %s\n", root); return 2; }
    size_t run = 0, fails = 0, deferred = 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        size_t bl = strlen(e->d_name);
        if (bl < 4 || strcmp(e->d_name + bl - 3, ".ct") != 0) continue;
        if (strstr(e->d_name, ".neg.ct") || strstr(e->d_name, ".lint.ct")) continue; // 语义层已覆盖
        if (!supported(e->d_name)) { deferred++; continue; }
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", root, e->d_name);
        char* src = read_file_str(path);
        if (!src) { fprintf(stderr, "无法读取 %s\n", path); fails++; continue; }
        char pm[64];
        panic_substr(src, pm);
        int is_panic = pm[0] != 0;
        size_t len = strlen(src);
        ctron_parse_result pr = ctron_parse_src(src, len);
        free(src);
        if (pr.ndiags) {
            fprintf(stderr, "%s: 解析诊断 %zu 条(应归 C2/C3 检查)\n", e->d_name, pr.ndiags);
            ctron_parse_result_free(&pr);
            fails++;
            continue;
        }
        rt_run rr = ctron_rt_run(pr.file);
        int ok = 0;
        if (is_panic) {
            ok = rr.st == RT_PANIC && strstr(rr.msg, pm) != NULL;
            if (!ok)
                fprintf(stderr, "%s: 期望 panic '%s',实得 [%d] %s\n", e->d_name, pm, rr.st,
                        rr.msg ? rr.msg : "");
        } else {
            ok = rr.st == RT_OK && rr.tests_run == rr.tests_total && rr.tests_total > 0;
            if (!ok)
                fprintf(stderr, "%s: 期望全部 %zu 测试通过,实得 [%d] 通过 %zu/ %zu, %s\n",
                        e->d_name, rr.tests_total, rr.st, rr.tests_run, rr.tests_total,
                        rr.msg ? rr.msg : "");
        }
        if (!ok) fails++;
        else run++;
        ctron_rt_run_free(&rr);
        ctron_parse_result_free(&pr);
    }
    closedir(d);
    printf("suite_rt: %zu files run(pass), %zu failures, %zu deferred(未支持)\n", run, fails, deferred);
    if (deferred > 0)
        fprintf(stderr, "suite_rt: 警告 —— %zu 个文件仍未纳入允许表\n", deferred);
    return fails ? 1 : 0;
}
