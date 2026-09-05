// suite_diff.c —— 自举差分:Ctron 词法器输出 vs C 版 lexer
// 计数模式 + 种类序列模式 + payload 模式(逐字)。corpus 列表经 lex_num 模板逐文件扩面。
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "rt.h"
#include "sem.h"
#include "pkg.h"

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

// 执行差分(seq=5 main 模式 / seq=6 test 模式):Ctron 求值器输出 vs C rt 运行同一输入
// 模块源内 read_file 目标字面量统一为 ../selfhosted/input_ev2.ct,套件按 ip 换靶。
static int diff_exec(const char* msrc, const char* ip, int seq, const char* label) {
    char* m2 = replace_first(msrc, "../selfhosted/input_ev2.ct", ip);
    if (!m2) {
        fprintf(stderr, "%s 模块模板换靶失败\n", label);
        return 1;
    }
    ctron_parse_result pr = ctron_parse_src(m2, strlen(m2));
    free(m2);
    if (pr.ndiags) {
        fprintf(stderr, "%s 模块解析失败\n", label);
        ctron_parse_result_free(&pr);
        return 1;
    }
    rt_run mr = ctron_rt_run_main(pr.file);
    ctron_parse_result_free(&pr);
    if (mr.st != RT_OK) {
        fprintf(stderr, "%s: 模块运行失败 st=%d msg=%s\n", label, mr.st, mr.msg ? mr.msg : "");
        ctron_rt_run_free(&mr);
        return 1;
    }
    size_t ilen;
    char* input = read_file_str(ip);
    if (!input) {
        fprintf(stderr, "%s 输入无法读取\n", ip);
        ctron_rt_run_free(&mr);
        return 1;
    }
    ctron_parse_result pf = ctron_parse_src(input, ilen = strlen(input));
    free(input);
    if (pf.ndiags) {
        fprintf(stderr, "%s 参考解析诊断 %zu\n", label, pf.ndiags);
        ctron_rt_run_free(&mr);
        ctron_parse_result_free(&pf);
        return 1;
    }
    const char* want = "";
    long want_rc = 0;
    rt_run orr;
    memset(&orr, 0, sizeof orr);
    if (seq == 5) {
        orr = ctron_rt_run_main(pf.file);
        if (orr.st != RT_OK) {
            fprintf(stderr, "%s 参考运行失败 st=%d msg=%s\n", label, orr.st, orr.msg ? orr.msg : "");
            ctron_rt_run_free(&mr);
            ctron_rt_run_free(&orr);
            ctron_parse_result_free(&pf);
            return 1;
        }
        want = orr.out ? orr.out : "";
        want_rc = orr.exit_code;
    } else {
        orr = ctron_rt_run(pf.file);
        int pass = orr.st == RT_OK && orr.tests_run == orr.tests_total && orr.tests_total > 0;
        if (pass) { want = ""; want_rc = 0; }
        else { want = orr.msg ? orr.msg : ""; want_rc = 1; }
    }
    const char* mout = mr.out ? mr.out : "";
    int ok = strcmp(mout, want) == 0 && mr.exit_code == want_rc;
    if (!ok) {
        fprintf(stderr, "%s 执行差分失败\n  参考(rc=%ld): %s\n  Ctron(rc=%ld): %s\n",
                label, want_rc, want, mr.exit_code, mout);
    }
    ctron_rt_run_free(&mr);
    ctron_rt_run_free(&orr);
    ctron_parse_result_free(&pf);
    return ok ? 0 : 1;
}

