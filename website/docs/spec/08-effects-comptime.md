<!-- 站点同步件:源头 docs/spec/,勿直接编辑;漂移由 pages workflow --check 把关 -->
<!-- 英文待翻:中文占位 -->
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

- 编译器可利用 `#[pure]` 做优化与并行证明;`parallel.map` 闭包的纯度由**推断**得出(规则同上,§7.7),无需在闭包上书写注解。
- `#[trusted]` 数量与位置随包发布元数据上报;`ctron lint --trusted` 列出全部信任边界。

## 8.4 comptime:有边界的编译期执行

- `comptime fn` 在编译期(CVM)执行:**`#[pure]` 语义 + 总时间预算**(默认 1s/编译单元,清单可调)。超预算 E6010;副作用/不确定性 E6020。
- `const NAME: T = expr`:expr 在编译期求值(可调用 `comptime fn`);`static let` 的常量形式同理(§7.6)。
- 泛型值参数(`comptime N: USize`,定长数组维度 `T[N]`)是 v0.3 唯一的类型级 comptime;**类型产出函数**(`fn Matrix(comptime N) -> type`)预留 v2。
- **parametricity 保持**:comptime 代码不得反射泛型参数的运行时类型(E6030);类型反射仅经显式 `@derive` 声明,插件展开为普通代码——杜绝 Zig comptime 式泛型反射。

## 8.5 编译预算(与 §8.4 配套的编译速度保护)

- 单态化实例总数/包有上限(默认 8192,可调);超限诊断建议 `&Trait` 化。
- comptime 预算、实例预算、模块无循环依赖(§2.6)共同构成"编译速度否决权"的语言级落地。

## 8.6 与测试集的对应

`tests/07_capabilities.ct`(能力注入)、`tests/07_pure.neg.ct`(E4020)、`tests/04_generics_comptime.ct`(comptime/const)。
