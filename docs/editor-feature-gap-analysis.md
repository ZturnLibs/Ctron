# Ctron 插件功能差距分析 —— 对标主流语言插件(2026-09-05)

> 范围:`editors/vscode-ctron` + `lsp/src/main.ct` 当前实现 vs 参照系。
> 结论先行:编辑器壳层已达同类水位;差距集中在**语义层**(类型感知补全/推断/引用)与**工具链层**(格式化/测试集成/重构)。
> 三条能力升级路线见 §4,优先级矩阵见 §5。

## 1. 参照系选取(三档)

| 档 | 插件 | 参照意义 |
|---|---|---|
| 金标准 | **rust-analyzer** | 现代语言插件功能天花板;编译器驱动 LSP 的范本 |
| 金标准 | **gopls + Go 插件** | 工程化维度最好(vet/测试集成/工作区);"够用且稳定"哲学 |
| 金标准 | **clangd** | C 系参照;跨文件索引/语义高亮/内存布局 hover |
| 同体量 | **zls (Zig)** / **ols (Odin)** / **nimsuggest** | 与 Ctron 同阶段的小语言:单人/小团队 LSP,看它们"先做什么后做什么"的取舍 |
| 上限参照 | **tsserver / Pyright** | 快速迭代语言的纯 LSP 架构与 quick-fix 丰富度 |

zls/ols 的共同教训:早期把力气花在**格式化、跳转、补全、诊断**四件套,重构/层级/调用图都靠后——Ctron 应循此序。

## 2. 功能矩阵(现状对照)

图例:✅ 已有 | 🟡 有但不完整/启发式 | ❌ 无 | ⛔ 被前置依赖阻塞

### 2.1 编辑器壳层(TextMate/配置层)

| 功能 | rust-analyzer | zls | **ctron 现状** |
|---|---|---|---|
| 语法高亮 | 🟡(大量让位 semantic tokens) | ✅ | ✅ 含插值/后缀/非法标点标红,高于同体量平均 |
| 语义高亮 semanticTokens | ✅ | ✅ | ✅ 轻启发式(声明/调用/成员/类型/宏;类型感知待 sem) |
| 缩进/注释续行/自动闭合 | ✅ | ✅ | ✅(§1.6 延续集,已是 Ctron 特化) |
| 代码片段 | 少(rust 用 proc-macro) | ✅ | ✅ 22 条 |
| **格式化** | ✅ rustfmt | ✅ zig fmt | 🟡 缩进级已交付(format-on-save 可用);canonical 待 ctron-fmt |
| 折叠 | ✅(语法级) | ✅ | ✅ brace 级 foldingRange |
| 文件图标/语言状态栏 | ✅ | 🟡 | ❌(低成本,可随手补) |

### 2.2 诊断与快速修复

| 功能 | rust-analyzer | gopls | **ctron 现状** |
|---|---|---|---|
| 词法/语法诊断 | ✅ | ✅ | 🟡 仅词法 E1001 族;**真解析器未接入**(selfhosted 的 parsetree.ct 可复用,§4-B) |
| 语义诊断(E2xxx 类型/match 穷尽、E3xxx Send/alloc) | ✅ | ✅ | ✅ 保存时 `ctronc check --format=json` 包装 |
| Lint(未使用/遮蔽前奏) | ✅ | ✅(vet) | ❌ 同上 |
| **Quick fixes(机器可执行修复)** | ✅ 丰富 | ✅ | 🟡 词法确定性修复(`;` 删除/`::`→`.`)+ fixes[] 通道已接(C 侧尚未产出 fixes) |
| 诊断错误码悬停文档 | ❌(rust 用标签) | 🟡 | ✅ 已带 code(可再加 §10.1 链接,差异化小甜点) |

### 2.3 导航与符号

