// suite_diff.c —— 自举差分:Ctron 词法器输出 vs C 版 lexer
// 计数模式 + 种类序列模式 + payload 模式(逐字)。corpus 列表经 lex_num 模板逐文件扩面。
#include <dirent.h>
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

// 把 msrc 中 needle 首次出现整段替换为 repl(自举模块输入路径换靶)
static char* replace_first(const char* msrc, const char* needle, const char* repl) {
    const char* hit = strstr(msrc, needle);
    if (!hit) return NULL;
    size_t pre = (size_t)(hit - msrc);
    size_t nn = strlen(needle), rn = strlen(repl), ln = strlen(msrc);
    char* out = (char*)malloc(ln - nn + rn + 1);
    if (!out) abort();
    memcpy(out, msrc, pre);
    memcpy(out + pre, repl, rn);
    memcpy(out + pre + rn, msrc + pre + nn, ln - pre - nn + 1);
    return out;
}

// 单个差分:msrc = Ctron 模块源码(已含目标输入), ip = 输入文件路径, seq=0 计数 1 种类 2 payload
static int diff_one(const char* msrc, const char* ip, int seq, const char* label) {
    ctron_parse_result pr = ctron_parse_src(msrc, strlen(msrc));
    if (pr.ndiags) {
        ctron_parse_result_free(&pr);
        fprintf(stderr, "%s 解析失败\n", label);
        return 1;
    }
    rt_run rr = ctron_rt_run_main(pr.file);
    ctron_parse_result_free(&pr);
    if (!(rr.st == RT_OK && rr.exit_code == 0)) {
        fprintf(stderr, "%s: 运行失败 st=%d rc=%ld msg=%s\n", label, rr.st,
                rr.exit_code, rr.msg ? rr.msg : "");
        ctron_rt_run_free(&rr);
        return 1;
    }
    size_t ilen;
    char* input = read_file_str(ip);
    if (!input) {
        fprintf(stderr, "%s 输入无法读取\n", ip);
        ctron_rt_run_free(&rr);
        return 1;
    }
    ctron_lex_result lr = ctron_lex(input, ilen = strlen(input));
    int ok = 0;
    if (seq == 3) {
        // 解析器差分:C 侧对同一输入 parse + ctron_file_show 作参考
        ctron_parse_result pf = ctron_parse_src(input, ilen);
        if (pf.ndiags) {
            fprintf(stderr, "%s 参考解析诊断 %zu\n", label, pf.ndiags);
        } else {
            char* buf = NULL;
            size_t bufn = 0;
            FILE* mf = open_memstream(&buf, &bufn);
            if (!mf) { ok = 0; }
            else {
                ctron_file_show(pf.file, mf);
                fclose(mf);
                ok = rr.out && buf && strcmp(rr.out, buf) == 0;
                if (!ok) {
                    fprintf(stderr, "%s 解析差分失败\n  C: %s\n  Ctron: %s\n", label,
                            buf ? buf : "(空)", rr.out ? rr.out : "(空)");
                }
                free(buf);
            }
        }
        ctron_parse_result_free(&pf);
    } else if (!seq) {
        char buf[64];
        snprintf(buf, sizeof buf, "%ld", (long)lr.ntoks);
        ok = rr.out && strstr(rr.out, buf) != NULL;
    } else {
        char* want = (char*)malloc(1);
        want[0] = 0;
        size_t cap = 1;
        for (size_t k = 0; k < lr.ntoks; k++) {
            const char* nm = ctron_tok_name(lr.toks[k].kind);
            const char* txt = lr.toks[k].text ? lr.toks[k].text : "";
            int is_str = lr.toks[k].kind == TOK_STR;
            int with = seq == 2
                && (is_str || lr.toks[k].kind == TOK_IDENT || lr.toks[k].kind == TOK_INT
                    || lr.toks[k].kind == TOK_FLOAT);
            char line[512];
            if (with && is_str) {
                size_t a = lr.toks[k].span.start, bb = lr.toks[k].span.end;
                size_t wlen = bb > a && bb <= ilen ? bb - a : 0;
                char* raw = (char*)malloc(wlen + 1);
                memcpy(raw, input + a, wlen);
                raw[wlen] = 0;
                snprintf(line, sizeof line, "%s:%s", nm, raw);
                free(raw);
            } else if (with)
                snprintf(line, sizeof line, "%s:%s", nm, txt);
            else
                snprintf(line, sizeof line, "%s", nm);
            size_t need = strlen(want) + strlen(line) + 2;
            if (need > cap) { cap = need * 2; want = (char*)realloc(want, cap); }
            strcat(want, line);
            strcat(want, k + 1 < lr.ntoks ? "\n" : "");
        }
        ok = rr.out && strcmp(rr.out, want) == 0;
        if (!ok) {
            fprintf(stderr, "%s 差分失败\n  C: %s\n  Ctron: %s\n", label,
                    want ? want : "(空)", rr.out ? rr.out : "(空)");
        }
        free(want);
    }
    free(input);
    ctron_lex_result_free(&lr);
    ctron_rt_run_free(&rr);
    return ok ? 0 : 1;
}

