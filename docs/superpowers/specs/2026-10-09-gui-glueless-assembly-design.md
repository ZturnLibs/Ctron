# GUI 无胶水装配终态设计——来源/事件/装配三面归一(2026-10-09 定稿)

> 来源 = 用户问「GUI 开发要 read_file 读 .ctml、事件处理函数手动绑定,能否简化」的整轮设计对话。
> 六项裁决 + 三项补充条款全部对话内当场落定,本文为完整整理记录(逐条见 §2)。
> 基线 = 1008 typed-props v1 落库后(d90aebf6);总原则继承「能力优先于 hack」(0924 横切裁决)
> 与「最小完备红线」(spec §13.1)。文档化入口现状(run(ViewCall))见 pkgs/gui/README §入口 API。

## 0. 总纲

**一个 GUI 应用的用户面只剩四样东西:Model、handler 函数、view、一行装配。**
其余一切(read_file 文本搬运、bind 通道闭包、act 名字分发器、组件桩堆、测试装配转发)
全部由编译器 desugar 与域包合成。三条不变式贯穿全部裁决:

1. **静态树红线**:视图树必须编译期可枚举。它是四件东西的共同前提——组件可达闭包
   (事件收集/桩裁剪)、编译期合约检查、热重载 60 帧差分、双端骨架同形(编译器骨架表
   ↔ 运行时 gt_parse 逐字节一致)。任何"按值换树形"的提案一律不收(§5.3)。
2. **值单一起源 = 模型**:唯一活状态在装配点 `make()` 造出、被 desugar 装箱捕获的那一份
   Box 上。视图/组件永不持有状态;`CTRON_GUI_STATE` 注入与热重载保状态都靠这一环。
3. **零语言核心**:全部落点 = compiler GUI 面(gui_parse.ct)+ pkgs/gui + driver_emit 锚面。
   视图层新语法(ev 声明/前置块/默认值/opt)均为 CTML 层文法 + desugar 合成,合成产物
   是真 Ctron 代码,双宿(解释/发射)同源。

## 1. 考据快照(设计依据,实证在册,实现者免重考)

| # | 事实 | 证据 |
|---|---|---|
| 1 | 桩根因 = 事件收集从合并骨架**全部树根**走查,非实例化组件全量进合成 act | gui_parse.ct:3896 `gui_ds_collect_slots`(`isch==0` 者全收,组件树事件进 cvn/cvf) |
| 2 | 合成三件 = `__gui_bind_<V>`/`__gui_act_<V>`/`__gui_run_<V>`,源文本合成双宿同源 | gui_parse.ct:4536/4587/4827 |
| 3 | 调用形事件双端已通(编译期 desugar + 域包运行时按头分发) | gui_ev_head2(gui_parse.ct:1575 名字形/调用形通吃);gui_expr.ct:873 ev_fire;E8120 存在/签名门 |
| 4 | 文件形态编译期并非不可见:发射锚烘焙 + 检查面已解析(含 GUI-27 use 合并) | driver_emit.ct:351-354 ctron_embed_src;`gui_ctml_file_use` 节点级合并(s78_xfileview) |
| 5 | 跨文件 view 导入已落库:`use mod.Name` 行式 + `pub view` 导出,DFS 合并,热重载感知 | GUI-27(1004 收官,tests/gui/s78_xfileview) |
| 6 | typed-props v1:struct 值穿通道,`__gui_ds_read/items` 字段下钻,通道 `\x02root.rel` marker | d90aebf6,s92 |
| 7 | 热重载:60 帧比对、原址重解析、Box 状态保留;GUI-27 后探针走 gt_anchor_src 合并源 | w4_hotreload/w4_ctml_reload/s26_reload_d |
| 8 | 组件库 GuiBlock 恒随合并:`use gui_widgets.{任一符}` 一符拖全库 | gui_snippets(锚符 gui_widgets_ver);pkgs/gui_widgets 16 件 |
| 9 | 旧三闭包入口已降内部(rt_run_src 系),README 标「勿在新代码使用」 | pkgs/gui/README §入口 API |
| 10 | 现状两形态能力分界:内嵌档有编译期合成(调用形/typed-props),文件档只到检查面 | s41_run_d vs gui_contacts;§2 裁 1 即为此设 |

