# 服务器泳道 P2 异步内核实施计划(reactor + 协程 + 同形切换)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development(既定模式,用户已选)按任务执行。Steps 用 checkbox 跟踪。

**Goal:** 落地路线 P2——`ctron_rt` 协程运行时(ucontext + 定时器轮 + kqueue/epoll reactor)、net 垫片混合化(协程上无色挂起/裸线程回退 P1 阻塞)、发射模板模式分支(CTRON_RT=coro),出口 = 6 夹具 coro 模式全绿 + ctecho 源码零改动跑协程运行时 + 确定性 1000 种子 + 切换微基准 ≤200ns + echo p50 vs P1 ≤1.15×(C10K 本地/nightly)。

**Architecture:** rt 只做**调度基建**(不碰任务状态契约):模板侧 ct_task/ct_shim_tramp/state/pmsg 原样,rt 以 opaque key 等待/唤醒。切换机制 = **weak extern**:模板与垫片对 `ctron_rt_*` 一律 weak 声明,未链接 ctron_rt.c 时符号为 NULL → 默认 pthread 路径逐字节不变(零链接冲击、非 net 程序零感知);`CTRON_RT=coro` 且已链接时改道。reactor v1 = kqueue(darwin)+ epoll(linux)+ poll()(windows 回退,IOCP 列后)。协程栈 ucontext 64KB mmap + 池化复用。确定性模式 = 单调度线程 + 种子化就绪序。

**Tech Stack:** C ucontext/kqueue/epoll、driver_emit.ct 模板小改(模式分支)、现有 tests/net/run.sh 环境矩阵。

## Global Constraints

- pathspec/hunk 提交纪律;并行机刷泳道共享工作树,目标文件先 `git status`,脏则 hunk 暂存(git apply --cached / hash-object 法,前波两用例在台账)。
- 默认路径**逐字节不变**:driver_emit.ct 修改后跑 `compiler/native.sh && compiler/test/smoke.sh` 固定点/冒烟必须与改前一致(全绿;他泳道既有红按台账甄别)。任何改变默认发射产物的改动 = 打回。
- 模板/垫片注释**无花括号**(Ctron 注释纪律对模板字符串同样适用——模板行本身是 C 代码,花括号在 C 里合法;仅 Ctron 源注释禁)。
- extern 形参白名单继续有效(标量/Str/视图/Box 按值);rt 全部为 C 层 API,不进 Ctron 面(P2 不新增 Ctron 可见 API)。
- CI 回环纪律;C10K/微基准不入 CI 主环(CTRON_NET_BENCH / nightly 标记)。
- §7.10 差距声明继续有效:64KB 固定栈为过渡口径,不外推 C100K。

---

### Task 1 (P2-A): ctron_rt 协程核心 + 单元冒烟(纯 C,先于一切集成)

**Files:**
- Create: `std/net/c_src/ctron_rt.h`、`std/net/c_src/ctron_rt.c`
- Create: `tests/net/rt_core_smoke/{c_src/ctron_rt.c → 符号链接, c_src/ctron_net.c → 不链接!本夹具只链 rt —— c_src/ 放 rt 一个符号链接 + src/main.c(纯 C main,非 Ctron)}`

注意:tests/net/run.sh 只处理含 `src/main.ct` 的目录;rt_core_smoke 用 **C main**,由本任务自带 `tests/net/rt_core_smoke/run.sh`(build+run,断言输出)驱动,不进 run.sh 主环。

**Interfaces(P2-C/D/E 全依赖,冻结):**

```c
/* ctron_rt.h —— 调度基建;task/chan 以 opaque key(调用方私有指针)挂接 */
void    ctron_rt_init(int workers);            /* workers<=0 → cpu 核数上限 4;幂等 */
int     ctron_rt_active(void);                 /* getenv("CTRON_RT")=="coro" 且已 init;结果缓存 */
void*   ctron_rt_current(void);                /* 当前协程 key;裸线程/未 init → NULL */
void*   ctron_rt_run(void (*fn)(void*), void* arg, void* key);
        /* 建协程跑 fn(arg);key=调用方私有(模板传 ct_task*);返回 key */
void    ctron_rt_join_key(void* key);          /* 挂起当前协程直到 key 协程结束;裸线程 → 自旋等待 state 由调用方负责(模板侧不走此路) */
void    ctron_rt_notify_done(void* key);       /* key 协程体结束前由包装器回调:唤醒 join 等待者 */
void    ctron_rt_park(void);                   /* 挂起当前协程,直到被 wake/超时/reactor 就绪 */
void    ctron_rt_wake(void* key);              /* 唤醒因 park 挂起的协程 */
void    ctron_rt_sleep_ms(int64_t ms);         /* 定时器轮;裸线程回退 nanosleep */
void    ctron_rt_wait_fd(int fd, int write_side, int64_t timeout_ms);
        /* 向 reactor 注册就绪兴趣并 park;超时/错误返回后由调用方重试 syscall */
void    ctron_rt_cancel_wake_all(void);        /* scope 取消广播时唤醒全部停车协程 */
int64_t ctron_rt_yield_bench(int rounds);      /* 微基准:rounds 次 yield 配对,返回 ns */
```

