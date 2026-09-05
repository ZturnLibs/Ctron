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

## 安装(开发模式)

```sh
# 1. 构建 LSP 宿主(种子编译器)
cd compiler_c && make && cd ..

# 2. 方式 A:F5 调试运行
code editors/vscode-ctron   # VSCode 打开扩展目录,按 F5

# 2. 方式 B:打包安装
cd editors/vscode-ctron && npx @vscode/vsce package && code --install-extension ctron-lang-0.1.0.vsix
```

服务器定位顺序:`ctron.server.command` 设置 → 扩展内置 `server/ctron-lsp.sh`(内部依次尝试 `$CTRONC`、`compiler_c/build/ctronc`、PATH)。

## 设置

| 键 | 默认 | 说明 |
|---|---|---|
| `ctron.server.command` | "" | 自定义启动命令 |
| `ctron.server.debug` | false | 输出面板打印 LSP 收发日志 |
| `ctron.ctroncPath` | "" | ctronc 路径(保存时语义检查用) |
| `ctron.checkOnSave` | true | 保存时运行 `ctronc check --format=json` |

## 已知边界(0.2)

- rename/references 为**同文件词法级**(不做作用域感知:同名字符串/注释已跳过,但不同作用域的同名符号会一并改名)——语义级待 sem 接入;
- `ctronc check` 读取磁盘文件,语义诊断以保存为准(未保存改动只出词法诊断);
- 位置编码声明为 utf-8,非 ASCII 行内列偏移在旧客户端可能有偏差;
- 解释器执行,大文档有可感知延迟——C10 转译后端落地后可原生运行。

## 结构

```
editors/vscode-ctron/
  package.json                 # 扩展清单(languages/grammars/snippets/configuration)
  language-configuration.json  # 注释/括号/缩进(§1.6 延续集)/onEnter
  syntaxes/ctron.tmLanguage.json
  snippets/ctron.json
  src/extension.js             # 手写 LSP 客户端(零 npm 依赖)
  server/ctron-lsp.sh          # 启动器:定位 ctronc → run lsp/src/main.ct
../../lsp/src/main.ct          # LSP 服务器本体(Ctron 语言实现)
```
