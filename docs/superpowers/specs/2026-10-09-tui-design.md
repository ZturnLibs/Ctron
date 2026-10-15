# pkgs/tui —— 终端界面域包设计规范 v1

2026-10-09 立项。上游讨论:CLI/TUI 支持设计对话(同日);摸底基线:CLI 五洞+Env 发射面缺口(COVERAGE.md:880)、gui 域包全套参照架构。
姊妹文档:CLI 档(std/cli+底座内建)另行成文;本文只管 TUI。

---

## 0. 一句话与档位

**终端全屏交互应用的域包:单元格网格渲染、约束布局、键鼠事件、令牌主题、headless 测试驱动。**

**档位 = T2(系统面),论证**:唯一系统依赖是 POSIX libc(termios/ioctl TIOCGWINSZ/poll/SIGWINCH/write),纯用户态 C,**零第三方 vendored**——`c_src/tui_term.c` 全部自写(~450 行)。对照 gui(T3:raylib→GPU/窗口服务)按 lib/std/README.md 判定线落 T2。新 T3 劝退条款不适用于本包。

**零编译器税(与 CLI 泳道的关键分野)**:不新增任何内建、不动 compiler/。所有 C 交互走 `#[trusted] extern "c"` 窄桥(pkgs/gui/bind/raylib.ct 先例,36 行形态)。因此 TUI 泳道**完全独立于编译器泳道**,可与 CLI 底座批并行。

**泳道所有权**:GUI 泳道延伸(gui 架构姊妹包),归 gui 泳道所有。

## 1. 准入四问自答(宪章 lib/std/README.md:92)

1. **依赖面**:libc 系统调用 ≤ T2 上限;零 C 源依赖,自写胶水落 c_src/(红线:包内 c_src 只放自写胶水)。
2. **消费面**:用户裁决立项(1009);验收应用=examples/todo-tui(W4);真实需求源=CLI 工具生态配套。
3. **三宿主税**:**零内建→零编译器三线税**。Ctron 层(ctron test/自举解释/发射 C)天然同源;门以**发射臂为准**(gui s 系列同款:ctron-emit→cc 链 tui_term.c);seed/interp 臂对 extern 声明的消费能力实施期探针定口径(登记 §11);R 线(ctronr)豁免(gui 同口径)。
4. **泳道所有权**:gui 泳道(上文)。

## 2. 包结构与物理形态

```
pkgs/tui/                      W3 落地
  tui.ct          ~300  门面:TuiBuf/TuiTree/Event/Theme 类型 + extern 窄桥 +
                        run_app/test 入口声明 + 子文件 re-export(显式清单)
  tui_buffer.ct   ~400  单元格网格:Buffer SoA + 写面 + diff + SGR/ANSI 序列生成
  tui_layout.ct   ~300  约束布局:三尺寸模式 + vbox/hbox 求解器
  tui_render.ct   ~600  渲染:树 DFS→网格(折行/边框/对齐/宽字符/进度)
  tui_event.ct    ~150  事件常量(fn 出面) + C 侧解码产物 → Event 映射
  tui_driver.ct   ~700  run_app 主循环 + test headless 驱动 + d_* 协议
  tui_theme.ct    ~300  Theme 令牌 + theme_default + 能力探测 + 色编码(纯数据+纯函数,零 extern)
  c_src/tui_term.c ~420 termios raw + 输入解码状态机 + SIGWINCH + 输出缓冲
  README.md
pkgs/tui_widgets/              W4 拆出(交互组件;与 tui 的分界=是否产事件/持焦点态)
pkgs/tui_themes/               二期(主题分发包;pkgs/themes 先例)
```

纪律:域子模块平铺文件(不可子目录寻址);门面 re-export 显式请求——「use 面缺什么,发射面就缺什么」;C 符号一律 `ctron_tui_*`/`tui_*` 前缀。

## 3. 核心模型

### 3.1 单元格网格 TuiBuf(SoA 平行 List)

```ct
pub struct TuiBuf {
    pub var w: I32
    pub var h: I32
    var chs: List[Str]      // 每格一个字素的 UTF-8 子串
    var fg: List[I32]       // -1=终端默认;否则 0xRRGGBB packed(g_parse_color 同款)
    var bg: List[I32]
    var bold: List[Bool]    // 属性具名拆槽(仓库无位运算算符,位集禁用)
    var under: List[Bool]
    var rev: List[Bool]
    var wd: List[Bool]      // 宽字符尾随格标记(主格=false)
}
```

