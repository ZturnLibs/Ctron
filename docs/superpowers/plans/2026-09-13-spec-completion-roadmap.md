# 语言规范未实现部分补齐路线图(Spec Completion Roadmap)

> **For agentic workers:** 本文档是主路线图。P0 各切片为任务级可执行;P1/P2 为切片概要,
> 执行时按仓库 SOP(HANDOFF 同步 + fx/负例/锚 + 三线一致 + 单独提交)各自产出日期命名的
> 详细计划后再实施。REQUIRED SUB-SKILL: superpowers:executing-plans 或 subagent-driven-development。

**Goal:** 补齐 v0.7 规范已规划但实现面缺失的部分,使三线(自举/C 宿主/R)与 spec 对齐。

**Architecture:** 按依赖与设计风险分四期。每期切片独立可测、独立提交;语言级特性保持
三线一致纪律(自举/C 宿主/R 线);所有新诊断码走「spec §10 登记 + 码表 + fx/负例锚」SOP。

**Tech Stack:** Ctron(自举编译器 compiler/,36 模块)、compiler-c(C 参考宿主)、
compiler-rust(R 线)、smoke/suite/meta_check 三层门禁。

## Global Constraints

- 三线一致:语言语义变更必须自举 + C 宿主 + R 线同步,或明确登记单线口径(如 E2050 先例)。
- 诊断新码:先 spec §10 + compiler/README 码表登记,再实现;负例/夹具锚随片落。
- 门禁底线:smoke --full 全绿 + suite 双侧一致 + meta_check 一致;自举固定点逐字节。
- 规范修订走「先改本文档 + 增补测试锚点,再改实现」(§10 末尾冻结规则)。
- 发射面语义变更必须同步考虑编译器自举传播(发射的检查会进入编译器自身,
  参见 HANDOFF「I32 发射面检查被自举传播阻断」条目)。

---

## 已核实撤案(不列入计划)

- §2.5 coherence(孤儿规则 E5010):**已实现**(sem_comptime.ct:178,双实现差分)。
- §7.3 Atomic API:load/store/fetch_add **已实现**(eval/trans 双面)。
- §3.5/§8.1 trait 对象 `&Trait` 动态分发:**能力对象路径已实现**(07_capabilities.ct,
  `&Clock` 注入 + 动态调用,双实现通过);泛型 bound 的 `&Trait` 泛化待观察,非当前缺口。

---

## P0 立即可做(小切片、无设计依赖)

### 切片 P0-A:struct 泛型注解实例化点 bound 核对

**Spec:** §3.9.2(v0.6 修订遗留挂账,fn 调用点已有 bound_sat,struct 注解点缺)。
**Scope:** `let p: Pair[I32, NoShow] = ...` 注解位的 TypeArgs bound 核对,复用 bound_sat
结构化谓词;负例 `fx_bound_ann_neg.ct`(E2050);正例扩展 fx_gstruct。
**Files:** compiler/src/sem_type.ct(Let 注解处理位,参照调用点 check_tpar_bounds 先例)、
compiler/test/fx_bound_ann_neg.ct(新)、smoke.sh(tc_fx 行)。
**Verify:** 负例 check 拦截 E2050;正例 emit 双面逐字;全量门禁。
**依赖:** 无。**规模:** 0.5 天。

### 切片 P0-B:调用点推断 C 线 neg 面

**Spec:** v0.7 修订三(E2060/E2061)——自举/R 线已落,C 宿主缺 neg 判定。
**Scope:** compiler-c 语义检查补 E2060(无法推断)/E2061(候选冲突)最小面;
suite 现有 04f_infer_* 用例双侧通过(当前宿主侧缺诊断,以「自举过/宿主未过」
能力缺口条目在册)。
**Files:** compiler-c/src/(语义检查 C 实现处,镜像自举 sem 的 infer 判定)、
compiler-rust 对应面核对。
**Verify:** suite 58/58 且能力缺口清单不再含推断条目;负例双侧拦截。
**依赖:** 需先读 compiler-c 的检查结构(侦察 0.5 天)。**规模:** 1-2 天。

### 切片 P0-C:S/D 域 const 穿发射

**Spec:** §3.3(String)/§8.4(const 编译期求值);现状 I/6/B 已穿,S/D 缺。
**Scope:** driver_emit const 折叠直出扩展——S:运行时字符串值经 C 转义辅助
(引号/反斜杠/换行/制表)emit 为 `static const char* t_X = "...";`;
D:df_can 规范文本核对(仅接受合法 C double 字面量形态,含 `-`/小数点;
指数形态先拒或转写)。fx_comp_ok 扩正例;不可表示形态走 E2010 或静默回退登记。
**Files:** compiler/src/driver_emit.ct(const 分支)、新增 C 转义助手(Ctron 写,
发射器自身函数,+1 decls 需同步锁)、compiler/test/fx_comp_ok.ct、fx_comp_type_neg.ct。
**Verify:** Str/F64 const 程序 emit 编译运行双面一致;全量门禁。
**依赖:** 需先核 D 域文本格式口径(df_can 输出形态盘点,0.5 天)。**规模:** 1 天。

---

## P1 发射面语义对齐(中等,方向既定)

### 切片 P1-A:发射面 Drop/RAII(§6.4)

