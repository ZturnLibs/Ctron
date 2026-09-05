# Ctron C 版:C5 自举路线(优先目标;C5a✅ C5b✅ C5c✅ C5d①✅ C5d②✅ C5e①✅)

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

1. **C5c 已完成**:Ctron 词法器 v2(`lex_kind.ct`,纯 Ctron:关键字表/整数/多字符运算符最长匹配/标点/注释/`_`)输出**逐记号种类序列**,与 C 版 `lex` **逐字比对通过**(计数模式 + 种类序列模式两用例)。顺带暴露三条 Ctron 语言钉子:无 `\|\|`(用德摩根)、无 `continue`(预留字)、语句需换行分隔。
2. **C5d(完成)**:Ctron 词法器 v4(`lex_corpus.ct`)——全集关键字/复合赋值与比较运算符/**字符串字面量**(test 名等)/注释/**§1.6 换行过滤**(前导 K-1、连续折叠、延续集双向抑制),先扫后滤两遍(List[Str]+List 运行支持);对真实语料 `tests/05_own.ct` **全记号种类序列逐字一致**。语言钉子再记:无 `break`(预留字)。差分用例扩至 4(计数/数值/运算符/真实语料)。
3. **C5e(①已完成)**:Ctron 词法器加**浮点消歧**(`.`+数字→Float;`21.double()` 回落 Int+Dot)、浮点后缀 f32/f64 词边界收编;浮点综合输入**种类序列逐字一致**。C5e②:指数/进制小数、payload 级文本比对,差分输入扩到更多语料直至全 61。
3. 之后:解析器(输出 vs C-AST v1 文本)、类型检查… 照此逐个移植+差分+删除 C 版;完整固定点为远期终点。

## 冻结纪律

自举代码会踩中语义含混(如 `08_bare.ct` 频次和 6 vs 4);每处先裁定再让 Ctron 代码使用,避免"自举期改语言"。
