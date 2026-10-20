# 批C-2 泛型 struct 单态化命名统一 实施计划(Box[T] 实例码贯通)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 批C 真终点——`Box[泛型struct[实参]]` 的实例码全链贯通,解阻 `test/d_*` 家族回填
(TUI 泳道挂账)。根因已于 2026-10-20 代码级闭环(COVERAGE「批C 根因闭环」节):
①`B:` 码丢实例实参(`ct_ty_code`/`ct_typeof` 对 `Box[泛型struct[…]]` 产 `B:<基名>`,
trans_ty.ct:243/:1463 取 `nt[2][1][1]` 裸名);②`ct_call_infer_tys`(trans_ty.ct:2397)
对形参 `Box[泛型struct[TPar]]` 无分支,落到缺省 `ct_scalar_ty("I32")`(:2407)。

**Architecture:** `B:` 码格式升级为 `B:<完整内码>`(如 `B:u:TuiTest__U5_Model`)——产码点
两处、消费点九处统一改走「内码归一」;`ct_call_infer_tys` 增 Box 分支(拆内码→实例
split→按形参 TArgs 映射 tpsn)。发射 C 文本对**非泛型 Box 存量零变化**(`B:u:Point` 的
`ct_ctype` = `t_Point*`,与旧 `B:Point` 同果),只有泛型实例从「塌 base」变为「带实例码」。

**Spec:** COVERAGE「批C 根因闭环」节(d59258ad)+ 本文件。
**前置实证**(2026-10-20,`/tmp` 探针):
- `Box[S[M]].v.n` 未实例化 emit 绿;实例化(经闭包传 `Box[TuiTest[Model]]`)后
  `ct_expr:Member@<行>`(tt.v.buf 型别塌 i)。
- 三套 mangling 并存:`t_TuiTest__U5_Model`(构造)/`t_TuiTest__I`(typedef)/
  `t_test__u_Model`(函数)。
- fn 值/closure arity 仍 ≤3(trans_conc.ct:792;trans_ty.ct:1262 F 码族)——**本批不扩**,
  泛型 struct 携回调(单参 script)是既定绕行方向。

## Global Constraints

- `B:` 码格式变化波及全部 `Box[T]` 存量(std/net/db/tui/gui):**每 Wave 独立绿 +
  收尾全量 smoke --full + suite 双列 + gui/tui 阶梯**;红不落库。
- 家法:三处注册(sem/trans/eval)惯例;`mk("Named")`/`mk("TArgs")` 造型节点;
  字符串内裸 `{` 须 `\{`;提交 pathspec 限定;`.cache/emit` 清缓存真跑。
- **不扩 fn 值 arity**(trans_conc.ct:792 / trans_ty.ct:1262 F 码族不动,独立账)。
- 解释口径(eval_*.ct)不受影响(本批纯发射面/型别面);seed(rt_eval.c)不改。

---

### Wave 1: `B:` 码携全内码(产码点 + ct_ctype)

**Files:**
- Modify: `compiler/src/trans_ty.ct`(:243 注解产码点、:1463 构造产码点、:394 ct_ctype B: 臂)

**Tasks:**
- [ ] :243 `return "B:" + nt[2][1][1]` → `return "B:" + ct_ty_code(nt[2][1], file)`
      (标量→域码 `B:i`;结构→`B:u:<名>`;泛型实例→`B:u:<名>__<enc>`)
- [ ] :1463 同改(`Box[T](…)` 构造点 typeof)
- [ ] :394 ct_ctype B: 臂 `return "t_" + byte_slice(code,2,code.len) + "*"` →
      `return ct_ctype(byte_slice(code, 2, code.len)) + "*"`
      (`B:u:Point` → `ct_ctype("u:Point")+"*"` = `t_Point*`,与旧 `B:Point` 同果)
