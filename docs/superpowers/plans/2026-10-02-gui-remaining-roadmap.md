# GUI 剩余工作完整清单(按优先级,逐条可执行)

> 2026-10-02 基线,1003 全面收官:A/B/C/D 档全清+v2 尾巴(GUI-21/22)+E 档
> (GUI-28/29)+六红清零(ebebfc7f 发射三缝:env 先行判别/蹦床 void+B: 臂/s33
> 名通道契约;阶梯 82 过 0 败=历史首次全绿);GUI-25 真窗手验通过(fb7d4289)。
> 1004:F 档两裁决件销账(GUI-30 持久化默认化/GUI-31 主题包)。
> 残余=GUI-27 组件跨文件导入(排期靠后)、typed-props 组件实参(cf2132cf 登记)、
> numeric×textarea emit segfault(1004 立案)、G 档远期。

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

- ✅ **GUI-01(已销账) tabs 组件**——标签页:触发器行 + when 内容切换(复用 Select 机制);陈列室+断言。
- ✅ **GUI-02(已销账) switch 组件**——checkbox 滑块变体(视觉改造,机制全同)。
- ✅ **GUI-03(已销账) radio 组件**——单选组:checkbox 姐妹件,实例后缀 pick + 单选清他(多选 list 反向)。
- ✅ **GUI-04(已销账) badge 组件**——纯样式小徽标(便宜件包收尾)。
- ✅ **GUI-05(已销账) accordion 折叠**——头按钮 + when 内容(便宜件包收尾)。
- ✅ **GUI-06(已销账) tooltip 组件化**——hover:名 + when 配方固化为陈列室组件模板(地基已全)。
- ✅ **GUI-07(已销账) toast 组件化**——overlay + tick 自动消失(消费 §2.7,演示位 + 断言)。
- ✅ **GUI-08(已销账) menu 加速器显示**——菜单项引用快捷键表同动作 id,⌘/Ctrl 字样经 gui_platform 显示。
- ✅ **GUI-09(已销账) menu 键盘导航**——展开态方向键/Enter/Esc(配方同 select 键盘导航)。

## B 档:输入/交互深化(小引擎面或配方深化)

- ✅ **GUI-10(已销账) select v2 overlay 锚定**——✅ 1003 eeb4e3a5:下拉 overlay 悬浮(不推挤;陈列室轨道 y 恒等断言;单根包 vbox 避多根 segv);静态锚已落(MenuBar v2 同源),动态锚(查询触发器矩形)留 v3。
- ✅ **GUI-11(已销账) progress 组件**——✅ 1003 ccc32eb4:v1 分段折衷落库(bind 组装 10 格 █/░ 条+百分比叶,prog_set 钳位);值驱动宽(GUI-16)解锁后可升级连续条。
- ✅ **GUI-12(已销账) 内嵌重建保真**——✅ 1003 eb5cb5bc:GuiBlock 增 d[6] 体 {/} 行:列槽,emit 面源字节切片直出(探针双验+不过回退 token 旧径);叶连字符+连字样式名双根治;解释臂无 src 恒旧径(行为不变,在册口径)。

## C 档:引擎能力缝(小改动,解锁多组件)

- ✅ **GUI-13(已销账) 表达式下标与字符串切分**——✅ 1003 9ded6221:`items[i]` 下标(each: 通道路由+组件 env List 前置)+内建四件 cut/fld/sethas/contains+bxv 全家族 itemvar/item 穿参+检查面 E8195 形态门;s57+e8195 语料。Table 多列/多选集解析/字符串族解锁落地。
- ✅ **GUI-14(已销账) 程序化 focus() / scroll_into_view**——✅ 1003 2f226fe8:gui_focus_req(name)/gui_scroll_req(name)/gui_scroll_to(px) 三原语(API 面,零新属性;emit 期消费/collect 期钳位/一次性帧末过期);s70 六段。§7 例外以「运行时本地态按名寻址」解除。
- ✅ **GUI-15(已销账) 用户级 on:after 定时事件**——✅ 1003 1350a3a8:on:after="ms:handler" 引号串事件(元素存活域一次性+on:input 重臂防抖+隐灶不燃);解析三面引号通道;d_after_pump 显式泵/d_tick 确定性;s71 六段。toast/延迟动作/防抖地基落地。
- ✅ **GUI-16(已销账) 值驱动宽高**——✅ 1003 完整收口:v1 元素面(2b3176ea)+完整面容器宽(用户三裁决:统一语义 w 在即 FIXED 不写才 GROW/scroll-overlay 豁免/progress 升级连续条);迁移=s37 free 摘惰性 w、slider 轨道 220 转真(拖拽数学随调)、gallery card 摘惰性 w、progress v2 连续条(pbar 块字符退役);s72 五段试金石。

## D 档:P2 B 档(各自独立,中等工作量)——✅ 1003 十条全落库

