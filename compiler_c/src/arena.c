// arena.c —— 块链 bump 分配器。分配永不移动;块链头插,整体遍历释放。
#include "arena.h"
#include <stdlib.h>
#include <string.h>

enum { ARENA_MIN_BLOCK = 1 << 16, ARENA_ALIGN = 16 };

typedef struct blk {
    struct blk* next;
    size_t cap;   // 本块可分配字节数
    size_t used;  // 已用字节数
} blk;

struct ctron_arena {
    blk* head;
};

static size_t align_up(size_t n) { return (n + ARENA_ALIGN - 1) & ~(size_t)(ARENA_ALIGN - 1); }
static size_t blk_payload_off(void) { return align_up(sizeof(blk)); }

static blk* blk_new(size_t cap) {
    blk* b = (blk*)malloc(blk_payload_off() + cap);
    if (!b) abort();
    b->next = NULL;
    b->cap = cap;
    b->used = 0;
    return b;
}

ctron_arena* ctron_arena_new(void) {
    ctron_arena* a = (ctron_arena*)calloc(1, sizeof(ctron_arena));
    if (!a) abort();
    return a;
}

void* ctron_arena_alloc(ctron_arena* a, size_t n) {
    n = align_up(n ? n : 1);
    if (!a->head) a->head = blk_new(n < ARENA_MIN_BLOCK ? ARENA_MIN_BLOCK : n);
    if (a->head->used + n > a->head->cap) {
        size_t c = a->head->cap * 2;
        if (n > c) c = n;
        blk* b = blk_new(c);
        b->next = a->head;
        a->head = b;
    }
    void* p = (char*)a->head + blk_payload_off() + a->head->used;
    a->head->used += n;
    memset(p, 0, n);
    return p;
}

char* ctron_arena_strndup(ctron_arena* a, const void* p, size_t n) {
    char* s = (char*)ctron_arena_alloc(a, n + 1);
    memcpy(s, p, n);
    s[n] = '\0';
    return s;
}

void ctron_arena_free(ctron_arena* a) {
    if (!a) return;
    blk* b = a->head;
    while (b) {
        blk* nx = b->next;
        free(b);
        b = nx;
    }
    free(a);
}
