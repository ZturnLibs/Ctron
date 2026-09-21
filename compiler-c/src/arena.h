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

/// 累计需求字节(CTRON_MEM_DEBUG=1 时有效;定界探针用,见 docs/linux-seed-memory-evidence.md)。
long ctron_mem_total(void);
/// 逐块对账(cap vs used;CTRON_MEM_DEBUG=1 时有效)。
void ctron_mem_audit(const ctron_arena* a);
/// 若 p 是 arena 最后一次分配且余量足够,原地扩展到 new_len 字节并返 1(拼接零拷贝快路径)。
int ctron_arena_try_extend(ctron_arena* a, void* p, size_t old_len, size_t new_len);

#endif
