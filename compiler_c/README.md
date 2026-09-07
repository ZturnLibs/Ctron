# Ctron 编译器 C 版(C1 词法 ✅ / C2 解析 ✅ / C3 语义层 ✅ / C4-a…i 解释器 ✅(34 文件全量运行) / C10 转译后端 ✅(34/34 语料原生差分))

> **分支**:`discuss-c-implementation`。**决策记录**:Ctron 存在两套独立、各自完整的编译器实现——Rust 版(`compiler/`)与 C 版(`compiler_c/`),互不依赖;两者共享**语言设计**(`docs/superpowers/specs/…ctron-language-design.md`)、**规范**(`docs/spec/` v0.5)与**一致性语料**(仓库根 `tests/`,61 文件)。最终自举目标不变:以 C 版为种子编译器,后续用 Ctron 自身实现 Ctron。

## 已交付里程碑

### C1 词法器 ✅
记号模型(§1.3–§1.5)、换行显著性过滤(§1.6)、E1001 循环错误恢复;58 单元断言 + 61 文件零词法诊断。详见 `docs/superpowers/plans/2026-09-05-c-lexer.md`。

### C2 解析器 ✅
规范 §1.7 EBNF **全语法**递归下降 + §1.8 消歧 + §4 优先级,产出纯数据 AST;诊断 E1001 / E3030(static var 恢复)。

- **集成验收**(与 Rust 版同一口径):61 文件中仅 `01c_parse.neg.ct` 报 E1001、`06_static_var.neg.ct` 报 E3030,**其余零解析诊断**。
- 单元锚点:声明/类型三态(空=切片、单整型=定长数组、其余=泛型实参)、`]` 后跟 `(`/`{` 快速路径与顶层逗号回退、比较不可链、`else` 同行、赋值目标校验、2000 层 `(`/1500 层类型括号嵌套上限(嵌套过深 E1001,不栈溢出)。
- `ctronc parse <file> [--ast]`:AST 确定性文本。
- `ctronc parse-ct <file> [module]`:直接调用 **Ctron 实现的解析器/词法器** 处理任意 Ctron 测试码
  (默认 parsetree.ct 结构化树解析器;可指定 selfhost/lex_num.ct 等模块,read_file 目标自动替换)。
- ASan/UBSan 全绿;`make test` 全绿(四件套)。

## 目录与构建

```
compiler_c/
  Makefile              # make / make test / make clean
  src/arena.{h,c}       # 块链 bump 分配器:编译器对象整体一次释放
  src/token.{h,c}       # 记号模型
  src/lexer.{h,c}       # 词法器 + §1.6 换行过滤
  src/ast.h             # C 版纯数据 AST(字段即契约)
  src/parser_internal.h # 解析器内部共享契约(类型/原型)
  src/parser_core.c     #   列表基建/游标/AST 构造辅助
  src/parser_decl.c     #   文件与声明解析(fn/struct/enum/class/trait/impl/use/const/static)
  src/parser_expr.c     #   类型/表达式/块与语句/模式解析
  src/parser.c          #   入口(ctron_parse_src;诊断 E1001/E3030 按序合并)
  src/ast_show.c        # AST → 确定性 Debug 文本
  src/main.c            # CLI: ctronc <version|lex|parse|sem|pkg|run|check|trans|build|test>
  tests/…               # 单元 + 语料套件(见下)
  src/rt_internal.h     # 解释器内部共享契约(val/rt/原型)
  src/rt_core.c         #   值构造/缓冲/数值辅助/环境/字符串插值/函数调用与断言域
  src/rt_eval.c         #   域辅助(和类型/数组/类/并发)+ 表达式求值主分发
  src/rt_stmt.c         #   语句/块求值 + 入口(test 块/main/runner)
  src/trans_internal.h  # 转译后端内部共享契约(sb/ty/tc/原型)
  src/trans_core.c      #   字符串缓冲/类型模型/上下文与作用域
  src/trans_expr.c      #   表达式发射/内建与调用/方法与 trait 分发/泛型单态化
  src/trans_conc.c      #   并发运行时发射(spawn/join/Channel/Mutex/Atomic/scope/parallel)
  src/trans_stmt.c      #   if·match 值发射 + 语句/块/函数体发射
  src/trans_rt.c        #   生成码助手发射(算术检查/字符串/容器/断言族)
  src/trans.c           #   文件级:声明收集/头部装配/转译入口
```

