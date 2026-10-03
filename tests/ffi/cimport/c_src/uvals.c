/* tests/ffi/cimport —— union 字节缓冲 sizeof 等值互证:
   cimport 生成的 Vals_raw_len()(C sizeof 公式)须与 C 真实 sizeof(Vals) 精确相等。 */
#include <stdint.h>
#include <stddef.h>
#include "../sample.h"

int64_t uvals_size_is(int64_t n) {
    return (size_t)n == sizeof(Vals) ? 1 : 0;
}

/* T45:union 关键字限定指针形参——C 侧按真实 union 写读,Ctron 侧持 &Vals 缓冲 */
void vals_poke_k(union Vals* v, int64_t tag) {
    v->l = tag;
}
int64_t vals_peek_k(union Vals* v) {
    return v->l;
}
