# Ctron GUI 开发设计规范(CTML)——设计记录

> 状态:**设计冻结草案**(2026-09-16 设计会话裁决记录;同日完整性审计修订落地，
> C1 裁决 = 颜色一律 Str 字面量,C2–C5/G/F/T 各条见文内"裁决"标记;六项二选一裁决
> 定案 = 1A/2A/3A/4B/5A/6A,悬项清零)。实现未排期，GUI 是新泳道,
> 排期须登记 `spec-completion-roadmap` 并与在途切片协调发射器改动面。
> 归化路径:本文 §4(CTML 语言)/§5(样式)在实现落地后分别并入 `docs/spec/` 新章与
> std 域文档;§10 错误码段经 §10 注册表核对后登记。
> 上游依据:自举成熟度评估(compiler/ 固定点全绿)、FFI v0 现状(§9.6 兑现欠账)、
> 2026-08 Rust GUI 实测等社区证据(§2)。

## 0. 本文回答什么

Ctron 的 GUI 开发采用 **web 式体验**:标签组织结构、CSS(子集)写样式、Ctron 写事件与逻辑。
本文给出:目标/非目标、证据链与痛点清单、总体架构与底层选型、CTML 语言设计、样式系统、
运行时与数据流、里程碑、风险防线、被否决的备选案,以及一次完整的应用开发演绎(§10)。

---

## 1. 目标与非目标

### 1.1 目标

1. **声明式结构**:UI 是状态的函数;标签树 + 属性绑定表达结构,不手写控件排布代码。
2. **样式与逻辑解耦**:改外观不碰逻辑;样式可复用(令牌/共享类),复用机制确定无歧义。
3. **热重载**:保存即见,**不丢应用状态**。
4. **HTML/CSS 心智零成本**:语法贴 HTML/CSS,人类与 AI agent 都以既有词汇表工作。
5. **中文一等**:CJK 显示与 IME 输入是验收项,不是遗留项。
6. **产物口径**:静态单二进制(§9.7),无 webview、无运行时解释器、无 GC 停顿感知。
7. **纯 Ctron 开发面(2026-09-19 目标增补)**:应用默认**零 C 文件**，用户只写
   `.ct`/`.ctml`。C 仅存在于两处:①gui 域库内部的固定桥(measure/flush,§11.2/§12.3,
   用户不写、不携带、源码树不可见);②**逃生口**，域能力未覆盖时,用户经既有 FFI
   通道(§9.6)自写 C 或接入第三方库,属例外路径而非常规开发体验。

### 1.2 非目标(明示不做)

- **完整 CSS 语义**:cascade/specificity 计算、元素选择器、grid、动画、继承链
  ，均不进第一版(§5.4 红线)。
- **web 档 GUI**:浏览器渲染走 §9.2 stdweb 既有路线,与本文互不替代。
- **原生控件外观**:自绘渲染,视觉是自己的(对标 egui/Slint,不模拟系统控件)。
- **无障碍完整交付**:标记语义位(role/label)第一天就有,读屏树完整对接属 M4/P3。

---

## 2. 证据链(设计依据)

### 2.1 社区痛点清单(2026-08 Rust GUI 实测 55 框架 + QML/Slint/Tauri/RmlUi 反馈)

| # | 痛点 | 证据 | 本文对策 |
|---|---|---|---|
| P1 | 文本输入/IME 是淘汰线 | 实测仅 Slint/egui 全过;GPUI 无内置文本输入、composer 崩溃;多家 composer 不可见/错位 | 受控 input + composer(marked range)进 M3 架构,不留事后补 |
| P2 | 无障碍是自绘框架坟场 | 自绘几乎全灭;存活者靠 AccessKit 级基础设施或原生控件 | 标记内建 role/label 语义位;完整对接 M4 |
| P3 | 状态管理决定 API 形态 | Elm 全局状态抽象泄漏;细粒度响应(属性绑定)是主流趋势 | 绑定表达式直读 model 字段 + 脏追踪失效,不走 Elm 事件循环 |
| P4 | DSL 无工具链即死 | Makepad live_design 零文档不可用;Pane UI schema 不全;Slint 因编辑器/live-preview 加分 | 绑定表达式 = Ctron 本体(编译器同源);LSP/fmt 免费继承 |
| P5 | 热重载丢状态是真实抱怨 | Tauri reload 丢 UI 状态被点名 | §6.3:标记/样式热替换,状态树不重建 |
| P6 | 体积/内存是原生自绘卖点 | Rust GUI hello-world 依赖占地 1–6GB/框架;webview 内存高企 | 静态单二进制,comptime 折叠,release 零运行时解析 |
| P7 | 全量 web 引擎是专门项目量级 | RmlUi/Blitz 十万行级;LVGL 连 XML 引擎都从 9.5 撤了 | CSS 做**子集**并冻结(§5.4),不为兼容性破例 |

### 2.2 先例定位

- **Slint / QML**:专用声明式 UI DSL 的成功模板(live-preview、属性绑定、围绕 DSL 建生态);
  教训 = 跨语言摩擦,故本文绑定表达式内联 Ctron(§4.4)。
- **RmlUi(继承 libRocket)/ Sciter**:HTML/CSS 子集在原生应用/游戏中可行,但引擎体量
  证明不能自研全量;Sciter 的商业双许可反证开源子集路线的价值。
- **Dioxus→Blitz**:2026 年转向自建 HTML/CSS 渲染引擎,动机"AI agent 需要能渲染 HTML 的
  东西"——HTML 兼容语法是 agent 时代的分发优势,本文采纳(§1.1 目标 4)。
- **egui / Nuklear / raygui**:立即模式验证了"拉模式零回调"在 FFI 受限期的可行性,
  但不满足声明式结构目标，本文取其底层契约(§3.2),弃其交互模型。

### 2.3 "web 体验"的五要素拆解

声明式结构 / 样式与逻辑解耦 / 热重载(保状态)/ 心智零成本 / agent 协作友好。
标记与 CSS 只是载体,五要素才是验收口径。

---

## 3. 总体架构与底层选型

### 3.1 分层

```
┌────────────────────────────────────────────────────┐
│ app.ctml(结构+样式)      app.ct(逻辑:状态+事件) │ ← Ctron 源码
├────────────────────────────────────────────────────┤
│ CTML/样式编译器(comptime 期解析与折叠)            │ ← Ctron 自研
│ retained 元素树 + 属性绑定 + 事件分发 + 热重载     │ ← Ctron 自研
│ 控件组件库(button/input/checkbox/list…)           │ ← Ctron 自研
├────────────────────────────────────────────────────┤
│ 布局引擎:Clay(flex/box → 绘制命令缓冲)          │ ← C 库(FFI)
│ 文本:raylib 内置字体 → FreeType + SheenBidi       │ ← C 库(FFI)
│ 窗口/输入/渲染:raylib                              │ ← C 库(FFI)
├────────────────────────────────────────────────────┤
│ 操作系统(Win32/Cocoa/X11-Wayland + OpenGL)        │
└────────────────────────────────────────────────────┘
```

### 3.2 底层选型依据(回调密度 = Ctron FFI 的一票否决项)

| 层 | 选型 | 契合性 | 替换路径 |
|---|---|---|---|
| 窗口/输入/绘制 | raylib | 拉模式零回调,FFI v0 可绑 | 窗口层→sokol 式自写 shim;绘制→SDL3 SDL_gpu |
| 布局 | Clay | 命令缓冲输出,除字体测量外零回调 | 布局协议不变,上下两层任意换 |
| 文本 | FreeType + SheenBidi | 纯调用数据驱动 | — |
| (M3)IME | 平台 shim(薄 C) | 需 C-ABI 回调(§7 前置) | — |

- **Clay 命令缓冲是关键隔离层**:上层(Ctron 控件)与下层(渲染后端)被它解耦,
  raylib→SDL_gpu→自写 shim 的换代不触及 UI 代码。
- 落选记录:GLFW(回调架构)、GTK4/Tk(GObject/Tcl 对象模型)、LVGL(回调密集且
  控件层留在 C,违背 dogfooding 目标)、Nuklear 直绑(控件层必须 Ctron 自研,见 §9)。

### 3.3 FFI 前置欠账(实现本设计前必须兑现的切片)

> **状态注记(2026-09-16 审计后更新)**:切片 1/2/3 已由 FFI 泳道在工作区兑现
> (`tests/ffi/` 11/11 绿:repr_c / str_marshall / callback / abi_width;未落库,
> 提交为前置)。本节保留原始需求记录。

1. `#[repr(c)]` 最小切片:`Vector2/Color/Rect` 按值传递(raylib 绑定的全部 struct 需求);
2. `Str → const char*` 只读借用编组(§9.6 三约定中最薄的一条);
3. (M3 前)C-ABI 函数指针:IME 平台 shim 与未来事件回调共用;
4. 诊断:GUI 段错误码预留 E8100–E8999(经 §10 注册表核对后登记)。

---

## 4. CTML 语言设计

### 4.1 文件与组织

