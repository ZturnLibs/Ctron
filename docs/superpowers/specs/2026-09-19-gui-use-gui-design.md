# GUI 用户面收敛完整设计——`use gui` 域(L1)+ comptime CTML(L2)

> 状态:设计评审稿(2026-09-19)。上游:`2026-09-16-gui-ctml-design.md`(下称"规范";
> 本文只引不改,偏离处 §10 逐条申报)、`2026-09-19-m1-bindings-events.md`(M1 阶梯)、
> `2026-09-19-gui-user-surface-analysis.md`(差距量化,下称"分析文")。
> 本文回答:**从今天的夹具形态走到规范 §1.1 目标 7(纯 Ctron 开发面),工程上怎么落**——
> 模块布局、公共 API 签名、运行时内部结构、测试面、迁移销账、切片与验收。
> 结论先行:两级着陆。**L1 = `use gui` 运行时域包**(零编译器改动,删用户树 ~90% 胶水);
> **L2 = comptime CTML 编译期**(§11.1 管线,删剩余解析/注册,诊断前移)。两级各自
> 独立交付、独立有价值,L1 的运行时代码在 L2 全部复用。

---

## 1. 目标与非目标

目标:

1. 用户树零 `#[trusted]`、零解析器、零布局/命中/事件循环、零 headless 脚手架;
   写计算器 = model + 状态机 + `app.ctml` + 3 行 main(分析文 §3 的形态);
2. 过程零编译器改动(L1 全部落在 std/gui 域包 + C shim 不动);L2 的编译器改动
   严格沿规范 §11.1 既定管线,不新增方向;
3. 测试面:GUI 断言进既有 test 块体系,零新 DSL(规范 P4 教训:DSL 无工具链即死);
4. 逃生口全保留(分析文 §4):原始 FFI/自写解析/自绘/主题替换。

非目标(明示不做):

- 不做组件库全量(§13 目录按里程碑走,本文只交付 L1 需要的 vbox/hbox/label/button
  + L1.5 的 each/when/input 承接);
- 不做热重载环进 L1 包(W4 已在夹具级交付,包级热重载随 L2 的骨架原址替换,
  §6.3;L1 用户需要时沿用 examples/gui_counter 的 --hot 环形态);
- 不改 CTML 语法(§4.2 规范性草案冻结;L1 消费的语法面 = 现行夹具子集 + 深度 N 嵌套)。

## 2. 现状底盘(2026-09-19 实测)

**可依赖的既有设施(全部已在库,本设计零新增地基):**

| 设施 | 证据 | 本设计中的角色 |
|---|---|---|
| 包/模块系统 | `use app.util.{double}` 组导入+全限定;`use std.str.{words}`;pub/pub(pkg)/私有三级;Ctron.ctcl 清单(tests/modules/use_ok、stdpath) | `use gui` 的直接载体 |
| std 三级解析 | ①CTRON_STDPATH ②exe 旁 lib ③回落 `../std`(stdpath 夹具) | gui 域包的发现路径 |
| 函数类型 + 闭包 | `fn(I32) -> I32` 形参、`|x| x + 3` 字面量、闭包实参自动适配(03g) | L1 绑定/事件钩子的全部类型基础 |
| test 块体系 | `test "..." { assert_eq(...) }` + suite 驱动(01–10 语料) | GUI 断言的宿主(§6) |
| canonical 解析器 | std/gui/parse.ct(S5/S6 交付,自述"P2-B 前无消费者,登记同步义务") | L1 运行时的解析层,升级为深度 N 后即用 |
| C shim 单一真源 | std/gui/c_src/ctron_gui.c(W6 换面;Clay 窄桥+注入缝+探针+flush) | L1 原样保留,extern 声明收编进包(§4.5) |
| 主题令牌 | std/gui/theme.ct(§13.3) | 样式默认值来源 |
| M1 运行时语义 | when/each/input 已交付(s10–s12 夹具,内嵌快照形态) | L1.5 承接进域包(§8 SL-5) |

**缺口(本文逐个给出落点):** 域包入口与运行时(§4)、深度 N 嵌套解析(canonical 升级)、
测试驱动器(§6)、发射器四修复(§7)、comptime 管线(L2,§5)。

## 3. 收敛总路线

