# GUI 基础组件库与外观体系设计(能力缝 → 组件库 → 主题)

> 状态:设计定稿待终审(2026-09-24 会话,brainstorming 流程)。§1/§2 架构节用户已批;
> §3–§6 按已批方向拟稿,随书面规格一并终审。
> 上游:2026-09-19-gui-master-design.md(§13「组件=pub view 无特权」宪法、主题折叠)、
> 2026-09-20-gui-scroll-design.md(能力性容器判据先例)、sl8c-design.md(view+props
> 消费面,机刷在飞)。

## 0. 裁决记录(2026-09-24 用户四问)

1. **范围** = 组件集 + 外观体系一起定(一份规格,分阶段落地)。
2. **架构** = 能力进内建、组件全部库级:运行时只补原子能力缝(含能力性容器,与
   scroll 同类),新语义组件一律 gui 包内 pub view 组合 + 默认样式;第三方可等效
   自制(组件=pub view 无特权)。现有内建 vbox/hbox/label/button/input/checkbox/
   spacer/scroll 元素集保持——input 的升级走能力缝(§2.2),不新增语义元素。
3. **视觉** = 深色扁平现代风(Linear/GitHub Dark 基调):扁平 + 细边框 + accent 蓝,
   状态区分靠亮度/边框/accent,不用阴影(Clay 能力面内做专业感;现有默认主题已在半路)。
4. **切片** = 能力先行·四件起步:第一批能力缝立刻被首批组件消费,每片全绿即落库;
   全目录路线图只列不做(防过度设计)。
5. **主题**(2026-09-24 审阅反馈)= 用户自定义全套主题样式是一等需求:主题文件可
   定义全套令牌,一步换装全套观感(§3.4)。

## 1. 总体架构(三层)

```
③ 外观体系  Theme struct 全套令牌 + 四态视觉规范 + 默认样式折叠规则
            (主题=.ct 文件构造 Theme 全字段字面量,theme_apply 一步换装;
             组件默认样式只引令牌,不写裸色值)
② 组件库    gui/widgets/*.ct —— pub view 组合 + 默认样式,零特权
            (首批:select/list/dialog 三新组件 + input 升级的 w-input 预设)
① 能力缝    gui.ct/parse.ct/ctron_gui.c —— 只加能力不加语义组件
            (交互态折叠/文本焦点/overlay 浮层/令牌解析,见 §2)
```

**能力缝判定铁律**:只有「无运行时支持就做不出真货」的才进内建(与 scroll 同判据)。
能组合出来的,做进内建即违宪。

**依赖登记**:组件消费形态(view+props 语法)依赖 SL-8c-2/3/4(机刷在飞,蓝本
sl8c-design.md)。首批组件动工排其落库之后;若延期,组件可用现有 bind/act + 元素
组合先出 v1(功能等价,签名面待 8c 收敛后切换)。

## 2. 能力缝(内建面,第一批四个)

### 2.1 交互态视觉折叠

- **style 语法**:平属性状态前缀,与现有扁平 style 同构:
  ```
  style b {
    bg: "#22222e"  border: "#32323f"
    hover-bg: "#2a2a38"  active-bg: "#1d1d27"  focus-border: "#2563eb"
  }
  ```
  支持的状态前缀:`hover-` `active-` `focus-` `disabled-`;首版仅修饰色彩类属性
  (bg/fg/border),尺寸/间距不参与态折叠。
- **机制**:P-H1 每帧交互注册表已判 hover/按下命中;flush 属性折叠处按当前态取值——
  定义了 `hover-bg` 则 hover 时覆盖 `bg`,未定义则不覆盖(逐属性回退,零全局默认)。
  零新事件、零 Clay 改动。
- **disabled 并轨**:现有 nflag 通道保持语义不变,视觉走 `disabled-*` 属性 +
  §3.1 灰化令牌作库级默认。

### 2.2 文本焦点编辑

- **焦点模型**:单焦点;v1 可聚焦元素仅 input。点击 input 得焦,点击其它可交互
  元素转移,点击空白失焦;Esc 手动失焦。
- **编辑键**(焦点在 input 时由运行时消费,不进 key 闭包):可打印字符、Backspace、
  Delete、←→(移动插入点)、Home/End;其余键仍走 key 闭包。方向/Home/End/删除
  属编辑面,归运行时;IME 组词随 M3 既有登记。
