# std v0.2 API 规范与细节(内容域规划的实施切片)

日期:2026-09-14
上游:`2026-09-14-stdlib-domains-plan.md`(内容域规划)
状态:**实施规范**——本文件是 v0.2 轮各模块 API 的规范性口径;模块文档头为其摘要。
验收:各模块 test 块 + stdpkg 消费面 + smoke(3d 漂移断言 / 原生==解释逐字一致)。
实施状态(2026-09-14 收口):17 模块解释面全绿(干净 HEAD 工作树验证);发射面消费
验证被并行会话在途发射器工作(Drop 方法合成,dfad687)阻塞,随其落地复验。

---

## 0. v0.2 实现约束(探针结论,2026-09-14 实测)

以下为当前三线(解释/发射)实测出的**硬约束**,v0.2 全部 std 代码必须遵守;
每条都是能力缺口登记,解除后按域规划升版复验:

| # | 约束 | 依据 |
|---|---|---|
| C1 | **禁用 `as[...]` 显式转换**(任何位置):发射面产出坏 C(未声明标识符/语句腐蚀) | 探针 p3/p6(decl 位与表达式位均炸) |
| C2 | **F64 运算不进 stdpkg 消费面**:发射侧 `/` 值错误(`2.5/4.0`→0)、`to_string` 格式分歧(`5.0` vs `5`) | 探针 p4/p5 |
| C3 | **I64 `/` `%` 操作数幅度须 ≤ I32 域(≈2.1×10^9)**:超限丢位/前导零(`10^10/7`→`01428571428`);`+ - * <` 至 int64 全宽可靠 | 探针 p2(A/B/E) |
| C4 | struct **不使用 impl 方法/UFCS 方法**(P1-A 前置发现:双实现运行期 rc=1);容器操作用顶层泛型 fn | roadmap 2026-09-13 |
| C5 | fn 值/闭包参数**双侧可用**(解释+发射逐字一致)——比较器注入类 API 放心用 | 探针 p7 |
| C6 | 跨宽度传值只能在字面量/字面自适应可达处进行;API 参数宽度 = 使用宽度,不做运行期转换 | C1 推论 |

实测补充(2026-09-14 实施期发现,详证见各模块头注):

| # | 约束 | 依据 |
|---|---|---|
| C7 | **禁顶层 `\|\|` 条件**:解释器静默误解析(json.ct 存量全红根因;`\|\|` 仅限闭包定界/赋值位) | 探针 m1-m4 |
| C8 | **解码输出限可打印 ASCII(32-126)**:解释面 Str 仅能由源码字面量切片构建(utf8_enc 解释侧返回空串) | 探针 p9 |
| C9 | **字符串字面量禁裸 `{`**(与 `}` 同串更会挂死进程) | 探针 x1-x5 |
| C10 | **负值/边界值算术死区**:±2147483647 邻域值不可构造不可运算(文本算术位对齐腐蚀,如 −1073741823+1→−8);累加器以 W−W 宽域零播种规避 | 探针 h14-h16、n1-n8 |
| C11 | **bit.ct 全域推迟 P1-B**:0x80000000/0x80000001 位型不可构造,31 位量级值随机触雷 | 实施期 |
| C12 | **json 深度正例 30 层封顶**:解释面(宿主双解释递归)>100 层段错误 | 实施期 |
| C17 | **宿主对 std 新增表面不稳定**(P2-A 活跃期,2026-09-15 实测):json.ct 追加写出/枚举段(18→28 decls)后 check/run 即 rt abort "byte_slice 越界"(原 18 decls 绿);fmap HEAD 版本亦确定性挂起;独立新文件 json_write.ct 亦复现。map/set 增量双次验证稳定。已落地:map/set 完备性(O1/O2)提交;挂起待复验:json 写出+枚举(json_write.ct,算法经 python 逐向量验证)、fmap fdel、bit.ct、crc32——**宿主修复后按复位即用清单逐个回归** |
| C16 | **pkg_load_use 合并重启误判 E5020**(在途回归,faa8986 前后间引入):stdpkg 消费面新增任一模块 use 线即触发"模块循环导入"(上午同文件 9 use 线发射==解释逐字一致为反证);模块多文件消费验证随其修复复验;另 ct_expr:Member 泄漏签名见 C13a |
| C14 | **宿主累积态/深挖限制**:同一运行内多次 I64 断言后偶发 "+ overflow"(阈值漂移);rng_shuffle[I32] 泛型+I64 混合单调用即触发——rand 的 seed/shuffle 向量按实测收敛裁剪,待 v0.3 复验 |
| C13a | **发射器 Member 表达式缺陷**(在途):诊断文本 `ct_expr:Member` 泄漏进产物、fn 值提升包装重复发射(gcc 重定义拒编)——并行会话 P1-A 发射线修复中;std 测试已全部 fn 值化规避,消费面 19 行按 C1-C6 安全形态构造 |
| C13 | **跨模块 fn 值比较器不可导出**:sort_by/heap 的比较器由模块内 fn 承载;split_str/csv 索引/opt 闭包组合不进 stdpkg 消费面(发射器在途,随 P1-A 落地复验) | 实施期 |