- **SoA 定案理由**:gui GuiTree 28 平行 List 同构,发射面已实证;`List[Cell]` 值类型下标写回在解释口径不通(assign target:Index 坑,gui_parse.ct:943 实证配方)。
- 写面:`buf_put(b, x, y, ch, fg, bg)` 单格 / `buf_fill(b, x, y, w, h, ...)` 区域 / `buf_line` / `buf_rect`(盒线)。
- 越界写入=静默钳除(渲染期宽容,布局期诊断,§5)。

### 3.2 节点树 TuiTree(保留式,view 产物)

```ct
pub struct TuiTree {
    pub var tag: List[Str]     // vbox/hbox/text/para/bar/box/spacer/raw
    pub var txt: List[Str]     // 文本内容/样式参数("single"/"double"/"round")
    pub var cls: List[Str]     // 样式类(主题令牌解析键)
    pub var wmode: List[Str]   // "len"/"pct"/"fill"
    pub var wval: List[I32]
    pub var hmode: List[Str]
    pub var hval: List[I32]
    pub var al: List[Str]      // 交轴对齐 left/center/right(top/middle/bottom 同字段)
    pub var nfc: List[I32]     // 首子(-1=叶)
    pub var ns: List[I32]      // 弟链(-1=末)
    pub var aid: List[Str]     // 可寻址 id(headless 断言定位/组件实例路由)
}
```

builder(合并式:子树各 List 逐个 push + 索引偏移;List 引用语义下零拷贝 push):

```ct
pub fn vbox(kids: List[TuiTree]) -> TuiTree
pub fn hbox(kids: List[TuiTree]) -> TuiTree
pub fn text(s: Str) -> TuiTree            // 单行,不折行
pub fn para(s: Str) -> TuiTree            // 折行段落(自然高=折行数)
pub fn bar(s: Str) -> TuiTree             // 顶栏(accent 底反色)
pub fn box(kids: List[TuiTree], line: Str) -> TuiTree   // 边框容器
pub fn spacer(w: I32, h: I32) -> TuiTree
pub fn with_id(t: TuiTree, id: Str) -> TuiTree
```

每帧全量重建(O(树大小) 分配税;增量 builder 列优化债,触发=profile 实证)。

**树模型与 ctml(W5)**:builder(每帧重建)与 ctml(静态骨架+bind 活问,gui 现行模型)两形态**同汇 TuiTree**——ctml 解析产物=含表达式槽的 TuiTree(txt 存表达式串+expr 档),下游布局/渲染/diff 管线不分叉;组件可逐节点混用两形态。文法探针(1009):`gt_parse`(pkgs/gui/gui_parse.ct:1259)pub 且纯字节处理,文法核心可平移;`bxv_eval`(gui_expr.ct:764)非 pub 且签名携 GuiTree,须平移重写(~400 行)或推动 pub 化重构;bind 询问协议(`buf[0]=="prop:x"` 询问→push `"i:42"` 应答)为纯字符串协议,零成本同款——应用的 bind 应答臂理论可跨 gui/tui 通用。

### 3.3 事件 Event(单 update 回调,act 也走事件流)

```ct
pub struct Event {
    pub var kind: I32       // EV_KEY/EV_CHAR/EV_MOUSE/EV_RESIZE/EV_ACT/EV_TICK
    pub var code: I32       // 功能键码(EV_KEY);鼠标键号(EV_MOUSE)
    pub var ch: I32         // 字素码点(EV_CHAR)
    pub var shift: Bool     // 修饰键具名拆槽(Cell attrs 同款配方;仓库无位运算算符,
    pub var ctrl: Bool      // 算术码判「含 ctrl」需枚举 2/3/6/7,1009 裁决弃)
    pub var alt: Bool
    pub var mx: I32         // 鼠标单元格坐标
    pub var my: I32
    pub var w: I32          // EV_RESIZE 新尺寸
    pub var h: I32
    pub var name: Str       // EV_ACT:组件事件头名("头:实例",gui 同款合约)
    pub var args: List[Str] // EV_ACT:ev_arg_* 解码由组件层负责
}
```

