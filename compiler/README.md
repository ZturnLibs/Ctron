# compiler —— Ctron 自举编译器(模块化优化版)

> 源自 `selfhosted/`(自举唯一差分源):把 6153 行的手工单文件快照 `cc.ct`
> 与追加部件 `tools/trans_part.ct` 重组为**按模块组织、单一定义源**的编译器树。
> 语义与可观测行为逐字不变(见下方验收);夹具与黄金基线仍以 `selfhosted/` 为源,本目录不复制。
>
> **自举方案、三级证明的复现命令与实现不变量见 [BOOTSTRAP.md](BOOTSTRAP.md)。**

## 结构

| 文件 | 行/fn | 职责 |
|---|---|---|
| `src/lex.ct` | 298 / 18 | 词法器:token 串(哨兵 `#EOF`)+ 换行过滤(规范 §1.6 行延续) |
| `src/parse.ct` | 1413 / 35 | 解析器:token → 结构化文件节点树(`p_file`;节点 = `List[Str]`,child[0] = tag) |
| `src/sem.ct` | 2163 / 58 | 单文件语义检查全集(14 码 + E2020 调用目标解析 + E2010 调用 arity;`sem_walk2` → 诊断串) |
| `src/eval.ct` | 2848 / 83 | 树行走求值器:I32/Bool/Str 插值/Float/if·while·for/match/Option·Result·`?`/struct·枚举/UFCS/闭包/List 引用语义/trait·impl |
| `src/trans.ct` | 1131 / 30 | C 代码生成器 v3(`t_` 用户符号 / `ctron_*` 运行时 / 类型码 i·s·b·f·L·A·N) |
| `src/driver_run.ct` | 46 | 入口(运行):parse → 语义 12 项 → 解释执行 main/test |
| `src/driver_check.ct` | 165 | 入口(检查):parse → 语义检查即止,文本面 `check OK decls=N`;`--format=json` 出 §10.2 冻结 schema 诊断(span 为 v0 近似定位) |
| `src/driver_emit.ct` | 72 | 入口(发射):parse → 生成等价 C(产物 gcc 可编译,`<bin> run <file>` 覆锚) |
| `build.sh` | — | 按上表拼接出单文件产物(宿主 seed 可解释的 `.ct`) |
| `ctc.sh` | — | 统一驱动:`ctc.sh <in>` 运行 / `ctc.sh check <in>` 检查 / `ctc.sh emit <in> [out.c]` 发射 |
| `native.sh` | — | 编译出原生编译器二进制 `bin/ctron-cc` 与 `bin/ctron-emit` |
| `test/smoke.sh` | — | 验收冒烟(快面 15 项;`--full` 加自发射收官与固定点) |
| `test/suite.py` | — | 用 `tests/` 一致性测试集(可执行规范)验证本编译器,对照 C 宿主 |

Ctron 当前为单文件程序模型(无本地多文件模块),模块化以**确定性拼接**实现:
`cc_run.ct` = lex+parse+sem+eval+driver_run;`cc_check.ct` 换 driver_check;
`cc_emit.ct` = 四核心 + trans + driver_emit。核心 167 fn 三产物共享,改动单点生效。

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
- 发射器能力面 = `fixtures/trans_v0–v3` + 编译器自发射;"编译任意 Ctron 程序"
  (struct/闭包/并发/插值/值位 if·match)仍按 selfhosted 排期;
- `pkg_chk.ct`(模块级检查 + E6010 comptime 预算)属包管理 oracle,未纳入;
- 宿主 seed(`compiler-c/build/ctronc`)仅承担首次引导,bootstrap/ladder 流程仍在
  `selfhosted/`。
