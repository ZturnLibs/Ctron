# editors — Ctron 编辑器支持总览

| 编辑器 | 目录/形态 | 高亮机制 | 状态 |
|---|---|---|---|
| VSCode | `../vscode-ctron/`(扩展) | TextMate + LSP 客户端 | ✅ v0.5.0(含 CTML/CTCL) |
| Vim / Neovim | `../vim-ctron/`(插件) | Vim syntax + LSP(经 ctron-lsp.sh) | ✅ 2026-10-24 |
| Helix | `../tree-sitter-ctron/` + 接线说明 | tree-sitter | ✅ grammar 0.1.0 |
| Zed | `../tree-sitter-ctron/` + 接线说明 | tree-sitter | ✅ 同上 |
| nvim-treesitter | `../tree-sitter-ctron/` | tree-sitter | ✅ 同上 |
| Sublime Text | `../sublime-ctron/Ctron.sublime-syntax` | sublime-syntax(YAML) | ✅ 0.1.0 |
| TextMate / JetBrains | `../textmate-ctron/Ctron.tmbundle` | 现有 tmLanguage 复用 | ✅ 基线高亮 |
| Emacs | `../emacs-ctron/ctron-mode.el` | font-lock | ⚠️ 未经运行验证(宿主无 Emacs) |

LSP(诊断/补全/跳转/重命名)与编辑器无关:`lsp/src/main.ct` 经
`../vscode-ctron/server/ctron-lsp.sh` 以 stdio 运行,任何 LSP 客户端均可接入。

## Helix 接线(tree-sitter-ctron)

`languages.toml`:

```toml
[[grammar]]
name = "ctron"
source = { path = "editors/tree-sitter-ctron" }

[[language]]
name = "ctron"
scope = "source.ctron"
file-types = ["ct"]
roots = ["Ctron.ctcl"]
grammar = "ctron"
language-servers = ["ctron-lsp"]
```

## Zed 接线

Zed 扩展需 `extension.toml` + Rust 委托 crate(发布渠道所需),grammars 直接引用
`tree-sitter-ctron` 仓库;未随附骨架 —— 发布前在 [zed-industries/extensions]
流程中包一层即可,高亮层与本 grammar 完全一致。

## nvim-treesitter 接线

`nvim-treesitter` 的 parser 目录约定与本仓库 `editors/tree-sitter-ctron` 兼容
(`grammar.js` + `src/parser.c` 已生成);本地 finger-print 方式注册,或等发布到
registry 后走标准安装。

## JetBrains(IntelliJ / PyCharm / GoLand …)

基线高亮零开发:Settings → Editor → TextMate Bundles → 添加
`editors/textmate-ctron/Ctron.tmbundle`。原生插件(PSI 级补全/重构)需要
Grammar-Kit + IntelliJ SDK,属独立工程,未包含。

## Emacs

`emacs-ctron/ctron-mode.el` 放入 `load-path` 后 `(require 'ctron-mode)`,
`.ct` 自动关联。宿主机无 Emacs,**未经运行验证**;启用前先字节编译。

## Sublime Text

`Ctron.sublime-syntax` 放入 `Packages/User/` 即可(Package Control 发布需另建
channel 仓库)。
