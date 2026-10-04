# GUI P2/P3 后续任务清单

> **For the next agent:** 本文件 = GUI 组件库战役收官后全部剩余任务的移交清单。
> 基线 = 阶梯 54/0 全绿（bc70d3b+e8238b3 后）、SL-8c 全片收官、Todo v10 终验通过。
> 每件标注[依赖面/预估量/验收方式]，按优先级排序；完成一件删一行。

## 泳道上下文（30 秒速读）

- **域包**:gui.ct(5300+ 行,单文件门面)+ gui/c_src/{ctron_gui.c, ft_shim.c, ime_shim.m}+ gui/parse.ct(死副本勿动)+ gui/theme.ct(文档镜像)
- **组件消费**:`view Comp (prop: Type) { ... }` 同源声明 + `<Comp prop: expr/>` 实例(大写首字母);props env 末位优先;列表 props 经 `each:名` bind 通道透传
- **规格**:docs/superpowers/specs/2026-09-24-gui-widgets-design.md(十裁决/十缝/路线图)
- **阶梯**:sh tests/gui/run.sh(54 夹具;s31–s48 为组件库系);门禁=阶梯+net 14/14+净树 smoke
- **坑位全集**:gui/README.md 坑位章 + 记忆 ctron-gui-lane-state.md + ctron-compile-driver-gotchas.md

---

## P2 待做（按优先级排序）

### 1. 选区模型（input/textarea 文本选择）
- **依赖**:焦点模型(已有)+渲染光标(已有)
- **设计**:selection_start/selection_end C 侧 I32 对;Shift+←→/Home/End 扩选;双击选词(P3);选区高亮=渲染拆三段(前/选中反色/后);Ctrl+C 复制选中(消费既有剪贴板)
- **量**:~200 行 gui.ct + ~30 行 C;中件
- **验收**:s_fixture 选区高亮色断言+Ctrl+C 剪贴板内容断言

### 2. undo/redo（input/textarea）
- **依赖**:编辑管线(已有);无选区时退格删尾即可
- **设计**:操作栈(编辑前 snapshot 值+caret,50 深度);Ctrl+Z undo / Ctrl+Shift+Z redo;model 单一起源不变(undo 恢复 snapshot→fire on:input)
- **量**:~80 行 gui.ct;小件
- **验收**:s_fixture 编辑→undo→redo 断言值复原

### 3. dialog 内容投影（slot 机制）
- **依赖**:ViewCall 运行时展开(已有)——需扩展支持子节点透传
- **设计**:ViewCall 解析分支:非自闭合时收集子树 id 范围,存入节点自有槽;展开期将子树拼入组件体 slot 标记位(组件体 `<slot/>` 占位)
- **量**:解析+渲染各 ~30 行;中件;**语言面设计**(slot 语义=编译面 co-design)
- **验收**:Dialog 内嵌自定义操作按钮组

### 4. tree（树形视图）
- **依赖**:view 递归(已有);WList 先例
- **设计**:嵌套 view 递归展开(node children 属性);缩进=depth×SPACE_LG;折叠=when 门+展开态 prop
- **量**:~60 行组件;小件
- **验收**:三层树渲染+折叠/展开切换断言

### 5. combobox（input+Select 合体）
- **依赖**:input(真文本)+Select(下拉)+焦点环(已有)
- **设计**:input 聚焦时按值过滤 options→when 展示匹配项→点击/Enter 选中回填
- **量**:~50 行组件;小件
- **验收**:输入过滤+选项选中回填 input

### 6. grid 布局助手
- **前置**:核查 Clay grid 支持面(Clay_LayoutConfig 是否有 grid 方向)
- **设计**:若无原生 grid,hbox×each 行列组合即可模拟
- **量**:~40 行;小件
- **验收**:3×3 网格几何断言

### 7. 右键菜单
- **依赖**:§2.1 button-2 事件缝(设计在册,未实现)+overlay(已有)
- **设计**:rt_hit_name 扩展 button-2;菜单=overlay+行按钮
- **量**:事件缝 ~20 行+菜单组件 ~40 行;中件
- **验收**:右键→菜单可见→点行→菜单关+动作触发

### 8. 图标（消费图像管线）
- **依赖**:image 元素(已有)
- **设计**:图标=小尺寸 image+SVG 栅格缓存(v1 用 Unicode 符号字符代,SVG 后续)
- **量**:~20 行组件;小件
- **验收**:图标渲染+尺寸缩放

