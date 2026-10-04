# GUI G 档(GUI-32~44)执行规划——远期 13 项分解、地基盘点与波次排期

> 2026-10-04 立稿。来源=路线图 G 档(裁决 #9 在册:排期靠后、目标不删);
> 基线=1004 F 档销账后(7a9aebcf,A–F 档全清,阶梯 82 过/1 败=基线现役债)。
> 本文回答:13 项各自**做什么/依赖什么地基/缝多大/怎么排波次/哪些必须先裁决**。
> 执行仍走惯例:每件计划先行(mini)→ 夹具先行 → 双口径验证 → 阶梯全绿 → 落库 → 记忆。

## 0. 总原则(规范性,继承既有横切裁决)

1. **最小完备红线**(spec §13.1):内建组件全量 ≤ 18,长尾组件由用户组合承担。
   本档组件类(GUI-38/39/40/41/42)**一律组合件/库形态,不新增引擎内建、不新增绘制原语**
   ——每个组件必须能由 Clay 命令 + 文本渲染 + canvas(已有)实现。
2. **能力优先于 hack**(0924 横切裁决):引擎面缺口走扩展不走配方绕行;配方只承担
   "地基已全"的纯组合。规划中每项标明缝级,缝级=引擎/编译面真实改动,不许降级为 hack。
3. **双口径验证**:每件 headless `test()/d_*` 探针断言(检查面/装配面/渲染面/交互面四层
   惯例)先行;涉真窗视觉的项(动画/多窗/触摸)随件 `--run` 手验记录。
4. **机刷协作守则**:夹具号 s79 起建议槽,**执行前实测再占**(撞号分目录不撞号,
   s78_xfileview/s78_themepkg 先例);提交 pathspec 限定;发射泳道在飞三文件
   (driver_emit/trans_conc/trans_expr)零交集原则继续。

## 1. 地基盘点(1004 考据快照)

| 件 | 地基已有(证据) | 缺口 | 缝级 |
|---|---|---|---|
| 32 多窗 | 单窗循环(rt_window_loop:InitWindow/BeginDrawing/Clay 全局 init);GUI-24 窗口级 API | 窗口层抽象(raylib **不支持多 OS 窗**);虚拟窗 z 序/焦点路由/事件分发 | **架构级** |
| 33 富文本 | ft 测宽(rt_meas_w)/ellipsis/maxlines、字重(W5c)、gui_font_register、link+open_link、hit 面 | 内联标记解析 + span 分段排版(word-wrap 协同) | 引擎缝(中-大) |
| 34 RTL | sheenbidi 已 vendored+s14/s15/s17 三夹具(文字 bidi✓);hbox/vbox 线性布局 | 布局镜像(子序反转+pad/align 左右对调)+dir 继承 | 引擎缝(中) |
| 35 无障碍 | retained GuiTree(ntag/属性槽)、焦点环(rt_focus_next/Tab 导航) | 语义属性(role/label)槽 + 平台 shim + 播报 | **架构级**(平台 C shim) |
| 36 触摸 | 事件轮询 gui_poll_event(码 1–6)、raylib 触摸/手势 API(拉模式✓)、click_kind 双击聚类 | 触摸事件族 + 手势状态机(tap/longpress/swipe/pinch) | 引擎缝(中) |
| 37 内部拖放 | drag 缝 rt_dgev_frame/gui_dgev_*(逐帧 drag 事件✓)、rt_hit_name 命中查询、d_drag 探针、OS 文件 drop(evt 6) | 释放命中 drop 目标 + payload 传递 + dropin 事件 | 引擎缝(中) |
| 38 date/time | std time.ct 历法全套(leap_days_in_month/days_from_civil/weekday/format/parse)、grid✓ each+下标✓、select 键盘导航✓、overlay✓ | 组合件本体;弹层动态锚(GUI-10 v3 遗留) | 零~小缝 |
| 39 color picker | canvas 树内标记+直绘+探针(GUI-25)、slider(GUI-16/17 同族)、hex 解析(g_parse_color,内部) | 组合件本体;hex→packed 的 pub 面(顺手小件) | 零缝 |
| 40 command palette | overlay(Esc 自锁✓)、过滤(contains✓)、键盘导航配方(select✓)、gui_hotkey✓、virtual each✓ | 组合件本体 | 零缝 |
| 41 chart | 值驱动宽高(GUI-16✓)、each+下标+cut/fld(GUI-13✓)、canvas 直绘✓、Clay RECT 树内 | 图表库本体;线图树内 z 序(canvas 现 flush 后=永远盖 UI) | 零缝(条图)/裁决(线图) |
| 42 过渡动画 | bind 每帧重算✓、gui_now✓、值驱动 w/h✓、on:after 定时(GUI-15✓) | ease/anim 库;声明式 transition(另行裁决) | 零缝(库) |
| 43 chord | gui_hotkey 表(combo="mod+K" 单段:mods=mod/ctrl+1 shift+2 alt+4)+gui_hotkey_match 路由位、gui_now、d_send_key 探针 | 序列 combo 解析 + 前缀武装状态机 + 超时 | 引擎缝(小) |
| 44 异步→UI | **spawn+Send(E3010 守卫)+Channel send/recv 均已落地**(ct_spawn/ct_ch_*;并发 GC 退避✓)——路线图"等语言面"前提已兑现 | 非阻塞取件(现 send/recv 全阻塞,recv 会冻 UI);UI 线程帧首 drain 面 | 跨泳道(小) |

关键单点说明(逐项规划的地基引用):

- **帧循环**(gui.ct rt_window_loop):每帧序 = drag 状态机 → poll_event(码 1 键/2 点/
  3 字符/4 滚轮/5 右键/6 文件拖入)→ 事件路由链(focus input → overlay → Tab → hotkey 表
  → key 闭包)→ 重组装 bind → rects 重收集 → BeginDrawing/clear/**flush**/canvas 直绘/EndDrawing。
  新事件族=扩事件码或复用码加子态;canvas 在 flush 后 = overlay z 序(GUI-41 线图裁决点的根因)。
- **bind 每帧重跑** + 值驱动 w/h + gui_now = 动画的地基已闭环(纯库即可)。
- **hotkey 表形态**:单段 combo(mod+K);chord 需把"空格分隔序列"引入解析与匹配状态机。
- **通道面**:send/recv 阻塞(ct_ch_send/recv,cond_wait);try_recv 不存在;且 ct_ch_* 是
  应用 TU 内 static 函数,pkgs/gui/c_src/ctron_gui.c 独立 TU **extern 不可达**——GUI-44 v1
  走 C 胶水侧自建投递队列可完全绕开编译泳道(见逐项规划)。

## 2. 波次排期(四波,依赖拓扑 + 风险分层)

| 波 | 件 | 共性 | 规模合计 |
|---|---|---|---|
| 一(先行快胜) | GUI-40 → 38 → 39 → 42 → 41v1 | 纯组合/库,零引擎缝;每件=陈列室段+四层断言 | ~4–5.5 天 |
| 二(交互缝) | GUI-43 → 37 → 44 | 引擎/C 胶水小-中缝;探针面扩展 | ~4–5 天 |
| 三(文本/布局) | GUI-33 → 34 | 排版与布局深水面;黄金度量面敏感(s21) | ~3.5–5 天 |
| 四(架构级) | GUI-36 → 35 → 32 | 各需前置评审;32 最大且需窗口层决策 | ~7–10 天 |

排序理由:波一全部零缝且互相独立,先落产生即时用户价值并验证组合件惯法(为后续组件
立模板);波二三件都是"事件/并发面小缝+大验收面",chord 最小先行;波三触碰文本排版
核心路径(s21 黄金度量/ft 面),排在中段、单独波次便于回归归因;波四每件带架构评审门,
GUI-32 压轴(窗口层决策未裁决前不动)。**波间无硬依赖,可机刷并行;波内按序(共享文件面)。**

## 3. 逐项规划

> 每项:目标 / 消费形态草图(地道语法,写码前仍按惯例核对 idiom 清单与双宿主坑位)/
> 落点面 / 验收 / 建议夹具槽 / 规模 / 裁决点。草图中的属性名与 API 名为**规划名**,
> 执行 mini 计划时定稿。

### GUI-40 command palette(零缝,波一首件,0.5–1 天)

- 目标:⌘K 唤起的全局命令面板(过滤+键盘导航+执行),作为 overlay/过滤/导航/热键
  四地基的"组合件样板"。
- 草图:

  ```ct
  // 登记:mod+K → 动作 id;act 里置 open=true(Esc 关闭走 overlay 自锁✓)
  gui_hotkey("mod+K", "palette")
  view palette(q: Str) {
      // 应用侧 bind 面维护 List[Str] lines:逐帧按 q 过滤登记表(contains✓)
      <when cond={open}>
          <overlay class="palette">
              <input bind={q} placeholder="输入命令…"/>
              <each it in={lines}>
                  <button on:click={run_cmd(it)}><label text="{fld(it, '|', 0)}"/></button>
              </each>
          </overlay>
      </when>
  }
  ```

- 落点:pkgs/gui 门面收口(components 段)+ examples/gui_widgets 陈列室段;
  模糊匹配 v1=contains 子序计分(应用侧纯函数)。
- 验收:headless——hotkey 触发开、过滤行数、↓/Enter 执行路径(d_send_key 序列)、
  Esc 关闭;渲染面 d_cmd 断言面板层级。
- 夹具槽 s79_palette。裁决点:无。

### GUI-38 date/time picker(零~小缝,波一,1–1.5 天)

- 目标:date_pick(月历网格选日)+ time_pick(时分选择),bind 值=days_from_civil
  基准日 / 分钟数(0–1439)。
- 草图:

  ```ct
  view date_pick(value: I32) {        // value = days_from_civil(y,m,d)
      <hbox>   // ‹ › 换月 + 标题行(weekday/leap_days_in_month 全套✓)
      </hbox>
      <each r in={weeks}>             // 每行 7 格:leading 空位 + 月内天;选中态 when
      </each>
  }
  // 弹层消费:icon button → overlay 挂 date_pick
  ```

- 落点:同上组合件段;**弹层动态锚**(GUI-10 v3 遗留:查询触发器矩形)若随件落,
  作为共享小缝先行(color picker 弹层同消费)——否则 v1 内联展开形态。
- 验收:月历 correctness(跨月/闰年/周对齐,驱动=std time.ct 对拍)、选中写回、
  time_pick 步进键盘路径。
- 夹具槽 s80_datepick。裁决点:弹层动态锚随件 or 内联先行(执行时定,倾向随件)。

### GUI-39 color picker(零缝,波一,1 天)

- 目标:SV 方(canvas)+ hue 条 + hex 输入 + 预览 swatch 的标准取色器。
- 草图:

  ```ct
  view color_pick(hex: Str) {
      canvas w:256 h:192 on:draw={paint_sv}      // GUI-25 直绘✓
      hue_strip / hex_input(bind, #RRGGBB 校验) / preview_swatch
  }
  ```

- 落点:组合件段;顺手小件=`gui_color_parse(hex) -> I32` pub 面(现 g_parse_color
  内部,检查面同款语义 #RRGGBB)。
- 验收:canvas 像素探针(d_px_at)验 SV 渐变、hex 双向、拖拽选点(d_drag)写回。
- 夹具槽 s81_colorpick。裁决点:无。

### GUI-42 过渡动画 v1(零缝·库,波一,0.5 天)

- 目标:pkgs/gui 门面内 anim 库(ease 函数族 + 时间线帮手),bind 每帧重算+
  值驱动 w/h 消费;陈列室动效段(展开/滑入/淡入)。
- 草图:

  ```ct
  // pkgs/gui/anim.ct(门面 re-export)
  pub fn ease_out_cubic(u: F64) -> F64 { ... }      // 无三元:if 表达式
  pub fn anim(now: I32, t0: I32, ms: I32, a: I32, b: I32) -> I32 {
      // u = 钳位((now-t0)/ms) → eased lerp → I32;gui_now() 驱动
  }
  // 消费:h: "{anim(gui_now(), t0, 240, 0, 320)}" ——值驱动尺寸/位置逐帧演化
  ```

- 落点:pkgs/gui/anim.ct + 陈列室;**写码前核对双宿主 F64 坑位清单**(host-divergences)。
- 验收:d_tick 驱动帧,断言关键帧值(单调/终点钳位/超时恒值);真窗 --run 手验顺滑。
- 夹具槽 s82_anim。裁决点:声明式 `transition:` 属性(引擎代插值)**不在本件**,
  另行裁决(见 §4 决策 5)。

### GUI-41 chart v1(条图零缝/线图 overlay,波一,1–1.5 天)

- 目标:pkgs/charts 前身——pkgs/gui 门面内 chart 段:bar_chart/sparkline(树内
  Clay RECT)+ line_chart(canvas);轴/刻度=label 组合。数据入参=逗号串
  (view props 型别惯例,GUI-13 cut/fld 解析)。
- 草图:

  ```ct
  view bar_chart(values: Str, w: I32, h: I32) {
      <hbox class="grow">  // each 条:值驱动 h%(GUI-16✓),max 归一应用侧
      </hbox>
  }
  view line_chart(values: Str, w: I32, h: I32) {
      <canvas w="{w}" h="{h}" on:draw={plot_line}/>   // v1=overlay z 序(见裁决 4)
  }
  ```

- 落点:chart 段 + 陈列室数据段;hover tooltip v2(依赖命中查询,已有 rt_hit_name)。
- 验收:渲染面 d_cmd_x/w 断言条几何(归一正确性)、canvas 像素探针验折线;
  零数据/单点/负值边界。
- 夹具槽 s83_chart。裁决点:线图树内 z 序(见 §4 决策 4)。

### GUI-43 chord 序列组合键(引擎小缝,波二首件,1 天)

- 目标:序列热键("g g"、"mod+d d" vim 风格),非前缀键零干扰(不吞)。
- 落点:gui_hotkey 解析器扩**空格分隔序列**(单段兼容字节恒等);gui_hotkey_match
  加前缀臂:首中前缀→武装(gui_now 戳),超时(默认 800ms,gui_hotkey_chord_ms
  可配)或非前缀键→解除并按原路由;完成→act。渲染面不动。
- 验收:headless d_send_key 序列断言(完成/超时/干扰键不吞三径);既有 s33 键消费
  边界回归(单段路径字节恒等)。
- 夹具槽 s84_chord。裁决点:武装期视觉指示(HUD)v1 不做(另立案)。

### GUI-37 内部拖放(引擎中缝,波二,1.5–2 天)

- 目标:应用内拖拽重排/投递(列表重排为锚定场景)。
- 形态:

  ```ct
  <item on:drag={drag_start(i)} on:dropin={drop_at(i)}>…</item>
  // drag 缝已逐帧武装✓;本件补:释放(up 且 dgev armed)→ rt_hit_name(释放点)
  // → 命中带 on:dropin 节点 → fire(handler, payload=首发实参)
  ```

- 落点:rt_window_loop 释放路径(dgev armed 分支)+ 检查面 dropin 事件名收编;
  d_drag 探针扩展断言 dropin 触发与 payload;over 高亮(v2)与自动滚动(v2)不进本件。
- 验收:重排端到端(d_drag 拖 row1→row3,断言序变)、payload 保真、无目标释放=无火。
- 夹具槽 s85_dragdrop。裁决点:payload 形态 v1=首发 drag 实参直传(多值=应用侧
  打包单 Str),不立 data-* 属性族(最小完备)。

### GUI-44 异步任务→UI(跨泳道小缝,波二,1–2 天)

- 目标:后台任务(spawn)结果安全上屏(UI 线程唯一触 UI)。
- 形态(裁决 6 定向后的推荐路):

  ```ct
  // C 胶水侧投递队列(v1,零编译泳道波及):
  spawn { var s = fetch(); ctron_gui_async_post(s) }   // Str→char* 借用编组✓(FFI v0)
  // 帧首 drain(gui 注册面):gui_async_on("on_result") → drain 非空即 act("on_result",[item])
  ```

- 落点:ctron_gui.c(互斥锁队列 post/drain,~40 行 C)+ gui.ct 注册面 + 帧首
  drain 挂点(rt_window_loop 与 headless d_frame 同挂)+ 检查面零改动。
  **路线图前提修正**:spawn/Channel 已落地,"等语言面"已兑现;v1 不需要 try_recv
  (ct_ch_* 应用 TU static,跨 TU 不可达,见 §1);try_recv 语言缝另立案编译泳道
  (泛化价值,非本件阻塞项)。
- 验收:headless——d_chan_feed 注入→d_frame 后断言 handler 触发与顺序;真窗
  fetch 模拟(延迟任务)不冻 UI(帧计数探针)。
- 夹具槽 s86_async。裁决点:post 载荷 v1=Str(结构化=应用侧编码),多通道=多
  action id;不做 Chan[T] 泛型贯通。

### GUI-33 富文本 v1(引擎中-大缝,波三,2–3 天)

- 目标:label 内联富文本(字重/斜体/等宽/链接)。
- 草图(裁决 2 定向后,推荐 markdown 子集):

  ```ct
  <label rich="**加粗** *斜体* `code` 与 [链接](https://example.com)" />
  ```

- 落点:引擎面=rich 属性收编(检查面+运行时):文本解析→span 段(字重 W5c✓/
  颜色/链接标记)→逐段 rt_meas_w 排放(单行 v1);链接段 hover 光标+click
  open_link✓。**多行 wrap 富文本 v2**(Clay 拆行与分段排版协同是深水面,
  s51/s75 教训在册)。
- 验收:分段测宽对拍(纯串拼接 vs 分段和,ft 面)、hit 命中链接段、转义
  (字面 `*` 的逃逸形态);既有 ellipsis/maxlines 回归。
- 夹具槽 s87_richtext。裁决点:标记语法(§4 决策 2)。

### GUI-34 RTL 布局镜像(引擎中缝,波三,1.5–2 天)

- 目标:dir="rtl" 元素级(继承)布局镜像;文字面 bidi 已有(s14/s15/s17)。
- 落点:引擎面=hbox 子序反转+pad/margin 左右对调+text align 起始侧对调
  (rt_emit 组装臂);dir 解析与继承(检查面收编);渲染面 d_cmd_x100 断言镜像。
  scroll/overlay/菜单锚随镜像自洽(v1 不做独立 rtl 变体夹具,统一走镜像)。
- 验收:镜像坐标断言(嵌套 hbox 双层)、文字 bidi 与布局镜像叠加、ltr 零回归
  (黄金 s21 字节/度量恒等)。
- 夹具槽 s88_rtl。裁决点:根级整体 vs 元素级继承——推荐**元素级+继承**(局部
  混排场景,根级=其特例)。

### GUI-36 触摸/手势(引擎中缝,波四,2 天)

- 目标:触摸事件族与基础手势(tap/longpress/swipe;pinch v2)。
- 落点:事件面=触摸码族(raylib GetTouch*/GetGesture* 拉模式✓)+合成规则
  (桌面单触=鼠标既有路径零分叉);手势状态机引擎侧(阈值可配);新事件
  on:tap/on:longpress/on:swipe(检查面收编+方向参数);headless d_gesture 注入探针。
- 验收:手势识别四径断言(阈值边界)、触摸与鼠标共存、既有 click/dblclick 零回归。
- 夹具槽 s89_touch。裁决点:目标平台 v1=桌面触屏(开发机可验),移动端(M3)
  与 IME M3 同窗评审(§4 决策 3)。

### GUI-35 无障碍 v1(架构级·平台 shim,波四,3+ 天)

- 目标:语义标注面全平台落 + macOS 屏幕阅读器播报 v1(shim 先行一家)。
- 落点:①语义面(检查面+运行时):aria_role/aria_label 属性收编入 GuiTree 槽,
  缺省 role 由 tag 推(button/input/label…);headless d_a11y_dump() 树投影探针
  (语义树可断言,不依赖平台)。②平台 shim:mac NSAccessibility(子树投影+焦点
  跟随播报;焦点环 rt_focus_next✓ 为导航序真源);Win UIA/Linux AT-SPI 另立案。
- 验收:d_a11y_dump 全量断言(role/label/focus 链);真窗 VoiceOver 手验记录
  (可达性=播报+焦点导航,非全操作 v1)。
- 夹具槽 s90_a11y。裁决点:平台范围与深度(§4 决策 3)。

### GUI-32 多窗口 v1(架构级·压轴,波四,3–5 天 + 前置评审)

- 目标:应用级多窗口(浮板/检查器/多文档)。
- 形态(裁决 1 定向后的推荐路):**v1=引擎级虚拟多窗**(单 OS 宿主窗):窗口管理器
  (每窗独立 GuiTree+rects+overlay 栈+焦点;自绘标题栏/拖拽移动/缩放/z 序;
  事件路由=焦点窗优先),raylib/Clay 全局态零波及(v1 不换底层)。

  ```ct
  var win: I32 = gui_window_open("检查器", 320, 480, inspector_view)
  gui_window_title(win, "检查器 · 文档 2")
  gui_window_close(win)
  ```

- 落点:gui.ct 窗口管理器段(最大单件)+ 驱动器扩展(d_win_probe);
  **OS 级多窗=窗口层换装(SDL3,spec §3.2 预注册替换路径)另案评审**——牵
  native.sh 链接面/三平台矩阵,不在 v1。
- 验收:headless 双窗事件路由断言(焦点切换/各自 overlay/关闭语义)、z 序与
  拖拽;真窗 --run 手验(与 OS 窗的观感差距记录在案,交裁决 1 复核)。
- 夹具槽 s91_win。裁决点:见 §4 决策 1(本件执行前置门)。

## 4. 裁决点汇总(执行前或执行中需用户裁决)

| # | 件 | 问题 | 选项 | 推荐 |
|---|---|---|---|---|
| 1 | GUI-32 | 多窗形态 | A 虚拟多窗 v1,OS 级另案评审 / B 直接窗口层换装 SDL3 / C 只做虚拟不做 OS 级 | **A**(不动底层先解锁应用形态;OS 级牵三平台另评) |
| 2 | GUI-33 | 内联标记语法 | A markdown 子集 / B 自定义尖括号 tag | **A**(无嵌套尖括号转义冲突,用户熟悉;`*` 逃逸形态随件定) |
| 3 | GUI-35/36 | 平台范围 | A mac shim+桌面触屏先行,Win/Linux/移动另案 / B 三平台齐上 | **A**(开发机可验;语义面全平台落,shim 分步) |
| 4 | GUI-41 | 线图 z 序 | A v1 接受 overlay(canvas 盖 UI),树内 canvas 缝另案 / B 本件立案 Clay CUSTOM 贯通 | **A**(条图树内零缝已覆盖多数场景;CUSTOM 贯通=G 档尾独立件) |
| 5 | GUI-42 | 声明式 transition 属性(引擎代插值) | A 不做,库形态终结 / B 另案立案 | **B 另案**(库先行收集真实需求,避免引擎面过早固化;不阻塞本件) |
| 6 | GUI-44 | 上屏通道 | A C 胶水投递队列(零编译泳道波及,推荐)/ B 等 try_recv 语言缝 | **A**(try_recv 另案编译泳道,泛化价值保留) |
| 7 | 组件落点 | date/color/palette/chart/anim 放哪 | A pkgs/gui 门面内(消费 use gui.{...} 不变)/ B 独立 pkgs | **A**(GUI 域能力非独立域;独立域留给真分工需求,分层宪章四问再启) |

## 5. 风险与在册交互

- **基线现役债两笔不归本档**(s30_props_d 野指针、numeric×textarea emit segfault):
  波次执行中全量回归遇红按三级对照流程归因(基线 worktree 复现即移交原册),不堵塞。
- **发射泳道在飞三文件**(driver_emit/trans_conc/trans_expr):本档波一/波二全部
  零编译面波及(新事件名 on:dropin 等走 brace 形直通,GUI-20 on:drop 先例;引号
  串形态才受持检查面);若裁决 6 转 B 路或 try_recv 立案,须与发射泳道协调静默窗。
- **文本排版深水面**(波三):s21 黄金度量/s51 ft flush/s75 maxlines 三处敏感,
  GUI-33/34 执行时黄金对照先行、分臂落、逐臂回归。
- **坑位引用**(写码前必读):无三元(if 表达式)/无位运算(算术等价)/F64 双宿主
  坑位/emit 直发 20480B 上限(corpus+chk 替代)/read_file 双口径(seed=Option,
  发射=Str)/字符串裸 `{` 须 `\{`;夹具号实测再占(s79–s91 为建议槽)。
- **规模口径**:以上"天"沿路线图惯例(单件 0.5–2 天档),架构级件按 3–5 天;
  机刷并行可显著压缩,不作为承诺口径。