typedef struct { const char* module; const char* input; int seq; } TCase; // seq:0=计数 1=种类 2=payload

int main(int argc, char** argv) {
    const char* root = argc > 1 ? argv[1] : "selfhost";
    TCase cases[] = {
        {"lex_small.ct", "input_small.ct", 0},
        {"lex_kind.ct", "input_ops.ct", 1},
        {"lex_adv.ct", "input_adv.ct", 1},
        {"lex_corpus.ct", "../tests/05_own.ct", 1},
        {"lex_float.ct", "input_floats.ct", 1},
        {"lex_pay.ct", "input_pay.ct", 2},
        {"lex_str.ct", "input_str.ct", 2},
        {"lex_num.ct", "input_num.ct", 2},
        {"parse_ast.ct", "input_parse_ast.ct", 3},
        {"parsetree.ct", "input_ptree.ct", 3},
    };
    size_t fails = 0, nrun = 0;
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        char mp[4096], ip[4096];
        snprintf(mp, sizeof mp, "%s/%s", root, cases[i].module);
        if (cases[i].input[0] == '.')
            snprintf(ip, sizeof ip, "%s", cases[i].input);
        else
            snprintf(ip, sizeof ip, "%s/%s", root, cases[i].input);
        char* msrc = read_file_str(mp);
        if (!msrc) { fails++; fprintf(stderr, "%s 无法读取\n", mp); continue; }
        nrun++;
        fails += diff_one(msrc, ip, cases[i].seq, cases[i].module);
        free(msrc);
    }
    // 多语料扩面:lex_num 模板按 corpus 文件换靶,payload 级(seq=2)差分
    char tpath[4096], num_ip[4096];
    snprintf(tpath, sizeof tpath, "%s/lex_num.ct", root);
    snprintf(num_ip, sizeof num_ip, "%s/input_num.ct", root);
    char* tsrc = read_file_str(tpath);
    if (!tsrc) { fails++; fprintf(stderr, "%s 无法读取\n", tpath); }
    else {
        DIR* d = opendir("../tests");
        if (!d) { fprintf(stderr, "无法打开 ../tests\n"); fails++; }
        else {
            struct dirent* e;
            while ((e = readdir(d)) != NULL) {
                if (e->d_type != DT_REG) continue;
                size_t bl = strlen(e->d_name);
                if (bl < 4 || strcmp(e->d_name + bl - 3, ".ct") != 0) continue;
                char cp[4096];
                snprintf(cp, sizeof cp, "../tests/%s", e->d_name);
                char* msrc = replace_first(tsrc, num_ip, cp);
                if (!msrc) { fprintf(stderr, "%s 模板替换失败\n", cp); fails++; continue; }
                nrun++;
                fails += diff_one(msrc, cp, 2, e->d_name);
                free(msrc);
            }
            closedir(d);
        }
    }
    free(tsrc);
    // 真实语料 parse-AST 差分(seq=3):parse_ast.ct 模板换靶,逐文件 vs C ctron_file_show
    {
                // 真实语料 parse-AST 差分(seq=3):parse_ast.ct 模板换靶,目录自动纳入全部语料
        // 参考带解析/语义诊断(01c_parse.neg E1001 / 06_static_var E3030)的文件自动豁免
        {
            char ptpath[4096], pip[4096];
            snprintf(ptpath, sizeof ptpath, "%s/parse_ast.ct", root);
            snprintf(pip, sizeof pip, "%s/input_parse_ast.ct", root);
            char* ptsrc = read_file_str(ptpath);
            if (!ptsrc) { fails++; fprintf(stderr, "%s 无法读取\n", ptpath); }
            else {
                DIR* pd = opendir("../tests");
                if (!pd) { fprintf(stderr, "无法打开 ../tests\n"); fails++; }
                else {
                    struct dirent* pe;
                    while ((pe = readdir(pd)) != NULL) {
                        if (pe->d_type != DT_REG) continue;
                        size_t pbl = strlen(pe->d_name);
                        if (pbl < 4 || strcmp(pe->d_name + pbl - 3, ".ct") != 0) continue;
                        char pcp[4096];
                        snprintf(pcp, sizeof pcp, "../tests/%s", pe->d_name);
                        // 参考需零诊断;有 E1001/E3030 的负例文件不参与 parse 差分
                        char* pinput = read_file_str(pcp);
                        if (!pinput) continue;
                        ctron_parse_result ppre = ctron_parse_src(pinput, strlen(pinput));
                        free(pinput);
                        int clean = ppre.ndiags == 0;
                        ctron_parse_result_free(&ppre);
                        if (!clean) continue;
                        char* msrc = replace_first(ptsrc, pip, pcp);
                        if (!msrc) { fprintf(stderr, "%s 模板替换失败\n", pcp); fails++; }
                        else {
                            nrun++;
                            fails += diff_one(msrc, pcp, 3, pe->d_name);
                            free(msrc);
                        }
                    }
                    closedir(pd);
                }
            }
            free(ptsrc);
        }
    }
    printf("suite_diff: %zu cases, %zu failures\n", nrun, fails);
    return fails ? 1 : 0;
}