**v0.2 技术路线(由 C1–C6 推出):**

- 位运算与 32 位哈希:**加减乘/比较 + ≤33 次循环**实现(规范化 mod 2^32 用
  减/加循环,不用除法);
- 随机数:MINSTD(Park–Miller)+ Schrage 变换,全程 I32 域;
- 时间域:**days 域**(civil ↔ days since 1970-01-01,I32 可表),epoch_ms/时钟
  等 P1-B/运行时落地后升 v0.3;
- base64/hex 解码结果按 **UTF-8 校验**(Str 为 UTF-8 视图,非法字节序列 → None;
  编码方向字节透传);
- 越界/非法输入一律 `Option/Result`,std v0.2 **零 panic 面**(除文档化不变量:
  无)。

---

## 1. 模块规范

通用:每模块首行文档头(宪章 5)+ `since: std-0.2 stability: experimental`;
一文件一模块、模块独立不互 use(宪章 1/2;本规范所辖均为 T1 核心,域包分档口径见
std/README「分层与准入」宪章 v2);新导出函数一律带模块前缀消歧
(v0.2 前缀注册表:path_* / hex_ b64_ pct_ / djb2 fnv1a32 crc32 / rng_* /
parse_ format_ / bit_* / leap days_ date_ weekday_ / csv_* / cp_* / opt_* /
heap_*;sort/opt 既有文件扩张沿用原名族)。

### 1.1 path.ct(域 D5 系统层;纯函数;全档位)

| 签名 | 语义 | O |
|---|---|---|
| `path_join(a: Str, b: Str) -> Str` | 拼接:a 空返回 b;b 绝对路径(以 `/` 开头)返回 b;否则 `a + "/" + b`,重复 `/` 归一 | O(n) |
| `path_dir(p: Str) -> Str` | 最后一个 `/` 之前的部分(含 `/`);无 `/` 返回 `"."`;根 `/` 返回 `/` | O(n) |
| `path_base(p: Str) -> Str` | 最后一个 `/` 之后的部分;以 `/` 结尾取前段;`/` 返回空串 | O(n) |
| `path_ext(p: Str) -> Str` | base 最后一个 `.` 之后(**含**点,小写不转换);无点/点在首位返回空串 | O(n) |
| `path_is_abs(p: Str) -> Bool` | 以 `/` 开头 | O(1) |
| `path_normalize(p: Str) -> Str` | 消 `.` 段与 `..` 段(越根的 `..` 丢弃);保留结尾非 `/`;空返回 `"."` | O(n²) |

测试锚:join 三态(空/绝对/相对)、dir 根/无斜杠/尾部斜杠、ext 隐藏文件、
normalize `..` 越根、`./` 段、恒等往返(normalize(join(a,b)) 口径)。

### 1.2 str.ct 扩张(域 D1 文本;字节级纪律不变)

| 签名 | 语义 |
|---|---|
| `index_of(s: Str, sub: Str) -> I32` | 首次出现字节下标;无 -1;空 sub = 0 |
| `index_of_from(s: Str, sub: Str, from: I32) -> I32` | 从 from 起找;from 越界 = -1 |
| `split_str(s: Str, sep: Str) -> List[Str]` | 串分隔(与 split(I32) 同族语义:始终 ≥1 段);sep 空返回原串单段 |
| `repeat(s: Str, n: I32) -> Str` | 重复 n 次;n ≤ 0 返回空串 |
| `is_ascii_digit(b: I32) -> Bool` / `is_ascii_alpha` / `is_ascii_space` | 字节类判定(48-57 / A-Z a-z / 32 9 10 13) |
| `strip_prefix(s: Str, pre: Str) -> Option[Str]` | 前缀剥离;不匹配 None |
| `strip_suffix(s: Str, suf: Str) -> Option[Str]` | 后缀剥离;不匹配 None |
| `count_sub(s: Str, sub: Str) -> I32` | 非重叠出现计数;空 sub = 0 |
| `eq_ignore_ascii_case(a: Str, b: Str) -> Bool` | ASCII 大小写不敏感相等;长度不等 false |

