/* rt_reactor_smoke/src/main.c —— P2-B 冒烟(纯 C main + pthread,不经 Ctron 编译器)。
 *
 * 覆盖(任务书口径,全部经 ctron_rt_wait_fd 真·reactor 路径):
 *   T0  裸线程回退:可写端立返、静默读端 ~30ms 超时(直接 poll 路径);
 *   T1  B 先 park 在 wait_fd(read),A 于 +50ms 写 1 字节 → B 被"就绪唤醒"
 *       (非 poll-race/非超时:elapsed ≥45ms 且 << 5000ms 超时)并读到该字节;
 *   T2  写向:A 等可写(空闲 socketpair 立即就绪 → 水平触发"注册即就绪"路径),
 *       灌满至 EAGAIN 后再等可写,B 排走 ≥4KB(macOS AF_UNIX EVFILT_WRITE 低水位
 *       阈,排太少不触发)→ A 醒并补写 'Z',B 端读到最后;
 *   T3  超时:静默 fd 上 wait_fd(read, 50ms) → elapsed ∈ [45, 400)ms,无悬挂;
 *   T4a 对端 close → EOF 即就绪 → 等待者提前醒,read 返回 0;
 *   T4b 自 close(等的就是它)→ 内核静默摘注册(登记限制),等待者靠超时醒,
 *       无崩溃;随后一次正常交换证明 reactor 仍健康;
 *   T5  压力:8 协程 × 16 轮 socketpair 往返(8 对,并发深 park 于 wait_fd,
 *       reactor 扇入交付),worker=2 与 4 各整跑一遍(run.sh 循环)。
 *
 * 全程 60s 看门狗:任何 lost-wakeup 类挂死 → 退出码 97。
 * 时序口径:CLOCK_MONOTONIC;下界断言排除"stub 式立返",上界断言排除
 * "只有超时能醒"(若就绪唤醒失效,T1/T2/T4a 只能靠 5000ms 超时退出 → 必炸上界)。
 */
#include <assert.h>
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

#include <sys/socket.h>

#include "ctron_rt.h"

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
        exit(1); \
    } \
} while (0)

static double ms_of(uint64_t dt_ns) { return (double)dt_ns / 1e6; }

/* ───────── 看门狗(lost-wakeup 类挂死 → 97) ───────── */
static atomic_int g_alive = 1;
static void *watchdog(void *unused)
{
    (void)unused;
    for (int i = 0; i < 600; i++) {              /* 60s,100ms 步进 */
        struct timespec ts = { 0, 100 * 1000 * 1000 };
        nanosleep(&ts, NULL);
        if (!atomic_load(&g_alive)) return NULL;
    }
    fprintf(stderr, "FAIL: WATCHDOG TIMEOUT — 疑似 lost-wakeup 挂死\n");
    _exit(97);
}

static void sleep_ms(int ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000 * 1000 };
    nanosleep(&ts, NULL);
}

/* ───────── T0: 裸线程 wait_fd 回退(直接 poll) ───────── */
static void t0_bare(void)
{
    int sp[2];
    uint64_t t0, dt_w, dt_t;
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0, "T0: socketpair");

    t0 = now_ns();
    ctron_rt_wait_fd(sp[1], 1, 1000);            /* 空闲写端:立即可写 */
    dt_w = now_ns() - t0;
    CHECK(dt_w < 200000000ull, "T0: 空闲写端 wait_fd 未立即返回(直接 poll 路径坏)");

    t0 = now_ns();
    ctron_rt_wait_fd(sp[0], 0, 30);              /* 静默读端:30ms 超时 */
    dt_t = now_ns() - t0;
    CHECK(dt_t >= 25000000ull && dt_t <= 400000000ull, "T0: 读端 30ms 超时未按时返回");
    close(sp[0]); close(sp[1]);
    printf("PASS T0 bare-thread wait_fd: writable=%llu.%03llums, quiet-30ms-timeout=%llu.%03llums\n",
           (unsigned long long)(dt_w / 1000000ull),
           (unsigned long long)((dt_w % 1000000ull) / 1000ull),
           (unsigned long long)(dt_t / 1000000ull),
           (unsigned long long)((dt_t % 1000000ull) / 1000ull));
}

