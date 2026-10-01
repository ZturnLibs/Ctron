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
| 字符串转义(`\n \{ \u{}`) | 🟡 | 词法校验面已闭环(`fx_str_esc_neg`/`fx_interp_neg`:非法转义/插值未终止/跨行);解码仍在求值/发射层,与 C 宿主词法解码属架构分歧(登记) |
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

### P6 行(ctron-http 应用框架,2026-09-22→23)

| 波次 | 项 | 锚定 |
|---|---|---|
| P6-A | comptime 路由核(平面 Str const 表+运行时 first-match+类型化参数)+ 中间件链骨架 | tests/http/frm_route 双臂 67 绿;x_bench hit 52ns ≤100 门;bench 家族门禁不入 CI 主环 |
| P6-B | 公网中间件五件(cors/csrf/sechdr/limit/timeout;时钟恒注入) | tests/http 82/82 + net 14/14;注入防护语料 |
| P6-C | 认证三件套(cookie 会话/JWT HS256/PBKDF2)+ multipart + JSON body 绑定 | frm_auth 语料双臂(JWT 黄金向量/alg 混淆拒/时钟注入);登记: utf8_enc 内建解释臂劈叉→au_ch 表格面 |
| P6-D | 静态文件(强 ETag 内容寻址/304/Range 206·416/穿越拒 fail-closed;Last-Modified 与 sendfile 登记)+ OpenAPI 3 同源导出(快照=路由表改即红)+ CTML 转义窄面(五实体安全默认+显式豁免) | tests/http frm_static×6+frm_openapi+frm_html 双臂 **16/16**(da08562);st_serve IO 薄胶待 P6-E e2e;大串断言按 corpus 惯例拆小文件(解释器堆不回收在册债的单文件腐坏规避) |

| P6-E | examples/todo_api 端到端(REST CRUD+JWT 写保护 login 签发/CORS 预检/限流 503/OpenAPI 同源/静态页 static 响应面/健康端点/优雅停机排空退出) | run.sh 自客户端 nc 打自服务端 **20/20 双 RT 矩阵**(默认+CTRON_RT=coro;da08562 static 面首次 IO 接线于同构应用);存储=内存平行 List(Str 形 id/done——List[I64] 元素读发射债绕行),业务 ≤300 行 |
| P6-F | 门禁收口:全请求周期回环基准(vs 手写 C 双基线:阻塞串行+kqueue/epoll 事件环,同源 C 客户端,digest pin,×3 取最小)+ 热路径 no_alloc 断言(ctron_amalloc 锚计数注入,A/B 双跑差分门=0)+ 路由 ≤100ns 复跑 + parse 基准复核 | **周期四跑 0.563×/1.060×/1.049×/1.106×(quiet 窗复测义务已销,2026-09-25 00:03 load≈9–16),判定 = 登记档 1.05–1.15(net bench 家族语义,rc=0;P1 echo 1.114 同待遇)**——C 基线稳(44.7µs)而 ctron 默认臂 43–50µs 随调度抖,差值归因 = 垫片 per-read poll 门 + parse lane 拷贝(P1 echo 同族,消除项随 P9 I8/垫片演进);**coro 臂 0.998–1.139×(归档)= 协程 RT 与手写 C 事件环同量级(P2 同形承诺兑现)**;**no_alloc PASS**(四轮全绿,10 万请求稳态 bump 差=0;热路径零分配构造+堵漏证明);路由 hit 66ns ≤100(负载窗);parse 复核 229 vs 48 ns = 4.77× RED(P9 I8-typedef 在册处置,4.54→4.77 负载窗漂移照录);net 14/14、todo_api 20/20、http 98/(11 在册红);编译器 = HEAD(a514f07)worktree 隔离构建(CTRON_EMIT 覆盖口,机刷共享树清场事故后的可复现口径) |

P6-D/E 时点套件环境红(2026-09-24 收口复核,11 红全谱):frm_auth_a_guard/jwt/sess +
frm_mw_a_csrf(interp)+ http/frm/auth.ct(inline)= 5 件 = 解释器 137 击杀在册债;
client_fixtures_x_client_e2e + sse_ws_x_sse/ws_e2e ×双 RT = 6 件 —— 旧档「负载超时」
在 HEAD 复核为**构建红**(t_client_write_str/t_http_method_start 发射缺声明,饿死同族
= client/message 门面锚穿形,divergences 2026-09-24 节),归编译泳道修复;mw_chain
E2020 已销(e96d5f6 直 use 改形)。

### P8 行(生态与部署,2026-09-25→26)

| 波次 | 项 | 锚定 |
|---|---|---|
| P8-A | Connect 协议客户端(http/frm/connect.ct:unary POST+proto 头两行,classify 三态+17 名状态码表+HTTP 映射) | tests/connect **4/4**(a9a5878;含双轮 e2e:成功帧字节断言+500 失败分类;夹具服务端帧校验 08 96 01→08 2A) |
| P8-B | S3 兼容客户端签名面+往返(s3/s3.ct 域包:SigV4 四层链 HMAC^4/canonical/sts/signature/auth_header/basic_time;mock 鉴权结构三针守门+单槽存储) | tests/s3 **6/6**(d858343+b324db2;含 e2e:PUT/GET/DELETE 签名往返全链;黄金向量 python 独立实现钉值 key 尾 0417bcd2/sig 5f5e13fc…;**minio 真靶 0926 全链 ✅**:brew minio 2025-10,凭证调用点修正后 **PUT 200+ETag=md5(body)/GET 回读逐字节/DELETE 204** 三轮;根因=调用点传 AWS 示例键 403 InvalidAccessKeyId) |
| P8-C | NDJSON 逐行游标(std/ndjson.ct T1 零 use:NdLine 值 struct,空白行跳过/CRLF 剥离/尾行无换行;窗口交消费方) | tests/ndjson **8/8**(fcff689;含游标×行内容消费集成+json match 正道集成[match-on-Result emit 缺口 interp 专臂,在册];坏行策略=消费方 json.parse,Result 面修复后深集成回切) |
| P8-E | 部署面+website 指南(docs/deploy.md:产物/运行期表/scratch 配方/可观测三件;website/server-guide.md) | 文档交付(5f872cb);本地验收=e2e 22/22 同源,容器跑通归 nightly 真靶段 |
| P8-F | 门禁收口+终审 | 全波 9 套件+net+todo_api **98 断言 0 红**(00:39 终审);P8-D pkg defer(设计评审待拍板,归下波首件) |

