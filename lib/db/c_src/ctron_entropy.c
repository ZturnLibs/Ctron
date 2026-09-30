/*
 * ctron_entropy.c - Ctron std/uuid 熵源垫片(P5-B;db/c_src 首件)
 *
 * extern 面(std/uuid.ct):int64_t ctron_entropy_fill(int64_t* buf, int64_t n)
 *   → 成功填 n 条【字节 lane】(每 lane 一字节值 0..255;net read_t
 *     lane 约定同形)返回 0;失败返回 -1(fail-closed,调用方
 *     uuid_v4/uuid_v7 返回空串,绝不满凑伪熵)。注意 n 计 lane 不计
 *     字节——lane 是 int64 槽,16 lane = 128 字节存储。
 *
 * 放置理由(P5-B 登记):std/uuid 是零 std 依赖叶,而 uuid 消费方 db
 * 不得 use net(加载器严格互斥树 + 门面 db: StdDb 贯穿口径),垫片
 * 不随 ctron_net.c;P5 db 泳道自有 c_src 邻位(ctron_net.c 之 net/c_src 同形),
 * 需要真熵的夹具显式链接本文件(tests/crypto_vec run.sh 同 tests/net 链法)。
 * interp 口径无 extern 运行时:真熵面仅发射臂;纯形 uuid_v4_from/v7_from
 * 双臂可测(std/uuid.ct 头注同口径)。
 *
 * 源优先级:getrandom()(Linux,循环取满处理 EINTR/短读)→ arc4random_buf()
 * (Apple/BSD)→ /dev/urandom(fread 兜底,POSIX/其他)。三档任一失败即 -1。
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#if defined(__linux__)
#include <sys/random.h>
#define CT_HAS_GETRANDOM 1
#endif

#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#include <stdlib.h>
#define CT_HAS_ARC4 1
#endif

/* 取 k 字节真熵入 p(源分派;任一失败 -1,k=0 恒 0) */
static int64_t ct_entropy_bytes(unsigned char* p, size_t k) {
#if defined(CT_HAS_ARC4)
    /* arc4random_buf 无失败返回值,恒满足 */
    arc4random_buf(p, k);
    return 0;
#elif defined(CT_HAS_GETRANDOM)
    size_t off = 0;
    while (off < k) {
        ssize_t r = getrandom(p + off, k - off, 0);
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (r == 0) {
            return -1;
        }
        off += (size_t)r;
    }
    return 0;
#else
    FILE* f = fopen("/dev/urandom", "rb");
    if (f == NULL) {
        return -1;
    }
    size_t got = fread(p, 1, k, f);
    fclose(f);
    if (got != k) {
        return -1;
    }
    return 0;
#endif
}

static int64_t ct_entropy_fallback_urandom(unsigned char* p, size_t k) {
    FILE* f = fopen("/dev/urandom", "rb");
    if (f == NULL) {
        return -1;
    }
    size_t got = fread(p, 1, k, f);
    fclose(f);
    if (got != k) {
        return -1;
    }
    return 0;
}

int64_t ctron_entropy_fill(int64_t* buf, int64_t n) {
    if (n < 0) {
        return -1;
    }
    if (n == 0) {
        return 0;
    }
    if (buf == NULL) {
        return -1;
    }
    unsigned char stackbuf[256];
    int64_t off = 0;
    while (off < n) {
        int64_t chunk = n - off;
        if (chunk > 256) {
            chunk = 256;
        }
        if (ct_entropy_bytes(stackbuf, (size_t)chunk) != 0) {
            /* Linux getrandom 残档回落 /dev/urandom(已部分填写则重取该块) */
            if (ct_entropy_fallback_urandom(stackbuf, (size_t)chunk) != 0) {
                return -1;
            }
        }
        int64_t i;
        for (i = 0; i < chunk; i++) {
            buf[off + i] = (int64_t)stackbuf[i];
        }
        off += chunk;
    }
    return 0;
}
