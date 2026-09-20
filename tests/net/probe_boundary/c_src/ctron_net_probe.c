/* ctron_net_probe.c - FFI boundary probe (emit surface)
 *
 * 边界探针(登记见 .superpowers/sdd 计划执行记录):
 *   探针A(出参):Box[Box64] → 发射面 t_Box64* 堆胞指针,与 C 侧 Box64* 互为
 *     ABI 镜像(独立 TU,布局同型即可;tests/ffi/repr_c 已证声明序布局)。
 *     C 写 *b = 42,调用方经 b.v 自动解引用读回 —— 按引用出参成立。
 *   探针B(数组衰减):T[N] 实参过界不拷贝 —— 发射 (首址+长度) 视图
 *     ctron_view_6(ctron_abi.h 镜像),C 经 buf.d[i] 写入即落调用方数组。
 *     注:发射器仅预发 i/6/b/s/f 五种视图 typedef,I8/U8 元素视图
 *     (ctron_view_w8s 等)为发射面缺口(未知类型名),字节垫片须走
 *     I64/I32 通道或 Box[struct](见执行记录)。
 */
#include <stdint.h>
#include "../../../ffi/ctron_abi.h"

/* 镜像 Ctron 侧 struct Box64 { var v: I64 }(声明序 = C 声明序,单 I64 字段) */
typedef struct { int64_t v; } Box64;

int64_t ctron_probe_fill(ctron_view_6 buf, int64_t cap) {
    int64_t n = 2;
    if (buf.n < n || cap < n) { return -1; }
    buf.d[0] = 111;  /* 'o' */
    buf.d[1] = 107;  /* 'k' */
    return n;
}

int64_t ctron_probe_out_box(Box64* b) {
    b->v = 42;
    return 0;
}
