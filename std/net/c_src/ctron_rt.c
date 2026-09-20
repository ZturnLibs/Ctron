/* ctron_rt.c —— Ctron P2 协程运行时实现(自绘上下文切换的 N:M 调度)。
 *
 * ══ 上下文切换(为什么不用 ucontext)══
 *   darwin/arm64 的 getcontext/makecontext/swapcontext 自 macOS 10.6 起弃用且
 *   标注 "No longer supported";实测(本任务冒烟阶段)在"互斥保护的就绪队列 +
 *   协程跨 worker 线程迁移"的标准 N:M 用法下确定性崩溃(SIGBUS/SIGSEGV/
 *   协程记录被堆写穿),最小复现仅需 2 worker × 2 协程 × 互斥队列(详见任务
 *   报告)。故本实现自绘切换:仅保存/恢复 SysV ABI 被调用方保存寄存器 +
 *   sp + 返回地址,无 syscall,无信号掩码处理(运行时与冒烟不依赖信号;
 *   用户信号处理与协程混用是 P2 已登记限制)。arm64/x86_64 同一段汇编覆盖
 *   darwin 与 Linux(SysV ABI 一致);其他架构接入点见 rt_ctx/rt_ctx_init。
 *
 * ══ lost-wakeup / double-resume 分析(实现按此不变量构建)══
 *
 * 协程生命周期状态(RT_ST_*):READY(在就绪队)→ RUNNING(某 worker 上执行)
 * → DESCHED(park/yield/forever 离场收尾中)→ PARKED(停车,等 wake/定时器)
 * / READY(yield 回队)/ DONE(结束,永不 resume)。
 *
 * 不变量 I(栈确定性离场后才可被 resume):
 *   READY / PARKED 只在两处于全局锁 G 内写入 ——
 *     (a) 创建路径(run():初始 ctx 已备好新栈,quiesced 平凡成立);
 *     (b) completer(worker 循环在切回后、同一 OS 线程上执行,且与后续 pop
 *         合并在同一次 G 持有内)。
 *   resume 只能经就绪队出队 ⇒ 看到可运行状态时,目标协程栈已确定性离场,
 *   ctx 自包含,跨线程恢复安全,且同一时刻至多一个 runner(出队在 G 内将
 *   READY→RUNNING,恰好一个 popper 赢)。⇒ double-resume / 栈复用冲撞不可能。
 *
 * 不变量 II(wake-before-park 不丢,park 三窗口全覆盖):
 *   sticky 旗标 wake_pending。wake/timer/join/notify 统一走 coro_wake_locked:
 *     - 目标 PARKED     → 转 READY 入队(由不变量 I,此刻栈必已离场);
 *     - 目标 READY      → 吸收(协程必将运行,本轮唤醒已交付);
 *     - 目标 DONE       → no-op;
 *     - RUNNING/DESCHED → wake_pending=1。
 *   park 入口在 G 内先查 pending(停车前唤醒 → 消费,不阻塞);DESCHED 窗口
 *   (已过入口检查、尚未被 completer 定稿)内到达的唤醒置 pending,由
 *   completer 定稿时消费(转 READY 入队)。三窗口:入口前 / DESCHED 中 /
 *   PARKED 后,分别被入口检查 / completer / 直接入队覆盖,均在 G 内,
 *   无一丢失。
 *
 * 不变量 III(done 旗标):notify_done 在 G 内置 done(release)并唤醒全部
 *   join 等待者;join 在 G 内查 done 并登记等待者 ⇒ 登记-完成竞争无窗口。
 *   裸线程 join 对 _Atomic done 自旋/退避。join 等待者以 on_join 标记在列,
 *   提前醒(粘滞 pending/取消广播)后仍在列则不重复登记。
 *
 * 定时器与 wake 合流:timer 到期走 coro_wake_locked 同一路径;armed/arm_seq
 *   世代号使过期定时器(manual wake 已先行 / 协程已重新武装)被静默丢弃,
 *   不产生串扰 pending。
 *
 * 锁纪律:G 绝不跨切换持有(park/yield 在切换前解锁;worker 循环在切换前
 *   解锁),故无"锁随上下文迁移"的跨线程 unlock UB。worker 循环每轮迭代
 *   重新加锁 —— finalize/timers/pop 与 popper 认领全部在锁内串行。
 * 空闲策略:轮询 + 全局递增退避(20µs→160µs),不用 condvar 阻塞等待 ——
 *   N ≤ min(cpu,4) 的小池开销可忽略,且唤醒延迟有界。
 * 栈:64KB mmap + 低地址 1 页 PROT_NONE guard(页粒度取 sysconf,darwin/arm64
 *   页为 16KB,写死 4KB 会被 mprotect 取整放大、殃及栈本体);0xA5 填充用于
 *   DONE 时登记高水位(仅登记,不强制);空闲池(上限 256)复用。
 */
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1 /* MAP_ANON/_SC_NPROCESSORS_ONLN 等 BSD 面 */
#endif
#include "ctron_rt.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

