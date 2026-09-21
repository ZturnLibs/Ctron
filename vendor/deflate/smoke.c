/* vendor/deflate/smoke.c —— libminiz.a 链接冒烟(全离线)
 *
 * 口径:std/http 垫片只取 raw deflate + crc32 三件(tdefl_compress_mem_to_mem /
 * tinfl_decompress_mem_to_mem / mz_crc32);本冒烟即此三件的往返 + 已知向量,
 * 过 = 打印 "DEFLATE-OK <MZ_VERSION>" 并返回 0。
 * raw 口径:压缩 flags 不带 TDEFL_WRITE_ZLIB_HEADER(window_bits<=0),解压
 * flags 不带 TINFL_FLAG_PARSE_ZLIB_HEADER —— 与 ctron_deflate.c 垫片同形。
 */
#include <stdio.h>
#include <string.h>
#include "miniz.h"

static int roundtrip(const unsigned char *in, size_t n, int level)
{
    unsigned char comp[1 << 16];
    unsigned char decomp[1 << 16];
    size_t clen = tdefl_compress_mem_to_mem(comp, sizeof(comp), in, n,
                                            tdefl_create_comp_flags_from_zip_params(level, 0, 0));
    if (clen == 0 || clen >= sizeof(comp))
        return 1;
    size_t dlen = tinfl_decompress_mem_to_mem(decomp, sizeof(decomp), comp, clen, 0);
    if (dlen != n)
        return 2;
    if (n > 0 && memcmp(in, decomp, n) != 0)
        return 3;
    return 0;
}

int main(void)
{
    /* 载荷一:大重复文本(高压缩比) */
    char rep[64000];
    for (size_t i = 0; i < sizeof(rep); i++)
        rep[i] = (char)('a' + (i % 26));
    /* 载荷二:伪随机(不可压仍须往返) */
    unsigned char rnd[4096];
    unsigned int s = 12345;
    for (size_t i = 0; i < sizeof(rnd); i++) {
        s = s * 1103515245u + 12345u;
        rnd[i] = (unsigned char)(s >> 16);
    }
    /* 载荷三:空 / 单字节 */
    if (roundtrip((const unsigned char *)rep, sizeof(rep), 6)) return 1;
    if (roundtrip(rnd, sizeof(rnd), 1)) return 2;
    if (roundtrip(rnd, sizeof(rnd), 9)) return 3;
    if (roundtrip((const unsigned char *)"", 0, 6)) return 4;
    if (roundtrip((const unsigned char *)"x", 1, 6)) return 5;

    /* 越界炸弹出为负判据:输出缓冲给 0,解压必须失败(返回 (size_t)-1) */
    unsigned char comp[256];
    size_t clen = tdefl_compress_mem_to_mem(comp, sizeof(comp), rep, 4096,
                                            tdefl_create_comp_flags_from_zip_params(6, 0, 0));
    if (clen == 0) return 6;
    if (tinfl_decompress_mem_to_mem(NULL, 0, comp, clen, 0) != TINFL_DECOMPRESS_MEM_TO_MEM_FAILED)
        return 7;

    /* crc32 已知向量:"" = 0; "123456789" = 0xCBF43926 */
    if (mz_crc32(MZ_CRC32_INIT, (const mz_uint8 *)"", 0) != 0u) return 8;
    if (mz_crc32(MZ_CRC32_INIT, (const mz_uint8 *)"123456789", 9) != 0xCBF43926u) return 9;

    printf("DEFLATE-OK %s\n", MZ_VERSION);
    return 0;
}
