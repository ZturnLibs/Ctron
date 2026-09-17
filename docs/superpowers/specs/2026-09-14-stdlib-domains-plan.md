# Ctron 标准库内容域能力规划(std Domains Plan)

日期:2026-09-14
状态:方案(设计记录,未实施)
范围:std/ 规范源的内容域划分、逐域能力面、与语言阶段(P0–P3)的依赖映射、治理与演进纪律。

---

## 1. 定位与输入

标准库是 **Ctron 源码形态分发**的模块集合(`std/` 为规范源,vendored 消费,
组织宪章见 `std/README.md`)。本方案回答三个问题:

1. 内容域怎么分——哪些域、每域哪些模块、每个模块具体提供什么能力;
2. 顺序怎么排——哪些能力当前方言就能做,哪些被语言/内建面能力卡住,卡在哪;
3. 治理怎么走——稳定性分级、版本化、错误与命名纪律,避免 Go 1 式过早冻结。

输入(已盘点):

- **语言规范 v0.7**(`docs/spec/`):类型系统(泛型 bound/`@derive`)、错误模型
  (Option/Result/`?`/Error trait)、内存(GC + own/Arena + alloc effect)、并发
  (结构化 scope/Channel/Mutex/Send)、能力对象(`&Clock`/`&Fs`/`&Env` 注入)、
  三档 `full/web/bare` 与 `core < alloc < std|stdweb` 分层(§9.1)。
- **std 现状**:7 模块(str/sort/map/set/fmap/fs/json),全部为纯 Ctron、
  字节级、函数式、测试随模块。
- **内建面(自举 `compiler/src/eval_call.ct` 现状)**:`print/println/assert/
  assert_eq/assert_ne/panic`、`read_file/read_dir/fs_exists/fs_write/fs_delete`、
  `now_ms/ctron_entry`、`utf8_enc/byte_at/byte_slice`;成员方法面 List/String/
  Option/Result/Scope/Channel/Mutex/Atomic/parallel/Simd。
- **已被路线图钉死的 std 名字**(不得改名,新域须兼容):
  - `std.process.Env`:`Env.system()` / `get -> Option` / `args`
    (`tests/roadmap/r2b_env.ct`);
  - `std.time.Clock`:`Clock.real()` / `now` / `elapsed_since`(单调不减)
    (`tests/roadmap/r2b_time.ct`);
  - 迭代器:`trait Iterator[T] { fn next(&var self) -> T? }` + for 集成 +
    惰性适配器 `map/filter/take` + 终结子 `sum/count/any/all/collect`
    (`tests/roadmap/r3b_iter_trait.ct`、`r3b_adapters.ct`);
  - `stdweb.dom.set_title/title`(spec §9.2,P1-D 起);
  - `bit` 模块(spec §4.5 挂账:"位运算经 stdlib bit 模块");
  - `parallel.map/reduce/fold`(spec §7.7,iter 模块内)。

---

## 2. 对标分析:不成熟语言的标准库版本

