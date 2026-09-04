# §3 类型系统

## 3.1 类型种类

| 种类 | 语法示例 | 语义 |
|---|---|---|
| 整数 | `I8 I16 I32 I64 ISize U8 U16 U32 U64 USize` | 定宽;默认检查算术(§4.5) |
| 浮点 | `F32 F64` | IEEE-754;`ISize/USize` 与目标指针同宽 |
| 布尔 | `Bool` | `true`/`false`;无整数互换 |
| 字符串借用 | `Str` | 不可变 UTF-8 视图;`.len`(字节) `.char_len`(字符) |
| 字符串持有 | `String` | GC 堆 UTF-8;隐式降格为 `Str` |
| 结构体 | `struct` 声明 | **值类型**(§6.1) |
| 类 | `class` 声明 | **引用类型**(GC 堆,§6.1) |
| 枚举 | `enum` 声明 | 判别和(sum type) |
| 元组 | `(I32, Str)` | 匿名积;`.0 .1` 访问;单元 `()` 类型为 `Void`,值为 `void` |
| 切片 | `I32[]` | 长度+元素视图;借用语义 |
| 定长数组 | `I32[3]` | 内联连续;可隐式退化为切片(§3.6) |
| 定长向量 | `Simd[F32, 8]` | 硬件向量(§9.5) |
| 可选 | `Option[T]`,糖 `T?` | 无 null(§5.1) |
| 结果 | `Result[T, E]` | 错误传播载体(§5.1) |
| 引用 | `&T` / `&Trait` | 共享**只读**视图 / trait 对象(§3.5) |
| 装箱 | `Box[T]` | 显式 GC 堆单值;访问自动解引用 |
| 函数 | (仅经闭包/推断使用) | v0.3 无一等函数类型语法(闭包即函数值) |
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
- trait 对象:仅 `&Trait`(借用形式,v0.3);动态分发;非 Send 传播按其真实类型判定(对象携带 Send 位,§7.4)。

## 3.5 引用与 trait 对象

- `&T` = 共享只读视图:**不可变约束**,非生命周期标注。full/web 档下由 GC 保证存活;own/bare 上下文中额外遵守"不逃逸出被调方"规则(可判定,无需标注,§6.3)。
- 可变传递经 `var` 参数/`Mutex`;不存在 `&mut`。
- 上行:`&FakeClock` → `&Clock` 隐式;下行禁止。

## 3.6 隐式转换(完备清单,之外全禁止)

1. `String` → `Str`;
2. `T[N]` → `T[]`(定长退化切片);
3. `T` → `T?`(`Some` 包装)、`T` → `Result[T, E]` 仅限 `Ok` 包装于显式构造,不作隐式;
4. 值 → `&T` / `&Trait`(自动借为只读视图);
5. 整数字面量自适应(§3.7)。

数值宽窄转换必须显式:`x.as[U64]()`(内建方法,窄化语义 = 截断)。

## 3.7 类型推断与字面量自适应

- **局部推断**:`let/var` 绑定、闭包参数(可省)块内推断。
- **签名必标注**:函数参数、返回类型、字段、`const`/`static` 必须显式类型——签名即契约(P1/P5)。
- 整数字面量按期望类型解释(变量类型、比较对象、实参);无约束时默认 `I32`;浮点默认 `F64`。

## 3.8 标准前奏(隐式可用,无需 `use`)

```
类型:Option Result Box List Map Set String Str StringBuilder
     Channel Sender Receiver Mutex Atomic Global Arena Region Pool
     Simd Never Void Bool 及全部数值类型
值/函数:assert assert_eq assert_ne panic expect
变体:Some None Ok Err true false void
```

- `Option[T] { Some(T) | None }`、`Result[T, E] { Ok(T) | Err(E) }` 为普通枚举,可 match(§4.6)。
- 前奏符号可被本地声明遮蔽(lint 提示)。

## 3.9 泛型

- 语法 `fn f[T, V](...)` / `struct Pair[A, B]`;**方括号**。
- 实现方式:**单态化**默认(静态分发,零成本);单态化爆炸由编译预算约束(§8.5),超限提示改 `&Trait`。
- 值参数为 comptime(定长数组维度等,§8.4);类型级 comptime 预留。
- 泛型参数无生命周期参与(内存安全由 GC/Send 体系保证,非生命周期)。

## 3.10 与测试集的对应

`tests/03_values_refs.ct`(值/引用/Box)、`tests/04_generics_comptime.ct`(泛型/数组退化)、`tests/07_capabilities.ct`(trait/prop/impl)。
