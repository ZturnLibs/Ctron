# Ctron 反哺批次规划(FB:来自 Loom M0 dogfooding 的 15 项实证发现)

- 日期:2026-10-05
- 来源:Loom 项目(/Users/zyj/Zturn/loom)M0 walking skeleton 全程用 Ctron 实现,系统性踩出语言/工具链问题 15 条,每条有最小复现、规避方案与定位线索。台账:Loom 仓库 `docs/superpowers/plans/2026-10-05-m0-plan1-walking-skeleton.md` 调试日志 I-X;本规划将其转化为 Ctron 侧的修复工项(FB-*)。
- 原则:每项 = 复现(双通道)→ 根因(定位到 compiler/src 文件行)→ 修复 → 双通道测试(panic/neg/behavior,tests/README §1 约定)→ smoke+suite 不回归 → 自举固定点(native.sh 后产物逐字节一致)。
- 优先级判定:**P0 = 静默产生错误值(最危险)**;P1 = 能力缺口;P2 = 体验/语义统一。

## 批次总表

| FB | Loom 编号 | 现象(一句话) | 根因定位 | 级别 | 批次 |
|---|---|---|---|---|---|
| FB-1 | F22b | `byte_slice` 负长度/越界 → memcpy 巨量崩溃(发射臂运行时模板无校验) | `compiler/src/driver_emit.ct:112`(模板);解释臂 `eval_call.ct:356` 同查 | P0 | **本批** |
| FB-2 | F16 | 无 mkdir 内建(用户被迫 FFI) | 仿 fs_write 五点:parse_pkg.ct:55 名单 / sem_calls.ct:89 prelude / eval_call.ct 分发 / trans_expr.ct 映射 / driver_emit.ct 模板 | P1 | **本批** |
| FB-3 | F6/F9/F7 | 字面量 ≥2^31 在数组元素位/比较位按 I32 折断(4294967296→0);bit.* 实参同病(发射 693 行 uint64 cast 正确,疑实参字面量发射截断) | 字面量发射位:trans_expr.ct Int 分支 + comptime 折叠(sem_ceval.ct);I64 值域直入清单(§3.1.1)未含"数组元素赋值/比较位" | P0 | 下一批(需先做最小复现探针归因:字面量发射 vs 折叠) |
| FB-4 | F10/F13 | I64 数组视图元素读 / 循环体标量赋值按 int32(静默错值,最危险) | 发射侧元素读/赋值的宽度槽(trans_expr.ct Index/Assign 分支的 ctype 推断) | P0 | 下一批 |
| FB-5 | F15 | `U8.as[I64]()` 按符号扩展 | conv_as 的 U8→I64 路径应零扩展 | P0 | 下一批(修复后 Loom 的 u8widen 规避可删除) |
| FB-6 | F18 | `&&` 不短路(已知坑,HANDOFF §4;Loom 再次踩中) | eval 臂 B_AND 求值序 + 发射臂条件化 | P0 | ✅ 不复现销账(1006 双臂探针:&&/|| 短路序均正确,副作用右 operand 未求值;近期发射链重构顺带治愈或 F18 归因有偏,台账留探针口径) |
| FB-7 | F14 | 发射器把复杂表达式临时变量提升到所在 C 块且不去重 → 多语句同块重定义 | trans_expr.ct 临时命名(按语句唯一化或最小作用域) | P1 | ✅ 落库(1006):真面=While 提升预发按「站点」重复落零初始化声明(同名同型分支"本站点亦发"),同包装块多站点即 C 重定义(redefinition of 't_t',Loom 生成器 t_t×N 同源)。修=包装块内 wemitted 去重,只发首现;后续站点由体内 let 提升赋值承接(语义对拍双臂逐字一致)。锚 fx_loop_relet(smoke 扫描)+tests/fb7_loop_relet。备注:复杂表达式 ANF 临时(T31 语句表达式作用域)本批探针不复现,本件只涉提升声明面 |
| FB-8 | F5 | 定长数组按值传参/返回(TupleE/Args@0) | trans_expr.ct 数组实参/返回码路径 | P1 | ◐ 传参面落库(1006,fb6-and 分支):ct_ctype 补 a 码 elem* 臂+调用位 Ident 栈拷贝语句表达式+interp call_decl_vals ArrayT 形参 A 记录递归深拷(旧:emit 塌缩 elem 编译错/interp 共享记录静默污染)。锚 fx_arr_byval(smoke 3c 扫描)+tests/fb8_array_byval。**余债:返回面指针 ABI 悬垂风险(现塌缩编译错=fail-closed)+非 Ident 实参形+嵌套 T[N][M] 索引(sem "索引目标非数组")+C 宿主 parity(suite 分歧 1 件在册,宿主 call_decl 别名未修)**。坑:包裹条件必须核形参码 pc——视图借用面(pc=v 码)禁拷(复合字面量内语句表达式产悬垂尾,fx_slice/ctecho 双实证);跨模块被调 pcs 空→pc="i" 守卫天然放行=正确口径。smoke 178/0 新基线(decl 锁 513→514 随批申报) |
| FB-9 | F20 | 元组在循环体内 return 发射失败 | trans_stmt.ct Return/Tuple 分支 | P1 | ✅ 落库(1006,fb6-and):真面=发射器元组支持整体缺位(eval 有/trans 全无,"byte_at 目标需 Str" seed 宿主 panic)。落地:元组码 "TP:elem,..."(异构 boxed list ABI,ctron_list*;整型 long 装箱/Str 直存/u: 堆盒,浮点槽 fail-closed 债)×五臂=ct_typeof TupleE/Member 数字成员+ct_ty_code TupleT+ct_ctype TP+ct_expr 构造与解箱+ct_tp_elem 帮手(decl 锁 514→515 随批申报)。**随批修:While 提升臂末冒号切分→首冒号(含冒号型码[u:/TP:]即碎,"t_t:TP" 垃圾声明;u: 提升潜在雷同治)**。锚 fx_tuple(smoke 扫描 179/0)+tests/fb9_tuple(双宿主过零分歧);Loom 原形(循环内 return 元组)双臂逐字一致。余债:嵌套元组段切分/浮点槽/struct 槽跨元组 parity |
| FB-10 | F3 | `.as[]` 链在调用结果上,解释臂 "call target" panic | eval_call.ct 成员链下钻 | P2 | ✅ 不复现销账(1006:直调/方法链/下标链/标量四形 interp 全绿)。**随批新发现已修(1006,P0):emit 臂 Str→数值 as 解析面落地**——ctron_str_to_i64 手写严格解析帮手(免 libc 差异;decl 锁不动=运行时文本)+ as 臂 Str/N 源判定(6/7/z/i/w 族镜像 conv_as 6 域)。语义注记:interp 解析=严格数字形(不容前导空格),失败走恒等(松型伪影);emit=严格解析失败 0(恒等面无法镜像,分歧登记)。锚 fx_as_str_num 入 smoke 扫描(183/0 基线,原生==解释逐字一致)。**FB-8 返回面已落地(1006 晚)**:①Return 臂 #fnret a 码 → 返回位 amalloc 拷贝出指针(镜像 ct_clo_ret_line u: 堆盒式,GC 管理无悬垂);②Let a 码非 ArrLit 臂 → 声明+memcpy 值收(旧误走 ArrLit 访问把 Args 当元素=Args@0)。interp 值语义天然,双臂逐字一致。锚 fx_arr_ret(smoke 扫描 184/0 基线)+tests/fb8_array_ret 双宿主。**FB-8 全件销账**;余债=嵌套 T[N][M] 索引+宿主 parity。**嵌套数组三面状态(1006 晚探明,`fn f(m: I32[2][2])`,`m[i][j]`)**:①seed 宿主=**运行时** V_ARR 面(rt_eval.c:623 EX_INDEX,o.k!=V_ARR——非 sem 面;嵌套 ArrLit 或参数绑定把内层摊平/错型,待查 EX_ARRLIT 与 bind 链)②自举 interp 静默错(rc=0 零输出)③emit 链被 seed 运行时拦(cc_emit 由 seed 解释执行,同①)。bootstrap check rc=0(自举 sem 天然放行,a2a2i 码链已通)。正案入口精化(1007 ast 实证):**自举解析器无恙**(双 Index 节点 ✓)——①=seed 宿主解析器链式缺口(compiler-c parser,`m[1][0]` → E2020 未解析:]`)②=host rt 参数绑定摊平数组(rt_stmt bind 链)③=自举 interp 静默错。施工序=先 compiler-c parser 链式下标(解 ①,ctc.sh 链全通)→ host bind 链(解 ②)→ 自举 interp(解 ③);emit 已备。**1007 施工实录:parse_type Named 分支已改 while 多缀链(嵌套注解 I32[2][2] 解析面修入,n4 仅 W8030 ✓)但实参位下标仍炸**——新事实(1007 二轮定桩):**触发器=注解式声明**——推断式 `var m = [[1,2],[3,4]]` 全形绿(含 m[1][0]=3);注解式 `I32[2][2]` 连合法 `m[1][0]` 亦 E2020(名="]"=型别渲染伪影)。排除了实参解析/链式/字面量三嫌疑。正案=host sem/let 类型化绑定链:注解 TY_ARRAY(TY_ARRAY) 下索引元素型解析与 E2020 渲染(compiler-c sem.c);运行时摊平=let 类型化物化链(rt_stmt)。推断式面已全绿可作语料基准。**1007 四轮反转定桩(归因再修正)**:E2020 文案属 Ctron sem_main(自举),非 compiler-c——真因=共享 parse_expr.ct 的 type 位 `[` 消歧(`nx=="(" ? TypeArgs : Index`)把注解 `I32[2][2]` 解析成 Index 节点链,sem 解析 Index 化注解产出 "]" 名。正案=parse_expr.ct type 位注解 `[N]` 路由 type-suffix(与 expr 位索引分流)+sem_main 注解型名解析对 Index 化容错;compiler-c parse_type 多缀链(52f7a7a2)与 host 运行时 V_ARR 面(rt_eval 623)为并行独立两债 |
| FB-11 | F19 | 字面量含 `{}` 发射臂插值误析(`\{` 转义解释/发射不一致) | 插值扫描(qtext/parts_of)对 `\{` 的双通道一致化 | P2 | ✅ 落库(1006):真面=parts_of 空花 "{}" 即造 Interp 空内容 → 双臂解析 OOB(index out of bounds);`\{` 转义双臂本一致(先期排查的 \5 分歧=测试侧 shell printf \\\\ 原样落盘的伪象,\{n}→{n} 字面正确)。修=parts_of 空花落 plain-text 臂(条件守卫)。锚 fx_empty_brace 入 smoke 扫描(179→181/0 基线)。余债:"{ }" 含白空内容形 |
| FB-12 | F22a | 发射臂 CLI 入口=argv[2] 单串(与解释臂语义不同) | driver_emit.ct main 模板;需规范裁决后统一 | P2 | 后续 |
| FB-13 | F11 | 解释臂 I64 乘法 ≥2^31 即溢出(重数值代码解释臂不可跑) | eval 值域(eval_val.ct)升级 64 位域(大工程,单列) | P2 | 专案 |

