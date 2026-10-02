# GUI 交互缝四件(S1-S4)——事件面应用声明补齐

> 日期:2026-10-02。泳道:gui。前置:文本编辑面战役收官(候选窗/快捷键/连击/
> 右键菜单/撤销重做/滚动/键重复全落库)。本片=规格 §2.1/§2.2/§2.4/§2.11 P1
> 能力缝中「引擎已在、只差暴露」的密度最高一片。

## 目标

应用能声明订阅的事件面补齐四缝,全部走既有架构(事件表/bxv 受限求值/act
通道),零新运行时概念:

- **S1 `on:submit`**:input 回车提交(镜像 on:input 的隐式尾参契约);
- **S2 `on:dblclick`**:元素双击事件(内部连击引擎已有,应用声明面补齐;
  双击命中带 dblclick 事件的元素时只发 dblclick 不发 click);
- **S3 `hover:名`**:悬停态 bind 暴露(§2.4 显式扩展点;`hover:ho` 裸属性,
  `when={ho}` 消费 "1"/"0",tooltip 类组件地基);
- **S4 `mods` 事件实参内建名**:`{sv(m, mods)}` 直取当前事件修饰键位
  (bit1=ctrl/⌘ bit2=shift bit4=alt;零语法变更,arity 门自然适配)。

## 设计要点

- **S1**:事件表已泛化捕获任意 `on:名`(域包/编译器同构)——只补分发与门:
  域包 rt_input_key input 分支 k==257 时查节点 `submit` 事件,简单名走
  `act("名:"+值)`(镜像 on:input 名通道),调用形走 ev_fire;编译器 E8120
  隐式尾参门扩 `evn=="submit" && tag=="input"`(imp=1)。
- **S2**:evt2 命中解析后,click_kind==1 且命中节点事件表含 `dblclick`
  →改发 dblclick 表达式(ev_fire),跳过 click;headless 驱动 d_dblclick
  (两次同帧注入 click,kind 聚类=1 走同一路径)。
- **S3**:零 schema 方案——`hover:名` 裸属性入事件表(ev_name="hover",
  ev_fn=名);渲染期扫描节点 hover 事件,注册 C 名值表(帧戳失效);
  bind 询问中央助手先查悬停表再落应用 bind(悬停值属运行时,跨帧一帧滞后
  可接受);零 GuiTree 字段变更(编译器 sk 镜像零扰动)。
- **S4**:bxv 名解析回退链挂 `mods` 内建(打标 "i:N");props 视图实参根门
  会拒 `mods` 根(props 视图暂不可用,登记限制)。

## 明确不做(本片)

- 修饰键载荷的「进 act 实参表」形态(S4 的内建名即本期口径;实参表形态
  破坏既有 args 消费者,需另行裁决);
- tooltip/toast 等**组件**(消费 S3/S4 的地基另立组件片);
- props 视图内 mods/hover 的门放宽(登记限制先行)。

## 切片序与验收

每片:域包(+编译器门)→ s52_event_seams 夹具判别段 → 阶梯 → 落库。

- S1 验收:input on:submit,send 257 → 模型接收值;textarea 回车仍换行不触发。
- S2 验收:button on:click+on:dblclick;单击计 click,双击计 dblclick 且
  click 不重复计;d_dblclick 驱动。
- S3 验收:button hover:ho + label when={ho}:d_hover(注入悬停)→ label 显
  "1",移出显 "0"。
- S4 验收:on:click={sv(m, mods)} → 模型收到位修饰键位(d_mod 注入后 1,
  无修饰 0)。

## 风险与坑位预案

- 编译器 gui_parse.ct 为双泳道共编:动前核 git 状态,逐 hunk 归属;
- 事件表路径通用,新事件名不触 E8100 白名单(已有 input 先例),但 S3 裸
  属性解析需确认编译器对未知裸属性的处理面(静默跳过则零改动,拒绝则补
  hover: 认领);
- 双份渲染教训:新增每帧注册(hover 表)须确认挂点唯一性。
