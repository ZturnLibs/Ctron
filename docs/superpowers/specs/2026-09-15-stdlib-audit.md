# std v0.2 已完成模块审计:遗漏 / 增强 / 创新

日期:2026-09-15
对象:std 17 模块(基线 802465a),对标 Go 1.22 / Rust 1.78 / Python 3.12 / Zig 0.13 的**同域 API 面**
方法:逐模块三分法——**遗漏**(同域内成熟语言有而 Ctron 无)、**增强**(已有但质量/安全/性能可升)、**创新**(Ctron 有或可独有的差异化能力)
上游:2026-09-15-stdlib-gap-analysis.md(域级覆盖)、2026-09-15-stdlib-scope-boundary.md(边界政策)

---

## 1. 遗漏清单(均已实测核实)

### 高严重(破坏域内完备性)

| # | 模块 | 遗漏 | 对标 |
|---|---|---|---|
| O1 | map | **无 `map_len`、无 `map_del`**——连大小都查不到、键不可删除(Map 只有 new/put/get/has/keys/values 六件) | Go `len/delete`、Python `len/del` |
| O2 | set | **无集合代数**:union/intersection/difference/subset,亦无 del——Set 只是"去重 List" | Python set 全套运算 |
| O3 | json | **无写出路径**(esc 仅转义字符串;对象/数组/标量→JSON 文本缺失)、**无成员枚举**(qkeys/qchildren:查得到 `$.a` 的值,枚举不了对象有哪些键)——"只读了一半" | Go json.Marshal、Python json.dumps+keys() |
| O4 | fmap | 无 `fdel` | Go delete |
| O5 | heap | 无 `heap_from`(O(n) 建堆)——逐 push 为 O(n²) 功能等效 | Rust `from Vec`/heapify |

### 中严重

| # | 模块 | 遗漏 |
|---|---|---|
| O6 | str | trim_left/trim_right(单侧)、partition(三段切分)、split_n(限段)、replace_n(限次)、rindex(反向查找)、is_ascii_upper/lower/xdigit |
| O7 | strconv | format_hex/bin(已实现待复位,受 C10 旧账)、带前缀整数解析(0x/0b/0o) |
| O8 | path | path_rel(两路径间相对化)、glob-lite(fnmatch 子集) |
| O9 | csv | header→列名访问(按列名取值而非下标) |
| O10 | time | epoch 秒域换算(P1-B 后 I64 可承载,**现在即可做**)、带时刻的 ISO 解析 |
| O11 | unicode | utf8_from_cp(码点→UTF-8 文本;受 C8 字符串构建限制,随 P1-B 面

复验) |

### 遗漏的共性根因
v0.2 各模块按"最小可用面"收录,缺的是**完备性收口**(删除/代数/写出/反向)——正是 Go/Rust/Python 在 1.0 后补课的部分;趁 v0.3 一次收掉,成本远低于事后。

---

## 2. 增强清单(已有能力的质量升级)

| # | 模块 | 增强 | 级别 |
|---|---|---|---|
| E1 | **fmap/map** | **无键 djb2 → SipHash-1-3 + 随机种子**:现行哈希可被构造同桶键泛洪(DoS)。Go/Python 均因该攻击全线换 SipHash——**安全级增强,建议最优先** | 安全 |
| E2 | sort | 插入排序 O(n²) → 归并(仍稳定,API 与语义逐字兼容,n>1k 场景解锁) | 性能 |
| E3 | heap | 逐 push(O(n²) 等效)→ from_list heapify O(n) | 性能 |
| E4 | str/fmap/json | 测试与实现的串拼接 O(n²) → StringBuilder 内部化(发射面同步) | 性能 |
| E5 | str | index_of 朴素 O(nm) → BMH(n 数据规模解锁) | 性能 |
| E6 | rand | MINSTD(周期 2^31−1)→ PCG32/xoshiro | 质量 |
| E7 | fs | API 能力化(`&Fs` 注入,§8.1;全局内建退为 Fs.system() 细节) | 架构 |
| E8 | unicode | cp_iter 列表分配 → 迭代器形态(随 r3b) | 性能 |

---