- 扩展名 **`.ctml`**(Ctron Markup Language)。`.ui` 否决:Qt Designer/GTK Builder
  已占用该扩展名的工具链语义(§9)。
- **单文件组件**:结构与样式同文件(`view` 块 + `style` 块),不设独立样式文件
  (样式复用走 §5 机制,不靠全局样式表)。
- 两种形态,**内嵌优先**:
  - **内嵌形态(M0–M2)**:`view/style` 块直接写在 `.ct` 内，编译器/LSP/fmt/comptime
    全部免费继承,规避"DSL 无工具链即死"(P4);
  - **独立形态(M2 起)**:`.ctml` 独立文件,供设计者协作与 agent 生成;经 `use` 导入。
  - 两形态同语法同编译路径,差别仅在物理位置。
- 包配置按 `.ctml` 后缀收集(glob 规则随 P2-B 定;清单文件 = `Ctron.ctcl`,v1 提案)。
- **UI 入口声明(2026-09-19 增补,J18,源:gui-master-design §10.6)**:包清单 `Ctron.ctcl`
  增 `gui` 节声明 UI 入口,构建期烘焙为 gui 域默认锚——应用代码零文件感知:

  ```ct
  // Ctron.ctcl
  pkg {
      manifest_version = 1
      name = "gui_calc"
      version = "0.1.0"
  }
  gui {
      entry = "ui/calc.ctml"      // 相对包根
  }
  ```

  - **三级优先级(规范性)**:显式 `run_file(src, …)` > 清单 `gui.entry` > 默认
    `"app.ctml"`(零配置约定)。三级各司其职:自定义内容 / 包级声明 / 快速起步;
  - **机制 = 构建期烘焙**:emit 驱动读清单键 → 烘焙为 `run_d`/`run_kb_d` 家族默认锚;
    产物**不反向运行期读清单**(清单是构建元数据,避免新 CWD 依赖)。路径声明为默认,
    内容内嵌(真单二进制)随 dist 域裁决;
  - **收益**:dist 打包读同键即知携带物(UI 文件单一真源);多 view/多窗口经
    `views = [...]` 自然扩展;L2 骨架编译期化后键不废弃,语义降级为"开发期源文件
    位置"(热重载监听目标);
  - **落点**:emit 驱动读清单键(ctcl 键定位复用 caps 检查基建),随 P2-B CTCL 清单
    合流。此前的"main 首个 read_file 字面量锚"约定降级为无清单声明时的回落默认。

### 4.2 语法(规范性草案)

```text
File        → (UseDecl | ViewDecl | StyleDecl)*
              // UseDecl(v10 增补,2026-09-19):独立 .ctml 顶层 use,供绑定/事件
              // 表达式引用域操作(作用域规则同 §4.2 要点 2′);内嵌形态免此行
ViewDecl    → "view" IDENT PropList? Block
PropList    → "(" Prop ("," Prop)* ")"             // props 必填(§4.3 契约 2)
Prop        → IDENT ":" Type
Block       → "{" Element* "}"
Element     → OpenTag (Content ElementClose)? | SelfClose
OpenTag     → "<" IDENT Attr* ">"
Attr        → IDENT "=" Literal              // 静态属性
            | IDENT "=" "{" Expr "}"          // 绑定表达式(Ctron 表达式)
            | "on:" IDENT "=" "{" Expr "}"    // 事件绑定(函数/闭包引用)
            | "class" "=" STRING_LIT          // 样式类(空格分隔多类)
            | "class" "=" "{" Expr "}"        // 动态类:求值 Str;与静态并存时合并序
                                              // = 静态在前、动态在后(§5.2 规则 3)
Content     → TEXT | "{" Expr "}" (插值) | Element*
ControlElem → "<each" IDENT "in" "=" "{" Expr "}" ("key" "=" "{" Expr "}")? ">"
              Element* "</each>"
            | "<when" "cond" "=" "{" Expr "}" ">" Element* "</when>"
SlotElem    → "<slot" ("name" "=" STRING_LIT)? "/>"       // 内容分发占位
StyleDecl   → "style" IDENT StyleExt? "{" Prop* "}"
StyleExt    → "extends" IDENT                 // 单亲派生,仅限本文件
```

要点:

1. **标签即组件**:`<vbox>`/`<hbox>`/`<label>`/`<button>`/`<input>`/`<checkbox>`/
   `<spacer>`/`<img>` 为内建集;`<FooBar …/>` 形态(大驼峰)引用本文件或 `use` 导入的
   **view 组件**,实现组合复用。
2. **绑定表达式是 Ctron 表达式**:`visible={count > 0}`、`text={fmt("{}", name)}`，
   编译期类型检查与宿主语言贯通(QML 靠 JS 弱类型、Tauri 靠 IPC 类型补丁,此处同源免疫)。
2′. 绑定作用域(规范性):绑定/插值表达式可引用 ① view 的 props(含宿主 model)、
   ② 词法包围它的 each 绑定变量、③ 所在编译单元的顶层声明(fn/const,含 `use` 导入);
   表达式必须**纯读**，出现赋值或可变方法调用即 E8190(裁决 4B:纯度门与
   类型门 E8110 分码),子组件回写父状态为 E8170。
3. **事件(2026-09-17 可读性评审修订:自动闭包语义)**:`on:` 前缀及 **fn 类型
   props** 的 `{expr}` 由编译器自动包为闭包、**事件触发时求值**,允许副作用，
   示例 `on:click={add_todo(model)}` 即"点击时调用",无需显式闭包语法;处理函数
   变更 model 后由运行时统一失效(§6.2),签名不匹配仍为 E8120。
   与绑定表达式的分工(同一对花括号,两条规则):普通属性 `{}` = 绑定槽
   (失效重算,§6.1)、受纯读门 E8190;`on:`/fn props 的 `{}` = 事件时求值、
   免纯读门——差异由前缀/类型可见标记,文档与 LSP 悬浮提示必须并列呈现两条。
4. 结构控制用元素形态:`<each>`(列表)/`<when>`(条件),保持"一切都是标签"的
   心智模型。
5. **插值**:`计数:{count}` 文本插值,与 Ctron 字符串插值同语法。
6. **语义位**:`role`/`label` 属性对全部元素可用(M4 消费,P0 起解析占位)。

### 4.3 组件模型与组合(规范性)

- `view X` 编译为:X 的静态骨架(元素树形状)+ 绑定槽表(属性/文本/类到
  Ctron 表达式的映射)+ **事件表**。骨架 comptime 折叠为常量数据,release 零解析。
- 组件 props = view 名后的 `PropList`,编译为对宿主 struct 字段/参数的只读引用;
  props 变更经绑定槽失效,不做虚拟 DOM diff。

**组合三原则(规范性,MUST)**:

1. 组合唯一:组件之间互相引用、嵌套、分发内容,全部经组合完成，
   组件间不设继承(结构/行为/样式均无 view 级继承)；
2. **显式引用**:跨文件组件复用必须经 `use` 显式导入;同文件互引用合法
   (同一编译单元,声明顺序无关);跨文件循环引用由 §2 模块循环禁令拦截;
3. **复用三通道分离**:视觉复用→样式系统(§5 令牌/extends),行为复用→
   Ctron 函数/trait,结构复用→props+slot;禁止混道表达(如以样式继承表达行为差异)。

在此之上,组件交互遵循五条契约:

1. **引用**:PascalCase 标签 = view 组件。同文件直接引用;跨文件 `use` 导入
   (view 默认文件私有,`pub view` 可导出，可见性模型与 §5 样式类一致)。
   循环引用被模块循环禁令(§2)天然拦截;**自引用合法**(树形递归渲染)。
2. **Props 单向向下:父经属性绑定传数据与回调,子只读;子组件禁止**回写
   父状态(单向数据流,违者 E8170)。props 全必填，缺失或多余 = E8100;
   字面量默认值语法**不引入**(裁决 2A,定案)。
3. **事件向上**:子组件的对外事件 = fn 类型的 prop;父侧 `on:xxx={handler}` 是
   fn 类型 props 的语法糖，与内建元素同一套机制,无第二事件系统。
4. **Slot 内容分发**:`<slot/>`(默认)/`<slot name="…"/>`(具名)是子 view 的
   占位;父在组件标签内的子元素按序填入默认 slot,带 `slot="…"` 属性者填入具名
   slot(web-components 同款约定)。
5. **状态归属**:组件自身无可绑定状态，状态只存于 model(经 props 下传)
   与运行时本地(焦点/hover/展开,运行时持有、不进绑定表达式)。view 局部可绑定
   状态**不引入**(裁决 3A,定案):交互态一律上浮 model 或归运行时本地。

视觉复用走 §5(令牌/样式 extends),行为复用走 Ctron 函数/trait,
结构复用走 props+slot，三种复用三条通道,不混入继承。

