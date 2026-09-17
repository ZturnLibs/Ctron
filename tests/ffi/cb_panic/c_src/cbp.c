/* tests/ffi/cb_panic —— C 侧:调用 Ctron 回调一次。 */
#include "../../ctron_abi.h"

int64_t run_cb(ct_fn1 f, int64_t x) {
    return f(x);
}