```
L0 今天          夹具/示例各自内嵌:29 extern + 解析器 + 布局 + 命中 + 循环 + 断言
                 gui_calc 2002 行 = 业务 880 + 平台 1080(分析文 §1)
   │  SL-1..4    std/gui 域包:use gui 一行,平台代码全部进包
L1 ┤             用户树:app.ctml + model + 状态机 + 5 行 main + 钩子
   │             gui_calc ≈ 700 行(全业务);gui_counter ≈ 60 行
   │  SL-6..9    编译器:gui_parse/gui_lower 三产物;view/style 内嵌;E8xxx 检查面
L2 ┤             绑定/事件 {expr} 化;钩子/锚点/注册全部消失
   │             gui_calc main = gui.run(Calc(model: CalcModel())) 一行
```

两级各自的"删什么":

| | L1 删除 | L2 删除 |
|---|---|---|
| 用户树里的 | 29 extern、解析器、布局发射、命中、事件循环、headless 脚手架 | 剩余钩子(text/click/key 注册)、read_file 锚、dispatch 手写映射 |
| 机制 | 平台代码进 std/gui 域包,运行时解析 | 骨架 comptime 编译,诊断前移 E8xxx,保存即见热重载(§6.3) |
| 编译器改动 | **零** | gui 前端三产物 + 内嵌块关键字(§11.1 既定) |

## 4. L1:`use gui` 域包设计

### 4.1 模块布局

```
std/gui/
  Ctron.ctcl        # 新增:name = "gui"(域包清单;随 std 三级解析被发现)
  gui.ct            # 包入口:pub run / run_kb / test / Size;re-export 常用类型
  parse.ct          # 已有(canonical)→ 升级:深度 N 嵌套(fc/ns 子链)+ 多类样式
  theme.ct          # 已有(令牌)
  runtime.ct        # 新增:骨架遍历 + 绑定求值 + Clay 发射 + 几何回填 + 命中 + 事件分发
  driver.ct         # 新增:测试驱动器(headless 注入 + 断言读回,§6)
  bind/
    raylib.ct       # 已有(InitWindow 等,收编;不再复制进用户树)
    shim.ct         # 新增:ctron_gui.c 窄桥的 #[trusted] extern 声明(gui_cfg/gui_text/
                    #   cmd 探针/inject/poll/flush;W6 单一真源不动一个字节)
  widgets/
    basic.ct        # 新增:vbox/hbox/label/button 的样式默认值与求值(L1 内联进 runtime,
                    #   独立成文件随 L1.5 组件扩充时拆分——先不拆,YAGNI)
c_src/ctron_gui.c   # 不动(L1 零 C 改动;收缩见 §4.5)
```

行数预算:gui.ct ≈60,runtime.ct ≈420(≈gui_calc 删除段的平台部分),driver.ct ≈180,
parse.ct 升级 +80。域包合计新增 ≈750 行 Ctron——**写一次,所有 GUI 应用共享**。

### 4.2 公共 API(规范性签名)

```ct
// gui.ct —— 用户可见的全部(L1;事件=注册表,2026-09-19 D3 修订)
pub struct Click { pub var name: Str
                   pub var run: fn() }        // on:click 处理名 → 闭包体

pub fn run(src: Str, title: Str, w: I32, h: I32,
           text: fn(Str) -> Str,             // 绑定钩子(绑定少,单钩子够用)
           clicks: List[Click]) -> I32       // 事件注册表:收口装配点 + 独立处理体
pub fn run_kb(src: Str, title: Str, w: I32, h: I32,
              text: fn(Str) -> Str,
              clicks: List[Click],
              key: fn(I32) -> Void) -> I32   // 键盘保持单钩子(码表映射本就是函数)
pub fn test(src: Str, w: I32, h: I32,
            text: fn(Str) -> Str,
            clicks: List[Click],
            script: fn(Driver) -> Void) -> I32
// 回退形态(SL-0 探针红时):List[Click] → click: fn(Str) -> Void 单钩子,
// 用户回到 if-chain;其余 API 不变
```

用户侧形态(收口与独立的结合点):

```ct
var clicks = List[gui.Click]()
clicks.push(gui.Click("d7",  fn() { press_digit(m, "7") }))   // 每项独立闭包体
clicks.push(gui.Click("add", fn() { press_op(m, "+") }))
...
run_kb(src, "ctron calculator", 320, 420, texts, clicks, keymap)
```

