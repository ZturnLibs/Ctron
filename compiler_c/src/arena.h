// arena.h —— 编译器内存池(块链 bump 分配,整体一次释放)。
// C 版约定:编译器进程内对象(字符串/记号载荷)全部活在 arena 中,
// 无逐对象 free;词法/解析完成后释放整条块链。
#ifndef CTRON_ARENA_H
#define CTRON_ARENA_H

#include <stddef.h>

typedef struct ctron_arena ctron_arena;

ctron_arena* ctron_arena_new(void);
/// 分配 n 字节(自动 16 字节对齐、清零),永不移动,无需单独释放。
void* ctron_arena_alloc(ctron_arena* a, size_t n);
/// 复制 [p, p+n) 为 NUL 结尾字符串(多分配 1 字节)。
char* ctron_arena_strndup(ctron_arena* a, const void* p, size_t n);
void ctron_arena_free(ctron_arena* a);

#endif
