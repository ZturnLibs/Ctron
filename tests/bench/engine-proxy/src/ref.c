/* ref.c —— engine-proxy C 参照(与 src/main.ct 同构同序;digest 行逐字节钉)
 * 全程非负整数域,与 Ctron I64 %/ 语义恒同;时间行同格式只报告。 */
#include <stdint.h>
#include <stdio.h>
#include <time.h>

extern int64_t sv_stub_work(int64_t iters);

int main(void) {
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    int64_t digest = 0, frames = 0;
    for (int64_t sid = 0; sid < 100; sid++) {
        for (int64_t seg = 0; seg < 10; seg++) {
            for (int64_t frame = 0; frame < 20; frame++) {
                int64_t fid = (sid * 100 + seg * 20) + frame;
                int64_t seed = (fid * 2654435761) + 12345;
                int64_t acc = 0;
                for (int64_t j = 0; j < 256; j++) {
                    seed = (((seed % 2147483648) * 1103515245) + 12345) % 2147483648;
                    int64_t sample = seed % 4000;
                    acc = ((acc + (sample * 31)) + j) % 1000003;
                }
                int64_t sv = sv_stub_work(100000);
                acc = (acc + (sv % 97)) % 1000003;
                digest += acc;
                frames += 1;
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &b);
    int64_t ms = (b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000;
    printf("engine-proxy digest %lld frames %lld\n", (long long)digest, (long long)frames);
    printf("engine-proxy rtf_ms %lld audio_s 320\n", (long long)ms);
    return 0;
}