```ct
// card.ctml —— 可复用组件:props 向下接收,slot 分发内容,事件向上
pub view Card(title: Str, on_close: fn()) {
  <vbox class="card">
    <hbox class="card-head">
      <label class="card-title">{title}</label>
      <button class="ghost" on:click={on_close}>×</button>
    </hbox>
    <slot/>
  </vbox>
}

// page.ctml ， 消费方:组合 Card,填入内容与具名 slot
use card.Card

view Page(model: Model) {
  <Card title={model.user.name} on:close={dismiss(model)}>
    <label slot="tip">拖拽排序已启用</label>   // 填入具名 slot(若声明)
    <label>正文内容…</label>                    // 填入默认 slot
  </Card>
}

// tree.ctml ， 自引用递归(树形数据渲染的唯一合法递归形态)
view TreeNode(node: Node) {
  <vbox class="tree-node">
    <label class="row">{node.name}</label>
    <each child in={node.children}>
      <TreeNode node={child}/>
    </each>
  </vbox>
}
```

组件递归深度与 `<each>` 展开均受 E6010 同源的 comptime 预算约束(骨架编译期),
运行时树深由数据规模决定、无静态上限。

#### 4.3.1 引用与互引用(细则)

- **同文件互引用**:两个 view 可互相引用(编译单元内定点解析,声明顺序无关)，
  案例A 中 `Field` 先于 `ErrorTip` 定义即合法;
- **跨文件引用**:`pub view` 导出 + `use card.Card` 导入名字后,PascalCase 标签即组件;
  未导入的未知标签 = E8100;
- **自引用**:合法,是树形数据的唯一递归形态(§4.3 末 `TreeNode`);
- **可见性**:view 默认文件私有,`pub view` 可导出，与 §5 样式类同规则。

```ct
// form.ctml ， 案例 A:同文件互引用(Field 引用其后定义的 ErrorTip)
view Field(label: Str, error: Str) {
  <vbox class="field">
    <label class="field-label">{label}</label>
    <slot/>
    <when cond={error.len > 0}><ErrorTip text={error}/></when>
  </vbox>
}
view ErrorTip(text: Str) { <label class="error">{text}</label> }

view ContactForm(m: Model) {
  <Field label="邮箱" error={email_error(m.draft.email)}>
    <input bind={m.draft.email}/>
  </Field>
}
```

#### 4.3.2 组合案例集

**案例 B:列表组合(行组件抽取)**——数据经 props 进、事件经回调出,
行组件自身无状态,多页面复用零适配:

```ct
pub view TodoItem(todo: Todo, on_toggle: fn(), on_remove: fn()) {
  <hbox class="item">
    <checkbox checked={todo.done} on:toggle={on_toggle}/>
    <label class={todo.done ? "done" : "undone"}>{todo.title}</label>
    <spacer/>
    <button class="ghost" on:click={on_remove}>×</button>
  </vbox>
}

// 消费:each 展开即组件实例化
<each todo in={model.todos}>
  <TodoItem todo={todo} on_toggle={flip(todo)} on_remove={remove(model, todo)}/>
</each>
```

**案例 C:布局壳(具名 slot 的结构复用)**，一壳多页:

```ct
pub view Shell(title: Str) {
  <vbox class="shell">
    <label class="shell-title">{title}</label>
    <hbox class="shell-body">
      <vbox class="sidebar"><slot name="sidebar"/></vbox>
      <vbox class="main"><slot/></vbox>
    </hbox>
  </vbox>
}

// 三个页面共享同一壳:
<Shell title="设置">
  <vbox slot="sidebar"><NavItems/></vbox>
  <FormBody/>
</Shell>
```

**案例 D:事件不冒泡**，子组件不向父冒泡事件;父把 handler 当 props 传入,
子只调用自己声明的回调。深度嵌套时逐层透传是组合的显式代价:透传超过两层
应提升状态而非加转发层(风格指引,lint 预留,不作硬诊断)。

#### 4.3.3 复用三通道案例

| 通道 | 机制 | 案例 |
|---|---|---|
| 视觉 | 令牌 + `pub style` + `extends` | §5.2 `btn`/`btn-danger`:同一 `Button` 组件,换 class 即换外观,无 DangerButton 子类 |
| 行为 | 普通 Ctron fn/trait | `email_error` 被注册页与设置页的 input 绑定共用;trait 约束一组组件的最小回调集 |
| 结构 | props + slot | 案例 C:一壳三页 |

```ct
// valid.ct ， 行为复用 = 普通函数,与 UI 零耦合
pub fn email_error(s: Str) -> Str {
    if s.contains("@") { return "" }
    return "邮箱格式不正确"
}
// 多处消费(注册页/设置页),绑定表达式直调:
<Field label="邮箱" error={email_error(m.draft.email)}>…</Field>
```

#### 4.3.4 反例对照:继承式设计的组合翻译

| 其他框架的继承形态 | Ctron 组合翻译 |
|---|---|
| `DangerButton extends Button` | 同一 `Button` 组件 + `style btn-danger extends btn`(§5)，继承发生在样式维度,组件维度零继承 |
| 抽象基类提供默认行为 | 行为提取为普通 fn,多处经 props 注入同一 fn 值 |
| 模板方法模式(子类覆写步骤) | 壳组件 + 具名 slot 注入各步骤 |
| `Panel extends Container extends Component` | `<Panel><slot/></Panel>` 嵌套 + Shell 壳(案例 C) |

### 4.4 诊断(预留)

| 码(预留) | 场景 |
|---|---|
| E8100 | 标签/属性未声明或拼写(view/内建集外且无导入) |
| E8110 | 绑定表达式类型不匹配属性契约(如 `visible={count}`,非 Bool) |
| E8190 | 绑定表达式纯度违规(赋值/可变方法调用,§4.2 要点 2′)(裁决 4B:与类型门分码);**仅普通属性绑定**，`on:`/fn 类型 props 自动闭包免此门(§4.2 要点 3,2026-09-17) |
| E8120 | 事件处理器签名不匹配 |
| E8130 | 样式属性不在子集内(§5.1)、值类型错误,或颜色串非 `#RRGGBB[AA]` 格式(§5.2) |
| E8140 | `extends` 目标不存在/跨文件(禁) |
| E8150 | `class` 引用了未导入且非本地的样式 |
| E8160 | `<slot>`/`slot=` 引用无匹配宿主(具名不存在/组件标签外使用 slot 属性) |
| E8170 | 子组件绑定表达式回写父 model 状态(单向数据流违规) |
| E8180 | `<each>` key 表达式类型不满足 Eq(编译期);**key 重复为运行时契约**:dev/测试口径断言,release 按首次出现保留 |
| E8191 | 布局静态溢出:全 fixed 且无 wrap 子树,Σ子尺寸+gap+padding > 容器 fixed(2026-09-17 §7,经注册表核对后登记) |
| E8192 | 死 fill:fill/grow 落在运行时必然零剩余空间的链(同上) |
| E8193 | **警告级(可关)**:`<each>` 无 key 且 each 体含输入型控件(input/checkbox/textarea)——索引对齐下重排将静默串位(2026-09-17 可读性评审;随 W3 检查面) |

---

## 5. 样式系统

### 5.1 CSS 子集清单

**支持(2026-09-17 改定:暴露面 = Clay v0.14 能力面全量映射)**:
box model(padding 四边;无 margin，Clay 无此概念,间距用 gap+padding+spacer)、
flex(direction/gap/align/justify/wrap)、sizing(fixed/hug/fill + **percent** +
**min/max 约束**)、视觉(bg/color/border(四边独立 + betweenChildren)/
radius(四角独立)/opacity)、字体(font-size/weight/family + **text-align/
letter-spacing/line-height/wrap**)、伪类(`:hover`/`:active`/`:focus`)、
类选择器(仅 `.class` 单类形态)、**aspect-ratio**、scroll(水平+垂直)、
**锚定浮层**(attachTo = 父/指定 id/窗口根,吸附点 + offset + expand + zIndex +
pointerCapture + clipTo)、元素 `id` 属性(浮层锚定;M4 语义树复用)、image、
CUSTOM 自绘逃生口。

**语义钉死(2026-09-17 可读性评审)**:`gap` = **主轴**子间距(vbox=行间、
hbox=列间),非 CSS 双轴;wrap 换行方向间距 Clay 无原生(登记缺口,引擎升级
再评),当前配方 = 行容器交替 padding。`padding` 只允许单值(四边同值)或具名
四边(`padding-left` 等),**不做 CSS 多值简写**(无 TRBL 记忆坑)。长度一律
逻辑像素整数,单位隐含(C4)。

**不支持(红线 = Clay 没有的语义)**:cascade 与 specificity 计算、元素/后代
选择器、`!important`、继承链、grid(Clay 无此能力,引擎对齐即"无")、动画/过渡、
`@media`(断点后置于容器查询形态另议)、margin(不设脱糖)、瀑布流 masonry
(gui 域库组件承担:上一帧几何列平衡)。

**运行时职责边界(2026-09-17)**:Clay 的 query/debug API(`Clay_PointerOver`/
`OnHover`/`GetElementData`/debugMode)**不暴露**，命中/焦点/失效归运行时
(§12.2 单一真源);查询 API 仅作 provenance/inspector 的内部数据源。

