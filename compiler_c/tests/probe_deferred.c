// probe_deferred.c —— 临时探针:强制运行 deferred 语料,输出每文件状态。
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "rt.h"

static const char* const FILES[] = {
    "04_generics_comptime.ct", "06f_parallel.ct", "08_bare.ct",
    "09_simd.ct", "10_trace.ct", "10_web_dom.ct",
};

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

int main(int argc, char** argv) {
    const char* root = argc > 1 ? argv[1] : "../tests";
    for (size_t i = 0; i < sizeof FILES / sizeof FILES[0]; i++) {
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", root, FILES[i]);
        char* src = read_file_str(path);
        if (!src) { printf("%-28s READ-FAIL\n", FILES[i]); continue; }
        ctron_parse_result pr = ctron_parse_src(src, strlen(src));
        free(src);
        if (pr.ndiags) { printf("%-28s PARSE-DIAG %zu\n", FILES[i], pr.ndiags); ctron_parse_result_free(&pr); continue; }
        rt_run rr = ctron_rt_run(pr.file);
        printf("%-28s st=%d run=%zu/%zu msg=%s\n", FILES[i], (int)rr.st, rr.tests_run, rr.tests_total,
               rr.msg ? rr.msg : "");
        ctron_rt_run_free(&rr);
        ctron_parse_result_free(&pr);
    }
    return 0;
}