- **一期事件建模 = I32 码 + fn 常量**(零语言改动)。enum+match 语言批落库后整体换装(触发条件在案,§10)。
- 键码常量一律 **fn 出面**(`pub fn k_up() -> I32 { return 1001 }`):跨文件 const 在 interp unbound、跨文件 const 不随选择性合并发射,双坑在册(gui_theme.ct:82 注释、坑位 65-67)。
- 常量表(节选):`k_up/k_down/k_left/k_right/k_home/k_end/k_pgup/k_pgdn/k_ins/k_del/k_enter/k_tab/k_backspace/k_esc/k_f1..k_f12/k_ctrl_a..k_ctrl_z`(ctrl 码=1..26 与 C0 对齐)。

## 4. 布局规范

- **尺寸三模式**:`len(n)` 定长 / `pct(p)` 容器百分比(**四舍五入口径钉死:p*容器/100 向零截断**) / `fill` 均分剩余(**余数从左到右逐格分配**)。
- **求解顺序**(vbox 垂直/hbox 水平同构):定长先扣 → pct 按容器尺寸计 → fill 均分剩余。
- **布局越界分层口径(1009 裁决)**:静态可知错误(pct 负数、非法模式串、定长和超容器且无 fill)在解析/求解期报 E 码 rc≠0——e8 负例门的对象;纯运行期越界(内容超视口、终端缩小)渲染钳 0+首帧一次性诊断,不崩。
- 文本自然高:para = 折行数;text = 1;未指定高度(hmode 缺省)用自然高,len 显式覆盖则裁剪。
- 交轴对齐按 `al` 字段;主轴除定长外拉伸满。

## 5. 渲染与 diff 规范

DFS 携矩形 (x,y,w,h) 写 back buffer;帧尾 back↔front diff 产 ANSI 字节串一次 write。

- **折行**:para 空格断点 word wrap + CJK 逐字断;**宽字符不拆半**(行尾容不下整宽字则提前换行);跨容器矩形边界的宽字符整对丢弃。
- **宽字符**:主格 + 尾格(wd=true);headless 读断言对尾格透明。
- **盒线**:single `─│┌┐└┘├┤┬┴┼` / double `═║╔╗╚╝` / round `─│╭╮╰╯`;标题嵌上边线(`┌ title ─`)。
- **bar**:整行 accent 底 + bg 色 fg(反色由主题算,不依赖终端 rev)。
- **progress**:ratio → `█` 填充 + `░` 空缺,定比四舍五入。
- **diff**:逐格比较,连续相同段跳过;每行首个不同格定位光标(`ESC[y;xH`)顺写至行尾或下一不同;SGR 状态机记 last fg/bg/attr 避免重复发码;色编码按终端档位(§7)。
- 光标全程隐藏;input 组件(组件包)临时显示 + 定位,失焦即隐。

## 6. 输入规范(c_src/tui_term.c 状态机)

- **raw 口径**:`cfmakeraw` 但**保留 OPOST**(\n 语义不变);VMIN=1 VTIME=0;快照存胶水全局。
- **解码表**:CSI `A B C D`=方向 / `H F`=Home/End / `5~ 6~`=PgUp/PgDn / `1~ 7~`=Home、`4~ 8~`=End、`2~`=Ins、`3~`=Del;SS3 变体同映射;`ESC char`=Alt+char(alt=true);`\r`=Enter、`\t`=Tab、`\x7f`=Backspace、`\x01..\x1a`=Ctrl+字母;SGR 鼠标 `ESC[<b;x;yM/m`(1000h+1006h)。
- **ESC 歧义**:读入 ESC 后 poll(0) 无续字节 = 独立 ESC 键。
- **Ctrl+C 产事件不默认退**(TUI 惯例,退出权在应用 update);Ctrl+\ 仍走 SIGQUIT。
- **SIGWINCH**:信号处置置胶水标志,主循环查获 → ioctl 取新尺寸 → EV_RESIZE。
- **tick**:`tui_wait(ms) -> I32`(poll 超时返回 0)驱动 EV_TICK(光标闪烁/动画;无订阅者时 tui_wait 用长超时省电)。
- 鼠标 motion(1002/1003)一期不开,登记债。