模块拆分原则(2026-09-06):原 trans.c(3866 行)/rt.c(2095)/parser.c(2141) 单体按既有
分节标记机械拆分;跨模块符号一律经 *_internal.h 声明(非 static),节内私有助手保持
static;全量套件逐字回归通过(230 diff + 61 语料 + 34 执行),行为零漂移,0 告警。

构建环境:仅 libc,C11(`cc`);零外部依赖。验收命令 `make test`。

## C 版架构决策(贯穿后续里程碑)

- **内存**:单 arena 块链分配;AST 与记号载荷字符串整体一次释放;解析期读取记号文本后再释放词法 arena。
- **AST**:判别式结构体 + arena 数组;解析器与词法器各持诊断队列、按序合并(词法在前)。
- **嵌套深度上限 256**:类型与表达式共享深度计数(与语料/参照行为同源);超限产 E1001"嵌套过深",恢复路径不栈溢出。
- **AST Debug 文本**:`ctron_file_show` 单行 Rust-Debug 同族格式(变体名 + 字段),确定性输出 = C 版自举差分产物契约(C-AST v1)。
- 错误恢复:顶层/体内循环全部带停滞守卫(`ensure_progress`),每条错误路径都推进。

### C3-a 语义检查 ✅
单文件 11 项检查(Send 三检查点/match 穷尽/pure/comptime/no_spawn/own 块 move 与 gc-mut/浅拷贝 lint/must-use lint);`suite_sem` 读 marker 对 61 文件诚实评分,行为文件零诊断。详见 `docs/superpowers/plans/2026-09-05-c-sem.md`。

### C3-d E2010 保守子集 ✅(条件 Bool + let 字面量类别)
对齐 Rust 版 check.rs 的基础类型检查:`while`/`if` 条件可证明非 Bool → E2010;let 注解原语与
字面量类别(数值/Bool/Str)冲突 → E2010。derive_type 同步扩 INT/FLOAT(含后缀)/比较与逻辑/
UN_NOT/具名函数返回类型(match 穷尽同步受益)。推导不出 → 保守不报;数值↔数值宽度自适应
(如 `let x: U64 = 5`)不报。已入 suite_sem 已实现集。

### C3-e E2010 扩展 ✅(镜像 Rust check.rs 检查面)
一元 - 需数值 / 一元 ! 需 Bool / && 需 Bool / 算术需数值(字串构造走插值,不走 `+`)/
`?` 操作数与所在函数返回类型须 Option·Result / `or` 接收者须 Option·Result /
struct·class 字段存在性(调用者位置跳过;prop·impl 一并识别)。全部保守:类型推导不出不报;
61 文件行为语料零诊断维持,正反探针 13 例符合预期。

### C9 工具链面 ✅(`ctronc pkg` + `--format=json`)
新增 `pkg <dir>` 子命令(模块级检查直达 CLI,exit 0/1);`check`/`pkg` 支持 `--format=json`,
按规范 §10.2 冻结 schema 输出(code/severity/message/file/span/notes/fixes;语义与模块级
诊断当前无位置,span 诚实置 0)。字串转义经 \u 覆盖;JSON 合法性经 python json 验证。

## 后续里程碑(C 版路线,独立推进)

| 里程碑 | 内容 | 出口 |
|---|---|---|
### C3 语义层 ✅(C3-a 单文件 + E3040 分配效果 + C3-c 模块级)
61 文件全部 neg/lint marker 命中(单文件与模块级);行为语料零诊断。详见
`docs/superpowers/plans/2026-09-05-c-sem.md` 与 `docs/superpowers/plans/2026-09-05-c-pkg.md`。

