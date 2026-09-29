/* tests/ffi/cimport —— C 实现:与 cimport 生成的绑定链接。 */
#include <stdint.h>
#include <string.h>
#include "../sample.h"

int64_t sample_add(int64_t a, int64_t b) { return a + b; }
double sample_scale(double v, int factor) { return v * factor; }
size_t sample_count(const char *s, int limit) { return strlen(s) < (size_t)limit ? strlen(s) : (size_t)limit; }
void sample_reset(void) {}

/* 定长数组字段(v0.9):构造/求和/sizeof 互证 */
int64_t samplearr_make_t;
SampleArr samplearr_make(int64_t total) {
    SampleArr a;
    a.total = total;
    a.scores[0] = 10;
    a.scores[1] = 20;
    a.scores[2] = 30;
    a.scores[3] = 40;
    return a;
}
int64_t samplearr_score_sum(SampleArr a) {
    return (int64_t)a.scores[0] + a.scores[1] + a.scores[2] + a.scores[3];
}
int64_t samplearr_sizeof(void) { return (int64_t)sizeof(SampleArr); }