## 7. 主题与能力探测

```ct
pub struct Theme {
    var bg: Str        // #RRGGBB 或 "default"=终端默认色直通(SGR 49/39)
    var fg: Str
    var surface: Str
    var border: Str
    var text: Str
    var muted: Str
    var accent: Str
    var sel_bg: Str
    var sel_fg: Str
    var err: Str
    var ok: Str
    var warn: Str
    var pad_h: I32
    var pad_v: I32
}
```

- **Theme 显式传参(1009 裁决,弃 gui_theme 槽先例)**:`run_app[M](view, update, m, theme: Theme)` 入口收取,渲染层全链显式传递;view 只产 cls 串不触 Theme——主题只被 render 层消费,扩散面可控,零全局隐藏态(gui_theme 的 C 侧槽绕行不再需要)。`theme_default()` 深色;字段序即文档槽位惯例(备未来烘焙)。
- **`"default"` 是 TUI 与 GUI 的关键差异**:缺省主题 bg 用终端默认色,尊重用户终端自身配色(好公民行为)。
- **能力探测(启动一次,存胶水 int)**:`NO_COLOR`→mono;`COLORTERM=truecolor|24bit` 或 TERM 含 `direct`→24bit;TERM 含 `256color`→256(最近邻降级);否则 16(查表)。mono 档只出黑 white/rev。
- tui_theme.ct **零 extern,纯数据+纯函数**(色编码纯 Ctron 串拼接;比 gui_theme 更纯:无槽、无平台探测)。色能力档(非主题数据,进程级事实)仍存胶水 int。

## 8. 驱动与生命周期铁律

```ct
pub fn run_app[M](view: fn(Box[M]) -> TuiTree,
                  update: fn(Box[M], Event),
                  m: Box[M],
                  theme: Theme) -> I32   // 调用点显式 TypeArgs(v0 契约);theme 显式传参(§7)
```

生命周期**顺序钉死**:

1. `tui_enter()`:termios 快照 → cfmakeraw(留 OPOST)→ alt screen `?1049h` → 隐藏光标 `?25l` → 鼠标 `?1000h ?1006h` → 清屏。
2. 首帧:view → 布局 → 渲染 → diff 写。
3. 循环:`tui_wait(超时)` → 事件 drain → 逐个 update(改模型)→ view → 渲染 diff。
4. `tui_leave()`:`?1006l ?1000l` → `?25h` → `?1049l` → 恢复 termios 快照。

**任何退出路径必还原**(铁律):C 胶水 `atexit` 注册 leave + panic 路径覆盖(ctron_panic 冲刷范式先例);「run_app 内 panic 后终端已还原」列为 s 系列门之一。非 tty 环境调 run_app = panic("tui: stdin 非终端")(防呆;CI 一律走 test)。

## 9. headless 测试协议(test + d_* 族)

```ct
pub fn test[M](view: ..., update: ..., m: Box[M], theme: Theme, w: I32, h: I32,
               script: fn(Box[TuiTest])) -> I32
```

不进 raw/alt screen,虚拟 TuiBuf 直断言。TuiTest 面:

| 面 | 语义 |
|---|---|
| `d_key(code)/d_char(cp)/d_mods` | 键事件注入 |
| `d_mouse(btn, x, y)` | 鼠标点击注入(转 act 路由) |
| `d_resize(w, h)` | 尺寸变更注入 |
| `d_act(name, args)` | 组件事件直驱(actc 后缀名直驱同款) |
| `d_cell(x, y)` | 读单格(ch/fg/bg/attr,宽字符尾格透明) |
| `d_text(x, y, len)` / `d_find(s)` | 读串 / 全屏找串(返回首格坐标) |
| `d_expect_text(x, y, w, s)` / `d_expect_find(s)` | 断言族(失败 panic 带坐标诊断) |
| `d_frame()` | 帧计数 |

组件 act 路由合约(与 gui_widgets 对称):组件点击/确认 → `EV_ACT`(`name = "头:实例"`)进事件流;每组件头名表随组件包 README(§10 W4)。

## 10. 分期与组件

