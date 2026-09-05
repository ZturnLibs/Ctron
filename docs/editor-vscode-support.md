# Ctron 编辑器支持分析:VSCode 语法高亮、智能提示与工具链集成

状态:分析提案(v0.1,2026-09-05)
目标:以最小成本让 `.ct` 文件获得一等开发体验——高亮、补全、诊断、跳转、重构。

---

## 0. 资产盘点(可复用的既有设施)

| 资产 | 位置 | 对编辑器支持的价值 |
|---|---|---|
| 规范 §1 词法/EBNF 语法 | `docs/spec/01-lexical-grammar.md` | TextMate grammar 与 tree-sitter 的**唯一权威依据**(关键字表、运算符表、换行规则) |
| Rust 编译器 lib 接口 | `compiler/src/lib.rs`:`parse_src` / `check_src` / `check_package` | LSP server 可**直接嵌入**,无需子进程 |
| C 编译器 CLI | `compiler_c`:`ctronc lex/parse/sem` | Phase 1 包装式 LSP 的诊断后端;自举路线的最终宿主 |
| JSON 诊断契约(冻结) | 规范 §10.2:`ctron check --format=json` | 诊断 → LSP publishDiagnostics 的映射**已经标准化**(code/severity/span/notes/fixes) |
| 稳定错误码注册表 | 规范 §10.1(E1001…E6030) | hover/quick-fix 可附错误码文档链接 |
| 前奏 API 最小清单(规范性) | 规范 §3.8.2 | **机器可读的补全数据源**:Option/Result/Str/List/Mutex/Channel/Task/fmt 全部成员签名齐全 |
| 61 文件一致性语料 | `tests/*.ct` | grammar 快照测试与 LSP 集成测试的**现成黄金语料** |
| 设计文档既定承诺 | `docs/superpowers/specs/…language-design.md` | "day-1 生态基建:tree-sitter 语法 + LSP"——本分析即其落地路径 |

结论:**编辑器支持不需要发明任何新信息**,全部输入已在仓库中,核心工作是接线。

---

## 1. 总体架构:三层递进

```
┌─────────────────────────────────────────────────────────┐
│  Layer 3  工具链集成:测试 CodeLens / AST 视图 /        │
│           Ctron.toml 校验 / doc-test / formatter        │
├─────────────────────────────────────────────────────────┤
│  Layer 2  LSP server(ctron-lsp):诊断 / 补全 /        │
│           hover / 定义 / 引用 / 语义 tokens / inlay     │
├─────────────────────────────────────────────────────────┤
│  Layer 1  VSCode 声明式贡献:TextMate 高亮 /           │
│           language-configuration / snippets / 文件关联  │
└─────────────────────────────────────────────────────────┘
```

关键决策:Layer 1 是**纯 JSON、零依赖、1~2 天可交付**的,应立即做;
Layer 2 分四步走(包装 CLI → Rust 嵌入 → 语义增强 → 自宿主),与编译器路线对齐。

```
editors/
  vscode-ctron/                 # VSCode 扩展(TypeScript 极薄壳)
    package.json                # languages / grammars / configuration / snippets / commands
    language-configuration.json
    syntaxes/ctron.tmLanguage.json
    snippets/ctron.json
    src/extension.ts            # Layer≥1 时:启动 LSP client(vscode-languageclient)
  ctron-lsp/                    # Rust LSP server(compiler/ 的 workspace 成员)
    src/main.rs                 # tower-lsp / lsp-server,stdio
    src/completion.rs           # 含由 §3.8.2 生成的 prelude 数据
  xtask/ (可选)                 # 由 spec/编译器生成 prelude.json、错误码文档
```

---

## 2. Layer 1:声明式贡献(Phase 0)

### 2.1 TextMate grammar(`syntaxes/ctron.tmLanguage.json`)

以规范 §1.3–§1.5 为准,scope 设计:

