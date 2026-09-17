# GUI MVP 阶梯(M0 最小路径)——实施计划

> 上游规范:`docs/superpowers/specs/2026-09-16-gui-ctml-design.md`(§3 架构/§7 里程碑/§11 实现/§12 依赖/§13 组件与布局)。
> 方法:仓库传统——黄金差分、注入式测试、每步当日可验证。
> 总原则:**零编译器改动先跑通全链路**——vendored C 库 + 薄 shim + 运行时解析 `.ctml`;
> 编译器集成(comptime 折叠/内嵌关键字/E8xxx)整体排第二波。

## 现状基线(2026-09-16)

- FFI 三主债(repr(c)/Str 编组/C-ABI 回调)已由并行泳道在工作区兑现,`tests/ffi` 11/11 绿,
  **待落库(S0)**;
- 包配置已切 `Ctron.ctcl`(`pkg { ... }`,P2-B 落地中)——GUI 夹具随新格式;
- `vendor/gui/` 已置 raylib 5.5 精简源(13MB,rglfw 单文件)与 clay v0.14 单头(266KB);
- 编译器原生二进制可用:`compiler/bin/{ctron-cc,ctron-emit}`。

## 泳道阶梯 S0–S6

### S0 前置落库(0.5 天,协调为主)

- 任务:①协调 FFI 泳道提交工作区批次(8 编译器源 + tests/ffi + §9.6 规范修订);
  ②GUI 规范 + 本计划落库;③roadmap 登记 GUI 泳道一行;
- DoD:提交后 `compiler/test/suite.py` 全绿;两份文档进 git。

### S1 第一像素:raylib 绑定冒烟(1–2 天)

- 产出:`vendor/gui/build.sh`(静态库);`std/gui/bind/raylib.ct`(S1 七函数,
  全标量);`tests/gui/s1_smoke/`(Ctron.ctcl + src/main.ct + c_src/gui_shim.c);
- 窄接口(S1 集,全标量,零 struct 过桥——struct 留给 S2 定点验证):
  `InitWindow/CloseWindow/WindowShouldClose/SetTargetFPS/BeginDrawing/EndDrawing`;
  shim:`gui_clear(r,g,b,a) / gui_draw_text(s,x,y,size) / gui_frame_count()`;
- 验证:`cc` 链接成功;冒烟程序开窗画 "ctron gui" 300 帧后 rc=0 退出
  (自动化验 rc + 帧计数;**窗口运行本身为交互验收**,CI 降级为构建+链接断言);
- DoD:`sh tests/gui/s1_smoke/run.sh` 在 macOS 本地全绿。

### S2 GUI struct 过 ABI(0.5 天)

- 产出:`bind/raylib.ct` 增 `#[repr(c)] struct Color { r,g,b,a: U8 }` 与
  `Vector2 { x,y: F32 }`(F32 域已落地);main 改直调 `ClearBackground/DrawTextV`;
- 验证:c_src 回读 helper(`gui_probe_color()` 返回 C 侧收到的 Color 分量)断言逐字节;
- DoD:repr(c) 按值传参在 GUI struct 上实证(§3.3-1 的 GUI 定点)。

### S3 Clay 布局桥 + 唯一绘制口(2–3 天)

- 产出:`c_src/ctron_gui.c`(shim 扩展)——**Clay 大 struct 留在 C 侧**,对 Ctron 只暴露
  标量窄接口:`gui_begin(w,h) / gui_open(id) / gui_cfg(direction,gap,padding,…)
  / gui_text(s,size,color…) / gui_close() / gui_end() → 命令数`;measure 桥
  (MeasureTextEx);`ctron_gui_flush`(Clay 命令 → raylib,§11.2 唯一绘制口);
- 验证:固定三元素树的 Clay 命令总数读回断言;肉眼首像素(自动布局无回归基线前,
  以命令缓冲快照为黄金——§11.7 F2 决议,先落 `tests/gui/s3_golden/` 快照 v1);
- DoD:窗口渲染出 Clay 布局的文本+矩形;命令数断言绿。

### S4 事件与测试注入(1 天)

- 产出:§12.3b 翻译表最小集(键/鼠/滚轮边沿);shim 增**仅测试编译**的
  `gui_inject_key/inject_click`;model 可变写 → 失效标志;
- 验证:注入空格 → model 翻转 → shim 读回当前颜色变化,断言绿;
- DoD:交互状态机(无 UI 声明)闭环可自动化。

### S5 `.ctml` 运行时解析器(2–3 天)

- 产出:`std/gui/parse.ct`(Ctron 写,read_file 模式——自举模块同款):最小语法面 =
  `view` + `vbox/hbox/label/button` + `style` 五属性(direction/gap/padding/bg/font-size)
  + 令牌引用 + 颜色串校验;驱动 S4 状态机;
- 验证:骨架 IR 文本 dump 黄金差分(`tests/gui/s5_golden/`)+ 渲染冒烟;
- DoD:改 `.ctml` 文本,窗口内容随之变(**此时是重启生效,非热重载**)。
- 注记:本步为**运行时解析**,与 spec 的 comptime 折叠共用同一 gui_parse 模块;
  解释口径先行,编译口径在第二波。