/* ───────────────────────── 常量 ───────────────────────── */
#define RT_STACK_SIZE   (64u * 1024u)          /* 每协程栈(冻结口径) */
#define RT_POOL_CAP     256                    /* 栈空闲池上限 */
#define RT_MAX_WORKERS  64
#define RT_MAP_BUCKETS  1024                   /* key→record 哈希(2 的幂) */
#define RT_BACKOFF_MAX_SHIFT 3                 /* 空闲退避上限:20µs << 3 = 160µs */

/* guard 页大小取真页粒度(darwin/arm64 页为 16KB)。 */
static long rt_pagesize(void)
{
    static long ps;                              /* 单调初始化,良性竞争 */
    if (!ps) {
        ps = sysconf(_SC_PAGESIZE);
        if (ps <= 0) ps = 4096;
    }
    return ps;
}

/* ═══════════════ 上下文切换(自绘;见文件头注) ═══════════════ */
#if defined(__aarch64__) || defined(__x86_64__)

typedef struct rt_ctx {
#if defined(__aarch64__)
    unsigned long x19, x20, x21, x22, x23, x24, x25, x26, x27, x28;
    unsigned long fp, lr, sp;
    /* AAPCS64:V8–V15 的低 64 位为 callee-saved(高 64 位 caller-saved),
     * 故仅存 d8–d15(+64B);不吃这笔账会在切回后污染 FP 活跃值。 */
    unsigned long d8, d9, d10, d11, d12, d13, d14, d15;
#elif defined(__x86_64__)
    unsigned long sp;   /* 伪帧(升序):[r15][r14][r13][r12][rbx][rbp][pc][pad]
                           8 槽:6 次弹出 + ret 弹 pc 后 rsp=top-8 ≡ 8 (mod 16),
                           符合 SysV 调用入口(见 rt_ctx_init) */
#endif
} rt_ctx;

#if defined(__APPLE__)
#define RT_SWAP_SYM "_rt_swap_ctx"   /* darwin C 符号带下划线前缀 */
#else
#define RT_SWAP_SYM "rt_swap_ctx"
#endif
void rt_swap(rt_ctx *from, rt_ctx *to) __asm__(RT_SWAP_SYM);

#if defined(__aarch64__)
__asm__(
    ".text\n"
    ".globl " RT_SWAP_SYM "\n"
    RT_SWAP_SYM ":\n"
    "    stp x19, x20, [x0]\n"
    "    stp x21, x22, [x0, #16]\n"
    "    stp x23, x24, [x0, #32]\n"
    "    stp x25, x26, [x0, #48]\n"
    "    stp x27, x28, [x0, #64]\n"
    "    stp x29, x30, [x0, #80]\n"
    "    mov x9, sp\n"
    "    str x9, [x0, #96]\n"
    "    stp d8, d9, [x0, #104]\n"
    "    stp d10, d11, [x0, #120]\n"
    "    stp d12, d13, [x0, #136]\n"
    "    stp d14, d15, [x0, #152]\n"
    "    ldp x19, x20, [x1]\n"
    "    ldp x21, x22, [x1, #16]\n"
    "    ldp x23, x24, [x1, #32]\n"
    "    ldp x25, x26, [x1, #48]\n"
    "    ldp x27, x28, [x1, #64]\n"
    "    ldp x29, x30, [x1, #80]\n"
    "    ldr x9, [x1, #96]\n"
    "    ldp d8, d9, [x1, #104]\n"
    "    ldp d10, d11, [x1, #120]\n"
    "    ldp d12, d13, [x1, #136]\n"
    "    ldp d14, d15, [x1, #152]\n"
    "    mov sp, x9\n"
    "    ret\n"
);
/* 初始 ctx:进入 tramp(sp 16 对齐;lr=tramp;x19-x28/d8-d15 清零)。 */
static void rt_ctx_init(rt_ctx *c, void *stack_low, size_t stack_size, void (*tramp)(void))
{
    (void)stack_size;
    memset(c, 0, sizeof *c);
    c->sp = ((unsigned long)stack_low + stack_size) & ~(unsigned long)15;
    c->lr = (unsigned long)tramp;
}

