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
19. **`@derive(Show, Eq)` 结构化方法(v0.6)**:值类型具备 `.show() -> Str`(`名(字段=值,...)`,声明序,双通道逐字)与 `.eq(other) -> Bool`(逐字段,同静态型别);派生为结构化(字段全标量/Str/可派生 struct),注解 v0 为声明性,详见 `docs/spec/03-types.md` §3.11。
20. **bound 核对(E2050,v0.6)**:`[T: Show + Eq]` 于显式 TypeArgs 调用点核对;满足谓词结构化(标量/Str/嵌套 struct;原语/枚举/类不满足;Eq 标量原生可等);TPar 实参传递放行;未知 bound 名 v0 不查。
21. **泛型调用点显式 TypeArgs(v0 契约)**:`f[I32](x)`;无调用点推断;嵌套泛型调用经外层型参解析;递归特化 = emit 期诊断"递归超限"。std 容器为 `Map[K, V]`/`Set[V]`(函数式 API,`compiler/test/stdpkg/`)。

## 4. 错误码注册表(v0 种子;与 `meta_check.py` 中的注册表保持同步)

| 码 | 含义 |
|---|---|
| E1001 | 解析错误(通用语法违规;含比较不可链) |
| E2010 | 类型不匹配 |
| E2020 | 未解析的名称 |
| E2030 | match 不穷尽 |
| E2050 | bound 不满足(泛型实参不满足型参 bound) |
| E2060 | 无法推断类型实参(v0.7;请显式标注) |
| E2061 | 类型实参候选冲突(v0.7) |
| E2070 | break/continue 出现在循环外(v0.7) |
| E2071 | break/continue 越过带 Drop 局部的作用域(v0.7) |
| E2072 | break/continue 穿越闭包边界(v0.7) |
| E4040 | `#[trusted]` 仅限 extern "c" 声明 |
| E4041 | `#[repr(c)]` 仅限 struct 声明(v0.6 §9.6) |
| E4042 | 捕获闭包作 C-ABI 回调实参(C 函数指针无 env 槽;v0.6 §9.6) |
| E4050 | 类直接持有需确定性释放的资源字段(§6.2) |
| W8050 | extern "c" 未标记 `#[trusted]`(信任边界须可枚举审计) |
| W8051 | repr(c) struct 含非 C-ABI 字段(容器/能力类型;v0.6 §9.6) |
| W8052 | extern 形参/返回非 C-ABI 类型(容器/能力类型;v0.6 §9.6) |
| E3010 | spawn 捕获了非 Send 值 |
| E3020 | channel 收发非 Send 类型 |
| E3030 | `static var` 不存在 |
| E3031 | 非 Send 类型作为全局/静态存储 |
| E3040 | no_alloc 上下文(own 块/`#[no_alloc]`/bare 档)中出现 GC/String 分配 |
| E3050 | own 块内 move/borrow 违规(含 use-after-move) |
| E3060 | own 块内对 GC 值可变借用 |
| E3070 | 闭包可变捕获未显式 `Mutex[T]` 包装(R 线 R-P3a,v0.6 §4.7 草案) |
| E4010 | 能力使用超出 manifest 声明 |
| E4020 | `#[pure]` 函数含副作用 |
| E4030 | `#[no_spawn]` 上下文 spawn |
| E5010 | trait 孤儿规则违规 |
| E5020 | 循环依赖 |
| E6010 | comptime 预算超限 |
| E6020 | comptime 副作用/不确定 |
| W8010 | struct 含可变类引用字段(拷贝为浅共享) |
| W8020 | must-use 结果被丢弃 |

(错误码分段:E1xxx 解析;E2xxx 类型;E3xxx 内存/并发;E4xxx 效果;E5xxx 模块;E6xxx comptime;W8xxx lint。新码先加注册表再使用。)

## 5. 与规范的关系

- **语言规范(`docs/spec/` v0.5)是语义与语法权威**;本目录的"钉子"是测试先行的裁决记录,冲突处以规范为准(规范已吸收全部钉子)。
- 每个 `*.neg.ct` 是一条类型/并发/内存规则的**可执行反例**;每个 `*.ct` 是一条语义承诺。

