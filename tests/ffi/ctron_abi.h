/*
 * ctron_abi.h —— C 侧镜像发射器 ABI 契约(§9.6 v0.6)。
 * 发射面为唯一真源:driver_emit 预发 typedef(ct_i/ct_fn1..3/ctron_view_*)变更须同步此处。
 * 各夹具 c_src 经 `#include "ctron_abi.h"` 共享(引号包含相对 c_src/ 查找)。
 */
#ifndef CTRON_ABI_H
#define CTRON_ABI_H

#include <stdint.h>

typedef int64_t ct_i;
typedef ct_i (*ct_fn1)(ct_i);
typedef ct_i (*ct_fn2)(ct_i, ct_i);
typedef ct_i (*ct_fn3)(ct_i, ct_i, ct_i);
typedef struct { int32_t* d; int64_t n; } ctron_view_i;
typedef struct { int64_t* d; int64_t n; } ctron_view_6;

#endif /* CTRON_ABI_H */
