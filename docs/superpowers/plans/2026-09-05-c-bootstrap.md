# Ctron C 版:C5 自举路线(优先目标;C5a✅ … C5f✅ C5g✅ 本文件已交付,下一阶梯 C6 解析器差分)

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
3. **C5f(完成)**:Ctron 词法器输出 **Str 原始源切片** payload
   (引号含入,双方切同一段源码 → 转义/插值天然逐字一致),修复驱动在比对前 free 源码的
   悬垂;suite_diff 7 用例全过。
4. **C5g(本文件交付)**:`lex_num.ct` 数值分支**行级替换**补全 → 进制前缀(0x/0o/0b 仅小写,
   大写回落 0X10→Int+Ident)/下划线任意位/指数(e·E±digits,无点也成 Float)/**12 后缀全表**
   (i8·i16·i32·i64·isize·u8·u16·u32·u64·usize·f32·f64,词边界,payload 文本排除后缀),
   与 C `lex_number` 同表同切法;`input_num.ct` 全特征用例 → suite_diff 第 8 用例
   (payload 逐字)通过;随后 suite_diff 改模板换靶:**lex_num 对整个 tests/*.ct 语料逐文件
   payload 差分**(引号原切片/Ident/Int/Float 文本,51 文件全过)——Ctron 词法器与 C 版
   词法器在真实语料上达全字一致,是解析器差分的输入保证。ASan/UBSan 全绿。
5. **C6a(本文件交付)**:首个 Ctron **解析器差分**闭环 —— `parse_ast.ct`(纯 Ctron 递归下降):
   记号流(词=原文/数值=文本~后缀/标点=原始串,换行丢弃)+ Atomic 共享游标(语言钉子:
   List 索引写透 rt 缺陷 → 改 Atomic[I32] store/load 推进);语法子集 fn/类型化形参/
   Return 多句/尾表达式/调用实参 + 表达式全优先级阶梯(and→compare→range→additive→
   multiplicative→unary→postfix,镜像 C parser);输出 **C-AST v1 文本**(File/Fn/Block/
   Int{text,suffix}/Float/Binary/Call…)。`input_parse_ast.ct`(9 个 fn:进制/后缀/指数/
   下划线/一元/区间/布尔/嵌套优先级/调用实参)与 C `ctron_file_show` **逐字节一致**;
   suite_diff 第 9 用例(seq=3 解析差分)通过。钉子再记:字符串字面量内 `{` 需 `\{` 转义
   (否则按插值扫描)。60 用例全过;ASan/UBSan 全绿。
6. **C6b(下一阶梯)**:解析子集扩面(Let/If/Match/Str 部件/成员·索引/类型形参/测试声明
   …→ 全语法递归下降),再对真实语料做 parse-AST 差分;随后语义层、删 C 版,固定点为远期终点。

## 冻结纪律

自举代码会踩中语义含混(如 `08_bare.ct` 频次和 6 vs 4);每处先裁定再让 Ctron 代码使用,避免"自举期改语言"。
