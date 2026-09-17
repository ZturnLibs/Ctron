# 布局原因链(provenance)与布局静态验证——设计记录

> 状态:**设计冻结**(2026-09-17 设计会话裁决)。上游 = 2026-09-16 GUI 规格
> (`2026-09-16-gui-ctml-design.md`)§13.2"创新空间"三项的深化落地。
> 实现未排期:prov-v0 插入 MVP 阶梯(`2026-09-16-gui-mvp-ladder.md`)S5/S6 之间;
> inspector 维持 M2;静态验证随第二波 W3。排期前登记 `spec-completion-roadmap`
> GUI 泳道行。

## 0. 本文回答什么

2026-09-16 规格 §13.2 把布局创新空间定位在三处:布局原因链(provenance)、
确定性可快照、comptime 预布局。本文把前两者落成可实现的设计(数据模型、
记录层、呈现层),并把第三项**收窄为布局静态验证**。同场裁决:inspector
面板形态 = 停靠 + 浮动**双模式可切换**。

## 1. 目标与非目标

### 1.1 目标

1. **"它为什么在这/为什么这么宽"一键可答**:每元素每轴给出 rule + inputs + result;
2. **输入可溯源**:原因链每项可跳回 CTML 源(file:line + 属性名);
3. **确定性自检**:推导结果与 Clay 实际几何逐帧断言相等,推导逻辑漂移当场报警;
4. **release 零开销**:provenance 与 inspector 全部 dev 口径产物,release 编译剔除。

### 1.2 非目标(明示不做)

- **不改布局算法**:Clay flex 子集维持 §13.2 冻结,本文纯增观测与验证层;
- **不做绝对几何的编译期预演**(裁决 3,§2);
- **inspector 不做面板内编辑**:改样式走热重载,改代码即改外观是既有心智;
- **不解冻公共浮层语义**:浮动检视卡走 dev-only 内部浮层(§6.3),
  `dialog`/`tooltip` 等 popup 类组件维持 T2 推迟裁决(§13.1)。

## 2. 裁决记录

| # | 议题 | 裁决 | 落选案(理由) |
|---|---|---|---|
| 1 | provenance 记录层 | **A:shim 后置推导**——布局结束后由 shim 依据(自身声明的每元素配置 + 最终几何 + 自有文本测量值)反推原因链;零 Clay 补丁,升级零负担 | B:读 Clay v0.14 内部中间数组(忠实但内部字段升级是隐性破坏点,黄金快照测不出内部读取断裂);C:Ctron 侧第二解释器复算(同一 flex 算法双真源,违反单一真源原则,直接否决) |
| 2 | 推导等价性 | **自检断言**:推导 result 逐帧等于命令缓冲实际几何,不等 = dev panic | 人工保持与 Clay 内部逻辑对齐(黄金快照只测输出,测不出推导漂移;自检可以) |
| 3 | comptime 预布局 | **收窄为布局静态验证**(§7):绝对几何预演不可行——文本测量依赖运行时字体(FreeType 与 S3 启发式数值不同);fixed 子树溢出与死 fill 的编译期判定价值更实,且业界 flex 做不到 | 维持绝对几何预演(收益受字体度量不确定限制,不实) |
| 4 | inspector 形态 | **双模式可切换**:停靠(默认;内部「紧凑/对照」两档密度,即草稿 A/C 合并)+ 浮动检视卡;工具条/快捷键切换,选择存运行时本地状态(会话内有效,不进 model) | 三种顶层形态独立实现(A/C 实为停靠区内部密度差,做成两种顶层形态是重复机制);先只做停靠(丢失"点谁看谁"核心体验) |

## 3. 数据模型(dev-only,规范性)

```ct
// 仅开发口径编译;release 二进制整体剔除,gui 运行时体积目标(<1MB)不受影响
class ProvSrc   { var file: Str; var line: I32; var attr: Str }        // CTML 源引用
class ProvInput { var src: ProvSrc?; var kind: Str; var value: I32 }   // kind: text.measure / parent.content / sibling / gap / padding
class ProvReason{ var axis: Axis; var rule: ProvRule; var inputs: List[ProvInput]; var result: I32 }
```

- 每元素每帧至多 4 条(W/H/X/Y),按 retained 树节点 id 索引;
- 存储走**帧级 scratch**(dev 口径帧界重置,与 §6.5 分配器口径一致);
  主线程私有(§6.2 线程模型——布局本就在主线程);
- `ProvSrc` 由 gui_parse 阶段记录:运行时口径 = 解析位置;comptime 口径 = 源码
  span;样式类展开后可定位到**具体属性行**(用户类 → pub style 定义行)。

## 4. 推导规则(规范性)

**W/H 轴**(rule → inputs):

| rule | inputs | 说明 |
|---|---|---|
| `fixed` | 样式属性值(带源引用) | 声明即终值 |
| `hug_text` | text.measure(×n) + gap(n−1) + padding | 文本子元素,测量值 shim 自有 |
| `hug_children` | max(子终宽/高) + gap + padding | 容器 hug,取子最终几何 |
| `grow` | parent.content − 兄弟 fixed 总和 − gap 总和;自身权重 | 份额分配 |

**X/Y 轴**:`line_start`(行首)/ `prev_sibling`(前兄弟终值 + gap)/
`align_offset`(父对齐偏移)/ `wrap_newline` / `scroll_offset`。

- padding 与 gap 一律进 `inputs`,不单独立 rule;
- **嵌套可递归展开**:inspector 中点"text.measure=28"→ 显示"font-size:20 ×
  测量启发式/字体度量",再跳到该 font-size 所在样式行——原因链是树,不是行;
