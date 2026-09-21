/* ctron_deflate.c - Ctron std/http 压缩垫片:miniz 3.1.2 包装(raw deflate + crc32)
 * P4-B:压缩门面的唯一 C 依赖面。体例同 ctron_tls.c(无状态,无停车点——纯 CPU
 * 函数,不引用任何 ctron_rt_* 符号;多 worker 并发安全:miniz 三件 API 均无共享
 * 状态,且本垫片不用任何 static)。
 *
 * 取面(有意收窄,见 vendor/deflate/VENDORED.md):
 *   - tdefl_compress_mem_to_mem —— raw deflate(window_bits<=0,无 zlib 头);
 *     miniz 内部 MZ_MALLOC 压缩器(堆),协程栈预算无涉;
 *   - tinfl_decompress —— 【不经】tinfl_decompress_mem_to_mem:后者在栈上开
 *     tinfl_decompressor(约 40KB,字典 32KB + 表),破 net 垫片"协程栈冻结
 *     64KB / 单帧 ≤4KB"预算口径(ctron_tls.c 文件头同源纪律)。本垫片自管:
 *     解压状态 MZ_MALLOC 上堆,逐字复刻 mem_to_mem 语义(NON_WRAPPING_OUTPUT_BUF
 *     + 全量输入一次给足;DONE → 字节数,HAS_MORE_OUTPUT → 容量不足,余 → 损坏);
 *   - mz_crc32 —— gzip CRC-32(miniz 16 项 nibble 表实现,无状态)。
 *
 * lane 暂存(ctron_tls.c 同款纪律,登记):&I64[] 字节道 = 每字节一条 int64
 * lane(布局 { int64_t* d; int64_t n; }),与 C 连续字节缓冲不同构 —— 三件入参
 * 一律经堆上暂存 MZ_MALLOC(n) 转 bytes 后进 miniz,出参再逐条写回 lane
 * (0..255 直取直写,不设逐元素域检查,信任自有生产面,登记)。
 *
 * 容器分工(登记):本垫片只出 raw deflate;gzip 容器组框(10 字节头 + FLG 遍历
 * + crc32/isize 尾)在 Ctron 侧 std/http/enc.ct —— 垫片极薄,组框逻辑纯层可测。
 *
 * rc 约定(镜像 net/tls 垫片):>0 = 字节数(crc32 = 新 crc 值);<0 = 失败码:
 *   -1 边界/参数错(off、cap、level 域);-2 输出容量不足(压缩溢出/解压
 *   炸弹护栏 —— out_cap 为【必填】护栏参数,本垫片不存在无上限解压面);
 *   -3 数据损坏(含 OOM:堆暂存失败,与损坏同码,登记)。0 不可达:空输入的
 *   合法 raw deflate 流仍 ≥2 字节,压缩/解压成功恒 >0(登记:0 不作成功值)。
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

/* 视图 lane 镜像:&I64[] 发射 ctron_view6 { d, n } 按值(§9.6;ctron_net.c
 * ct_view6 独立 TU 布局同型即 ABI 兼容) */
typedef struct { int64_t* d; int64_t n; } ct_view6;

/* lane 段 → 连续 bytes(堆;调用方 MZ_FREE)。返回 NULL = OOM。 */
static uint8_t* ct_lanes_to_bytes(const ct_view6* v, int64_t off, int64_t n)
{
    uint8_t* p = (uint8_t*)MZ_MALLOC((size_t)(n > 0 ? n : 0));
    if (!p)
        return NULL;
    for (int64_t i = 0; i < n; i++)
        p[i] = (uint8_t)v->d[off + i];
    return p;
}

/* 连续 bytes → lane 段(逐条写回)。 */
static void ct_bytes_to_lanes(ct_view6* v, int64_t off, int64_t n, const uint8_t* p)
{
    for (int64_t i = 0; i < n; i++)
        v->d[off + i] = (int64_t)p[i];
}

/* raw deflate 压缩:in[0..in_n) → out 道的 [out_off, out_off+out_cap)。
 * level:0 = 存储块(zlib 语义),1..9 = zlib 探针档;越域 -1。
 * 返回写入字节数(>0);容量不足 -2;参数/OOM -1/-3。 */
