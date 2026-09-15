# 标准库范围边界政策(v1)

日期:2026-09-15
上游:2026-09-14-stdlib-domains-plan.md、2026-09-15-stdlib-gap-analysis.md
问题:网络库、文件系统等相关部分是否属于标准库?标准库的范围划分到哪里?
状态:政策(对 spec §9.1 三档分层给出满配形态与判定准则)

---

## 1. 结论

1. **文件系统在标准库内**,且随运行时成熟持续深化(整文件 → 句柄/缓冲/元数据/目录树)。四家对标语言(Go/Rust/Python/Zig)无反例。
2. **网络库不在 v1 标准库**,形态为"一等第三方模块"(按需 vendored,P2-B 包管理后成型)。这是依赖链的必然结论,非保守口味:**std http 的先决条件是 std TLS,std TLS 的先决条件是 std 加密基线(AES-GCM/SHA-256/曲线)——三层均不具备且 v1 无运维承诺**。对标先例分裂:Go/Zig 背靠大厂/自举消费故 http 入 std;Rust std 仅裸 socket,http 归 crates——Ctron 处境与 Rust 同构。
3. 标准库范围 = **core < alloc < std | stdweb 四层**(spec §9.1 的满配形态,见 §3);"网络/加密/数据库/UI/正则"等均在 std 之外(§5)。

## 2. 判定准则(进 std 的五条,按序裁决)

1. **语言承诺配套**:语言规范引用的类型/机制(Option/Result/迭代器/fmt/test 块)必须在 std——不是选择,是规范义务;
2. **自举第一用户**:编译器/工具链消费的(json/path/str/toml)优先做真——Zig 教训:std 是编译器的代码库;
3. **全平台一致面**:std 只覆盖 full/web/bare 三档语义一致的能力;平台特化(移动/GPU/浏览器深集成)走 stdweb/平台包;
4. **安全基线**:安全默认值进 std(SipHash 防泛洪、常量时间比较、恒定种子策略),加密算法族整体不进(依赖链+审计成本);
5. **退出成本**:进 std = 冻结风险(Go 1 教训)。提级 stable 的准入 = 两个真实消费方 + 一次更名预演 + 三线门禁连续绿。

裁决顺序:5 反证 → 4 硬拦 → 3/2 正向 → 1 义务。

## 3. 四层满配形态(v1 目标)

### core(bare 可用;无分配/无 OS)
Option/Result/迭代器 trait/str 基础(ASCII+解码面)/math 基础与常量/hash 基础(djb2/fnv)/bit/无分配容器(List 薄面)/元组与比较助手。

### alloc(需要分配)
容器全量(Map/Set/FMap/SipHash 化 HashMap/heap/deque/位集)/StringBuilder/iter 适配器族/函数式持久结构。

### std(OS 面;full/web)
- **fs**:句柄(打开/读/写/seek)、元数据(size/mtime/perm)、目录树(walk/glob)、复制/改名——**含文件系统,深化方向锁定**;API 按 §8.1 改造为 `&Fs` 能力注入形(r2b FakeFs 锚已备);
- **time**:Clock(real/monotonic,§8.1 注入形)/Duration/时刻与 UTC 历法;时区走数据包(D);
- **io**:Reader/Writer trait + 缓冲/行扫(依赖 §9.5 统一层);
- **process**:Env/args/exit/熵源/spawn(B);
- **encoding**:json(含写出/数值类型/流式)/csv/toml(P2-B 配套)/hex/base64/varint/定宽编解码;
- **hash**:SipHash-1-3(HashMap 二代默认)/sha256(C 档复活)/crc32;
- **log**:级别+kv+可注入 sink(§8.1 Log);
- **test**:金样/过滤/属性 lite。

### stdweb(web 专有)
dom/fetch/事件/storage(web 特化不与 std 主线同步节奏)。

### std 之外(一等第三方模块,非 std)
net(tcp/http/.client)、加密全家(aead/kdf/曲线)、regex、数据库驱动、UI、msgpack/yaml/xml、async 运行时、timezone 数据包。

## 4. 提级与降级机制

- **提级**(第三方 → std):满足 §2 五条 + 依赖链闭合(如 net 需先有 TLS 基线)+ 包管理可承载版本化;
- **降级**(std → 第三方):Rust 先例(rand/regex 移出成功)——前置:生态替代存在 + 一个主版本周期的弃用窗;
- 触发复核的时间点:P2-B 包管理落地(第三方成型)、P3(正则/网络复审)、生态出现"事实标准模块"。

## 5. 与对标语言的边界对照

| 面 | Go | Rust | Python | Zig | Ctron v1 |
|---|---|---|---|---|---|
| fs | ✓ std | ✓ std | ✓ std | ✓ std | ✓ std(含) |
| net(TCP) | ✓ std | ✓ std | ✓ std | ✓ std | ✗ 一等模块 |
| http | ✓ std | ✗ crates | ✓ std | ✓ std | ✗ 一等模块(TLS 依赖链) |
| 加密 | ✓ std | ✗ crates | ✓ std | ✓ std | ✗ 一等模块 |
| http 无 TLS 可用 | — | — | — | — | — |

注:Go/Zig 的 http-in-std 以大厂运维/自举消费为前提;Rust 的最小 std 以 crates 生态为前提。Ctron v1 两个前提均无,故边界取"fs 在、net 不在"的自洽切面;包管理+加密基线成熟后,http 是第一个提级候选。

## 6. 对现有 std 的映射动作

1. **分层标注**:std/README 与模块头补 `layer: core|alloc|std` 字段(v0.3 首批任务);
2. **fs 能力化**:fs.ct API 随 r2b 改造为 `&Fs` 注入形,全局内建退为 Fs.system() 的实现细节;
3. **stdweb 隔离**:确认 stdweb 不进 std/ 主线分发(当前 ✓);
4. **边界复核点**:写入 roadmap(P2-B/P3 两个触发点)。
