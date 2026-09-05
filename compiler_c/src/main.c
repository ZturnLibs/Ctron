// main.c —— ctronc 命令行(C 版 Ctron 编译器)。
// 子命令:version | lex | parse | check | run | pkg
// check/pkg 支持 --format=json(规范 §10.2 冻结 schema:diagnostics[] =
//   {code, severity, message, file, span{line_start,col_start,line_end,col_end}, notes[], fixes[]})。
// 位置信息诚实输出:词法/解析诊断有行列;语义/模块级诊断当前无位置(span 全 0)。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "pkg.h"
#include "sem.h"
#include "rt.h"
#include "trans.h"

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

// ---------- JSON 诊断输出(§10.2) ----------
typedef struct {
    const char* code;
    const char* severity; // error | warning
    const char* message;
    const char* file;
    unsigned line, col;   // 未知为 0
} jdiag;

static void json_puts(const char* s) {
    if (!s) s = "";
    putchar('"');
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else putchar(*p);
        }
    }
    putchar('"');
}

static void print_diags_json(const jdiag* d, size_t n) {
    printf("{\n  \"diagnostics\": [");
    for (size_t i = 0; i < n; i++) {
        printf("%s\n    {\"code\": ", i ? "," : "");
        json_puts(d[i].code);
        printf(", \"severity\": ");
        json_puts(d[i].severity);
        printf(", \"message\": ");
        json_puts(d[i].message);
        printf(", \"file\": ");
        json_puts(d[i].file);
        printf(", \"span\": {\"line_start\": %u, \"col_start\": %u, \"line_end\": %u, \"col_end\": %u}",
               d[i].line, d[i].col, d[i].line, d[i].col);
        printf(", \"notes\": [], \"fixes\": []}");
    }
    printf("%s]\n}\n", n ? "\n  " : "");
}

static const char* sev_of(const char* code) {
    return code && code[0] == 'W' ? "warning" : "error";
}

static int has_flag(int argc, char** argv, int from, const char* flag) {
    for (int i = from; i < argc; i++)
        if (strcmp(argv[i], flag) == 0) return 1;
    return 0;
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
        fprintf(stderr, "usage: ctronc check <file> [--format=json]\n");
        return 2;
    }
    const char* path = argv[2];
    int as_json = has_flag(argc, argv, 3, "--format=json");
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
    if (as_json) {
        jdiag* jd = total ? (jdiag*)calloc(total, sizeof(jdiag)) : NULL;
        size_t n = 0;
        for (size_t i = 0; i < pr.ndiags; i++) {
            jd[n].code = pr.diags[i].code;
            jd[n].severity = sev_of(pr.diags[i].code);
            jd[n].message = pr.diags[i].message;
            jd[n].file = path;
            jd[n].line = pr.diags[i].span.line;
            jd[n].col = pr.diags[i].span.col;
            n++;
        }
        for (size_t i = 0; i < sr.ndiags; i++) {
            jd[n].code = sr.diags[i].code;
            jd[n].severity = sev_of(sr.diags[i].code);
            jd[n].message = sr.diags[i].message;
            jd[n].file = path;
            n++;
        }
        print_diags_json(jd, n);
        free(jd);
    } else {
        for (size_t i = 0; i < pr.ndiags; i++) {
            ctron_diag* d = &pr.diags[i];
            printf("%s:%u:%u %s: %s\n", path, d->span.line, d->span.col, d->code, d->message);
        }
        for (size_t i = 0; i < sr.ndiags; i++) {
            printf("%s %s: %s\n", path, sr.diags[i].code, sr.diags[i].message);
        }
        if (!as_json) printf("%zu diagnostics\n", total);
    }
    int ok = (total == 0);
    free(sr.diags);
    ctron_arena_free(arena);
    ctron_parse_result_free(&pr);
    free(src);
    return ok ? 0 : 1;
}

static int cmd_pkg(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: ctronc pkg <dir> [--format=json]\n");
        return 2;
    }
    const char* root = argv[2];
    int as_json = has_flag(argc, argv, 3, "--format=json");
    pkg_res r = ctron_pkg_check(root);
    if (as_json) {
        jdiag* jd = r.n ? (jdiag*)calloc(r.n, sizeof(jdiag)) : NULL;
        for (size_t i = 0; i < r.n; i++) {
            char* path = (char*)malloc(strlen(root) + strlen(r.d[i].rel) + 2);
            sprintf(path, "%s/%s", root, r.d[i].rel);
            jd[i].code = r.d[i].code;
            jd[i].severity = sev_of(r.d[i].code);
            jd[i].message = r.d[i].msg;
            jd[i].file = path;
        }
        print_diags_json(jd, r.n);
        if (jd) {
            for (size_t i = 0; i < r.n; i++) free((void*)jd[i].file);
            free(jd);
        }
    } else {
        for (size_t i = 0; i < r.n; i++)
            printf("%s/%s %s: %s\n", root, r.d[i].rel, r.d[i].code, r.d[i].msg);
        printf("%zu diagnostics\n", r.n);
    }
    int ok = (r.n == 0);
    ctron_pkg_res_free(&r);
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

