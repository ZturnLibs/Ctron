# R 线路线图测试设计(测试先行语料:tests/roadmap/)

> 创建:2026-09-07。上游:`docs/superpowers/specs/2026-09-07-r-roadmap.md`(R-P2…R-P7)。
> 方法论:延续本项目"**测试先行**"——语料先于实现钉死语法/语义裁决(红 = 规范锚,实现跟上即转绿,
> 先例:`09_simd.ct`/`10_web_dom.ct`)。范围仅 Rust 线;61 文件 v0.5 冻结语料**零改动**。

## 0. 总体设计

### 0.1 语料位置与隔离

新语料全部放 **`tests/roadmap/`**(单文件)+ `tests/roadmap/modules/<case>/`(多文件包):

- 文件命名 `r<里程碑号>_<主题>.ct`,后缀约定与主流语料一致(`.neg.ct`/`.panic.ct`/`.lint.ct`)。
- 三个主流套件(run/check/native)与 campaign.py 对 `roadmap/` 前缀**跳过**(同 `modules/` 先例);
  v0.5 的 61 文件冻结语料与既有 116 项符合性判定**不受任何影响**。
- `meta_check.py` 自动覆盖(rglob),标记/错误码/命名校验免费获得。

### 0.2 锚点状态机(roadmap_suite.rs)

新套件 `compiler-rust/tests/roadmap_suite.rs` 以**表驱动**记录每个文件的当前锚定状态:

| 状态 | 含义 | 断言 |
|---|---|---|
| `InterpGreen` | 今天解释器就应全 test 通过 | `run_test_file` 全绿;原生允许 skip(trans 域外) |
| `CheckDiags(&[codes])` | 未实现锚:check 必须产出**且仅因**这些码失败 | 诊断码 ⊇ 列表(钉住"红的原因是未实现,不是语料笔误") |
| `ModuleDiags(&[codes])` | modules 用例的 CheckDiags | `check_package` 诊断码 ⊇ 列表 |
| `Deferred` | 需要宿主环境/尚不可执行 | 仅统计 |

每条记录附里程碑 tag;落地翻转流程 = 该文件在主流套件登记(零诊断+运行+原生差分),
roadmap_suite 表项改为 `InterpGreen` 或删除——**进度表即路线图燃尽图**(测试打印里程碑计数)。

### 0.3 语料即规范:本批钉死的 API/语义(v0.6 修订草案)

**容器(R-P2a)**——构造沿 `Channel[U8](4)` 惯例 `Type[T](…)`:

| API | 裁决 |
|---|---|
| `List[T]()` / `.push(x)` / `.pop() -> T?` / `.len` | 增长数组;`.len` 为属性(钉子 18) |
| `xs[i]` / `.get(i) -> T?` / `.set(i, x)` | 下标越界 panic "index out of bounds";`get` 安全返回 Option |
| `for x in list` | 直接可迭代 |
| `Map[K,V]()` / `.insert(k,v)` / `.get(k) -> V?` / `.remove(k) -> V?` / `.contains_key(k)` / `.len` | get 缺键返回 None(无 null 原则);K 初期限 Str/整数内建 |
| `m.keys() -> List[K]` / `.values() -> List[V]` | **迭代顺序不承诺**(确定性种子冻结,§11.7);需顺序先 `.sort()` |
| `Set[T]()` / `.insert(x) -> Bool` / `.contains(x)` / `.remove(x)` / `.len` | insert 返回"是否新增" |
| `StringBuilder()` / `.push_str(s: Str)` / `.to_string() -> String` / `.len` | owned 字符串构建器 |
| 容器 Send 性 | 元素 Send 则容器 Send(spawn 捕获合法) |
| 容器分配属性 | 构造/push = `alloc` → `#[no_alloc]`/bare/own 拦截(既有 E3040 机制) |

**std 能力(R-P2b)**——真实实现由 std 提供,测试以 07 的 Fake 注入模式钉 API 形状:

| API | 裁决 |
|---|---|
| `std.fs`:`read_to_string(fs, path) -> Result[String, FsError]`、`write(fs, path, data) -> Result[Void, FsError]`、`exists(fs, path) -> Bool` | Fs: Cap;真实 IO 仅经能力值 |
| `std.time`:`Clock: Cap` + `now() -> U64` + `elapsed_since(start) -> U64` | 与 07 同形,真实实现为单调时钟 |
| `std.process`:`Env: Cap` + `env(name) -> Str?` + `args() -> List[Str]` | 缺环境变量返回 None |
| manifest `[caps]` | fs/time/env 键越权 = E4010(机制已有,扩展键集) |

**捕获闭包(R-P3a)**——v0.6 §4.7 裁决:

- 默认**按值拷贝**捕获;`&T` 只读借用捕获(借用不得随闭包逃逸存储);
- **可变捕获必须显式 `Mutex[T]` 包装**,裸 var 捕获 = 新码 **E3070**(注册见 §2);
- 闭包可从函数返回(环境随闭包存活);无 Fn/FnMut 分裂(§13 拒绝清单)。

**迭代器(R-P3b)**:

- `trait Iterator[T] { fn next(…) -> T? }`(self 形态 `var self` vs `&var self` 留 R-P3b 实现时钉,
  语料标注两可点);适配器 `map/filter/take/zip/enumerate/rev`,终结 `collect/sum/min/max/count/any/all/foreach`;
