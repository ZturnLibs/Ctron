# Ctron FFI 支持分析与缺口补齐报告(v0.6,2026-09-15)

> 对象:现役自举版本 `compiler/`(Ctron 写的编译器,发射 C 后端);规范依据 `docs/spec/09-profiles-ffi.md` §9.6/§9.8。
> 本文分四部分:补齐前的现状审计 → 本次补齐内容 → 与主流语言 FFI 对比 → 剩余缺陷与排期建议。

---

## 一、补齐前的现状审计

FFI 三线(自举 `compiler/`、C 宿主 `compiler-c/`、`compiler-rust/`)在本次之前均已实现 `extern "c"` 声明面:

| 层 | 现状 | 位置 |
|---|---|---|
| 解析 | `FnExt` 节点;`#[trusted]` 经 `#` 分支线程化;**ABI 字符串不校验** | `parse_decl.ct` `p_extern` |
| 语义 | W8050(extern 未标 trusted)/ E4040(trusted 用于非 extern) | `sem_main.ct` |
| 发射 | extern 原型前置 + 裸符号调用;链接 = `cc` 编译期与 `c_src/*.c` 同批(无 dlopen) | `driver_emit.ct` / `trans_expr.ct` |
| 测试 | `tests/modules/ffi_math/`(I64 加法回环)+ roadmap 负例锚 | — |

### 审计发现的缺口(按严重度)

1. **extern 原型带 `static`(链接性缺陷)**——发射器给 `FnExt` 发 `static int64_t ctron_add(...)`。static 把符号钉在 TU 内,跨 TU 链接是未定义行为;此前仅靠链接器宽容(Darwin/ELF 都恰好解析到全局定义)才可用。宿主线(`compiler-c/src/trans.c`)发的是 `extern`,三线不一致。
2. **C-ABI 回调缺失**——extern 形参写 `fn(I64) -> I64` 会按内部表示发射 `ct_clop`(fn+env 胖指针),C 侧无法构造/调用;语言没有任何把 Ctron fn 变成 C 函数指针的路径。
3. **`#[repr(c)]` 无实现,且解析面实际是坏的**——`#[attr]` 后面跟 `struct` 会掉进 p_file 的 fn 分支被误解析。规范 §9.6 "struct 按声明布局(`#[repr(c)]` 默认对 FFI 导出)"没有任何落地。
4. **Str 编组无约定**——`Str` 即 `const char*` 双向零成本直通,但 C 返回的串(C-owned)没有任何受控手段转成 Ctron 生命周期;`ctron_amalloc` arena 指针外泄给 C 的悬垂风险只存在于规范文字。
5. **边界类型无治理**——`List`/`Atomic`/`Option` 等容器类型进 extern 形参,发射后是 `ctron_list*` 一类内部指针,C 侧无从构造,静默产生不可互操作的签名(Rust 的 improper_ctypes 等价缺口)。
6. 其余:解释器口径正确拒绝 extern 调用(panic 指引发射路径);`Bool` 边界映射三线统一为 `int`(与规范 `bool(C99)` 措辞不一致,ABI 等价);发射产物中残留两处调试输出(`//DBGHINT`、`//DBG-letty`)。

---

## 二、本次补齐内容(v0.6)

### 2.1 C-ABI 回调(fn 指针形参)

- **类型面**:`ct_abi_ctype`——`F1/F2/F3`(fn 类型码)在 extern 边界映射为裸 C 函数指针 `ct_fn1/2/3`(`ct_i (*)(ct_i...)`,发射器预发 typedef),区别于内部闭包对 `ct_clop`。
- **调用点**:裸 fn 名 → `(ct_fnK)(t_<名>)` 零包装直取 C 地址(元数校验,不符即发射期硬失败);局部 fn 值(`ct_clop` 装箱)→ 摘 `->fn` 槽;捕获闭包 → 语义面 E4042 + 发射面双保险硬失败(`ctron-emit` 不跑语义检查)。
- **语义**:E4042——捕获闭包不可作 C-ABI 回调实参(C 函数指针无 env 槽;与 Rust "仅非捕获 fn 可作 `extern "C" fn` 参数" 同口径)。`ctron_abi.h` 契约:C 侧持有 Ctron fn 地址即 `ct_fnK`,可存柄、循环回调。
- **按值往返**:指针宽度经 `ct_i`(int64)统一;比较器等 C 回调签名以 `ct_i` 宽度声明(C 侧经原型隐式升宽)。

### 2.2 `#[repr(c)]` struct

- 解析:`#` 分支路由 `#[repr(c)] struct/enum`(属性名入节点尾槽,镜像 fn 的 trusted 槽;`p_struct2`/`p_enum2` 加 `anm` 参)。
- 语义:E4041(repr 仅限 struct);W8051(repr(c) struct 含非 C-ABI 字段,镜像 Rust improper_ctypes 警示面)。
- 发射:不变——C struct 本就按声明序布局,Ctron 声明序直出 C struct,与 C 侧同型定义即 ABI 兼容(含填充;测试实证 I64/F64/I32 → 24 字节)。属性在此后端是**显式 FFI 导出标记**而非布局改变,与规范"默认对 FFI 导出"一致。

