# GUI 正式示例(examples/gui_counter + gui_cjk)——设计记录

> 状态:已实现(2026-09-19 会话;方向经用户确认 = examples/ 正式示例,方案 A 双示例)。
> 上游:`2026-09-16-gui-ctml-design.md`(§10 演绎为目标形态)、`2026-09-16-gui-mvp-ladder.md`
> (S1–S9/W1/W2 已落库)。定位:**examples/ 门面**,非测试夹具——严格验证仍归 tests/gui 阶梯。

## 0. 本文回答什么

GUI 泳道能力面已到"可演示"级(S6 绑定竖切 + S7 真窗口 + S9 中文窗口),但 examples/ 下
没有 GUI 示例,能力对外不可见。本文定两个自包含示例的内容、形态与验收口径,以及
当前能力面约束(限深 1/单绑定/ASCII 窗口文本)的如实标注方式。

## 1. 目标与非目标

目标:

1. examples/ 落两个 GUI 示例,各亮一个卖点:
   - **gui_counter**——声明式路径门面:`.ctml` 结构+样式 → 解析 → `{ident}` 绑定 →
     `on:click` 事件 → Clay 布局 → flush 绘制,全链路一份源码可读;
   - **gui_cjk**——中文一等门面:FreeType 中文纹理窗口 + 真实点击计数(S9 形态)。
2. 每示例双运行口径:headless(默认,自动断言,无显示依赖)/ 窗口(`--run`,交互验收)。
3. 零编译器改动、零夹具改动、零 std/gui 改动——纯增量目录。

非目标(明示不做):

- 不做 `use gui` 包级集成(随 P2-B;示例暂内嵌解析器快照,与夹具同口径);
- 不做 Clay TEXT → FreeType 的渲染集成(M3 在途;gui_counter 窗口文本保持 ASCII);
- 不追求 CTML 完整语法(each/when/input 未实现,写了会误导)。

## 2. 形态决策

| 决策点 | 定案 | 依据 |
|---|---|---|
| 自包含形态 | 每示例自带 src/ + c_src/ + app.ctml + run.sh | 夹具同款已验证模式;P2-B 落地后统一切 `use gui` |
| 同源登记 | shim/解析器文件头注明同步自 s6/s7/s9 | 防漂移:阶梯更新时示例随之手动同步 |
| headless 开关 | `env_get("CTRON_GUI_HEADLESS")` | 发射/解释双口径已支持 env_get;**不占 argv**——`run <file>` 是编译器自举锚约定,会顶替 main 首个 read_file 锚 |
| read_file 锚 | gui_counter 的 main 首语句 = `read_file("app.ctml")` | 与 s5/s6 夹具同机制:无 `run` 参数时按 CWD 相对读取(run.sh 已 cd 示例目录) |
| 命中测试 | 从 Clay 命令缓冲收集 RECT 命中(s6/s7 写死矩形泛化) | 示例应展示真实模式:几何来自布局回填,双按钮可区分 |
| gui_cjk headless | ft_shim 副本加探针(ft_last_w/ft_last_h/ft_probe_nonzero) | s9 shim 无读出口;探针纯增量只进示例副本;字体缺失 skip(rc=0)——严格断言归 s8/s9 夹具 |
| 目录命名 | gui_counter / gui_cjk(下划线) | 与 tests/gui 夹具命名一致 |

## 3. 各示例设计

### 3.1 examples/gui_counter

```
gui_counter/
  Ctron.ctcl          pkg 清单(name = "gui_counter")
  README.md           是什么/怎么跑/能力边界
  app.ctml            view Counter:label "count: {count}" + button "+1"(btn)+ button "clear"(btn-danger)
  src/main.ct         双模式入口(蓝本 s6 headless 断言 + s7 窗口循环)
  c_src/ctron_gui.c   s7 shim 副本(超集:inject 队列 + raylib 合并 poll + Clay 桥 + cmd 探针 + flush)
  run.sh              emit → cc 链接 vendored raylib;默认 headless;--run 开窗
```

headless 口径(自动):初始帧 label 字节断言 "count: 0" → inject 点击 btn1 ×3 →
逐字节断言 "count: 3" → inject btn2 ×1 → 断言 "count: 0"(clear 生效,命中未串位)→
inject 空白区 ×1 → 计数不变(负例)→ rc=0。

窗口口径(`--run`):s7 循环——poll 合并真实鼠标 → 几何命中 → dispatch → 重布局 →
flush;60fps。

### 3.2 examples/gui_cjk

```
gui_cjk/
  Ctron.ctcl          pkg 清单(name = "gui_cjk")
  README.md
  src/main.ct         双模式入口(蓝本 s9;headless 冒烟 + 窗口交互)
  c_src/ft_shim.c     s9 shim 副本 + 3 探针(读渲染缓冲宽/高/非零 alpha 计数)
  run.sh              默认:构建 + headless 冒烟(无 CJK 字体则 skip 打印);--run 开窗
```

headless 口径:`ft_load_cjk(24)` 失败 → "skip" rc=0;成功 → `ft_render("Ctron 中文窗口")`
→ 断言宽/高 > 0 且非零 alpha 像素计数 > 0(对齐 s8 的位图非空口径)。
窗口口径:标题纹理(静态)+ 计数纹理(dirty 重渲)+ 圆角按钮真实点击,同 s9。

## 4. 验收口径

1. `sh examples/gui_counter/run.sh` headless 全绿(rc=0,断言全过);
2. `sh examples/gui_cjk/run.sh` 构建链接 OK + headless 冒烟绿(或字体缺失 skip);
3. `--run` 交互验收:counter 点按钮计数变化;gui_cjk 中文显示 + 点击计数增长
   (与本会话 s9 交互验收同法,手动);
4. 回归:tests/gui 阶梯全绿(示例为纯增量,不应碰红任何夹具);
5. 每示例 README 如实标注能力边界:限深 1、单 `{ident}` 绑定、仅 on:click、
   窗口 ASCII 文本(M3 Clay→FreeType 集成后切中文)、解析器快照待 P2-B 切 `use gui`。

## 5. 漂移对策与后续演进

- shim/解析器为 s6/s7/s9 的登记副本:阶梯修复(如 emit 缺口修复、F32 边界恢复)时
  随 PR 手动同步两示例;文件头注释为同步指令的落点;
- **shim 定性为过渡形态(2026-09-19 目标增补,GUI 规范 §1.1 目标 7/§11.2)**:
  用户面零 C 为交付口径——bind 层(MVP 阶梯 W6)落地后删除两示例的 c_src,
  示例源码树零 C;用户写 C 仅剩第三方接入逃生口场景;
- P2-B 包级 `use gui` 落地 → 两示例切换为正式形态(删自包含快照),本文件随之销账;
- M3 Clay TEXT → FreeType 集成落地 → gui_counter 的 app.ctml 文本切中文,
  与 gui_cjk 合流为单一综合示例的评估点。
