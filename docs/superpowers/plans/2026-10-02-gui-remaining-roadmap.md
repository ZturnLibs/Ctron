# GUI 剩余工作完整清单(按优先级,逐条可执行)

> 2026-10-02 基线。前置已销账:输入体验全集、交互缝四件、拖拽缝+Slider、
> 组件嵌套 props、作用域 env、MenuBar v2、select 键盘导航、多选 list、
> input 变体、macOS 观感+DPI、MenuBar/Table/List/Select/Dialog/WList 组件、
> 数据面修复(浮条配对/作用域 env)。六红(s33/34/36/38/43/44)为发射泳道
> 红账,不在本清单。

## A 档:P1 组件尾巴(纯组合件,可立即动工;每项 0.5 天内)——✅ 2026-10-03 全档销账
> 九条全落库(gui-a-tail 分支,夹具 s66_cbx_var/s67_comp_a/s68_menu_acc+陈列室):
> 引擎面=checkbox switch/radio 裸属性变体+元素级圆角(gui_radius 消费即复位)
> +pub gui_now+each 行简单名 checked 的 eachrow inst.to_string 修复(发射臂
> int 直接作 char* 崩,触发面=GUI-03 radio);视图面=tabs(when cond={cur==N}
> 数值条件)/badge/accordion(slot)/tooltip(hover 通道)/toast(透明 overlay+
> gui_now 窗口)/menu 加速器(acc_of 与 gui_hotkey 同动作 id,⌘ 经 gui_platform)
> /menu 键盘导航(↓↑/Enter/Esc 键闭包拦截,快捷键 d_mod+d_send_key 实发)。
> 坑位:button 文本按词折行拆 TEXT(行宽须显式,三犯);发射臂分支内 Str 变量
> 绕行(字形叶字面量直调)。

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

- **GUI-10 select v2 overlay 锚定**——✅ 1003 eeb4e3a5:下拉 overlay 悬浮(不推挤;陈列室轨道 y 恒等断言;单根包 vbox 避多根 segv);静态锚已落(MenuBar v2 同源),动态锚(查询触发器矩形)留 v3。
- **GUI-11 progress 组件**——✅ 1003 ccc32eb4:v1 分段折衷落库(bind 组装 10 格 █/░ 条+百分比叶,prog_set 钳位);值驱动宽(GUI-16)解锁后可升级连续条。
- **GUI-12 内嵌重建保真**——✅ 1003 eb5cb5bc:GuiBlock 增 d[6] 体 {/} 行:列槽,emit 面源字节切片直出(探针双验+不过回退 token 旧径);叶连字符+连字样式名双根治;解释臂无 src 恒旧径(行为不变,在册口径)。

## C 档:引擎能力缝(小改动,解锁多组件)

- **GUI-13 表达式下标与字符串切分**——✅ 1003 9ded6221:`items[i]` 下标(each: 通道路由+组件 env List 前置)+内建四件 cut/fld/sethas/contains+bxv 全家族 itemvar/item 穿参+检查面 E8195 形态门;s57+e8195 语料。Table 多列/多选集解析/字符串族解锁落地。
- **GUI-14 程序化 focus() / scroll_into_view**——✅ 1003 2f226fe8:gui_focus_req(name)/gui_scroll_req(name)/gui_scroll_to(px) 三原语(API 面,零新属性;emit 期消费/collect 期钳位/一次性帧末过期);s70 六段。§7 例外以「运行时本地态按名寻址」解除。
- **GUI-15 用户级 on:after 定时事件**——✅ 1003 1350a3a8:on:after="ms:handler" 引号串事件(元素存活域一次性+on:input 重臂防抖+隐灶不燃);解析三面引号通道;d_after_pump 显式泵/d_tick 确定性;s71 六段。toast/延迟动作/防抖地基落地。
- **GUI-16 值驱动宽高**——**v1 已随 1003 2b3176ea 落地**(w:/h: "{expr}" 引号串载体逐帧求值,元素面);本档余留=完整面:容器宽消费(s37「容器宽恒 GROW」存量口径站岗,破坏性语义变更需裁决)+样式折叠缓存交互(gt_dim_v 已逐帧绕缓存,完整面待定缓存契约)。设计案另册待裁决。

## D 档:P2 B 档(各自独立,中等工作量)——✅ 1003 十条全落库

- **GUI-17 可拖分隔条**——✅ 1003 2b3176ea:水平分隔(上下窗格)d_dragv 驱动+mousey 绝对指针内建(permille 相对盒在分隔条场景饱和);前置 GUI-16 最小面同提交落地。
- **GUI-18 tree 组件**——✅ 1003 6bdfc813:扁平表配方(应用侧可见集重算+缩进/▸▾ 前缀),点击经实例后缀定位(s56);递归 view 形态依赖下标表达式留 v2。
- **GUI-19 combobox**——✅ 1003 03859b7e:输入即开(on:input 过滤+下拉同帧)+overlay 下拉+点选填入(s59);过滤谓词应用侧手写(GUI-13 落库可换 contains)。
- **GUI-20 OS 文件拖入**——✅ 1003 ffa4acd1:窗口级 on:drop(事件码 6,IsFileDropped 轮询+IsWindowReady 门),逐文件 fire+droppath/dropn 内建;headless d_drop(s61)。
- **GUI-21 虚拟化长列表**——✅ 1003 718db27b(6663cf9b 改号 s69):scroll 标记容器(gui_cfg2 clip+偏移槽)进共享运行时+可见窗口配方(bind 只组装窗口行,前占位+总高 O(1));overscan/virtual each 标签形态留 v2。
- **GUI-22 文本 ellipsis/行钳制**——✅ 1003 b638b4f4:label ellipsis+max_w 预算,rt_meas_w 直调 ft 实测(FT_OFF 回启发式)+逐码点截断(s63);行钳制(maxlines)留 v2。
- **GUI-23 链接 OpenURL**——✅ 1003 ffa4acd1:link 样式按钮 on:click 内 open_link(headless 记录槽 d_open_url 断言,真窗 raylib OpenURL)(s60)。
- **GUI-24 窗口级 API**——✅ 1003 a21734e0:关闭拦截(根 on:close+glfw flag 复位吞请求)+win_op 编码面(fullscreen/max/min/restore/undecorated/bordered)+win_icon(s62);真窗拦截语义待 --run 手验。
- **GUI-25 canvas 树内整合**——✅ 1003 b92f8ea2:canvas 标记属性容器(零新标签)+每帧 on:draw(rt_canvas_frame)+canvas_px 绘制原语(headless 记录槽)(s65);真窗直绘待 --run 手验。
- **GUI-26 grid 布局**——✅ 1003 37f76691:核查结论=Clay v0.14 无 Grid API(升级牵连黄金金值,登记裁决项);v1=GROW 均分网格行配方(hbox 多 GROW 子自动等宽,s64);多列数据驱动依赖 GUI-13 下标留 v2。
- 附带:**GUI-16 值驱动宽高最小面**(1003 2b3176ea)随 GUI-17 落地——`w:/h: "{expr}"` 逐帧求值(gt_dim/gt_dim_v,引号串载体);容器 w 消费因 s37 存量口径站岗撤销,完整面(容器宽+样式折叠缓存)仍是 C 档议题。

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
