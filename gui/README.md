# gui 域包 —— Ctron 窗口应用运行时(T3 平台面)

clay + raylib 窄桥、CTML 视图解析、域运行时与 headless 验收驱动的单域集合。
**消费形态 = 顶层命名空间 `use gui.{...}`**(2026-09-23 起;域目录在仓库根与
`std/` 平级,分层与门禁见 `std/README.md` 域包章)。

## 布局

```
gui.ct            门面:入口 API + 运行时 + 解析 + 驱动器(消费面唯一入口)
gui/parse.ct      CTML 解析(gt_* 词法/树构建;SL-8a 表达式槽捕获)
gui/theme.ct      主题令牌面
gui/bind/raylib.ct  extern 窄桥(#[trusted];用户不接触)
gui/c_src/        C 胶水单一真源(ctron_gui.c 等;native 链接面)
```

## 快速上手(双口径)

```ct
use gui.{run_kb_d, test, d_click, d_frame, d_type_char, d_expect_text}

struct Model { var count: I32 }

fn inc() { }                       // on:click 处理函数声明(E8120)

fn bind_all(buf: List[Str], m: Box[Model]) {
    if buf[0] == "count" { buf.push("i:" + m.count.to_string()) }
}

fn act(name: Str, m: Box[Model]) {
    if name == "inc" { m.count = m.count + 1 }
}

fn main() -> I32 {
    var src: Str = read_file("app.ctml")       // 或 ctron_embedded()(内嵌 view)
    var m = Box[Model](Model { count: 0 })
    if env_get("CTRON_GUI_HEADLESS") != "" {
        return test(src, 320, 640, |buf| bind_all(buf, m),
            |name| act(name, m), |k| k, |t, a, key| { /* 断言脚本 */ })
    }
    run_kb_d("app", 320, 640, |buf| bind_all(buf, m), |name| act(name, m), |k| k)
    return 0
}
```

```ctml
// app.ctml —— 视图与样式
view Counter {
  <vbox class="root">
    <label class="t">count: {count}</label>
    <button class="b" on:click={inc}>+1</button>
    <spacer class="gap"/>
  </vbox>
}
style root { direction: column gap: 8 padding: 16 }
style t { fg: "#ffffff" size: 24 }
```

## CTML 能力面

- 元素:`vbox` `hbox` `label` `button` `input`(自闭合) `when` `each` `spacer`(自闭合);
  样式 `style <名> { 属性… }` 按 class 关联。
- **表达式绑定(SL-8a/8b)**:槽内容为 Ctron 表达式,规范形 = 去空白原文——
  `{draft + "!"}` 叶串接、`cond={n > 0}` 比较、`{n * 2}` 算术、`{form.draft}`
  点链;求值器文法 = 比较/加减/乘除模/负号/路径(`.len` 取长),值带型通道
  (`i:` 整 / `b:` 布 / 裸 = 串)。**纯读门 E8190**:槽内赋值/自增减即拒;
  `on:` 事件槽豁免(事件期求值,当前为处理函数名单形态,表达式事件随 SL-8c)。
- bind 通道:每帧按槽名询问用户 bind 闭包(`buf[0]` 分发,答案 push);
  `when:名`→"1"/"0"、`each:名`→逐项 push 为域保留前缀。
- 已知限制:叶字面量含标点在内嵌形态会被重建插空格(避用);input 只渲空盒,
  回显靠 mirror label;视口须容得下全内容(Clay 剔除视口外渲染命令)。

## 入口 API(SL-8c ③钩子退役后)

- **文档化入口 = `gui.run(ViewCall)`**(编译器 desugar 合成 __gui_bind/__gui_act/__gui_run,
  TodoApp 照抄形态;s41_run_d 三层验收)。
- **键盘应用** = `rt_run_kb_anchor(title, w, h, bind, act, key)`(内部面仍 pub;
  gui_themes/gui_widgets 键盘切换演示用此)。
- 旧 run/run_kb/run_d/run_kb_d 已降内部(rt_run_src 系),勿在新代码使用。

## 驱动器(headless 断言面)

