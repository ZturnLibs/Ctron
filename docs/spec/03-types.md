# §3 类型系统

## 3.1 类型种类

| 种类 | 语法示例 | 语义 |
|---|---|---|
| 整数 | `I8 I16 I32 I64 ISize U8 U16 U32 U64 USize` | 定宽;默认检查算术(§4.5);`ISize/USize` 与目标指针同宽 |
| 浮点 | `F32 F64` | IEEE-754 |
| 布尔 | `Bool` | `true`/`false`;无整数互换 |
| 字符串借用 | `Str` | 不可变 UTF-8 视图;`.len`(字节) `.char_len`(字符) |
| 字符串持有 | `String` | GC 堆 UTF-8;隐式降格为 `Str` |
| 结构体 | `struct` 声明 | **值类型**(§6.1) |
| 类 | `class` 声明 | **引用类型**(GC 堆,§6.1) |
| 枚举 | `enum` 声明 | 判别和(sum type) |
| 元组 | `(I32, Str)` | 匿名积;`.0 .1` 访问;单元 `()` 类型为 `Void`,值为 `void` |
| 切片 | `T[]` | **可变视图**:长度+指针;元素可写仅当绑定根为 `var`(§4.2);**恒非 Send**(§7.4) |
| 只读切片 | `&T[]` | 只读视图;Send 当且仅当 `T` Send(§7.4);`T[] → &T[]` 隐式(§3.6) |
| 定长数组 | `I32[3]` | 内联连续值类型;可隐式退化为切片(§3.6) |
| 定长向量 | `Simd[F32, 8]` | 硬件向量(§9.5) |
| 可选 | `Option[T]`,糖 `T?` | 无 null(§5.1) |
| 结果 | `Result[T, E]` | 错误传播载体(§5.1) |
| 引用 | `&T` / `&Trait` | 共享**只读**视图 / trait 对象(§3.5) |
| 装箱 | `Box[T]` | 显式 GC 堆单值;访问自动解引用;载荷可变字段经别名写共享可见(v0.6 §3.11.1) |
| 函数 | `fn(I32) -> Bool` | **函数类型**:仅用于参数/返回类型位置;闭包字面量是该类型的值(§4.7) |
| 底类型 | `Never` | `panic` 等不返回表达式的类型,可协变于任何期望 |

- **无 `null`、无 `any`、无隐式数值转换、无渐进类型**(P2;拒绝清单)。

## 3.2 值语义与引用语义(二分,读者可见)

- `struct` 赋值/传参/返回**整体拷贝**(编译器可消除冗余拷贝,可观察语义不变);字段拷贝为**浅拷贝**:若字段含类引用,拷贝共享同一实例——`ctron lint` 默认对含 `var` 类字段的 struct 给 W8010。
- `class` 赋值**共享实例**;字段默认 `let`(不可变),可变字段必须显式 `var`。**含 `var` 字段的类非 Send**(§7.4)。
- `enum` 是值类型;变体载荷按字段规则。

## 3.3 `Str` 与 `String`

- 字面量是 `Str`,存静态存储,任何档位可用。
- `String` 为 `alloc` 层类型;`String` → `Str` 隐式(只读视图);`s.to_string()` 显式升级(分配,§6.5)。
- 切片按**字节索引**且必须落在 UTF-8 边界,否则 panic("invalid utf8 boundary");字素/码点迭代为只读 API。

## 3.4 trait(名义、显式实现)

```c
trait Clock {
    fn now(&self) -> U64          // 方法
    prop name: Str                // 属性:零参计算只读值,调用无括号
}

impl Clock for FakeClock {
    fn now(&self) -> U64 { return self.base }
    prop name: Str { return "fake" }
}
```

- **名义**:方法仅通过显式 `impl` 生效;孤儿规则见 §2.5。
- trait 可含默认方法体/默认属性。
- **属性(`prop`)**:零参只读计算值;必须无副作用(纯读,`#[pure]` 语义);`xs.len`、`e.message` 即属性。
- **bound**:`fn render[T: Show](x: T)`;bound 组合用 `+`(`T: Hash + Eq`)。
- **超 trait**:`trait Env: Clock + Fs + Log { ... }`——实现方必须同时实现全部超 trait;能力上下文组合(§8.1)即用此机制。
- trait 对象:仅 `&Trait`(借用形式,v0.3);动态分发;非 Send 传播按其真实类型判定(对象携带 Send 位,§7.4)。

## 3.5 引用与 trait 对象

- `&T` = 共享只读视图:**不可变约束**,非生命周期标注。full/web 档下由 GC 保证存活;own/bare 上下文中额外遵守"不逃逸出被调方"规则(可判定,无需标注,§6.3)。
- 可变传递经 `var` 参数/`Mutex`;不存在 `&mut`。
- 上行:`&FakeClock` → `&Clock` 隐式;下行禁止。

## 3.6 隐式转换(完备清单,之外全禁止)

