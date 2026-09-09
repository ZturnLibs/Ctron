# compiler/src 分模块重构设计(2026-09-09)

## 目标与约束

`compiler/src/` 四个核心文件过大:sem.ct 3712 行、eval.ct 2873 行、parse.ct 1721 行、
trans.ct 1350 行。按逻辑与职责拆为单一职责的模块文件,每个文件一个明确职责,
不再有千行文件。

硬约束:

1. **语义与可观测行为逐字不变**——函数只搬家、不改动;新增行仅限 `//` 注释头。
2. **fn 总数不变**(smoke 锁定 `decls=243`;纯注释不产生 decl)。
3. **拼接顺序保持原文件内相对顺序**——重构后 build 产物与基线 diff 仅剩注释头差异,
   可机械证明"只搬家"。
4. 函数定义顺序不影响语义(src 内无顶层 `let`/`static` 状态,调用按名运行期解析,
   跨文件调用本就是常态),保序是额外保险,不承担正确性职责。

## 模块图谱

拆分沿现有 `// ----` 分节横幅与函数簇天然边界,**连续切段、不重排**:

### sem.ct(3712 行)→ 14 个检查项单元

| 文件 | 职责(检查码) |
|---|---|
| `sem_walk.ct` | W8020 must-use / E4030 no_spawn 树上行走 |
| `sem_send.ct` | Send 内核(s_prim/s_special/decl_node/send_of) |
| `sem_own.ct` | E3060 own 内 GC 可变写 |
| `sem_pure.ct` | E4020/E6020 pure·comptime 能力调用(&Trait 接收者) |
| `sem_spawn.ct` | E3010 spawn 闭包捕获非 Send |
| `sem_move.ct` | E3050 own 内 arena 句柄 use-after-move |
| `sem_exh.ct` | E2030 match 穷尽 |
| `sem_alloc.ct` | E3040 分配效果(own/#[no_alloc]/契约) |
| `sem_main.ct` | `sem_walk2` 12 项主控编排(接口文件) |
| `sem_type.ct` | E2010 类型统一 v1(含 E2020 前奏助手) |
| `sem_calls.ct` | E2020 调用目标解析 |
| `sem_comptime.ct` | E6020 comptime 副作用扫描 |
| `sem_ceval.ct` | E5010/E6010 助手 + 步数预算求值器 ceval |
| `sem_closure.ct` | E3070 闭包可变捕获(两遍式) |

### eval.ct(2873 行)→ 9 个值域/执行域单元

| 文件 | 职责 |
|---|---|
| `eval_val.ct` | 数值文本解析 + 值构造器 + 比较/算术 |
| `eval_width.ct` | 宽度域 u8/i8/u16/i16 |
| `eval_float.ct` | Float 十进制定点(df_* 家族) |
| `eval_env.ct` | fmt/环境链/文本工具/字符串求值 |
| `eval_trait.ct` | trait/impl 方法域 |
| `eval_pat.ct` | 绑定克隆/原地写 + pat_match |
| `eval_expr.ct` | `eval_expr` 核心表达式遍历 |
| `eval_call.ct` | 调用分派:fn 值/UFCS/成员内建(eval_call/call_mem) |
| `eval_run.ct` | 语句/块/静态初始化/run_tests |

### parse.ct(1721 行)→ 5 个层单元

| 文件 | 职责 |
|---|---|
| `parse_node.ct` | 树节点助手 + 插值原文切分(qtext/parts_of) |
| `parse_expr.ct` | 表达式优先级链 + 类型 + or 层 |
| `parse_stmt.ct` | 语句/模式/if/match/block |
| `parse_decl.ct` | 声明(fn/struct/enum/class/trait/use…)与入口 `p_file` |
| `parse_pkg.ct` | 模块加载器 v0(use 解析/可见性/循环检测) |

### trans.ct(1350 行)→ 4 个发射单元

| 文件 | 职责 |
|---|---|
| `trans_ty.ct` | 符号约定/运算符表/类型码/环境/C 字符串转义 |
| `trans_expr.ct` | 表达式发射(ct_expr/实参/提升/零值/数组码) |
| `trans_stmt.ct` | struct 表/块/语句/match/if 值位发射 |
| `trans_emit.ct` | 函数发射/main 锚替换/文件样板 |

lex.ct(310 行)与 driver_{run,check,emit}.ct 已是单职责,不动。

## build.sh

`CORE` 变量按原相对顺序列出全部 32 个核心文件;三产物组合方式不变
(cc_run / cc_check / cc_emit)。拼接是唯一"链接"机制,Ctron 单文件程序模型不变。

## 验收

1. 重构前后 build 产物 diff **仅注释行差异**(机械证明"只搬家不改码");
2. `test/smoke.sh --full` 全绿(黄金逐字/负例拦截/decls=243/发射往返/自举固定点);
3. `test/test/suite.py` 一致性测试集与 C 宿主对齐(50/50);
4. fn 总数不变(= 240 核心 + 3 驱动锚)。

## 不做什么

- 不合并/重命名任何函数,不改任何诊断文案与 golden 基线;
- 不引入本地多文件模块机制(Ctron 语言层面尚未支持,拼接仍是构建方式);
- 不动 selfhosted/、compiler-c/、compiler-rust/ 与历史 docs。
