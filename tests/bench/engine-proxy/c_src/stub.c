/* stub.c —— engine-proxy FFI 忙循环桩(设计 §11:忙循环等效周期,无真库依赖)
 * 两侧同链同 -O1:C 参照与 Ctron 臂链同一目标文件,周期成本恒等。
 * volatile 防止空转消除;返回累加值防调用消除。 */
#include <stdint.h>

int64_t sv_stub_work(int64_t iters) {
    volatile int64_t acc = 0;
    for (int64_t i = 0; i < iters; i++) {
        acc += i % 7;
    }
    return acc;
}