#elif defined(__x86_64__)
__asm__(
    ".text\n"
    ".globl " RT_SWAP_SYM "\n"
    RT_SWAP_SYM ":\n"
    "    push %rbp\n"
    "    push %rbx\n"
    "    push %r12\n"
    "    push %r13\n"
    "    push %r14\n"
    "    push %r15\n"
    "    mov %rsp, (%rdi)\n"
    "    mov (%rsi), %rsp\n"
    "    pop %r15\n"
    "    pop %r14\n"
    "    pop %r13\n"
    "    pop %r12\n"
    "    pop %rbx\n"
    "    pop %rbp\n"
    "    ret\n"
);
/* 初始 ctx:8 槽伪帧(升序)[r15][r14][r13][r12][rbx][rbp][pc=tramp][pad]。
 * 6 次弹出后 rsp=f+48,ret 从 f+48 弹 pc → 入口 rsp=f+56=top-8 ≡ 8 (mod 16),
 * 符合 SysV 调用入口(函数入口 rsp 恰为调用方 push 返回地址后的形态)。
 * (评审必修 1:旧 7 槽布局入口 rsp ≡ 0,违反 SysV。) */
static void rt_ctx_init(rt_ctx *c, void *stack_low, size_t stack_size, void (*tramp)(void))
{
    memset(c, 0, sizeof *c);
    unsigned long top = ((unsigned long)stack_low + stack_size) & ~(unsigned long)15;
    unsigned long *f = (unsigned long *)(top - 8 * sizeof(unsigned long));
    f[0] = 0; f[1] = 0; f[2] = 0; f[3] = 0; f[4] = 0; f[5] = 0;
    f[6] = (unsigned long)tramp;
    f[7] = 0;                                        /* 对齐 pad */
    c->sp = (unsigned long)f;
}
#endif /* __aarch64__/__x86_64__ 切换实现 */
#endif /* 外层平台门槛 */

/* ───────────────────────── 数据结构 ───────────────────────── */
enum {
    RT_ST_READY = 1,   /* 在就绪队(栈已离场或新建:不变量 I) */
    RT_ST_RUNNING,     /* 某 worker 正在执行(不在队) */
    RT_ST_DESCHED,     /* park/yield/forever 已过入口检查,切换收尾中 */
    RT_ST_PARKED,      /* 停车,等 wake/定时器(completer 定稿) */
    RT_ST_DONE         /* 结束;任何唤醒路径对其 no-op */
};

enum { DESCHED_PARK = 1, DESCHED_YIELD = 2, DESCHED_FOREVER = 3 };

typedef struct rt_coro {
    void          *key;          /* 调用方私有 opaque key,rt 只作等值比较 */
    void         (*fn)(void*);
    void          *arg;

    rt_ctx         ctx;          /* 自包含上下文;跨线程恢复安全 */
    unsigned char *stack;        /* mmap 基址(含 guard 页) */
    size_t         stack_map_sz;

    int            state;        /* G 内读写 */
    int            wake_pending; /* sticky 唤醒(G 内读写) */
    int            desched_kind; /* DESCHED_*;仅离场协程自己写、completer 读 */
    int            on_join;      /* 已登记在某目标 join 链上(G 内) */

    _Atomic int    done;         /* notify_done 旗标(裸线程 join 自旋对象) */
    int            armed;        /* 定时器武装中(G 内) */
    uint64_t       arm_seq;      /* 武装世代号:过期定时器静默丢弃 */

    struct rt_coro *qnext;       /* 就绪队链 */
    struct rt_coro *jnext;       /* join 等待者链(挂在目标 record 上) */
    struct rt_coro *allnext;     /* 全体协程链(cancel 广播遍历) */
    struct rt_coro *mapnext;     /* key 哈希桶链(record 为墓碑,不回收) */
} rt_coro;

