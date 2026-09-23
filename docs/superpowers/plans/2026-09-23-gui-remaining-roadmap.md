# GUI 规范未完成部分路线图(2026-09-23)

> 承接完成度评估,为剩余面定序。原则:v10 终锚依赖优先、可立即实施者先行、
> 阻塞面保持登记不空转。

## 状态基线(本日实测)

- 已解锁:s26 自愈(run_kb_d 段错误随 peer 落库消失)、gui_calc 绿、
  trans_expr/stmt/ty 已落库(仅 trans_emit.ct 在飞)。
- 仍阻塞:8c-2/3/4(interp 半 = P1b 跨包 stub「index target」;native 半 =
  trans_emit.ct 在飞)。蓝本:plans/2026-09-23-sl8c-design.md。

## 执行队列

### P-W1 checkbox 元素(本日开工;v10 依赖)
`<checkbox checked={done} class="ck" on:click={toggle}/>`:
- 解析:三面白名单 + 自闭合(镜像 input);checked 必填(E8100);登记 btns
  (点击走既有 rt_hit_name→act 名字分发,实例索引缺口维持登记)。
- 渲染:选中 = accent 底 + "x" 文本,未选 = 暗底无文本(ASCII 字形安全)。
- 验收:e8 pos + 原生探针(点击→状态翻转→重渲)。

### P-W2 style extends(§5 StyleExt;本日次件)
`style b extends a { … }`:解析认领 + 样式表构建期单亲合并(子覆盖父);
循环继承 E8100。验收:e8 pos/neg + 现有样式夹具回归。

### P-8c-2/3/4(阻塞;解除即按蓝本动工)
props 值流(props 环境)/on: 表达式事件闭包/gui.run 单入口+钩子退役。
解除条件:trans_emit.ct 落库(native 半)+ P1b interp 收口(解释半)。
解除检查:`git status | grep trans_emit` 为空 + interp 探针过。

### P-v10 终锚装配(依赖 8c)
§10.3 Todo 照抄能跑:props 视图 + 表达式事件 + `disabled={}`(需按钮
禁用态与 RECT 映射契约的每帧化,或 props 环境直查)+ `key={}`(实例分发,
L1 缺口对偶面)+ checkbox 行级勾选(P-W1 交付)。

### P-M3 中文输入(独立大山头,不阻塞于 8c)
IME 组词 preedit 一等状态(§6.4);平台 shim + 字形栈协作;焦点/IME 不丢
为 §6.3 硬验收(P3 遗留面一并兑现)。

### P-M4 远期
`ctron build --release`(<2MB 静态单二进制)、多窗口、动画 tween、
无障碍树、CTML→web 档(§11.8 登记)。

## 明确不做(维持登记)
E8170 子回写(组件模型落地后才非空)、求值缓存/脏追踪(D5 裁决每帧全量)、
字面量叶标点根治(骨架 IR 槽表直通,远期)。
