/* baseline_echo.c —— 手写 C thread-per-connection echo 基线(Task 8,P1-E 吞吐门禁)
 * 结构镜像 examples/ctecho,差异仅语言/门面:同垫片默认值(§11.3,参考
 * std/net/c_src/ctron_net.c)——listen 置 SO_REUSEADDR,accept/connect 置
 * TCP_NODELAY + SO_KEEPALIVE;每 accept 一 pthread(read/write 循环,4KB 栈上缓冲)。
 *
 * 用法:
 *   baseline_echo                       # 服务器(CTECHO_PORT,缺省 8080;0 = 内核分配)
 *   baseline_echo client <port> [rounds]  # 压测客户端:单连接 64B ping-pong ×rounds
 *                                         # (缺省 100000),3 轮取最小,gettimeofday 计时,打印 µs
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/time.h>

/* §11.3 TCP 默认面:与垫片 ct_tcp_defaults 同款 */
static void ct_tcp_defaults(int fd) {
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
}

/* 每连接一线程:读到 eof/错即止;逐段回写(同 ctecho handle,n!=w 断链) */
static void *handle(void *arg) {
    int fd = (int)(intptr_t)arg;
    char buf[4096];
    ct_tcp_defaults(fd);
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(fd, buf + off, (size_t)(n - off));
            if (w <= 0) goto done;
            off += w;
        }
    }
done:
    close(fd);
    return NULL;
}

static int serve(int port) {
    int one = 1;
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) { perror("socket"); return 1; }
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)); /* §11.3 */
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons((uint16_t)port);
    if (bind(lfd, (struct sockaddr *)&a, sizeof(a)) != 0) { perror("bind"); return 1; }
    if (listen(lfd, 128) != 0) { perror("listen"); return 1; }
    socklen_t al = sizeof(a);
    if (getsockname(lfd, (struct sockaddr *)&a, &al) == 0)
        printf("baseline listening on 127.0.0.1:%d\n", ntohs(a.sin_port));
    fflush(stdout);
    pthread_t t;
    for (;;) {
        int cfd = accept(lfd, NULL, NULL);
        if (cfd < 0) break;
        if (pthread_create(&t, NULL, handle, (void *)(intptr_t)cfd) == 0)
            pthread_detach(t);
        else
            close(cfd);
    }
    return 0;
}

/* 单轮:连接 → rounds 次 64B write/read 往返(读侧补齐到 64B)→ 返回耗时 µs,-1 败 */
static long long pingpong_once(int port, long rounds) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons((uint16_t)port);
    if (connect(fd, (struct sockaddr *)&a, sizeof(a)) != 0) { close(fd); return -1; }
    ct_tcp_defaults(fd); /* 客户端侧同默认面,双端一致 */
    struct timeval tv = { 5, 0 }; /* 探活/压测兜底:错连非 echo 端口时不悬挂 */
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    char msg[64], back[64];
    memset(msg, 'x', sizeof(msg));
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    for (long i = 0; i < rounds; i++) {
        ssize_t off = 0;
        while (off < (ssize_t)sizeof(msg)) {
            ssize_t w = write(fd, msg + off, sizeof(msg) - (size_t)off);
            if (w <= 0) { close(fd); return -1; }
            off += w;
        }
        off = 0;
        while (off < (ssize_t)sizeof(back)) {
            ssize_t r = read(fd, back + off, sizeof(back) - (size_t)off);
            if (r <= 0) { close(fd); return -1; }
            off += r;
        }
    }
    gettimeofday(&t1, NULL);
    close(fd);
    return (long long)(t1.tv_sec - t0.tv_sec) * 1000000LL + (t1.tv_usec - t0.tv_usec);
}

static int client(int port, long rounds) {
    long long best = -1;
    for (int run = 0; run < 3; run++) { /* 3 取最小 */
        long long us = pingpong_once(port, rounds);
        if (us < 0) { fprintf(stderr, "client: run %d failed\n", run + 1); return 1; }
        if (best < 0 || us < best) best = us;
    }
    printf("%lld\n", best);
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 3 && strcmp(argv[1], "client") == 0)
        return client(atoi(argv[2]), argc >= 4 ? atol(argv[3]) : 100000);
    const char *e = getenv("CTECHO_PORT");
    return serve(e && *e ? atoi(e) : 8080);
}
