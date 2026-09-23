# Ctron 标准库(std)

标准库是 **Ctron 源码形态分发**的模块集合:程序经 `use std.<模块>.{...}` 消费,
分发方式为将所需模块源码随程序携带(vendored)。本目录是标准库的**规范源**——
任何修改先改这里,再同步到消费方副本。

## 模块清单

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

## 组织宪章

1. **一文件一模块**:文件名 = 模块名(小写单词,无连字符);`use std.<文件名>.{...}` 消费。
   大域演化为**一域一目录**(net/http/tls/db/gui;目录名 = 域名,`use std.<域>.<子模块>.{...}`;2026-09-23 转正)。
2. **模块独立(分档流用)**:T1 核心互不 `use`,任何子集可单独 vendored;域包 use 只许下行(T3→T2→T1);域目录内 use 允许、禁环(菱形规避依加载器 ⑥ 口径);跨域 use 须包头注登记消费闭集(http/client、db/pg 先例);T1 禁止携带 C 源。详见「分层与准入」。
3. **函数式值传递**:容器 API 不可变更新(put 返回新映射,原值不变);无全局状态。
4. **命名消歧**:同文件内跨类型同名函数用类型前缀(FMap 族 `f*`);导出函数用
   `pub`,内部助手不带 `pub` 且以下划线开头可读性更好时允许普通名。
5. **文档头**:每模块首行 `// std/<名>.ct —— 用途;口径(字节级/稳定性/复杂度)`。
6. **测试随模块**:每模块含 `test` 块(§4.10),`ctron-cc run std/<名>.ct` 独立可跑,
   失败 rc=1;消费面测试在 `compiler/test/stdpkg/src/main.ct`(use 全量消费)。
7. **字节级纪律**:Str 按字节处理(UTF-8 透传,不做码点拆分),与 wc 等工具口径一致。
8. **无副作用**:std 不做 IO(除 `fs.ct`/`time.ct` 的 Clock 面)、不依赖真实时钟;排序稳定、哈希确定性。T2/T3 域包以显式 IO/平台门面为包用途,本条约束其纯函数面;T1 全项适用。
9. **实现约束 C1–C12**(v0.2 API 规范 §0 登记):禁 as[]/F64 消费面/顶层 `||`/负值宽算术等解释面缺口纪律,详见 `docs/superpowers/specs/2026-09-14-stdlib-v02-api-spec.md`。

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

## 分发与同步

- **规范源**:本目录。`compiler/test/stdpkg/std/` 是**同步副本**(测试种子),
  smoke 含逐字节漂移断言——修改 std/ 后必须同步副本,否则门禁红。
- **示例**:examples/* 的 `std/` 副本是**钉定快照**(按需子集),允许落后;
  升级示例属示例维护,不强制同步。
- `config.ct`(2026-09-17):CTCL 清单校验内核(`config_diags`),黄金对拍第四线内核同源(selfhosted/ctcl_chk.ct 为驱动镜像,两处须同步修改)。
- **vendored C 分工**(2026-09-23):第三方 C 库统一落仓库根 `vendor/<域>/`(gui/clay、gui/raylib、tls/mbedtls、deflate);域包包内 `c_src/` 只放自写胶水(`ctron_*.c`)。二者不得混放;T1 禁止携带任何 C 源。
- 未来包管理器(CTCL 清单语义落地后,见 docs/superpowers/specs/2026-09-16-config-language-v1.md)以本目录为上游注册表形态。

## 演进纪律

- 新模块:先过「分层与准入」准入四问定档 → 按组织宪章写 → 模块 test 块独立跑绿 → 加入本清单与种子同步 →
  消费面用例进 stdpkg/src/main.ct → smoke 全绿 → 单独提交。
- 修改既有模块:同步种子副本 + 消费面回归 + 示例按需升级。
- 语言能力依赖(如方法/泛型/闭包新特性)在三线(自举/C 宿主/R)落地后才可使用。
