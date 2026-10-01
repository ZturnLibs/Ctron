/* tests/lang/bench/src/ref.c —— T44 C 同构对照(§9.4 P3)
 * 与 main.ct 逐步同构:k1 纯算术循环;k2 每轮 16 元填充(惯用 C = 复用一块
 * 缓冲,零每轮分配——与 Ctron 侧 List 惯用形的差 = allocator 税归因位)。
 * 编译口径与 ctron 臂一致:cc -O1。 */
#include <stdio.h>
#include <stdlib.h>

typedef struct { long long* items; int n; int cap; } list;

static void list_push(list* l, long long v) {
    if (l->n == l->cap) {
        int nc = l->cap ? l->cap * 2 : 8;
        long long* ni = (long long*)malloc(sizeof(long long) * (size_t)nc);
        for (int q = 0; q < l->n; q++) ni[q] = l->items[q];
        free(l->items);
        l->items = ni;
        l->cap = nc;
    }
    l->items[l->n++] = v;
}

static long long k1(long long n) {
    long long acc = 1;
    for (long long i = 0; i < n; i++) {
        acc = (acc * 48271) % 2147483647;
        long long t = (acc % 1000) * ((acc % 997) + 1);
        acc = (acc + t) % 2147483647;
    }
    return acc;
}

static long long k2(long long n) {
    long long acc = 0;
    long long buf[16];
    for (long long i = 0; i < n; i++) {
        for (long long j = 0; j < 16; j++) buf[j] = (i * 31 + j * 7) % 1000;
        acc = (acc + 16) % 2147483647;
    }
    (void)list_push; /* 推送路径留作非惯用对照,惯用臂=栈缓冲复用 */
    return acc;
}

int main(void) {
    printf("%lld\n", k1(5000000));
    printf("%lld\n", k2(400000));
    return 0;
}
