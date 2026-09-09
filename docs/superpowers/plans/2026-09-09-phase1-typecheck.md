# Phase 1：类型检查器 v0 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

承接 `2026-09-08-spec-gap-closure.md`（五阶段总计划）的 Phase 1。所有改动仍在 `compiler/`（自举 Ctron 源）内，验收口径不变：smoke --full 全绿（黄金逐字、自发射、固定点）+ suite.py 50/50 + 自检（编译器自身过检查）。

**Goal:** 在 Phase 0 的 bare 调用面之上，落成保守可证的类型检查 v0：E2020 全量（Ident 读解析）、E2010 基础类型统一（let/return/实参/二元/条件/`?` 合法性）、变体构造 arity、以及 W8030/W8040 的经验性启用判定。

**Architecture:** 扩展 Phase 0 引入的 `sem_calls_all` 单遍走（uce/ucb）为带类型环境的组合检查走：新增平行名集（callable 集 vs value 集）、statics/consts/struct 字段/枚举变体表、类型 kind 推导（ty_kind/ex_ty/compat）。所有检查**保守可证**：任一侧类型未知即放行；字面量按期望整/浮类型自适应（§3.7）；`T → T?`、`String → Str`、`T[N] → T[]` 隐式按 §3.6 白名单放行。

**Tech Stack:** 同总计划（Ctron 自举 + C 宿主 seed + sh/python3 测试面）。

## Global Constraints（沿总计划 + Phase 0 实战新增）

- 新诊断只用已注册码（E2020/E2010/W8030/W8040 均在 §10.1 表内）；文本面 `CODE: msg` 逐字不变（黄金锁定）。
- 语言坑（BOOTSTRAP 不变量 #1/#2/#11）：无 `;`、无 `||`（用 or2/or3）、单行 if 块不容 `return`、`\}` 非法转义。
- 每任务落地后立即过三道门：`ctc.sh check build/cc_run.ct`（自检）、`test/smoke.sh`（快面）、`test/suite.py`（50/50）；绿则提交。
- decl 锁定数字随新增 fn 同步（当前 cc_run=195 / emit=224）。

## 设计裁决（v0 保守性边界）

1. **双名集**：bare 调用目标 ∈ callable 集 = `Fn/FnC/FnPub`（**不含 Method**——eval 的 find_decl 不解析方法）∪ 前奏可调用（prelude_ok）∪ 用户枚举变体；Ident 读 ∈ value 集 = callable ∪ `static let/const` ∪ 前奏值名（`parallel`、`arena` 必须入集——corpus 有 `parallel.map` 与 own 块内 `arena.array`）。发现误报一律先修名集再谈检查逻辑。
2. **类型 kind 串**：`n:<名>`（Named/TArgs 头名）、`opt:<内>`、`ref:<内>`、`slice`、`arr`、`tup`、`fn`、`void`、`range`、空串=未知。
3. **compat(exp, act, e)**：任一侧未知 → 放行；相等 → 放行；Int/Float 字面量 vs 数值期望 → 放行（§3.7 自适应）；`exp=="opt:X" && act=="X"` → 放行（§3.6 T→T?）；`exp=="n:Str" && act=="n:String"` → 放行；`exp=="slice" && act=="arr"` → 放行（定长退化）；其余 → E2010。
4. **E2010 触发面（v0）**：带注解 let 的初始化、return 实参 vs 函数返回注解、bare 调用实参 vs 形参注解（两侧皆知）、二元算术的 Str 规则（§4.5：`- * / %` 遇 Str 必报；`+` 单侧 Str 报）与 Bool 混入、`&&`/if/while 条件已知非 Bool、`?` 出现在返回非 Option/Result/AnyError 的函数（§5.3）。
5. **变体 arity**：前奏 `Some/Ok/Err=1, None=0`；用户枚举按 KTuple/KStruct 槽位数。仅 arity，不查载荷类型（v1 尾项）。
6. **W8030/W8040 经验性启用**：实现后先跑门禁——若 suite/黄金/自检任一被 W 码打红（驱动把 W 同视为 rc=1），则本阶段不启用，如实记入边界（W8030 的插值盲区先补：Interp 原始文本含名即视为已使用）。
7. **解析器 span 标注顺延**：需要 sem_walk2 返回值改形（诊断串 + 平行行号表）并重验黄金，体量独立，移入 Phase 1.5，本阶段 JSON span 维持 v0 近似定位。

---

