# Ctron 标准库 vs 成熟语言基础库——差异与缺失分析

日期:2026-09-15
基线:std v0.2 收口态(17 模块,commit 802465a 系;解释面 17/17 绿)
上游:2026-09-14-stdlib-domains-plan.md(内容域规划)、2026-09-14-stdlib-v02-api-spec.md(实施规范,C1–C16)
方法:以**能力面清单**(非文档宣传)对标,逐域矩阵 + 缺失归因四分法(A 语言能力未达 / B 运行时未达 / C std 可做未做 / D 有意识不做)。

---

## 1. 对标集与判据

| 语言 | 版本基准 | 入选理由 | 对 Ctron 的镜鉴 |
|---|---|---|---|
| Go | 1.22 std | "全电池"典范;net/http 一步到位成就生态;Go 1 兼容承诺的冻结教训 | 域覆盖节奏、SipHash 教训(哈希泛洪)、fuzzing 一等化 |
| Rust | 1.78 stable std | "小 std + crates 生态"典范;trait 形 IO/迭代器;Send/Sync 纪律 | 最小核的边界在哪、迭代器即语言界面、std 不做 async 运行时 |
| Python | 3.12 stdlib | 全电池极端;数据科学/AI 时代事实入口 | batteries 的质量代价、hypothesis 式测试文化、pathlib 的 API 设计 |
| Zig | 0.13 std | 自举消费自家 std;显式 allocator/comptime 贯穿 | 与 Ctron 最同构:显式 Arena(§6.6)+ comptime(§8.4)+ 编译器即 std 第一用户 |

判据:比较"用 std 解决日常任务的覆盖度",不比较文档规模;统一按上表四家**稳定版**能力面。

---

## 2. Ctron std v0.2 现状能力面(实测清单)

| 模块 | 导出面(节选) |
|---|---|
| str | words/contains/lines/join/trim/split(_str)/replace/to_lower/to_upper/pad_*/index_of(_from)/strip_prefix/suffix/count_sub/eq_ignore_ascii_case/repeat/is_ascii_*/str_get/str_some(30+) |
| sort | sorted(_desc)/reversed/sort_by/binary_search_by/sorted_by(_desc/_keys)(比较器注入,稳定) |
| map/set | 函数式不可变 Map[K,V]/Set[T](put/get/has/keys/values/add/mem/tolist/fromlist) |
| fmap | FMap[V] Str 键哈希映射(djb2;fput/fget/fhas/flen/fwords/fvals) |
| heap | 比较器小顶堆(push/pop/peek/sorted)+ I32 便利助手 |
| path | join/dir/base/ext/is_abs/normalize(Unix) |
| fs | read_or/exists(+内建 fs_exists/write/delete/read_file/read_dir) |
| json | parse(路径展平 DOM)/qtag/qval/esc/junesc(序列化转义) |
| enc | hex/base64/percent 编解码(解码限可打印 ASCII,C8) |
| hash | djb2/fnv1a32(非加密) |
| rand | MINSTD 确定性(rng_seed/next/range/shuffle;C14 裁剪) |
| strconv | parse_i64/parse_bool(format 族待 P1-B) |
| time | 纯历法 days 域:leap/days_from_civil/civil_from_days/weekday/date_format/parse(Clock 待 r2b) |
| csv | RFC4180 子集 parse/write/field + get/some 助手 |
| unicode | cp_at/cp_is_start/cp_valid_utf8/cp_iter/cp_count(解码面) |
| opt | and_then/or_else/map2/to_result/get_i/some_i |
| heap 外助手 | enc_get/str_get/sc_get_*/time_get/csv_get 等(match 形解包,C13a 规避) |

Prelude(语言级,非 std):Option/Result/List/Map/Set/String/StringBuilder/Box/Channel/Task/Scope/Mutex/Atomic/Global/Arena/AnyError、fmt 插值、assert 族。

---

## 3. 域级对比矩阵

图例:● 完整 / ◐ 部分 / ○ 无;括号内为 Ctron 归因(A 语言 / B 运行时 / C 可做未做 / D 不做)