- **回显**:文本 + 光标竖线渲染进 input 自身(mirror label 模式退役);光标闪烁 P2,
  首版常亮。
- **事件**:`on:input={set_draft}`、`on:submit={add}`(Enter)。复用 s13「名:下标」
  先例,事件名扩为**「名:载荷」**——`act("set_draft:" + 新文本)`,act 闭包签名
  `fn(Str)` 不变,处理函数按 `:` 前缀分发。
- **API 面**:`<input class value={draft} placeholder="…">`;value 仍走 bind 通道
  (单一起源=模型);placeholder 属性,空值且未聚焦时以 TEXT_MUTED 显示。

### 2.3 overlay 浮层容器

- `<overlay>` 新内建**能力性容器**(与 scroll 同判据):Clay floating 全屏附着,
  子内容浮于常规树之上;z 序 = 声明序(floating zIndex,后声明者在上)。
- 定位属性:`align: center`(水平垂直居中,dialog 用);`x: I32`/`y: I32`
  (绝对偏移,下拉锚定用)。
- 显示控制复用 when 通道(overlay 置于 `when` 体内条件渲染),无新状态通道。
- **模态语义**(dialog 组件消费):overlay 全屏半透明遮罩底(MASK 令牌)+ 遮罩
  点击走既有命中通道回调 on:close;遮罩吃掉穿透点击。

### 2.4 style 裸词令牌解析(轻)

实证:gt_style 对属性值只存原文(引号串/裸词),无令牌解析——「组件默认样式只引
令牌」需此缝:**裸词值与已加载主题令牌同名时,加载期折叠解析为令牌值**;`"#…"`
字面量原样直通。落点在样式折叠(域运行时,查 §3.4 运行时令牌表),非 parser。
兜底:若此缝延期,组件默认样式以字面量书写、主题包整体替换组件默认样式表
(master 设计 L1 兜底口径),二选一在实施计划定夺。

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

### 3.2 四态视觉规范(全令牌表达,不运行时混色)

| 态 | 规则 | 库级默认落点 |
|---|---|---|
| hover | 底色亮一档、边框升 BORDER_STRONG(值手写进令牌/样式,不混色) | `hover-bg`/`hover-border` |
| active(按下) | 底色暗一档 | `active-bg` |
| focus | ACCENT 边框 2px | `focus-border` |
| disabled | DISABLED_FG/BG 灰化,去 accent | `disabled-*` |

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
  替换),非编译期 const——主题换装对解析/组件层零感知,组件默认样式零改动即随
  令牌全套换观感(「换令牌=换全套」)。
- **可分发**:主题文件可进任意包,`use` 进应用再 apply——theme.ct 旧注释「用户
  theme 经 use 整体替换」的精神落地为「主题 = 可分发 CT 工件」;use 级静态替换
  (选择合并拦截)仍留 L2 comptime 终态(master 设计既定,不提前)。
- **覆围口径 v1**:全套**令牌**(色板/圆角/字号/间距/禁用/遮罩)。组件默认样式的
  结构性覆盖(换形状语汇,非换色)= 可选第二层,走 §7「主题包整体替换组件默认
  样式表」口径,P2。
- **官方样本**:`gui/theme_light.ct` 随包交付——既是第二主题,又是机制的就地验收
  (同一 app 换 apply 一行,全套观感切换)。
- **热重载**:令牌表是运行时状态,CTML 热重载环不受影响;主题自身热切换 P2。

## 4. 首批交付物:input 升级 + 三新组件(功能/API/默认观感)

input 是**内建元素的能力升级**(§2.2)+ 库级默认样式预设(`w-input` 类,gui/widgets
提供);select/list/dialog 是**库级新组件**(pub view)。

| 交付物 | 功能 | API(props/事件) | 默认观感 |
|---|---|---|---|
| **input 真文本框**(内建升级) | 单行文本编辑(焦点/插入点/编辑键,§2.2) | `<input class="w-input" value={draft} placeholder="…" on:input on:submit>` | SURFACE 底 + BORDER 边 + RADIUS_SM + SPACE_MD 内距;焦点 ACCENT 边;placeholder TEXT_MUTED |
| **select 下拉** | 单选;触发器展开选项列表,pick 回调 | `options: List[Str]`、`selected: I32`、`on:pick`;v1 内联展开(when 展开,零浮层依赖),v2 换 overlay `x,y` 锚定 | 按钮形触发器 + 列表项 hover 反白 + 选中项 ACCENT 底,ELEVATED 浮起 |
| **list 选择列表** | 可点行列表 + 选中态,each 数据驱动兼容 | `items: List[Str]`、`selected: I32`、`on:pick`(行级复用「名:下标」) | 行 = 可点 hbox;选中行 ACCENT 左条 + 微亮底;行 hover 反白 |
| **dialog 模态框** | 模态卡片:标题 + 内容 + 操作区,遮罩关闭 | `title: Str`、`open: Bool`(when 通道)、`on:close`(遮罩/关闭钮) | overlay 居中;ELEVATED 卡片 + RADIUS_LG;遮罩 MASK |

