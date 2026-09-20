/* c_smoke.c —— P2-C coro_hybrid C 级冒烟:net 垫片协程停车证明(纯 C,不经编译器)。
 *
 * 矩阵:ctron_net.c + ctron_rt.c 双链(真强符号顶替弱垫底),CTRON_RT=coro。
 * 主证(workers=1 时最严:仅一条 worker,若垫片滞留线程,进度协程即饿死):
 *   T0  裸面(linked-but-bare 矩阵位):sleep 走 nanosleep 回退;read_t 50ms
 *       超时门(P1 poll 路);read_t 永久 + 静默非阻塞 fd → EAGAIN 落 ct_err
 *       (P1 EAGAIN 分支逐字节回退证明);
 *   T1  停车点①:协程 read_t 永久,写端 +80ms 才写 → 深停车被就绪唤醒,
 *       字节正确;期间进度协程持续推进(worker 未被滞留);
 *   T2  停车点①超时:协程 read_t 60ms 静默 → ETIMEDOUT(协程内取 errno 槽
 *       —— TLS 槽属执行线程,主线程 join 后读不到,见任务报告登记),
 *       elapsed ∈ [55, 2000)ms;
 *   T3  停车点②:协程 ctron_net_write 256KB 灌非阻塞 socketpair,EAGAIN →
 *       wait_fd(写向)停车,主线程并发排干 → 全量送达且逐字节正确;
 *   T4  停车点④:协程 ctron_net_sleep_ms(80) → 定时器堆醒,elapsed ≥75ms;
 *   T5  停车点⑤:协程 udp_recvfrom 永久,主线程 +60ms 发单报文 → 就绪唤醒,
 *       报文正确。
 * run.sh 以 CTRON_RT_WORKERS=1 与 4 各整跑一遍(1 = 停车严格证,4 = 跨 worker
 * 唤醒形态)。全程 60s 看门狗:任何 lost-wakeup 类挂死 → 退出码 97。
 */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/socket.h>

#include "c_src/ctron_rt.h"

#define BIG_N (256 * 1024)

/* net 垫片面(ctron_net.c;ct_view6 声明序布局镜像,tests/ffi/repr_c 先例) */
typedef struct { int64_t* d; int64_t n; } ct_view6;
int64_t ctron_net_now_ns(void);
void    ctron_net_sleep_ms(int64_t ms);
int64_t ctron_net_last_errno(void);
int64_t ctron_net_read_t(int64_t fd, ct_view6 buf, int64_t cap, int64_t timeout_ms);
int64_t ctron_net_write(int64_t fd, ct_view6 buf, int64_t n);
int64_t ctron_net_udp_recvfrom(int64_t fd, ct_view6 buf, int64_t cap, int64_t timeout_ms);

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
        exit(1); \
    } \
} while (0)

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static double ms_of(uint64_t dt) { return (double)dt / 1e6; }

static void bare_sleep_ms(int ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000 * 1000 };
    nanosleep(&ts, NULL);
}

/* ───────── 看门狗(lost-wakeup 类挂死 → 97) ───────── */
static atomic_int g_alive = 1;
static void *watchdog(void *unused)
{
    (void)unused;
    for (int i = 0; i < 600; i++) {
        bare_sleep_ms(100);
        if (!atomic_load(&g_alive)) return NULL;
    }
    fprintf(stderr, "FAIL: WATCHDOG TIMEOUT — 疑似停车 lost-wakeup 挂死\n");
    _exit(97);
}

/* ───────── 进度协程:停车窗口内 worker 必须仍可调度它(workers=1 最严) ───────── */
static atomic_int g_ticks;
static void progress_body(void *arg)
{
    (void)arg;
    for (;;) {
        atomic_fetch_add_explicit(&g_ticks, 1, memory_order_relaxed);
        ctron_rt_sleep_ms(5);
    }
}
static int ticks(void) { return atomic_load_explicit(&g_ticks, memory_order_relaxed); }

static void set_nonblock(int fd)
{
    CHECK(fcntl(fd, F_SETFL, O_NONBLOCK) == 0, "SETFL O_NONBLOCK");
}

