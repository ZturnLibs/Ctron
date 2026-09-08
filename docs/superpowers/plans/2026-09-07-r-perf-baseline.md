# R 线性能基线(编译器/解析器)—— 设计与首次实测

> 创建:2026-09-07。范围:仅 Rust 线(`compiler-rust/`)。性质:**基线测量**(不是达标验收)——
> 设计文档 §8.2 的编译速度预算是 P4 终态目标,本轮建立可复现基线,量化现状与预算的差距。
> 方法论延续:零依赖(Cargo 无第三方 crate,基准 harness 手写计时,不用 criterion)。

## 1. 测什么:五级流水 + 三类规模

**流水级**(逐级独立计时,定位瓶颈在哪一段):

| 级 | API | 说明 |
|---|---|---|
| lex | `ctron::lex` | 词法(token 流) |
| parse | `Parser::new(tokens).parse_file_public()` | 语法(AST),不含 lex |
| check | `ctron::check_src(src, profile)` | sem+check 全流程(含 lex,即用户感知的 `ctron check`) |
| trans | `Trans::new().trans_file(&ast)` | C11 代码生成(文本输出) |
| interp | `interp::run_test_file(src, profile)` | check + 解释执行(test 块全跑) |

**规模维度**:

1. **冻结语料**(61 文件,真实代码形态):逐文件中位数,聚合 MB/s,最慢 Top5;
2. **合成规模**(1k/5k/10k/50k 行单文件):行吞吐随规模的曲线(验线性,暴露超线性陷阱);
   合成代码用已验证语法(算术/宽度/插值/match/泛型实例化/UFCS/闭包),harness 断言零诊断;
3. **多文件包**(100 模块链式依赖):`check_package` 的包级开销(模块系统成本)。

**微基准与端到端**:

- 解释执行微基准:递归 fib(25)(调用开销)+ 100 万次循环(语句分发开销)——树遍历解释器的两个支配成本;
- `ctron build` 端到端(子进程):trans / cc / 总时间 + 产物体积;
- 峰值内存:`/usr/bin/time -l` 下 check 50k 行的 max RSS。

## 2. 预算口径(设计文档 §8.2,终态目标)

| 预算 | 值 | 现状口径 |
|---|---|---|
| 10 万行**增量** check | < 200ms | 增量编译未实现——本轮测**全量** check 作基线 |
| 10 万行**全量 debug 构建** | < 5s | trans + cc(-O0) 总和 |

## 3. 方法约定

- **release 构建**(`cargo run --release --example perf`);附一次 debug 对照量化工具链差距;
- 计时:1 次预热 + N 次迭代取中位数(小文件 N=30,合成/包 N=3~5);
- 环境如实记录:机器/CPU/内存/Rust 版本/日期;同机复跑方差 >10% 需注明;
- harness = `compiler-rust/examples/perf.rs`(库 API 直测,不经 CLI,消除进程启动噪声;
  端到端例外走 CLI 子进程);结果以 Markdown 表打印,人工回写本文档。

## 4. 实测结果(2026-09-07)

环境:Apple M3 / 16 GB / macOS(darwin 25.2.0 arm64) / rustc 1.89.0 / release 构建 / 中位数。
harness:`compiler-rust/examples/perf.rs`(`cargo run --release --example perf`,支持 `quick`/`e2e` 分段)。

### A. 冻结语料(53 文件,51 可转译,共 32 KB)

| 级 | 吞吐 |
|---|---|
| lex | 105.4 MB/s |
| parse(含 lex) | 50.1 MB/s |
| check | **16.2 MB/s** |
| trans | 31.6 MB/s |

单文件 check 最慢 Top5 均在 0.25 ms 以内(最慢 04_generics_comptime 0.24 ms);任何冻结语料文件
五级全流 < 0.4 ms。解释执行 36 个行为/panic 文件合计 **2 ms**。