- **W3a** pkgs/tui 骨架:门面+窄桥+tui_term.c+enter/leave 铁律+门 s1(构建链接)。
- **W3b** buffer/布局/渲染:门 s2 冒烟、s3 布局三模式、s4 折行+CJK、s5 diff 字节断言。
- **W3c** 输入事件+run_app:门 s6(解码状态机 cc 直编单测+Ctron 层映射)+panic 还原门+panic 消息重放(stderr 缓冲,§18)+tui.quit/set_tick。
- **W3d** 主题+探测:门 s7(三档色编码)。
- **W4** pkgs/tui_widgets 交互组件:门 s8。一期组件合约(全约表见 §14):
  - `list`(items, sel)高亮选中+滚窗 / `input`(value)单行编辑(光标/插入删/Home End,最重)/ `statusbar`(hints)按键提示条 / `dialog`(title, lines, opts)模态 overlay / `progress`(ratio)。
  - 每组件:宿主处理器头名表 + d_* 断言用例。验收应用 = examples/todo-tui。
- **W5a** ctml 运行时版(**零编译器税**):tui_parse.ct 平移 gt_parse 文法→TuiTree 骨架(标签集换 TUI 集;尺寸/对齐进属性 `width="fill" height="3" align="center"`;style 窄集 fg/bg/bold/under/rev+主题令牌);应用手写 bind 应答 fn(gui test() 的 bind 参数形态先例);条件样式走 `class:if={...}` 新文法点(1009 裁决;`on:click` 前缀属性解析路径同款,tui_parse 内实现零协调,gui 未来可跟)。组件=纯 prop 契约+宿主头名事件(gui_widgets 同模式);高频动态段(滚窗/选中)组件内部 builder 拼,组件对外可双形态(builder/ctml 同汇 TuiTree)。
- **W5b**(触发式)编译期烘焙:gui sk_build 同款直出 C 静态数组,运行时零解析;触发=解析税 profile 实证+ctml 组件生态成型。**不做**:复用 gui 编译器 desugar 管线(GuiTree 28 表 vs TuiTree 10 表形状+像素 vs 字格语义皆异,复制税大于收益)。
- **W5c**(触发式):enum 换装(Event)/ Windows Console / 主题分发包。

**一期不做(边界)**:鼠标 motion、图像协议(sixel/kitty)、六画、tmux 穿透、Windows(登记债)、emoji 双宽精确表(0x1100 起粗判+登记)、异步输入源、多窗格分屏。

## 11. 登记债与待裁决

**登记**:
- seed/interp 臂 extern 消费口径探针(W3a 实施首件;门发射臂为准不受阻)。
- **运行时 panic 钩子机制**(跨泳道债,归编译器/运行时批,CLI/TUI 共需;一期缓解=stderr 缓冲重放,§18)。
- 焦点路由器(消费/冒泡;触发=组件内部态实证,§13)。
- 每帧树重建分配税(增量 builder 候,触发=profile)。
- scroll 任意子树虚拟化(触发=大内容 profile,§15)/ 鼠标 motion / emoji 双宽精确表 / Windows Console API(触发=需求实证)。
- OSC 52 读剪贴板 / bell / overlay 栈 / mask 暗化 / scrollbar 装饰 / 二期组件族(§14/§16/§17 各节)。
- Event enum 换装(触发=语言 enum+match 批落库)。

**裁决台账(1009 用户批复:12 项全按推荐定案)**:
1. 布局越界=分层口径:静态可知错误报 E 码 rc≠0(e8 负例门对象),纯运行期越界钳 0+首帧诊断(§4)。
2. Event 修饰键=shift/ctrl/alt 三 Bool 具名拆槽(§3.3,与 Cell attrs 同配方)。
3. 事件建模=一期 I32 码+fn 常量,enum+match 批落库整体换装——**排期决策非绕行**,与「能力优先于 hack」裁决相容(登记债在案)。
4. 回调形态=单 update,act 进事件流(事件种类键/鼠/resize/tick/act+未来焦点冒泡,单流更可扩展)。
5. 全局态=折中:Theme 显式传参(入口参数+渲染层全链,§7);tui.quit/色能力档留胶水全局(进程级事实,非应用数据)。
6. 组件状态=宿主持(list top/input cur 在模型;MUV 纯度,与 gui_widgets 哲学一致);有态组件模式候样板实证(二期触发)。
7. 档位落位=pkgs/tui 先行,消费面实证+API 稳后迁 lib/(机械迁移)。
8. tui_theme 驻 tui 包内(14 字段无平台探测,独立包收益不抵开销);tui_themes 分发包候需求实证。
9. 组件包=W4 拆 pkgs/tui_widgets(gui 对称;头名表独立 README)。
10. `box` 命名=实施期首选 box,实测撞字/语义混淆则 framed。
11. Ctrl+C=产事件不默认退,配套强制约定:statusbar 缺省文案必含「q 退出」提示。
12. ctml 子集=条件样式 `class:if` 新文法点(on: 解析路径同款);bxv 一期 tui 内平移重写 ~400 行(裁 TUI 子集),pub 化重构列 gui 泳道触发式债;树模型=builder/ctml 双轨并存同汇 TuiTree。