| 功能 | rust-analyzer | zls | **ctron 现状** |
|---|---|---|---|
| 大纲 documentSymbol | ✅ | ✅ | ✅(顶层声明,88 符号自解析验证) |
| 定义跳转 | ✅(跨工作区) | ✅ | 🟡 仅同文件 |
| **引用查找 references** | ✅ | ✅ | ❌(同文件版成本低:标识符扫描,§5-P1) |
| 工作区符号搜索 workspace/symbol | ✅ | ✅ | ❌ ⛔ 需跨文件索引(§4-C) |
| document highlight / selectionRange | ✅ | ✅ | ❌(同文件版成本低) |
| **重命名 rename** | ✅(工作区) | ✅ | ❌(同文件版 = 声明+标识符扫描,§5-P1) |
| 实现/类型定义跳转(go-to-impl) | ✅(口碑功能) | ✅ | ❌ ⛔ 需 impl↔trait 映射(sem) |
| 调用层级 call hierarchy | ✅ | ❌ | ❌(远期) |

### 2.4 补全与提示

| 功能 | rust-analyzer | zls | **ctron 现状** |
|---|---|---|---|
| 关键字/类型/文档内符号补全 | ✅ | ✅ | ✅ |
| **类型感知成员补全**(`xs.` 只出 List 成员) | ✅ | ✅ | 🟡 静态成员全集,不区分接收者类型 ⛔ 需推断 |
| UFCS 补全(`x.foo` 候选自由函数) | ✅(方法即 UFCS) | ❌ | ❌ ⛔ 需值类型 |
| **签名帮助 signatureHelp** | ✅ | ✅ | ❌(同文件版便宜:调用点回溯声明,§5-P1) |
| 自动 import(use 组插入) | ✅ | ✅ | ❌ ⛔ 需模块系统(C8) |
| 补全项携带文档/占位片段 | ✅ | 🟡 | 🟡 有 detail,无 documentation/insertText 片段化 |
| postfix 补全(`x.match` 展开模板) | ✅(招牌) | ❌ | ❌(P3 锦上添花) |

### 2.5 推断与内联提示

| 功能 | rust-analyzer | zls | **ctron 现状** |
|---|---|---|---|
| hover 显示推断类型 | ✅(招牌) | ✅ | ❌ ⛔ 需 sem/推断 |
| **inlay hints**(let 类型 / 调用点形参名) | ✅ | ✅ | ❌(形参名提示可先行,§5-P2) |
| 递归/闭包捕获提示 | ✅ | ❌ | ❌(远期;Ctron 可做 Send 捕获提示,差异化) |

### 2.6 工具链集成(gopls 最强维度)

| 功能 | gopls/Go | rust-analyzer | **ctron 现状** |
|---|---|---|---|
| **测试 CodeLens / Test Explorer** | ✅ | ✅ | ❌ ⛔ 需 `ctronc test --filter` CLI(先决) |
| doc-test 集成(§10.4) | ❌(Go 有 example) | ✅(rust doc-test) | ❌ ⛔ 同上;**Ctron 规范已内置,做了即是卖点** |
| AST/结构视图 | ✅(rust: SSA/IR 视图) | ✅ | ❌ C-AST v1 Webview 视图(服务自举差分,§4-A3) |
| 调试器(DAP) | ✅ delve | ✅ lldb | ❌(远期;解释器级 stepping 另议) |
| Ctron.toml 校验/补全 | ✅ go.mod | ✅ Cargo.toml | ❌(json.schema 贡献即可起步) |
| 项目脚手架命令 | ✅ | ✅ | ❌(低成本) |

## 3. 与同体量插件比,当前的真实位置

- **壳层(高亮/缩进/片段)**:不落后,zls/ols 起步也这一套;插值高亮与非法标点标红已超出平均。
- **LSP 四件套(诊断/补全/跳转/hover)**:都有了"形",缺"神"——诊断只有词法级、补全无类型、跳转不跨文件。zls 用编译器全量接入把这四件做深,Ctron 当前解释器单文件架构是天花板。
- **完全没有而竞品普遍有的**:格式化、rename、signatureHelp、references、quick fixes、测试集成——共 6 项,是"用起来像不像正经语言"的分水岭。

## 4. 三条能力升级路线(做什么取决于怎么接)

### 路线 A:包装既有 CLI(数天级,解冻大量 ⛔)
`ctronc` 已有的能力经 LSP 包装即可变现,无需新语言实现:
1. **`ctron-fmt` 先于一切格式化需求**:规范 §1.6 已定义唯一形态(首点式链、else 同行);在 `parsetree.ct`(自举树,已 159 用例逐字节)上加打印器即是 fmt——格式化/rangeFormatting/format-on-save 一次到位;
2. **`ctronc check --format=json` 保存时包装**:诊断从词法级跃升为全编译器级(E2010…E6030 + lints),且 §10.2 的 `fixes[]` 直接映射 CodeAction quick fixes——**契约本来就是为这个设计的**;
3. `ctronc parse --ast` → Webview AST 视图(服务自举差分,编辑器用户与编译器开发者双赢)。