| 元素 | 规则要点 | scope |
|---|---|---|
| 关键字 | §1.3 清单 30 个:`fn let var const static comptime if else match while for in return struct class enum trait impl own scope test use pub extern prop true false void self` | `keyword.control` / `storage.type` / `storage.modifier` |
| 预留字 | `break continue do async await interface module`(当前非法)与 `or` | `invalid.illegal`(即时反馈!)+ `keyword.operator` |
| 注释 | 只有行注释;`///` 为文档注释 | `comment.line` / `comment.line.documentation` |
| 字符串 | 双引号;转义 `\n \t \r \\ \" \0 \{ \u{HEX}`;**插值 `{expr}`**;`\{` 转义 | `string.quoted.double.ctron` + 嵌套插值 context |
| 数字 | `0x/0o/0b`、`_` 分隔、12 个整型后缀 + `f32/f64` | `constant.numeric.*` |
| 类型 | 内建 `I32 U64 Str …`;前奏 `Option Result List Box …`;PascalCase 启发式 | `support.type` / `entity.name.type` |
| 声明名 | `fn` 后 IDENT、`struct/class/enum/trait` 后 IDENT | `entity.name.function` / `entity.name.type` |
| 属性 | `#[ … ]` 与 `@derive( … )` | `meta.annotation.ctron` + `storage.modifier` |
| 运算符 | `+% -% ..= => -> ? & \|\|` 全集 | `keyword.operator.*` |
| 非法标点 | **`;` 与 `::` 禁用**;`/*` 会被误当块注释 | `invalid.illegal`(Ctron 特有的高价值规则) |
| 泛型 | `List[I32]` 的 `[T]` | `meta.generic.ctron`(启发式) |

三个 Ctron 特有的实现要点:

1. **字符串插值是语法糖核心**:`"hi {name} x{n}"`——在 string context 内用
   `begin: \{`、`end: \}`,内部 `include: '#expression'`(受限子集:标识符、链式调用、索引);
   `\{` 必须先于插值规则匹配为转义。这是与 Rust/Rust 风格 grammar 最大的差异点。
2. **无块注释、无 char 字面量**:不要写 block-comment 规则;反而把 `/*` 标为
   `invalid.illegal` 帮助来自 C/Rust 的用户;`'` 不进 autoClosingPairs。
3. **`|` 是闭包界定符**:`scope { |s| … }`、`|x| x > 0`——不得作为配对标点高亮,
   否则会在 grammar 层引发配对错乱。

### 2.2 `language-configuration.json`

- **注释**:仅 `lineComment: "//"`(无 blockComment)。
- **括号/自动闭合**:`( ) [ ] { } "`;**排除 `'` 与 `|`**(无 char、闭包竖线)。
- **折叠**:基于 `{ }` 与缩进。
- **换行缩进规则(直接翻译 §1.6 延续集)**——这是 Ctron 独有、体验差异最大的一条:

```jsonc
"increaseIndentPattern": ".*[{(=->=>&&or,]$|.*(?:\\+\\%|-\\%|\\.\\.|\\.\\|=|==|!=|<=|>=|[+*/%<>])$", // 行尾延续集
"decreaseIndentPattern": "^\\s*[)}\\]]|^\\s*(else\\b)",   // else 必须与 } 同行
"onEnterRules": [ { "beforeText": "^\\s*///.*$", "action": { "indent": "none", "appendText": "/// " } } ]
```

行首 `.`(首点式链式调用,§1.6 规则 2)天然落在"上一行未完成→不缩进重置"的路径上,
配合 formatter 未来输出唯一形态。

### 2.3 snippets(`snippets/ctron.json`)

`test "…"{ }`、`fn`、`match` 臂、`struct`、`impl Trait for Type`、`own (arena) { }`、
`scope { |s| }`、`let x = …?`、`#[pure]` / `@derive(...)`。约 15 条即可覆盖高频书写。

**Phase 0 交付物**:可 `code --install-extension ctron-ctron-0.0.1.vsix` 的包。
验收:61 个 `tests/*.ct` 全部语料在 VSCode 中无错乱高亮(用 `developer: inspect TM scopes` 抽查)。

---

## 3. Layer 2:LSP server(`ctron-lsp`)

### 3.1 宿主选型:Rust 版编译器嵌入

| 方案 | 评价 |
|---|---|
| ① 包装 `ctronc` CLI,节流调用 | **Phase 1 快速验证**:JSON 契约(§10.2)已冻结,映射 `< 100 行`;缺点:每次全量解析,延迟 100ms+ |
| ② **Rust 嵌入(推荐)**:`tower-lsp` + `ctron::parse_src/check_src` | 单文件全量 lex+parse 在普通文件上 <1ms 量级,无需增量解析即可流畅;`lib.rs` 接口现成 |
| ③ C 版嵌入 | JSON-RPC 栈在 C 里成本高;留给自举后 |
| ④ 自宿主:用 Ctron 写 LSP | **长期终态**,与编译器 C5-C8 自举路线对齐;Phase ② 的 trait 设计需为迁移留缝(协议层/语义层隔离) |

### 3.2 功能分期

**L1 基线(周)**
- `textDocument/didOpen|didChange`(全量推送)→ `parse_src` → `publishDiagnostics`;
  `code: "E1001"` 等稳定码直通(§10.1 注册表即文档)。
