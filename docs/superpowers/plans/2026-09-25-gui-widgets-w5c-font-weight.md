# GUI 组件库波次五c实施计划:字体字重(§2.8;十缝收官件)

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans。Steps 用 checkbox 跟踪。
> 规格:`docs/superpowers/specs/2026-09-24-gui-widgets-design.md` §2.8;上游已落库至 cc6bd53/act v2 merge。

**Goal:** `font_weight` 样式属性(400/700)——ft_shim 合成加粗(FT_Outline_Embolden,免粗体字体文件)+advance 增量进测量与渲染(测量==渲染不变式)+纹理缓存键三元化 (串,px,weight)+gui_text_w 影子字重表(flush TEXT 序号并行)+s41_font 夹具。

**Architecture:** 不加载粗体字面——weight≥600 时 FT_Load_Render 后对 outline 做 Embolden(strength=px/16),advance 增量 px/24 同加于 measure_n(测量==渲染锁死);缓存槽加 weight 字段,查找三元相等;Ctron 折叠侧 weight≠400 走 gui_text_w(7 参),ctron_gui.c 以 g_text_weights[] 按 TEXT 产出序记录,flush 读回——Clay TextElementConfig 无自定义槽的绕行(非 hack:序号即命令面事实)。

**Tech Stack:** ft_shim.c + ctron_gui.c + gui.ct + tests/gui/s41_font(read_file 形态)。

## Global Constraints

- pathspec 提交;动工前 git 重对齐(peer 编译器四文件在飞)。
- s41 依赖 FT 实测(宽差断言),run.sh 不设 CTRON_GUI_FT_OFF;黄金系夹具不受扰(无 font_weight 使用)。
- 回退:无字体环境(heuristic)bold 宽度增量不加——FT_OFF 下 bold≡400 宽,登记已知限制。
- 真窗 flush 口径 :materialize;headless 断言面 = TEXT 命令几何差。

---

### Task 1: ft_shim weight 化

- `ft_render_n_w(utf8,len,r,g,b,weight)`(weight≥600:FT_Outline_Embolden(strength=px/16)+advance 增量 px/24);`ft_measure_n_w` 同增量;`gui_ft_text_w(s,len,px,weight)` 缓存三元化(槽加 weight 字段);旧 `gui_ft_text`=weight 400 零破坏。
- Task 2 接通后一起验证。

### Task 2: ctron_gui.c gui_text_w + flush 字重表

- `gui_text_w(s,size,weight,r,g,b,a)`(g_text_weights 追加)+`gui_begin_layout` 复位计数;flush TEXT 分支按 TEXT 序号取 weight 传 gui_ft_text_w。
- **夹具 s41_font**:双 label 同文("width")weight 400/700 → TEXT 命令宽差(w700>w100 断言,跨平台安全)+len 相等+400 基线不受扰;阶梯注册。

### Task 3: gui.ct font_weight + 收尾

- 容器/label/button/input 渲染分支:font_weight 样式读(缺省 400)→≠400 走 gui_text_w;README/规格 §2.8 落地注记(合成加粗口径/FT_OFF 限制);记忆;net 门禁。

## Self-Review

- 规格覆盖:§2.8 weight 面(400/700)全项;font-family 用户字面注册/回退链=登记 P2(规格原文「内置默认族;用户自带 TTF 注册面 P2」)。
- 风险:Embolden 后 bitmap 变宽可能溢出 g_bw(测宽未含增量)——ft_render_n_w 测宽轮同加增量,先保证缓冲尺寸。
