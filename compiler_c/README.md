# Ctron 编译器 C 版(C1 词法 ✅ / C2 解析 ✅ / C3 语义层 ✅ / C4-a…i 解释器 ✅(34 文件全量运行))

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
  src/parser.{h,c}      # 递归下降解析器(parse_src 入口)
  src/ast_show.c        # AST → 确定性 Debug 文本
  src/main.c            # CLI: ctronc <version|lex|parse [--ast]>
  tests/test_lex.c      # 词法单元(58 断言)
  tests/suite_lex.c     # 61 文件零词法诊断
  tests/test_parse.c    # 解析单元(锚定行为)
  tests/suite_parse.c   # 61 文件解析分类验收
  src/sem.{h,c}         # 语义检查(单文件)
  src/pkg.{h,c}         # 模块级检查(Ctron.toml + 跨文件)
  src/rt.{h,c}          # C4-a 解释器(数值/逻辑/字符串/范围域)
  tests/suite_sem.c     # 61 文件单文件语义 marker 评分
  tests/suite_pkg.c     # modules/* 包级语义评分
  tests/suite_rt.c      # 执行层允许表评分(行为/panic)
```

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
- **`byte_at` O(1) 快路径**:非尾字节直取,疑似结尾才回退全检(语义不变)。
- **`call_decl` 实参求值修复**:实参先在调用方环境求值,再进被调环境绑定——修复形参遮蔽调用方同各局部导致的错误求值(如 `or3(b == 34, b == 92, b < 32)` 中形参 `b` 撞名);`make test` 全套绿(61 文件 + 208 diff 用例)。

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

## 后续里程碑(C 版路线,独立推进)

| 里程碑 | 内容 | 出口 |
|---|---|---|
| C4-b/c/d | GC 集合/类/match/闭包;own/arena;并发(scope/spawn/Channel/Mutex) | 行为/panic 语料逐域并入 suite_rt 允许表 |
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
