<!-- 站点同步件:源头 docs/spec/,勿直接编辑;漂移由 pages workflow --check 把关 -->
<!-- 英文待翻:中文占位 -->
# §2 名字与模块

## 2.1 结构模型

- **包(package)**:分发与版本单位,根清单为 `Ctron.toml`(→ 迁移至 **CTCL** `Ctron.ctcl`,见下 2.7 修订注);包名 = 清单 `name`,全小写。
- **模块(module)**:一个 `.ct` 文件 = 一个模块;**目录 = 命名空间模块**,与文件树一一对应。模块路径 = 包内相对路径。
- 符号的完整路径形如 `包名.模块路径.符号`——物理可 grep(P8)。

## 2.2 导入

- `use 包名.模块.符号;` 显式具名导入;**禁止通配导入**(`use m.*`)与**禁止重导出**(模块不得把导入的符号再 `pub`)。
- 组导入:`use std.net.{TcpListener, Request}`(可尾逗号)。
- 路径一律**从包根起**(Go 式全限定),无相对导入、无 `super/self` 路径模块。`self` 仅用于 impl 内类型指代(§1.7)。

## 2.3 可见性

| 级别 | 语法 | 可见范围 |
|---|---|---|
| 模块私有(默认) | (无标记) | 仅本模块文件 |
| 包内 | `pub(pkg)` | 同包所有模块 |
| 公开 | `pub` | 任何导入方 |

- 适用于:类型、字段、函数、常量、静态、trait 项。
- 字段可见性独立于类型可见性;未 `pub` 的字段在包外不可读写(结构化构造字面量同样受限)。

## 2.4 名字解析

- 解析顺序(作用域链):局部块 → 模块顶层 → `use` 导入集 → 前奏(§3.8)。
- **遮蔽允许**:同块内后声明的 `let/var` 遮蔽外层同名绑定;同一块内禁止重复声明同名。
- 未解析名 → E2020。

## 2.5 trait 孤儿规则(coherence)

- `impl T for X` 合法**当且仅当** trait `T` 或类型 `X` 至少一个定义于当前包;无例外、无泛型参数豁免。违反 → E5010。
- 推导:前奏类型的能力缺口必须在包内新类型上解决(新类型模式),或等待 stdlib 演进。

## 2.6 循环依赖

- **包间与模块间循环依赖均禁止**(E5020)。这是编译速度否决权(P4)的语言级保证:解析与检查可单遍、增量缓存可按模块失效。

## 2.7 包元数据(`Ctron.toml`)

> **修订注(2026-09-16,提案待评审,非语言修订)**:清单格式由 TOML 方言(`Ctron.toml`)迁往 **CTCL(Ctron Config Language,`Ctron.ctcl`)**。本节下方 TOML 示例仅作历史记录;规范性定义以 [`docs/superpowers/specs/2026-09-16-config-language-v1.md`](https://github.com/Zturn/Ctron/blob/main/docs/superpowers/specs/2026-09-16-config-language-v1.md) 为准(含块式文法、fail-closed 注册表、`caps = ["fs"]` 列表形、deps 三互斥形、三线解析器契约与迁移计划)。迁移落地前,现行三线解析器与在库 `Ctron.toml` 保持原状。

```toml
[package]
name    = "myapp"
version = "0.1.0"

[deps]
ctron-http = "1.2"          # 严格 semver;lockfile 固定

[caps]                      # 能力声明(§8.2):越权使用 = 编译错误
fs.read = true
net.listen = true

[profile]                   # 档位与目标(§9)
default = "full"
```

- 依赖解析:严格 semver + lockfile(内容寻址);工作区 workspace 支持。
- 能力声明是包级**上限**:程序实际使用的能力集 ⊆ 声明集,超出 → E4010。

## 2.8 与测试集的对应

孤儿/循环依赖的可执行反例属多文件用例,P1 起由 `tests/modules/` 承载(测试格式已定义于 `tests/README.md`)。
