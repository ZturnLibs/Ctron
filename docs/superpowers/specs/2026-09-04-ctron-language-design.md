# Ctron 语言设计文档(v0.1 脑暴稿)

日期:2026-09-04
状态:待评审(brainstorm 产出,尚未实现)
决策记录:内存模型 = **GC 默认 + 所有权逃生舱**;性能底线 = **必须匹敌 C/Rust**(均已由用户确认)

---

## 0. 一句话定位

> **Ctron 是一门为"AI 写、人读"时代设计的系统级语言**:编译器当护栏、效果全显式、一条命令工具链;GC 档保生成顺畅与阅读清爽,`own` 档与裸机档追平 C;同一门语言覆盖服务器、桌面、WebAssembly 与嵌入式。

## 1. 问题定义与需求来源

### 1.1 场景前提

AI 已承担大部分代码**编写**,人类的主要工作转为**阅读、审查、验证**生成的代码。这个前提改变语言设计的优化目标:

| 传统语言优化目标 | Ctron 的目标 |
|---|---|
| 写起来省字符 | 读起来零歧义;AI 写、人读各取所需 |
| 表达力强、灵活 | 可预测性强;一种问题只有一种惯用写法 |
| 错误运行时暴露 | 编译期拦下;AI 生成的代码"能编译 ≈ 大概率对" |
| 语法简洁优先 | tokenizer/LLM 友好优先(显式花括号、可 grep 符号) |

### 1.2 社区真实痛点(2024–2026 调研证据)

| 痛点 | 来源语言 | Ctron 的回应 |
|---|---|---|
| 借用检查器学习曲线陡、需"背下来" | Rust | GC 为默认;借用检查只出现在显式 `own` 块内 |
| 错误处理冗长、nil 遍地 | Go | `Result + ?`、无 null、sum types |
| 类型是事后补丁、不健全逃生舱(`any`) | Python/TS | 静态强类型、健全、无 `any` |
| 性能与 GIL、打包混乱 | Python | AOT 原生码、单二进制、cargo 式包管理 |
| 内存安全、UB、无包管理 | C/C++ | 默认内存安全;C ABI 一级互操作 |
| async 函数染色 | Rust/JS | 无 async/await 关键字,无色轻量任务 |
| 编译慢毁反馈循环 | Rust/Kotlin/Swift | 查询式增量编译;编译速度对特性有否决权 |
| node_modules/生态碎片 | JS/TS | 官方一体化工具链 + 严格 semver + lockfile |

### 1.3 AI 时代的直接证据

- 语言间生成质量差距大:MultiPL-E 上 Python 最高、Rust 历史最低(~40% pass@1),主因训练数据量与语言难度(arXiv 2410.03981)。
- 编译器反馈闭环让 agent 能自我迭代修复,HN 社区已有"Go 是 agent 最佳语言"的讨论。
- Armin Ronacher《A Language For Agents》(2026-02)实证结论:agent 偏好花括号(缩进被 tokenizer 破坏)、Result 而非异常(agent 倾向全 catch 后烂恢复)、可 grep 的包前缀符号、显式效果声明、一条命令 lint+build+test;讨厌宏、barrel 重导出、别名、flaky 测试。
- AI 生成代码的 CVE 在上升(CSA 2026 报告),编译期护栏价值放大。

### 1.4 硬性需求清单(本项目)

R1 原生跨平台(OS/桌面/服务器/移动侧) R2 Web(Wasm)一等公民 R3 嵌入式(裸机、硬实时可达成) R4 性能匹敌 C/Rust(基准差距 ≤15%,own/裸机档追平) R5 低内存、低能耗、充分发挥硬件(SIMD/多核/NUMA/io_uring) R6 可扩展、插件化(编译器/工具链/运行时三层) R7 AI 生成一次通过率高、人类阅读负担低 R8 快编译、快反馈 R9 完整独立(不依附宿主语言生态),C ABI 互操作

---

## 2. 总体路线选择(已决策)

### 路线 A(选定):单一语言 + 三档运行时 profile

同一门语言、同一套语法与类型系统,通过**运行时档位**适配场景:

