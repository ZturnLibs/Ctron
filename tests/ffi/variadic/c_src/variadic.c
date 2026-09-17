/* tests/ffi/variadic —— C 侧:va_arg 累加变参(int64_t 提升域)。 */
#include <stdarg.h>
#include <stdint.h>

int64_t sum_all(int32_t n, ...) {
    va_list ap;
    va_start(ap, n);
    int64_t acc = 0;
    for (int32_t i = 0; i < n; i++) {
        acc += va_arg(ap, int64_t);
    }
    va_end(ap);
    return acc;
}
