" Ctron filetype plugin
if exists('b:did_ftplugin')
  finish
endif
let b:did_ftplugin = 1

" §1.2:仅行注释;/// 为文档注释(长条目在前,回车续行得到 ///)
setlocal comments=:///,://
setlocal commentstring=//\ %s

setlocal formatoptions-=t formatoptions+=croql

" 模块解析:use 路径按包名查找 .ct
setlocal suffixesadd=.ct

" Ctron 禁用 ';' 与 '::'、无 char 字面量:matchpairs 保持默认 ( ) { } [ ],
" 不要加入 ' 与 |(后者是闭包界定符)

let b:undo_ftplugin = 'setl com< cms< fo< sua<'