| 语言(版本) | 标准库形态 | 关键事实 | 对 Ctron 的启示 |
|---|---|---|---|
| **Go 1.0**(2012) | 全电池:~35 个顶级包,`net/http`+`encoding/json`+`fmt` 一步到位 | 「Go 1 兼容承诺」自 1.0 冻结全部公开 API;RE2 口径的 `regexp`、`time.Format` 的历史包袱被永久锁死 | std 的**有用性**来自 json/fs/fmt 三个域;**债务**来自过早的兼容承诺。Ctron:v1 前一律 experimental,冻结只发生在 API 经过两轮真实消费之后 |
| **Rust 0.9→1.0**(2015) | 1.0 前大清洗:删 green runtime(RFC 0230),`rand`/`uuid`/`url` 移出 std;std 收缩为「语言配套」 | 教训是**先减后冻**:不稳定的域宁可移出 std 交给生态;`std::net` 1.0 时甚至未稳定 | Ctron 无生态,vendored 模式下「移出 std」= 移出 `std/` 清单。net/async 这类域**明确不进** std 近期清单,进「一等第三方模块」层 |
| **Zig 0.10–0.15** | std 大而全(fs/json/http/crypto),但纪律统一:**显式 allocator 参数**贯穿全部容器;std 同时是编译器自身的代码库 | 自举是 std 质量的第一驱动:编译器要解析 json → `std.json` 必须真;显式资源参数让 std 天然分层(bare 可用子集) | Ctron 已有同构机制:own/Arena 显式分配(§6.3)+ alloc effect;且 `json.ct` 已被配置语言(CTCL)预定为自举消费方。**把"自举第一用户"作为域优先级判据** |
| **Odin**(当前) | `core:`(小而完备,纯自含)+ `vendor:`(C 库绑定分离) | core 每模块自含、互不依赖;绑定永远不混入 core | 与 std/ 组织宪章第 2 条(模块独立不互 use)同构;**F 案维持**,另立 `vendor/` 形态承载 FFI 包装(CBox 等-bindgen 产物),不污染 std |
| **V vlib**(当前) | 自举全量吃 vlib,域很广(net/web/orm) | 域铺得快但 API 反复破坏,文档与实现漂移 | 反面教材:**域的扩张速度必须慢于语言能力的落地速度**;每个新模块必须过"test 块独立跑绿 + 消费面 + 三线一致"门禁(宪章已有,坚持) |
| **Lua 5.1–5.4** | 最小核心 8 模块(base/string/table/math/io/os/coroutine/debug) | 嵌入场景的"最小完备面";string 内建模式匹配是唯一"越界"域 | 验证"小核心 + 确定性"路线可行;Ctron 的 core/bare 层目标面可以此为上界参照 |
| **Swift 5** | 语言核心极小;平台库(Foundation)单独分发 | 「语言 std」与「平台库」分离,版本节奏解耦 | Ctron 的 `core < alloc < std` 分层即此思路;**stdweb 单独节奏**(§9.2)与此一致,方案内保持独立车道 |

六条提炼准则(方案的全部裁决都由它们推出):

1. **自举第一用户优先**:编译器/工具链要消费的能力最先做真(json → 配置语言 CTCL;
   path → 工具;test 块 → std 自身)。
2. **域分层对齐档位**:每模块标注所属层(core/alloc/std/stdweb)与可用档位;
   bare 可用子集 = 纯计算域,永不隐式扩大。
3. **错误模型先行,能力注入跟进**:解析类一律 `Option/Result`(禁 panic);
   IO/时钟/环境一律走 `&Cap` 能力 trait 注入(测试换 fake,roadmap 已有
   fs_fake/env/time 三锚)。
4. **确定性是公共契约**:排序稳定、哈希算法钉死可复现、PRNG 可播种、
   遍历序文档化(宪章第 8 条的域级展开)。
5. **测试即合同**:模块 test 块(宪章 6)+ stdpkg 消费面 + 负例/边界
   (空输入、单元素、越界口径)三件套,缺一不收。
6. **先 experimental 后 stable**:冻结的准入条件是"两个真实消费方 +
   一次完整更名预演",不是时间表。

---

## 3. 内容域划分(D0–D7)

物理形态维持 `std/` 单层平铺(宪章 1,vendored 简单);**分域是文档与节奏概念**,
README 清单按域分组。每模块头标注:层(`core/alloc/std/stdweb`)、档位、稳定性。

### D0 数值与位域(层:core;档位:全;稳定性:experimental)

| 模块 | 具体能力 | 语言依赖 | 备注 |
|---|---|---|---|
| `bit.ct`(新) | `and/or/xor/not`、`shl/shr`、`rotl/rotr`、`popcount`、`clz/ctz`、`byteswap16/32`、`reverse_bits`;全族 I32/I64 双宽 | **无**(纯算术可写) | §4.5 挂账模块,名已钉;发射面若走内建直映可后补优化,先纯实现 |
| `math.ct`(新) | 常量 `PI/E`;`floor/ceil/round/trunc`(F64→F64);`sqrt/pow`(纯实现或内建 `math_*` 直映 libm);`clamp/lerp/rem_euclid`(整数+F64);`mean/median`(slice) | sqrt/pow 建议登记内建(发射直映 libm,解释侧宿主函数,同 `utf8_enc` 先例);纯实现作为 v0 兜底 | 前奏已有 `abs/min/max`;模块只补前奏没有的 |
| `rand.ct`(新) | **可播种 PRNG**:`SplitMix64`/`xoshiro128**`(算法钉死进文档头);`next_u64/next_f64/next_range(lo,hi)`;`shuffle(seed, List[T])`(消费 sort 纪律) | 无 | 宪章 8(不依赖真实时钟)⇒ std 的 rand 恒为种子型;**真实熵源不在此域**(归 D5,`process`/内建) |
| `hash.ct`(新) | `fnv1a_64 / djb2 / crc32`(Str→U64/I32);算法与种子口径写进文档头;迭代序无关 | 无 | `fmap.ct` 已用 djb2 mod 质数——fmap 保持自带实现(宪章 2 独立性),hash.ct 负责把**口径**文档化,两处算法必须一致(测试锚断言同输入同输出) |
| `strconv.ct`(新) | `parse_i64(s) -> Option[I64]`、`parse_f64(s) -> Option[F64]`(ABNF 口径 + 越界 None);`format_i64(i, base, width, pad)`(2/8/10/16 进制);`to_text` 族 | §3.1.1 v0 口径(I64=规范十进制文本)——parse 即文本校验+规范化;**P1-B 定宽存储落地后复验** | json.ct 已内置数字 ABNF 解析;strconv 把口径抽为公共语义(实现可各自内联,维持独立性) |