## 编辑器支持(DX) runtime 扩展(服务 `ctronc run` 域)

- **I/O 内建**:`read_line()`(stdin 读一行,EOF 空串)/ `read_bytes(n)`(恰好读 n 字节)/ `flush_out()`(print 缓冲立即落盘)——语言服务器(`../lsp/`)与管道程序的基础设施;不影响 suite_rt 契约(测试不调用这些名字)。
- **`byte_at`**:曾试 O(1) 快路径,因存在越界读 UB 被回退(a3bf0b7 恢复先 strlen 边界检查,语义正确);解释器下逐字节 strlen 为 O(n) ——扫描密集程序(如 LSP)应单趟设计避免 O(n²),另见 `../docs/editor-feature-gap-analysis.md`。
- **`call_decl` 实参求值修复**:实参先在调用方环境求值,再进被调环境绑定——修复形参遮蔽调用方同名局部导致的错误求值(如 `or3(b == 34, …)` 中形参 `b` 撞名;首次修复曾被未提交窗口事故覆盖,21a5b3e 最小重放);全套测试保持绿。

## 后续里程碑(C 版路线,独立推进)
`src/rt.c` + `suite_rt`:纯数值/逻辑/字符串/范围域 5 文件 16 test 块真实运行通过,panic 消息断言;
GC/并发/match/own 等域列允许表为 deferred(C4-b/c/d 逐域并入)。详见
`docs/superpowers/plans/2026-09-05-c-rt.md`。

### C4-i 解释器 deferred 域收敛 ✅(34 文件全量运行,0 deferred)
元组/元组下标、const 预求值(comptime 语义)、Simd splat/lane/to_array/元素级白名单运算、
parallel.map/reduce(序贯形态)、stdweb.dom 最小锚(set_title/title)、AnyError 两段式
(? 擦除记录 main:1 + context 物化继承传播链)、Str.contains;suite_rt 允许表收口为全集,
deferred>0 转为硬告警。随附修正语料 `08_bare.ct` 自校验循环(频次之和应对 `seen` 全表求和;
原写法对 data 求和得 Σcount²,与断言 4 矛盾)。详见本表后补记。

### C10-j 并发/单元格域修复 + P1-E⑧⑩ 同构移植 ✅(31/34 语料原生差分)

- **修复四个生成代码缺陷**:`fn_lookup` 内层循环变量遮蔽外层匹配下标(多函数时参数
  类型拷错,`None` 实参解析失败);任务体 setjmp 捕获 panic 后未置 `panicked=1`
  (`join_or` 恒 Ok,取消广播死锁);值位置 panic 赋 void(Never 语义:只发 panic 语句,
  dest 靠 calloc/初始化器置零);scope/with 块值临时变量 `= 0` 不适配 struct 载荷
  (`= {0}`)+ 声明移出内层块(语句表达式尾部读取同层可见)。
- **头部装配顺序定型**:classes → enums → sums → arrs → structs → globals → helpers →
  protos(class 指针被 sums 依赖;enum 被 sums 值内嵌;struct 字段可含单元格指针)。
- **fn 类型/闭包值(P1-E⑧⑩ 同构)**:`T_FNPTR` + 无原型 `ctron_fnptr`(空参表,调用点
  显式 cast);闭包值 → 顶层 static 函数(clo_sb,形参即源名);用户函数引用即函数指针;
  `f(f(x))` 间接调用;`Option/Result.map` 专用助手(Ok/Some 重包,Err/None 原样)。
  解锁 01e_multiline_chain / 03g_fn_types。
- **Atomic 单元格域**:`Atomic[T](init)` 构造 + `load/store/fetch_add`(fetch_add 返回
  旧值,__int128 承载回写;emit_atomic_call 收 (类型,接收者文本) 支持任意接收者表达式,
  如 `self.counter.fetch_add(1)`);`Atomic/Global/Mutex` 类型注解 → T_MUTEX 句柄
  (struct 字段按指针表示,`decl_ty_tc` 同步触发 typedef)。解锁 06d_globals。
