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
 * ══ P2-B reactor(wait_fd 就绪后台线程)══
 *   专用 reactor 线程,首次协程 wait_fd 惰性创建,进程生命周期常驻不 join
 *   (与 worker 同口径)。后端族(编译期):kqueue(darwin,EVFILT_READ/
 *   WRITE)/ linux 族(io_uring 探测胜出或 epoll 回退,EPOLLIN|OUT)/ POSIX
 *   poll 循环(其他,100ms 轮询节奏——已登记限制;Windows 移植需换 WSAPoll;
 *   IOCP=windows 族未建,环境依赖登记见 T51 评估)。linux 族内 io_uring/
 *   epoll 运行时探测定夺(NODROP 特性门,T51),env CTRON_RT_REACTOR=
 *   epoll|io_uring 压制,ctron_rt_reactor_name() 观测。
 *   一律水平触发(io_uring=POLL_ADD 完成即一次性内生 one-shot;ET/one-shot
 *   +rearm 列 P9),kevent/epoll_wait 统一压 100ms 上限——新登记的"注册时
 *   即就绪"事件不依赖后端对挂起等待的即时唤醒语义,最坏 100ms 兜底交付
 *   (io_uring 豁免:提交方自带 enter 冲刷,就绪判定在提交时,完成即原生
 *   唤醒,免 tick)。
 *   fd+方向 → key 登记表与后端兴趣增删全部在全局锁 G 内(与 park/wake 同锁,
 *   无第二把锁、无锁序问题)。wait_fd:登记 + 后端注册(park 前同一次 G 持有内
 *   完成)→ coro_suspend(DESCHED_PARK);reactor 就绪 → 摘登记 → coro_wake_locked
 *   (三窗口握手内交付,不变量 II 直接覆盖,无丢唤醒窗口;交付可落在 park 前
 *   ——fd 已就绪,pending 消费立即返回,调用方重试自见成功)。超时复用定时器堆
 *   (armed/arm_seq 世代):到点统一 wake 路径醒,调用方重试 syscall 自见超时/
 *   错误 ⇒ wait_fd 返回 ≠ fd 就绪(粘滞误醒/cancel/顶替均提前返回),调用方
 *   必须重试 syscall(冻结契约)。timeout_ms 口径:0=非阻塞探测立即返回,<0=
 *   无限等,>0=等待下界。裸线程:无 park,直接 poll() 等就绪/超时(同契约)。
 *   同 (fd,dir) 并发 wait_fd:last-wins 顶替(被顶者不补 wake——补醒会造成两
 *   协程互顶活锁;其靠自身超时/取消/后续就绪醒)。登记清理:醒来后摘"仍属本
 *   协程"的登记;reactor 交付即摘(防持久就绪让 reactor 空转);(fd,dir) 无登记
 *   时从后端摘除对应滤波器(fd 已 close 经 F_GETFD 探测跳过删除,内核已自动
 *   摘 knote/节点,并规避 fd 号复用 ABA;残留窗口:close+重开同号发生在
 *   F_GETFD 与摘除之间,登记限制)。fd close 而等待者仍 park:kqueue/epoll
 *   无事件、靠超时醒(登记限制);poll 回退以 POLLNVAL 交付。
 *
 * ══ P2-E 确定性调度(CTRON_RT_SEED)══
 *   CTRON_RT_SEED=<n>(非空且 strtoul 全串可解,init 时解析一次)→ 单
 *   worker + 就绪队弹出位由种子 LCG 决定:≥2 就绪时 pop = draw % 队长 抽取
 *   (每 pop 恰消耗一次抽取;单就绪直取、不消耗抽取)。契约面:同种子 ⇒
 *   同就绪交错 ⇒ 程序输出逐字节同——只覆盖调度序,且仅纯 spawn/channel
 *   程序(定时器到期、IO 到达、reactor 交付仍是真实时间,不在契约内)。
 *   串行化机制(spawn 闸):裸线程(main)的 ctron_rt_join_key 是 spawn
 *   风暴的终点信号——闸开前 worker 不弹出(idle 退避空转);否则 main 的
 *   spawn 入队与 worker 弹出并发,任一 pop 时刻的就绪集合本身非确定,任何
 *   抽取法都救不回。闸开后队列唯一变更者 = 单 worker 自身(finalize/
 *   wake-all/定时器全在其 G 临界区内),弹出序 = 确定性 LCG 序。
 *   登记限制:①裸线程先于首个 join 停车在通道上(模板 P1 condvar 面,不经
 *   rt)会令闸永不打开 → SEED 模式挂起;契约图式 = spawn→join(模板 scope
 *   图式,coro_det 夹具口径)。闸是单向的:开后不关——裸线程若在首个
 *   join 之后再度 spawn,入队与单 worker 弹出重新并发,风暴竞态被无声
 *   重新引入(无报错,仅输出序退回非确定);SEED 下裸线程侧只在闸开前
 *   spawn、开闸后不得再 spawn。②非法/空 SEED → 非种子模式:默认路径
 *   (多 worker、FIFO pop)逐字节同 SEED 机制引入前,单就绪快路径在种子
 *   模式下也无新增行为。
 *
 * ══ P3-A 时延首件(事件量交付 + 兴趣驻留 + 探针消除)══
 *   P2 门禁三实测归因 ≈15µs/往返的四分量:per-read poll(0) 探针(垫片侧,
 *   P3-A 以 MSG_DONTWAIT 直试消除)、EV_ADD/EV_DELETE + F_GETFD 每读增删
 *   (本侧,驻留消除)、park/wake 配对(~0.1µs,忽略)、空闲退避(事件到达
 *   吃满一档 nanosleep,20–160µs,大概率主导项)。处置:
 *   ①事件量交付:worker 空队不再纯 nanosleep 轮询,改 cv 上限时退避等待
 *     (20µs<<shift,≤160µs 兜底自醒 —— 看门狗/定时器不饿);ready_push
 *     (交付/定时器/回队/spawn 全部收口于此)在"推入者非 worker 且有 worker
 *     睡在 cv 上"时 signal 立即踢醒一名。推入者为 worker 时不踢:worker 循环
 *     推入后原地继续弹出,惊动同侪只会引入偷活/迁移噪声(yield/ping-pong
 *     负载实测敏感);此类多推突发最坏退回 ≤160µs 自醒节奏,与旧轮询同界。
 *   ②兴趣驻留:登记表改常驻 —— (fd,dir) 首次 wait_fd 建登记,重复 wait_fd
 *     幂等复用(last-wins key 顶替语义不变),醒后不摘、交付不摘,常驻至
 *     fd 关闭(ctron_net_close → ctron_rt_forget_fd 钩子摘;直连 close(2)
 *     由注册时顺手桶清扫兜底)。后端按 one-shot 武装(kqueue EV_ADD|
 *     EV_ONESHOT / epoll EPOLLONESHOT):交付即被内核消费,重挂只在下一次
 *     wait_fd —— 消除每读 EV_ADD/EV_DELETE + F_GETFD 三笔,同时保留"交付即
 *     撤"的防空转性质(one-shot 未武装的登记不参与后端报告,水平触发不会
 *     令 reactor 空转)。armed 软旗标 = 后端 one-shot 的登记表镜像:交付置 0
 *     (吞批内/迟到事件),wait_fd 武装置 1,醒后若登记仍属本协程则解除。
 *   ③垫片探针消除见 ctron_net.c(MSG_DONTWAIT 直试)。
 *   驻留语义登记:同 (fd,dir) 并发 wait_fd 维持 last-wins 顶替(仅最后登记者
 *   被交付唤醒,被顶者靠自身超时/取消/后续就绪醒 —— 驻留下"后续就绪"= 新一
 *   轮武装后的交付);epoll one-shot 按 fd 整体,交付一方向后若反向仍 armed
 *   则以反向掩码重挂(kqueue 滤波器按方向独立,无连带);close→forget 之间
 *   同号 fd 复用的 ABA 窗口(带超时等待者由自身超时兜底;无超时停车者
 *   (如 TLS 握手 timeout=0)不受兜底——挂死形态,结构性消除见 P4 首件
 *   (forget_fd 先于 close 重排))与 P2 头注 F_GETFD 残留窗口同类,登记。
 *
 * 锁纪律:G 绝不跨切换持有(park/yield 在切换前解锁;worker 循环在切换前
 *   解锁),故无"锁随上下文迁移"的跨线程 unlock UB。worker 循环每轮迭代
 *   重新加锁 —— finalize/timers/pop 与 popper 认领全部在锁内串行。
 * 空闲策略(P3-A 改版):空队 = cv 时限退避等待(20µs→160µs 递增,事件量
 *   交付见上节;无条件超时自醒保看门狗/定时器有界)。
 * 栈(P9 起):mmap 栈 + 低地址 1 页 PROT_NONE guard(页粒度取 sysconf,
 *   darwin/arm64 页为 16KB,写死 4KB 会被 mprotect 取整放大、殃及栈本体);
 *   尺寸 env 可配(CTRON_RT_STACK_KB,缺省 1024 = 大 VA+惰性提交口径,T34
 *   最小符合径 a77f141f;匿名 mmap 零填充,惰性分页);空闲池(上限 256)
 *   复用;触顶 = sigaltstack 诊断链 rc=101(T34 最小径)。
 *
 * ══ T34 work-stealing(§7.1 调度口径;2026-10-02)══
 *   形态:两级就绪结构。①全局 FIFO 链(P2 原形)——裸线程侧全部推入
 *   (reactor 交付/裸 wake/main spawn)与本地溢出泼回;种子模式唯一使用面。
 *   ②本地环形双端队(每 worker,容量 256)——worker 上下文内的推入收口本
 *   worker:yield 回队/定时器醒/wake/spawn 全走 owner 尾端(LIFO,刚 yield
 *   的协程最热,ping-pong/yield 负载局部性);饥饿 worker 弹出链 = 本地→
 *   全局→自邻位轮转偷取(受害者头端,最老者,FIFO 利广依赖)。
 *   锁纪律:两级队全部 G 内读写 —— 不变量 I/II 的 G 串行化论证(completer
 *   定稿与 pop 同临界区、出队即认领)原样成立,零新增无锁不变量;G 竞争面
 *   与改前同(P3-A 门禁不回退为验收项,非目标=吞吐重构)。
 *   溢出:本地队满 256 → 老的一半按序泼回全局队(自 head 顺序摘,先后序
 *   保持;泼回者 = worker,不踢醒,同 worker 推入口径)。
 *   踢醒:P3-A 口径不变 —— 仅非 worker 推全局队踢醒一名睡者;worker 推
 *   本地队不踢(惊动同侪只添偷活噪声),饥饿者吃 ≤160µs cv 自醒后 steal
 *   兜底。窃取计数 g_steals 经 ctron_rt_steals() 观测导出(w7 锚消费)。
 *   种子模式(P2-E)全量旁路:worker 推入也走全局队、弹出走 LCG 抽取、
 *   nworkers 压 1 ⇒ 确定性契约逐字节不变(coro_det 常设回归站岗)。
 *   栈面(§7.1 可增长连续栈):正案判决同日入册 —— 拷贝式需 emit 栈图+
 *   根区间改写+callee-saved 寄存器镜像修复三件机器;分段式 guard 续跑亦需
 *   发射器 prologue 配合;退路「1MB 大栈+guard+触顶诊断」(a77f141f)为
 *   §7.1 字面终形,详见 docs/superpowers/specs/2026-09-26-server-p9-*。
 *
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
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>                               /* F_GETFD:摘后端兴趣前探 fd 存活 */
#include <poll.h>                                /* 裸线程 wait_fd 回退 + poll 后端 */
#include <dirent.h>                              /* T51 NUMA 拓扑探测(/sys 节点计) */
#include <sys/mman.h>
#if defined(__APPLE__)
#include <sys/event.h>                           /* kqueue(P2-B reactor 后端) */
#elif defined(__linux__)
#include <sys/epoll.h>                           /* epoll(P2-B reactor 后端) */
#endif

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