### D1 文本域(层:core;档位:全)

| 模块 | 具体能力 | 语言依赖 | 备注 |
|---|---|---|---|
| `str.ct`(已有,扩张) | 补:`index_of(sub) -> I32`(不存在 -1)、`index_of_from`、`split_str(sep: Str)`(多字节分隔)、`repeat(n)`、`is_ascii_digit/is_ascii_alpha/is_ascii_space`、`count_sub`、`strip_prefix/strip_suffix -> Option[Str]`、`eq_ignore_ascii_case` | 无 | 保持字节级纪律(宪章 7);新增 API 全部带空输入/越界测试锚 |
| `unicode.ct`(新,**薄**) | `char_len` 已是前奏;补:`decode_cp_at(i) -> I32`(码点解码,非法返回 -1)、`is_valid_utf8`、`iter_cps -> List[I32]`、`to_utf8(cp) -> Str`(转发内建 `utf8_enc`) | 无(`utf8_enc` 内建已有) | **显式非目标**:字素分割/大小写映射/宽度表——需要 Unicode 数据表,等真实需求(P2+)再说;json.ct 的字母数字 `\u` 表口径登记在案可复用 |
| `fmt` 扩展(前奏,非 std) | 插值已钉(§4.11);对齐/精度/进制**不做**进 fmt——由 `strconv.format_*` 承担,fmt 只做拼接 | — | 避免前奏膨胀;Go 教训:格式化动词一旦发布即是永久 API |

### D2 集合与迭代(层:alloc/std;档位:full/web 为主)

| 模块 | 具体能力 | 语言依赖 | 备注 |
|---|---|---|---|
| `iter.ct`(新,声名已由 r3b 钉) | `Iterator[T]` trait(for 集成);适配器 `map/filter/take/skip/take_while/zip/enumerate/chain`(惰性);终结子 `sum/count/any/all/fold/reduce/collect/last/position`;slice 与 List 的默认 Iterator 实现 | trait 方法/UFCS/闭包(r3b 正在落地);单态化零成本(§3.9) | **这是 D2 的枢纽**;parallel.map 的入参契约(§7.7)挂在同一名下 |
| `sort.ct`(已有,扩张) | 补:`sort_by(cmp: fn(T,T) -> I32)`(比较器注入,稳定保证不变)、`binary_search(xs, x) -> I32`(不存在 -1 或插入点双形态,钉一种)、`is_sorted`、`argsort(keys) -> List[I32]` | 闭包/函数类型已有 | 插入排序 O(n²) 保留为 v0 口径(文档头已声明);n>~2k 的场景等发射面优化,不改 API |
| `map.ct`/`set.ct`/`fmap.ct`(已有,小扩) | map/set 补:`del`、`len`、`each/each_i`(遍历);fmap 补:`del`、`len`、迭代序文档化(桶序) | 无 | 函数式不可变纪律不变;**不做**可变哈希表——等 alloc effect 发射面成熟(P1 后)再评估 `class` 形态容器 |
| `opt.ct`(新,薄) | Option/Result 组合子:`map2/and_then/or_else/transpose/to_result`(前奏已有 map/or/expect,补齐链式缺口) | 泛型已有 | 命名与 `or`/`?` 语义对齐(§4.4/§5.3),禁同义别名(运算符宪法精神) |
| `heap.ct`(新) | 二叉堆(比较器注入):`push/pop/peek -> Option`、`from_list/sorted -> List`;比较器版本泛型 `Heap[T]` | 泛型 + 闭包 | 自含实现(不 use sort);优先队列是 bare 也可用的纯结构 |
| `deque.ct`(缓) | 环形缓冲 deque:`push_front/push_back/pop_front/pop_back` | — | v0.4 再做;List 双端 O(n) 够用前不抢跑 |

