# GUI 组件库波次一实施计划:主题体系 + 交互态管线

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development(既定模式)或 superpowers:executing-plans。Steps 用 checkbox 跟踪。
> 规格:`docs/superpowers/specs/2026-09-24-gui-widgets-design.md`(六轮审阅,裁决 #1–#10)。

**Goal:** 落地规格 §3 主题体系(Theme struct/theme_apply/theme_auto/八主题/令牌解析)+ §2.1 交互态管线(hover/active/disabled 视觉折叠)+ 驱动器颜色面——为后续 input 焦点/overlay/组件波次铺地基。

**Architecture:** 令牌表 = C 侧 I32 槽数组(22 槽,色=packed/尺寸=像素,全 I32 跨 extern 边界,gui_inject_key 先例);theme_apply 逐槽写入,解析缓存挂 GuiTree(代际失效);裸词令牌解析落点 = `g_parse_color`(gui.ct:289 已有「未知颜色令牌」panic 分支)+ 尺寸路径 gs2i;交互态 = 复用 P-H1 命中注册表矩形求交(C poll)+ `d_hover` 注入,折叠在 class 属性取值处按态回退。

**Tech Stack:** Ctron 域包(gui.ct/parse.ct/theme.ct/gui/themes/*.ct)+ gui/c_src/ctron_gui.c 增量 + tests/gui 阶梯夹具。

## Global Constraints

- **pathspec 提交纪律**(机刷泳道在飞):每步 `git commit -- <files>` 限定;动工前 `git log --oneline -3` 重对齐。
- **门禁采数**:`sh tests/gui/run.sh` 全绿 + `sh ci.sh 8` / `sh ci.sh 9` + `tests/net` 不回归;黄金/几何断言一律 `CTRON_GUI_FT_OFF=1` 口径(跨平台钉值)。
- **emit bin**:夹具用 `compiler/bin/ctron-emit` + `CTRON_STDPATH=$ROOT/std`(域包三级解析);域包链接 = ctron_gui.c + ft_shim.c + libfreetype.a + libraylib.a(仿 s27 run.sh)。
- **显式请求契约**:跨包 use 面缺什么发射面缺什么(含 struct)。
- **零 SL-8c 依赖**:本波次不使用 view+props;组件波次(四)另立计划等 SL-8c 落库。
- **夹具号段**:阶梯 s29_ev_expr_d/s30_props_d 已被 SL-8c 占用,本波次从 **s31** 起(规格 §6 号段同步对齐,见 Task 1)。
- 颜色口径:`#RRGGBB` 6 位不变;`#RRGGBBAA` 8 位扩展属规格 §7 登记项,**不在本波次**(随 overlay 波次 MASK 需求一并做)。

## 波次地图(规格切片 → 计划)

| 波次 | 内容 | 计划 |
|---|---|---|
| **一(本计划)** | §3 主题全链 + §2.1 交互态 + 颜色面/令牌解析(§2.4)/平台明暗(§2.5) | 本文 |
| 二 | §2.2 文本焦点编辑 + input 升级 + 剪贴板粘贴 + 夹具迁移(s12/s19/todo) | 另立 |
| 三 | §2.3 overlay + dialog + `#RRGGBBAA` 色解析 | 另立 |
| 四 | select/list 组件(等 SL-8c)+ 陈列室示例 + 组件×each 探针 | 另立 |
| 五 | §2.6 图像 / §2.7 定时器 / §2.8 字体字重 / §2.10 尺寸透出 / §2.11 快捷键 | 另立 |

---

### Task 1: 驱动器颜色面 + 规格号段对齐

**Files:**
- Modify: `gui.ct`(驱动器区 ~2089:extern 声明 + `d_cmd_bg_r/g/b` 三封装)
- Modify: `docs/superpowers/specs/2026-09-24-gui-widgets-design.md`(§6 夹具号段 s29–s37 → s31–s39 映射注记)
- Create: `tests/gui/s31_state/`(run.sh 仿 s27 + src/main.ct 骨架:静态 bg 断言)
- Modify: `tests/gui/run.sh`(阶梯列表追加 s31_state)

**Interfaces:**
- Produces: `d_cmd_bg_r(i: I32) -> I32` / `d_cmd_bg_g` / `d_cmd_bg_b`(读 Clay RECT 命令背景色 0–255;C 侧 `gui_cmd_bg_r/g/b` 已在,仅补 gui.ct extern 与封装)——s31/s32 及后续 hover 断言全部消费。

**Steps:**
- [ ] gui.ct 驱动器区加 `extern "c" fn gui_cmd_bg_r(i: I32) -> I32`(g/b 同)与三封装 `pub fn d_cmd_bg_r(t: Box[Driver], i: I32) -> I32`(签名比照 d_cmd_x100,~2106)
- [ ] s31_state/src/main.ct 骨架:`test()` 内 button 元素渲染后 `d_cmd_count` 扫 RECT,断言 bg_r/g/b = style 直写色值(先证颜色读回通路)
- [ ] s31_state/run.sh 仿 s27(ctron-emit + 四件链接 + 平台框架旗);`tests/gui/run.sh` 列表追加 s31_state
- [ ] 跑 `sh tests/gui/s31_state/run.sh` 绿 + 阶梯全绿;规格 §6 加号段映射注记(旧 s29_state→s31_state、s30_focus→s33_focus、s31_overlay→s34_overlay、s32_theme→s32_theme 不变段号恰空、s33_image→s35_image、s34_tick→s36_tick、s35 组件×each→s39、s36_font→s37_font、s37_hotkey→s38_hotkey)
- [ ] 提交(pathspec:gui.ct + s31 + run.sh + 规格)

### Task 2: 令牌槽 + 裸词令牌解析(§2.4)

**Files:**
- Modify: `gui/c_src/ctron_gui.c`(尾部增量:`int g_theme_slots[32]; int g_theme_gen = 0;` + `gui_theme_slot(int i,int v)`(写槽+gen++)+ `gui_theme_slot_get(int i)` + `gui_theme_gen(void)`;头部声明区同步)
- Modify: `gui.ct`(令牌名→槽位 const 表 22 项;`g_parse_color` 裸词分支:查槽→slot_get 直接返回 packed;尺寸路径 `gs2i` 调用处的裸词分支同法;GuiTree 加解析 memo 双平行表 + gen 失效)
- Modify: `gui/parse.ct`(GuiTree 定义加 memo 字段 + gt_new 初始化)
- Modify: `tests/gui/s31_state/`(扩:style 写 `bg: ACCENT` 断言折叠后 = "#2563eb" 直写的同值)

**Interfaces:**
- Produces: 槽位表(固定序,Task 3 theme_apply 逐槽写入、Task 5 主题文件全量消费)——槽 0..21 依序 = bg_base, surface, elevated, border, border_strong, text, text_muted, accent, accent_hover, danger, success, mask(色 12 槽,packed I32), radius_sm/md/lg, size_xs/sm/md/lg/xl, space_sm/md/lg, disabled_fg, disabled_bg(色 2 槽;共 22 槽,顺序即 Theme 字段序,规格 §3.1 表序)
- Produces: 表空时裸词令牌 panic「未知颜色令牌」(现行为保持,零回归)

**Steps:**
- [ ] ctron_gui.c 尾部三函数 + 槽数组(净增量,不动既有)
- [ ] gui.ct 令牌名 const 表(名→槽序)与 `tok_get(name) -> I32`(-1 = 非令牌);`g_parse_color`:# 分支不变,否则 `tok_get` 命中→`gui_theme_slot_get` 返回 packed,未命中走原 panic
- [ ] 尺寸令牌:bg/fg/border 之外的属性取值处(定位 gt_cls_prop 消费侧,执行时以 w:2527 系夹具锚定)加同形分支
- [ ] memo:GuiTree 加 `smemo_k: List[Str]`/`smemo_v: List[I32]`,折算前查 memo;`gui_theme_gen()` 变化即清(memo 双检:gen 存首项)
- [ ] s31 扩令牌断言(无 theme_apply 时令牌 = 缺省深色值,Task 3 落 theme_default 后回填此半);跑夹具绿
- [ ] 提交(pathspec 限定)

### Task 3: Theme struct + theme_apply + theme_default(§3.4)

**Files:**
- Modify: `gui.ct`(pub struct Theme 22 字段;`pub fn theme_apply(t: Theme)` 逐槽 extern 写入;`pub fn theme_default() -> Theme` 自 theme.ct 常量构造;theme.ct 注释同步「经 theme_default 消费」)
- Modify: `tests/gui/s31_state/`(令牌断言半截回填:apply theme_default 后 ACCENT = "#2563eb")

**Interfaces:**
- Produces: `pub struct Theme { var bg_base: Str … var disabled_bg: Str }`(色 14 字段 Str/#RRGGBB,尺寸 8 字段 I32,字段序 = 槽序 = 规格 §3.1)
- Produces: `theme_apply(t: Theme)`(色字段过 g_parse_color 折 packed,尺寸直写;每次调用 gen++,解析 memo 自然失效)——用户面:`use gui.{Theme, theme_apply}`,主题文件 Task 4 消费

**Steps:**
- [ ] Theme struct 22 字段(theme_default 构造字面量先在测试里编译过,锁完备性)
- [ ] theme_apply:14 色槽 + 8 尺寸槽逐条 extern 调用(无循环/无反射,平铺 22 行)
- [ ] theme_default():值 = theme.ct 常量;theme.ct 头注释改「缺省表构造源」
- [ ] s31:apply(default) 后令牌断言回填;跑绿
- [ ] 提交

### Task 4: 八主题文件 + s32_theme 夹具(§3.5)

**Files:**
- Create: `gui/themes/theme_dark.ct` / `theme_light.ct` / `theme_mac_dark.ct` / `theme_mac_light.ct` / `theme_win_dark.ct` / `theme_win_light.ct` / `theme_linux_dark.ct` / `theme_linux_light.ct`(各 `pub fn make() -> Theme` 全字段字面量;dark = theme_default 同值;light 纸白系;mac/win/linux 按规格 §3.5 基调,深浅对共享形状/间距语汇只换色板)
- Create: `tests/gui/s32_theme/`(run.sh + src:逐主题 `use gui.themes.{…}` + theme_apply + 断言 folded bg/accent 随主题变 + 表回 dark 后复原)

**Interfaces:**
- Consumes: Theme/theme_apply(Task 3)、令牌解析(Task 2)
- Produces: `use gui.themes.{theme_dark}` 加载形态(域根三级解析探针;若子模块加载面不顺,备案 = 主题文件平铺 gui/ 顶层 `use gui.{theme_dark}`,登记坑位不阻塞)

**Steps:**
- [ ] theme_dark.ct + theme_light.ct 先落(机制验收最小对);探针:`use gui.themes.{theme_dark}` 编译/运行通,不通走备案并登记
- [ ] s32_theme 夹具:dark→light 切换断言(bg_base folded 值翻转,组件样式零改动)
- [ ] 六平台主题文件落(色板按规格 §3.5:mac #0a84ff 系大圆角 6/10/14、win 小圆角 2/4/8 低饱和、linux #3584e4 系 4/6/12;每对深浅共享 radius/space 语汇)
- [ ] s32 扩八主题循环断言(逐套 apply→断言 accent 折叠值 = 该文件 accent 字段)
- [ ] `tests/gui/run.sh` 追加 s32_theme;跑绿;提交

### Task 5: 平台/明暗探测 + theme_auto + 环境钉值(§2.5)

**Files:**
- Modify: `gui/c_src/ctron_gui.c`(增量:`gui_platform(void)` 编译期宏分支返回 0/1/2 = mac/win/linux;`gui_os_dark(void)`:mac=popen `defaults read -g AppleInterfaceStyle` 含 "Dark"、win=popen `reg query …AppsUseLightTheme` 0x0、linux=popen `gsettings get org.gnome.desktop.interface color-scheme` 含 "dark";任一探测失败回退 1=dark;进程内缓存一次)
- Modify: `gui.ct`(`pub fn gui_platform() -> Str` / `pub fn gui_os_dark() -> Bool` extern 封装;`pub fn theme_auto()`:CTRON_GUI_THEME 非空→按八值名直取对应主题文件 make();空→platform×os_dark 选平台主题;`run`/`run_kb`/`run_d`/`run_kb_d`/`test` 入口头部缺省调 theme_auto(显式 apply 在其后仍覆盖))
- Modify: `tests/gui/s32_theme/`(扩:auto 缺省 = 宿主平台主题(钉值断言 CTRON_GUI_THEME=linux_dark 亦通)+ 显式 apply 压过 auto)

**Interfaces:**
- Produces: `CTRON_GUI_THEME=mac_dark|mac_light|win_dark|win_light|linux_dark|linux_light|dark|light`(headless 确定性唯一入口)
- Produces: 主题五主题文件外部可复用的 `make()` 形态(theme_auto 内部 use)

**Steps:**
- [ ] C 两函数(popen 缓存,失败回退 dark;headless 无桌面环境不挂:popen 超时保护用 `2>/dev/null` 短命令)
- [ ] theme_auto + 入口缺省接入(run 系四入口 + test;注意 test 强制无窗路径同样生效)
- [ ] s32 扩:钉值八值逐个断言生效;不钉值时 auto = 平台×明暗(本机 macOS 断言 mac_*);显式 apply 后 auto 不再干扰
- [ ] 跑绿;提交

### Task 6: 交互态管线(hover/active/disabled 折叠,§2.1)

**Files:**
- Modify: `gui/c_src/ctron_gui.c`(增量:每帧 poll 后 `g_hover_idx/g_active_idx` 求解——复用 P-H1 命中注册表矩形表,鼠标位×矩形求交取后者(声明序顶层);`gui_inject_hover(int idx)`/`gui_inject_active(int on)` 测试缝;`gui_state_hover(void)`/`gui_state_active(void)` 读面)
- Modify: `gui.ct`(extern 封装;`d_hover(t, name)`/`d_active(t, on)` 驱动器——name 经与 d_click 同一名→注册表解析;style 解析侧:`gt_style` 值词 `hover-bg` 系前缀属性自然入表(gword 词符集核查,缺 `-` 则扩);折叠侧:class 属性取值包一层 `prop_state(classes, prop)`——hover 态且 `hover-<prop>` 在则回退取之,active/focus/disabled 同构;focus 源头随波次二,本波次 focus-* 只入表不折叠)
- Modify: `tests/gui/s31_state/`(全量:hover 注入→bg 折叠值变 hover 色;active 同;disabled={} → disabled-fg/bg 灰化;无态属性时零覆盖)

**Interfaces:**
- Consumes: d_cmd_bg_*(Task 1)、令牌折叠(Task 2)
- Produces: `d_hover(name)` / `d_active(on)`(波次三 tooltip/菜单、波次四 list 行反白断言消费);`hover-/active-/disabled-` 三前缀 × bg/fg/border 折叠(focus- 语法在册,波次二接源)

**Steps:**
- [ ] C:注册表矩形求交 + 两读面 + 两注入口(先读 s28_hitreg 相关节,复用其矩形/名存储;不动 click 分发路径)
- [ ] gui.ct:词符集核查(gword 对 `-`)与 gt_style 无需改则零改;折叠封装 prop_state 接入 class 属性消费点(gt_cls_prop 调用处)
- [ ] d_hover/d_active 驱动器
- [ ] s31 全量断言(四态各自:注入→bg_r/g/b 变;disabled 复用 nflag 通道置位)
- [ ] 跑 s31/s32 + 全阶梯绿;提交

### Task 7: 收尾门禁与落库

**Steps:**
- [ ] `sh tests/gui/run.sh` 全绿(31+ 夹具);`sh ci.sh 8` 与 `sh ci.sh 9` 绿;`tests/net` 14/14 不回归;净树 smoke
- [ ] gui/README.md 补:主题面(Theme/theme_apply/theme_auto/钉值八值/八主题)+ 交互态前缀语法 + s31/s32 登记
- [ ] 记忆更新(gui 泳道状态:波次一落库哈希/坑位)
- [ ] 逐件 pathspec 提交;`git log --oneline` 终对齐

## Self-Review

- 规格覆盖:本计划 = 规格 §2.1(部分:focus 源随波次二)/§2.4/§2.5/§3 全链;波次二–五地图在案,规格 §2.2/2.3/2.6–2.11 与 §4 不在本计划(依赖 SL-8c 或前序波次),无静默遗漏。
- 类型一致性:槽序 = Theme 字段序 = 规格 §3.1 序,Task 2/3/4 三处同表引用;d_cmd_bg_* 签名与既有 d_cmd_x100 同形。
- 占位符:无 TBD;两处执行期探针(gword 词符集、use gui.themes 解析面)均有备案路径,非空泛。
