# compiler —— Ctron 自举编译器(模块化优化版)

> 源自 `selfhosted/`(自举唯一差分源):把 6153 行的手工单文件快照 `cc.ct`
> 与追加部件 `tools/trans_part.ct` 重组为**按模块组织、单一定义源**的编译器树。
> 语义与可观测行为逐字不变(见下方验收);夹具与黄金基线仍以 `selfhosted/` 为源,本目录不复制。
>
> **自举方案、三级证明的复现命令与实现不变量见 [BOOTSTRAP.md](BOOTSTRAP.md)。**

## 结构

单职责模块树(36 个 `.ct`,每文件一个职责,头部注明接口;沿用原分节横幅切分,函数零改动):

| 模块组 | 文件 | 职责 |
|---|---|---|
| 词法 | `src/lex.ct` | token 串(哨兵 `#EOF`)+ 换行过滤(规范 §1.6 行延续) |
| 解析 | `src/parse_node.ct` | 树节点助手 + 插值原文切分(qtext/parts_of) |
| | `src/parse_expr.ct` | 表达式优先级链(p_or→…→p_pri)+ 类型 + or 层 |
| | `src/parse_stmt.ct` | 语句/模式/if/match/block |
| | `src/parse_decl.ct` | 声明(fn/struct/enum/class/trait/use…)与入口 `p_file` |
| | `src/parse_pkg.ct` | 模块加载器 v0(use 解析/可见性/循环检测/caps) |
| 语义 | `src/sem_main.ct` | 主控 `sem_walk2`:12 项全集编排(接口:→ 诊断串) |
| | `src/sem_walk.ct` | W8020 must-use / E4030 no_spawn / E3020(树上行走) |
| | `src/sem_send.ct` | Send 内核(send_of;E3020/E3031/E3010 共用) |
| | `src/sem_own.ct` | E3060 own 内 GC 可变写 |
| | `src/sem_pure.ct` | E4020/E6020 pure·comptime 能力调用 |
| | `src/sem_spawn.ct` | E3010 spawn 闭包捕获非 Send |
| | `src/sem_move.ct` | E3050 own 内 arena 句柄 use-after-move |
| | `src/sem_exh.ct` | E2030 match 穷尽 |
| | `src/sem_alloc.ct` | E3040 分配效果(own/#[no_alloc]/契约) |
| | `src/sem_type.ct` | E2010 类型统一 v1(含 E2020 前奏助手)+ E2040 字面量宽度门 |
| | `src/sem_calls.ct` | E2020 调用目标解析 |
| | `src/sem_comptime.ct` | E6020 comptime 副作用扫描 |
| | `src/sem_ceval.ct` | E6010 步数预算求值器 ceval + E5010 助手 |
| | `src/sem_closure.ct` | E3070 闭包可变捕获(两遍式) |
| 求值 | `src/eval_val.ct` | 值构造器/进制字面量/比较/算术 |
| | `src/eval_width.ct` | 宽度域 u8/i8/u16/i16 |
| | `src/eval_float.ct` | Float 十进制定点(df_* 家族) |
| | `src/eval_env.ct` | 环境链/fmt/文本工具/插值串求值 |
| | `src/eval_trait.ct` | trait/impl 方法域 |
| | `src/eval_pat.ct` | 绑定克隆/原地写 + pat_match |
| | `src/eval_expr.ct` | 核心表达式遍历 `eval_expr` |
| | `src/eval_call.ct` | 调用分派:fn 值/UFCS/成员内建(call_mem) |
| | `src/eval_run.ct` | 语句/块/statics_env/run_tests |
| 发射 | `src/trans_ty.ct` | 符号/运算符/类型码/环境/C 字符串转义 |
| | `src/trans_conc.ct` | 并发/闭包发射(Phase 4):双遍协议、spawn 捕获 env+shim、with/with_mut、parallel |
| | `src/trans_expr.ct` | 表达式发射(ct_expr/实参/提升/数组码) |
| | `src/trans_stmt.ct` | struct 表/块/语句/match/if 值位发射 |
| | `src/trans_emit.ct` | 函数发射/main 锚替换/文件样板 |
| 驱动 | `src/driver_run.ct` | 入口(运行):parse → 语义 12 项 → 解释执行 main/test |
| | `src/driver_check.ct` | 入口(检查):parse → 语义检查即止,文本面 `check OK decls=N`;`--format=json` 出 §10.2 冻结 schema 诊断(span 为 v0 近似定位) |
| | `src/driver_emit.ct` | 入口(发射):parse → 生成等价 C(产物 gcc 可编译,`<bin> run <file>` 覆锚) |
| 脚本 | `build.sh` | 确定性拼接出单文件产物(宿主 seed 可解释的 `.ct`) |
| | `ctc.sh` | 统一驱动:`ctc.sh <in>` 运行 / `ctc.sh check <in> [--profile=bare]` 检查 / `ctc.sh emit <in> [out.c]` 发射 |
| | `native.sh` | 编译出原生编译器二进制 `bin/ctron-cc` 与 `bin/ctron-emit` |
| | `bench.sh` | 性能基线:微基准三路 + 前端 check + 后端发射 + 黄金解释(基线见 BOOTSTRAP.md §2b) |
| | `test/smoke.sh` | 验收冒烟(54 项;`--full` 加自发射收官与固定点) |
| | `test/suite.py` | 用 `tests/` 一致性测试集(可执行规范)验证本编译器,对照 C 宿主 |

Ctron 当前为单文件程序模型(无本地多文件模块),模块化以**确定性拼接**实现:
`cc_run.ct` = lex + parse_* + sem_* + eval_* + driver_run;`cc_check.ct` 换 driver_check;
`cc_emit.ct` = 核心 + trans_* + driver_emit(trans 仅入此产物)。核心 fn 三产物共享,
改动单点生效;段内拼接序 = 原单文件的函数相对顺序(保序便于与历史产物 diff)。

## 用法

```bash
make -C compiler-c                     # 构建宿主 seed(首次引导唯一依赖)
compiler/ctc.sh selfhosted/input_cc.ct        # 运行:正例 rc=0 / 负例诊断 rc=1
compiler/ctc.sh check compiler/build/cc_run.ct   # 自编译检查面
compiler/ctc.sh check <in> --format=json         # §10.2 JSON 诊断契约(agent 循环消费面)
compiler/ctc.sh emit selfhosted/fixtures/trans_v3.ct out.c  # 发射 C → gcc
compiler/native.sh                     # 编译出原生编译器(约 9s)
compiler/test/smoke.sh --full          # 全量验收(18 项)
```

## 原生编译器(bin/)

`./native.sh` 走自举链产出两个本机二进制(发射器编译编译器源 → C → cc):

| 二进制 | 来源 | 能力 |
|---|---|---|
| `bin/ctron-cc` | cc_run.ct → 9.2k 行 C | `<bin> run <file.ct>`:parse → 语义 12 项 → 解释执行 |
| `bin/ctron-emit` | cc_emit.ct → 11.2k 行 C | `<bin> run <file.ct> > out.c`:parse → 发射等价 C |

实测(同任务对照):发射 `cc_run.ct` 原生 0.010s vs seed 解释版 2.25s(**~225×**);
解释 `input_cc3` 2ms。产物与 seed 发射逐字节一致(自举固定点在原生侧成立),
宿主 C 编译器自此只剩"首次引导"职责。

## 相对 selfhosted 的优化点

1. **快照去重**:`cc.ct` 是把 parser(同 `sem_chk.ct`/`ev2.ct` 内嵌副本)、语义、求值器
   三处手工拼接的快照;本目录每个函数只有一份定义,修 bug/扩域不再需要三处同步。
2. **死代码清除**:从 main 可达性分析剔除 7 个不可达函数共 446 行 ——
   `pnode`/`pjoin`(348 行 C-AST 打印器,差分轨道专用)、旧版 `sem_walk`(W8010 已在
   `sem_walk2` 内)、`p_prop_item`、`callee_root_alloc`、`is_arena_op_m`、`u_set`(在位版
   `u_set_ip` 存活)。
3. **发射器纳入同一构建**:`trans_part.ct` 的"genmod 切 main + 追加"手术改为
   `driver_emit.ct` 与其余驱动平级,发射模式成为一等公民(不再依赖 `tools/genmod.py`)。
4. **check 驱动内建**:自编译检查面(parse+语义、decl 计数)替代 genmod `--count` 注入,
   ladder 第 2 步口径原样可用。
5. **分节横幅功能化**:C9x 时期的过程性标记(C8a/C9b①/C9j⑧…)改写为按检查项/值域命名;
   各模块头部注明职责与接口。

## 验收(实测,`test/smoke.sh --full` 18 ok / 0 fail,约 19s)

- 黄金对照 `input_cc/2/3` 逐字一致;负例 `input_cc_neg` W8010 编译期拦截(rc=1);
- check 自检 `cc_run.ct` 绿,decls=168;对 `selfhosted` 旧源 decl 计数与 ladder 锁定的
  C 解析器数字逐一吻合(sem_chk=109 / parsetree=57 / ev2=107 / cc=175);
- 发射往返 `fixtures/trans_v0–v3` 四件全绿(原生执行 == 宿主解释,逐字);
- 自发射收官:`cc_run.ct` 经发射 → gcc → 原生解释器,三个黄金 + 负例拦截全绿;
- 自举固定点:原生发射器与 seed 发射器对同一源的发射产物**逐字节复现**;
- 原生二进制验收:`bin/ctron-cc` 解释三个黄金 + 负例拦截全绿,
  `bin/ctron-emit` 发射 `trans_v3` 往返逐字一致、发射 `cc_run.ct` 与 seed 逐字节一致。

## tests/ 一致性测试集验证(test/test/suite.py)

按 `tests/README.md` 标记语义跑 `bin/ctron-cc`,并以 C 参考宿主同口径对照
(宿主走 `check`/`test` 两阶段;本编译器单步 `run`)。跳过 `roadmap/`(红=规范锚)、
`modules/`(多文件包)与 `target` 非 full 件。

记分卡(2026-09-08,两批能力补齐后 **50/50 与 C 参考宿主全对齐**):

| 类别 | bin/ctron-cc | C 宿主(参考) |
|---|---|---|
| behavior | 31/31 | 31/31 |
| neg(编译失败拦截) | 14/14 | 14/14 |
| lint(警告出现) | 2/2 | 2/2 |
| panic(运行期消息) | 3/3 | 3/3 |
| 合计 | **50/50** | 50/50 |

第一批:检查算术 overflow/除零/UTF-8 切点 panic、u8/i8/u16/i16 宽度域、
Option/Result 组合子 `or`/`expect`/`is_some`/`map`/`context`(message/cause/trace
错误链)、static let 预绑定、E3030/E1001 检查项。

第二批:`as[T]()` 显式转换、元组枚举变体(构造 + SubTup 模式)、own 块、
Scope/spawn/join/join_or + Channel(send/recv,共享队列 + 读游标)顺序化并发模拟、
`Mutex.with_mut/with`、`Global`/`Atomic.fetch_add`、Box 自动解引用、
元组值 `(a, b)` 与 `let (a, b)` 解构、`.0/.1` 元组索引、`char_len`、
`List.contains`(Str 子串语义)、derive(Show) 兜底、`parallel.map/reduce`、
`Simd.splat/lane/to_array` 元素级白名单算术、impl Drop 作用域退出逆序触发。

第十六批(2026-09-09,src 分模块重构):sem/eval/parse/trans 四大文件按职责拆为
**32 个单职责模块**(sem 14 = 一检查码一文件,主控 sem_main.ct;eval 9 值域/执行域;
parse 5 层;trans 4 发射单元),沿原分节横幅连续切段,函数零改动、fn 总数不变;
build.sh 以 CORE/TRANS 保序拼接。产物与拆分前逐行 diff **仅注释头差异**(机械证明
"只搬家");smoke --full 33/33、suite 51/51 双侧对齐、modules 7/7、自举固定点逐字节复现。

第二十六批(2026-09-10,泛型 struct 单态化):注解驱动实例化——`Pair[I32, Str]`
注解 → 实例化码 u:Pair__I_S(走既有 u: ctype 通路),typedef 预扫按注解集
打印(场型 TPar 槽代入);字面量场值型别推断同码;域访问解装箱码取实例化
域型。管线:ct_struct_tps/ct_struct_inst_code/ct_struct_inst_split/
ct_field_code_inst(编码尾缀单一流)。fx_gstruct(seed==native 逐字 42 x)
+ stdpkg/tlist 回归;smoke --full 54/54;suite 51/51。挂账:多实例化歧义
字面量、泛型 struct 方法、嵌套泛型。

第二十五批(2026-09-10,泛型 fn 单态化发射 v0):显式 TypeArgs 调用点按型别
单态特化——pass1 直出特化定义(AST 型别替换:Named(TPar) → 实参型节点;
mangle t_name__<码>),pass2 调用点引特化名;返回码经 TPar 替换推导
(identity[I32]→i / [Str]→s);eval 侧 TypeArgs 擦除动态派发(语义一致)。
fx_generic 四实例化(i/s × identity/pick)seed==native 逐字;smoke --full
53/53。v0 挂账:泛型体内嵌泛型调用、bound/derive 体系、泛型 struct。

第二十四批(2026-09-10,own 块发射):own (名) { 体 } 透明语义落地——语句位体
平铺,let 初值位尾值声明式捕获;sem tcb 补 Own 块下钻(块内绑定/引用不再误报
E2020)。own (arena) 显式生命周期注记,arena API 发射挂账。fx_own 双向逐字
(31);smoke --full 52/52。

第二十三批(2026-09-10,CI 门禁 + typed List + std 种子包):**ci.sh 一条命令
全量门禁**(meta → 拼接 → smoke --full → native → suite → bench)+ GitHub
Actions workflow。**typed List[I32]**——LI 类型码贯穿 typeof/ctype/Index
(标量槽读)/for-in(发射器 List 迭代从无到有)/push(标量槽 cast)/hoist 排除。
**std 种子包**——pkg_load_use 解除 std.* 跳过(→ 入口包旁 std/ 目录;缺失
忽略,语料兼容),修复合并两缺陷(未展开 Use 被滤除、重启扫描索引偏移)、
跨模块同名 decl 首个胜出、pkg_check_caps eager 索引修复;stdpkg(IntMap/
IntSet 函数式容器)seed==native 逐字。Bool.to_string true/false 形态补齐。
smoke --full 51/51;suite 51/51;固定点复现。

第二十二批(2026-09-10,Phase 5 收尾:fn 返回 fn + ? 传播发射):
**fn 返回 fn**——FnType 返回型维度入码(F<ar>=返标量 / G<ar>=返 fn 值;
ct_fn_ret 认 FnType),G 调用结果 cast ct_clop,适配器按返回型经 long;
mk(k)->fn(I32)->I32 闭包捕获构造参数,invoke(mk(5),2) 全通。**? 传播
发射**——Try 节点语句/let 初值位 ANF:ct_res 暂存 + variant!=0 → 短路
return t_q,载荷 i32 绑定;ct_is_value 黑名单入 Try。fx_fnret/fx_try
seed==native 逐字(18 10 / 20 err);smoke --full 49/49;suite 51/51。

第二十一批(2026-09-10,Phase 5 切片五:--profile 档位 + \u{HEX} 通用解码):
**--profile 档位(检查面)**——ctc.sh check --profile=bare:sem_walk2 线程 prof,
bare 档 = 文件级 no_alloc 上下文(隐式 GC 分配一律 E3040;ANCHORPROFILE 锚注入);
full 档同源通过(门控生效),run 面不受影响。**\u{HEX} 通用解码**——qtext 的
单映射硬编码(4E2D→中)升级为 1-6 位 HEX → 码点 → utf8_enc 内建出 UTF-8 字节
(宿主 rt_eval 新增 utf8_enc;发射器样板 ctron_utf8_enc;ASCII/BMP/增补平面
全可);fx_uhex 四行 seed==native 逐字。smoke --full 47/47。

第二十批(2026-09-10,Phase 5 切片四:用户枚举 × Result 载荷 + Option/Result 构造):
Named Result/Option → R 码;Ok/Err/Some(expr)/None 构造发射(载荷按型别包装:
用户枚举 → amalloc 装箱,指针经 long,标量直入);match R 臂多 Err 臂循环链 +
嵌套用户枚举模式(Err(DivByZero)/Err(NegSqrt(k)) → 解装箱 variant 判定 + 载荷
绑定;临时名按站点行号唯一化)。fx_enumres 三形态 seed==native 逐字(106);
smoke --full 44/44。挂账:装箱载荷的别名语义细化、? 传播发射。

第十九批(2026-09-10,Phase 5 切片三:带捕获闭包值):fn 值表示升级为
ct_clop = ct_clo{fn,env} 装箱对;间接调用走 ct_cfnK(env 首参);具名 fn 适配器
同步装箱;ct_emit_clov 生成带捕获静态 fn(捕获 → env 数组,创建时快照,镜像
eval env 引用语义)。parallel/spawn/with 的原始 fnptr 路径不变。fx_cloval
(存变量/作实参/多捕获)seed==native 逐字;smoke --full 43/43。挂账:fn 返回
fn(F 码不含返回型)。

第十八批(2026-09-10,Phase 5 切片:枚举 + 一等 fn 值发射):**用户枚举发射**——
每枚举 ct_enum_<E>{variant,p[2]},裸变体名/单元变体 Ident 构造,match variant 下标
链 + KTuple 载荷绑定(trans_ty 新增枚举查询族 + 类型码 E:<名>)。**一等 fn 值**——
fn(I32)->I32 类型码 F<arity>(ct_fn1–3),具名 fn 实参 → 签名适配 thunk(pass1 直出),
闭包字面量 → 静态 fn(Closure 节点补行号戳,命名跨遍确定),fn 值间接调用。挂账:
带捕获闭包值、用户枚举作为 Option/Result 载荷、同行双闭包命名冲突。fx_fnval/fx_enum
seed==native 逐字;smoke --full 42/42;suite 51/51;固定点复现。

第十七批(2026-09-10,Phase 4 真并发运行时):**发射侧 pthread 真并发落地**——
scope/spawn(捕获闭包 → ct_i env 数组 + shim)/join/join_or/Channel(send·recv,有界
队列)/Mutex(with 值拷贝·with_mut 指针可见写)/Atomic fetch_add/Global/parallel
(map·reduce)/任务 panic → 结构化取消广播 → 阻塞 send 得 Err(ScopeCancelled)。
架构:**双遍发射**(ct_fn 以 env "#p" 跑两遍——pass1 直出文件作用域件(shim/typedef),
pass2 发射函数体;eln 按 env 门控),零静态、自举闭环安全;发射器新增 Static 声明
支持。smoke --full 40/40(七并发夹具原生==解释逐字 + 固定点复现)、suite 51/51。
v0 限制:嵌套 spawn、一等闭包值、struct 值捕获、用户枚举发射挂账 Phase 5;
`&&`/`||` 求值不短路,eager 索引守卫需嵌套 if(教训入册)。

第十五批(2026-09-09,FFI 解释口径收口):解释器调用 extern 声明 → 明确指引
"extern fn 仅原生口径可用(经 ctron-emit + cc 链接 c_src 后运行)"(此前为
含混的 no fn 未定义名)。发射口径不变。

第十四批(2026-09-09,E3070 闭包可变捕获,§4.7 v0.6):两遍式检查——全文件
var 绑定名集 × 每个闭包的赋值目标集,交集即 E3070(诊断含机械修复建议
"改用 Mutex[T].with_mut";嵌套闭包独立检查;闭包自身参数不标记)。新遍历器
cscan/cassign_names/cblock2/mblock/mexpr 全下钻(Block 与 BlockExpr 双形态)。
教训入册:遍历器缺 Block/BlockExpr 分派 = 静默漏检(本轮第 N 次同类坑)。

第十三批(2026-09-09,E6010 规格化:步数预算求值器):comptime 求值从"全量解释器
直调"改为**专用步数预算求值器 ceval**(镜像宿主 pkg ceval;1200 步,静默回绕的
I 域算术,const 引用/comptime fn 单表达式体/If 值位)——超预算 → **E6010**
(spec 本义;替代此前的 runaway 启发),不支持形态 → 静默回退全量求值(运行期
statics_env 同口径),const 求值自此**无挂起/无栈溢出**。runaway 启发五函数移除。