## 2. 裁决记录(全部落定,2026-10-08/09 对话)

| 裁 | 议题 | 落定 | 内容 |
|---|---|---|---|
| 1 | 文件形态命运 | **A2 desugar 接线** | desugar 增加锚文件输入源(经 GUI-27 已落库的 `gui_ctml_file_use` 合并器);双形态同语法同能力,`.ctml` 转正为可热重载的编译期输入。弃 A1(双形态宪章,文件档永久手写 act)与 A3(文件退役弃热重载) |
| 2 | 桩策略 | **严格合约 + opt 豁免** | 事件默认必接(漏接 = E 码点名);`opt` 标记显式豁免(漏接 = 合成 no-op 臂)。prop 默认必填,带默认值可省(§5.4)。弃宽松 no-op(错签/漏实现静默) |
| 3 | 事件习语 | **调用形唯一文档化** | `on:click={handler(实参)}` 唯一文档形态;名字形 `{inc}` 降 legacy,W4 迁存量示例时清 |
| 4 | 范围 | **零语言核心** | 落点 = 编译器 GUI 面 + pkgs/gui + driver_emit 锚面;一等视图值/一等函数 handler 不做(备案 §11) |
| 5 | 根视图引用形态 | **A′ 双形并存,文件档默认锚根** | 文件档默认 = `read_view` 锚 + `run_app`/`test_app` 无名装配(根 = 锚文件末位声明视图);显名形(`run(Name(...))`/`test_view(Name(...))`)为显式回退(根 ≠ 末位/歧义);内嵌档唯显名形。锚行内建正名 **`read_view`**(read_file 保持任意 IO 原义) |
| 6 | 组件事件合约 | **`ev` 声明 + `on:` 统一接线** | 组件签名 `ev` 子句声明事件+载荷签名;组件体内只引自身数据 prop 与声明事件;接线一律 `on:名字={处理器(实参)}`,与内建元素同一语法。弃 handler-prop(备案 §11:一等函数值落地后的换皮候选)与名字匹配现状 |
| 补 a | 属性连接符 | **统一 `=`** | 组件 prop 冒号形退役;属性文法一条 `名 = 值`(值 ∈ 字符串字面量/`{表达式}`/裸词)。冒号仅存两处:`on:` 前缀(事件名的命名空间分隔符,名字的一部分)与 style 块(CSS 文法域)。视图签名 `(p: Person)` 是 fn 形参类型标注,不动 |
| 补 b | 视图内逻辑 | **前置计算块** | 根标签前允许 var 不可变绑定链(任意 Ctron 表达式,含 if 表达式);尾置树即返回(`return <视图>` 同效);赋值/循环/spawn 禁入(E 码,渲染纯度)。树内条件仍走 `when`,**不收按值返回不同子树**(静态树红线) |
| 补 c | 可选性 | **prop 默认值 + 事件 opt** | 见 §5.4;此项同时把裁 2 的严格合约内置化 |

## 3. 终态用户面(两档完整样例,同一应用)

### 3.1 文件档(文档默认:视图居 .ctml,四层布局,可热重载)

目录:`src/model.ct`(领域)+ `src/main.ct`(装配)+ `app.ctml`(视图入口)+ `components.ctml`(组件)。

```ct
// ───────── src/model.ct:领域层,零 UI ─────────
struct Person { var name: Str; var role: Str }
struct Model {
    var p: Person
    var open: Bool
    var tags: List[Str]
    var draft: Str
    var status: Str
}

fn make() -> Model {
    var tl = List[Str]()
    tl.push("内核组")
    tl.push("工具链")
    return Model { p: Person { name: "林一", role: "编译器" },
                   open: true, tags: tl, draft: "", status: "" }
}

fn card_fold(m: Box[Model]) {
    if m.open { m.open = false } else { m.open = true }
}

fn pick_tag(m: Box[Model], row: I32) {
    m.status = "已选分组" + row.to_string()
}

fn set_draft(m: Box[Model], text: Str) {
    m.draft = text
}
```