| 入口 | 源 | 键盘 | 用途 |
|---|---|---|---|
| `run(src, title, w, h, bind, act)` | 显式 | 无 | 窗口(纯鼠标) |
| `run_kb(src, title, w, h, bind, act, key)` | 显式 | 有 | 窗口(键入) |
| `run_d(title, w, h, bind, act)` | 默认锚 app.ctml(缺失回落内嵌) | 无 | 窗口 + 热重载环 |
| `run_kb_d(title, w, h, bind, act, key)` | 同上 | 有 | **窗口 + 键入 + 热重载(最常用)** |
| `test(src, w, h, bind, act, key, script)` | 显式 | 注入 | **headless 断言(强制无窗)** |
| `test_sk(tree, …)` | 骨架直通(gui_sk_load) | 注入 | headless(编译期骨架口径) |

热重载环:60 帧内容比对 → gt_parse 原址重解析 → 树换源,闭包状态天然保留。

## 驱动器(headless 断言面)

`d_frame`(重绘)、`d_click`(按处理函数名点击)、`d_type_char`/`d_press_key`
(注入键)、`d_expect_text`/`d_expect_absent`(文本断言)、
`d_cmd_count/type/text_len/text_byte/x100/y100/w100/h100`(命令缓冲逐条访问,
黄金帧差分与几何断言底座)。

## 文本管线(M3 合流,2026-09-23)

flush 的 TEXT 命令走 `gui/c_src/ft_shim.c` 字符串纹理缓存:`gui_ft_text`
((串,px) 键,LRU 64 槽,白色 RGBA 渲染、颜色绘制时 tint)→ `gui_ft_text_draw`
(懒上传 + DrawTexture);测量 `ctron_measure` = FreeType 实测 advance(与渲染同
迭代序,布局盒宽==实测)。默认字体位图路径仅作无 CJK 字体环境兜底(ASCII 仍可用,
即旧点阵观感的唯一残留场景)。

- `CTRON_GUI_FT_OFF=1`:钉回 0.55 启发式测量——坐标/黄金夹具(s3/s5/s17/s18/s21)
  专用钉值,跨平台稳定(macOS Hiragino 与 Linux Noto 的 advance 不同,实测值不可作
  跨平台黄金)。
- 无 CJK 字体环境:测量回启发式、绘制回默认字体(旧行为,零回归)。
- 链接面:凡链 `gui/c_src/ctron_gui.c` 的 run.sh 须加 `gui/c_src/ft_shim.c` +
  `vendor/gui/build/libfreetype.a` + `-I vendor/gui/freetype/include`。
- 直绘路径(`ft_render`/`ft_tex_*`)语义零变化,`examples/gui_cjk` 仍为直绘示范;
  声明式路径中文示范 = `examples/gui_counter`(「计数: {count}」)。

## 组件库(波次四,2026-09-26)

- **声明**:同源 .ctml 内 `view Card (name: Str) { ... }` + 实例 `<Card name: {u}/>`
  (大写首字母;props 冒号形,值=引号串直取/{表达式}求值/裸词);**应用根=最后声明的
  视图**(组件先声明后用)。
- **内置三件套**(s42_comp 为用法范本):
  - `Select`(触发器+when+each 行;事件头 sel_toggle/sel_pick,行事件带实例下标);
  - `WList`(标题+行列表;list_pick);
  - `Dialog`(when+overlay 模态+遮罩 close;键盘模态链 Esc 关)。
  事件头=组件合约,应用侧按头实现即接;列表 props 经同名 `each:名` bind 通道透传。
- **组件×each**:each 内组件实例,实参引用迭代变量(`name: {u}`)走展开期 itemvar 直取。
- 已知限:组件内 when/each 可用;跨文件分发(view 导入)与 dialog 内容投影待后续。

## 主题面(波次一,2026-09-25)

- **Theme struct**(25 字段,字段序=令牌槽序)+ `theme_apply(t)` 一步换装;
  `theme_default()` 缺省深色;`theme_auto()` = `CTRON_GUI_THEME` 八值钉值
  (mac_dark…linux_light/dark/light)> 平台×明暗探测;六入口缺省 `theme_auto_apply`
  (此前显式 apply 优先不扰)。