// 模块级 oracle 差分(seq=8):Ctron pkg_chk 对包目录输出 vs C ctron_pkg_check 非 JSON 文本
static int diff_pkg(const char* msrc, const char* ip, int seq, const char* label) {
    (void)seq;
    char* m2 = replace_first(msrc, "../tests/modules/comptime_budget", ip);
    if (!m2) {
        fprintf(stderr, "%s 模块模板换靶失败\n", label);
        return 1;
    }
    ctron_parse_result pr = ctron_parse_src(m2, strlen(m2));
    free(m2);
    if (pr.ndiags) {
        fprintf(stderr, "%s 模块解析失败\n", label);
        ctron_parse_result_free(&pr);
        return 1;
    }
    rt_run mr = ctron_rt_run_main(pr.file);
    ctron_parse_result_free(&pr);
    if (mr.st != RT_OK || mr.exit_code != 0) {
        fprintf(stderr, "%s: 模块运行失败 st=%d rc=%ld msg=%s\n", label, mr.st,
                mr.exit_code, mr.msg ? mr.msg : "");
        ctron_rt_run_free(&mr);
        return 1;
    }
    pkg_res r = ctron_pkg_check(ip);
    char* buf = NULL;
    size_t bufn = 0;
    FILE* mf = open_memstream(&buf, &bufn);
    if (!mf) {
        ctron_pkg_res_free(&r);
        ctron_rt_run_free(&mr);
        return 1;
    }
    for (size_t i = 0; i < r.n; i++)
        fprintf(mf, "%s/%s %s: %s\n", ip, r.d[i].rel, r.d[i].code, r.d[i].msg);
    fprintf(mf, "%zu diagnostics\n", r.n);
    fclose(mf);
    ctron_pkg_res_free(&r);
    const char* mo = mr.out ? mr.out : "";
    const char* cb = buf ? buf : "";
    int ok = strcmp(mo, cb) == 0;
    if (!ok) {
        fprintf(stderr, "%s 模块级差分失败\n  C: %s\n  Ctron: %s\n", label, cb, mo);
    }
    free(buf);
    ctron_rt_run_free(&mr);
    return ok ? 0 : 1;
}

// cc 驱动差分(seq=9):Ctron cc.ct(parse→单文件语义 12 项→run)输出 vs C 同管线
static const char* const CC_KNOWN[] = {"W8010", "W8020", "E4030", "E3020", "E3031", "E3060",
                                       "E4020", "E6020", "E3010", "E3050", "E2030", "E3040"};
static int diff_cc(const char* msrc, const char* ip, int seq, const char* label) {
    (void)seq;
    char* m2 = replace_first(msrc, "../selfhosted/input_cc.ct", ip);
    if (!m2) {
        fprintf(stderr, "%s 模块模板换靶失败\n", label);
        return 1;
    }
    ctron_parse_result pr = ctron_parse_src(m2, strlen(m2));
    free(m2);
    if (pr.ndiags) {
        fprintf(stderr, "%s 模块解析失败\n", label);
        ctron_parse_result_free(&pr);
        return 1;
    }
    rt_run mr = ctron_rt_run_main(pr.file);
    ctron_parse_result_free(&pr);
    if (mr.st != RT_OK) {
        fprintf(stderr, "%s: 模块运行失败 st=%d msg=%s\n", label, mr.st, mr.msg ? mr.msg : "");
        ctron_rt_run_free(&mr);
        return 1;
    }
    // C oracle:parse + sem(仅已实现 12 码入账);无诊断才原生运行
    char* input = read_file_str(ip);
    if (!input) {
        fprintf(stderr, "%s 输入无法读取\n", ip);
        ctron_rt_run_free(&mr);
        return 1;
    }
    ctron_parse_result pf = ctron_parse_src(input, strlen(input));
    free(input);
    if (pf.ndiags) {
        fprintf(stderr, "%s 参考解析诊断 %zu\n", label, pf.ndiags);
        ctron_rt_run_free(&mr);
        ctron_parse_result_free(&pf);
        return 1;
    }
    ctron_arena* arena = ctron_arena_new();
    ctron_sem_result sr = ctron_sem_check(pf.file, arena);
    char* buf = NULL;
    size_t bufn = 0;
    FILE* mf = open_memstream(&buf, &bufn);
    if (!mf) {
        free(sr.diags);
        ctron_arena_free(arena);
        ctron_rt_run_free(&mr);
        ctron_parse_result_free(&pf);
        return 1;
    }
    int any = 0;
    for (size_t i = 0; i < sr.ndiags; i++) {
        int in = 0;
        for (size_t j = 0; j < sizeof CC_KNOWN / sizeof CC_KNOWN[0]; j++)
            if (strcmp(sr.diags[i].code, CC_KNOWN[j]) == 0) in = 1;
        if (in) {
            fprintf(mf, "%s: %s\n", sr.diags[i].code, sr.diags[i].message);
            any = 1;
        }
    }
    fclose(mf);
    free(sr.diags);
    ctron_arena_free(arena);
    ctron_parse_result_free(&pf);
    long want_rc = 0;
    if (!any) {
        // 干净:原生运行 main(输出进 oracle)
        free(buf);
        buf = NULL;
        char* src2 = read_file_str(ip);
        if (!src2) { ctron_rt_run_free(&mr); return 1; }
        ctron_parse_result pf2 = ctron_parse_src(src2, strlen(src2));
        free(src2);
        if (pf2.ndiags) {
            ctron_rt_run_free(&mr);
            ctron_parse_result_free(&pf2);
            return 1;
        }
        rt_run rr2 = ctron_rt_run_main(pf2.file);
        ctron_parse_result_free(&pf2);
        if (rr2.st != RT_OK) {
            fprintf(stderr, "%s 参考运行失败 st=%d msg=%s\n", label, rr2.st, rr2.msg ? rr2.msg : "");
            ctron_rt_run_free(&mr);
            ctron_rt_run_free(&rr2);
            return 1;
        }
        FILE* mf2 = open_memstream(&buf, &bufn);
        if (mf2) { fputs(rr2.out ? rr2.out : "", mf2); fclose(mf2); }
        want_rc = rr2.exit_code == 0 ? 0 : 1;
        ctron_rt_run_free(&rr2);
    } else {
        want_rc = 1; // 有诊断:编译失败,不运行
    }
    const char* mo = mr.out ? mr.out : "";
    const char* cb = buf ? buf : "";
    int ok = strcmp(mo, cb) == 0 && mr.exit_code == want_rc;
    if (!ok) {
        fprintf(stderr, "%s cc 差分失败\n  参考(rc=%ld): %s\n  Ctron(rc=%ld): %s\n",
                label, want_rc, cb, mr.exit_code, mo);
    }
    free(buf);
    ctron_rt_run_free(&mr);
    return ok ? 0 : 1;
}

