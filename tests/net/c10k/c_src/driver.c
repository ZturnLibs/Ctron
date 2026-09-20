/* driver.c —— C10K 门禁驱动(P2-F;nightly/本地,不入 CI 主环)。
 *
 * 对 ctecho(coro 模式)并发建 N 条 TCP 连接,全程保持不关(并发度 = N),
 * 再逐条做一次 64B 回显往返并逐字节校验。判据:N/N 全部成功才算过
 * (rc=0);任一建连/回显失败 rc=1。budget 秒总预算看门狗,任何阶段
 * 超时即败,不悬挂。
 *
 * 用法: driver <port> <N> [budget_s=180]
 * 输出末行: c10k-driver: N=<n> echo_ok=<k> fail=<f> elapsed=<t>s
 *
 * 实现口径:非阻塞 connect(EINPROGRESS 常态)+ poll(POLLOUT)/getsockopt
 * (SO_ERROR) 收敛建连(监听 backlog 溢出的 SYN 重传由 budget 兜住);
 * 回显段 poll(POLLOUT/POLLIN) 驱动补齐读写。fd 不够(socket() EMFILE)
 * 时响亮报错——抬高 ulimit 是 run.sh 的前置职责。
 */
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static int xfcntl(int fd, int flags)
{
    int f = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, f | flags);
}

/* 建连收敛:全部 fd poll(POLLOUT),就绪者 getsockopt(SO_ERROR) 定案 */
static int finish_connects(int *fd, char *state, int n, double deadline)
{
    int pending = 0;
    for (int i = 0; i < n; i++)
        if (state[i] == 0) pending++;

    while (pending > 0) {
        if (now_s() > deadline) {
            fprintf(stderr, "driver: connect budget exhausted, remaining=%d\n", pending);
            return -1;
        }
        struct pollfd *pf = calloc((size_t)pending, sizeof *pf);
        int *idx = calloc((size_t)pending, sizeof *idx);
        int k = 0;
        for (int i = 0; i < n; i++) {
            if (state[i] == 0) {
                pf[k].fd = fd[i];
                pf[k].events = POLLOUT;
                idx[k] = i;
                k++;
            }
        }
        poll(pf, (nfds_t)k, 200);
        for (int j = 0; j < k; j++) {
            if (!(pf[j].revents & (POLLOUT | POLLERR | POLLHUP))) continue;
            int i = idx[j], err = 0;
            socklen_t l = sizeof err;
            getsockopt(fd[i], SOL_SOCKET, SO_ERROR, &err, &l);
            if (err == 0) { state[i] = 1; pending--; }
            else {
                fprintf(stderr, "driver: connect #%d failed: %s\n", i, strerror(err));
                free(pf); free(idx);
                return -1;
            }
        }
        free(pf); free(idx);
    }
    return 0;
}

/* 64B 定长写补齐(nonblocking;POLLIN/POLLOUT 按 dir 等就绪) */
static int xfer_all(int fd, char *buf, int len, int dir, double deadline)
{
    int off = 0;
    while (off < len) {
        double left = deadline - now_s();
        if (left <= 0) return -1;
        struct pollfd p = { fd, dir == 0 ? POLLOUT : POLLIN, 0 };
        poll(&p, 1, (int)(left * 1000.0));
        ssize_t r;
        if (dir == 0)
            r = write(fd, buf + off, (size_t)(len - off));
        else
            r = read(fd, buf + off, (size_t)(len - off));
        if (r > 0) { off += (int)r; continue; }
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) continue;
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: driver <port> <N> [budget_s=180]\n");
        return 2;
    }
    int port = atoi(argv[1]);
    int n = atoi(argv[2]);
    double budget = argc >= 4 ? atof(argv[3]) : 180.0;
    if (n <= 0 || port <= 0) { fprintf(stderr, "driver: bad args\n"); return 2; }
    double t0 = now_s(), deadline = t0 + budget;

    int *fd = calloc((size_t)n, sizeof *fd);
    char *state = calloc((size_t)n, 1);
    char (*msg)[64] = calloc((size_t)n, 64);
    if (!fd || !state || !msg) { fprintf(stderr, "driver: oom\n"); return 2; }
    for (int i = 0; i < n; i++) {
        fd[i] = -1;
        memset(msg[i], 'A' + (i % 26), 64);
    }

    /* 段一:全量非阻塞建连(全部保持打开 → 并发度 = n) */
    for (int i = 0; i < n; i++) {
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) {
            fprintf(stderr, "driver: socket #%d: %s(fd 不够——先抬高 ulimit -n)\n",
                    i, strerror(errno));
            return 1;
        }
        xfcntl(s, O_NONBLOCK);
        int one = 1;
        setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
        struct sockaddr_in a;
        memset(&a, 0, sizeof a);
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.sin_port = htons((unsigned short)port);
        if (connect(s, (struct sockaddr *)&a, sizeof a) != 0 && errno != EINPROGRESS) {
            fprintf(stderr, "driver: connect #%d: %s\n", i, strerror(errno));
            return 1;
        }
        fd[i] = s;
    }
    if (finish_connects(fd, state, n, deadline) != 0) return 1;
    double t_conn = now_s() - t0;

    /* 段二:逐条 64B 回显往返,逐字节校验(连接全程保持 → 段二期间
     * 服务端仍同时挂着 n 条活连接) */
    int ok = 0, bad = 0;
    char back[64];
    for (int i = 0; i < n; i++) {
        if (xfer_all(fd[i], msg[i], 64, 0, deadline) != 0 ||
            xfer_all(fd[i], back, 64, 1, deadline) != 0 ||
            memcmp(msg[i], back, 64) != 0) {
            bad++;
            continue;
        }
        ok++;
    }
    double t_all = now_s() - t0;

    for (int i = 0; i < n; i++)
        if (fd[i] >= 0) close(fd[i]);

    printf("c10k-driver: N=%d echo_ok=%d fail=%d connect=%.1fs total=%.1fs\n",
           n, ok, bad, t_conn, t_all);
    free(fd); free(state); free(msg);
    return (ok == n && bad == 0) ? 0 : 1;
}
