# GUI 自动装配双机制仲裁简报(2026-09-26)

> **裁决(2026-09-26,用户「继续」确认推荐项):A 为正统。** desugar 为唯一
> 自动装配机制持续迭代;B(emit 期合成)归档于分支 `sl8c4b`(004115a→a06fec9,
> 含 E2E/s42/e84 全套验证资产),不并入 main;其独立价值(read_file NULL 修复
> 688f65e、四条发现、门约定实证)已收割入 main/记忆。若 A 日后需要 emit 直出
> 能力,B 可从归档复活。

> 背景:SL-8c-4b 期间形成两条独立的「自动装配」实现,同一目标(bind/act 钩子
> 由编译器生成,用户零手写)、不同机制、均已实证。本简报供裁决正统与归并节奏。

## 机制 A:parse 期 desugar(机刷泳道,已在 main,阶梯 52/0)

- **形态**:`gui.run(Comp)` 组件形态;parse 期文本注入合成 `__gui_bind` /
  `__gui_act` / `__gui_run` 三 fn(check 面对合成源全量语义核对 = 合成正确性活证)。
- **已交付**:props 重写(gui_ds_rw)、each 体内事件隐式实例下标(ievs 收集 +
  "头:N → 头(__p_*, N)" 后缀分支,ev_suffix_i)、on:input 隐式尾参 text、
  s41_run_d 域包夹具(三层验收:合成面/直驱面/真窗面);todo_v10 逐行删除/勾选行销账。
- **特征**:合成源再经编译管线(文本级);与 §10 终锚组件形态(§3 组件元素)同构。

## 机制 B:emit 期闭包合成(worktree 分支 sl8c4b,004115a+86dea75+a06fec9)

- **形态**:`run_d(title, w, h, props)` / `test(src, w, h, props, script)` 四/五参形;
  trans_expr 特例拦截,driver_emit 直出装配闭包(ct_clo 语句表达式,capture = props)。
- **已交付**:Box/值 props 双形态、数据实参按位解码(ev_arg_i/b/s)、语义门
  (walk_e 挂载:单视图单 prop + 字面量型对位,E8100.auto/E8110.auto)、
  s42_auto_d 域包夹具(headless 全链)、e84 语料三件、真窗 E2E(预注入点击)。
- **特征**:零中间源码,发射期直出,与既有 ct_clo ABI 同构;each/on:input 未接
  (ievs 约定可对齐);特例散布 trans_expr/driver_emit 两处。

## 对照

| 维度 | A:desugar(main) | B:emit 合成(sl8c4b) |
|---|---|---|
| 调用面 | gui.run(Comp)——终锚形态 | run_d/test 四/五参——渐进形态 |
| 合成时机与介质 | parse 期,源码文本 | 发射期,C 闭包直出 |
| each 行事件 | ✅(ievs + ev_suffix_i) | ❌(登记,约定已对齐) |
| on:input 尾参 | ✅ | ❌ |
| props 型宽 | 值 + 装箱(__p_* 契约) | 值 + Box(实证) |
| 实数据实参解码 | gui_ds_rw 重写 | ev_arg_* 按位 |
| 验收 | s41 三层 + 阶梯 52/0 | 探针 E2E + s42 + e84 |

## 裁决选项

1. **A 为正统**(推荐倾向):与终锚组件形态同构,机刷持续迭代中;B 分支归档,
   其独立价值已先行收割(read_file NULL 修复 688f65e 已入 main;四条发射/检查面
   发现入册)。
2. **B 为正统**:desugar 冻结;B 补 each/on:input 对齐 ievs 约定后接管。
3. **共存**:A 主攻 gui.run(Comp) 终锚;B 四参形作为轻量渐进口——两套生成器
   长期并存,维护成本需认。

## 附:本片独立价值(无论裁决如何均已收割)

- read_file 缺文件 NULL → 空串(688f65e,run_d 兜底 SEGV 根治,main 阶梯 52/0)。
- 四条发现:check 驱动单文件口径 / emit 不走 walk 门 / diag_render 纯数字键踩界 /
  Str 字面量+引用形参 lit_fit 脆弱点。
- 每行实例分发的门约定实证(each 内 da = argc+1 隐式尾参,与 A 落地一致)。
