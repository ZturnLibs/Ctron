# 十分钟上手

本文是语言速览:十分钟走完变量、控制流、struct/trait、错误处理、标准库与测试。工具链安装与构建细节见[入门](getting-started.md)。

## 第 0 分钟:Ctron 是什么

**AI 原生的系统编程语言**,设计目标是"AI 写、人审":结构化诊断带稳定错误码(`E2010`、`E2080`……),语法无歧义,语言不在背后做魔法——

| Ctron 没有的 | 你用的代替 |
|---|---|
| `null` | `Option[T]` |
| 隐式数值转换 | 显式 `x.as[U64]()` |
| 运算符重载、宏 | `+` 就是加法,源码即真相 |
| 未定义行为 | 规范枚举一切合法行为 |

一套源码、两条执行路:`ctron run` 解释执行(零依赖,开发循环最快),`ctron build` 发射成可读、自包含的 C 再调本机 cc 产原生可执行。标准库以 **Ctron 源码**随工具链分发,`use std.*` 是编译期读源合并 AST——可以直接读 stdlib 源码理解 API,不需要文档。

## 第 1–2 分钟:装好,跑起来

```bash
curl -fsSL https://github.com/ZturnLibs/Ctron/releases/latest/download/install.sh | sh
export PATH="$HOME/.ctron/bin:$PATH"
```

```bash
$ ctron new hello && cd hello    # 生成 Ctron.ctcl(清单)+ src/main.ct
$ ctron run src/main.ct          # 解释执行:解析 → 检查 → 求值,一条管线
hello, ctron
$ ctron check src/main.ct --format=json
{"diagnostics":[]}
$ ctron build src/main.ct        # 发射 C → 本机 cc → 可执行
$ ./src/main
```

`run`/`check`/`new` 不需要 C 工具链,只有 `build` 需要本机 cc(`CC` 环境变量可覆盖)。清单是 CTCL 格式的 `Ctron.ctcl`:

```ctcl
pkg {
    manifest_version = 1
    name = "hello"
    version = "0.1.0"
}
```

## 第 2–4 分钟:变量、类型与函数

无分号,**换行就是语句终止**。`let` 不可变、`var` 可变,都必须初始化。函数签名(参数/返回/字段/const)必须标注类型——签名即契约;局部绑定靠推断:

```ctron
fn main() {
    let name = "ctron"          // 不可变
    var n = 3                   // 可变
    n += 1                      // 对 let 重赋值 = E2080 编译错

    let ratio = 2.5             // 浮点默认 F64
    let width: I64 = 1_000_000  // 整数字面量自适应期望类型,无约束默认 I32
    println("{name} v{n}")      // 插值:字段/方法链/索引都行,标量自动转文本
}
```

类型系统速览:定宽整数 `I8…I64/U8…U64`、`F32/F64`、`Bool`;字符串分两件——`Str`(不可变借用视图,字面量即它)与 `String`(GC 堆持有,隐式降格为 `Str`,`s.to_string()` 升级)。**没有隐式数值转换**,宽窄都走 `as`:

```ctron
fn shout(msg: Str) -> I64 {
    let n: U64 = msg.len.as[U64]()   // 显式转换,窄化=截断
    return n.as[I64]() + 1
}
```

三个马上有用的细节:

```ctron
assert_eq(7 / 2, 3)            // 整除截断;默认检查算术,溢出/除零 panic
assert_eq(255u8 +% 1u8, 0u8)   // +% -% 是显式回绕的唯一入口
assert_eq(21.double(), 42)     // UFCS:21.double() ≡ double(21)
```

## 第 4–5 分钟:控制流——全是表达式

**没有三元运算符**——`?` 与 `:` 已被其他语法占用,`if` 表达式是唯一写法:

```ctron
let label = if n > 0 { "pos" } else { "neg" }   // 作值时 else 必需
```

`match` 也是表达式,**穷尽性编译期强制**——枚举新增变体,所有非通配 match 立即报错:

```ctron
match read_file(path) {
    Some(s) => { println("got {s.len} bytes") }
    None    => { println("missing: {path}") }
}
```

循环与 range,`break`/`continue` 已转正:

```ctron
var sum = 0
for i in 0..5   { sum += i }    // 0..5 = 0,1,2,3,4
for i in 0..=5  { sum += i }    // 双闭
var n = 1
while n < 100 { n = n * 3 }
```

闭包用 `|` 界定,捕获是**创建时拷贝**(可变共享只能走 `Mutex`/`Atomic`/`Global`):

