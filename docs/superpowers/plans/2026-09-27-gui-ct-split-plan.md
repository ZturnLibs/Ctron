# gui.ct 拆分规划

> 背景:pkgs/gui/gui.ct 已 8184 行(215 fn + 101 extern),单文件体量超过
> lib/std/std.ct(最大 lib 域)。本文件规划拆分目标/约束/阶段/依赖。

## 现状测量

| 区段 | 行范围 | 行数 | 内容 |
|---|---|---|---|
| extern 窄桥 | L18–402 | 385 | 101 个 C extern 声明 |
| 迷你词法 | L403–583 | 181 | gws/gword/glit/gstr/gtok_* |
| **主题面** | L584–1957 | **1374** | Theme struct + 9 主题 + 令牌解析 + 槽写入 |
| 跨文件导入 | L1958–2394 | 437 | GUI-27 锚合并 |
| 运行时辅助 | L2395–2473 | 79 | 绑定求值/布局发射/命中 |
| 表达式求值器 | L2474–2602 | 129 | SL-8b bxv_* |
| 下标/切分内建 | L2603–3220 | 618 | GUI-13 |
| 事件表达式 | L3221–3369 | 149 | SL-8c-3 |
| **事件实参+渲染分派** | L3370–4784 | **1415** | SL-8c-4a + rt_emit + 交互态 + 焦点/光标 + 快捷键 |
| **编辑管线+窗口循环** | L4785–6203 | **1419** | input/textarea 编辑 + rt_window_loop 双循环 |
| 入口+驱动 | L6204–6992 | 789 | test/test_sk + Driver + d_* |
| 输入辅助 | L6993–7619 | 627 | char_str + 热重载 + 杂项 |
| 程序化聚焦 | L7620–7725 | 106 | focus/scroll_into_view |
| on:after | L7726–8184 | 459 | 定时事件 |

## 约束

1. **loader 单文件解析**:`use gui.{X}` → `pkgs/gui/gui.ct` 一个文件,同目录兄弟文件
   (parse.ct/theme.ct)不被选择性合并扫描——**多文件域包 = loader 兄弟文件 import,
   根本阻塞(peer 在册)**。
2. **跨包 use 可用**:域包可 `use 其他域名.{符号}` 选择性合并(如 `use gui_theme.{Theme}`),
   条件 = 被引方在仓库根/pkgs/ 有独立 .ct 文件。
3. **peer 在飞**:gui_parse.ct 8c-4④迁移中,gui.ct 本体暂无在飞(可安全编辑)。
4. **parse.ct/theme.ct 死副本**:已实证零消费,可安全删除(清理项)。

## 拆分方案(两阶段)

### 阶段一:零 loader 依赖(立即可做)

**只拆一个自洽块——主题面(1374 行)→ `pkgs/gui_theme/gui_theme.ct`。**

- 主题面是 gui.ct 中最大的自洽块:Theme struct + 9+1 主题函数 + theme_apply +
  令牌解析(gtok_*/g_parse_color/g_parse_alpha) + gui_theme_slot/gui_alpha extern
- 零依赖:不引用 GuiTree/rt_emit/编辑管线/解析器——纯数据+纯函数
- gui.ct 通过 `use gui_theme.{Theme, theme_apply, theme_default, ...}` 引入
  (跨包 use,loader 已支持)
- 消费端 use 面不变:gui.ct re-export(或有方案后直接从 gui_theme 取)
- **gui.ct 减重 1374 → ~6800 行**

后续可按同模式继续拆:
- `pkgs/gui_hotkey/gui_hotkey.ct`(~80 行,快捷键注册/匹配,零依赖)
- `pkgs/gui_timer/gui_timer.ct`(~100 行,tick 原语+on:after,零依赖)

每拆一件 = 独立 commit + 阶梯全绿验证。

### 阶段二:loader 兄弟文件 import 落地后(需编译面)

**loader 支持 `pkgs/gui/*.ct` 同目录多文件选择性合并后:**

