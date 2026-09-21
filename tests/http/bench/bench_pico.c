/* bench_pico.c —— P4-D 解析基准 picohttpparser 对照侧(测试线;不入产物)
 *
 * 口径(bench_parse.ct 头注同源):
 *   - 语料三报文与 bench_parse.ct 逐字节同文;公平性由字节 digest 双侧对 pin
 *     (bench.sh 断言 digest 一致才采数)。
 *   - digest 同算法(有界模乘 h*31+b+idx % 1e9+7;两侧无回绕差异)。
 *   - 计时 = clock_gettime(CLOCK_MONOTONIC)(ctron 侧 now_ns 同源);
 *     热身 1000 轮;正式 N(env CTRON_HTTP_BENCH_N,缺省 100000)× 3 报文。
 *   - 每 parse 结果字段求和进校验和(防 DCE),与 ctron 侧同构(字段名按
 *     pico 出参对应)。
 *   - pico 契约:num_headers 入参 = headers 槽容量(上游 max_headers 取自
 *     该入参;置 0 即零槽 → -1),出参被覆写为实际头数。
 * 构建:cc -O1 -w -o bench_pico.bin bench_pico.c pico/picohttpparser.c(bench.sh)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pico/picohttpparser.h"

static const char *MSG1 =
    "GET /index.html?q=ctron&v=2 HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "User-Agent: ctron-bench/1.0\r\n"
    "Accept: text/html,application/xhtml+xml\r\n"
    "Accept-Language: en-US,en;q=0.9\r\n"
    "\r\n";

static const char *MSG2 =
    "POST /api/v1/submit HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "Content-Type: application/x-www-form-urlencoded\r\n"
    "Content-Length: 34\r\n"
    "X-Trace-Id: 7f3a9c2e51b8d4\r\n"
    "\r\n"
    "name=ctron&value=42&flag=ok&pad=xx";

static const char *MSG3 =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Transfer-Encoding: chunked\r\n"
    "Server: ctron-bench\r\n"
    "\r\n"
    "a\r\n0123456789\r\n"
    "a\r\nabcdefghij\r\n"
    "0\r\n\r\n";

static long long bc_digest(const unsigned char *a, long long na,
                           const unsigned char *b, long long nb,
                           const unsigned char *c, long long nc) {
    long long h = 17, idx = 0, i;
    for (i = 0; i < na; i++, idx++) { h = (h * 31 + a[i] + idx) % 1000000007LL; }
    for (i = 0; i < nb; i++, idx++) { h = (h * 31 + b[i] + idx) % 1000000007LL; }
    for (i = 0; i < nc; i++, idx++) { h = (h * 31 + c[i] + idx) % 1000000007LL; }
    return h;
}

static long long bc_now_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) { return -1; }
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

/* 一轮 = 三报文各 parse 一次;校验和累加防 DCE。返回 -1 即任何一次不完整 */
static long long bc_round(long long n1, long long n2, long long n3) {
    const char *method, *path, *msg;
    size_t method_len, path_len, msg_len, num_headers;
    int minor_version, status;
    struct phr_header headers[32];
    long long cs = 0;
    ssize_t r;

    num_headers = 32;
    r = phr_parse_request(MSG1, (size_t)n1, &method, &method_len, &path, &path_len,
                          &minor_version, headers, &num_headers, 0);
    if (r <= 0) { return -1; }
    cs += (long long)r + (long long)num_headers + (long long)method_len + (long long)path_len + minor_version;

    num_headers = 32;
    r = phr_parse_request(MSG2, (size_t)n2, &method, &method_len, &path, &path_len,
                          &minor_version, headers, &num_headers, 0);
    if (r <= 0) { return -1; }
    cs += (long long)r + (long long)num_headers;

    num_headers = 32;
    r = phr_parse_response(MSG3, (size_t)n3, &minor_version, &status, &msg, &msg_len,
                           headers, &num_headers, 0);
    if (r <= 0) { return -1; }
    cs += (long long)r + (long long)num_headers + status + minor_version;
    return cs;
}

int main(void) {
    long long n1 = (long long)strlen(MSG1);
    long long n2 = (long long)strlen(MSG2);
    long long n3 = (long long)strlen(MSG3);

    /* 正确性钉 */
    if (bc_round(n1, n2, n3) < 0) {
        fprintf(stderr, "bench_pico: corpus sanity fail\n");
        return 1;
    }

    long long digest = bc_digest((const unsigned char *)MSG1, n1,
                                 (const unsigned char *)MSG2, n2,
                                 (const unsigned char *)MSG3, n3);

    long long iters = 100000;
    const char *env = getenv("CTRON_HTTP_BENCH_N");
    if (env && *env) {
        char *end = NULL;
        long long v = strtoll(env, &end, 10);
        if (end && *end == '\0' && v > 0) { iters = v; }
    }

    /* 热身(不计时) */
    for (long long w = 0; w < 1000; w++) {
        if (bc_round(n1, n2, n3) < 0) { return 3; }
    }

    long long cs = 0;
    long long t0 = bc_now_ns();
    for (long long k = 0; k < iters; k++) {
        long long c = bc_round(n1, n2, n3);
        if (c < 0) { return 4; }
        cs += c;
    }
    long long t1 = bc_now_ns();

    long long total_ns = t1 - t0;
    long long reqs = iters * 3;
    printf("kind=pico\n");
    printf("digest=%lld\n", digest);
    printf("iters=%lld\n", iters);
    printf("reqs=%lld\n", reqs);
    printf("ns_total=%lld\n", total_ns);
    printf("ns_per_req=%lld\n", total_ns / reqs);
    printf("checksum=%lld\n", cs);
    return 0;
}