/* ───────────────────────── 常量 ───────────────────────── */
#define RT_STACK_SIZE_DEF   (64u * 1024u)       /* 每协程栈缺省(P2 冻结口径;P9 起可配) */

/* P9 前置测量面:栈尺寸 env 可配(CTRON_RT_STACK_KB,4–1024 KB;缺省 64 =
 * 原冻结口径,零行为变化)。栈经济专案(设计呈报 2026-09-26)供数:小栈下
 * 的功能回归与内存密度实测。首次取用即缓存(进程内恒定)。 */
static size_t rt_stack_size(void)
{
    static size_t cached;
    if (cached == 0) {
        const char *e = getenv("CTRON_RT_STACK_KB");
        long kb = (e && *e) ? atol(e) : 1024;
        if (kb < 4) kb = 4;
        if (kb > 1024) kb = 1024;
        cached = (size_t)kb * (size_t)1024;
    }
    return cached;
}
#define RT_POOL_CAP     256                    /* 栈空闲池上限 */
#define RT_MAX_WORKERS  64
#define RT_LQ_CAP       256                    /* T34 work-stealing:本地就绪环容量(2 的幂) */
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
    int        idx;              /* 本 worker 下标(steal 受害者轮转起点;init 时写定) */
    /* T34 work-stealing:本地就绪环形双端队 —— owner 尾端 push/pop(LIFO,
     * 刚 yield 的协程最热);他 worker 偷头端(最老者,FIFO 利广依赖)。G 内
     * 读写(不引入无锁不变量;不变量 I/II 的 G 串行化论证原样保留)。 */
    rt_coro   *lq[RT_LQ_CAP];
    size_t     lq_head;          /* 最老元素下标(模 RT_LQ_CAP) */
    size_t     lq_len;           /* 现存元素数 */
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
static _Atomic long g_steals = 0;           /* T34:跨 worker 偷取累计(仅观测) */

/* P3-A 事件量交付:空队 worker 在 cv 上限时退避等待;ready_push 在推入者非
 * worker 且 g_sleepers>0 时 signal 踢醒一名(见文件头注 P3-A ①)。 */
static pthread_cond_t g_idle_cv = PTHREAD_COND_INITIALIZER;
static int g_sleepers = 0;                  /* 正在 cv 等待的 worker 数(G 内读写) */

/* P2-E 确定性调度(见文件头注):SEED 模式下单 worker + 种子化弹出序。
 * 三者均 G 内读写;g_seed_state 仅在 ready_pop 的抽取路径推进。 */
static int      g_seed_mode = 0;            /* CTRON_RT_SEED 非空且可解析 */
static uint64_t g_seed_state = 0;           /* LCG 状态(种子即初态) */
static int      g_gate_open = 0;            /* spawn 闸:裸线程 join 前不弹出 */

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
/* 时钟钩(T10;§11.6):net 垫片强覆盖以贯通虚拟钟(ctron_net_clock_jump);
 * 未链垫片/实钟态 = 本弱缺省(单调实钟)。 */
__attribute__((weak)) uint64_t ctron_rt_clock_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static uint64_t now_ns(void)
{
    return ctron_rt_clock_ns();
}

static size_t key_bucket(void *k)
{
    uint64_t x = (uint64_t)(uintptr_t)k;
    x ^= x >> 16;
    x *= 0x9E3779B97F4A7C15ull;
    x ^= x >> 32;
    return (size_t)x & (RT_MAP_BUCKETS - 1);
}

/* ───────────────────────── 就绪队(G 内) ─────────────────────────
 * T34 work-stealing 两级结构:
 *   全局队(单 FIFO 链,同 P2 原形)——裸线程侧全部推入(reactor 交付/裸
 *     wake/spawn)与本地队溢出泼回;种子模式(P2-E)唯一使用面。
 *   本地队(每 worker 环形双端,见 rt_worker)——worker 上下文内的推入
 *     (yield 回队/定时器醒/wake/spawn)收口本 worker,吃满局部性;饥饿
 *     worker 经 steal 轮转取他队最老者。全程 G 内,无锁纪律零新增。 */
static void gq_push(rt_coro *c)
{
    c->qnext = NULL;
    if (g_ready_tail) g_ready_tail->qnext = c;
    else              g_ready_head = c;
    g_ready_tail = c;
}

static rt_coro *gq_pop(void)
{
    rt_coro *c = g_ready_head;
    if (!c) return NULL;
    g_ready_head = c->qnext;
    if (!g_ready_head) g_ready_tail = NULL;
    c->qnext = NULL;
    return c;
}

/* 本地队 owner 端推(LIFO 尾端);满则老的一半泼回全局队头序保持
 * (自 head 起顺序摘——泼回后全局队内先后序 = 原本地队老→新,G 内无并发)。 */
static void lq_push(rt_worker *w, rt_coro *c)
{
    if (w->lq_len == RT_LQ_CAP) {
        size_t spill = w->lq_len / 2, i;
        for (i = 0; i < spill; i++) {
            gq_push(w->lq[w->lq_head]);
            w->lq_head = (w->lq_head + 1) & (RT_LQ_CAP - 1);
            w->lq_len--;
        }
    }
    w->lq[(w->lq_head + w->lq_len) & (RT_LQ_CAP - 1)] = c;
    w->lq_len++;
}

/* owner 弹(尾端,最热);空返回 NULL。 */
static rt_coro *lq_pop(rt_worker *w)
{
    rt_coro *c;
    if (w->lq_len == 0) return NULL;
    w->lq_len--;
    c = w->lq[(w->lq_head + w->lq_len) & (RT_LQ_CAP - 1)];
    return c;
}

/* 偷取弹(头端,最老——广依赖公平);空返回 NULL。 */
static rt_coro *lq_steal(rt_worker *w)
{
    rt_coro *c;
    if (w->lq_len == 0) return NULL;
    c = w->lq[w->lq_head];
    w->lq_head = (w->lq_head + 1) & (RT_LQ_CAP - 1);
    w->lq_len--;
    return c;
}

/* 统一就绪推入(P2 原 ready_push 的 T34 路由版):
 *   worker 上下文且非种子模式 → 本 worker 本地队(无踢醒:P3-A 同款论证,
 *     惊动同侪只添偷活噪声;饥饿者 ≤160µs 自醒后 steal 兜底);
 *   裸线程(reactor/裸 wake/main spawn)或种子模式 → 全局队 + 踢醒一名
 *     睡者(闸未开不踢,P2-E 口径逐字节保留)。 */
