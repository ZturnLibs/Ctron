# GUI 组件库波次五b实施计划:tick 原语 + 尺寸约束 + 快捷键表(§2.7/§2.10/§2.11)

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans。Steps 用 checkbox 跟踪。
> 规格:`docs/superpowers/specs/2026-09-24-gui-widgets-design.md`;上游已落库至 47a587f。字体字重(§2.8)为最大 C 手术件,留 5c。

**Goal:** 三条零依赖能力缝——①tick 原语(运行时毫秒钟+帧计数+`d_tick` 注入;可见消费者=光标闪烁,仅注入时钟时激活,真窗常亮至 P2 转正——确定性口径);②尺寸约束(min_w/max_w/min_h/max_h → gui_cfg3 packed minMax,Clay 能力透出);③应用快捷键表(C 侧注册表+双循环 match+「名」直发,键闭包回落序不变)。

**Tech Stack:** gui.ct + ctron_gui.c + s36_tick/s37_size/s38_hotkey 三夹具(read_file 形态)。

## Global Constraints

- pathspec 提交;动工前 git 重对齐。门禁:阶梯 36/4 存量 + 三新夹具绿 + net。
- 确定性:tick 仅注入时钟时驱动可见行为(真窗 GetTime 路径 P2 转正);快捷键 match 在编辑管线与模态之后(事件路由优先级链 §2.11 钉死:焦点编辑键 > 模态 Esc/吞 > 快捷键表 > key 闭包)。
- 编译器面禁触(read_file 形态夹具)。

---

### Task 1: tick 原语 + 光标闪烁(注入激活)

C: g_inject_ms(-1 缺省)/gui_inject_ms/gui_now_ms(注入优先,回落 GetTime*1000)/g_frame_n/gui_frame_tick(每 rt_draw_frame ++);gui.ct: extern+d_tick(t,ms)+d_frames();input 渲染分支:注入时钟激活时 (ms/500)%2 控光标相位。
**夹具 s36_tick**:d_focus→键入 "hi"→注入 0→帧含 "hi|";注入 600→帧无 "|";注入 1200→复现;d_frames() 跨帧递增。+阶梯注册。

### Task 2: 尺寸约束(§2.10)

C: gui_cfg3(13 参全 I32:gui_cfg 全参+wminmax/hminmax packed min*10000+max;wmode=3 → minMax)。gui.ct 容器分支:min_w/max_w/min_h/max_h 四样式在则改走 gui_cfg3(wmode=3)。
**夹具 s37_size**:bg 容器 grow+max_w 80 → w100==8000;grow+min_h 60 → h100==6000;无约束容器不受扰(回归段)。+阶梯注册。

### Task 3: 快捷键表(§2.11)

C: 16 条目表{mods,key,action[64]}+gui_hotkey_set(combo 编码 mods 位+键码, action)+gui_hotkey_match(key,mods)->len(-1 未中,字节读 gui_hotkey_action_byte)。gui.ct: extern+`gui_hotkey(combo: Str, action: Str)` 解析("mod+s"/"ctrl+shift+k" 词法)+d_hotkey 装配+双循环 evt1 分支:编辑管线/模态之后、key 闭包之前 match→act(action 字节装配);d_send_key 镜像。
**夹具 s38_hotkey**:注册 mod+s→save;d_mod+d_send_key(83)→state="saved";无修饰 83→回落 key 闭包;未注册组合不受扰。+阶梯注册。

### Task 4: 收尾

README(三缝用法+确定性口径);规格 §2.7/§2.10/§2.11 落地注记;记忆;门禁(net)。

## Self-Review

- 三缝均有可见断言消费者(闪烁相位/maxw 几何/save 记账);事件路由优先级链不被破坏(s33/s34 回归红线)。
- 键闭包签名不动(修饰键经 extern 读,规格载荷编码备选不采用——零破坏)。
