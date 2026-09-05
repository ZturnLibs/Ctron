// suite_lex.c —— 集成验收:遍历共享语料目录(<root>/tests),递归收集 *.ct,
// 断言每个文件词法零诊断且记号数 >= 2(与 Rust 版 lex_suite 同一验收口径)。
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"

typedef struct { char** items; size_t n, cap; } strvec;

static void sv_push(strvec* v, const char* p) {
    if (v->n == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 64;
        v->items = (char**)realloc(v->items, v->cap * sizeof(char*));
        if (!v->items) abort();
    }
    v->items[v->n++] = strdup(p);
}

static int ends_with(const char* s, const char* suf) {
    size_t n = strlen(s), m = strlen(suf);
    return n >= m && strcmp(s + n - m, suf) == 0;
}

static void walk(const char* dir, strvec* out) {
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        if (e->d_type == DT_DIR) walk(path, out);
        else if (ends_with(e->d_name, ".ct")) sv_push(out, path);
    }
    closedir(d);
}

static char* read_file(const char* path, size_t* out_len) {
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
    *out_len = got;
    return buf;
}

int main(int argc, char** argv) {
    const char* root = argc > 1 ? argv[1] : "../tests";
    strvec files = {0};
    walk(root, &files);
    if (files.n < 57) {
        fprintf(stderr, "suite_lex: 测试文件应不少于 57 个,实际 %zu\n", files.n);
        return 1;
    }
    size_t fails = 0;
    for (size_t i = 0; i < files.n; i++) {
        size_t len;
        char* src = read_file(files.items[i], &len);
        if (!src) { fprintf(stderr, "无法读取 %s\n", files.items[i]); fails++; continue; }
        ctron_lex_result r = ctron_lex(src, len);
        if (r.ndiags != 0 || r.ntoks < 2) {
            fprintf(stderr, "词法失败: %s (%zu diagnostics)\n", files.items[i], r.ndiags);
            for (size_t j = 0; j < r.ndiags; j++)
                fprintf(stderr, "  %u:%u %s: %s\n", r.diags[j].span.line, r.diags[j].span.col,
                        r.diags[j].code, r.diags[j].message);
            fails++;
        }
        ctron_lex_result_free(&r);
        free(src);
    }
    printf("suite_lex: %zu files, %zu failures\n", files.n, fails);
    for (size_t i = 0; i < files.n; i++) free(files.items[i]);
    free(files.items);
    return fails ? 1 : 0;
}
