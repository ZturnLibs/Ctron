// suite_trans.c —— C10-a 转译后端差分验收:
// 对 tests/trans_fixtures/*.ct 逐文件:
//   解释器侧:ctron_rt_run / ctron_rt_run_main(in-process)
//   原生侧:ctron_trans_file → cc -o → 执行(stdout/stderr/exit 落盘回读)
// 判定:panic 夹具(文件名含 .panic.)双方报错消息逐字一致且 exit!=0;
//       其余双方 exit==0、stderr 空、stdout 与 rr.out 逐字节一致。
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
static char* read_file_or_empty(const char* path) {
    char* s = read_file_str(path);
    if (!s) { s = (char*)malloc(1); s[0] = 0; }
    return s;
}

static void run_native(const char* bin, char* out_path, char* err_path, size_t n) {
    snprintf(out_path, n, "/tmp/ctron_trans_out.txt");
    snprintf(err_path, n, "/tmp/ctron_trans_err.txt");
    char cmd[1024];
    snprintf(cmd, sizeof cmd, "%s > %s 2> %s", bin, out_path, err_path);
    int rc = system(cmd);
    FILE* f = fopen("/tmp/ctron_trans_rc.txt", "wb");
    if (f) { fprintf(f, "%d", rc); fclose(f); }
}

static int read_rc(void) {
    char* s = read_file_str("/tmp/ctron_trans_rc.txt");
    int rc = s ? atoi(s) : -1;
    free(s);
    return rc;
}

int main(int argc, char** argv) {
    const char* root = argc > 1 ? argv[1] : "tests/trans_fixtures";
    DIR* d = opendir(root);
    if (!d) { fprintf(stderr, "suite_trans: 无法打开 %s\n", root); return 2; }
    mkdir("build", 0755);
    mkdir("build/tf", 0755);

    size_t pass = 0, fails = 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        size_t bl = strlen(e->d_name);
        if (bl < 4 || strcmp(e->d_name + bl - 3, ".ct") != 0) continue;
        char path[4096], cpath[512], bin[512];
        snprintf(path, sizeof path, "%s/%s", root, e->d_name);
        snprintf(cpath, sizeof cpath, "build/tf/%s.c", e->d_name);
        snprintf(bin, sizeof bin, "build/tf/%s.bin", e->d_name);

        char* src = read_file_str(path);
        if (!src) { fprintf(stderr, "%s: 无法读取\n", e->d_name); fails++; continue; }
        ctron_parse_result pr = ctron_parse_src(src, strlen(src));
        free(src);
        if (pr.ndiags) { fprintf(stderr, "%s: 解析诊断 %zu\n", e->d_name, pr.ndiags); fails++; continue; }

        ctron_trans_result tr = ctron_trans_file(pr.file);
        if (tr.err) { fprintf(stderr, "%s: trans: %s\n", e->d_name, tr.err); fails++; continue; }
        FILE* cf = fopen(cpath, "wb");
        if (!cf) { fprintf(stderr, "%s: 无法写 %s\n", e->d_name, cpath); fails++; continue; }
        fwrite(tr.code, 1, strlen(tr.code), cf);
        fclose(cf);
        ctron_trans_result_free(&tr);

        char cmd[1024];
        snprintf(cmd, sizeof cmd, "cc -std=c11 -O2 -o %s %s 2>/dev/null", bin, cpath);
        if (system(cmd) != 0) { fprintf(stderr, "%s: cc 失败\n", e->d_name); fails++; continue; }

        // 解释器侧
        int is_main = strstr(e->d_name, ".main.") != NULL;
        int is_panic = strstr(e->d_name, ".panic.") != NULL;
        char* marker = NULL;
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
        char outp[128], errp[128];
        run_native(bin, outp, errp, sizeof outp);
        int rc = read_rc();
        char* nout = read_file_or_empty(outp);
        char* nerr = read_file_or_empty(errp);

        int ok = 1;
        char why[256] = {0};
        if (is_panic) {
            if (!(rc != 0 && rr.st == RT_PANIC)) { ok = 0; snprintf(why, sizeof why, "panic 状态不一致(rt=%d rc=%d)", rr.st, rc); }
            else {
                char* nl = strchr(nerr, '\n');
                if (nl) *nl = 0;
                if (strcmp(nerr, rr.msg) != 0) { ok = 0; snprintf(why, sizeof why, "panic 消息不一致:[%s] vs [%s]", rr.msg, nerr); }
                else if (marker && !strstr(rr.msg, marker)) { ok = 0; snprintf(why, sizeof why, "缺 marker 子串 %s", marker); }
            }
        } else if (is_main) {
            if (rc != 0) { ok = 0; snprintf(why, sizeof why, "native exit=%d stderr=[%s]", rc, nerr); }
            else if (strcmp(nout, rr.out ? rr.out : "") != 0) { ok = 0; snprintf(why, sizeof why, "stdout 不一致"); }
        } else {
            if (rc != 0) { ok = 0; snprintf(why, sizeof why, "native exit=%d stderr=[%s]", rc, nerr); }
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
    printf("suite_trans: %zu pass, %zu failures\n", pass, fails);
    return fails ? 1 : 0;
}