契约(每条都是用户要懂的全部):

1. `src` = CTML 源(现行 `.ctml` 语法;**main 首语句 `read_file("app.ctml")` 锚定
   约定不变**——发射锚机制要求字面量在用户 main,文件头注释须写明;L2 内嵌形态
   落地后本参数消失);
2. `text(name)` 返回绑定名 `{name}` 的显示值;运行时每帧对绑定名调用(工具应用
   ≤几十个绑定,直调开销可忽略;脏槽缓存随 L2);
3. **事件注册表 = 收口装配点,逐项闭包 = 独立处理体**(D3 修订):按钮行为在注册表
   里逐项声明,就地写闭包,不经中央 if-chain;注册表可枚举 → Driver 按名寻址、
   动作清单/文档零成本。共享动作(按钮+键盘+菜单多触发点)走**命名函数多处引用**
   (规范 §4.3.3 行为复用通道);观察性收口(日志/审计)归运行时——运行时是
   handler 的唯一调用者,`gui.trace(fn(meta))` 框架钩子即可,用户代码不参与集中化;
4. `key(code)` 收 raylib 键码(可选项 → `run_kb`;无重载无默认值时的双入口形态,
   L2 后键盘经 `on:key` 表达式声明,双入口合并);
5. 返回值 = 退出码(窗口关闭/测试失败);**headless 判断不进用户代码**:
   `test()` 强制 headless,`run()` 强制窗口——env 开关从用户 main 消失;
6. `Driver` 面向测试脚本(§6):`click_name/click_xy/type_key/press/expect_text/frame`。

### 4.3 运行时内部(runtime.ct)

数据流(单帧,对齐规范 §12.3e,标注与 L2 的差异):

```text
事件:gui_poll_event → (2)命中:RECT 几何 ×100 域比较 → 按钮 DFS 序 → ev_fn 名 → click(name)
                     → (1)key → key(code)                  [run_kb]
布局:深度遍历 fc/ns 树 → 逐节点 gui_open/gui_cfg(样式求值)/gui_text(绑定求值) → gui_end_layout
绘制:BeginDrawing → gui_clear(theme 色) → ctron_gui_flush → EndDrawing
```

- **树表示 = fc/ns(首孩子/下一兄弟,两条 `List[I32]`)**:parse.ct canonical 从
  "平行表 + root=0 定位"升级出子链;选型依据:双宿主发射已实证(gui_calc 全绿),
  `List[List[T]]` 原生读回未落地(§7 修复后可换,登记);
- **绑定求值**:leaf 文本 = 前缀 + text(绑定名) + 后缀,每帧重建(L1 无脏追踪);
  显示串 > 12 字节缩字号的自适应保留为 runtime 内建策略(帧内测量,不经用户);
- **样式求值**:cls_prop 多类"后者覆盖"规则(parse.ct 升级一并收编);颜色经 theme
  令牌缺省,#RRGGBB 字面量运行时校验(失败 panic 带 E8130 文案,L2 转编译期);
- **失效模型(L1 = 有事件才重布局 + 每帧重绘)**:60fps 工具应用口径(F1 门槛内);
  求值缓存/脏槽随 L2(C3 裁决的求值层增量);
- **退出打印**:窗口关闭回传 `gui_exit disp=...` 一行(对照 W4 快照口,调试用)。

### 4.4 双口径与热重载的关系

`test()` = headless:**不开窗**,shim 注入缝(gui_inject_click/key)喂数,断言读回
命令缓冲探针(gui_cmd_*)——域库面已全有,driver.ct 只是薄封装。`run()` = 窗口。
`CTRON_GUI_HEADLESS` env **退役**(用户面不再出现;夹具如需保留由阶梯自管)。
热重载:L1 不含(§1 非目标);W4 两口径在夹具继续存活至 L2 §6.3 落地。

### 4.5 C shim:L1 不动,L2 收缩

L1:`ctron_gui.c` 一个字节不改;其窄桥(gui_cfg 11 参 packed 口径在内)经
`bind/shim.ct` 收编为包内 extern——**packed 口径的丑陋被封装在包内**,用户永不直呼。
L2/引擎对齐:按规范 §11.2 收缩为 measure 桥 + flush 两件;bind/{clay,raylib}.ct
承接布局发射(骨架常量直驱);注入缝/探针保留为测试真源(阶梯依赖)。
`#[repr(c)]`/Str 编组地基已由 tests/ffi 兑现(§3.3 注记),收缩是纯工程活。