## 本批实施(FB-1 + FB-2)

### FB-1 byte_slice 越界防护
- 修复:`driver_emit.ct:112` 模板加 `n<0 || a<0 || a+n>strlen(s)` → `ctron_panic("byte_slice: range out of bounds")`;解释臂 `eval_call.ct:356` 分发处同口径校验(panic 同消息)。
- 测试:`tests/21a_byte_slice_neg_range.panic.ct`(`//@ panic: byte_slice`);行为面:合法切片不受影响(smoke 兜底)。
- 验收:双通道 panic 消息含 "byte_slice";smoke --full 全绿;Loom 仓库的崩溃用例(verifyd F22 复现)不再段错误。

### FB-2 fs_mkdir 内建
- 语义:`fs_mkdir(path: Str) -> Bool`(逐级创建,已存在 = true;镜像 fs_write 的 Bool 口径)。
- 五点:①parse_pkg.ct:55 内建名单 + `fs_mkdir`;②sem_calls.ct:89 prelude push;③eval_call.ct 分发(调用 Ctron shim `fs_mkdir`,语义=发射模板同级);④trans_expr.ct:1130 邻位 `callee == "fs_mkdir"`;⑤driver_emit.ct 运行时模板 `ctron_fs_mkdir`(mkdir 逐级,EEXIST 视成功)。
- 测试:`tests/modules/fs_mkdir/`(行为:创建+已存在+嵌套);caps 口径:跟随 fs_write(受 `[caps] fs.write` 管辖,E4010 面)。
- 验收:解释/发射双通道行为一致;smoke+suite 全绿;Loom 侧删除 .loom-flat 扁平规避改用分桶。