第十二批(2026-09-09,FFI 切片,modules 收官):**extern "c" fn 声明**(§9.6,
无函数体,#[trusted] 前缀兼容;FnExt 节点贯通 sem 前奏/调用解析/发射原型——
C 侧符号无 t_ 前缀)、**语句位 assert 家族已有发射**(沿 return 1 约定)、
**test 块发射**(int fn:断言失败 return 1,成功 return 0;无 main 的包自动
生成逐测试检查的 main)。suite modules/ffi_math 走"发射 → 链接 c_src → 原生
运行"口径(解释器无 FFI)。modules 自举 7/7 = 宿主 pkg 7/7,多文件包收官。
坑:字符串字面量内裸 `{` 开启插值(未终止即 E1001),发射模板一律 `\{` 转义
且逐字面量配平。

第十二批(2026-09-12,R 线 feat/cli-test-fmt + feat/v07-bootstrap-port;
**修订一/二自举移植已落地**——修订二:parse/eval_run 状态种 b·c/trans 直映/
sem chk_break E2070;修订一:lex OROR/p_oror 第 0 层+起始位零参闭包/
eval 短路/trans 直映/sem E2010 宽松口径 + 全 Binary nstamp 修复;
**修订三自举已落地(裸型参位名级推断,E2060/E2061;结构化形态待型别串
保留实参)**;C 线 oracle 同步(两修订最小面 + eval_block 流跳过修复 +
TypeArgs 用户 fn 直调);smoke 87/87 + suite 56/56 + meta_check 93 文件
全绿;E2071/E2072、C 线推断 neg 面待后续):
**v0.7 三项松绑 + 新诊断码**——`||` 逻辑或(第 0 优先级,起始位零参闭包位置消歧,
§4.0 运算符宪法)、break/continue 转正(语句级,绑最近循环)、泛型调用点推断
(Go 式仅实参,显式 TypeArgs 恒合法)。新码:**E2060** 无法推断类型实参
(仅当有具体实参信息;实参类型不可得时宽松)、**E2061** 类型实参候选冲突、
**E2070** break/continue 在循环外、**E2072** break/continue 穿越闭包边界、
**E2071** break/continue 越过带 Drop 局部的作用域(loop_scope_base 基线扫描,
R 线已启用;C 直映发射的 RAII 前提)。设计全文:
docs/superpowers/specs/2026-09-12-v07-operator-constitution.md;
R 线实现:compiler-rust(oror/breakc/infer 三 suite + fixtures)。
R 线同批:check 侧 prelude 内建族登记、标量 to_string、&&/|| prim_cat
宽松口径对齐 C sem——check_suite 全绿。

