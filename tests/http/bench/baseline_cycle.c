/* baseline_cycle.c —— 手写 C HTTP 全请求周期基线 + 同源压测客户端(P6-F 门禁)
 * 结构镜像 tests/net/bench/baseline_echo.c(§11.3 默认值同款),差异仅协议形:
 * HTTP GET → 定长响应 → close(与 ctron 侧 bench_cycle.ct / todo_api 装配同构,
 * Connection:close 一连接一请求 = P6-E 在册生产形态;keep-alive 为志向)。
 *
 * 双基线口径(取快者为对照,最强诚实基线):
 *   serve          阻塞 accept 串行环(单线程,无反应器开销)
 *   serve-reactor  kqueue(darwin)/epoll(linux) 事件环串行服务
 *                    (spec §九「手写 C epoll」字面口径;串行单连接下事件环
 *                     只多一次 kevent/epoll_wait 系统调用,预期慢于阻塞形)
 * 客户端(client 模式)由同一二进制驱动全部服务端(同客户端协议,同族口径):
 *   每轮 = connect → write 请求 → read 至 EOF(服务端 close)→ close;
 *   关闭置 SO_LINGER{1,0} 以 RST 收尾——双方零 TIME_WAIT,长跑免端口耗尽
 *   (协议形两侧同构,数据在 RST 前已按序送达,计时口径不受影响)。
 *   热身 200 轮不计时;CLOCK_MONOTONIC 整段计时;FNV-1a 64 摘要 pin 响应字节。
 *
 * 用法:
 *   baseline_cycle serve [port]          # 阻塞串行基线(GET /__done 优雅退出)
 *   baseline_cycle serve-reactor [port]  # 事件环串行基线(同上)
 *   baseline_cycle client <port> [rounds] [path]   # 压测客户端(缺省 10000 轮 /health)
 * 输出(client):ns_per_req=<整> digest=<hex> rounds=<n> bytes=<n>
 * 退出码:0 正常(含 /__done 排空退出);非 0 = 失败。
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <time.h>
#ifdef __APPLE__
#include <sys/event.h>   /* kqueue */
#else
#include <sys/epoll.h>   /* epoll */
#endif

/* 与 ctron 侧 RESP 逐字节同文(digest pin 自证) */
static const char RESP[] =
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 18\r\n"
    "\r\n"
    "ctron-cycle-bench\n";
static const char RESP404[] = "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n";
static const size_t RESP_LEN = sizeof(RESP) - 1;
static const size_t RESP404_LEN = sizeof(RESP404) - 1;

/* §11.3 TCP 默认面:与垫片 ct_tcp_defaults 同款 */
static void ct_tcp_defaults(int fd) {
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
}

static int make_listener(int port) {
    int one = 1;
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) { perror("socket"); return -1; }
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)); /* §11.3 */
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons((uint16_t)port);
    if (bind(lfd, (struct sockaddr *)&a, sizeof(a)) != 0) { perror("bind"); return -1; }
    if (listen(lfd, 128) != 0) { perror("listen"); return -1; }
    return lfd;
}

/* 读一整个请求头(至 \r\n\r\n;请求无体)。返回头尾偏移,-1 = 断/越限。
 * done 置 1 当且仅当请求行含 " /__done "(与 ctron 侧路由 id=2 同形)。 */
static long read_request(int fd, char *buf, size_t cap, int *done) {
    size_t n = 0;
    *done = 0;
    while (n + 1 < cap) {
        ssize_t r = read(fd, buf + n, cap - 1 - n);
        if (r <= 0) return -1;
        n += (size_t)r;
        buf[n] = 0;
        char *end = strstr(buf, "\r\n\r\n");
        if (end) {
            if (strstr(buf, " /__done ")) *done = 1;
            return (long)(end - buf);
        }
    }
    return -1;
}

/* 服务一连接:读头 → 定长响应 → close。返回 1 = 收到停机指令。
 * 注:服务端 close 恒为 FIN(勿试 linger-0 RST —— RST 会丢客户端未读响应,
 * 2026-09-24 试验实证;客户端 RST 则在 EOF 后,无数据损耗)。 */
static int serve_conn(int fd) {
    char buf[4096];
    int done = 0;
    ct_tcp_defaults(fd);
    long hl = read_request(fd, buf, sizeof(buf), &done);
    if (hl < 0) { close(fd); return 0; }
    /* 路由(与 ctron 侧同形两路由:/health 200;/__done 200+停机;其余 404) */
    const char *resp = strstr(buf, " /health ") ? RESP : RESP404;
    size_t len = (resp == RESP) ? RESP_LEN : RESP404_LEN;
    size_t off = 0;
    while (off < len) {
        ssize_t w = write(fd, resp + off, len - off);
        if (w <= 0) break;
        off += (size_t)w;
    }
    close(fd);
    return done;
}

