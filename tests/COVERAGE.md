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

---

## 2026-09-15 增补(P1-B/B2/A2/P2-A/GC spike 收口语料)

上文补测 backlog(P0-P3 各项)已全部落地;本次随定宽存储/panic 展开/comptime
语句循环/E4050 资源 lint 收口,新增:

- `03i_width_checked.ct` —— 8/16/64 位定宽检查算术 + as[U64]/as[USize] 全宽度转换
- `03j_width_overflow.panic.ct` —— U8 加法上溢 panic(integer overflow +)
- `04h_comptime_stmt.ct` —— comptime 体内 var/while/for 语句与赋值(§8.4)
- `roadmap/r4d_drop_panic_unwind.ct` —— panic 展开 Drop 逆序(红:宿主 rt panic 不跑 Drop)
- `roadmap/r6a_u64_overflow.panic.ct` —— U64 加法上溢(红:宿主 64 位无符号 Add 缺上界)
- `roadmap/r6b_res_class.neg.ct` —— 类持有资源字段 E4050(红:宿主 sem 无此检查)
- `roadmap/r6c_as_u64_negsrc.ct` —— 负值源 as[U64] 模 2^64(红:宿主 conv 误用 2^128 模)

计分:suite 66/66 双侧(66 文件;panic 桶 4);roadmap 锚 25(全部在自举线按设计
表现,红 = 宿主/R 线落后点,见 roadmap 文件头"锚点"行)。

## 2026-09-15 审计增补(二)——trust 边界/续行/预算负例

- `01k_op_continuation.ct` —— §1.6 二元运算符开头续行(01e 首点式的姊妹形态)
- `roadmap/r6d_trusted_unmarked.neg.ct` —— W8050 extern 未标记 #[trusted](红:宿主无此检查)
- `roadmap/r6e_trusted_nonextern.neg.ct` —— E4040 #[trusted] 仅限 extern(红:宿主无此检查)
- `roadmap/r6f_comptime_budget.neg.ct` —— E6010 comptime 预算超限不挂起(红:宿主 check 不评 comptime const)
- `roadmap/r6g_trusted_extern.ct` —— #[trusted] extern 检查面正例(红:单文件 extern 宿主 test 无法链接;FFI 运行面由 modules/ffi_math 覆盖)

计分:suite 67/67 双侧;roadmap 锚 29;smoke 109 ok(唯一红 = std 泳道 drift)。
审计结论:自举线已实现特性的语料覆盖到位;剩余"未绿"均对应宿主/R 线落后点
或远期项(r2a 容器/r2b 环境/r2d fmt/r3b 适配器/iter/r4b 位置/r5a const-size),
每个红锚文件头含语义承诺与翻转判据。

## 2026-09-15 审计增补(三)——语义级复核发现

- `03i` 扩展 —— u64/usize 后缀字面量(§3.7 字面量自适应)
- `roadmap/r1a_trailing_dot.neg.ct` —— 行尾 `.` 非法(§1.6;红:自举解析器吞点
  误放行,宿主正确拒绝——自举解析器守卫缺口,与新发现宿主缺陷同批在册)

## 2026-09-18 审计增补(四)——插值负向面三线审计

针对 §1.4/§4.11 插值的非法形态逐项三线探测(自举 `ctron-cc run` / C 宿主
`ctronc run` / R 线 `ctron run`),并落锚:

- `roadmap/r6h_interp_unclosed.neg.ct` —— 插值未闭合(`"v={n tail"`)。
  宿主/R 线均 E1001「未终止的插值」;自举线 ad-hoc "unbound:n" 拒绝但不产出
  E1001 → 红。已登记 roadmap_suite(NegGreen("E1001"))。
- `roadmap/r6i_interp_nested_str.neg.ct` —— 片段内嵌套字符串字面量
  (`"outer { "inner" } tail"`)。宿主/R 线均 E1001;自举线误报 E2020
  (unresolved: inner,拒绝但码不符)→ 红。已登记 roadmap_suite(NegGreen)。

**待规范所有者裁决(三线对照,先裁决再锚定/修实现):**

