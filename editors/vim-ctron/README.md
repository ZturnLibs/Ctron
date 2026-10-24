# editors/vim-ctron — Ctron 的 Vim/Neovim 插件

为 `.ct` 源文件提供语法高亮、缩进与文件类型支持的零依赖 Vimscript 插件。
规则权威依据与 VSCode 扩展相同:规范 `docs/spec/01-lexical-grammar.md` §1.2–§1.6;
语法规则集与 `editors/vscode-ctron/syntaxes/ctron.tmLanguage.json` 对齐。

## 功能

| 类别 | 内容 |
|---|---|
| 语法高亮 | §1.3 关键字全集(含 `break`/`continue`、预留操作字 `or`)、预留字 `do async await interface module` 标红;`///` 文档注释;字符串插值 `{expr}` 与转义 `\{`/`\u{HEX}` 区分;`0x/0o/0b`、`_` 分隔、整型/浮点后缀;内建类型、前奏类型、trait、PascalCase 启发式;`#[...]`/`@derive(...)`;`fn`/`struct`/`const`/`static let` 声明名;枚举变体行首启发式;方法调用 vs 属性访问 |
| 非法标记 | `;`、`::`、`/*`、`'…'` 字符字面量、预留字 → Error 色(与编译器 E1001 族一致的第一眼反馈) |
| 缩进 | §1.6 换行终止规则:行尾延续集 `+sw`、首点式链式 `.` 行 `+sw`、`}`/`)`/`]` 对齐开启行、尾随逗号平级 |
| ftplugin | `comments=:///,://`(回车续行得到 `///`)、`commentstring`、`suffixesadd=.ct` |

## 安装

要求 Vim ≥ 8.2 或 Neovim ≥ 0.7(纯 Vimscript 9 向下兼容写法,两端通用)。
本机(2026-10-24)已按以下方式安装:

**Neovim(LazyVim)**:`~/.config/nvim/lua/plugins/ctron.lua` 本地 spec

```lua
return {
  {
    dir = "~/Zturn/Ctron/editors/vim-ctron",
    name = "vim-ctron",
    lazy = false,
    init = function()
      vim.filetype.add({ extension = { ct = "ctron" } })
      vim.api.nvim_create_autocmd({ "BufRead", "BufNewFile" }, {
        pattern = "*.ct",
        group = vim.api.nvim_create_augroup("ctron-ft", { clear = true }),
        callback = function(args)
          if vim.bo[args.buf].filetype ~= "ctron" then
            vim.bo[args.buf].filetype = "ctron"
          end
        end,
        desc = "ctron: 强制 .ct 的 filetype",
      })
    end,
  },
}
```

> 必须 `vim.filetype.add`/强制兜底:`.ct` 无内建规则时 nvim 会兜底识别成 `conf`,
> 而 `setfiletype` 对已有 ft 不生效。lazy.nvim 托管 rtp,pack 目录的 ftdetect 不会被
> 纳入检测链,故用 spec 而非 pack 链接。

**Vim**:`ln -sfn <仓库>/editors/vim-ctron ~/.vim/pack/ctron/start/vim-ctron`

> 前提:macOS 的系统 `/usr/share/vim/vimrc` 会阻止 `defaults.vim` 加载,
> `filetype`/`syntax` 默认全关 —— 需要最小 `~/.vimrc`:

```vim
set nocompatible
filetype plugin indent on
syntax on
```

**其他形态**:vim-plug 写 `Plug '<仓库>/editors/vim-ctron'`;或手动把
`syntax/ ftdetect/ ftplugin/ indent/` 并入 `~/.vim/`(或 `~/.config/nvim/`)。

`ftdetect/ctron.vim` 用 `set filetype=`(而非 `setfiletype`)强制声明 `.ct`,
避免被其他插件的既有映射抢先。

## LSP(诊断/补全/跳转,与 VSCode 同一后端)

高亮是静态的;诊断、补全、hover、rename 来自 Ctron 实现的 LSP
(`lsp/src/main.ct`,经 `editors/vscode-ctron/server/ctron-lsp.sh` 启动)。
任意通用 LSP 客户端可直接复用,例如 Neovim ≥ 0.11 内置客户端:

```lua
-- ~/.config/nvim/lua/ctron-lsp.lua;需先 build compiler-c 得到 ctronc
vim.lsp.config.ctron = {
  cmd = { vim.fn.expand('<仓库>/editors/vscode-ctron/server/ctron-lsp.sh') },
  filetypes = { 'ctron' },
}
vim.lsp.enable('ctron')
```

Vim 可用 `vim-lsp`/`coc.nvim` 指向同一脚本。

## 已知限制(与 VSCode 版对齐的启发式)

- 枚举变体、PascalCase 类型为行首/词形启发式,非常量语境下的误判与 TextMate 版一致;
- 缩进是启发式而非 formatter(`ctron-fmt` 落地前无唯一形态契约);字符串赋值行尾 `=`
  会被视为延续集(无法在不解析的前提下区分);
- `module` 在规范 §1.3 列为预留字,但 `pkgs/ctron/plugin.ct` 等第一方代码已用其作绑定名;
  本语法与 TextMate 版一样按预留字标红(等编译器侧裁决后统一调整);
- **CTML(`.ctml`)/ CTCL(`.ctcl`/`Ctron.lock`)已支持**(2026-10-24):
  `syntax/ctml.vim`(use/view/style/标签属性/`{expr}` 内嵌完整 ctron 语法,
  经 `syn include` 复用 `syntax/ctron.vim`)与 `syntax/ctcl.vim`(块头/字段/严格
  整数与转义/六类错法标红);差异:view 头部单色(TextMate 对 props 内 ctron
  细粒度着色),snippets 与补全不随附(vim 侧无对应机制,LSP 亦不服务这两种语言)。
