# std README 宪章 v2 修订 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 按已核准 spec(docs/superpowers/specs/2026-09-23-std-tiering-design.md)§6 清单,把 std/README.md 从宪章 v1 升到 v2:三档分层成文、域包入册、准入四问、拆分触发。

**Architecture:** 纯文档修订,唯一改动文件 std/README.md。编辑顺序:清单表(加档列+补欠账行+域包注册表)→ 新增「分层与准入」章 → 组织宪章 #1/#2/#8 修订(引用新章)→ 分发与同步加 vendored 纪律 → 演进纪律加准入门。单提交落库(pathspec 限定)。

**Tech Stack:** Markdown;无代码改动,无 TDD 对象。验收 = 锚点 grep + git diff 范围断言。

## Global Constraints

- 唯一允许改动文件:`std/README.md`。禁止碰任何 `.ct`、编译器、smoke.sh。
- 档名逐字用 spec 口径:`T1 核心` / `T2 系统面` / `T3 平台面`;判定线 = **平台服务依赖**;试金石 = **headless 能否构建+跑绿**。
- since/口径列只写有据锚(包头注、09-21 spec、§11 冻结 v0.8.1),禁止编造版本号。
- 提交 pathspec 限定 `std/README.md`(工作区有他泳道在制品,见机刷泳道纪律)。
- README 不在 smoke 漂移断言面(该断言只 glob `std/*.ct`),无需同步种副。
- 术语与 spec 一字不差:准入四问 = 依赖面/消费面/三宿主税/泳道所有权;流用规则五条;拆分触发三条(CTCL 注册表/API freeze/平台矩阵)。

---

### Task 1: 模块清单表加"档"列 + 补欠账行 + 域包注册表