- **自检断言(裁决 2)**:推导 result 必须等于命令缓冲实际几何,不等 = dev
  panic。该断言使推导逻辑与 Clay 求解器的等价性成为逐帧受控项,而非一次性
  对齐承诺。

## 5. 记录层接口(prov-v0,随 S5.5)

- shim 在 `gui_open/gui_close` 之间维护**影子树**(声明配置已知),布局结束后
  置推导(方案 A);
- 新增窄接口(全标量/Str,沿用 S3 shim 边界纪律):
  - `gui_prov_enable()`——dev-only 开关;
  - `gui_prov_dump(node)`——headless 文本 dump;
- 解释口径下两函数进 §11.5 dev 白名单(白名单纪律不变);
- **先做调试显微镜,不接 UI**:prov-v0 的第一个消费者是 S5/S6 的黄金差分
  调试——IR/几何不符时,dump 直接给出每轴原因,替代盲猜。

## 6. Inspector(M2)

### 6.1 双模式(裁决 4)

- **停靠模式(默认)**:窗口右侧常驻 inspector 区,内部提供「紧凑(树上链下
  堆叠)/ 对照(树与详情分栏)」两档密度切换——纯 flex 分栏,**零新机制**;
- **浮动模式**:点选元素旁浮出检视卡,只显示当前选中元素的紧凑原因链 + 源码行;
- 切换:inspector 工具条 + 快捷键;选择记运行时本地状态(§4.3 契约 5),
  会话内有效。

### 6.2 面板三区(两模式共用数据面)

盒模型(位置/尺寸/padding/Gap)/ 每轴原因链(递归展开,§4)/ 源码跳转
(`ProvSrc` → LSP 定义跳转同通道)。

### 6.3 浮层边界(dev-only 内部浮层)

浮动卡的层级由 **inspector 状态机私有机制**承载:帧尾**最后绘制**、命中由
inspector 自管(不进 app 命中测试)、卡内容仍是 CTML view(inspector 整体
用 CTML 写,dogfooding)。该机制**不进公共组件集、不暴露为布局/样式语义**,
不构成对 T2 popup 推迟裁决的解冻。

### 6.4 落地顺序

M2 先交停靠模式 → 浮动模式为同里程碑增量(依赖 §6.3 dev-only 浮层通道)。
release 构建两模式一并剔除。

## 7. 布局静态验证(W3,编译期)

- **E8191 静态溢出**:子树内全部尺寸 fixed 且无 wrap 时,
  Σ子尺寸 + Σgap + padding > 容器 fixed 尺寸 → 编译期报;
- **E8192 死 fill**:fill/grow 落在运行时必然零剩余空间的链上 → 编译期报;
- 诊断码经 §10 注册表核对后登记(沿用 §4.4 E8xxx 惯例);
- **适用范围(宁漏报不误报)**:子树内任一尺寸属性为绑定表达式 → 该子树放弃
  检测;hug/fill 混合子树不可静态判 → 不报;
- 负例语料进 `tests/09_gui/`,check 口径逐字诊断差分(黄金基线法)。

## 8. 里程碑

| 步 | 内容 | 前置 |
|---|---|---|
| **S5.5** prov-v0(插入 MVP 阶梯 S5/S6 间,~1 天) | 影子树 + 后置推导 + 自检断言 + `gui_prov_dump` 黄金差分 | S3 桥(已完成) |
| M2 inspector | 停靠模式 → 浮动模式增量;prov 记录层接 UI | S5.5 |
| W3 静态验证 | E8191/E8192 检查面 + 负例语料 | W3 检查面切片 |

## 9. 回填清单

- `2026-09-16-gui-ctml-design.md` §13.2"创新空间"加指针行(本文完成时回填);
- 同规格 §4.4 诊断表登记 E8191/E8192(经注册表核对);
- `2026-09-16-gui-mvp-ladder.md` 增补 S5.5 行(writing-plans 阶段落地)。

## 10. 否决案存档

| 备选 | 否决理由 |
|---|---|
| 读 Clay v0.14 内部中间数组(记录层方案 B) | 内部字段升级是隐性破坏点;黄金快照只测输出测不出内部读取断裂 |
| Ctron 侧第二解释器复算(方案 C) | 同一 flex 算法出现双真源,违反单一真源原则 |
| comptime 绝对几何预布局 | 文本测量依赖运行时字体(FreeType vs S3 启发式数值不同),编译期不可判 |
| inspector 三种顶层形态独立实现 | A/C 本质是停靠区内部密度差;重复机制 |
| inspector 面板内编辑样式 | 与热重载开发循环重复;改代码即改外观是既有心智 |

## 11. 能力评审补记(2026-09-17,规格审阅期)

对 §13.2 flex 子集做了能力边界推演(视觉稿存档 `.superpowers/brainstorm/`,
主题 layout-capability;下列两项为**推荐默认,终审可改**):

- **能力结论**:工具型软件(IDE 三栏/聊天/邮件主从/设置表单/卡片墙/播放器)
  flex 全覆盖——主流桌面应用本身多为 flex 实现。真实边界四项,均有裁决路径:
  二维网格(bento)与瀑布流 → grid 触发条款;浮层(角标/tooltip/modal)→
  T2 推迟(inspector dev-only 内部浮层为先例通道);固定宽高比 → 本节裁决 2;
- **裁决 1 margin 不设**:§5.1 原列 margin,但 Clay 无 margin 概念——从子集
  删除,间距一律 gap + padding + spacer 表达,不设脱糖(最小子集原则);
  已回填 §5.1;
- **裁决 2 aspect-ratio / masonry 归入 grid 触发条款**:需求实证后再评;
  aspect 为单属性低成本候选(Clay 原生支持),媒体场景实证即补。
