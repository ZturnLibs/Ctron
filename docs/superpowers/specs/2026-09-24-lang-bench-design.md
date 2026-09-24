# tests/lang/bench —— Ctron 语言真实性能基准 v1 设计

日期:2026-09-24
状态:待用户审阅(设计阶段,未动实现)
泳道:横切(tests 面,不占编译器/服务器/gui 泳道槽)

## §0 裁决记录

设计前呈三问,用户未即时应答,按推荐档采认;有异议改本节,spec 其余部分随之微调。

| # | 裁决点 | 采认档 |
|---|---|---|
| ① | v1 范围 | **kernel 套件 + 峰值内存**;编译时只轻记(emit 耗时 + 生成 C 行数随采数日志归档,不设门、不独立成族) |
| ② | 门禁策略 | **建档先行 + 回归自比门**:首波只采数登记;门 = 与 baseline.tsv 建档值自比,绝对比值门等有 spec 门禁表承诺背书再立 |
| ③ | kernel 清单 | **5+1 稳妥集**(fannkuch / binary-trees / mandelbrot / N-queens / 整型 matmul / strwork);F64 重 kernel(n-body / spectral-norm)缓列 v2——双宿主 F64 坑在案 + FP 形差致 digest 难 pin |

## §1 目标与非目标

**回答的问题**:
1. Ctron 生成代码(emit 臂 → C → cc)相对**同算法手写 C** 的普遍开销是多少?按代码形状(整数分支/分配/FP/递归/数组索引/字符串)如何归因?
2. 峰值内存(arena 行为,含 binary-trees 对 arena 回收能力的暴露)。
3. 附带归档:emit 编译耗时 + 生成 C 行数(编译器自身性能的最低限基线,治理 arena/fmap 债的量化抓手之一)。

**不回答的**(范围外,见 §9):
- 跨语言(非 C)绝对排名——不同机不同命,只信同机差分比值;
- 解释臂 / seed 臂性能(另一面,归编译泳道);
- 自举全链编译时基准(轻记除外);
- 每请求时延/吞吐类 IO 基准(tests/net、tests/http/bench 两族已在做,不动)。

**方法论立场**(与既有三族一致):同机差分 + 工作量逐字节 pin + 防优化折叠 + min-of-N + RED 数字照录不改数不换语料不粉饰。**不与任何跨机发表数字(CLBG 榜、TechEmpower 轮)对表。**

## §2 方案对比(定案依据)

- **方案 A(定案):同机差分 kernel 套件 + C 包装器统一采集。** 六个 kernel 各以 Ctron 与同算法手写 C 实现两份,进程级统一计时。优点:直接归因 codegen 税;bench.sh 家族惯例零学习成本;「同一 C 件服务双侧」有 baseline_cycle.c 客户端同位先例。
- 方案 B(否):直接接入 benchmarks game 官方程序与脚本。缺点:CLBG 惯例是各语言「惯用写法 + 平台脚本」,与仓库公平性口径(digest pin、同算法同形)不合;其 IO 型项(k-nucleotide 等)测的是 stdlib 不是语言;CLBG 榜机非本机,无法差分。仅作 kernel 选题灵感来源。
- 方案 C(否):进程外 hyperfine 统计套件。缺点:引外部依赖;kernel 级进程内/进程级计时家族已有 min-of-N 惯例且更可控。hyperfine 留给编译时长这类纯进程级场景,且不设为仓库依赖(可选工具,另账)。

## §3 目录与构件

```
tests/lang/bench/
  bench.sh              # 一键门禁:构建 → digest pin → 采集 → 自比判门 → 登记行
  README.md             # 口径 + 采数登记表 + 归因档(家族惯例形态)
  baseline.tsv          # 建档值(bench.sh record 生成提交;自比门数据源)
  harness/
    runwrap.c           # 进程级采集包装器(家族首个,详见 §4)
  kernels/
    fannkuch.ct    fannkuch_base.c
    btree.ct       btree_base.c
    mandelbrot.ct  mandelbrot_base.c
    nqueens.ct     nqueens_base.c
    matmul.ct      matmul_base.c
    strwork.ct     strwork_base.c
```

每 kernel 一对文件:`.ct`(emit 臂)与 `_base.c`(同算法手写 C),**算法逐行同形**(同循环次序、同表达式分组、同数据布局),输出同一 digest 三整数行。

## §4 采集协议