// 单个差分:msrc = Ctron 模块源码(已含目标输入), ip = 输入文件路径, seq=0 计数 1 种类 2 payload
static int diff_one(const char* msrc, const char* ip, int seq, const char* label) {
    if (seq == 9) return diff_cc(msrc, ip, seq, label);
    if (seq == 8) return diff_pkg(msrc, ip, seq, label);
    if (seq == 5 || seq == 6) return diff_exec(msrc, ip, seq, label);
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
    } else if (seq == 4) {
        // 语义差分:C 侧 parse + ctron_sem_check 的 CODE: message 行
        ctron_parse_result pf = ctron_parse_src(input, ilen);
        if (pf.ndiags) {
            fprintf(stderr, "%s 参考解析诊断 %zu\n", label, pf.ndiags);
        } else {
            ctron_arena* arena = ctron_arena_new();
            ctron_sem_result sr = ctron_sem_check(pf.file, arena);
            char* buf = NULL;
            size_t bufn = 0;
            FILE* mf = open_memstream(&buf, &bufn);
            if (!mf) { ok = 0; }
            else {
                for (size_t i = 0; i < sr.ndiags; i++) {
                    fprintf(mf, "%s: %s\n", sr.diags[i].code, sr.diags[i].message);
                }
                fclose(mf);
                const char* co = rr.out ? rr.out : "";
                const char* cb = buf ? buf : "";
                ok = strcmp(co, cb) == 0;
                if (!ok) {
                    fprintf(stderr, "%s 语义差分失败\n  C: %s\n  Ctron: %s\n", label,
                            cb, co);
                }
                free(buf);
            }
            free(sr.diags);
            ctron_arena_free(arena);
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
    const char* root = argc > 1 ? argv[1] : "../selfhosted";
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
        {"ev2.ct", "input_ev2.ct", 5},
        {"ev2.ct", "input_ev2b.ct", 5},
        {"ev2.ct", "input_ev2t.ct", 6},
        {"ev2.ct", "input_ev2tf.ct", 6},
        {"pkg_chk.ct", "../tests/modules/orphan", 8},
        {"pkg_chk.ct", "../tests/modules/circular", 8},
        {"pkg_chk.ct", "../tests/modules/visibility", 8},
        {"pkg_chk.ct", "../tests/modules/caps", 8},
        {"pkg_chk.ct", "../tests/modules/comptime_budget", 8},
        {"pkg_chk.ct", "../tests/modules/use_ok", 8},
        {"pkg_chk.ct", "../tests/modules/ffi_math", 8},
        {"cc.ct", "input_cc.ct", 9},
        {"cc.ct", "input_cc_neg.ct", 9},
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
    // 树版(结构化 AST)语料差分:parsetree.ct 模板换靶,同样目录自动纳入/诊断豁免
    {
        char ttpath[4096], tip[4096];
        snprintf(ttpath, sizeof ttpath, "%s/parsetree.ct", root);
        snprintf(tip, sizeof tip, "%s/input_ptree.ct", root);
        char* tsrc2 = read_file_str(ttpath);
        if (!tsrc2) { fails++; fprintf(stderr, "%s 无法读取\n", ttpath); }
        else {
            DIR* td = opendir("../tests");
            if (!td) { fprintf(stderr, "无法打开 ../tests\n"); fails++; }
            else {
                struct dirent* te;
                while ((te = readdir(td)) != NULL) {
                    if (te->d_type != DT_REG) continue;
                    size_t tbl = strlen(te->d_name);
                    if (tbl < 4 || strcmp(te->d_name + tbl - 3, ".ct") != 0) continue;
                    char tcp[4096];
                    snprintf(tcp, sizeof tcp, "../tests/%s", te->d_name);
                    char* tinput = read_file_str(tcp);
                    if (!tinput) continue;
                    ctron_parse_result tpre = ctron_parse_src(tinput, strlen(tinput));
                    free(tinput);
                    int tclean = tpre.ndiags == 0;
                    ctron_parse_result_free(&tpre);
                    if (!tclean) continue;
                    char* msrc2 = replace_first(tsrc2, tip, tcp);
                    if (!msrc2) { fprintf(stderr, "%s 模板替换失败\n", tcp); fails++; }
                    else {
                        nrun++;
                        fails += diff_one(msrc2, tcp, 3, te->d_name);
                        free(msrc2);
                    }
                }
                closedir(td);
            }
        }
        free(tsrc2);
    }
    // 语义差分(seq=4):sem_chk.ct(树上 W8010)模板换靶;仅当 C 诊断 ⊆ 已实现码 {W8010}
    {
        static const char* known[] = {"W8010", "W8020", "E4030", "E3020", "E3031", "E3060", "E4020", "E6020", "E3010", "E3050", "E2030", "E3040"};
        char spath[4096];
        snprintf(spath, sizeof spath, "%s/sem_chk.ct", root);
        char* ssrc = read_file_str(spath);
        if (!ssrc) { fails++; fprintf(stderr, "%s 无法读取\n", spath); }
        else {
            DIR* sd = opendir("../tests");
            if (!sd) { fprintf(stderr, "无法打开 ../tests\n"); fails++; }
            else {
                struct dirent* se;
                while ((se = readdir(sd)) != NULL) {
                    if (se->d_type != DT_REG) continue;
                    size_t sbl = strlen(se->d_name);
                    if (sbl < 4 || strcmp(se->d_name + sbl - 3, ".ct") != 0) continue;
                    char scp[4096];
                    snprintf(scp, sizeof scp, "../tests/%s", se->d_name);
                    char* sin = read_file_str(scp);
                    if (!sin) continue;
                    ctron_parse_result spr = ctron_parse_src(sin, strlen(sin));
                    free(sin);
                    if (spr.ndiags) { ctron_parse_result_free(&spr); continue; }
                    ctron_arena* sarena = ctron_arena_new();
                    ctron_sem_result ssr = ctron_sem_check(spr.file, sarena);
                    int inscope = 1;
                    for (size_t i = 0; i < ssr.ndiags && inscope; i++) {
                        int kn = 0;
                        for (size_t j = 0; j < sizeof known / sizeof known[0]; j++)
                            if (strcmp(ssr.diags[i].code, known[j]) == 0) kn = 1;
                        if (!kn) inscope = 0;
                    }
                    if (inscope) {
                        char* msrc = replace_first(ssrc, "../selfhosted/input_ptree.ct", scp);
                        if (!msrc) { fprintf(stderr, "%s 模板替换失败\n", scp); fails++; }
                        else {
                            nrun++;
                            fails += diff_one(msrc, scp, 4, se->d_name);
                            free(msrc);
                        }
                    }
                    free(ssr.diags);
                    ctron_arena_free(sarena);
                    ctron_parse_result_free(&spr);
                }
                closedir(sd);
            }
        }
        free(ssrc);
    }
    printf("suite_diff: %zu cases, %zu failures\n", nrun, fails);
    return fails ? 1 : 0;
}
