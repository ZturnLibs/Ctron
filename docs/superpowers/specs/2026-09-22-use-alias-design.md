# use 别名导入 · 设计方案

- 日期:2026-09-22
- 状态:已评审批准(机制方案 A:选择性合并 + 引用闭包 + 合并期重命名)
- 关联:`docs/spec/02-names-modules.md` §2(导入/解析顺序)、`compiler/src/parse_pkg.ct`(模块加载器 v0)、f15554c(use 组导入跨行,本文前置)

## 1. 背景与场景

语言现状:`use` 只有具名与组两种形态(§2.2),符号一律以原名进入导入方命名空间;合并是**整模块扁平并入**,同名 decl 合并期 E5030 拦截,解析链无模块限定调用(`mod.fn()` 不存在)。

别名导入覆盖的常用开发场景:

| # | 场景 | 例 | v1 |
|---|---|---|---|
| S1 | 同名冲突消解 | `use app.a.{open as file_open}` + `use app.b.{open as net_open}` | ✓ |
| S2 | 长名短化 | 12 个 `ctron_tls_*` → `tls_*` | ✓ |
| S3 | FFI/绑定面友好名 | `std/tls/bind.ct` 绑定面落地时 | ✓ |
| S4 | 迁移门面 | `use app.new.{Widget as Panel}` 渐进改名 | ✓ |
| S5 | trait 名撞名 | `use a.{Filter as Scan}` + `use b.{Filter}` | ✓ |
| — | 模块别名 `use m as n` | — | ✗ 登记不排期(前置依赖:模块限定调用语法,§2.4 解析链无此形态;做别名无消费口) |

## 2. 语法(v1)

```ctron
use path.{Sym}                          // 现状:原名导入
use path.{Sym, Sym2 as Alias2,}         // 新:组内混用,可尾逗号(跨行已支持,f15554c)
use path.Sym as Alias                   // 新:单符号形式
```

- `as` 为组项位置/路径末位置的**上下文关键字**:词法不变(仍是 ident);解析器在"组项已读到一个 ident 后"或"路径末段已闭合(`.` 末段即符号)后"识别 `ident as ident` 三元组。`use a.as.d` 不受影响(`as` 作路径段时,其后是 `.` 而非 ident)。
- **不做**(全部解析层拒绝,非静默):
  - `{A as B as C}` 链式 → E1001(`as` 后须 ident,再后须 `,` 或 `}`);
  - 别名到关键字(`as fn`)→ E1001(expect-ident 失败);
  - 通配导入、重导出 → 规范 §2.2 既有禁令,不变;
  - `A as A` 冗余形式 → 合法,语义等价裸导,不做特判;
  - 组项仍为**单段符号**(seed/Rust 现存点分段宽松接受面维持原样,别名项不放宽到点分段)。

## 3. 表示(三宿主)

| 宿主 | 形态 |
|---|---|
| 自举 | `Syms` 节点由扁平符号表改为**交替对** `[orig1, alias1, orig2, alias2, …]`;无别名时 alias 槽 = orig。不变式:偶数长度。`driver_ast.ct` 的 `"Use","NN"` 形态不变(`Syms` 仍是字符串列表节点)→ `.ctast` roundtrip 免改 |
| seed(C) | `cimport` 增 `alias` 字段(空串 = 无别名,消费侧规范化为末段名);`ast_show.c` Use 打印追加别名显示 |
| Rust | `ImportItem { segs: Vec<String>, alias: Option<String> }`,`UseDecl.imports: Vec<ImportItem>` |

诊断与加载语义一律以 `(orig, alias)` 对为操作单位;报 miss/priv 时报 **orig**。

## 4. 加载器语义(核心)

落点:自举 `pkg_load_use`(parse_pkg.ct)/ seed `pkg.c` / Rust 对应加载器。三条 use 处理主流程不变(路径展开 → 读文件 → parse → caps 检查),改动从可见性检查起:

1. **可见性检查**:按 orig 查模块 decl,E2020.use.miss / E2020.use.priv 行为与报名不变。
2. **选择性合并**:请求集 R(全部 orig)→ 引用闭包 C:
   - 新引用收集器(三宿主各一,自举侧新 fn 暂名 `pkg_refs_of_decl`):收集 decl 体内**裸 ident**(调用名、类型名、构造器名、const/static 名、trait 名;字段名/方法名不收),与模块 decl 名集合求交,迭代扩张至不动点;
   - 只合并 C ∪ R 的 decl——保证 `via_helper` 内部调私有 `private_helper` 形态继续可用(use_ok 为回归锚)。
