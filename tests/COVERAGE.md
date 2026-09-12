# 测试覆盖审计(v0.5 规范 ↔ 测试集)

> **第六批补测已落地(2026-09-12,v0.7 三项松绑;规范已修订至 v0.7)。**
> R 线 compiler-rust 新增三 suite:oror(4,`||` 真值表/优先级/位置消歧/续行)、
> breakc(8,break/continue 绑最近循环/E2070/E2071/E2072/fmt/C 发射)、
> infer(5,调用点推断/E2060/E2061/显式并存);夹具 `compiler-rust/tests/fixtures/`
> (04d/04e/04f 已提升共享 tests/,自举+宿主双过;04f 结构化形态、04d/04f 负例、04e 闭包/Drop 负例留 R 线 fixtures)。spec 修订:§1.3/§1.5/§1.6/§3.7/§3.9.1/
> §4.0(新)/§4.2/§4.3/§4.4/§4.7 + §10 五个新码。
> 设计:`docs/superpowers/specs/2026-09-12-v07-operator-constitution.md`。
>

> **第五批补测已落地(2026-09-07):路线图锚点语料 `tests/roadmap/`。** R 线(R-P2…R-P7)测试先行,
> 22 个单文件 + 2 个多文件用例钉死容器/std 能力/捕获闭包/迭代器/模式守卫/fmt 夹具/panic 源位置/
> arena Send/comptime 常量尺寸/path 依赖的语法与语义裁决;由 `roadmap_suite.rs` 表驱动锚定状态
> (首跑实测:绿 5 / 锚 17),主流套件零回归。设计:`docs/superpowers/plans/2026-09-07-r-tests-design.md`。
> 附带修复:解释器 `Index` 第二求值路径越界为 Rust panic(击穿测试进程)→ 改 `Flow::Panic`(与主路径一致)。
> 新码注册:**E3070**(闭包可变捕获未显式 Mutex 包装,R-P3a)。
>
> **第四批补测已落地(2026-09-04):语言符合性覆盖 100%。** 覆盖按三层口径核算:
>
> | 口径 | 范围 | 状态 |
> |---|---|---|
> | **A 语言符合性(.ct 测试)** | 规范语义/语法/诊断码,116 项 | **116/116 = 100%**(第四批补齐:Simd、trace 位置链、stdweb 锚、FFI(c_src)、`} else {` 排版、显式 Void;AnyError 设计随 v0.5 钉死) |
> | **B 工具链行为(compiler 集成测试)** | JSON 诊断 schema、`--deterministic`、`lint --trusted`、bare 体积检查,4 项 | 归属 P1-D 实现计划 |
> | **C 构建与性能门禁(CI 基准)** | own ±5% / GC ≤15% / bare <100KB | 归属 P2/P3 阶段出口 |
>
> 测试集现状:**61 个冻结语料文件 + 24 个 roadmap 锚点文件**(86 个 .ct,meta_check 全过)。
> 注:锚定文件在实现就绪前保持"规范锚"状态(红),实现跟上即翻转——这是测试先行的设计本意。
> **行项 100% ≠ 用例空间 100%**:输入空间的深度覆盖(边界值/组合/并发交错)由 P1-D 的属性测试与模糊测试承接。

## 第五批补测清单(2026-09-07,roadmap 锚点)

`r2a_list/r2a_map/r2a_set/r2a_sb`(容器 API 面:`Type[T]()` 构造、get→Option 无 null、
keys() 迭代序不承诺)、`r2a_list_oob.panic`(越界 "index out of bounds")、
`r2a_container_send`(元素 Send 则容器 Send)、`r2a_container_alloc.neg`(E3040 覆盖容器,已生效)、
`r2b_fs_fake/r2b_time/r2b_env`(std.fs/time/process 能力形状 + Fake 注入)、
`modules/caps_fs`(E4010 键集扩展,已生效)、`r3a_capture`(拷贝捕获/闭包逃逸/Mutex 可变捕获,解释器已绿)、
`r3a_capture_var.neg`(E3070 新码)、`r3b_adapters/r3b_iter_trait`(惰性适配器链、Iterator trait + for)、
`r3c_guards/r3c_guard_exhaustive.neg`(守卫/或模式;守卫臂不参与穷尽)、
`r2d_fmt_fixture/r2d_fmt_chain`(fmt 幂等夹具,行为测试身份)、
`r4b_panic_location.panic`(panic 消息携带 `.ct:行号`,R-P4b 翻转判据)、
`r4c_arena_send.neg`(arena 非 Send,已生效)、`r5a_const_size`(const 作数组尺寸,已实现)、
`r5a_semantics`(comptime 语义回归锚)、`modules/path_dep`(`[deps] path=` 依赖格式,R-P7)。