typedef struct rt_worker {
    pthread_t  th;
    rt_ctx     sched;            /* 切回 worker 自身 pthread 栈的上下文 */
} rt_worker;

typedef struct {                 /* 定时器最小堆条目 */
    uint64_t   deadline;         /* CLOCK_MONOTONIC ns */
    rt_coro   *c;
    uint64_t   seq;              /* arm_seq 世代 */
} rt_timer;

/* ───────────────────────── 全局状态 ───────────────────────── */
static pthread_mutex_t rt_g = PTHREAD_MUTEX_INITIALIZER;

static rt_worker g_workers[RT_MAX_WORKERS];
static int  g_nworkers = 0;
static int  g_inited   = 0;
static int  g_env_cached = 0, g_env_is_coro = 0;

static rt_coro *g_ready_head = NULL, *g_ready_tail = NULL;

static rt_timer *g_timers = NULL;
static int       g_ntimers = 0, g_timercap = 0;

static rt_coro *g_map[RT_MAP_BUCKETS];      /* 墓碑式:record 常驻(done 可查) */
static rt_coro *g_all = NULL;

static void *g_pool[RT_POOL_CAP];           /* 已 retire 的栈(含 guard 布局) */
static int   g_npool = 0;

static _Atomic long g_hwm_stack = 0;        /* 高水位登记(仅登记,不强制) */
static _Atomic unsigned g_backoff;          /* 空闲退避档位(良性竞争) */

static _Thread_local rt_coro  *tls_cur    = NULL;
static _Thread_local rt_worker *tls_worker = NULL;

/* ══ TLS 槽位缓存危害(本实现的关键约束)══
 * darwin/arm64 clang 把 _Thread_local 的槽位地址(TLV 解析结果)缓存在
 * callee-saved 寄存器里;协程跨 worker 迁移后,rt_swap 恢复的正是 **旧线程**
 * 的寄存器镜像 ⇒ 读到别的线程的 TLS 单元 ⇒ coro_suspend 可能拿到错误
 * worker 的 sched 上下文(幽灵 resume,实证 SIGBUS/堆写穿)。
 * 对策:TLS 一律经 noinline 访问器读取 —— 每次调用在当前线程重新解析槽位,
 * 编译器无法跨 rt_swap 缓存。 */
__attribute__((noinline)) static rt_coro   *rt_tls_cur(void)    { return tls_cur; }
__attribute__((noinline)) static void      rt_tls_set_cur(rt_coro *c) { tls_cur = c; }
__attribute__((noinline)) static rt_worker *rt_tls_worker(void) { return tls_worker; }
__attribute__((noinline)) static void      rt_tls_set_worker(rt_worker *w) { tls_worker = w; }

static void rt_lock(void)   { pthread_mutex_lock(&rt_g); }
static void rt_unlock(void) { pthread_mutex_unlock(&rt_g); }

/* ───────────────────────── 工具 ───────────────────────── */
static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static size_t key_bucket(void *k)
{
    uint64_t x = (uint64_t)(uintptr_t)k;
    x ^= x >> 16;
    x *= 0x9E3779B97F4A7C15ull;
    x ^= x >> 32;
    return (size_t)x & (RT_MAP_BUCKETS - 1);
}

/* ───────────────────────── 就绪队(G 内) ───────────────────────── */
static void ready_push(rt_coro *c)
{
    c->qnext = NULL;
    if (g_ready_tail) g_ready_tail->qnext = c;
    else              g_ready_head = c;
    g_ready_tail = c;
}

static rt_coro *ready_pop(void)
{
    rt_coro *c = g_ready_head;
    if (c) {
        g_ready_head = c->qnext;
        if (!g_ready_head) g_ready_tail = NULL;
        c->qnext = NULL;
    }
    return c;
}

/* ───────────────────────── key→record 哈希(G 内) ───────────────────────── */
static rt_coro *map_get_locked(void *k)
{
    rt_coro *c;
    if (!k) return NULL;
    for (c = g_map[key_bucket(k)]; c; c = c->mapnext)
        if (c->key == k) return c;      /* 桶头为最新 ⇒ 同 key 重注册时新 record 优先 */
    return NULL;
}

