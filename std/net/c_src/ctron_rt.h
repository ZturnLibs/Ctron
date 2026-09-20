/* ctron_rt.h —— Ctron P2 协程运行时(调度基建)。
 *
 * 冻结接口(P2-C/D/E 全依赖,签名不可改;内部实现可自由演化):
 *   - task/chan 以 opaque key(调用方私有指针,如模板侧 ct_task*)挂接;
 *     rt 从不解引用/检视 key 内容,仅作等值比较与哈希键。
 *   - 模型: N worker 线程 × 就绪队列 + 定时器最小堆;协程 = ucontext
 *     (getcontext/makecontext/swapcontext),每协程 64KB mmap 栈 + 空闲池复用。
 *   - 线程口径: 协程上下文内为"协程模式"(ctron_rt_current()!=NULL);
 *     裸 pthread 内为"裸线程模式"(current()==NULL),join/sleep 走自旋/nanosleep 回退。
 *   - 本任务(Template Task 1 / P2-A)wait_fd 为 stub:登记-noop 后立即返回,
 *     reactor 于 Task 2 (P2-B) 接入 —— 调用方按契约在超时/错误后重试 syscall。
 *
 * 并发不变量(lost-wakeup 分析见 ctron_rt.c 头注):
 *   状态推进全部在全局调度锁 G 内;协程离场(swapcontext 出栈)前持锁置
 *   DESCHED,同线程 worker 循环收回控制权后置 suspended=1 再定稿状态
 *   (PARKED/READY/DONE)。任何"可被 resume"的状态(READY/PARKED)只在
 *   suspended=1 之后于 G 内写入 ⇒ 唤醒方在 G 内看到 PARKED 时,目标栈已
 *   确定性离场,跨线程 swapcontext 安全,且 park 前唤醒以 pending 旗标粘滞,
 *   不丢唤醒。
 */
#ifndef CTRON_RT_H
#define CTRON_RT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* workers<=0 → min(cpu 核数, 4);幂等(重复 init 为 no-op) */
void    ctron_rt_init(int workers);

/* getenv("CTRON_RT")=="coro" 且已 init;首次调用后缓存 getenv 结果 */
int     ctron_rt_active(void);

/* 当前协程 key;裸线程/未 init → NULL */
void*   ctron_rt_current(void);

/* 建协程跑 fn(arg);key=调用方私有(模板传 ct_task*);返回 key。
 * 内部包装器:fn 返回后先 ctron_rt_notify_done(key),再永久 park
 * (绝不返回到已死栈)。未 init 时惰性 init(默认 worker 数)。 */
void*   ctron_rt_run(void (*fn)(void*), void* arg, void* key);

/* 挂起当前协程直到 key 协程结束。裸线程 → 自旋等待 done 旗标
 * (state 责任在调用方;模板侧不走此路)。key 未知/已完成 → 立即返回。 */
void    ctron_rt_join_key(void* key);

/* key 协程体结束前由包装器回调:置 done 旗标并唤醒 join 等待者 */
void    ctron_rt_notify_done(void* key);

/* 挂起当前协程,直到被 wake/超时/reactor 就绪(裸线程为 no-op) */
void    ctron_rt_park(void);

/* 唤醒因 park 挂起的协程;对未停车协程粘滞记 pending(先唤醒后停车不丢) */
void    ctron_rt_wake(void* key);

/* 定时器堆;裸线程回退 nanosleep */
void    ctron_rt_sleep_ms(int64_t ms);

/* 向 reactor 注册就绪兴趣并 park;P2-A 为 stub(登记-noop + 立即返回),
 * 超时/错误返回后由调用方重试 syscall。reactor 于 P2-B 落地。 */
void    ctron_rt_wait_fd(int fd, int write_side, int64_t timeout_ms);

/* scope 取消广播:唤醒全部停车协程(DONE 协程除外) */
void    ctron_rt_cancel_wake_all(void);

/* 微基准:rounds 次 yield 配对(离场→重新入队→被 resume),返回总 ns;
 * ns/次 = 返回值 / (2*rounds)。裸线程回退 sched_yield 配对测量。 */
int64_t ctron_rt_yield_bench(int rounds);

#ifdef __cplusplus
}
#endif

#endif /* CTRON_RT_H */