### S6 绑定 + 事件竖切(M1 语义,2 天)

- 产出:`{expr}` 文本绑定 + `on:click` + 脏槽失效 → 重放;迷你 demo
  (计数器 + 中文 label);
- 验证:`gui_inject_click` → `gui_debug_last_text()` 读回绑定文本变化;IR 黄金更新;
- DoD:**规范 §10 演绎的迷你版跑通**——`.ctml` + model + 事件,零编译器改动。

## 第二波(编译器集成,S6 后排期)

> **W1 施工预案(2026-09-16 细化,FFI 泳道落库即可开工)**:
> 1. 新增 `compiler/src/gui_parse.ct`——块扫描器:在 token 流上做平衡大括号扫描
>    (词法已把字符串收为单 token,**token 级平衡扫描对 `{count}` 类文本安全**,
>    无需字符串感知),产出 view/style 块的 token 区间表;
> 2. `compiler/src/parse_decl.ct` p_file——顶层 IDENT=="view"/"style" 时移交
>    gui_parse(关键字认领 = 一个字符串分支,**lex.ct 零改动**);
> 3. `compiler/src/driver_check.ct`——`--dump-gui` 调试口径:块区间 → IR dump
>    (黄金差分锚点);sem 12 项对 gui 块直通(W3 才接入 E8xxx);
> 4. 验收:`check` 口径自编译不崩 + dump 黄金 + 既有 63 suite 全绿。
> ⚠ 重叠预警:parse_decl.ct 当前被 FFI 泳道工作区占用(M)——W1 待其落库后开工。

- W1 语言关键字认领:`view`/`style` 块边界进解析器(§11.1 C2 裁决),块内容移交 gui 前端;
- W2 `gui_lower` comptime 折叠:令牌求值/extends 展开/多类合并/颜色格式校验;
- W3 E8xxx 检查面进 `ctc check`(负例语料 `tests/09_gui/`,逐字诊断差分);
- W4 热重载(解释口径原址替换)+ 原生口径 snapshot/restore(裁决 1A);
- W5 CI:命令缓冲快照回归进 make test;Linux xvfb 渲染冒烟。

## 交付物清单(文件级)

```
vendor/gui/{raylib/(src+LICENSE), clay/(clay.h), build.sh, VENDORED.md(版本/来源/许可登记)}
std/gui/{bind/raylib.ct, parse.ct(S5), theme.ct(默认令牌,随 S5)}
tests/gui/{s1_smoke/, s2_ab/, s3_clay/, s4_events/, s5_golden/, s6_demo/, run.sh}
```

## 风险与对策

| 风险 | 对策 |
|---|---|
| raylib vendored 体积(源 13MB) | 精简编译子集(rcore/rglfw/rshapes/rtext/rtextures/utils,砍 models/audio);构建产物不进 git |
| Clay pre-1.0 API 破坏 | 版本钉 v0.14(§12.4);升级走黄金快照回归 |
| CI 无显示环境 | Linux xvfb;或降级为构建+链接+命令数断言 |
| FFI 批次未落库(S0 阻塞) | S1–S6 全部依赖 extern/Str/repr(c)——S0 是硬前置 |
| `Ctron.ctcl` 格式仍在演进(并行 P2-B) | 夹具随泳道格式走,GUI 夹具最小化依赖配置面 |

## 本会话已执行(2026-09-16,S1–S3 完成)

- [x] vendor/gui 置 raylib 5.5 src + clay v0.14 clay.h(Release tarball 拉取,许可齐)
- [x] vendor/gui/build.sh:raylib 精简子集静态库(1.38MB)——两处真实修复:GLFW include
      路径;rglfw.c 在 macOS 须按 `-x objective-c` 编译(捆绑 GLFW 的 Cocoa 源为 .m)
- [x] std/gui/bind/raylib.ct:S1 七函数 + S2 增 Color/Vector2 repr(c) 与
      ClearBackground/DrawTextV
- [x] **S1** tests/gui/s1_smoke:emit → 链接 raylib + macOS 框架全绿
      (开窗交互验收留 `run.sh --run` 手动)
- [x] **S2** tests/gui/s2_ab:repr(c) GUI struct 过 ABI 定点全绿——
      Color(4×U8)/Vector2(2×F32)按值双向往返(§3.3-1 GUI 定点兑现)
- [x] **S3** tests/gui/s3_clay:Clay 布局桥 headless 全绿——Ctron 驱动 shim 窄接口 →
      Clay 全量重放 → 命令缓冲读回断言;几何确定性实证:文本 (16,16)、
      固定块 (16,54) 100×40(测高 = 字号×1.25 启发式推算逐位命中)
- [x] **S4** tests/gui/s4_events:事件注入 + 交互状态机 headless 全绿——
      shim 测试缝(`gui_inject_key/click` + 事件队列,raylib 轮询合并惰性待窗口)+
      Ctron 侧边沿分发/hit-test(§11.3 命中归运行时)/model 可变写 → 重布局 →
      颜色与计数读回;正例(键翻转/块内点)与负例(块外点不命中)双覆盖