## 12. 门组织(tests/tui/)

`run.sh` 单 for 阶梯(s1–s9+e8 负例语料),ci.sh 挂载;夹具标准布局(Ctron.ctcl+src/main.ct+run.sh);全部默认 headless,`--run` 才进真终端(附 README 手测清单)。负例语料沿 e8 形态:`*.neg.ct` 期待 rc≠0+E 码注记(布局越界、非法 pct、宽字符相关)。门清单:s1 构建/链接、s2 渲染冒烟、s3 布局三模式、s4 折行+CJK、s5 diff 字节、s6 解码状态机、s7 色编码三档、s8 组件(input 字素编辑/滚窗跟随/overlay)、s9 应用约定(tui.quit、panic 消息重放、小终端降级屏)。

---

## 13. 焦点系统

- **一期:焦点态在宿主模型,update 全权路由**(`var focus: I32` 下标或 aid 串),无隐藏状态、MUV 纯度保持。域包给助手:`focus_next(cur, n) -> I32` / `focus_prev`(环绕)。
- 键路由惯例(应用转接,域包文档钉死):Tab/Shift-Tab 跨组件;方向键在焦点组件内消费(list 移选中、input 移光标)。
- **光标显示权**:TuiTree 节点带光标请求槽(`cx: List[I32]`/`cy: List[I32]`,-1=无;input 渲染期按字素下标换算列位请求),渲染器帧尾取请求定位并临时显示光标,失焦/帧尾无请求即隐。
- **二期(触发=组件内部态实证)**:焦点路由器——EV_KEY 先派焦点组件,组件 update 回调返回 Bool(消费/冒泡),未消费冒泡应用。一期不引入(为保 update 单回调纯函数形态)。

## 14. 组件库全约(W4,pkgs/tui_widgets;模式=纯 prop 契约+宿主头名事件,gui_widgets 同源)

**list**:
- props:`items: List[Str]`、`sel: I32`、`top: I32`(滚窗顶,宿主持——MUV 纯度)。
- 助手:`list_scroll(top, sel, view_h) -> I32`(保证 sel 可见的新 top,三界各钉:sel<top→sel;sel≥top+h→sel-h+1;界内→top)。
- 键(应用转接):j/k/↑/↓ 移动、PgUp/PgDn 翻页、Home/End 跳端。
- 事件:`list_pick:<实例>`(Enter)、`list_sel:<实例>`(载荷 `i:` 新选中)。
- 样式:行 `row`/选中 `row_sel`;超宽行截断尾 `…`。
- 虚拟化:只物化 top..top+h 行(§19)。
- d_*:滚窗跟随、选中高亮格、截断省略。

**input**:
- props:`value: Str`、`cur: I32`(插入点,**字素下标**,非字节)、`focused: Bool`。
- 键:可打印字符插入、Backspace 删前、Del 删后、←/→ 移字素、Home/End 行首尾、Ctrl+U 清行(Ctrl+A/E readline 惯例二期)。
- UTF-8:光标运算走 unicode.ct 字素 iter,IME 直通(终端负责合成,应用收成型 UTF-8;d_char 逐码点注入)。
- 事件:`input_change:<实例>`(载荷 `s:` 新值)、`input_submit:<实例>`(Enter)。
- 样式:focused 边框 accent/非焦 muted;光标请求(§13)。

