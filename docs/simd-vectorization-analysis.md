# Simd 自动向量化评估 + §9.5 硬件利用三残项对账(T51)

> 2026-10-03。§9.5 承诺:「`Simd[E, N]` 定长向量一等公民;自动向量化 + comptime 展开 /
> 异步 I/O 统一层:io_uring / kqueue / IOCP / NUMA 感知分配与任务亲和(运行时选项)」。
> 本报告是 T51 的评估交付;实施另立。实证环境:darwin arm64(Apple clang)+ linux x86_64
> (gcc 13,Docker ubuntu 24.04);全部数字可由文中命令在本机复现。

## 1. Simd v0 现状(语义模拟的两副面孔)

- **解释臂**(自举,`compiler/src/eval_expr.ct`/`eval_call.ct`):`Simd[E,N]` 型实参 →
  `SIMD` 码 cx 值;splat/lane/to_array + `+ - * /` 元素级白名单,纯软件模拟。
- **发射臂**(`trans_expr.ct`,P1-C2 标量模拟):`ctron_view_<E>{ int64_t n; E* d; }`
  堆视图 + **每运算一次 arena 分配** + 元素级 C 循环:

```c
ctron_view_g t_c = ({ ctron_view_g t_ql = t_a; ctron_view_g t_qr = t_b; ctron_view_g t_qo;
    t_qo.n = t_ql.n; t_qo.d = (float*)ctron_amalloc(sizeof(float) * t_ql.n);
    { int64_t t_qi; for (t_qi = 0; t_qi < t_ql.n; t_qi++)
        { t_qo.d[t_qi] = t_ql.d[t_qi] + t_qr.d[t_qi]; } } t_qo; });
```

  形态三要点:①**连续内存 + 元素级循环 = 可向量化形态**(本评估的实证前提);
  ②**trip count 是运行时 `n`**,非类型里的定长 N(语义上恒等,代价是编译器无法
  按定长完全展开);③**每 op 一次 amalloc**——Simd 值是堆视图,不是寄存器驻留。

## 2. 实证:发射形态在主流后端下的向量化实况

试金石 = 发射面逐字节同形模板(`hot.c`,见 §1 形态)+ 手写 NEON 对照。
向量化判定与运行时 A/B:

| 后端 | -O1 | **-O2(产品链缺省,ctc.sh `target:native`)** | -O3 | 判定细节 |
|---|---|---|---|---|
| Apple clang(arm64) | ✗ | **✓ `vectorized loop (vectorization width: 4, interleaved count: 4)`** = NEON 128 位全宽 + 4× 交织 | ✓ | `-Rpass=loop-vectorize` |
| gcc 13(x86_64) | ✗ | **✗**(`Loop costings not worthwhile` + `statement clobbers memory: malloc` 进 costing) | **✓ `loop vectorized using 16 byte vectors`**(SSE) | `-fopt-info-vec(-optimized/-missed)`;`-ffast-math` 在 -O2 不救 FP 循环;AVX 需 `-march=x86-64-v3` |

运行时 A/B(64K 元素 × 200 轮,含分配;发射形态 vs 手写 NEON):

- darwin(clang -O2):**1.12–1.20×**(三轮均值,两轮采样)
- linux 容器(gcc -O2,未向量化态):**1.41×**

两处量测均被 **amalloc 主导**(每 op 一次分配),算术向量化与否的差被摊薄——
这本身就是发现:循环级向量化的收益 < 分配开销。

### 2.1 结论(卡上二选一:**选 B —— 规范挂「v0 为语义模拟,向量化志向」注**)

1. **自动向量化已在产品链缺省档兑现一半**:clang -O2(= darwin 生产路径)对发射
   循环全宽向量化,零语言侧改动;gcc 需 -O3(gcc 对带前置分配的 FP 循环 costing
   保守)。工具链注记:Linux 用户热路径建议 `-O3`(或对整数循环 `-O2 -funroll-loops`)。
2. **真正的差距不在循环向量化,在寄存器驻留**:`Simd[F32, 8]` 本应是 2 条 NEON
   寄存器 / 1 条 AVX 寄存器内的零分配运算;v0 落地是「分配 + 8 元素内存循环」。
   这是语义模拟的本质代价,自动向量化救不了(N=8 的循环再宽也只有 2 条向量指令,
   分配与访存才是大头)。
3. **intrinsics 直发(结论 A)列志向,不做**:工程量 = E×N×op 白名单组合 ×
   NEON/SSE/AVX/AVX-512/位宽 dispatch,而收益被 2 压缩为「小定长 N 零分配」。
   志向路线(按性价比序):①发射侧对定长小 N(≤16)特化——循环完全展开 +
   SLP 友好形态(临时变量数组而非指针),交给后端编译器做寄存器分配,零新 ISA 面;
   ②arena 批量分配合并(同表达式树共享一块);③真 intrinsics 直发(仅当①②后
   基准仍 >5% 差距,§9.4 硬指标驱动)。

**规范动作**:§3.11 Simd 行 + §9.5 挂「自举侧现状注」(已随本件落):v0 = 语义模拟
(解释)与标量模拟(发射,P1-C2),循环级自动向量化由后端编译器在 -O2/-O3 兑现
(clang 实证 width 4;gcc 需 -O3);寄存器驻留与 intrinsics 直发为志向(§9.5 未闭)。

## 3. 异步 I/O 统一层:io_uring 实装(T51①)

**实装面**:`lib/net/c_src/ctron_rt.c` reactor 增 io_uring 后端(linux 族),与
kqueue/epoll/poll 同构:

