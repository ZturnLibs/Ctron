# sublime-ctron — Sublime Text 的 Ctron 语法

`Ctron.sublime-syntax`(YAML)为 Sublime Text 3/4 提供 `.ct` 的语法高亮:
关键字全集、类型/前奏、`#[...]`/`@derive` 注解、字符串(转义 + `{expr}` 插值)、
数字(进制/后缀)、非法标记注释。

- 权威依据:`docs/spec/01-lexical-grammar.md`;结构与
  `../vscode-ctron/syntaxes/ctron.tmLanguage.json` 对齐
- 校验:YAML 解析 + contexts 引用完整性(ruby YAML,本机已验)

## 安装

`Ctron.sublime-syntax` 复制到 `Preferences → Browse Packages → User/` 下即可;
右下角语法选择 → Ctron(或按 `.ct` 扩展名自动)。

## 已知取舍

- 字符串插值 `{expr}` 内不做子着色(整段按字符串基色,插值花括号着 punctuation);
- `view`/`style` GUI 标记块按普通 Ctron 规则着色(未内嵌 HTML 型标记子语法);
- 补全/LSP 未随附:可用 LSP 包指向 `../vscode-ctron/server/ctron-lsp.sh`(stdio)。
