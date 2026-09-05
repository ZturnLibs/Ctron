// suite_pkg.c —— C3-c 模块级验收:遍历 <root>/modules/*(含 Ctron.toml 的包),
// 对每个包做 pkg 检查,按各源文件 marker 评分(仅模块级码 E5010/E5020/E2020/E4010/E6010)。
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pkg.h"

static int has_toml(const char* dir) {
    char p[4096];
    snprintf(p, sizeof p, "%s/Ctron.toml", dir);
    FILE* f = fopen(p, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

#define MAXN 16
typedef struct { char codes[MAXN][8]; char msg[MAXN][64]; int has_msg[MAXN]; int n; } expect;

static void parse_markers(const char* src, expect* x) {
    const char* p = src;
    while ((p = strstr(p, "//@")) != NULL) {
        p += 3;
        while (*p == ' ' || *p == '\t') p++;
        const char* key = p;
        while (*p && *p != ':' && *p != '\n') p++;
        if (*p != ':') continue;
        size_t klen = (size_t)(p - key);
        p++;
        while (*p == ' ' || *p == '\t') p++;
        const char* val = p;
        while (*p && *p != '\n') p++;
        size_t vlen = (size_t)(p - val);
        if (klen >= 16 || vlen >= 64) continue;
        char k[16], v[64];
        memcpy(k, key, klen); k[klen] = 0;
        memcpy(v, val, vlen); v[vlen] = 0;
        if (strcmp(k, "fail") == 0 || strcmp(k, "warn") == 0) {
            if (x->n < MAXN) { snprintf(x->codes[x->n], 8, "%s", v); x->n++; }
        } else if (strcmp(k, "msg") == 0 && x->n > 0) {
            x->has_msg[x->n - 1] = 1;
            snprintf(x->msg[x->n - 1], 64, "%s", v);
        }
    }
}

static int module_code(const char* c) {
    return strcmp(c, "E5010") == 0 || strcmp(c, "E5020") == 0 || strcmp(c, "E2020") == 0
        || strcmp(c, "E4010") == 0 || strcmp(c, "E6010") == 0;
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

// 对单个 src 文件评分;返回失败数
static size_t grade_file(const char* dir, const char* pkgname, const char* fname, pkg_res* pr) {
    char full[4096];
    snprintf(full, sizeof full, "%s/src/%s", dir, fname);
    char* src = read_file_str(full);
    expect x = {0};
    if (src) { parse_markers(src, &x); free(src); }
    char rel[512];
    snprintf(rel, sizeof rel, "src/%s", fname);

    char produced[MAXN][8];
    char pmsg[MAXN][128];
    int nprod = 0;
    for (size_t i = 0; i < pr->n && nprod < MAXN; i++) {
        if (strcmp(pr->d[i].rel, rel) != 0) continue;
        snprintf(produced[nprod], 8, "%s", pr->d[i].code);
        snprintf(pmsg[nprod], 128, "%s", pr->d[i].msg);
        nprod++;
    }
    int ok = 1;
    char why[256] = {0};
    for (int i = 0; i < nprod && ok; i++) {
        int in = 0;
        for (int j = 0; j < x.n; j++)
            if (strcmp(produced[i], x.codes[j]) == 0) { in = 1; break; }
        if (!in) { snprintf(why, sizeof why, "多余诊断 %s", produced[i]); ok = 0; }
    }
    for (int j = 0; j < x.n && ok; j++) {
        if (!module_code(x.codes[j])) continue;
        int in = 0;
        for (int i = 0; i < nprod; i++)
            if (strcmp(produced[i], x.codes[j]) == 0) { in = 1; break; }
        if (!in) { snprintf(why, sizeof why, "缺模块期望码 %s", x.codes[j]); ok = 0; }
    }
    for (int j = 0; j < x.n && ok; j++) {
        if (!x.has_msg[j]) continue;
        int found = 0;
        for (int i = 0; i < nprod; i++)
            if (strcmp(produced[i], x.codes[j]) == 0 && strstr(pmsg[i], x.msg[j])) found = 1;
        if (!found) { snprintf(why, sizeof why, "%s 消息缺子串 '%s'", x.codes[j], x.msg[j]); ok = 0; }
    }
    if (!ok) {
        fprintf(stderr, "包 %s 文件 %s — %s\n", pkgname, rel, why);
        for (int i = 0; i < nprod; i++)
            fprintf(stderr, "  %s: %s\n", produced[i], pmsg[i]);
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    const char* root = argc > 1 ? argv[1] : "../tests/modules";
    DIR* d = opendir(root);
    if (!d) { fprintf(stderr, "suite_pkg: 无法打开 %s\n", root); return 2; }
    size_t npkgs = 0, nfiles = 0, fails = 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char dir[4096];
        snprintf(dir, sizeof dir, "%s/%s", root, e->d_name);
        if (!has_toml(dir)) continue;
        npkgs++;
        pkg_res pr = ctron_pkg_check(dir);
        char sdir[4096];
        snprintf(sdir, sizeof sdir, "%s/src", dir);
        DIR* ss = opendir(sdir);
        if (ss) {
            struct dirent* sf;
            while ((sf = readdir(ss)) != NULL) {
                if (strcmp(sf->d_name, ".") == 0 || strcmp(sf->d_name, "..") == 0) continue;
                size_t bl = strlen(sf->d_name);
                if (bl < 4 || strcmp(sf->d_name + bl - 3, ".ct") != 0) continue;
                nfiles++;
                fails += grade_file(dir, e->d_name, sf->d_name, &pr);
            }
            closedir(ss);
        }
        ctron_pkg_res_free(&pr);
    }
    closedir(d);
    printf("suite_pkg: %zu packages, %zu files, %zu failures\n", npkgs, nfiles, fails);
    return fails ? 1 : 0;
}
