" Ctron 文件类型检测(规范 §1.1;CTML/CTCL 见 vscode 扩展 package.json 的映射)
" 统一用 set filetype= 强制:setfiletype 在已有 ft(如 nvim 的 conf 兜底)时不生效
autocmd BufRead,BufNewFile *.ct set filetype=ctron
autocmd BufRead,BufNewFile *.ctml set filetype=ctml
autocmd BufRead,BufNewFile *.ctcl,Ctron.lock set filetype=ctcl