```ctron
let square = |x: I32| -> I32 { x * 2 }   // 期望函数类型处可省标注
let y = xs
    .filter(|x| x > 0)                    // 多行链用"首点式":
    .map(|x| x * 2)                       // 下一行以 . 开头=延续上一句
```

## 第 5–6 分钟:struct / class / Box 与 trait

赋值行为写在类型名上:`struct` **拷贝**,`class` **共享**,`Box` 显式上堆(访问自动解引用):

```ctron
struct Point {
    var x: I32
    var y: I32
}

class Tag {
    let id: I32               // 字段默认不可变,可变必须显式 var
}

test "三种语义" {
    var a = Point { x: 1, y: 2 }
    var b = a
    b.x = 10                  // 只改 b:a.x 仍是 1(值拷贝)

    let t1 = Tag { id: 7 }
    let t2 = t1               // 同一实例(引用共享)

    let p = Box[Point](Point { x: 3, y: 4 })
    assert_eq(p.x, 3)         // Box 自动解引用;别名写互相可见
}
```

trait 是名义的、显式实现的(孤儿规则:trait 或类型必须有一个在本包)。`prop` 是零参只读计算属性,调用**无括号**;泛型用**方括号**,bound 用 `+`;`@derive` 合成常见实现:

```ctron
trait Clock {
    fn now(&self) -> U64
    prop name: Str
}

@derive(Show, Eq)
struct Pixel {
    let x: I32
    let y: I32
}

struct Pair[A: Show + Eq, B: Show + Eq] { // 泛型 + bound
    let first: A
    let second: B
}

impl Clock for FakeClock {
    fn now(&self) -> U64 { return self.base }
    prop name: Str { return "fake" }
}
```

> trait 的完整展开(接收者两形态、trait 对象、孤儿规则、UFCS 解析顺序、v0 边界)见文末附录。

## 第 6–8 分钟:无 null 与错误处理

错误分两类:**可预期错误**进类型系统(`Option`/`Result`),**bug** 走 panic。无异常机制。

```ctron
@derive(Error)                            // 为 enum 生成 Error 实现
enum MathErr {
    DivByZero
}

fn safe_div(a: I32, b: I32) -> Result[I32, MathErr] {
    if b == 0 { return Err(DivByZero) }
    return Ok(a / b)
}

fn ratio(a: I32, b: I32, c: I32) -> Result[I32, MathErr] {
    let x = safe_div(a, b)?               // ?:Err → 提前返回并附位置元数据
    return safe_div(x, c)
}
```

取值四式(`T?` 只是 `Option[T]` 的糖):

```ctron
div_opt(1.0, 0.0).or(-1.0)                // 失败取默认(等价中缀 x or 默认)
ratio(1, 0, 1).expect("must not fail")    // 失败 panic(消息含 Show 表示)
ratio(1, 0, 1).context("computing ratio") // 包装错误链:message/cause/trace
match ratio(1, 0, 2) {                    // 穷尽分支
    Ok(v)  => println("ok {v}")
    Err(e) => println("err: {e.message}")
}
```

丢弃 `Result`/`Option` 返回值会有 must-use 警告(W8020)——错误必须被消费。

## 第 8–9 分钟:标准库、集合与测试

`use` 路径一律从包根/命名空间起,组导入带花括号,**禁止通配导入**:

```ctron
use std.str.{words, count_ch}    // std 模块:json csv uuid crypto fmap heap
                                 // iter sort strconv rand path time hashmap…
```

集合与字符串构建(前奏类型,免 use):

```ctron
var nums: List[I64] = List[I64]()    // 注意:数组字面量 [1,2,3] 是定长数组,
nums.push(42)                        // 不会自动变 List;可增长列表用 push
nums.push(7)
assert_eq(nums.len, 2)
assert_eq(nums[0], 42)               // 索引永远边界检查,越界 panic

var sb = StringBuilder()
sb.push_str("hello")
sb.push_str(", ctron")
assert_eq(sb.to_string(), "hello, ctron")
```

测试是语言内的 `test` 块,`ctron test` 直接跑;断言失败即 panic,消息带期望/实际值的 `Show` 表示:

```ctron
test "loops and ranges" {
    var sum = 0
    for i in 0..5 { sum += i }
    assert_eq(sum, 10)
}
```

```bash
ctron test tests/01_basics.ct     # 跑 test 块
ctron fmt -w .                    # 规范格式化(全仓唯一形态)
ctron lint --strict .             # 警告也红,适合 CI
```

## 第 10 分钟:彩蛋与避坑