- **内置八主题**:gui.ct 命名函数 `theme_dark()/theme_light()/theme_mac_dark()…`
  (域根平铺,`use gui.themes` 不可寻址);第三方主题 = 任意 .ct 文件定义
  `make() -> Theme`。
- **令牌裸词**:style 值可直接写令牌名(`bg: ACCENT`),25 枚 = BG_BASE…DISABLED_BG
  + RADIUS_* + SIZE_* + SPACE_*;存储期折 @槽标记,折叠期 O(1) 读。

## 字体字重(波次五c,§2.8)

- **`font_weight` 样式属性**(400/700,缺省 400):label/button/input/checkbox 四分支
  路由 gui_text_w。合成加粗 = FT_Outline_Embolden(px/16)+advance 增量 px/24——
  **免粗体字体文件**,测量与渲染双轮同增量(测量==渲染不变式)。
- **已知限制**:FT_OFF 下 bold≡400 宽(heuristic 无增量);font-family 用户字面
  注册/回退链 P2。
- **实现注记**:Clay 文本测量内联于 OpenTextElement 同步发生(每文本两次)——
  字重走「当前值」直传;flush 侧用影子表(产出序=TEXT 命令序)取重传缓存
  (缓存键 (串,px,weight) 三元,gui_ft_text_wt)。

## tick/尺寸约束/快捷键(波次五b,§2.7/§2.10/§2.11)

- **tick 原语**:运行时毫秒钟(注入优先)+帧计数;`d_tick(t, ms)`/`d_frames()`。
  可见消费者=光标闪烁——**仅注入时钟激活**(d_tick 确定性口径),真窗常亮至 P2 转正。
- **尺寸约束**:容器样式 `min_w/max_w/min_h/max_h`——max=钳制填充(GROW{min,max}),
  min=托底 hug(FIT{min,∞});装包 min*100000+max(min<21000/max<100000);
  容器宽度存量口径 = grow(w 样式对容器不生效,zitie 同款事实)。
- **快捷键表**:`gui_hotkey("mod+s", "save")` 注册(run 前任意时机),mods 位
  1=ctrl/cmd 2=shift 4=alt;事件路由链=焦点编辑键 > 模态 Esc/吞 > **快捷键表** >
  key 闭包;未中回落。菜单加速器显示随组件波次。

## 图像管线(波次五a,§2.6)

- **`<image src="路径" class ... />`**(自闭合):w/h 样式定盒(缺省 100x100),
  DrawTexturePro 拉伸填充;缓存 = 路径键 LRU 16 槽(flush 专用)。
- **架构红线:纹理加载只在 flush(真窗路径)**——headless 无 GL 上下文,
  LoadTexture 即崩;命令面只透传路径指针(树内 npre,跨帧稳定)。
- **失败占位**:fopen 判存在,缺失 → 无 image 配置的底色框(不崩);
  断言读面 = 命令类型 3=IMAGE + `d_cmd_*` 几何族。
- 编译器白名单(image)登记缓行——embedded 形态俟 SL-8c-4 后;现 read_file 形态可用。

## 浮层与模态(波次三,§2.3)

- **`<overlay>` 浮层容器**:Clay floating attach ROOT(真全屏,不随父 padding)+
  zIndex=1 + CAPTURE 吞穿透;样式 `align: center`(子元素居中,dialog 卡片)、
  `x:`/`y:` 偏移(下拉锚定 v2);显隐走 when 通道;z 序=声明序。
- **键盘模态**:末帧有 overlay 时键闭包挂起,Esc 由运行时消费转该 overlay 的
  on:click(自锁修复=Esc 永可达);headless 语义镜像在 d_send_key/d_click_xy。
- **命中语义**:rt_hit_name 后向最优先(绘制序顶层先中)——遮罩吃穿透、卡片不透;
  位置点击断言口 = `d_click_xy`(d_click 按名绕过命中面,且只扫按钮表)。
- **8 位色**:`#RRGGBBAA` 全面放行(C1 修订);6 位合法 alpha=FF;折叠经
  gui_alpha 置位→gui_cfg 消费即复位;断言读面 d_cmd_bg_a。dialog 库级组件随
  SL-8c-4(编译器白名单 overlay 登记缓行,embedded 形态俟其落库)。

## input 真文本框(波次二,§2.2)

