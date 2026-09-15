# P2-A comptime 语句/循环 设计记录 + 实施

> 上游:P2-A spike(2026-09-13 规格化);本文件 = 设计记录 + 实施切片合一。

## 设计决策

**范围:** ceval(步数预算求值器,E6010)从"单表达式体 + If 值位"扩展为支持
comptime fn 体内的 **Let / Assign(Eq)/ While / For(range)** 语句;值域仍限
I 域整数(容器/浮点 → 静默回退全量求值,与既有不支持形态同口径)。

**步数语义:**
- 每次条件求值、每条语句求值各计 1 步(ce_budget 既有 1200 步池,不新增预算口径);
- 循环体每迭代经条件门 + 体语句自然计步——无限循环必然耗尽预算 → E6010
  (spec 本义),无挂起风险;
- 超预算 → "over" 向上传播 → E6010;不支持形态(Break/Continue、非 Eq 复合赋值、
  非 I 值、For 非Range)→ "u" 静默回退全量求值(既有口径)。

**环境模型:** cenvN/cenvV 平行表(List 引用语义),Let push(同名遮蔽靠"取最后
出现"),Assign 取最后出现下标覆写;循环体每迭代的 Let 会累积条目——预算上限
天然约束(≤1200 条)。While 体 BlockExpr 解包;For 限 Range(含 ..=)。

**fn 调用:** 既有路径不变(参数 I 域绑定 → ceval_block(body));语句扩展后
多语句 comptime fn 体自然可达。

**发射/运行联动:** const 折叠(driver_emit ceval 直出)自动获得语句/循环能力;
解释侧 statics_env 全量求值本就支持 → 双面语义由 ceval 覆盖面决定,夹具按
"解释=发射"逐字验收。

## 实施

- `src/sem_ceval.ct` ceval_block:补 Let/Assign/While/For 四分支(上文语义);
- 夹具 `test/fx_comp_stmt.ct`:wsum(while 累加)+ fac(for 乘积),const 折叠
  三方(宿主/解释/发射)逐字;
- 门禁:smoke --full / suite / 固定点 / decls 锁同步。