- `full` 档:并发 GC + 轻量任务 + 完整 stdlib(服务器/桌面)
- `web` 档:编译到 WasmGC,GC 用宿主,任务映射到 JSPI/wasm threads
- `bare` 档:无 GC、无隐式分配、显式 arena/region,裸机可硬实时

内存模型:**GC 默认 + `own` 块所有权逃生舱**。热点代码用 `own (arena) { ... }` 切换到编译期借用检查的 Rust 子集,无生命周期标注(块内局部推断)。

- 优点:AI 一次通过率高(无全局生命周期);热点与嵌入式仍可零成本;一门语言通吃全部目标。
- 代价:GC 与 own 的边界规则要设计清楚(§5);编译器要做两套内存检查。
- 性能预算:GC 档基准测与 C 差 ≤15%(TechEmpower/benchmarks-game 口径);own/`bare` 档与 C/C++ 互有 5% 内。

### 路线 B(否决):所有权默认(Rust 路线)

C 级性能与确定性最强,但调研显示 Rust 是 LLM pass@1 最低的主语言之一,且人类阅读时生命周期标注持续加税——与 R7 冲突。

### 路线 C(否决):VM/JIT 优先(JVM/Graal 路线)

移植成本低、峰值吞吐可期,但裸机与"匹敌 C"基本不可达,启动与内存与 R4/R5 冲突。JIT 仅作为脚本模式的辅助(§8.4)。

---

## 3. 设计原则(P1–P8)

- **P1 读优先**:代码被读的次数比被写的次数多两个数量级。所有语法歧义争议以"读者第一"裁决。
- **P2 编译器即护栏**:不设不健全逃生舱(无 `any`、无 unchecked cast、无 unsafe 指针存在于 safe 子集之外)。AI 的代码要么编译过,要么得到可行动的报错。
- **P3 一条命令**:`ctron build / test / fmt / doc / lint / bench / run / publish` 一体化(cargo 模式)。
- **P4 反馈毫秒级**:查询式增量编译从第一行代码开始;任何拖慢编译的特性不予接纳(编译速度否决权,Zig 原则)。
- **P5 效果显式**:分配、I/O、并发、可变性、错误——要么出现在签名里,要么出现在调用点,绝不停留在"读实现才知道"。
- **P6 可预测 > 表达力**:拒绝类型体操、宏魔法、多种等价写法。惯用法唯一化。
- **P7 零成本分层**:不用的能力不编译进产物;运行时按档位裁剪到 KB 级。
- **P8 可 grep**:符号一律"包路径前缀",禁止 wildcard 导出与 barrel 重导出。

---

## 4. 语法(v0 草案)

> 语法是草案,以语义决策为准;正式语法在 RFC 阶段冻结。

### 4.1 基本形态

- **花括号块**,显式分界(tokenizer/LLM 友好,证据见 §1.3)。
- 表达式导向:`let x = if ok { 1 } else { 2 }`。
- 命名:类型/枚举/构造子 `PascalCase`;函数/变量 `snake_case`;常量 `SCREAMING_CASE`;包名全小写单词。
- `let` 不可变绑定,`var` 可变绑定;`fn` 定义函数。
- 统一调用语法(UFCS):`xs.map(f)` 等价 `iter.map(xs, f)`——方法糖可作用于自由函数,链式可读且不膨胀 OOP。
- 字符串插值:`"Hello, {name}!"`(UTF-8,Unicode 感知的长度/切片)。
- 显式导入,具名、可追溯:`use std.net.{TcpListener, Request}`;禁止 `use mod.*` 与重导出。
- 注释即文档,文档示例即测试(doc-test,编译并运行)。

### 4.2 示例:常规业务代码(full 档)

```c
use std.http.{Server, Request, Response}
use std.time.Clock

fn handler(req: &Request, clock: &Clock) -> Result<Response, HttpError> {
    let name = req.query.get("name") or "world"     // Option 的显式默认,无 null
    log.info("hit /hello at {clock.now()}")          // 显式依赖注入的时钟,可 mock
    return Response.text("Hello, {name}!")
}

fn main() -> Result<Void, Error> {
    let server = Server.bind("127.0.0.1:8080")?
    server.run(handler)?
    return Ok(void)
}
```

