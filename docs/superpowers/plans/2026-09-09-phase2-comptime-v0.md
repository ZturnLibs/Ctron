# Phase 2 切片：comptime 编译期求值 v0 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

承接总计划（2026-09-08-spec-gap-closure.md）Phase 2。裁决：**解析器 span 标注（Phase 1.5）顺延**——其实现需贯穿 lex→parser 十余个 p_* 签名或引入 O(n²)/全局态，属宽面管道工程；而 comptime 是规范的招牌能力（§8「有边界的编译期执行」）且当前完全缺位（`comptime fn` 只是个标记，实际在运行期执行），价值更高，先行。

**Goal:** `comptime`/`const` 语义前置到编译期：comptime fn 副作用静态扫描（E6020）、const/static-let 初始化在 check 面试求值（失败 → E6010）、const 注解类型核对（E2010）。求值器即 eval 自身的树行走（spec §9.7 "CVM 解释器"的 check 面用法）。

**Architecture:** 新增 `sem_comp(file, diags)`，在 sem_walk2 尾部、**仅当其余诊断为零时**执行（先名字/类型后求值，避免雪崩与级联噪声）。求值镜像 `statics_env` 的顺序迭代（env 逐 const 累积，前向引用同样失败——与运行期口径一致），但失败不再静默吞掉而是产出 E6010 并终止求值。

**Tech Stack:** 同总计划。

## Global Constraints

- 沿用全部既有约束（错误码已注册、文本面逐字、语言坑、三道门禁、decl 锁随 fn 数同步，当前 209）。
- **已知的 v0 限制（如实记录，不静默）**：comptime fn 内的 `while`/`for` 若写死循环，check 面试求值会挂起编译（无步数预算；真预算留 v1）。单文件自写代码模型下接受此风险。
- statics_env 现状：const 初始化失败被静默吞掉（绑定缺失 → 运行期"未解析名称"）——本切片将该类失败前置为 E6010，属行为收紧，门禁（语料 50/50）裁决。

### Task 1: E6020——comptime fn 副作用静态扫描

- [x] `scan_comp(file, e, diags, fnm)`：递归遍历 FnC 体（复用 tce 的遍历形态），bare 调用 ∈ {print, println, read_file, read_dir} 或成员名 ∈ {spawn, send, recv, store, fetch_add, with, with_mut} → `E6020: comptime 函数含副作用(effect):<名>`（dedup）。panic/assert 家族不ban（comptime 静态断言是合法用途，panic 失败在求值面映射为 E6010）。
- [x] 夹具 `fx_comp_neg.ct`：comptime fn 内 println → rc=1 含 E6020。

### Task 2: check 面试求值 + E6010 + const 类型核对

- [x] `sem_comp(file, diags)`：`diags.len > 0` 直接返回；否则按声明序对 `Const`(d[3]) 与 `static let`(d[4]) 逐个 `eval_expr(file, env, "", expr)`：
  - `vr[0] != "k"` → `E6010: comptime 求值失败(eval):<名> <fmt(vr[2])>` 并 return（不级联）；
  - 值标签核对标量族（I/W↔整型注解、S↔Str、B↔Bool、D↔F32/F64；U/命名跳过 v0）→ 不符 `E2010: const 类型不匹配(type):<名> 期望 <k> 实得 <标签>`；
  - 成功 → `env_add` 供后续 const 引用。
- [x] 接入 sem_walk2 尾部（sem_calls_all 之后）。
- [x] 夹具：`fx_comp_eval_neg.ct`（`const V: I32 = 1 / 0` → E6010）、`fx_comp_type_neg.ct`（`const V: Str = 42` → E2010）、正例 `fx_comp_ok.ct`（comptime fn 阶乘/拼接 + const 调用，check OK 且 run 输出正确）。

### Task 3: 门禁 + 文档

- [x] 三道门：自检（decl 锁 209→实测）、smoke 快面、**重建 native 后** suite 50/50。
- [x] smoke 2d 节接入三夹具；README 记分卡「第五批」；BOOTSTRAP 边界补「comptime v0 限制：无步数预算，comptime 死循环挂起编译」；总计划 Phase 2 状态更新（E6030/单态化/真预算留后续切片）。

### Task 4: 全量验证

- [x] `smoke.sh --full` 全绿；suite 50/50；五夹具 rc 断言。

## 实施记录

1. **E6010 语义修正**:spec §10.1 中 E6010 = "comptime **预算超限**"而非求值失败;
   求期 panic(eval 宿主级中止,不经 Ctron 流)以原文中止编译(rc=1)即正确的
   编译拒绝形态,E6010 锚点继续预留。原计划"失败 → E6010"作废,流级失败
   (vr[0] != "k")仍产出诊断。
2. **scan_comp 入口坑**:函数体本身就是 Block 节点,入口须走 `scb` 而非
   `scan_comp`(后者无 Block 分支)——语料化探针定位。
3. smoke 为 POSIX sh:`<()` 进程替换不可用,黄金比对改临时文件 diff。
4. decl 锁 209 → 215(+6 fn:comp_banned_call/comp_banned_mem/scan_comp/scb/
   vtag_ok/sem_comp)。
5. 实证:smoke 25/25;重建 native 后 suite 50/50;语料中所有 const/static-let
   在 check 面求值零失败(运行期静默吞绑定的路径未被语料触达)。