- [ ] 探针:`/tmp` 三件(Box[I32]/Box[非泛型struct]/Box[泛型struct[具体型]])emit + cc
      + run;decls 锁不变

### Wave 2: 消费点内码归一(九处)

**Files:**
- Modify: `compiler/src/trans_ty.ct`(:1357 Member B: 字段读;含本批已落的 `.v` 解引用臂)
- Modify: `compiler/src/trans_expr.ct`(:146 回调形参 cast、:452 字段读、:2643 trampoline 形参)
- Modify: `compiler/src/trans_stmt.ct`(:1136/:1141 Drop 登记名、:1216 赋值臂 isb 内码)
- Modify: `compiler/src/trans_conc.ct`(:105 捕获 decl)

**Tasks:**
- [ ] Member B: 臂(trans_ty:1357 / trans_expr:452 / trans_stmt:1216):内码 `u:` 前缀时
      字段型走 `ct_field_ty_inst(file, inner, field)`(实例替换),否则 `ct_field_ty`
- [ ] `.v` 解引用臂:内码直返(本批已落 `ct_ty_code(Named 内名)` 版,改直返内码)
- [ ] `t_<名>*` 三处(trans_expr:146/:2643;ct_ctype 已改)→ `ct_ctype(内码) + "*"`
- [ ] trans_stmt:1136/:1141 `has_drop_impl(file, ct_struct_name_of(内码))`
- [ ] trans_conc:105 / trans_expr:144:`ct_ctype(内码)` 中转(现走 ct + 变量,核对)
- [ ] 探针:存量 Box 用法抽查(gui bind 捕获、http/db Box 形参)emit+cc+run 逐字

### Wave 3: `ct_call_infer_tys` Box 分支(推断闭环)

**Files:**
- Modify: `compiler/src/trans_ty.ct`(:2397 ct_call_infer_tys,现有三分支后增第四支)

**Tasks:**
- [ ] 形参 `Named Box` + TArgs[0] = `Named 泛型struct[TArgs…含 TPar]` 分支:
      实参码 `B:<内码>` → 内码 `u:<名>__<enc>` → `ct_struct_inst_split` →
      `ct_inst_arg_codes` → 按内层 struct tps 位置映射回 tpsn(`ct_inst_ty_node` 包码)
- [ ] 探针:批C 复现件(tui_driver 真 context 的 TuiTest[M] + test/d_* 族)emit →
      **生成 C 无 undeclared、单实例 mangling 唯一**(grep `TuiTest__` 恰一枚)
- [ ] tests/tui 回填:s13_test 家族(test[M] + d_key/d_expect_text 全链,headless 断言
      渲染/帧计数/按键)双跑绿

### Wave 4: 回填 + 收官

**Files:**
- Modify: `pkgs/tui/tui_driver.ct`(test/d_* 家族原稿回填,批C 批注节摘除)
- Modify: `pkgs/tui/tui.ct`(facade use 行重挂 test/d_*)
- Create: `tests/tui/s13_test/`(夹具)
- Modify: `tests/COVERAGE.md`(批C 销账)、`compiler/test/smoke.sh`(decls 锁随批)

**Tasks:**
- [ ] tui_driver test/d_* 家族回填(12eec78a 原稿 + TuiTest[M] 泛化;facade re-export)
- [ ] s13 夹具:test[Model] + d_expect_text/d_key/d_frame 断言(渲染/帧计数)
- [ ] 全量门:smoke --full + suite 双列 + gui 阶梯 + tui 阶梯 + ctcl_check + meta_check
- [ ] COVERAGE 批C 销账登记 + Commit

## 明确不做

- fn 值/closure arity 扩到 >3(trans_conc.ct:792、trans_ty.ct:1262 F 码族)——独立账。
- 解释口径泛型 struct(runtime 侧;emit-only 门维持)。
- `Box` 用户自定义 struct 声明路径(既有旁路维持)。
- MSVC / windows 泛型面(G7 挂账域)。