- `documentSymbol`(outline):fn/struct/class/enum/trait/impl/const/test。
- `hover`:声明签名 + 相邻 `///` 文档。

**L2 语义核心(2~3 周)**
- **补全**:按上下文分派——
  - `.` 后:字段 / 方法 / prop(无括号提示差异)/ 元组索引 / UFCS 自由函数;
  - 类型位:内建 + 前奏泛型(带 `[T]` snippet);
  - `use` 后:模块路径;
  - 值位:局部绑定(作用域感知)+ fn + const + 枚举变体 + 关键字。
  - **数据源**:运行时由 `xtask` 从规范 §3.8.2 表生成 `prelude.json`(签名+文档),
    与规范同源,规格变更即重生成——杜绝文档与补全漂移。
- `gotoDefinition` / `findReferences` / `rename`(文件内先做,跨文件随 pkg 层补)。
- `signatureHelp`:调用处展示 `fn` 形参(含 receiver)。

**L3 体验增强(按需)**
- **semantic tokens**(覆盖 TextMate 启发式):区分参数/局部/字段/prop/变体/内建类型;
- **inlay hints**:`let` 推断类型、`with_mut(|var a|)` 闭包参数类型;
- **code actions**:§10.2 的 `fixes` 字段本就要求"机器可执行修复建议"——
  E3030(`static var`→`static let`)、must-use 补 `.or()` 等可直接落为 quick fix;
- 折叠范围、选择范围、document highlight。

**L4 工具链(Layer 3)**
- **test CodeLens**:每个 `test "…"` 上方 `▶ run`,调 `ctronc test --filter`;
  doc-test 块(§10.4)同样可跑——"文档即回归"从编辑器一步触达。
- **AST 视图**:Webview 渲染 `ctronc parse --ast` 的 C-AST v1 确定性文本
  (对编译器开发者自身就是生产力工具,自举差分调试直接看树)。
- **`Ctron.toml`**:贡献 JSON schema,manifest 字段(能力声明等 §8.2)校验与补全。
- **错误码悬浮文档**:诊断 hover 链接到 §10.1 注册表锚点。

---

## 4. 测试策略(复用语料,零新增成本)

1. **grammar 快照**:用 `vscode-tmgrammar-test`,把 `tests/*.ct` 直接作为夹具,
   断言关键行的 scope 序列(61 文件已是全语言覆盖,含所有 `.neg` 非法样本);
2. **LSP 集成**:`vscode-languageclient` 测试骨架打开语料文件,
   断言诊断码与文件名后缀 marker 一致(`.neg.ct` → 必须报、行为 `.ct` → 必须零诊断)
   ——**与 `suite_sem` 同一验收口径**,编辑器诊断与编译器行为永不漂移;
3. **CI**:GitHub Actions:`cargo test`(编译器)+ `npm test`(grammar/LSP)+ `vsce package`。

## 5. 发布与路线图

| 阶段 | 内容 | 工作量 | 出口 |
|---|---|---|---|
| P0 | Layer 1 全套(高亮/缩进/片段) | 1~2 天 | .vsix 可装,61 语料高亮正确 |
| P1 | 包装式诊断 LSP | 1 天 | 保存即出错误码诊断 |
| P2 | Rust 嵌入 LSP:诊断+outline+hover | 2~4 天 | 打开即诊断,大纲可用 |
| P3 | 补全/定义/引用/重命名 + prelude.json 生成 | 1~2 周 | 语义补全与跳转可用 |
| P4 | semantic tokens / inlay / quick fixes / CodeLens / AST 视图 | 持续 | 与 §10 fixes 契约闭环 |
| P5 | ctron-fmt(规范已定义唯一形态)+ 自宿主 LSP | 随自举路线 | 生态完备 |

发布:VS Marketplace + Open VSX 双渠道(`ovsx`),CI 自动打包;
`vsce` 包名建议 `ctron-lang`,与二进制 `ctron`/`ctronc` 命名族一致。

## 6. 风险与对策

- **TextMate 正则能力有限**:插值内嵌表达式、`[]` 三态(切片/定长/泛型)只能启发式高亮;
  正解是 P3 的 semantic tokens 接管语义着色,TextMate 只做"第一眼正确"。
- **两套编译器行为差异**:LSP 锚定 Rust 版,验收锚定 `tests/` 语料(与两编译器共同基准一致)。
- **自宿主迁移**:L2 起把"协议适配"与"语言分析"分层,终态用 Ctron 重写分析层、保留协议壳。
