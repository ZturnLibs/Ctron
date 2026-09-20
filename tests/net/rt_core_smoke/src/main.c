/* rt_core_smoke/src/main.c —— P2-A 冒烟(纯 C main + pthread,不经 Ctron 编译器)。
 *
 * 覆盖(任务书口径):
 *   1. wake-before-park:先 wake 后 park,pending 粘滞语义 → park 不阻塞;
 *   2. 两协程 ping-pong 各 1000 次,回合计数严格交替断言(双 resume / 乱序必炸);
 *   3. join_key:协程模式(park 等待)与裸线程模式(自旋)双路径;
 *   4. sleep_ms(20) 实测 ≥15ms;
 *   5. yield_bench(100000) 打印总 ns 与 ns/yield(≤200ns 为 P2-G 门禁预演)。
 *
 * 全程 60s 看门狗:任何 lost-wakeup 类挂死 → 退出码 97,CI 不悬挂。
 */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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

/* ───────── T1: wake-before-park(三窗口竞速:prior→RUNNING / DESCHED / PARKED) ─────────
 * 目标协程上报 entered 后短暂自旋再 park,主线程此刻送 wake:
 *   - wake 落在自旋期(RUNNING)→ pending 粘滞,park 入口消费,不阻塞;
 *   - 落在 DESCHED 窗口 → completer 消费回队;
 *   - 落在 PARKED → 直接入队。
 * 任一窗口 park 都必须返回(否则看门狗 97)。 */
typedef struct {
    atomic_int entered, woken, after_park;
} t1_ctx;
static void t1_body(void *p)
{
    t1_ctx *c = (t1_ctx*)p;
    atomic_store(&c->entered, 1);
    for (volatile int i = 0; i < 4000 && !atomic_load_explicit(&c->woken, memory_order_acquire); i++)
        ;                                        /* 给 wake 落在 RUNNING 窗口的机会 */
    ctron_rt_park();
    atomic_store(&c->after_park, 1);
}

/* ───────── T2: ping-pong ×1000,严格交替 ─────────
 * 协议:B 先宣告就绪并 park(gate);A 等 gate 后首发 wake。此后每轮
 * wake 必落在对端 PARKED(或 DESCHED/RUNNING-with-pending)窗口,
 * 交替由 park/wake 中继结构性保证;回合计数奇偶断言兜底。 */
#define PP_ROUNDS 1000
static long pp_turn = -1;                        /* -1 未开局;此后 0,1,2,… 每回合 +1 */
static atomic_int pp_b_ready;
typedef struct { void *self_key, *other_key; int id; } pp_arg_t;

static void pp_body(void *p)
{
    pp_arg_t *a = (pp_arg_t*)p;
    if (a->id == 1) {
        atomic_store_explicit(&pp_b_ready, 1, memory_order_release);
        ctron_rt_park();                         /* bootstrap gate:等 A 首发 wake */
    }
    if (a->id == 0) {
        while (!atomic_load_explicit(&pp_b_ready, memory_order_acquire))
            ctron_rt_sleep_ms(0);                /* 让出 worker,等 B 进 gate */
    }
    for (int i = 0; i < PP_ROUNDS; i++) {
        long t;
        CHECK(ctron_rt_current() == a->self_key, "pp: current() 必须是本协程 key");
        t = ++pp_turn;
        CHECK(t % 2 == (long)a->id, "pp: 回合奇偶错位(双 resume 或乱序)");
        ctron_rt_wake(a->other_key);
        ctron_rt_park();                         /* 等对端回敬;最后一轮由收尾 wake 唤醒 */
    }
    ctron_rt_wake(a->other_key);                 /* 收尾补一发:多余 wake 被 READY/DONE 吸收 */
}

/* ───────── T3: 协程内 join_key(park 路径)+ yield_bench + sleep ───────── */
static void child_body(void *p)
{
    int *x = (int*)p;
    ctron_rt_sleep_ms(5);
    *x = 42;
}

typedef struct {
    void  *self_key, *child_key;
    int    x, x_after_join;
    void  *cur_in_joiner;
    int64_t bench_ns;
    uint64_t sleep_dt_ns;
    int    sleep_looked_ok;
} t3_ctx;

static void t3_body(void *p)
{
    t3_ctx *j = (t3_ctx*)p;
    uint64_t t0;

    /* sleep_ms(20) 下界 */
    t0 = now_ns();
    ctron_rt_sleep_ms(20);
    j->sleep_dt_ns = now_ns() - t0;
    j->sleep_looked_ok = (j->sleep_dt_ns >= 15000000ull);

    /* 协程模式 join:挂起当前协程等子协程 */
    CHECK(ctron_rt_current() == j->self_key, "t3: current() 口径");
    j->child_key = ctron_rt_run(child_body, &j->x, (void*)(uintptr_t)0xC1);
    CHECK(j->child_key == (void*)(uintptr_t)0xC1, "t3: run 返回 key");
    ctron_rt_join_key(j->child_key);             /* park 等 done */
    j->cur_in_joiner  = ctron_rt_current();
    j->x_after_join   = j->x;

    /* 微基准:100000 次 yield 配对 */
    j->bench_ns = ctron_rt_yield_bench(100000);
}