读这段代码不需要看实现即可知道:会失败(`Result`)、错误类型是什么、依赖什么能力(`Clock`)、可能产生 I/O(经由能力对象)。

### 4.3 示例:模式匹配与 sum 类型

```c
enum Shape {
    Circle { radius: F64 }
    Rect   { w: F64, h: F64 }
}

fn area(s: &Shape) -> F64 {
    return match s {
        Circle { radius } => Math.PI * radius * radius
        Rect { w, h }     => w * h
    }   // 穷尽性编译期检查,新增变体 → 所有 match 报错
}
```

### 4.4 示例:热点路径 `own` 块(性能档)

```c
fn parse_all(input: &[U8]) -> Result<List<Event>, ParseError> {
    own (arena) {                        // 切换到所有权模式:无 GC、无写屏障
        var events = arena.list(Event)   // 显式 arena 分配
        var pos: USize = 0
        while pos < input.len {
            let (ev, next) = parse_one(arena, input[pos..])?   // 借用 arena,块内推断,无标注
            events.push(ev)
            pos = next
        }
        return Ok(events.into_gc())      // 只有显式 into_gc 的数据才进入 GC 堆
    }   // arena 整体释放,一个 free
}
```

`own` 块规则(§5.3 详述):块内启用 move/borrow 检查(块内局部推断,无生命周期标注);离开块只能带出 `Copy` 值或显式 `into_gc()` 的数据;块内禁止隐式 GC 分配。

---

## 5. 内存管理(核心章节)

### 5.1 默认层:低延迟并发 GC(full/web 档)

- **分代、并发、三色**,精确栈;目标 P99 停顿 < 0.5ms、与堆大小无关(Generational ZGC 已证明可达)。
- 年轻代分配走 **mimalloc 式线程本地 size-class 分配器**——小对象分配 ~10ns,无锁。
- **分配削减优先于回收优先**:值类型 + 就地构造 + 逃逸分析使"分配数"本身趋近零,GC 只是兜底(证据:大量 `Arc<Rc>` 的 Rust 反而不如批量 GC 快——把小对象GC做好比强制手动管理更优)。
- GC 可插拔(运行时接口标准化):默认并发分代;可换 引用计数(移动端省内存)/ 无 GC(bare 档)。
- 卡片:GC 档的"匹敌 C"路径 = 逃逸分析 + 标量替换 + 分配消除(LLVM/Graul 已验证的成熟技术),让热路径对象根本不上堆。

### 5.2 值/引用二分(语法层可见,读者一眼分清)

- `struct` = **值类型**(栈/内联,赋值即拷贝,编译器消除冗余拷贝)。
- `ref class`(简称 `class`)= **GC 堆引用类型**,默认无别名可变(方法默认拿 `&self` 只读视图,`var self` 才可变)。
- 装箱显式:`Box<T>`。数组/切片 `T[]`、`List<T>` 惰性,无隐藏堆分配。

### 5.3 `own` 块:作用域所有权子集(性能逃生舱)

- 语法 `own (alloc) { ... }`,`alloc` 是 `Arena | Region | Pool | Static`。
- 块内规则 = Rust 的一个**可判定子集**:move 默认、borrow 检查、无生命周期标注(块内单向流,推断必成功——若推断失败给出"改用 GC 值"的降级建议,不与用户缠斗)。
- 块外规则:出块仅 `Copy` 或 `into_gc()`;GC 值禁止在块内被可变借用(防止跨 GC/own 边界的隐藏别名)。
- 面向 AI 的护栏:own 块借用报错的修复建议是机械的(拷贝/移动/改 GC),不会像全局生命周期那样把架构掀翻。

### 5.4 `bare` 档:完全显式

- 无 GC、无运行时分配器(可选静态池);所有分配经显式 arena 参数(Zig 模式)。
- `#[no_alloc]` 函数约束:隐式分配 = 编译错误;中断服务例程默认 `#[no_alloc] #[no_spawn]`。
- 硬实时达成路径:零隐式分配 + 无 GC 停顿 + 可确定性构建。

### 5.5 所有权与资源

- 析构确定性:文件/锁/socket 用 RAII(编译期 drop,无需 GC 参与);GC 只管内存,不管资源——Swift 的教训:资源交给 GC 是 bug 之源。

---

## 6. 类型系统

