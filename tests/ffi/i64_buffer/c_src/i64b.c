/* tests/ffi/i64_buffer —— C 侧:经视图读 I64 数组(镜像发射器 ctron_view_6)。 */
#include "../../ctron_abi.h"

int64_t sum_i64(ctron_view_6 v) {
    int64_t acc = 0;
    for (int64_t i = 0; i < v.n; i++) {
        acc += v.d[i];
    }
    return acc;
}

int64_t first_i64(ctron_view_6 v) {
    return v.d[0];
}
