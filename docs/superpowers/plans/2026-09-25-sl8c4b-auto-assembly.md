# SL-8c-4b:auto-act/auto-bind 装配闭包发射 + gui.run 单入口 + 钩子退役 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用户装配从「手写 bind/act 两个钩子闭包」退役为「`run_d(title, w, h, model)` 一行」——编译器在 run_d 调用点特例发射装配闭包(auto-act 按事件表分派、auto-bind 按 props 类型应答),应用代码只留业务。

**Architecture:** 零新语言面、零域包新入口:trans_expr 对 `callee=run_d && argc=4` 特例化(ctron_embedded/gui_sk_load 同款先例),driver_emit 的 has_gui 区发射合成闭包 shim(trans_conc 站点 shim 同配方),捕获 = 调用点第 4 实参(view props 值)。事件表 × props 类型表(gui_props_typed,8c-4 地基已落)为生成依据;4a 的 act v2 契约(fn(Str, List[Str]) + ev_arg_* 解码)为装配目标。check 面加四参形 props 类型门。eval 侧装配随 P1b(在册,不阻塞)。

**Tech Stack:** compiler/src/{trans_expr,driver_emit,gui_parse}.ct(发射面);gui.ct 零改动或仅文档面;s41_auto_d 新夹具 + gui_counter 迁移示范;worktree 执行法。

## Global Constraints

- **执行隔离**:.worktrees/<分支> worktree(机刷 gui.ct 常态在飞);compiler/bin、vendor/gui/build 软链主仓;`git add -A` 会误吞软链(忽略规则带斜杠不匹配符号链接)——逐文件 add。
- **design spike 先行(Task 1)**:合成闭包发射是本片唯一无先例机制,探针不绿不动真格生成器。
- decl 锁:driver_emit 增量 → 自举 decl 锁实测校准(先例 363→366→367)。
- 双口径铁律:发射面改动必须 ctron-cc 与 native 双臂验证;域包夹具解释口径红(P1b 在册)不新增即守约。
- 号段:s41 起(s40=8c-4a);示例迁移选 gui_counter(todo_v10 归机刷在飞,勿碰)。
- 提交 pathspec 限定;编译器源码字符串裸 `{` 须 `\{`;禁 `;`;无三元(J20)。
- E8120/E8110 语义不动(auto 形态复用既有门;argc=声明形参个数)。

## 核心设计裁决(D-4b)

1. **调用面**:用户写 `run_d(title, w, h, model)`(四参形)。第 4 实参 = view props 按位传值(单 prop 视图即模型;多 prop 依次)。trans_expr 特例:展开为既有六参 `run_d(title, w, h, 生成bind, 生成act)`。
2. **auto-act 约定(恢复 `{addn(m, 5)}` 全形态)**:事件表达式实参里,根为 prop 名的参数由生成闭包从捕获的 props 取值直传;其余参数按位 `ev_arg_i/b/s(args, k)` 解码(k = 该参数在实参表的位次)。E8120 口径不变(实参个数 = 声明形参个数)。4a 的 `{addn(5)}`+`_impl` 形继续合法(手写 act 应用);auto 形态下 handler 直接就是真函数,`_impl` 转发层可删。
3. **auto-bind 约定**:骨架 bind 槽路径(nbid,如 `m.count`)按根查 props:命中则按 prop 类型发值(I32→`i:`/Bool→`b:`/Str→裸),未命中路径回落零值应答(与现 bind 通道缺答语义一致);props 视图零手写。
4. **单入口**:run_d 四参形为唯一文档面;run/run_kb/run_kb_d 与六参 run_d 降内部(rt_ 化,不删符号,迁移渐进);test/test_sk 保留(测试口径,钩子仍手写)。
5. **eval 侧**:P1b 收口前以 test_sk 直通(蓝本 §6 既有裁决),本片不做。

---

### Task 1: design spike——driver_emit 合成闭包探针

**Files:**
- Modify(worktree 临时,不落库): `compiler/src/driver_emit.ct`(has_gui 区直出一个固定 shim)
- Modify(worktree 临时): `examples/gui_counter/src/main.ct`(临时切四参形走探针路径)