### B. 合成单文件规模(声明型代码:算术/插值/match/struct/闭包/UFCS)

| Ctron 行数 | lex ms | parse ms | check ms | check 行/s | trans ms | trans 行/s |
|---|---|---|---|---|---|---|
| 917 | 0.2 | 0.5 | 1.4 | 641k | 0.8 | 1.16M |
| 4 505 | 1.1 | 2.4 | 7.0 | 644k | 4.3 | 1.06M |
| 8 973 | 2.1 | 4.8 | 14.0 | 643k | 8.7 | 1.03M |
| 44 785 | 11.0 | 25.6 | 76.7 | 584k | 67.8 | 660k |

- check 到 9k 行**严格线性**(643k 行/s 恒定);45k 行轻微退化(-9%),trans 退化较明显
  (1.03M → 660k 行/s,-36%)——疑似 sink String 拼接超线性,记为 R-P4 观察点。

### C. 多文件包

100 模块(链式 `use` 依赖)check_package:**12.2 ms**——模块解析/可见性检查开销可忽略。

### D. 解释执行微基准(树遍历解释器)

| 基准 | 时间 | 折算 |
|---|---|---|
| 递归 fib(25)(~24 万次调用) | 262 ms | ~1.1 μs/调用 |
| 100 万次 while 循环(每次 2 语句) | 371 ms | ~185 ns/语句 |

注:D2 步数护栏(默认 200 万步)会截断上述基准,harness 以 `CTRON_MAX_STEPS=0` 解除;
护栏本身工作正常(护栏价值在防 AI 生成代码死循环,基准场景豁免)。

### E. build 端到端与内存

| 场景 | 总耗时 | 备注 |
|---|---|---|
| build 06f_parallel(真实并发语料) | 56 ms | 产物 49 KB |
| build 合成 1k 行 | 52 ms | 产物 32 KB |
| build 合成 10k 行 | 97 ms | |
| build 合成 45k 行 | 282 ms | trans+cc 全量 |
| trans 1k 行(含进程启动) | 3 ms → C 1 896 行 | ≈2.1 倍膨胀 |
| cc -O0(1k 行产物) | 48 ms | **build 时间大头在 cc** |
| check 50k 行峰值 RSS | **~125 MB** | |
| debug vs release(1k check) | 9 vs 3 ms | 差 3 倍,基线必须用 release |

## 5. 结论与对预算的评估

对照设计文档 §8.2 预算(10 万行增量 check < 200ms;全量 debug 构建 < 5s):

| 预算项 | 外推(10 万行) | 评估 |
|---|---|---|
| check | ~170 ms(全量,非增量) | **已达预算线内**——但注意口径:合成声明型代码是乐观下界,真实代码(泛型密集/深表达式)更慢;增量编译未实现 |
| 全量构建(trans + cc) | ~630 ms | **远低于 5s 预算**;大头是 cc(~60%),Ctron 自身 trans 占比极小 |

**瓶颈定位**(按五级流水):check 是编译期最慢级(16 MB/s,比 lex 慢 6.5 倍)——语义分析
(solve/borrow/Send/alloc 推断)是优化首选拼图;trans 次之且 45k 行处有超线性苗头。
解释器 ~185 ns/语句,符合设计文档"CVM 不承诺性能、不进生产路径"的定位——性能路径是 trans/cc。

**对 R-P4 的输入**:① trans sink 改 push-str 分块,消 45k 超线性;② check 的 solve 热点剖析
(perf 用 instruments/sample);③ 增量编译是 §8.2 预算的正经兑现路径,全量达标不替代它;
④ 基线复测:每里程碑落地后重跑本 harness,防止性能回退(建议挂入 R-P4a 验收)。

**诚实声明**:本基线的合成语料是声明型代码(函数/类型/控制流),未压测泛型重度单态化、
深嵌套表达式与超大 match——这些是真实项目 check 成本的上偏因素;预算结论标注为"乐观下界"。