3. **合并期重命名**:请求对中 alias ≠ orig 的 decl,以**浅拷贝**并入(新 List 头 `[kind, alias, 子节点引用…]`,子节点共享);闭包拉入的 decl 保持原名并入。
4. **撞名**:
   - E5030(既有,契约不动,smoke 3d- 锚定):裸名/闭包名合并时与本文件已有 decl 或彼此同名;
   - **E5035(新)**:别名与命名空间中任何将并入名冲突(本地 decl、闭包原名、前序别名、同组裸导名)。文本含别名与被撞名。
5. **语义收紧(方案 A 代价,即 §2.2 显式导入本意)**:未导入且未被闭包引用的模块符号不再随整模块搭车进入命名空间。落库前以全量门禁(smoke/suite/示例)做存量审计,红点 = 搭车依赖,补显式 use 或登记。
6. **前奏限制**:`std.time/net/db/fs` 等前奏类走"文件缺失静默跳过"路径(isstd 且 read None),无 decl 可改名 → 带 `as` 请求前奏符号 → 新诊断 **E2020.use.nat**(前奏/native 符号不支持别名),不静默。
7. 其余不变:E5020 循环(含"同模块双 use 行 → E5020 误报"的既有登记)、E4010 caps、E5010 孤儿;`use path.Sym as Alias` 的 mpath 逻辑不动。

## 5. fmt 与投影面

- **fmt 零行为改动**:token 流直过,`Sym as Alias` 空格由现空格表天然产出(ident–ident → 空格);组内无 `.` 不涉 R4 链断行。金样 `fx_fmt_golden.ct` 追加别名形态重冻结,parity 三宿主自动覆盖。
- **`ctc doc`(iface 投影)**:不消费 Use 节点,零影响;闭源工件 `.ast` 消费路径 `ast_load_node` 因 `Syms` 仍是字符串列表而形态不变。
- **`.ctast` roundtrip**:形态 `"NN"` 不变,Syms 内交替对逐字序列化;driver_ast 金样追加别名用例。

## 6. 诊断矩阵

| 码 | 触发 | 宿主 |
|---|---|---|
| E1001 | `as` 后非 ident / 链式 / 别名到关键字 | 三宿主(解析层,既有码) |
| E2020.use.miss / .priv | orig 不存在 / 非 pub(报 orig) | 三宿主(既有码,不变) |
| E2020.use.nat | 前奏符号带别名请求 | 三宿主(新) |
| E5030 | 裸名/闭包名合并撞名(契约不变) | 三宿主(既有码,不变) |
| E5035 | 别名撞命名空间已占名 | 三宿主(新) |

## 7. 测试计划

- `tests/modules/use_alias_basic/`(P1a):S2/S3/S4 形态——组内混用、单符号别名、`via_helper` 私有 helper 闭包回归;boot run + 宿主 pkg 双绿。
- `tests/modules/use_alias_conflict/`(P1b):S1/S5——两模块同名 open 各别名;E5035 负例(别名撞本地 decl);E2020.use.nat 负例。
- fmt:金样追加重冻结;parity.sh 全量 0 分歧。
- 门禁:suite.py(自举 73/73 持平 + modules 新用例)、smoke.sh --full、`.ctast` roundtrip 自验;存量搭车依赖审计 = 门禁全量跑(P1b)。
- 宿主 pkg neg 面:seed 同步实现 E5035/E2020.use.nat,modules neg 可断言宿主诊断码。

## 8. 分期

- **P1a**:三宿主解析 + 表示(交替对/alias 字段)+ E5035 + 合并期改名(**保持整模块合并**)——S2/S3/S4 先通,fmt 金样、use_alias_basic、.ctast 自验随片落库。
- **P1b**:选择性合并 + 引用闭包 + 收紧审计——补齐 S1/S5,use_alias_conflict、E2020.use.nat 随片落库。
- **P2(登记不排期)**:模块别名 `use m as n`(前置:模块限定调用语法,§2 扩展提案);前奏符号别名(若未来前奏类转为真实模块则自然解锁)。

## 9. 开放问题(登记,不阻塞 P1)

- 同一 orig 既裸导又别名导(`{A, A as B}`):v1 允许(两次浅拷贝、两个绑定同一实体);若未来 trait/impl 分派出现二义再收紧。
- 闭包拉入的原名 decl 对导入方可见(可被直接调用):与现状"搭车可见"同级的温和泄漏,v1 接受;收紧需命名空间分层,超纲。
- 诊断 i18n:diags 文本走 diag_msg.ct 既有 zh/en 面,E5035/E2020.use.nat 双语补齐。