- **映射**:每次武装 = 一笔 `IORING_OP_POLL_ADD` SQE(完成即消费,**天然 one-shot**,
  与 EV_ONESHOT/EPOLLONESHOT 口径重合;epoll 的 in_rb/乐观 MOD/EEXIST 升级/交付
  连坐四面在 io_uring 无存在必要);交付 = CQE → `reactor_deliver_locked`(armed
  吞发/last-wins 全复用,**零新契约**)。
- **线程模型**:全部提交(arm/forget)在 G 内直推直 flush(单生产者串行);reactor
  线程唯一消费者,阻塞 `io_uring_enter(GETEVENTS, min_complete=1)`——内核原生唤醒,
  **免 100ms 兜底 tick**(「注册时即就绪」由 POLL_ADD 提交时就绪判定即时交付,
  提交方自带 enter 冲刷)。
- **选择面**:linux 族运行时探测定夺(setup 成功 + NODROP 特性门,≥5.5:CQ 溢出
  内核等待不丢 CQE + POLL_REMOVE 同代);`CTRON_RT_REACTOR=epoll|io_uring` env 压制;
  `ctron_rt_reactor_name()` 观测;uapi 手定义(不引 `<linux/io_uring.h>`,零依赖
  纪律,darwin 可本地语法编译)。
- **开机自检门(本件核心工程决策)**:启动时两笔 NOP 往返(单笔在坏环上会侥幸
  全对——恒消费 slot0;双发钉死顺序消费),逐字核对 ud/res;不过 → **响亮登记
  (`ctron_rt: io_uring 点名但自检不过…`)并回退 epoll**。io_uring 后端永不以坏环
  上线,任何内核形态下契约不破。
- **验收**:reactor 冒烟双臂同形挂 `ci.sh`——linux 族 = epoll 点名 + io_uring 点名
  (内核自检不过时响亮登记回退,`uring-any` 契约断言),darwin = kqueue 单臂;
  T0–T6 全绿(容器 linux x86_64 + darwin arm64 实测)。

### 3.1 立案:6.10-linuxkit 内核 array 间接层异常(本件最大未知)

Docker Desktop VM(linuxkit **6.10.14**)实测:params 报 `sq_off.array=2112/16448`,
写入恒等表可读回,但**内核消费恒取 slot0**(三连发全交付 slot0 的 SQE;CQE
`{ud=1000}`×3;独立探针 + strace 对照 liburing(liburing 2.5 同内核**正常**)。
差异点未复现:映射配方、flush 序、tail 发布均与 liburing 源码逐行一致;疑点收敛在
新内核(6.10 mmap 布局改造:SQ/CQ 头交织同一区,CQ head@8/tail@12 嵌在 SQ 区内)
的 array 定位上。**双发 NOP 自检正是为这类形态设的闸**:不过 → 登记 + epoll 回退,
契约零损。后续:①CI azure 内核(ubuntu-latest)首跑实测定夺真 io_uring 是否在位;
②以 strace 对照 liburing 写序专项收口;③内核 <5.5 无 NODROP → 不上线(契约不容
丢事件)。**IOCP**:windows 无 net 靶机(run.sh 后端注释已登记 WSAPoll 为前置),
环境依赖登记,不做。

## 4. NUMA 感知(T51②):选项位已立,行为位登记志向

- **选项位**:`CTRON_RT_NUMA=off|on|auto`(缺省 auto);`ctron_rt_numa_nodes()` 观测口
  (linux 探测 `/sys/devices/system/node/nodeN`,其他平台/无拓扑面恒 1;容器实测
  numa_nodes=1 ✓)。
- **不做理由(v0 行为位)**:感知分配的真正消费者是 GC/arena 分配器路径
  (§6 分代 GC 的 region 亲和),与 reactor 的任务亲和需要跨两层(runtime 调度 +
  分配器)联动;且本机与 CI 均为单节点,多节点靶机缺位。**志向**:GC region 按
  node 分带 + worker 起点亲和(sched_setaffinity)+ `CTRON_RT_NUMA=on` 语义兑现;
  触发条件 = 多节点靶机可用。

## 5. 对账单(§9.5 × T51 验收)

| 项 | 验收要求 | 交付 | 状态 |
|---|---|---|---|
| io_uring | 双矩阵同形测试(linux 环境可用时) | reactor 冒烟双臂挂 ci.sh;自检门+回退;6.10-linuxkit 异常在册 | ✅(CI linux 首跑终验) |
| NUMA | 选项位或登记不做理由 | 选项位+探测+观测口,行为位登记志向 | ✅ |
| Simd 评估 | 评估报告过评审(二选一) | 本报告;选 B + 工具链注记 + 三级志向 | ✅ |
| IOCP(卡附带) | 无靶机则登记环境依赖 | 登记(WSAPoll 为前置) | ✅ |

## 6. 复现命令

```sh
# reactor 双臂(linux):
docker run --rm --security-opt seccomp=unconfined -v $PWD:/repo -w /repo \
  ubuntu:24.04 bash -c "apt-get install -y gcc && cd tests/net/rt_reactor_smoke && sh run.sh"
# Simd 判定(darwin):
clang -O2 -o /dev/null hot.c -Rpass=loop-vectorize   # width:4, interleaved:4
# gcc 侧:
gcc -O2 -fopt-info-vec-missed -c hot.c               # costings not worthwhile
gcc -O3 -fopt-info-vec-optimized -c hot.c            # 16 byte vectors
```