static void map_put_locked(rt_coro *c)
{
    size_t b = key_bucket(c->key);
    c->mapnext = g_map[b];
    g_map[b] = c;
}

/* ───────────────────────── 定时器最小堆(G 内) ───────────────────────── */
static void heap_push(uint64_t deadline, rt_coro *c, uint64_t seq)
{
    if (g_ntimers == g_timercap) {
        int ncap = g_timercap ? g_timercap * 2 : 64;
        rt_timer *nt = realloc(g_timers, (size_t)ncap * sizeof(rt_timer));
        if (!nt) abort();
        g_timers   = nt;
        g_timercap = ncap;
    }
    int i = g_ntimers++;
    g_timers[i] = (rt_timer){ deadline, c, seq };
    while (i > 0) {
        int p = (i - 1) / 2;
        if (g_timers[p].deadline <= g_timers[i].deadline) break;
        rt_timer t = g_timers[p]; g_timers[p] = g_timers[i]; g_timers[i] = t;
        i = p;
    }
}

static rt_timer heap_pop(void)
{
    rt_timer min = g_timers[0];
    g_timers[0] = g_timers[--g_ntimers];
    int i = 0;
    for (;;) {
        int l = 2 * i + 1, r = l + 1, m = i;
        if (l < g_ntimers && g_timers[l].deadline < g_timers[m].deadline) m = l;
        if (r < g_ntimers && g_timers[r].deadline < g_timers[m].deadline) m = r;
        if (m == i) break;
        rt_timer t = g_timers[m]; g_timers[m] = g_timers[i]; g_timers[i] = t;
        i = m;
    }
    return min;
}

/* 到期定时器 → 统一唤醒路径(G 内)。 */
static void timers_fire_locked(void)
{
    uint64_t now = now_ns();
    while (g_ntimers && g_timers[0].deadline <= now) {
        rt_timer e = heap_pop();
        rt_coro  *c = e.c;
        if (!c || c->state == RT_ST_DONE) continue;          /* 墓碑/已结束 */
        if (!c->armed || c->arm_seq != e.seq) continue;      /* 过期世代:静默丢 */
        c->armed = 0;
        if (c->state == RT_ST_PARKED) {
            c->state = RT_ST_READY;
            ready_push(c);
        } else {
            /* RUNNING/DESCHED:截止已过 → sticky 唤醒即将发生的/进行中的
             * sleep park(提前返回;语义正确:deadline 已到)。 */
            c->wake_pending = 1;
        }
    }
}

/* ───────────────────────── 栈分配 / 回收 ───────────────────────── */
static unsigned char *stack_alloc(size_t *map_sz)
{
    unsigned char *base;
    long ps = rt_pagesize();
    *map_sz = (size_t)RT_STACK_SIZE + (size_t)ps;
    rt_lock();
    if (g_npool > 0) {
        base = g_pool[--g_npool];
        rt_unlock();
        memset(base + ps, 0xA5, RT_STACK_SIZE);          /* 重铺填充供高水位 */
        return base;
    }
    rt_unlock();
    base = mmap(NULL, *map_sz, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) return NULL;
    (void)mprotect(base, (size_t)ps, PROT_NONE);         /* guard(可选失败不致命) */
    memset(base + ps, 0xA5, RT_STACK_SIZE);
    return base;
}

/* 仅在协程 DONE(FOREVER completer,栈已确定性离场)后调用。
 * 登记 64KB 栈高水位(自栈底远端向上找首个被改写字节),然后入池/释放。 */
static void stack_retire(rt_coro *c)
{
    unsigned char *lo = c->stack + rt_pagesize();
    long used = 0;
    /* 栈自高端(top)向低端(guard)生长:已用字节 = top 端前缀。
     * 自 top-1 向下找首个 0xA5 脏字节,used = top - 该字节(评审 Minor 4b)。 */
    for (size_t i = RT_STACK_SIZE; i > 0; i--) {
        if (lo[i - 1] != 0xA5) { used = (long)(RT_STACK_SIZE - (i - 1)); break; }
    }
    long prev = atomic_load_explicit(&g_hwm_stack, memory_order_relaxed);
    while (used > prev &&
           !atomic_compare_exchange_weak_explicit(&g_hwm_stack, &prev,
                                                  used, memory_order_relaxed,
                                                  memory_order_relaxed))
        ;
    rt_lock();
    if (g_npool < RT_POOL_CAP) {
        g_pool[g_npool++] = c->stack;
        rt_unlock();
        return;
    }
    rt_unlock();
    munmap(c->stack, c->stack_map_sz);
    c->stack = NULL;
}