1. `String` → `Str`;
2. `T[N]` → `T[]`(定长退化切片;**数组字面量 `[a, b, c]` 的直接类型为 `T[N]`**,元素类型按上下文适配);
3. `T[]` → `&T[]`(切片只读化,Send 判定随之改变,§7.4);
3. `T` → `T?`(`Some` 包装)、`T` → `Result[T, E]` 仅限 `Ok` 包装于显式构造,不作隐式;
4. 值 → `&T` / `&Trait`(自动借为只读视图);
5. 整数字面量自适应(§3.7)。

数值宽窄转换必须显式:`x.as[U64]()`(内建方法,窄化语义 = 截断)。

## 3.7 类型推断与字面量自适应

- **局部推断**:`let/var` 绑定、闭包参数(可省)块内推断。
- **签名必标注**:函数参数、返回类型、字段、`const`/`static` 必须显式类型——签名即契约(P1/P5)。
- 整数字面量按期望类型解释(变量类型、比较对象、实参);无约束时默认 `I32`;浮点默认 `F64`。

## 3.8 标准前奏(隐式可用,无需 `use`)

### 3.8.1 类型与值

```
类型:Option Result Box List Map Set String Str StringBuilder
     Channel Sender Receiver Task Scope Mutex Atomic Global AnyError
     Arena Region Pool Simd Never Void Bool 及全部数值类型
trait:Show Eq Error Cap Clone Hash Iter
值/函数:assert assert_eq assert_ne panic expect fmt
变体:Some None Ok Err true false void
```

- `Option[T] { Some(T) | None }`、`Result[T, E] { Ok(T) | Err(E) }` 为普通枚举,可 match(§4.6)。
- 前奏符号可被本地声明遮蔽(lint 提示)。

### 3.8.2 前奏 API 最小清单(规范性:P1 必须提供;扩充走 RFC)

| 类型/trait | 成员(方法 `()` / 属性无括号) |
|---|---|
| `Option[T]` | `is_some` `is_none`(prop);`map(f)` `or(默认)` `expect(msg)` |
| `Result[T, E]` | `is_ok` `is_err`(prop);`map(f)` `or(默认)` `expect(msg)` `context(str)`(§5.4) |
| `Show` | `fn show(&self) -> Str`;`@derive(Show)` 可生成 |
| `Eq` | `fn eq(&self, other: &self) -> Bool`;`@derive(Eq)` 可生成 |
| `Error` | `prop message: Str`、`prop cause: &Error?`、`prop trace: Str`(位置链,默认空,§5.3/§5.4);`@derive(Error)` 可生成 |
| `AnyError` | 前奏错误擦除类型(class,实现 Error):`context` 的返回错误类型;`?` 向 AnyError 自动擦除(§5.4) |
| `Cap` | 空标记 trait:**能力 trait 必须继承它**(`trait Clock: Cap`),`#[pure]` 检查以此判定(§8.3);具体类型**无需也无法**单独实现 Cap——它只标注 trait 的类别 |
| 数值类型 | `as[T]()`(显式转换,窄化=截断,§3.6);`abs()` `min(a,b)` `max(a,b)` |
| `Simd[E, N]` | `Simd[E, N].splat(v)`;`lane(i) -> E`;`to_array() -> E[N]`;`+ - * /` 元素级(运算符白名单,§3.1/§9.5) |
| `Str` / `String` | `len`(字节)`char_len`(字符);`slice(range)`(字节切片,须落字符边界,§3.3);`contains(s)`;`to_string()`(分配,§6.5);`iter()` |
| `T[]` / `&T[]` | `len`(prop);`iter()`;索引 `[i]`(§4.5) |
| `List[T]` | `new()` `push(v)` `pop()` `len`(prop);索引 |
| `Arena` | `array[T](n)` `zeros[T](n)` `list[T]()`;`Arena.fixed(n)`(bare);句柄仅移动(§6.3);`into_gc()`(arena 数据出块唯一入口,§6.3) |
| `Mutex[T]` | `with(f: fn(&T) -> R) -> R`(只读访问);`with_mut(f: fn(var T) -> R) -> R` |
| `Atomic[I32]` | `Atomic[I32](init)`;`load()` `store(v)` `fetch_add(d) -> I32`(返回旧值;整数族) |
| `Global[T]` | `Global[T](name, init)`;`with`/`with_mut` 同 Mutex(§7.6;注册经 manifest 审计) |
| `Drop` | `fn drop(var self)`;值类型确定性析构(§6.4),作用域退出逆序执行 |
| `Channel[T]` | `Channel[T](cap) -> (Sender, Receiver)`;`send(v) -> Result` `recv() -> Result`(§7.3) |
| `Task[T]` | `join() -> T`(panic 重抛);`join_or() -> Result[T, TaskPanic]` |
| `fmt` | `fmt(parts: Str, values...) -> String`(插值脱糖目标,§4.11;分配) |