/* ───────── T1: B 深 park 等可读,A(+50ms)写 1 字节唤醒 ───────── */
typedef struct {
    int      rfd;
    atomic_int entered;
    uint64_t dt_ns;
    int      got;
} t1_ctx;
static void t1_body(void *p)
{
    t1_ctx *c = (t1_ctx*)p;
    char b = 0;
    atomic_store_explicit(&c->entered, 1, memory_order_release);
    ctron_rt_wait_fd(c->rfd, 0, 5000);           /* 深 park:等对端写 */
    c->dt_ns = now_ns() - c->dt_ns;              /* dt_ns 先天0,入口处存 t0 */
    c->got = (int)read(c->rfd, &b, 1);
    c->got = (c->got == 1 && b == 'X') ? 1 : 0;
}

/* ───────── T2: 写向——立即就绪 + 灌满 EAGAIN 后等排空 ───────── */
typedef struct {
    int      wfd;
    atomic_int filled, done;
    uint64_t dt_ready_ns, dt_full_ns;
    int      woke_and_wrote;
} t2_ctx;
static void t2_body(void *p)
{
    t2_ctx *c = (t2_ctx*)p;
    static char buf[4096];
    uint64_t t0;
    long total = 0;

    t0 = now_ns();
    ctron_rt_wait_fd(c->wfd, 1, 2000);           /* 空闲 socketpair 写端:注册即就绪 */
    c->dt_ready_ns = now_ns() - t0;
    CHECK(c->dt_ready_ns < 1000000000ull, "T2: 空闲写端等可写 >1s(水平触发注册即就绪坏)");

    CHECK(fcntl(c->wfd, F_SETFL, O_NONBLOCK) == 0, "T2: SETFL NONBLOCK");
    for (;;) {                                   /* 灌满直至 EAGAIN */
        ssize_t k = write(c->wfd, buf, sizeof buf);
        if (k < 0) { CHECK(errno == EAGAIN || errno == EWOULDBLOCK, "T2: 灌满写错"); break; }
        total += (long)k;
        if (total > (1 << 20)) break;            /* 防御:1MB 上限 */
    }
    atomic_store_explicit(&c->filled, 1, memory_order_release);

    t0 = now_ns();
    ctron_rt_wait_fd(c->wfd, 1, 5000);           /* 等 B 排走字节 → 可写唤醒 */
    c->dt_full_ns = now_ns() - t0;
    c->woke_and_wrote = (write(c->wfd, "Z", 1) == 1);
    atomic_store_explicit(&c->done, 1, memory_order_release);
}

/* ───────── T3: 静默 fd 50ms 超时 ───────── */
typedef struct { int fd; uint64_t dt_ns; } t3_ctx;
static void t3_body(void *p)
{
    t3_ctx *c = (t3_ctx*)p;
    uint64_t t0 = now_ns();
    ctron_rt_wait_fd(c->fd, 0, 50);
    c->dt_ns = now_ns() - t0;
}

/* ───────── T4a: 对端 close → EOF 就绪提前醒 ───────── */
typedef struct { int rfd; atomic_int entered; uint64_t dt_ns; int eof; } t4a_ctx;
static void t4a_body(void *p)
{
    t4a_ctx *c = (t4a_ctx*)p;
    char b = 0;
    atomic_store_explicit(&c->entered, 1, memory_order_release);
    ctron_rt_wait_fd(c->rfd, 0, 5000);
    c->dt_ns = now_ns() - c->dt_ns;
    c->eof = (read(c->rfd, &b, 1) == 0);         /* EOF:read 返回 0 */
}