### 2.3 Str 编组

- 新内建 `str_from_c(s: Str) -> Str`:发射面 `ctron_str_from_c`(C 侧 `char*` 深拷入 arena——Ctron-owned、免 free、独立于 C 侧缓冲寿命);解释面恒等(解释域无 C 堆)。所有权三约定(§9.6)由此落为:借用直通(入参)/ `str_from_c` 深拷(C-owned → Ctron-owned)/ 禁止外泄 arena 指针。
- `ct_typeof` 补 `str_from_c → "s"`(此前 `var a = str_from_c(...)` 推断回落 I32,发射成 `int32_t` 接指针)。
- 已知坑记录:Dawin 上 `size_t`(unsigned long)与发射面 `uint64_t`(unsigned long long)是不同类型别名,直接声明 libc `strlen` 会重定义冲突——size_t 面经 c_src 垫片过界(夹具有注)。

### 2.4 边界治理与修复

- **链接修复**:extern 原型 `static` → `extern`(与宿主线对齐;形参按 `ct_params_str_ext` 边界映射)。
- **ABI 校验**:extern ABI 串非 `"c"` → E1001(镜像宿主 parser_decl);缺省 ABI 合法化(裸 `extern fn`,与宿主 ast.h `abi 可 NULL` 对齐)。
- **W8052**:extern 形参/返回为容器/能力类型(List/Atomic/Option/Result/Box/Mutex/Channel/Global)→ 警示(不拦,保留高级用法)。
- **W8053**:extern 返回 fn 类型 v0 不支持(仅形参向;返回侧需值域约定)。
- 清理发射产物调试残留:`//DBGHINT`(trans_expr)、`//DBG-letty`(trans_stmt,5e007e5 引入,每个带注解 let 都打)。

### 2.5 测试与性能(§9.8 落地)

- `tests/ffi/`(规范承诺的 FFI 用例落点,`run.sh` 独立验收 + `suite.py` `ffi/` 小节):行为四件(callback/repr_c/str_marshall/abi_width)+ 负例三件(E1001/E4041/E4042)+ lint 四件(W8050/W8051/W8052/W8053),11/11。
- `compiler/test/smoke.sh` 3j 段:FFI emit+链接+运行冒烟 + 负例/lint 拦截。
- `compiler/test/bench_ffi.sh` + `tests/ffi/bench/`:FFI 边界微基准(Ctron 发射面 `-O2` vs 纯 C 同构基线,预热+3 取最小):

| 场景 | Ctron 发射面 | 纯 C 基线 | 比值 |
|---|---|---|---|
| 标量调用(200M 次) | 0.74 ns/op | 0.74 ns/op | **1.00×** |
| fn 指针回调(2 调用/op) | 0.98 ns/op | 0.24 ns/op | 4.08×(见注) |
| 32B struct 按值(150M 次) | 0.78 ns/op | 0.78 ns/op | **1.00×** |
| 256B Str 编组深拷(2M 次) | 23 ns/op | 22 ns/op | 1.05× |

  注(AArch64/Apple Silicon):cb 比值是**循环形状差**而非过界开销——两侧同为 2 次调用/op(包装 noinline + 间接回调),基线被 gcc doloop 变换为 `b.ne` 零开销分支,Ctron `while` 为 `cmp/b.lt`,绝对差 ~0.7ns/次。标量调用与 struct 按值 **与手写 C 完全同速**,达成 §9.4 "own/热点档与 C 互有 5% 内"在 FFI 面的口径。

---

## 三、与主流语言 FFI 支持对比

| 能力 | Ctron v0.6 | Rust | Go(cgo) | Zig | Swift | Nim | Python(ctypes) |
|---|---|---|---|---|---|---|---|
| extern 声明面 | `extern "c" fn` + `#[trusted]` | `extern "C"` + unsafe | `C.func` 伪包 | `extern fn` 原生 | 系统语原生 | `importc` 宏 | CDLL 手声明 |
| ABI 类型映射 | 定宽/int/Str/视图,声明序 struct | repr(C) + 宽类型全集 | CType 别名 | C 指针/类型一等 | C 直通 | 全集 | 全集手配 |
| 回调(→C) | 裸 fn 零包装 `ct_fnK`;捕获闭包 E4042 拦 | 非捕获 fn + `unsafe` | cgo 原型(重,经锁) | 裸 fn/calling convention | `@convention(c)` 仅非捕获 | `NimCtx`/裸 proc | CFUNCTYPE(装箱慢) |
| 串编组 | 直通 + `str_from_c` arena 深拷 | CString/CStr 显式 | `C.CString` malloc 拷 | 双向 allocator | String ↔ cString 拷 | cString 拷/unsafe | 手 manage |
| struct 布局治理 | repr(c) 标记 + W8051 字段警示 + 值传/值返 | repr(C) + savor 语义 | 反射对齐(脆) | extern struct | 内存布局注解 | object 导出 | Structure 手排 |
| 链接模型 | 编译期符号(`c_src/*.c` 同批 cc) | 链接属性/`+cargo` | cgo 桥 TU | 链接参数 | 模块导入 | `--passL` | dlopen |
| 动态加载 dlopen | ✗(排期) | libloading 生态 | dlopen 包 | cImport/dlopen | dlopen | dynlib 直言 | 原生 |
| 头文件消费(C→Ctron) | ✗ 手写绑定 | bindgen 生态 | cgo 自动 | `@cImport` 自动 | clang importer 自动 | c2nim/nimterp | 手 |
| 泛型/容器跨界 | 禁(W8052 警示) | 禁(编译错) | 禁 | 禁 | 禁 | 宏展开可选 | 禁 |