**Files:**
- Modify: `std/README.md`(「## 模块清单」节,现表 19 行)

**Interfaces:**
- Consumes: 现 19 行表原文(逐字保留「内容」列文本);包头注锚(http client=P4-C、tls bind=P3-C、db=P5-C、gui=SL 桥 L1、net=§11 冻结 v0.8.1)。
- Produces: 21 行 T1 表(uuid/json_write 补行)+ 新「域包(T2/T3)」表;Task 2/3 引用"域包"概念以此表为准。

- [ ] **Step 1: 现表头与 19 行替换为带档列的 21 行表**

将现表(表头 `| 模块 | 内容 | 依赖 | since |` 起,至 `math.ct` 行止)整体替换为:

```markdown
| 档 | 模块 | 内容 | 依赖 | since |
|---|---|---|---|---|
| T1 | `str.ct` | 字符串工具:words/contains/lines + v0.2 扩张(index_of/split_str/strip_*/count_sub 等) | 无 | 0.1 |
| T1 | `sort.ct` | 排序:sorted/sorted_desc/reversed + v0.2 扩张(sort_by/binary_search_by,比较器注入) | 无 | 0.1 |
| T1 | `map.ct` | Map[K, V]:函数式不可变映射(双 List 平行槽) | 无 | 0.1 |
| T1 | `set.ct` | Set[T]:函数式不可变集合 | 无 | 0.1 |
| T1 | `fmap.ct` | FMap[V]:Str 键哈希映射(djb2 mod 质数,开放寻址) | 无 | 0.1 |
| T1 | `fs.ct` | 文件系统便利层:read_or/exists 等(内建 fs_* 之上;r* 前缀) | 无(编译器内建) | 0.1 |
| T1 | `json.ct` | JSON 解析(路径展平 DOM)+序列化转义(RFC 8259 ABNF 严格) | 无 | 0.1 |
| T1 | `path.ct` | 路径纯函数:join/dir/base/ext/normalize(Unix `/` 口径) | 无 | 0.2 |
| T1 | `enc.ct` | 编码:hex/base64(RFC 4648)/percent;解码输出限可打印 ASCII(C8) | 无 | 0.2 |
| T1 | `hash.ct` | 确定性 32 位哈希:djb2/fnv1a32(算法钉死;crc32 待 v0.3) | 无 | 0.2 |
| T1 | `rand.ct` | 确定性 PRNG:MINSTD/Park–Miller(Schrage;I64 承载,C10) | 无 | 0.2 |
| T1 | `strconv.ct` | parse_i64/parse_bool(format_hex/bin 待 P1-B,C10 死区) | 无 | 0.2 |
| T1 | `time.ct` | 纯历法:days 域 civil↔date/weekday(Hinnant 算法;Clock 待 v0.3) | 无(编译器内建) | 0.2 |
| T1 | `csv.ct` | CSV(RFC 4180 子集;引号转义/CRLF;write∘parse=id) | 无 | 0.2 |
| T1 | `unicode.ct` | UTF-8 解码面:cp_at/valid/iter/count(字素/宽度表显式非目标) | 无 | 0.2 |
| T1 | `opt.ct` | Option/Result 组合子:and_then/or_else/map2/to_result | 无 | 0.2 |
| T1 | `heap.ct` | 二叉堆:比较器注入小顶堆(push/pop/peek/sorted;函数式) | 无 | 0.2 |
| T1 | `crypto.ct` | 密码学原语(纯 Ctron):SHA-256(FIPS 180-4 向量锚;HMAC/PBKDF2 待续,§12 前置) | 无 | 0.5 |
| T1 | `math.ct` | 数学助手(整数域):clamp/iabs/pow2 系 + v0.4/v0.5 扩张(max/min/clamp64/64 位界端 + sat_* 全集/sat_abs/abs_diff/div_ceil/div_floor) | 无 | 0.3 |
| T1 | `uuid.ct` | RFC 9562 UUID(v4 真熵 / v7 unix_ms 前缀;§12 纯 Ctron 驱动前置) | 无 | 0.8 |
| T1 | `json_write.ct` | JSON 写出与成员枚举(与 json.ct 配对的写出半边) | 无 | —(并入 json 待裁决,见「分层与准入」) |
```

- [ ] **Step 2: 表后追加域包注册表**

紧接上表空一行后追加:

```markdown
### 域包(T2/T3,2026-09-23 入册)

域包随所属泳道演进,不参与 std since 升版;消费形态同 `use std.<域>…`。

| 档 | 域 | 内容 | 依赖 | 稳定口径 | 门禁 | 所有 |
|---|---|---|---|---|---|---|
| T2 | `net` | 服务器档传输门面(§11)+ bind extern 面 + rt 协程运行时 | libc/socket + 包内 `c_src/` | 冻结 v0.8.1(§11) | 域包消费测试,headless 绿为准入条件 | 服务器泳道 |
| T2 | `http` | HTTP/1.1 严格子集:parse/message/client/sse/ws/form + binddeflate | `vendor/deflate` | P4-C 面 | 同上 | 服务器泳道 |
| T2 | `tls` | TLS 门面(bind) | `vendor/tls/mbedtls` | P3-C 面 | 同上 | 服务器泳道 |
| T2 | `db` | pg/redis 线协议 + pool + rowmap(§12.1) | libc/socket + 包内 `c_src/` | P5-C 面(§12 草案) | 同上 | 服务器线 P5 |
| T3 | `gui` | clay+raylib 窄桥 + ctml 解析 + 域运行时 | `vendor/gui`(clay/raylib)+ 窗口系统(X11/GL) | SL 桥过渡(L1) | 永不进 std 通用门禁;泳道 headless 命令缓冲 | gui 泳道 |

档的判定线 = **平台服务依赖**:T2 与 T3 的分界不是"有无大型 C 库",而是
是否拖窗口/GPU 等平台服务;试金石 = headless 环境能否构建+跑绿验收。
详见下文「分层与准入」章。
```

- [ ] **Step 3: 锚点验证**

Run: `grep -c "^| T1 |" std/README.md && grep -c "^| T2 |" std/README.md && grep -c "^| T3 |" std/README.md`
Expected: `21`、`4`、`1`

### Task 2: 新增「分层与准入」章

**Files:**
- Modify: `std/README.md`(「## 组织宪章」节之后、「## 分发与同步」节之前插入)

**Interfaces:**
- Consumes: spec §2 模型表、§3 四问与流用规则、§4 场景表、§5 触发条件。
- Produces: 「分层与准入」章;Task 3 宪章 #1/#2 修订以"详见「分层与准入」"引用本章。

- [ ] **Step 1: 插入整章**

在 `## 分发与同步` 标题前插入:

```markdown
## 分层与准入(宪章 v2,2026-09-23)

**三档模型**(判定线 = 平台服务依赖):

| | T1 核心 | T2 系统面 | T3 平台面 |
|---|---|---|---|
| 定义 | 纯 Ctron,零 C 源(编译器内建视同无依赖) | libc/socket + 自带/vendored 纯用户态 C | 依赖窗口/GPU 等平台服务 |
| use 权 | 不 use 任何域包 | use T1;域内 use;跨域须登记 | use T1(T2 须登记) |
| 宪章适用 | 全项 | #3/#8 有界(显式 IO/状态门面即包用途) | 同 T2 |
| 门禁 | stdpkg 全量消费 + parity 矩阵 + 种子逐字节同步 | 域包自带消费测试,headless 绿 | 永不进 std 通用门禁,泳道自有验收 |
| vendored C | 禁止 | `vendor/<域>/`,包内 `c_src/` 只放自写胶水 | 同左 |

**准入四问**(新包全过才准入;任何一问不过 → 09-21 spec §3.3 候补层,或按
CTCL 注册表包形态外置):

1. **依赖面**:依赖 ≤ 所申报档上限?零 C 源→T1;纯用户态 C→T2;平台服务→T3
   (T3 默认劝退:std 内 T3 只应是历史例外,新 T3 直接走注册表包形态)。
2. **消费面**:有无真实需求源(泳道/examples/规范承诺)?"Go 有所以要有"不成立。
3. **三宿主税**:T1 新面三线(ctron test/自举解释/发射 C)可验?T2 的 C 面
   逐宿主一致或显式登记受限口径(net.ct 头注先例)?付不起→候补层。
4. **泳道所有权**:归哪个泳道所有(db=服务器线 P5 先例)?无主包不入。

**场景处理**:T1 候补转正走下文演进纪律原文流程;T2 新系统面(url/SCRAM 等)
随需求泳道落地;**T3 新平台面默认不进 std**(gui 是历史例外,不扩员);
档次迁移只许 T3→注册表,不许 T1→T2(核心面拖 C 即破坏 vendored 闭集)。

**拆分触发条件**(何时把包请出 std):①CTCL 包管理器/注册表落地 → T3 全部
迁出,gui 第一个;②API freeze(1.0)→ T1 冻结,T2 只冻结已稳面(§11 v0.8.1
先例),T3 明示不在范围;③平台矩阵展开(Windows/无显示 CI)→ T3 构建税自负。

**悬案**:json_write 并入 json(09-21 spec §8.3.1)维持待裁决;域目录形态
已随宪章 v2 转正(组织宪章 #1)。
```

- [ ] **Step 2: 锚点验证**

Run: `grep -c "准入四问\|拆分触发条件\|分层与准入" std/README.md`
Expected: `>= 4`

### Task 3: 组织宪章 #1/#2/#8 修订 + vendored 纪律 + 演进纪律准入门

**Files:**
- Modify: `std/README.md`(「## 组织宪章」3 条、「## 分发与同步」加 1 条、「## 演进纪律」首条改写)

**Interfaces:**
- Consumes: Task 2 的「分层与准入」章(引用关系:#1/#2 指向它)。
- Produces: 宪章 v2 定稿文本;无后续任务。

- [ ] **Step 1: 宪章 #1 替换**

旧:`1. **一文件一模块**:文件名 = 模块名(小写单词,无连字符);\`use std.<文件名>.{...}\` 消费。`
新:

```markdown
1. **一文件一模块**:文件名 = 模块名(小写单词,无连字符);`use std.<文件名>.{...}` 消费。
   大域演化为**一域一目录**(net/http/tls/db/gui;目录名 = 域名,`use std.<域>.<子模块>.{...}`;2026-09-23 转正)。
```

- [ ] **Step 2: 宪章 #2 替换**

旧:`2. **模块独立**:std 模块之间不相互 \`use\`——任何子集可单独 vendored,不缺依赖。`
新:

```markdown
2. **模块独立(分档流用)**:T1 核心互不 `use`,任何子集可单独 vendored;域包 use 只许下行(T3→T2→T1);域目录内 use 允许、禁环(菱形规避依加载器 ⑥ 口径);跨域 use 须包头注登记消费闭集(http/client、db/pg 先例);T1 禁止携带 C 源。详见「分层与准入」。
```

- [ ] **Step 3: 宪章 #8 尾加界注**

旧:`8. **无副作用**:std 不做 IO(除 \`fs.ct\`/\`time.ct\` 的 Clock 面)、不依赖真实时钟;排序稳定、哈希确定性。`
新:

```markdown
8. **无副作用**:std 不做 IO(除 `fs.ct`/`time.ct` 的 Clock 面)、不依赖真实时钟;排序稳定、哈希确定性。T2/T3 域包以显式 IO/平台门面为包用途,本条约束其纯函数面;T1 全项适用。
```

- [ ] **Step 4: 分发与同步节追加 vendored 纪律**

在「- `config.ct`(2026-09-17)…」条目后追加:

```markdown
- **vendored C 分工**(2026-09-23):第三方 C 库统一落仓库根 `vendor/<域>/`(gui/clay、gui/raylib、tls/mbedtls、deflate);域包包内 `c_src/` 只放自写胶水(`ctron_*.c`)。二者不得混放;T1 禁止携带任何 C 源。
```

- [ ] **Step 5: 演进纪律首条加准入门**

旧:`- 新模块:按组织宪章写 → 模块 test 块独立跑绿 → 加入本清单与种子同步 →`
新:

```markdown
- 新模块:先过「分层与准入」准入四问定档 → 按组织宪章写 → 模块 test 块独立跑绿 → 加入本清单与种子同步 →
```

- [ ] **Step 6: 锚点验证**

Run: `grep -c "一域一目录\|分档流用\|vendored C 分工\|准入四问定档" std/README.md`
Expected: `4`

### Task 4: 全文验收 + 落库

**Files:**
- Verify & commit: `std/README.md`

**Interfaces:**
- Consumes: Task 1–3 全部落点。
- Produces: 单提交 `docs(std): 宪章 v2——三档分层+域包入册+准入四问+拆分触发`。

- [ ] **Step 1: 范围断言——工作区只多 README 一处改动**

Run: `git diff --stat -- std/README.md && git status --porcelain -- std/`
Expected: 仅 `std/README.md` 一行 M;无其他 std/ 下未跟踪新文件

- [ ] **Step 2: 门禁免疫确认**

Run: `git diff --name-only | grep -c "\.ct$"` 
Expected: `0`(无 .ct 改动 → 漂移断言/parity 矩阵不受影响;若本机已构建 `compiler/bin`,可加跑 `sh compiler/test/smoke.sh 2>&1 | tail -3` 确认全 ok,未构建则跳过并在提交信息注明"README 不在门禁面")

- [ ] **Step 3: 交叉引用完整性**

Run: `grep -c "详见「分层与准入」\|见「分层与准入」" std/README.md`
Expected: `2`(宪章 #2 与模块清单域包表尾各一处)

- [ ] **Step 4: 单提交落库(pathspec 限定)**

```bash
git add std/README.md
git commit -m "docs(std): 宪章 v2——三档分层+域包入册+准入四问+拆分触发" -- std/README.md
```

- [ ] **Step 5: 回写 spec 状态行**

`docs/superpowers/specs/2026-09-23-std-tiering-design.md` 头部状态行改为:
`> 状态:**已落地(2026-09-23;README 宪章 v2 随 <commit-sha> 落库)**`
与本计划一并以 pathspec 限定提交:
`git commit -m "docs(std): 分层 spec 状态回写——已落地" -- docs/superpowers/specs/2026-09-23-std-tiering-design.md docs/superpowers/plans/2026-09-23-std-tiering-readme.md`
