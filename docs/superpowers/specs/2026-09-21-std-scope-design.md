# std 范围裁定与创新设计(对照成熟语言标准库)

> 状态:**设计稿 v1(2026-09-21)**——std 泳道范围宪章 + 创新方向。对照基线:Go(1.22,batteries-included ~150 包)、Python(3.12,200+ 模块)、Rust(std 刻意小 + crates 生态)、stb/Zig(源码伴随分发,最接近 Ctron 模型)。规范锚:std/README 组织宪章、§11 网络(冻结 v0.8.1)、§12 数据访问(草案,P5 前定稿)、§9 config。

## 1. 现状底盘(2026-09-21 实测)

23 模块 / 约 240 pub fn / 约 400+ 断言。分域:

| 域 | 模块 | 对照成熟库的覆盖判定 |
|---|---|---|
| 文本 | str(38 fns)、strconv、unicode、csv | Go `strings`/Python `str` 常用面齐(replace/pad/partition/rindex/split_n/eq_ignore_ascii_case…);byte 级纪律是差异点 |
| 数据交换 | json、json_write、enc(hex/b64/pct)、config(CTCL) | json 对位 RFC 8259;enc 对位 RFC 4648;config 是自有规范(§9) |
| 结构容器 | map、set、fmap、heap、sort、opt | 函数式值传递是差异点;对照 Go containers/Python collections 缺有序变体与可变族(语言无宏,泛型深度受限) |
| 系统 | fs、path、time(历法) | 最小面;time Clock 待 v0.3(内建 now_ms 已在 C rt) |
| 数值 | math(23 fns,整数域+饱和全集)、rand(MINSTD) | 整数域饱和系已超 C stdlib;F64 面待 C2 |
| 网络/服务 | net(25)、net/bind、tls(10)、http/{parse,message,binddeflate} | 服务器泳道在建;RFC 9110/9112 严格子集 + 走私加固,质量锚已树立 |
| 界面 | gui(32) | gui 泳道所有 |

## 2. 范围裁剪的四个硬约束(判定项)

1. **源码形态分发**:模块即分发单位,随程序携带 → 必须小、闭集、零内部依赖(宪章 2"模块独立")。Go 式大而全互依在这里是负资产;正确参照系是 stb/Zig,不是 Go/Python。
2. **三宿主实现税**:每个 pub fn 须在 Rust 解释 / 自举解释 / 发射 C-rt 三口径行为逐字一致 → std 面积 × 3 的 parity 义务。功能准入必须有三线可验证兜底;**小而精是硬约束,不是品味**。
3. **语言能力边界**:F64 面(C2)、宏缺失、泛型深度 → regex 引擎、序列化派生等域当前成本畸高。
4. **消费面即需求源**:examples(CLI 对数工具、Todo、gui_calc)+ 服务器线 + gui 线是仅有真实需求源。"Go 有所以要有"不成立。

## 3. 范围裁定

### 3.1 核心层(已覆盖,持续加深)
文本/数据/结构/系统/数值五域(见 §1)——对位成熟库的"CLI 工具 + 数据交换"常用子集,判定:**已达 70–80%,按调用面加深,不按目录表铺**。

### 3.2 规范承诺层(std 泳道必须规划,有外部锚)
| 目标 | 锚 | 优先级依据 |
|---|---|---|
| **std/crypto:SHA-256/HMAC/PBKDF2(纯 Ctron)** | §12.2 规范级前置,RFC 6234/4231/6070 向量 | §12 纯 Ctron 驱动(SCRAM-SHA-256)的硬依赖;向量现成,确定性最高 |
| time Clock 面 | 内建 now_ms 已存在;v0.3 承诺 | 包装 + 单调钟语义,小切片 |
| F64 数学面 | C2 解除 | abs/floor/ceil/round/sqrt/pow/常量一次铺;rand 均匀分布 |
| url 解析 / query 编码 | P4-B 配套 | http 泳道拉入时随片 |

### 3.3 候补层(真实需求触发,预登记不预实现)
sha 系 checksum(crypto 之外)、toml/yaml(config.ct 已占 CTCL,勿重复)、有序容器变体、无符号域 sat 系(U64 分歧收敛后)。