- **焦点模型**:单焦点;点击 input 得焦(光标置尾),点击其它失焦,Esc 失焦。
- **编辑键**(焦点内运行时消费,不进 key 闭包):字符/Backspace/Delete(字节级,码点级
  随 M3)/←→/Home/End/Enter=on:submit/Ctrl+V=粘贴(真窗自系统剪贴板同步)。
- **on:input「名:载荷」**:`on:input={set_draft}` → `act("set_draft:" + 新文)`,
  应用按首个冒号拆分;s33 为全链示范。on:submit 无载荷直发。
- **值单一起源=模型**:bind 活问渲染;无 on:input 的 input 编辑不持久(下帧还原,
  钉值非缺陷)。headless:`d_focus/d_blur/d_clip/d_mod` + `d_send_char/d_send_key`
  (消费返 0,未消费返原码——键消费边界断言口)。
- 已知限制:无选区(caret-only)/无 undo/超宽截断尾显/光标 `|` 字符形态常亮。

## 交互态(波次一,§2.1)

- **态前缀属性(下划线形)**:`hover_bg / hover_fg / active_bg / active_fg /
  disabled_bg / disabled_fg`——态命中则覆盖,未定义逐级回落基值,零全局默认;
  **连字符形(`hover-bg`)俟编译器 lex 词法补丁后切换**(裸 '-' 落算符 token 致
  样式块段错误,见坑位)。
- headless 注入:`d_hover(t, 名)` / `d_active(t, 0|1)`;真窗路径 = 指针求交
  (rt_hover_node,与点击命中同序)。
- 令牌/交互态折叠断言底座:`d_cmd_bg_r/g/b`(RECT 命令色读回);夹具 s31_state
  (四态全量)/s32_theme(八主题+auto+钉值)。

## 坑位(实证登记,续接必读)

- 跨包符号**必须显式请求**:use 面缺什么,发射面就缺什么(选择性合并契约;
  跨文件 const 亦不随传递,theme_default 故为自含字面量)。
- **编译器词法器裸 '-'**:CTML 样式属性名含 '-'(如 hover-bg)时词法落减号算符
  token,样式块解析段错误且无诊断码——态前缀用下划线形规避;健壮性债归编译泳道。
- 同包多文件各自独立合并:parse.ct 为死副本(gt_* 活码在 gui.ct);域子模块
  (`use a.b`)解析为 `<根>/b.ct` 平铺,子目录不可寻址。
- headless 视口给足(建议高 640):内容超视口被 Clay 剔除,断言面即"消失"。
- input 回显写 mirror label(`{draft}`);`{ident}` 简单槽走名字通道,
  表达式槽走求值器——两态并存零破坏。
- 零参方法(`.pop()` 等)发射面缺口:以重建列表 + 成员赋值配方绕行。
- 域包形态构建:`CTRON_STDPATH` 指向 std 三级解析根(域根随仓库布局解析),
  native 链接 `gui/c_src/ctron_gui.c` + `gui/c_src/ft_shim.c` +
  `vendor/gui/build/libraylib.a` + `vendor/gui/build/libfreetype.a`(M3 合流起)。

## 门禁与活样例

- 阶梯 `sh tests/gui/run.sh`(s1–s28 + e8 语料);`ci.sh [8/9]` 挂载。
- 示例:`examples/todo`(键入/列表/空态全链)、`gui_counter`(最小活模型/声明式
  中文示范)、`gui_calc`(全场景断言)、`gui_files`(真实 IO 首例:文件查看器)、
  `gui_cjk`(直绘中文渲染)、`gui_themes`(八主题键盘切换陈列室)、`gui_widgets`
  (Select/WList/Dialog 组件交互演示);各目录 `run.sh` 直跑,`--run` 开真窗口。

## 分层与稳定口径

T3 平面面(依赖窗口/GPU):**永不进 std 通用门禁**;泳道自有 headless 命令
缓冲验收为准。vendored C 在 `vendor/gui/`(clay/raylib),包内 `c_src/` 只放
自写胶水。设计文档:`docs/superpowers/specs/2026-09-16-gui-ctml-design.md`
(规范)、`docs/superpowers/specs/2026-09-19-gui-use-gui-design.md`(域包设计)。