组件文件各自携带默认 `style`(引令牌);消费示例(陈列室 + 真实改造)见 §6。## 5. 全目录路线图(只列不做,防过度设计)

**P1**(消费现有能力缝即可):
slider(拖拽事件缝²)、progress(纯组合)、tabs(纯组合)、switch(checkbox 变体)、
radio(组合)、menu(overlay)、tooltip(overlay)、toast(overlay)、badge(纯组合)。

**P2**(消费新能力缝):拖拽事件(drag 事件缝)、光标闪烁、Tab 焦点环导航、
右键菜单、图标、滚动条视觉、程序化 `focus()`/`scroll_into_view`、多行 textarea。

² 拖拽事件缝:pointer move + 按住位移进事件通道(「名:载荷」复用),P2 随 slider 细化。

## 6. 验收与测试(能力→示例→测试,house 方法论)

- **能力夹具**(阶梯新增):s29_state(hover/active/focus/disabled 折叠——d_cmd 断言
  折叠后绘制属性,无需新注入口)、s30_focus(input 编辑全链:得焦/键入/退格/submit/
  失焦;d_type_char/d_press_key 已有)、s31_overlay(z 序/居中/遮罩回调)、
  s32_theme(theme_apply 换 light 后折叠值全套断言 + 缺省深色不受扰)。
- **组件验收**:examples/todo 改造换真 input(退役 mirror label,回归既有断言);
  新示例 examples/gui_widgets 四件套陈列室(headless 断言 + `--run` 真窗口)。
- **驱动器增量**:hover 态断言走折叠后命令缓冲,零新注入;焦点态同口径。
- **门禁**:sh tests/gui/run.sh 阶梯 + ci.sh [8/9];黄金/坐标夹具延续
  CTRON_GUI_FT_OFF=1 钉值;净树 smoke 与 tests/net 不回归。
- **视觉验收**:gui_widgets `--run` 人工过一遍四态(扁平风基准确认)。

## 7. 坑位与风险登记

- **样式合并序**:用户覆盖组件默认 vs extends 子不覆父——方向相反,合并序钉死 + 夹具锁(§3.3)。
- **Clay 视口剔除**:overlay/展开列表增高后,headless 视口须给足(既有坑,夹具 640 高起)。
- **编辑键消费边界**:焦点内编辑键运行时吃掉不进 key 闭包;s30 夹具锁此边界。
- **事件载荷转义**:「名:载荷」载荷含 `:` 时分发须取首个 `:`(处理函数名不含 `:`,
  载荷任意文本安全);夹具覆盖。
- **SL-8c 在飞**:组件 props 消费面若延期,v1 降级 bind/act 组合(§1 依赖登记)。
- **select v2 锚定**:触发器坐标查询依赖命中注册表矩形,v1 内联展开不依赖。
- **遮罩色 8 位制式**:MASK 用 `#RRGGBBAA`,但 C1 裁决/theme.ct 注释只承诺 6 位
  `#RRGGBB`——色解析是否认 8 位未验证;若只认 6 位,遮罩降级不透明深色(如
  `#101016`)或扩色解析(实施计划定夺,倾向降级)。
- **Theme 字段面负担**:全字段字面量 ~20+ 字段,主题作者手写负担——官方主题文件
  即模板,文档给可复制骨架;「从默认改三色」场景可给 `theme_default()` 改成员配方
  (构造后成员赋值,零新语法)。
- **令牌表全局态与热重载**:theme_apply 后的表是运行时全局态,与热重载环/多入口
  (run 与 test)的交互口径=s32 夹具覆盖;主题热切换 P2 前不做增量 apply。
- **令牌解析兜底**:§2.4 若延期走「主题包整体替换组件默认样式表」口径,二选一定夺。
