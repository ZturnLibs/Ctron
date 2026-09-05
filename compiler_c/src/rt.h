// rt.h —— C4-a 解释器(行为/panic 语料执行)。纯数值子集(见 suite_rt 允许表)。
#ifndef CTRON_RT_H
#define CTRON_RT_H

#include <stddef.h>
#include "ast.h"

typedef enum {
    RT_OK = 0,      // 全部 test 块通过
    RT_ASSERT_FAIL, // 断言失败(消息含原因)
    RT_PANIC,       // 运行时 panic(消息含 'overflow'/'division by zero' 等)
    RT_ERROR,       // 解释器不支持构造(语义面外)
} rt_status;

typedef struct {
    rt_status st;
    const char* msg; // 失败/panic 消息(堆,由调用方 strdup 后无需释放则由 rt 释放)
    size_t tests_run;
    size_t tests_total;
    int unsupported; // RT_ERROR 时记录的构造说明
} rt_run;

/// 运行 cfile 内全部 test 块(序贯);panic/断言失败即中止本文件。
rt_run ctron_rt_run(const cfile* f);
void ctron_rt_run_free(rt_run* r);

#endif