### 差距判读(诚实口径)

1. **已对齐主流量级**:声明面 + 信任标注(Rust unsafe 的审计化等价物)、回调非捕获限定、repr(c) struct、串深拷原语、边界治理警示面、调用开销与 C 持平——这一层 Ctron v0.6 已达到 Rust/Zig 的**机制对等**(精度逊于生态)。
2. **显著落后项(生态面)**:
   - **无 `@cImport`/bindgen 等价物**——C 头 → Ctron 绑定全手写。Zig `@cImport`、Swift clang importer、Rust bindgen 是生态护城河;这是 FFI 可用性的最大缺口。
   - **无 dlopen/符号动态解析**——纯编译期链接;无法做插件系统。Zig/Nim/Python 一等支持。
   - ** variadic(变参)extern 不支持**——printf 类函数无法直接声明(Rust/全部主流支持)。
   - **extern 返回 fn 类型 / struct 内 fn 指针字段**未支持(W8053)——Rust `Option<extern fn>` 惯用法不可用。
   - **`#[link_name]`/符号重命名**缺失——被绑符号名必须与 Ctron 名一致(前缀冲突时无解,只能 c_src 垫片)。
   - **错误传播约定**——C 错误码/errno → Result 的边界转换无语言支持(规范 §9.2 的"JS 异常转 Result"同款缺口,C 面同样存在)。
   - **捕获闭包跨界的 context-pointer 模式**——C API `void* userdata` 惯用法需手写垫片(Rust 亦如此,非独有缺口)。
3. **独有优势**:三线差分 oracle(C 宿主逐字对数)+ `#[trusted]` 包级审计(`ctron lint --trusted`)是主流语言没有的安全面;发射面即 C,免 FFI "第二 ABI"问题(Go cgo 的调度器交互成本在此模型下不存在)。

---

## 四、剩余缺陷与排期建议

| # | 缺陷/缺口 | 严重度 | 建议 |
|---|---|---|---|
| 1 | 无 C 头自动绑定(@cImport/bindgen) | 高 | P2:发射器复用解析器做 decl-level cimport(C 解析器宿主已有) |
| 2 | dlopen/`dlsym` 动态面 | 高 | P1.5:`extern "c" fn ... = dlsym(...)` 声明式动态符号 |
| 3 | 变参 extern(`...`) | 中 | 发射面直出 C 变参原型即可;sem 拦非 c ABI |
| 4 | `#[link_name]`/符号重命名 | 中 | FnExt 节点加尾槽;发射/审计随 trusted 面 |
| 5 | extern 返回 fn / struct 内 fn 指针字段 | 中 | W8053 解除:ret 映射 ct_fnK + 绑定面薄包装 |
| 6 | USize↔size_t 类型别名冲突(Darwin) | 中 | 边界码 `7` 按 target 映射 `size_t`(需 target 三元组进发射器) |
| 7 | I64 定长数组发射计数解析缺陷(`a36` 码,count 吞元素码) | 中 | 数组码改 `a<N>:<ec>` 分隔;连带 03f 全家 |
| 8 | 宿主线(compiler-c)未同步 E4041/E4042/W8051-53 | 低 | C 宿主 sem 端口(负例在 tests/ffi,不入宿主差分,无阻断) |
| 9 | 错误传播约定(errno → Result) | 低 | std.ffi 包装层先行 |
| 10 | 自举守卫:发射的 C 形参缺省(漏实参)静默通过(Ctron 侧无 arity 检查;本次 `p_enum2` 漏传踩中) | 低 | 发射器 fn 调用 arity 断言(emit 期 panic) |

### 本次顺带修复的既有问题

- smoke.sh cc_run decl 锁定:9ca8e21 改了消息(283)未改 grep(281),快面门禁在 HEAD 上常红;现按真实 decls=283 锁定。
- 发射产物 `//DBGHINT` / `//DBG-letty` 调试残留清除(后者影响所有带注解 let 的发射输出)。