static void ready_push(rt_coro *c)
{
    rt_worker *w = rt_tls_worker();
    atomic_store_explicit(&g_backoff, 0, memory_order_relaxed);
    if (w != NULL && !g_seed_mode) {
        lq_push(w, c);
        return;
    }
    gq_push(c);
    if (g_sleepers > 0 && rt_tls_worker() == NULL &&
        !(g_seed_mode && !g_gate_open))
        pthread_cond_signal(&g_idle_cv);
}

/* P2-E:种子 LCG 一步(Numerical Recipes 参数;加法项保证 seed=0 也产非零
 * 流),右移丢弃短周期的低位。仅 G 内调用 ⇒ 推进序 = 弹出序,可重放。 */
static uint64_t seed_draw_locked(void)
{
    g_seed_state = g_seed_state * 6364136223846793005ull + 1442695040888963407ull;
    return g_seed_state >> 16;
}

static rt_coro *ready_pop(void)
{
    rt_coro *c = g_ready_head;
    if (!c) return NULL;
    if (g_seed_mode && c->qnext) {
        /* P2-E:≥2 就绪 → 弹出位 = draw % 队长(确定性交错面)。单就绪走
         * 下方原快路径,不消耗抽取;非种子模式完全不进此分支,行为逐字节
         * 同引入前。队长 O(n) 一遍(单 worker 下短队;SEED 模式性能不在
         * 契约面)。 */
        rt_coro *p, *prev = NULL;
        uint64_t n = 0, k;
        for (p = c; p; p = p->qnext) n++;
        k = seed_draw_locked() % n;
        p = c;
        while (k) { prev = p; p = p->qnext; k--; }
        c = p;
        if (prev) prev->qnext = c->qnext;    /* 队中摘链 */
        else      g_ready_head = c->qnext;
        if (g_ready_tail == c) g_ready_tail = prev;
        c->qnext = NULL;
        return c;
    }
    return gq_pop();
}

/* T34 work-stealing:worker 弹出链 —— 种子模式走 ready_pop(P2-E LCG 抽取
 * 位,确定性契约);非种子 = 本地队 → 全局队 → 邻位起轮转偷取(各受害者取
 * 一,取到即返;下轮迭代继续 = 实质虹吸)。返回 NULL = 全域确空,调用方走
 * idle 退避。 */
static rt_coro *worker_pop(rt_worker *self)
{
    rt_coro *c;
    int k;
    if (g_seed_mode) return ready_pop();
    c = lq_pop(self);
    if (c) return c;
    c = gq_pop();
    if (c) return c;
    for (k = 1; k < g_nworkers; k++) {
        c = lq_steal(&g_workers[(self->idx + k) % g_nworkers]);
        if (c) {
            atomic_fetch_add_explicit(&g_steals, 1, memory_order_relaxed);
            return c;
        }
    }
    return NULL;
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
    *map_sz = rt_stack_size() + (size_t)ps;
    rt_lock();
    if (g_npool > 0) {
        base = g_pool[--g_npool];
        rt_unlock();
        return base;
    }
    rt_unlock();
    base = mmap(NULL, *map_sz, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) return NULL;
    (void)mprotect(base, (size_t)ps, PROT_NONE);         /* guard(可选失败不致命) */
    return base;
}

/* 仅在协程 DONE(FOREVER completer,栈已确定性离场)后调用。
 * 登记 64KB 栈高水位(自栈顶向下找首个仍干净的 0xA5 字节,其上方即已用前缀),
 * 然后入池/释放。 */