int64_t ctron_deflate_compress(ct_view6 in, int64_t in_n, ct_view6 out,
                               int64_t out_off, int64_t out_cap, int64_t level)
{
    if (in_n < 0 || in_n > in.n || out_off < 0 || out_cap < 0
        || out_off > out.n || out_cap > out.n - out_off
        || level < 0 || level > 9)
        return -1;
    if (out_cap == 0)
        return -2;
    uint8_t* src = ct_lanes_to_bytes(&in, 0, in_n);
    if (!src)
        return -3;
    uint8_t* dst = (uint8_t*)MZ_MALLOC((size_t)out_cap);
    if (!dst) {
        MZ_FREE(src);
        return -3;
    }
    size_t clen = tdefl_compress_mem_to_mem(
        dst, (size_t)out_cap, src, (size_t)in_n,
        tdefl_create_comp_flags_from_zip_params((int)level, 0 /* raw */, 0));
    MZ_FREE(src);
    if (clen == 0) {
        MZ_FREE(dst);
        return -2;  /* miniz 0 = 溢出/失败(成功恒 ≥2 字节,见文件头) */
    }
    ct_bytes_to_lanes(&out, out_off, (int64_t)clen, dst);
    MZ_FREE(dst);
    return (int64_t)clen;
}

/* raw inflate:in 道 [in_off, in_off+in_n) → out[0..out_cap)。
 * out_cap = 炸弹护栏(必填)。返回解压字节数(>0);护栏不足 -2;损坏 -3;
 * 参数 -1。解压状态与两侧暂存上堆(栈预算理由见文件头)。 */
int64_t ctron_deflate_decompress(ct_view6 in, int64_t in_n, int64_t in_off,
                                 ct_view6 out, int64_t out_cap)
{
    if (in_n < 0 || in_off < 0 || in_off > in.n || in_n > in.n - in_off
        || out_cap < 0 || out_cap > out.n)
        return -1;
    if (out_cap == 0)
        return -2;
    uint8_t* src = ct_lanes_to_bytes(&in, in_off, in_n);
    if (!src)
        return -3;
    uint8_t* dst = (uint8_t*)MZ_MALLOC((size_t)out_cap);
    if (!dst) {
        MZ_FREE(src);
        return -3;
    }
    tinfl_decompressor* dc = (tinfl_decompressor*)MZ_MALLOC(sizeof(tinfl_decompressor));
    if (!dc) {
        MZ_FREE(src);
        MZ_FREE(dst);
        return -3;
    }
    size_t in_bytes = (size_t)in_n;
    size_t out_bytes = (size_t)out_cap;
    tinfl_init(dc);
    tinfl_status st = tinfl_decompress(
        dc,
        src, &in_bytes,
        dst, dst, &out_bytes,
        TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF /* 全量输入一次给足,无 HAS_MORE_INPUT */);
    MZ_FREE(dc);
    MZ_FREE(src);
    if (st == TINFL_STATUS_DONE) {
        ct_bytes_to_lanes(&out, 0, (int64_t)out_bytes, dst);
        MZ_FREE(dst);
        return (int64_t)out_bytes;
    }
    MZ_FREE(dst);
    if (st == TINFL_STATUS_HAS_MORE_OUTPUT)
        return -2;  /* 输出容量不足(炸弹护栏生效,fixtures 断言此码) */
    return -3;      /* 损坏/欠输入(NEEDS_MORE_INPUT 亦归损坏:单成员整流口径) */
}

/* CRC-32(Continuation 语义):crc 传入上次值(MZ_CRC32_INIT = 0 起),返回新值。
 * gzip 尾/校验面用;in_off 起取 in_n 字节。 */
int64_t ctron_deflate_crc32(ct_view6 in, int64_t in_n, int64_t in_off, int64_t crc)
{
    if (in_n < 0 || in_off < 0 || in_off > in.n || in_n > in.n - in_off)
        return -1;
    if (in_n == 0)
        return crc;
    uint8_t* p = ct_lanes_to_bytes(&in, in_off, in_n);
    if (!p)
        return -3;
    int64_t r = (int64_t)mz_crc32((mz_ulong)crc, p, (size_t)in_n);
    MZ_FREE(p);
    return r;
}
