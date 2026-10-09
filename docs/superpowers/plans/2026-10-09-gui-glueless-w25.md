# GUI 无胶水装配 W2.5 实施计划(默认值/opt + ev 契约 + 前置计算块)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> 规格 = docs/superpowers/specs/2026-10-09-gui-glueless-assembly-design.md §5.2/§5.3/§5.4(裁 6 + 补 b/补 c)。
> 前序:W1a/W1b(6a77c1ba/7aa0d28e)、W2(86478cc0)已落库。重建口诀:改 compiler/src 后
> `rm -f compiler/build/cc_*.ct && sh compiler/build.sh && sh compiler/compiler/native.sh`
> (实际路径 cd compiler;seed 解释 build/cc_emit.ct 失败时看 E1001 `\}` 族)。

**Goal:** 组件签名行自描述合约——prop 默认值、事件 opt、ev 声明 + on: 统一接线、视图前置计算块;Dialog 三路合约迁移实证。

**Architecture:** 签名行文法在 d[5] props 串的解析层扩展(gui_props_typed 剥默认值/opt 记表);默认值走实例展开的垫机制(vreg_ps/vc_prop,7963ef04 先例),opt 缺接线 = 合成 no-op 臂;ev 子句 = 签名行后缀解析 + 组件体白名单门 + 实例 on:名 接线记录入合成臂;前置块 = 根标签前 var 链解析(desugar 合成 __gui_prep_V + 运行时 gt_parse 同步学,「内嵌/独立同语法同路径」宪章)。

**Tech Stack:** 同 W1/W2。

## Global Constraints

- 静态树四基石;`d_click` 返回名交 actc;夹具槽执行前实测(s95/s96/s97 空闲已核);pathspec 限定;driver_emit/trans_conc/trans_expr 对端在飞禁触;双端同形(s22 差分站岗)。
- 三件各自独立落库(每件全绿一批),顺序:a 默认值/opt → b ev+on: → c 前置块。

---

### W2.5a:prop 默认值 + 事件 opt

**落点勘察(已核):**
- 签名串:d[5] = `"( title: Str = \"分组\", rows: List[Str] )"`——gui_props_typed(compiler gui_parse.ct:1718 前后)按 `名: 型` 拆,须剥 `= 常量` 段记 dflt 表。
- 垫机制:运行时展开 vc_prop/vreg_ps(pkgs/gui/gui_parse.ct:1193-1230)——实例未提供的 prop 展开期垫空;默认值 = 垫常量(0x01 哨兵直取形,同字面串)。
- 检查面:gui_ck_elem 实例支「缺失=E8100」须豁免有默认 prop(对照 dflt 表)。
- opt 事件:W2.5b 的 ev 落点,dflt 同批在签名解析侧留表(事件表本批不动,免半面)。

**Tasks:**
- [ ] T1 s95_attreq 扩展:TagRow 加 `title: Str = "分组"`,实例一省略一覆盖,断言两渲染;失败态先行
- [ ] T2 gui_props_typed 剥默认值段(名/型/默认三平行表),双端同形(runtime gt_props_names 镜像)
- [ ] T3 垫机制喂默认(运行时展开 + 内嵌 sk 面同构);检查面豁免
- [ ] T4 e8 负例(默认类型不符);全门;提交

### W2.5b:ev 声明 + on: 统一接线(组件合约)

**落点勘察(已核):**
- 签名行:`view TagRow (…) ev pick(row: I32), refresh() opt`——gui_block 捕获 view 名后扫描 `ev ` 子句(d[5] 侧或新槽 d[7],对齐 GUI-27 的 d[7]/d[8] 槽惯例)。
- 组件体白名单门:gui_ck_elem 组件视图走查时,`{名(…)}` 引用 ∈ 数据 prop ∪ 声明事件,否则 E8123。
- 实例接线:实例节点 `on:名={handler(m)}` ——gt_node/gui_sk_node 实例 attr 循环放行(现被 prop 循环吞),接线记录 = (事件名, 靶 fn, 捕获表达式);合成 act 臂按「事件名+实例序」分发(现 cvf 臂的头名分发升级)。
- E 码:漏必接 = E8121 复用(已有);接线签名不符 = E8122(本件落地);未声明事件接线 = E8124。
- Dialog 迁移:pkgs/gui_widgets Select/WList/Dialog 事件头改 ev 声明,gui_snippets 实例点显式 on: 接线(实证件)。

**Tasks:**
- [ ] T5 签名 ev 子句解析(编译 d[7] + 运行时 gt 镜像)+ 双端差分
- [ ] T6 实例 on: 接线解析(两解析面)+ 合成臂按接线记录生成
- [ ] T7 白名单门 + E8122/E8124 + e8 负例三件
- [ ] T8 Dialog 三路迁移(gui_widgets + gui_snippets + s50 回归)
- [ ] T9 全门;提交

### W2.5c:前置计算块

**落点勘察(已核):**
- 解析:根标签前 var 行(gui_block/gui_sk_node/gt_parse 三面;var 链以行语句形态,`<` 起树)。
- 合成:`__gui_prep_V(props...)` 每帧帧首求值,结果经 bind 通道 `pre:名` 应答;树侧 `{名}` 编译为通道查询。
- 禁面:赋值/循环/spawn → E 码;树形静态红线(不收按值换树)。
- 双端:gt_parse 同步学 var 行(同语法同路径宪章,s22 差分站岗)。

**Tasks:**
- [ ] T10 三面解析(var 链捕获 → prep 源合成)+ s96 夹具(派生值三消费 + if 表达式)
- [ ] T11 副作用禁面 E 码 + e8 负例
- [ ] T12 全门;提交;W2.5 收官记忆

## Self-Review

- 覆盖:规格 §5.2(ev/on:/白名单/E8122/E8124)→ W2.5b;§5.3(前置块/prep/禁面)→ W2.5c;§5.4(默认值/opt;opt 随 b 的 ev 表落地,a 先做 prop 侧)→ W2.5a。规格 §5.2 名字形退役=裁 3,随 b 的迁移一并清。
- 无占位:勘察行号已核;合成臂/垫机制有先例指向。风险=①垫机制双端形态差(runtime vc_prop vs 内嵌 sk 面)②ev 子句与既有 props 串解析的兼容(d[5] 文本含 ev 段的旧夹具零波及须验证)。