### Task 1: 核实类型节点布局

- [ ] `sed -n` 读 `p_typ`（sem.ct 同款 tag）、`p_enum2` 的 KTuple/KStruct 槽位、sem.ct:446 `env_ty`（查找方向）、sem.ct:732 `let_ty`（能否复用）、Field 节点槽位（sem W8010 用 f[2]/f[3]）。
- [ ] 产出：布局速记追加到本文件末尾「实施记录」。

### Task 2: E2020 全量（Ident 读解析）

**Files:** Modify `compiler/src/sem.ct`（sem_calls_all 预扫扩 statics/consts/variants/枚举名表 + 值名集 `value_ok`）；Test `compiler/test/fx_unresolved_read_neg.ct`。

- [ ] 夹具（期望 E2020：读未定义名 `conuter`）：

```ctron
fn main() -> I32 {
    var n: I32 = 0
    n = conuter + 1
    println(n.to_string())
    return 0
}
```

- [ ] 预扫扩表：`stN/stT`（Static d[1]/d[3]、Const d[1]/d[2]）、`vars`（全部枚举变体名）、`enums`（枚举类型名）。
- [ ] `value_ok(nm)` = prelude_ok(nm) ∪ {parallel, arena}；Ident 检查：`!in_list(loc,nm) && !in_list(fns,nm) && !in_list(stN,nm) && !in_list(vars,nm) && !in_list(enums,nm) && !value_ok(nm)` → E2020（dedup 沿用）。
- [ ] 三道门 → Commit。

### Task 3: E2010 基础统一

**Files:** Modify `compiler/src/sem.ct`（ty_kind/k_int/k_num/compat/ex_ty + 检查点接入 ucb/tcb 走，携带 envT/retK）；Test `compiler/test/fx_type_neg.ct`（一组小负例：let 注解矛盾、return 矛盾、实参矛盾、Str 参与 `-`、`?` 用于 I32 返回函数）。

- [ ] 实现（骨架）：

```ctron
fn ty_kind(ty: List[Str]) -> Str          // Named→n:名 TArgs→头名 opt:/ref:/slice/arr/tup/fn
fn k_int(k: Str) -> Bool                   // 10 整型
fn k_num(k: Str) -> Bool
fn compat(exp: Str, act: Str, e: List[Str]) -> Bool   // 设计裁决 #3
fn ex_ty(e, envN, envT, fns, rets, stN, stT) -> Str   // 表达式 kind;未知返 ""
```

- [ ] 检查点：Let（注解 vs init）、Return（vs retK）、Call bare 实参（vs Param[3] kind，`prms` 平行表）、Binary（Str/Bool 规则）、While/If 条件已知非 Bool、Try（retK 已知且非 opt:/n:Result/n:AnyError → E2010）。
- [ ] env 绑定：PatId → envN/envT push（注解优先，否则 init kind）；shadowing 双记录（保守放行）。
- [ ] 三道门 + 自检修复 → Commit。

### Task 4: 变体构造 arity

- [ ] `prelude_arity(nm) -> I32`（Some/Ok/Err=1 None=0 其余 -1）；用户变体表 `varAr`（平行 vars）。
- [ ] Call 检查处接线：prelude_ok 且 arity≥0 → 校验；rt ∈ vars → 校验。
- [ ] 夹具追加 `None(1)` / `Ok(a,b)` 负例 → 门禁 → Commit。

### Task 5: W8030/W8040 经验判定

- [ ] 走内收集 `reads`（每个 Ident 名 + Interp 文本子串命中）；fn 走完 diff 声明集 → W8030（`_` 前缀豁免）；Let 绑定名 ∈ 前奏名集 → W8040。
- [ ] 跑门禁：全绿 → 保留；任一红 → 回退此任务，边界文档记「W8030/W8040 待测试锚点先行」。
- [ ] Commit（保留或回退均记录实证结果）。

### Task 6: 冒烟/锁定/文档

- [ ] smoke.sh：decl 锁定更新；新增 fx_unresolved_read_neg/fx_type_neg 的 check 面负例项（rc=1 + 码 grep）。
- [ ] README 记分卡「第四批」；BOOTSTRAP 不变量补类型检查 v0 口径；总计划 Phase 1 状态更新。

### Task 7: 全量验证

- [ ] `smoke.sh --full` 全绿；`suite.py` 50/50；三夹具 rc 断言。

## 实施记录

（Task 1 起追加布局速记与实证结论）