/* ───────── 协程体 ───────── */
typedef struct {                  /* 读面(read_t / udp_recvfrom 共用载荷) */
    int      fd;
    int      cap;
    int      timeout;              /* <0 永久 */
    int      is_udp;
    int64_t *lane;
    int64_t  rc;
    int64_t  eno;                 /* 协程上下文内取 TLS errno 槽 */
    uint64_t t0;
    uint64_t elapsed;
} rctx_t;

static void read_body(void *p)
{
    rctx_t *c = (rctx_t*)p;
    ct_view6 v = { c->lane, c->cap };
    c->t0 = now_ns();
    c->rc = c->is_udp
        ? ctron_net_udp_recvfrom(c->fd, v, c->cap, c->timeout)
        : ctron_net_read_t(c->fd, v, c->cap, c->timeout);
    c->eno = ctron_net_last_errno();
    c->elapsed = now_ns() - c->t0;
}

static int64_t g_big[BIG_N];      /* 写压载荷:(i*7+13)&0xff */
typedef struct {
    int      fd;
    int64_t  rc;
    uint64_t elapsed;
} wctx_t;
static void write_body(void *p)
{
    wctx_t *w = (wctx_t*)p;
    ct_view6 v = { g_big, BIG_N };
    uint64_t t0 = now_ns();
    w->rc = ctron_net_write(w->fd, v, BIG_N);
    w->elapsed = now_ns() - t0;
}

typedef struct {
    int      ms;
    uint64_t elapsed;
} sctx_t;
static void sleep_body(void *p)
{
    sctx_t *s = (sctx_t*)p;
    uint64_t t0 = now_ns();
    ctron_net_sleep_ms(s->ms);
    s->elapsed = now_ns() - t0;
}