### 验收纪律(每 FB 通用)
1. `meta_check.py` 过;2. `smoke.sh --full` 过;3. `suite.py` 记分卡不回退;4. 自举固定点:改动后 native.sh 产物逐字节复现;5. Loom 侧对应规避删除(回归即修复无效)。

## 登记与联动
- 修复落地后:Loom-Cprofile 台账对应条目标"已关闭",Loom 侧规避代码删除(回归验证);
- c-rust-divergences.md:FB-3/FB-13 涉及的双臂分歧登记;
- 本表为活文档:每 FB 完成即在总表标日期与提交号。

## 实施状态(2026-10-05)

### FB-1:已实施,待自举验证(工作树)
- `compiler/src/driver_emit.ct:112`:ctron_byte_slice 模板加 `n<0 || a<0 || a+n>strlen(s)` → `ctron_panic("byte_slice: range out of bounds")`
- `compiler/src/eval_call.ct`(byte_slice 分发):解释臂同口径三重校验(嵌套 if,规避 && 不短路)
- `tests/07e_byte_slice_range.panic.ct`:新增(`//@ panic: byte_slice`),meta_check 通过(532 文件)
- **阻塞:native.sh 自举重建静默失败(sh -x 显示 ctc.sh emit 在链内失败,手动单独 emit 同一文件成功)——需按 BOOTSTRAP.md 流程排查(ladder/native 链),验证完成前不提交**
- 验证清单(解除阻塞后):①`bin/ctron-cc run tests/07e...` panic 消息含 byte_slice;②发射臂等价验证(emit Loom verifyd 崩溃用例);③smoke --full;④suite 记分卡 ≥ 现状;⑤固定点