- [x] **S5** tests/gui/s5_golden:.ctml 运行时解析器(Ctron 写,~300 行)全绿——
      最小语法面(view + vbox/hbox + label/button + class + style 五属性 +
      颜色串 #RRGGBB 校验 + 令牌查找 + 限深 1)→ IR 黄金差分 + 同源数据驱动
      Clay 布局几何断言(中文按字节计数:label 16B、按钮 6B;按钮 bg #c83c3c 读回)
- [x] **S6** tests/gui/s6_demo:绑定竖切全绿——`.ctml` 声明结构与绑定
      (`on:click={inc}` + `{count}`),脚本注入 10 次点击 → 注册分发 → model 可变写 →
      重布局,每轮逐字节断言 label 文本("count: 0" → "count: 10");
      §10 演绎迷你版达成,**MVP 阶梯 S1–S6 全部完成**
- [x] **S7** tests/gui/s7_window:窗口交互 demo 构建链接全绿——真实鼠标点击
      (shim poll 的 raylib 合并路径生效)+ 事件分发 + 绑定求值 + flush 渲染,
      60fps 主循环;`run.sh --run` 启动(手动交互验收);按钮文本暂 ASCII
      (默认字体无 CJK 字形,中文渲染随 M3 FreeType)
- [x] std/gui 提升:parse.ct(canonical 解析器,445 行)+ theme.ct(默认令牌,§13.3);
      包级 use 集成随 P2-B(夹具暂内嵌快照)
- [x] 全阶梯回归:S1–S7 七夹具连续全绿(2026-09-16)

### S0 状态(2026-09-17 销账)

FFI 泳道已自行落库并扩张到位:**tests/ffi 20 过 / 0 败**(11 → 20 夹具:新增
variadic/export/dyn_link/layout/opt_return/cb_panic/link_math 等),S0 无需代提交。
⚠ 新在飞批次(诊断 i18n/错误码审计,2026-09-17 spec)正占用编译器前端
(lex/parse_decl/parse_pkg + 全部 sem_*),**W1 仍待其落库**;
GUI 阶梯 S1–S7 对最新发射器回归 7/7 绿(兼容性追踪 ✓)。

### S6 实现发现(登记)

1. **元素编号约定**:root=0、子元素自 1 计——事件表 ev_el 按此编号,分发过滤必须
   对齐(踩过:过滤写死 1 命中 label);一般化 cmd→el 映射随第二波运行时;
2. **叶子文本 trim 规则**:`{` 前的尾随空格是语义字符("count: {count}" 的空格),
   有绑定时前缀只去左空白;
3. 防御性口径:`List` 元素上直接链方法(`syms[i].to_string()`)建议经局部变量中转
   (本步未定罪但拍平后全绿);嵌套 if 改写 while 体 Bool 拍平为单层 `&&`。

### S5 实现发现(登记)

1. **发射二进制 read_file 锚机制**:`run <file>` 会用 `<file>` 内容顶替程序首个
   read_file 字面量的读取——S5 程序须以默认烘焙锚 + 固定工作目录方式运行
   (run.sh 已 `cd` 夹具目录);多文件资源读取待 P2-B 包资源或 argv 口径;
2. **Clay 文本默认按词换行**:容器窄于文本宽即分行(S5 调试实证:
   "ctron gui 界面" 在 100 宽盒中拆 3 行文本命令)——label 类组件应宽 grow;
3. 解析器原语全部使用已验证发射能力:`src.len`/`byte_at`/`byte_slice(src,s,e)` 右开/
   Str `+`/`==`/List 索引 `[]`/`Atomic[I32](0)` 游标/read_file——零新语言面。

### 实证发现的发射面缺口(转交 FFI/P0-G 泳道)

1. **字符串插值含函数调用**:emit 生成坏 C(`ct_expr:interp 类型` 源码泄漏进输出),
   解释口径正常——插值发射面需补齐或钉死为"仅简单表达式";
2. **裸 F32 参数/返回**:extern F32 标量参数到 C 侧为 0、返回值位型直通
   (200.0f 被读作 int64 1128792064);s2 证明 struct 内 F32 字段正常,缺口仅裸标量。
   GUI shim 在修复前以 **I32 边界**运行(几何 ×100 / 尺寸整数像素),接口形态不变。

### S3 实现注记(对规范的反哺)

- Clay v0.14 API 与文档差异:`sizing` 已并入 `Clay_LayoutConfig`;
  `Clay_Padding` 字段序 = {left, right, top, bottom}(位置初始化踩坑,top 落 0)——
  shim 按钉死版本适配并留注释(§12.4 发射面唯一真源规则);
- shim 窄接口 `gui_cfg`(14 参)为 S3 最小面,S4 起按控件语义拆分;
- 下一:S4 事件 + 测试注入(依赖 shim 增 `gui_inject_*` 测试缝)。