**Spec 要求:** Drop 的 `fn drop(var self)` 确定性执行——作用域退出按声明逆序、
panic 展开保证执行;类引用不触发;Arena 整体释放。
**现状:** seed 解释侧已实现(run_block 逆序 drop);发射侧无 cleanup 路径
(E2071 的存在根据);panic 走 longjmp 不展开。
**设计要点(实施前 0.5 天设计记录):**
- 作用域退出:含 Drop 的 let 在其作用域收尾处内联 `ctron_drop_T(&t_x);` 调用;
  多退出点(return/break/continue/panic)需要 cleanup 区块或 setjmp 复用。
- panic 展开:ct_panic 路径在 longjmp 前执行当前 shim/fn 已登记的 drop 列表
  (任务结构持栈式登记,或编译期静态链)。
- break/continue 越过 Drop 作用域:现 E2071 静态拒绝——若 cleanup 内联可行,
  可按 spec 明文解除(纯增量)。
**Verify:** fx_drop 夹具(正常退出/早退 return/panic 展开/嵌套作用域逆序)
双面逐字;R 线同步。
**规模:** 3-5 天(发射器语义最深的一块,建议独立会话)。

### 切片 P1-B:ISize/USize/U64 定宽存储(§3.1)

**Spec 要求:** U64/ISize/USize 与目标指针同宽(64 位);当前发射类型擦除 int32,
U64 值截断(已登记)。
**设计要点:** 64 位存储槽(U64/ISize/USize → int64_t,独立于 "6" 的语义——
定宽回绕 vs 6 域不回绕);检查算术边界 2^64(无符号);as 转义链全宽度矩阵;
字面量直入宽度槽(解除 I32 入口限制的最后一段)。
**依赖警示:** 与「I32 发射面检查被自举传播阻断」同族——发射检查会进编译器自身,
定宽槽落地后 txt_num/dvi 等内部回绕点需一并迁移(或引入内部专用无检查路径,
需设计记录)。**Verify:** 03b 宽度全集从「宿主通过」转双侧绿;新增 U64 算术夹具。
**规模:** 3-5 天。

### 切片 P1-C:Simd 运算面盘点与口径(§9.5)

**Scope:** 盘点 eval/trans 的 Simd 现状(前奏 splat/lane/to_array 已钉);
决策:标量模拟口径成文(spec 允许实现口径)或补齐向量发射。
**Verify:** Simd 夹具双面逐字。**规模:** 盘点 0.5 天,补齐视决策 1-3 天。

---

## P2 需设计 spike(先出设计记录再排期)

### 切片 P2-A:comptime 完整形态(§8.4)

现状:单表达式体 comptime fn + If 值位 + const 折叠(语句/循环未做)。
**Spike 问题:** ceval 扩展语句/循环的步数语义;comptime 值容器(List/Str 构造
在编译期堆模型);预算口径(1s/编译单元如何映射到 1200 步)。
**产出:** 设计记录 → ceval 扩展切片。

### 切片 P2-B:Ctron.toml 解析与包元数据语义(§2.7)

**Spike 问题:** 元数据哪些键进语言语义(包名/版本/依赖)?解析器用 Ctron 写
(自举一致性)还是宿主?依赖解析与 §2.6 循环检测的关系。
**产出:** 设计记录 → 解析切片(+诊断码)。

### 切片 P2-C:构建配置系统(§4.5 release 开关/§5.4 采集关闭/§8.5 预算清单)

**Spike 问题:** 配置来源(命令行/文件/内嵌注解)、与 profile 的关系、
发射面检查开关的实现位(发射器全局标志,自举传播再评估)。
**产出:** 设计记录 → 配置系统切片。此切片解锁:I32 检查发射的可行路径之一。

### 切片 P2-D:精确诊断 §10.2(span 行列/notes/fixes)

**Spike 问题:** parser span 从「行号尾槽」升级为行列结构(节点槽位扩展,
编译器自身 AST 兼容);fixes 编辑模型(发射/应用面)。
**产出:** 设计记录 → span 切片(建议与 LSP 合并立项)。

---

## P3 跨线/远期(spec 本身标注远期,不在本轮)

- 宿主 i64 parity(compiler-c 的 i64 为 W 域模型,与自举 6 域对齐需宿主重构)
- §9 后端矩阵(WasmGC/JSPI/Cranelift/LLVM/裸机)
- LSP 完整(hover/跳转/补全,P5)
- R 线逐切片同步(上述语言级切片各自内含)

---

## 执行顺序建议

```
P0-A(0.5d) → P0-C(1d) → P0-B(1-2d)   ← 一周内可全部落地
P1-A(3-5d,独立会话) → P1-B(3-5d) → P1-C(0.5-3d)
P2 各 spike 穿插在门禁等待期
```

## 自查记录

- 规范覆盖:§2.5/§3.5/§7.3 已实现(核实撤案);§2.7/§3.1/§3.9/§4.5/§5.4/§6.4/
  §6.5/§8.4/§9.5/§10.2 各有条目;§9 后端矩阵属远期标注 ✓。
- 类型一致性:切片间共享机制(#clcodes/预扫/bound_sat/ct_targ_env)均已在
  main 落地,本计划只消费 ✓。
- 占位符:P0 全任务级;P1-A/B 含设计要点与验收;P2 以设计 spike 为交付物
  (设计 spike 本身即任务产出,非占位符)✓。