### D3 编码与数据交换(层:std;档位:full/web)

| 模块 | 具体能力 | 语言依赖 | 备注 |
|---|---|---|---|
| `json.ct`(已有,收口) | 已有:解析(路径展平 DOM)+序列化转义+ABNF 严格化。收口项:写路径 `write_json(dom) -> Option[Str]`(现仅转义?)、深度/宽度边界文档化、与 config.ct 的测试互引 | 无 | 自举第一用户(P2-B CTCL 清单不直接用它,但工具链 json 面用它) |
| `enc.ct`(新) | `hex_encode/hex_decode -> Option[Str]`、`base64_encode/decode -> Option[Str]`(RFC 4648 标准 alphabet,`-` padding 口径钉死)、`percent_encode/decode -> Option[Str]`(URL 组件口径) | 无 | 输入输出均为 Str 文本形态;**二进制文件读写不在此域**(D5 依赖 U8[] 域,缓) |
| `csv.ct`(新) | RFC 4180 子集:解析(引号/转义/CRLF)、写(自动引号)、首行 header 开关;`List[List[Str]]` 形态 | 无 | 纯文本域,当前方言即可;确定性行序 |
| `config.ct`(新,**取代原计划的 `toml.ct`**) | CTCL(`Ctron.ctcl`)解析/规范渲染/访问 API;文法与三线解析器契约一致,诊断 E504x 三线同文 | CTCL 提案(2026-09-16 config-language-v1 §9 支持路线 L1/L2) | 自举第一用户(编译器清单检查与 stdlib 同源);ABNF 与 json.ct 同规格纪律(登记进 §10 语义面);原 `toml.ct`(TOML 子集)方案作废 |

### D4 时间域(层:std;档位:full/web;bare 限纯函数部分)

| 模块 | 具体能力 | 语言依赖 | 备注 |
|---|---|---|---|
| `time.ct`(新,名已由 r2b 钉) | ① `Clock`(`real()` 构造、`now`、`elapsed_since`——r2b_time 语义承诺);② **纯历法层**:`civil_from_days / days_from_civil`(Howard Hinnant 算法,纯可测)、`format_iso8601(epoch_ms) -> Str`、`parse_iso8601 -> Option[I64]`、`days_in_month/is_leap` | ① 依赖内建 `now_ms`(已有);② 无 | 模块内**唯一 IO 面 = Clock**(与 fs.ct 同入 IO 例外清单);测试块只用纯历法层与注入形状,不依赖真实时间(宪章 8) |

### D5 系统域(层:std;档位:full/web)

| 模块 | 具体能力 | 语言依赖 | 备注 |
|---|---|---|---|
| `path.ct`(新) | **纯函数**:`join(a,b)`、`dir/base/name`、`ext`、`is_abs`、`normalize`(消 `./..`)、`split_ext`;Unix 口径 v0(`/`),Windows 差异登记不实现 | 无 | 自举第二用户(工具链遍历源码树);纯函数全部可测,是性价比最高的新模块 |
| `fs.ct`(已有,扩张) | 补:`write_or(path, content)`、`append_or`、`mkdir`、`list_dir(path) -> Option[List[DirEntry]]`(升级内建 `read_dir` 的 Str 返回→结构化)、`copy_file`、`walk(path, glob) -> List[Str]`(排序确定性遍历+通配过滤) | **内建面依赖**:`fs_mkdir/fs_copy/fs_rename` 登记为内建;`read_dir` 结构化(条目含 name/type/size) | `glob` 采用 `*?` 通配(先例:shell 语义),**regexp 明确不做**(§5 拒绝清单) |
| `process.ct`(新,名已钉) | `Env.system()/get/args`(r2b_env 承诺);补:`set_var`、`cwd`、`exit(code)`、`hostname`、真实熵源 `entropy_u64`(OS 熵,供 rand 播种,Web 档降级 None) | **内建面依赖**:`env_get/env_set/args/cwd/exit/entropy` 内建登记 | 全模块走能力形状:`Env` 是 `: Cap` trait,`system()` 为默认实现,测试用 `FakeEnv`(与 07_capabilities 注入形状一致) |
| `log.ct`(新) | `trait Log: Cap`(`debug/info/warn/error(msg, kv)`);`StdLog`(写 stderr,级别门限);`CaptureLog`(测试捕获,断言友好) | 能力 trait 体系(已有) | 依赖注入示例与 `r2b_fs_fake` 同型;日志格式一条线钉死(时间戳可选注入 Clock) |
| `bench.ct`(新) | `measure(name, iters, f: fn() -> Void) -> BenchResult(ns_per_op)`;报告文本对齐 `tools/bench` 口径 | 内建 `now_ms`;闭包 | 自举第三用户(仓库已有 tools/bench 与 selfhosted/bench 两个宿主版,收口进 std) |

