/* tests/ffi/opt_return —— C 侧:NULL 表示失败的惯用法。 */
#include <stdint.h>

const char* cfg_maybe(int32_t i) {
    if (i == 0) {
        return 0;
    }
    return "configured";
}
