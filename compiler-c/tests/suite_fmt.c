// suite_fmt.c —— ctron fmt(C 宿主移植)验收:
//   ① 金样逐规则(R1–R8 + 回归钉子,期望输出 = Rust 参考实现生成后冻结);
//   ② 金样幂等 fmt(fmt(x))==fmt(x);
//   ③ 全语料(共享 tests/,跳 roadmap 阶段区)幂等 + trans 等价代理
//      (orig/fmt 双份 parse→trans,产物字节相等;orig trans 失败则跳过该面)。
// 与 compiler-rust/tests/fmt_suite.rs 同一验收口径。
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fmt.h"
#include "lexer.h"
#include "parser.h"
#include "trans.h"

static int fails = 0;

typedef struct {
    const char* name;
    const char* in;
    const char* want; // want_err 时未用
    int want_err;     // 1 = 词法诊断 → fmt 报错退出(规范 R8)
} golden;

static void goldens_check(const char* tag, const char* got, const char* want) {
    if (strcmp(got, want) != 0) {
        printf("FAIL %s\n--- want ---\n%s\n--- got ---\n%s\n", tag, want, got);
        fails++;
    }
}

static char* fmt_of(const char* src, size_t len, int* err) {
    ctron_fmt_result r = ctron_fmt_src(src, len);
    char* out;
    if (r.ndiags > 0) {
        *err = 1;
        out = NULL;
    } else {
        *err = 0;
        out = r.out; // 所有权转移
    }
    // diags 无论成败都要释放
    for (size_t i = 0; i < r.ndiags; i++) {
        free(r.diags[i].code);
        free(r.diags[i].message);
    }
    free(r.diags);
    return out;
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

// ---------- ① + ② 金样 ----------
static const golden G[] = {
    {"R2/R5 空格表+缩进",
     "fn main() {\nlet x=1+2\nvar y = x*3\n}\n",
     "fn main() {\n    let x = 1 + 2\n    var y = x * 3\n}\n", 0},
    {"R2/R3 内联块不拆 + } else 同行",
     "fn f(n: I32) -> Str {\nlet label = if n > 0 { \"pos\" } else { \"neg\" }\n"
     "if n > 0 {\nreturn \"pos\"\n} else {\nreturn \"neg\"\n}\n}\n",
     "fn f(n: I32) -> Str {\n    let label = if n > 0 { \"pos\" } else { \"neg\" }\n"
     "    if n > 0 {\n        return \"pos\"\n    } else {\n        return \"neg\"\n    }\n}\n",
     0},
    {"R1 空行折叠(至多 1 行)",
     "fn a() {}\n\n\n\nfn b() {}\n",
     "fn a() {}\n\nfn b() {}\n", 0},
    {"R4 链断行(同缩进延续,Rust 参考实现输出冻结;相对缩进为 spec R4 v1 遗留)",
     "fn g(xs: List[I32]) -> I32 {\nreturn xs\n.map(twice)\n.filter(gt0)\n.len()\n}\n",
     "fn g(xs: List[I32]) -> I32 {\n    return xs\n    .map(twice)\n    .filter(gt0)\n"
     "    .len()\n}\n",
     0},
    {"R6 注释:独占行/行尾//@ 标记逐字保留",
     "// head\nfn f() {\n// own line\nvar x = 1 // trailing\n//@ panic: marker\n}\n",
     "// head\nfn f() {\n    // own line\n    var x = 1 // trailing\n    //@ panic: marker\n}\n",
     0},
    {"R7 字面量保真(进制/浮点/插值转义零改写)",
     "fn f() {\nlet a = 0xFF\nlet b = 2.5\nlet s = \"hi {name} x\\n\"\n}\n",
     "fn f() {\n    let a = 0xFF\n    let b = 2.5\n    let s = \"hi {name} x\\n\"\n}\n", 0},
    {"R5 闭包管道/range/一元 -",
     "fn f(xs: List[I32], n: I32) {\nlet t = xs.map(|x| x + 1)\nvar s: I32 = 0\n"
     "for i in 0..n {\ns += -1 + i\n}\n}\n",
     "fn f(xs: List[I32], n: I32) {\n    let t = xs.map(|x| x + 1)\n    var s: I32 = 0\n"
     "    for i in 0..n {\n        s += -1 + i\n    }\n}\n",
     0},
    {"R5 前缀 ! & 左侧恒空格(return!/&&!b 回归)",
     "fn f(b: Bool) -> I32 {\nif !b && !b {\nreturn!1\n}\nvar p = &b\nreturn 0\n}\n",
     "fn f(b: Bool) -> I32 {\n    if !b && !b {\n        return !1\n    }\n    var p = &b\n"
     "    return 0\n}\n",
     0},
    {"R2 空块恒 {} + 调用/索引紧贴",
     "fn f(xs: List[I32]) {\nvar y = xs[0]\ng(y, xs[1])\nnoop() {}\n}\n",
     "fn f(xs: List[I32]) {\n    var y = xs[0]\n    g(y, xs[1])\n    noop() {}\n}\n", 0},
    {"R8 词法诊断报错退出(分号)",
     "fn main() {\nvar x = 1;\n}\n",
     "", 1},
    {"R8 无前导空行 + 单换行结尾 + 尾随空白修剪",
     "\n\n\nfn a() {}\n   \n\t\n",
     "fn a() {}\n", 0},
};

static void run_goldens(void) {
    size_t ng = sizeof G / sizeof G[0];
    for (size_t i = 0; i < ng; i++) {
        int err;
        char* out = fmt_of(G[i].in, strlen(G[i].in), &err);
        if (G[i].want_err) {
            if (!err) {
                printf("FAIL %s: 期望词法诊断报错,fmt 却成功\n", G[i].name);
                fails++;
            }
        } else if (err) {
            printf("FAIL %s: 意外语法诊断\n", G[i].name);
            fails++;
        } else {
            char tag[128];
            snprintf(tag, sizeof tag, "%s(金样)", G[i].name);
            goldens_check(tag, out, G[i].want);
            // ② 幂等
            int err2;
            char* out2 = fmt_of(out, strlen(out), &err2);
            if (err2 || strcmp(out2, out) != 0) {
                printf("FAIL %s(幂等): 二次 fmt 改写输出\n", G[i].name);
                fails++;
            }
            free(out2);
        }
        free(out);
    }
    printf("goldens: %zu 组\n", ng);
}

// ---------- ③ 语料幂等 + trans 等价代理 ----------
typedef struct {
    char** items;
    size_t n, cap;
} strvec;

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
            if (strcmp(e->d_name, "roadmap") == 0) continue; // R 泳道阶段区(Rust roadmap_suite 门控)
            walk(path, out);
        } else if (ends_with(e->d_name, ".ct")) {
            sv_push(out, path);
        }
    }
    closedir(d);
}

