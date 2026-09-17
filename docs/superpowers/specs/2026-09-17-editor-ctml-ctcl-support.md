# 编辑器 CTML/CTCL 支持(vscode-ctron 扩展 v0.5)——设计记录

- 日期:2026-09-17。
- 上游规范(语法的唯一权威依据,本文不复制语法细则):
  - CTCL:`2026-09-16-config-language-v1.md`(`.ctcl` 配置语言;文法 §4、清单注册表 §5、诊断 §6、规范形态 §8);
  - CTML:`2026-09-16-gui-ctml-design.md`(GUI 标记语言;语言 §4、样式 §5)。
- 落点:`editors/vscode-ctron/`(编辑器泳道);编译器/LSP 服务器**零改动**。

## 0. 目标与非目标

**目标**:`.ctml` 与 `.ctcl`(含 `Ctron.lock` 同文法)在 VSCode 中获得与 `.ct` 同级的
第一眼体验——语法高亮、括号/注释/缩进、代码片段、上下文补全与悬浮提示。

**非目标**:
- LSP 服务器扩展(`lsp/src/main.ct` 只服务 `ctron` 语言;CTML/CTCL 的语义级诊断
  属编译器/工具侧 `tools/ctcl_check.py` 泳道);
- `ctronc check` 对 `.ctcl` 的接入(编译器 check 入口按 Ctron 源码解析,喂 `.ctcl`
  会产生伪诊断;待 §9 L1 `std.config` 落地后另接);
- tree-sitter、npm 依赖引入(扩展保持零依赖手写惯例)。

## 1. 裁决

| # | 裁决 | 理由/否决的替代 |
|---|---|---|
| D1 | 两个独立语言 id:`ctml`、`ctcl`;scopeName `source.ctml`/`source.ctcl` | 后缀 = 语言身份(CTCL D1);合并进 `ctron` 会让 HTML 式标记与块式配置共用一套启发式,两头失真。否决:复用 `ctron` 语言 id |
| D2 | `Ctron.lock` 经 `filenames` 关联到 `ctcl` 语言 | CTCL 规范 §3:锁文件与清单同文法同解析器(唯一保留 `.lock` 后缀者) |
| D3 | CTML 的绑定表达式 `{…}`、插值 `{…}`、view props 表 `(…: T)` 内部 `include: 'source.ctron'` | 绑定表达式 = Ctron 本体(CTML §4.2 要点 2),高亮免费继承,零漂移;否则要复制一份 Ctron 表达式规则 |
| D4 | CTCL 高亮只表达**文法层**(块/键/四类值/注释);注册表级未知键标红等语义判断不做 | 文法冻结、注册表演进(CTCL D7);编辑器标红会随注册表漂移成假警报。非法拼写(`#` 注释/单引号/浮点/尾逗号/`[section]`)按 §6 E504x 常见错法表标 `invalid.illegal`——与 `.ct` 语法高亮的"非法标点即时标红"同款价值 |
| D5 | 补全/悬浮 = 扩展侧本地注册表(`src/ctcl.js`/`src/ctml.js`),数据手抄自两规范的注册表(§5 键表、§4.2 内建标签集、§5.1 样式属性子集) | LSP 不认识这两种语言;规范页小(键 ≤10/标签 ≤10/属性 ≤15),手抄 + 注明出处即可,**不引入生成流水线**(否决:xtask 从规范生成 JSON——收益不抵工程量,规范变更时人工同步) |
| D6 | 能力值补全 = `{fs, time, net, log}` | 与 `compiler-rust/src/sem.rs` `cap_key_by_def`(Clock/Fs/Net/Log)及自举线 `parse_pkg.ct` 一致 |
| D7 | `ctml.js` 组件标签补全动态扫描本文档 `view Name` 声明 + 内建集 | CTML §4.3:同文件互引用合法、声明顺序无关;跨文件 `use` 导入的组件解析属编译器语义,编辑器侧只做本文档 |
| D8 | 保存时 `ctronc check` 不对 `.ctml`/`.ctcl` 触发(languageId 过滤已是 `ctron`,维持) | 见非目标;`checkNow` 命令同样只认 `ctron` |

## 2. 交付物

```
editors/vscode-ctron/
  package.json                        # +languages ctml/ctcl、+grammars、+snippets;version 0.5.0
  language-configuration.ctml.json    # // 注释;{} [] () "";onEnter /// 续行
  language-configuration.ctcl.json    # // 注释;{} [] "";行尾 { 增缩进(块头 { 必须行尾)
  syntaxes/ctml.tmLanguage.json       # view/style 声明、标签/属性/class、绑定与插值(source.ctron 内嵌)、
                                      #   样式属性/颜色 #RRGGBB[AA]、非法 /* 与 <!-- -->
  syntaxes/ctcl.tmLanguage.json       # 块头/键控块、字段、四类值、// 注释;
                                      #   非法:# 注释、单引号、浮点、列表尾逗号、[section]
  snippets/ctml.json                  # view/style/extends/each/when/slot/on:click 按钮…
  snippets/ctcl.json                  # pkg 清单、comptime、dep 三形、键控块
  src/ctcl.js                         # 上下文补全(块头/各块键/caps 值/Bool 值)+ 键/块 hover
  src/ctml.js                         # 补全(< 标签、属性、样式属性)+ 标签/属性/样式属性 hover
  src/extension.js                    # registerCompletionItemProvider/HoverProvider × {ctml, ctcl}
```

## 3. 上下文识别(两模块同法,轻量行扫描)

- **ctcl.js**:自光标向上找最近块头(`IDENT ["arg"] {`)与块尾 `}`,得当前块名;
  行内含 `caps = [` 且光标在列表内 → 能力值补全;光标前是 `= ` 且键为 Bool 型 → true/false。
- **ctml.js**:向上最近未闭合 `<tag` → 属性位补全(标签特有属性 + 通用 + on:* 事件);
  光标在 `style` 块内 → 样式属性补全;前缀 `<` → 标签补全(内建 + 本文档 view)。

hover 同表驱动:命中 `块名/键/标签/属性/样式属性` 即给一行规范出处摘要。

## 4. 验收

1. 全部 JSON 过 `python3 -m json.tool`;JS 过 `node --check`;
2. 用规范附录样例(D.1/D.2 清单、§4.3 card.ctml、§10 todo.ctml)做高亮抽样:
   块头/键/值/注释各自 scope 正确;`{expr}` 内部呈 `source.ctron` 着色;
3. `Ctron.ctcl` 真实文件(仓库 22 个)打开无错乱高亮;
4. 补全:pkg 块内键、caps 列表内能力、`<` 后标签、style 块内属性均出候选。

## 5. 边界登记(后续版本)

- CTML 跨文件组件补全/跳转(`pub view` + `use`)待语义信息进编辑器(随 M2 LSP 泳道);
- CTCL 注册表校验诊断(E504x)待 `tools/ctcl_check.py` 以 `ctron.check.ctcl` 形式接入;
- `Ctron.lock` 的补全注册表(resolve 块键)随 v0.6 依赖解析立表后同步。