static void stack_retire(rt_coro *c)
{
    unsigned char *lo = c->stack + rt_pagesize();
    size_t ssz = rt_stack_size();
    long used = (long)ssz;                           /* 全脏兜底:整栈用满 */
    /* 栈自高端(top)向低端(guard)生长:已用字节 = top 端前缀。
     * 自 top-1 向下扫首个 ==0xA5 的干净字节,其上方 [i..top) 全为脏字节,
     * used = RT_STACK_SIZE - i(P2-B 评审修复:旧谓词找 !=0xA5,自顶扫第一个
     * 字节即中招,used 恒为最小值,高水位统计形同虚设)。 */
    for (size_t i = ssz; i > 0; i--) {
        if (lo[i - 1] == 0xA5) { used = (long)(ssz - i); break; }
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
/* 空闲退避(P3-A 事件量交付版):调用时持 G,返回时已放 G(worker 循环头部
 * 重锁)。cv 时限等待 —— ready_push 踢醒(交付到达立即返工)或 ≤160µs 超时
 * 自醒(看门狗/定时器有界,与旧 nanosleep 同界)。虚假唤醒无害:返回后循环
 * 头重锁重查队列。 */
static void idle_backoff(void)
{
    unsigned b = atomic_load_explicit(&g_backoff, memory_order_relaxed);
    long ns = 20000l << (b > RT_BACKOFF_MAX_SHIFT ? RT_BACKOFF_MAX_SHIFT : b);
#if defined(__APPLE__)
    /* darwin:无 pthread_condattr_setclock,用相对时限变体(静态初始化 cv) */
    struct timespec ts = { 0, ns };
    g_sleepers++;
    pthread_cond_timedwait_relative_np(&g_idle_cv, &rt_g, &ts);
    g_sleepers--;
    rt_unlock();
#elif defined(__linux__)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_nsec += ns;
    if (ts.tv_nsec >= 1000000000l) { ts.tv_sec += 1; ts.tv_nsec -= 1000000000l; }
    g_sleepers++;
    pthread_cond_timedwait(&g_idle_cv, &rt_g, &ts);   /* cv 已按 MONOTONIC 初始化 */
    g_sleepers--;
    rt_unlock();
#else
    /* 其他 POSIX:无踢醒面,退回纯 nanosleep(登记限制,同旧口径) */
    struct timespec ts = { 0, ns };
    rt_unlock();
    nanosleep(&ts, NULL);
#endif
    if (b < RT_BACKOFF_MAX_SHIFT)
        atomic_store_explicit(&g_backoff, b + 1, memory_order_relaxed);
}

static void rt_altstack_install(void);
static void rt_numa_probe(void);              /* T51 NUMA 拓扑探测(init 调用,定义在访问器区) */

static void *worker_main(void *p)
{
    rt_altstack_install();   /* T34:每 worker 线程各自 sigaltstack(SA_ONSTACK 线程局部) */
    rt_worker *ws = (rt_worker*)p;
    rt_coro *just_desched = NULL;
    rt_coro *retire = NULL;          /* (P2-B 修复) FOREVER 离场待回收,跨迭代保留:
                                        原声明在循环体内,若 finalize-FOREVER 恰逢
                                        空队迭代(idle_backoff+continue),retire 被
                                        丢弃 → 栈永不入池/释放、高水位永不登记。 */
    rt_tls_set_worker(ws);
    for (;;) {
        rt_coro *c;
        rt_lock();                       /* 每轮迭代持 G:finalize/timers/pop 全在锁内 */
        if (just_desched) {              /* 与 pop 合并一次加锁(收益路径) */
            coro_finalize_locked(just_desched);
            if (just_desched->desched_kind == DESCHED_FOREVER)
                retire = just_desched;   /* 栈回收/高水位移出锁外 */
            just_desched = NULL;
        }
        timers_fire_locked();
        /* P2-E spawn 闸:种子模式下,裸线程首个 join_key 前不弹出(见文件
         * 头注)——否则 main 的 spawn 入队与弹出并发,就绪集合时刻非确定。
         * 闸关时走下方空队路径(idle 退避/retire 回收照常)。 */
        c = (g_seed_mode && !g_gate_open) ? NULL : worker_pop(ws);
        if (!c) {
            if (retire) {                /* (P2-B 修复) 空队也照常回收,不再丢弃 */
                rt_unlock();
                stack_retire(retire);
                retire = NULL;
                continue;                /* 循环头重锁 */
            }
            idle_backoff();              /* 放 G 退避;循环头重锁 */
            continue;
        }
        atomic_store_explicit(&g_backoff, 0, memory_order_relaxed);
        c->state = RT_ST_RUNNING;        /* 出队即认领:单 runner 保证 */
        rt_unlock();                     /* G 绝不跨上下文切换 */
        if (retire) { stack_retire(retire); retire = NULL; }
        rt_tls_set_cur(c);               /* current() 口径;tramp 首入口读它 */
        rt_swap(&ws->sched, &c->ctx);
        rt_tls_set_cur(NULL);
        just_desched = c;
    }
}

/* ═══════════════ P2-B reactor:fd 就绪后台线程(wait_fd) ═══════════════ */
enum { RT_FD_READ = 0, RT_FD_WRITE = 1 };

typedef struct rt_fdwait {
    int              fd;
    int              dir;        /* RT_FD_* */
    void            *key;        /* last-wins 当前等待协程 key(仅等值比较,同 rt 契约) */
    int              armed;      /* P3-A:1=有停车等待者(交付有效);0=已交付/无人等待
                                    (吞事件)—— 后端 one-shot 的登记表镜像 */
    int              in_rb;      /* P3-A(epoll):fd 已在后端注册过(ADD/MOD 选择;
                                    epoll 注册按 fd 一条,双方向 entry 共享该事实) */
    struct rt_fdwait *hnext;     /* (fd,dir) 哈希桶链 */
} rt_fdwait;

#define RT_FDMAP_BUCKETS 512                  /* 2 的幂 */

static rt_fdwait *g_fdmap[RT_FDMAP_BUCKETS];  /* 登记表(G 内读写) */

#if defined(__APPLE__)
# define RT_BACKEND_KQUEUE 1
#elif defined(__linux__)
# define RT_BACKEND_EPOLL 1                   /* linux 族:io_uring(探测胜出)/epoll(回退) */
#else
# define RT_BACKEND_POLL 1                    /* POSIX poll 循环(Windows 移植需换 WSAPoll;登记限制) */
#endif

#if defined(RT_BACKEND_EPOLL) && !defined(RT_NO_IOURING)
# define RT_IOURING_CAP 1                     /* T51 §9.5:io_uring 候选(运行时探测定夺) */
#endif

static int       g_rb_fd = -1;                /* kqueue/epoll 描述符;poll 回退恒为 -1 */
static int       g_reactor_started = 0;       /* G 内读写 */
static pthread_t g_reactor_th;

/* T51:linux 族内 io_uring/epoll 运行时后端谓词。kqueue/poll 族恒 0(编译期
 * 常量,谓词分支被死码消除);RT_URING() 调用点恒在 G 内或后端定性后单线程。 */
#if defined(RT_IOURING_CAP)
static int g_rb_uring = 0;                    /* 1=io_uring;0=epoll(启动时定性,此后只读) */
# define RT_URING() g_rb_uring
#else
# define RT_URING() 0
#endif

static size_t fd_bucket(int fd, int dir)
{
    uint64_t x = (uint64_t)(uint32_t)(unsigned)fd * 2u + (unsigned)dir;
    x *= 0x9E3779B97F4A7C15ull;
    return (size_t)(x >> 32) & (RT_FDMAP_BUCKETS - 1);
}

static rt_fdwait *fdwait_find_locked(int fd, int dir)
{
    rt_fdwait *e;
    for (e = g_fdmap[fd_bucket(fd, dir)]; e; e = e->hnext)
        if (e->fd == fd && e->dir == dir) return e;
    return NULL;
}

/* 摘除 (fd,dir) 登记(不 free);找不到返回 NULL。 */
static rt_fdwait *fdwait_take_locked(int fd, int dir)
{
    rt_fdwait **pp = &g_fdmap[fd_bucket(fd, dir)];
    while (*pp) {
        rt_fdwait *e = *pp;
        if (e->fd == fd && e->dir == dir) {
            *pp = e->hnext;
            e->hnext = NULL;
            return e;
        }
        pp = &e->hnext;
    }
    return NULL;
}

/* G 内:P3-A 驻留登记清扫 —— 摘除桶内 fd 已关闭的登记(直连 close(2) 不经
 * ctron_net_close → ctron_rt_forget_fd 钩子的路径;内核已在 close 时自动摘
 * knote/节点,此处仅登记表内存,不触碰后端)。注册路径顺手调用,桶链短。 */
static void fdwait_sweep_bucket_locked(size_t b)
{
    rt_fdwait **pp = &g_fdmap[b];
    while (*pp) {
        rt_fdwait *e = *pp;
        if (fcntl(e->fd, F_GETFD) < 0) { *pp = e->hnext; free(e); }
        else pp = &e->hnext;
    }
}

/* G 内:P3-A 兴趣武装(one-shot)。驻留登记幂等复用,武装总是一次后端
 * (重)启用 —— 交付即被内核消费(one-shot 自动撤销),重挂只在下一次
 * wait_fd:每读的 EV_ADD/EV_DELETE + F_GETFD 三笔收敛为每停一笔。
 * 返回 0=成功,-1=后端拒绝(fd 已坏)——调用方将不 park 直接返回,让调用方
 * 重试 syscall 见错。 */
#if defined(RT_IOURING_CAP)
/* ═══════════ T51 §9.5 io_uring 后端(完成队列族,linux) ═══════════
 * 与 kqueue/epoll 的就绪队列同构映射:每次武装 = 一笔 IORING_OP_POLL_ADD
 * SQE(完成即消费,天然 one-shot,与 EV_ONESHOT/EPOLLONESHOT 口径重合);
 * 就绪交付 = CQE → reactor_deliver_locked(armed 吞发/last-wins 全复用,
 * 零新契约)。仅用 POLL_ADD/POLL_REMOVE 两操作,POLL_ADD 完成即一次性 ——
 * epoll 的 in_rb/乐观 MOD/EEXIST 升级/交付连坐重挂四面在 io_uring 无存在
 * 必要(one-shot 是内核原生的,非注册语义拼出)。
 *
 * uapi 手定义(不引 <linux/io_uring.h>):零外部依赖纪律 + darwin 侧可
 * 语法编译验证;布局对 kernel uapi 逐字节钉死(6.x 源照抄,向后兼容读面:
 * poll 事件走偏移 28 —— 旧内核读 u16 poll_events、新内核读 u32
 * poll32_events,共址,POLLIN=1/POLLOUT=4 皆 u16 域内)。
 *
 * 线程模型:全部提交(G 内)由武装/摘除方直推直 flush(单生产者经 G 串行
 * 化,SQ 尾 store-release);reactor 线程是唯一消费者 —— 阻塞在
 * io_uring_enter(GETEVENTS, min_complete=1),内核原生唤醒(完成即醒),
 * 无需 kqueue/epoll 的 100ms 兜底 tick:「注册时即就绪」由 POLL_ADD 提交
 * 时的内核就绪检查即时交付(提交方自己的 enter 调用顺带冲刷)。
 * 探测门(io_uring_setup 成功 + NODROP 特性):NODROP ≥5.5 —— CQ 溢出
 * 内核等待而非丢弃,契约不容丢事件;POLL_REMOVE 恰同代。不满足 → epoll
 * 回退,行为同形。env CTRON_RT_REACTOR=epoll 压制回 epoll;=io_uring 点名
 * 而环境不给 → abort(后端创建失败惯例)。 */
static void reactor_deliver_locked(int fd, int dir);   /* 前向:ctio_drain 交付面(定义在 arm 之后) */
struct ctio_sqe {                             /* io_uring_sqe 前 64B(只用所列字段) */
    uint8_t  opcode;
    uint8_t  flags;
    uint16_t ioprio;
    int32_t  fd;
    uint64_t off;                             /* POLL_REMOVE:待取消 req 的 user_data */
    uint64_t addr;
    uint32_t len;
    uint32_t ev;                              /* 偏移 28:poll_events/poll32_events 共址 */
    uint64_t user_data;                       /* 偏移 32 */
};
struct ctio_cqe {                             /* io_uring_cqe(16B) */
    uint64_t user_data;
    int32_t  res;
    uint32_t flags;
};
struct ctio_sq_off { uint32_t head, tail, ring_mask, ring_entries, flags, dropped, array, resv1; uint64_t user_addr; };
struct ctio_cq_off { uint32_t head, tail, ring_mask, ring_entries, overflow, cqes, flags, resv1; uint64_t user_addr; };
struct ctio_params {
    uint32_t sq_entries, cq_entries, flags, sq_thread_cpu, sq_thread_idle, features, wq_fd;
    uint32_t resv[3];
    struct ctio_sq_off sq_off;
    struct ctio_cq_off cq_off;
};
#define CTIO_OP_POLL_ADD    6                 /* uapi 枚举序:NOP=0..WRITE_FIXED=5 */
#define CTIO_OP_POLL_REMOVE 7
#define CTIO_SETUP_CQSIZE   (1u << 3)
#define CTIO_ENTER_GETEVENTS 1u
#define CTIO_FEAT_NODROP    (1u << 1)         /* 探测门:≥5.5(溢出等待 + POLL_REMOVE) */
#define CTIO_FEAT_SINGLE_MMAP (1u << 0)        /* SQ/CQ 单区映射(新布局族) */
#define CTIO_TAG_REMOVE     (1ull << 62)      /* REMOVE 回执哨兵(真实 ud = fd*2+dir < 2^62) */

static int       g_uring_fd = -1;
static uint8_t  *g_sq_ring;                   /* SQ 环(header + array[]) */
static struct ctio_sqe *g_sqes;               /* SQE 数组(独立 mmap 段) */
static uint8_t  *g_cq_ring;                   /* CQ 环 */
static struct ctio_sq_off g_sq_off;
static struct ctio_cq_off g_cq_off;
static uint32_t  g_sq_mask;
static uint32_t  g_cq_mask;
static unsigned  g_sq_pending = 0;            /* G 内:已 push 未 flush 计数 */
static int       g_selftest_seen = 0;         /* 自检已核对 CQE 数(init 前单线程) */

static long ctio_sys(long nr, long a, long b, long c, long d)
{
#if defined(__x86_64__) || defined(__aarch64__)
    return syscall(nr, a, b, c, d);           /* setup=425/enter=426(x86_64 与 arm64 同号) */
#else
    (void)nr; (void)a; (void)b; (void)c; (void)d;
    return -ENOSYS;                           /* 其他架构:探测必败 → epoll 回退 */
#endif
}

static uint64_t ctio_ud(int fd, int dir)
{
    return ((uint64_t)(uint32_t)(unsigned)fd << 1) | (uint64_t)(unsigned)dir;
}

/* G 内:push 一笔 SQE(只写不提交;flush 成对)。环满不该发生(单生产者
 * 逐笔即推即冲),发生即拒绝(调用方按后端拒绝口径不 park 直返)。 */
static int ctio_push_locked(const struct ctio_sqe *s)
{
    uint32_t tail = __atomic_load_n((uint32_t *)(g_sq_ring + g_sq_off.tail), __ATOMIC_RELAXED);
    uint32_t head = __atomic_load_n((uint32_t *)(g_sq_ring + g_sq_off.head), __ATOMIC_ACQUIRE);
    uint32_t slot;
    if ((uint32_t)(tail - head) > g_sq_mask) return -1;   /* 满(防御,契约上不可达) */
    slot = tail & g_sq_mask;
    ((uint32_t *)(g_sq_ring + g_sq_off.array))[slot] = slot;   /* sqarray 恒等映射 */
    g_sqes[slot] = *s;
    __atomic_store_n((uint32_t *)(g_sq_ring + g_sq_off.tail), tail + 1, __ATOMIC_RELEASE);
    g_sq_pending++;
    return 0;
}

/* G 内:enter 冲刷全部 pending。失败回卷(单生产者,G 内,安全)。 */
static int ctio_flush_locked(void)
{
    int r;
    if (g_sq_pending == 0) return 0;
    do {
        r = (int)ctio_sys(426, g_uring_fd, (long)g_sq_pending, 0, 0);   /* io_uring_enter:只提交 */
    } while (r < 0 && errno == EINTR);
    if (r < 0) {
        uint32_t tail = __atomic_load_n((uint32_t *)(g_sq_ring + g_sq_off.tail), __ATOMIC_RELAXED);
        __atomic_store_n((uint32_t *)(g_sq_ring + g_sq_off.tail), tail - (uint32_t)g_sq_pending,
                         __ATOMIC_RELEASE);
        g_sq_pending = 0;
        return -1;
    }
    g_sq_pending = 0;                          /* 提交数 ≤ pending 恒成立(G 串行) */
    return 0;
}

/* G 内:武装 = 一笔 POLL_ADD 即推即冲。完成即一次性(one-shot 内生)。
 * 已就绪 fd 由内核在提交时判定,CQE 即刻落地并唤醒阻塞中的 reactor ——
 * 免 100ms 兜底 tick 的「注册即就绪」路径。失败(环满/flush 败)= 后端
 * 拒绝,调用方不 park 直接重试 syscall 见错(同 kqueue/epoll 拒绝口径)。 */
static int ctio_arm_poll_locked(rt_fdwait *e)
{
    struct ctio_sqe s;
    memset(&s, 0, sizeof s);
    s.opcode = CTIO_OP_POLL_ADD;
    s.fd = e->fd;
    s.ev = (uint32_t)(e->dir == RT_FD_WRITE ? POLLOUT : POLLIN);
    s.user_data = ctio_ud(e->fd, e->dir);
    if (ctio_push_locked(&s) != 0) return -1;
    return ctio_flush_locked();
}

/* G 内:fd 关闭钩子配套 —— 双方向 POLL_REMOVE(取消在飞 POLL_ADD,释放
 * 文件引用;io_uring 不随 close 自动摘,与 kqueue/epoll 的内核自动摘差异
 * 在此补齐)。已完成的取消响 ENOENT(哨兵 CQE,收割时吞)。 */
static void ctio_forget_polls_locked(int fd)
{
    int d;
    for (d = 0; d < 2; d++) {
        struct ctio_sqe s;
        memset(&s, 0, sizeof s);
        s.opcode = CTIO_OP_POLL_REMOVE;
        s.fd = -1;
        s.off = ctio_ud(fd, d);               /* 匹配待取消 req 的 user_data */
        s.user_data = CTIO_TAG_REMOVE | ctio_ud(fd, d);
        if (ctio_push_locked(&s) == 0) (void)ctio_flush_locked();
    }
}

/* reactor 线程:收割全部可用 CQE 并按 armed 吞发交付;无主 CQE(取消回执/
 * 迟到重复/REMOVE 哨兵)一律吞 —— 同 reactor_deliver_locked 的吞发判据。 */
static void ctio_drain_deliver_locked(void)
{
    for (;;) {
        uint32_t head = __atomic_load_n((uint32_t *)(g_cq_ring + g_cq_off.head), __ATOMIC_RELAXED);
        uint32_t tail = __atomic_load_n((uint32_t *)(g_cq_ring + g_cq_off.tail), __ATOMIC_ACQUIRE);
        struct ctio_cqe *cqes;
        uint64_t ud;
        if (head == tail) return;
        cqes = (struct ctio_cqe *)(g_cq_ring + g_cq_off.cqes);
        ud = cqes[head & g_cq_mask].user_data;
        __atomic_store_n((uint32_t *)(g_cq_ring + g_cq_off.head), head + 1, __ATOMIC_RELEASE);
        if (ud & CTIO_TAG_REMOVE) continue;
        reactor_deliver_locked((int)(ud >> 1), (int)(ud & 1u));
    }
}

/* 开机自检(启动线程,reactor 未起,G 已持有):**两笔** NOP 往返 —— 提交后
 * 界定等待(~40ms 上限)逐笔收割 CQE,ud/res 逐字核对。两笔缺一不可:单笔
 * 在「array 间接层失效」的内核上会侥幸全对(恒消费 slot0;6.10-linuxkit
 * 实测在案),双发才钉死顺序消费正确性。任何一环不达(内核 ABI 不在预期
 * 形态/极端过载)即 -1,调用方回退 epoll —— io_uring 后端永不以坏环上线
 * (环境探测先行,契约不破)。 */
static int ctio_selftest(void)
{
    int k;
    for (k = 0; k < 2; k++) {
        struct ctio_sqe s;
        memset(&s, 0, sizeof s);
        s.opcode = 0;                          /* IORING_OP_NOP */
        s.user_data = 0xDEAD0001ull + (uint64_t)k;
        if (ctio_push_locked(&s) != 0) return -1;
    }
    if (ctio_flush_locked() != 0) return -1;
    {
        int i;
        for (i = 0; i < 400; i++) {            /* 400 × 100µs = 40ms 上限 */
            uint32_t head = __atomic_load_n((uint32_t *)(g_cq_ring + g_cq_off.head), __ATOMIC_RELAXED);
            uint32_t tail = __atomic_load_n((uint32_t *)(g_cq_ring + g_cq_off.tail), __ATOMIC_ACQUIRE);
            if (head != tail) {
                struct ctio_cqe *cqes = (struct ctio_cqe *)(g_cq_ring + g_cq_off.cqes);
                uint64_t ud = cqes[head & g_cq_mask].user_data;
                int32_t res = cqes[head & g_cq_mask].res;
                __atomic_store_n((uint32_t *)(g_cq_ring + g_cq_off.head), head + 1, __ATOMIC_RELEASE);
                if (ud != 0xDEAD0001ull + g_selftest_seen || res != 0) return -1;
                if (++g_selftest_seen == 2) return 0;
                continue;
            }
            {
                struct timespec ts = { 0, 100 * 1000 };
                nanosleep(&ts, NULL);
            }
        }
    }
    return -1;                                 /* 40ms 无 CQE:环不通 */
}

static void *reactor_main_uring(void *unused)
{
    (void)unused;
    for (;;) {
        int r = (int)ctio_sys(426, g_uring_fd, 0, 1, CTIO_ENTER_GETEVENTS);   /* 阻塞至 ≥1 CQE */
        if (r < 0) {
            if (errno == EINTR) continue;
            struct timespec ts = { 0, 10 * 1000 * 1000 };   /* 非常规错:退避续跑 */
            nanosleep(&ts, NULL);
            continue;
        }
        rt_lock();
        ctio_drain_deliver_locked();
        rt_unlock();
    }
}
#endif /* RT_IOURING_CAP */

static int fdwatch_arm_locked(rt_fdwait *e)
{
#if defined(RT_BACKEND_KQUEUE)
    struct kevent ke;
    EV_SET(&ke, (uintptr_t)e->fd, e->dir == RT_FD_WRITE ? EVFILT_WRITE : EVFILT_READ,
           EV_ADD | EV_ONESHOT, 0, 0, NULL);
    /* EV_ADD 幂等更新:首次注册与重启用同形 */
    return kevent(g_rb_fd, &ke, 1, NULL, 0, NULL) == 0 ? 0 : -1;
#elif defined(RT_BACKEND_EPOLL)
    if (RT_URING()) return ctio_arm_poll_locked(e);   /* T51:io_uring 接管武装 */
    struct epoll_event ev;
    /* epoll one-shot 按 fd 整体注册:掩码 = 两方向 armed 并集(未武装方向
     * 不入掩码 —— 驻留旧登记不得消费本次 one-shot、不得引入虚假交付)。 */
    rt_fdwait *o = fdwait_find_locked(e->fd, e->dir == RT_FD_WRITE ? RT_FD_READ : RT_FD_WRITE);
    uint32_t mask = (uint32_t)(e->dir == RT_FD_WRITE ? EPOLLOUT : EPOLLIN);
    if (o && o->armed) mask |= (uint32_t)(o->dir == RT_FD_WRITE ? EPOLLOUT : EPOLLIN);
    memset(&ev, 0, sizeof ev);
    ev.events = mask | EPOLLONESHOT;
    ev.data.u64 = (uint64_t)(uint32_t)e->fd;
    if (e->in_rb || (o && o->in_rb)) {
        if (epoll_ctl(g_rb_fd, EPOLL_CTL_MOD, e->fd, &ev) == 0) {
            e->in_rb = 1; if (o) o->in_rb = 1;
            return 0;
        }
        if (errno != ENOENT) return -1;    /* 乐观 MOD 失效(登记限制外):降级 ADD */
    }
    if (epoll_ctl(g_rb_fd, EPOLL_CTL_ADD, e->fd, &ev) != 0) {
        if (errno != EEXIST) return -1;    /* 残留旧注册:升级为 MOD */
        if (epoll_ctl(g_rb_fd, EPOLL_CTL_MOD, e->fd, &ev) != 0) return -1;
    }
    e->in_rb = 1; if (o) o->in_rb = 1;
    return 0;
#else
    (void)e;                               /* poll 回退:登记表即后端状态(重建时只取 armed) */
    return 0;
#endif
}

/* G 内:就绪交付(P3-A 驻留版)——按 armed 吞发,再按三窗口握手唤醒登记者。
 * 不摘登记、不摘后端(one-shot 已被内核消费)、不 free:登记常驻至 fd 关闭。
 * 查无此人/未武装 = 事件无主或已交付(等待者超时/cancel 已醒/批内重复),
 * 吞掉不补投 —— 同时是水平触发残留不致 reactor 空转的开关。 */
static void reactor_deliver_locked(int fd, int dir)
{
    rt_fdwait *e = fdwait_find_locked(fd, dir);
    rt_coro *c;
    if (!e || !e->armed) return;
    e->armed = 0;                          /* one-shot 已消费;重挂只在下一任 wait_fd */
    c = map_get_locked(e->key);
    if (c) coro_wake_locked(c);            /* PARKED→入队;DESCHED/RUNNING→pending;
                                              READY/DONE 吸收(不变量 II) */
#if defined(RT_BACKEND_EPOLL)
    if (!RT_URING()) {
        /* epoll one-shot 按 fd 连坐:本方向交付消费掉了整条注册,反向仍
         * armed 时须以反向掩码重挂,否则反向等待者被本次连带(只能靠超时
         * 醒)。仅同 fd 双向并发等待才发生;kqueue 滤波器按方向独立,无此
         * 路径;io_uring 亦无(POLL_ADD 按方向独立一次性)。 */
        rt_fdwait *o = fdwait_find_locked(fd, dir == RT_FD_WRITE ? RT_FD_READ : RT_FD_WRITE);
        if (o && o->armed) (void)fdwatch_arm_locked(o);
    }
#endif
}

#if defined(RT_BACKEND_KQUEUE)
static void *reactor_main(void *unused)
{
    struct kevent evs[64];
    struct timespec tick;                  /* 100ms 上限:新登记的"注册即就绪"兜底 */
    (void)unused;
    tick.tv_sec = 0;
    tick.tv_nsec = 100 * 1000 * 1000;
    for (;;) {
        int n = kevent(g_rb_fd, NULL, 0, evs, 64, &tick);
        int i;
        if (n <= 0) continue;              /* tick / EINTR:重挂等 */
        rt_lock();
        for (i = 0; i < n; i++)
            reactor_deliver_locked((int)evs[i].ident,
                                   evs[i].filter == EVFILT_WRITE ? RT_FD_WRITE : RT_FD_READ);
        rt_unlock();
    }
}
#elif defined(RT_BACKEND_EPOLL)
static void *reactor_main(void *unused)
{
    struct epoll_event evs[64];
    if (RT_URING()) return reactor_main_uring(unused);   /* T51:io_uring 接管事件循环 */
    (void)unused;
    for (;;) {
        int n = epoll_wait(g_rb_fd, evs, 64, 100);
        int i;
        if (n <= 0) continue;              /* tick / EINTR */
        rt_lock();
        for (i = 0; i < n; i++) {
            int      fd = (int)evs[i].data.u64;
            uint32_t e  = evs[i].events;
            if (e & (EPOLLIN  | EPOLLERR | EPOLLHUP)) reactor_deliver_locked(fd, RT_FD_READ);
            if (e & (EPOLLOUT | EPOLLERR | EPOLLHUP)) reactor_deliver_locked(fd, RT_FD_WRITE);
        }
        rt_unlock();
    }
}
#else /* RT_BACKEND_POLL */
static void *reactor_main(void *unused)
{
    struct pollfd pfds[256];               /* 每轮自登记表重建(去重 fd,合并双向) */
    (void)unused;
    for (;;) {
        int n = 0, j, b, r;
        rt_lock();
        for (b = 0; b < RT_FDMAP_BUCKETS && n < 256; b++) {
            rt_fdwait *e;
            for (e = g_fdmap[b]; e && n < 256; e = e->hnext) {
                /* P3-A 驻留:只 poll armed 登记(one-shot 等价)——交付后的
                 * 驻留旧登记若仍入集,持久就绪会让轮询空转。 */
                if (!e->armed) continue;
                for (j = 0; j < n; j++)
                    if (pfds[j].fd == e->fd) break;
                if (j == n) {
                    pfds[n].fd = e->fd;
                    pfds[n].events = 0;
                    pfds[n].revents = 0;
                    n++;
                }
                pfds[j].events |= (short)(e->dir == RT_FD_WRITE ? POLLOUT : POLLIN);
            }
        }
        rt_unlock();
        r = poll(pfds, (nfds_t)n, 100);    /* 100ms 轮询节奏(登记限制)+ 新登记兜底 */
        if (r <= 0) continue;
        rt_lock();
        for (j = 0; j < n; j++) {
            short rev = pfds[j].revents;
            int   fd  = pfds[j].fd;
            if (!rev) continue;
            if (rev & (POLLIN  | POLLERR | POLLHUP | POLLNVAL))
                reactor_deliver_locked(fd, RT_FD_READ);
            if (rev & (POLLOUT | POLLERR | POLLHUP | POLLNVAL))
                reactor_deliver_locked(fd, RT_FD_WRITE);
        }
        rt_unlock();
    }
}
#endif

/* G 内:惰性启动(首次协程 wait_fd);常驻不 join,与 worker 同口径。
 * 后端/线程创建失败 → abort(与 ctron_rt_init / stack_alloc 惯例一致)。 */
static void reactor_start_locked(void)
{
    if (g_reactor_started) return;
#if defined(RT_BACKEND_KQUEUE)
    g_rb_fd = kqueue();
    if (g_rb_fd < 0) abort();
#elif defined(RT_BACKEND_EPOLL)
# if defined(RT_IOURING_CAP)
    {   /* T51:io_uring 探测定夺(env CTRON_RT_REACTOR 可压制)。探测门 =
         * setup 成功 + NODROP 特性(≥5.5:CQ 溢出内核等待不丢 CQE +
         * POLL_REMOVE 同代);任一不满足 → epoll 回退(交付语义同形,
         * 后端选择仅是实现细节)。env 点名 io_uring 而环境不给 → abort
         * (后端创建失败惯例);点名 epoll → 跳过探测;未设/未知值 → auto。 */
        const char *rv = getenv("CTRON_RT_REACTOR");
        int want = !(rv && *rv && strcmp(rv, "epoll") == 0);
        if (want) {
            struct ctio_params p;
            int fd;
            memset(&p, 0, sizeof p);
            p.flags = CTIO_SETUP_CQSIZE;
            p.cq_entries = 1024;               /* 突发就绪余量(G 内即时收割,防御性) */
            fd = (int)ctio_sys(425, 64, (long)&p, 0, 0);        /* io_uring_setup */
            if (fd >= 0 && (p.features & CTIO_FEAT_NODROP)) {
                /* 映射(liburing 同法):SINGLE_MMAP 族(≥5.4;6.10+ 新布局为其
                 * 强化——SQ/CQ 头交织同一区:SQ head@0/tail@4,CQ head@8/tail@12,
                 * CQ 段嵌在 SQ 区内,array 区尾)只做一次 offset-0 映射,sq/cq 基
                 * 同址;无 SINGLE_MMAP 的旧布局才分立 0x8000000。
                 * 坑位实测(6.10-linuxkit):①mask 是环内偏移,真值要从映射环读
                 * (拿偏移当掩码 → 第二笔 CQE 永远读回 slot0);②array 间接层在
                 * 新布局下行为存疑 → 开机 NOP 自检(下方)不过即回退 epoll。 */
                size_t sq_sz = (size_t)p.sq_off.array + (size_t)p.sq_entries * sizeof(uint32_t);
                size_t cq_sz = (size_t)p.cq_off.cqes + (size_t)p.cq_entries * sizeof(struct ctio_cqe);
                size_t sqe_sz = (size_t)p.sq_entries * sizeof(struct ctio_sqe);
                int single = (p.features & CTIO_FEAT_SINGLE_MMAP) ? 1 : 0;
                size_t ring_sz = single ? (sq_sz > cq_sz ? sq_sz : cq_sz) : sq_sz;
                uint8_t *ring = (uint8_t *)mmap(NULL, ring_sz, PROT_READ | PROT_WRITE,
                                                MAP_SHARED | MAP_POPULATE, fd, 0);
                if (single) {
                    g_sq_ring = ring;
                    g_cq_ring = ring;
                } else if (ring != MAP_FAILED) {
                    ring_sz = sq_sz;
                    g_sq_ring = ring;
                    g_cq_ring = (uint8_t *)mmap(NULL, cq_sz, PROT_READ | PROT_WRITE,
                                                MAP_SHARED | MAP_POPULATE, fd, 0x8000000ULL);
                } else {
                    g_cq_ring = MAP_FAILED;
                }
                g_sqes = (struct ctio_sqe *)mmap(NULL, sqe_sz, PROT_READ | PROT_WRITE,
                                                 MAP_SHARED | MAP_POPULATE, fd, 0x10000000ULL);
                if (ring != MAP_FAILED && g_cq_ring != MAP_FAILED && g_sqes != MAP_FAILED) {
                    uint32_t i;
                    for (i = 0; i < p.sq_entries; i++)        /* sqarray 恒等初值 */
                        ((uint32_t *)(g_sq_ring + p.sq_off.array))[i] = i;
                    g_sq_off = p.sq_off;
                    g_cq_off = p.cq_off;
                    /* ring_mask 是环内偏移,真值在映射环里(liburing 同法)——
                     * 拿偏移当掩码会让第 2 个 CQE 永远读回 slot0(实测在案) */
                    g_sq_mask = *(uint32_t *)(g_sq_ring + p.sq_off.ring_mask);
                    g_cq_mask = *(uint32_t *)(g_cq_ring + p.cq_off.ring_mask);
                    g_uring_fd = fd;
                    if (ctio_selftest() == 0) {
                        g_rb_uring = 1;        /* 定性后只读:arm/deliver/forget 谓词分支 */
                    } else {
                        /* 自检不过(内核 ABI 不在预期形态):回退 epoll,契约不破 */
                        munmap(g_sqes, sqe_sz);
                        munmap(g_sq_ring, ring_sz);
                        if (!single) munmap(g_cq_ring, cq_sz);
                        close(fd);
                        g_sq_ring = NULL; g_cq_ring = NULL; g_sqes = NULL; g_uring_fd = -1;
                    }
                } else {
                    if (ring != MAP_FAILED) munmap(g_sq_ring, ring_sz);
                    if (!single && g_cq_ring != MAP_FAILED) munmap(g_cq_ring, cq_sz);
                    if (g_sqes != MAP_FAILED) munmap(g_sqes, sqe_sz);
                    close(fd);
                    g_sq_ring = NULL; g_cq_ring = NULL; g_sqes = NULL;
                }
            } else if (fd >= 0) {
                close(fd);                     /* 特性门不过(5.1–5.4):epoll 回退 */
            }
        }
        if (g_rb_uring == 0 && rv && *rv && strcmp(rv, "io_uring") == 0) {
            /* 点名 io_uring 而自检不过:内核在位但 ABI 不合本运行时预期 ——
             * 响亮登记后回退 epoll(同 IOCP 无靶机口径,不静默不硬炸;
             * 探测失败细节见 docs/simd-vectorization-analysis.md T51 段) */
            static const char msg[] = "ctron_rt: io_uring 点名但自检不过(内核 ABI 不合预期),回退 epoll(T51 登记)\n";
            write(2, msg, sizeof msg - 1);
        }
    }
# endif
    if (g_rb_uring == 0) {
        g_rb_fd = epoll_create1(0);
        if (g_rb_fd < 0) abort();
    }
#endif
    if (pthread_create(&g_reactor_th, NULL, reactor_main, NULL) != 0) abort();
    g_reactor_started = 1;                 /* pthread_create 先行 ⇒ reactor_main 读
                                              g_rb_fd 已同步可见 */
}

/* ───────────────────────── 冻结 API ───────────────────────── */
/* ── T34 最小符合径:栈触顶 panic(§7.1;guard 页命中 → 明确诊断后终止)──
 * 经 g_all 全体链定位 fault 地址所属协程栈;非协程栈(主线程)链回默认处置。
 * async-signal-safe 面:write/_exit/链遍历(只读,致命路径容忍竞态窗口)。 */
static void rt_segv_handler(int sig, siginfo_t *si, void *uc);

static void rt_altstack_install(void)
{
    stack_t ss;
    struct sigaction sa;
    ss.ss_sp = malloc(SIGSTKSZ);
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;
    sigaltstack(&ss, 0);
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = rt_segv_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, 0);
    sigaction(SIGBUS, &sa, 0);
}

static void rt_segv_handler(int sig, siginfo_t *si, void *uc)
{
    void *fault = si->si_addr;
    const rt_coro *c = g_all;
    (void)sig; (void)uc;
    while (c) {
        if (c->stack && (const char *)fault >= (const char *)c->stack &&
            (const char *)fault < (const char *)c->stack + c->stack_map_sz) {
            static const char msg[] =
                "ctron_rt: task stack overflow(guard 命中,触顶;§7.1/T34)\n";
            write(2, msg, sizeof msg - 1);
            _exit(101);
        }
        c = c->allnext;
    }
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    raise(sig == SIGBUS ? SIGBUS : SIGSEGV);
}

void ctron_rt_init(int workers)
{
    int i;
    rt_lock();
    if (g_inited) { rt_unlock(); return; }           /* 幂等 */
    rt_altstack_install();   /* T34:主线程侧安装(worker 入口各自再装) */
#if defined(__linux__)
    {   /* P3-A:idle cv 时基 = CLOCK_MONOTONIC(绝对时限 timedwait 用;
         * darwin 走相对时限变体,用静态初始化 cv,无需此处初始化) */
        pthread_condattr_t ca;
        if (pthread_condattr_init(&ca) != 0) abort();
        if (pthread_condattr_setclock(&ca, CLOCK_MONOTONIC) != 0) abort();
        if (pthread_cond_init(&g_idle_cv, &ca) != 0) abort();
        pthread_condattr_destroy(&ca);
    }
#endif
    {   /* P2-E:CTRON_RT_SEED 非空且全串可解析 → 种子模式(单 worker +
         * 种子化弹出序);空串/含尾随垃圾 → 非种子模式(默认行为)。 */
        const char *e = getenv("CTRON_RT_SEED");
        if (e && *e) {
            char *end = NULL;
            unsigned long v = strtoul(e, &end, 10);
            if (end && *end == '\0') {
                g_seed_mode = 1;
                g_seed_state = (uint64_t)v;
            }
        }
    }
    {   /* T51 §9.5:NUMA 选项位(CTRON_RT_NUMA)。v0 只探测登记不改行为
         * (off/on/auto 值面先立;感知分配/亲和=志向),观测口
         * ctron_rt_numa_nodes()。 */
        const char *e = getenv("CTRON_RT_NUMA");
        if (e && strcmp(e, "on") == 0) {
            /* on 点名:节点数 >1 的亲和/分配行为属志向,当前仅登记于观测口 */
        }
        rt_numa_probe();
    }
    if (workers <= 0) {
        long n = sysconf(_SC_NPROCESSORS_ONLN);
        if (n <= 0) n = 2;
        workers = (int)(n < 4 ? n : 4);              /* min(cpu 核数, 4) */
    }
    if (workers > RT_MAX_WORKERS) workers = RT_MAX_WORKERS;
    if (workers < 1) workers = 1;
    if (g_seed_mode) workers = 1;                    /* 种子契约:单 worker
                                                        (多 worker 本身即非确定源;压过调用方/env 的任何 worker 数) */
    g_nworkers = workers;
    for (i = 0; i < workers; i++) {
        g_workers[i].idx = i;                        /* T34:steal 轮转起点(init 写定,worker 未见) */
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
    rt_ctx_init(&c->ctx, c->stack + rt_pagesize(), rt_stack_size(), rt_tramp);

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
    /* P2-E spawn 闸开闸点:裸线程(main)进 join = spawn 风暴终点信号。
     * 在所有早退路径之前置位(闸开前无可协程已完成,首个 join 必达此处)。 */
    if (g_seed_mode && !rt_tls_cur()) g_gate_open = 1;
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

/* 绝对 deadline 停车(T10):armed+入堆+park;早醒(取消广播/虚假唤醒)
 * 不内循环——调用方以 now vs deadline 判到期(ctron_net_sleep_ns 契约)。 */
void ctron_rt_sleep_until(uint64_t deadline)
{
    if (!rt_tls_cur()) {
        uint64_t now = now_ns();
        if (now < deadline) {
            uint64_t rel = deadline - now;
            struct timespec ts = { (time_t)(rel / 1000000000ull), (long)(rel % 1000000000ull) };
            while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
                ;
        }
        return;
    }
    rt_coro *self = rt_tls_cur();
    rt_lock();
    self->armed = 1;
    self->arm_seq++;
    heap_push(deadline, self, self->arm_seq);
    rt_unlock();
    coro_suspend(DESCHED_PARK);
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
    int dir = write_side ? RT_FD_WRITE : RT_FD_READ;

    if (fd < 0) return;
    if (!rt_tls_cur()) {                   /* 裸线程:无 park,直接 poll 等就绪/超时
                                              (契约同:醒后由调用方重试 syscall;
                                              revents 不看——重试自见可读/可写/错误) */
        struct pollfd p;
        int64_t t = timeout_ms;
        int r;
        if (t > 2147483647) t = 2147483647;
        memset(&p, 0, sizeof p);
        p.fd = fd;
        p.events = (short)(dir == RT_FD_WRITE ? POLLOUT : POLLIN);
        do {
            r = poll(&p, 1, (int)(t < 0 ? -1 : t));
        } while (r < 0 && errno == EINTR); /* EINTR 重启整个时限(登记限制) */
        return;
    }
    if (timeout_ms == 0) return;           /* 非阻塞探测:登记无意义,调用方 syscall 即判 */
    {
        rt_coro *self = rt_tls_cur();
        rt_fdwait *e;
        int fresh = 0;
        rt_lock();
        self->armed = 0;                   /* 卫生:作废残留旧世代定时器(登记 Minor:
                                              sleep_ms 提前醒路径遗留 armed 堆条目;
                                              仅限本函数入口,不触碰 sleep_ms 契约) */
        reactor_start_locked();
        /* P3-A 兴趣驻留:登记幂等 —— 命中即复用 entry(常驻至 fd 关),未命中
         * 才建新;顺手清扫桶内 fd 已关的僵尸登记(直连 close(2) 的残留)。 */
        e = fdwait_find_locked(fd, dir);
        if (!e) {
            fdwait_sweep_bucket_locked(fd_bucket(fd, dir));
            e = (rt_fdwait *)calloc(1, sizeof *e);
            if (!e) { rt_unlock(); return; }   /* ENOMEM 降级:立即返回,调用方重试 */
            e->fd = fd;
            e->dir = dir;
            e->hnext = g_fdmap[fd_bucket(fd, dir)];
            g_fdmap[fd_bucket(fd, dir)] = e;
            fresh = 1;
        }
        /* last-wins:同 (fd,dir) 重复 wait_fd 顶替 key(P2 语义不变:被顶者
         * 不补 wake——防两协程互顶活锁;靠自身超时/取消/后续就绪醒。驻留下
         * "后续就绪" = 新一轮武装后的交付,文件头注 P3-A 已登记)。 */
        e->key = self->key;
        e->armed = 1;
        if (fdwatch_arm_locked(e) != 0) {
            /* 后端拒绝(fd 已坏):不 park,让调用方重试 syscall 直接见错;
             * 新建登记即摘(坏 fd 无驻留价值),旧登记留待清扫/复用。 */
            e->armed = 0;
            if (fresh) free(fdwait_take_locked(fd, dir));
            rt_unlock();
            return;
        }
        if (timeout_ms > 0) {              /* 超时复用定时器堆(arm_seq 世代防串扰):
                                              到点统一 wake 路径醒,调用方重试自见超时 */
            self->armed = 1;
            self->arm_seq++;
            heap_push(now_ns() + (uint64_t)timeout_ms * 1000000ull, self, self->arm_seq);
        }
        rt_unlock();
        coro_suspend(DESCHED_PARK);        /* 三窗口握手(文件头注不变量 II):
                                              登记与后端武装均在 park 前 G 内完成 ⇒
                                              reactor 交付只能命中 提前醒(pending 消费,
                                              fd 已就绪)/ DESCHED(completer 回队)/
                                              PARKED(直接入队),无丢唤醒窗口 */
        /* 醒来(就绪/超时/cancel/粘滞误醒一视同仁):驻留不摘登记 —— 仍属本
         * 协程则解除武装(未消费的 one-shot 不再指向本协程,迟到的水平触发
         * 事件被吞,下一任等待者重新武装);被顶替的登记不碰(属后来者)。 */
        rt_lock();
        {
            rt_fdwait *mine = fdwait_find_locked(fd, dir);
            if (mine && mine->key == self->key) mine->armed = 0;
        }
        rt_unlock();
    }
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

/* P3-A 兴趣驻留配套:fd 关闭钩子 —— 摘除该 fd 两方向的驻留登记。
 * ctron_net_close 在 close 之后调用(内核已在 close 时自动摘 knote/epoll
 * 节点,只须清登记表内存,不触碰后端);直连 close(2) 的路径由注册时顺手
 * 桶清扫兜底。未知 fd → no-op。同号复用 ABA 窗口(close→本钩子之间新 fd
 * 同号注册被误摘;带超时等待者靠自身超时兜底,无超时停车者(如 TLS 握手
 * timeout=0)不受兜底——挂死形态,结构性消除见 P4 首件(forget_fd 先于
 * close 重排))与 P2 头注 F_GETFD 残留窗口同类,登记(见文件头注)。 */
void ctron_rt_forget_fd(int64_t fd64)
{
    int fd = (int)fd64;
    int d;
    if (fd < 0) return;
    rt_lock();
    for (d = 0; d < 2; d++)
        free(fdwait_take_locked(fd, d));
#if defined(RT_IOURING_CAP)
    if (RT_URING()) ctio_forget_polls_locked(fd);   /* io_uring 不随 close 自动摘:显式取消 */
#endif
    rt_unlock();
}

/* T51 §9.5 观测口:reactor 后端名(kqueue/io_uring/epoll/poll;未启动 =
 * "none")。冒烟与诊断消费,冻结接口新增不改旧。 */
const char *ctron_rt_reactor_name(void)
{
    if (!g_reactor_started) return "none";
#if defined(RT_BACKEND_KQUEUE)
    return "kqueue";
#elif defined(RT_BACKEND_EPOLL)
    return RT_URING() ? "io_uring" : "epoll";
#else
    return "poll";
#endif
}

/* T51 §9.5 NUMA 选项位:CTRON_RT_NUMA=off|on|auto(缺省 auto;v0 语义 =
 * 拓扑探测登记,行为无差 —— 感知分配/自动亲和为志向,见
 * docs/simd-vectorization-analysis.md)。观测口 ctron_rt_numa_nodes():
 * linux 探测在线节点数(/sys/devices/system/node/nodeN),其他平台恒 1。 */
static int g_numa_nodes = 1;

int ctron_rt_numa_nodes(void)
{
    return g_numa_nodes;
}

static void rt_numa_probe(void)
{
#if defined(__linux__)
    DIR *dp = opendir("/sys/devices/system/node");
    struct dirent *de;
    int n = 0;
    if (!dp) return;                           /* 无拓扑面(容器/老内核):恒 1 */
    while ((de = readdir(dp)) != NULL) {
        const char *s = de->d_name;
        if (strncmp(s, "node", 4) == 0) {
            int ok = 0;
            for (s += 4; *s; s++)
                if (*s < '0' || *s > '9') break;
                else ok = 1;
            if (ok && *s == '\0') n++;
        }
    }
    closedir(dp);
    if (n > 0) g_numa_nodes = n;
#else
    (void)0;
#endif
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

/* T34 work-stealing 观测口:worker 间偷取累计次数(仅观测,不参与任何契约;
 * 种子模式恒 0)。加法导出,冻结接口不动。 */
int64_t ctron_rt_steals(void)
{
    return atomic_load_explicit(&g_steals, memory_order_relaxed);
}

/* ───────────────────────── P7-F /debug/scopes 快照 ─────────────────────────
 * 形态:"[[entries],[wait_edges]]"——entries = [{i,s}](i = g_all 走链序,
 * LIFO:最新 spawn 在 0;s = ready/running/desched/parked/done);wait_edges =
 * [waiter_i, target_j](join 方向:waiter 挂在 target 的 jnext 链上)。
 * rt 锁内走链快照(与 cancel_wake_all 同锁形);静态缓冲,串行调用方口径
 * (serial 模式 g_all 空 → "[[],[]]")。 */
const char *ctron_rt_scopes_json(void)
{
    static char buf[16384];
    static const char *stname[] = { "", "ready", "running", "desched", "parked", "done" };
    rt_coro *all[256];
    rt_coro *c, *w;
    int total = 0, i, j, n, first = 1;
    rt_lock();
    for (c = g_all; c && total < 256; c = c->allnext)
        all[total++] = c;
    n = snprintf(buf, sizeof buf, "[[");
    for (i = 0; i < total; i++) {
        int st = all[i]->state;
        if (st < 1 || st > 5) st = 1;
        n += snprintf(buf + n, sizeof buf - (size_t)n, "%s{\"i\":%d,\"s\":\"%s\"}",
                      i ? "," : "", i, stname[st]);
        if (n >= (int)sizeof buf - 2) { rt_unlock(); return buf; }
    }
    n += snprintf(buf + n, sizeof buf - (size_t)n, "],[");
    for (j = 0; j < total; j++) {
        for (w = all[j]->jnext; w; w = w->jnext) {
            for (i = 0; i < total; i++) {
                if (all[i] == w) {
                    n += snprintf(buf + n, sizeof buf - (size_t)n, "%s[%d,%d]",
                                  first ? "" : ",", i, j);
                    first = 0;
                }
            }
        }
    }
    n += snprintf(buf + n, sizeof buf - (size_t)n, "]]");
    rt_unlock();
    return buf;
}
