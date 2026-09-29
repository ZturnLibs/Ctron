/* tests/ffi/ptr_param —— C 实现:裸指针形参(路径 1 选项 A:解引用只在 C 侧) */
#include <stdint.h>

void fill(int64_t *p) { *p = 42; }
int64_t peek(int64_t *p) { return *p; }
void swap(int64_t *a, int64_t *b) { int64_t t = *a; *a = *b; *b = t; }

typedef struct {
    int64_t id;
} Pkt;

void pkt_bump(Pkt *p) { p->id += 1; }
int64_t pkt_id(Pkt *p) { return p->id; }