```ct
// ───────── src/main.ct:装配层,只剩这么多 ─────────
use gui.{run_app, test_app, d_click, d_frame, d_expect_text, d_expect_absent}

fn main() -> I32 {
    read_view("app.ctml")        // 视图源声明:编译期锚/烘焙 + 运行期热重载探针
    if env_get("CTRON_GUI_HEADLESS") != "" {
        return test_app(m: make(), 420, 400, |t, actc, k| {
            d_frame(t)
            d_expect_text(t, "林一")
            d_expect_text(t, "内部联系人")
            actc(d_click(t, "pick_tag"), List[Str]())   // 行后缀名→合成 act 解码 row=0
            d_frame(t)
            d_expect_text(t, "已选分组0")
            actc(d_click(t, "card_fold"), List[Str]())
            d_frame(t)
            d_expect_absent(t, "内部联系人")
            d_expect_text(t, "林一")
        })
    }
    return run_app(m: make())    // 根 = app.ctml 末位声明视图;装配面零视图名
}
```

```ctml
// ───────── app.ctml:组合层(智能根视图,直引模型 fn 是正确分层)─────────
use components.{PersonCard, TagRow}

view Contacts (m: Model) {
  <vbox class="root">
    <PersonCard p={m.p} open={m.open} on:fold={card_fold(m)}>
      <label class="extra">内部联系人</label>
    </PersonCard>
    <TagRow title="分组" rows={m.tags} on:pick={pick_tag(m)}/>
    <input bind={m.draft} on:input={set_draft(m)}/>
    <label class="status">{m.status}</label>
  </vbox>
}

style root { direction: column gap: 10 padding: 16 }
style extra { fg: TEXT_MUTED size: 12 h: 18 }
style status { fg: ACCENT size: 12 h: 18 }
```

```ctml
// ───────── components.ctml:组件层(prop/ev 参数化,模型无关,可复用)─────────
// 前置计算块示范:var 链 + 尾置树;事件=on: 触发自身声明事件,零外部名。
pub view PersonCard (p: Person, open: Bool) ev fold() {
  var full  = p.name + " · " + p.role
  var badge = if open { "展开中" } else { "已折叠" }

  <vbox class="card">
    <label class="card_name">{full}</label>
    <label class="card_sub">{p.role}</label>
    <label class="badge">{badge}</label>
    <button class="fold" on:click={fold()}>折叠</button>
    <when cond={open}>
      <slot/>
    </when>
  </vbox>
}

pub view TagRow (title: Str = "分组", rows: List[Str]) ev pick(row: I32), refresh() opt {
  <vbox class="tagrow">
    <hbox class="head">
      <label class="tagrow_t">{title}</label>
      <button class="rf" on:click={refresh()}>刷新</button>
    </hbox>
    <each tg in={rows}>
      <button class="tag" on:click={pick()}>{tg}</button>
    </each>
  </vbox>
}

style card { direction: column gap: 4 bg: SURFACE padding: 12 }
style card_name { fg: TEXT size: 18 h: 26 font_weight: 700 }
style fold { bg: ELEVATED fg: TEXT w: 70 h: 26 hover_bg: BORDER }
style tagrow { direction: column gap: 4 }
style tag { bg: ELEVATED fg: TEXT w: 90 h: 26 hover_bg: BORDER }
```

实例形态对照:`<TagRow rows={m.tags} on:pick={pick_tag(m)}/>`(默认 title 生效、refresh 未接
= no-op)与全量形皆合法。显名回退形 = `run(Contacts(m: make()))` /
`test_view(Contacts(m: make()), 420, 400, body)`,拼写与内嵌档全同。

### 3.2 内嵌档(单文件自含,零锚零 IO)