### D6 并行与同步(层:std;档位:full)

| 模块 | 具体能力 | 语言依赖 | 备注 |
|---|---|---|---|
| `parallel`(前奏命名空间内,§7.7) | `map/reduce/fold`:`&T[]` 只读入参、闭包纯度推断、确定性分块模式(§10.4) | **依赖重**:结构化并发运行时(P4)、Send 全量检查、纯度推断 | 域内唯一成员;**不做** WaitGroup/Semaphore/OnceCell——scope/Mutex/static-let 已覆盖,Go 教训:同步原语面宁小勿大 |
| `iter-parallel`(缓) | `Iterator` 的并行适配器(按块) | iter.ct + parallel 都落地后 | 排 v0.5+;当前只登记意向,防 V 式抢跑 |

### D7 档位专项(独立车道,按后端节奏)

| 模块 | 具体能力 | 语言依赖 | 备注 |
|---|---|---|---|
| `stdweb.dom` 等 | §9.2 已钉最小面(set_title/title);扩充按 WasmGC/JSPI 后端(P1-D)推进:文本/属性读写 → fetch(异常转 Result)→ canvas | web 后端落地 | **单独节奏、单独门禁**;与 std/ 主线不共享演进纪律表(允许落后主线) |
| `core` 层(bare) | 容器带显式 Arena 参数(§6.6/§9.1):`core_list/core_ring/core_fmt`(定长缓冲写入,零 GC 分配) | bare 档编译(§9.3) | v1 不动 `std/` 物理形态;以 `core_*.ct` 前缀或独立目录立项时再裁决;上界参照 Lua 核心面 |

---

## 4. 分阶段路线(与语言阶段对齐)

判据(准则 1/6):**自举第一用户 > 纯方言可完成 > 语言能力解锁**;
每个模块的出口 = 宪章 Checklist(test 块绿 → README 清单+种子同步 → stdpkg 消费 → smoke 绿 → 单独提交)。

### std v0.2「纯方言轮」(现在即可开工;仅依赖当前内建面)

```
bit → strconv → hash → rand → math → path → str 扩张 → enc → time(纯历法层) → sort_by → opt → heap → csv → unicode(薄)
```

- 每模块 0.5–2 天;全部当前三线可跑(解释面为准,发射面逐字一致随门禁)。
- `math.sqrt/pow` 若走纯实现则本轮全绿;若登记内建则解释侧先行、发射侧后补,
  夹具按「能力缺口清单」口径登记。
- 收益:ctgrep/ctwc/ctwf 三个示例立即消费 path/str/enc;CTCL 清单(P2-B,按 config-language-v1 提案)
  与工具链的直接弹药。

### std v0.3「能力注入轮」(依赖 R-P2b/R-P3b 与内建登记)

```
iter(trait 正式化, r3b 锚转绿) → process.Env 全面 → time.Clock 接通 → log → fs 扩张(read_dir 结构化 + mkdir/copy) → bench → json 收口
```

- 与 spec-completion roadmap 的 P1/P2 穿插;每项「内建登记 → 解释 → 发射 → R 线」
  四步走,诊断/能力缺口进 §10 码表。

### std v0.4「发射面成熟轮」(依赖 P1-A Drop 发射、P1-B 定宽、P4 并发运行时)

```
bytes 域(U8[]/Box 落地后立项) → parallel(map/reduce/fold) → deque → iter-parallel(评估) → config(CTCL,与 P2-B 联动;取代原 toml 计划)
```

