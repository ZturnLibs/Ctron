/* tests/ffi/cimport/sample.h —— cimport 夹具头文件(§9.6 v0.7) */
#ifndef SAMPLE_H
#define SAMPLE_H

#define SAMPLE_VERSION 42
#define SAMPLE_RATIO 2.5
#define SAMPLE_NAME "ctron"

typedef struct {
    int64_t x;
    double weight;
    const char *label;
    uint32_t flags;
} SamplePoint;

extern int64_t sample_add(int64_t a, int64_t b);
extern double sample_scale(double v, int factor);
extern size_t sample_count(const char *s, int limit);
extern void sample_reset(void);

#endif