| 形态 | 自举线 | C 宿主 | R 线 | §1.4/§4.11 口径 |
|---|---|---|---|---|
| 嵌套块 `"v={ {x} }"` | 接受(v=1) | 接受(v=1) | 接受(v=1) | 白名单外(不支持嵌套 `{}`) |
| 空 `{}` | ad-hoc 拒绝(unbound:#EOF) | 接受(v=空) | 接受(v=void) | 白名单外 |
| 片段内语句 | ad-hoc 拒绝(unbound:let) | 接受(v=空) | 接受(v=void) | 明文禁止 |

三线一致的嵌套块可考虑升格进 spec(修订 §1.4 白名单),或三线统一按 E1001
收口;C/R 接受语句片段与空 `{}` 为待修项候选。裁决前不落行为锚(避免钉死
违规格面)。

**在册事实:** E6030 为 spec §10 预留码(parametricity,未到实现期),非漏测。
~~R 线 roadmap_suite 语料卫生自 09-15 r6 批次起滞后(7 件未登记,套件红)~~
→ **2026-09-19 已清**:r6b–r6g/r1a 七件按 R 线实测行为补登(实测口径:r1a
E1001 已拦 = NegGreen;r6b/r6d/r6e/r6f 检查未实现 = NegPending;r6c interp
as[U64] 负源得 -1、r6g 无 test 块空泛成立 = RunRed)。roadmap_suite 2/2 绿
(锚 31 = 绿 5 / 红 26);翻转时按文件头判据迁移主套件。

## 2026-09-19 审计增补(五)——r6c R 线修复翻转 + R 线全量基线在册

**r6c 修复与翻转(R 线):** 三线探针分离病灶——自举线一直正确(基准);R 线
唯一病位是 interp 字面量求值:无后缀整数字面量经 i64 解析,U64 上界
18446744073709551615 截为 -1(`let a: U64 = …` 靠后续强转侥幸正确,无期望
类型的实参位露馅;`convert_as` 本身正确)。修复:interp `Expr::Int` 无后缀且
超 i64 正程时保真为 `UInt`(parse_int 拆 parse_int_mag 复用)。r6c 翻转
RunRed → Green(roadmap_suite 绿 6/红 25)。R 线 trans 的 as 发射引用
`ct_as_ii` 助手但仓库内无定义,emit 面存疑在册(锚定面是 interp,未扩散)。

**C 宿主 r6c 病灶细化(仍在册红):** ① `wrap_int` 对 bits≥64 直接透传,
无 mod 2^64 掩码(as 侧 -1 透传,显示为 2^128-1);② as 调用实参位的
U64 上界字面量 decl 检查按有符号宽判,误报 "integer overflow (decl)";
③ 未标注 let 的自适应丢失 us 标志(实参位字面量得 -1)。

**R 线全量基线(本批首跑全量 cargo test,均在册、先于本批):**
check_suite 17 条 sem 落后(一元负 Bool/Str、E2060 推断、dist 域名称、
impl-for 解析);fmt_suite 1 条(01i 分号语料 fmt 失败);native_suite 6 条
(ffi extern cc 链接缺符号);run_suite 22 条(ffi 外部声明不可调用)。
**本批净变化:** lex_suite 补 `*.neg.ct` 过滤(neg 语料判定面是 fail: 码,
不入"全语料零词法诊断"断言)→ 绿;roadmap/lex/breakc/infer/oror/test
六套件全绿;check/fmt/native/run 四套件红为既有债务,待各自责任面切片。

## §网络与服务器(服务器泳道,2026-09-20 起)

| 波次 | 项 | 锚定 |
|---|---|---|
| S0 | caps 键集 net/db(E4010) | tests/modules/caps_net(绿:自举 + 宿主pkg 双线,宿主键集 M-T3-1 收账) |
| S0 | 规范 v0.8(11-net 定稿/§7.10/键表) | docs/spec v0.8 |
| P1 | std/net 阻塞基线(时钟/TCP/UDP/resolve/默认值) | tests/net/(probe_boundary/clock_sanity/tcp_echo/tcp_defaults/udp_roundtrip/fd_churn,6/6 绿) |
| P1 | r7b pure 触网 E4020 | tests/roadmap/r7b_pure_net.neg.ct(RunRed;翻转待 R 线 std/net 解析/跨函数纯度传播,按翻转协议迁 NegGreen) |
| P1 | 1k 回环冒烟无 fd 泄漏 | examples/ctecho + tests/net/fd_churn(500 轮双端);1k 满额归 nightly |
| P1 | 吞吐 vs C ≤1.05× | tests/net/bench(ratio 1.114 已登记,落 1.05–1.15 归因档:per-read poll 门 + 4KB 暂存 + lane 逐字节加宽;P2 处置,不堵出口) |
| P2 | rt 协程核心(自绘切换 arm64/x86_64,弃 ucontext;定时器堆+池化栈) | tests/net/rt_core_smoke(纯 C 冒烟,主环守卫跳过;yield_bench 87–102ns) |
| P2 | reactor(kqueue/epoll/poll 回退)+ wait_fd 真实现 | tests/net/rt_reactor_smoke(8 协程×16 轮 socketpair 压力;超时/对端关闭/自关路径全绿) |
| P2 | net 垫片混合化(五停车点协程无色挂起,裸线程 P1 回退逐字节不变) | tests/net/coro_hybrid(主环 main.ct + c_smoke 专属块 workers={1,4} 停车严格证) |
| P2 | 发射模板 coro 模式分支(弱定义哑元机制,CTRON_RT=coro 改道) | tests/net/run.sh 环境矩阵(默认 12/12 + coro 矩阵 12/12);examples/ctecho 源码零改动双模(§7.10 同形) |
| P2 | 确定性调度 CTRON_RT_SEED(单 worker + LCG 抽取 + spawn→join 单向闸) | tests/net/coro_det(主环 2 模 + 种子重放专属块:SEED=42 + 0..99 各双跑 cmp 101/101;nightly 1000 尾注) |
| P2 | 门禁 C10K(nightly/本地) | tests/net/c10k(纯 C 目录主环跳过;实测 10000/10000 回显全绿 + 探活绿,connect 0.4s/total 1.1s;CI 冒烟档 C10K_N=100 绿) |
| P2 | 门禁 切换微基准 ≤200ns | tests/net/bench/bench.sh rt 段(CTRON_NET_BENCH=1;实测 89–102ns 四跑全绿;Rosetta 翻译态豁免在册) |
| P2 | 门禁 echo coro-vs-P1 ≤1.15× | bench.sh coro 维度(实测 2.073–2.203 三跑,**红,登记归因**见计划执行记录:每阻塞读 reactor 登记/摘除 + park/wake + 空闲退避唤醒 ~15µs/往返;coro-vs-C 2.33–2.47 归档;**P3-A 转绿 1.019–1.040,≤1.5 检查点与 ≤1.15 原门双过,见 P3 行**) |

P2 波提交域 48ed59c..ce43e83(任务台账与门禁数字:计划执行记录);net 主环计
例口径 = 行为夹具 9 + c_smoke 2 + coro_det_replay 1 = 12。

### P3 行(TLS + 传输补全 + 时延首件,2026-09-21)

| 波次 | 项 | 锚定 |
|---|---|---|
| P3 | 时延首件(事件量交付 + 兴趣驻留 + 探针消除) | tests/net/bench/bench.sh 三门禁:coro-vs-P1 **1.019–1.040**(P2 红门转绿,≤1.5 检查点与 ≤1.15 原门双过;kick 主导 ≈24.5µs/往返分量实证)、yield 74.8–87.7ns(P2 在册 89–102ns 不退化)、门禁一 1.105–1.122;c10k 驻留 fd 生命周期复验 N=10000 全绿 delta=0(fd 泄漏门 LEAK_INJECT 证伪口径保持) |
| P3 | vendored mbedTLS 3.6.7(子集构建,离线可重建) | vendor/tls/build.sh + smoke.sh(`TLS-OK 3.6.7`;.a 合计 ~1.29M;全量重建 13.1s,零网络,依赖仅 cc/ar/awk/POSIX sh) |
| P3 | std/tls 门面(BIO-over-hybrid,零新停车点;client hostname opt-out 保留链验证) | tests/net/tls_smoke(入主环双矩阵自动双跑;CTRON_RT_WORKERS=1 停车严格证 2/2;ALPN 偏好序断言;eof/close_notify 钉住) |
| P3 | 互操作矩阵(vs openssl s_server/s_client 双向,2×2×2) | tests/net/tls_interop/run.sh **8/8**(方向 D1/D2 × TLS1.2/1.3 × 双 RT 面;LibreSSL 3.3.6 零降格;ALPN 权威断言在我方;链验证双侧 REQUIRED 对称口径;HTTPS -www 回显 = D1 cell 内绿;不入主环,主环 13→14 口径不受扰) |
| P3 | 握手吞吐门 ≤1.5× | tests/net/bench/bench_tls.sh(CTRON_NET_BENCH=1;ratio **0.503** 宽口径 / **0.750** 公平 ours_spawn 对照,均绿;基线 exec 支配已登记) |
| P3 | Unix domain socket 四件(listen/accept/connect/unlink) | tests/net/unix_sock(回环/half-close 双向/陈旧重绑自愈/ENOENT/超限 EINVAL/协程面 resolve;路径上限 104;Drop 只关 fd 不摘文件;主环 13→14 双矩阵) |
| P3 | DNS 异步化(2 线程 helper 池 + done 槽) | c_smoke T6 差分证(workers=1 最严:787 resolves/60ms 窗口、进度协程 ticks+12;红路径演练有牙)+ 裸线程面 P1 逐字节不变 |

P3 波提交域 ce43e83..29dcdb1(1d7d90e / 687653d / 8339569 / 7a1465a / db2b4dd /
8a16854 + 收口两笔;任务台账与门禁数字:计划执行记录);net 主环计例口径
12→14 = 行为夹具 9 + tls_smoke 1 + unix_sock 1 + c_smoke 2 + coro_det_replay 1。
P3 在册登记项:resolve 不可取消(取消广播不中断在途);AF_UNIX accept/connect
无停车点(协程滞留 worker,同 TCP 口径,P2-C 扩面候选);_WIN32 分支推演未实证
(AF_UNIX 哑元 + DNS 池全裁);Windows 证书库 P8(CA 走系统 bundle 文件路径);
Drop 顺序 fd 复用 ABA caveat(超时兜底,文件头注);tls_read cap<=0 返 0;
每块超时语义(ctron_tls_read 每调用重置 conf.read_timeout,握手钉 0)。

### P4 行(HTTP/1.1 协议半层,2026-09-21)

| 波次 | 项 | 锚定 |
|---|---|---|
| P4-A | std/http 协议半层首件(请求行/状态行/头部解析 + 报文构造/chunked 编解码/100-continue 钩子;零 use 纯 Ctron,同 tls.ct ⑥ 口径) | tests/http/run.sh **30/30**(std inline 2 + corpus 14 × 解释器/emit 双臂;走私面:obs-fold/TE+CL/重复 CL/裸 LF·裸 CR/值内 CTL 全拒;上限四类独立 err 码;分片到达可重入含 dst 满排空) |
| P4-A | forget_fd 重排(摘 rt 驻留登记先于 close —— P3 在册 ABA caveat 结构性收口) | bench 三门禁复验(改前 1.055/1.128 → 改后 1.075–1.129/1.025–1.111,首跑单点红系并行泳道负载噪声,×3 复跑全绿;ns/yield 86 → 47–58);c10k N=100 fd 泄漏门 delta=0;双矩阵 14/14 × 2 不变;divergences 服务器面 (h) 登记(跨模块 struct 形发射缺口) |
| P4-B | 压缩面(miniz 3.1.2 vendored 单文件惯例,SHA256 zip f0446d86…)+ gzip 纯 Ctron 组框(CRC32 恒校验,python 互证)+ Accept-Encoding 协商(`*;q=0` 排除 identity 三钉,RFC 9110 §12.5.3)+ RFC 1123 黄金向量互逆 + query/form(C8 域) | tests/http **39/39 双臂**(压缩 x_ 夹具 emit 专臂链 libminiz.a;炸弹 cap 强制、tinfl 堆暂存;deflate=raw 口径/zlib 容器与 FHCRC 未实现登记);移交编译泳道四件(c6sub 双负/use 路径 `-` segv/Option·List[struct] 错型/值域定宽乘法)→ divergences (i) |
| P4-C | 客户端(client_request:重定向 301/302/303→GET 弃体、307/308 保方法保体、5 跳上限 err7、keep-alive 单槽 bru 复用计数、陈旧连接重试一次、101 直通)+ SSE 写面(id-NUL 整字段忽略)+ WebSocket(RFC 6455 三 MUST:分片序列态 err9/10、UTF-8 良构校验 1007 惰性档、version 严格 13;SHA-1 std/crypto RFC 3174 向量;lane-b64 因 std/enc C8 域约束自出) | tests/http **57/57 双臂**(x_ e2e 夹具 emit × {默认, CTRON_RT=coro} 双矩阵;§5.7 掩码/分片语料逐字节;粘包余量压实;修复波断言逮住零长帧悬挂/len7 标记误算两真 bug);len64 证据转双臂在库断言(ws_frame_head 单点) |
| P4-D | 解析基准(vs picohttpparser)+ 结构化 fuzz 长跑 + 登记收口 | 基准:N=1e6 ×3 取最小,ctron **209 ns/req** vs pico **46 ns/req** = **4.54×**(复跑 4.44×)→ 门 ≤2× **RED 登记档**:实测 < 8× 悲观界但仍在 I64-lane 宽度税量级(~4.8M parse/s ≈ 754 MB/s),处置 P9 I8-typedef,数字照录不粉饰;fuzz:结构化生成十类(合法/坏版本/走私/越限/chunked 异常/WS 帧边/垃圾/gzip)× interp+emit 双臂 × 24 seeds(120 段),本地 ≥10min(nightly ≥30min 惯例入 runner 头注),**零崩溃零挂死**(watchdog 段超时);差分对拍 6144 样本:pico_accepts_we_reject=1265(严格子集预期差)、**we_accept_pico_rejects=0**;fuzz 实证解释器值域定宽乘法 std/http 四处(值跨 2^28–2^31 带即炸,emit 正确)→ C10 宽域惯用法四处结构性修复 + divergences (i) 收口 |

## §ctron fmt 三宿主对齐(工具链泳道,2026-09-21)

R-P2d `ctron fmt` 由 Rust 宿主移植至 C 宿主与自举编译器,三宿主同规范(docs/fmt-spec.md)同输出。

| 面 | 项 | 锚定 |
|---|---|---|
| C 宿主 | `compiler-c/src/fmt.{c,h}` + `ctronc fmt`(完整 CLI 契约)+ `tests/suite_fmt.c` | 11 金样(R4 期望按 Rust 参考冻结)+ 语料幂等 160 + trans 等价代理 69 + 词法脏报错 1;`make -C compiler-c test` 挂载 |
| 自举 | `compiler/src/fmt.ct` + `scan5`(lex.ct 加法改造:原始流+字节 span+注释 span)+ `driver_fmt.ct` → `bin/ctron-fmt` | 金样钉子 `compiler/test/fx_fmt_golden.{ct,expected}`(smoke 3f:金样逐字节/native==seed 双口径/幂等/R8 负例);`ctc fmt`(-w/--check/pkg 目录)契约 5 断言入 ctc_smoke 第 9 段 |
| 对拍 | `tests/fmt/parity.sh` | 三宿主(Rust 参考/C 宿主/自举)tests 160 + std/examples 45 逐字节零分歧;词法脏一致报错 1 |
| 语义对齐裁决 | `.or(` 成员位置紧贴(Rust lexer prev_is_dot 上下文)、CRLF 注释尾 `\r` 修剪、`...` 逗号后紧贴 | 以 Rust 参考实现输出为真值冻结;spec R4"链断行相对缩进 1 级"为 v1 遗留(现行实现=同缩进延续,r2d_fmt_chain 形态即权威) |

已知红账(非本面):smoke conc_fs / std 快照漂移 / fs 种子单测(HEAD 既有,net·fs 泳道);
suite_parse/sem 的语料对齐债(C 宿主解析器落后面,HEAD 既有);fmt_suite 01i(Rust 侧既有)。

**2026-09-21 续片修复**(compiler-c 门禁卫生):
- `token.c` TOK_NAMES 表漏 "Ellipsis" 条目(§9.6 加 TOK_ELLIPSIS 未同步)→ 名字表
  错位一格且尾部越界读,EOF 名读出 NULL 使 test_lex 段错误——补条目,test_lex 58/0 全绿。
- `suite_lex` 补 `.neg.ct` 过滤(对齐 Rust lex_suite 先例):neg 语料判定面是诊断码,
  不入"全语料零词法诊断"断言 → 127 文件零误报。