### 3.4 明确不做(及理由)
- **regex 引擎**:自写成本畸高(三宿主税 × 引擎复杂度);str 结构化面已覆盖 CLI 级需求。若真实需求出现,优先"结构化选择器"创新(§5.3)而非 POSIX 克隆。
- **压缩通用库**:http 泳道已按需自建 binddeflate(http 配套,非通用入口)。
- **database 驱动本体**:§12 归服务器线 P5;std 只供 crypto 原语。
- **大而全对齐 Go/Python**:违背分发模型与三宿主税。

### 3.5 组织形态演进(承认既成事实)
域目录子模块已出现(std/http/{parse,message,binddeflate}、std/net/bind)——"一文件一模块"宪章在大域演化为"一域一目录、目录内零 use、加载器菱形规避(⑥ 口径)"。建议宪章 v2 正式收录此形态,并保持**目录内互不 use**(模块独立纪律不破)。

## 4. 质量准入门槛(向成熟库吸收的纪律)

1. **外部权威锚定**:每模块验收锚 = RFC 向量或 Unix 权威工具对数(服务器线已示范:RFC 9110 严格子集、走私加固)。enc 对 `base64`/`xxd`、csv 对 RFC 4180(已做)、time 对 `date`、hash/crypto 对 RFC/NIST 向量、json 对 `jq`。
2. **版本纪律**:since/stability 头(已有);破坏性演进走 since 升版。
3. **饱和/陷阱两分**:数值域 sat_*(安全出口)与 trap 系(C10 死区)语义两分并文档化(v0.4/v0.5 已示范)。

## 5. 创新设计(五项)

### 5.1 三线逐字一致作为一等承诺(Parity Badge)
- 机制:smoke 增加 std 全模块矩阵腿——每模块 × 三口径(ctron test / 自举解释 / 发射原生),输出逐字一致;模块头记 `parity: 3/3`。
- 差异化:没有成熟语言承诺"标准库跨实现逐字节一致"(Go 单实现;Rust 靠生态)。Ctron 三解释器架构下这是刚需,反转成卖点:**换宿主,不改一个字节的程序行为**。

### 5.2 对数式验收(Log-anchored Testing)
- 机制:把 examples 已示范的"与真工具对数"(ctwc↔wc、ctgrep↔grep -F、ctwf↔sort/uniq)升格为 std 全域验收哲学,每模块 README 记对数命令。
- 差异化:stdlib 正确性锚在 Unix 四十年权威上,验收语言是"与真工具逐字节对数",不是自证测试。

### 5.3 Agent-facing 契约头(Machine-readable Contract)
- 机制:模块头机器可读块从 since/stability 扩到 per-fn 一行契约(签名|复杂度|失败模式|最小示例);`ctron doc std.str` 抽取为 LLM 上下文。命名纪律(无重载、无隐式转换、全 snake)本身即 agent 友好设计。
- 差异化:第一个把"给 agent 读"作为 stdlib 文档形态目标的语言——与 Ctron"agent 照抄能跑/修复定位"的核心场景直接对齐。

### 5.4 vendor 切片工具化(`ctron vendor std.str,std.csv out/`)
- 机制:按 use 闭集自动拷贝模块至程序目录并生成钉版清单(现 smoke 3i 人工 cp)。
- 差异化:把 stb 式"拷走即用"从纪律变成命令,闭集与版本自动保证。

### 5.5 确定性即契约(Total Determinism)
- 机制:成文承诺——除 fs/net/clock 显式门面外,std 全域同输入同输出:rand 种子确定、map 插入序、sort 稳定、hash 钉死、无全局状态(宪章 8 雏形升格为规范附录);验收 = 同程序双跑逐字一致(ctwf 双跑腿已有)。
- 差异化:golden testing / agent 重放 / 跨宿主复现的硬通货。

## 6. 近期落点(优先级序)

1. **std/crypto SHA-256 首件**:§12 硬前置,RFC 6234 向量现成,纯整数域(不依赖 C2)——最大确定性交付。
2. **parity 矩阵腿进 smoke**(§5.1 最小落地)。
3. **time Clock 面**(内建已有,stdlib 包装 + 单调语义)。
4. **`ctron vendor` 原型**(§5.4)。

## 7. 决议请求

- 范围裁定(§3)与明确不做清单(§3.4)是否核准为 std 泳道宪章?
- 组织形态宪章 v2(§3.5 域目录收录)是否随下一版 README 修订?
- 五项创新的落地序是否按 §6?(1、2 可并行,3、4 随泳道间隙)
