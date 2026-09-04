# §5 错误模型

原则:错误分两类——**可预期错误**(进入类型,必须处理)与 **bug**(panic,只在任务边界可捕获)。无异常机制(拒绝清单)。

## 5.1 载体类型(前奏定义)

```c
enum Option[T] { Some(T) | None }
enum Result[T, E] { Ok(T) | Err(E) }
```

- 无 null:`T?` 只是 `Option[T]` 语法糖;解包仅经 `?`、`or`、`expect`、`match`。
- `E` 必须实现 `Error` trait(§5.4)才可作为 `?` 传播的错误类型。

## 5.2 取值与默认

| 形式 | Option | Result | 失败行为 |
|---|---|---|---|
| `x or 默认` / `x.or(默认)` | 取 Some 值 | 取 Ok 值 | 返回默认 |
| `x.expect(msg)` | 取值 | 取值 | panic(msg + 原值 Show) |
| `x?` | 传播 None | 传播 Err | 提前返回(§5.3) |
| `match` | 穷尽分支 | 穷尽分支 | — |

## 5.3 `?` 传播

- `expr?` 合法**仅当**所在函数返回 `Result[_, E']`(E 实现且可转换/同一)或 `Option[_]`。
- 语义:`Err(e)` → 立即 `return Err(转换(e))`;`None` → `return None`。
- **位置元数据(记录/物化两段式)**:每次 `?` 将调用点(文件:行)**记录**进位置链(诊断元数据,不改变 `E` 类型,实现可用伴随后设数据);`context` 与顶层失败打印负责**物化**为 `AnyError.trace` / 诊断输出。release 可由构建配置关闭采集。错误链用于人审与 agent 修复定位。

## 5.4 `Error` trait 与错误链

```c
trait Error {
    prop message: Str          // 人读摘要
    prop cause: &Error?        // 根因链;无根因为 None
    prop trace: Str            // 位置链:file:line 以 "; " 连接,默认空(§5.3)
}

@derive(Error)                  // 为 enum 生成实现(§8.3)
enum HttpError { Timeout(U64) | BadStatus(I32) }
```

- **`AnyError`**(前奏 class,实现 `Error`)是**错误擦除类型**:`message` = 最近上下文,`cause` = 被包装错误,`trace` = 累积位置链。
- `result.context(msg) -> Result[T, AnyError]`:包装错误并**物化**位置链(message = msg,cause = 原错误,trace 追加当前 file:line)。
- `?` 向返回 `AnyError` 的函数传播时,任意 `E: Error` **自动擦除转换**(§5.3 "可转换"的唯一内建形态)。
- 诊断展示:错误值被 `Show` 时按链输出 `msg … while msg2 … while …(trace)`。

## 5.5 panic

- `panic(msg: Str) -> Never`;表达 bug(不变量破坏、溢出、越界、断言失败)。
- 展开规则:**任务边界捕获**——panic 沿栈展开至所属任务,任务句柄 `join()` 返回 `Err(TaskPanic)`,或经 scope 作用域树传播(§7.2);主任务 panic = 进程以非零码退出并打印位置链。
- **不经 panic 传递资源责任**:RAII drop 在展开时保证执行(§6.4);GC 内存不受影响。
- `assert/assert_eq/assert_ne/expect` 失败即 panic。

## 5.6 设计裁决与理由(规范性)

- 无异常:控制流必须可见(P1);agent 全 catch 烂恢复是实证反模式。
- 无错误码枚举强制:错误即值,`Error` trait 统一链式上下文;库可自由定义 E 类型。
- `Result` 必须被消费:`Result`/`Option` 返回值被丢弃 = W8020 警告(must-use)。

## 5.7 与测试集的对应

`tests/02_option_result.ct`(传播/取默认/错误链)、`tests/02_match_exhaustive.neg.ct`(穷尽性)、`tests/07_pure.neg.ct`(能力调用非 pure)。
