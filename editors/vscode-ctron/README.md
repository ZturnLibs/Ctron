# Ctron VSCode 扩展(ctron-lang)

Ctron 语言的 VSCode 支持:语法高亮 + 智能提示。语言服务器(`lsp/src/main.ct`)**用 Ctron 语言自身实现**,经种子编译器 `ctronc run` 解释运行——与仓库自举路线一致。

## 功能

| 功能 | 层 | 说明 |
|---|---|---|
| 语法高亮 | TextMate | 关键字/类型/数字后缀/字符串插值 `{expr}`/`///` 文档注释/`#[...]` `@derive(...)`;**非法标点即时标红**(`;` `::` `/*` `'…'`、预留字) |
| 缩进与换行 | language-configuration | 按 §1.6 延续集(行尾 `= -> => && .. ( [ ,` 增缩进);`///` 回车自动续行 |
| 代码片段 | snippets | 22 条:`test`/`fn`/`match`/`scope`/`own`/`prop`/`mutex`/… |
| 实时诊断 | LSP | 词法级 E1001 族:未终止字符串/插值、禁用标点、非法转义(打开即查,按键即更) |
| 语义诊断 | check 包装 | 保存时运行 `ctronc check --format=json`(§10.2 冻结契约),报告 E2xxx…E6xxx 语义错误/lint;命令 `Ctron: 立即检查当前文件` |
| 快速修复 | CodeAction | 词法确定性修复(`;` 删除、`::`→`.`);编译器 `fixes[]` 通道已接通 |
| 文档大纲 | LSP | fn/struct/class/enum/trait/impl/const/test(含 `pub` 前缀) |
| 悬浮提示 | LSP | 声明签名 + 相邻 `///` 文档;关键字/前奏类型内置文档(§1.3/§3.8.2) |
| 补全 | LSP | 关键字 + 前奏类型 + 本文档声明(前缀粗筛);`.` 后给前奏成员集 |
| 定义跳转 | LSP | 本文档内声明 |
| **重命名** | LSP | 同文件词法级重命名(prepareRename 校验;关键字/非法名拒绝) |
| **引用查找** | LSP | 同文件,含/不含声明可选 |
| **文档高亮** | LSP | 光标词全部出现点 |
| **签名帮助** | LSP | 调用点回溯声明,形参名与 activeParameter 提示(`(` `,` 触发) |
| **形参名提示** | LSP | inlay hints:调用点实参前显示形参名(≤24KB 文档;大文档跳过) |
| **格式化** | LSP | 缩进规整(4 空格/层,`} / ) / ] / else` 起始行去层)——配合编辑器 format-on-save |
| **代码折叠** | LSP | 花括号块折叠(优于默认缩进折叠) |
| **语义着色** | LSP | semantic tokens:调用/方法/属性/声明(+decl 修饰)/内建与自定义类型/注解宏 分类着色,覆盖 TextMate 启发式 |

### CTML(.ctml,GUI 标记)与 CTCL(.ctcl / Ctron.lock,配置)——v0.5 新增

| 功能 | 层 | 说明 |
|---|---|---|
| CTML 语法高亮 | TextMate | `view`/`style`/`pub`/`extends` 声明;内建标签 vs PascalCase 组件标签分色;属性/`on:*` 事件;`{expr}` 绑定与插值**内嵌 source.ctron** 着色;样式属性/`#RRGGBB[AA]` 颜色;`/*` 与 `<!--` 即时标红 |
| CTML 提示 | 本地注册表 | `<` 后标签补全(内建集 + 本文档 `view` 组件扫描);标签内属性补全(元素特有 + 通用 + `on:*`);style 块内属性补全(CSS 子集)+ direction 值;标签/属性/样式属性/事件悬浮文档 |
| CTCL 语法高亮 | TextMate | 块头/键控块、字段、四类值(Str/I64/Bool/List[Str]);**常见错法即时标红**:`#` 注释(E5042)、单引号、浮点(G1)、列表尾逗号、`[section]` 段头(E5040) |
| CTCL 提示 | 本地注册表 | 块位补全(pkg/comptime/dep…带模板);键位补全按所在块给注册表键(pkg: manifest_version/name/version/caps;dep: path/git/rev/version);caps 列表内补能力值 fs/time/net/log;值位补全(manifest_version=1、use_color=true/false);块/键/能力悬浮文档 |
| 编辑基础 | language-configuration | 两语言各自注释/括号/缩进(CTCL 行尾 `{` 增缩进;CTML 标签行增缩进、`</tag>` 减缩进)/`///` 续行 |
| 代码片段 | snippets | CTML 16 条(view/style/extends/each/when/slot/按钮/受控 input…);CTCL 7 条(pkg 清单/dep 三形/键控块/fmt) |