### 路线 B:深化 Ctron 实现的 LSP(数周级,渐进)
1. 接入 selfhosted 真解析器(parsetree.ct 已全语料差分):精确到节点的诊断区间、括号配平、foldingRange、selectionRange;
2. 同文件引用/重命名/highlight:声明表 + 标识符出现扫描(现有 find_decl 扩展即可);
3. signatureHelp:调用点向前扫描到 `(`,回溯声明签名,含形参名;
4. 语义 tokens:标识符分类(形参/字段/变体/内建)需要"轻命名解析",是走向推断的中间站;
5. inlay hints 先行版:调用点形参名(不需要推断);let 类型提示等推断就绪。

### 路线 C:跨文件与类型感知(依赖编译器里程碑)
1. 工作区索引(read_file 已有;或 `ctronc` 提供 index 子命令)→ workspace/symbol、跨文件 def/refs、自动 use;
2. 类型感知补全 / hover 类型 / go-to-impl → 需要 sem 以库形态进 LSP;**B 泳道 C10-a 转译后端落地后,LSP 可转译为原生二进制,性能与嵌入同时解决**——这是与 C10 路线最大的协同点。

## 5. 优先级矩阵(性价比 × 依赖)

| 序 | 功能 | 路线 | 依赖 | 量级 |
|---|---|---|---|---|
| P1 ✅ | 同文件 rename / references / document highlight | B | 无 | 已交付(2026-09-05) |
| P1 ✅ | signatureHelp(同文件) | B | 无 | 已交付(2026-09-05) |
| P1 ✅ | **`ctronc check --format=json` 诊断+quick fixes 包装** | A | 无(契约冻结) | 已交付:保存时语义诊断 + fixes[] 映射 + 词法确定性修复(`;` 删除/`::`→`.`) |
| P2 🟡 | **格式化**:缩进级已交付;canonical 形态 | A | parsetree.ct 打印器 | 余 2~3 天 |
| P2 🟡 | 真解析器接入(精确诊断区间/folding/selectionRange) | B | selfhosted 模块合并 | folding 已交付;解析器接入待做 |
| P2 ✅ | inlay hints(调用点形参名) | B | 无 | 已交付(2026-09-05;>24KB 跳过) |
| P2 ✅ | semantic tokens(轻启发式) | B | 无 | 已交付(2026-09-05;类型感知版待 sem) |
| P2 | `ctronc test --filter` CLI → 测试 CodeLens/Test Explorer | A | CLI 新子命令 | 2~4 天 |
| P3 | 工作区索引 → 跨文件 def/refs/workspace symbol/自动 use | C | index 或多文件 run | 1~2 周 |
| P3 | 类型感知补全/hover 推断类型/go-to-impl | C | sem 进 LSP(或 C10 转译) | 随编译器 |
| P3 | AST 视图 / Ctron.toml schema / 脚手架命令 | A | 无 | 各 0.5~1 天 |
| 远期 | postfix 补全、extract/inline 重构族、调用层级、DAP | C | 上述基建 | — |

**差异化卖点(Ctron 独有,竞品没有的)**:Send/alloc 效果随 hover 展示(规范 P5 承诺)、能力 manifest 越界诊断联动、doc-test CodeLens(§10.4 文档即回归)、own 块分配域着色、C-AST 视图。建议随对应基建顺路实现,不单开战线。

## 6. 结论

- 当前插件 = "壳层完备 + LSP 四件套有形无神"。同体量语言(zls/ols)证明:**格式化 + rename + 全量诊断**三项补齐后即可称为"正经语言体验";
- 最优路径不是在解释器 LSP 里重写语义,而是 **A 路线包装 CLI(契约已冻结)先解冻 6 项缺失中的 3 项**,B 路线只做无依赖的同文件能力;
- **C10-a 转译后端是 LSP 的终局解**(原生二进制 + sem 可用),建议 B 泳道交付后即启动 LSP 转译迁移,避免在解释器架构上过度投入。