static int serve_loop(int port, int reactor) {
    int lfd = make_listener(port);
    if (lfd < 0) return 1;
    if (reactor) {
#ifdef __APPLE__
        int kq = kqueue();
        if (kq < 0) { perror("kqueue"); return 1; }
        struct kevent ev;
        EV_SET(&ev, lfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
        if (kevent(kq, &ev, 1, NULL, 0, NULL) < 0) { perror("kevent(add)"); return 1; }
        for (;;) {
            struct kevent out;
            int ne = kevent(kq, NULL, 0, &out, 1, NULL);
            if (ne <= 0) { perror("kevent(wait)"); return 1; }
            if ((int)out.ident == lfd) {
                int cfd = accept(lfd, NULL, NULL);
                if (cfd >= 0 && serve_conn(cfd)) return 0;
            }
        }
#else
        int ep = epoll_create1(0);
        if (ep < 0) { perror("epoll_create1"); return 1; }
        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN;
        ev.data.fd = lfd;
        if (epoll_ctl(ep, EPOLL_CTL_ADD, lfd, &ev) < 0) { perror("epoll_ctl"); return 1; }
        for (;;) {
            struct epoll_event out;
            int ne = epoll_wait(ep, &out, 1, -1);
            if (ne <= 0) { perror("epoll_wait"); return 1; }
            if (out.data.fd == lfd) {
                int cfd = accept(lfd, NULL, NULL);
                if (cfd >= 0 && serve_conn(cfd)) return 0;
            }
        }
#endif
    } else {
        for (;;) {
            int cfd = accept(lfd, NULL, NULL);
            if (cfd < 0) { perror("accept"); return 1; }
            if (serve_conn(cfd)) return 0;
        }
    }
}

static uint64_t fnv1a(uint64_t h, const char *p, size_t n);

/* 一轮:connect → write → read 至 EOF(累积 FNV-1a)→ linger0 close */
static int one_round(int port, const char *req, int reqlen, char *buf, size_t cap,
                     uint64_t *dig, unsigned long long *bytes) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return 1;
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons((uint16_t)port);
    if (connect(fd, (struct sockaddr *)&a, sizeof(a)) != 0) { close(fd); return 1; }
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    size_t off = 0;
    while (off < (size_t)reqlen) {
        ssize_t w = write(fd, req + off, (size_t)reqlen - off);
        if (w <= 0) { close(fd); return 1; }
        off += (size_t)w;
    }
    ssize_t n;
    while ((n = read(fd, buf, cap)) > 0) {
        if (dig) { *dig = fnv1a(*dig, buf, (size_t)n); *bytes += (unsigned long long)n; }
    }
    struct linger lg;
    lg.l_onoff = 1;
    lg.l_linger = 0;   /* RST 收尾:双方零 TIME_WAIT(口径见文件头) */
    setsockopt(fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));
    close(fd);
    return (n < 0) ? 1 : 0;   /* 读到 EOF(0)= 正常;负 = 错 */
}

static uint64_t fnv1a(uint64_t h, const char *p, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        h ^= (unsigned char)p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

static int client_run(int port, long rounds, const char *path) {
    char req[256];
    int rl = snprintf(req, sizeof(req),
                      "GET %s HTTP/1.1\r\nHost: b\r\n\r\n", path);
    if (rl <= 0 || rl >= (int)sizeof(req)) return 2;
    char buf[4096];
    long warm = (rounds / 2 < 200) ? rounds / 2 : 200;   /* 探针小轮数 → 零热身(就绪探测用) */
    long r;
    for (r = 0; r < warm; r++) { if (one_round(port, req, rl, buf, sizeof(buf), NULL, NULL)) return 3; }
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    uint64_t dig = 0xcbf29ce484222325ULL;
    unsigned long long bytes = 0;
    for (r = 0; r < rounds; r++) {
        if (one_round(port, req, rl, buf, sizeof(buf), &dig, &bytes)) return 3;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long long ns = (long long)(t1.tv_sec - t0.tv_sec) * 1000000000LL
                 + (long long)(t1.tv_nsec - t0.tv_nsec);
    printf("ns_per_req=%lld\n", ns / rounds);
    printf("digest=%016llx\n", (unsigned long long)dig);
    printf("rounds=%ld\n", rounds);
    printf("bytes=%llu\n", bytes);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s serve|serve-reactor [port] | client <port> [rounds] [path]\n", argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "client") == 0) {
        if (argc < 3) return 2;
        int port = atoi(argv[2]);
        long rounds = (argc > 3) ? atol(argv[3]) : 10000;
        const char *path = (argc > 4) ? argv[4] : "/health";
        return client_run(port, rounds, path);
    }
    int reactor = (strcmp(argv[1], "serve-reactor") == 0);
    int port = (argc > 2) ? atoi(argv[2]) : 8093;
    return serve_loop(port, reactor);
}
