; Ctron highlight queries — 标准捕获名,适配 Helix/Zed/nvim-treesitter
; 权威依据:docs/spec/01-lexical-grammar.md §1.3–§1.5

; ── 注释 ──
(line_comment) @comment

; ── 关键字族 ──
[
  "fn" "let" "var" "const" "static" "comptime"
  "struct" "class" "enum" "trait" "impl"
  "use" "pub" "extern" "prop" "test"
  "or" "own" "scope"
] @keyword

[
  "if" "else" "match"
] @keyword.conditional

[
  "while" "for" "in" "return"
] @keyword.repeat

"self" @variable.builtin
(wildcard) @variable.builtin
(boolean) @constant.builtin
(void) @type.builtin

; ── 类型 ──
((identifier) @type
  (#match? @type "^[A-Z]"))

; SCREAMING_CASE 常量(§1.3 命名约定)
((identifier) @constant
  (#match? @constant "^[A-Z][A-Z0-9_]+$"))

; ── 函数与字段 ──
(function_decl name: (identifier) @function)
(test_decl (string) @string)
(call_expression
  function: (identifier) @function.call)
(call_expression
  function: (member_expression member: (identifier) @function.method.call))
(member_expression
  member: (identifier) @property)
(field_decl name: (identifier) @property)
(field_init name: (identifier) @property)
(field_pattern name: (identifier) @property)

; ── 属性注解 ──
(decorator) @attribute

; ── 字面量 ──
(number) @number
(string) @string
(escape_sequence) @string.escape
(string (interpolation) @embedded)

; ── 运算符与标点 ──
[
  "+" "-" "*" "/" "%" "+%" "-%"
  "+=" "-=" "*=" "/=" "%="
  "==" "!=" "<" ">" "<=" ">="
  "&&" "||" "!" "or"
  ".." "..=" "="
  "=>" "->" "?" "&" "|"
] @operator

[
  "," ":" "."
] @punctuation.delimiter

[
  "(" ")" "[" "]" "{" "}"
] @punctuation.bracket
