# GUI 组件库波次二实施计划:input 真文本编辑(§2.2)

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans。Steps 用 checkbox 跟踪。
> 规格:`docs/superpowers/specs/2026-09-24-gui-widgets-design.md` §2.2;上游波次一已落库(4b9443a)。

**Goal:** input 升级真文本框——焦点模型(单焦点/点击转移/Esc 失焦)、编辑键(字符/退格/删除/←→/Home/End/Ctrl+V 粘贴)、回显进 input 自身(焦点光标 `|` 前缀插入)、on:input「名:载荷」/on:submit 事件、placeholder 灰显;mirror label 模式退役。

**Architecture:** 文本零 C 侧存储——编辑时经 bind 通道活问当前值(bind=模型单一起源),Ctron 侧拼装新文,`act(头名 + ":" + 新文)` 送达模型,下帧 bind 回读渲染;C 侧只存**光标 I32/焦点节点 I32/剪贴板 char[]/修饰键旗**四个原语。渲染:焦点时 `prefix + "|" + suffix` 单 TEXT 命令(光标字符形态,accent 色随 P2);未聚焦空值渲 placeholder(TEXT_MUTED 槽 6)。

**Tech Stack:** gui.ct(渲染分支/编辑管线/窗口循环接线)+ ctron_gui.c 四原语组 + tests/gui/s33_focus 新夹具。

## Global Constraints

- pathspec 提交;动工前 git 重对齐(pb/OTLP 泳道在飞)。
- 门禁:阶梯(33 过/4 败存量)+ s33 新绿 + tests/net 不回归;FT_OFF 钉值口径不变。
- 值单一起源=模型:on:input 缺席的 input 编辑不持久(下帧 bind 回读即还原)——规格钉值,非缺陷。
- 退格/删除按**字节级**(沿 s12 口径;码点级随 M3 IME 批次)。
- s12(自包含老形态)不触域包,零迁移;s19/todo 有 mirror label 且无 cmd 计数断言,预期零迁移(回归确认)。
- 夹具号:s33_focus(号段注记 §6)。

---

### Task 1: C 原语 + 驱动器注入面

**Files:**
- Modify: `gui/c_src/ctron_gui.c`(尾部增量:g_focus_id/g_caret/g_clip[4096]/g_mod_ctrl 四组 + gui_focus_set/get、gui_caret_set/get、gui_clip_set_c(Str 入)/gui_clip_len/gui_clip_byte、gui_inject_mod/gui_mod_ctrl(IsKeyDown 回落))
- Modify: `gui.ct`(extern 声明 + `d_focus(t)`(聚焦首个 input)/`d_blur(t)`/`d_clip(t, s)`/`d_mod(t, on)`)

**Steps:**
- [ ] C 四组原语(净增量)
- [ ] gui.ct extern + 四个 d_* 驱动器
- [ ] 编译冒烟(既有夹具回归抽测 s31)
- [ ] 提交

### Task 2: input 渲染分支 + click-to-focus

**Files:**
- Modify: `gui.ct`(rt_emit:label 与容器分支之间插 `tag == "input"` 分支——盒(gui_cfg,h/bg 走 gt_st_prop)+ 值文本(nbid 走 bind,简单名/表达式两口径)+ 焦点 `|` 光标(caret 钳制 len=外部改文钉值)+ placeholder 灰显(未聚焦空值,TEXT_MUTED 槽 6)+ `hits.push(id)`;两窗口循环 click 分支:命中节点 tag=="input" → gui_focus_set(node),否则 -1)

**Interfaces:**
- Produces: 焦点语义 = gui_focus_node() == id;input 可点击(hits 在册,click-to-focus 复用 rt_hover_node)
- Produces: `gui_focus_node()/gui_caret_get()` 渲染期读面(T3 编辑管线共用)

**Steps:**
- [ ] rt_emit input 分支(占位渲染:值+光标+placeholder)
- [ ] click-to-focus 接线(两循环)
- [ ] s31/s19/todo 回归(s19 预期:input 值文本+mirror 双显,d_expect_text 仍绿)
- [ ] 提交

### Task 3: 编辑管线 + 事件「名:载荷」

**Files:**
- Modify: `gui.ct`(`rt_input_char(t, bind, act, ch) -> Bool`:焦点校验→bind 活问当前值→utf8_enc 插入→caret 前移→fire;`rt_input_key(t, bind, act, k) -> Bool`:259/261/262/263/268/269 编辑、257→on:submit、256→失焦、Ctrl+86→粘贴(gui_clip 同步+字节装配)、结构编辑后 fire;`rt_ev_payload(t, node, evname, payload, act)`:ev 表扫描,ev_head + ":" + payload 直发;两窗口循环 evt3/evt1 分支:焦点在 input 时优先编辑管线,未消费回落 key 闭包)

**Interfaces:**
- Consumes: Task 1 原语、Task 2 焦点读面
- Produces: 「名:载荷」事件通道(规格 §2.2;select/list P1 复用)
- Produces: 键消费边界 = 焦点内编辑键运行时吃掉不进 key 闭包(s33 夹具锁)

**Steps:**
- [ ] 三函数 + 两循环接线
- [ ] s31/s19/todo 回归(s19 无 on:input:编辑不持久口径确认,键闭包路径原样)
- [ ] 提交

### Task 4: s33_focus 夹具 + 阶梯注册

**Files:**
- Create: `tests/gui/s33_focus/`(run.sh 仿 s31 + src:placeholder 初态→d_focus→键入回显(input 自身 TEXT,无 mirror)→退格/删除/←→/Home/End→d_clip+d_mod+Ctrl+V 粘贴→on:input 载荷断言(bind 侧记账)→on:submit→Esc 失焦→编辑键回落 key 闭包断言)
- Modify: `tests/gui/run.sh`(追加 s33_focus)

**Steps:**
- [ ] 夹具四态+编辑全链断言
- [ ] 阶梯注册全绿
- [ ] 提交

### Task 5: 收尾

**Steps:**
- [ ] README:input 真文本框用法(on:input 载荷拆分首个 `:`)/编辑键表/d_focus 面/已知限制(无选区/字节级退格/超宽截断)
- [ ] 规格 §4.1 已知限制勾选更新(input 行)
- [ ] 门禁:阶梯+net;记忆更新
- [ ] 提交

## Self-Review

- 规格覆盖:§2.2 全项(焦点模型/编辑键/剪贴板粘贴/回显/事件/placeholder/钉值)有落点;IME/选区/undo 为 §4.1 在册限制,不在本波次。
- 类型一致性:四原语名与 extern/d_* 一一对应;rt_ev_payload 复用 ev_head(波次一 d_btn_index 教训:节点 id vs btns 下标已区分)。
- 占位符:无;fire 无 on:input 时的不持久语义为规格钉值非空泛。