// trans/build —— C10-a:Ctron → C 转译后端。test 块文件用 ctronc test 执行。
static int cmd_trans(int argc, char** argv) {
    const char* path = NULL;
    const char* out = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else if (!path) path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "usage: ctronc trans <file> [-o out.c]\n");
        return 2;
    }
    size_t len;
    char* src = read_file(path, &len);
    if (!src) { fprintf(stderr, "无法读取 %s\n", path); return 2; }
    ctron_parse_result pr = ctron_parse_src(src, len);
    free(src);
    if (pr.ndiags) {
        for (size_t i = 0; i < pr.ndiags; i++)
            printf("%s:%u:%u %s: %s\n", path, pr.diags[i].span.line, pr.diags[i].span.col,
                   pr.diags[i].code, pr.diags[i].message);
        ctron_parse_result_free(&pr);
        return 1;
    }
    ctron_trans_result tr = ctron_trans_file(pr.file);
    ctron_parse_result_free(&pr);
    if (tr.err) {
        fprintf(stderr, "trans: %s\n", tr.err);
        ctron_trans_result_free(&tr);
        return 2;
    }
    if (out) {
        FILE* f = fopen(out, "wb");
        if (!f) { fprintf(stderr, "无法写入 %s\n", out); return 2; }
        fwrite(tr.code, 1, strlen(tr.code), f);
        fclose(f);
    } else {
        printf("%s", tr.code);
    }
    ctron_trans_result_free(&tr);
    return 0;
}

static int cmd_build(int argc, char** argv) {
    const char* path = NULL;
    const char* out = "a.out";
    int keep = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "-k")) keep = 1;
        else if (!path) path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "usage: ctronc build <file> [-o bin] [-k]\n");
        return 2;
    }
    size_t len;
    char* src = read_file(path, &len);
    if (!src) { fprintf(stderr, "无法读取 %s\n", path); return 2; }
    ctron_parse_result pr = ctron_parse_src(src, len);
    free(src);
    if (pr.ndiags) {
        for (size_t i = 0; i < pr.ndiags; i++)
            printf("%s:%u:%u %s: %s\n", path, pr.diags[i].span.line, pr.diags[i].span.col,
                   pr.diags[i].code, pr.diags[i].message);
        ctron_parse_result_free(&pr);
        return 1;
    }
    ctron_trans_result tr = ctron_trans_file(pr.file);
    ctron_parse_result_free(&pr);
    if (tr.err) {
        fprintf(stderr, "trans: %s\n", tr.err);
        ctron_trans_result_free(&tr);
        return 2;
    }
    const char* cpath = keep ? "ctron_build_out.c" : "/tmp/ctron_build_out.c";
    FILE* f = fopen(cpath, "wb");
    if (!f) { fprintf(stderr, "无法写入 %s\n", cpath); return 2; }
    fwrite(tr.code, 1, strlen(tr.code), f);
    fclose(f);
    ctron_trans_result_free(&tr);
    char cmd[1024];
    snprintf(cmd, sizeof cmd, "cc -std=c11 -O2 -o '%s' '%s'", out, cpath);
    int rc = system(cmd);
    if (!keep) remove(cpath);
    if (rc != 0) {
        fprintf(stderr, "build: cc 失败\n");
        return 1;
    }
    printf("build: %s\n", out);
    return 0;
}

// test —— 运行文件内全部 test 块(C 解释器);输出 rr.out;panic/断言失败 exit 1。
static int cmd_test(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: ctronc test <file>\n");
        return 2;
    }
    size_t len;
    char* src = read_file(argv[2], &len);
    if (!src) { fprintf(stderr, "无法读取 %s\n", argv[2]); return 2; }
    ctron_parse_result pr = ctron_parse_src(src, len);
    free(src);
    if (pr.ndiags) {
        for (size_t i = 0; i < pr.ndiags; i++)
            printf("%s:%u:%u %s: %s\n", argv[2], pr.diags[i].span.line, pr.diags[i].span.col,
                   pr.diags[i].code, pr.diags[i].message);
        ctron_parse_result_free(&pr);
        return 1;
    }
    rt_run rr = ctron_rt_run(pr.file);
    if (rr.out) printf("%s", rr.out);
    int ok = rr.st == RT_OK && rr.tests_run == rr.tests_total && rr.tests_total > 0;
    if (!ok && rr.msg) fprintf(stderr, "%s\n", rr.msg);
    size_t run = rr.tests_run, total = rr.tests_total;
    ctron_rt_run_free(&rr);
    ctron_parse_result_free(&pr);
    if (!ok) return 1;
    printf("%zu/%zu tests passed\n", run, total);
    return 0;
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
    if (strcmp(sub, "pkg") == 0) return cmd_pkg(argc, argv);
    if (strcmp(sub, "run") == 0) return cmd_run(argc, argv);
    if (strcmp(sub, "test") == 0) return cmd_test(argc, argv);
    if (strcmp(sub, "trans") == 0) return cmd_trans(argc, argv);
    if (strcmp(sub, "build") == 0) return cmd_build(argc, argv);
    fprintf(stderr, "usage: ctronc <version|lex|parse|check|pkg|run|test|trans|build> [args]\n");
    return 2;
}