## 第四批补测清单(2026-09-04)

`09_simd.ct`(splat/lane/to_array/元素级白名单)、`10_trace.ct`(`?` 记录 + `context` 物化 AnyError.trace)、`10_web_dom.ct`(`//@ target: web` + `stdweb.dom` 最小 API 锚)、`modules/ffi_math/`(`extern "c"` + `#[trusted]` + `c_src/` 构建规则);`01_basics.ct` 补多行 `} else {` 与显式 `Void`;`05i_deep_cause.ct` 的 `middle` 签名随 AnyError 修正。
规范 v0.5 配套:`extern` 关键字与 EBNF、`AnyError`/`Error.trace`/两段式位置链、`Simd`/`Str.contains` 前奏行、§9.2 stdweb 最小 API、§9.6 extern 声明示例、README §6 `c_src/` 规则。

## 第三批补测清单(2026-09-04)

`00_doctest.ct`(doc-test 格式锚)、`01d_strings.ct`(转义/插值链含索引/字节与字符)、`01e_multiline_chain.ct`(§1.6 首点式换行)、`02e_match_patterns.ct`(字面量/struct/变体模式)、`03c_str_string.ct`(Str/String/隐式降格)、`03d_props_traits.ct`(prop/默认方法/超 trait 组合/空 impl)、`03e_generics_types.ct`(泛型 struct+bound+class+`@derive(Show,Eq)`)、`03h_utf8_boundary.panic.ct`、`05f_must_use.lint.ct`(W8020)、`05g_into_gc_isolation.ct`(深拷贝隔离)、`05i_deep_cause.ct`(两层 cause 链)、`06g/06h_noalloc_trait`(`#[no_alloc]` 函数与 trait 契约 E3040)、`08b_nospawn.neg.ct`(E4030)、`08d_comptime_effect.neg.ct`(E6020)、`modules/comptime_budget/`(E6010,首例按 `[comptime] budget_ms` 配置预算)。
配套:注册 E4030/E6010/E6020;§1.4 插值扩展到索引;前奏 Str 补 `slice(range)`;EBNF `ClassItem` 补 `PropImpl`、`Method` 补 `{ DeclAttr }`(trait 方法上的 `#[no_alloc]` 由此合法)。

## 第二批补测清单(2026-09-04)

**P0 —— v0.4 新特性:** `03f_slices.ct`(切片二分/T[N] 退化/隐式只读化)、`03g_fn_types.ct`(函数类型/多参闭包)、`06b_slice_nonsend.neg.ct`、`06c_static_nonsend.neg.ct`(E3031)
**P1 —— 核心语义:** `05d_drop.ct`(RAII 逆序)、`02b_option_propagation.ct`(Option `?`/expect/`T?`/元组变体/块臂)、`04b_logic.ct`(`&&`/`!`/德摩根/`%`符号/复合赋值/遮蔽/range 值/else-if)、`03b_numeric_widths.ct`(宽度全集/进制/分隔/自适应/`as` 截断)、`02d_divzero.panic.ct`、`05b_panic_join.ct`(`panic()`/Never/`join_or`)、`05e_own_gc_mut.neg.ct`(E3060)、`06f_parallel.ct`(数据并行)、`06d_globals.ct`(static let/Global/Atomic)、`06e_cancel.ct`(取消传播)
**基建:** `01c_parse.neg.ct`(E1001 注册 + 比较不可链)、`tests/modules/` 五例(use_ok 组导入+pub/pub(pkg)、orphan E5010、circular E5020、visibility E2020、caps E4010);README §6/§7 多文件格式与 std 隐式链接规则;前奏表补 Atomic/Global/Drop 行;数组字面量归属(`T[N]`)写入 §3.6。

---

## 首批审计基线(v0.4 规范 ↔ 18 个测试文件,历史)