第十一批(2026-09-09,模块检查补全):**E5010 孤儿规则**(impl 的 trait 与 for
类型均非本包声明即拦截)、**E4010 caps**(use std.<fs|time>.<Name> + 本文件
&Name 参数而 Ctron.toml [caps] 未声明)、**E6010 前置拦截**(无终止守卫的
自递归 comptime fn 及其 const 依赖——静态判定先行,防求值挂起;真步数预算
留宿主 pkg oracle)。modules/ 自举 6/7(余 ffi_math 待 FFI 切片),宿主 pkg 7/7。

第十批(2026-09-09,Phase 3 v0:模块加载器):**多文件包可编译运行**——驱动常开
pkg_load_use:use 节点结构化([Use, Segs, Syms]),按包名剥离 + 路径展开读取
src 下模块文件,整模块合并(滤除 Use);符号可见性检查(非 pub → E2020);
栈式循环检测(E5020 消息含 circular);pub(pkg) 解析为 FnPub(单包语义 v0);
std.* 导入保持忽略(stdlib 未落地)。`ctron_entry` 内建(宿主 rt 恒空串/发射
运行时取 argv[2])供加载器定位入口。suite 新增 modules 小节:自举 3/7
(use_ok/visibility/circular),宿主 pkg oracle 7/7;caps/comptime_budget/
orphan/ffi_math 待后续切片(E4010/E6010/E5010/FFI)。已知限制:同名私有符号
跨模块合并取首个;发射多文件包需 bin/ctron-emit(ctc.sh emit 无 CLI 入口)。