**runwrap.c**(进程级包装器,一次调用跑一个子进程):
- `fork`/`exec` 子进程,`clock_gettime(CLOCK_MONOTONIC)` 包夹,`waitpid` 后取 `getrusage(RUSAGE_CHILDREN)`(本进程仅此一子,值即该子进程);
- 子进程 stdout 原样转发,末尾追加测量行:`wall_ns=` `user_ns=` `sys_ns=` `maxrss_kb=`;
- `ru_maxrss` 平台单位归一(darwin 字节 / linux 千字节 → 统一 KB,`#if __APPLE__` 分支);
- 两侧 kernel 都经它采集——**计时器/资源口径天然同源**,等价于 bench_cycle「同一 C 客户端驱动全部服务端」的先例。

**采数**:
- 每 kernel 每侧 ×5 次取值:**wall/user/sys 取 min**(噪声下界稳健估计,CLBG 同口径),**maxRSS 取 median-of-5**(min 会低估峰值);
- 工作量定参:每 kernel 参数**常量内嵌后冻结**(不设 env 调参面,保数字可比),以「ctron 侧单跑 ≥300ms」为定参目标(C 侧按比例快,工作量两侧恒等);首采校准后即冻结,复跑大 N 走登记档手改常量并注明;
- 进程级计时含启动摊销(ctron 侧 arena/运行时初始化、C 侧无),工作 ≥300ms 时摊薄,归因注记入 README;
- 全套目标时长 ≤ ~2min(6 kernel × 2 侧 × 5 跑 + 构建),nightly 可挂。

**digest pin(禁采数语义沿 http 族)**:
- 每 kernel 输出三行:`sum=<I64>` `xor=<I64>` `n=<I64>`(结果流的有界算术摘要;**刻意避开 FNV 乘法链与 U64 cast**——`as[U64]` C 宿主三红在册,I64 有界求和/异或两侧皆定义良好);
- 两侧三行不等 → **FAIL rc=1 禁止采数**(mandelbrot 见 §5 FP 形差锚定规则);
- `n=` 另防 DCE 折叠(与 http 族 checksum 非零防折叠同语义)。

## §5 kernel 规格(5+1 稳妥集)

每个 kernel 注明:测什么形状 / 算法口径 / 归因预期。

| # | kernel | 形状 | 口径与规则 |
|---|---|---|---|
| 1 | **fannkuch-redux**(n≈11) | 分支密集整数、数组交换 | CLBG 定义:全排列 flips 求和,checksum 天然整数。定参首采校准后冻结。 |
| 2 | **binary-trees**(depth≈16) | 分配/回收 + 递归 | 节点 ctron 侧 Box、C 侧 malloc/free 同构;checksum = ∑ 节点深度(CLBG 口径,整数)。**maxRSS 比值预期倾斜:arena 若无回收将直接暴露在峰值上——这是测量点不是缺陷**,登记归因,不因过门改语义。 |
| 3 | **mandelbrot**(尺寸≈800) | FP 紧密循环(唯一 FP 项) | 输出 = 逃逸迭代计数之和(整数),非位图。**FP 形差锚定规则**:两侧表达式逐项同形(同括号同次序),cc -O1 无 fast-math(cc 不重结合 FP);digest 仍不等 → 归因 FP 形差,登记档禁采门(http digest pin 同语义)。 |
| 4 | **N-queens**(n≈12) | 回溯递归、函数调用开销 | 解数天然整数;位运算面若 ctron 侧不稳,退化为逐列数组标记(两侧同形优先于技巧)。 |
| 5 | **matmul**(384×384) | 数组索引紧循环 | **I64 有界值域**(输入 ∈ [0,100],内积上限 ~4×10⁶,和 ≤10⁹)——无溢出、无 UB、无 U64 cast 坑;一维 I64[] 手工索引展开(两侧同形)。 |
| 6 | **strwork**(语料≈64KB) | 字符串通路(lane 税最敏感面) | 语料两侧同 LCG 程序化生成(乘子 1103515245、模 2³¹,I64 域无溢出,**禁裸 2⁶⁴ LCG**);操作 = 查找/定长 lane 填充拼接/FNV-类 hash 改有界加法变体(与 bench_cycle fill 同形的手写 byte_at 扫描,**不走未证实的 std.str 面**);与 http parse 的 8× lane 税归因互为印证。 |

**通用写码规则**(compiler 泳道坑位 22 条 + 双宿主分歧清单适用):
- 字符串裸 `{` 须 `\{`;禁 `;`;锚文本防发射形漂移处照抄家族先例;
- 禁 `as[U64]`(双宿主坑);U64 需求一律以 I64 有界值域设计绕开;
- matmul/strwork 常量表若大,程序化生成(fmap 债,勿喂大字面量);
- kernel 不入 tests/lang/run.sh 计例(bench 家族惯例,emit 专臂)。

