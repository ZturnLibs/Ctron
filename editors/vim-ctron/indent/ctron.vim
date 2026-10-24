" Ctron indent:规范 §1.6 换行终止规则的务实实现
" - 行尾延续集(, = -> => && || or .. ..= 算术/比较 ( [ { |)→ 下一行 +sw
" - 行首 . 永远是续行(首点式链式)→ +sw
" - 当前行以 } ) ] 开头 → 对齐其块/括号的开启行
" - 尾随逗号 → 与上一行平级(字段/参数/变体列表)
" 依赖 syntax 高亮区分注释与字符串(syntax on 时准确;否则尽力而为)。
" 注意:这是缩进启发式,不是 formatter;formatter 输出唯一形态见规范 §1.6。

if exists('b:did_indent')
  finish
endif
let b:did_indent = 1

setlocal indentexpr=CtronIndent()
setlocal indentkeys=0{,0},0),0],0.,!^F,o,O,=else

let b:undo_indent = 'setl inde< indk<'

if exists('*CtronIndent')
  finish
endif

" 字符是否落在注释/字符串/插值内(跳过,避免误判行尾延续符)
function! s:InStringOrComment(lnum, col) abort
  let l:name = synIDattr(synID(a:lnum, a:col, 1), 'name')
  return l:name =~ 'ctronComment\|ctronString\|ctronInterp\|ctronEscape'
endfunction

" 行尾最后一个"代码"字符(自右向左跳过空白/注释/字符串)
function! s:TailChar(lnum) abort
  let l:text = getline(a:lnum)
  for l:i in range(strlen(l:text) - 1, 0, -1)
    if l:text[l:i] =~ '\s'
      continue
    endif
    if s:InStringOrComment(a:lnum, l:i + 1)
      continue
    endif
    return l:text[l:i]
  endfor
  return ''
endfunction

" 上一条"代码"行(跳过纯注释行)
function! s:PrevCode(lnum) abort
  let l:l = prevnonblank(a:lnum - 1)
  while l:l > 0 && s:InStringOrComment(l:l, match(getline(l:l), '\S') + 1)
    let l:l = prevnonblank(l:l - 1)
  endwhile
  return l:l
endfunction

" 当前行首字符对应的开启行缩进(} ) ] → 配对开启行);未配对返回 -1
function! s:PairIndent(lnum, open, close) abort
  let l:save = getpos('.')
  call cursor(a:lnum, 1)
  let l:skip = 'synIDattr(synID(line("."),col("."),1),"name")=~"ctronComment\\|ctronString\\|ctronInterp\\|ctronEscape"'
  let l:ln = searchpair(a:open, '', a:close, 'bWn', l:skip)
  call setpos('.', l:save)
  return l:ln > 0 ? indent(l:ln) : -1
endfunction

function! CtronIndent() abort
  let l:cur = getline(v:lnum)
  let l:first = matchstr(l:cur, '^\s*\zs\S')
  let l:plnum = s:PrevCode(v:lnum)
  if l:plnum <= 0
    return 0
  endif
  let l:base = indent(l:plnum)

  if l:first =~# '[})\]]'
    if l:first ==# '}'
      let l:ind = s:PairIndent(v:lnum, '{', '}')
    elseif l:first ==# ')'
      let l:ind = s:PairIndent(v:lnum, '(', ')')
    else
      let l:ind = s:PairIndent(v:lnum, '\[', '\]')
    endif
    let l:base = l:ind >= 0 ? l:ind : max([l:base - shiftwidth(), 0])
    return l:base
  endif

  " §1.6 规则 2:行首 . 是前一行表达式的继续
  if l:first ==# '.'
    return l:base + shiftwidth()
  endif

  let l:tail = s:TailChar(l:plnum)
  if l:tail ==# ','
    " 尾随逗号:字段/参数/变体列表的兄弟行,平级
    return l:base
  endif
  if l:tail =~# '[=><&|+*/%.([{-]'
    " §1.6 延续集(单字符覆盖 -> => >= ..= 等;? 不在延续集)
    return l:base + shiftwidth()
  endif
  if getline(l:plnum) =~# '\<or[ \t]*\%(//.*\)\?$'
    return l:base + shiftwidth()
  endif
  return l:base
endfunction
