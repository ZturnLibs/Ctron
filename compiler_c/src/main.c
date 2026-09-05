// main.c —— ctronc 命令行(C 版 Ctron 编译器)。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "sem.h"
#include "rt.h"

static const char* VERSION = "0.1.0";

static char* read_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = '\0';
    *out_len = got;
    return buf;
}

static int cmd_lex(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: ctronc lex <file>\n");
        return 2;
    }
    size_t len;
    char* src = read_file(argv[2], &len);
    if (!src) {
        fprintf(stderr, "无法读取 %s\n", argv[2]);
        return 2;
    }
    ctron_lex_result r = ctron_lex(src, len);
    for (size_t i = 0; i < r.ndiags; i++) {
        ctron_diag* d = &r.diags[i];
        printf("%s:%u:%u %s: %s\n", argv[2], d->span.line, d->span.col, d->code, d->message);
    }
    printf("%zu tokens, %zu diagnostics\n", r.ntoks, r.ndiags);
    int ok = (r.ndiags == 0);
    ctron_lex_result_free(&r);
    free(src);
    return ok ? 0 : 1;
}

static int cmd_parse(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: ctronc parse <file> [--ast]\n");
        return 2;
    }
    const char* path = argv[2];
    int show_ast = 0;
    for (int i = 3; i < argc; i++)
        if (strcmp(argv[i], "--ast") == 0) show_ast = 1;
    size_t len;
    char* src = read_file(path, &len);
    if (!src) {
        fprintf(stderr, "无法读取 %s\n", path);
        return 2;
    }
    ctron_parse_result r = ctron_parse_src(src, len);
    for (size_t i = 0; i < r.ndiags; i++) {
        ctron_diag* d = &r.diags[i];
        printf("%s:%u:%u %s: %s\n", path, d->span.line, d->span.col, d->code, d->message);
    }
    if (show_ast) {
        ctron_file_show(r.file, stdout);
    } else {
        printf("%zu decls, %zu diagnostics\n", r.file->ndecls, r.ndiags);
    }
    int ok = (r.ndiags == 0);
    ctron_parse_result_free(&r);
    free(src);
    return ok ? 0 : 1;
}

static int cmd_check(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: ctronc check <file>\n");
        return 2;
    }
    const char* path = argv[2];
    size_t len;
    char* src = read_file(path, &len);
    if (!src) {
        fprintf(stderr, "无法读取 %s\n", path);
        return 2;
    }
    ctron_parse_result pr = ctron_parse_src(src, len);
    ctron_arena* arena = ctron_arena_new();
    ctron_sem_result sr = ctron_sem_check(pr.file, arena);
    size_t total = pr.ndiags + sr.ndiags;
    for (size_t i = 0; i < pr.ndiags; i++) {
        ctron_diag* d = &pr.diags[i];
        printf("%s:%u:%u %s: %s\n", path, d->span.line, d->span.col, d->code, d->message);
    }
    for (size_t i = 0; i < sr.ndiags; i++) {
        printf("%s %s: %s\n", path, sr.diags[i].code, sr.diags[i].message);
    }
    printf("%zu diagnostics\n", total);
    int ok = (total == 0);
    free(sr.diags);
    ctron_arena_free(arena);
    ctron_parse_result_free(&pr);
    free(src);
    return ok ? 0 : 1;
}

static int cmd_run(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: ctronc run <file>\n");
        return 2;
    }
    size_t len;
    char* src = read_file(argv[2], &len);
    if (!src) {
        fprintf(stderr, "无法读取 %s\n", argv[2]);
        return 2;
    }
    ctron_parse_result pr = ctron_parse_src(src, len);
    if (pr.ndiags) {
        for (size_t i = 0; i < pr.ndiags; i++)
            printf("%s:%u:%u %s: %s\n", argv[2], pr.diags[i].span.line, pr.diags[i].span.col,
                   pr.diags[i].code, pr.diags[i].message);
        ctron_parse_result_free(&pr);
        free(src);
        return 1;
    }
    rt_run rr = ctron_rt_run_main(pr.file);
    if (rr.out) printf("%s", rr.out);
    if (rr.msg) fprintf(stderr, "%s\n", rr.msg);
    int rc = (int)(rr.exit_code & 0xFF);
    if (rr.st == RT_PANIC || rr.st == RT_ERROR) rc = 1;
    ctron_rt_run_free(&rr);
    ctron_parse_result_free(&pr);
    free(src);
    return rc;
}

int main(int argc, char** argv) {
    const char* sub = argc > 1 ? argv[1] : "";
    if (strcmp(sub, "version") == 0) {
        printf("ctronc %s (C 实现)\n", VERSION);
        return 0;
    }
    if (strcmp(sub, "lex") == 0) return cmd_lex(argc, argv);
    if (strcmp(sub, "parse") == 0) return cmd_parse(argc, argv);
    if (strcmp(sub, "check") == 0) return cmd_check(argc, argv);
    if (strcmp(sub, "run") == 0) return cmd_run(argc, argv);
    fprintf(stderr, "usage: ctronc <version|lex|parse|check|run> [args]\n");
    return 2;
}