/* ───────────────────────── 核心:唤醒 / 停车 ───────────────────────── */
/* G 内调用。见文件头注不变量 II:三窗口全覆盖,无丢失。 */
static void coro_wake_locked(rt_coro *c)
{
    switch (c->state) {
    case RT_ST_PARKED:
        c->armed = 0;                    /* 手动唤醒使未到期定时器作废 */
        c->state = RT_ST_READY;          /* PARKED ⇒ 已离场(不变量 I),入队安全 */
        ready_push(c);
        break;
    case RT_ST_READY:
    case RT_ST_DONE:
        /* READY:必将运行,本轮唤醒吸收;DONE:永不 resume */
        break;
    default:                             /* RUNNING / DESCHED */
        c->wake_pending = 1;             /* sticky:下次 park 入口或 completer 消费 */
        break;
    }
}

/* 协程离场原语:仅协程上下文内调用。解锁后切到本 worker 调度上下文;
 * 返回即已被某 worker resume(状态已被 popper 置 RUNNING)。 */
static void coro_suspend(int kind)
{
    rt_coro *self = rt_tls_cur();
    rt_worker *w = rt_tls_worker();
    if (!self || !w) return;                     /* 裸线程:契约外,no-op */
    rt_lock();
    if (kind != DESCHED_FOREVER && self->wake_pending) {
        self->wake_pending = 0;                  /* 停车前已被唤醒:不阻塞 */
        rt_unlock();
        return;
    }
    self->state = RT_ST_DESCHED;
    self->desched_kind = kind;
    rt_unlock();                                 /* G 绝不跨上下文切换 */
    rt_swap(&self->ctx, &w->sched);
    /* resume 返回点;state 已由 popper 置 RUNNING */
}

/* completer(worker 循环,同一 OS 线程,切换返回后,G 内调用)。
 * 按离场类别定稿状态;这是 READY/PARKED/DONE 的唯一运行期写入点。 */
static void coro_finalize_locked(rt_coro *c)
{
    switch (c->desched_kind) {
    case DESCHED_YIELD:
        c->wake_pending = 0;             /* 离场途中到达的唤醒被重新入队吸收 */
        c->state = RT_ST_READY;
        ready_push(c);
        break;
    case DESCHED_FOREVER:
        c->state = RT_ST_DONE;           /* pending 蓄意忽略:永不 resume */
        break;
    case DESCHED_PARK:
    default:
        if (c->wake_pending) {           /* DESCHED 窗口内的唤醒 → 立即回队 */
            c->wake_pending = 0;
            c->state = RT_ST_READY;
            ready_push(c);
        } else {
            c->state = RT_ST_PARKED;
        }
        break;
    }
}

/* ───────────────────────── 协程主体 ───────────────────────── */
static void rt_tramp(void)
{
    rt_coro *self = rt_tls_cur();        /* 首次换入前 popper 已设置 */
    void (*fn)(void*) = self->fn;
    void *arg = self->arg;
    void *key = self->key;

    fn(arg);
    ctron_rt_notify_done(key);           /* 冻结契约:体结束前回调 */
    for (;;)                             /* 永久停车,绝不返回到已死栈; */
        coro_suspend(DESCHED_FOREVER);   /* FOREVER 不被 wake/cancel/join 触碰 */
}

/* ───────────────────────── worker 循环 ───────────────────────── */
/* 空闲退避:调用时持 G,返回时已放 G(worker 循环头部重锁)。 */
static void idle_backoff(void)
{
    unsigned b = atomic_load_explicit(&g_backoff, memory_order_relaxed);
    long ns = 20000l << (b > RT_BACKOFF_MAX_SHIFT ? RT_BACKOFF_MAX_SHIFT : b);
    rt_unlock();
    struct timespec ts = { 0, ns };
    nanosleep(&ts, NULL);
    if (b < RT_BACKOFF_MAX_SHIFT)
        atomic_store_explicit(&g_backoff, b + 1, memory_order_relaxed);
}

