/* tests/ffi/ext_fn_ret —— C 侧:返回 Ctron 兼容回调(ct_fn1),并接受回传。 */
#include "../../ctron_abi.h"

int64_t b_inc_impl(int64_t x) {
    return x + 1;
}

ct_fn1 get_inc(void) {
    return b_inc_impl;
}

int64_t apply_twice(ct_fn1 f, int64_t x) {
    return f(f(x));
}