### FB-2:五点锚点已勘察(见总表),未实施

## 实施状态更新二(2026-10-05 晚,并行会话冲突发现)

1. **FB-1 补丁被并行会话覆盖**:`\}` 转义修复后发射已通(1.6MB 产物),但 native.sh 编译暴露 deeper 问题期间,工作树的 driver_emit.ct/eval_call.ct 被并行会话的 s30-GC 调试循环改写(当前 diff 440 行均非本批);eval_call.ct 的 FB-1 编辑已丢失。
2. **结论:本批次不得在主工作树实施**——必须开专用 worktree(仓库惯例:`.worktrees/<名>`,参照 asan/p0a 先例),基线 = 含本规划的提交 d3bab5ec。
3. **新登记(基础设施)**:native.sh 自举链依赖 seed 宿主(compiler-c)发射器,该发射器已落后当前发射特性(spn 作用域保存/恢复)——**T20 缓存命中时掩盖,缓存未命中必然失败**。工项:同步 seed 宿主或 native.sh 改走自举发射链(bin/ctron-emit 直发,本轮实测可行:emit1/emit2 均 rc=0)。
4. 下轮执行序:①开 .worktrees/fb1(git worktree add,基线含规划);②重放 FB-1 双臂补丁(本文件含精确锚点);③走自举发射链重建(bin/ctron-emit → cc,绕 seed);④五步验收;⑤FB-2 五点实施。