### 6.1 决策表

| 决策点 | 选择 | 理由 |
|---|---|---|
| 名义 vs 结构 | 名义类型 + 名义 trait(显式 impl) | 报错定位、可 grep、文档自明;TS 结构型的教训 |
| null | 无 null;`Option<T>`(糖:`T?`),必须解包 | 十亿美元错误,健全 null 安全是社区最高共识愿望 |
| 错误 | `Result<T, E>` 在返回类型里 + `?` 传播;panic 仅表 bug,只在任务边界可捕获 | Joe Duffy《Error Model》+ Ronacher 的 agent 证据 |
| 泛型 | 单态化默认,`dyn Trait` 显式动态分发 | 零成本;动态分发是读者的显式信号 |
| HKT | 不做 | 类型体操与 R7 冲突;GAT 级别也不进 v1 |
| 推断 | 块内全推断;**签名必须标注** | 签名即契约(AI 与人都靠它局部阅读) |
| 继承 | 不做;组合 + trait | 现代共识 |
| 运算符重载 | 不做(数值/定长 SIMD 白名单除外) | 可预测性;硬件语义需要 `*` 于向量 |
| 渐进类型 | 不做 | sound gradual typing 有数量级减速实证(POPL16),且 `any` 是 AI 代码劣化的入口 |
| 元编程 | `comptime`(Zig 式统一),见 §6.3 | 不另造宏语言;proc-macro 是 Rust 编译慢主因之一 |

### 6.2 效果系统 = 能力对象(capabilities),而非新类型系统

I/O 类效果建模为**必须持有的能力值**:`fn read(path: Path, fs: &Fs) -> Result<Bytes>`。

- 可读:依赖出现在参数里,读者一眼看到(比 effect 关键字更少仪式)。
- 可测:测试注入 `FakeFs`,无需 mock 框架。
- 可审计:manifest 声明程序可用能力(`Ctron.toml` 权限段,拒绝 = 编译期/启动期失败)。
- 内建注解只保留三个:`#[no_alloc]`、`#[no_spawn]`、`#[pure]`(可被编译器用于优化与并行证明)。

### 6.3 comptime:有边界的编译期执行

- 泛型即 comptime 类型参数:`fn Matrix(comptime N: USize) -> type`。
- comptime 函数在 CVM(§8.4)上执行,**纯函数 + 总时间预算**,超时/副作用 = 编译错误(杜绝 proc-macro 式无限膨胀与不可缓存)。
- **保持 parametricity**:comptime 不得反射泛型参数的运行时类型(Zig 的教训);类型反射仅限显式 `@derive(Json, Eq)` 声明,由注册的 derive 插件(§10)展开为普通代码。

### 6.4 数值

- `I8..I64 / U8..U64 / ISize / USize / F32 / F64`;默认**检查算术**(溢出 = panic,可 release 关闭);`x +% y` 显式回绕;`Simd[F32, 8]` 定长向量一等公民。

---

## 7. 并发(无色)

### 7.1 模型

- **轻量任务 + work-stealing 调度**(goroutine 式),无 async/await 关键字:所有代码一种颜色;可挂起点由编译器标记,底层用分段栈(挂起才付切换成本,Go 模式)。
- **结构化并发作用域**:`scope { |s| s.spawn(...) }` 块退出即 join;子任务失败 → 取消兄弟 → 错误沿作用域树传播(取消是一等公民,达 Erlang 监督树效果)。
- **通道** CSP 原语;有界默认(背压显式)。
- **数据并行单独建模**(Rayon 证据:与 I/O 并发不可混用):`parallel.map / parallel.reduce`,迭代器自动 SIMD 化 + 分块窃取。
- 跨任务共享:`Send`(可迁移)自动 trait;可变共享必须经 `Atomic / Mutex / channel`——数据竞争在编译期消失,规则同 Rust Send 但无生命周期参与,错误信息可机械修复。

### 7.2 硬件利用(R5)

- io_uring / kqueue / IOCP 统一异步 I/O 层(运行时内建,API 无色)。
- NUMA 感知分配器与任务亲和(运行时选项);`Simd` 类型 + 自动向量化;comptime 展开保证热点无循环开销。
- GPU:`kernel` 块编译到 SPIR-V/PTX(v2 路线,MLIR 插件,§10),不进 v1 核心。

