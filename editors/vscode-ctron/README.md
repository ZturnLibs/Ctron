# Ctron VSCode 扩展(ctron-lang)

Ctron 语言的 VSCode 支持:语法高亮 + 智能提示。语言服务器(`lsp/src/main.ct`)**用 Ctron 语言自身实现**,经种子编译器 `ctronc run` 解释运行——与仓库自举路线一致。

## 功能

| 功能 | 层 | 说明 |
|---|---|---|
| 语法高亮 | TextMate | 关键字/类型/数字后缀/字符串插值 `{expr}`/`///` 文档注释/`#[...]` `@derive(...)`;**非法标点即时标红**(`;` `::` `/*` `'…'`、预留字) |
| 缩进与换行 | language-configuration | 按 §1.6 延续集(行尾 `= -> => && .. ( [ ,` 增缩进);`///` 回车自动续行 |
| 代码片段 | snippets | 22 条:`test`/`fn`/`match`/`scope`/`own`/`prop`/`mutex`/… |
| 实时诊断 | LSP | 词法级 E1001 族:未终止字符串/插值、禁用标点、非法转义(打开即查,按键即更) |
| 文档大纲 | LSP | fn/struct/class/enum/trait/impl/const/test(含 `pub` 前缀) |
| 悬浮提示 | LSP | 声明签名 + 相邻 `///` 文档;关键字/前奏类型内置文档(§1.3/§3.8.2) |
| 补全 | LSP | 关键字 + 前奏类型 + 本文档声明(前缀粗筛);`.` 后给前奏成员集 |
| 定义跳转 | LSP | 本文档内声明 |

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

## 已知边界(骨架版)

- 诊断为**词法级**;语法/语义诊断(await parse/sem 并入)由编译器里程碑推进后接入;
- 位置编码声明为 utf-8(character = 行内字节偏移),非 ASCII 行内的列偏移在旧客户端可能与 utf-16 有偏差;
- 跨文件跳转、语义补全 awaiting 模块系统(C8)与 sem 接入;
- 解释器执行,大文档(百 KB 级)会有可感知延迟——Ctron 自举编译器落地后消除。

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