**Interfaces:**
- Consumes: trans_conc 闭包 shim 配方(env 数组 + ct_clo_L<行> 命名);域包 run_d 六参签名
- Produces: 合成闭包发射配方三件套(typedef/shim/闭包值组装)的可行形态,回填本计划 Task 3/4

- [ ] **Step 1: worktree 建立与软链**(.worktrees/sl8c4b,compiler/bin、vendor/gui/build 软链)
- [ ] **Step 2: driver_emit has_gui 区临时直出探针 shim**(固定体:向 stderr 打一名,证明发射+链接+回调通)
- [ ] **Step 3: gui_counter 临时改四参 run_d,trans_expr 加临时特例走探针闭包**
- [ ] **Step 4: 双口径验证**(ctron-cc 臂红=P1b 在册可受;native 臂须 rc=0 + 探针打印可见)
- [ ] **Step 5: 探针结论回填本计划(Task 3/4 的发射文本形),临时改动不出 worktree**

### Task 2: check 面——四参 run_d props 类型门

**Files:**
- Modify: `compiler/src/gui_parse.ct`(或 driver_check 接线处;gui_props_typed 已在)

**Interfaces:**
- Produces: `run_d` 四参调用 → 实参 4 类型 vs view 声明 props 类型(gui_props_typed)逐一核对,不符 E8120(新子码或复用);非 props 视图(无 props)四参形 = E8100

- [ ] **Step 1: e8 语料 pos/neg 各一件**(props 类型对/错)
- [ ] **Step 2: 门实现 + runner 全绿**
- [ ] **Step 3: 提交(check 面,独立可落)**

### Task 3: auto-act 生成器(driver_emit)

**Files:**
- Modify: `compiler/src/driver_emit.ct`(has_gui 区;ev 表/props 类型已可用)
- Modify: `compiler/src/trans_expr.ct`(四参特例正式化,替换 Task 1 临时探针)

**Interfaces:**
- Consumes: Task 1 配方;骨架 ev_fn 表(头 + 表达式原文);gui_props_typed;4a v2 契约
- Produces: 生成 act:头匹配分派 → handler(prop 根参取捕获,余参 ev_arg_i/b/s 按位解码);未匹配头 = 静默弃(与手写 act 同语义)

- [ ] **Step 1: 生成器主体**(事件表遍历 × gui_ev_head2 形态复用:prop 根参数映射/数据参解码)
- [ ] **Step 2: s41_auto_d 夹具(先红)**:props 视图 + `{addn(m, 5)}`/`{setadj(m, 3, -1)}` 全形态,断言模型变更与 4a s40 等价判据
- [ ] **Step 3: native 绿 + 阶梯零新增红**
- [ ] **Step 4: 提交**

### Task 4: auto-bind 生成器(driver_emit)

**Files:**
- Modify: `compiler/src/driver_emit.ct`

**Interfaces:**
- Consumes: 骨架 bind 槽路径表(nbid);props 类型表
- Produces: 生成 bind:路径根查 props → 按类型发 `i:`/`b:`/裸值;未命中应答空(现通道语义)

- [ ] **Step 1: 生成器主体**(字段读 = emit 侧既有用户 struct 字段访问机制)
- [ ] **Step 2: s41 扩断言(活模型逐帧回显,input mirror 路径)**
- [ ] **Step 3: 双口径回归 + 提交**

### Task 5: 迁移 + 单入口收口 + 登记

- [ ] **Step 1: gui_counter 切四参形**(删 bind/act lambda 与 dispatch;业务 fn 不动)= 迁移配方样板
- [ ] **Step 2: run/run_kb/run_kb_d/六参 run_d 降内部更名 rt_*(符号保留)**,全仓消费迁移,阶梯+五示例全绿
- [ ] **Step 3: decl 锁校准实测**
- [ ] **Step 4: 登记三件**(路线图 8c-4b 落库段 / 记忆 / 蓝本 §4 对勾);todo_v10 终锚登记待机刷收口后补切

## 后续(不在本片)

- eval 侧装配(P1b 后);J18 ctcl gui.entry(吞 title/w/h,四参形再缩一行);多 prop 视图/组件嵌套(§3 组件元素)随终锚。