**单位语义(C4 组补遗)**:全部长度为**逻辑像素**，物理像素 = 逻辑值 × DPI scale
(scale 每帧校验,§12.3a);不设其他单位。

### 5.2 复用四机制(全部骑 Ctron 既有设施,零新语言规则)

1. **设计令牌 = comptime 常量**:颜色/间距/字号即带类型常量,放普通 `.ct` 模块;
   改主题 = 改一个模块;令牌类型化(Color/Length),拼错是编译错误。

   ```ct
   // theme.ct ， C1 裁决:颜色一律 Str 字面量,无专用类型/记号(§9 否决 `#` 色字面量)
   pub const COLOR_BG:     Str = "#0f172a"
   pub const COLOR_ACCENT: Str = "#2563eb"
   pub const SPACE_MD:     I32 = 12
   ```

   颜色串由 gui_lower 在 comptime 校验 `#RRGGBB` / `#RRGGBBAA` 格式,非法 = E8130;
   间距等长度为 I32 逻辑像素(§5.1)。
   **style 块作用域(2026-09-17 可读性评审)**:与绑定作用域同一规则(§4.2 要点 2′)
   ，所在编译单元的顶层声明(fn/const,含 `use` 导入);默认主题经显式
   `use gui.theme` 导入,无隐式注入。

2. **共享样式 = `pub style` + `use`**:组件文件内 `style` 默认文件私有,`pub` 即导出;
   消费方 `use` 导入——解析规则"本地优先,再查导入",确定名字查找替代 specificity。

   ```ct
   // styles.ctml
   pub style btn { padding: 8 16; radius: 6; bg: COLOR_ACCENT }
   pub style btn-danger extends btn { bg: "#dc2626" }
   ```

3. **消费侧合并规则(规范性,两条字面规则替代整条 cascade)**:
   - **多类合并**:`class="btn btn-danger"` 按 class 列表顺序,同属性**后者覆盖前者**;
   - **extends**:单亲、限本文件、只覆写属性;跨文件派生一律走"令牌 + 多类"。
4. **组件库命名空间**:第三方控件库随包 `pub style`,模块名即命名空间
   (`use cardlib.styles`),不设 `@import`/scoped-attribute 补丁机制。

### 5.3 编译期折叠

令牌求值、extends 展开、多类合并全部在 comptime 期完成,产物是**静态样式表常量**
，release 零运行时解析,热重载时按模块依赖增量重编(§6.3)。

### 5.4 规范红线(冻结)

禁止元素选择器与全局样式表;复用必须经显式导入发生。 此线守住,样式系统不会
长回 cascade 泥潭;破例即推翻 §2.1 P7 的整个前提。

---

## 6. 运行时与数据流

### 6.1 retained 树 + 细粒度绑定

- view 实例化生成 retained 元素树(非虚拟 DOM,无 diff);绑定槽持有
  "表达式 → 元素属性"的订阅关系。
- 状态 = 普通 Ctron `class` 实例(引用共享,§6.1 语义);绑定表达式直读字段。
- 列表对齐(规范性):`<each>` 可选 `key={…}`(须满足值语义 Eq)。有 key 按 key
  对齐复用元素，重排/中间删除不串位、输入态不漂移;无 key 按索引对齐(要求
  each 体不含输入型控件);热重载的元素对齐沿用同一条规则。key 重复 = E8180。

### 6.2 事件流

```
平台输入(raylib 轮询) → 运行时命中测试 → 事件表分发 handler(model 可变写)
  → 事件返回后统一失效(脏绑定槽) → Clay 布局(**全量重放**,增量在求值缓存层,C3)
 → 命令缓冲 → raylib 绘制
```

失效是事件边界的批量动作,不做每表达式即时传播;细粒度体现在"只重算脏槽",
不体现在即时性。

线程模型(规范性):UI 单线程，元素树、事件分发、失效与绘制全部在主线程;
后台计算用既有 spawn/channel(§7),运行时在主循环 poll 通道并把到达消息投递为
普通事件(gui 域提供 `Task → on:done` 封装);元素树与失效队列为主线程私有,
view 名出现在 spawn 闭包捕获中按 E3020 同源拒绝。

异常语义(规范性):handler 内 panic 遵循 §5 进程级语义(展开至 main、进程退出,
含 P1-A2 Drop 展开);错误边界/对话框**不引入**(裁决 5A,定案)。

### 6.3 热重载(保状态,规范承诺)

- 监听 `.ct/.ctml` 变更 → 仅重编译变更模块的骨架/样式/绑定槽 → 状态树原址保留,
  元素树按骨架结构对齐(新增元素创建、删除元素销毁、形状不变者复用);
  本条承诺的宿主是解释口径(§11.5),原生口径降级路径见同节。
- 验收口径:改样式/结构/逻辑三者之一,输入框焦点与半提交的 IME 组词不丢。

### 6.4 文本输入与 IME(M3)

- `<input>` 为受控组件:`bind` 双向绑定;运行时维护光标/选区/合成窗口。
- IME composer(标记文本 marked range)是一等状态,进元素树可观察面，实测证明
  这是淘汰线(P1),不允许"输入框能出字但组词不可见"的实现。
- 平台侧经薄 C shim 暴露 GLFW/原生 IME 合成事件(依赖 §3.3-3 的 C-ABI 回调欠账)。
- **M0–M2 能力边界(规范性)**:`GetCharPressed` 经平台字符路径可收 IME 上屏文本
  ，"能输入中文、组词预编辑不可见"是本阶段的定义形态,非缺陷;M3 起补 preedit。

### 6.5 内存口径

事件驱动低频分配 + arena 现状(bump 永不回收)在工具型应用下可接受;常驻大列表
场景登记 GC 契约设计(`2026-09-15-gc-contract-design.md`)的远期迁移考量,
本设计不引入新内存机制。

**C 侧分配器口径(F3)**:Clay 经 `Clay_SetAllocator` 接**帧级 scratch 双缓冲**
(帧界重置——若落永不回收的 bump arena,每帧布局临时分配将线性累积);
raylib 内部分配走默认堆(仅初始化/字体加载,低频);字形缓存有界(§11.6)。
长驻增长为观察项,衔接 GC 契约远期。

---

## 7. 里程碑(每级回应一个调研痛点)

| 里程碑 | 内容 | 回应 | 前置 |
|---|---|---|---|
| M0 静态渲染 | 内嵌 view+style → 像素;Clay 映射 | P6 | FFI 切片已兑现(§3.3 注记,待落库) |
| M1 绑定与事件 | 属性绑定 + each/when + on:* | P3 | — |
| M2 热重载 + 独立 .ctml | 保状态热替换;独立文件形态 + LSP | P4/P5 | — |
| M3 文本输入/IME | 受控 input + composer + FreeType/SheenBidi | **P1** | C-ABI 回调(已兑现,§3.3 注记) |
| M4 无障碍 + inspector | role/label 读屏树;元素检查器 | P2 | — |

工程量口径:M0–M2 季度级(单人,有 Clay 与自有编译器两个杠杆);M3 是最硬一仗;
M4 完整读屏对标 AccessKit 量级,P3 缓。

> **2026-09-17 引擎对齐增补**(暴露面 = Clay 能力面,§5.1):样式面增项
> (aspect/percent/min-max/四边 border/四角 radius/文本扩展/水平 scroll)随
> M0–M1 样式管线顺手落地;**浮层原语 + 元素 id 公共化登记 M2**(inspector
> 浮动卡为先行验证);dialog 模态输入语义(焦点锁/捕获)维持 M3+。

---

## 8. 风险与防线

| 风险 | 防线 |
|---|---|
| 范围蔓延拖向浏览器引擎(Blitz/RmlUi 先例) | §5.1 子集冻结入规范;"每加一个属性须过 Clay 映射门" |
| DSL 工具链缺席(Makepad 教训) | 内嵌形态起步,LSP/fmt/诊断同天交付 |
| 热重载破坏状态(先例普遍踩坑) | §6.3 验收口径进 CI(焦点/IME 不丢) |
| IME 不可用(淘汰线) | M3 设 composer 可见性为显式验收项,不达标不发布 |
| 常驻内存增长 | 事件低频分配 + GC 契约远期衔接,不新造机制 |
| 与在途发射器切片冲突 | 排期前登记 roadmap,发射面改动走统一评审 |

---

## 9. 决策记录(否决案存档)

