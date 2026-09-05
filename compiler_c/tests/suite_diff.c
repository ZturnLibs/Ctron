// suite_diff.c —— C5c 差分种子:Ctron 词法器输出 vs C 版 lexer(逐字/计数)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "rt.h"

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
    buf[got] = 0;
    return buf;
}

static int run_ctron_count(const char* module, long* out_count, int* ok) {
    char* src = read_file_str(module);
    if (!src) { *ok = 0; return 1; }
    ctron_parse_result pr = ctron_parse_src(src, strlen(src));
    free(src);
    if (pr.ndiags) { ctron_parse_result_free(&pr); *ok = 0; return 1; }
    rt_run rr = ctron_rt_run_main(pr.file);
    *ok = rr.st == RT_OK && rr.exit_code == 0 && rr.out && strncmp(rr.out, "tokens=", 7) == 0;
    if (*ok) *out_count = atol(rr.out + 7);
    ctron_rt_run_free(&rr);
    ctron_parse_result_free(&pr);
    return 0;
}

int main(int argc, char** argv) {
    const char* root = argc > 1 ? argv[1] : "selfhost";
    struct { const char* module; const char* input; } cases[] = {
        {"lex_small.ct", "input_small.ct"},
    };
    size_t n = 0, fails = 0;
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        char mp[4096], ip[4096];
        snprintf(mp, sizeof mp, "%s/%s", root, cases[i].module);
        snprintf(ip, sizeof ip, "%s/%s", root, cases[i].input);
        // C 版参考计数(词法器)
        size_t ilen;
        char* input = read_file_str(ip);
        ctron_lex_result lr = ctron_lex(input, strlen(input));
        long want = (long)lr.ntoks;
        ctron_lex_result_free(&lr);
        free(input);
        long got = 0;
        int ok = 0;
        run_ctron_count(mp, &got, &ok);
        n++;
        if (!ok || got != want) {
            fails++;
            fprintf(stderr, "差分失败: %s vs C(%ld), Ctron(%ld) ok=%d\n",
                    cases[i].module, want, got, ok);
        }
    }
    printf("suite_diff: %zu cases, %zu failures\n", n, fails);
    return fails ? 1 : 0;
}