第九批(2026-09-09,Phase 1.5 span 标注):**JSON 诊断 span 精确化**——lex 的
scan 增 lns 平行出参(逐 token 行号),parser 线程化(p_file 起全部建树路径)并在
10 类节点(Call/Member/Let/Assign/Return/Try/Static/Struct/Match/StructLit)
尾部追加行号戳;sem 诊断内部携带 "CODE@行"(34 处推送位),驱动文本面 strip_at
剥离(黄金逐字保持),JSON 面解析 @行 为精确 span(无戳节点回退探针近似)。
兼容性放宽:类型参数期望与 trait 期望(含 &Trait)不判 E2010。suite 协议注意:
自举 native 需重建后跑(否则对照旧二进制)。

第八批(2026-09-09,发射面 trans v5:值位 if + 插值):**值位 if**(§4.6 if 是
表达式)——fn 尾与 let 初值位的 if 发射为 C 三元;两分支须为纯表达式块(else
的 BlockExpr 包装形态已适配,嵌套 if-else 链递归),否则命中即 panic(文档化,
fn 尾值此前被静默丢弃的发射缺陷随之修复)。**字符串插值**(§4.11)——发射器
内联调用解析器把 Interp 片段解析为表达式,按类型转 C 串(i/6/b/s/N;float
panic)后 ctron_str_concat 链接;纯 Text 串输出逐字节不变(固定点保持)。
夹具 trans_v5 往返逐字一致。剩余发射缺口:枚举/闭包/并发/值位 match 泛化。