视图(view/style)直接写在 .ct 内,装配 `run(Contacts(m: make()))` /
`test_view(Contacts(m: make()), 420, 400, body)`;其余与 §3.1 逐字同构(model/handler/
组件视图同文件平铺)。适用:小工具、示例、逻辑视图强耦合应用。域包组件同法可用
(`use gui_widgets.{gui_widgets_ver}` → GuiBlock 并池 → `<Select .../>` 直接实例化)。

### 3.3 消失的手写面(与今日对照)

| 手写面 | 今天 | 终态 |
|---|---|---|
| bind 通道(逐槽应答) | 手写(gui_contacts 24 行) | 编译器合成(bind/typed-props/prep 通道) |
| act 分发(含后缀解码) | 手写(24 行,含逐字节前缀解码) | 编译器合成(调用形直调) |
| 装配 | 三闭包 + read_file | `run(...)` / `run_app(...)` 一行 |
| headless 装配 | `ctron_embedded()` + 转发闭包 | `test_view(...)` / `test_app(...)` 一行 |
| 组件库桩堆 | ~20 空壳 fn(gui_snippets 实测) | 零(可达闭包外零臂) |
| 视图内格式化 | 塞 bind 闭包 | 前置计算块(视图面正位) |
| 组件事件合约 | 名字匹配 + 文档表(组件文件零信息) | ev 签名行自描述 + 实例点显式接线 |

## 4. 四层布局与依赖方向(文件档)

```
model.ct         领域层:Model + make + handler(零 UI)
components.ctml  组件层:prop/ev 参数化,模型无关,可复用(可进库)
app.ctml         组合层:根视图(智能层,应用私有,直引模型 fn)
main.ct          装配层:read_view + run_app/test_app
```

**依赖方向恒为单向流:装配 → 视图 → 组件。** 两条硬理由(对话已裁决):
1. 组件层绝不 import 模型——组件复用性(库件跨应用共用)全靠 prop/ev 参数化;
2. 模型实例必须从装配流入——视图可 import 的是 fn(纯代码),不是活状态;模块级
   var 是在册发射器 SIGSEGV 雷,且状态注入/热重载保状态都依赖 Box 从装配点流入。

## 5. 语法归一面(CTML 层,双档同权)

### 5.1 属性文法(补 a)

`名 = 值`,值 ∈ `"字符串字面量"` | `{表达式}` | 裸词。组件 prop 冒号形退役。
冒号豁免两处:`on:` 前缀(事件名命名空间分隔)+ style 块(CSS 文法域)。
落点:gt_parse/gui_parse.ct/检查面三处词法 + 存量夹具迁移(随 W4)。

### 5.2 事件合约(裁 6)

- **声明**:签名行 `ev 子句`,逗号列多路,各带载荷签名:`ev close(), cancel(), confirm()`、
  `ev pick(row: I32), refresh() opt`。
- **触发**:组件体内 `on:click={fold()}` 引自身声明事件;载荷由运行时上下文供给
  (行后缀解码、input 文本)或触发点显式受限实参,解码函数由声明型别选定(ev_arg_i/b/s 族)。
- **接线**:实例点 `on:名字={处理器(实参)}`,内建与组件同一语法;处理器形状被声明完全
  确定——首参 `Box[Model]`(隐式捕获),其余按位对齐载荷:`ev pick(row: I32)` →
  `fn pick_tag(m: Box[Model], row: I32)`。个数/型别不符 = E 码。
- **返回恒 Void**:事件是单向通知;"回应"走状态回流(改模型 → 下一帧数据 prop 流回)。
  当帧询问式回调不做(破坏渲染纯度与逐帧重放)。
- **组件体白名单门**:体内引用名必须 ∈ 数据 prop ∪ 声明事件,否则 E 码(拦笔误/
  漏声明/外部名泄漏三类)。根视图(宿主自有树)不设此门,直引宿主 fn(接线本就地)。
- **检查面三拦**:实例漏接必接事件 = E;接了未声明事件 = E;处理器错签 = E。
  opt 事件漏接 = 合成 no-op 臂,不告警(有意忽略的显式声明)。

### 5.3 前置计算块(补 b)

