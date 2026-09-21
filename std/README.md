# Ctron 标准库(std)

标准库是 **Ctron 源码形态分发**的模块集合:程序经 `use std.<模块>.{...}` 消费,
分发方式为将所需模块源码随程序携带(vendored)。本目录是标准库的**规范源**——
任何修改先改这里,再同步到消费方副本。

## 模块清单

| 模块 | 内容 | 依赖 | since |
|---|---|---|---|
| `str.ct` | 字符串工具:words/contains/lines + v0.2 扩张(index_of/split_str/strip_*/count_sub 等) | 无 | 0.1 |
| `sort.ct` | 排序:sorted/sorted_desc/reversed + v0.2 扩张(sort_by/binary_search_by,比较器注入) | 无 | 0.1 |
| `map.ct` | Map[K, V]:函数式不可变映射(双 List 平行槽) | 无 | 0.1 |
| `set.ct` | Set[T]:函数式不可变集合 | 无 | 0.1 |
| `fmap.ct` | FMap[V]:Str 键哈希映射(djb2 mod 质数,开放寻址) | 无 | 0.1 |
| `fs.ct` | 文件系统便利层:read_or/exists 等(内建 fs_* 之上;r* 前缀) | 无 | 0.1 |
| `json.ct` | JSON 解析(路径展平 DOM)+序列化转义(RFC 8259 ABNF 严格) | 无 | 0.1 |
| `path.ct` | 路径纯函数:join/dir/base/ext/normalize(Unix `/` 口径) | 无 | 0.2 |
| `enc.ct` | 编码:hex/base64(RFC 4648)/percent;解码输出限可打印 ASCII(C8) | 无 | 0.2 |
| `hash.ct` | 确定性 32 位哈希:djb2/fnv1a32(算法钉死;crc32 待 v0.3) | 无 | 0.2 |
| `rand.ct` | 确定性 PRNG:MINSTD/Park–Miller(Schrage;I64 承载,C10) | 无 | 0.2 |
| `strconv.ct` | parse_i64/parse_bool(format_hex/bin 待 P1-B,C10 死区) | 无 | 0.2 |
| `time.ct` | 纯历法:days 域 civil↔date/weekday(Hinnant 算法;Clock 待 v0.3) | 无 | 0.2 |
| `csv.ct` | CSV(RFC 4180 子集;引号转义/CRLF;write∘parse=id) | 无 | 0.2 |
| `unicode.ct` | UTF-8 解码面:cp_at/valid/iter/count(字素/宽度表显式非目标) | 无 | 0.2 |
| `opt.ct` | Option/Result 组合子:and_then/or_else/map2/to_result | 无 | 0.2 |
| `heap.ct` | 二叉堆:比较器注入小顶堆(push/pop/peek/sorted;函数式) | 无 | 0.2 |
| `math.ct` | 数学助手(整数域):clamp/iabs/pow2 系 + v0.4/v0.5 扩张(max/min/clamp64/64 位界端 + sat_* 全集/sat_abs/abs_diff/div_ceil/div_floor) | 无 | 0.3 |

## 组织宪章

1. **一文件一模块**:文件名 = 模块名(小写单词,无连字符);`use std.<文件名>.{...}` 消费。
2. **模块独立**:std 模块之间不相互 `use`——任何子集可单独 vendored,不缺依赖。
3. **函数式值传递**:容器 API 不可变更新(put 返回新映射,原值不变);无全局状态。
4. **命名消歧**:同文件内跨类型同名函数用类型前缀(FMap 族 `f*`);导出函数用
   `pub`,内部助手不带 `pub` 且以下划线开头可读性更好时允许普通名。
5. **文档头**:每模块首行 `// std/<名>.ct —— 用途;口径(字节级/稳定性/复杂度)`。
6. **测试随模块**:每模块含 `test` 块(§4.10),`ctron-cc run std/<名>.ct` 独立可跑,
   失败 rc=1;消费面测试在 `compiler/test/stdpkg/src/main.ct`(use 全量消费)。
7. **字节级纪律**:Str 按字节处理(UTF-8 透传,不做码点拆分),与 wc 等工具口径一致。
8. **无副作用**:std 不做 IO(除 `fs.ct`/`time.ct` 的 Clock 面)、不依赖真实时钟;排序稳定、哈希确定性。
9. **实现约束 C1–C12**(v0.2 API 规范 §0 登记):禁 as[]/F64 消费面/顶层 `||`/负值宽算术等解释面缺口纪律,详见 `docs/superpowers/specs/2026-09-14-stdlib-v02-api-spec.md`。

## 分发与同步

- **规范源**:本目录。`compiler/test/stdpkg/std/` 是**同步副本**(测试种子),
  smoke 含逐字节漂移断言——修改 std/ 后必须同步副本,否则门禁红。
- **示例**:examples/* 的 `std/` 副本是**钉定快照**(按需子集),允许落后;
  升级示例属示例维护,不强制同步。
- `config.ct`(2026-09-17):CTCL 清单校验内核(`config_diags`),黄金对拍第四线内核同源(selfhosted/ctcl_chk.ct 为驱动镜像,两处须同步修改)。
- 未来包管理器(CTCL 清单语义落地后,见 docs/superpowers/specs/2026-09-16-config-language-v1.md)以本目录为上游注册表形态。

## 演进纪律

- 新模块:按组织宪章写 → 模块 test 块独立跑绿 → 加入本清单与种子同步 →
  消费面用例进 stdpkg/src/main.ct → smoke 全绿 → 单独提交。
- 修改既有模块:同步种子副本 + 消费面回归 + 示例按需升级。
- 语言能力依赖(如方法/泛型/闭包新特性)在三线(自举/C 宿主/R)落地后才可使用。
