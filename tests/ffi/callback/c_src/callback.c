/* tests/ffi/callback —— C 侧:持有 Ctron fn 指针并回调;经切片视图回写数组。
 * ABI 契约见 ../ctron_abi.h(镜像发射器预发 typedef)。 */
#include "../../ctron_abi.h"

int64_t apply_n(ct_fn1 f, int64_t x, int64_t n) {
    int64_t r = x;
    for (int64_t i = 0; i < n; i++) {
        r = f(r);
    }
    return r;
}

void sort_i32(ctron_view_i v, ct_fn2 cmp) {
    for (int64_t i = 0; i + 1 < v.n; i++) {
        for (int64_t j = 0; j + 1 < v.n - i; j++) {
            if (cmp(v.d[j], v.d[j + 1]) > 0) {
                int32_t t = v.d[j];
                v.d[j] = v.d[j + 1];
                v.d[j + 1] = t;
            }
        }
    }
}
