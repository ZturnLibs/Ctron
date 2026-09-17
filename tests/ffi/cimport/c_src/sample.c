/* tests/ffi/cimport —— C 实现:与 cimport 生成的绑定链接。 */
#include <stdint.h>
#include <string.h>
#include "../sample.h"

int64_t sample_add(int64_t a, int64_t b) { return a + b; }
double sample_scale(double v, int factor) { return v * factor; }
size_t sample_count(const char *s, int limit) { return strlen(s) < (size_t)limit ? strlen(s) : (size_t)limit; }
void sample_reset(void) {}
