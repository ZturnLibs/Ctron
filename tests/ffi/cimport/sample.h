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

/* 位域:cimport 应整构跳过(布局不可按声明序表达,逐字段映射=静默错绑) */
struct bits {
    unsigned int lo : 3;
    unsigned int hi : 5;
    int rest;
};

/* union:cimport 以 U64 字段缓冲承载(8 补齐);raw_len 与 C sizeof 互证 */
typedef union {
    double d;
    long l;
    int i;
} Vals;

#endif
