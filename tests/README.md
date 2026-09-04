# Ctron 一致性测试集(可执行规范)

> **测试即规范**。编译器尚不存在,本目录是 Ctron 语言的第一份可执行规范(P0 阶段"一致性测试集"的种子)。每份测试文件同时钉死一批语法/语义决策(见 §3"钉子");P1 引用实现必须让本目录全绿,这是编译器的验收标准。

## 1. 文件类型

| 后缀 | 类型 | 含义 |
|---|---|---|
| `*.ct` | 行为测试 | 必须编译通过,且所有 `test` 块运行通过 |
| `*.neg.ct` | 编译失败测试 | 必须**编译失败**,且诊断中包含所有 `fail:` 列出的错误码(允许有额外诊断) |
| `*.lint.ct` | lint 测试 | 必须编译通过并产生所有 `warn:` 列出的警告 |
| `*.panic.ct` | panic 测试 | 必须编译通过、运行时 panic,且 panic 消息包含 `panic:` 的子串 |

## 2. 标记(marker)语法

标记是文件头部 `//@ key: value` 形式的注释,每行一个:

```
//@ fail: E3010          该文件必须编译失败,含错误码 E3010(可多行列出多个)
//@ msg: 必须 Send        可选;期望诊断消息包含的子串(仅配 fail 使用)
//@ warn: W8010          该文件必须产生警告 W8010(可多行)
//@ panic: overflow      运行时 panic 消息须包含 "overflow"(仅 *.panic.ct)
//@ target: bare          限定目标档位 full|web|bare(缺省 full,限定 bare 的文件不得在 full 下编译)
```

规则:

- 行为测试文件内**禁止**出现 `fail/warn/panic` 标记(它们属于对应后缀的文件)。
- 一切 `test "名称" { ... }` 块运行通过才算通过;一个文件可含多个 test 块。
- `test` 块内可用前奏断言:`assert(cond)`、`assert_eq(a, b)`、`assert_ne(a, b)`、`expect(msg)`(Option/Result 取值,失败即 panic)。
- 今天如何验证测试集自身:`python3 tests/meta_check.py`(校验标记一致性、错误码合法性、命名约定)。
- P1 之后:`ctron test tests/` 全量执行(行为/panic 跑运行,.neg/.lint 跑编译诊断比对)。

## 3. 钉子(写测试时被迫钉死的决策, supersede 设计文档 v0.2 的对应草案,待并入 v0.3)

写具体测试代码时,以下决策被明确钉死(多数是 v0.2 未定义或用了两可记法之处):

1. **泛型一律方括号** `Result[I32, E]`、`Option[T]`、`Channel[U8](4)`——不用 `<>`(消除 `a<b>c` 解析歧义,tokenizer 友好;与 `Simd[F32, 8]` 统一)。**这修正 v0.2 §4.2 示例的记法。**
2. **字面量类型**:整数字面量默认 `I32`,浮点默认 `F64`;后缀显式指定(`255u8`、`2.5f32`);整数字面量在期望类型明确时自适应(如 `let x: U64 = 5`)。
3. **块即表达式**:块的最后一条表达式是块值;`return` 提前退出函数;语句换行分隔,**无分号**。
4. **闭包**:`|x| x + 1`、`|| expr`、多参 `|a, b|`;可变参数 `|var a| { ... }`。
5. **循环**:`for x in iter`;range `0..n` 左闭右开、`0..=n` 双闭;`while cond`。
6. **回绕运算**:`+%`/`-%` 显式回绕;默认检查算术,溢出 panic(消息含 "overflow")。
7. **`or` 中缀**:`opt.or(默认)` 与 `opt or 默认` 等价(Option/Result 取值失败给默认);`expect(msg)` 取值失败 panic。
8. **match**:臂以换行分隔(无逗号)、`=>` 分支;通配 `_`;穷尽性编译期强制(E2030)。
9. **struct/class 构造**:`Type { field: value }` 字面量;字段默认 `let`,可变字段显式 `var`;`Box[T](v)` 显式装箱,字段/方法访问自动解引用。
10. **trait 与 impl**:方法体写在 `impl Trait for Type`;`&Trait` 特征对象参数,隐式向上转型;类可另有无继承方法。
11. **derive 注解** `@derive(Error)`(插件展开)与编译器注解 `#[no_alloc] #[no_spawn] #[pure] #[trusted]` 两族并存。
12. **错误链**:`Error` trait 含 `.message: Str`、`.cause: Error?`;`result.context(str)` 附加上下文,`?` 自动累积(§4.5 的落地)。
13. **字符串**:字面量是 `Str`(不可变借用); owned 是 `String`(GC 分配);`s.to_string()` 转换;插值 `"hi {name}"`。
14. **scope 并发**:`scope { |s| ... }` 是表达式(块值为结果);`s.spawn(closure) -> Task[T]`,`join() -> T`(任务 panic 则重抛);`Channel[T](cap) -> (Sender[T], Receiver[T])`,`send/recv -> Result`(取消作用域返回 `Err(ScopeCancelled)`);`Mutex[T](v)`:`m.with_mut(|var x| ...)` 独占可变访问、`m.with(|x| ...)` 只读(v0.4 拆分)。
15. **own 块 API**:`arena.array[T](n) -> T[]`、`arena.zeros[T](n) -> T[]`、`arena.list[T]()`、`.push(x)`、`.into_gc()`;arena 句柄**仅移动**(赋值即 move,再用 = E3050)。
16. **bare 档**:分配器作参数传递;`Arena.fixed(n)` 构造静态 arena;一切隐式分配 = E3040。
17. **`static let` 合法,`static var` 不存在**(E3030);可变全局唯一路径是 `Global[T]`(§7.3)。
18. **属性访问无括号**:`xs.len`、`e.message`(读属性);方法调用带括号。

## 4. 错误码注册表(v0 种子;与 `meta_check.py` 中的注册表保持同步)

| 码 | 含义 |
|---|---|
| E2010 | 类型不匹配 |
| E2020 | 未解析的名称 |
| E2030 | match 不穷尽 |
| E3010 | spawn 捕获了非 Send 值 |
| E3020 | channel 收发非 Send 类型 |
| E3030 | `static var` 不存在 |
| E3031 | 非 Send 类型作为全局/静态存储 |
| E3040 | no_alloc 上下文(own 块/`#[no_alloc]`/bare 档)中出现 GC/String 分配 |
| E3050 | own 块内 move/borrow 违规(含 use-after-move) |
| E3060 | own 块内对 GC 值可变借用 |
| E4020 | `#[pure]` 函数含副作用(能力 I/O) |
| E5010 | trait 孤儿规则违规 |
| E5020 | 循环依赖 |
| W8010 | struct 含可变类引用字段(拷贝为浅共享) |
| W8020 | must-use 结果被丢弃 |

(错误码分段:E1xxx 解析;E2xxx 类型;E3xxx 内存/并发;E4xxx 效果;E5xxx 模块;E6xxx comptime;W8xxx lint。新码先加注册表再使用。)

## 5. 与规范的关系

- **语言规范(`docs/spec/` v0.4)是语义与语法权威**;本目录的"钉子"是测试先行的裁决记录,冲突处以规范为准(规范 v0.4 已吸收全部钉子)。
- 每个 `*.neg.ct` 是一条类型/并发/内存规则的**可执行反例**;每个 `*.ct` 是一条语义承诺。