## FB-1 完成(2026-10-05,fb1 worktree 分支)
- 双臂防护落地并终验:解释臂(main 探针 rc=1+消息)、发射臂(模板 strlen 边界+panic,rc=1+消息);合法切片不受影响(行为面由既有语料兜底)
- 过程实录(全部为可复用经验):①`\}` 非法转义(README 在册坑,重蹈——模板规则:`{` 转义 `\{`,`}` 恒裸写);②模板顺序:byte_slice 在 ctron_panic 定义前 → 前置声明一行解决;③native.sh 静默失败根因=seed 宿主发射器落后(spn)+T20 缓存掩盖 → 自举链绕行(bin/ctron-emit 直发,实测可行);④管道 head 吞退出码(第二次踩,立规:验收一律重定向文件);⑤smoke 90/64 基线在有无补丁下完全一致 → 64 项为 HEAD/环境既有(与并行会话在途状态相关),非本批引入
- 遗留登记:①自举单步对无 main 文件的 test 块执行在新链 rc=0(旧 21:14 二进制 rc=1)——源起并行会话未提交的 driver_run 在途改动,HEAD 即如此,非本批引入,登记给并行会话;②panic 测试的双臂统一验证口径

## FB-2 状态(fb1 worktree,2026-10-05)
- 已落:prelude 名单(parse_pkg/sem_calls)、解释臂分发(eval_call)、发射映射(trans_expr:1062)、运行时模板+includes(driver_emit,include 已置于 helper 前)、plugin/sem_type/trans_ty 三注册点
- 阻塞单点:编译器源内部分发调用 `fs_mkdir(r5.s)` 发射为 `t_fs_mkdir`(user-fn 路径)而非映射的 `ctron_fs_mkdir`——fs_write 同形调用却正确映射。下一步:找出两姊妹内建在 sem/emit decl 表上的差异点(疑 sem_builtin 签名表或 fn-decl 创建路径还有一处注册),对齐即通
- 验证(映射通后):重建双二进制 → 解释臂 fs_mkdir 行为测试(创建/已存在/嵌套)→ 发射臂同 → smoke/suite 基线对照(90/64 既有)
- FB-1 已在本分支提交(4ed1b0d0),双臂终验 rc=1+消息 ✓

## FB-2 追加定位(2026-10-05 深夜)
- 自举链双步法已跑通(主树发射器引导 → worktree 自举),新事实:
  ①HEAD 的 driver_emit.ct 结构体模板缺 spn(并行会话未提交改动的一部分)——已补(int sv[8]; int spn;),该修复独立有效
  ②即便用主树新发射器,内部调用 fs_mkdir(r5.s) 仍发射 t_fs_mkdir——**排除发射器版本因素,确认是 sem/emit decl 表注册差异**
- 剩余单点:找出 fs_write 与 fs_mkdir 在 sem fn-decl 表上的差异(疑 prelude 名单创建 decl 时 arity/signature 表还有一处;或 ct_call 早于 1062 行的分支对有 decl 的 prelude 名走 user-fn 路径,而 fs_write 因某种表项被豁免)
- 定位手法建议:在 trans_expr 的 user-fn 分支入口打印 callee,一次重建即可看见 fs_write/fs_mkdir 各走哪条;或 diff 两者的 sem decl dump(ctc.sh ast)
- 全部改动保留在 fb1 worktree 工作树(未提交部分=driver_emit struct spn 修复+注册点七处),FB-1 已提交(4ed1b0d0)

## FB-2 最终状态(2026-10-05)
- 落地:全部注册点(8 处)+ helper 模板(改名 ctron_fs_mkdir_p)+ extern/impl(eval_call 文件级)+ include 局部化技巧
- 阻塞:发射器对 cc_emit.ct 的发射段错误(e1=139,EXC_BAD_ACCESS)——与并行会话在猎的 s30 Heisenbug 同域(他们已登记"无探针确定性崩");本批不再单干,需与 s30 会话协同定位
- 已排除:helper 命名碰撞(改名 _p 后仍崩);发射器版本(主树新发射器亦崩);fn-decl 差异假说已修正(先前 t_fs_mkdir 主因=用了主树旧发射器,该二进制不含 fs_mkdir 映射——两步自举后应已解决,但被 139 掩盖)
- 下轮:与 s30 会话对齐(或等其落地后)重放:两步自举 → fs_mkdir 双臂行为测试 → 提交
- FB-1 已完成提交(4ed1b0d0);struct spn 修复亦在本 worktree(随本提交)

