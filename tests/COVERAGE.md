# 测试覆盖审计(v0.4 规范 ↔ 测试集)

> **第二批补测已落地(2026-09-04)**:新增 16 个单文件测试 + 5 个多文件模块用例,覆盖从 **48/121(40%)提升到约 92/121(76%)**。下文各章表格为首批审计基线(历史),第二批覆盖项见本节清单;剩余缺口约 18 项,集中在 P2 长尾与需后端的项。

## 第二批补测清单(2026-09-04)

**P0 —— v0.4 新特性:** `03f_slices.ct`(切片二分/T[N] 退化/隐式只读化)、`03g_fn_types.ct`(函数类型/多参闭包)、`06b_slice_nonsend.neg.ct`、`06c_static_nonsend.neg.ct`(E3031)
**P1 —— 核心语义:** `05d_drop.ct`(RAII 逆序)、`02b_option_propagation.ct`(Option `?`/expect/`T?`/元组变体/块臂)、`04b_logic.ct`(`&&`/`!`/德摩根/`%`符号/复合赋值/遮蔽/range 值/else-if)、`03b_numeric_widths.ct`(宽度全集/进制/分隔/自适应/`as` 截断)、`02d_divzero.panic.ct`、`05b_panic_join.ct`(`panic()`/Never/`join_or`)、`05e_own_gc_mut.neg.ct`(E3060)、`06f_parallel.ct`(数据并行)、`06d_globals.ct`(static let/Global/Atomic)、`06e_cancel.ct`(取消传播)
**基建:** `01c_parse.neg.ct`(E1001 注册 + 比较不可链)、`tests/modules/` 五例(use_ok 组导入+pub/pub(pkg)、orphan E5010、circular E5020、visibility E2020、caps E4010);README §6/§7 多文件格式与 std 隐式链接规则;前奏表补 Atomic/Global/Drop 行;数组字面量归属(`T[N]`)写入 §3.6。

**仍开放的缺口(第三批,P2/后端):** 字符串转义/插值链/多行链、doc-test 样例、match 字面量与 struct 模式、`prop`/默认方法/超 trait 组合、泛型 struct/bound/`@derive(Show,Eq)`、Str/String 行为、隐式转换全集(String→Str)、W8020、`#[no_alloc]` 契约、ISR(E4030)、comptime 预算(E6010/6020/6030)、`into_gc` 隔离性、`?` 位置链、FFI/Simd/JS 桥(需对应后端)。

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
