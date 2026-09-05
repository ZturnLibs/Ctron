// test_parse.c —— 解析器单元测试(锚定 §1.7/§1.8 与语料验收行为;C 版独立用例)。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

static int g_checks = 0, g_fails = 0;
#define CHECK(cond)                                                             \
    do {                                                                        \
        g_checks++;                                                             \
        if (!(cond)) { g_fails++; fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); } \
    } while (0)

static ctron_parse_result P(const char* s) { return ctron_parse_src(s, strlen(s)); }

static int has_code(const ctron_parse_result* r, const char* code) {
    for (size_t i = 0; i < r->ndiags; i++)
        if (strcmp(r->diags[i].code, code) == 0) return 1;
    return 0;
}
static int any_msg_has(const ctron_parse_result* r, const char* sub) {
    for (size_t i = 0; i < r->ndiags; i++)
        if (strstr(r->diags[i].message, sub)) return 1;
    return 0;
}
static void expect_clean(const char* src, const char* what) {
    ctron_parse_result r = P(src);
    if (r.ndiags != 0) {
        g_checks++;
        g_fails++;
        fprintf(stderr, "FAIL [%s] 应零诊断,实际 %zu:\n", what, r.ndiags);
        for (size_t i = 0; i < r.ndiags; i++)
            fprintf(stderr, "  %u:%u %s: %s\n", r.diags[i].span.line, r.diags[i].span.col,
                    r.diags[i].code, r.diags[i].message);
    } else g_checks++;
    ctron_parse_result_free(&r);
}

static void test_decls_and_recovery(void) {
    // static var → E3030 恢复
    ctron_parse_result r = P("static var COUNTER: I32 = 0");
    CHECK(has_code(&r, "E3030"));
    ctron_parse_result_free(&r);
    // else 必须与 } 同行
    r = P("fn f() {\n    if a {\n    }\n    else {\n    }\n}");
    CHECK(has_code(&r, "E1001") && any_msg_has(&r, "else"));
    ctron_parse_result_free(&r);
    r = P("fn f() {\n    if a {\n    } else {\n    }\n}");
    CHECK(r.ndiags == 0);
    ctron_parse_result_free(&r);
    // 比较不可链
    r = P("fn f(x: I32, y: I32, z: I32) { if x < y < z { return 1 } }");
    CHECK(has_code(&r, "E1001") && any_msg_has(&r, "不可链"));
    ctron_parse_result_free(&r);
    // 无效赋值目标
    r = P("fn f() { 1 + 2 = 3 }");
    CHECK(has_code(&r, "E1001") && any_msg_has(&r, "赋值目标"));
    ctron_parse_result_free(&r);
}

static void test_feature_surface(void) {
    // 泛型 + 类型实参 + 定长数组/切片消歧
    expect_clean("fn f(xs: I32[]) -> Void { return void }\nfn g(m: Map[Str, I32]) -> Void { return void }\n"
                 "fn h() -> Void { var buf: I32[3] = [1, 2, 3]\n return void }\n"
                 "fn k() -> Atomic[I32] { return x }", "类型三态");
    // 声明面:use 组/trait 超类/derive/pub(pkg)/extern
    expect_clean("use std.net.{TcpListener, Request}\n"
                 "trait Env: Clock + Named { }\n"
                 "@derive(Show, Eq)\nstruct Pixel {\n    let x: I32\n}\n"
                 "pub(pkg) fn triple(x: I32) -> I32 { return x * 3 }\n"
                 "#[trusted]\nextern \"c\" fn ctron_add(a: I64, b: I64) -> I64\n"
                 "class Box2 {\n    pub prop size: I64 {\n        return 1\n    }\n}\n"
                 "trait Clock: Cap {\n    fn now(&self) -> U64\n    pub prop size: I64\n}", "声明面");
    // trait 无体方法保留 + 空 impl 合法
    expect_clean("trait Drop { fn drop(&self) }\nimpl Drop for Ticket { }\n", "trait/impl");
    // 表达式面:闭包/scope/own/match/构造字面量/链式
    expect_clean("fn t() {\n    scope { |s|\n        let t = s.spawn(|| 42)\n        t.join()\n    }\n"
                 "    own (arena) { }\n"
                 "    let p = Point { x: 1, y: 2 }\n"
                 "    match p {\n        Pt { x, y } => x\n        _ => 0\n    }\n"
                 "    let q = opt.map(f).or(0)\n"
                 "    let v = xs[i].len\n"
                 "    let z = Simd[F32, 4].splat(v)\n"
                 "    let ch = Channel[I32](4)\n"
                 "    let c = |var a: I32| -> I32 { return a * 2 }\n"
                 "    return void\n}", "表达式面");
    // 语句/模式/循环
    expect_clean("fn t(pair: (I32, I32)) {\n    let (a, b) = pair\n    for _ in 0..4 { }\n"
                 "    var x = 1\n    x += 1\n    b.x = 10\n    while x < 10 { x = x + 1 }\n"
                 "    return Ok(v)\n}", "语句面");
}

static void test_deep_nesting_capped(void) {
    // 2000 个 '(' → 恢复路径不得栈溢出,须产出"嵌套过深"
    size_t n = 2000;
    char* src = (char*)malloc(n * 2 + 64);
    size_t o = 0;
    o += (size_t)snprintf(src + o, 64, "fn f() -> I32 { return ");
    for (size_t i = 0; i < n; i++) src[o++] = '(';
    src[o++] = '1';
    for (size_t i = 0; i < n; i++) src[o++] = ')';
    src[o++] = ' '; src[o++] = '}'; src[o] = '\0';
    ctron_parse_result r = P(src);
    CHECK(has_code(&r, "E1001") && any_msg_has(&r, "嵌套过深"));
    ctron_parse_result_free(&r);
    free(src);
    // 深层类型
    char* src2 = (char*)malloc(n * 2 + 64);
    o = 0;
    o += (size_t)snprintf(src2 + o, 64, "fn g(x: ");
    for (size_t i = 0; i < 1500; i++) src2[o++] = '(';
    o += (size_t)snprintf(src2 + o, 32, "I64");
    for (size_t i = 0; i < 1500; i++) src2[o++] = ')';
    o += (size_t)snprintf(src2 + o, 32, ") -> Void { return void }");
    src2[o] = '\0';
    r = P(src2);
    CHECK(has_code(&r, "E1001") && any_msg_has(&r, "嵌套过深"));
    ctron_parse_result_free(&r);
    free(src2);
}

static void test_ast_show_deterministic(void) {
    // AST 文本两次打印一致(确定性契约雏形)
    ctron_parse_result r = P("fn main() -> I32 { return 1 + 2 }");
    CHECK(r.ndiags == 0);
    FILE* f1 = tmpfile();
    FILE* f2 = tmpfile();
    ctron_file_show(r.file, f1);
    ctron_file_show(r.file, f2);
    fflush(f1);
    fflush(f2);
    rewind(f1);
    rewind(f2);
    int c;
    int same = 1;
    while ((c = fgetc(f1)) != EOF) {
        if (fgetc(f2) != c) { same = 0; break; }
    }
    if (fgetc(f2) != EOF) same = 0;
    CHECK(same);
    fclose(f1);
    fclose(f2);
    ctron_parse_result_free(&r);
}

int main(void) {
    test_decls_and_recovery();
    test_feature_surface();
    test_deep_nesting_capped();
    test_ast_show_deterministic();
    printf("test_parse: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