/* ───────── 健康交换协程:close 路径后证 reactor 仍存活 ───────── */
typedef struct { int fd; int ok; } hx_ctx;
static void hx_body(void *p)
{
    hx_ctx *c = (hx_ctx*)p;
    unsigned char v = 0;
    ctron_rt_wait_fd(c->fd, 0, 2000);            /* 走 reactor:登记→park→就绪交付 */
    c->ok = (read(c->fd, &v, 1) == 1 && v == 'H');
}

/* ───────── T4b: 自 close(等的就是它)→ 超时醒,无崩溃 ───────── */
typedef struct { int fd; atomic_int entered; uint64_t dt_ns; } t4b_ctx;
static void t4b_body(void *p)
{
    t4b_ctx *c = (t4b_ctx*)p;
    atomic_store_explicit(&c->entered, 1, memory_order_release);
    ctron_rt_wait_fd(c->fd, 0, 250);
    c->dt_ns = now_ns() - c->dt_ns;
}

/* ───────── T5: 8 协程 × 16 轮 socketpair 往返(reactor 扇入压力) ───────── */
#define STRESS_N   8
#define STRESS_RND 16
typedef struct {
    int      fd;                                 /* 协程侧端点(读+写同一 fd) */
    int      id;
    atomic_int ok;
} t5_ctx;
static void t5_body(void *p)
{
    t5_ctx *c = (t5_ctx*)p;
    int r;
    for (r = 0; r < STRESS_RND; r++) {
        unsigned char v;
        CHECK(ctron_rt_current() == (void*)(uintptr_t)(0x100 + c->id), "T5: current() 口径");
        ctron_rt_wait_fd(c->fd, 0, 10000);       /* 深 park 等主线程发字节 */
        if (read(c->fd, &v, 1) != 1) { atomic_store(&c->ok, 0); return; }
        if (v != (unsigned char)((r << 4) | c->id)) { atomic_store(&c->ok, 0); return; }
        v = (unsigned char)(v + 1);              /* 回声 +1 */
        if (write(c->fd, &v, 1) != 1) { atomic_store(&c->ok, 0); return; }
    }
    atomic_store_explicit(&c->ok, 1, memory_order_release);
}