---

## 8. 编译器与工具链架构

### 8.1 分层

```
源码 → 手写递归下降解析(快、报错好)
     → HIR → 类型检查/效果检查(查询式,Salsa 式增量)
     → CIR(SSA,携带效果/分配信息,单 IR 多用途)
     → 后端(全部插件化,§10):
        ├ Cranelift  → dev 构建(比 LLVM 快 ~40%,产物慢 ~14%)
        ├ LLVM       → release 构建(-O2/-O3 + LTO + PGO)
        ├ WasmGC     → web 档;Wasm MVP → 嵌入式 wasm 运行时
        └ CVM 解释器 → comptime 执行 / ctron run 脚本模式 / 调试
```

### 8.2 编译速度(P4 否决权的落地)

- 查询式编译 + 持久化增量缓存(rust-analyzer 的 durable incrementality 经验)。
- 语言级保证:**禁止循环依赖**(包与模块两级)、无头文件、单遍可解析、泛型实例化成本受控(单态化预算超限 → 提示 dyn)。
- 内容寻址的全局构建缓存,CI 直接命中。
- 预算:中型项目(10 万行)增量 check < 200ms;全量 debug 构建 < 5s。

### 8.3 一条命令工具链(P3)

`ctron build/test/fmt/doc/lint/bench/run/publish/add/target`;严格 semver + lockfile + workspace;`ctron fmt` 输出**唯一规范格式**(AI 产物自动归一);`ctron check` 输出机器可读 JSON 诊断(每条带稳定错误码如 `E1024` + fix 建议),这是给 agent 的第一接口。

### 8.4 CVM(共用虚拟机)

一个小型栈式 IR 解释器,同时服务 comptime 执行、脚本模式(`ctron run xxx.ct` 零等待启动)、教学调试;不承诺性能,不进生产路径。

### 8.5 产物

- 默认静态单二进制(运行时目标 < 300KB);交叉编译内建(`ctron build --target riscv32imac-none`)无需外部工具链(lld 内嵌,含 minilibc 选项)。

---

## 9. 跨平台矩阵(R1/R2/R3)

| 档位 | 目标 | 内存 | 并发 | stdlib 层 |
|---|---|---|---|---|
| `full` | Linux/macOS/Windows/移动 NDK | 并发分代 GC(可插拔) | 任务+通道+并行 | `core < alloc < std` |
| `web` | WasmGC(浏览器/边缘/插件沙箱) | 宿主 GC | JSPI/wasm threads | `core < alloc < stdweb` |
| `bare` | Cortex-M/RISC-V/裸机 wasm | 无(arena/静态池) | 可选协作调度器库 | `core` |

- 分层同 Rust `std/no_std` 但按**档位**而非特性开关组织;同一 crate 的代码可声明"至少需要哪层"。
- JS 互操作:类型化桥(无 `any` 泄漏);C ABI 一级互操作(`extern "c"` + `ctron bindgen` 从 C 头生成绑定)。

---

## 10. 插件化与扩展性(R6)——"编译器本身即插件宿主"

三层插件协议,**全部稳定 API**:

1. **编译器插件**(在 CVM/wasm 沙箱内运行,保证确定性与可缓存):
   - `derive` 插件:实现 `@derive(Json, Eq)` 类代码生成(受 comptime 纯函数约束);
   - `lint`/诊断插件:拿到类型化 HIR,产出带错误码的警告;
   - **codegen 后端插件**:Cranelift/LLVM/Wasm 本身就是首批插件——第三方可加自定义后端(如 GPU/DSL),这是"可扩展"的最大杠杆。
2. **工具链插件**:`ctron <子命令>` 外部可执行(git 模式);LSP server 内建,插件可扩展语义面板。
3. **运行时插件**:稳定 C ABI 动态库 + IDL 接口描述,支持热重载(桌面/服务器档)。

配套生态基建 day-1:tree-sitter 语法 + LSP + 语言服务器协议;后续 v2:面向 agent 的 MCP 服务(check/fix/docs/语义 diff)。

---

## 11. AI 原生设计(R7,差异化核心)