static void *worker_main(void *p)
{
    rt_worker *ws = (rt_worker*)p;
    rt_coro *just_desched = NULL;
    rt_tls_set_worker(ws);
    for (;;) {
        rt_coro *retire = NULL, *c;
        rt_lock();                       /* 每轮迭代持 G:finalize/timers/pop 全在锁内 */
        if (just_desched) {              /* 与 pop 合并一次加锁(收益路径) */
            coro_finalize_locked(just_desched);
            if (just_desched->desched_kind == DESCHED_FOREVER)
                retire = just_desched;   /* 栈回收/高水位移出锁外 */
            just_desched = NULL;
        }
        timers_fire_locked();
        c = ready_pop();
        if (!c) {
            idle_backoff();              /* 放 G 退避;循环头重锁 */
            continue;
        }
        atomic_store_explicit(&g_backoff, 0, memory_order_relaxed);
        c->state = RT_ST_RUNNING;        /* 出队即认领:单 runner 保证 */
        rt_unlock();                     /* G 绝不跨上下文切换 */
        if (retire) stack_retire(retire);
        rt_tls_set_cur(c);               /* current() 口径;tramp 首入口读它 */
        rt_swap(&ws->sched, &c->ctx);
        rt_tls_set_cur(NULL);
        just_desched = c;
    }
}

/* ───────────────────────── 冻结 API ───────────────────────── */
void ctron_rt_init(int workers)
{
    int i;
    rt_lock();
    if (g_inited) { rt_unlock(); return; }           /* 幂等 */
    if (workers <= 0) {
        long n = sysconf(_SC_NPROCESSORS_ONLN);
        if (n <= 0) n = 2;
        workers = (int)(n < 4 ? n : 4);              /* min(cpu 核数, 4) */
    }
    if (workers > RT_MAX_WORKERS) workers = RT_MAX_WORKERS;
    if (workers < 1) workers = 1;
    g_nworkers = workers;
    for (i = 0; i < workers; i++) {
        if (pthread_create(&g_workers[i].th, NULL, worker_main, &g_workers[i]) != 0)
            abort();
    }
    g_inited = 1;
    rt_unlock();
    /* worker 先设 tls_worker 再抢 G 才可能 pop 协程 ⇒ sched 上下文必已就绪 */
}

int ctron_rt_active(void)
{
    rt_lock();
    if (!g_env_cached) {
        const char *e = getenv("CTRON_RT");
        g_env_is_coro = (e != NULL && strcmp(e, "coro") == 0);
        g_env_cached = 1;
    }
    int r = g_env_is_coro && g_inited;
    rt_unlock();
    return r;
}

void *ctron_rt_current(void)
{
    rt_coro *c = rt_tls_cur();
    return c ? c->key : NULL;
}

void *ctron_rt_run(void (*fn)(void*), void *arg, void *key)
{
    rt_coro *c;
    size_t msz;
    if (!fn) return NULL;
    if (!g_inited) ctron_rt_init(0);                 /* 惰性 init */
    c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->key = key; c->fn = fn; c->arg = arg;
    c->stack = stack_alloc(&msz);
    c->stack_map_sz = msz;
    if (!c->stack) { free(c); return NULL; }
    c->state = RT_ST_READY;
    c->desched_kind = 0;
    rt_ctx_init(&c->ctx, c->stack + rt_pagesize(), RT_STACK_SIZE, rt_tramp);

    rt_lock();
    c->allnext = g_all;
    g_all = c;
    map_put_locked(c);                               /* 墓碑式:done 后仍可查 */
    ready_push(c);
    rt_unlock();
    return key;
}

void ctron_rt_notify_done(void *key)
{
    rt_coro *c, *w, *wn;
    if (!key) return;
    rt_lock();
    c = map_get_locked(key);
    if (c && !atomic_load_explicit(&c->done, memory_order_relaxed)) {
        atomic_store_explicit(&c->done, 1, memory_order_release);
        /* 唤醒全部 join 等待者(与 join 的登记同在 G 内 ⇒ 无登记-完成窗口) */
        w = c->jnext;
        c->jnext = NULL;
        while (w) {
            wn = w->jnext;
            w->jnext = NULL;
            w->on_join = 0;
            coro_wake_locked(w);
            w = wn;
        }
    }
    rt_unlock();
}

