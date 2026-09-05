# Ctron C 版:C5 自举路线(优先目标;C5a✅ C5b✅ C5c 种子✅)

> 战略转向:自举优先。参照 §14.1 阶梯:ctron0-C 逐步接受"用 Ctron 写的编译器模块",逐字差分(61 文件金丝雀),最终 ctronN 编译自己。
> 判定口径:自举"开始" = **第一个 Ctron 写的编译器模块能在本工具链运行且输出与 C 版逐字一致**。

## C5a 自举入口(✅ 本文件已交付)

- `ctronc run <file>`:执行 Ctron `fn main`,输出进 stdout,退出码 = main 返回值。
- Ctron 侧运行内置(与语料测试解释器同构):`read_file(Str) -> Str?`(Option)、`print/println(Str)`、整数/布尔 `.to_string()`、`+` 字符串拼接。
- `selfhost/hello.ct`:`fn main` 读 `tests/01_basics.ct`、`.len`/`.to_string()`/拼接/match/print 全链路。
- `suite_run`:运行 `selfhost/*.ct` 的 main,核对退出码与输出;八件套 + ASan 全绿。

## 已交付(C5b/c 种子)

- **字节原语**:Ctron 可见 `byte_at(Str, i)`/`byte_slice(Str, a, b)`;配合已有 `.len`/`.slice`/`to_string`/`+`。
- **第一个 Ctron 词法器** `selfhost/lex_small.ct`(纯 Ctron:字节迭代 + 字符分类(德摩根 or2)+ 注释/空白/记号计数),`fn main` 读输入并打印 `tokens=N`。
- **差分套件 `suite_diff`**:Ctron 词法器计数 vs C 版 `lex` 计数逐字比对;首个用例通过
  (`let x = 42 + foo // tail` → 7 = 7)——**§14.1 流式移植的第一个闭环信号达成**。
- 修复:Ctron 语法无 `||`(德摩根 or2);`rt_run_main` 未设 `R.f`(NULL 段错)。

## 自举阶梯(续)

1. **C5c 扩大词法器面**:字符串/插值/进制后缀/换行过滤,差分从计数扩到 **token 流序列**;输入从自建样例扩到 `tests/*.ct`。
2. 之后:解析器(输出 AST 文本 vs C-AST v1)、类型检查… 照此逐个移植+差分+删除 C 版;完整固定点(ctronN 自编译逐位一致)为远期终点。

## 冻结纪律

自举代码会踩中语义含混(如 `08_bare.ct` 频次和 6 vs 4);每处先裁定再让 Ctron 代码使用,避免"自举期改语言"。