| 域 | Go | Rust | Python | Zig | Ctron v0.2 |
|---|---|---|---|---|---|
| 字符串(Unicode 感知) | ● | ● | ● | ◐ | ◐(ASCII 全,Unicode 解码面;大小写/字素 ○) |
| 格式化(宽度/精度/进制) | ● fmt | ● fmt | ● %/format | ● comptime fmt | ○(插值 ✓;format 族待 A:P1-B/C2) |
| 数值转换 | ● strconv | ● | ● int/str | ● | ◐(i64/bool;f64/进制 待 A) |
| 数学函数 | ● math | ● f64/num | ● math/statistics | ● math/big | ○(仅前奏 abs/min/max;F64 面 待 A:C2) |
| 位运算 | ● math/bits | ● | ● int | ● @|& | ○(**bit.ct 推迟,A:C14 残留**) |
| 随机 | ● rand | ○(crate) | ● random/secrets | ● | ◐(MINSTD 确定性;熵 B;分布 C) |
| 哈希(非加密) | ●(SipHash 内建) | ●(SipHash) | ●(SipMMR3) | ●(Wyhash) | ◐(djb2/fnv1a;**SipHash 缺=C**) |
| 加密 | ● crypto 全家 | ○(RustCrypto) | ● hashlib/secrets | ● crypto 全家 | ○(D:远期生态) |
| 容器 | ● | ● | ● | ● | ◐(函数式 Map/Set/heap;**可变泛型 K 哈希表缺=A 需 Hash bound**;deque/Btree ○) |
| 迭代器 | ● range 协议 | ● Iterator 核心 | ● itertools | ● | ○(**A:r3b 迭代器 trait 落地中**;for-in ✓) |
| 排序 | ● slices/sort | ● sort_* | ● list.sort | ● pdq | ●(注入比较器,稳定 ✓) |
| 路径 | ● path/filepath | ● Path | ● pathlib | ● | ◐(Unix 六函数;元数据/遍历 B) |
| 文件系统 | ● io/fs | ● fs | ● os/pathlib | ● fs | ◐(整文件读写;**句柄/缓冲/元数据/目录树=B**) |
| IO 流/缓冲 | ● io/bufio | ● Read/Write | ● io | ● Reader/Writer | ○(**B:io_uring 统一层 §9.5 未达**) |
| 时间 | ●(单调+时区) | ◐(std 基础) | ●(zoneinfo) | ● | ◐(纯历法;**Clock/时长/时区=B/D**) |
| JSON | ● | ○(serde) | ● | ● | ◐(**DOM 查询形;流式/数值类型/写出完备性=C**) |
| CSV | ● | ○(crate) | ● | ◐ | ●(子集) |
| 二进制编码 | ● encoding/binary | ● byteorder | ● struct | ● | ○(**C:varint/定宽编解码可做**) |
| 正则 | ● RE2 | ○(regex crate) | ● re | ○ | ○(**D 有意识推迟;建议 P3 复审**) |
| 压缩/归档 | ● compress/archive | ○(crate) | ● zlib/zip/tar | ● | ○(D 远期) |
| 并发原语 | ● sync | ● sync | ● | ● Thread/atomic | ◐(前奏 Channel/Mutex/Atomic/scope;WaitGroup/Once/RwLock=B/A) |
| 网络 | ● net/http | ○(std 基础+crate) | ● | ● http | ○(**D:v1 不进 std,随生态**) |
| 进程/环境 | ● os | ● process/env | ● | ● process | ◐(r2b Env 落地中;spawn B) |
| 日志 | ● slog | ○(tracing) | ● logging | ◐ | ○(**C:logging-lite 可做**) |
| 测试 | ● testing+fuzz | ●(内建 test) | ● unittest | ● testing | ◐(test 块+assert ✓;过滤/夹具/金样/fuzz=C) |
| 数据库 | ● database/sql | ○ | ● sqlite3 | ○ | ○(D 远期) |

覆盖度粗计(按域全权):Go 24/24,Ctron ● 1 + ◐ 11 + ○ 12。

---

## 4. 逐域差异与缺失分析

### 4.1 字符串(C:可做未做为主)
**已有**:字节级 ASCII 面完整(搜索/切分/剥离/计数/比较/大小写/填充/重复),与 wc/grep 工具口径一致。
**缺失**(对标 Go strings/Rust str):
1. Unicode 感知大小写(`to_lower("É")`)——需 Unicode 表,建议随 unicode.ct 的"表模块"立项(A);
2. `split_n`(限段切分)、`replace_n`(限次替换)、`trim_set`(任意字符集剥离)、`fields_by`(谓词切分)——纯 Ctron 可做(C);
3. 查找族补全:`rindex`(反向)、`find_all`、`has_prefix/suffix` 别名(现 starts_with ✓);
4. Rust 的 `str::split_whitespace` 迭代器形态——依赖 r3b。
**结论**:工具口径已够自举与 CLI;补 C 级四件即可宣称"文本处理 v0.3 完备"。

### 4.2 格式化与转换(A:C2/P1-B + C)
**缺失**:进制格式化(format_hex/bin 已写好待复活)、f64 解析/格式化(C2:发射面 F64 除法值错误)、**格式迷你语言**(Go `%8.2f`/Rust `{:>8}`)——建议按 Zig comptime fmt 形状在 §8.4 comptime 上立项(P2+);定点/十进制(货币安全)Python decimal 对应物远期。
**关键缺口**:parse_f64 被裁(C2)——JSON 数值→f64 全链被此卡住。

