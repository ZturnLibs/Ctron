" Vim syntax file
" Language:     Ctron Markup Language (.ctml)
" Maintainer:   Ctron project (editors/vim-ctron)
" Last Change:  2026-10-24
" 规则集与 editors/vscode-ctron/syntaxes/ctml.tmLanguage.json 对齐:
" use / view{markup} / style{prop: value} / 标签与属性 / {expr} 内嵌 source.ctron。
" vim 优先级实证:同位置 contained 内后定义者胜;\zs 使有效起点后移。

if exists('b:current_syntax')
  finish
endif

" 嵌入 source.ctron:syn include 为 ctron 全部条目加 contained 并建簇。
" 注意:自动簇只收编原文件中"无 contained"的条目 —— 串内组(InterpVar/Prop/
" Method/Escape)需显式并入,否则插值内标识符不着色。
unlet! b:current_syntax
syn include @ctmlCtronBase syntax/ctron.vim
let b:current_syntax = 'ctml'
syn cluster ctmlCtron contains=@ctmlCtronBase,ctronInterpVar,ctronInterpOp,ctronInterpProp,ctronInterpMethod,ctronEscape,ctronString,ctronNumber

" ── 注释(§:/// 为文档注释;doc 后定义→同位胜)──────────────────
syn match ctmlLineComment '//.*'
syn match ctmlDocComment '///.*'

" ── use 声明(父 match 着色 use,contained 子条着色路径)─────────
syn match ctmlUse display '^\s*\<use\>[ \t]\+[a-zA-Z_][A-Za-z0-9_.]*' contains=ctmlUsePath
syn match ctmlUsePath contained display '\%(^\s*\<use\>[ \t]\+\)\@<=[a-zA-Z_][A-Za-z0-9_.]*'

" ── 字符串(仅转义;CTML 的插值在文本层而非串内)────────────────
syn region ctmlString contained oneline start=/"/ skip=/\\\\\|\\"/ end=/"/ contains=ctmlEscape
syn match ctmlEscape contained display '\\[ntr\\"0{]\|\\u{[0-9a-fA-F_]\+}'

" ── 插值 {expr}:嵌入完整 ctron 语法 ──
syn region ctmlInterp contained start=/{/ end=/}/ contains=@ctmlCtron

" ── view 声明(pub? view Name(props)? { markup })──────────────
" 头部单色(matchgroup);TextMate 版对 props 内 ctron 细粒度着色的差异见 README
syn region ctmlView matchgroup=ctmlViewHead
  \ start=/\<view\>[ \t]\+[A-Z]\k*\%([ \t]*\%([^(){}]\|([^()]*)\)*\)\?[ \t]*{/
  \ end=/}/
  \ contains=@ctmlMarkup

" ── style 声明(pub? style name (extends base)? { prop: value; })──
syn region ctmlStyle matchgroup=ctmlStyleHead
  \ start=/\<style\>[ \t]\+[a-z][a-z0-9_-]*\%([ \t]*extends[ \t]\+[a-z][a-z0-9_-]*\)\?[ \t]*{/
  \ end=/}/
  \ contains=@ctmlStyleBody

" ── 样式体:值词先定义,属性名后定义(同位 `direction:` 归属性)────
syn match ctmlValueWord contained display '[a-z][a-z0-9-]*'
syn match ctmlStyleProp contained display '[a-z][a-z0-9-]*\ze[ \t]*:'
syn match ctmlSemi contained display ';'
syn match ctmlHexColor contained display '#\%(\x\{8}\|\x\{6}\|\x\{3,4}\)\>'
syn match ctmlNumber contained display '-\?[0-9][0-9_]*'
syn match ctmlConstant contained display '\<[A-Z_][A-Z0-9_]*\>'

" ── 标记:标签内条目(名字类后定义,`<vbox` 归 tag 而非属性)───────
syn region ctmlTag oneline start=/</ end=/>/
  \ contains=ctmlIn,ctmlAttrVar,ctmlEventName,ctmlEventSep,ctmlAttrName,ctmlAssign,ctmlString,ctmlInterp,ctmlTagName
syn keyword ctmlIn contained in
syn match ctmlAttrName contained display '[a-zA-Z_][A-Za-z0-9_-]*'
syn match ctmlAttrVar contained display '[a-z_]\k*\ze\s*in\>'
syn match ctmlEventSep contained display 'on:'
syn match ctmlEventName contained display 'on:[a-zA-Z][A-Za-z0-9_-]*' contains=ctmlEventSep
syn match ctmlAssign contained display '='
syn match ctmlTagName contained display '<\zs[a-zA-Z][A-Za-z0-9_-]*'
syn match ctmlTagClose display '</[a-zA-Z][A-Za-z0-9_-]*[/]\?>'

syn cluster ctmlMarkup contains=ctmlLineComment,ctmlDocComment,ctmlTag,ctmlTagClose,ctmlInterp
syn cluster ctmlStyleBody contains=ctmlLineComment,ctmlDocComment,ctmlStyleProp,ctmlSemi,ctmlHexColor,ctmlString,ctmlNumber,ctmlConstant,ctmlValueWord

" ── 非法标记:最后定义(同位 `<!--` 归非法而非标签区)─────────────
syn match ctmlInvalid display '/\*'
syn match ctmlInvalid display '<!--'

" view/style 跨行;同步窗口放宽
syn sync minlines=200

hi def link ctmlLineComment  Comment
hi def link ctmlDocComment   Comment
hi def link ctmlInvalid      Error
hi def link ctmlUse          Keyword
hi def link ctmlUsePath      Identifier
hi def link ctmlString       String
hi def link ctmlEscape       SpecialChar
hi def link ctmlViewHead     Define
hi def link ctmlStyleHead    Define
hi def link ctmlStyleProp    Type
hi def link ctmlSemi         Delimiter
hi def link ctmlHexColor     Number
hi def link ctmlNumber       Number
hi def link ctmlConstant     Constant
hi def link ctmlValueWord    Constant
hi def link ctmlTagName      Function
hi def link ctmlTagClose     Function
hi def link ctmlIn           Keyword
hi def link ctmlAttrVar      Identifier
hi def link ctmlEventName    Identifier
hi def link ctmlEventSep     Delimiter
hi def link ctmlAttrName     Identifier
hi def link ctmlAssign       Operator

let b:current_syntax = 'ctml'