### 4.6 Worked example:目标版 gui_calc 全文(L1 口径)

```ct
// examples/gui_calc/src/main.ct —— 全部用户代码(≈660 行业务 + 以下装配)
use gui.{run_kb, Click}

fn main() -> I32 {
    var src: Str = read_file("app.ctml")            // 发射锚(L2 随内嵌形态消失)
    var m = CalcModel()                              // model.ct:class + 状态机
    var clicks = List[Click]()
    clicks.push(Click("d7",  fn() { press_digit(m, "7") }))   // 独立处理体 ×19
    clicks.push(Click("add", fn() { press_op(m, "+") }))
    // ... 其余按键逐项声明(收口装配点)
    run_kb(src, "ctron calculator", 320, 420,
        fn(name: Str) -> Str { return m.display },   // 绑定:唯一 UI 出口
        clicks,
        fn(k: I32) { var n: Str = keymap(k)          // 键盘:码→动作名映射
                      if n != "" { dispatch(n, m) } })
    return 0
}
```

消失清单(对照今天 1958 行):extern 102、词法+解析 425、样式查询 37、布局发射 146、
命中 70、事件循环与双口径 282、headless 脚手架与场景 ≈185(**场景移入 test 块,§6**)。
保留:状态机 275、十进制算术 605(业务;格式化 150 行随 §7-④ 可下沉 std)、
app.ctml 44、装配 5。**用户树零 `#[trusted]`、零平台语义。**

## 5. L2:comptime CTML(编译期)

### 5.1 管线(严格沿规范 §11.1,本文补工程缝)

- **语法缝**:lexer 增 `view`/`style` 两块级关键字;parser 认领块边界后整块 token 流
  移交 gui 前端(规范 §11.1 C2 裁决,内嵌形态);独立 `.ctml` 走标记 tokenizer 全量解析。
  **内嵌/独立同语法同路径**(§4.1),物理位置不同而已;
- **gui_parse → 骨架 IR + 槽表 IR**:`GuiNode{tag, style_id, child_fc, child_ns,
  ev_slot}` + `Slot{node, attr, expr_id}`;**fc/ns 表示法从 L1 运行时直通 IR**,
  runtime.ct 的遍历代码零改动换数据源;
- **gui_lower**:令牌求值/extends 展开/多类合并/颜色校验(§5.3)→ 样式表常量;
- **三产物**:cc_run = 骨架解释执行(runtime.ct 换驱动源,解释口径热重载宿主 §11.5);
  cc_check = E8xxx 静态诊断;cc_emit = `static const GuiNode gui_sk_N[]` + 槽三元组
  C 静态数据;
- **用户面变化**:`{ident}` → `{expr}`(纯读门 E8190);`on:` → 表达式事件(自动闭包,
  事件时求值);read_file 锚/钩子注册全删——`gui.run(Calc(model: CalcModel()))`,
  view props 携带 model(§10.3 形态)。

### 5.2 与 M1-d 的合流

M1-d(gui_parse/gui_check 认领 when/each/input + E 语料)就是 L2 的第一片。
合流后 canonical parse.ct 的运行时角色收缩为:测试语料对照 + 动态源(用户 read_file
自写解析场景)的参考实现;域包 runtime 改为只吃骨架 IR。

### 5.3 诊断面

E8100(未知标签/props 缺失)/E8110(绑定类型)/E8120(事件签名)/E8130(颜色)/
E8170(子回写)/E8190(纯读)按规范 §4.4 注册进 check 口径;L1 的 panic 文案已按
同码书写,负例语料两口径差分(§11.7 黄金基线法)。

## 6. 测试面:`test` 块 + Driver(零新 DSL)

```ct
// examples/gui_calc/src/calc_test.ct —— headless 场景从 185 行脚手架变成十几行/场景
use gui.{test, Driver}

test "12 + 3 = 15" {
    var m = CalcModel()
    test(CALC_SRC, 320, 420,
        fn(name: Str) -> Str { return m.display },
        fn(name: Str) { dispatch(name, m) },
        fn(t: Driver) {
            t.click_name("d1"); t.click_name("d2"); t.click_name("add")
            t.click_name("d3"); t.click_name("eq")
            t.expect_text("display", "15")
        })
}

test "9 ÷ 0 = Error → AC 恢复" { … t.expect_text("display", "Error") … }
test "键盘 7*6=42" { … t.type_keys("7*6=") … }
```

