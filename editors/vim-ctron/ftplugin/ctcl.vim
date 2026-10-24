" CTCL filetype plugin
if exists('b:did_ftplugin')
  finish
endif
let b:did_ftplugin = 1

" 仅 // 行注释(CTCL 无 ///)
setlocal comments=://
setlocal commentstring=//\ %s
setlocal formatoptions-=t formatoptions+=croql

let b:undo_ftplugin = 'setl com< cms< fo<'