协程体包装:`ctron_rt_run` 内部 makecontext 执行 `tramp`:先 `fn(arg)`,后 `ctron_rt_notify_done(key)`,再永久 park(不可返回到死栈)。栈 64KB mmap + 空闲池(上限 256)复用;guard 记录高水位(登记用,不强制)。

**Steps:**
- [ ] 写 ctron_rt.h/.c(ucontext: getcontext/makecontext/swapcontext;调度器 = workers 线程 × 就绪双队列 + 定时器最小堆;互斥保护;TLS 当前协程;wait_fd 的 fd→key 注册表先留空实现——reactor 任务接入)
- [ ] rt_core_smoke/src/main.c:C 冒烟——两协程 ping-pong 各 1000 次(顺序断言)、join_key 等待、park/wake 配对、sleep_ms(20) 实测 ≥15ms、yield_bench(100000) 打印 ns/次
- [ ] `tests/net/rt_core_smoke/run.sh`:cc -O1 链 ctron_rt.c + main.c(pthread 链)-o app && ./app;断言 ping-pong 顺序与 sleep 下界,打印 yield ns/次
- [ ] 跑通;记录 yield ns/次 到报告(≤200ns 为 P2-G 门禁预演)
- [ ] pathspec 提交(std/net/c_src/ctron_rt.{h,c} + tests/net/rt_core_smoke/**)

**验收:** 冒烟全绿;yield ns/次 报告登记;默认路径未触碰(driver_emit.ct 未改,smoke.sh 不必跑)。

---

### Task 2 (P2-B): reactor(kqueue/epoll/poll 回退)+ wait_fd 接入

**Files:**
- Modify: `std/net/c_src/ctron_rt.c`(reactor 线程 + fd 注册表)
- Create: `tests/net/rt_reactor_smoke/{c_src 符号链接, src/main.c, run.sh}`

**Interfaces(在 P2-A 基础上补):** `ctron_rt_wait_fd` 真实现——reactor 专用后台线程:kqueue(darwin)/epoll(linux)/poll 循环(windows,100ms 轮询回退);fd+方向 → key 注册表(互斥);就绪 → wake(key)。ET 演进不需要(水平触发即可,one-shot+rearm 列 P9)。

**Steps:**
- [ ] reactor 实现 + wait_fd 接入(park 前注册,park 后注销)
- [ ] rt_reactor_smoke/src/main.c:协程 A 对 socketpair 写端等可写、协程 B 读端等可读;B 先 park 等 A 写;双向数据断言;关闭 fd 唤醒路径断言
- [ ] run.sh 跑通;pathspec 提交
- [ ] 验收:双 smoke 绿;默认路径仍未触碰

---

### Task 3 (P2-C): net 垫片混合化(协程无色挂起 / 裸线程 P1 回退)

**Files:**
- Modify: `std/net/c_src/ctron_net.c`(read_t/write/shutdown 前的写等待/sleep_ms/udp_recvfrom 五处停车点)
- Create: `tests/net/coro_hybrid/{c_src 双符号链接(ctron_net.c+ctron_rt.c), src/main.ct, run.sh 并入主环}`

**Interfaces:** `ctron_net.c` 顶部 `__attribute__((weak)) extern` 声明 `ctron_rt_current/ctron_rt_wait_fd/ctron_rt_sleep_ms`(未链 rt → NULL)。停车点改造(每处同一模式):

```c
/* read_t 为例:循环内 EWOULDBLOCK/EAGAIN 分支 */
if ((rc < 0) && would_block()) {
    if (rt_wait_fd) { rt_wait_fd(fd, 0, timeout_ms); continue; }
    /* 裸线程:P1 poll() 原路径,逐字节不变 */
}
```

`ctron_net_sleep_ms`:`rt_sleep_ms && ctron_rt_current() ? rt_sleep_ms(ms) : 原 nanosleep`。weak 取址在未链接时为 NULL(ELF/Mach-O 皆然)——夹具矩阵实证。

**Steps:**
- [ ] 五处停车点按模式改造;未链 rt 的既有 6 夹具必须零变化(回归:tests/net/run.sh 全绿,行为逐字节同)
- [ ] coro_hybrid 夹具(main.ct):协程内 socketpair 式回环读写 + sleep——通过 Ctron 层 `scope spawn` 走 rt(coro 模式),断言数据与顺序;run.sh 环境:`CTRON_RT=unset`(裸线程路径)与 `CTRON_RT=coro`(协程路径)双跑
- [ ] tests/net/run.sh 增 coro_hybrid(内含双环境);全绿;pathspec 提交
- [ ] 验收:旧 6 夹具回归零变化 + coro_hybrid 双环境绿

---

### Task 4 (P2-D): 发射模板模式分支(weak extern + CTRON_RT 改道)

**Files:**
- Modify: `compiler/src/driver_emit.ct`(并发模板区:ct_spawn/ct_join/ct_join_or/ct_ch_send/ct_ch_recv/ct_cancel_broadcast/ct_shim_tramp 尾部通知 七个触点)

**Interfaces(模板侧新增,全部 weak 声明,置于 ct_task 定义后):**

```c
extern int ctron_rt_active(void) __attribute__((weak));
extern void* ctron_rt_run(void (*fn)(void*), void* arg, void* key) __attribute__((weak));
extern void ctron_rt_join_key(void* key) __attribute__((weak));
extern void ctron_rt_park(void) __attribute__((weak));
extern void ctron_rt_wake(void* key) __attribute__((weak));
extern void ctron_rt_notify_done(void* key) __attribute__((weak));
extern void ctron_rt_cancel_wake_all(void) __attribute__((weak));
```

七触点改法(默认路径逐字节保留):
1. `ct_spawn`:calloc ct_task 后 `if (ctron_rt_active && ctron_rt_active()) { ctron_rt_run(ct_shim_tramp, t, (void*)t); return t; }` else 原 pthread_create。
2. `ct_shim_tramp`:`t->shim(t->env); t->state = 1;` 后补 `if (ctron_rt_notify_done) ctron_rt_notify_done((void*)t);`(裸线程下 notify_done 为 NULL → 无操作;协程下 rt 收尾唤醒 join 者;panic 路径 longjmp 后同样经 state=2 由 join 侧处理——notify 亦须在 setjmp 捕获后统一触发,实现者按「state 置位后必 notify」语义落)。
3. `ct_join/ct_join_or`:`pthread_join` 换为 `if (coro) ctron_rt_join_key((void*)t) else pthread_join(...)`;其后 state/result/pmsg 逻辑共用不动。
4. `ct_ch_send/ct_ch_recv` 的 cond_wait 处:`if (coro) { ctron_rt_park(); continue; }`(continue 后重查 cnt/cancelled——循环已有 while,语义自然闭合)else 原 cond_wait。注意先解锁再 park、醒来再上锁(模板侧按现有锁序落,park 前必须 unlock mu)。
5. `ct_cancel_broadcast`:末尾补 `if (ctron_rt_cancel_wake_all) ctron_rt_cancel_wake_all();`。

**Steps:**
- [ ] 模板七触点修改(逐字保留默认分支)
- [ ] **固定点门禁**:`compiler/native.sh && compiler/test/smoke.sh` 全绿且与改前同数(他泳道既有红按台账甄别;发射产物 diff 抽验一个简单程序改前改后逐字节同——模板仅在运行期分支,产物应含新增 weak 声明行,产物 diff 允许=新增声明行,其余逐字节同)
- [ ] tests/net 全套 + caps_net 回归(默认路径)
- [ ] **coro 矩阵**:`CTRON_RT=coro` 跑 tcp_echo/clock_sanity/coro_hybrid/tests/06_concurrency.ct(经 ctron-cc 解释口径不行——06 套件走 emit 需 c_src……解释口径无 rt;改为:tests/net 三夹具 + 新增 coro_conc 夹具(channel 往返 + spawn/join + scope 取消)coro 模式跑)
- [ ] ctecho **源码零改动**:`CTRON_RT=coro` 下 run.sh 冒烟 3/3(同形不变式机械验证)
- [ ] pathspec 提交(driver_emit.ct hunk 隔离——该文件是编译泳道热区,先 git status)
- [ ] 验收:固定点不破 + 默认回归绿 + coro 矩阵绿 + ctecho 同形绿

---

### Task 5 (P2-E): 确定性调度器 + 1000 种子

**Files:**
- Modify: `std/net/c_src/ctron_rt.c`(CTRON_RT_SEED → 单 worker + 种子化就绪序)
- Create: `tests/net/coro_det/{src/main.ct, c_src 链接, run.sh}`

**语义:** SEED 非空 → workers=1,就绪队列弹出位由种子 LCG 决定(多元素时按 seed 派生序),yield/park 时序可复现;断言面:coro_det 夹具(channel 多生产者消费者)在**同种子两次运行输出逐字节同**、不同种子允许不同。1000 种子循环(run.sh for i in $(seq 1000) 抽 100 于 CI、全量 nightly)同种子重放全绿。

**验收:** 同种子逐字节重放实证(diff 两跑输出);CI 抽 100 种子绿。

---

### Task 6 (P2-F): 门禁三件 + 登记收口

**Files:**
- Create: `tests/net/c10k/{src/main.ct, c_src, run.sh}`(本地/nightly,不入 CI 主环)
- Modify: `tests/net/bench/`(协程切换微基准脚本化 + echo p50 对比模式)
- Modify: `tests/COVERAGE.md`、`docs/c-rust-divergences.md`、计划执行记录

**门禁:**
1. **C10K**:coro 模式 ctecho 拓扑(本地起服务)+ 10k 并发连接各一轮回显;通过判据 = 全部成功 + 无 fd 泄漏。macOS 本地先跑;ulimit -n 提示写入脚本。
2. **切换微基准** ≤200ns:rt_core_smoke 的 yield_bench 脚本化登记(裸 swapcontext 配对口径)。
3. **echo p50 vs P1 ≤1.15×**:bench.sh 加 CTRON_RT=coro 维度(ctecho coro vs P1 阻塞 ctecho,同协议)。
4. 登记:COVERAGE P2 行、divergences(r7b 不动;若 rt 暴露新发射面事实随记)、计划执行记录 P2 出口判定。

**验收:** 三门禁数字落执行记录;C10K 标注 nightly;run.sh 主环不涨时长(门禁均标记跳过)。

---

## Self-Review

- 设计覆盖:P2 全部出口项(同形/确定性/C10K/微基准/p50)映射 P2-D/E/F;无色化=P2-C;取消传播=P2-D 触点 4/5;§7.10 同形契约=ctecho 零改动机械验证。
- 架构风险前置:weak extern 未链接行为(ELF/Mach-O)=P2-C 夹具矩阵实证;固定点门禁=P2-D 硬门禁;模板是编译泳道热区=hunk 隔离纪律。
- 明确不做(P2 外):IOCP 真实现(poll 回退)、可增长栈(§7.1 完全符合=P9)、work-stealing、Ctron 面 API 变化、R 线协程口径(解释器无 rt,登记)。

---

## 执行记录

### 任务台账(2026-09-20;逐任务细节见 .superpowers/sdd/p2-task-*-report.md)

- **Task 1 (P2-A)** ad43419..23e5ba5:ctron_rt 核心。**架构变更:弃 ucontext**——darwin/arm64 弃用面在标准 N:M 用法(2 worker × 互斥队列 × 协程迁移)确定性崩溃,改自绘切换(SysV callee-saved + sp/pc,arm64/x86_64 双汇编);TLS 幽灵 resume → noinline 访问器;yield_bench 87–100ns(门 200ns 预演)。
- **Task 2 (P2-B)** 23e5ba5..56b2295(+90b9c97 主环守卫):reactor(kqueue/epoll/poll 回退,水平触发 100ms 兜底)+ wait_fd;三窗口握手保形;顺修高水位谓词反置 + worker 空队迭代吞 FOREVER 回收。
- **Task 3 (P2-C)** 6cfd0ef:垫片五停车点无色挂起(裸线程回退逐字节不变)。**Mach-O 无 undefined-weak**(链接期报 undefined,ELF 落 NULL)→ 架构定案 = 弱定义哑元 + 强定义顶替;errno 槽迁移串块 → noinline 访问器。
- **Task 4 (P2-D)** 66abe59 + d2ccb03(并行泳道残片预修)+ e93bde9(WIN32 必修):模板模式分支七触点;TLS 直访全量收口(13 处);chan 成功路径 wake-all;固定点防火墙源级实证;**WIN32 九哑元无条件发射**(#if 裁切留存活符号引用致 mingw 链接断);ctecho 源码零改动双模 3/3(§7.10 同形)。
- **Task 5 (P2-E)** 55c9032:CTRON_RT_SEED = 单 worker + 就绪 LCG 抽取 + spawn 闸(裸线程首 join 前不弹——就绪集非定纯抽取不可救);coro_det 夹具(2 生产×2 消费争用 + cap-32 日志通道调度指纹)。
- **Task 6 (P2-F)** 本批:门禁三件脚本化 + 登记收口。落地:tests/net/c10k/(driver.c + run.sh,nightly);bench.sh 三门禁段(rt 微基准 + coro 维度);coro_det/run.sh 接入 net 主环专属块(审查 Important:此前无人跑 = 确定性无回归保护),主环 11→12 例;证据计数修正 102→101(cmp 对口径);rt.c 头注契约补句(spawn 闸单向,开闸后裸线程再 spawn 无声重引入风暴竞态)。

### P2 出口判定(门禁逐项,2026-09-20 实测)

| 门禁 | 口径 | 实测 | 判定 |
|---|---|---|---|
| 同形(§7.10) | ctecho 源码零改动 CTRON_RT=coro | net 主环默认 12/12 + coro 矩阵 12/12(rc=0);ctecho 双模 3/3(Task 4 实证) | 绿 |
| 确定性 | 同种子双跑逐字节 cmp | coro_det:SEED=42 ×1 对 + 0..99 ×100 对 = **101/101 全绿**(多轮复验);异种子(7/99)/无种子完成性绿 | 绿 |
| C10K(nightly) | coro ctecho + N 并发各一轮回显 + 存活/探活 | **10000/10000 全绿**(connect 0.4s,total 1.1s;探活绿);CI 冒烟档 C10K_N=100 绿 | 绿 |
| 切换微基准 | yield_bench(100000) ns/yield ≤200 | 94.1 / 100.1 / 102.3 / 89.4 ns(四跑,arm64 原生;Rosetta 翻译态豁免在册) | 绿 |
| echo coro-vs-P1 | ctecho 同源码双二进制,≤1.15× | **2.203 / 2.073 / 2.182(三跑)——超门** | **红,登记归因** |

- **门禁三红归因**(非 harness 偏差:同客户端驱动、同源码双二进制、同 -O1、同 §11.3 默认面,唯一差异 = 运行时模式):coro 侧每阻塞读 = 0 超时 poll 探针 + reactor 登记/摘除(kqueue EV_ADD/EV_DELETE + F_GETFD)+ park/wake 切换对 + worker 空闲退避唤醒延迟(20→160µs 全局递增),合计 ~15µs/往返;P1 侧 = 单 poll 门 + recv(~13µs/往返)。门面共担成本(per-read poll 门 + 4KB 暂存 + lane 加宽,P1 在册归因)两端同担,不放大该比值。
- **处置**:登记归因 + P3 优化项:(a) worker 唤醒改事件量/退避随唤醒交付复位;(b) kqueue 兴趣驻留 + one-shot rearm(已列 P9);(c) 视图直收绕过 4KB 暂存(P1 在册优化项同源)。**P2 出口 = 四绿一红,红项在册不粉饰**;运行时性能优化不属本收口任务范围,另行开题。
- bench.sh 退出码口径统一(偏差注):P1 原形 ratio>1.05 即 rc=1,与「1.05–1.15 登记归因不强堵」语义矛盾(登记档常态红);统一为 >1.15 出口红 / 登记档 rc=0 带档注,门禁三 ≤1.15 为 P2 硬门。三端口改 $$ 派生(POSIX sh 空 RANDOM,P1 台账 M-T7-4 同款规避)。
- run.sh 主环时长注:coro_det_replay 专属块入环 +~1.7s(审查 Important 明令,门禁三件不在此列——c10k/bench 均独立脚本,主环跳过实测:c10k 无 src/main.ct 被守卫跳过,bench 无 c_src 目录首守卫跳过)。
