/* minilibc time.h —— 时钟族仅声明(bare 档 time 层 std 档级,T35 tier 门拦截;
 * 此为发射运行时死代码的编译面)。 */
#ifndef CT_MLC_TIME_H
#define CT_MLC_TIME_H

typedef long time_t;
struct timespec { time_t tv_sec; long tv_nsec; };
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

int clock_gettime(int clk, struct timespec* ts);
time_t time(time_t* t);

#endif
