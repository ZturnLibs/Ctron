/* host_shim/cc2_bit.c —— bit.* 算术臂宿主口(W5b-bit 2026-10-28;CC2 同款链接面)。
   纯整数运算零分配(解释器 malloc 有界泄漏债不适用)。
   返回 = 宽度 bits 两补、符号扩展至 long long(解释器 u32 惯例"4294967295 存 -1"
   与 i64 直槽由 Ctron 侧 bit_ret64 分域折回)。
   op: 0=and 1=or 2=xor 3=not(b 忽略) 4=shl 5=shr;bits ∈ {32,64};sgn ∈ {0,1}。
   语义权威 = W5 修正后 bit_comb(rt_bit rt_eval.c 同族公式,逐值对照通过)。 */
#include <stdint.h>
#include <time.h>

/* R-P2b:std.time.Clock NS 宿主口(CLOCK_MONOTONIC 进程纪元;bin 直调链接面) */
long long ctron_clock_now_ns(void) {
    static long long base = -1;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long long ns = (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
    if (base < 0) { base = ns; }
    return ns - base;
}

long long ctron_bit_i64(int op, long long a, long long b, int bits, int sgn) {
    unsigned long long m = bits == 64 ? ~(unsigned long long)0 : ((1ULL << bits) - 1);
    unsigned long long ua = (unsigned long long)a & m;
    unsigned long long ub = (unsigned long long)b & m;
    unsigned long long r = 0;
    if (op == 0) {
        r = ua & ub;
    } else if (op == 1) {
        r = ua | ub;
    } else if (op == 2) {
        r = ua ^ ub;
    } else if (op == 3) {
        r = ua ^ m;
    } else if (op == 4) {
        r = (ua << b) & m;
    } else if (op == 5) {
        if (sgn) {
            long long sv = (bits == 32) ? (long long)(int32_t)ua : (long long)ua;
            r = (unsigned long long)(sv >> b) & m;
        } else {
            r = ua >> b;
        }
    }
    if (bits == 32) {
        return (long long)(int32_t)(uint32_t)r;
    }
    return (long long)r;
}