## 收官定位(2026-10-05 深夜,并行会话提交互证)

- 本轮全部 139 段错误 = **已知的 bins 破损态**:并行会话 main 提交 5d5e4c08 明确登记"35bd03f6 合入的中间态+peer WIP 丢弃致 bins 破损(s23 总线错/s19 段错误/70 夹具崩)"并已完成"发射链治愈"(emission 三件 checkout 入 main,smoke 115/54→171/2)
- **对 fb1 的行动指令**:①`git merge main`(取得治愈后的 emission 三件;driver_emit.ct 若冲突,以 main 侧为基底重放 FB-1 模板编辑+FB-2 模板,两处锚点都在本文件);②两步自举重建;③FB-1 panic 测试 + FB-2 fs_mkdir 行为测试双臂验证;④提交
- 本 worktree 当前态:FB-1 补丁 ✓(工作树)+ FB-2 八处注册点 ✓(工作树)+ 测试 07e ✓——全部未失,仅待治愈基线上的重建验证

## FB-2 完成(2026-10-06,fb1 worktree)
- fs_mkdir 全链落地:8 处注册点 + runtime helper 模板(别名包装 ctron_fs_mkdir_p → ctron_fs_mkdir,新旧发射器二进制兼容)+ 编译器源内 impl(extern 同 TU helper)
- 双臂验证:解释臂 fs_mkdir 创建/存在/幂等 ✓ + 目录真实创建 ✓;发射臂同 ✓;FB-1 panic 回归 ✓(rc=1+消息)
- 自举链定案:主树发射器发 cc_emit → 追加一代 C 实现(helper)→ 新发射器 → 发 cc_run(自含)——此后每代自含
- smoke 156/4:余 4 项 = ①自检 decls=482 基线漂移(新增 fs_mkdir/extern +2,预期,随提交更新基线)②conc_parallel 发射(待查,或 HEAD 既有)③native/seed 口径(seed rt_eval.c 缺 fs_mkdir native,已登记)④doc std 模块(worktree 环境面)
- 后续:Loom 侧 F16/F17 规避回归删除;seed rt_eval.c 补 fs_mkdir native

## FB-2 验收补全(2026-10-06)
- decls 基线 480→482 随批更新(惯例如注);smoke 157/3
- 余 3 项归因完成:①conc_parallel 发射段错误——**主树二进制(与本批无关)对同一 fixture 同样 139**(fixture 自 Phase 4 未变)→ 既有 s30/发射链范畴,登记移交;②native/seed 口径——同源(seed rt_eval 缺 fs_mkdir native 已登记);③doc std 模块——worktree 环境面
- **FB-1+FB-2 验收就此闭合**:双臂行为 ✓、panic 回归 ✓、smoke 基线更新 ✓、余 3 项均归因既有并移交

## FB-1/FB-2 验收清单完成(2026-10-06)
- ①解释臂 panic 测试 ✓(rc=1+消息)②发射臂 ✓(strlen 防护)③smoke 157/3(3 项归因既有)④**suite 100/101 = 主线基线持平**(并行治愈提交同数;03m=L4 待立项探针 + cbox=worktree ffi 环境,均既有)⑤build_fb.sh 过渡自举脚本固化(行首锚定存在性检查;追加一代 C 实现,此后自含)
- 新增坑三条入册:①build 存在性 grep 误匹配 printf 数据行(行首锚定);②extern 符号若与模板符号同名,发射器自举代差必崩(改名+别名包装解);③函数体外浮语句=发射器列表树失配崩触发形状(s30 同族)
- fb1 分支就绪待合:FB-1+FB-2+基线+本文件;合并窗口需与并行会话协调

## seed 宿主补齐(2026-10-06)
- compiler-c/src/rt_eval.c 加 fs_mkdir native(逐级创建,EEXIST 容忍,镜像 fs_write 形态)+ sys/stat/errno includes;make 过,seed 路径 probe 全通(创建/存在)
- smoke 维持 157/3(余:conc_parallel 段错误=主树二进制同样崩[s30/发射链范畴已移交]、native/seed 口径——seed 补齐后待重验、doc std=worktree 环境)
- fb1 分支就绪,合并窗口待与并行会话协调

