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
extern float sample_fscale(float v, float k);

/* 位域:cimport 应整构跳过(布局不可按声明序表达,逐字段映射=静默错绑) */
struct bits {
    unsigned int lo : 3;
    unsigned int hi : 5;
    int rest;
};

/* union:cimport 以 U8[N] 字节缓冲承载(N = 最大成员按联合对齐补齐 = C sizeof);
   本例最大成员 char[12]=12B、联合对齐 8 → N = 16 = C sizeof */
typedef union {
    double d;
    char s[12];
    long l;
} Vals;

/* 定长数组字段(v0.9):scores → var scores: I32[4],布局与 C 同构(sizeof=24) */
typedef struct {
    int64_t total;
    int32_t scores[4];
} SampleArr;

extern SampleArr samplearr_make(int64_t total);
extern int64_t samplearr_score_sum(SampleArr a);
extern int64_t samplearr_sizeof(void);

#endif
