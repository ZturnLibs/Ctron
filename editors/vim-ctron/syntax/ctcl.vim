" Vim syntax file
" Language:     Ctron Config Language (.ctcl / Ctron.lock)
" Maintainer:   Ctron project (editors/vim-ctron)
" Last Change:  2026-10-24
" 规则集与 editors/vscode-ctron/syntaxes/ctcl.tmLanguage.json 对齐:
" 块头(known/通用)+ "名" / 字段 = 值 / 仅十进制整数 / 严格转义字符串 /
" 错法标红(# 注释、[节头]、' 引导、0x/0o/0b、浮点、尾随逗号)。
" vim 优先级:同位后者胜 —— 非法组最后定义;集合内 ] 不转义([^]])。

if exists('b:current_syntax')
  finish
endif

" ── 注释(仅行注释)──
syn match ctclLineComment '//.*'

" ── 字符串(仅 \" 与 \\ 转义;其余转义非法)──
syn region ctclString oneline start=/"/ skip=/\\\\\|\\"/ end=/"/ contains=ctclEscape,ctclInvalidEscape
syn match ctclEscape contained display '\\["\\]'
syn match ctclInvalidEscape contained display '\\[^"\\]'

" ── 数值(仅十进制整数,无前导零)──
syn match ctclNumber display '\%(-\)\?\<\%(0\|[1-9][0-9_]*\)\>'

" ── 布尔 ──
syn keyword ctclBoolean true false

" ── 块头(known 后定义 → 命中时胜过通用块头;引号名由字符串组着色)──
syn match ctclBlockHead display '^[ \t]*\zs[a-z_][a-z0-9_]*\ze\%([ \t]\+"[^"]*"\)\?[ \t]*{'
syn match ctclBlockHeadKnown display '^[ \t]*\zs\%(pkg\|comptime\|dep\|lock\|resolve\|workspace\|profile\|member\|cap\|menu\|fmt\|bench\)\>\ze\%([ \t]\+"[^"]*"\)\?[ \t]*{'

" ── 字段(name = value;== 不着色)──
syn match ctclField display '^[ \t]*[a-z_][a-z0-9_]*\ze[ \t]*='
syn match ctclAssign display '=\%(=\)\@!'

" ── 标点(先于非法组定义)──
syn match ctclBraces display '[\[\]{}]'
syn match ctclComma display ','

" ── 非法标记(最后定义,同位胜过标点/数值)──
syn match ctclInvalidHash display '#.*'
syn match ctclInvalidSection display '^[ \t]*\[[^]]*\]\?'
syn match ctclInvalidQuote display "'[^']*"
syn match ctclInvalidNumBase display '\v<0[xXoObB][0-9a-fA-F_]+>'
syn match ctclInvalidFloat display '\v<[0-9]+\.[0-9_]+'
syn match ctclInvalidTrailingComma display ',[ \t]*\]'

" CTCL 无跨行结构,同步开销低
syn sync minlines=50

hi def link ctclLineComment           Comment
hi def link ctclString                String
hi def link ctclEscape                SpecialChar
hi def link ctclInvalidEscape         Error
hi def link ctclNumber                Number
hi def link ctclBoolean               Boolean
hi def link ctclBlockHead             Label
hi def link ctclBlockHeadKnown        Keyword
hi def link ctclField                 Identifier
hi def link ctclAssign                Operator
hi def link ctclInvalidHash           Error
hi def link ctclInvalidSection        Error
hi def link ctclInvalidQuote          Error
hi def link ctclInvalidNumBase        Error
hi def link ctclInvalidFloat          Error
hi def link ctclInvalidTrailingComma  Error
hi def link ctclBraces                Delimiter
hi def link ctclComma                 Delimiter

let b:current_syntax = 'ctcl'