- `bytes` 立项前置裁决:Str 二进制口径(binary-safe Str vs `Bytes` 类型)——
  需要一次设计记录,涉及 `read_file` 返回型与 UTF-8 边界检查(§3.3)的豁免面。
- U64/ISize 定宽落地后,hash/strconv/time 的宽度口径全量复验(§3.1.1 撤销项)。

### std v0.5+「档位专项轮」

- stdweb 按后端(P1-D)逐版;core 层在 bare 目标(thumbv7em/riscv32)编译可用后立项。
- **regexp/net/http/async/UI/ORM:本方案明确拒绝**(v1 拒绝清单,§5)。

---

## 5. 治理纪律(对宪章的扩展,不推翻)

1. **版本化**:std 引入 `std/VERSION`(行式 `std-0.2`);模块文档头第二行
   `// since: std-0.2 stability: experimental|stable`。破坏性变更 = VERSION 升位
   + README 变更日志段 + 消费面同步。
2. **稳定性分级**:experimental(默认,v1 前一切如此)→ stable(准入:两个真实
   消费方 + 一次完整更名预演 + 三线门禁连续两轮绿)→ deprecated(先 lint
   W8xxx + 别名一版,再删)。
3. **跨模块命名消歧**:模块自带惯用前缀注册表(`fs→r*` 已裁;`path_*`、
   `json_*`、`time_/civil_`、`hex_/b64_/pct_`);str.ct 既有无前缀种子函数
   (words/contains/…)冻结为历史口径,不再新增无前缀名。
4. **错误策略**:解析类(parse/decode)一律 `Option/Result`,**禁 panic**
   (除文档化的不变量);文件类 v0 维持 Bool/Option 现状,v0.3 扩张时统一迁
   `Result[_, FsError]`(@derive(Error),一次升位完成)。
5. **确定性契约表**(README 新增一节):排序稳定、fmap 迭代=桶序、map/set
   迭代=首插序、hash 算法+种子钉死、rand 可播种、walk/list_dir 排序输出。
6. **文档头复杂度标注**(宪章 5 的强化):每个导出函数头注 O 记号
   (如 `// O(n*m)`),doc 面自动携带。
7. **三线与 vendored 纪律不变**:模块独立不互 use(小件允许 <100 行的受控
   自含重复);种子副本漂移断言、stdpkg 消费面、示例钉定快照三件套照旧。

## 6. 验收锚汇总(新增模块的测试钉)

| 域/模块 | 必备锚 |
|---|---|
| 全部新模块 | 空输入、单元素、越界/非法输入(Option/None 路径)、幂等性(适用处) |
| bit | 全函数 I32/I64 各 1 正例 + 边界(MIN/MAX、移位 ≥ 宽度=截断口径钉死) |
| strconv | ABNF 边界(±0/前导零/越宽/空/十六进制)、round-trip(format∘parse=id) |
| hash | 已知测试向量(FNV/DJB2/CRC32 各 2 向量)+ 与 fmap 口径一致性断言 |
| rand | 固定种子序列断言(前 8 个值钉死进测试) |
| time(纯) | 闰年边界(1900/2000/2100)、epoch↔civil round-trip、ISO8601 解析负例 |
| path | join/normalize 的 `..` 越根、尾 `/`、空段口径(每条一断言) |
| enc | RFC 4648 官方向量(base64 4 组)+ 非法字符 None 负例 |
| iter | r3b 既有三文件 + collect 类型钉、惰性断言(take 不消费后续) |
| csv | 引号内逗号/CRLF/嵌引号三经典 + header 开关 |
| log/bench | CaptureLog 断言序列、BenchResult 数值合理性(>0) |

## 7. 自查

- 与 `std/README.md` 组织宪章:全部条款保留,本方案只做**扩展**(分域是文档
  概念,版本化与稳定性是新章节),无冲突 ✓。
- 与 spec v0.7:`bit`(§4.5)、`parallel`(§7.7)、`stdweb.dom`(§9.2)、
  `core/alloc/std` 分层(§9.1)、能力注入(§8.1)均有规范出处;D0–D7 无一域
  需要先改语言规范 ✓。
- 与 spec-completion roadmap(2026-09-13):std v0.3/v0.4 显式挂在 P1-A/B、
  P2-B、P4 之后,内建登记走同一诊断码 SOP ✓。
- 拒绝清单有据:regexp/net 减法有 Go/Rust 两个对标案例支撑 ✓。
