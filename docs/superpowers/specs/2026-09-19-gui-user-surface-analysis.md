# GUI 用户面收敛分析——把非业务逻辑从应用代码里拿走

> 动机:2026-09-19 会话用户审阅 examples/gui_calc 后指出——写一个计算器,`#[trusted]`
> C 声明一大片、解析/布局/命中/事件循环等非业务代码喧宾夺主。本文量化问题、给出
> 每一块非业务代码的"去处",并对齐/扩展既有规划(P2-B `use gui`、M1 编译器对齐、规范 §10)。
> 性质:**分析 + 路线**,不是新方向——目的地仓库已写定,本文补的是差距量化和缺口清单。

## 1. 问题量化:gui_calc 里用户到底替平台写了多少

examples/gui_calc = app.ctml 44 行 + src/main.ct 1958 行。逐段归类:

| 段落 | 行数 | 归类 |
|---|---|---|
| 29 个 `#[trusted] extern "c"` 声明 | 102 | 平台 |
| 迷你词法(ws/word/lit/str_lit/span_until) | 156 | 平台 |
| 颜色解析(hex1/parse_color) | 58 | 平台 |
| **十进制字符串算术(竖式/长除/舍入/规范化)** | 605 | **业务**(其中格式化/规范化 ≈150 行属通用件) |
| **状态机(Model/press_*/dispatch/key_handler)** | 275 | **业务** |
| CTML 解析器(深度 2 嵌套扩展) | 269 | 平台 |
| 样式查询(cls_prop 多类覆盖) | 37 | 平台 |
| 布局发射 + 命中(emit_node/collect_rects/hit_rect/handle_click) | 146 | 平台 |
| headless 断言脚手架(chk/assert_disp) | 25 | 平台(测试) |
| main 双口径(解析→分支→事件循环→flush) | 282 | 平台(其中 headless 场景脚本 ≈180 行本质是**测试用例**) |

**结论:业务 ≈ 880 行,平台 ≈ 1080 行——用户为写 1 行业务被迫写 1.2 行平台胶水。**
且胶水里有三处是"语言/发射器缺口逼出来的变形"(Box[Struct] 代 class、首孩子/兄弟链
代 List[List]、字符串代浮点),缺口修掉后业务本身还能更短。

## 2. 每一块的去处

