# std HashMap[K,V] 设计——可变泛型哈希映射 v1（注入形）

- 日期：2026-10-09
- 泳道：std 容器族（新开）
- 状态：设计已获用户批准（A 案修宪附则 + 注入形 v1）；本文入册后转实施计划
- 上游挂账：`2026-09-15-stdlib-gap-analysis.md:112`（"最大缺口：可变泛型键哈希表"）
  × `2026-09-14-stdlib-domains-plan.md:103`（克制令"等 alloc effect 成熟"）——本次裁决解除对峙

## 1. 背景与消费面证据

std 现有容器全部函数式：`Map[K,V]` 双平行 List O(n) 线性扫、`Set[T]` List 线性去重、
`FMap[V]` Str 键专用定容 256 槽、`heap` 比较器注入但 push O(n) 重建。没有泛型键的
O(1) 映射。真实痛点（准入四问第 2 问）：

- `lib/http/client.ct:26` keep-alive 连接缓存因"发射面 struct 值语义 + 跨模块可变
  状态通道受限"退化为**四枚标量胞单槽**，键 = `hash(host)+port`；
- `examples/todo_api/src/main.ct:182` 三条平行 `List[Str]` 当记录表；
- `pkgs/web/core.ct:9` headers = `List[Pair]` 线性扫；路由表线性匹配；
- `lib/std/json.ct:376` DOM = 路径展平 `List[List[Str]]`，查询线性扫。

## 2. 裁决记录（2026-10-09 用户已批）

- **R1 范围**：只落 `HashMap[K,V]` 单件。deque/位集/BTreeMap/MultiMap 一律触发件制
  （消费面真出现再立项）。
- **R2 形态 = A 案**：宪章 3 开附则——可变容器以**引用语义载体（Box 持堆上结构）**
  入 std，与函数式族并存，必须申报 `//@ tier: alloc`。B 案（HAMT 持久化哈希，
  宪章零改动）否决，留作函数式族未来升级方向。
- **R3 语言前置 = 注入形 v1**：哈希/相等以**逐调用 fn 形参**注入（`sort.ct` 比较器
  先例，发射链已证绿）。不做 `Hash` trait——发射面 trait 方法调用在册未落地
  （spec-gap 计划 t_bump/t_hi 账），v1 不被编译器泳道绑架。二代触发件 = Hash trait
  落库 + trait 方法发射 + SipHash 密钥化（scope-boundary 已规划）→ bound 面
  `K: Hash + Eq`。
- **R4 并存**：`Map`/`Set`/`FMap` 保留不下架（gap-analysis 原文立场升格为基线）。

## 3. 载体与探针门（实施第一步）

主形态 `Box[HMap[K,V]]`：`Box` 写穿别名共享（`fx_boxalias` 在 smoke 发射循环绿；
`pkgs/gui/gui_driver.ct:465` 全 var 字段 `Box[Driver]` 大面先例），`List.push` 就地
可变且 `List[struct]` 发射底座绿（`fx_list_struct`）。

- **探针 P1**（先于 hashmap 本体）：Box + 泛型 struct + `var List[K]`/`var List[V]`
  字段，经 Box 形参做 push 与下标写，第二别名读见；种子 + 发射双臂输出逐字一致。
- **探针 P2**：`Box[HMap[Str, V]]` 局部特化作形参（决定 v1 是否配 Str 键糖面）。
- **回退**：P1 红 → 函数式形态（`hm_put` 返回新映射；O(1) get / O(n) put；FMap 泛型
  化，宪章 3 零改动，仅 R2 改记"暂缓"）。

硬约束（全部发射面在册事实）：

1. **fn 值不入 struct 字段**（T16 在册红，iter 先例）→ 注入只走形参，sort 先例；
2. **无闭包字面量**（C13），比较器/哈希一律具名 fn 值；
3. **无比特算符**（&/|/^ 解释面缺口），哈希用乘加 + mod 等价；
4. **哈希中间值全程 < 2^31**（C10 eval 乘法溢出门；fmap `mod 100003` 先例）；
5. **负值归一** `((x % m) + m) % m`（fmap:34 先例）；
6. 容器操作用**顶层泛型自由函数**，不用 impl 方法/UFCS 作主验证面（v0.2 API 规范
   C4 契约）；
7. T1 核心互不 use（宪章 2）→ 自带哈希与比较器，不引 std.hash/std.str。

## 4. API 面（v1）

命名 `hm_*` 前缀 + 结构 `HMap[K,V]`：`use` 合并单一命名空间，同名 decl 静默首胜，
须与 `map`/`set`/`heap` 的 put/get/has 错开（FMap `f*` 先例）。

| 符号 | 签名形 | 语义 |
|---|---|---|
| `hm_new[K,V]` | `() -> Box[HMap[K,V]]` | 空映射，初容 16 |
| `hm_put[K,V]` | `(m, k: K, v: V, h: fn(K)->I64, eq: fn(K,K)->Bool)` | 就地写；存在则覆盖 |
| `hm_get[K,V]` | `(m, k, dft: V, h, eq) -> V` | 缺键返回 dft（FMap fget 先例，避 Option 泛型面） |
| `hm_has[K,V]` | `(m, k, h, eq) -> Bool` | |
| `hm_del[K,V]` | `(m, k, h, eq)` | 活词表重建（fdel 先例；v1 无墓碑，O(n) 文档化） |
| `hm_len[K,V]` | `(m) -> I32` | = keys.len |
| `hm_keys[K,V]` | `(m) -> List[K]` | 插入序拷贝（确定性，宪章 8） |
| `hm_vals[K,V]` | `(m) -> List[V]` | 与 keys 一一对应 |
| `hm_hash_str` / `hm_eq_str` | `(Str)->I64` / `(Str,Str)->Bool` | djb2 mod 100003 拷贝自 fmap（字节级双面一致） |
| `hm_hash_i64` / `hm_eq_i64` | 同形 | 中间值 < 2^31 + 负值归一 |