GUI 是一等域包——UI 写在 `.ctml` 标记里,逻辑留在 Ctron,平台(解析/布局/命中/循环)全在 `gui` 域包,用户代码一行事件循环都不用写(完整可跑示例见 `examples/gui_counter`):

```xml
view Counter {
  <vbox class="root">
    <label>计数: {count}</label>
    <button class="btn" on:click={inc}>+1</button>
  </vbox>
}
```

新手最常见的十个坑:

1. **无分号**——换行即语句终止,`;` 是语法错误;`} else {` 必须同行。
2. **没有三元**——`if cond { a } else { b }` 表达式是唯一正统写法。
3. **`||` 是逻辑或**——`or` 只用于 Option/Result 取默认;位运算不在语言算符里(走 std 的 bit 模块方法)。
4. **比较不可链**——写 `a < b && b < c`。
5. **无隐式数值转换**——`n.as[U64]()`;整数字面量无约束时默认 `I32`。
6. **字符串里的 `{` 要写 `\{`**——插值内不能嵌套 `{}`;串字面量不能跨行。
7. **`[1, 2, 3]` 是定长数组**(可退化切片),要可增长列表用 `List[I32]()` + `push`。
8. **struct 赋值拷贝,class 赋值共享,Box 别名共享**——字段默认不可变,可变必须显式 `var`。
9. **`let` 不可重赋值**(含 `+=`,E2080);闭包捕获是创建时快照。
10. **match 必须穷尽**——枚举加变体后,所有非通配 match 编译期报错。

**下一步**:语言规范见[规格总览](spec/README.md)(12 章,冻结草案 v0.8);可跑的完整应用见[示例](examples.md);工具链安装与项目模式细节见[入门](getting-started.md)。

---

# 附:trait 深入——七种情况

先立一个心法:**Ctron 是"名义 trait"语言——方法只来自显式 `impl`,没有继承,没有隐式实现**。类型的行为来源是 `impl Trait for Type`、自由函数 + UFCS、(class 体内的方法/属性)。struct 声明体里**只有字段**,不写方法——行为面永远可以在 `impl` 块里找到。

## ① 基础件:trait 声明、impl、两种接收者

接收者只有两种:`&self`(只读视图)与 `var self`(可变,体内可推进状态);跨边界可变共享经 `var` 形参或 `Mutex`:

```ctron
struct Counter {
    var cur: I32
    let limit: I32
}

trait Iterator[T] {
    fn next(var self) -> T?          // var self:状态机型 trait 用可变接收者
}

impl Iterator[I32] for Counter {     // impl 必须钉死 trait 的全部型参(单态化)
    fn next(var self) -> I32? {
        if self.cur >= self.limit { return None }
        let v = self.cur
        self.cur += 1
        return Some(v)
    }
}

test "for 直迭代" {
    let c = Counter { cur: 0, limit: 4 }
    var acc = 0
    for v in c { acc += v }          // 实现 Iterator[T] 即可被 for 直接迭代
    assert_eq(acc, 6)
}
```

trait 里可以有方法签名、`prop` 签名、默认方法体——**不能有字段**;状态永远放在实现类型里。

## ② prop、默认方法体、超 trait 组合

`prop` 是零参只读计算值,调用**无括号**(`xs.len`、`e.message` 都是 prop);必须纯读、无副作用。默认方法体可以调用超 trait 成员,实现方空 `impl` 即继承:

```ctron
trait Clock: Cap {                   // : Cap 仅 I/O 能力 trait 需要(见④)
    fn now(&self) -> U64
}

trait Named: Cap {
    prop name: Str                   // trait 属性声明
}

trait Env: Clock + Named {           // 超 trait:实现 Env 须先实现两者
    fn describe(&self) -> Str {      // 默认方法体
        return "{self.name} @ {self.now()}"
    }
}

class FakeEnv {
    let base: U64
}

impl Clock for FakeEnv {
    fn now(&self) -> U64 { return self.base }
}

impl Named for FakeEnv {
    prop name: Str { return "fake" }
}

impl Env for FakeEnv {               // describe 用默认体 → 空 impl 合法
}

test "props, default methods, supertraits" {
    let env = FakeEnv { base: 100 }
    assert_eq(env.now(), 100)
    assert_eq(env.name, "fake")      // prop 无括号
    assert_eq(env.describe(), "fake @ 100")
}
```

## ③ 泛型 + bound:签名处约束,体内解锁派生方法

bound 写在型参上,多 bound 用 `+`;有了 bound,`.show()`/`.eq()` 这类派生方法才能在泛型体内调用:

```ctron
@derive(Show, Eq)
struct Pixel {
    let x: I32
    let y: I32
}

fn render[T: Show](v: T) -> Str {        // bound:T 必须满足 Show
    return v.show()                      // 没有这行 bound,这里调不了
}

struct Pair[A: Show + Eq, B: Show + Eq] {
    let first: A
    let second: B
}
```

`Show`/`Eq` 的满足是**结构化谓词**,不必逐个手写 impl:字段全为标量/`Str`/可派生值类型的 struct 即自动具备两个方法(递归,深度限 6);`List`/`Atomic`/枚举/class 字段**不可派生**。`.show()` 的规范格式为 `名(字段=值,字段=值)`(声明序、逗号分隔)。`@derive(Error)` 同理为错误枚举生成 `message`/`cause`/`trace` 实现。impl 也可以自带型参实现泛型类型:`impl[T] Seq[T, List[T]] for List[T]` 形。

## ④ trait 对象 `&Trait`:依赖注入的正统姿势

`&Trait` 是 v0 唯一的 trait 对象形态(只读借用、动态分发)。上行 `&FakeClock` → `&Clock` 隐式,**下行禁止**。值传给 `&Trait` 形参自动借用——测试时注入 fake 实现:

```ctron
trait Clock: Cap {                   // 能力 trait 必须继承 Cap(空标记 trait)
    fn now(&self) -> U64             // 具体类型不实现 Cap,它只标注 trait 类别
}

class FakeClock {
    let base: U64                    // let 字段 → 深度不可变 → Send
}

impl Clock for FakeClock {
    fn now(&self) -> U64 { return self.base }
}

fn elapsed_since(clock: &Clock, start: U64) -> U64 {
    return clock.now() - start       // 经 trait 对象动态分发
}

test "capability injection via trait object" {
    let clock = FakeClock { base: 100 }
    assert_eq(elapsed_since(clock, 58), 42)   // 值 → &Clock 自动借用+上行
}
```

`: Cap` 只在 trait 表示 I/O 能力(会进 `#[pure]`/包清单能力检查)时需要;普通 trait 不用写。std 的 `Fs` 注入(`fn read_a(fs: &Fs) -> Result[String, FsError]` + `MemFs` fake)是同一套路的实战版,见仓库 `tests/07a_cap_fs_inject.ct`。

## ⑤ 孤儿规则:E5010 与新类型模式

`impl T for X` 合法当且仅当 **trait 或类型至少一个定义于当前包**,无泛型豁免。给前奏类型直接 impl 是编译错,标准解法是包内新类型包一层:

```ctron
// impl Show for I32                  // ✗ E5010:Show 与 I32 均来自前奏
struct UserId {
    let v: I32
}

impl Show for UserId {               // ✓ UserId 是本包类型 → 合法
    fn show(&self) -> Str {
        return "UserId({self.v})"
    }
}
```

## ⑥ UFCS:自由函数即方法,解析顺序固定

`recv.m(a)` 首先按"以 `recv` 为第一实参的函数"解析,顺序为:**类型自身固有方法 → 当前可见 trait 的 impl 方法 → 前奏**。自由函数天然长在接收者身上:

```ctron
fn double(x: I32) -> I32 { return x * 2 }

test "ufcs" {
    assert_eq(double(21), 42)
    assert_eq(21.double(), 42)       // 两种写法,同一函数
}
```

std 迭代链就是"双型参 trait + UFCS 自由函数"的组合样板——`trait Seq[T, S]` 的适配器方法(`map`/`filter`/`take`…)带默认实现,终结器 `sum`/`count`/`collect` 是自由函数,一条 `xs.map(|x| x * 2).filter(|x| x > 4).sum()` 惰性零中间集合。可跑锚样例:仓库 `tests/modules/iter_adapters`。

## ⑦ v0 边界(已知限制)

- **trait 声明本身不写 `pub`**——v0 无 pub trait 语法位,trait 接口面跨模块恒可见;可见性只标在成员上。
- **trait 对象仅 `&Trait` 只读借用**;owned 形态 `Box[&Trait]` 是预留位,未落地。
- **`&Trait` 保守非 Send**;含 `var` 字段的 class 整体非 Send,跨任务须经 `Mutex`。
- `impl` 的型参**全部钉死**才能落地(`impl Iterator[I32] for Counter`);"实现方再泛化"的形态是 `impl[T] ... for Type[T]`。
- 前奏符号可被本地声明遮蔽(有 lint 提示),但孤儿规则挡住的是 impl,不是名字。
