/* tests/ffi/cimport —— union 字节缓冲 sizeof 等值互证:
   cimport 生成的 Vals_raw_len()(C sizeof 公式)须与 C 真实 sizeof(Vals) 精确相等。 */
#include <stdint.h>
#include <stddef.h>
#include "../sample.h"

int64_t uvals_size_is(int64_t n) {
    return (size_t)n == sizeof(Vals) ? 1 : 0;
}
