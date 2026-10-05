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
| FB-6 | F18 | `&&` 不短路(已知坑,HANDOFF §4;Loom 再次踩中) | eval 臂 B_AND 求值序 + 发射臂条件化 | P0 | 下一批 |
| FB-7 | F14 | 发射器把复杂表达式临时变量提升到所在 C 块且不去重 → 多语句同块重定义 | trans_expr.ct 临时命名(按语句唯一化或最小作用域) | P1 | 后续 |
| FB-8 | F5 | 定长数组按值传参/返回(TupleE/Args@0) | trans_expr.ct 数组实参/返回码路径 | P1 | 后续 |
| FB-9 | F20 | 元组在循环体内 return 发射失败 | trans_stmt.ct Return/Tuple 分支 | P1 | 后续 |
| FB-10 | F3 | `.as[]` 链在调用结果上,解释臂 "call target" panic | eval_call.ct 成员链下钻 | P2 | 后续 |
| FB-11 | F19 | 字面量含 `{}` 发射臂插值误析(`\{` 转义解释/发射不一致) | 插值扫描(qtext/parts_of)对 `\{` 的双通道一致化 | P2 | 后续 |
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
