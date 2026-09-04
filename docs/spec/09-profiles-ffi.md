# §9 档位与互操作

## 9.1 三档模型

| 档位 | 目标 | 内存 | 并发 | stdlib 层 |
|---|---|---|---|---|
| `full` | Linux / macOS / Windows / 移动 NDK | 并发分代 GC(可插拔) | 任务+通道+并行 | `core < alloc < std` |
| `web` | WasmGC(浏览器/边缘/沙箱) | 宿主 GC | JSPI / wasm threads | `core < alloc < stdweb` |
| `bare` | Cortex-M / RISC-V / 裸机 wasm | 无(arena/静态池) | 可选协作调度器库 | `core` |

- 分层按**档位**组织(非特性开关);包可声明最低所需层。
- **兼容方向(硬规则)**:`bare ⊆ full ⊆ (任何档)` 向上兼容——bare/own 代码任何档位可用;依赖 GC 分配(alloc 属性)的代码进 bare = E3040(§6.5 机械判定)。

## 9.2 web 档

- 后端 WasmGC;GC 用宿主;任务挂起经 JSPI/栈切换;wasm threads 启用时恢复全量 Send 检查(§7.8)。
- JS 桥:类型化(无 `any` 泄漏);JS 异常在边界转为 `Result`;JS 回调按 web 档 Send 近似规则;DOM/Canvas/Fetch 经 `stdweb`。
- **stdweb 最小 API(v0.5 钉死,P1-D 起可用)**:`use stdweb.dom` 后——`dom.set_title(Str) -> Void`、`dom.title() -> Str`。其余 DOM/Canvas/Fetch 以此模式逐版扩充(锚定测试:`10_web_dom.ct`)。

## 9.3 bare 档

- 交叉编译内建:`ctron build --target <triple>`,工具链自包含(内嵌 lld + minilibc 选项)。
- 首批 bare 目标(tier-1):`thumbv7em-none-eabi`、`riscv32imac-unknown-none`;其余 LLVM 支持目标 tier-2。
- ISR 约束、显式分配器、硬实时路径见 §6.6。

## 9.4 性能口径(R4 规范化)

| 路径 | 指标 | 性质 |
|---|---|---|
| own/热点档 | 与 C 互有 5% 内 | **硬指标**(P3 出口) |
| GC 档 | 与 C 差距 ≤15% | 带退出条件的目标:未达标则收紧 GC 默认策略并引导热点走 own(§15 风险对策) |
| bare | 零隐式分配、无 GC 停顿、可确定性构建 | 硬指标 |

## 9.5 硬件利用

- `Simd[E, N]` 定长向量一等公民;自动向量化 + comptime 展开。
- 异步 I/O 统一层:io_uring / kqueue / IOCP(运行时内建,API 无色)。
- NUMA 感知分配与任务亲和(运行时选项)。
- GPU(`kernel` 块 → SPIR-V/PTX):预留 v2,经 codegen 插件(§10.5)。

## 9.6 FFI 与 `#[trusted]`

- `extern "c"` 函数声明 + `#[trusted]` 标记 = safe 子集外**唯一**入口;包级审计(`ctron lint --trusted`)。声明语法(v0.5):

```c
#[trusted]
extern "c" fn ctron_add(a: I64, b: I64) -> I64     // 无函数体;定义在 C 侧
```
- **C ABI 类型映射**(节选):`I8↔int8_t` `USize↔size_t` `F64↔double` `Bool↔bool(C99)` `T[N]↔T[N]` `T[]↔(ptr,len)`(经包装);struct 按声明布局(`#[repr(c)]` 默认对 FFI 导出)。
- **所有权三约定**(bindgen 按此生成包装):
  1. `C-owned`:C 分配 C 释放,Ctron 仅调用期借用;
  2. `Ctron-owned`:跨边界移交所有权必须经包装类型(如 `CBox[T]`),drop 责任显式;
  3. `borrowed`:临时借用,生命周期 = 调用期,包装层内不外泄。
- **禁止**把 arena 内存交 C 长期持有(释放即悬垂);需要时深拷贝出边界。

## 9.7 产物与工具链(规范性概要)

- 默认静态单二进制;体积口径:bare+core 运行时 < 100KB(硬指标),full 完整运行时 < 1MB(目标)。
- 后端矩阵(全部插件化):Cranelift(dev)/ LLVM(release)/ WasmGC(web)/ Wasm MVP(嵌入式 wasm)/ CVM 解释器(comptime、`ctron run` 脚本、调试)。
- 一条命令:`ctron build/test/fmt/doc/lint/bench/run/publish/add/target/check`;`ctron check --format=json` 见 §10.2。

## 9.8 与测试集的对应

`tests/08_bare.ct`(bare 显式 arena)、`tests/08_bare_alloc.neg.ct`(bare E3040);FFI 多文件用例 P1 起由 `tests/ffi/` 承载。