### 9. 滚动条视觉
- **依赖**:scroll 容器(已有)
- **设计**:滚动容器右侧细条=可拖矩形,位置/大小=滚动偏移/内容比
- **量**:~40 行;小件;需拖拽事件缝(§5 P2)
- **验收**:滚动位置映射+拖动

### 10. 程序化 focus()/scroll_into_view()
- **依赖**:焦点原语(已有 gui_focus_set)
- **设计**:pub fn focus(id: I32)+scroll_into_view(id)(滚动偏移计算= Clay_GetScrollOffset)
- **量**:~20 行;小件
- **验收**:程序化聚焦/滚动断言

### 11. 用户级 on:after 定时事件
- **依赖**:tick 原语(已有)
- **设计**:事件路由链尾+帧计数比对→on:after 触发
- **量**:~30 行;小件
- **验收**:定时触发断言

### 12. 窗口图标/无边框
- **依赖**:raylib SetWindowIcon/SetWindowState
- **设计**:入口 API 扩展
- **量**:~20 行 C+10 行 gui.ct;小件
- **验收**:真窗手动

### 13. 字体族命名映射
- **依赖**:族注册(已有,数字 id)
- **设计**:gui_font_named(name: Str, path: Str)→C 侧名称表;font_family 样式可写族名(gtok 风格查找)
- **量**:~40 行;小件
- **验收**:命名注册+样式引用

### 14. 字体回退链 P2 增强
- **现状**:渲染/测量缺字遍历注册族面(已落库)
- **增强**:内建 k_fonts 多候选互备(当前只试首中)+ emoji 检测(非 BMP 码点→特定回退)
- **量**:~30 行;小件

### 15. Tab 焦点环增强
- **现状**:input+textarea 树序环进/环退(已落库)
- **增强**:button/checkbox/select 触发器也入环(可聚焦元素全集);焦点环可视化(焦点环外框样式)
- **量**:~30 行;小件

---

## P3 远期方向

- **组件跨文件分发** ✅ 销账(2026-10-04,40b08df4/1580a606/37fcc25e/c1873a49):装载器三能力(门面 re-export 门后移/消费方引用闭包/嵌套 wholesale+redo 隔离)解锁多文件域包;组件库已独立 pkgs/gui_widgets(16 组件规范形态,gui_snippets 消费中);陈列室消费缓行两缺口(手写 bind×类型化列表 props/键闭包×desugar 形态)见拆分规划执行状态节
- **多窗口**:Clay 多上下文或换窗口库——架构级决策
- **动画/过渡**:插值消费 tick 原语
- **RTL 布局镜像**:文字 bidi 已有,布局镜像待做
- **无障碍树**:Clay/raylib 面内无基础设施,需自建
- **触摸/手势**:桌面定位暂缓
- **富文本 inline markup**:label 多样式运行
- **date/time picker / color picker / command palette / chart**:组合组件

---

## 已落库完成项（不重做）

以下 P2 项已全部实现并有夹具覆盖——**勿重复开发**:
- ✅ 光标闪烁转正(934ea65)
- ✅ 光标形状 I-beam/手型(934ea65)
- ✅ Tab 焦点环(99982c6)
- ✅ 多行 textarea(747c91c)
- ✅ 高对比无障碍主题(05186db)
- ✅ 字体族注册(c3c8080)
- ✅ 字体回退链(5d15afb)
- ✅ 尺寸约束 min_w/max_w/min_h/max_h(99982c6 前)
- ✅ 快捷键表(99982c6 前)
- ✅ tick 原语+d_tick(99982c6 前)
- ✅ 8 位色 #RRGGBBAA(ad41065)
- ✅ image 元素(329bb89)
- ✅ IME 组词(bc42f1b peer)

---

## 事件路由优先级链（写代码前必背）

```
焦点元素编辑键 > overlay 顶层 dialog Esc > 应用快捷键表 > key 闭包
```

## 组件事件头合约（写组件前必背）

```
sel_toggle / sel_pick / list_pick / dialog_dismiss / dialog_open / new_snip / save_snip / del_ask / set_content / set_draft
```
组件事件头=合约,应用侧按头实现 fn 即接;列表 props 经同名 `each:名` bind 通道透传。
