# emacs-ctron — Emacs 的 Ctron major mode

`ctron-mode.el`:`.ct` 文件的高亮(关键字/类型/数字/函数名/注释/字符串)、
注释命令(`comment-dwim` 等,`//` 与 `///`)、`.ct` 自动关联。

- 权威依据:`docs/spec/01-lexical-grammar.md` §1.2–§1.5
- **状态:宿主机无 Emacs,未经运行验证。** 启用前请先字节编译并目检:
  `emacs --batch -f batch-byte-compile ctron-mode.el`

## 安装

```elisp
;; init.el
(add-to-list 'load-path "~/Zturn/Ctron/editors/emacs-ctron")
(require 'ctron-mode)
```

## 已知缺口

| 缺口 | 说明 |
|---|---|
| §1.6 感知缩进 | 未实现,`indent-relative` 兜底(延续集/首点式规则见 `../vim-ctron/indent/ctron.vim` 可移植) |
| 插值 `{expr}` 子着色 | 字符串整段单色(syntax-table 方案;`{` 转义 `\{` 已覆盖) |
| GUI view/style 标记 | `tests/gui` 的 `.ct` 内嵌 view/tag 未做子着色(可参照 tree-sitter 语法的容错策略) |
| 非法标记(;/::/块注释) | 已按 warning 标红 ✓ |

## LSP

eglot / lsp-mode 通用客户端指向 `../vscode-ctron/server/ctron-lsp.sh`(stdio)
即可获得诊断/补全/跳转(与 VSCode 同一后端)。