| 新文件 | 内容(从 gui.ct 迁出) | 行数 |
|---|---|---|
| gui_parse.ct | gt_parse/gt_node/gt_attrs/gt_style/gt_leaf_text + 迷你词法 | ~800(替换死副本) |
| gui_render.ct | rt_emit + 交互态折叠 + overlay/focus/cursor + hotkey | ~700 |
| gui_edit.ct | 编辑管线(rt_input_char/key)+ 焦点环 + 剪贴板 | ~500 |
| gui_driver.ct | Driver struct + test/test_sk + d_* 全族 + 窗口循环 | ~800 |
| gui_expr.ct | bxv_* 求值器 + 下标/切分 + 事件表达式 + 实参解码 | ~900 |
| gui.ct | extern + 入口 + 组件声明 + 胶水 | ~2000 |

最终 gui.ct ~2000 行(入口+胶水),子文件各 500–900 行。

### 阶段三:组件库独立(可选)

Select/WList/Dialog 组件视图 + w-* 样式预设 → `pkgs/gui_widgets/`(或 gui/widgets.ct)。
依赖阶段二 loader 支持后自然可达;消费 = `use gui.widgets.{Select, ...}`。

## 执行要点

- **阶段一立即可做**:无需 peer/loader 改动,零编译面触碰
- **阶段一验证**:阶梯 52+ 全绿 + gui_themes gui_snippets 兼容(use 面变更为跨包)
- **阶段二前置**:peer loader 兄弟文件 import 落地(路线图在册)
- **parse.ct/theme.ct 清理**:确认零消费后 `git rm`(独立小 commit)
- **不急的事**:渲染/编辑/驱动三块互相纠缠最深,放阶段二最后拆

## 执行状态(2026-10-04,三阶段全部落地)

- 阶段一 40b08df4:主题面→pkgs/gui_theme(572 行;规划 1374 估算含解析器前缀,实测主题面 572)。
  装载器解锁两件:可见性门移到内嵌 use 展开之后(门面 re-export);seed env_get 桩→真 getenv(site-root 在 seed 首次走通)。
- 清理 7d0717fa:parse.ct/theme.ct 死副本删除。
- 阶段二① 1580a606:解析面→gui_parse.ct+消费方引用闭包(门面跨文件引用进 keep)。
- 阶段二② 37fcc25e:表达式面→gui_expr.ct+顶层 redo 隔离重展开(desugar 注入 use 路径)。
- 阶段二③④⑤ c1873a49:渲染/编辑/驱动三面独立+嵌套 wholesale 合并(嵌套站点不过滤,全局收敛在顶层闭包)。
  门面终态 520 行(头注+use 面+GuiTree+extern 窄桥+运行时辅助);**GuiTree struct 留驻门面=终态**(发射序+结构可见性本位,勿再外移)。
- 阶段三 3a0c2562:组件库 pkgs/gui_widgets(Select/WList/Dialog+w_* 令牌预设),gui_snippets 首消费方;⚠️ kebab 类名跨包合并样式不应用(在册观察),库取 snake_case。
- 终态行数:gui.ct 520 / gui_parse 1431 / gui_expr 943 / gui_render 1481 / gui_edit 985 / gui_driver 2329 / gui_theme 572 / gui_widgets 60。
- 阶段三全量统一 360b04dd:库扩容至 16 组件规范形态(纯 prop 化,统一对话框三路契约);**内嵌词法 UTF-8 叶能力落地**(is_ac/is_ident_start/cont 加 ≥0x80,双宿主同批;推翻「中文叶须文件形态」在册限制);gui_snippets 适配统一契约+库全事件名集桩(骨架面按合并库全量注册事件表)。陈列室消费缓行两在册缺口:手写 bind×类型化列表 props、自定义键闭包×desugar 形态(候专门切片)。
- 验收线:阶梯 82 过/1 败(s30_props_d 在册)+ suite 100/100 双列 + ffi 37/0 + smoke 170/3(decls 锁漂移/conc_parallel/Rust 臂,均基线既有)。