**P8 出口门禁判定(0926 三次收口)**:①pkg add e2e ✅(registry↔ctpkg 全链:
manifest 解析→deps→vendor 双包落盘→lockfile 两行;is_dep 头匹配缺陷修复后)
②Connect 互调 ✅;③S3 夹具往返 ✅ + **minio 真靶全链 ✅**(建桶→PUT/GET/DELETE
三轮;fresh 数据目录亦通;nightly 例行段 CTRON_S3_MINIO=1 入 runner);④website 指南 ✅;
⑤todo_api 容器配方 ✅(容器内跑通归 nightly)。**P8 按 5/5 门全绿交付收口**。
**is_dep 头匹配缺陷**(80f669a 后修复):dep 头判 b4=='{' 拷自 is_pkg 定式,
对 dep NAME { 恒不匹配(t0+4=名首字母)→in_dep 永不置位;修=dep 锚定行尾
(te-2 空格+te-1 '{')。逐行 trace 一轮定位。

**P8 as-built 偏差**:http.client StructLit 残缺陷(往返客户端裸 socket 绕行);
发射器长串拼接/`\n` 字面量组合缺陷(s3_nl/join2 组合原语绕行);`scope` 关键字
撞名(参数改名即清,divergences 在册)。

### P7 行(可观测与运维,2026-09-24→25)

| 波次 | 项 | 锚定 |
|---|---|---|
| P7-A | std/log 结构化 kv(级别码/阈值门/行构造三件纯面;值转义含 UTF-8 透传) | tests/log **9/9** 双臂(a63470e);T1 档零 use 纯模块,ts 与 IO 留消费方 |
| P7-B | std/pb protobuf 线格式(varint 9 组正编/10 组负规范形/tag*8+wire/zigzag 分路/ld/fixed64 LE) | tests/pb **17/17** 双臂(c02e687+5fd6271);官方向量锚(150/testing/-2)+负例矩阵+往返 walk;**纯算术实现**(无位算符纪律,divergences 09-25) |
| P7-C | W3C traceparent 提取/注入(http/frm/trace.ct 零 use) | tests/trace **7/7** 双臂(3dd6f1a);严格 55 字形+非法矩阵+请求提取;黄金向量字节精确 |
| P7-D | Prometheus 文本 exposition(http/frm/metrics.ct 零状态写面)+ todo_api /metrics | tests/metrics **7/7** 双臂 + e2e **22/22**(f96995c);http_total{code} counter×10 + todo_served gauge;热路径调用方 lane 零分配 |
| P7-E | OTLP/HTTP 导出(四层嵌套 ETSR 编码,fixed64 时间面)+ Collector 夹具解码比对 + traceparent 跨服务传播 | tests/otlp **4/4**(含 e2e)+ tests/trace **8/8**(含传播 e2e)(5fd6271+db2cde9);编码→真回环 POST→解码→tid 逐字节+name 双匹配;传播 A 提取→再注入→B 回显 trace-id 一致 |
| P7-G | 优雅停机演练(串行形:排空退出+停机后新连接拒绝)+ 登记 | e2e **22/22**(含停机拒绝探针);inflight 并发形随 keep-alive/协程环志向演进 |

**P7 出口门禁判定(2026-09-25)**:①todo_api 合法 Prometheus 文本 ✅;②OTLP 夹具解码
比对一致 ✅;③traceparent 跨服务同 trace-id ✅;④/debug/scopes ✅ **v1 交付**(3b9f002:ctron_rt_scopes_json rt 锁内快照,
entries 五态+jnext 反查等待边;确定性夹具 [2,3] join 边全钉,tests/rt_scopes 5/5;
serial/coro 双形;树父权 ct_task 层残留=逐层名映射,归编译器任务面);
⑤tracing 层级=连接/请求树 ⏸ 随④残留。**P7 按 4/5 门 + ct_task 层残留交付收口**。

**P7 as-built 偏差与移交**:①std/config 实为 CTCL manifest 校验器,非应用运行时
配置面——todo_api env 配置形维持,应用配置读法提请 std 需求评审;②emit 主文件
20480B SIGKILL + `\|` 死旋(divergences 09-24/25 节)归编译泳道;③OTLP 批量/JSON
形态列志向。**09-25 深夜追加**:e2e 缺声明 6 红(饿死族 client/message 面)经 P1b
契约补全(use 清单全量请求)全清——http 套件 103/5,余 5=137 解释器债族;OTLP send
仍以裸 socket 形(net 门面直连)维持,http.client 合并修复后可回切 client_request。

### 完整应用示例行(2026-09-26→27)

| 波次 | 项 | 锚定 |
|---|---|---|
| APP-1 | examples/todo_app 完整应用(带登录 todo 管理):注册/登录/登出(pbkdf2_sha256+每用户盐+iters 落档)/HMAC Cookie 会话(HttpOnly+SameSite=Lax,恒时比较+过期域)/多用户隔离/NDJSON 文件持久化(重启不丢)/服务端渲染 HTML(ht_esc XSS 安全默认) | run.sh 两相位 e2e **26/26**(注册/重复与短密错密拒/Cookie 签发/未登录门卫/空态/增删翻/隔离门/登出/metrics/重启持久化);四模块 check 0E+test 全绿+fmt 净;真浏览器烟测截图取证(中文标题端到端+用户手工互动) |
| APP-2 | 双层发射缺陷定案(divergences「todo_app 发射爆炸+运行野跳双层案」):①字符串表裸 `{` lexer 毒(P7 同族,解释臂宽容/发射臂 3.95GB 爆炸——初判主分发预算有误)②fn 值形参 ct_clop 野跳转(签名白名单缺口) | 星形单路径四模块拆分(data/sess/views/app)+主要文件 shim;`\{|}~` 转义+删 fn 形参后 237KB 一次发射全绿;应用侧 UTF-8 感知百分号解码(utf8_enc 发射口径)补 form C8 域中文能力 |

### P5 行(std/db 数据访问层,2026-09-21)

| 波次 | 项 | 锚定 |
|---|---|---|
| P5-A | std/json 数值/布尔保真(类型化访问器:jget_i64 溢出检测/jget_f64 17 位窗/jget_bool;越界 = Err 非 silent) | tests/json_fidelity **11/11** 双臂(2^63/2^64 边界负例含 9223372036854775808/99999999999999999999 两串、F64 精度负例、布尔/null 判别、嵌套路径) |
| P5-B | std/crypto 二进制面 + std/uuid v4 真熵/v7 | tests/crypto_vec **24/24** 双臂(RFC 6234 SHA / RFC 4231 HMAC / RFC 6070 PBKDF2 官方向量全锚 + "abc" sha256;uuid 10 万枚唯一性 + 版本位断言) |
| P5-C | 协议夹具回放框架(双薄源共驱一核:script 十六进制回放 / fd 垫片直读)+ PG wire v3 核心(startup/AuthOk/Query/RowDescription/DataRow/CommandComplete/ReadyForQuery/ErrorResponse/Terminate) | tests/db/run.sh 双臂(replay_fixtures 8 手工按公开协议构造,README 登记);fd 源面 emit 专臂(x_fd_edge EBADF/errno 槽、x_fd_pipeline 管线帧 mtype/fill 持态/send-all 整发) |
| P5-D | SCRAM-SHA-256 全流程(RFC 5802/7677,PBKDF2 密码链 + server-final v= 校验 + Z 门 phase==3 信任面)+ 扩展查询(Parse/Bind/Describe/Execute/Sync)+ 事务(BEGIN/COMMIT/ROLLBACK + 取消传播分级) | replay_scram 13 夹具(RFC 7677 原例 i=4096 + i=1 改制;**生成器 hashlib 自检双落地**)、错口令/签名不符 x_scram_neg、nonce 真熵 x_scram_nonce |
| P5-E | Redis RESP2(五形 +/-/:/$/*,NULL bulk,一层数组)+ 有界连接池(容量必填/FIFO/Z('T') 陷阱复位/drain/脏连接绝不发放/双重归还防呆/等待接线背压)+ 行→struct 运行时绑定(**D-P5-1 偏差**:运行时 Err 面,`@derive(DbRow)` 列编译泳道志向) | redis_replay 13 夹具(rr_replay 五形逐字节 + 编码器 hex 锚 + 512MiB 乘前门 + 截断拒收)、pool 4 夹具(pool_core/pool_wait 双臂)、rowmap 2 夹具(rm_anchors r7d 运行时 Err 锚 noent/type/null/row-oob/val-domain + I64_MIN 下顶格) |
| P5-F | **fd 半包判别收口**:redis bulk 数据段 TCP 分片 ≠ 协议违例——incomplete 面(rc 0/err 0/used 0/kind 0)三途(头行无 CRLF/bulk 数据未到齐 sane/数组元素未到齐),redis_recv_fd 续读 + 垫片 would-block 面(非阻塞夹具 socketpair);脚本源截断映射 err 2 拒收语义不变 | x_rd_fd 重钉:真 TCP 分片跨两次 recv(incomplete 面逐值锚)+ incomplete 三臂 + 管线 fill + send-all;脚本面 rr_replay 截断钉同绿;redisx_i64s I64_MIN 负向累计 + EXPIRE I64_MIN hex 锚钉;pool 借出/回池栅两处对齐(借出半开/回池全闭语义注 + 槽尾再借 err 2 钉) |
| P5-F | **fd 真源全会话**(pg.ct fd walkers:pg_scram_handshake_fd 含 SASL 双下行、pg_query_fd 帧分派核同构;dbpg/dbredis connect 垫片) | x_pg_fd_session(emit 臂,链 dbpg+entropy 垫片):socketpair 真源 SCRAM 全链(sigok=1/pid/key 逐值)+ 简单查询行集 + 扩展查询 + 事务状态 I→T→T→I;服务端字节 = 夹具同源帧逐字复用 |
| P5-F | **tests/db 双臂总门禁** | **54/54**(守卫 15 [漂移 5+声明冒烟 5+在位 5] + interp 臂 16 + emit 臂 23;夹具面 = script 8+13+13+4+2 手工构造 + corpus 7 + x_ emit 专臂 3;较 P5-E 53 增 1 = x_pg_fd_session);四矩阵不变:json 11/11、crypto 24/24、http 59/59、net 14/14 |
| P5-F | nightly 真靶(真 Postgres/Redis;不入 CI 主环) | tests/db/nightly/(env CTRON_PG_DSN/CTRON_REDIS_URL 驱动,nc 探活回退;构建门先跑 = 夹具健康检查):**本机实测 构建 2/2 PASS + 真靶 2 SKIP**(5432/6379 不可达,无靶跳过口径逐行登记);PG 面 = connect+SCRAM+简单+预编译+事务,Redis 面 = SET/GET 回环+INCR×3+DEL |
| P5-F | 性能门 简单查询回环 vs libpq ≤1.5× | tests/db/perf/(baseline_libpq.c 同构基线 + ctron 臂,min-of-3 整进程墙钟):**本机实测 SKIP 登记**——缺本地 libpq 开发面(pg_config 不可用、常见路径无 libpq-fe.h;缺失前置逐行输出),不虚构数字;装 libpq + 有真靶后重跑 |

## §ctron fmt 三宿主对齐(工具链泳道,2026-09-21)

R-P2d `ctron fmt` 由 Rust 宿主移植至 C 宿主与自举编译器,三宿主同规范(docs/fmt-spec.md)同输出。

| 面 | 项 | 锚定 |
|---|---|---|
| C 宿主 | `compiler-c/src/fmt.{c,h}` + `ctronc fmt`(完整 CLI 契约)+ `tests/suite_fmt.c` | 11 金样(R4 期望按 Rust 参考冻结)+ 语料幂等 160 + trans 等价代理 69 + 词法脏报错 1;`make -C compiler-c test` 挂载 |
| 自举 | `compiler/src/fmt.ct` + `scan5`(lex.ct 加法改造:原始流+字节 span+注释 span)+ `driver_fmt.ct` → `bin/ctron-fmt` | 金样钉子 `compiler/test/fx_fmt_golden.{ct,expected}`(smoke 3f:金样逐字节/native==seed 双口径/幂等/R8 负例);`ctron fmt`(-w/--check/pkg 目录)契约 5 断言入 ctron_smoke 第 9 段 |
| 对拍 | `tests/fmt/parity.sh` | 三宿主(Rust 参考/C 宿主/自举)tests 160 + std/examples 45 逐字节零分歧;词法脏一致报错 1 |
| 语义对齐裁决 | `.or(` 成员位置紧贴(Rust lexer prev_is_dot 上下文)、CRLF 注释尾 `\r` 修剪、`...` 逗号后紧贴 | 以 Rust 参考实现输出为真值冻结 |
| R4 v1 定版(2026-09-21 用户裁决) | 链断行相对缩进 1 级三宿主同步落地 | spec R4 文字与实现自此一致;金样/套件期望已重冻结,parity 303 绿零分歧 |
| R9+R10(2026-09-30) | use 组自动折行(内联形态超 100 列,恒补尾逗号)+ 导入排序(组内项按名/顶层语句按路径,稳定字节序;空行分段次序保留;注释守卫冻结;无组尾随别名并入语句 span 防悬空) | spec R9/R10;三宿主同步(Rust fmt.rs 预处理/C fmt.c 1:1/自举 fmt.ct);suite_fmt 17 金样+432 语料幂等+93 trans 等价;fmt_suite 34 测(断言② 修订为 use 声明多重集比对);fx_fmt_golden 重冻结三宿主逐字节;parity 默认根适配 lib/std 后 494 绿 0 分歧 |

已知红账(非本面):smoke conc_fs / std 快照漂移 / fs 种子单测(HEAD 既有,net·fs 泳道);
**suite_diff 差分墙:37 红 → 4 红(2026-10-01 大头销账)**:三差分模板同步落库——
lex_num/parse_ast/parsetree 补齐 v0.7 修订一(||/&&/or 中缀+成员位置上下文)与修订二
(break/continue)+ match 守卫吞并(R-P3c,C 侧 dump 不含守卫)+ 或模式 Or([...])+
use 组逐名展开(含 as 别名/组内 NL 不敏感)+ extern ABI + struct 字段逗号 + @derive class;
顺手修 C 侧真 bug:ast_show.c 漏 PAT_OR case(或模式 dump 成空 pattern,Rust Debug 口径
Or([...]) 补齐);pkg_chk 模块级 7 包按「文件+码+计数」归一(自举双语诊断表 vs C 英文串
的语言面分歧在册,消息文本剥离)。**余 4 红(另案)**:①sem_chk.ct oracle(第三块模板,
树上 W8010 语义)在 03n/03l/07a 崩「索引目标非数组」——较新构造(List[Struct] 成员链等)
未同步,独立进程可复现非进程内问题;②02f_match_guard_exhaustive 语义差分——sem_chk 欠
T02 守卫穷尽(E2030 守卫臂不算覆盖)逻辑。fmt_suite 01i(Rust 侧既有)。

**2026-09-21 续片修复**(compiler-c 门禁卫生):
- `token.c` TOK_NAMES 表漏 "Ellipsis" 条目(§9.6 加 TOK_ELLIPSIS 未同步)→ 名字表
  错位一格且尾部越界读,EOF 名读出 NULL 使 test_lex 段错误——补条目,test_lex 58/0 全绿。
- `suite_lex` 补 `.neg.ct` 过滤(对齐 Rust lex_suite 先例):neg 语料判定面是诊断码,
  不入"全语料零词法诊断"断言 → 127 文件零误报。

**2026-09-28 T01 bit.* 位运算内建落库**(spec-gap W1;§4.0 宪法「位运算恒走 bit 模块」兑现):
- 前奏命名空间 `bit.*`(parallel 先例):and/or/xor/not/shl/shr × i32/u32/i64/u64,共 24 入口;
  补码语义按声明宽度、shl 回绕、shr 有符号算术/无符号逻辑、移位数越界 panic "bit shift range"。
- 四面同判:自举解释(eval_call 文本域逐位组合+双零早退)/自举发射(trans_expr 直出 C 位运算+
  ct_bit_sh* 运行时助手)/C 宿主解释(rt_eval __int128)/C 宿主发射(trans_conc emit_bit)。
- 锚:tests/bit_ops.ct(6 块)+bit_shift_{hi,neg}.panic.ct;std/crypto 六处内联位函数回切 bit.*
  (SHA/SCRAM 向量零回归,自举解释臂 8.0s→5.3s);R 线内建面缺位在册(crypto R 臂 0/3 预存红,
  bit.* 随 R 线内建面批次补,失败数不增)。
- 顺手修:自举 i64/isize 后缀宽字面量此前误落 I32 域截 0(eval_expr 宽路径仅认无后缀),
  03b 补锚;宿主负大字面量/宽正字面量算术检查缺口(unary neg/Sub 于 v_int bits=32 恒炸)为
  宿主在册债,锚侧以 i64 后缀规避开(divergences 待登)。

**2026-09-28 T02 模式守卫+或模式三线落地(R-P3c 销账)**(spec-gap W1;§4.6):
- r3c 两锚迁入主套件为 02f_match_guards.ct / 02f_match_guard_exhaustive.neg.ct(E2030);
  三线落地:自举解析/求值/穷尽既有(R-P3c 半成品收口)——C 宿主补 parser(或模式 PAT_OR +
  臂守卫)/rt(pat_bind_or 替身依序 + EX_MATCH 守卫绑定后判定)/sem(守卫臂不算覆盖,或模式
  替身展开计覆盖);R 线补 ast(Pattern::Or + MatchArm.guard)/parser/check(守卫跳过覆盖 +
  或模式展开)/interp(try_match Or 前置拦截 + 守卫假值回落)。
- 发射臂登记(不阻塞本翻转):自举发射 match 面窄(Result/用户枚举/Option[Str] 特化),
  表达式位 match 与字面量模式臂 ct_expr panic —— 即 P8-C 在册「match-on-Result emit 缺口
  interp 专臂」同族,守卫/或模式发射随该债一并扩;C 宿主 trans 同族登记。
- suite 77/78 双线(+2 全绿,06f/06e 两预存红不变);cargo 13 套全绿;meta/fmt parity 过。

**2026-09-28 T03 StringBuilder 前奏三线落地**(spec-gap W1;§3.8.2 R-P2a 销账):
- r2a_sb 锚迁入主套件 03j_stringbuilder.ct;API:StringBuilder()/push_str/to_string/len(字节)。
- 三线:自举(SB 值=段列表宿主 List 别名语义,call_mem 专属臂须先于通用 to_string)/
  C 宿主(V_SB 复用 listnode 段表)/R 线(Value::StrBuilder,通用 to_string 截胡排除;
  裸类型名构造器调用回退补——def_by_name 命中即构造,含无泛型形)。
- 发射臂:native_suite 口径 trans 拒绝=skip;StringBuilder 发射(拼接语义)随容器发射批次。
- suite 78/79 双线 +1 全绿;cargo 13 套全绿;meta/fmt parity 过。

**2026-09-28 T04 W 码三负锚补齐**(spec-gap W1;§5.6/§3.8/§1.3):
- 新锚 01m_shadow_prelude.lint.ct(W8040 参数位遮蔽)+01n_unused_binding.lint.ct(W8030 未使用
  绑定,fn 体形——自举 W8030 仅扫 fn 体,test 块位登记差异);05f(W8020)既有,注册表「锚待补」注记清除。
- 实现补齐:自举 W8020 泛型返回(Result[I32,Str] Named+TArgs 形)+fn/test 体**块尾表达式位**
  (walk_b 加 body 参,值位块不判防误报)+参数位 W8040(is_prelude_name 名单对齐 C 宿主;行传 0
  交驱动探针——Fn 无属性路径不盖章,nline 契约内回退);C 宿主补 fn/test 尾位 W8020(w8020_tail)
  +**W8030 全新**(bind.used 标记:bind_find 命中+check_expr EX_IDENT 读取位;保守面仅原语类型
  绑定——插值部件宿主侧纯文本无 AST,聚合/Option/Drop 绑定免报对齐自举豁免口径)。
- 登记差异:R 线无 W8030/W8040 发射面(lint 锚经 check_suite 零诊断口径空过,R 线欠账);
  自举 W8030 不扫 test 块位。
- suite 80/81 双线(lint 4/4×2;06f/06e 两预存红不变);cargo 14 套全绿;meta/fmt parity 过。

**2026-09-28 T05 行尾点守卫落库**(spec-gap W1;§1.6):
- r1a 锚迁入主套件 01o_trailing_dot.neg.ct(E1001);自举词法层 filter_nl 守卫(行尾 `.` 后
  未抑制的 NL → E1001,消息键 E1001.trailing.dot 双语登记)——此前自举解析器吞点误放行,
  宿主/R 线既有拒绝面不变;parse_suite/check_suite 双登记,roadmap_suite 移除 r1a。
- suite 81/82 双线(06f/06e 预存红不变);cargo 14 套全绿;meta/fmt parity 过。

**2026-09-28 T06 E6030 parametricity 封闭性论证**(spec-gap W1;§8.4):
- 结论:**现行语言面下 E6030 违规不可达,码位由封闭性承载**(不设重复空检查,能力优先裁决)。
  论证四点:①语言无任何反射算子(type_of/type_name/reflect 全库零命中,§3.8 前奏无此面);
  ②ceval(E6010 步数预算求值器)值域仅 I 域整型字面量/const 引用/算术/比较/逻辑与——无类型
  表示可入值域;③类型名在表达式位仅解析为构造器/变体(sem E2020 面),不存在产生类型信息的
  值形态;④@derive 为编译期结构化展开(硬编码 Show/Eq/Error 集,结构谓词深度限 6),非运行期
  反射。将来若引入类型级 comptime(§8.4 预留的类型产出函数)或反射 API,须同步补 E6030 实检。
- 规范 §10.1 E6030 行注记随本论证更新(预留 → 封闭性承载)。

## §web P0 编译销账(编译泳道,2026-09-27→28)

| 项 | 锚定 |
|---|---|
| P0-3/P0-2/P0-1/P0-4 member-emit 四件 | 探针转正 `03k_fn_call_chain_member`/`03j_closure_param_member`/`03l_list_struct_member`/`03n_struct_fn_field_array`(销账提交 fc79e6c/ec006dd/b0e5bfc/edb59fb)。臂况分记:03k/03l/03n 种子双臂绿、03j interp 双臂绿;种子发射臂残留:03j(值位置闭包按设计非捕获 trans_expr.c:1587,`tag` 未声明——候选 **L7**);自举原生发射臂残留:03l emit `ct_expr:Member@23`(L 码擦除 trans_ty.ct:250+定宽 char** 槽=P5 b4)——**已销账(T14-② 参数化 List 码+装箱容器 ABI,T54 补 fn 值链 #fret 字段提取位+蹦床 u: 解盒;03l 正本 emit 臂绿 `HIT /app -> app:alice`+`SUM 42`,见 spec-gap 计划 T54)**、03n emit `emit:并发值含 struct 值传递`(在册同族)、03j(同 L7 族)——条件登记 **L6/L7**(spec §9,候选·待用户裁决) |
| L5 StringBuilder 判定 | **立(终判)**:探针 `03i_string_builder_probe` 双编译器 check 挂名过(0 诊断)、运行面拒(种子 `未知函数: StringBuilder` / 引导 `E2020`);编译/运行分相不合 `.neg.ct` 契约,维持行为件红账 |
| L4 Bytes 判定 | **立(终判,引导双臂实测坐实)**:探针 `03m_binary_nul_probe` 种子双臂不可执行(单文件不并 use);引导 interp + emit→gcc 双臂执行成功而门红(`len:4/nuls:0/mid_ok:0`——/bin/echo 101136 B fat Mach-O 首 NUL 在偏移 4,Str=C 串读边界截断,nuls=0 且 mid_ok=0 → 立) |
| **web-P0 收口门** | 修复四件:03k/03l/03n 种子双臂绿 ✓、03j interp 双臂绿 ✓(emit 双臂红,残留=值位置闭包捕获,候选 L7 在册——非本批销账域,见上行);03i/03m 判立红账在册(非新增红);**合并后 suite.py 合计 87/88(自举,红=06f_parallel 既有)+88/88(宿主);03i 随 T03 转绿(L5 销账)**(模块桶 14/15、13/15 另列;bootstrap 红 2 = 03i+06f_parallel[基线既有:解释器无 parallel 内建];种子红 2 = 03i+03m;bootstrap 侧 03m rc=0 而 gate 红,suite 判 rc 不判 gate,红账以 divergences 判定为准);smoke --full 150/0;ctron_smoke 21/0;meta gate 已知红 2 = 03i/03m 行为件无 test 块(即两探针,判立红账一部分,不扩大) |

臂覆盖声明(套件量程,防「双臂绿」误推;03j 账误即由此出):本套件行为桶只跑种子 `check`+`test` 与引导 `run` 两类臂——**无种子 `build`(发射)臂、无引导 `emit` 臂**。故任何「双臂绿」结论必须来自显式逐臂运行,不得从 suite 通过推得。

设计:`docs/superpowers/specs/2026-09-27-web-framework-design.md` §9;判定详证与逐字输出:
`docs/c-rust-divergences.md`「web 框架 P0 探针」节。

**2026-09-28 T07 单态化实例预算 E6040 落库**(spec-gap W1;§8.5):
- driver_emit 预扫计数(镜像 struct insts 预扫惯例):显式 TypeArgs 泛型 fn 特化去重键
  (mono_e/mono_b 走查,形状同 sem_walk)+泛型 struct 实例,超限 → E6040+rc=1,消息含
  「建议 &Trait 化」;CTRON_MONO_BUDGET 可调(默认 8192;seed 路径 env_get 哑在册,旋钮走
  原生 bin)。v0 计显式位,推断特化随 §3.9.1 ex_ty 保留实参登记并入(锚文件头注记)。
- 锚 compiler/test/fx_mono_budget_neg.ct(4 显式特化去重 4——dedup 即证)+smoke emit 臂
  (CTRON_MONO_BUDGET=3 → E6040 rc=1);注册表先行:E6040 入 §10.1 + meta_check。
- suite 81/82 双线维持;db 套件回基线;meta/fmt parity 过;native 自举固定点过
  (编译器自身显式特化计数在 8192 内,预扫零开销可观察)。

**2026-09-28 T08 comptime 预算口径裁决落地**(spec-gap W1 裁决门;用户裁定:步数预算为 v1 终态):
- §8.4 修订注:1200 步步数预算(ceval,E6010 判据)为终态口径——确定性友好(同输入同判定,
  与 §10.3 无张力);1s 时间口径废弃(时钟依赖不可复现)。清单键 comptime.budget_ms 定性为
  声明位(C 宿主 pkg 解析+校验+pkg 子命令展示,不进执行;实证 grep 无第二消费方),量纲统一
  (budget_steps 改名)随 CTCL 迁移批次(T48)定夺。
- comptime_budget 模块锚注记对齐步数口径;r6f 锚头注本即步数口径无需动。

**2026-09-28 T09 resolve 多记录双栈落库**(spec-gap W2;§11.5):
- C 垫片 ct_getaddrinfo_all(AF_UNSPEC 全记录 → inet_ntop 文本 "\n" 打包)+门面
  ctron_net_resolve_all(TLS 静态;helper 池 all 模式——job 缓冲 64→2048,池线程产全记录
  打包串,槽所有权/停车纪律与 resolve_first 逐条同构);net.ct 门面 net_resolve_all →
  List[Str](规范点分/冒分文本)+ addr_is_v6;首记录面 net_resolve 保留。
- 夹具 resolve_all(数值 v4/v6 确定性单记录/localhost ≥1/非法数值串空表+err;零外联纪律);
  消费方 tcp_echo/unix_sock 双 API 化(协程对切 all 模式 = helper 池新面冒烟)。
- **表示裁决**:v0 记录以规范文本承载(List[struct] 发射 = divergences (h) 族,同 T14
  struct 值容器底座;SocketAddr 值 struct 随其并入);List[struct] 发射断点实证 = 元素
  类型丢失(char* 槽)+索引取值无提取分支。
- 顺手修三笔:①Str.contains(§3.8.2 v0.5 钉死承诺)interp 缺位+emit 丢实参(10_trace 侧
  经 List.contains 绕过故隐形)——interp str_contains 分支+emit strstr 直出+R check 臂,
  01d 补锚;②UFCS 发射 from=2 计数错位(实参恒漏首个+个数核对错位;04g 发射臂 rc=1 即此,
  主套件从不发射故长期隐形)——重写实参对位(pcs[1..]);③R 线 std.fs.read_or 原生调用面
  (机刷 03m 探针前向修复,read_file 同族臂)。
- 新登记:04g 发射臂类方法分发缺位(c.hi() → t_hi 未声明;UFCS 修后暴露的下一层,
  归类方法发射批次)。
- net 双矩阵 15/15×2;db 44/10 基线;suite 87/88 维持;cargo 14/0;meta/fmt 过。

**2026-09-28 T10 sleep_ns/可取消/虚拟时钟落库**(spec-gap W2;§11.6/§11.4):
- C 三件:ctron_net_sleep_ns(协程面 ctron_rt_sleep_until 绝对 deadline 停车,取消广播早醒
  →返 1(deadline 未到判据),裸线程真睡返 0=join 等效口径);ctron_net_clock_jump(虚拟态
  偏移累加,实钟态 -1+EINVAL);虚拟钟(CTRON_CLOCK=virtual:冻结基点+跳变量,env 惰性读)。
- rt 两件:ctron_rt_clock_ns 弱钩(rt.c 弱缺省实钟,net 垫片强覆盖贯通——虚拟跳变驱动
  定时器堆依 deadline 序确定性触发)+ctron_rt_sleep_until;net.c 补弱哑元(裸矩阵链接纪律)。
- 门面 net_sleep_ns/net_clock_jump(bind extern 直调,方法发射缺口在案);夹具 clock_sleep
  三矩阵:实钟臂(下界/单调/取消 elapsed<1s)、虚拟臂(5s 虚拟定时器经跳变 33ms 真实完成)
  ——模式探测 jump(0),实钟臂默认+coro、虚拟臂 run.sh 第三矩阵 coro+virtual 专跑块。
- 新登记:①scope 闭包体 return 语句发射泄漏(return 直出宿主函数+块值丢失;块值须尾表达
  式形态,coro_conc 先例;spawn 闭包不受影响)②ct_spawn pthread/协程混合分派下取消早退
  深时序(~500ms 中间态)归 P9 栈经济注记③裸线程睡眠虚拟跳变不打断(专跑臂在协程矩阵,
  语义注记)。
- net 三矩阵 17/17×2+专跑;db 44/10 基线;suite 87/88 维持。

**2026-09-28 T11 能力键细分落库**(spec-gap W2;§11.1/§12.1):
- 键集扩为 {fs, time, net.listen, net.connect, net.resolve, db.connect}(粗键 net/db 不再
  授予,fail-closed 硬切;在库 ctecho 清单同步升细键)。映射:listen 族(tcp/unix listen+accept)
  /connect 族(tcp connect+udp 全族+unix connect)/resolve 族(net_resolve+all);db 域统一。
- 四线注册表同步:schema(ctcl_manifest_schema members 真源)/宿主 pkg.c(check_caps 重写:
  补域二段形——caps_net 宿主 informational 分歧顺手闭/R check.rs+main.rs/自举第四臂
  selfhosted/ctcl_chk.ct;E5043 消息统一 sorted 序,四线对拍 104 例全绿)。
- 自举 parse_pkg:细键检测(use 导入驱动,cap_net_fine 映射+去重)+粗面兼容(能力对象
  &Net/&Db=任一细键);**顺手修**:无清单语境 read_file 缺失回落 Some("") 被当空清单
  deny-all——此前粗检测 &Param 门槛掩盖,细键导入驱动面暴露;空文本=无清单放行。
- 夹具 caps_fine_neg(仅 net.resolve,listen/connect 导入 → E4010×2)/caps_fine_ok
  (resolve 正例,干净 extern 面——listen/connect 门面 Box 出参 W8052 真阳性属 ABI 面不重复);
  suite modules 跑器补 CTRON_STDPATH(域包门面解析,run.sh 同惯例)。
- diff.py 排除 .worktrees(他泳道工作树陈旧清单/二进制不入四线对拍)。
- modules 16/17×2(唯一红=use_alias_nat/dup_static 既有意红基线);net 17/17×2;suite
  87/88 维持;manifest 三门+ctcl selftest+cargo 14/0+meta/fmt 过;R 线细键检测(对象面
  note_cap_use 仍粗键)登记 R 线欠账。

**2026-09-28 T12 gzip 收口**(spec-gap W2;§11.7):
- 主体已由 webfw 泳道交付(P4-B http/enc.ct:gzip 容器组框 RFC 1952 纯 Ctron +
  miniz 垫片 CRC-32 单实现 + Accept-Encoding 协商;enc_fixtures x_gzip_round/
  x_crc32/x_deflate_round/a_negotiate)——计划卡分析时序差,本件缩为互操作补齐。
- 补:①x_gzip_interop(python gzip.compress 真实流 93 字节 hex 语料 → 本实现
  gunzip 逐字节还原 292 字节;头布局锚 1f8b/08/00/mtime=0)②x_gzip_out(本实现
  产出流 hex 出)③run.sh 互操作臂(本实现流 → python gzip.decompress 逐字节
  差分)——消费向/供给向双闭环。
- 登记发射怪癖:x_ 夹具 fn main + 显式 `&数组` 实参形 → 发射驱动 SIGSEGV
  (截断 C);test 块 + 数组直传(隐式视图转换,gzip_round 惯例)为正形。
- http 110/1(唯一红=frm_auth_a_jwt interp OOM,实证与 bit-crypto 无关:
  原版 crypto 对照 1.46GB vs bit 版 1.32GB/50 HMAC——解释器 arena 无回收
  在册债(T31 目标)所致)。

**2026-09-28 T13 Atomic 真原子落库**(spec-gap W2;§7.3):
- 自举发射:fetch_add 由全局锁 RMW(ct_glock 三步)改 __atomic_fetch_add(SEQ_CST,
  返旧值语义不变);load/store 升 __atomic_load_n/__atomic_store_n(原裸读写无序)。
- C 宿主镜像:ctron_mutex_<w>_{load,store,fetch_add} 三 helper 同改(原裸读写/
  __int128 RMW);Atomic with/with_mut 保持互斥语义(锁归属正确,不动)。
- 验证:coro_conc 增 atomic_smoke(独立函数——同函数双 scope 撞发射固定名 t_sc,
  名不唯一化登记;4 任务×50k fetch_add 双矩阵精确 200k + store/load 往返);
  宿主最小探针(fetch_add 返旧值→store→load 链路正确);net 17/17×2。
- 门禁:suite 87/88 维持、modules 16/17×2(基线红不变)、db 44/10 基线、http 110/1
  (jwt interp OOM 预存)、cargo 14/0、meta/fmt/ffi 过。
**2026-09-28 web 2a 开波**(web 框架核心纯函数面;计划文件:
`docs/superpowers/plans/2026-09-28-webfw-2a-core.md`):
- Task 0 包骨架落库:`web/Ctron.ctcl`(pkg web;caps net.listen 供应链审计标记)+
  `web/core.ct` 包头占位(Task 1-4 填充);语义门 `ctc.sh check web/core.ct` → check OK。
- 包测试驱动口径(seed 单文件):语义门 `sh compiler/ctc.sh check <file>`;test 块
  `compiler-c/build/ctronc test <file>`(suite.py 两相位同款;跨模块被测文件按
  compiler/build.sh 式拼接,bootstrap ctc.sh 无 test 子命令)。

**2026-09-28 web 2a 收口(Task 5;16e702e..本笔,worktree webfw-2a)**:
- 语义门:`ctc.sh check web/core.ct web/router.ct web/view.ct` 全 OK(decls=134)+
  三文件裸跑 rc=0;负臂实证:err_json 旧 end_status(status 底座 text/plain)对新断言
  `Content-Type == application/json` 实咬(assert failed rc=1),修后绿。
- 本笔面:err_json CT 修正(M-T1w-1,end_status 改走 json() 底座)/strs 独立断言(M-T1w-2)/
  csv_rows(RFC 4180 最小引用形,本地实现——use std.csv 入 core 会翻转 view 合并态 E5030,
  见下)/attachment/xml/yaml 补 §4.4 格式矩阵/install(§4.6 插件挂载)/test_call(§4.7 测试口,
  dispatch 别名)/view 信任边界注记(M-T4w-1)/README API 表(§4 定稿面 2a/2b 两列)。
- 新编译器坑在册(M-T5w-1):pkg_load_use 选择性合并 keep 闭包含 **test 体引用**
  ("Test 恒随"口径)——core 测试体引用 `text()` 即把 core.text 拉进 use 方(view)合并面,
  与 view.text 撞名 E5030(实证:引用时 rc=1/去除 rc=0);同型:`use std.csv` 入 core 同样
  翻转 view 合并态。包内跨文件同名 decl 禁入被 use 方 test 体。
- 覆盖清单:Resp 构造器族(html/json/text/redirect/status/err_json/xml/yaml)+ with/with_status/
  attachment + resp 三读 + json_obj 五件(end/end_status)+ csv_rows + Req 六取参(query/
  query_all/form/form_all/header/cookie)+ param/param_i64/param_f64 + pdec 边域 + req_of/
  req_full/req_set_params + Router 全组合子(get/post/put/delete/patch/middleware/mount_at/
  install)+ route_match 三档/405 Allow/has_conflict/dispatch/test_call + 中间件短路穿透 +
  视图 el 族(结构性转义/raw/children/自闭形)。param_f64 在册确认(spec §4.3)。
- 2b 待办指针:serve 循环/body_limit/not_found/method_not_allowed 挂点/static/openapi/
  bytes(挂 §7-8 二进制门)/send_file/req.json+JDoc 校验族/session 族(with_sessions/
  grant_session(5 参含 now=§8-A5 注入)/drop_session/flash)/README 示例迁移(todo_app 对照 §10 验收门)/原生 e2e 门
  (挂 L6/L7 销账)。全景见 `web/README.md` API 表两列。
- suite 回归:web/ 不在 tests/,suite.py 计数不动(87/88+88/88 基线,本笔复跑确认)。

**2026-09-28 T14 Channel 去限制三片落库**(spec-gap W2;§7.3/§12.3):
- **①容量上限解除**(0e004a2):自举模板 ct_chan buf[64] 堆化+取模 %64→%cap+cap<1→1
  (fail-closed)+注册表定长 64→增长数组(realloc 加锁,原无锁竞态顺手收口);宿主发射器
  本即类型化堆环无需动;锚 coro_conc chan_cap_smoke(单线程 128 发不阻塞=在途容量实证)。
- **②List[struct] 发射底座**(b6d26fe;divergences (h) 族首件收口):元素码 Lu:<名> 贯通
  (注解位 ct_ty_code/构造推断位 ct_typeof/索引派型位);push 堆盒装箱(值拷贝);索引解
  引用取回;for-in 补 L/Lu: 两族(此前 for-over-list 发射面整个缺位);L(char*)/LI(int)
  既有行为零扰动;锚 fx_list_struct(装箱/索引/for-in/struct 字段持有/值拷贝语义)。
- **③句柄作字段+struct 入通道**(369b9bc):Sender/Receiver/Channel 注解型码 h——作
  struct 字段/形参/返回位贯通(P5-E 池持通道「emit int32 截断」缺口收口,db/pool 消费方
  迁移随 db 泳道);struct 值入通道 = send 装箱 + match Ok 臂解引用(解构位 rx 绑定
  hR<元素码>);锚 fx_chan_structs;登记:recv().expect 于 struct 载荷未支持(match 为
  消费正形)、句柄注解无型参(hR 元素码仅解构位可得)。
- 同底座连带解锁:SocketAddr struct 化(T09 v0 文本承载可升级)、form.ct List[struct]
  产错型强转登记位、04g 类方法发射((h) 族下一件)。
- 门禁:net 17/17×2、suite 87/88、db 44/10、http 110/1(基线红不变)、cargo 14/0、
  meta/fmt/ffi 过。

**2026-09-28 T15 Iterator for 集成三线落地**(spec-gap W3;§3.8.2/§4;R-P3b 首件):
- r3b_iter_trait 锚迁入主套件 03k_iter_for(E2030 语义正交,无 neg);锚修订:&var self
  (非文法形)→ var self(EBNF 正形,起草笔误);适配器测试拆出 r3b_iter_adapters(T16 域)。
- 三线 interp:自举 For 处理器 U 分支(find_impl_method next + 共享 env 运行方法体——
  self 绑定于循环 env,var cur += 1 经 env 写回对下一轮可见);C 宿主 ST_FOR V_STRUCT 分支
  (cls_method + 同 env 直跑 body 免 call_method_body 弹栈丢 mutation);R 线 Stmt::For
  Struct 分支(find_method_block + fenv self 绑定;R 线 struct 值字段 Rc<RefCell> 共享 =
  mutation 天然对循环侧可见)。
- 顺手修:解析器 &var 前缀死循环(& 消费后 var 被当参数名 + else 双 adv 越过 self →
  p_typ 死循环;防御消耗按 var 语义)。
- 发射臂登记:var self 方法按值传递 = mutation 丢失(Iterator for 发射缺口;泛 var 参数
  引用语义=发射层系统改造,归后续批次)。
- suite 88/89×2(+1 全绿);cargo 14/0;meta 过;db 44/10 基线;net 17/17。

**2026-09-28 T16+T17 std/iter 归位+06f 翻转**(spec-gap W3;§7.7):
- **T16 std/iter.ct 适配器面**:数组终结器(sum/count/any/all/collect)+适配器 struct
  (MapArr/FilterArr/TakeArr + 终结函数)。**阻塞登记**:UFCS 链式调度(`xs.map(f).filter(f).sum()`)
  需同名函数作用于不同适配器类型 = 函数重载或泛型 trait 分发(皆 T25 域);泛型 struct
  MapIter[A,B] 被实例化字母表限标量/Str 阻断。r3b_adapters/r3b_iter_adapters 锚维持红
  (T25 依赖,翻转条件在案)。
- **T17 06f 翻转**:std/iter.ct 创建(§7.7 归位);加载器补前奏符号 use 显式导入恒可见
  (§3.8 前奏隐式可用——use std.iter.{parallel} 为文档化冗余,pkg_is_prelude_sym 判定)。
  06f_parallel.ct 常驻红首次翻绿。parallel 前奏直用保持兼容(过渡双轨)。
- **suite 89/89 双线——主套件首次 100%**(自举+宿主全绿);cargo 14/0;meta/db/net/http
  基线全维持(唯一红=http frm_auth_a_jwt interp OOM 预存在,归 T31 arena 债)。

**2026-09-28 T21 宿主检查面红账五件销账**(spec-gap W4):
- r6b(E4050 类持资源字段)/r6c(as[U64] 负源模 2^64)/r6d(W8050)/r6e(E4040)/r6f(E6010)
  五锚迁入主套件 05h/03l/05i/05j/05k;r6d/r6e 宿主先前行已绿(免改)。
- C 宿主三件补齐:E4050(D_CLASS 字段 Mutex/Channel/Sender/Receiver 检查)+
  wrap_int U64 修(bits==64 && us 时掩码 2^64-1——此前 bits>=64 直通透传,负源 -1 经
  __int128 全宽无符号读出 = 2^128-1)+E6010(ceval_21 迷你树走查求值器:字面量/ident/
  算术/比较/While/Let/Return/Assign/comptime fn 调用;步数>1200 → E6010;
  ST_ASSIGN 补 env 绑定更新;depth 跟踪移除——只增不减致 35 轮假爆,步数已兜底)。
- E4050 消息对齐自举(去「需确定性释放的」——msg 子串锚一致)。
- suite 94/94 双线;cargo 14/0;meta/db/net 基线全维持。

**2026-09-28 T22+T23 宿主运行面+e2e 红账核销**(spec-gap W4):
- T22:r4d/r6a 两锚**已在前批迁主套件**(03k_u64_overflow.panic / 05h_drop_panic_unwind.panic)
  且双线绿(09-15 宿主 fits 上界+rt_panic_unwind 落地时已修);COVERAGE §172-173 红账
  条目系过期登记,本次核销。
- T23:e2e 缺声明 6 红**已在前批全清**(09-25 深夜 P1b 契约补全——use 清单全量请求);
  http 110/1(唯一红=frm_auth_a_jwt interp OOM,T31 arena 债);OTLP send 裸 socket 形
  维持在册。

**2026-09-28 T20 deterministic+内容寻址缓存落库**(spec-gap W4;§10.3):
- --deterministic CLI 旋钮(ctc.sh):设 CTRON_RT_SEED=1 → 种子化弹出序+单 worker
  (机制已有 ctron_rt.c:1010-1027;旋钮只做 env 传播);并发夹具同种子三跑一致实证。
- 内容寻址缓存(ctc.sh emit 臂):源文件哈希+编译器 cc_emit.ct 哈希前 12 位 = 缓存 key
  (防编译器升级后陈旧缓存);.cache/emit/<key>.c 命中即复用,免重发射;未命中发射后入缓。
  同源双跑产物逐字节一致实证(cmp 过)。
- suite 94/94×2;net 17/17;fmt parity 479/0 全维持。
- 登记:缓存命中不重跑 build.sh(编译器源变更即 cc_emit hash 变 → 自动 miss);
  跨机复用需迁移 .cache 目录(v1 本机口径)。

**2026-09-28 T18 列级精确 span 落库**(spec-gap W4;§10.2):
- **解析器诊断(pdiags)入 JSON 面**:json_diag_lc(LINE:COL 解析版)——精确 line_start/
  col_start/line_end/col_end(列级 1-based);JSON 早退路径补(chk_fmt 提前初始化至 pdiags
  检查前——此前 pdiags 早退恒走文本面)。验证:`let x = ;` → E1001 {line:2, col:13}。
- **sem 诊断行号保持**:@LINE 机制既有;列=1 近似(v1 待 AST 列标注贯通)。
- 已知限制在册:部分 sem 诊断行号 nline() 对嵌套 Binary 节点返回垃圾值(01c 的 E1001
  chain 检查 60100140——预存在,AST 尾槽行号戳覆盖面不足所致);sem 列=1。
- suite 94/94 双线维持。

**2026-09-28 T19 fix-it 首批落库**(spec-gap W4;§10.1):
- json_diag fixes 产出:按码生成修复建议——E3030(replace edit:static var→let,
  span 精确 line/col)、E2020(note:检查拼写或加 use)、W8010(note:改值字段或 Box)。
- 验证:E3030 → fixes=[{title, edits:[{kind:replace, span:{L1:C1-11}, text:"let"}]}]。
- 契约:edits.kind ∈ replace|insert|delete;span 1-based(§10.2 冻结 schema)。
- suite 94/94 双线维持;meta 过。

**2026-09-29 T25 &Trait 发射侧动态分发落库**(spec-gap W5;§3.5;T24 ABI 实施):
- 四步路径全通:①型码(&Trait → t:<Trait> → ct_obj 胖指针 {data,vtable})②vtable 常量
  发射(trait 方法槽 struct + thunk 包装(具体型↔void* 桥接)+ static const 常量,仅
  &Trait 参数位引用的 trait 产 vtable——Drop/UFCS 走既有路径不干扰)③构造位(调用点
  具体型实参 → GNU 语句表达式装箱:堆盒+vt 取址)④调用位(接收者 t: 码 → vtable 间接
  跳转 `((vt_T*)obj.vtable)->m(obj.data)`)。
- **&Trait 引用门**(ct_trait_used_as_ref):仅 fn 形参位出现 `&Trait` 的 trait 产
  vtable——net 域包 7 个 trait/impl(Net/StdNet 等)无 &Trait 参数位引用 → 不产
  vtable → 零干扰(net 17/17 维持);Drop impl 有专属路径(ct_drop_fn)天然隔离。
- 方法体发射:vtable emit 内联 standalone fn(t_<Type>__<method>,self 绑定具体型)
  ——thunk 消费;既有 pass1/pass2 Impl 处理不受扰。
- 验证:/tmp/trait_obj.ct(Clock/FakeClock/elapsed_since&Clock)发射→cc→run rc=0
  (100-50=50 断言过=动态分发正确);07_capabilities interp 侧本即绿。
- suite 94/94 双线;net 17/17;db 44/10 基线;cargo 14/0(顺手清 r6c 过期引用);
  meta ✓;fmt parity 479/0。
- 登记:发射面限定=非泛型 trait + 方法返回 ct_i(标量);prop getter 槽/泛型 trait
  对象/超 trait 前缀=v2(设计文档在案)。

**2026-09-29 T26 能力对象注入形态归位落库**(spec-gap W5;§8.1/§11.2;R-P2b 两锚翻转):
- **r2b_fs_fake/r2b_env 翻转**:锚迁主套件 `07a_cap_fs_inject`(Fs 能力 trait + MemFs
  fake 注入 + FsError message + @derive(Error) class 声明面)/`07b_cap_env`
  (Env.system/get/args);roadmap_suite 删两行,语料删除(翻转协议)。
- **std/fs.ct**:能力面入 std——`trait Fs: Cap`(read_to_string/write/exists,
  Result[String/FVoid, FsError] 错误面)+ `pub struct FsError { message }`。
- **loader**:Trait 声明接口面恒可见(v0 无 pub trait 语法位;跨模块 impl/参数引用
  须先可导入,§3.4/§8.1);`use std.process.{Env}` 前奏符号冗余导入放行(T17 先例)。
- **@+class 解析路由修复**:`@derive(...)` 前缀 class 曾坠 p_fn 段错误(parse_decl
  @ 分支只路由 struct/enum/trait);Class 节点尾槽 Drvs(消费方按 [0] 标签扫不受扰)。
- **Env 进程环境能力对象**(§8.1;07b):前奏命名空间形(Env.system()→句柄;
  get→Option[Str],缺失=None;args→List[Str])。自举:eval NS 路由 + ctron_prog_args_
  n/at 运行时底座(发射模板 main 入口寄存 argc/argv;编译器自举经 extern 直调破鸡生蛋);
  C 宿主:rt v_ns("Env")+ EnvHandle 分发 + main 寄存;R 线:Value::EnvHandle +
  std::env 同面。**Env 发射面=interp 三线先行**(stdweb.dom 同族),用户程序发射
  Env 方法列登记随 stdweb 真实化批次。
- **发射 vtable 全签名(T25 v1 面=恒 ct_i 单 self 槽的扩展)**:槽=trait 声明位真返回
  码+非 self 形参;thunk 桥接(真返回,void 无 return;形参名保留原名=方法体 t_<原名>
  引用);调用位非接收者实参按声明序追加。net 真窗 `tests/net/cap_inject`:FakeNet
  (struct,确定性时钟)与 StdNet 同 &Net 槽注入,probe I32/now_ns I64/sleep_ms 带参
  void 三槽形,双矩阵 18/18。
- **R 线前奏近似**:Fs/Net trait 方法签名 + FsError(struct)/Env 承载于 register_
  prelude(单文件 check 无 loader;**面与 std/fs.ct、net.ct 真源同步纪律**——std 面
  变更须同步此处,07a/07b 行为锚);trait_has_method 扩查前奏型表;Prelude 型 defs
  字段可读(FsError.message)。
- 登记:①db 门面 trait 化缓期——现能力面仅 dsn 纯函数+协议半层,连接面(P5 后续)
  落地才有真能力方法,提前造 probe trait=死 trait(T15 裁决反例);②class 字面量
  发射全缺(StructLit 非值类型 panic)——W6/GC 相邻债,发射臂 fake 注入=struct 承载;
  ③`use std.fs.Fs` 点式符号导入 loader 未实现(§2.5 主形态;现按在库先例取括号组
  导入形,点式 loader 面独立登记);④测试套 96/96 双线;modules use_alias_nat(自举
  E2020.use.miss 消息不含 use.nat 锚码)/dup_static(宿主)两红=HEAD 预存(stash
  实证),非本批引入;tests/ffi/arr_field(机刷泳道 WIP,无 Ctron.ctcl)堵 meta_check
  门——非己债登记。

## §web 2c 次波增量(web 泳道,2026-09-28)

- **compress_mw(fb0de38)**:gzip 响应压缩(http/enc 薄收编)——四门(CE 已带→协商→阈值→文本族)+ CL 原位换值 + Vary;**勿裸上线闸在册**:gzip 成员恒含 NUL 而 NUL 过 Str 即失(活证 utf8_enc(0).len==0/"AB"+NUL+"CD"=="ABCD")→ U+0100 载体形(单射无损+CRC 兜底,评审证实)wire 面待 render 咽喉解码或 L4 Bytes;emit 臂 e2e 4096→98B + python gzip.decompress 差分在案
- **guard.ct(9bf1e64)**:csrf 双提交(frm/csrf 薄收编,写方法 cookie vs form 令牌,败 403,通过面恒发新 cookie)+ 令牌桶限流(frm/limit 薄收编,per-key=XFF 首段[可伪造=v1 边界],in-flight shed 503,时钟注入可测,键表满 fail-open 纯面钉死);Mutex[List[BktEnt]] 原位替换写回经 429 断言自证
- **门**:十文件 check OK + 裸跑 rc=0(mw/guard 走 canonical bin/ctron-cc + CTRON_STDPATH;seed 宿主 crypto 闭包挂死在册);suite 94/94+94/94(基线随上游演进)
- **指针**:L6/L7=T54/T55 候选入册 spec-gap 计划(待用户裁决);压缩线两条件(x_web_compress 夹具+wire 形决策)不得滞留

**2026-09-29 T29 切片1:M1 tracing GC 保守根先行落库**(spec-gap W6;§6.2/T28 M1 契约):
- 发射运行时 mark-sweep:超级块刻切+头{sz,mk}+空闲链+空块 free;根=机器栈[帧址,
  main 基]+chan 环+mcell 注册表+bump 区+载荷传播(list 魔数跟扫 items);list_new/Box/
  Lu 盒入 GC 堆;spawn 置并发退避(任务栈不扫,自动回落 bump);CTRON_GC=1 开启,
  默认 bump(浸泡后翻默认);阈值 CTRON_GC_THRESHOLD(8MB)。
- 泄漏锚 `tests/gc/cycle_reclaim`(gc 泳道新立):自引用 List 环 200 轮,collect 后
  live bytes 回落断言;双档 2/2。观测口 ct_gc_live_bytes/ct_gc_collect。
- 验证:锚 2/2+suite 默认/GC 档 96/96×2+net 双矩阵 18/18×2+smoke 155/0;decl 422。
- 登记(设计注记,任务卡保守扫描条款):①保守标记不保留纯内部指针/静态 C 全局不扫
  (M1.5 精确帧位图=契约 §6.3 替换);②coro/并发档恒 bump(契约注记在案);③闭包 env/
  view lane/字符串留 bump(M2 面);④S2a/语义零变更(解释臂无 GC,双臂一致)。
- 余债:peer 本地链未含远端 T26 合并(锁 420 vs 436);本片在 t29-gc-m1 分支待整合。

**2026-09-30 T29 切片2:db GC 档验收+翻面证伪**(spec-gap W6;T29 验收面收口):
- db GC 矩阵:db/run.sh 编译器自身臂 `env -u CTRON_GC` 豁免(自举编译器巨型 bump
  arena 经 env 继承激活 GC → 保守扫描近挂死实证,46min CPU 病灶);GC=1 臂与默认臂
  **同红集(40/14)**=GC 无害性证明(新增红=peer 库根重构 pg_* 符号 E2020×5+种子
  漂移守卫路径陈旧×5,在册非 GC 债)。
- **GC 默认档翻面证伪推迟**:同上病理;翻面前置=arena 界定/编译器自指豁免或精确
  帧位图(切片3)。CTRON_GC 维持显式 opt-in。
- 验收:T29 卡三条全过(泄漏锚/suite off 双档/net+db GC 档),卡转 ✅(切片3 增强另册)。

**T55 销账(2026-09-30,提交 7590794)**:值位置闭包捕获原生臂落地——03j emit 同病顺带翻转(基线 2 errors→`mw:/x`),探针 t55_probe/t55_mw_smoke 双臂绿,suite 98/98×2;创建时快照边界+T27 注记在案(spec §9 L7)。

**2026-09-30 T30 into_gc 深拷真实化落库**(spec-gap W6;§6.3):
- 发射侧 from 恒等 → 真深拷:let 构造位绑 #elem/#elemsz/#ielem(List[T]/arena.list[T];
  Option/Result #elem 先例),into_gc 位 ct_ig_l(src,mode,esz,imode) 静态分派
  (0=标量槽/1=Str strdup/2=嵌套表一层递归/3=struct 盒 memcpy);无绑定位恒等回退(v1 兼容,
  GC=off 档同语义=拷贝目标 arena 化,隔离不变)。
- C 宿主嵌套别名修复:list_clone_deep 内部对 V_LIST 槽递归(g[0].push 别名污染 inner=2!=1
  实证);clone_val 通用口保持 List=引用共享语义不动(初版误改通用口→conc 族 15 红,
  smoke 实证后回退收窄——教训:clone_val 是宿主全量克隆口,语义改动只准入 into_gc 专用径)。
- 新锚 05i_into_gc_deep_copy(int/Str 平坦+出 fn 域存活+双向独立)interp/emit×
  默认/GC=1 四象限绿;suite 99/99 双线(含新锚)。
- 登记三笔:①05g emit 臂红=存量 own 块尾 return void 发射缺口(A/B 基线实证,非本批);
  ②嵌套 List 发射读侧 int 槽 strcmp(0x5) 崩=lane 读型不对称存量(嵌套拷贝本身双侧通,
  锚裁剪登记);③peer 库根重构 suite.py 两处 stdpath 陈旧(主循环裸跑/modules 段),
  lib/std 修正后 99/99 复绿——根因是 03m/07a 两个 std 导入件红而 CI 前次未及跑本批。

**2026-10-01 T31 帧回收负结果落账**(spec-gap W6;bench 纪律:只信同机差分):
- 实施面:ct_amark/ct_areset/ct_eveto_* 模板助手(T26 钻孔四件套)+call_decl_vals/call_cv
  Ctron 级 wrapper+五处逃逸否决(闭包创建/push/send/with_mut/Member·Index Assign)+
  标量返回门(tag 5/8 否)+out 增长门。
- 结果:suite user 2.62s→3.22s(+23%,real 14.92→24.64s 含子进程噪声)+ 06d_globals
  红(Global with_mut 写回逃逸未盖)→ **证伪**:解释器每调用一次 Ctron 级 wrapper 的
  dispatch 开销高于帧内 bump 复用收益;且逃逸面(任意 pre-call 容器/字段的写入路径)
  无法穷尽审计。已干净回退,suite 99/99 复绿。
- 重开前置:值模型原生 mark/reset(eval Val C 级帧钩子/宿主 rt 同源),列 M2;
  模板助手设计稿留存计划卡。T31 不阻 T32(GC 门禁对 emit 臂计量)。

**2026-10-01 T32 GC 性能门禁落库**(spec-gap W6 收官;§9.4):
- tests/gc/bench.sh:分配 churn 核(3k×100 List 建填弃),同二进制双档 CTRON_GC=1/off,
  digest 硬门(两档输出逐字一致)+ ×3 取 min 比值;入 ci.sh [7/9]。
- 首版实测:**比值 10.28**(GC 689ms vs bump 67ms;darwin arm64,cc -O1,同机同 bin)。
  归因:①churn=纯垃圾最劣面——每 8MB 阈值全堆 mark-sweep,保守根扫描(栈+bump 区+
  注册表)每轮固定成本×6-7 轮;②M1 无分代/增量(M2 面);③bump 档永不回收,
  有限 churn 下天然零回收成本。长驻服务面(存活集大、垃圾率低)摊销显著更好。
- §9.4 退出条件语义:登记比值与归因即交付;门禁硬化(比值>1.15 转 fail)随 GC 调优
  批次(阈值自适应/分代/根集精确化=M1.5/M2)。

**2026-10-01 T33 parallel 真并行落库**(spec-gap W7;§7.7):
- 发射侧 fork-join:K=8 定长分块 pthread/join(与 scope 任务池同风格;分块序固定=
  确定性),map 各块写结果表不相交槽;reduce 按分块序合并空块跳过(结合律文档承诺
  在卡);List(List/LI)与定长数组(I32/I64 宽,§7.7 &T[] 视图)双形;CTRON_PAR=off
  串行回退(List 形;数组形恒并行,登记)。
- 纯度门:ct_emit_clo 捕获即编译期拒绝(「非捕获闭包含捕获」panic=结构性先例,纯度
  推断的发射面承载);interp 保持顺序化(结果确定性同)。
- parallel let 专用型别:map→LI(发射恒 int 槽)/reduce→6。
- 加速比锚:N=10^6 同 bin 对照,串行 5ms vs 并行 1ms ≈**5×**(darwin arm64,cc -O1)。
- 06f 三路绿(interp/emit 并行/emit off);suite 99/99+smoke 161/0+net 18/18;
  gc/run.sh 补 bench 夹具 skip(net 泳道先例)。
