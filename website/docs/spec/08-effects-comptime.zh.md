<!-- 站点同步件(中文):源头 docs/spec/,勿直接编辑;漂移由 pages workflow --check 把关 -->
<!-- 英文正文 = 同名无后缀 .md(手维护译件,不随本工具再生) -->
# §8 效果系统与 comptime

## 8.1 能力对象(capabilities)——I/O 效果建模

I/O 类效果 = **必须持有的能力值**,依赖出现在参数里(P5):

```c
fn read(path: Path, fs: &Fs) -> Result[Bytes]
fn handler(req: &Request, clock: &Clock) -> Result[Response, HttpError]
```

- 能力是普通值/引用(`&Fs`、`&Clock`),可组合为 trait;测试注入 fake 实现(`FakeClock`),无需 mock 框架。
- **不做新类型系统/效应关键字**——能力参数即签名即契约;已知代价是深链传递样板,缓解:入口集中构造 + 任务局部传递(编译期可判定,不跨任务,RFC 细化)。
- 长链传递污染库签名时,能力可打包为上下文 trait(超 trait 组合):`trait Env: Clock + Fs + Log`——接收 `&Env` 即同时持有三者。

## 8.2 能力审计

- 包清单声明能力上限(§2.7 `[caps]`);程序实际使用集 ⊆ 声明集,超出 = E4010。
- 服务器档键集(v0.8):`net.listen` / `net.connect` / `net.resolve`(§11.1)、`db.connect`(§12.1)——语义同 fs 键:manifest 声明上限,实际使用集 ⊆ 声明集,超出 = E4010;`#[pure]` 触达 = E4020。
- main 的能力由**运行时初始化**按 manifest 授予(启动期失败优于运行期越权)。
- `Global[T]` 可变全局纳入审计视图(§7.6)。

## 8.3 注解契约(全部内建注解,就此四个 + derive)

| 注解 | 语义 | 违规 |
|---|---|---|
| `#[pure]` | 无 `&Cap` 能力调用(**能力判定机制**:能力 trait 必须继承前奏标记 `trait Cap`,§3.8.2;对 `&Cap` 接收者的方法调用即非纯)、无 spawn、无全局可变;分配允许(不可观察) | E4020 |
| `#[no_alloc]` | 函数体内无 GC 分配(§6.5);用于 trait 方法 = 实现契约 | E3040 |
| `#[no_spawn]` | 函数体内禁止 spawn | E4030 |
| `#[trusted]` | 开放不健全操作(仅 FFI/底层,§9.6);包级可枚举审计 | lint 统计 |
| `@derive(A, B)` | 声明式代码生成,由沙箱内 derive 插件展开(普通代码,非宏手术) | 插件诊断 |

> **修订注(2026-10-03,T52 插件沙箱 v1 落库)**:插件 = 普通 Ctron 包(清单 `plugin "kind.name"` 块声明,CTCL 注册表增殖文法零改动);编译器进程内以受限调用面执行(复用自举解释器 = comptime CVM 全量形态,零进程外插件/动态链接/FFI)。沙箱边界三层:①静态纯度门(extern 禁 + I/O/时钟/并发白名单 + 能力调用门,E6020.sandbox 域);②静态规模门(256 KiB/512 decl;执行期步数预算列 v2——`Global[T]` 实证为单绑定持久盒无跨调用计数载体,与 comptime v0→v1 同演进路径);③确定性(禁时钟/环境/并发,同输入同输出锚)。derive 插件约定入口 `ctron_derive(DeriveInput) -> Str`(合成源码文本经重 parse 注入,产物禁 use/test/@derive 递归);内建集 {Show, Eq, Error} 维持 v0 声明性口径不动。设计案与 HIR 暴露宽度裁决:仓库内部设计文档。

> **修订注(2026-10-10,derive(Json) 官方插件充实)**:`@derive(Json)` 产物契约钉形如下。①产物方法名带型别(`T_to_json` 等):同名方法跨接收者型别不作重载分派, mangling 消歧。②读半边为「校验先行双件」:`T_json_err(entries, path) -> Str`(空串 = 合法,否则错误码@路径)与 `T_from_json_at(entries, path) -> T`(校验通过后取值);`Result` 载荷槽为 64 位整数,不携带 struct 值,故读件不返 `Result[T, E]`。③字段型别面:I32/I64/Bool/Str、嵌套派生 struct,以及 `List[标量]`/`Option[标量]`(接口 v2 起支持;`Option` 以 null 为母语,字段缺失与 null 分判);`F64`、组合泛型(`List[List[T]]`/`List[Option[T]]` 等)、struct 元素列表、`Map` 与定长数组当前不受支持——插件合成对未定义函数的调用,以编译期未解析错误指明型别(接口 v1 的 `ty` 为头标识符平文本,型别实参不可达;v2 修订增 `ty_full` 完整型别文本槽,实参槽位其余面仍为接口后续修订事项)。④读半边产物引用 `std.json` 查询面,消费包须显式 `use` 请求(跨包引用即请求);写半边产物零依赖自包含。

- 编译器可利用 `#[pure]` 做优化与并行证明;`parallel.map` 闭包的纯度由**推断**得出(规则同上,§7.7),无需在闭包上书写注解。
- `#[trusted]` 数量与位置随包发布元数据上报;`ctron lint --trusted` 列出全部信任边界。

## 8.4 comptime:有边界的编译期执行

- `comptime fn` 在编译期(CVM)执行:**`#[pure]` 语义 + 总步数预算**(默认 1200 步/编译单元,可调)。超预算 E6010;副作用/不确定性 E6020。
  > **修订注(2026-09-28,T08 用户裁决)**:预算口径由 v0.3 草案的「1s 时间预算」改为**步数预算为 v1 终态**——步数天然确定可复现(同输入同判定),与 §10.3 确定性编译无张力;时间口径依赖宿主时钟,同输入不可复现,故不采用。编译器内置 1200 步/编译单元(E6010 判据,`sem_ceval.ct` ceval)。清单键 `comptime.budget_ms`(CTCL 注册表,C 宿主 pkg 解析+fail-closed 校验)为声明位,当前不进入预算执行;量纲命名统一原定随 CTCL 迁移批次(spec-gap T48)定夺——T48 已落(2026-10-02),键名维持 `budget_ms` 现状(声明位未激活,改名无消费方,留待预算执行实装时一并处理)。
- `const NAME: T = expr`:expr 在编译期求值(可调用 `comptime fn`);`static let` 的常量形式同理(§7.6)。
- 泛型值参数(`comptime N: USize`,定长数组维度 `T[N]`)是 v0.3 唯一的类型级 comptime;**类型产出函数**(`fn Matrix(comptime N) -> type`)预留 v2。
- **parametricity 保持**:comptime 代码不得反射泛型参数的运行时类型(E6030);类型反射仅经显式 `@derive` 声明,插件展开为普通代码——杜绝 Zig comptime 式泛型反射。

## 8.5 编译预算(与 §8.4 配套的编译速度保护)

- 单态化实例总数/包有上限(默认 8192,可调);超限诊断建议 `&Trait` 化。
- comptime 预算、实例预算、模块无循环依赖(§2.6)共同构成"编译速度否决权"的语言级落地。

## 8.6 与测试集的对应

`tests/07_capabilities.ct`(能力注入)、`tests/07_pure.neg.ct`(E4020)、`tests/04_generics_comptime.ct`(comptime/const)。