- ✅ **GUI-17(已销账) 可拖分隔条**——✅ 1003 2b3176ea:水平分隔(上下窗格)d_dragv 驱动+mousey 绝对指针内建(permille 相对盒在分隔条场景饱和);前置 GUI-16 最小面同提交落地。
- ✅ **GUI-18(已销账) tree 组件**——✅ 1003 6bdfc813:扁平表配方(应用侧可见集重算+缩进/▸▾ 前缀),点击经实例后缀定位(s56);递归 view 形态依赖下标表达式留 v2。
- ✅ **GUI-19(已销账) combobox**——✅ 1003 03859b7e:输入即开(on:input 过滤+下拉同帧)+overlay 下拉+点选填入(s59);过滤谓词应用侧手写(GUI-13 落库可换 contains)。
- ✅ **GUI-20(已销账) OS 文件拖入**——✅ 1003 ffa4acd1:窗口级 on:drop(事件码 6,IsFileDropped 轮询+IsWindowReady 门),逐文件 fire+droppath/dropn 内建;headless d_drop(s61)。
- ✅ **GUI-21(已销账) 虚拟化长列表**——✅ 1003 718db27b(6663cf9b 改号 s69):scroll 标记容器(gui_cfg2 clip+偏移槽)进共享运行时+可见窗口配方(bind 只组装窗口行,前占位+总高 O(1));**v2 ✅ 1003 c7f22a47:virtual each 标签形态**(each class virtual:N+运行时窗口,固定 overscan 2,合成占位空盒;itemvar/实例下标窗内保真;视口外 overscan 行 Clay 剔除=可观测契约;s76)。
- ✅ **GUI-22(已销账) 文本 ellipsis/行钳制**——✅ 1003 b638b4f4:label ellipsis+max_w 预算,rt_meas_w 直调 ft 实测(FT_OFF 回启发式)+逐码点截断(s63);**v2 ✅ 1003 2401107a:maxlines 行钳制**(label maxlines:N 串级行钳+尾行省略;多行布局=Clay 原生 \n 拆行,ft 侧零改动——\n 化 measure/raster 曾致 s51 黄金度量回归已回退;s75)。
- ✅ **GUI-23(已销账) 链接 OpenURL**——✅ 1003 ffa4acd1:link 样式按钮 on:click 内 open_link(headless 记录槽 d_open_url 断言,真窗 raylib OpenURL)(s60)。
- ✅ **GUI-24(已销账) 窗口级 API**——✅ 1003 a21734e0:关闭拦截(根 on:close+glfw flag 复位吞请求)+win_op 编码面(fullscreen/max/min/restore/undecorated/bordered)+win_icon(s62);真窗拦截语义待 --run 手验。
- ✅ **GUI-25(已销账) canvas 树内整合**——✅ 1003 b92f8ea2:canvas 标记属性容器(零新标签)+每帧 on:draw(rt_canvas_frame)+canvas_px 绘制原语(headless 记录槽)(s65);**真窗直绘 ✅ 1003 fb7d4289 手验通过**(双层遮挡修复:fire 移绘制相 flush 后——帧头 fire 被 clear 抹、flush 前 fire 被盒背景盖;真窗截图像素实证;连带登记两条编译债:顶层可变全局 var 双驱 SIGSEGV/Box 字段复合赋值发射 panic)。
- ✅ **GUI-26(已销账) grid 布局**——✅ 1003 37f76691:核查结论=Clay v0.14 无 Grid API(升级牵连黄金金值,登记裁决项);v1=GROW 均分网格行配方(hbox 多 GROW 子自动等宽,s64);**多列数据驱动 ✅ 1003 对端 15ecef4f(grid each+下标实参)+dd0d3cbd(Table 多列+按钮带子树)**。
- 附带:**GUI-16 值驱动宽高最小面**(1003 2b3176ea)随 GUI-17 落地——`w:/h: "{expr}"` 逐帧求值(gt_dim/gt_dim_v,引号串载体);容器 w 消费因 s37 存量口径站岗撤销,完整面(容器宽+样式折叠缓存)仍是 C 档议题。

## E 档:跨泳道(GUI 需求,编译/构建面执行,需协调)

- ⏳ **GUI-27(未做) 组件跨文件 view 导入**——最大痛点:组件现只能同文件声明;编译面 view 导入(多次登记,需与发射泳道排期)。
- ✅ **GUI-28(已销账) ctcl gui.entry 烘焙**(J18)——✅ 1003 09232060:清单 gui 节 entry 键→emit 期烘焙 ctron_gui_entry 常量(新内建四注册面;无清单恒 app.ctml 零迁移);gui.ct 四消费点(锚读取+热重载监视);s73 三缝断言。
- ✅ **GUI-29(已销账) ctc 自动链接 + `ctc new --gui` 脚手架**(J19-④⑤)——✅ 1003 09232060:dependencies.gui 探测→自动链接 vendored 全套(awk END-exit 覆盖坑=found 标志);ctc new <dir> --gui 双路模板;gui_calc run.sh 咒语吸收为零细节消费示范;s74 e2e。