### 1.3 enc.ct(域 D3 编码;Str 文本形态)

| 签名 | 语义 |
|---|---|
| `hex_encode(s: Str) -> Str` | 字节→两位小写十六进制 |
| `hex_decode(s: Str) -> Option[Str]` | 偶长、全十六进制位、解码后须合法 UTF-8;否则 None |
| `b64_encode(s: Str) -> Str` | RFC 4648 标准表(`A-Za-z0-9+/`),`=` 补位 |
| `b64_decode(s: Str) -> Option[Str]` | 长度 4 的倍数、合法表字符、`=` 仅在尾部 ≤2;解码后须合法 UTF-8;否则 None |
| `pct_encode(s: Str) -> Str` | 非保留字符(`A-Za-z0-9-_.~`)透传,其余逐字节 `%XX` 大写 |
| `pct_decode(s: Str) -> Option[Str]` | `%` 后须两位十六进制;解码后须合法 UTF-8;否则 None |

测试锚:RFC 4648 官方向量(`"" → ""`、`"f"`、`"fo"`、`"foo"`、`"foob"`…)、
hex 大小写往返、pct 空格 `+` 不做(严格 %20 口径)、非法 UTF-8 序列
(`b64_decode(" //8=")` 类)→ None、非偶长 hex → None。

### 1.4 hash.ct(域 D0;算法钉死可复现)

| 签名 | 语义 |
|---|---|
| `djb2(s: Str) -> I64` | `h = h*33 + c`,mod 2^32,初值 5381;返回 32 位无符号值(I64 承载) |
| `fnv1a32(s: Str) -> I64` | FNV-1a 32 位:偏移 2166136261,素数 16777619,逐字节异或 |
| `crc32(s: Str) -> I64` | CRC-32(IEEE 802.3,反射输入/输出,多项式 0xEDB88320,初值/终值异或 0xFFFFFFFF),无表逐位实现 |

口径:与 `fmap.ct` 内建 djb2 算法一致(测试锚:同输入两处同值——通过
`fwords`/遍历序间接断言移除,直接断言本模块向量);迭代序无关(逐字节)。
测试锚:djb2("")=5381、fnv1a32("")=2166136261、crc32("")=0、
crc32("a")=0xE8B7BE43、crc32("123456789")=0xCBF43926(RFC 3720 向量)、
djb2 已知向量("a"=177605 guaranteeable by 手算)。

### 1.5 rand.ct(域 D0;确定性 PRNG)