| 备选 | 否决理由 |
|---|---|
| `.ui` 扩展名 | Qt Designer/GTK Builder 工具链语义已占用 |
| 独立 CSS 文件 | 子集小,组件内聚优于全局表;样式复用已由 §5 承载;`.cts` 与 TypeScript 冲突 |
| 完整 CSS 引擎 | 十万行级专门项目(RmlUi/Blitz);LVGL 撤 XML 引擎佐证维护成本 |
| 私有 DSL 表达式 | 跨语言摩擦(Slint 教训);Ctron 同源编译,类型检查贯通 |
| GLFW 底座 | 回调架构,FFI 欠账期不可用 |
| LVGL 全托管 | 控件层留在 C,dogfooding 归零;回调密集;2026 商业分叉治理观察 |
| GTK4 绑定 | GObject+信号闭包踩满 FFI 缺口;依赖链毁 §9.7 单二进制目标 |
| Elm 架构状态模型 | 全局状态抽象泄漏(实测社区共识);改用 model 直读 + 脏失效 |
| SDL3 起步 | FFI 面 3–5 倍(SDL_Event 128B tagged union);保留为渲染升级路径 |
| `#` 色字面量进语言(C1 裁决 2026-09-16) | 零语言侵入:颜色一律 Str 字面量 `"#RRGGBB[AA]"`,comptime 格式校验(E8130);`Color` 专用类型随之不设 |
| 约束求解布局(Auto Layout/ConstraintLayout 类,2026-09-16 §13.2) | 冲突调试不可视;求解器性能不可预测;与"布局原因可解释"目标直接冲突 |
| CSS Grid(同上) | 第二布局引擎;线性 flex 覆盖工具型场景,需求实证后再评(§13.2 后置) |
| 内置组件大全集(raygui 式 20+ 控件) | 违背最小完备 + 无特权原则;长尾由组合承担(§13.1) |

---

## 10. 演绎:从零开发一个 Todo 应用

> 以下为**目标形态演绎**(设计验证用例,非现状可运行代码;命令按 §9.7 终态 CLI 表达)。
>
> **v10 修订(2026-09-19,用户裁决):本演绎定为 GUI 域的终形态统一验收锚点**——
> M1-e(现状能力版)、L2 SL-8(表达式事件)与渲染回归同以此为准,"照抄能跑"是唯一
> 完成判据。清单语言定案 `Ctron.ctcl`(config-language v1;否决 toml)。v10 修正:
> 三元表达式改为 if 表达式(语言无 `?:`)、each 含输入控件补 `key`(§6.1)、
> List 无删除原语 → 过滤重建配方(删除原语登记 stdlib 缺口)、补 view 导入与
> 主题导入行、新增 headless 测试段(§11.7)。**文件组织定案:按领域分文件**
> (model.ct = 数据 + 领域操作;todo.ctml = 视图;main.ct = 装配),不按种类拆分。

### 10.1 建包

```bash
ctron new todo && cd todo
```

```ct
// Ctron.ctcl(包清单,config-language v1;J18:gui 节声明 UI 入口)
pkg {
    manifest_version = 1
    name = "todo"
    version = "0.1.0"
}
gui {
    entry = "src/todo.ctml"      // UI 入口:热重载监听目标;dist 打包携带物(§4.1)
}
dependencies {
    gui { std = true }           // gui 域(M0 起);ctc 据此自动链接 vendored 依赖
}
```

### 10.2 第一步(M0):静态骨架 + 样式,保存即见

```ct
// src/main.ct(v10:补 view 导入行与主题导入行)
use gui
use gui.theme                    // 默认主题令牌(§5.2:显式导入,无隐式注入)
use todo.{TodoApp}               // .ctml 经 P2-B glob 收集,文件名 = 模块名(§4.1)

fn main() -> I32 {
    gui.run(TodoApp())
}
```

```ct
// src/todo.ctml
view TodoApp {
  <vbox class="root">
    <label class="title">待办</label>
    <vbox class="list">
      <label class="empty">今天没有安排</label>
    </vbox>
  </vbox>
}
style root  { direction: column; gap: 12; padding: 16; width: 340 }
style title { font-size: 20; weight: 600; color: COLOR_TEXT }
style empty { color: COLOR_TEXT_DIM }
```

```bash
ctron run        # 窗口出现:标题 + 空态文案
```

**幕后**:comptime 解析 `.ctml` → 静态骨架 + 样式表常量;FFI 进 raylib(建窗/绘制)、
Clay(布局);全部零回调路径。改 `style title` 的 `font-size` 保存 → 窗口即时更新
(此阶段即为无状态热重载)。

### 10.3 第二步(M1):状态 + 绑定 + 事件

```ct
// src/model.ct —— 领域文件:数据 + 领域操作同文件(v10 裁决:按领域分文件,
// 不按种类——操作离开 Model 无意义,内聚单元 = model.ct;复用粒度是 fn 不是文件)
pub class Todo     { var id: I32; var title: Str; var done: Bool }
pub class Model {
    var todos: List[Todo]
    var draft: Str
    var next_id: I32
}
// v10 注:字段默认值未支持(收敛设计 §7-⑪),构造走 make() 配方;
// 方法声明在类型体外(trait+impl / UFCS / 模块 fn,§4.3.3 行为通道)
pub fn make() -> Model {
    return Model { todos: List[Todo](), draft: "", next_id: 1 }
}
pub fn add_todo(m: Model) {
    if m.draft.len == 0 { return }
    m.todos.push(Todo(id: m.next_id, title: m.draft, done: false))
    m.next_id = m.next_id + 1
    m.draft = ""
}
pub fn flip(t: Todo) { t.done = !t.done }
pub fn remove(m: Model, t: Todo) {
    // List 删除原语未落地(收敛设计 §7-⑩),过滤重建配方;原语落地后收回一行
    var kept = List[Todo]()
    for x in m.todos {
        if x.id != t.id { kept.push(x) }
    }
    m.todos = kept
}
```

```ct
// src/main.ct(同步更新，props 必填,缺失 = E8100;`use gui` 不变)
use gui
use model.{make}
use todo.{TodoApp}

fn main() -> I32 {
    gui.run(TodoApp(model: make()))
}
```

```ct
// src/todo.ctml(增补;v10:each 体含输入控件 → 必须带 key,§6.1;
// 顶层 use 允许(v10 文法澄清:File → (UseDecl | ViewDecl | StyleDecl)*),
// 绑定/事件表达式的作用域含 use 导入,§4.2 要点 2′)
use model.{add_todo, flip, remove}

view TodoApp(model: Model) {
  <vbox class="root">
    <hbox class="toolbar">
      <input bind={model.draft} placeholder="要做什么?"
             on:submit={add_todo(model)} class="draft"/>
      <button on:click={add_todo(model)} disabled={model.draft.len == 0}>添加</button>
    </hbox>
    <each todo in={model.todos} key={todo.id}>
      <hbox class="item">
        <checkbox checked={todo.done} on:toggle={flip(todo)}/>
        <label class={if todo.done { "done" } else { "undone" }}>{todo.title}</label>
        <spacer/>
        <button class="ghost" on:click={remove(model, todo)}>×</button>
      </hbox>
    </each>
    <when cond={model.todos.len == 0}>
      <label class="empty">今天没有安排</label>
    </when>
  </vbox>
}
```

**幕后**:`<each>` 编译为对 `model.todos` 的长度订阅，push/remove 触发该槽失效,
仅重排受影响的子树;`bind={model.draft}` 使 input 成为受控组件;`disabled={…}` 是
Bool 类型检查下的绑定槽,E8110 在编译期拦类型错误。

> 注(里程碑对齐):checkbox 属 T1(M0–M1 目录);M1-e 前以 toggle 按钮替代演绎,
> 本节为终形态目标面。

### 10.3.5 验收脚本(v10 新增;headless 全自动,§11.7/M1-e 统一锚)

```ct
// src/todo_test.ct —— "照抄能跑"的终态判据:无需窗口,CI 直跑
use gui.{test}
use model.{make}
use todo.{TodoApp}

test "添加 → 勾选 → 删除,列表与空态正确" {
    var m = make()
    test(TodoApp(model: m), |t| {
        t.type_into("draft", "写周报")
        t.click("add")
        t.type_into("draft", "交报表")
        t.click("add")
        t.expect_text("写周报")
        t.expect_item_count(2)
        t.click_item("todo-1", "remove")     // key 寻址:元素 id + 动作名
        t.expect_item_count(1)
        t.expect_text("交报表")
        t.expect_text("今天没有安排", when_empty = false)
    })
}
```

- 驱动面按 **ctml 声明寻址**(`bind` 名 / `key` 值 / 动作名),不写死坐标——
  几何来自布局回填,与运行时同一命中路径;
- 断言走命令缓冲文本/计数,渲染黄金帧差分(F2)另测视觉;
- 此脚本与 M1-e 的 Todo 夹具、L2 SL-8 的验收共用同一份源。

### 10.4 第三步(M2):热重载开发循环

- 改 `style item` 加 `radius: 4` → 保存 → **列表项圆角即时生效,已输入的 draft 与
  勾选状态原样保留**(焦点不丢,§6.3 验收口径)。
- 在 `todo.ctml` 新增一个 `<when>` 分支 → 保存 → 元素树结构对齐,状态原址。
- `.ctml` 与 `.ct` 均可热替换;逻辑(函数体)热替换受限于原生代码路径时,
  降级为"保存即重启动,状态经快照恢复"(裁决 1A:显式 `snapshot/restore`)。

### 10.5 第四步(M3):中文输入

`<input>` 的 IME 路径:拼音组词期间,候选串以 marked range 形式内联显示在输入框内
(不丢、不错位);上屏后经 `bind` 进入 `model.draft`。此步依赖平台 IME shim 与
FreeType/SheenBidi 文本栈(§6.4)。