### 4.3 数学(A:C2)
整域缺失。F64 依赖 C2(发射面除法);整数域可先行:`clamp/ceil_div/log2/pow2/mod_pow`(C);`sqrt/pow/exp/log` 需 F64 或内建 math_*(发射直映 libm,解释侧宿主函数——同 utf8_enc 先例,可立项)。statistics(mean/median/方差)纯 C 可做(C)。

### 4.4 随机(B+C)
MINSTD 确定性核 ✓。缺:现代生成器(PCG32/xoshiro——C 可做,纯整数);熵种子(B:process 域);真随机/加密随机(B+D);分布采样(normal/指数,C 可做)。Go 教训:全局 rand 源 + top-level 函数在测试中是灾难——Ctron 的显式状态传递(rng_next(s))方向正确,坚持。

### 4.5 哈希与加密(C + D)
**结构性缺口:SipHash-1-3**。FMap 用无键 djb2 → **哈希泛洪 DoS**(攻击者构造同桶键)。Go 2 时代因 DDoS 全线换 SipHash;Python 同。建议:SipHash-1-3(32 位友好,纯整数可写,C)作为 FMap 第二代默认 + 键随机化(种子来自 process 熵,B)。
加密族(sha256/hmac/aead)维持 D(远期生态),但 **sha256 纯 Ctron 可写**(纯整数+循环,Python hashlib 同构)——作为"加密面第一步"候选。

### 4.6 容器(A + C)
**最大缺口:可变泛型键哈希表**。FMap 仅 Str 键;Map/Set 为 O(n) 函数式。需语言侧 Hash trait + Eq bound(A)后做 `HashMap[K,V]`(开放寻址 + SipHash)。其余缺失:deque/环形缓冲(C,List 可搭)、BtreeMap(有序映射+范围查询,C 大件)、MultiMap(C)、位集(C)。函数式不可变族是 Ctron 特色(对齐宪章 3),保留与可变族并存(cocurrent 语义对齐 §7)。

### 4.7 迭代器(A:r3b)
r3b 落地中(trait + for + map/filter/take/sum/count/any/all/collect)。对标 Rust 缺:zip/enumerate/take_while/skip_while/chunk/flat_map/scan/rev、collect 进容器、惰性保证。迭代器是 Rust 生态的"通用货币"——Ctron 的对应物是 **Iterator trait + UFCS 链**,方向一致,补齐适配器族即达"日常够用"。

### 4.8 文件系统与 IO(B 为主)
现状 = 整字符串读写,无句柄/缓冲/元数据/目录遍历/临时文件/glob。**架构缺口:fs.ct 走全局内建,偏离 §8.1 能力模型**(roadmap 已有 r2b_fs_fake 锚)。补齐顺序建议: capability 形 Fs(Fake 注入)→ 元数据/目录树 → 句柄+缓冲(依赖 B:io_uring 统一层)→ glob(C,fnmatch 子集)。Go io/fs 接口 + Python pathlib 是 API 对标。

### 4.9 时间(B + D)
纯历法 ✓。缺:Clock(单调/墙上,r2b)、Duration 类型与运算、时刻(时刻=历法+时戳)、时区(zoneinfo——IANA 数据表是重负担,建议 D:std 只做 UTC+固定偏移,zoneinfo 走数据包)、定时器(B)。Go 单调时钟读数(interval 测量不被 NTP 干扰)是 bench 正确性关键——bench.ct 依赖此。

### 4.10 JSON(C:最有战略价值)
现状 = 路径展平 DOM(查询友好)。对标 Go/Rust serde/Python json 缺:
1. **写出路径**:write_json(对象/数组/转义反向)——`qtag/qval` 有查询无构造,C;
2. **数值类型**:当前值恒为文本,`$.age` 拿到 "33" 而非 33——需带类型的 Value 树或按需强转(C);
3. **流式解析**(大文件)——B(增量读)+C;
4. serde 式 derive(`@derive(Json)`,§8.4 预留)——语言侧 A。
注:json.ct 已是自举消费候选(Ctron.toml 走 TOML,但工具链 json 面用它)——"std 即编译器第一用户"战略兑现点。

### 4.11 二进制编码(C)
Go encoding/binary 对应物全缺:定宽大端/小端编解码、varint(protobuf 基础)、位打包。纯整数可写(C),是网络协议/文件格式的前置。

### 4.12 正则(D,建议 P3 复审)
domains-plan 曾拒绝。复审理由:agent 时代文本抽取高频;Go 证明 RE2 式线性引擎(无回溯)可兼顾安全与性能。建议 P3 以"RE2 子集"立项,或先以 glob+str 组合顶住。

