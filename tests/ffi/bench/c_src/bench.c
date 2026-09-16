/* tests/ffi/bench —— C 侧:被测热点(noinline,与真实跨 TU FFI 同形)+ 纯 C 循环基线。 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../../ctron_abi.h"

/* 32 字节 struct(镜像 #[repr(c)] BPt 声明序) */
typedef struct { int64_t a; double b; int64_t c; int64_t d; } BPt;

__attribute__((noinline)) int64_t b_add(int64_t a, int64_t b) {
    return a + b;
}

int64_t b_loop_calls(int64_t n, int64_t x) {
    int64_t acc = x;
    for (int64_t i = 0; i < n; i++) {
        acc = b_add(acc, 1);
    }
    return acc;
}

__attribute__((noinline)) int64_t b_inc(int64_t x) {
    return x + 1;
}

__attribute__((noinline)) int64_t b_apply(ct_fn1 f, int64_t x) {
    return f(x);
}

int64_t b_loop_cb(int64_t n, int64_t x) {
    /* 与 Ctron 侧同形:每迭代同样经 noinline 包装 b_apply(否则 Ctron 每_op 两次
     * 调用 vs 基线一次,对比失真);volatile 防 gcc 对常量指针 devirt 内联 */
    volatile ct_fn1 f = b_inc;
    int64_t acc = x;
    for (int64_t i = 0; i < n; i++) {
        acc = b_apply(f, acc);
    }
    return acc;
}

__attribute__((noinline)) double b_pt_sum(BPt p) {
    return (double)(p.a + p.c + p.d) + p.b;
}

int64_t b_loop_struct(int64_t n, int64_t a, double b, int64_t c, int64_t d) {
    BPt p;
    p.a = a;
    p.b = b;
    p.c = c;
    p.d = d;
    double acc = 0.0;
    for (int64_t i = 0; i < n; i++) {
        acc = b_pt_sum(p);
    }
    return (int64_t)acc;
}

static char b_buffer[257];

const char* b_buf(void) {
    if (b_buffer[0] == 0) {
        memset(b_buffer, 'h', 256);
        b_buffer[256] = 0;
    }
    return b_buffer;
}

/* 纯 C 编组基线:malloc + memcpy + free(对照 Ctron str_from_c 的 arena 深拷) */
int64_t b_loop_str(int64_t n) {
    const char* s = b_buf();
    int64_t acc = 0;
    for (int64_t i = 0; i < n; i++) {
        char* c = (char*)malloc(257);
        memcpy(c, s, 257);
        acc += c[0];
        free(c);
    }
    return acc;
}