## 3. 创新盘点(已有)与可做创新

### 3.1 已有的差异化能力(四家对标语言没有或不保证)

| # | 创新 | 说明 |
|---|---|---|
| I1 | **JSON 路径展平 DOM**(parse/qtag/qval) | 成熟语言给树,必须遍历;Ctron 给"路径→值"的 KV 投影——**对 agent/工具链按路径取值零遍历**,是 v0.2 最具差异化的设计 |
| I2 | **确定性即契约** | MINSTD 可播种/排序稳定/哈希算法钉死/无隐藏时钟(宪章 8)——同输入同输出跨运行复现;四家无一保证(Python hash 随机化、Go map 遍历随机) |
| I3 | **能力注入测试形状** | FakeFs/FakeClock/FakeEnv 作为 API 设计而非测试技巧(§8.1) |
| I4 | **test 块即合同 + 三线逐字 + 自举固定点** | std 质量门是字节级的,不依赖人的自觉 |
| I5 | **约束登记表 C1–C16** | 把解释器现实当作一等文档化契约(罕见做法,对 AI 协作开发尤为有效) |
| I6 | **sorted_by_keys 平行键排序** | 规避闭包 ABI 的约束催生型 API——按平行键表排序,调用方免写比较器 |
| I7 | **函数式不可变 Map/Set 为默认** | 共享可变状态的缺位由语言级容器形态对冲(§7 Send 纪律的容器侧表达) |

### 3.2 可做的创新方向(成熟语言 std 均无)

| # | 方向 | 说明 |
|---|---|---|
| F1 | **JSON 扁平 DOM 补全为"可查询可回写 KV"**:qkeys/qchildren(枚举成员)+ qset/qdel(回写)+ 与 parse 对称的 write | 补上 O3 后,I1 从"只读查询模型"升格为完整范式;配合 qset 可做无树遍历的结构化编辑 |
| F2 | **确定性测试包**:固定种子 rand + 金样断言 + 快照比对助手 | 面向 agent 代码生成场景:生成→运行→**字节级比对**;四家的测试框架均无字节级一等支持 |
| F3 | **自证模块**:模块 test 块 + 漂移断言作为 vendored 分发的完整性自证 | 分发即验证,收货方一条命令自检(宪章 6 的分发面推广) |
| F4 | **截断 JSON 容错解析**:对 LLM 输出的常见残缺(截断/尾逗号/单引号)给出修复式解析与差异报告 | 场景化创新:四家解析器都以"严格拒绝"为正确;agent 时代"尽力恢复+报告"是真实需求,与 json.ct 的严格主解析并存(独立入口,不污染 RFC 口径) |
| F5 | **能力注入 API 全面化**:所有 IO/时钟面函数提供 `&Fs`/`&Clock` 变体 | 测试即 API(§8.1 的 std 全面兑现) |

---

## 4. 优先级建议(并入 v0.3)

**P1(完备性 + 安全)**
1. O1 map_len/map_del、O2 集合代数 + set_del、O4 fdel(容器完备性,纯 C,各 0.5 天)
2. O3 json 写出 + qkeys/qchildren(解锁 F1)
3. E1 SipHash-1-3 + 种子化(安全)

**P2(质量与常用面)**
4. O6 str 六件、O10 time epoch 秒域(I64 已承载)、O9 csv header 访问
5. E2 归并排序、E3 heapify(from)、E4 StringBuilder 内部化
6. F2 确定性测试包

**P3(随平台/语言解锁)**
7. O7 format 族(P1-B 复验)、O11 utf8_from_cp(C8 复验)、O8 glob、E5 BMH、E6 PCG32、E7 fs 能力化(随 r2b)、E8(随 r3b)、F4 容错解析、F5 全面化

---

## 5. 自查

- 遗漏均经 grep 实测(非印象):map 六件/set 五件/fmap 九件/json 四件/heap 无 from/str 无单侧 trim,见 §1 对照。
- 增强不改变 API 与语义(E2 稳定性保持、E1 仅哈希内部),均为纯实现升级。
- 创新盘点区分"已有"(I 组,可在文档/发布说明主张)与"可做"(F 组,待立项),避免把愿景当现状。