### 10.6 发布

```bash
ctron build --release   # 静态单二进制,目标 < 2MB(含 gui 域运行时)
./todo                  # 无运行时依赖,无 webview,无解释器
```

### 10.7 全链路一图(本次演绎发生的全部事)

```
todo.ctml ─(comptime 解析/折叠)→ 骨架常量 + 绑定槽表 + 样式表常量
model.ct ─(Ctron 编译)→ 原生代码(状态与事件处理)
gui.run ──→ retained 树实例化 ──→ Clay 布局 ──→ 命令缓冲 ──→ raylib 绘制
用户点击 ──→ raylib 轮询 ──→ 运行时命中测试 ──→ add_todo(model) ──→ 脏槽失效 ──→ 重布局/重绘
```

，演绎完毕。全程用户只写了两种文件:`.ct`(逻辑)与 `.ctml`(结构+样式),
以及一份令牌模块;窗口、布局、渲染、失效全部是框架与底层库的职责。

---

## 11. 实现技术规范(实现侧补遗)

> §1–§10 规定"设计什么",本章规定"怎么实现"，编译器集成、绑定层、渲染管线、
> 热重载宿主、平台与验收口径。实现期偏离须经本章节评审并回写。

### 11.1 编译器管线集成

- gui 前端位于 parse 之后、sem 之前,**两形态边界不同(C2 裁决)**——独立 `.ctml` 由
  独立标记 tokenizer 全量解析(复用 `lex.ct` 基建,不进 §1 语言词法);内嵌形态下
  语言解析器认领块边界:`view`/`style` 进关键字表,块内容作为 token 流整块移交
  gui 前端(语言 grammar 仅新增两个块级关键字);`gui_parse`(view/style → 骨架 IR +
  槽表 IR)+ `gui_lower`(comptime 折叠:令牌求值/extends 展开/多类合并/常量槽求值/
  颜色格式校验);
- 三产物口径:cc_run(解释口径,M0–M2 开发主路径,骨架解释执行)/
  cc_check(静态检查面,E8xxx 诊断)/cc_emit(骨架常量 + 槽表发射为 C 静态数据);
- 发射形态(规范性):骨架 = `static const GuiNode gui_sk_N[]`;绑定/事件槽 =
  `{node_idx, attr_id, target_idx}` 常量三元组数组，全部 comptime 产物,运行时零解析;
- 库链接:raylib/Clay/FreeType/SheenBidi 以 vendored `c_src/` 形态进包
  (沿用 ffi_math 多文件包约定);`#[trusted] extern` 声明集中收在 `std/gui/bind/`
  内部模块,用户面不接触 trusted 边界。

### 11.2 FFI 绑定层

- `std/gui/bind/{raylib,clay,ft,sb}.ct`:`#[trusted] extern` 声明 + safe 包装
  (标量/struct 边界检查;§3.3 的 repr(c)/Str 切片是这些文件的地基);
- **Clay measure 桥**:布局需要文本测量回调，M0 由 C 侧固定 shim
  `ctron_gui_measure` 提供(纯 C 实现,零 Ctron 回调);M3 换 FreeType 测量,
  仍走同一 C shim 签名;
- **渲染适配 shim**:`ctron_gui_flush(Clay 命令缓冲) → raylib`,百行级固定 C,
  是唯一允许直呼 raylib 绘制 API 的位置;后端换代(SDL_gpu/自写 shim)只动此文件。

> **2026-09-19 目标增补(用户面零 C,§1.1 目标 7 的兑现口径)**:bind 层从"地基描述"
> 升格为**交付口径**，gui 域对应用暴露的 API 面(`std/gui`)必须纯 Ctron:
> bind extern + safe 包装 + 布局/事件/控件运行时全部在 Ctron 侧,**应用默认零 C 文件、
> 零 shim 携带**。据此定性:S1–S9 夹具与示例源码树中的 `c_src/*.c`(Clay 窄接口桥 +
> FreeType 纹理桥)是 MVP"零编译器改动先跑通"的**过渡形态**,属待偿还债(登记
> MVP 阶梯 W6)，偿还后 C 收缩为上两处域库内部固定件(measure 桥 + flush),
> 随 `std/gui` 包构建,用户源码树不可见。用户写 C 的唯一常规场景 = 域能力未覆盖时的
> 自定义或第三方库接入(§9.6 FFI 通道,逃生口定位;能力地基，repr(c) 按值、
> Str 编组、C-ABI 回调——已由 tests/ffi 20 夹具兑现,bind 层是纯工程活)。

### 11.3 渲染与命中管线

- 帧循环 = raylib **轮询模型**(无阻塞 WaitEvent,协议见 §12.3a):`WindowShouldClose`
  主循环 → 输入快照差分生成事件 → poll 后台通道(§6.2)→ 脏检查(有失效才重布局/
  重绘,否则跳过绘制)→ 空闲降帧省电;事件级阻塞等待登记为 SDL_gpu 换代收益;
- Clay 每次布局输出元素几何(位置/尺寸/裁剪),回填元素树供命中测试;
- 命中测试:主线程,自顶向下按 z 序 + clip 裁剪遍历几何树,命中最深可交互元素;
  **流内 z 序 = 树序 = 绘制序(后声明者在上);浮层元素按 zIndex 升序**
  (Clay floating 原生排序;2026-09-17 引擎对齐修订，z 序仅浮层作用域,
  流内不设 z-index);
- 焦点:运行时本地状态(§4.3 契约 5),Tab 焦点环按树序,M3 起生效。

### 11.4 文本管线

- M0–M2:raylib 内置字体(`LoadFontEx` + codepoints 范围,CJK 常用集按需页入);
- M3:FreeType 光栅化 + SheenBidi(bidi/shaping)+ 字形图集 LRU 缓存(arena 友好,
  预算见 §11.6);测量走 §11.2 measure 桥;
- IME:平台 shim(preedit/commit)事件进主循环队列,composer 为元素树可见状态。

### 11.5 热重载宿主机制(裁决成文)

- **开发口径 = 解释执行 + 受控 extern 直调**(cc_run 扩展,C4 裁决):解释器对
  `std/gui/bind` 白名单内的 `#[trusted]` extern 经 dlsym 直调宿主进程加载的
  vendored 库(白名单外维持 panic 指引)，文件变更 → 重跑 gui_parse/gui_lower →
  骨架/槽表/样式原址替换,状态树保留;**M0–M4 热重载全功能(M3 IME 依赖此能力)**;
- **原生口径**:保存即重编译+重启,model 经快照恢复(**裁决 1A**:显式
  `snapshot/restore` 函数;`@derive` 序列化不采用)，§10.4 降级路径的落地形态;
- 双口径验收同一:焦点与 IME 组词不丢(解释口径硬验收,原生口径快照验收)。

### 11.6 平台矩阵与性能口径

- M0 平台:macOS(Cocoa)/ Linux(X11+Wayland)/ Windows(Win32)，即 raylib
  支持面,不裁剪;
- 性能登记(对齐 §9.4,进 CI 基准):输入→绘制 P99 < 16ms;布局 = Clay 全量重放
  (微秒级)+ 求值缓存增量(§12.3c);冷启动 < 100ms;字形图集 < 64MB;
  **开发口径(解释)帧率门槛(F1)**:千元素级树 60fps,超出即原生口径验证触发点;
- 产物:静态单二进制,gui 域运行时增量目标 < 1MB(叠加 §9.7 full 档目标)。

### 11.7 测试与验收策略

- E8xxx 负例语料进 `tests/09_gui/`,check 口径逐字诊断差分(沿用黄金基线法);
- **渲染回归(F2)**:命令缓冲快照为主(Clay_RenderCommand 流序列化 hash，
  跨平台确定性),像素 hash 仅同平台辅验;进 make test;解释/发射双口径
  输出一致性差分(与仓库三方对齐方法同构);
- 控件库每个控件:黄金帧 + 脚本化交互回放(点击/键入/IME 序列);
- 热重载验收:改样式/结构/逻辑三者,焦点与 IME 不丢(§6.3)。

### 11.8 资源与远期登记(不阻塞 M0–M4)

- 资源:`assets/` 目录约定,`img` 相对路径解析;图标字体随 M4;包化随 P2-B;
- 剪贴板/拖放:raylib 有剪贴板口,M3+ 按需;
- 动画/过渡:声明式 tween(样式属性插值),M4+;
- 多窗口:M1+ 按需求,元素树按窗口分株;
- **CTML → web 档编译**:同一骨架/槽表 IR 对接 §9.2 stdweb 后端——本设计的
  长期收益,登记远期;
- 无障碍树(M4):role/label → 读屏后端,对标 AccessKit 模式,依赖平台回调面。

---

## 12. 依赖与集成规范(规范性)

> 本章钉死"用什么方案、哪些开源依赖、各自唯一职责、如何配合"。
> 版本号为 M0 启动时的参考锚,启动时钉死并登记于 vendor 清单;升级走 §12.4 流程。

### 12.1 开源依赖清单