```ctml
pub view PersonCard (p: Person, open: Bool) ev fold() {
  var full  = p.name + " · " + p.role          // var 不可变绑定链,任意 Ctron 表达式
  var badge = if open { "展开中" } else { "已折叠" }   // 条件取值 = if 表达式(J20 宪法)
  <vbox class="card"> ... {full} ... {badge} ... </vbox>   // 尾置树 = 返回值
}
```

- 语句仅限根标签前;`return <视图>` 与尾置同效。赋值/循环/spawn 禁入(E 码)。
- 机制:desugar 合成 `__gui_prep_<V>(props...)`(真 Ctron fn,双宿同源,每帧帧首一次),
  结果经 bind 通道以 `pre:名` 应答;树侧 `{名}` 编译为通道查询。**树侧受限求值器(bxv)
  零扩展**——全表达式能力活在 prep fn 里。
- 红线重申:不收 `if x { return <a/> } else { return <b/> }`(静态树红线,§0.1);
  条件结构走 `when`,两副骨架 = 两个视图。
- 运行时解析器(gt_parse)同步学"根标签前 var 行"——「内嵌/独立同语法同路径」宪章
  (GUI-27 顺修同款)。

### 5.4 可选性(补 c)

- **prop 二态**:`title: Str = "分组"`(有默认,可省)/ `rows: List[Str]`(必填,漏 = E,
  E8110 族沿用)。默认值限编译期常量(字面量/可折叠表达式);prop 间引用 v1 不做;
  类型不符 = E。**不做"无默认的可选 prop"**(Optional 在 GUI 通道是在册雷区,
  tri-state 在 Model struct 内建模)。
- **事件二态**:必接(默认)/ `opt`(显式豁免,漏接 = no-op 臂)。组件作者责任随附:
  opt 事件触发点照常渲染,要藏用 `when` + 数据 prop 自行门控(v1 不自动隐藏)。
- 根视图同规则:`run_app(m: make())` 省略带默认 prop,默认生效。
- 合成落点:默认值折进 prop 表作兜底应答臂;`opt` 进事件表选 no-op 臂。

## 6. 装配面(裁 5 = A′)

| 档 | 真窗 | headless |
|---|---|---|
| 文件档(默认锚根) | `run_app(m: make())` | `test_app(m: make(), w, h, body)` |
| 显名回退(两档通用) | `run(Name(props...))` | `test_view(Name(props...), w, h, body)` |
| 内嵌档(唯显名) | 同上 | 同上 |

- **`read_view("app.ctml")`**:新内建(ctron_embedded 同族,三处注册 eval/sem/emit)。
  编译期 = 锚(desugar 视图源 + 检查源 + 烘焙);运行时 = 读盘为主、烘焙副本回落
  (「读视图永不因 CWD 崩」内建语义),热重载环以它为探针。取代「main 首个
  read_file 字面量为发射锚」启发式(gui_files 类应用歧义消除)。`read_file` 保持
  任意 IO 原义。components.ctml 不经它手(use 行在解析层并入)。
- **`run_app`/`test_app`**:desugar 拦截形(与 run 同族,不进普通函数解析),靶 =
  锚文件合并源末位声明视图(GUI-27「应用根=末位声明」升编译期);具名实参按根
  prop 表类型检查。test_app 的 w/h/body 位置实参,文法 desugar 自定,零运行时面。
- **target 定位管道现成**:靶视图居末 + `__ordn__` 头携带靶名(gui_ds_target 修后形态),
  靶名来源从「调用点拼写」换「锚文件末位」仅增量一个判别。
- 名字出现规律一条:**视图在哪儿,名字才在哪儿**——内嵌 view 在本文件,名字即出处;
  文件档视图在锚文件,装配面零名。

## 7. 编译期合成与可达闭包(裁 1/裁 2 落点)

- **收集改可达闭包**:从装配靶起沿 `call:` 实例边 worklist 走查(平面数组上标准
  算法),闭包外视图零贡献——桩消失。s49 的多树根修正保留(靶根必在闭包内)。