> **实施批修订（2026-10-09，R3 二次修订）**：①哈希签名 I64→I32（回避 I64/I32
> 混宽缝隙，fmap 同域先例）；②**注入形裁撤，改具体键双件 `HMapS[V]`（Str 键）+
> `HMapI[V]`（I64 键），哈希/相等内联**——探针实证：具体型 fn 值传入泛型 fn 型
> 形参的发射 trampoline 按 ct_i 裸传（heap Darwin 4c known-red「比较器形参」同款
> 在册债），注入形在 Str 键上发射臂必红；具体键双件零 fn 值参数、API 去注入样板。
> 泛型键 `K: Hash + Eq` bound 面维持 W3 触发件不变。同批探针钉边登记：泛型宿主
> fn 内对助手传具体型显式 TypeArgs 在 seed 面结果型不绑定（E2010；发射臂同形
> 绿，seed/emit 宽严反向分歧）；经 Box 泛型元素值位读/写发射无装载语义（B1–B4
> 探针，重建+字段重赋值绕行）。

## 5. 引擎

开放寻址线性探查。`slots: List[I32]`（len = 容量，-1 空，值 = keys/vals 下标）+
`keys: List[K]`、`vals: List[V]` 追加式平行 List（**规避泛型 V 零值构造**，FMap
先例）。初容 16，负载 > 3/4 倍增重建 slots（键重探查）；探查界 = 容量；删除不缩容
（文档化）。迭代序 = 插入序（keys 追加式）。

## 6. 宪章修订（随批入 README.md）

宪章 3 追加附则：

> 3a. **可变容器附则**（2026-10-09）：可变容器以引用语义载体（Box 持堆上结构）入
> std，与函数式族并存；必须申报 `//@ tier: alloc`；API 用顶层泛型自由函数 + 显式
> 注入（比较器/哈希 sort.ct 先例），fn 值不入 struct 字段；本条"不可变更新"继续
> 约束函数式族（map/set/heap/FMap）。

模块清单表加行：T1 `hashmap.ct`（可变泛型哈希映射：开放寻址 + 注入形哈希/相等；
Box 载体，O(1) 均摊，since 0.9）。

## 7. 门禁清单（演进纪律 + 红点清单）

1. `./ctron test lib/std/hashmap.ct` 模块独立跑绿；
2. 字节级副本同步 `compiler/test/stdpkg/std/hashmap.ct`（smoke 漂移门必红项）；
3. 消费面用例进 `compiler/test/stdpkg/src/main.ct`；
4. `sh compiler/test/smoke.sh --full` 全绿（3j2 自举/3j3 Rust/4c 发射 parity 自动
   扫到新模块；Rust 臂或 4c 不绿则按 heap Darwin 先例登记名单随批申报，目标绿）；
5. `python3 tools/std_doc.py` 生成 `website/docs/std/hashmap.md` + `--check` 绿 +
   mkdocs nav 加行（hash 与 heap 之间）；
6. README 宪章 3a + 模块表行；
7. `tests/COVERAGE.md` 顶部批注块记账；
8. decls 锁不动（零 compiler/src 改动）；若被迫动，`sh compiler/build.sh` 后
   `./compiler/ctc.sh check compiler/build/cc_run.ct` 实测回写；
9. 机刷纪律：全程 pathspec 限定提交、不重建编译器驱动、不 stash/add -A。

## 8. 非目标（v1 明确不做）

有序容器变体（std-scope 在册"不做"）、宏派生（语言无宏）、墓碑删除、缩容、
SipHash、迭代器协议统合（for-over-Iterator 发射是独立在册账）、动 `List` 本身、
Hash trait（W3 触发件）。

## 9. 路线

| 波次 | 内容 | 验收 |
|---|---|---|
| W1（本 spec） | 探针 P1/P2 + `std/hashmap.ct` + 宪章 3a + 全部门禁 | §7 清单 |
| W2（已落地 60ffdc6b + a453f30a，2026-10-09） | http client keep-alive 多槽（HttpKeep 自持 KeepMap 引擎）+ todo_api hmi 双 Map 换装；**web headers 裁决不做**（HTTP 头有序可重名，`List[Pair]` 语义正确） | c0_pure 双臂绿；bru 递进断言逐字保持；**todo_api e2e 22/0 已兑现**（U8 迁移批 c93ec0e4 后复跑） |
| W3（触发件） | Hash trait + 发射面 trait 方法调用 → 二代 `K: Hash + Eq` bound 面 + SipHash | COVERAGE 记账 |
| 触发件 | deque / 位集 / BTreeMap；http 缓存换装 `Box[HMapS[I64]]`（候 trans 线补结构体 Box 字段跨模块 typedef 闭包 + typedef 依赖序） | 各自消费面/前置出现时 |

### W2 实施批新增登记（发射面形状债，详见 COVERAGE 2026-10-09 W2 批）

1. 结构体 Box 字段**跨模块**类型闭包缺失（t_HMapS 悬空；同模块免疫探针实证）；
2. 发射 typedef 按**声明序**非依赖序（序错悬空）；
3. 裸 V 返回于非泛型宿主值位 E2010 + V 不参与实参位推断（E2060）——泛型容器
   值面给非泛型宿主须配具体型面；seed/emit 宽严反向第三例；
4. V=struct 经 Box push 强转指针（Todo 值 Map 不可行 → 双 Map 形）。