- `click_name`:处理名 → 按钮节点 → 布局回填几何 → 注入中心点(不写死坐标,
  与今日 headless 同法);`type_keys`:逐键注入;`expect_text`:绑定名 → text 钩子
  读回 + 逐字节断言(panics 带 E8 文案风格);`frame`:推一帧(断几何);
- 驱动器复用域库注入缝,**真实鼠标事件路径已被 headless 与窗口双口径覆盖**
  (raylib 合并路径由阶梯 s4/s7 夹具背书);
- `ctc test`/suite 驱动 test 块的既有基建直接消费——GUI 测试与其他测试同一入口;
- 阶梯红利用途:s4–s12 夹具逐个换 Driver 形态后,各自 ≈200 行内嵌脚手架销账。

## 7. 语言/发射器修复清单(解锁表)

| # | 缺口(2026-09-19 实证) | 解锁 | 落点 |
|---|---|---|---|
| ① | class 字段赋值发射缺失(报错文本直排生成的 C) | model 用 `class`(引用语义)替代 `Box[Struct]` 变形 | trans_emit;GUI 优先级最高 |
| ② | `List[List[T]]` 外层索引读回段错误 | 树表示可换孩子表;`each` 嵌套组件(§4.3 TreeNode 案例)顺畅 | emit 容器泛型 |
| ③ | F64 `to_string` 原生截断(0.1+0.2→"0") | 浮点业务可显示;字符串算术降级为可选 | fmt_value 发射面 |
| ④ | 科学计数法字面量解析错(1e15→1.5315)+ I64 后缀 C 宿主宽度丢失 + as[U64] C 宿主红 | 定点/宽整计算可用;十进制算术 605 行有机会回到 200 行内 | 各自登记(部分已在册:r6c 注记) |
| ⑤ | 十进制定点格式化/解析缺失 | djoin/规范化/舍入 ≈150 行下沉 std/strconv(disp/parse_fixed) | stdlib 域计划顺手项 |
| ⑥ | **fn 值 Str 返回:原生发射无返回型转换**(调用协议恒返 ct_i;F/G 类型码不含返回型信息) | L1 绑定钩子被迫走 List 缓冲通道而非直返 Str;L2 `on:click` 表达式不受影响(语句位) | SL-0 探针实证(2026-09-19);需类型码携带返回型,跨宿主评审 |
| ⑦ | **F0(零参 fn 类型)端到端缺失**:ct_ty_code 对 fa<1 打回 "i";clov 闸门 np<1 panic;ct_cfn0/ct_fn0 typedef 缺;`fn(){}` 字面量 C 宿主崩溃 | Click 注册表(run: fn())与零参钩子 | SL-0 探针实证;另注:trans_ty ctype F0 漏映射已修(2026-09-19,域包 SL-0 顺手,严格改善) |
| ⑧ | **模块合并缺口**:跨模块泛型实例化 ctor 同名即 E5030(用户闭包推断 List[Str] vs 域包内部实例化);pub struct/pub extern 无解析路由(E2020.use.priv 只认 FnPub) | 域包被迫单文件形态(std/gui.ct 917 行);SL-0.7 修后拆分 | gui 域包 SL-1 实证 |
| ⑨ | C 宿主模块合并不确定性行为(同一内容两次等价构建 E5030↔段错误 139) | 待最小复现定界;域包单文件规避 | SL-1 过程观察,登记 |
| ⑩ | **List 删除原语缺失**(无 pop/remove_item;M1-b"计数截断模型"同源) | Todo 演绎的 remove 动作;过渡配方 = 过滤重建(§10 v10 actions.ct) | stdlib 域顺手项(2026-09-19 v10 修订登记) |
| ⑪ | **字段默认值缺失**(class/struct 字段 `= expr` 两宿主同拒;构造必须全字段字面量) | Todo Model 的 `todos/draft/next_id` 默认;过渡配方 = `make()` 构造器 fn(§10 v10 model.ct) | SL-0 探针实证(2026-09-19);语言工效项 |
| ⑯ | **read_file 跨宿主语义分歧**:解释口径返回 T/Some 包装(必须 match 解构),原生口径直返 Str——同一份用户代码无法双口径正确。最小复现:`var s: Str = read_file("app.ctml")` + `s.len` → 解释口径 panic "len target recv=T/Some"、原生 LEN:295 ✓ | GUI 开发快路径(解释口径热重载)被阻;修复归 C 宿主泳道(interp read_file 对齐 native 直返 Str,或 native 补 Option 语义——二选一须评审) | SL-2v 验证过程实证(2026-09-20,rf.ct 复现);gui 域包当前以原生口径为验收路径 |
| ⑰ | **闭包/fn 值参数限指针宽度**(struct 按值穿不过闭包槽);**闭包体 If 语句**发射非法 C;**空闭包体**解释口径段错误 | Driver 经 Box[Driver] 装箱;控制流提具名 fn;避免空体——三条配方已绕行 | SL-2v/SL-4 实证(2026-09-20) |

