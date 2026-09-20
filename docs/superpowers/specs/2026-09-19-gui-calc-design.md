# examples/gui_calc(仿 macOS 计算器)——设计记录

> 状态:已实现(2026-09-19 会话;用户提议"用 Ctron GUI 写一个参考 macOS 计算器的例子")。
> 上游:`2026-09-19-gui-examples-design.md`(gui_counter/gui_cjk 门面)、`2026-09-16-gui-ctml-design.md`。
> 定位:**综合示例**(深度 2 嵌套布局 + 状态机 + 字符串算术 + 双输入),门面非夹具。

## 0. 本文回答什么

GUI 示例已有 counter(声明式最小面)与 cjk(中文直绘),缺一个"接近真实应用"的综合
示例。计算器正好压到当前能力面的三条边界:容器嵌套(网格按键)、可变 model
(状态机)、非整数运算(小数),并顺带验明 Latin-1 字形面。

## 1. 目标与非目标

目标:examples/gui_calc 双口径(headless 自动断言 / --run 真窗口);零夹具改动;
域库只允许纯增量微调。

非目标:热重载环(gui_counter 已演示);CTML `use gui` 包级集成(P2-B);
等号重复求值 / AC 的 C 态 / 科学计数显示(macOS 差异如实记录于 README)。

## 2. 关键决策

| 决策点 | 定案 | 依据 |
|---|---|---|
| 数值方案 | 十进制字符串算术(竖式加减乘 + 长除,12 位小数四舍五入) | F64 双口径 to_string 均不可靠(原生截断、科学计数字面量错);I64 后缀 C 宿主解释口径宽度标记丢失;`as[U64]` C 宿主在册红(2026-09-19)。字符串算术是唯一双口径一致且 0.1+0.2=0.3 精确的路 |
| 可变 model | `Box[Struct]`(disp/acc/op/typing/err 全 Str/Bool) | 发射口径 class 字段赋值未支持("struct 字段赋值未支持"直排入 C);struct 局部字段赋值 + Box 写路径双口径已实证 |
| 解析器 | s6/s7 快照扩展:容器递归(限深 2)+ 多类样式(后类覆盖) | 计算器网格必须嵌套;CTML 嵌套目标语义预演 |
| 树表示 | 首孩子/下一兄弟两条 List[I32] | List[List[I32]] 原生口径外层索引读回段错误(未落地);两条 I32 表 + 索引赋值是双口径一致最小面 |
| 字形 | ÷ × ± 用 Latin-1(32..255);减号 ASCII '-' | raylib 默认字体 224 字形;U+2212 超面 |
| 键盘 | GetKeyPressed 合并路径 + gui_inject_key 同路断言 | 域库 S4 面现成;0-9 . + - * / = Enter ESC % |
| 域库改动 | 注入队列 64→256(std/gui/c_src/ctron_gui.c) | 队列进程累计不回卷,gui_calc 全场景 ~70 注入,64 静默丢尾;对全部夹具纯增量(GUI 阶梯 14/14 复验) |

## 3. 结构

```
gui_calc/
  Ctron.ctcl  app.ctml(view Calc:根 vbox → display label + 5×hbox 行 → 19 button;
              style root/display/row/num/fn/op/zero,"num zero" 双宽演示)
  src/main.ct 迷你词法(s6/s7 快照)→ 深度 2 解析器 → Box[Model] 状态机 →
              字符串算术 → 递归 Clay 布局 → 命令缓冲断言(headless)/ flush(窗口)
  run.sh      构建 + headless;--run 开窗(对齐 gui_counter)
  README.md   用法 + 能力边界如实
```

headless 验收(命令缓冲逐字节断言):12+3=15;9÷0=Error→AC 恢复;0.1+0.2=0.3;
1÷3=0.333333333333;50%=0.5;5±=-5;2+3+ 链式折叠显示 5,+4==9;键盘 7*6==42;
空白区不命中;9999999999×9999999999=Error;12×5=60。命令数恒 39
(display TEXT + 19×(RECT+TEXT)),rects 恒 19。

## 4. 验收记录(2026-09-19)

1. `sh examples/gui_calc/run.sh` headless 全绿;
2. `--run` 真窗口:macOS 视觉(深面板/右对齐大字/浅灰功能行/橙色运算符/双宽 0/
   ÷×± 字形)经窗口截屏确认;
3. 回归:tests/gui 阶梯 14/14 绿;gui_counter run.sh 全绿(含快照往返与 E3);
4. 遗留(环境面,非应用):合成鼠标/键盘事件跨 Space 未送达后台会话窗口
   ——窗口口径的真实鼠标交互由 gui_counter 同源路径背书。

## 5. 演进

- P2-B `use gui` 落地 → 删自包含解析器快照;
- 发射口径 class 字段赋值落地 → Box[Struct] 可换 class(或维持现状,Box 即正规);
- M3 Clay→FreeType 落地 → 按键面切 ÷ × − ± 全 Unicode 字形;
- I64/F64 双口径修绿后,字符串算术可作为定点算术教学附录保留(精确无浮点是卖点)。