### 4.13 并发原语(前奏 ✓ + B/A)
前奏已有 Channel/Mutex/Atomic/scope/Task(结构化并发语言级——Go/Rust std 都没有,是 Ctron 优势项)。缺:WaitGroup(scope 已覆盖大半)、Once/static-let 已覆盖、RwLock(读多写少场景)、AtomicU64(B)、sleep/ticker(B)。对标结论:**无需扩原语,补文档与用例**;worker pool 建议走 std 之外的"一等模块"。

### 4.14 网络/压缩/数据库(D)
维持拒绝清单。教训对照:Go 的 net/http 是生态引爆点,但 Go 背后有大厂运维承诺;Ctron v1 无此条件——按 domains-plan 走"一等第三方模块"形态,待包管理(P2-B)成型后评估提级。

### 4.15 测试/诊断/日志(C)
test 块 + assert 语言级是优势(Go/Rust 都要宏/框架)。缺:测试过滤/夹具/金样(golden)助手、**fuzzing**(Go 差异化能力;属性测试 lite 可先做:随机+收缩,C 但受 C14 限制)、logging-lite(级别+kv+可注入 sink,对接 §8.1 Log 能力)。

---

## 5. 缺口分级与补齐路线

### v0.3 候选(纯方言可做,按价值排序)
1. **SipHash-1-3**(FMap 二代默认,安全项)
2. **json 写出路径 + 数值类型化查询**(自举战略)
3. **math 整数域 + 常量 + statistics**(F64 面随 C2)
4. **str 四件**:split_n/replace_n/trim_set/rindex
5. **二进制编码**:varint + 定宽 LE/BE
6. **glob-lite** + **path 元数据**(随 fs 内建扩展)
7. **logging-lite**(对接 Log 能力)
8. **测试金样/属性 lite**(C14 解除后属性全量)
9. **crc32 复活**(宿主稳定性随 P1-A2 收敛后)
10. **deque/位集**(C)

### v1 前置(随语言/运行时)
- bit.ct 复活(A:C14 残留解除)
- format 迷你语言(A:comptime,P2 形状按 Zig)
- 迭代器适配器族(A:r3b)
- Clock/Duration/sleep(B:r2b)
- fs 元数据/目录树/句柄缓冲(B:io_uring)
- HashMap[K,V] 可变泛型(A:Hash bound)
- parse_f64/format_f64(A:C2)
- 派生 Json(A:§8.4 derive 插件)

### 拒绝清单(维持,含复核点)
net/http、加密全家、数据库、UI、async 运行时、timezone 数据包、ORM。复核点:P3 复审 regex(RE2 子集)与 net(生态条件)。

---

## 6. 跨切面结构性差异

1. **分发模型**:四家对标语言都有系统级安装/包管理;Ctron v1 = vendored 源码(P2-B 包管理未落地)→ **std 必须阶段性偏"胖"**(Zig 模式),随包管理成型再走 Rust 式"减法"——与 domains-plan 的升降级策略一致。
2. **能力模型错位**:Go/Python 的 os 全局隐式 vs Ctron §8.1 能力注入。现状 fs.ct 直呼全局内建、time.ct 无 Clock——**偏离自家能力模型**。r2b 接线时应把 fs/time 的 API 面改造为 `&Fs`/`&Clock` 注入形(roadmap 的 fs_fake/FakeClock 锚已备)。
3. **自举第一用户**:Zig 教训的兑现点——json/path/str/toml(未来)按"编译器要吃"排优先级,当前选择正确;建议 Ctron.toml(P2-B)落地时把 toml.ct 提到 v0.3 前排。
4. **安全基线**:SipHash(泛洪)、常量时间比较、secrets——四家对标语言全部内建防线,Ctron 全缺。**这是唯一"安全级"缺口,建议提到 v0.3 首位**。
5. **错误与测试文化**:Ctron 的语言级 test 块 + Result 必须 must-use + 能力注入 Fake 是相对四家的优势项;缺的是 harness 层(过滤/金样/fuzz),非语言层。

## 7. 结论

v0.2 的 17 模块在"ASCII 文本 + 纯函数 + 工具口径"内已达到**自举与 CLI 够用**线;与成熟语言的差距集中在四条轴线:
- **Unicode 全量**(表与算法,A)
- **IO 流/句柄/时间**(运行时,B)
- **语言能力依赖面**(迭代器/位运算/格式化/F64,A)
- **安全基线**(SipHash/secrets,C 级可做,建议最优先)

四家教训的吸收点:Go 的兼容冻结风险(已用 since/stability 对冲)、Rust 的 std 最小化(vendored 阶段先不学)、Python 的 batteries 质量陷阱(以 test-即-合同对冲)、Zig 的自举即质量(已在兑现)。