方法:逐条对照 `docs/spec/` v0.4 的规范性特性与 `tests/*.ct`,标注 ✅ 已覆盖 / 🟡 部分 / ❌ 未覆盖 / ⛧ 需基建(多文件或后端)。
统计:**121 项规范性特性中,✅ 48(40%)、🟡 9、❌ 60、⛭ 4**。结论:核心展示路径(Send/own/错误模型/值引用二分)覆盖扎实,**模块系统整章为零、v0.4 新特性大半未测**。

## §1 词法与语法(15 项:✅5 🟡3 ❌7)

| 特性 | 状态 | 锚点/缺口 |
|---|---|---|
| 整数默认 I32 / 浮点默认 F64 / 布尔 / 简单插值 / 行注释 | ✅ | `01_basics.ct` |
| 字面量后缀 | 🟡 | 仅 `u8`;`f32/i64/usize` 等未测 |
| 块值/`else` 同行排版 | 🟡 | 隐含于各测试,无专门用例 |
| EBNF 产生式整体 | 🟡 | 主干覆盖,角落未测 |
| 进制 `0x/0o/0b` + `_` 分隔 | ❌ | |
| 期望类型自适应(`let x: U64 = 5`) | ❌ | |
| 字符串转义(`\n \{ \u{}`) | ❌ | |
| 插值字段/方法链(`{clock.now()}`) | ❌ | |
| 多行首点链式排版(§1.6 新规则) | ❌ | **v0.4 新规则无测试** |
| 比较不可链(parse 负例) | ❌ | 需先注册 E1xxx 码 |
| doc-test(`///` 代码块) | ❌ | 格式亦未定义 |

## §2 名字与模块(8 项:❌8 —— 整章未覆盖)

多文件 `use`/全限定路径、组导入 `{}`、`pub`、`pub(pkg)`、遮蔽、孤儿规则 E5010、循环依赖 E5020、caps 越权 E4010——**全部缺失**。根因:测试格式目前只支持单文件,多文件项目格式未定义(见"基建缺口")。

## §3 类型系统(25 项:✅10 🟡4 ❌11)

| 特性 | 状态 | 锚点/缺口 |
|---|---|---|
| 检查算术+回绕、Option、Result、enum 具名/单元变体、元组、定长数组、泛型 fn、Box、`&Trait` 隐式上行 | ✅ | `01/02/03/04/07` |
| Str/String(`to_string` 仅出现在 neg)、`@derive`(仅 Error)、数组字面量类型归属、切片可变写(仅 bare) | 🟡 | |
| 数值宽度全集(I8..I64/ISize…) | ❌ | 仅用 I32/U8/U32/U64/F64 |
| `as[T]()` 显式转换 | ❌ | |
| `T?` 语法糖 | ❌ | |
| `T[N] → T[]` 退化、隐式转换清单(String→Str、T[]→&T[]、值→&T) | ❌ | |
| **`&T[]` 只读视图二分(v0.4)** | ❌ | **新特性无任何测试** |
| enum 元组变体(`Timeout(U64)`) | ❌ | |
| match 字面量模式、struct 模式 | ❌ | |
| `prop` 属性(定义+使用) | ❌ | 前奏 `.len` 隐含,用户定义无 |
| trait 默认方法体、超 trait 组合(`Env: Clock + Fs`) | ❌ | 仅 `: Cap` 标记 |
| 泛型 struct/class、bound(`T: Show`)、`@derive(Show, Eq)` | ❌ | |
| `Simd[E, N]` | ❌ | |
| **函数类型 `fn(...) -> T`(v0.4)** | ❌ | **新特性无测试** |

## §4 表达式(16 项:✅9 🟡2 ❌5)

✅ 优先级、`+=`、if 表达式、match 穷尽+通配、for/range/`..=`、while、闭包(零参/单参/`|var|`)、UFCS、`or`。
🟡 `%`(仅正数场景)、`void`(隐含)。
❌ **`&&`/`!`/德摩根**(全测试集没有用过逻辑与!)、其余复合赋值(`-=` `*=` `/=` `%=`)、else-if 链与 match 块臂、range 作为值/多参类型闭包、`assert_ne`。

## §5 错误模型(13 项:✅4 ❌9)

✅ Result `?`、`or` 双形态、`context`/`cause`、溢出 panic。
❌ **Option `?` 传播**、`expect` 于 Option、显式 `panic()`、**除零 panic**、`join_or` 任务 panic 重抛、W8020 must-use、`ScopeCancelled`、深 cause 链(≥2 层)、`?` 位置链元数据。