- **parallel 命名空间**:map(切片, fn)/reduce(切片, 初值, fn) 序贯形态发射(对齐
  解释器),语句表达式局部变量保证实参单次求值。解锁 06f_parallel。
- **for 通配模式**:`for _ in 0..4` 放行 PAT_WILD(循环变量 `_`,体内不可引用)。
  解锁 06_concurrency。
- **Drop RAII 补全**:drop 织入移至块尾表达式之后(对齐解释器"尾值先、drop_scope 后");
  方法帧跳过 receiver self(消除 drop 内自递归);织入经 ensure_method_fn 确保原型+方法体。
  解锁 05d_drop。
- **dom 命名空间**:set_title/title 最小锚(static 全局 + strdup,对齐 rt)。解锁 10_web_dom。
- **bare arena 域**:`Arena` 类型注解(句柄 void*,无状态)+ `Arena.fixed(n)` +
  `arena.zeros[T](n)` 零数组(值等价 rt);数组字面量注解元素类型优先(宽度/符号自适应);
  fn 数组参数注册 typedef。解锁 08_bare。

### C10-p 泛型单态化 + 元组 + derive(Show) + const ✅(34/34 语料原生差分达成)

- **泛型 fn 单态化**(03e/04g):带类型形参(`fn swap[T](pair: (T, T))`)的 fn 原体/
  原型跳过发射,调用点按实参类型统一绑定(unify_gen:单名 Named 命中形参名;元组按元素
  递归),复用 C10-h 的 subs 替换 + ensure_mono_fn 临时缓冲发射(原型/函数体在替换下
  解析,`(T,T)` 直接解析为具体元组 typedef);实例名 `ctron_user_<名>__<Mangle>` 去重。
- **二元组值域**(T_TUP):`(A, B)` 类型注解/字面量/下标 `.0`/`.1`/解构 `let (a, b)`(非
  Channel 的一般路径);typedef `ctron_tup_<M0>_<M1>` 走 sums 段;元素限标量域(struct
  载荷诚实拒绝)。
- **泛型 struct/class 实例化**(03e):字面量点按字段实参推导类型实参 → 实例注册进
  structs/classes 表(`Pair__Pixel_Pixel`),typedef/字段访问/drop/构造助手统一走既有
  路径;字面量返回 ty 携带索引(顺带修复非泛型字面量 bits=0 的同源隐患)。
- **derive(Show) 结构化合成**:`v.show()` 无 impl/默认方法时,字段全可显示(int/float/
  bool/str/递归 struct)即合成 show 方法(fmt 助手 + concat 链,rt 同族格式);bound
  满足的结构可显示性同走此路径(`render(p.second)` 泛型体内 `.show()` 经替换后分发)。
- **const/comptime**(04g):const 声明入 globals;非常量初始化(comptime fn 调用)→
  `ctron_ginit()` 运行时初始化(main 序言调用;函数原型在头部,装配次序安全)。

### C10-q Simd 域 ✅(09_simd,34/34 收口)

`Simd[E, N].splat(v)` / `.lane(i)`(越界 panic 逐字)/ `.to_array()`(f64 切片)/
元素级白名单 `+ - * /`(单次求值语句表达式,同长检查);元素统一 double 承载(对齐 rt
v_flt);typedef `ctron_simd_f64_<N>` 走 sums 段,splat/toarr 助手经 emit_helper 新家族
(arrs 段之后发射,前向依赖安全)。

**suite_corpus_trans:34 pass / 0 failures / 0 untranspiled —— 行为语料原生差分全量收口。**

