# GUI 基础组件库与外观体系设计(能力缝 → 组件库 → 主题)

> 状态:设计定稿待终审(2026-09-24 会话,brainstorming 流程)。四轮审阅:首轮定架构
> 与切片;二轮增补自定义全套主题与主题集(含「能力优先于 hack」横切裁决);三轮
> 合理性批判修订(hover 管线如实重估、驱动器颜色访问器、四态去深色偏置、v1 明面
> 已知限制);四轮丰富度补全(图像/定时器/剪贴板三缝 + P1 目录扩容 + 组件×each 探针)。
> 上游:2026-09-19-gui-master-design.md(§13「组件=pub view 无特权」宪法、主题折叠)、
> 2026-09-20-gui-scroll-design.md(能力性容器判据先例)、sl8c-design.md(view+props
> 消费面,机刷在飞)。

## 0. 裁决记录(2026-09-24 用户)

1. **范围** = 组件集 + 外观体系一起定(一份规格,分阶段落地)。
2. **架构** = 能力进内建、组件全部库级:运行时只补原子能力缝(含能力性容器,与
   scroll 同类),新语义组件一律 gui 包内 pub view 组合 + 默认样式;第三方可等效
   自制(组件=pub view 无特权)。现有内建 vbox/hbox/label/button/input/checkbox/
   spacer/scroll 元素集保持——input 的升级走能力缝(§2.2),不新增语义元素。