- 命名冲突裁决:`Mutex.with`(只读)与 `with_mut`(可变)成对——测试钉子 14 的 `m.with(|var a| ...)` 自 v0.4 起统一为 `with_mut`,`with` 仅只读。
- 本表是**最小集**而非封闭集;stdlib 其余模块(`iter`/`net`/`fs`/...)不属于前奏,需 `use`。

## 3.9 泛型(v0.6 修订:实例化/bound/嵌套语义)

- 语法 `fn f[T, V](...)` / `struct Pair[A, B]`;**方括号**。
- 实现方式:**单态化**默认(静态分发,零成本);单态化爆炸由编译预算约束(§8.5),超限提示改 `&Trait`。
- 值参数为 comptime(定长数组维度等,§8.4);类型级 comptime 预留。
- 泛型参数无生命周期参与(内存安全由 GC/Send 体系保证,非生命周期)。

### 3.9.1 实例化(v0.6)

- **泛型 fn**:调用点**显式 TypeArgs**(v0 契约,无调用点推断):`render[Pixel](p)`。每个不同的实参型别组合产生一个特化(`t_名__<实参码>`,如 `t_get__i_s`);同组合多调用点共享一份特化。
- **泛型 struct**:两路实例化——(a)**注解驱动**:`var m: Map[I32, Str] = Map { ... }`(注解型别钉实例);(b)泛型 fn 体内按签名实例化(体内 `Map { ... }` 字面量与签名同实例)。**实例化实参当前限标量/Str**(实例码字母表 I/S/B/L)。
- **嵌套泛型调用**:泛型 fn 体内可调泛型 fn,型参实参经外层绑定解析:`fn add[V: Eq](s: Set[V], v: V) { mem[V](s, v) }` 中 `mem[V]` 随外层 V 特化。
- **递归特化必须诊断**:泛型 fn 体内(直接或间接)调用自身同实例 → 编译期诊断"递归超限"(实现以预提升深度限表达;放宽需 seen 集,走 RFC)。

### 3.9.2 bound(v0.6)

- 语法:`fn render[T: Show](v: T)`、`struct Pair[A: Show + Eq, B: Show + Eq]`;`+` 连接多 bound。
- **检查点**:显式 TypeArgs 调用点,逐型参核对;违反 = **E2050**(诊断携带实参型别名)。
- **满足谓词(结构化)**:`Show`/`Eq` 由"字段全为标量(I32/I64/Bool/Str)或可满足的值类型"的 struct 满足(递归,深度限 6);原语、枚举、类不满足。`Eq` 额外认可标量原生可等(顶层)。谓词与 `.show()`/`.eq()` 的派生能力面一致(§3.11)。
- **TPar 传递**:调用点实参为外层型参名(无声明的名字)→ 放行,由实参处的外层 bound 负责;嵌套字段位不适用。
- 未知 bound 名(trait 体系落地前的预留)v0 不核对。

### 3.11.1 Box 别名共享语义(v0.6 新增)

- **赋值/传参/绑定共享堆 cell**:`let b = a`(a 为 Box)复制的是句柄;b 与 a 指向同一堆单值。经任一别名 `x.f = v`(可变字段)的写对全部别名可见。
- **自动解引用贯通读写**:`p.x` 读与 `p.x = v` 写均自动解引用;Box 值作为 fn 参数/返回传递句柄。
- **对照**:struct 赋值为深拷贝(互不影响,§3.2);struct 内嵌 Box 字段的 struct 拷贝为浅共享(box 指针被复制)。
- **锚定**:`compiler/test/fx_boxalias.ct`(别名共享/跨 fn 写/struct 拷贝对照)。

## 3.10 与测试集的对应

`tests/03_values_refs.ct`(值/引用/Box)、`tests/04_generics_comptime.ct`(泛型/数组退化)、`tests/07_capabilities.ct`(trait/prop/impl)、`tests/03e_generics_types.ct`(bound/derive,宿主↔自举一致)。

## 3.11 `@derive(Show, Eq)` 与结构化方法(v0.6 新增)

- **方法面**:值类型接收者可调 `.show() -> Str` 与 `.eq(other) -> Bool`(恰一实参)。
- **派生口径 = 结构化**:凡字段全为标量/Str/可派生值类型的 struct 即具备两方法;`@derive(...)` 注解 v0 为**声明性**(解析保留、不门控),语义由 bound(§3.9.2)核对承载。待 derive 插件体系(设计文档 §10)落地后收严为注解门控。
- **格式(规范性,双通道逐字一致)**:`.show()` 产出 `名(字段=值,字段=值)`——字段声明序、逗号分隔、`字段=值`;嵌套值类型递归同格式;标量按插值同型转换(§4.11)。`.eq(other)` 为逐字段相等(型别名相同且字段全等);实参须同静态型别。
- **能力边界**:List/Atomic/枚举/类字段不可派生(两侧同界:解释与发射一致诊断)。
- `@derive(Json)` 及其余插件派生:预留(§8.4/设计文档 §10)。
