/* tests/ffi/dyn_link —— C 侧:符号进本进程全局域(与产物同批链接),
 * dyn_add 仅经运行期 dlsym(RTLD_DEFAULT) 到达。 */
#include <stdint.h>

int64_t dyn_add(int64_t a, int64_t b) {
    return a + b;
}

int64_t static_mul(int64_t a, int64_t b) {
    return a * b;
}