**statusbar**:props:`hints: List[Str]`(交替 键/说明)。渲染:键反色块+说明,gap 分隔,超宽截断。无事件。

**dialog**:
- props:`title: Str`、`lines: List[Str]`、`opts: List[Str]`、`focus: I32`、`open: Bool`。
- 键:←/→/Tab 换钮、Enter 确认、Esc 取消。
- 事件:`dialog_pick:<实例>`(载荷 `i:` 钮下标)、`dialog_close:<实例>`(Esc)。
- 渲染:overlay 居中(宽定长/高自然),渲染序=主树后(§16)。

**progress**:props:`ratio: I32`(0..1000 千分比——F64 双宿主坑规避,不用浮点比)、`label: Str`。渲染:`━` 填充/`░` 空缺+label。无事件。

**二期清单(触发式)**:table(多列对齐+排序标)、tabs、menu/menubar、checkbox/toggle、**textarea**(多行编辑,大件)、password(input mask 变体)、scrollbar 装饰、spinner、toast(tick 驱动)、palette(命令面板)。

## 15. 滚动

- `scroll(child, top: I32)` viewport 原语(驻 tui 核心非组件包):子树超视口裁剪,top 宿主持。滚轮事件映射 `top ± 3`(常量 fn 出面)。
- 一期全物化+裁剪;任意子树虚拟化二期(触发=大内容 profile)。滚动条装饰二期。

## 16. overlay

- 一期**单 overlay**(dialog 自含);渲染序=主树→overlay 树,后写覆盖;z 序=树序,无 z 模型。
- mask 暗化背景不做(逐格改写成本高、真终端无半透明),登记债。overlay 栈(多层模态)二期。

## 17. 剪贴板与系统面

- `tui.copy(s: Str)`:**OSC 52** 写剪贴板(`ESC]52;c;<base64>BEL`;base64 经 enc.ct,纯 Ctron 串拼接;主流终端 kitty/WezTerm/alacritty/iTerm2 支持)。读剪贴板不做(查询-应答异步),登记。
- `tui.set_title(s: Str)`:OSC 2 窗口标题。
- 终端原生选区:Shift+拖=绕过应用鼠标捕获,终端自身职责,应用不干预(文档说明)。
- bell 二期。

## 18. 应用级约定

- **MUV 数据流钉死**:事件→update 改模型→view 出树→布局渲染;事件处理器不直接改屏。
- **退出协议**:update 调 `tui.quit()`(胶水 bool,gui `rt_close_check` 同族;泛型 M 不约定字段),run_app 循环检测 → 返回 I32(0=正常),main 转进程码。
- **tick**:`tui.set_tick(ms)`(0=无,EV_TICK 关闭)——有动画/闪烁需求才开,空闲长超时省电(§19)。
- **小终端降级**:w<20 或 h<6 → 域包内置「终端太小」单行提示屏,不崩不乱画;resize 到 w/h=0 防御钳 1。
- **panic 消息重放(缺口+零税治法,本规范关键发现)**:panic 的 stderr 直写发生在 alt screen 还原之前,C atexit 晚于 fprintf——**裸方案 panic 消息必丢**。治法(零编译器税):`tui_enter` 时 `setvbuf(stderr, 私有缓冲, _IOFBF, N)` 全缓冲;leave 顺序=还原终端→`fflush(stderr)`→恢复 stderr 默认无缓冲。TUI 期语义:eprint 进重放缓冲(约定:TUI 生命期内应用不依赖 stderr 时序);panic 消息在还原后的主屏可见。SIGABRT 不走 atexit,登记。**根治=运行时 panic 钩子机制(跨泳道债,归编译器/运行时批,CLI/TUI 共需)**。

## 19. 性能预算

- **事件驱动重画**:无事件不重画(除非 tick 开启);空闲 `tui_wait` 长超时(500ms)省 CPU。
- diff 输出典型帧 <4KB(跳相同段);首帧/resize 全帧 w*h。
- list 滚窗=天然虚拟化(只物化可见 h 行);scroll 原语一期全物化。
- 树重建 O(n) 分配税:GC 翻面自适应阈值已落库(S1),TUI 低帧率无压力;profile 债同 §3.2。
- 输入延迟:按键→上屏 <1 帧(事件驱动即时重画)。
