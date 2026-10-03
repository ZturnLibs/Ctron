# GUI-27 组件跨文件 view 导入——执行计划(2026-10-04)

> E 档最后一项。规格依据 = docs/superpowers/specs/2026-09-16-gui-ctml-design.md
> §4.3/§4.3.1:`pub view` 导出 + `use card.Card` 导入,view 默认文件私有,
> 未导入未知标签 = E8100。现状:组件只能与实例同文件声明(s49 已盖内嵌 .ct
> 跨**模块**合并面,本件补独立 .ctml 跨**文件**导入面,互不重叠)。

## 消费面现状(实测核对)

- 真应用锚路径 = 运行期 `read_file(ctron_gui_entry())` → `gt_parse(src)` →
  vreg 注册表展开 `call:` 实例(热重载 60 帧内容比对)。单文件载入 = 卡点①。
- 检查面 = ctron-chk `gui_is_ctml` 支路 `gui_ctml_file(s)` 单文件 →
  `gui_check_file`。实例 props 深查按 vr_nm/vr_pp 平行表;view 不在表内 =
  逐 prop E8110 噪声(无 prop 则静默)= 卡点②。
- 骨架面 = `gui_sk_build(合并后整 file)` 仅内嵌 GuiBlock,独立 ctml 不入骨架
  ——本件零波及。发射面(driver_emit)零改动,与发射泳道在飞三文件
  (driver_emit/trans_conc/trans_expr)无交集。

## 方案(双口径同语义,合并算法两宿各一份)

**导入形态(v1)**:ctml 顶层行式 `use mod.Name` / `use mod.{A, B}`;
`pub view X` 导出。mod = 首段 = 同目录 `<mod>.ctml`;名称逐一校验 pub。

**运行时合并(pkgs/gui/gui.ct)**:`gt_anchor_src(anchor)` = DFS
imports-先-自身-后(应用根=最后声明视图契约保持),文本级拼接(运行时
gt_parse 无行:列,零位置顾虑);路径集环守卫;缺文件跳过(检查面管红)。
gt_parse 顶层学 `use` 行跳读(至 EOL)与 `pub` 词吞并。热重载探针
`read_file(reload)` → `gt_anchor_src(reload)`:无 use 行文件字节恒等
(w4_hotreload/s26 回归保障),导入族变更可见。

**检查面合并(compiler/src/gui_parse.ct)**:`gui_ctml_file_use(path,…)`
node 级合并——GuiBlock 追加槽 d[7]=fileid/d[8]=pub("1"/"");**旧节点无槽 =
通配可见**(内嵌形态与既有单文件路径零波及)。use 行校验:模块缺文件
E8100「use 目标不存在」;名称非 pub/不存在 E8100「use 名未导出」。

**可见性门(gui_check_file × gui_ck_elem)**:vr_nm 平行扩 vr_file/vr_pub;
实例支先过 `gui_ck_view_vis`:本文件全可见 ∪ 导入闭包 pub;非独立形态下
未知标签 = E8100「未声明组件」、不可见 = E8100「未导入组件」(门过才进
props 深查,消旧噪声)。

## 夹具 s78_xfileview(阶梯 83)

- components.ctml:`pub view Badge`(按钮+文本,含事件)+ 私有 `view Chip`
  (Badge 内部消费)+ `pub view Row`(投影 slot + 内部再实例 Badge = 传递面)。
- app.ctml:`use components.{Badge, Row}` + Root 实例化两者 + slot 投影。
- 断言:发射原生 headless——导入组件 TEXT 渲染、导入组件事件 fire、slot
  投影保宿主语义;检查面金文件 OK + 三负例(app_bad_priv/app_bad_mod/
  app_bad_label 各 E8100)。
- run.sh 另核热重载合并源:改 components.ctml 后 gt_anchor_src 字节变。

## 验收

阶梯 84 全绿;e8 语料 + w 族(w1/w4_hotreload/w4_ctml_reload/s26_reload_d)
零回归;s22 双端差分不受扰(单文件无 use = 合并恒等)。

## 收官实录(1004)

- 验收:e8 语料 51/51(五新件);阶梯 82 过/1 败——s30_props_d 段错误为
  基线既有红(worktree HEAD 基线与主树双环境复现,归 P9/T31 栈面或对端
  发射在飞件,本泳道移交不在册);suite 100/100 双列;smoke 158 ok/2 fail
  (均基线既有);w4_hotreload/w4_ctml_reload/s26_reload_d 全绿=无 use 行
  字节恒等实证;w1_block 绿=内嵌检查零波及。
- 两存量缺口顺修(独立 .ctml 检查面,本件首曝):①独立词法括号单字符化
  (props 形参表捕获在独立面从未工作);②实例子树走查先吃开尖(投影实例
  带子树级联误报)。均按「内嵌/独立同语法同路径」宪章对齐。
- 发射面(driver_emit/trans_*)零改动——与发射泳道在飞三文件零交集;
  泳道协作:对端中段转向 GUI-30/31 并占 s77_binddef 槽位,本夹具让号为
  s78_xfileview。
- decls 申报:编译面 +8 fn(锁数 472→479,详见 tests/COVERAGE.md 本日记)。