自举侧(C9j②③):Ctron 写的 C 代码生成器 v1/v2 往返逐字一致;自举阶梯 14/14
(含全深度自译化 cc 解释 cc 解释 input_cc 逐字一致;stage 4 升级为 fixtures 全扫,
新夹具自动纳入)。v2(2026-09-07):for-in-range / for-in-定长数组 / 定长数组
(字面量 let·索引·.len)/ F64·F32 浮点(rt fmt_val 同款整值 %.1f 否则 %g)/
assert·assert_eq(失败 return 1);并修 ct_typeof 潜伏错误(Mul/Sub/Div 一律误判
b,println 算术实参误打 true/false → 按操作数推导 f/i)。v1 修复:①`ct_stmt`
各分支裸 `return` → `return env`(void 污染符号表,后续 let 推导对 void 环境
求 `.len` 崩);②`ct_block` 补尾槽发射(p_block 末元素为裸尾表达式时按语句发射,
修复 `{ println(1) }` 单尾语句块静默丢失);③摘除 EMIT 调试打印;④ladder.sh
CTRON_SEED 换靶后 stage 5 须从 compiler_c 目录解析相对输入锚。


| C5b/c…g ✅ | Ctron 词法器 v1→v5(selfhost)+ 差分 harness | 首个"Ctron 写模块 ↔ C 版逐字一致"闭环;v5(lex_num)对全部 tests/*.ct 语料 payload 逐字一致(进制/下划线/指数/12 后缀),详见 bootstrap 计划 |
| C6a ✅ | Ctron 解析器种子 parse_ast.ct(递归下降子集) | 输出 vs `ctron_file_show`(C-AST v1)逐字节一致(9 fn 语料) |
| C6b① ✅ | 子集扩:NL 换行记号/Assign/While/For/If(else·链)/成员·索引后缀 | 17 fn 语料逐字节一致 |
| C6b②③ ✅ | Str 部件(Text/Interp)/test 声明/类型后缀(Named 实参·?·&·fn 类型) | 24 fn/test 语料逐字节一致 |
| C6c ✅ | 真实语料 parse-AST 差分(parse 模板换靶) | 01_basics/03c_str_string/08_bare_alloc.neg 与 C-AST v1 逐字节 |
| C6d ✅ | 类型实参后缀消歧/ComptimeVal/Bool/BlockExpr/pub/tok 越界守卫 | 真实语料 parse 差分扩至 6 文件全过(66 用例) |
| C6e ✅ | match/模式(通配·字面量·Pascal 无载荷·元组变体·结构·绑定) | 逐字节一致 |
| C6f①② ✅ | 切片/定长数组·数组字面量;struct/enum/use/const/static 声明族·类型形参·@derive·结构字面量 | 语料 parse 差分扩至 16 文件 |
| C6f③ ✅ | class/impl 声明族 + receiver 方法(var self→Receiver) | 语料 parse 差分扩至 21 文件 |
| C6f④ ✅ | 闭包 |x| 与 own(arena) 块表达式 | 语料 parse 差分扩至 28 文件 |
| C6f⑤ ✅ | trait(supers/无体 Method/PropSig)/&self receiver/scope 块 | 语料 parse 差分扩至 31 文件 |
| C6g ✅ | Try?/元组/续行链/attrs/comptime/TupleIndex/Prop 容器标签/Simd 消歧/use 花组/\u 钉 | **49/50 无诊断语料文件 parse-AST 逐字节全等**(109 用例) |
| C6h ✅ | parse 差分目录自动扫描门禁(新语料自动纳入,诊断文件自动豁免) | C6 解析器差分收口 |
| C7a ✅ | 结构化 AST 数据树模块 parsetree.ct(节点=List,v0: fn/let/return/assign/二元全优先级) | 树打印=C-AST v1 逐字节(110 用例) |
| C7b① ✅ | 树版扩 if/While/For | 夹具 7 fn 逐字节 |
| C7b② ✅ | 树版入 Str 部件(Text/Interp)/test 声明/调用·成员后缀 | 夹具 9 fn/test 逐字节 |
| C7b③ ✅ | 树版入 match/模式/struct/enum/泛型类型实参 | 夹具 12 项逐字节 |
| C7b④ ✅ | 树版入 use/const/static/class/impl/trait/类型形参/元组类型 | 夹具 18 项逐字节 |
| C7c ✅ | 树版入 TypeArgs 消歧/Index/Try?/.0/own/闭包/scope | 夹具 20 项逐字节 |
| C7d ✅ | NL 记号+语句级消歧(Expr 语句/尾前瞻/空 return) | 夹具 23 项逐字节 |
| C7e ✅ | 回绕算符/void/pub/\u 解码/@derive-Enum/类型三态/数组字面量/续行 | 夹具=全语法 input_parse_ast 逐字节 |
| C7f① ✅ | 树版 let/for 模式化/`{`块→BlockExpr/TypeArgs 数字→ComptimeVal | 语料树差分 41/49 |
| C7f② ✅ | attrs #[] 入树/use .{组}/impl Prop 标签/for 模式/元组表达式 | **全语料树差分 49/49** |
| C8a ✅ | 语义检查首发 sem_chk.ct:W8010 浅拷贝(树上行走) | 与 C sem 差分全等 |
| C8b ✅ | W8020 must-use + E4030 no_spawn(树上行走,含嵌套闭包/scope) | 三检查 38/38 语料与 C 全等 |
| C8c ✅ | Send 内核(ty_send 同构)+ E3020 Channel 元素 + E3031 static 存储 | 五检查 41/41 语料与 C 全等 |
| C8d ✅ | E3060 own 内类成员可变写(绑定 env 近似 + own 上下文) | 六检查 42/42 语料与 C 全等 |
| C8e ✅ | E4020/E6020 pure/comptime 能力调用(pwalk+env) | 八检查 44/44 语料与 C 全等 |
| C8f ✅ | E3010 spawn 闭包捕获 Send(捕获集 + env 类型节点) | 九检查 45/45 语料与 C 全等 |
| C8g ✅ | E3050 own arena use-after-move(arena_binds/moved 集) | 十检查 46/46 语料与 C 全等 |
| C8h① ✅ | E2030 match 穷尽(枚举/Option/Result 变体表 + 臂覆盖) | 检查 47/47 |
| C8h② ✅ | E3040 分配效果(own/no_alloc/契约 + 函数效果摘要) | **C3 单文件 12 项全集 Ctron 树上收官:49/49(207 用例)** |

### C10-f①② 转译覆盖率推进 ✅(13/61 语料原生编译执行差分)
通配 let / TY_REF 透明 / EX_OWN 语句与块尾透明 / .as[T]() 回绕截断 / range 值 +
for-in-range 值 / static let 全局 / None 单元模式 / let if·块 值 / let 临时变量保持
初始化顺序(C 遮蔽作用域与 rt 绑定顺序的差异)/ 字面量进制归一 + U64 极值 ULL 承载 /
decl 无符号上界差一修复。新增 `suite_corpus_trans`:61 语料可转译者自动
"cc 原生执行 vs 解释器" 差分(stdout/exit/panic 逐字),neg/lint 跳过。
13 pass / 21 untranspiled(需并发运行时/trait/闭包/context 链,= C10-g+)。

### C10-i context/AnyError 链 ✅(21/44 行为语料原生执行差分)
`Result.context(msg)` 物化错误链(Err → 链节点;Ok 原样)+ `?` 两段式擦除
(fn 错误目标 AnyError 时具体枚举载荷自动转链);AnyError 链节点属性
`.message/.cause/.trace` + `is_some/is_ok`。链节点 = 头部 `ctron_anyerr`
(message/cause/trace);cause = `ctron_opt_err`(内嵌 Option)。
02_option_result / 05i_deep_cause / 10_trace 原生执行 = 解释器;18/18 回归保持。

### C10-h trait 参数单态化 ✅(18/44 行为语料原生执行差分)
`&Trait` 形参 → T_TRAIT 标记(decl_ty_tc 守卫内、prelude 后;TY_REF 递归 sub 后再查);
泛型原体与头部原型跳过发射;调用点按实参具体类型惰性例化(原型进头部、函数体走临时缓冲进
专用段;subs 替换内层优先;首参数驱动,键 = `__<Mangle(aty)>`)。trait 默认方法/impl 方法分发
沿用 C10-g② 基座。`07_capabilities`(FakeClock 注入)原生执行 = 解释器;17/17 回归保持。
设计预案见 lane-split 附;陷阱实录同源(ensure 去重早退设 out_ret/out_sb 切换/in_test 泄漏→裸 return)。

### C10-g① UFCS + List 容器域 ✅(16/44 行为语料原生执行差分)
UFCS(21.double() ≡ double(21));List[T] 容器(构造/push/into_gc 深拷贝/.len/索引/
for-in);void 函数 return;test 内 return 静默中止;系列修复(decl 差一/进制归一/
let 临时变量顺序/段错误)。残留:并发 scope/spawn、trait/impl 分发、闭包 .map、
context 链、泛型参数、simd/dom/parallel。

### C10-e 转译扩面:插值 + class/Box ✅(差分 14/14)
插值片段解析 → fmt 助手(i64/f64/bool,rt fmt_val 语义)+ concat 链;class 引用语义
(malloc 构造助手/箭头访问/指针共享);Box[T](v) 显式堆分配 + 自动解引用。
闭包/context 链/并发运行时 → C10-f+。

### C10-d 转译扩面:Option/Result/?/载荷变体 ✅(差分 12/12)
和类型按实例化生成 C 结构(tag+union);Some/None/Ok/Err 期望类型提示推导;`?` 分解为
tag 检查 + 早退(let/return;test 体 void return);.or/.expect;用户枚举载荷变体
(构造/绑定/嵌套变体条件)。class/闭包/context 链 → C10-e。

### C10-c 转译扩面:struct/enum/match ✅(差分 10/10)
struct 值语义(by-value,对齐 clone_val)/字段读写(rt "member assign" 消息)/构造指定初始化;
enum 单元变体(tag 结构+变体宏,裸变体构造);match 三位置(return/let/语句,块尾 match 归语句),
模式 = 字面量/通配/绑定/单元变体/struct 模式,无兜底臂尾置 rt 逐字 panic;值臂 if/else 链。
Option/Result/?/载荷变体/闭包 → C10-d。

### C10-b 转译扩面:Str + 数组/切片 ✅(差分 9/9)
Str 域(字面量/拼接/strcmp 比较/len/char_len/contains/slice/byte_at/byte_slice,消息逐字)
+ 数组域(定长 T[N] 与切片 T[] 统一 ctron_arr_<wl> 结构,对齐 rt V_ARR;索引读写/
.len/for-in/越界 panic 逐字)。插值显式拒绝留 C10-c。

### C10-a 转译后端骨架 ✅(Ctron → C,数值域差分)
`ctronc trans <file> [-o out.c]` / `build <file> [-o bin] [-k]` / `test <file>`;
语义逐字对齐 rt.c:检查算术(溢出/除零/assign 消息)、回绕二补截断、入口 coerce(decl)、
返回不 coerce、int128 比较、test 块顺序执行 + panic 长跳;无后缀大字面量语义(算术 i32 宽度,
raw 承载)对齐。v1 拒绝域显式报错(Str/容器/GC/并发…)。验收:`suite_trans` 对 6 个夹具
解释器 vs 原生可执行差分(stdout/stderr/exit/panic 消息逐字),ASan/UBSan 全绿。
泳道与协作协议见 `docs/superpowers/plans/2026-09-05-lane-split.md`。

| C10-b… | 转译扩面:Str/数组/struct/enum/match/闭包;Option/Result/?;目标 = 61 语料全量可编译执行 | suite_trans 夹具逐域并入,与 rt 差分逐字 |
| E6030/W8030 | 规范预留码(comptime 反射/未使用绑定) | 随实现与语料补 |

计划文档:C1…C4 见各 `docs/superpowers/plans/2026-09-05-c-*.md`;**自举路线** `docs/superpowers/plans/2026-09-05-c-bootstrap.md`;**泳道分工** `docs/superpowers/plans/2026-09-05-lane-split.md`。
