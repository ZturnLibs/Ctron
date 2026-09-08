// suite_corpus_trans.c —— C10 语料转译差分:对 tests/*.ct 中每个可转译文件:
//   解释器侧(ctron_rt_run / ctron_rt_run_main)vs 原生侧(trans → cc → 执行)
//   对比 stdout / exit / panic 消息。neg/lint 文件跳过(检查器域);转译不支持的文件
//   记入 untranspiled(信息性,不计失败)—— 覆盖率随 C10 里程碑推进。
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "rt.h"
#include "trans.h"

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
static char* slurp(const char* path) {
    char* s = read_file_str(path);
    if (!s) { s = (char*)malloc(1); s[0] = 0; }
    return s;
}

int main(int argc, char** argv) {
    const char* root = argc > 1 ? argv[1] : "../tests";
    DIR* d = opendir(root);
    if (!d) { fprintf(stderr, "suite_corpus_trans: 无法打开 %s\n", root); return 2; }
    mkdir("build/tc", 0755);

    size_t pass = 0, fails = 0, untrans = 0, skipped = 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        size_t bl = strlen(e->d_name);
        if (bl < 4 || strcmp(e->d_name + bl - 3, ".ct") != 0) continue;
        if (strstr(e->d_name, ".neg.") || strstr(e->d_name, ".lint.")) { skipped++; continue; }
        char path[4096], cpath[512], bin[512];
        fprintf(stderr, "[try] %s\n", e->d_name);
        snprintf(path, sizeof path, "%s/%s", root, e->d_name);
        snprintf(cpath, sizeof cpath, "build/tc/%s.c", e->d_name);
        snprintf(bin, sizeof bin, "build/tc/%s.bin", e->d_name);

        char* src = read_file_str(path);
        if (!src) { fprintf(stderr, "%s: 无法读取\n", e->d_name); fails++; continue; }
        ctron_parse_result pr = ctron_parse_src(src, strlen(src));
        free(src);
        if (pr.ndiags) { fprintf(stderr, "%s: 解析诊断\n", e->d_name); fails++; continue; }

        ctron_trans_result tr = ctron_trans_file(pr.file);
        if (tr.err) { untrans++; ctron_trans_result_free(&tr); ctron_parse_result_free(&pr); continue; }
        FILE* cf = fopen(cpath, "wb");
        if (!cf) { fprintf(stderr, "%s: 无法写 %s\n", e->d_name, cpath); fails++; continue; }
        fwrite(tr.code, 1, strlen(tr.code), cf);
        fclose(cf);
        ctron_trans_result_free(&tr);

        char cmd[1024];
        snprintf(cmd, sizeof cmd, "cc -std=c11 -O2 -o %s %s 2>/dev/null", bin, cpath);
        if (system(cmd) != 0) { fprintf(stderr, "%s: cc 失败\n", e->d_name); fails++; continue; }

        // 解释器侧:有 fn main → run_main;否则 test 块
        int is_main = 0;
        for (size_t i = 0; i < pr.file->ndecls; i++)
            if (pr.file->decls[i].kind == D_FN && !strcmp(pr.file->decls[i].fn_.name, "main")) is_main = 1;
        char* marker = NULL;
        int is_panic = strstr(e->d_name, ".panic.") != NULL;
        if (is_panic) {
            char* s2 = read_file_str(path);
            char* p = strstr(s2, "//@ panic:");
            if (p) {
                p += 10;
                while (*p == ' ') p++;
                char* en = strchr(p, '\n');
                marker = strndup(p, en ? (size_t)(en - p) : strlen(p));
            }
            free(s2);
        }
        rt_run rr = is_main ? ctron_rt_run_main(pr.file) : ctron_rt_run(pr.file);

        // 原生侧
        char cmd2[1024];
        snprintf(cmd2, sizeof cmd2, "%s > /tmp/ctc_out.txt 2> /tmp/ctc_err.txt", bin);
        int rc = system(cmd2);
        char* nout = slurp("/tmp/ctc_out.txt");
        char* nerr = slurp("/tmp/ctc_err.txt");

        int ok = 1;
        char why[256] = {0};
        if (is_panic) {
            if (!(rc != 0 && rr.st == RT_PANIC)) { ok = 0; snprintf(why, sizeof why, "panic 状态 rt=%d rc=%d", rr.st, rc); }
            else {
                char* nl = strchr(nerr, '\n');
                if (nl) *nl = 0;
                if (strcmp(nerr, rr.msg) != 0) { ok = 0; snprintf(why, sizeof why, "panic 消息 [%s] vs [%s]", rr.msg, nerr); }
                else if (marker && !strstr(rr.msg, marker)) { ok = 0; snprintf(why, sizeof why, "缺 marker %s", marker); }
            }
        } else if (is_main) {
            if (rc != 0) { ok = 0; snprintf(why, sizeof why, "native exit=%d", rc); }
            else if (strcmp(nout, rr.out ? rr.out : "") != 0) { ok = 0; snprintf(why, sizeof why, "stdout 不一致"); }
        } else {
            if (rc != 0) { ok = 0; snprintf(why, sizeof why, "native exit=%d stderr=[%.80s]", rc, nerr); }
            else if (!(rr.st == RT_OK && rr.tests_run == rr.tests_total && rr.tests_total > 0)) {
                ok = 0; snprintf(why, sizeof why, "rt 状态 %d %zu/%zu", rr.st, rr.tests_run, rr.tests_total);
            } else if (strcmp(nout, rr.out ? rr.out : "") != 0) { ok = 0; snprintf(why, sizeof why, "stdout 不一致"); }
        }

        if (ok) pass++;
        else { fails++; fprintf(stderr, "%s: %s\n", e->d_name, why); }
        free(marker);
        free(nout);
        free(nerr);
        ctron_rt_run_free(&rr);
        ctron_parse_result_free(&pr);
    }
    closedir(d);
    printf("suite_corpus_trans: %zu pass, %zu failures, %zu untranspiled, %zu skipped(neg/lint)\n",
           pass, fails, untrans, skipped);
    return fails ? 1 : 0;
}