## 6. 多文件测试(tests/modules/)

> **格式迁移注(2026-09-16,提案待评审)**:包清单将迁往 CTCL `Ctron.ctcl`(规范性定义与迁移计划见 `docs/superpowers/specs/2026-09-16-config-language-v1.md` §11);迁移落地前,本节所述 `Ctron.toml` 与 `meta_check.py` 校验保持原状。

目录即最小包:`tests/modules/<case>/`,须含 `Ctron.toml` 与 `src/*.ct`(入口约定 `src/main.ct`)。

- **入口文件的标记决定类型**:`//@ fail:` → neg、`//@ warn:` → lint、`//@ panic:` → panic;无标记但含 `test` 块 → 行为;**无标记且无 test 块 = 普通源码**(不做标记检查,如 `circular/src/b.ct`)。
- neg/lint/panic 的判定作用于**整个包**的编译/运行结果;行为用例如常跑 `test` 块。
- 用例目录可含 **`c_src/*.c`**:随包编译并链接(FFI 用例,`extern "c"` 声明语法见规范 §9.6;锚定用例:`modules/ffi_math/`)。
- `meta_check.py` 对每个用例校验 `Ctron.toml` 存在性与标记规则。

## 6b. FFI 用例(tests/ffi/)

规范 §9.8 承诺的 FFI 用例落点(自举发射面专测;解释器无 FFI 口径,不跑运行面):

- **行为夹具**(子目录含 `c_src/`,入口 `src/main.ct`):`ctron-emit` 发射 C → `cc` 同批编译 `c_src/*.c`(编译期符号链接)→ 原生运行 `test` 块;包约定同 §6(`Ctron.toml`)。
- **根下 `*.neg.ct` / `*.lint.ct`**:与 §1/§2 同标记语义,经 `bin/ctron-cc run` 判定。
- 验收:`sh tests/ffi/run.sh`(独立)与 `compiler/test/suite.py` 的 `ffi/` 小节(同口径);CI 冒烟见 `compiler/test/smoke.sh` §3j;性能见 `compiler/test/bench_ffi.sh`。
- C 侧 ABI 契约:`tests/ffi/ctron_abi.h` 镜像发射器预发 typedef(`ct_i`/`ct_fn1..3`/`ctron_view_*`),发射面为唯一真源。
- 锚定用例:`callback/`(C-ABI 回调)、`repr_c/`(`#[repr(c)]` 按值往返)、`str_marshall/`(Str 编组)、`abi_width/`(逐宽度映射)。

## 7. std 的可用性

`tests/` 根下的单文件测试隐式链接 std(full 档),可直接 `use std.iter.{parallel}`;modules 用例按各自 `Ctron.toml` 声明(依赖/能力)。

## 8. doc-test

`///` 文档注释中的 ```c 围栏代码块会被**编译并执行**(断言失败 = 测试失败),块内可用前奏断言与被文档声明的符号。格式锚样例:`00_doctest.ct`。

## 9. 路线图锚点语料(tests/roadmap/)

**测试先行**的 R 线路线图语料(设计:`docs/superpowers/plans/2026-09-07-r-tests-design.md`,
上游:`docs/superpowers/specs/2026-09-07-r-roadmap.md`)。文件命名 `r<里程碑>_<主题>.ct`,
标记/后缀约定与 §1 完全一致,`meta_check.py` 同样校验。

- **红 = 规范锚**:为尚未实现的特性钉死语法/语义(先例:`09_simd.ct`/`10_web_dom.ct`),
  由 `compiler-rust/tests/roadmap_suite.rs` 表驱动断言其"今天的锚定状态"
  (Green/RunRed/CheckRed/NegPending/NegGreen/PanicMsgRed);主流四套件与 campaign 跳过 `roadmap/` 前缀。
- **翻转协议**:里程碑落地 → 该文件迁出 roadmap/(进 tests/ 根 + 主套件登记)→ 锚点表删行,
  进度表(绿/锚计数)即路线图燃尽图。
- 多文件用例在 `roadmap/modules/`,同 §6 约定。