- `for` 接受 Iterator;数组/List/Range 实现之;惰性(链式零中间集合)——单态化零成本;
- 内建适配器方法面(`xs.map(f).filter(g).sum()`)独立于用户 trait,先行钉死。

**模式增强(R-P3c)**:守卫 `pat if cond =>`;或模式 `A | B =>`;守卫臂**不参与穷尽**(穷尽不足 = E2030);
字符串字面量模式合法。

**comptime(R-P5a)**:`const` 可用作定长数组尺寸 `I32[N]`;comptime fn 纯函数子集语义不变
(现状运行期求值语义等价,本批含语义回归锚防 R-P5a 重构破坏)。

## 1. 测试矩阵(状态为首跑实测校准,2026-09-07)

| 文件(tests/roadmap/) | 里程碑 | 钉住的语义 | 实测初始状态 |
|---|---|---|---|
| `r2a_list.ct` | R-P2a | List 全方法面/泛型函数接容器/for | RunRed(check 已过,运行时未实现) |
| `r2a_list_oob.panic.ct` | R-P2a | 下标越界 panic "index out of bounds" | RunRed |
| `r2a_map.ct` | R-P2a | Map get→Option/remove/contains/keys/values | RunRed |
| `r2a_set.ct` | R-P2a | Set 去重/insert 返回值 | RunRed |
| `r2a_sb.ct` | R-P2a | StringBuilder 拼接/to_string | RunRed |
| `r2a_container_send.ct` | R-P2a | 容器跨 spawn 移动(Send 传递性) | RunRed(check 已接受容器 Send) |
| `r2a_container_alloc.neg.ct` | R-P2a | no_alloc 上下文容器操作 = E3040 | **NegGreen(E3040 已生效)** |
| `r2b_fs_fake.ct` | R-P2b | std.fs API 形状 + Fake 注入 | RunRed(std.* 桩已解析) |
| `r2b_time.ct` | R-P2b | std.time Clock 真实实现形状 | RunRed |
| `r2b_env.ct` | R-P2b | std.process env→Option/args | RunRed |
| `r3a_capture.ct` | R-P3a | 拷贝捕获/闭包返回/Mutex 捕获 | **Green(解释器已支持)**;原生差分 R-P3a |
| `r3a_capture_var.neg.ct` | R-P3a | 裸 var 捕获 = E3070 | NegPending(check 现状零诊断放行) |
| `r3b_adapters.ct` | R-P3b | map/filter/sum 适配器链(惰性) | RunRed(数组 map 已有,余缺) |
| `r3b_iter_trait.ct` | R-P3b | 用户实现 Iterator + for 集成 | RunRed |
| `r3c_guards.ct` | R-P3c | 守卫/或模式/字符串模式/穷尽配合 | CheckRed(E1001 解析拒绝) |
| `r3c_guard_exhaustive.neg.ct` | R-P3c | 守卫臂不参与穷尽 → E2030 | NegPending(现状 E1001→翻转为 E2030) |
| `r2d_fmt_fixture.ct` | R-P2d | fmt 夹具:嵌套 if/match 排版 | **Green** |
| `r2d_fmt_chain.ct` | R-P2d | fmt 夹具:首点链换行(§1.6) | **Green** |
| `r4b_panic_location.panic.ct` | R-P4b | panic 消息含 `.ct:行号` 源位置 | PanicMsgRed(现 panic 无位置) |
| `r4c_arena_send.neg.ct` | R-P4c | arena 句柄跨 spawn = E3010 | **NegGreen(E3010 已生效——checker 已判 arena 非 Send)** |
| `r5a_const_size.ct` | R-P5a | const 作定长数组尺寸 | **Green(检查+运行均已实现,超预期)** |
| `r5a_semantics.ct` | R-P5a | comptime 求值语义回归锚 | Green |
| `modules/caps_fs/` | R-P2b | 用 Fs 未声明 [caps].fs → E4010 | **已生效(E4010 机制完整)** |
| `modules/path_dep/` | R-P7 | path 依赖格式钉死(app 依赖 lib) | 锚红(app 以 E2020 失败;lib 零诊断) |

**明确不进语料、归实现里程碑验收的**(记录防止覆盖缺口误判):`ctron test` CLI 行为(R-P2c)、
fmt 幂等性(R-P2d,套件级断言)、`#line` 生成属性(R-P4b,转译输出断言)、ASan 无泄漏(R-P4c)、
CI 矩阵(R-P4d)、doctest 执行(R-P5b)、LSP(R-P6b)、WASI 产物(R-P6a)、git 依赖网络路径(R-P7)。

## 2. 新错误码注册

| 码 | 含义 | 章节 |
|---|---|---|
| **E3070** | 闭包可变捕获未显式 `Mutex[T]` 包装 | §4.7(v0.6 草案) |

注册位置:tests/README.md §4、tests/meta_check.py `ERROR_CODES`、docs/spec/10(预留行,同 E3031 先例)。

## 3. 翻转协议(实现落地时)

1. 里程碑实现合入 → 对应语料在主流套件登记:`check_suite` 零诊断(或 neg 期望码)、
   `run_suite` 正常运行、`native_suite` 原生差分;
2. `roadmap_suite` 表项移除(或改 `InterpGreen` 保留语义回归价值);
3. COVERAGE.md 增补批次清单(延续第一~四批的编年体例);
4. 进度表打印的剩余锚计数减一——路线图燃尽。
