# GUI 组件库波次三实施计划:overlay 浮层 + 模态语义 + 8 位色(§2.3)

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans。Steps 用 checkbox 跟踪。
> 规格:`docs/superpowers/specs/2026-09-24-gui-widgets-design.md` §2.3 + §7 色解析项;上游波次一/二已落库(e95ab70)。

**Goal:** `<overlay>` 能力性容器(Clay floating 全屏附着、z 序声明序、遮罩吞穿透)、「名:close」模态键盘语义(键闭包挂起+Esc 转 on:close,自锁修复)、`#RRGGBBAA` 8 位色解析(C1 修订:6 位合法 alpha=FF);s34_overlay 夹具。dialog 库级组件随 SL-8c-4(波次四)。

**Architecture:** overlay = 新标签白名单 + `gui_floating` C 面(floating attach PARENT/zIndex=1/全屏 grow/childAlignment center 可选/x,y 偏移);命中语义翻转为**后向最优先**(rt_hit_name 从后向前,顶层矩形先中——遮罩吃穿透与卡片不透全靠绘制序,零新机制);键盘模态 = 窗口循环扫描末帧 hits 检出 overlay → 编辑管线后:Esc 转该 overlay on:click、余键吞、键闭包挂起。8 位色 = `g_parse_alpha`(9 位长判别)+ C 侧 pending_alpha(gui_alpha 置位、gui_cfg 消费即复位)——零 ABI 变更。

**Tech Stack:** gui.ct + ctron_gui.c + tests/gui/s34_overlay(read_file 形态,零编译器触碰)。

## Global Constraints

- pathspec 提交;动工前 git 重对齐(driver_emit/net 在飞——**编译器面本波次禁触**:overlay 编译侧白名单(gui_ck/sk builder)登记缓行,embedded 形态 dialog 俟其落库)。
- 门禁:阶梯 34/4 存量 + s34 新绿 + tests/net;FT_OFF 口径不变。
- 颜色:6 位 `#RRGGBB` 行为不变(alpha=FF);8 位仅经 `g_parse_alpha` 通道生效。
- 命中翻转回归红线:s13(行级按钮)/s28(命中注册表)/s33(编辑全链)必须全绿。

---

### Task 1: 8 位色通道(§7)

**Files:**
- Modify: `gui.ct`(`g_parse_alpha(v) -> I32`:len==9 取 7,8 十六位,否则 255;extern gui_alpha/gui_cmd_bg_a)
- Modify: `gui/c_src/ctron_gui.c`(g_pending_alpha + gui_alpha(int) + gui_cfg_impl 消费即复位 + gui_cmd_bg_a 读面)
- Modify: `gui.ct` 驱动器(d_cmd_bg_a)

**Steps:**
- [ ] 三件落地;s31 回归(6 位零变化)
- [ ] 提交

### Task 2: overlay 容器(§2.3)

**Files:**
- Modify: `gui.ct`(运行时白名单 +1140 区;rt_emit overlay 分支:gui_floating 全屏+children 常规递归;`align: center` 样式 → childAlignment;x/y 偏移样式;hits/hinst 在册)
- Modify: `gui/c_src/ctron_gui.c`(gui_floating 14 参全 I32:floating config attach PARENT/zIndex 1/offset;layout 全屏 grow;alignc → childAlignment center)
- Modify: `gui.ct`(rt_hit_name 翻转后向最优先——顶层矩形先中,遮罩吃穿透)

**Steps:**
- [ ] 三件落地;s13/s28/s33 命中回归红线
- [ ] 提交

### Task 3: 键盘模态(自锁修复)

**Files:**
- Modify: `gui.ct`(两窗口循环:末帧 hits 扫描检出 overlay → 编辑管线之后:Esc(256) 转该 overlay 的 on:click 处理器、余键吞;键闭包挂起;无 overlay 原路径)

**Steps:**
- [ ] 接线 + s31/s33 回归(无 overlay 路径零变化)
- [ ] 提交

### Task 4: s34_overlay 夹具

**Files:**
- Create: `tests/gui/s34_overlay/`(read_file app.ctml 形态;run.sh 仿 s33)
- Modify: `tests/gui/run.sh`(追加)

**断言面:**
- overlay 显隐(when 通道)、居中卡片几何(x100 口径)、遮罩 8 位色 alpha(gui_cmd_bg_a==0x99)、
  遮罩点击→on:close(d_click 按名)、卡片点击不透(遮罩回调不触发——点击名分派面)、
  键盘模态(d_send_key Esc→关闭;开启期键闭包挂起——记账标签不变)、z 叠序(双层 overlay 后者在上——d_cmd 序断言)

**Steps:**
- [ ] 夹具+注册+全绿
- [ ] 提交

### Task 5: 收尾

**Steps:**
- [ ] README:overlay 用法/模态语义/8 位色;规格 §7 色解析项销账注记
- [ ] net 门禁;记忆更新
- [ ] 提交

## Self-Review

- 规格覆盖:§2.3 全项(浮层/居中/偏移/when 显隐/z 序/遮罩模态/键盘模态+Esc 自锁)+§7 色解析;dialog 组件本体归 SL-8c-4(波次四),规格依赖登记一致。
- 编译器触碰:零(白名单登记缓行;read_file 形态夹具绕开 gui_ck/sk 面)。
- 命中翻转风险:回归红线三夹具在列。
