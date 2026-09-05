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
    const char* msg; // 失败/panic 消息(堆;ctron_rt_run_free 释放)
    size_t tests_run;
    size_t tests_total;
    int unsupported; // RT_ERROR 时记录的构造说明
    char* out;       // 程序输出(print 追加;堆;free 释放)
    long exit_code;  // main 返回值
} rt_run;

/// 运行 cfile 内全部 test 块(序贯);panic/断言失败即中止本文件。
rt_run ctron_rt_run(const cfile* f);
/// 运行顶层 `fn main`(自举/执行入口);输出进 r.out,返回值入 r.exit_code。
rt_run ctron_rt_run_main(const cfile* f);
void ctron_rt_run_free(rt_run* r);

#endif