## §6 门禁(裁决②:建档先行 + 回归自比)

- **首波**:采数入 README 登记表 + 生成 `baseline.tsv`(行 = kernel、c_wall/c_user/c_rss、ct_wall/ct_user/ct_rss、ratio、日期、git rev),rc=0 全归档;
- **此后每跑**:fresh_ratio(ctron/C)对 baseline.tsv 逐 kernel 自比(**比值对比值**——机漂/负载同乘两侧分子分母,比值天然免疫,门只看 codegen 面回归;绝对墙钟漂移不入门)——
  - ≤1.05× → GREEN;
  - 1.05–1.15× → 登记档 rc=0 带档注(家族惯例,噪声在案);
  - \>1.15× → RED rc=1,数字照录登记归文档;
- `bench.sh record` 子命令 = 采数冻结为新 baseline.tsv(回归锚更新须 README 注明缘由);
- **绝对比值门(ctron/C ≤ N×)本 spec 不立**——待首波归因数据在手,若要立须走门禁表承诺(spec §九 同款),不在本 spec 范围;
- 退出码沿家族:0 门内(含登记档)/ 1 超门或 digest 不等 / 2 环境缺件;
- 启用口:`CTRON_LANG_BENCH=1`,不入 CI 主环,nightly 建议(沿 bench 家族惯例)。

## §7 编译时轻记(裁决①:不设门)

每 kernel 构建时顺手记录三数,打印归档行 + 入 README 备注,**不作门**:
- `ctron-emit run` 墙钟(ms);
- 生成 C 行数;
- `cc` 耗时(ms)。

理由:编译器自身性能是「语言真实性能」的一等维度(arena/fmap 债已有 AMEM 直方图证据),但 v1 的主体是生成代码质量;自举全链、seed/emit 双臂分解计时归编译泳道另账。轻记数是那条账的免费起点。

## §8 与现有家族的关系

- 惯例全沿用:`CTRON_*_BENCH=1` 启用口、digest pin 禁采数、min-of-N、退出码 0/1/2、`CTRON_EMIT` worktree 覆盖口、`CTRON_STDPATH` 设置、登记档语义、RED 照录文化;
- 新增件仅二:`harness/runwrap.c`(进程级采集,家族首个)、`baseline.tsv`(自比门数据源);
- tests/net、tests/http、tests/ffi 三族**不动**;tests/lang/ 目录为新建(kernel 不混入其它 run.sh)。

## §9 范围外(明确不做)

1. 跨语言(非 C)对比与跨机排名;
2. 解释臂 / seed 臂计时;
3. 自举全链编译时基准(§7 轻记除外);
4. nightly JSONL 趋势库(v2 候选,README 登记表先行);
5. hyperfine 等外部工具依赖;
6. F64 重 kernel(n-body / spectral-norm)——待双宿主 F64 归一后 v2 再议;
7. 绝对比值门与本 spec 之外的门禁表承诺。

## §10 交付物与验收

**交付物**:
1. `tests/lang/bench/` 全套(bench.sh、README.md、harness/runwrap.c、kernels/ 12 文件);
2. 首波采数:README 登记表(六 kernel × wall 比值 / maxRSS / emit 耗时 / 生成 C 行数)+ baseline.tsv;
3. 本 spec。

**验收**:
- `CTRON_LANG_BENCH=1 sh tests/lang/bench/bench.sh` 首波全绿退出,六个 kernel digest pin 全过;
- 二次运行自比门 GREEN(对首波 baseline);
- 六 kernel 采数全部落在 README 表,RED(若有)照录带归因;
- 编译时轻记三数齐全,不入任何门。

## §11 实施注意(给 writing-plans 的输入)

- 定参流程:每 kernel 先以临时参数首采校准至 ctron 侧 ≥300ms,冻结常量,再正式采数——计划里每 kernel 一个独立任务片;
- kernel 实现顺序建议:fannkuch → matmul → nqueens → btree → strwork → mandelbrot(整数先行,FP 形差风险项殿后);
- 写 `.ct` 侧前逐条过「双宿主能力分歧」清单与 compiler 泳道坑位 22 条;
- runwrap.c 需同时在 darwin(开发机)与 linux(CI/夜环)验证 ru_maxrss 归一;
- 每 kernel 片全绿即单独落库(「继续」驱动惯例),pathspec 限定提交。