第七批(2026-09-09,发射面 struct 支持/trans v4):**用户 struct 值类型可发射**——
`u:<名>` 类型码贯通(ct_ty_code/typeof/env/形参/返回),C 端 `t_<名>` typedef
(driver_emit 于运行时助手后前置发射),构造字面量 → C99 复合字面量(指定初始化),
字段读 → `.f`,按值传参/返回/赋值(§6.1 值语义天然成立),定长数组元素码升级为
多字符(a<N><元素码>,a_count/a_elem 解析)。不支持(命中即 panic,文档化):
struct 的 print/to_string、字段赋值、class 字面量;发射面其余缺口(枚举/闭包/
插值/值位 if·match)仍按 selfhosted 排期。夹具 trans_v4 往返逐字一致。

第六批(2026-09-09,§4.4 or 中缀取默认):自举 parser 补 `p_or` 层(最低优先级,
§4.3 层 1;元组/分组首元素同步改道),eval Binary "Or" 与 C 宿主 rt_eval B_OR
同语义落地:仅解包左侧 Some/Ok(1 载荷),默认值原样返回(与 `.or(默认)` 语义
唯一,钉子;急切求值)。此前宿主解析器收 `or` 而自举解析器报错、两侧求值器
均未实现的三方分歧收敛。语料新增 tests/04c_or_infix.ct(51/51 双侧)。
已知 suite 口径盲区:自举 run 单步在文件含 main 时不执行 test 块(宿主两阶段
会执行)——语料断言类问题由宿主侧兜底,记入边界。

