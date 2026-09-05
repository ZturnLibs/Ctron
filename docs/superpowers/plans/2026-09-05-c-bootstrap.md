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
6. **C6b①(本文件交付)**:解析子集扩至常用语句/表达式 —— 换行改记号为 "NL" token(否则
   无法区分 Expr 语句与尾表达式,钉子:C 以换行定语句边界),parse_block/file 按 C skip_newlines
   位点消费;新增 Assign(含 += -= *= /= %=)/While/For/If(else/else-if/尾表达式)
   语句,成员 .m、索引 [i]、多参调用后缀;扫描器补复合赋值最长匹配。input_parse_ast.ct 扩至
   17 fn(member/index/复合赋值/循环/if 三态/else-if 链/Expr(If) 语句),与 C `ctron_file_show`
   **逐字节一致**;suite_diff 60 用例全过;ASan/UBSan 全绿。
7. **C6b②/③(完成)**:字符串字面量部件(Text 透传=qstr 互逆,\{ 坍缩;Interp 取 {} 内原始切片;
   \u 钉待办)+ test 声明 + 类型扩面(Named+泛型实参/Optional?/Ref&/fn 类型);input_parse_ast.ct
   扩至 24 fn/test,均与 C `ctron_file_show` 逐字节一致。
8. **C6c(本文件交付)**:首个**真实语料 parse-AST 差分** —— parse_ast.ct 补 +%/-%(WrapAdd/WrapSub)
   与 void 关键字后,整文件解析 `tests/01_basics.ct` 与 C 输出**逐字节一致**;suite_diff 增 parse
   corpus 换靶 3 文件(01_basics/03c_str_string/08_bare_alloc.neg)全过 → 63 用例全绿。全 51 文件
   扫描:3 过、余失败主因 = **as[T]/Box[T]/List[T] 等类型实参后缀消歧**、match/模式、
   struct/class/enum 声明族、own/scope 等。ASan/UBSan 全绿。
9. **C6d(本文件交付)**:类型实参后缀消歧 + 值语法补全 —— postfix `[` 前探匹配 `]` 后随
   `(`/`{` → TypeArgs(实参按 parse_type,整型字面量 → ComptimeVal("N")),否则 Index;
   另补 true/false→Bool、`{` 块作表达式 → BlockExpr、pub 可见性、tok() 越界守卫返
   #EOF(防非法输入 OOB/死循环,钉子)。真实语料 parse 差分扩至 **6 文件全过**(00_doctest/
   01_basics/03b/03c/04b/08_bare_alloc.neg),suite_diff 66 用例全绿;ASan/UBSan 全绿。
   余下失败主因:match/模式、struct/class/enum/trait/impl/own/scope 声明族。
10. **C6e(本文件交付)**:match 表达式与模式(镜像 C parse_match/parse_pattern)—— 扫描器补
   `=>`;模式:_ → Wildcard、Int/Float/Str/Bool 字面量 → Lit(...)、PascalCase 无载荷变体 →
   Agg{path,Unit}(先判 ( 与 {)、Some(v)/Err(e) 元组变体 → Agg{...,Tuple(...)}、Pt{ x }/
   Pt{ x: p } 结构模式 → Agg{...,Struct([StructPatField...])}、小写 → Ident 绑定;
   match 臂 NL 分隔、臂表达式含 if/调用/块;fixture(match 作语句/return-if/臂内 if/嵌套
   match/字面量+通配+结构+Option 模式)与 C-AST v1 **逐字节一致**;suite_diff 66 用例全绿。
11. **C6f①(本文件交付)**:类型后缀补全 + 值语法扩面 —— parse_type 拆分 base+type_postfix:
   `I32[]`→Slice、`I32[3]`/`I32[N]`→Array{elem,size}(定长数组 size 为表达式,含
   `[]`空/单整型消歧)、`&I32[]`→Ref(Slice(...));表达式加数组字面量 `[1,2,3]`→Array(...);
   let/for 绑定改用 parse_pattern(元组 `let (tx, rx) = ...` 等);tok 越界守卫防 OOM。
   parse 语料差分 6→**9 文件**(+03f_slices/05f_must_use.lint/08_bare),suite_diff 69 用例全绿。
12. **C6f②(本文件交付)**:声明族落地 —— struct(字段/pub/@derive→derives)、enum(Unit·
   Tuple 载荷·Struct 变体)、use(点分路径)、const、static(let/var)、fn/struct/enum 类型形参
   (`[T: Show + Eq]`→TypeParam{name,bound,is_comptime})、(T,T) 元组类型、表达式结构字面量
   StructLit{path,fields};decl_kind 前探(pub/@derive/NL)分发,attr 行后 NL 消费等钉。
   parse 语料差分 9→**16 文件**(+02e_match_patterns/02_match_exhaustive/05i_deep_cause/
   01_overflow/02d_divzero/03h_utf8/10_web_dom 等),suite_diff 76 用例全绿;ASan/UBSan 全绿。
13. **C6f③(本文件交付)**:class/impl 声明族 + receiver 方法 —— parse_fn 重构为共享
   parse_fndecl(tag: Fn/Method),方法收参识别 `var self`/`let self` → Receiver{is_var};
   class(items: Field/Method)/impl Trait for Type 落地(impl 含方法体)。
   parse 语料差分 16→**21 文件**(+03_values_refs/03e_generics_types/05d_drop/
   03_shallow_copy/06c_static_nonsend),suite_diff 81 用例全绿;ASan/UBSan 全绿。
14. **C6f④(本文件交付)**:闭包 `|x| expr`/`|a: T, var b| -> R expr`(ClosureParam{is_var,ty}/
   ret 可选)与 `own(arena) {}` 块(Own{arena,body})表达式;解锁 03g_fn_types/05_own*
   /05e/05g/06d_globals 等。parse 语料差分 21→**28 文件**,suite_diff 88 用例全绿。
15. **C6f⑤(本文件交付)**:trait 声明(supers/无体 Method/PropSig·PropImpl)、receiver
   `&self`、scope { |s| } 块;解锁 05b_panic_join/06_spawn_nonsend/07_capabilities 等。
   parse 语料差分 28→**31 文件**,suite_diff 91 用例全绿;ASan/UBSan 全绿。
16. **C6g(下一阶梯)**:`\u{...}` 解码钉、解包 `?` 后缀、函数值属性(#[pure] attrs 表)、
   if-let/守卫、剩余 03d/06_concurrency/06f/04_generics 等约 19 文件 → 全语料 parse 差分。

## 冻结纪律

自举代码会踩中语义含混(如 `08_bare.ct` 频次和 6 vs 4);每处先裁定再让 Ctron 代码使用,避免"自举期改语言"。
