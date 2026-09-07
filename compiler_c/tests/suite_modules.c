// suite_modules.c —— modules 包原生差分(C10-r):对 tests/modules/<包>/:
//   合并 src/*.ct(含 test 的主文件在前)→ 转译 → cc(一并链接 c_src/*.c)→ 原生运行。
// 语义对齐 Rust 版 native_suite::modules_native_run(P1-E⑰):
//   解析诊断包跳过;无 test 声明包跳过;转译域外包跳过;原生 exit 0 = 一致。
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "trans.h"

#define MAX_FILES 64

typedef struct {
    char* paths[MAX_FILES];
    char* srcs[MAX_FILES];
    int has_test[MAX_FILES];
    size_t n;
} fileset;

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

static void walk_ct(const char* dir, fileset* fs) {
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { walk_ct(path, fs); continue; }
        size_t bl = strlen(e->d_name);
        if (bl < 4 || strcmp(e->d_name + bl - 3, ".ct") != 0) continue;
        if (fs->n >= MAX_FILES) continue;
        char* src = read_file_str(path);
        if (!src) continue;
        fs->paths[fs->n] = strdup(path);
        fs->srcs[fs->n] = src;
        fs->has_test[fs->n] = strstr(src, "test \"") != NULL;
        fs->n++;
    }
    closedir(d);
}

// 稳定排序在 main 内以内联冒泡完成:主文件(含 test)在前,依赖按插入序。

int main(int argc, char** argv) {
    const char* root = argc > 1 ? argv[1] : "../tests/modules";
    DIR* d = opendir(root);
    if (!d) { fprintf(stderr, "suite_modules: 无法打开 %s\n", root); return 2; }
    mkdir("build/tm", 0755);

    size_t ran = 0, skipped = 0, fails = 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char case_dir[4096];
        snprintf(case_dir, sizeof case_dir, "%s/%s", root, e->d_name);
        struct stat st;
        if (stat(case_dir, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        fileset fs;
        memset(&fs, 0, sizeof fs);
        walk_ct(case_dir, &fs);
        if (fs.n == 0) continue;
        // 稳定排序:含 test 的主文件在前(插入序保依赖次序)
        size_t order[MAX_FILES];
        for (size_t i = 0; i < fs.n; i++) order[i] = i;
        for (size_t i = 0; i < fs.n; i++)
            for (size_t j = i + 1; j < fs.n; j++)
                if (!fs.has_test[order[i]] && fs.has_test[order[j]]) {
                    size_t t = order[i]; order[i] = order[j]; order[j] = t;
                }

        // 解析 + 合并 decls(浅拷贝;各文件 arena 存活至包处理结束)
        ctron_parse_result prs[MAX_FILES];
        size_t total = 0, nfiles = 0;
        int any_diags = 0, has_tests = 0;
        for (size_t i = 0; i < fs.n; i++) {
            size_t idx = order[i];
            prs[nfiles] = ctron_parse_src(fs.srcs[idx], strlen(fs.srcs[idx]));
            if (prs[nfiles].ndiags) any_diags = 1;
            total += prs[nfiles].file->ndecls;
            for (size_t k = 0; k < prs[nfiles].file->ndecls; k++)
                if (prs[nfiles].file->decls[k].kind == D_TEST) has_tests = 1;
            nfiles++;
        }
        if (any_diags || !has_tests) {
            skipped++;
            for (size_t i = 0; i < nfiles; i++) ctron_parse_result_free(&prs[i]);
            continue;
        }
        cdecl* merged = (cdecl*)calloc(total ? total : 1, sizeof(cdecl));
        size_t off = 0;
        for (size_t i = 0; i < nfiles; i++) {
            const cfile* pf = prs[i].file;
            memcpy(merged + off, pf->decls, pf->ndecls * sizeof(cdecl));
            off += pf->ndecls;
        }
        cfile mf;
        mf.decls = merged;
        mf.ndecls = total;

        ctron_trans_result tr = ctron_trans_file(&mf);
        if (tr.err) {
            skipped++; // 转译域外(信息性)
            free(merged);
            ctron_trans_result_free(&tr);
            for (size_t i = 0; i < nfiles; i++) ctron_parse_result_free(&prs[i]);
            continue;
        }
        char cpath[4096], bin[4096];
        snprintf(cpath, sizeof cpath, "build/tm/mod_%s.c", e->d_name);
        snprintf(bin, sizeof bin, "build/tm/mod_%s.bin", e->d_name);
        FILE* cf = fopen(cpath, "wb");
        if (!cf) { fprintf(stderr, "%s: 无法写 %s\n", e->d_name, cpath); fails++; continue; }
        fwrite(tr.code, 1, strlen(tr.code), cf);
        fclose(cf);
        ctron_trans_result_free(&tr);

        char cmd[8192];
        snprintf(cmd, sizeof cmd, "cc -O0 -w -std=gnu11 %s -o %s", cpath, bin);
        // 包内 c_src/*.c 一并链接
        char csdir[4096];
        snprintf(csdir, sizeof csdir, "%s/c_src", case_dir);
        DIR* cd = opendir(csdir);
        if (cd) {
            struct dirent* ce;
            while ((ce = readdir(cd)) != NULL) {
                size_t bl = strlen(ce->d_name);
                if (bl < 3 || strcmp(ce->d_name + bl - 2, ".c") != 0) continue;
                snprintf(cmd + strlen(cmd), sizeof cmd - strlen(cmd), " %s/%s", csdir, ce->d_name);
            }
            closedir(cd);
        }
        if (system(cmd) != 0) {
            fprintf(stderr, "%s: cc 编译失败\n", e->d_name);
            fails++;
            free(merged);
            for (size_t i = 0; i < nfiles; i++) ctron_parse_result_free(&prs[i]);
            continue;
        }
        char runcmd[4352];
        snprintf(runcmd, sizeof runcmd, "%s >/dev/null 2>/tmp/ctm_err.txt", bin);
        int rc = system(runcmd);
        ran++;
        if (rc != 0) {
            char* err = read_file_str("/tmp/ctm_err.txt");
            fprintf(stderr, "%s: 原生运行失败 rc=%d\n%.200s", e->d_name, rc, err ? err : "");
            free(err);
            fails++;
        }
        free(merged);
        for (size_t i = 0; i < nfiles; i++) ctron_parse_result_free(&prs[i]);
    }
    closedir(d);
    printf("suite_modules: %zu packages native run ok, %zu skipped, %zu failures\n", ran, skipped, fails);
    return fails ? 1 : 0;
}