第五批(2026-09-09,comptime v0/Phase 2 切片):**E6020**(comptime fn 副作用
静态扫描:bare I/O 与成员 spawn/send/recv/store/fetch_add/with*;assert/panic
不ban——静态断言合法,panic 失败映射见下)、**const/static-let check 面试求值**
(镜像 statics_env 顺序迭代,失败不再静默吞绑定:panic 以原文中止编译 rc=1,
流级失败产出诊断;E6010 按 spec 保留给"预算超限"锚点)、**const 注解标量核对**
(E2010)。v0 限制:无步数预算,comptime fn 死循环会挂起 check。

第五批(2026-09-12,I64 宽字面量直入 6 域 + 字面量宽度门):**宽字面量直入
I64 域**(§3.1.1——eval Int 超过 I32 宽度时取 v6(lit_to_dec(数字)),不再
txt_num 溢出 panic;十进制与 0x/0o/0b 同口径,lit_radix_dec 进制转十进制
文本域累算;Unary Neg 6 域符号翻转;发射侧宽字面量转十进制加 `LL` 后缀 +
ct_typeof 推断 "6",下划线分隔符剥离)、**E2040 字面量超出期望整数类型宽度**
(let 注解/赋值/返回/实参四点 compat 伴随门;无后缀字面量十进制/进制同口径,
comptime 域、二进制折叠表达式、带后缀字面量 v0 不查)、
**conv_as 补 6 域入口**(conv_as_6:二补截断 v mod 2^N,8/16 位落 W、
32 位落 I 镜像自举 U32=I32 存储口径、64 位留 6 域——宽字面量切片曾暴露
as[U32] 不截断的 suite 03b 分歧,本批修复);fx_time 扩宽字面量/进制块、
fx_litfit_neg 负例;decls 锁 271。**hex-E 词法缺陷修复(同批)**:数字 token
浮点检测对 0x 前缀不再判 e/E 为指数(0xDEADBEEF/0xdeadbeefcafe 曾误判
Float → E2010);fx_time 含双锚。**arena.list/into_gc 原生可跑(同批)**:
发射面 arena.list[T]() 复用 ctron_list_new + ct_typeof LI 推断、into_gc
恒等(镜像 seed vdeep 观察等价)、own 块尾位展平(own 作为 fn 体尾语句曾
panic,05_own 语料正是此形态);fx_own 扩 evens 块 seed==native 逐字;
arena.array[T](n) 仍挂账(双侧正语义未定义)。**comptime 域宽字面量入门
(同批)**:ceval Int 宽字面量直入 6 域、Unary Neg/Binary 算术经 val_arith、
比较经 vcmp(与运行期同口径)、vtag_ok 放行 n:I64↔6;**const 穿发射已修**
(存量缺口:const 引用在发射面是未定义符号——driver_emit ceval 折叠值直出
C 常量定义 I/6/B 域 + 跨 decl const 环境累积 + ct_typeof 按注解查 Const);
S/D 域 const 穿发射挂账。**I64 溢出检查收敛(同批)**:seed c6_in_i64 界门
(Add/Sub/Mul 超界 panic、MIN÷-1 与 MIN%-1 与 -MIN panic)+ 发射侧
ctron_i64_add/sub/mul/div/mod/neg 帮手(__builtin_*_overflow + 除零/MIN÷-1
镜像)+ 复合赋值 6 域走帮手 + const 折叠值超 int64 E2040;smoke 4b 双面
拦截断言;MIN 字面量取负用 (-9223372036854775807LL - 1) C 惯用法直出。
实现教训:emitted C 字符串里 `\}` 是非法转义(裸 `}`),六帮手行曾致
seed 词法 E1001、native.sh 静默失败。**闭包形参注解 ABI(同批)**:带注解
闭包形参(`|s: Str|`)的 shim 声明按 ClosureParam 注解生成(双 shim;无
#clcodes 提示时回落注解),Str 形参经 64 位槽位透明传递——此前硬编码
int32 截断为指针 UB;fx_clostr 双面逐字;无注解形参仍回落 int32。**#clcodes 生产侧已接通(同批,
A 块复活)**:ct_fntype_pcodes(被调 fn 显式 FnType 形参内参码)+ ct_call_args
门控绑定(仅 F/G 码 + Closure 实参触发),无注解闭包(`|s| …`)经调用点
提示透明传递;自发射固定点通过;泛型内参未替换型参兜底 "i" 挂账。**嵌套闭包发射已支持
(同批)**:三 shim 发射器(捕获/非捕获/spawn)重构为「qlines 缓冲 → #p=1
预扫(内层 shim 先行直出)→ 冲刷 → #p=2 体走」,闭包体内可再定义闭包;
fx_clostr spawn 嵌套用例双面逐字。

