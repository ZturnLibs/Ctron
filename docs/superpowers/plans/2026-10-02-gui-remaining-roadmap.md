# GUI 剩余工作完整清单(按优先级,逐条可执行)

> 2026-10-02 基线。前置已销账:输入体验全集、交互缝四件、拖拽缝+Slider、
> 组件嵌套 props、作用域 env、MenuBar v2、select 键盘导航、多选 list、
> input 变体、macOS 观感+DPI、MenuBar/Table/List/Select/Dialog/WList 组件、
> 数据面修复(浮条配对/作用域 env)。六红(s33/34/36/38/43/44)为发射泳道
> 红账,不在本清单。

## A 档:P1 组件尾巴(纯组合件,可立即动工;每项 0.5 天内)

- **GUI-01 tabs 组件**——标签页:触发器行 + when 内容切换(复用 Select 机制);陈列室+断言。
- **GUI-02 switch 组件**——checkbox 滑块变体(视觉改造,机制全同)。
- **GUI-03 radio 组件**——单选组:checkbox 姐妹件,实例后缀 pick + 单选清他(多选 list 反向)。
- **GUI-04 badge 组件**——纯样式小徽标(便宜件包收尾)。
- **GUI-05 accordion 折叠**——头按钮 + when 内容(便宜件包收尾)。
- **GUI-06 tooltip 组件化**——hover:名 + when 配方固化为陈列室组件模板(地基已全)。
- **GUI-07 toast 组件化**——overlay + tick 自动消失(消费 §2.7,演示位 + 断言)。
- **GUI-08 menu 加速器显示**——菜单项引用快捷键表同动作 id,⌘/Ctrl 字样经 gui_platform 显示。
- **GUI-09 menu 键盘导航**——展开态方向键/Enter/Esc(配方同 select 键盘导航)。

## B 档:输入/交互深化(小引擎面或配方深化)

- **GUI-10 select v2 overlay 锚定**——下拉改 overlay 悬浮(不推挤内容);静态锚先行(MenuBar 已验),动态锚(查询触发器矩形)登记。
- **GUI-11 progress 组件**——前置 GUI-17(值驱动宽),或 v1 分段 when 折衷。
- **GUI-12 内嵌重建保真**——叶字面量连字符/标点重建插空格根治(W1 重建器,编译面但 GUI 强需求;"INNER-ON→INNER -ON" 实证)。

## C 档:引擎能力缝(小改动,解锁多组件)

- **GUI-13 表达式下标与字符串切分**——`items[i]` 下标 + 切分内建;解锁 Table 多列、多选集解析、字符串处理族(规格多处以"前置"引用)。
- **GUI-14 程序化 focus() / scroll_into_view**——焦点原语开放给 view 层(解除 §7 宪法例外;表单提交后聚焦/错误行滚动可见)。
- **GUI-15 用户级 on:after 定时事件**——tick 原语 → 事件通道(防抖/延迟动作/toast 地基)。
- **GUI-16 值驱动宽高**——`w: {expr}` 逐帧求值(绕过样式折叠缓存的设计案);解锁 progress/fill/环形。

## D 档:P2 B 档(各自独立,中等工作量)

- **GUI-17 可拖分隔条**——消费拖拽事件缝(已落) + 值驱动宽(联动 GUI-16)。
- **GUI-18 tree 组件**——view 递归(已支持)+ 缩进 + 折叠 when 组合。
- **GUI-19 combobox**——input + select 合体(输入过滤 + 下拉)。
- **GUI-20 OS 文件拖入**——raylib IsFileDropped 现成;事件缝消费。
- **GUI-21 虚拟化长列表**——virtual each + row-h + overscan(master 设计蓝本已写)。
- **GUI-22 文本 ellipsis/行钳制**——测量轮截断 + 省略号。
- **GUI-23 链接 OpenURL**——text 元素 action 面(raylib OpenURL 现成)。
- **GUI-24 窗口级 API**——关闭拦截/全屏最大化/无边框/图标。
- **GUI-25 canvas 树内整合**——CTML canvas 元素 + 每帧绘制回调(gui_cjk 直绘已有)。
- **GUI-26 grid 布局**——前置:核查 Clay grid 支持面(已登记)。

## E 档:跨泳道(GUI 需求,编译/构建面执行,需协调)

- **GUI-27 组件跨文件 view 导入**——最大痛点:组件现只能同文件声明;编译面 view 导入(多次登记,需与发射泳道排期)。
- **GUI-28 ctcl gui.entry 烘焙**(J18)——UI 入口声明进包清单,构建期烘焙默认锚。
- **GUI-29 ctc 自动链接 + `ctc new --gui` 脚手架**(J19-④⑤)——30 行链接咒语从示例消失。

## F 档:待用户裁决(不裁决不动)

- **GUI-30 textarea/input 持久化默认化**——受控契约(现状)vs 自动同步(引入第二值源);影响所有输入应用模板。
- **GUI-31 主题包分发形态**——主题作为可导入包 vs 内置枚举扩充。

## G 档:C 档远期(裁决 #9 在册,排期靠后、目标不删)

- **GUI-32 多窗口**(架构级:raylib 单窗假设,需窗口层决策评审)
- **GUI-33 富文本/inline markup**
- **GUI-34 RTL 布局镜像**(文字 bidi 已有)
- **GUI-35 无障碍树/屏幕阅读器**
- **GUI-36 触摸/手势**
- **GUI-37 内部拖放**(列表重排/drop target;依赖拖拽事件缝 ✓)
- **GUI-38 date/time picker**
- **GUI-39 color picker**
- **GUI-40 command palette**
- **GUI-41 chart**
- **GUI-42 过渡动画**(插值,消费 tick ✓)
- **GUI-43 chord 序列组合键**(消费 §2.11 ✓)
- **GUI-44 异步任务到 UI**(跨泳道:语言面线程/通道就绪后合流)

## 执行建议

- 顺序:A 档(GUI-01→09)→ C 档(GUI-13 最优先,解锁最多)→ B/D 档按需 → E 档协调排期。
- 每条执行惯例:计划先行(mini)→ 夹具先行 → 双口径验证 → 阶梯全绿 → 落库 → 记忆。
- 外部依赖:六红发射泳道红账(s33/34/36/38/43/44)落窗后复验。
