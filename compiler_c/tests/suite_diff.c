// suite_diff.c —— 自举差分:Ctron 词法器输出 vs C 版 lexer
// 计数模式 + 种类序列模式(逐字)
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

typedef struct { const char* module; const char* input; int seq; } TCase;

int main(int argc, char** argv) {
    const char* root = argc > 1 ? argv[1] : "selfhost";
    TCase cases[] = {
        {"lex_small.ct", "input_small.ct", 0},
        {"lex_kind.ct", "input_ops.ct", 1},
        {"lex_adv.ct", "input_adv.ct", 1},
        {"lex_corpus.ct", "../tests/05_own.ct", 1},
        {"lex_float.ct", "input_floats.ct", 1},
    };
    size_t fails = 0;
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        char mp[4096], ip[4096];
        snprintf(mp, sizeof mp, "%s/%s", root, cases[i].module);
        if (cases[i].input[0] == '.')
            snprintf(ip, sizeof ip, "%s", cases[i].input);
        else
            snprintf(ip, sizeof ip, "%s/%s", root, cases[i].input);
        // 运行 Ctron 模块
        char* msrc = read_file_str(mp);
        if (!msrc) { fails++; fprintf(stderr, "%s 无法读取\n", mp); continue; }
        ctron_parse_result pr = ctron_parse_src(msrc, strlen(msrc));
        free(msrc);
        if (pr.ndiags) { ctron_parse_result_free(&pr); fails++; fprintf(stderr, "%s 解析失败\n", mp); continue; }
        rt_run rr = ctron_rt_run_main(pr.file);
        ctron_parse_result_free(&pr);
        if (!(rr.st == RT_OK && rr.exit_code == 0)) {
            fprintf(stderr, "%s: 运行失败 st=%d rc=%ld msg=%s\n", cases[i].module, rr.st,
                    rr.exit_code, rr.msg ? rr.msg : "");
            ctron_rt_run_free(&rr);
            fails++;
            continue;
        }
        // C 版参考
        size_t ilen;
        char* input = read_file_str(ip);
        ctron_lex_result lr = ctron_lex(input, ilen = strlen(input));
        free(input);
        int ok = 0;
        if (!cases[i].seq) {
            // 计数模式:Ctron 输出 'tokens=N' 或纯数字行
            char buf[64];
            snprintf(buf, sizeof buf, "%ld", (long)lr.ntoks);
            ok = rr.out && strstr(rr.out, buf) != NULL;
        } else {
            // 种类序列模式:逐 token 种类名,以 '\n' 连接,结尾 Eof
            char* want = (char*)malloc(1);
            want[0] = 0;
            size_t cap = 1;
            for (size_t k = 0; k < lr.ntoks; k++) {
                const char* nm = ctron_tok_name(lr.toks[k].kind);
                size_t need = strlen(want) + strlen(nm) + 2;
                if (need > cap) { cap = need * 2; want = (char*)realloc(want, cap); }
                strcat(want, nm);
                strcat(want, k + 1 < lr.ntoks ? "\n" : "");
            }
            ok = rr.out && strcmp(rr.out, want) == 0;
            if (!ok) {
                fprintf(stderr, "%s 序列差分失败\n  C: %s\n  Ctron: %s\n", cases[i].module,
                        want ? want : "(空)", rr.out ? rr.out : "(空)");
            }
            free(want);
        }
        ctron_lex_result_free(&lr);
        ctron_rt_run_free(&rr);
        if (!ok) fails++;
    }
    printf("suite_diff: %zu cases, %zu failures\n",
           sizeof cases / sizeof cases[0], fails);
    return fails ? 1 : 0;
}