第四批(2026-09-09,类型检查 v0/Phase 1):**E2020 全量**(Ident 读解析,callable/value
双名集)、**E2010 基础类型统一**(let 注解/return/bare 实参/赋值/二元 Str·Bool/
条件/? 合法性;保守可证,任一侧未知放行)、变体构造 arity(前奏+用户枚举)、
**W8030 未使用绑定**(插值原文子串补偿)与 **W8040 遮蔽前奏**;修复 eval/ev2/cc
三处 `var L` 大写 let 模式不绑定的潜伏死路。

第三批(2026-09-08,规范差距收补 Phase 0):**E2020 调用目标解析**与 **E2010 调用
arity**(仅 bare 调用面,成员调用与内建 arity 留类型检查 v1;同步修复 eval/ev2/cc
三处 `ty_head` 潜伏缺参 or3——按既有可观察行为忠实改写)、**for-in List 迭代**
(§4.6 可迭代缺口)、**§10.2 JSON 诊断契约 v0**(code/severity/message/file/span/
notes/fixes;span 以消息尾段名定位首含行,解析器 span 标注落地后替换)。

顺带修复解析器缺陷:`a[i] {` 的 `{` 前瞻被误判为泛型 TypeArgs,导致
if/while/match 条件上下文中比较表达式被吞(01g 类测试静默失败)——
现按 allow_struct 门控,仅表达式上下文允许 `expr[T]{`。