static int cmp_strv(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

static void run_corpus(const char* root) {
    strvec v = {0};
    walk(root, &v);
    qsort(v.items, v.n, sizeof(char*), cmp_strv);
    size_t n_ok = 0, n_err = 0, n_trans = 0;
    for (size_t i = 0; i < v.n; i++) {
        size_t len;
        char* src = read_file(v.items[i], &len);
        if (!src) continue;
        int err;
        char* out = fmt_of(src, len, &err);
        if (err) {
            n_err++; // 词法脏语料:fmt 报错退出即规范行为(R8)
            free(src);
            free(out);
            continue;
        }
        // 幂等
        int err2;
        char* out2 = fmt_of(out, strlen(out), &err2);
        if (err2 || strcmp(out2, out) != 0) {
            printf("FAIL 幂等: %s\n", v.items[i]);
            fails++;
        }
        free(out2);
        // trans 等价代理:parse(orig) 与 parse(fmt) 双份干净才可比
        ctron_parse_result p1 = ctron_parse_src(src, len);
        ctron_parse_result p2 = ctron_parse_src(out, strlen(out));
        if (p1.ndiags == 0 && p2.ndiags == 0) {
            ctron_trans_result t1 = ctron_trans_file(p1.file);
            ctron_trans_result t2 = ctron_trans_file(p2.file);
            if (!t1.err && !t2.err) {
                n_trans++;
                if (strcmp(t1.code, t2.code) != 0) {
                    printf("FAIL trans 等价: %s(fmt 前后发射 C 分歧)\n", v.items[i]);
                    fails++;
                }
            }
            ctron_trans_result_free(&t1);
            ctron_trans_result_free(&t2);
        }
        ctron_parse_result_free(&p1);
        ctron_parse_result_free(&p2);
        n_ok++;
        free(out);
        free(src);
    }
    for (size_t i = 0; i < v.n; i++) free(v.items[i]);
    free(v.items);
    printf("corpus: %zu 幂等绿 / %zu 词法脏(报错退出) / %zu trans 等价(根 %s)\n", n_ok, n_err,
           n_trans, root);
}

int main(int argc, char** argv) {
    run_goldens();
    run_corpus(argc > 1 ? argv[1] : "../tests");
    if (fails > 0) {
        printf("suite_fmt: %d FAIL\n", fails);
        return 1;
    }
    printf("suite_fmt: OK\n");
    return 0;
}
