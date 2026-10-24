# tree-sitter-ctron

Ctron(.ct)的 tree-sitter 语法(highlight-grade),供 Helix / Zed / nvim-treesitter
等 tree-sitter 系编辑器消费。

- 权威依据:`docs/spec/01-lexical-grammar.md`(§1.3 关键字、§1.4 字面量、§1.5 运算符、§1.7 EBNF)
- 定位:编辑器高亮,非编译器前端。§1.6 换行终止(延续集/首点式)为上下文敏感规则,
  此处把换行当空白,语句靠运算符/结构自定界
- 语料实证:第一方 1149 个 `.ct`(tests/lsp/lib/pkgs/selfhosted/compiler/dist)
  正样本解析 **0 ERROR/0 MISSING**(词法压力文件 selfhosted/input_*.ct 与 1 个
  EOF 截断文件除外,属预期)

## 构建 / 测试

```sh
tree-sitter generate     # grammar.js → src/parser.c
tree-sitter test         # test/corpus/smoke.txt(4 节快照)
tree-sitter highlight examples/smoke.ct
```

## 已知取舍(设计裁决,勿"修复")

| 取舍 | 原因 |
|---|---|
| `[...]` 后缀统一为 `subscript_expression`(索引/泛型实参不分) | §1.8 真歧义,GLR 双活后逐对 conflict 无解;高亮无需区分 |
| 插值内容 = 受限子集(interp_expr)+ 纯文本兜底(interp_text) | lsp/gui 文案含裸 `{ |s| … }`;_expr 全集在 GLR 下半匹配产生 ERROR 节点 |
| `string_content` 词法 prec(2) > `line_comment` prec(1) | `"///"` 字面量内 `//` 会被注释吞掉(extras 全局生效) |
| identifier 单 token 收大小写 | 命名约定是 lint(§1.3)非语法;`Net_probe` 等大写函数名必须可解析;PascalCase/SCREAMING 区分交给 highlights 的 `#match?` |
| `.{` 导入花括号为单 token | `use a.b.c.{ d }` 与 path 贪婪 `.` 的 GLR 歧义,词法层消歧 |
| match 守卫 `x if cond =>`(§1.7 之外) | tests/02f 语料实证形态 |

## Helix 接线

`languages.toml`(见 `../README.md`),grammars 的 `source.path` 指向本目录。

## Zed 接线

Zed 扩展工程中 `grammars/ctron` 指向本目录(或发布后的 git 仓库),
`extension.toml` 的 grammars 段引用 `ctron`。

## nvim-treesitter 接线

本目录即标准 tree-sitter 语法布局(`grammar.js` + `src/parser.c` +
`queries/highlights.scm` + `tree-sitter.json`),按 nvim-treesitter 的
本地 parser 注册方式接入即可。

## 目录

- `grammar.js` — 语法规则(含取舍注释)
- `queries/highlights.scm` — 高亮捕获(标准捕获名)
- `test/corpus/smoke.txt` — 快照语料(declarations / expressions / control_and_closures / gui view and style)
- `examples/smoke.ct` — 冒烟样本