1. **语法面向 tokenizer**:花括号、无缩进语义、无上下文相关语法;`ctron fmt` 保证 AI 产物归一到唯一格式。
2. **错误码体系**:每条诊断有稳定码 + 复修建议(示例先行),供 agent 循环消费;`ctron check --format=json` 一条命令完成 parse+type+lint。
3. **契约可读**:签名必标注 + 能力参数 + `#[pure]`/`#[no_alloc]` 注解 → 人审查 AI 代码时读签名即可圈定行为面。
4. **语义 diff**:`ctron diff` 输出结构化(签名/效果/行为三类)变更报告,服务"人审 AI PR"场景。
5. **doc-test 即回归**:文档示例全部编译运行,AI 改动立即可验证。
6. **反 AI 穴位清单**(明确禁止的坑):无 barrel/别名重导出、无宏、无 flaky 默认(时间/随机必须经能力注入)、无"类型错还能跑"(TS 误导 agent 的实证教训)。
7. **确定性模式**:`ctron test --deterministic` 冻结调度序与哈希种子,失败可复现。

---

## 12. 标准库草图

- `core`(全档位):数值、切片、`Option/Result`、惰性迭代器、模式匹配、`comptime`、`fmt`、UTF-8 字符串。
- `alloc`:`List/Map/Set/Box/String`(显式依赖分配器参数——bare 档也能用,只要给 arena)。
- `std`:`fs/net/http/json/log/crypto/time/test`——全部 I/O 经能力对象;测试框架内建(属性测试 + 快照)。
- `stdweb`:DOM/Canvas/Fetch 桥(类型化)。
- 治理:editions 机制演进不破坏;规范 + 一致性测试集开源。

---

## 13. 明确拒绝清单(与理由)

异常(控制流不可读)、null、`any`/渐进类型、HKT、文本宏/AST 手术、async/await 染色、继承、任意运算符重载、循环依赖、wildcard 导入、隐式数值转换、资源依赖 GC 回收。

---

## 14. 实施路线图(供后续 planning)

| 阶段 | 内容 | 出口标准 |
|---|---|---|
| P0 | 规范冻结 v0.2:形式语法 + 语义文档 + 一致性测试集 | 语法无歧义可解析 |
| P1 | 引用实现:解析 + 查询式类型检查 + CVM 解释器 + `ctron check/fmt/run` | 跑通 core 子集自举测试 |
| P2 | Cranelift 后端 + `full` 档运行时(GC/任务)+ `ctron build/test` | benchmarks-game 子集达 C 的 2x 内 |
| P3 | LLVM 后端 + PGO/LTO + own 块借用检查 | 基准达 C 的 1.15x 内;own 档 ±5% |
| P4 | WasmGC 档 + JS 桥 + `stdweb` | TechEmpower Web 类目可提交 |
| P5 | `bare` 档:交叉编译 + no_alloc 体系 + Cortex-M/RISC-V 板级 demo | blink + 硬实时环路 demo |
| P6 | 插件 API 稳定化 + LSP/tree-sitter + 包仓库 | 第三方 derive/lint 插件可发布 |

## 15. 主要风险与对策

| 风险 | 对策 |
|---|---|
| GC 档难达"匹敌 C" | 分配消除(逃逸分析+标量替换)为主、GC 为兜底;own 块兜底热点;P3 阶段未达标则收紧 GC 档默认策略 |
| own 子集与 GC 边界复杂 | own 规则设计为"可判定、可降级":推断失败建议改 GC 值,不与用户缠斗 |
| 双后端维护成本 | 后端从第一天就是插件接口;Cranelift/Wasm 各自独立测试矩阵 |
| 范围失控(系统级语言 + 全平台) | 严格按 P0→P6 阶段门禁;每阶段出口量化 |
| 冷启动生态 | 证据表明 agent 可移植既有库;C ABI + bindgen 先行,让"用存量 C 库"成为第一天能力 |

## 16. 术语表(本草案引入的概念)

`own 块` 作用域所有权子集 / `档位(profile)` full|web|bare / `能力对象` 显式注入的 I/O 权限值 / `CIR` SSA 中间表示 / `CVM` comptime 与脚本用共用解释器 / `derive 插件` 沙箱内代码生成器。