已知偏差:lint 件本编译器以 rc=1 退出(驱动把 W 码与 E 码同视),判定按"警告出现"计;
并发为顺序化模拟(spawn 即刻完整执行,满发送立即 Err(ScopeCancelled)),
语义与"结果与调度顺序无关"的测试面一致。

## 边界(沿 selfhosted 挂账,未在本目录扩大能力面)

- 类型检查 v0 已落(E2020 全量 + E2010 基础统一,保守可证);仍无完整统一器:
  泛型单态化、成员调用面(方法表解析)、跨语句流类型细化(窄化/收敛)留类型检查 v1;
  解析器 span 标注(JSON 诊断精确定位)留 Phase 1.5(见 docs/superpowers/plans)。
- 发射器能力面 = `fixtures/trans_v0–v5` + `fx_conc_*` 七件(并发真 pthread)+
  编译器自发射;剩余缺口:枚举字面量/一等闭包值/并发调度确定性差分按 Phase 5 排期;
- `pkg_chk.ct`(模块级检查 + E6010 comptime 预算)属包管理 oracle,未纳入;
- `#[trusted]` 现为解析兼容 + FFI 边界信任占位(caps 检查豁免位,语义化待 caps v2);
  Result 载荷中的用户枚举为装箱指针,绑定名即指针(别名语义细化挂账);
- 宿主 seed(`compiler-c/build/ctronc`)仅承担首次引导,bootstrap/ladder 流程仍在
  `selfhosted/`。
