// suite_parse.c —— C2 集成验收:tests/ 全部 .ct 文件过解析器。
// 预期:仅 01c_parse.neg.ct 报 E1001、06_static_var.neg.ct 报 E3030,其余零诊断。
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

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
        if (e->d_type == DT_DIR) {
            if (strcmp(e->d_name, "roadmap") == 0) continue; // R 泳道阶段区(Rust roadmap_suite 门控,C 版随新语法实现后纳入)
            walk(path, out);
        }
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

static int has_code(const ctron_parse_result* r, const char* code) {
    for (size_t i = 0; i < r->ndiags; i++)
        if (strcmp(r->diags[i].code, code) == 0) return 1;
    return 0;
}

int main(int argc, char** argv) {
    const char* root = argc > 1 ? argv[1] : "../tests";
    strvec files = {0};
    walk(root, &files);
    if (files.n < 61) {
        fprintf(stderr, "suite_parse: 测试文件应不少于 61 个,实际 %zu\n", files.n);
        return 1;
    }
    size_t fails = 0;
    for (size_t i = 0; i < files.n; i++) {
        const char* name = strrchr(files.items[i], '/');
        name = name ? name + 1 : files.items[i];
        size_t len;
        char* src = read_file(files.items[i], &len);
        if (!src) { fprintf(stderr, "无法读取 %s\n", files.items[i]); fails++; continue; }
        ctron_parse_result r = ctron_parse_src(src, len);
        int ok;
        if (strcmp(name, "01c_parse.neg.ct") == 0) ok = has_code(&r, "E1001");
        else if (strcmp(name, "06_static_var.neg.ct") == 0) ok = has_code(&r, "E3030");
        else ok = (r.ndiags == 0);
        if (!ok) {
            fprintf(stderr, "解析不符合预期: %s (%zu diagnostics)\n", files.items[i], r.ndiags);
            for (size_t j = 0; j < r.ndiags; j++)
                fprintf(stderr, "  %u:%u %s: %s\n", r.diags[j].span.line, r.diags[j].span.col,
                        r.diags[j].code, r.diags[j].message);
            fails++;
        }
        ctron_parse_result_free(&r);
        free(src);
    }
    printf("suite_parse: %zu files, %zu failures\n", files.n, fails);
    for (size_t i = 0; i < files.n; i++) free(files.items[i]);
    free(files.items);
    return fails ? 1 : 0;
}