- **合成四件**:`__gui_bind_<V>`(含 typed-props/prep 通道臂)、`__gui_act_<V>`
  (调用形直调 + 行后缀 ev_arg 解码 + 事件名×实例序分发)、`__gui_run_<V>`
  (文件档持锚路径:60 帧比对/原址重解析/缺文件回落烘焙)、`__gui_prep_<V>`(§5.3)。
- **库组件迁移**:gui_widgets 16 件的事件头名字合约(sel_toggle/sel_pick/…三路
  Dialog 等)迁 `ev` 声明;迁后库件对宿主零名字耦合,实例点按 `on:` 接线。
- **诊断码**:新 E 码按 spec §10 注册表登记 + e8_corpus 负例随件(漏必接事件/
  错签/未声明名/漏必填 prop/默认值型别/前置块禁面)。

## 8. 机制红线(违者即设计回退)

1. 静态树四基石不可破(§0.1)。
2. 值单一起源:任何"视图/组件直接抓状态"形态不收(§0.2,§4)。
3. 双端同形:每条新文法(前置块/`=`/ev/默认值/opt)必须编译器骨架与运行时 gt_parse
   同步实现,差分夹具随件(s22 系双端差分继续作门)。
4. 热重载语义:文件档合并源字节比对恒等性不受扰(无 use 行文件字节恒等,
   w4_hotreload/s26 回归保障)。

## 9. 波次排期(每件:mini 计划先行 → 夹具先行 → 双口径验证 → 阶梯全绿 → 落库记忆)

| 波 | 内容 | 缝级 | 主要落点 |
|---|---|---|---|
| W1a | 属性 `=` 归一(冒号形退役)+ 夹具迁移 | 小 | gui_parse 词法三处 + gt_parse + e8 |
| W1b | 桩裁剪(可达闭包)+ 合约 E 码族 + gui_snippets 桩清零实证 | 小 | gui_parse.ct(与在飞发射三文件零交集) |
| W2 | `run`/`test_view` 装配收口 + Driver 持 bind(`d_frame(t)` 零闭包) | 小 | gui_parse.ct + pkgs/gui/gui_driver.ct |
| W2.5 | 前置计算块(prep 合成)+ ev 声明/on: 接线 + 默认值/opt + gui_widgets Dialog 三路迁移实证 | 中 | gui_parse.ct + gt_parse + gui_widgets |
| W3 | 文件档锚源接线:`read_view` 内建(三处注册)+ 锚文件进 desugar(`gui_ctml_file_use` 已就绪,纯接线)+ `run_app`/`test_app` 合成(靶=锚末位视图) | 中 | gui_parse.ct + driver_emit(**在飞文件,先与发射泳道对时**) |
| W4 | 存量示例迁移(todo/gui_counter/gui_contacts/gui_calc)+ README 入口 API 收口 + 旧三闭包标 legacy | 零(文档+示例) | examples + pkgs/gui/README |

## 10. 验收与门禁(惯例全量)

- 夹具:每波新增 s 系夹具(建议槽 s93 起,执行前实测再占);e8 语料负例随诊断码;
  双端差分(s22 系)与 w4 热重载族零回归;阶梯全绿 + suite 双列 + smoke。
- 迁移实证载体:gui_contacts → 四层布局终态(§3.1 全量对照,main 141 行 → ~40 行);
  gui_snippets → 桩堆清零;gui_widgets Dialog → ev 三路合约首件。
- decls 锁申报 + COVERAGE 本日记;诊断码 spec §10 登记;机刷协作守则照旧
  (pathspec 限定、发射三文件在飞件对时)。

## 11. 非目标与备案

- **不做**:一等视图值(`read_view` 返回可调视图)/ 当帧询问式事件回调 /
  prop 间引用默认值 / Optional 进 GUI 通道 / opt 事件触发点自动隐藏 /
  按值返回不同子树(静态树红线)。
- **备案(触发即议)**:语言核心引入一等函数值后,handler-prop 可作 ev 声明的
  换皮形(接线点 `on:` 语法不动);语言级一等视图(fn 类型化 view)另立大档。
- 旧名字形事件 `{inc}` 与旧三闭包入口:降 legacy 保留兼容,文档不再出现。