| 依赖 | 参考版本 | 许可 | 唯一职责 | 集成形态 |
|---|---|---|---|---|
| **raylib** | 5.5 | zlib | 窗口生命周期、输入轮询、2D 绘制、内置字体光栅(M0–M2) | vendored c_src |
| **Clay** | v0.14 | zlib | 声明式 flex/box 布局 → 渲染命令缓冲 | 单头 vendored |
| **FreeType** | 2.13.x | FTL/GPL 双 | 字形光栅化(仅 M3 起) | vendored |
| **SheenBidi** | 当前稳定 | MIT | bidi + 文本 shaping(仅 M3 起) | vendored |
| utf8proc | 当前稳定 | MIT | Unicode 步进/宽度(M1+,按需) | vendored |

- stb_truetype/stb_image 经 raylib 内置,不单列不直引;
- Clay 仍 pre-1.0:版本钉死,API 破坏性升级按 §12.4 评审;
- §12.5 之外新增依赖 = 规范修订,须过"唯一职责已被覆盖吗/FFI 面/体积"三门。

### 12.2 职责边界矩阵(唯一职责律)

| 关注点 | 归属 | 说明 |
|---|---|---|
| 窗口生命周期 | raylib | 建/销/标题/尺寸 |
| 原始输入采样 | raylib | 轮询 API;边沿事件生成归运行时(§12.3b) |
| 布局/裁剪/滚动几何 | Clay | flex/box 求值 → 命令缓冲 |
| 命中测试/焦点/失效 | 运行时(Ctron) | 用自有几何回填,不借 Clay_PointerOver |
| 样式解析/令牌/折叠 | comptime | release 零运行时解析 |
| 文本测量 | measure 桥 | M0=raylib,M3=FreeType+SheenBidi,签名不变(§12.3c) |
| 光栅化与绘制 | flush shim | 唯一 raylib 绘制口(§12.3d) |
| 后台并发 | Ctron spawn/channel | raylib/Clay 调用仅限主帧循环(§6.2) |

### 12.3 集成点协议(具体配合细节)

#### 12.3a 生命周期与帧循环

```c
InitWindow(w, h, title); SetTargetFPS(120);
while (!WindowShouldClose()) { gui_frame(); }   // Ctron 侧:§12.3b–e
CloseWindow();
```

- raylib 为纯轮询模型:省电走"脏检查跳绘 + 活跃 120fps/空闲 10fps 降帧";
  事件级阻塞等待列为 SDL_gpu 换代收益(§3.2);
- DPI:`GetWindowScaleDPI` 每帧校验 → 全局 scale 因子进样式长度解析(单位 =
  逻辑像素,§5.1),跨屏迁移即生效;
- 窗口配置经 `gui.run` 显式参数传入:`gui.run(root, title: Str, size: Size, min: Size?)`
  (§10 演绎中为省略写法,以此为准)。

#### 12.3b 输入翻译表(raylib → 运行时事件)

| raylib 口 | 运行时事件 | 备注 |
|---|---|---|
| `GetCharPressed`(队列取尽) | `TextInput(char)` | IME commit 后的上屏文本 |
| `GetKeyPressed` + 上帧快照 | `KeyDown/KeyUp` | 边沿在运行时生成 |
| `IsMouseButtonPressed/Released` | `MousePress/Release` | 双击计时在运行时 |
| `GetMousePosition`(差分) | `MouseMove` | 无位移不发事件 |
| `GetMouseWheelMove` | `Wheel` | 喂 scroll 容器状态 |
| `GetKeyPressed = Enter`(input 焦点) | `Submit` | 运行时合成事件,仅 input 焦点时产生 |
| `IsWindowResized` | `Resize(w,h)` | 全树重布局 |
| `GetClipboardText`(按需) | `Paste`(M3) | |
| `IsFileDropped` | `DropFiles`(M3+) | |

生成规则:帧首快照 + 与上帧差分 → 边沿事件队列;状态型 API 不直接进 handler。

#### 12.3c 布局协议(Clay)

```text
Clay_BeginLayout()
  按 retained 树 DFS:Clay_OpenElement → Clay_ConfigureOpenElement(样式求值产物)
                      → Clay_Text(...) / 子元素 ... → Clay_CloseElement
commands = Clay_EndLayout()   // Clay_RenderCommand[]
```

- **求值缓存层增量(规范性措辞,C3 裁决)**:Clay 为立即模式,声明序列每帧
  全量重放(微秒级);增量性只在求值层，脏槽才重算表达式,重放时读缓存值。
  禁止按"跳过不脏子树"理解或实现;
- 测量:`Clay_SetMeasureTextFunction(ctron_gui_measure)`，C 固定 shim,M0 内部
  走 `MeasureTextEx`,M3 走 FreeType+SheenBidi,**签名不变**;
- 滚动:Clay scroll 配置输出滚动容器命令;滚轮只失效该子树;
- 命中测试不用 `Clay_PointerOver`:用自有几何回填(§11.3),保证与焦点/语义树一致。

#### 12.3d 渲染适配(ctron_gui_flush,唯一 raylib 绘制口)

| Clay_RenderCommand | raylib 调用 |
|---|---|
| TEXT | `DrawTextEx`(M3:字形图集批量绘制) |
| RECTANGLE | `DrawRectangleRounded`(圆角来自样式) |
| IMAGE | `DrawTexturePro` |
| SCISSOR_START/END | `BeginScissorMode/EndScissorMode` |
| CUSTOM | 控件自绘逃生口(焦点环/进度条) |

字体:`LoadFontEx`(ASCII + 常用 CJK 集按需页入)上传 GPU;M3 换 FreeType 管线。

#### 12.3e 帧内时序(总协议)

```text
1 输入快照差分 → 事件队列(§12.3b)
2 poll 后台 channel → 事件(§6.2 线程模型)
3 分发事件:handler 改 model(全部可变写都在这)
4 脏?→ 遍历树求值脏槽 → Clay 布局(§12.3c) → flush(§12.3d) → raylib 绘制
5 帧尾:几何回填(命中/焦点/IME 候选窗定位用)
```

#### 12.3f IME shim 协议(M3)

- 路线:`GetWindowHandle()` 取平台句柄(GLFWwindow*)挂 preedit/commit 回调;
- 事件形态:`ImePreedit{marked: Str, caret: I32}` / `ImeCommit(Str)` 进 §12.3e 队列;
  composer 为元素树可见状态(§6.4);
- 候选窗定位:input 的 boundingBox + 运行时光标 x → 屏幕坐标喂 shim;
- 风险登记:平台回调与 raylib 内部轮询的并存性由 M3 spike 首验;失败备选 =
  **raylib 补丁**(裁决 6A,定案;自写窗口层仍为 §3.2 的 B 档远期演进,非本备选)。

### 12.4 vendoring 与升级

- `vendor/gui/`:raylib 精简源、clay.h、freetype、sheenbidi;版本与来源登记;
- 升级 = PR + `tests/ffi` + 渲染黄金帧全绿 + `ctron_abi.h`/typedef 同步
  (发射面唯一真源,§11.1);
- 许可合规:zlib/MIT/FTL 均可静态闭源链接;FTL 的文档声明要求登记进发布清单。

### 12.5 不引入清单(规范性)

GLFW 直依赖(raylib 内含)、SDL/SDL_gpu(M0;仅作渲染升级路径,§3.2)、GTK/Qt、
Cairo/NanoVG(渲染自绘)、webview/JS 引擎、独立图像解码库(M0 用 raylib 内置,
重需求再立)，理由:唯一职责已被覆盖 / 控制 FFI 面 / 单二进制体积。

---

## 13. 内置组件与布局规范(2026-09-16 设计裁决)

### 13.1 内置组件目录(完整版)

**原则(规范性)**:

1. **最小完备**:MVP(M0–M1)内建 ≤ 10,全量(M3)≤ 18;长尾组件由用户组合承担
   (web 哲学:元素少 × 组合强);
2. **无特权**:每个内建组件 = `pub view`(§4.3 同一套组合契约)+ 默认样式表
   (§5.2)，用户可样式覆盖或整体 fork 替换;内建组件**不是编译器内建**,
   是 gui 库代码("语言自举"精神);
3. **无新绘制原语**:每个组件必须能由 Clay 命令(RECT/TEXT/IMAGE/SCISSOR/CUSTOM)
   + 文本渲染实现;
4. **通用 props 约定(全组件,规范性)**:`class`(样式类);`disabled`(交互组件禁用,
   运行时自动合并 `disabled` 类,不新增伪类);`visible`(显隐,编译为布局跳过、
   不占位;选用规则:**单元素显隐用 `visible`,分支/列表段用 `<when>`**
   ，2026-09-17 可读性评审);事件统一 `on:xxx={handler}`(自动闭包,§4.2
   要点 3);双向绑定统一 `bind={model.field}`。

**零样式默认表(2026-09-17 可读性评审,规范性):任何组件不写任何样式 =
尺寸 hug×hug、无 padding/gap**,方向按组件语义(vbox/panel = 列,hbox = 行,
scroll 同宿主容器)。例外:**无**——"不写样式 = 自然尺寸"全局成立(原 label
宽 grow 默认废止,见 T1 表)。