int main(void)
{
    pthread_t wdt;
    pthread_create(&wdt, NULL, watchdog, NULL);

    ctron_rt_init(0);                            /* workers<=0 → min(cpu,4) */
    CHECK(ctron_rt_active() == 1, "CTRON_RT=coro 未生效或 init 失败(run.sh 负责设 env)");
    CHECK(ctron_rt_current() == NULL, "裸线程 current() 必须为 NULL");
    ctron_rt_init(0);                            /* 幂等 */
    CHECK(ctron_rt_active() == 1, "init 非幂等");

    /* T1: wake 与 park 竞速(含 wake-before-park) */
    {
        t1_ctx c;
        atomic_init(&c.entered, 0);
        atomic_init(&c.woken, 0);
        atomic_init(&c.after_park, 0);
        void *k = ctron_rt_run(t1_body, &c, (void*)(uintptr_t)0xB1);
        while (!atomic_load_explicit(&c.entered, memory_order_acquire))
            sched_yield();
        atomic_store_explicit(&c.woken, 1, memory_order_release);
        ctron_rt_wake(k);
        ctron_rt_join_key(k);                    /* 裸线程 join 路径(自旋) */
        CHECK(atomic_load(&c.after_park) == 1, "T1: wake/park 竞速丢失唤醒或 park 阻塞");
        printf("PASS T1 wake-before-park(entered/woken/after_park=%d/%d/%d)\n",
               atomic_load(&c.entered), atomic_load(&c.woken),
               atomic_load(&c.after_park));
    }

    /* T2: ping-pong 各 1000,严格交替 */
    {
        pp_arg_t a = { (void*)(uintptr_t)0xA0, (void*)(uintptr_t)0xA1, 0 };
        pp_arg_t b = { (void*)(uintptr_t)0xA1, (void*)(uintptr_t)0xA0, 1 };
        atomic_init(&pp_b_ready, 0);
        pp_turn = -1;
        ctron_rt_run(pp_body, &a, a.self_key);   /* A 先入队(leader) */
        ctron_rt_run(pp_body, &b, b.self_key);
        ctron_rt_join_key(a.self_key);           /* 裸线程 join */
        ctron_rt_join_key(b.self_key);
        CHECK(pp_turn == 2 * PP_ROUNDS - 1, "T2: 回合总数不符");
        printf("PASS T2 ping-pong %d×%d 严格交替 (turn=%ld)\n",
               PP_ROUNDS, 2, pp_turn);
    }

    /* T3: sleep 下界 + 协程 join + yield_bench */
    {
        t3_ctx j;
        memset(&j, 0, sizeof j);
        j.self_key = (void*)(uintptr_t)0xC0;
        ctron_rt_run(t3_body, &j, j.self_key);
        ctron_rt_join_key(j.self_key);           /* 裸线程 join 等 bootstrap */
        CHECK(j.sleep_looked_ok, "T3: sleep_ms(20) 实测 <15ms");
        CHECK(j.x == 42 && j.x_after_join == 42, "T3: join_key 未等到子协程写值");
        CHECK(j.cur_in_joiner == j.self_key, "T3: join 后 current() 口径错");
        CHECK(j.bench_ns > 0, "T3: yield_bench 未执行");
        printf("PASS T3 coro join_key + sleep_ms(20)=%llu.%03llums (>=15ms)\n",
               (unsigned long long)(j.sleep_dt_ns / 1000000ull),
               (unsigned long long)(j.sleep_dt_ns % 1000000ull));
        printf("BENCH yield_bench(100000): total=%lldns, ns/yield=%lld.%03lld (pair/2 口径, P2-G 预演门限 200ns)\n",
               (long long)j.bench_ns,
               (long long)(j.bench_ns / (2 * 100000)),
               (long long)((j.bench_ns % (2 * 100000)) * 1000 / (2 * 100000)));
    }

    /* 裸线程 sleep_ms 回退路径 */
    {
        uint64_t t0 = now_ns();
        ctron_rt_sleep_ms(2);
        uint64_t dt = now_ns() - t0;
        CHECK(dt >= 1500000ull, "T4: 裸线程 sleep_ms(2) <1.5ms");
        printf("PASS T4 bare-thread sleep_ms(2)=%llu.%03llums\n",
               (unsigned long long)(dt / 1000000ull),
               (unsigned long long)(dt % 1000000ull));
    }

    atomic_store(&g_alive, 0);
    printf("SMOKE OK: rt_core_smoke 全绿\n");
    return 0;
}