| 非业务块 | 去处 | 机制 | 现状 |
|---|---|---|---|
| extern "c" 声明面 | `std/gui` 包 | `use gui`(P2-B 包级集成);bind 层收编,窄接口原则不变(§3.3 #[trusted] 收敛在 std 树) | 已规划;std/gui/bind/raylib.ct + c_src/ctron_gui.c 单一真源已就位,只差 use 接线 |
| 迷你词法 + 解析器 + 样式查询(≈462 行) | **编译器** | comptime CTML:`ctron build` 解析 .ctml → 骨架 + 样式常量;错误归 E8xxx 诊断(编译期报,不进运行时) | 规范 §10.2"幕后"已定;M1-d(gui_parse/gui_check 认领 when/each/input)在途;M1-a/b/c 运行时语义已交付 |
| 布局发射(绑定求值 → Clay 命令) | `std/gui` 运行时 | `gui::draw(骨架, 绑定上下文)`——遍历树、求值 `{}` 绑定、产命令 | 规范 §6 已定;parse.ct(canonical,452 行)已在 std 树待 use |
| 几何命中 + 事件路由 | `std/gui` 运行时 | poll → RECT 命中 → 按 handler 名/fn 值回调;用户侧只剩 `on:click={press(7)}` | 规范 §7;M1-b2(逐项事件句柄)登记在案 |
| 窗口/事件循环/双口径 main | `std/gui` | `gui.run(app)`——开窗/60fps/poll/flush 一体;headless 由测试驱动器掌舵 | 规范 §10.2 形态(`gui.run(TodoApp(model: ...))`) |
| headless 断言脚手架 | **工具链(缺口,新增)** | `ctc test`:事件脚本(click/type/expect text)驱动域库既有注入缝,断言命令缓冲 | 未规划——域库注入/探针已全有,缺用户侧 scenario 面与驱动器 |
| parse_color/hex/cls_prop | 编译期样式表 | .ctml style → 常量;主题令牌 std/gui/theme.ct 已有种子(§13.3) | M1-d/P2-B 范围 |
| Box[Struct] 代 class(状态可变) | **语言修复** | 发射口径 class 字段赋值落地(当前报错文本"struct 字段赋值未支持"直排进生成的 C) | 编译器缺口,本例暴露 |
| 首孩子/兄弟链代 List[List] | **语言修复** | `List[List[T]]` 原生外层索引读回修复 | 编译器 bug,本例暴露 |
| 字符串算术代浮点(≈150 行格式化除外) | 语言修复 + std | F64 to_string 原生截断/科学计数法字面量/I64 后缀 C 宿主/as[U64]——修绿后业务可回到 F64 或定点;十进制定点格式化可进 std/strconv | 数值四缺口在册(2026-09-19 探针实证) |

## 3. 收敛后:用户眼里的计算器

```
gui_calc/
  Ctron.ctcl        # name = "gui_calc";dependencies: gui = { std = true }
  app.ctml          # 44 行,不变(view Calc + style ×7)
  src/model.ct      # ~15 行:class CalcModel { var disp/acc/op: Str; ... }
  src/calc.ct       # ~600 行:状态机 + 十进制算术——纯逻辑,零平台符号
  src/main.ct       # 3 行:fn main() -> I32 { gui.run(Calc(model: CalcModel())) }
```

- 用户可见符号:`use gui` 一个;`#[trusted]` 从用户树消失(想自定义才出现);
- 事件:`on:click={press_digit(model, "7")}`——表达式事件(规范 §10.3 已定),dispatch
  if-chain、key_handler 映射表、hit-test 全部消失;
- 测试:`ctc test gui --script tests/calc.scn`(click 12 / type + / expect display 15),
  headless 断言从"示例作者手写 180 行"变成"测试脚本十几行";
- 业务 660 行 vs 现在 880 行(且无变形),**平台胶水 1080 → 0**。

## 4. "除非他想自定义"——保留的逃生口

收敛不是封死。`use gui` 之后这些口子全部保留:

1. **原始 FFI**:用户仍可写 `#[trusted] extern "c"`(§3.3 原则:收敛建议而非禁止);
   gui_calc 的快照形态本身就是这个逃生口的活文档——它证明了用户级平台代码可写可用;
2. **自定义解析/资产**:`read_file` + 自写解析器仍合法(带资源热重载场景);
3. **自绘/自定义控件**:窄绘制口(ctron_gui_flush 级)对 `use gui` 用户暴露为
   `gui::custom(draw: fn)` 一类缝,shim 窄接口原则不变;
4. **主题整体替换**:theme.ct 常量经 `use gui.theme` 导入,用户包可整体覆盖(§13.3)。

## 5. 实施排序(按"每单位改动删多少用户胶水"排)

1. **P2-B `use gui` 包级落地**——删 extern + 布局 + 命中 + 循环 ≈ 530 行/示例,
   单点收益最大;canonical parse.ct 已在 std 树等消费者;两示例(gui_counter/gui_calc)
   切正式形态后自包含快照即销;
2. **编译期 CTML(M1-d 延伸)**——删解析 + 样式查询 ≈ 500 行,且把 UI 错误从运行时
   panic 提前到编译期 E8xxx;保存即见的热重载由"重编译骨架"承接;
3. **`ctc test` GUI 口(新增)**——删断言脚手架;域库注入缝/命令探针已全有,
   缺 scenario 语法与驱动器;阶梯夹具同步受益(各夹具可删各自内嵌快照);
4. **语言/发射器四修复**——class 字段赋值、List[List] 原生、F64 格式化、
   科学计数法字面量:让业务代码不再需要变形(Box/兄弟链/字符串算术);
5. **std 助手(可选)**——十进制定点格式化进 strconv;大数运算若计算器类应用
   成为常见诉求再议 std/decimal(YAGNI)。

## 6. 原则对齐

Ctron 设计原则"AI 写人类读 / 无魔法 / 简洁"在 GUI 上的落地口径:
**平台层本身也必须是可读的 Ctron**(parse.ct/theme.ct 就是 Ctron 写的,C 只剩 shim
一条窄缝)——用户永远可以顺着 `use gui` 往下读,读到底只是薄 C;而不是被一个
黑盒框架接走。示例的职责随之改变:示例只展示业务 + 声明,平台怎么工作由
std/gui 源码和阶梯夹具回答。

## 7. 与既有计划的关系

- 不新增方向:P2-B/M1-d/规范 §10 均已写下这个目的地;
- 本文新增的缺口两项:`ctc test` GUI 口(§5.3)、发射器缺口清单的 GUI 视角归集(§5.4);
- gui_calc/gui_counter 的"自包含快照"定性不变(示例级过桥形态),P2-B 落地即切换,
  切换后两示例合计可删 ≈1600 行平台代码。
