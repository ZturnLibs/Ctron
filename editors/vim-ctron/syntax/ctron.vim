" Vim syntax file
" Language:     Ctron (.ct)
" Maintainer:   Ctron project (editors/vim-ctron)
" Last Change:  2026-10-24
" 权威依据:docs/spec/01-lexical-grammar.md §1.2–§1.6;
" 规则集与 editors/vscode-ctron/syntaxes/ctron.tmLanguage.json 对齐。
" vim 同位置优先级(:h syn-priority + 实证):keyword > contained > 全局;
" 同为全局 match 后定义者胜 —— 因此顺序必须是:先宽后窄,注释在 operator 之后。

if exists('b:current_syntax')
  finish
endif

" ── 字符串(§1.4:双引号;转义 \n \t \r \\ \" \0 \{ \u{HEX};插值 {expr})──
" 插值子集:标识符、字段/方法链、索引;字面 { 必须写作 \{。
syn region ctronString oneline start=/"/ skip=/\\\\\|\\"/ end=/"/ contains=ctronEscape,ctronInterp
syn match ctronEscape contained display '\\[ntr\\"0{]\|\\u{[0-9a-fA-F_]\+}'
syn region ctronInterp contained oneline start=/{/ end=/}/ contains=ctronInterpVar,ctronInterpOp,ctronInterpProp,ctronInterpMethod,ctronBuiltinType,ctronPreludeType,ctronTrait,ctronNumber
syn match ctronInterpVar contained display '[a-z_]\k*'
syn match ctronInterpOp contained display '[?+]\|\[\|\]'
syn match ctronInterpProp contained display '\.\zs[a-z_]\k*'
syn match ctronInterpMethod contained display '\.\zs[a-z_]\k*\ze[ \t]*('

" ── 数字(§1.4:0x/0o/0b、_ 分隔、12 整型后缀 + f32/f64、浮点指数)──
syn match ctronNumber display '\v<(0x\x%(_?\x+)*|0o[0-7]%(_?[0-7]+)*|0b[01]%(_?[01]+)*|\d%(_?\d+)*%(\.\d%(_?\d+)*)?%([eE][+\-]?\d%(_?\d+)*)?)%(i8|i16|i32|i64|isize|u8|u16|u32|u64|usize|f32|f64)?>'

" ── 关键字(§1.3 唯一权威清单)────────────────────────────────
syn keyword ctronConditional if else match
syn keyword ctronRepeat while for in
syn keyword ctronFnKw fn nextgroup=ctronFuncName skipwhite skipempty
syn keyword ctronKeyword let var comptime return use pub extern prop own scope test break continue
syn keyword ctronStructure struct class enum trait nextgroup=ctronTypeName skipwhite skipempty
syn keyword ctronStructure impl
syn keyword ctronKwConst const nextgroup=ctronConstName skipwhite skipempty
syn keyword ctronKwStatic static nextgroup=ctronLetInStatic,ctronConstName skipwhite skipempty
syn keyword ctronLetInStatic contained let nextgroup=ctronConstName skipwhite skipempty
syn keyword ctronBoolean true false
syn keyword ctronConstant void
syn keyword ctronSelf self
syn keyword ctronOpWord or

" 声明名(nextgroup 链,contained 优先于下方全局启发式)
syn match ctronFuncName contained display '[A-Za-z_]\k*'
syn match ctronTypeName contained display '[A-Z]\k*'
syn match ctronConstName contained display '[A-Z_][A-Z0-9_]*'

" ── 注解(§8.3:#[...] 与 @derive(...))─────────────────────────
syn match ctronDerive display '@derive\>[ \t]*([^)]*)'
syn region ctronAnnotation start=/#\[/ end=/\]/ contains=ctronAttrName,ctronString,ctronNumber
syn match ctronAttrName contained display '[A-Za-z_][A-Za-z0-9_.]*'

" ── 类型(内建 + 前奏 + trait)───────────────────────────────
syn keyword ctronBuiltinType Bool I8 I16 I32 I64 ISize U8 U16 U32 U64 USize F32 F64 Str String Void
syn keyword ctronPreludeType Option Result List Box Channel Mutex Atomic Global Arena Task Simd AnyError
syn keyword ctronTrait Show Eq Error Cap Drop

" 全局启发式:同位置后定义者胜 —— 变体必须在 userType 之后
syn match ctronUserType display '\v<[A-Z][A-Za-z0-9_]*>'
syn match ctronVariant display '^[ \t]\+\zs[A-Z]\k*\ze[ \t]*\%($\|,\|[(\{]\)'

" ── 成员访问:先属性后方法(方法后定义,`.foo(` 时方法胜)────────
syn match ctronProperty display '\.\@<!\.\zs[a-z_]\k*'
syn match ctronMethod display '\.\@<!\.\zs[a-z_]\k*\ze[ \t]*('

" ── 运算符(§1.5;长运算符先行)────────────────────────────────
syn match ctronOperator display '=>\|->\|==\|!=\|<=\|>=\|+=\|-=\|\*=\|/=\|%=\|+%\|-%\|\.\.=\|\.\.\|&&\|[|][|]\|[+\-*/%<>!=?&]'
syn match ctronPipe display '[|]'
syn match ctronWildcard display '\<_\>'

" ── 注释(§1.2:仅行注释;/// 为文档注释)──────────────────────
" 必须定义在 operator 之后:`//` 与 `/` 同位时后定义者胜
syn match ctronLineComment display '//.*'
syn match ctronDocComment display '///.*'

" ── 非法标记(§1.3:禁用 ; 与 ::;无块注释;无 char 字面量;预留字)──
syn match ctronInvalid display ';'
syn match ctronInvalid display '::'
syn match ctronInvalid display '/\*'
syn match ctronInvalidChar display "'[^']*'"
syn keyword ctronReserved do async await interface module

" 多行区域仅注解一种;字符串为 oneline,同步开销低
syn sync minlines=100

hi def link ctronDocComment   Comment
hi def link ctronLineComment  Comment
hi def link ctronDerive       Define
hi def link ctronAnnotation   Define
hi def link ctronAttrName     Define
hi def link ctronString       String
hi def link ctronEscape       SpecialChar
hi def link ctronInterpVar    Identifier
hi def link ctronInterpOp     Operator
hi def link ctronInterpProp   Identifier
hi def link ctronInterpMethod Function
hi def link ctronNumber       Number
hi def link ctronConditional  Conditional
hi def link ctronRepeat       Repeat
hi def link ctronFnKw         Keyword
hi def link ctronKeyword      Keyword
hi def link ctronKwConst      Keyword
hi def link ctronKwStatic     Keyword
hi def link ctronLetInStatic  Keyword
hi def link ctronStructure    Structure
hi def link ctronBoolean      Boolean
hi def link ctronConstant     Constant
hi def link ctronSelf         Identifier
hi def link ctronOpWord       Operator
hi def link ctronFuncName     Function
hi def link ctronTypeName     Type
hi def link ctronConstName    Constant
hi def link ctronBuiltinType  Type
hi def link ctronPreludeType  Type
hi def link ctronTrait        Type
hi def link ctronVariant      Constant
hi def link ctronUserType     Type
hi def link ctronMethod       Function
hi def link ctronProperty     Identifier
hi def link ctronOperator     Operator
hi def link ctronPipe         Delimiter
hi def link ctronWildcard     Operator
hi def link ctronInvalid      Error
hi def link ctronInvalidChar  Error
hi def link ctronReserved     Error

let b:current_syntax = 'ctron'