int main(void)
{
    pthread_t wdt;
    const char *wenv = getenv("CTRON_RT_WORKERS");
    int workers = wenv ? atoi(wenv) : 1;

    pthread_create(&wdt, NULL, watchdog, NULL);
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- T0:裸面(linked-but-bare:rt 已链入、主线程 current()==NULL) ---- */
    {
        uint64_t t0, dt_sleep, dt_read;
        t0 = now_ns();
        ctron_net_sleep_ms(30);                       /* 停车判据假 → nanosleep 回退 */
        dt_sleep = now_ns() - t0;
        CHECK(dt_sleep >= 25000000ull && dt_sleep < 2000000000ull,
              "T0: 裸面 sleep 30ms 未按时返回");

        {
            int sp[2];
            static int64_t lane[64];
            ct_view6 v = { lane, 64 };
            int64_t rc;
            CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0, "T0: socketpair");
            set_nonblock(sp[0]);
            set_nonblock(sp[1]);
            t0 = now_ns();
            rc = ctron_net_read_t(sp[0], v, 64, 50);       /* P1 poll 超时门 */
            dt_read = now_ns() - t0;
            CHECK(rc < 0 && ctron_net_last_errno() == ETIMEDOUT,
                  "T0: 裸面 read_t 50ms 未得 ETIMEDOUT");
            CHECK(dt_read >= 45000000ull && dt_read < 2000000000ull,
                  "T0: 裸面超时门时序坏");
            rc = ctron_net_read_t(sp[0], v, 64, 0);        /* 永久 + 静默非阻塞 → EAGAIN */
            CHECK(rc < 0 && ctron_net_last_errno() == EAGAIN,
                  "T0: 裸面 EAGAIN 未走 P1 ct_err 回退");
            close(sp[0]);
            close(sp[1]);
            printf("PASS T0 bare plane: sleep30=%.1fms read-timeout50=%.1fms, eagain->ct_err ok\n",
                   ms_of(dt_sleep), ms_of(dt_read));
        }
    }

    /* ---- rt 初始化 + 进度协程(此后全程在跑) ---- */
    ctron_rt_init(workers);
    CHECK(ctron_rt_active() == 1, "CTRON_RT=coro 未生效(run.sh 负责设 env)");
    CHECK(ctron_rt_current() == NULL, "主线程(裸)current() 必须 NULL");
    for (int64_t i = 0; i < BIG_N; i++) g_big[i] = (i * 7 + 13) & 0xff;
    ctron_rt_run(progress_body, NULL, (void*)(uintptr_t)0xF0);
    bare_sleep_ms(20);                                /* 让进度协程起跑 */

    /* ---- T1:停车点①——协程 read_t 永久,写端延迟 80ms 唤醒 ---- */
    {
        int sp[2];
        static int64_t lane[64];
        static rctx_t A;
        memset(&A, 0, sizeof A);
        A.fd = 0; A.cap = 64; A.timeout = -1; A.lane = lane;
        int t_before = ticks();
        CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0, "T1: socketpair");
        set_nonblock(sp[0]);
        set_nonblock(sp[1]);
        A.fd = sp[0];
        ctron_rt_run(read_body, &A, (void*)(uintptr_t)0xA1);
        bare_sleep_ms(80);                            /* 写端延迟:协程必须已深停车 */
        CHECK(write(sp[1], "P2C", 4) == 4, "T1: 主线程写");
        ctron_rt_join_key((void*)(uintptr_t)0xA1);
        CHECK(A.rc == 4, "T1: 唤醒后未读满 4 字节");
        CHECK(A.lane[0] == 'P' && A.lane[1] == '2' && A.lane[2] == 'C',
              "T1: 字节内容错");
        CHECK(A.elapsed >= 75000000ull && A.elapsed < 5000000000ull,
              "T1: 唤醒时序坏(过早=未停车/过晚=就绪唤醒失效)");
        CHECK(ticks() - t_before >= 3, "T1: 停车期间进度协程饿死(worker 被滞留)");
        close(sp[0]);
        close(sp[1]);
        printf("PASS T1 read-park: woke at %.1fms with 'P2C', ticks+%d\n",
               ms_of(A.elapsed), ticks() - t_before);
    }

    /* ---- T2:停车点①超时——协程 read_t 60ms 静默 → ETIMEDOUT ---- */
    {
        int sp[2];
        static int64_t lane[64];
        static rctx_t B;
        memset(&B, 0, sizeof B);
        B.fd = 0; B.cap = 64; B.timeout = 60; B.lane = lane;
        int t_before = ticks();
        CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0, "T2: socketpair");
        set_nonblock(sp[0]);
        set_nonblock(sp[1]);
        B.fd = sp[0];
        ctron_rt_run(read_body, &B, (void*)(uintptr_t)0xA2);
        ctron_rt_join_key((void*)(uintptr_t)0xA2);
        CHECK(B.rc < 0, "T2: 静默 read 未返回负");
        CHECK(B.eno == ETIMEDOUT, "T2: 协程内 errno 槽未置 ETIMEDOUT");
        CHECK(B.elapsed >= 55000000ull && B.elapsed < 2000000000ull,
              "T2: 超时停车未按时醒");
        CHECK(ticks() - t_before >= 3, "T2: 超时窗口内进度协程饿死");
        close(sp[0]);
        close(sp[1]);
        printf("PASS T2 read-park-timeout: ETIMEDOUT at %.1fms (timeout=60ms), ticks+%d\n",
               ms_of(B.elapsed), ticks() - t_before);
    }

    /* ---- T3:停车点②——256KB 灌非阻塞 socketpair,EAGAIN 停车 + 并发排干 ---- */
    {
        int sp[2];
        static wctx_t W;
        static unsigned char rb[4096];
        int64_t got = 0;
        memset(&W, 0, sizeof W);
        int t_before = ticks();
        CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0, "T3: socketpair");
        set_nonblock(sp[0]);
        set_nonblock(sp[1]);
        W.fd = sp[0];
        ctron_rt_run(write_body, &W, (void*)(uintptr_t)0xA3);
        while (got < BIG_N) {                         /* 主线程并发排干:迫使多轮 EAGAIN 停车 */
            ssize_t k = read(sp[1], rb, sizeof rb);
            if (k < 0) {
                if (errno == EINTR) continue;
                CHECK(errno == EAGAIN, "T3: 排干读非 EAGAIN 错");
                bare_sleep_ms(1);
                continue;
            }
            if (k == 0) break;
            for (ssize_t i = 0; i < k; i++) {
                int64_t want = (got * 7 + 13) & 0xff;
                CHECK((unsigned char)want == rb[i], "T3: 字节序错位");
                got++;
            }
        }
        CHECK(got == BIG_N, "T3: 排干不足全量");
        ctron_rt_join_key((void*)(uintptr_t)0xA3);
        CHECK(W.rc == BIG_N, "T3: write 未发满全量");
        CHECK(ticks() - t_before >= 3, "T3: 写停车期间进度协程饿死(worker 被滞留)");
        close(sp[0]);
        close(sp[1]);
        printf("PASS T3 write-park: %d bytes via EAGAIN parks, ticks+%d\n",
               BIG_N, ticks() - t_before);
    }

    /* ---- T4:停车点④——协程 sleep_ms(80) 定时器堆醒 ---- */
    {
        static sctx_t S;
        memset(&S, 0, sizeof S);
        S.ms = 80;
        int t_before = ticks();
        ctron_rt_run(sleep_body, &S, (void*)(uintptr_t)0xA4);
        ctron_rt_join_key((void*)(uintptr_t)0xA4);
        CHECK(S.elapsed >= 75000000ull && S.elapsed < 2000000000ull,
              "T4: 协程 sleep 未按时醒");
        CHECK(ticks() - t_before >= 3, "T4: sleep 停车期间进度协程饿死");
        printf("PASS T4 sleep-park: %.1fms (sleep=80ms), ticks+%d\n",
               ms_of(S.elapsed), ticks() - t_before);
    }

    /* ---- T5:停车点⑤——协程 udp_recvfrom 永久,延迟单报文唤醒 ---- */
    {
        int ufd = (int)socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in a;
        socklen_t alen;
        uint16_t port;
        static int64_t ulane[64];
        static rctx_t U;
        int t_before = ticks();
        CHECK(ufd >= 0, "T5: udp socket");
        memset(&a, 0, sizeof a);
        a.sin_family = AF_INET;
        a.sin_port = 0;
        a.sin_addr.s_addr = htonl(0x7f000001u);       /* 127.0.0.1,CI 纪律零外联 */
        CHECK(bind(ufd, (struct sockaddr*)&a, sizeof a) == 0, "T5: udp bind");
        alen = sizeof a;
        CHECK(getsockname(ufd, (struct sockaddr*)&a, &alen) == 0, "T5: getsockname");
        port = ntohs(a.sin_port);
        set_nonblock(ufd);

        memset(&U, 0, sizeof U);
        U.fd = ufd; U.cap = 64; U.timeout = -1; U.lane = ulane; U.is_udp = 1;
        ctron_rt_run(read_body, &U, (void*)(uintptr_t)0xA5);
        bare_sleep_ms(60);
        {
            int s = (int)socket(AF_INET, SOCK_DGRAM, 0);
            struct sockaddr_in d;
            CHECK(s >= 0, "T5: sender socket");
            memset(&d, 0, sizeof d);
            d.sin_family = AF_INET;
            d.sin_port = htons(port);
            d.sin_addr.s_addr = htonl(0x7f000001u);
            CHECK(sendto(s, "UDPXY", 5, 0, (struct sockaddr*)&d, sizeof d) == 5,
                  "T5: sendto 单报文");
            close(s);
        }
        ctron_rt_join_key((void*)(uintptr_t)0xA5);
        CHECK(U.rc == 5, "T5: 协程未收满 5 字节");
        CHECK(U.lane[0] == 'U' && U.lane[4] == 'Y', "T5: 报文内容错");
        CHECK(U.elapsed >= 55000000ull && U.elapsed < 5000000000ull,
              "T5: 唤醒时序坏");
        CHECK(ticks() - t_before >= 3, "T5: 停车期间进度协程饿死");
        close(ufd);
        printf("PASS T5 udp-park: woke at %.1fms with 'UDPXY', ticks+%d\n",
               ms_of(U.elapsed), ticks() - t_before);
    }

    atomic_store(&g_alive, 0);
    printf("SMOKE OK: coro_hybrid c_smoke 全绿 (workers=%d)\n", workers);
    return 0;
}
