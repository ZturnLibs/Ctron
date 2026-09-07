// suite_sem.c —— C3 语义验收:遍历 tests/ 全部 .ct,按文件头 marker 评估。
// 规则(对"已实现"检查诚实评分,防误报):
//   已实现集 = {E2010,E2030,E3010,E3020,E3031,E4020,E6020,E4030,E3050,E3060,W8010,W8020}
//             ∪ 解析层 {E1001,E3030}
//   pass ⟺ produced ⊆ expected ∧ (expected ∩ implemented) ⊆ produced ∧ msg 子串满足
// 模块级(E5010/E5020/E2020/E4010/E6010)与分配效果 E3040 不在此轮断言(见 suite_pkg)。
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "parser.h"
#include "sem.h"

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

// marker 解析:每行 //@ key: value
#define MAXEXP 16
typedef struct { char codes[MAXEXP][8]; int msg[MAXEXP]; char want_msg[MAXEXP][64]; int n; char target[8]; } expect;

static void add_expect(expect* x, const char* code, const char* msg) {
    for (int i = 0; i < x->n; i++)
        if (strcmp(x->codes[i], code) == 0) {
            if (msg) { x->msg[i] = 1; snprintf(x->want_msg[i], 64, "%s", msg); }
            return;
        }
    if (x->n >= MAXEXP) return;
    snprintf(x->codes[x->n], 8, "%s", code);
    x->msg[x->n] = msg ? 1 : 0;
    if (msg) snprintf(x->want_msg[x->n], 64, "%s", msg);
    x->n++;
}

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
        char k[16], v[96];
        if (klen < 16) {
            memcpy(k, key, klen); k[klen] = 0;
            size_t m = vlen < 95 ? vlen : 95;
            memcpy(v, val, m); v[m] = 0;
            if (strcmp(k, "fail") == 0 || strcmp(k, "warn") == 0) {
                // 值可能带多个码?规范单码;容忍逗号/空格多码
                char* save = NULL;
                for (char* t = strtok_r(v, ", ", &save); t; t = strtok_r(NULL, ", ", &save))
                    add_expect(x, t, NULL);
            } else if (strcmp(k, "target") == 0) {
                snprintf(x->target, 8, "%s", v);
            } else if (strcmp(k, "msg") == 0 && x->n > 0) {
                add_expect(x, x->codes[x->n - 1], v); // 关联最近一个 fail/warn
            }
        }
    }
}

static int implemented(const char* code) {
    static const char* const S[] = {"E2010", "E2030", "E3010", "E3020", "E3031", "E4020",
                                    "E6020", "E4030", "E3050", "E3060", "E3040",
                                    "W8010", "W8020", "W8040", "E1001", "E3030"};
    for (size_t i = 0; i < sizeof S / sizeof S[0]; i++)
        if (strcmp(code, S[i]) == 0) return 1;
    return 0;
}

int main(int argc, char** argv) {
    const char* root = argc > 1 ? argv[1] : "../tests";
    strvec files = {0};
    walk(root, &files);
    if (files.n < 61) {
        fprintf(stderr, "suite_sem: 测试文件应不少于 61 个,实际 %zu\n", files.n);
        return 1;
    }
    size_t fails = 0;
    for (size_t fi = 0; fi < files.n; fi++) {
        size_t len;
        char* src = read_file(files.items[fi], &len);
        if (!src) { fprintf(stderr, "无法读取 %s\n", files.items[fi]); fails++; continue; }
        expect x = {0};
        parse_markers(src, &x);
        ctron_parse_result pr = ctron_parse_src(src, len);
        ctron_arena* arena = ctron_arena_new();
        int profile = SEM_FULL;
        if (strcmp(x.target, "bare") == 0) profile = SEM_BARE;
        ctron_sem_result sr = ctron_sem_check_mode(pr.file, arena, profile);

        // produced = parse ∪ sem 的码集合
        char produced[MAXEXP][8];
        int nprod = 0;
        int has_msg[MAXEXP];
        char got_msg[MAXEXP][256];
        for (size_t i = 0; i < pr.ndiags; i++) {
            const char* c = pr.diags[i].code;
            int seen = 0;
            for (int j = 0; j < nprod; j++)
                if (strcmp(produced[j], c) == 0) { seen = 1; break; }
            if (!seen && nprod < MAXEXP) {
                snprintf(produced[nprod], 8, "%s", c);
                has_msg[nprod] = 1;
                snprintf(got_msg[nprod], 256, "%s", pr.diags[i].message);
                nprod++;
            }
        }
        for (size_t i = 0; i < sr.ndiags; i++) {
            const char* c = sr.diags[i].code;
            int seen = 0;
            for (int j = 0; j < nprod; j++)
                if (strcmp(produced[j], c) == 0) { seen = 1; break; }
            if (!seen && nprod < MAXEXP) {
                snprintf(produced[nprod], 8, "%s", c);
                has_msg[nprod] = 1;
                snprintf(got_msg[nprod], 256, "%s", sr.diags[i].message);
                nprod++;
            }
        }

        int ok = 1;
        char why[512] = {0};
        // (a) produced ⊆ expected
        for (int i = 0; i < nprod && ok; i++) {
            int in_exp = 0;
            for (int j = 0; j < x.n; j++)
                if (strcmp(produced[i], x.codes[j]) == 0) { in_exp = 1; break; }
            if (!in_exp) {
                snprintf(why, sizeof why, "多余诊断 %s", produced[i]);
                ok = 0;
            }
        }
        // (b) expected ∩ implemented ⊆ produced
        for (int j = 0; j < x.n && ok; j++) {
            if (!implemented(x.codes[j])) continue;
            int in_prod = 0;
            for (int i = 0; i < nprod; i++)
                if (strcmp(produced[i], x.codes[j]) == 0) { in_prod = 1; break; }
            if (!in_prod) {
                snprintf(why, sizeof why, "缺已实现期望码 %s", x.codes[j]);
                ok = 0;
            }
        }
        // (c) msg 子串(仅已实现码;解析层 E1001/E3030 为中文句式,不断英文子串)
        for (int j = 0; j < x.n && ok; j++) {
            if (!x.msg[j] || !implemented(x.codes[j])) continue;
            if (strcmp(x.codes[j], "E1001") == 0 || strcmp(x.codes[j], "E3030") == 0) continue;
            int found = 0;
            for (int i = 0; i < nprod; i++)
                if (strcmp(produced[i], x.codes[j]) == 0 && strstr(got_msg[i], x.want_msg[j])) found = 1;
            if (!found) {
                snprintf(why, sizeof why, "%s 消息缺子串 '%s'", x.codes[j], x.want_msg[j]);
                ok = 0;
            }
        }
        if (!ok) {
            fails++;
            fprintf(stderr, "语义不符合预期: %s — %s\n", files.items[fi], why);
            fprintf(stderr, "  expected:");
            for (int j = 0; j < x.n; j++) fprintf(stderr, " %s", x.codes[j]);
            fprintf(stderr, "\n  produced:");
            for (int i = 0; i < nprod; i++) fprintf(stderr, " %s", produced[i]);
            fprintf(stderr, "\n");
            for (size_t i = 0; i < sr.ndiags; i++)
                fprintf(stderr, "  sem: %s %s\n", sr.diags[i].code, sr.diags[i].message);
        }
        free(sr.diags);
        ctron_arena_free(arena);
        ctron_parse_result_free(&pr);
        free(src);
    }
    printf("suite_sem: %zu files, %zu failures\n", files.n, fails);
    for (size_t i = 0; i < files.n; i++) free(files.items[i]);
    free(files.items);
    return fails ? 1 : 0;
}
