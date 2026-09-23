/* tests/ffi/cimport —— union 字节缓冲 sizeof 互证:
   cimport 生成的 Vals_raw_len()(8 补齐缓冲)须覆盖 C 真实 sizeof(union Vals)。 */
#include <stdint.h>
#include <stddef.h>
#include "../sample.h"

int64_t uvals_size_is(int64_t n) {
    size_t real = sizeof(Vals);
    return ((size_t)n >= real && (size_t)n < real + 8) ? 1 : 0;
}