## §6 内存模型(12 项:✅7 ❌5)

✅ 值/引用二分、W8010、own+`into_gc`、E3040(own)、E3050、bare 显式分配、bare E3040。
❌ **Drop/RAII(整个确定性析构零测试)**、E3060(own 内对 GC 值可变借用)、`#[no_alloc]` 注解与 trait 契约、`into_gc` 深拷贝隔离性(arena 改动不影响 GC 值)、ISR 约束。

## §7 并发(15 项:✅7 ❌8)

✅ scope/spawn/join、Send 正例、E3010、E3020、channel 往返、`with`/`with_mut`(v0.4)、E3030。
❌ **E3031(非 Send 静态)**、`static let` 行为、`Global[T]`、`Atomic`、**`parallel.map`(数据并行零测试)**、取消传播/`ScopeCancelled`、**`T[]` 经 channel 的非 Send 负例(v0.4)**、`join_or`。

## §8 效果与 comptime(7 项:✅3 ❌4)

✅ 能力注入+`Cap`、E4020、comptime fn/const。
❌ `#[no_spawn]` E4030、comptime 预算 E6010、E6020/E6030、单态化预算。

## §9 档位与互操作(6 项:✅1 ❌3 ⛧2)

✅ bare target 标记+arena。❌ `extern "c"`+`#[trusted]` FFI、Simd 硬件利用、产物体积口径。⛭ JS 桥、WasmGC(需对应后端)。

## §10 诊断与符合性(4 项:✅2 ⛧2)

✅ 错误码注册表(meta_check 同步)、四类测试标记格式。⛭ JSON 诊断 schema、确定性模式(实现期验证)。

## 基建缺口(阻塞 ~15% 的剩余覆盖)

1. **多文件测试格式未定义**——§2 整章(8 项)+ FFI 依赖它。需在 `tests/README.md` 定义:目录即项目(`tests/modules/xxx/{Ctron.toml,src/...}`)、`//@ fail` 归属文件行号、neg 判定范围。
2. **E1xxx 解析错误码零注册**——比较不可链等 parse 负例无法编写;需先扩注册表(如 E1001 通用语法错误)。
3. **doc-test 无格式样例**——`///` 代码块的编译运行语义需要首个样例钉死。
4. (规范小缺口,审计中发现)数组字面量 `[1,2,3]` 的类型归属(`T[N]` 直标还是先 `T[N]` 再退化)未明确。

## 补测 backlog(26 个文件,按优先级)

**P0——v0.4 自身新特性(规范刚改,最该先钉):**
`03f_slices.ct`(T[]/`.len` 写读、`&T[]`、隐式、T[N] 退化)、`03g_fn_types.ct`(函数类型参数)、`06b_slice_nonsend.neg.ct`、`06c_static_nonsend.neg.ct`(E3031)

**P1——核心语义空洞:**
`05d_drop.ct`(RAII 逆序+panic 展开)、`02b_option_propagation.ct`(Option `?`/`expect`/`T?`)、`04b_logic.ct`(`&&`/`!`/德摩根/`%`符号/复合赋值)、`03b_numeric_widths.ct`(+`as`)、`02d_divzero.panic.ct`、`05b_panic_join.ct`(`panic()`/`join_or`)、`05e_own_gc_mut.neg.ct`(E3060)、`06f_parallel.ct`、`06d_globals.ct`(static let/Global/Atomic)、`06e_cancel.ct`

**P2——表达力补全:**
`01b_literals.ct`(进制/后缀/自适应/转义/插值链/多行链)、`02c_match_forms.ct`(字面量/struct 模式/块臂)、`03c_str_string.ct`、`03d_props_traits.ct`(prop/默认方法/超 trait)、`03e_generics_struct.ct`(泛型类型/bound/@derive(Show,Eq))、`03h_implicit_convs.ct`、`05c_must_use.lint.ct`、`06g_noalloc_trait.ct`+neg、`08b_isr.neg.ct`(E4030)、`08c_comptime_budget.neg.ct`(E6010)

**P3——基建先行:**
多文件格式定义 + `tests/modules/`(E5010/E5020/E4010/可见性/use)、E1xxx 注册 + `01c_parse.neg.ct`、doc-test 样例;FFI/P2 后端就绪后补 `09_*`。