3. **视觉** = 深色扁平现代风(Linear/GitHub Dark 基调):扁平 + 细边框 + accent 蓝,
   状态区分靠亮度/边框/accent,不用阴影(Clay 能力面内做专业感)。二轮修订:此基调
   保留为独立主题 dark 的基调与各主题兜底语汇;**缺省观感让位系统自适应**(裁决 #6)。
4. **切片** = 能力先行·四件起步:第一批能力缝立刻被首批组件消费,每片全绿即落库;
   全目录路线图只列不做(防过度设计)。
5. **主题**(一轮审阅反馈)= 用户自定义全套主题样式是一等需求:主题文件可定义全套
   令牌,一步换装全套观感(§3.4)。
6. **主题集**(二轮审阅反馈,三轮修订)= **明暗为等一等维度**:内置八套——平台六套
   (mac/win/linux × dark/light,Clay 能力面内的 HIG/Fluent/Adwaita 味诠释,不承诺
   原生拟真)+ 独立两套(dark/light)。**缺省 = `theme_auto`**:宿主 OS × 宿主明暗
   (§2.5);`CTRON_GUI_THEME` 钉值与显式 `theme_apply` 覆盖。
7. **方法论裁决(横切)**= 设计遇语言/能力不支持,**扩展能力是正路,不用 hack 兜底**。
   据此:色解析扩 `#RRGGBBAA`(修订 C1 六位承诺,规范回写登记);令牌解析(§2.4)定为
   必做;组件消费面等 SL-8c 落库,不造 bind/act 替身。
8. **丰富度与能力补全**(四轮审阅)= 图像/纹理、定时器、剪贴板三缝补入;P1 目录
   扩容(table/image/多选 list/输入变体/便宜件包);四态规范去深色偏置;无选区、
   无 IME 等列为 v1 明面已知限制(§4.1);批判分析两处实证错误(hover 现成、
   零注入口)按实测修正(§2.1)。

## 1. 总体架构(三层)

```
③ 外观体系  Theme struct 全套令牌 + 四态视觉规范 + 默认样式折叠规则
            (主题=.ct 文件构造 Theme 全字段字面量,theme_apply 一步换装;
             内置八套主题在 gui/themes/(平台×深浅+独立深浅),
             缺省 theme_auto=宿主 OS×宿主明暗;
             组件默认样式只引令牌,不写裸色值)
② 组件库    gui/widgets/*.ct —— pub view 组合 + 默认样式,零特权
            (首批:select/list/dialog 三新组件 + input 升级的 w-input 预设)
① 能力缝    gui.ct/parse.ct/ctron_gui.c —— 只加能力不加语义组件
            (交互态/文本焦点+剪贴板/overlay/令牌解析/平台明暗/图像/定时器,
             见 §2 七项)
```

**能力缝判定铁律**:只有「无运行时支持就做不出真货」的才进内建(与 scroll 同判据)。
能组合出来的,做进内建即违宪。

**依赖登记**:组件消费形态(view+props 语法)依赖 SL-8c-2/3/4(机刷在飞,蓝本
sl8c-design.md)。按裁决 #7:**组件动工序 = SL-8c 落库之后,不造 bind/act 替身**;
能力缝(§2)、主题面(§3)与夹具可先行,与 8c 无依赖冲突。

## 2. 能力缝(内建面)

### 2.1 交互态视觉折叠

- **前提修正(2026-09-24 实测)**:hover/按下态在运行时**并不存在**——`hover`/
  `PointerOver` 于 gui.ct 与 ctron_gui.c 零出现,现命中注册表是 click 按名合成分发面。
  本缝 = **新增逐帧指针态管线**:
  - 布局期逐元素指针命中查询(Clay_PointerOver 或指针×矩形求交)→ hover 集;
    指针按下命中 → active 集;
  - flush 属性折叠处按态取值:定义了 `hover-bg` 则 hover 时覆盖 `bg`,未定义则
    不覆盖(逐属性回退);active/focus/disabled 同口径。零全局默认。
- **style 语法**:平属性状态前缀,与现有扁平 style 同构:
  ```
  style b {
    bg: "#22222e"  border: "#32323f"
    hover-bg: "#2a2a38"  active-bg: "#1d1d27"  focus-border: "#2563eb"
  }
  ```
  支持前缀 `hover-` `active-` `focus-` `disabled-`;首版仅修饰色彩类属性
  (bg/fg/border),尺寸/间距不参与态折叠。
- **headless**:新增注入口 `d_hover(name)`(强制悬停态;active 态经按下注入钩子
  同法)——`d_click` 是按名合成、无指针位置,交互态断言必须走注入口,「零注入口」
  口径作废。
- **探针任务**:each 实例行级 hover(list 行反白消费)——click 有「名:下标」先例,
  hover 的实例索引须同构验证(§7/s35)。
- **disabled 并轨**:现有 nflag 通道保持语义不变,视觉走 `disabled-*` 属性 +
  §3.1 灰化令牌作库级默认。

### 2.2 文本焦点编辑(含剪贴板)

- **焦点模型**:单焦点;v1 可聚焦元素仅 input。点击 input 得焦,点击其它可交互
  元素转移,点击空白失焦;Esc 手动失焦。
- **编辑键**(焦点在 input 时由运行时消费,不进 key 闭包):可打印字符、Backspace、
  Delete、←→(移动插入点)、Home/End、**Ctrl/Cmd+V(插点粘贴)**;其余键仍走
  key 闭包。方向/Home/End/删除属编辑面,归运行时;IME 组词随 M3 既有登记。
- **剪贴板**:raylib 剪贴板 API 直调(master 设计已证可行);实现为运行时自持缓冲
  ——真窗口路径与系统剪贴板同步,headless 直控缓冲(测试缝,非替身)。
  复制/剪切/全选依赖选区模型,随 P2(§4.1 caret-only 口径)。
- **回显**:文本 + 光标竖线渲染进 input 自身(mirror label 模式退役);光标闪烁 P2,
  首版常亮。
- **事件**:`on:input={set_draft}`、`on:submit={add}`(Enter)。复用 s13「名:下标」
  先例,事件名扩为**「名:载荷」**——`act("set_draft:" + 新文本)`,act 闭包签名
  `fn(Str)` 不变,处理函数按首个 `:` 前缀分发。
- **API 面**:`<input class value={draft} placeholder="…">`;value 仍走 bind 通道
  (单一起源=模型);placeholder 属性,空值且未聚焦时以 TEXT_MUTED 显示。
- **钉值口径**:外部改文(如过滤/大写化)时插入点收敛行尾;超宽文本按盒宽截断尾显,
  插点可见性 P2。无选区模型(caret-only)与无 undo/redo 见 §4.1/P2。

### 2.3 overlay 浮层容器

- `<overlay>` 新内建**能力性容器**(与 scroll 同判据):Clay floating 全屏附着,
  子内容浮于常规树之上;z 序 = 声明序(floating zIndex,后声明者在上)。
- 定位属性:`align: center`(水平垂直居中,dialog 用);`x: I32`/`y: I32`
  (绝对偏移,下拉锚定用)。
- 显示控制复用 when 通道(overlay 置于 `when` 体内条件渲染),无新状态通道。
- **模态语义**(dialog 组件消费):overlay 全屏半透明遮罩底(MASK 令牌)+ 遮罩
  点击走既有命中通道回调 on:close;遮罩吃掉穿透点击。
- **键盘模态(v1 钉值)**:overlay 最顶层为 dialog 时,键闭包挂起(堵模态泄漏——
  否则弹窗开着背景快捷键照常触发)。
- **探针先行**:三处未钉——浮层命中序(遮罩吃穿透的判定次序)、多层 z 叠序、
  位于 scroll/裁剪祖先内时的行为。实施计划探针验证;若裁剪祖先内异常,**约束
  overlay 仅根级声明**(钉值)。

### 2.4 style 裸词令牌解析(定为必做,裁决 #7)

实证:gt_style 对属性值只存原文(引号串/裸词),无令牌解析——「组件默认样式只引
令牌」需此缝:**裸词值与已加载主题令牌同名时,加载期折叠解析为令牌值**;`"#…"`
字面量原样直通。落点在样式折叠(域运行时,查 §3.4 运行时令牌表),非 parser。
**热路径口径**:两级——加载期解析并缓存,`theme_apply` 时失效重建;逐帧仅取缓存,
禁逐帧字符串查表。「主题包整体替换组件默认样式表」的兜底口径作废,不留 stringly
替身。

### 2.5 平台与明暗探测(主题自适应的地基)

- `gui_platform() -> Str`("mac" / "win" / "linux"):c_src 胶水一行(编译期
  `__APPLE__`/`_WIN32` 宏分支)。
- `gui_os_dark() -> Bool`:宿主明暗探测,c_src 胶水(macOS 读
  AppleInterfaceStyle;Win 读注册表 AppsUseLightTheme;Linux 读
  `gsettings color-scheme`——仅 GNOME 系可靠,KDE/平铺 WM/无桌面回退深色;
  探测矩阵是持续维护面)。按裁决 #7 做真实探测缝,不约定死值。
- 二者供 `theme_auto` 缺省选择(§3.5)。
- 测试钉值:`CTRON_GUI_THEME=mac_dark|mac_light|win_dark|win_light|
  linux_dark|linux_light|dark|light` 覆盖探测——headless 断言跨平台确定性的
  唯一入口(宿主差异不进黄金)。

### 2.6 图像/纹理管线(P1 首位,四轮补入)

- `<image>` 新内建**能力性容器**(与 scroll 同判据——无运行时支持做不出真货):
  `src` 属性,LoadTexture 加载,**路径键 LRU 缓存**(仿 ft_shim 64 槽先例,防
  显存膨胀),Clay 图片元素绘制;尺寸 = w/h 样式或纹理原尺寸。
- 失败口径:加载失败渲占位框(边框 + 底色)不崩,状态可断言。
- headless:纹理命令进命令缓冲,`d_cmd_img` 类访问器断言存在/尺寸。
- 下游:图标(P2)、avatar/缩略图/图表地基。

### 2.7 定时器/帧回调原语(P1,四轮补入)

- tick 原语 = 运行时帧计数 + 毫秒钟(c_src 帧循环已有,暴露读数)。**组件内部
  动画直接消费**:spinner 相位、光标闪烁(P2 转正)、进度动画、toast 自动消失。
- 用户级 `on:after` 定时事件 P2(事件通道扩展)。
- headless:`d_tick(ms)` 注入推进(确定性,不睡真实时钟)。

## 3. 外观体系

### 3.1 令牌全集(= §3.4 Theme 字段;下表默认值 = 内建深色主题)

| 组 | 令牌 | 默认值 | 用途 |
|---|---|---|---|
| 底色 | BG_BASE / SURFACE / ELEVATED | `#181820` / `#22222e` / `#2a2a38` | 页面底 / 控件底 / 浮起面(弹层、卡片) |
| 边框 | BORDER / BORDER_STRONG | `#32323f` / `#454553` | 常规边 / 悬停边 |
| 前景 | TEXT / TEXT_MUTED | `#e6e6eb` / `#9a9aa8` | 主文本 / 次文本、placeholder |
| 强调 | ACCENT / ACCENT_HOVER | `#2563eb` / `#3b82f6` | 主操作、选中、焦点环 |
| 语义 | DANGER(已有) / SUCCESS(新) | `#c83c3c` / `#2f9e63` | 危险 / 成功 |
| 遮罩 | MASK | `#00000099` | 模态遮罩 |
| 形状 | RADIUS_SM / RADIUS_MD / RADIUS_LG | 4 / 8 / 12 | 输入框 / 按钮 / 卡片 |
| 字号 | SIZE_XS / SM / MD / LG / XL | 12 / 14 / 16 / 20 / 24 | 辅助 / 正文 / 标题 |
| 间距 | SPACE_SM / MD / LG(已有) | 4 / 8 / 16 | 不变 |
| 禁用 | DISABLED_FG / DISABLED_BG | `#6b6b78` / `#23232d` | 灰化态默认 |

### 3.2 四态视觉规范(全令牌表达,不运行时混色;方向中立)

| 态 | 规则 | 库级默认落点 |
|---|---|---|
| hover | 与底色**拉开一档对比**:深色主题亮一档、浅色主题暗一档(方向不绑死,具体值进各主题令牌) | `hover-bg`/`hover-border` |
| active(按下) | 比 hover 再进一档的按下反馈 | `active-bg` |
| focus | ACCENT 边框 2px | `focus-border` |
| disabled | DISABLED_FG/BG 灰化,去 accent | `disabled-*` |

> 修订说明:三轮审阅前的「hover=亮一档」是深色偏置,对四套浅色主题为反例,
> 已改写为对比度口径。

### 3.3 默认样式折叠与用户覆盖序

组件默认 style 只引令牌;**用户 class 同属性后折叠胜出**(用户覆盖组件默认)。
注意与 extends「子已定义者不覆盖」方向相反,合并序必须实现时钉死 + 夹具锁
(预期:extends 先折叠,用户 class 最后折叠)。

### 3.4 主题自定义与换装机制(用户裁决 #5,一等需求)

**主题 = 一个 .ct 文件,构造 `gui.Theme` 全字段字面量,`theme_apply` 一步换装。**

```ct
// my_theme.ct —— 用户自定义全套主题(light 示例)
use gui.{Theme}

pub fn make() -> Theme {
    return Theme {
        bg_base: "#f5f5f7"  surface: "#ffffff"  elevated: "#ffffff"
        border: "#d8d8de"   border_strong: "#b9b9c2"
        text: "#1a1a24"     text_muted: "#6e6e7a"
        accent: "#2563eb"   accent_hover: "#1d4fd8"
        danger: "#c83c3c"   success: "#2f9e63"  mask: "#101016"
        radius_sm: 4  radius_md: 8  radius_lg: 12
        size_xs: 12  size_sm: 14  size_md: 16  size_lg: 20  size_xl: 24
        space_sm: 4  space_md: 8  space_lg: 16
        disabled_fg: "#9a9aa8"  disabled_bg: "#ececf2"
    }
}

// 应用侧:
// use my_theme.{make}
// gui.theme_apply(make())   // run/test 之前调用一次
```

机制要点:

- **完备性免费**:Theme 为 gui 包定义的 pub struct,字段 = §3.1 令牌全集
  (snake_case 同形);字面量缺字段即编译错——「全套」由编译器保证,无 stringly
  校验面。现 theme.ct 常量迁为 `theme_default()` 的构造源(默认主题 = 不调用
  theme_apply 时的内建表)。
- **查表对象**:§2.4 裸词折叠查**运行时令牌表**(默认内建深色;theme_apply 整体
  替换并失效解析缓存,§2.4),非编译期 const——主题换装对解析/组件层零感知,
  组件默认样式零改动即随令牌全套换观感(「换令牌=换全套」)。
- **可分发**:主题文件可进任意包,`use` 进应用再 apply——theme.ct 旧注释「用户
  theme 经 use 整体替换」的精神落地为「主题 = 可分发 CT 工件」;use 级静态替换
  (选择合并拦截)仍留 L2 comptime 终态(master 设计既定,不提前)。
- **覆围口径 v1**:全套**令牌**(色板/圆角/字号/间距/禁用/遮罩)。组件默认样式的
  结构性覆盖(换形状语汇,非换色)= 可选第二层,走「主题包整体替换组件默认样式
  表」口径,P2(此处为显式扩展点,非 hack——§2.4 已定必做,二者并存分工:令牌管
  全局换肤,样式表覆盖管结构性换形)。
- **缺省行为**:`run`/`test` 入口缺省执行 `theme_auto()`——`gui_platform()` ×
  `gui_os_dark()` 二维选择(裁决 #6「默认与宿主一致」);`CTRON_GUI_THEME` 钉值与
  用户显式 `theme_apply` 均可覆盖。
- **内置主题集**:八套随包交付,置于 `gui/themes/`(§3.5)——平台×深浅六套 +
  独立深浅两套;独立主题既是用户可选样本,又是机制的就地验收(同一 app 换
  apply 一行,全套观感切换)。
- **热重载**:令牌表是运行时状态,CTML 热重载环不受影响;主题自身热切换 P2。

### 3.5 内置主题集(gui/themes/,裁决 #6;平台 × 明暗二维)

| 主题 id | 文件 | 基调 |
|---|---|---|
| mac_dark / mac_light | `theme_mac_dark.ct` / `theme_mac_light.ct` | macOS HIG 味:大圆角(RADIUS 6/10/14)、系统灰阶、mac 蓝 accent(#0a84ff 系)、克制边框;浅版=纸白底深灰字 |
| win_dark / win_light | `theme_win_dark.ct` / `theme_win_light.ct` | Fluent 味:Mica 灰阶、小圆角(2/4/8)、低饱和 accent、细边框为主 |
| linux_dark / linux_light | `theme_linux_dark.ct` / `theme_linux_light.ct` | Adwaita 味:中圆角(4/6/12)、libadwaita 灰阶、GNOME 蓝(#3584e4 系) |
| dark / light | `theme_dark.ct` / `theme_light.ct` | 独立基准对:深色扁平现代(Linear/GitHub Dark,裁决 #3)与同语汇浅色版 |

- 平台六套是**味道诠释而非原生拟真**:色板/圆角/间距/字号阶梯向各 OS 设计语言
  对齐;阴影、模糊、材质不在 Clay 能力面,不承诺(视觉近似,非 hack)。
- 每对深浅共享同族形状/间距语汇,只换色板令牌;八套共用同一 Theme 字段面与四态
  规范(§3.2)——主题集本身即「换令牌=换全套」的八个实证。
- 明暗等权:`gui_os_dark()` 探测决定缺省深浅(§2.5),用户可任意固定。
- 用户选择:`use gui.themes.{theme_dark}`(或任意自备主题文件)+
  `gui.theme_apply(...)`;缺省 `theme_auto` 可被 `CTRON_GUI_THEME` 钉值覆盖。

## 4. 首批交付物:input 升级 + 三新组件(功能/API/默认观感)

input 是**内建元素的能力升级**(§2.2)+ 库级默认样式预设(`w-input` 类,gui/widgets
提供);select/list/dialog 是**库级新组件**(pub view)。

| 交付物 | 功能 | API(props/事件) | 默认观感 |
|---|---|---|---|
| **input 真文本框**(内建升级) | 单行文本编辑(焦点/插入点/编辑键/剪贴板,§2.2) | `<input class="w-input" value={draft} placeholder="…" on:input on:submit>` | SURFACE 底 + BORDER 边 + RADIUS_SM + SPACE_MD 内距;焦点 ACCENT 边;placeholder TEXT_MUTED |
| **select 下拉** | 单选;触发器展开选项列表,pick 回调 | `options: List[Str]`、`selected: I32`、`on:pick`;v1 内联展开(when 展开,零浮层依赖),v2 换 overlay `x,y` 锚定 | 按钮形触发器 + 列表项 hover 反白 + 选中项 ACCENT 底,ELEVATED 浮起 |
| **list 选择列表** | 可点行列表 + 选中态,each 数据驱动兼容 | `items: List[Str]`、`selected: I32`、`on:pick`(行级复用「名:下标」);多选变体 `selected: List[I32]` 随 P1 | 行 = 可点 hbox;选中行 ACCENT 左条 + 微亮底;行 hover 反白 |
| **dialog 模态框** | 模态卡片:标题 + 内容 + 操作区,遮罩关闭 | `title: Str`、`open: Bool`(when 通道)、`on:close`(遮罩/关闭钮) | overlay 居中;ELEVATED 卡片 + RADIUS_LG;遮罩 MASK |

组件文件各自携带默认 `style`(引令牌);消费示例(陈列室 + 真实改造)见 §6。

### 4.1 v1 已知限制(明面清单,四轮审阅立此存照)

- **IME/中文文本输入缺席**(M3 依赖)——对本项目以 gui_zitie 为标杆的中文生态是
  生态级缺口,按最高优先级跟进,不以「随 M3」一笔带过。
- **input 无选区模型**(caret-only):shift+方向/双击选词/选区高亮 P2;无 undo/redo
  (P2)。
- **select v1 无键盘操作**(焦点模型仅 input)——可达性缺口,焦点环(P2)落地后
  补方向键导航。
- **select v1 内联展开布局扰动**:展开推开下方内容(v2 overlay 锚定解决)。
- **input 超宽截断尾显**,插点可见性 P2(轻量横移视窗)。
- **光标常亮**(闪烁 P2)。
- dialog 键盘模态泄漏已由 §2.3 钉值堵漏(顶层 dialog 时键闭包挂起),非限制。

## 5. 全目录路线图(只列不做,防过度设计;四轮扩容)

**P1**(消费 §2 能力缝,含三新缝):
slider(拖拽缝¹)、progress、**spinner**(tick³)、tabs、switch(checkbox 变体)、
radio、menu(overlay)、**menubar**(menu+hbox 组合)、tooltip(overlay)、
toast(overlay + tick 自动消失³)、badge、**table**(列头+对齐+行选中,
files/面板生态直接受益)、**image**(§2.6)、**多选 list**(`selected: List[I32]`)、
**input 变体**(password mask / 数值属性面,零新能力)、
**便宜件包**(divider / accordion / card——纯组合各一两行)。

**P2**(消费新能力缝):拖拽事件缝¹细化、可拖分隔条、tree(view 递归已支持,
缩进+折叠组合)、combobox(input+select 合体)、grid 布局助手(**前置:核查 Clay
grid 支持面**)、光标形状(I-beam/pointer)、光标闪烁(tick 转正)、Tab 焦点环导航、
右键菜单、图标(消费 §2.6)、滚动条视觉、程序化 `focus()`/`scroll_into_view`、
多行 textarea、**选区模型**、**undo/redo**、**焦点原语开放给 view 层**(解除
§7 宪法例外)、用户级 `on:after` 定时、窗口图标/无边框;
主题面:高对比无障碍主题、主题热切换。

**P3 方向**:date/time picker、color picker、command palette、chart、过渡动画
(插值,消费 tick)。

¹ 拖拽事件缝:pointer move + 按住位移进事件通道(「名:载荷」复用)。
³ spinner/toast 自动消失消费 §2.7 tick 原语。

## 6. 验收与测试(能力→示例→测试,house 方法论)

- **能力夹具**(阶梯新增):s29_state(hover 注入 `d_hover` + active/focus/disabled
  折叠,经**颜色访问器**断言折叠后绘制属性)、s30_focus(input 编辑全链:得焦/
  键入/退格/submit/剪贴板键(自持缓冲口径)/失焦)、s31_overlay(z 序/居中/遮罩
  回调 + 命中序/裁剪祖先探针)、s32_theme(八主题 apply 逐套折叠值全套断言;
  theme_auto 平台×明暗探测与 CTRON_GUI_THEME 钉值覆盖;显式 apply 后缺省自适应
  不再干扰)、s33_image(加载/缓存命中/失败占位/绘制尺寸)、s34_tick(d_tick 推进
  + spinner 相位/toast 自动消失)、s35_组件×each 探针(list/select 于 each 内
  实例化 + 组件内部 each,邻域坑验证见 §7)。
- **夹具迁移清单**:input 升级改 keystroke 消费边界——s12_input/s19_input_d/
  todo 断言面连锁,**迁移先行于 s30 合入**(逐个改口径,清单化销账)。
- **驱动器增量**(三轮实证缺口):`d_hover` 注入口、`d_cmd` 颜色访问器
  (`d_cmd_bg100`/`border100`/`fg100`——现访问器仅 count/type/text/几何,无颜色)、
  `d_tick`、图像命令访问器(`d_cmd_img`)、剪贴板注入(直控自持缓冲)。
- **组件验收**:examples/todo 改造换真 input(退役 mirror label,回归既有断言);
  新示例 examples/gui_widgets 陈列室(首批四件 + P1 渐次上架;headless 断言 +
  `--run` 真窗口)。
- **门禁**:sh tests/gui/run.sh 阶梯 + ci.sh [8/9];黄金/坐标夹具延续
  CTRON_GUI_FT_OFF=1 钉值;净树 smoke 与 tests/net 不回归。
- **视觉验收**:gui_widgets `--run` 人工过一遍四态(dark/light 基准 + 宿主平台
  主题各一遍;八主题 `CTRON_GUI_THEME` 钉值轮巡抽查)+ image 显示抽查。

## 7. 坑位与风险登记

- **样式合并序**:用户覆盖组件默认 vs extends 子不覆父——方向相反,合并序钉死 + 夹具锁(§3.3)。
- **Clay 视口剔除**:overlay/展开列表增高后,headless 视口须给足(既有坑,夹具 640 高起)。
- **编辑键消费边界**:焦点内编辑键运行时吃掉不进 key 闭包;s30 夹具锁此边界。
- **事件载荷转义**:「名:载荷」载荷含 `:` 时分发须取首个 `:`(处理函数名不含 `:`,
  载荷任意文本安全);夹具覆盖。
- **SL-8c 在飞**:组件动工等其落库,不造 bind/act 替身(§1 依赖登记)。
- **select v2 锚定**:触发器坐标查询依赖命中注册表矩形,v1 内联展开不依赖。
- **焦点半特权=宪法例外登记**:编辑能力 v1 仅内建 input 可消费,第三方不能自制
  真文本组件——与「无特权」宣称冲突,属登记在案的例外;终态 = P2 焦点原语开放给
  view 层(§5 在册),届时解除。
- **组件×each 段错误坑邻域**:「容器套 each」「each 挂 class」已登记段错误(绕行
  勿代修);组件库大面积进入该邻域(list/select 于 each 内实例化、组件内部 each),
  s35 探针先行,触雷登记新坑、不代修。
- **overlay 三未钉**:命中序/叠序/裁剪祖先(§2.3)探针先行,异常则约束仅根级。
- **令牌解析热路径**:两级缓存 + theme_apply 失效(§2.4),禁逐帧查表。
- **图像纹理缓存上限**:路径键 LRU(仿 ft_shim 64 槽),防显存膨胀;失败占位不崩。
- **遮罩色 8 位制式**:MASK 需 `#RRGGBBAA`,而 C1 裁决/theme.ct 注释只承诺 6 位。
  按裁决 #7 **扩色解析**:gui 域 gt 词法 + ctron_gui.c 色转换面同步支持 8 位,
  6 位继续合法(alpha 视 FF);C1 承诺修订为「#RRGGBB(A)」,规范回写登记——
  不做不透明降级替身。
- **Theme 字段面负担**:全字段字面量 ~20+ 字段,主题作者手写负担——官方主题文件
  即模板,文档给可复制骨架;「从默认改三色」场景可给 `theme_default()` 改成员配方
  (构造后成员赋值,零新语法)。
- **令牌表全局态与热重载**:theme_apply 后的表是运行时全局态,与热重载环/多入口
  (run 与 test)的交互口径=s32 夹具覆盖;主题热切换 P2 前不做增量 apply。
