// sem.h —— C3 语义检查入口(单文件)。
// 实现检查(消息含语料 marker 要求的英文子串):
//   E2010 类型不匹配(条件非 Bool / let 字面量类别与注解冲突;C3-d 保守子集)
//   E2030 match 穷尽 / E3010 spawn 捕获非 Send / E3020 channel 元素非 Send
//   E3031 静态存储非 Send / E4020 #[pure] 能力调用 / E6020 comptime 能力调用
//   E4030 #[no_spawn] 内 spawn / E3050 own 块 arena 句柄 use-after-move
//   E3060 own 块内对类值成员可变写 / W8010 浅拷贝 / W8020 must-use 丢弃
// 模块级在 pkg.c(E5010/E5020/E2020/E4010/E6010);分配效果 E3040 见 check_alloc_ctx。
#ifndef CTRON_SEM_H
#define CTRON_SEM_H

#include <stddef.h>
#include "arena.h"
#include "ast.h"
#include "token.h"

typedef struct {
    ctron_diag* diags; // 堆数组;由调用方 free
    size_t ndiags;
} ctron_sem_result;

typedef enum { SEM_FULL = 0, SEM_BARE, SEM_WEB } sem_profile;

/// 对单个已解析文件做语义检查;message 落在 arena。profile 决定档位语境(SEM_BARE = 全量 no_alloc)。
ctron_sem_result ctron_sem_check_mode(const cfile* f, ctron_arena* arena, int profile);

/// 默认 full 档。
ctron_sem_result ctron_sem_check(const cfile* f, ctron_arena* arena);

#endif