数据源即两份冻结规范(`docs/superpowers/specs/2026-09-16-config-language-v1.md` §5 注册表、`2026-09-16-gui-ctml-design.md` §4/§5),设计记录见 `docs/superpowers/specs/2026-09-17-editor-ctml-ctcl-support.md`。LSP 服务器(`.ct`)不服务这两种语言——语义级 CTCL 校验(E504x)待 `tools/ctcl_check.py` 泳道另接;`ctronc check` 保存时检查仍只对 `.ct` 触发。

## 安装(开发模式)

```sh
# 1. 构建 LSP 宿主(种子编译器)
cd compiler-c && make && cd ..

# 2. 方式 A:F5 调试运行
code editors/vscode-ctron   # VSCode 打开扩展目录,按 F5

# 2. 方式 B:打包安装
cd editors/vscode-ctron
npm run package        # 打包到 dist/ctron-lang-<版本>.vsix
npm run install:vsix   # 装进 VSCode(缺省最新;指定版本:npm run install:vsix -- 0.4.0,可简写 0.4)
```

服务器定位顺序:`ctron.server.command` 设置 → 扩展内置 `server/ctron-lsp.sh`(内部依次尝试 `$CTRONC`、`compiler-c/build/ctronc`、PATH)。

## 设置

| 键 | 默认 | 说明 |
|---|---|---|
| `ctron.server.command` | "" | 自定义启动命令 |
| `ctron.server.debug` | false | 输出面板打印 LSP 收发日志 |
| `ctron.ctroncPath` | "" | ctronc 路径(保存时语义检查用) |
| `ctron.checkOnSave` | true | 保存时运行 `ctronc check --format=json` |

## 已知边界(0.3)

- rename/references 为**同文件词法级**(不做作用域感知:同名字符串/注释已跳过,但不同作用域的同名符号会一并改名)——语义级待 sem 接入;
- `ctronc check` 读取磁盘文件,语义诊断以保存为准(未保存改动只出词法诊断);
- 位置编码声明为 utf-8,非 ASCII 行内列偏移在旧客户端可能有偏差;
- 解释器执行:≤24KB 文档体验流畅;更大文档跳过 inlay hints,其余特性 61KB/3s 内——C10 转译后端落地后可原生运行;
- canonical ctron-fmt(规范唯一形态)待后续版本。

## 结构

```
editors/vscode-ctron/
  package.json                          # 扩展清单(languages/grammars/snippets/configuration)
  language-configuration.json           # .ct:注释/括号/缩进(§1.6 延续集)/onEnter
  language-configuration.ctml.json      # .ctml:注释/括号/标签缩进/onEnter
  language-configuration.ctcl.json      # .ctcl/.lock:注释/括号/块缩进
  syntaxes/ctron.tmLanguage.json
  syntaxes/ctml.tmLanguage.json         # 绑定/插值内嵌 source.ctron(表达式高亮免费继承)
  syntaxes/ctcl.tmLanguage.json
  snippets/ctron.json
  snippets/ctml.json
  snippets/ctcl.json
  src/extension.js             # 手写 LSP 客户端(零 npm 依赖)
  src/ctml.js                  # CTML 补全/hover(标签/属性/样式注册表 + view 扫描)
  src/ctcl.js                  # CTCL 补全/hover(块/键/能力注册表)
  scripts/package.mjs          # 打包脚本 ESM(npm run package/sync/clean;零依赖,vsce 经 npx)
  server/ctron-lsp.sh          # 启动器:定位 ctronc → run lsp/src/main.ct
  dist/                        # 打包产物(vsce 输出目录;npm run package / npm run clean)
../../lsp/src/main.ct          # LSP 服务器本体(Ctron 语言实现)
```
