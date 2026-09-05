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
16. **C6g(本文件交付)**:补洞五连 —— 尾后缀 `?`→Try、元组表达式/模式、NL 续行链(换行后 '.'
   则视为后缀续行)、attrs `#[...]`/comptime fn/TupleIndex(.0)、闭包/own 早前扩、
   prop 标签按容器(PropSig·PropImpl/Prop)、Simd[F32,4] 逗号 TypeArgs 消歧、use 花括号组、
   `\u{4E2D}` 解码钉(有限表映射)。**parse-AST 差分覆盖全部 49 个无诊断语料文件**
   (唯一排除 = E3030 语义诊断的 06_static_var),suite_diff 109 用例全绿;ASan/UBSan 全绿。
   至此 C6 解析器差分(全语法)达成固定点。
17. **C6h(本文件交付)**:parse 差分改**目录自动扫描门禁**(tests/*.ct 自动纳入;参考带诊断的
   01c_parse.neg/06_static_var 自动豁免),不再依赖手工名单;suite_diff 109 用例全绿,
   ASan/UBSan 全绿 —— C6(解析器差分)收口。
18. **C7(下一阶梯,语义层前置)**:…C7a(本文件交付)**:新模块 `selfhost/parsetree.ct` —— Ctron
   **结构化 AST 数据树**(节点 = List[Str],child[0]=tag,其后按模式放标量/子节点),解析建树、
   `pnode` 走树打印 → C-AST v1。v0 覆盖:File/Fn(形参 Named 类型)/Block(多句+尾)/
   Return·Let(var/let·带类型)·Assign/表达式全优先级(Binary·Unary·Range·Int·Float·Ident·括号);
   夹具 input_ptree.ct(5 fn)与 C `ctron_file_show` 逐字节一致;suite_diff 110 用例全绿。
   语义检查所需的可遍历数据树自此可用;C7b 按 parse_ast 语法面逐块扩树(if/while/for/
   match/字符串部件/结构体族…)。
17b. C7b①(完成):树版扩 if(else/else-if/BlockExpr)/While/For,夹具扩 7 fn。
18b. C7b②(完成):树版入 Str 部件(Text/Interp)/test 声明/调用·成员后缀。
18c. C7b③(本文件交付):树版入 match/模式(通配·字面量·Pascal 无载荷·元组/结构/绑定)、
   struct/enum 声明、Named 泛型实参(Option[I32]);语言钉:else 须与 } 同行
   (Ctron 解析要求);夹具扩 12 fn/test/enum/struct,与 C 逐字节一致(110 用例)。
18v. **C9b①(本文件交付)—— Ctron 求值器扩面(纯函数式树行走)**:`selfhosted/ev2.ct` 快照自
   ev_num.ct,尾部换为解释器:值 = List[I/B/S/V/R](I32/Bool/Str/void/range),env = 绑定列表
   (纯重建,无 List 索引写依赖),流控 flow = k/r/a(return 仅在激活内,调用点吞 r 只放行 a),
   表达式/语句/块统一线程 env·out。覆盖:Int/Bool/Str 字面量、Str{parts}(Text + `{expr}`
   Interp 重解析求值)、Ident、算术/回绕/比较/Eq/`&&` 短路、! / 一元负、成员 `.len`/`.to_string`
   /`.slice`、内建 print/println/assert/assert_eq/assert_ne/byte_at/byte_slice/panic、let/var
   (类型注解忽略,I32 域)、块作用域(块退出丢头部新增绑定)、Assign(= 与复合)、Return 展开、
   While、For(range 含 ..=)、If/else-if(语句与尾表达式)、递归函数调用。验收:suite_diff 新增
   **执行差分 oracle**:seq=5 主模式(ev2.ct 解释 input_ev2.ct 输出 vs C `ctron_rt_run_main`
   同输入,逐字一致),seq=6 测试模式(解释器跑 input_ev2t.ct 全过 / input_ev2tf.ct 断言失败
   消息,vs C `ctron_rt_run`),211 cases + suite_run 15 files 全绿;ASan/UBSan 绿。
   已裁定钉子:块退出后不再可见的遮蔽名与 C 帧语义一致(env 前缀丢弃);解释函数调用时
   `return` 不向外泄(与 C 宿主一致);数值字面量/运算按 I32 域(后缀/溢出用例不纳入夹具);
   浮动/Float/List/闭包/类/UFCS 等后续阶梯。
18w. **C9b②(本文件交付)—— Ctron 模块级检查升级为 C pkg 逐字 oracle + E6010 comptime 预算**:
   `pkg_chk.ct` 自 tok 顶层提取版改写为镜像 `src/pkg.c` 全流程:toml 解析(Ctron.toml 注释/引号/
   [package]/[caps]/[comptime] budget_ms)、逐文件诊断归属(rel=`src/<f>`)、C 同序
   (每模块组 E5010→E2020→E4010,再 E5020、E6010)、消息文本与 `ctronc pkg` 逐字一致
   (E5010 含 trait for type;E2020 未知模块/不可见项;E4010 含 &参数名;E5020 按 stem 排序 DFS
   根锚定;E6010 const 求值 steps/depth 预算,消息 `comptime 超出预算(budget):<名> 求值超限`)。
   E6010 求值器 = Ctron 纯 int 子集镜像 C ceval:Int/Ident(参数)/一元 Neg/二元加減乘除模/单
   Return 块与尾表达式/comptime fn(FnC)递归;steps 超 budget_ms*10000(缺省 1e6)或深度超限
   → E6010;不可估节点静默跳过不误报。验收:suite_diff 新增 **seq=8 模块级 oracle**:
   pkg_chk.ct 模板按 7 个包目录换靶(orphan/circular/visibility/caps/comptime_budget 负例 +
   use_ok/ffi_math 行为零误报),与 C `ctron_pkg_check` 非 JSON 文本逐字一致(218 cases 全绿;
   make test + suite_run 15 files;ASan/UBSan 绿)。已裁定钉子:Ctron 侧 comptime 递归深度预算
   降至 40(宿主解释器每层≈多 C 帧,ASan 栈保护;语料 spin 仍必中,与 C 差分不受影响);
   comptime fn 跨文件检索暂限同文件(语料单文件;跨文件后续阶梯);E5020 环检测免于全局
   done 剪枝(语料二模块环结果与 C 相同)。18x. **C9c(本文件交付)—— 统一 cc 驱动(parse→单文件语义 12 项→run 单入口)**:新模块
   `selfhosted/cc.ct`(自足快照 4.8k 行):把 sem_chk.ct 的 parser+pnode+单文件语义 12 项
   (sem_walk2)与 ev2.ct 的解释器段(vI..run_block/run_tests)并入一文件(函数零冲突;去重
   保留各自所需 seq2/txt_num 预置)。管线:parse(树)→ sem_walk2:有诊断 → 逐条输出
   "CODE: msg" 并返回 1(不运行);无诊断 → 解释执行 fn main,否则 test 块(全过 0,
   断言失败打印消息返 1)。验收:suite_diff 新增 **seq=9 cc 驱动差分** —— 正例
   input_cc.ct(Ctron 子集主程序,`ctronc check` 0 诊断)输出与原生运行逐字一致(rc 0);
   负例 input_cc_neg.ct(class+struct 引用字段 → W8010)输出与 C sem 行逐字一致(rc 1)。
   suite_diff 220 cases + suite_run 16 files 全绿;ASan/UBSan 绿。已裁定钉子:Ctron rt 允许
   Str+Str `+`(运行域)而 C sem 判 E2010 —— 语义域不一致,cc 正例夹具避开;cc 语义阶段只
   报 Ctron 已实现 12 码(其余码原样下钻运行时,与 C check 的差异由后续单文件语义扩面覆盖)。
18y. **C9d①(本文件交付)—— Ctron 求值器运行域对齐:match + 数组**:`ev2.ct` 补值域 kind "A"
   (数组字面量求值)、`Index` 读(越界/非数组 panic)、`for x in xs` 迭代数组(逐元素绑定,退出
   丢绑定)、Member `.len`(A 计元素数);求值器 `match`:PatWild/PatLitI/PatLitS/PatLitB 逐字量
   比较 + PatId 绑定,臂表达式求值(支持 BlockExpr 臂内赋值),无匹配 panic(镜像 C rt
   "match 无匹配臂");`fmt` 未知名值对齐 C 默认 `<value>`(非 I/B/S/V)。验收:suite_diff
   新增 seq=5 用例 input_ev2b.ct(数组字面量/len/索引/for 区间叠加 for-over-array/if/match 于
   I32·Bool·Str 臂)与原生 `ctron_rt_run_main` 逐字一致;suite_diff 221 cases + suite_run 16 files
   全绿;ASan/UBSan 绿。已裁定钉子:Ctron 宿主无 `||`,模块代码勿写(Ctron 语言钉子复查);
   `cc.ct` 快照未含本批(下一轮并入)。
18z. **C9d②(本文件交付)—— Ctron 求值器 Option/Result 域:tag 值 + 变体构造 + 模式绑定 + `?` Try**:
   `ev2.ct` 补值 kind "T"(tag:payloads),Ident 裸 `Some/None/Ok/Err` 解析为 tag 值,call_id
   变体构造(载荷求值),match 模式 PatAgg(SubUnit 无载荷 / SubTup 逐元素绑定/字面量/通配),
   Try:`None|Err` → 提前返回整 tag(r-flow 经调用点转返回值);`Some|Ok` → 解开载荷。验收:
   suite_diff 新增 seq=5 用例 input_ev2c.ct(helper 返回 I32? + `?` 传播 + main match
   Some(x)/None,裸 None 变量)与原生逐字一致;suite_diff 222 cases + suite_run 16 files 全绿;
   ASan/UBSan 绿。注意:仓库 Makefile 被并行 C10-a 流临时接上未落盘的 tests/suite_trans.c,
   整体 `make test` 暂不可用(其 WIP);本批验收=逐 suite 构建运行。
19a. **C9d③(本文件交付)—— Ctron 求值器 struct/枚举域**:`ev2.ct` 补值 kind "U"(类型名+字段名/值对):
   StructLit 求值(逐字段,线程 env/out)、Member 字段读(u_field)、Assign 成员写(含复合 op;env_set
   重建 U,值拷贝语义镜像)、match struct 模式(PatAgg SubSt 绑定/嵌套子模式);用户枚举 unit 变体:
   Ident 裸变体名(扫 Enum decl 变体表)→ [T,name],match SubUnit。验收:suite_diff 新增 seq=5
   用例 input_ev2d.ct(struct 字面量/拷贝/字段写/struct 模式/枚举 unit 变体与函数传参/尾 return
   match)与原生逐字一致;suite_diff 223 cases + suite_run 16 files 全绿;ASan/UBSan 绿。
   注:并行 C10-a(trans)流 Makefile WIP 仍阻整体 `make test`,验收=逐 suite 构建运行。
19b. **C9e①(本文件交付)—— cc 快照刷新至 C9d 求值器 + UFCS**:统一驱动 `cc.ct` 重新自
   sem_chk(parser+语义 12 项)+ ev2(解释器段,含 C9d 系列 match/数组/Option/struct/enum)
   拼装(函数零冲突);seq9 新增富程序正例 `input_cc2.ct`(数组/for-over-array/区间/match/
   Option `?`/struct 字段/枚举 unit + 尾 return match,经 cc 全管线与原生逐字一致)。
   求值器再补 **UFCS**:`x.f(args)` 找不到内建方法时按文件自由 fn `f` 以接收者为首参调用
   (镜像 C rt file_fn 收参);新 seq5 用例 `input_ev2e.ct`(21.double()/6.scale(7)/len)。
   suite_diff 225 cases 全绿;ASan/UBSan 绿。
19c. **C9e②(本文件交付)—— Ctron 求值器闭包/fn 值**:`ev2.ct` 补 Ident 裸函数名 → fn-ref
   值 ["F",name](镜像 C file_fn)、闭包字面量求值 ["C",cp,body,capEnv](镜像 V_CLOSURE 捕获),
   call_cv 调用路径(env 绑定 C/F 值 → 以 fn 名找 decl/绑定参后 eval 闭包体);Ctron 语料
   fn 类型参数 `fn(I32)->I32` 接收具名 fn 与闭包字面量,多参闭包适配。验收:suite_diff 新增
   seq5 用例 input_ev2f.ct(apply_twice(double|闭包)/combine 多参/闭包变量调用/捕获外层局部)
   与原生逐字一致;suite_diff 226 cases 全绿;ASan/UBSan 绿。
19d. **C9f①(本文件交付)—— Ctron 求值器 List 域(引用语义)**:`ev2.ct` 补值种类 "L"
   (客体 List 直接持有宿主 List 对象 → push 原地变、别名可见,镜像 rt.c listnode;布局
   ["L", item…])。覆盖:`List[T]()` / `arena.list[T]()` 构造(call 路径 TypeArgs 换靶,
   同 rt 同参忽略恒空表)、`.push(v)`(返回 void)、`.len`、索引读(A/L 同轨)、
   索引赋值 `a[i] = v` 与复合 `a[i] op= v`(新增 Assign Index 目标分支;L 经宿主 List
   索引写写透共享后备,镜像 rt.c ST_ASSIGN 的 V_LIST/V_ARR 写透)、`.into_gc()` 深拷贝
   (vdeep:标量逐槽重建/嵌套 L 递归,镜像 clone_val)。已裁定钉子:①宿主 for-over-list
   不支持(rt 仅 range/数组/tuple),求值器不同步,夹具避开;②println(List) → `<value>`
   (fmt 默认轨,与 C fmt_val default 一致);③客体越界走宿主 panic,seq=5 参考侧需
   RT_OK,夹具不含 OOB。验收:suite_diff 新增 seq5 用例 input_ev2g.ct(len/求和/索引写
   复合写/别名 push 可见/into_gc 隔离/Str 元素/空表)与原生逐字一致;suite_diff 227
   cases 全绿;其余套件全绿;ASan/UBSan 绿。注:本批期间并行 C10 流 trans.c WIP 短暂
   阻断 make build/suite_diff,验收用 HEAD 稳定版 trans.c 链接验证(不影响结论)。
19e. **C9f②(本文件交付)—— Ctron 求值器 trait/impl 方法域**:`ev2.ct` 补类型底链名
   ty_head(Named/Ref/Slice/Optional)与四个检索器:find_trait/find_is_class/find_impl_method/
   find_impl_prop(镜像 rt.c cls_method/cls_prop:逐 Impl 扫 items → 本 impl trait_ty 的
   trait 默认体(须有 body);多 impl 按文件序续扫)。方法/prop 调用 call_method_vals/
   call_prop_vals:self 绑定 + Param 具名参数(Method 节点体在 [5],Receiver 不耗实参)。
   分发序镜像 rt:内建方法 → 类实例(U+is_class)方法(找不到即 panic 不落 UFCS) → UFCS;
   Member 读序:字段 → impl prop(仅类)→ panic。已裁定钉子:①宿主解析器无 inherent
   impl(`impl Type {}` 不支持,夹具用 trait impl 形式);②self 可变方法(方法内改字段
   外部可见)依赖类/结构体写语义区分(类原地写/结构体重建),不在本批(挂账 C9g);
   ③to_string/slice 内建未按接收者种类门控(与 rt 的门控差异,夹具避开)。
   验收:suite_diff 新增 seq5 用例 input_ev2h.ct(仿 03d 语料:trait Clock/Named/Env
   默认 describe 内 self.name prop + self.now() 方法互调/空 impl/env.now()·env.name·
   env.describe()/Tagged 多参方法 label(6))与原生逐字一致;suite_diff 228 cases 全绿;
   suite_run 16/suite_rt 34 全绿;ASan/UBSan 绿。
19f. **C9g(本文件交付)—— 绑定克隆与原地写(self 可变方法前置)**:镜像 rt.c
   env_let(结构体值非类深克隆入槽;类/List/标量保引用)与 ST_ASSIGN(成员写经绑定槽
   原地生效)。`ev2.ct` 新增 bind_of(Let/call_decl_vals/call_method_vals 自绑定与形参/
   Assign Ident 右值统一走克隆决策)与 u_set_ip(宿主 List 索引写原地改字段);成员写
   从 env_set 重建改为原地写(结构体绑定时已克隆 → 原地≡重建;类共享 → 别名可见,
   self 可变方法由此可用);vdeep 重写为按种类分发(U 域名/值交错递归、A/L 值槽递归、
   标量/T/F/C 逐槽拷贝不窥探)。钉子:①字符串载荷不可索引(v[1] 类裸槽它[0] 即
   宿主 rt panic"索引目标非数组"—— 本批踩中并修复,初版 vdeep 对标量载荷做 it[0]
   窥探所致);②for 迭代绑定不克隆(镜像 rt ST_FOR 直接棗写);③结构体字段写时若
   右值为结构体,rt 会克隆(774 行)—— ev2 暂按引用(夹具避开,挂账)。
   验收:suite_diff 新增 seq5 用例 input_ev2i.ct(结构体克隆独立/结构体参数值语义
   touch 不外泄/类别名 z-w 互见/方法内 self.v 写外部可见/复合成员写)与原生逐字一致;
   suite_diff 229 cases 全绿(O2 与 ASan 同数,消除构建时序假象);其余套件全绿。
   注:C9g 代码因并行竞态被卷入 40cdd0c(P1-D 提交)落库,本条为设计裁定与验收留痕;
   竞态双向版详见交接文档 19g 记录。
20a. **C9h(本文件交付)—— cc.ct 快照刷新至 C9g + 独立驱动 cc.sh(独立工具链第一步)**:
   ①`cc.ct` 重拼:语义段(C8a–E3040 的 12 项 sem_walk2)不动,解释器段整体换入当前
   ev2.ct(C9b①→C9g 全量:match/数组/Option·Result·Try/struct·enum/UFCS/闭包·fn 值/
   List 引用语义/trait·impl 方法/绑定克隆与原地写),前缀逐字节同源、函数零重名
   (155 fn/5488 行);②新增 `selfhosted/cc.sh` 独立驱动:任意 .ct 输入换靶 → 宿主 seed
   解释执行,正例运行/负例编译期拦截(rc=1),`selfhosted/` 自此自带工具链入口
   (宿主仅作 Ctron 解释器,自举完成后可自替换);③新增 seq9 夹具 input_cc3.ct
   (多 impl 分发/方法内 self 复合写外部可见/List 全套/fn 类型参数+闭包/Option match),
   C check 0 诊断,cc 全管线输出与原生逐字一致。验收:suite_diff 230 cases
   (O2/ASan 同数)+ suite_run 16 + make test 端到端恢复绿(并行 trans 已 12/12)。


## 自举产物目录

Ctron 实现的编译器模块统一在**项目根目录 `selfhosted/`**(lex_*/parsetree/parse_ast/sem_chk/pkg_chk + 夹具 + README)进行;`compiler_c/selfhost` 已删除,suite_run/suite_diff 直接以 `../selfhosted` 为模块根。C 版(compiler_c/src)仅作宿主与 oracle,保留不清理。

## 交接

新 session 请先读 **`docs/superpowers/plans/2026-09-05-c-bootstrap-HANDOFF.md`**(仓库/命令/战略口径/Ctron 语言钉子/下一批任务),再回到本计划文档里程碑(18a–18u)与 git log。

## 冻结纪律

自举代码会踩中语义含混(如 `08_bare.ct` 频次和 6 vs 4);每处先裁定再让 Ctron 代码使用,避免"自举期改语言"。