int main(void)
{
    pthread_t wdt;
    const char *wenv = getenv("CTRON_RT_WORKERS");
    int workers = wenv ? atoi(wenv) : 0;

    pthread_create(&wdt, NULL, watchdog, NULL);
    setvbuf(stdout, NULL, _IONBF, 0);            /* 无缓冲:挂死时可见已过测试点 */

    ctron_rt_init(workers);
    CHECK(ctron_rt_active() == 1, "CTRON_RT=coro 未生效或 init 失败(run.sh 负责设 env)");
    CHECK(ctron_rt_current() == NULL, "裸线程 current() 必须为 NULL");

    /* T0: 裸线程回退路径 */
    t0_bare();

    /* T1: 深 park 等可读 → 就绪唤醒 */
    {
        int sp[2];
        t1_ctx c;
        CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0, "T1: socketpair");
        memset(&c, 0, sizeof c);
        c.rfd = sp[0];
        c.dt_ns = now_ns();
        ctron_rt_run(t1_body, &c, (void*)(uintptr_t)0xF1);
        while (!atomic_load_explicit(&c.entered, memory_order_acquire))
            sched_yield();
        sleep_ms(50);                            /* 确保 B 已深 park */
        uint64_t t0 = now_ns();
        CHECK(write(sp[1], "X", 1) == 1, "T1: 写 1 字节");
        ctron_rt_join_key((void*)(uintptr_t)0xF1);
        uint64_t dt_main = now_ns() - t0;
        CHECK(c.got == 1, "T1: B 醒后未读到 'X'");
        double el = ms_of(c.dt_ns);
        CHECK(el >= 45.0 && el < 3000.0,
              "T1: 唤醒时序错(过早=stub式立返/poll-race;过晚=就绪唤醒失效只剩超时)");
        CHECK(dt_main < 3000000000ull, "T1: 主线程等 join 超过 3s");
        close(sp[0]); close(sp[1]);
        printf("PASS T1 wake-on-readiness: B parked→woke at %.1fms (timeout=5000ms), got 'X'\n", el);
    }

    /* T2: 写向(注册即就绪 + 灌满后排空唤醒) */
    {
        int sp[2];
        t2_ctx c;
        CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0, "T2: socketpair");
        memset(&c, 0, sizeof c);
        c.wfd = sp[0];
        ctron_rt_run(t2_body, &c, (void*)(uintptr_t)0xF2);
        while (!atomic_load_explicit(&c.filled, memory_order_acquire))
            sched_yield();
        sleep_ms(30);                            /* 确保 A 已 park 在可写等待 */
        /* 排走 ≥4096 字节:macOS AF_UNIX 的 EVFILT_WRITE 有低水位阈——仅排少量
         * 字节时 write() 已可用但 kqueue 不报告(实测 64B 不触发、4096B 触发)。
         * 该阈值语义正是"wait_fd 返回后必须重试 syscall"冻结契约的佐证。 */
        {
            static char drain[4096];
            ssize_t got = 0, k;
            while (got < 4096 && (k = read(sp[1], drain, sizeof drain)) > 0) got += k;
            CHECK(got >= 4096, "T2: 主线程排走字节不足 4096");
        }
        ctron_rt_join_key((void*)(uintptr_t)0xF2);
        CHECK(atomic_load(&c.done) == 1, "T2: A 未完成");
        CHECK(c.woke_and_wrote == 1, "T2: A 醒后补写 'Z' 失败");
        double el = ms_of(c.dt_full_ns);
        CHECK(el < 3000.0, "T2: 可写唤醒失效(A 只能靠 5000ms 超时退出)");
        {   /* B 端排干(非阻塞直至 EAGAIN),必须最后读到 'Z'
               (阻塞式 read 在取走 'Z' 后会永远等 EOF——socketpair 不给) */
            char last = 0, ch;
            CHECK(fcntl(sp[1], F_SETFL, O_NONBLOCK) == 0, "T2: 主线程 SETFL");
            while (read(sp[1], &ch, 1) == 1) last = ch;
            CHECK(errno == EAGAIN || errno == EWOULDBLOCK, "T2: 排干未至 EAGAIN");
            CHECK(last == 'Z', "T2: 对端未收到 A 醒后写的 'Z'");
        }
        close(sp[0]); close(sp[1]);
        printf("PASS T2 write-side: ready-immediate=%.1fms, after-EAGAIN-woke=%.1fms (timeout=5000ms)\n",
               ms_of(c.dt_ready_ns), el);
    }

    /* T3: 超时路径 */
    {
        int sp[2];
        t3_ctx c;
        CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0, "T3: socketpair");
        c.fd = sp[0];
        ctron_rt_run(t3_body, &c, (void*)(uintptr_t)0xF3);
        ctron_rt_join_key((void*)(uintptr_t)0xF3);
        double el = ms_of(c.dt_ns);
        CHECK(el >= 45.0 && el <= 400.0, "T3: 50ms 超时未按时醒(过早/过晚/悬挂)");
        close(sp[0]); close(sp[1]);
        printf("PASS T3 timeout: quiet fd woke at %.1fms (timeout=50ms)\n", el);
    }

    /* T4a: 对端 close → EOF 提前醒 */
    {
        int sp[2];
        t4a_ctx c;
        CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0, "T4a: socketpair");
        memset(&c, 0, sizeof c);
        c.rfd = sp[0];
        c.dt_ns = now_ns();
        ctron_rt_run(t4a_body, &c, (void*)(uintptr_t)0xF4);
        while (!atomic_load_explicit(&c.entered, memory_order_acquire))
            sched_yield();
        sleep_ms(30);
        close(sp[1]);                            /* 关对端(写端)→ 读端 EOF */
        ctron_rt_join_key((void*)(uintptr_t)0xF4);
        double el = ms_of(c.dt_ns);
        CHECK(c.eof == 1, "T4a: B 醒后 read 未得 EOF(0)");
        CHECK(el >= 25.0 && el < 3000.0, "T4a: EOF 唤醒时序错(过晚=EOF 未交付只剩超时)");
        close(sp[0]);
        printf("PASS T4a peer-close: woke at %.1fms with EOF (timeout=5000ms)\n", el);
    }

    /* T4b: 自 close → 超时醒,无崩溃;随后健康交换证明 reactor 无恙 */
    {
        int sp[2];
        t4b_ctx c;
        CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0, "T4b: socketpair");
        memset(&c, 0, sizeof c);
        c.fd = sp[0];
        c.dt_ns = now_ns();
        ctron_rt_run(t4b_body, &c, (void*)(uintptr_t)0xF5);
        while (!atomic_load_explicit(&c.entered, memory_order_acquire))
            sched_yield();
        sleep_ms(20);
        close(sp[0]);                            /* 关"等的就是它"的 fd:内核静默摘注册 */
        ctron_rt_join_key((void*)(uintptr_t)0xF5);
        double el = ms_of(c.dt_ns);
        CHECK(el >= 200.0 && el <= 2000.0, "T4b: 自 close 后 250ms 超时未按时醒/未醒");
        /* 健康交换(经 reactor):登记表/后端未被 close 路径污染 */
        {
            hx_ctx h;
            int sp2[2];
            CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sp2) == 0, "T4b: 健康交换 socketpair");
            memset(&h, 0, sizeof h);
            h.fd = sp2[0];
            ctron_rt_run(hx_body, &h, (void*)(uintptr_t)0xF6);
            sleep_ms(20);                        /* 让协程先进 reactor 登记+park */
            CHECK(write(sp2[1], "H", 1) == 1, "T4b: 健康交换写");
            ctron_rt_join_key((void*)(uintptr_t)0xF6);
            CHECK(h.ok == 1, "T4b: close 路径后 reactor 健康交换失败");
            close(sp2[0]); close(sp2[1]);
        }
        printf("PASS T4b self-close: waiter woke via timeout at %.1fms, no crash, reactor healthy\n", el);
    }

    /* T5: 8 协程 × 16 轮 socketpair 往返(全并发深 park + reactor 扇入) */
    {
        int sp[STRESS_N][2];
        t5_ctx ctx[STRESS_N];
        int i, r;
        for (i = 0; i < STRESS_N; i++) {
            CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sp[i]) == 0, "T5: socketpair");
            memset(&ctx[i], 0, sizeof ctx[i]);
            ctx[i].fd = sp[i][0];
            ctx[i].id = i;
            ctron_rt_run(t5_body, &ctx[i], (void*)(uintptr_t)(0x100 + i));
        }
        for (r = 0; r < STRESS_RND; r++) {
            for (i = 0; i < STRESS_N; i++) {     /* 先全部发出 → 8 深 park 同时被交付 */
                unsigned char v = (unsigned char)((r << 4) | i);
                CHECK(write(sp[i][1], &v, 1) == 1, "T5: 主线程写");
            }
            for (i = 0; i < STRESS_N; i++) {     /* 逐个收回声(阻塞读:对端必回) */
                unsigned char v;
                CHECK(read(sp[i][1], &v, 1) == 1, "T5: 主线程读回声");
                CHECK(v == (unsigned char)(((r << 4) | i) + 1), "T5: 回声值错");
            }
        }
        for (i = 0; i < STRESS_N; i++) {
            ctron_rt_join_key((void*)(uintptr_t)(0x100 + i));
            CHECK(atomic_load(&ctx[i].ok) == 1, "T5: 协程未全对");
            close(sp[i][0]); close(sp[i][1]);
        }
        printf("PASS T5 stress: %d coros x %d socketpair exchanges, all correct\n",
               STRESS_N, STRESS_RND);
    }

    atomic_store(&g_alive, 0);
    printf("SMOKE OK: rt_reactor_smoke 全绿 (workers=%d)\n", workers);
    return 0;
}