void ctron_rt_join_key(void *key)
{
    rt_coro *c, *self;
    if (!key) return;
    rt_lock();
    c = map_get_locked(key);
    if (!c) { rt_unlock(); return; }                 /* 未知 key:视为无需等待 */
    if (atomic_load_explicit(&c->done, memory_order_acquire)) { rt_unlock(); return; }
    self = rt_tls_cur();
    if (!self) {                                     /* 裸线程:自旋 + 退避 */
        int spins = 0;
        rt_unlock();
        while (!atomic_load_explicit(&c->done, memory_order_acquire)) {
            if (++spins < 2000) {
                sched_yield();
            } else {
                struct timespec ts = { 0, 100000 };  /* 100µs */
                nanosleep(&ts, NULL);
            }
        }
        return;
    }
    while (!atomic_load_explicit(&c->done, memory_order_acquire)) {
        if (!self->on_join) {                        /* 防重复登记(粘滞提前醒路径) */
            self->jnext = c->jnext;
            c->jnext = self;
            self->on_join = 1;
        }
        rt_unlock();
        coro_suspend(DESCHED_PARK);
        rt_lock();                                   /* resume:复查(粘滞 pending 可致提前醒) */
    }
    self->on_join = 0;                               /* 被 drain 或未登记,均复位 */
    rt_unlock();
}

void ctron_rt_park(void)
{
    if (!rt_tls_cur()) return;                       /* 裸线程:契约外 no-op */
    coro_suspend(DESCHED_PARK);
}

void ctron_rt_wake(void *key)
{
    rt_coro *c;
    if (!key) return;
    rt_lock();
    c = map_get_locked(key);
    if (c) coro_wake_locked(c);
    rt_unlock();
}

void ctron_rt_sleep_ms(int64_t ms)
{
    if (ms < 0) ms = 0;
    if (!rt_tls_cur()) {                             /* 裸线程回退 nanosleep */
        struct timespec ts = { (time_t)(ms / 1000), (long)((ms % 1000) * 1000000l) };
        while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
            ;
        return;
    }
    if (ms == 0) { coro_suspend(DESCHED_YIELD); return; }
    rt_coro *self = rt_tls_cur();
    rt_lock();
    self->armed = 1;
    self->arm_seq++;
    heap_push(now_ns() + (uint64_t)ms * 1000000ull, self, self->arm_seq);
    rt_unlock();
    coro_suspend(DESCHED_PARK);                      /* wake/到期经统一 wake 路径 */
}

void ctron_rt_wait_fd(int fd, int write_side, int64_t timeout_ms)
{
    (void)fd; (void)write_side; (void)timeout_ms;
    /* P2-A stub:登记-noop + 立即返回。reactor 于 P2-B(Task 2)落地后,
     * 此处向 fd→key 注册就绪兴趣并 park;调用方按冻结契约在返回后重试 syscall。 */
}

void ctron_rt_cancel_wake_all(void)
{
    rt_coro *c;
    rt_lock();
    for (c = g_all; c; c = c->allnext)
        coro_wake_locked(c);     /* PARKED→清 armed+入队;RUNNING/DESCHED→pending;
                                    READY/DONE 吸收(评审 Minor 4:清 armed 防
                                    幽灵定时器唤醒污染后续 park) */
    rt_unlock();
}

int64_t ctron_rt_yield_bench(int rounds)
{
    uint64_t t0, t1;
    int i;
    if (rounds < 0) rounds = 0;
    if (!rt_tls_cur()) {                             /* 裸线程回退:sched_yield 配对 */
        t0 = now_ns();
        for (i = 0; i < rounds * 2; i++) sched_yield();
        t1 = now_ns();
        return (int64_t)(t1 - t0);
    }
    t0 = now_ns();
    for (i = 0; i < rounds; i++) {
        coro_suspend(DESCHED_YIELD);                 /* 离场 → completer 回队 → resume */
        coro_suspend(DESCHED_YIELD);
    }
    t1 = now_ns();
    return (int64_t)(t1 - t0);                       /* rounds 次"配对"的总 ns */
}