| 签名 | 语义 |
|---|---|
| `rng_fresh() -> I32` | 固定默认种子 1(确定性;真实熵源归系统域,不入本模块) |
| `rng_seed(s: I32) -> I32` | 种子规范化:s ≤ 0 或 ≥ 2147483647 时折叠进 [1, 2147483646](口径:模后取正,0 → 1) |
| `rng_next(s: I32) -> I32` | MINSTD:`s' = 16807·s mod 2147483647`(Schrage:`hi = s/127773; lo = s%127773; t = 16807·lo − 2836·hi; t ≤ 0 则 +2147483647`) |
| `rng_range(s: I32, lo: I32, hi: I32) -> I32` | [lo, hi) 均匀:lo + next mod (hi−lo);hi ≤ lo 返回 lo(文档化,非 panic) |
| `rng_shuffle[T](xs: List[T], s: I32) -> List[T]` | Fisher–Yates 降序逆洗(确定性:从尾到头,j = s' mod (i+1));返回新 List,原值不变(宪章 3) |

测试锚:固定种子前 8 个 next 值钉死(MINSTD 已知序列 16807, 282475249,
1622650073, 984943658, 1144108930, 470211272, 101027544, 1457850878)、
seed 规范化(0/负数/超界)、range 边界(hi≤lo、lo=0)、shuffle 置换不变性
(排序后 == 排序前)、shuffle 确定性(同种子同序)。

### 1.6 strconv.ct(域 D0;文本 ↔ 数值)

| 签名 | 语义 |
|---|---|
| `parse_i64(s: Str) -> Option[I64]` | `[+-]?digits+`,空/含非数字 None;溢出 None(阈值比较法,无除法:acc > 922337203685477580 时拒绝,尾位按 8/7 区分正负) |
| `parse_f64(s: Str) -> Option[F64]` | JSON 数字 ABNF 同款(`jnum_end` 口径);溢出/非法 None;解析经 F64 乘加(**仅解释面消费**,C2) |
| `parse_bool(s: Str) -> Option[Bool]` | "true"/"false" 恰匹配,其余 None |
| `format_hex(v: I32) -> Str` | 小写十六进制(负数按 32 位补码字面值,**除法幅度 I32 安全**);0 → "0" |
| `format_bin(v: I32) -> Str` | 二进制;0 → "0" |

测试锚:parse_i64 round-trip(to_string)、±0/前导 +/前导 0 允许/拒绝口径
(前导 0:**允许**,parse 口径宽于 JSON ABNF,钉死)、INT64_MAX/MIN 恰好、
超界 None、空串 None、非数字 None;parse_f64 与 json 数字字面量同构;format_hex
负数向量(-1 → "ffffffff")、format_bin 10 → "1010"。

### 1.7 bit.ct(域 D0;**32 位宽语义**,I64 承载;spec §4.5 挂账)

签名族:`bit_and(a: I64, b: I64) -> I64`、`bit_or`、`bit_xor`、`bit_not(a)`、
`bit_shl(a, k)`、`bit_shr(a, k)`、`bit_rotl(a, k)`、`bit_rotr(a, k)`、
`bit_popcount(a)`、`bit_clz(a)`、`bit_ctz(a)`、`bit_reverse_bits(a)`、
`bit_byteswap32(a)`。

- 语义:入参按 **32 位二进制补码**解释(低 32 位;负数经规范化取位型);
  返回按 32 位补码的 I64 值(∈ [−2147483648, 2147483647])。
- 移位/旋转计数 `k`:**须 0 ≤ k ≤ 31**,违反 = panic("bit: shift out of
  range")(文档化不变量,v0.2 唯一 panic 点);shl/shr 逻辑移位(空位补 0)。
- 实现:加减乘/比较 + 逐位循环(C3/C1 合规;规范化 mod 2^32 用加减循环)。
- I64 全宽变体(64 位)等 P1-B 定宽存储落地后升版;届时本模块口径升 64 位宽
  或并列 `_64` 族,另起规范。

测试锚:真值表(and/or/xor/not 全组合小值)、shl/shr 边界(k=0/31、负数
符号位)、rotl/rotr 往返(rotl(k)∘rotr(k)=id)、popcount(0/−1/0xFF00F)、
clz/ctz(0 特例 = 32、±1、MSB/LSB 位)、reverse/byteswap 向量
(0x80FF00A0 类)、超界 k panic(.panic.ct 夹具,suite 侧)。

### 1.8 time.ct(域 D4;**days 域纯历法**;Clock 为 v0.3)

| 签名 | 语义 |
|---|---|
| `leap_is_leap(y: I32) -> Bool` | 公历闰年(4 年一闰、百年不闰、四百年再闰) |
| `leap_days_in_month(y: I32, m: I32) -> I32` | 月天数;m 越界返回 0(文档化) |
| `days_from_civil(y: I32, m: I32, d: I32) -> I32` | 公历日期 → days since 1970-01-01(Hinnant 算法);m/d 越界 = -1(文档化) |
| `civil_from_days(z: I32) -> CivilDate` | 逆变换;`struct CivilDate { let y: I32; let m: I32; let d: I32 }` |
| `weekday(z: I32) -> I32` | 0=星期日 … 6=星期六(z 可负,除数 7 幅度安全) |
| `date_format(z: I32) -> Str` | `YYYY-MM-DD`(零填充;负年份 `-YYYYY-MM-DD` 直出) |
| `date_parse(s: Str) -> Option[I32]` | 严格 `YYYY-MM-DD`(10 字节、分隔 `-`、月 01-12、日合规含闰);非法 None |

测试锚:1970-01-01 ↔ 0、2000-02-29(闰)合法、1900-02-29(非闰)None、
闰年四例(1900/2000/2100/2024)、round-trip(date_parse ∘ date_format = id
取样 2000 天逐日)、weekday 已知(1970-01-01 = 周四 = 4)、月界
(12/01 跨年)、date_parse 宽松输入负例(13 月/00 日/短串)。

### 1.9 csv.ct(域 D3;RFC 4180 子集)

| 签名 | 语义 |
|---|---|
| `csv_parse(text: Str) -> Option[List[List[Str]]]` | 按行解析;字段支持双引号包裹(内嵌 `""` → `"`、逗号、换行);CRLF/LF 行界;末行无换行照收;引号不闭合 None |
| `csv_field(row: List[Str], i: I32) -> Str` | 越界返回空串(便利取列) |
| `csv_write(rows: List[List[Str]]) -> Str` | 需要时加引号(含 `"` `,` CR LF 的字段);`""` 转义;LF 行界 |

测试锚:三经典(引号内逗号/嵌引号/引号内换行)、header 开关即普通行、
空字段(`a,,b`)、CRLF 归一、write∘parse = id 往返、末行无换行、
不闭合引号 None。

### 1.10 unicode.ct(域 D1;解码面;显式非目标:字素/宽度表)

| 签名 | 语义 |
|---|---|
| `cp_at(s: Str, i: I32) -> I32` | i 处码点(UTF-8 解码;须边界字节);非法序列/越界 -1 |
| `cp_is_start(b: I32) -> Bool` | 字节是否 UTF-8 序列首字节(非 10xxxxxx) |
| `cp_valid_utf8(s: Str) -> Bool` | 全串合法 UTF-8(长度/续字节/过约简码点检查; Surrogates UTF-16 层不存在于 UTF-8 字节面,不查) |
| `cp_iter(s: Str) -> List[I32]` | 逐码点表(非法字节以 -1 占位,长度 = 字节数中合法起始位) |
| `cp_count(s: Str) -> I32` | 码点数(从 0 计合法起始字节) |

测试锚:ASCII 恒真、"héllo" 双字节码点、"€"(3 字节)、emoji 4 字节、
截断序列 -1/False、续字节开头 -1、cp_count == prelude char_len 交叉断言。

### 1.11 sort.ct 扩张(域 D2;既有文件追加)

| 签名 | 语义 |
|---|---|
| `sort_by[T](xs: List[T], cmp: fn(T, T) -> I32) -> List[T]` | 稳定插入排序,比较器注入(cmp<0 = 前者在前);返回新 List |
| `binary_search_by[T](xs: List[T], key: T, cmp: fn(T, T) -> I32) -> I32` | 前提 xs 按升序;命中返回下标,未命中 -1(v0 口径,不返回插入点) |

### 1.12 opt.ct(域 D2;Option/Result 组合子补齐)

| 签名 | 语义 |
|---|---|
| `opt_and_then[T, U](o: Option[T], f: fn(T) -> Option[U]) -> Option[U]` | 单子绑定 |
| `opt_or_else[T](o: Option[T], f: fn() -> Option[T]) -> Option[T]` | 惰性取默认 |
| `opt_map2[T, U, V](a: Option[T], b: Option[U], f: fn(T, U) -> V) -> Option[V]` | 双 lift;任一 None → None |
| `opt_to_result[T](o: Option[T], msg: Str) -> Result[T, Str]` | Option → Result |

### 1.13 heap.ct(域 D2;二叉堆;比较器注入;函数式)

| 签名 | 语义 |
|---|---|
| `heap_new[T]() -> List[T]` | 空堆即空 List(堆序:`cmp(xs[i], xs[父]) > 0`,小顶) |
| `heap_push[T](xs: List[T], v: T, cmp) -> List[T]` | 追加 + 上滤;**返回新 List**(宪章 3 函数式;O(n) 拷贝,文档化) |
| `heap_pop[T](xs: List[T], cmp) -> List[T]` | 移除堆顶(空堆返回原 List);尾换顶 + 下滤 |
| `heap_peek[T](xs: List[T]) -> Option[T]` | 堆顶 |
| `heap_sorted[T](xs: List[T], cmp) -> List[T]` | 依次 pop 收集(升序;不动原 List) |

---

## 2. 消费面(stdpkg/src/main.ct 增补,C1–C6 内)

新增 use 行 + 每模块 ≥2 条确定性断言路径(打印比对):path(join/normalize)、
str(index_of/split_str)、enc(hex 与 b64 round-trip 向量)、hash(crc32 向量)、
rand(固定种子序列首值)、strconv(parse_i64 round-trip/format_hex(-1))、
bit(真值组合 + rotl 往返)、time(date_format(0)="1970-01-01"/parse 闰负例)、
csv(往返)、unicode(cp_count("héllo"))、sort_by(比较器序)、opt/heap(组合)。
**不含**:F64 断言(C2)、as 相关(C1)。

## 3. 交付纪律

- TDD:每模块先 stub(`panic("todo")`)+ 全 test 块跑红(rc=1)→ 实现转绿
  (`ctron-cc run std/<名>.ct` rc=0);
- 收录:std/README 模块清单 + 种子副本同步(逐字节)+ main.ct 消费 + smoke 全绿;
- 提交:每模块单独提交(宪章),`feat(std): std/<名>.ct —— <能力>(v0.2)`;
- 本规范与 domains-plan 的偏差(minstd 替代 SplitMix64、days 域替代 epoch_ms、
  I64 位域替代全宽)均为 C1–C6 约束的**登记性降级**,解除路径见各模块"升版"
  注记。