①②阻塞 L1 的**形态整洁**(不阻塞功能);③④⑤只影响业务代码写法,与 L1/L2 排期解耦。

## 8. 迁移与销账

| 对象 | 动作 | 时点 |
|---|---|---|
| std/gui/parse.ct | 升级深度 N + 多类( canonical 唯一落点,M1-d 同步义务兑现) | SL-2 |
| gui_counter | 切 `use gui`(首个消费者,竖切验证) | SL-3 |
| gui_calc | 切 `use gui` + test 块;README/设计记录更新 | SL-4 |
| ctron_gui.c | 不动 → L2 收缩为 measure+flush | SL-2 / L2 |
| 阶梯 s5–s12 夹具 | **内嵌快照冻结保留**(回归真源不追改);新夹具(如 use 域绿具)起用 Driver 形态 | 持续 |
| CTRON_GUI_HEADLESS env | 用户面退役;夹具如需由阶梯自管 | SL-4 |
| 两示例自包含定性 | "示例级快照"销账 → 正式形态;分析文 §5.1 随之销 | SL-4 |

## 9. 实施切片(每片独立验收)

| 切片 | 内容 | 验收 | 依赖 |
|---|---|---|---|
| SL-0 探针 | fn 值作为 run 形参、闭包捕获 model、struct 缺省域、`pub` fn 跨包、`List[struct{Str, fn}]` 容器发射——小 .ct 双宿主实跑。**已执行(2026-09-19)**:①Box 捕获 env 槽 int32 截断 → **已修**(trans_conc.ct ct_cap_decl B:/fn 分支);②Void 体闭包 shim 非法 C → **已修**(trans_conc.ct 尾发射 typeof 判 v);③F0/F4+ 类型码漏映射 → **已修**(trans_ty.ct 首字符泛化);④Str 返回/F0 端到端/容器发射 → **红,登记 §7 ⑥⑦**;⑤模块合并 E5030/pub struct 缺口 → **红,登记 §7 ⑧**。回归:自举 73/73;gui_counter 全绿 | 全绿项:Double 通道;红项分流 SL-0.6/0.7 | — |
| SL-1 域包骨架 | std/gui/Ctron.ctcl + gui.ct 入口 + bind/shim.ct 收编;`use gui.{run}` 可编译可调(空实现) | tests/gui/use_green 夹具:import+调 run 打印 rc=0 | SL-0 |
| SL-2 运行时 | parse.ct 深度 N 升级;runtime.ct 全量(布局/绑定/命中/事件/循环) | 域内自测:gui_counter 语义等价(点击计数/空白不命中) | SL-1 |
| SL-3 竖切 | gui_counter 切包:删内嵌解析器/循环/extern(913→≈60 行) | 该示例 run.sh 全绿;阶梯 14/14 不红 | SL-2 |
| SL-4 测试面 | driver.ct;gui_calc 切包 + 场景 test 块(2002→≈700 行) | 该示例全部场景 test 绿;run.sh = 构建+test;--run 窗口保留 | SL-2 |
| SL-5 语义承接 | runtime/parse 承接 when/each/input(M1-a/b/c 语义进包);widgets/basic 拆分 | s10–s12 语义的域包版夹具绿 | SL-2 |
| SL-6(L2-1) | gui_parse/gui_lower + cc_check E8xxx(M1-d 合流);独立 .ctml 检查 | E 语料黄金基线绿;M1-d 判据 | 排期与发射泳道协调 |
| SL-7(L2-2) | cc_run 骨架解释 + 骨架常量发射;内嵌 view/style 块 | §10.2 静态 Todo 形态可跑;渲染黄金帧差分绿 | SL-6 |
| SL-8(L2-3) | {expr} 绑定 + on: 自动闭包;钩子/锚退役 | §10.3 交互 Todo 形态可跑;calc 切表达式事件 | SL-7 |
| SL-9(L2-4) | 热重载宿主(骨架原址替换,§11.5)+ 渲染回归进 CI(F2) | 改样式/结构/逻辑焦点不丢(§6.3 硬验收) | SL-8 |

