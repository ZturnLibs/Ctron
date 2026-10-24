" CTML filetype plugin
if exists('b:did_ftplugin')
  finish
endif
let b:did_ftplugin = 1

" /// 文档注释与 // 行注释(长条目在前,回车续行得到 ///)
setlocal comments=:///,://
setlocal commentstring=//\ %s
setlocal formatoptions-=t formatoptions+=croql
setlocal iskeyword+=-

let b:undo_ftplugin = 'setl com< cms< fo< isk<'