**T0 容器(M0)**

| 组件 | props / 事件 | 语义 |
|---|---|---|
| `vbox` / `hbox` | 布局走样式 | 线性容器(direction 语法糖:column/row) |
| `spacer` | — | 弹性空白(fill 尺寸语法糖) |
| `panel` | — | 通用容器:背景/边框/裁剪(clip);**= 带默认样式的 vbox**(2026-09-17 可读性评审:容器同基不同默认样式，vbox/hbox 同样可带视觉样式,无特权) |
| `scroll` | 滚动位置=运行时本地 | 滚动容器:裁剪 + 滚轮/拖动(§12.3c 滚动命令) |

**T1 基础交互(M0–M1)**

| 组件 | props | 事件 | 注 |
|---|---|---|---|
| `label` | 插值文本 | — | 宽默认 **hug**(= 文本自然宽;2026-09-17 可读性评审修订，原 grow 默认与 hug 容器组合必触 E8192);撑满走主题 `.grow` 类或显式 grow;容器约束下的换行=自然行为 |
| `button` | 内容 = slot | `on:click` | hover/active 伪类;disabled |
| `checkbox` | `bind`(Bool) | `on:toggle` | 受控;勾选视觉自绘 |
| `input` | `bind`(Str)、`placeholder` | `on:submit`(Enter 合成)、`on:change` | 受控;M0–M2 上屏可达/无组词(§6.4 边界),M3 完整 IME |
| `image` | `src`、尺寸 | — | raylib 纹理;资源路径口径 §11.8 |

**T1.5 反馈(M1.5)**

| 组件 | props | 事件 | 语义 |
|---|---|---|---|
| `divider` | — | — | 分隔线 |
| `progress` | `value`(0..100) | — | 只读进度条(矩形自绘) |

**T2 完整交互(M2+;slider/select/tooltip/dialog 依赖浮层与拖拽)**

| 组件 | props | 事件 | 注 |
|---|---|---|---|
| `slider` | `min`/`max`/`step`、`bind`(I32) | `on:change` | 数值域先 I32(裸 F32 缺口修复后放开浮点) |
| `select` | `options`(List[Str])、`bind` | `on:change` | 弹层(floating)+ 键盘导航 |
| `custom` | `on:draw`(fn) | — | **自绘逃生口** → Clay CUSTOM 命令(§12.3d);坐标系 = 元素 boundingBox;图表/可视化的正规出口 |
| `tabs` | `active`(`bind`)、子项 `title` | `on:change` | 标签页容器 |
| `dialog` | `open`(Bool) | `on:close` | 模态浮层 + 焦点锁(M3+,依赖窗口层) |
| `tooltip` | 文本、延迟 | — | 悬停浮层(M4) |

**M3 伴随**:`textarea`，多行受控编辑(`bind` + `on:change`;滚动 + 换行;IME 同 input)。

> 2026-09-17 引擎对齐:T2 依赖的浮层原语已进子集(§5.1)，`select` 弹层、
> `tooltip` 的机制前置齐备;`dialog` 的焦点锁/模态捕获仍属运行时输入语义(M3+)。

不设(组合配方)

| 需求 | 组合配方 |
|---|---|
| toggle 开关 | `checkbox` + 样式变体(类切换) |
| radio 单选组 | `checkbox` 列表 + model 互斥逻辑(行为复用 = 普通函数,§4.3.3) |
| list 列表 | `each` + 容器 |
| tree 树 | 自引用 view + each(§4.3) |
| table 表格 | 行 `hbox` 组合(grid 不做:Clay 无此能力,引擎对齐,§5.1/§13.2) |
| menu 菜单 | `panel` + popup 定位组合 |
| number 输入 | `input` + 解析函数(转换显式留在 Ctron 逻辑) |
| richtext 富文本 | 远期 |

### 13.2 布局方案:Clay flex 子集,算法层不造新轮子

**采纳**(§5.1/§12.3c 已定,本节沉淀裁决理由):box model + 线性 flex
(direction/gap/align/justify/wrap)+ 尺寸三语义(fixed/hug/fill)+ 滚动 + 裁剪。
布局算法层**不创新**，flex 子集覆盖工具型 UI 绝大多数场景,Clay 已实现且微秒级。
**2026-09-17 引擎对齐**:grid 不做获得客观佐证——Clay v0.14 无 grid 能力;
超出 flex 的形态(浮层/aspect/percent)按"暴露面 = Clay 能力面"全量映射(§5.1)。

业界布局方案缺点盘点(否决依据,详见 §9):

| 方案 | 核心缺点 |
|---|---|
| CSS 完整级联 | 样式来源不可追溯;全局性隐式依赖 |
| 约束求解(Auto Layout/ConstraintLayout) | 冲突调试噩梦("无法同时满足");求解器性能不可预测;非线性心智 |
| CSS Grid | 表达力冗余,引入即第二布局引擎;线性 flex 已覆盖工具型场景 |
| 立即模式手动布局(ImGui cursor 式) | 无响应式能力;精细对齐难;与声明式目标冲突 |
| Flutter 约束传递协议 | 向用户暴露 constraints down/sizes up 协议,学习成本高 |
| 共性缺口 | 调试不可视("它为什么在这/为什么这么宽"无解,web devtools 是唯一例外);
  不可快照测试(状态藏在原生控件);flex 属性组合空间过大(9+ 对齐属性) |

创新空间(诚实定位):不在算法,在三处，

1. **布局原因链(provenance,登记 M2 inspector 核心卖点)**:开发口径下布局桥为每个
   元素记录"尺寸/位置来源"(父约束/自身样式/文本测量),inspector 一键回答
   "它为什么在这"。目前没有现成方案;web devtools 的 box model 面板是已知 closest;
2. **确定性可快照**:布局 = 纯函数(骨架 + 样式 + 约束)→ 命令缓冲黄金差分(§11.7)。
   原生控件方案做不到;自绘 + 自有布局桥天然可测;
3. **comptime 预布局**(可选,后置):无绑定静态子树尺寸编译期预演;收益待 bench,
   不承诺。(容器查询已登记 §5.1 远期，响应式锚定父容器而非窗口。)

> **2026-09-17 深化裁决**:三项的落地设计见 `2026-09-17-layout-provenance-design.md`
> ，记录层 = shim 后置推导 + 自检断言(推导 result 逐帧 == 命令缓冲几何);
> comptime 预布局收窄为布局静态验证(E8191 静态溢出 / E8192 死 fill,W3);
> inspector = 停靠 + 浮动**双模式可切换**。
> **同日引擎对齐裁决(用户):暴露面 = Clay 能力面全量映射(§5.1)**，浮层
> 原语公共化进 M2,inspector 浮动卡为其先行验证;"popup 推迟"裁决随之解冻,
> dialog 的模态输入语义仍维持 M3+。

### 13.3 默认主题与开箱体验

gui 域自带默认 `theme.ct`(颜色/间距/字号令牌)+ 每组件默认样式表(§5.2 pub style);
用户 theme 经 `use` 整体替换。目标:开箱即得体、全量可换肤、无隐藏全局样式。

### 13.4 组件扩展与自绘(规范性)

扩展三通道(按改动深度递增):

1. **样式覆盖**:class 合并 + 令牌(§5)，同一组件换皮,零代码;
2. 包装组合(推荐默认):`pub view` 包住内建组件,注入语义/默认值/事件适配;
   包装层可拦截事件(先自身处理再转发)——内建组件升级自动受益;
3. **fork 替换**:内建组件无编译器特权(§13.1 原则 2),复制源码改行为、
   以新名发布,是受支持路径。

**自绘组件(`custom` + `on:draw`)**:

- `<custom on:draw={fn}/>` 参与正常布局(有 boundingBox),输出 CUSTOM 命令;
- **绘制阶段 = 运行时直调,非 FFI 回调(规范性裁决)**:布局期 custom 元素的
  draw fn 经绑定槽表登记;绘制阶段 Ctron 运行时遍历命令流,遇 CUSTOM 就地
  普通 Ctron 调用，**闭包捕获自由**(E4042 不适用),draw fn 读 model 与
  绑定表达式同权;零 FFI 回调依赖;
- **DrawCtx 绘制原语**(shim 标量 API,§12.3d 同一绘制口):`rect` / `line` /
  `circle` / `text` / `image` / `push_clip` / `pop_clip`;颜色 = 打包色
  (`#RRGGBB` 或令牌);局部坐标系(元素左上为原点),自动裁剪至元素框;
- 立即模式语义:每帧重调 on:draw;失效与绑定同机制(读到的 model 脏 → 重放)。

```ct
// 示例:迷你柱状图，闭包捕获 model,零样板
fn draw_bars(ctx: DrawCtx) {
    var i: I32 = 0
    while i < model.values.len {
        ctx.rect(i * 30 + 4, 200 - model.values[i], 20, model.values[i], COLOR_ACCENT)
        i += 1
    }
}

<custom on:draw={draw_bars} class="chart"/>
```