SL-1..5 = L1(季度内可完成,单人口径);SL-6..9 = L2(对齐规范 §7 工程量口径 M0–M2)。
排序原则:**L1 全程零编译器改动**,与发射泳道(§7 修复、在途切片)完全解耦,可立即动工。

## 10. 决策记录(本文裁决与对上游的申报)

| # | 裁决 | 理由 |
|---|---|---|
| D1 | `use gui` 裸包导入 = 导入 gui.ct 入口的 pub 面;组导入 `use gui.{run,Driver}` 同义可用 | 规范 §10 行文用裸 `use gui`;对 AI 写代码最省改动;显式组导入留给严格口味 |
| D2 | L1 只供独立 `.ctml`(read_file 锚);内嵌 view/style 块随 L2 语法缝落地 | 对 §4.1"内嵌优先(M0–M2)"的**顺序偏离申报**:L1 目标=零编译器改动,内嵌形态必须动 lexer/parser;两形态同语法同路径的承诺不变 |
| D3 | L1 事件 = **handler 注册表**(`List[Click]{name, fn()}`,逐项闭包、独立处理体);单 dispatch 钩子降为 SL-0 探针红时的**回退形态**。文本绑定保持单钩子(绑定少,表形态过重);键盘保持单钩子(码表映射本就是函数);L2 表达式事件就地化后注册表退役 | 业内主流认知 = 声明点闭包(React/SwiftUI/Flutter);注册表兼得"收口装配点+独立处理体";可枚举 → Driver 按名寻址/动作清单零成本;依赖 fn 容器发射(List[struct{Str,fn}]),SL-0 定案 |
| D4 | 键盘钩子走 `run_kb` 双入口而非可选参数 | 语言无重载/缺省参;双入口成本 20 行,合并随 L2 |
| D5 | L1 无脏追踪,每帧全量重放 | §12.3c 裁决 Clay 本就全量重放;工具应用 F1 门槛内;求值缓存是 L2 范围(C3) |
| D6 | 树表示 fc/ns 而非平行表定界/嵌套容器 | 双宿主实证;`List[List]` 修复后允许换(纯内部,不触 API) |
| D7 | headless 判断退出用户面:test() 强制 headless | 分析文核心诉求之一;env 开关是夹具遗产 |
| D8 | 阶梯内嵌快照冻结不追改 | 夹具=回归真源;域包=新真源;两真源短期并存,SL-5 后新夹具只走域包 |
| D9 | shim 注入缝/探针长期保留 | 测试真源(阶梯+Driver 共用);非用户面 |
| D10 | 诊断码 L1 就以 E8xxx 文案出现在 panic | L2 check 面语料差分无缝;用户习惯提前对齐 |

## 11. 风险与防线

| 风险 | 防线 |
|---|---|
| SL-0 探针红(fn 值容器/闭包捕获发射缺口) | D3 已备最低形态(单 fn 形参);对应 §7 项前置,不阻塞 SL-1/2(extern 桥本就经包内声明) |
| 与 M1 阶梯并行冲突(同 std/gui 树) | SL-1/2 动工前查 peer 占用;parse.ct 升级与 M1-d 同步义务一次性合流 |
| 域包运行时的解释口径帧率(F1) | gui_calc 39 命令/帧,余量大;千元素级超门槛即按 §11.6 转原生口径验证 |
| 两真源漂移(夹具快照 vs 域包) | D8 冻结 + SL-5 收口;漂移窗口期内示例 README 注明谁是行为真源 |
| L2 编译器排期与发射泳道争用 | L1 独立交付(零编译器改动)本身是防线;SL-6 起排期前登记 roadmap(规范 §8) |
