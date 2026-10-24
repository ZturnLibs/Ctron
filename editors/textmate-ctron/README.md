# textmate-ctron — TextMate / JetBrains 的 Ctron 基线高亮

`Ctron.tmbundle` 直接复用 VSCode 扩展的 `ctron.tmLanguage`(转换成 plist XML),
为 TextMate 2 与 JetBrains 系 IDE 提供基线语法高亮。

- 来源:`../vscode-ctron/syntaxes/ctron.tmLanguage.json`(patterns/repository 原样)
- 校验:`plutil -lint` 通过(本机已验)

## TextMate 2

双击 `Ctron.tmbundle` 安装,或放入 `~/Library/Application Support/TextMate/Bundles/`。

## JetBrains(IntelliJ / PyCharm / GoLand / RubyMine …)

1. 安装内置插件 **TextMate Bundles**(默认自带);
2. Settings → Editor → TextMate Bundles → `+` 添加本目录的 `Ctron.tmbundle`;
3. Settings → Editor → File Types → TextMate files 中确认 `.ct` 关联。

## 范围说明

这是**基线高亮**(关键字/字符串/类型/注释),不含 LSP 能力。诊断/补全/跳转请经
通用 LSP 客户端接入 `../vscode-ctron/server/ctron-lsp.sh`;原生 IntelliJ 插件
(PSI 级)属独立工程,见 `../README.md`。