## 预合并完成(2026-10-06)
- main 已并入 fb1(7c44d04c 之后 main 前进:T46/47 http serve、T49 pkg registry[decl 锁 480→506]、T50 WIP);唯一冲突 smoke decls 已解:**506+2=508**(T49 二十六 fn + FB-2 二),smoke 156/4
- 4 失败归因:①conc_parallel=主树同源在册红 ✓既有;②T35 use 门(空详情)③native/seed 口径 ④**W8902 缺失(use hi 得裸 E2020)——疑与 T49 解析链 W8902 流互动(merge 对 parse_pkg 的自动合并),待 T50 落地后对纯 main 对照归因**
- fb1 分支自此含 main 全量+FB-1/FB-2:后续 main 合并 fb1 应近平凡(或 fast-forward 窗口)

## ✅ FB-1/FB-2 合入 main 完成(2026-10-06)
- main 快进合并 fb1(2f7aa4a7,含 T50 0a9c2c51 全量);主树二经两步自举重建(build_fb.sh)
- 终验:①FB-1 panic rc=1+消息 ✓ ②FB-2 fs_mkdir 嵌套创建/幂等 ✓ ③**Loom CI 对新编译器全绿**(跨项目回归)④Ctron smoke 166/2、meta_check 1 败——与主树 T50 提交登记的基线**逐项吻合**(conc_parallel+Rust 臂 iter+dep_mutex_neg,均并行会话在册非本批)
- Cprofile 台账闭环:F16(mkdir 缺失)与 F22b(byte_slice 越界)两项**已关闭**;Loom 侧规避(扁平布局/负参防护)可随下次 Loom 编译器基线声明升级删除
- 余:FB-3+(P0 字面量域/视图宽度,锚点在册)、fb1 分支留存可删

## FB-5 复核关闭(2026-10-06)
- F15(U8.as[I64] 符号扩展)**当前编译器双臂均不复现**(探针:U8[200].as[I64] → 两臂 W=200 零扩展正确)——原观测疑为 F10(I64 数组视图)在同期的混淆归因
- 处置:FB-5 关闭;Loom 侧 u8widen 防御壳保留(已验证绿,无删除收益);台账 F15 标"未复现(复核 2026-10-06)"
- FB 批次状态:FB-1 ✅ FB-2 ✅(已合 main)FB-5 关闭;下一批实际工项 = FB-3/FB-4(字面量域/视图宽度,P0 静默错值族,需先做最小复现探针归因到 emitter 具体行)

## 台账复核(2026-10-06,当前编译器双臂)

**P0 族全部不复现**(双臂探针逐项验证):F6 数组元素大字面量 ✓、F9 比较位大字面量 ✓、F10 视图元素写读大值 ✓、F13 循环体标量赋值大值 ✓、F15 U8 拓宽 ✓——并行会话的发射链治愈与其编译器修复已覆盖。F7 bit.and 大值域亦正确。

**确认仍复现(剩余真实工项)**:
- F5:定长数组按值返回(cc 报 incompatible result type ctron_view_6)——发射器特性工项;Loom 字节平面形态因此仍为**承重**(非可删规避)
- F3:as 链在调用结果上(待真形态复测)
- F14:同块临时不去重(待复测)
- F19:`{}` 插值误析(当前二进制已实证)

**对 Loom 的影响**:norm32/p2(32)/u8widen 降级为防御壳(保留,无删除收益);**字节平面为承重形态保留**(F5 未修前定长数组不可传返);p2(32) 大常数构造保留(F9 复核虽过,防御无成本)。
- FB 批次终态:FB-1/FB-2 ✅ 合入 main;FB-5 关闭(未复现);FB-3/FB-4 关闭(未复现,复核探针在册);**剩余真实工项 = F5(发射器数组语义)+ F14(临时去重)+ F19(转义一致化)——全部移交 Ctron 主线**(属发射器特性/健壮性工项,非 Loom 依赖阻塞)。