## F 档:裁决件——✅ 2026-10-04 两项全销账

- ✅ **GUI-30(已销账) textarea/input 持久化默认化**——✅ 1004:裁决=自动同步为默认(bind 变量唯一值源不变:显示每帧从变量读,输入即写回;第二值源不存在,引擎零缓冲);受控契约保留=有 on:input 者应用接管,合成面零分支。机制=collect 面收「无 on:input 的 input/textarea 纯点链 bind+型别(Str/I32)」入 bset,gen 面 act 合成 "__bs:路径:全文" 写回分支(Str 直赋;I32 经 ev_arg_i 桥,nsyms 已注册);运行时 rt_in_fire 无处理器时改发写回头。连带缝合=gui_sk_node is_input 漏 textarea(装配面 nbid 恒空,显示面走 gt 树无症状,写回面首曝)。I32 绑定编辑=十进制串语义(42 后打 7 得 427,探针实证)。s77 四层断言(合成直驱/I32 桥/运行时全链/受控口)+六夹具回归门全绿。
- ✅ **GUI-31(已销账) 主题包分发形态**——✅ 1004:裁决=主题作为可导入包(不进引擎;内置 theme_dark/light/mac_* 族保持平台×明暗最小核,与 0924「能力优先于 hack」横切裁决同向)。落点=pkgs/themes/themes.ct(Nord/NordLight/SolarizedDark/SolarizedLight/Dracula/GruvboxDark/TokyoNight 七主题,每主题一枚 pub fn 构造全套 gui.Theme 25 字段,尺寸全对齐缺省);消费=`use themes.{nord}` + `theme_apply(nord())` 两步;跨包 Theme 构造/use 请求零特殊。s78 三层断言(链接/换装令牌折叠/标准装配事件消费);样式面引用令牌(bg: BG_BASE)方随换装(盒无令牌引用则色不随)。
- ⚠️ **现役债立案(1004,s77 排障途中 HEAD 复现)**:numeric 标记 input × textarea 节点并存 → emit 段 segfault(strlen NULL)。最小形态=视图含 `<input bind={x} numeric/>` 与任意 `<textarea/>`;单独任一均过。与 GUI-30 改动无关(HEAD 版二进制同崩),s77 已绕开(不带 numeric);根因待查(疑装配改写或 trans 面)。
- ⚠️ **现役债立案(1004,全量回归发现;s30_props_d 运行段 segfault)**:t_rt_emit 渲染行走读野指针(0xad39...),emit/cc 段均过。**纯 HEAD 全树复现**(我方工作树+P0-C 半成品分别 stash 三级对照全崩)=已提交基线现役债,与 F 档改动无关。GC 默认 bump(未开)排除悬挂回收;嫌疑=T31 M1.5 精确帧根集(353e16b8)或 P9 栈面(ea678b40)合流后 s30 形态首踩(1003 阶梯全绿在合流前,合流后未复跑全阶梯);87 夹具中独此一家。归 GC/发射泳道复核,复现=`sh tests/gui/s30_props_d/run.sh`。

## G 档:C 档远期(裁决 #9 在册,排期靠后、目标不删)

- ⏳ **GUI-32(未做) 多窗口**(架构级:raylib 单窗假设,需窗口层决策评审)
- ⏳ **GUI-33(未做) 富文本/inline markup**
- ⏳ **GUI-34(未做) RTL 布局镜像**(文字 bidi 已有)
- ⏳ **GUI-35(未做) 无障碍树/屏幕阅读器**
- ⏳ **GUI-36(未做) 触摸/手势**
- ⏳ **GUI-37(未做) 内部拖放**(列表重排/drop target;依赖拖拽事件缝 ✓)
- ⏳ **GUI-38(未做) date/time picker**
- ⏳ **GUI-39(未做) color picker**
- ⏳ **GUI-40(未做) command palette**
- ⏳ **GUI-41(未做) chart**
- ⏳ **GUI-42(未做) 过渡动画**(插值,消费 tick ✓)
- ⏳ **GUI-43(未做) chord 序列组合键**(消费 §2.11 ✓)
- ⏳ **GUI-44(未做) 异步任务到 UI**(跨泳道:语言面线程/通道就绪后合流)

## 执行建议

- 顺序:A 档(GUI-01→09)→ C 档(GUI-13 最优先,解锁最多)→ B/D 档按需 → E 档协调排期。
- 每条执行惯例:计划先行(mini)→ 夹具先行 → 双口径验证 → 阶梯全绿 → 落库 → 记忆。
- 外部依赖:六红发射泳道红账(s33/34/36/38/43/44)落窗后复验。
