# SL-6 (L2-1) 实施计划：编译器 GUI 前端对齐 + M1-d 合流

> 2026-09-21。设计依据：specs/2026-09-19-gui-use-gui-design.md §5.1/§5.3/§9 SL-6 行；
> 诊断码规范 = specs/2026-09-16-gui-ctml-design.md §4.4；语法事实源 = std/gui.ct（域包解析器）。
> 基线：430218c..17e0b22（suite 73/73、阶梯 24/24、e8_corpus 5 件）。
> **状态：✅ 全部完成（19b41a3 检查面+语料 / bac800d 独立 .ctml）。e8_corpus 15/15。**

## 已落地（与切片对应）

- SL-6a+6b（19b41a3）：gui_ck_leaf 限深 1 → gui_ck_elem 递归走查（限深 8）；when/each/input
  认领；E8100 props 缺失族/E8110 契约形态/E8120 处理函数声明表/E8193 警告级（gwarns
  通道不置 rc）；dump 同步认领。
- SL-6c（bac800d）：gui_ctml_tokens 标记 tokenizer + gui_ctml_file（复用 gui_block）+
  driver_check 后缀分流；内嵌/独立同径走查；E8120 形态参数（indep 跳过——处理函数在伴生 .ct）。
- SL-6d（两片携带）：e8_corpus 5→15（pos/neg/warn 三类 × 内嵌/独立双形态）；runner 警告类。

## 坑位（已实证，续接必读）

- 编译器源码字符串内开括号须 `\{` 转义（裸 `{` 触发插值扫描 E1001"未终止的插值"，
  拼接产物行号实证）；花括号比较一律 byte_at 字节码（文件头契约注释的真义）。
- w2_fold 黄金差分对 E8120 敏感：夹具 on: 处理函数须在文件内声明（黄金 decls 4→5 已同步）。
- E8120 形态语义：内嵌 .ct 恒核对；独立 .ctml 跳过（fns 表语义上不可能有）——勿用
  fns.len>0 判 form（把内嵌无 fn 声明的文件也放行了，e8120 语料实证）。

## SL-7 入口设计笔记（SL-7α 已落地 87e7fde）

### SL-7α（2026-09-21，内嵌 view/style 块端到端）

- ctron_embedded() 内建四面（eval/sem/trans/driver_emit 烘焙）；gui_blocks_src 重建器
  （空格化 + `</` 粘连 + 剥 ~）；run_d/run_kb_d 文件优先+内嵌兜底；热重载空源守卫；
  s20_embed 落阶（无 app.ctml 独立运行）。
- **已知限制**：字面量叶文本含词法切分标点时重建插空格（W1 join 丢邻接，信息论极限）——
  骨架 IR 槽表直通根治；夹具字面量文本用无标点形态。
- **新登记红项**：域包 test() 驱动解释口径整体红（"index target"，⑳ 家族 struct-List
  读残留；read_file 源同炸）——归解释/发射泳道或后续 ⑳ 收口切片。
- **坑位**：peer stash-pop 冲突（UU trans_expr）按上游侧解决零损失；`git add` 路径外的
  peer 暂存态会被 commit 卷入——**提交前必查 `git status` 暂存列，commit 后必对
  `--stat` 核对文件清单**（本次 21 files 误卷已 soft-reset 拆分重提为 10 files）。
- w2_fold 红 = peer 未提交锚重构（`../selfhosted/input_cc.ct` → `ANCHORINPUT`）打断
  其 sed 假设——归 peer 随其重构收口。

### SL-7 渲染安全网已就位（2208e0f）

- **s21_frame_golden**：合成应用（内嵌源+字面量 label/button+when+each+input+三组样式）
  整帧命令缓冲逐条倾倒（type/box×100/text）黄金差分——骨架换源/绑定/热重载动渲染面前
  必跑。阶梯 26/26。
- gui_blocks_src 第 4 处域包词法适配：`={` 粘连（attr 值花括号探测裸字节）。
  坑位三犯后定则：**花括号一律字节码，含前瞻比较 tks[k+1]=="{\""' 形态**。
- std/gui 清 WHEN-VISIBLE 遗留调试 println（每帧污染 stdout）；d_cmd_x/y/w/h100 几何
  访问器补齐。w2_fold sed 更新 ANCHORINPUT（peer 锚重构收尾顺手修）。

### SL-7β（骨架 IR 本体·下一片开篇，未动工）

- **目标**：编译期解析 GuiBlock → GuiTree 15 字段平行表常量；运行时不再 gt_parse。
- **GuiTree 字段清单**（std/gui.ct:316，镜像源）：ntag/nflag/ncls/npre/nbid/npost
  (List[Str]) + nfc/ns/nes/nec/btns (List[I32]) + ev_name/ev_fn/sk/sv (List[Str])。
- **gt_parse 语义盘点**（移植须逐条镜像）：
  ①节点 id = push 序；②when 子树平铺——子元素提升为 when 的兄弟、各自 nflag=cond 名
  （rt_emit 按节点旗自查）；③each 节点 nbid=itemvar、npre=列表名，孩子=模板子树
  （fc 链挂 each 节点）；④叶文本 pre/bind/post 按 {ident} 切分（重建源的 ={ 粘连形态
  词法兼容）；⑤事件槽 nes/nec = 元素在 ev_name/ev_fn 的 [起,计) 切片；⑥button id
  注册 btns；⑦style 块 → sk.push(名.属性)/sv.push(值)。
- **切片序**：β1 ✅ 落库（a862e5b，树等价差分绿）；β2 ⛔ 已探明 ABI 阻塞（见下）；
  β3 cc_emit `static const GuiNode gui_sk_N[]` C 构造（发射口径换源）。

### β3 ✅ 落库（263cbf6，2026-09-21 深夜）

- peer lex/rt WIP 落库后发射链恢复；复验时真凶现形：**gui_sk_word 未剥 W1 行尾 ~，
  gs2i('8~')=158 炸布局**（cmd 切片=浮点 → 命令错位 → 样式值 ~，定位链三步）。
- gui_sk_word 统一剥尾 ~（骨架表净值化）；gui_sk_emit_str/int_arr 自包含转义
  （cc_run 拼接无 trans_ty 的 ct_cstr——驱动文件集差异坑）。
- s23_sk_native 落阶：骨架树渲染断言全绿（7 命令 2 RECT = s21 黄金同构）。
  w2_fold 黄金 sk 段净值同步。**SL-7 发射口径管线闭环**（α 源烘焙 → β1 等价 →
  β3 静态构造零运行时解析）。
- 残余：β2 解释口径（ABI 评审三路径待裁决，eval 侧现干净 panic）；~
  标记与字面量文本的歧义（骨架表净值已消，W1 dump 面保留）。

### β3 代码就位待验证（2026-09-21 深夜，已落库 supersede）

- **已写**：driver_emit β3 块（has_gui 探测 → gui_sk_build → 15 张 `static const` 数组
  + gui_sk_ls/gui_sk_li 助手 + gui_sk_load() 组装 t_GuiTree；插在前向声明循环后——
  typedef 已发射、先于用户函数体）+ trans 分发/sem 白名单/eval 干净 panic 兜底。
  gui_parse 增 gui_sk_emit_str_arr/gui_sk_emit_int_arr 发射辅助。
- **验证被阻塞**：peer 在 compiler-c(rt_core/arena/fmt) + compiler/src/lex.ct 的在制品
  编译进 ctron-emit → **任何 GuiBlock 程序发射中途 SEGV**（s20/s21 红；strlen(NULL)
  @ lldb；无 β3 对照同崩，非本片代码问题）。suite/e8（seed 路径）不受影响仍绿。
- **续接动作**：peer WIP 落库后 → 重建 → s23_sk_native 跑通即落库（夹具已写好）。
  教训：编译器源码调试期，先用最小 GuiBlock 输入 + ctron-chk run --dump-gui 分离
  「builder 编译态」与「发射链健康度」；工作树对照实验必须仓库根 + 双态各重建。

### β2 主体落地（da8cb00，2026-09-21 深夜）+ 组合渲染分歧登记

- **gui_sk_load 解释口径打通**：vL/vS/vI 节点包裹装配（gui_sk_wrap_s/i），与 gt_parse
  解释执行值形态同构——裸值直塞的 SEGV 根除。单构造（label/button/when/each/input）
  解释口径渲染各自全绿；双口径树同构（interpret/native sk_dump 逐字节一致）。
- **已登记分歧（β2 余项）→ 根因实锤（args11.ct 最小复现）**：解释口径 extern 桥
  **11 参调用编组错位**——gui_cfg(1..11) 落位 hval=0 bg=10（参 10 归零、参 11 参 10）。
  组合渲染的全部症状（容器 gui_cfg hval=0/bg 错 → RECT 异常；部分文本丢失）皆此根因。
  C 分发器 switch(k) 逐位核对无误——错位在 eval 侧帧填充/参计数的解释执行路径，
  归解释运行时泳道（编译口径无此问题，s23 已证）。
- **新增调试设施**：shim CTRON_GUI_TRACE=1（open/cfg/text/close 序列 stderr 直出，
  双口径共用）+ driver_emit DISPATCH n/k 与 FR[i] 帧串转储（同 env 门控）——常态零成本。
- **帧级实证（FR 转储）**：gui_cfg(1..11) 的帧 = [i:1..i:9, i:0, i:10]（k=11）——
  **参 10 位被 "i:0" 占据、参 11 丢失**——错位在 eval 侧参求值/帧填充（11 字面量
  求值链），不在 C 分发器。续接：eval_call extern 分支 vals 编码处单步。
- **环境警告**：peer 并发 build 竞态使 build 行数波动（cc_emit 10800↔19598）、
  二进制版本翻转——回归前必须静默重跑 build 至计数稳定（连三次一致）。

### β2 ABI 阻塞登记（2026-09-21 实证，lldb 定位；已被节点包裹方案根除）

- **现象**：eval 内建 gui_sk_load 构造 U 值（ GuiTree 同形 list）→ 域包 sk_dump/test_sk
  消费 → 编译口径（ctron-cc）SEGV（gui_sk_intlist 读 0xffffffffffffffff）。
- **根因**：编译后的编译器里 struct 声明 = 真实 C 结构体（t_GuiTree），而 U 列表是
  解释值世界对象；`pub fn sk_dump(t: GuiTree)` 编译期即按 C 结构体 ABI 收参——U 列表
  指针传入即字段读越界。值模型（节点）与结构体 ABI 两种表示在编译口径不互认。
- **定性**：与 SL-0.6 登记的「fn 返回型编码需跨宿主 ABI 评审」同类——**eval 值 ⇄
  原生结构体的边界跨越需要 ABI 评审裁决**，非 gui 单泳道可闭。
- **可行路径（待评审裁决后择一）**：
  ①gui_sk_load 返回骨架的「编码串」形态（如字段拼接文本），域包侧解码建树——
  零 ABI 依赖但多一跳解析（本质退回 SL-7α 源烘焙，只省词法）；
  ②编译器内建直接返回「逐字段 getter 族」（gui_sk_ntag(i) 等标量/串内建），
  域包 run_sk 逐字段重组 GuiTree——零 ABI 跨越，15×N 次调用开销但换源语义完整；
  ③β3 直接跳到 cc_emit C 静态构造（发射口径一步到位，原生 t_GuiTree 构造
  是同 ABI 世界，反而无此阻塞），解释口径暂留 SL-7α 源烘焙。
  **初步倾向 ③+②**：β3 先做（同 ABI 世界无阻塞、s21 黄金锁正确性），②作为
  解释口径补齐随行。
- 域包已备 test_sk 树入口（与 test 同口径，收 GuiTree；β3 落地即用）。

- 骨架 IR：GuiNode[tag, style_id, child_fc, child_ns, ev_slot] + Slot[node, attr, expr_id]。
  fc/ns 与域包 GuiTree 同表示——**runtime.ct 遍历零改动换数据源**（§5.1）。
- 三产物分工：cc_check 已就位（本片）；cc_emit = `static const GuiNode gui_sk_N[]` +
  样式表常量；cc_run = 解释口径从骨架建 GuiTree（替换 gt_parse 数据源）。
- **前置协调面**：GuiNode C 布局落 shim（std/gui/c_src）属域库单一真源变更（§4.5 "L1
  不动,L2 收缩"），与发射泳道共辆——动工前登记 roadmap。
- 样式表常量化前置：gui_lower 需多类合并/令牌求值（现 gui_cls_prop 运行时逐查）。
- 验收锚：§10.2 静态 Todo 形态可跑 + 渲染黄金帧差分绿（tests/gui 阶梯收编）。

## 语法事实（照抄域包解析器，不凭规范想象）

- 标签集（rt_emit 认领面）：`vbox hbox label button when each input`
- `<when cond={名}>…</when>`：包装元素，cond 必须花括号绑定名（SL-5 通道）
- `<each item in={列表名}>…</each>`：**itemvar 是位置头**（先于 attrs 解析，非属性）
- `<input bind={名} placeholder="…"/>`：**自闭合**（`/` 由调用方消费）
- 属性值两形态：`{单词}`（绑定名）与 `"字符串"`；`on:名={函数名}`
- 叶文本 `{ident}`：插值绑定标记

## 切片

### SL-6a+6b：检查面/LOWER 认领 + 语法可达诊断（一氏重写走查）

1. gui_ck_tag 白名单对齐域包（+when/each/input）
2. gui_ck_leaf → **gui_ck_elem 递归走查**（限深 8 守卫）：包装元素子树、自闭合、
   花括号属性值、each 位置头（裸词仅 each 头合法，余者 E8100）
3. 语法可达诊断（本片注册）：
   - E8100 props 缺失：input 无 bind / when 无 cond / each 无 in
   - E8110 属性契约：cond/bind 值非 `{名}` 形态（给字符串 = 绑定契约不符）
   - E8120 事件处理器未声明：on: 的 fn 名不在本文件 Fn/FnExt 声明表（签名级=后续 sem 联动）
   - E8193 警告级：each 体含 input 型控件（key 原语 L1 未有，索引对齐即串位风险）
     ——**警告入独立 gwarns 通道，不置 rc**（规范“警告级可关”）
4. gui_lower_element dump 同步认领（dump 与 check 同构走查）
5. driver_check：gwarns 打印不置错（--dump-gui 旗下，不动默认 check 路径）

### SL-6c：独立 .ctml 检查入口

- check 驱动对 `.ctml` 后缀分流：标记 tokenizer（<>=:{}"/ 字符类 + 词 + 串）→
  平衡扫描包成 GuiBlock File 节（复用 [1]kind [2]name [3]ntoks [4]joined 契约）→
  gui_check_file/gui_dump_file 同径。内嵌/独立同语法同路径（§5.1 语法缝）。

### SL-6d：E 语料黄金基线扩容 + runner 警告类

- 新语料：when/each/input 合成 pos（回归锁）；E8100×3（props 缺失族）；
  E8110（cond 字符串）；E8120（handler 未声明）；E8193 warn 级
- e8_corpus/run.sh 增 `*.warn.ct` 类：期待 rc=0 且输出含期望码
- 顺带修 runner 与 driver 的 gdiags/gwarns 契约

## 非目标（登记不顺手做）

- E8110 类型门全量 / E8120 签名级 / E8170 子回写 / E8180 key Eq——需 sem 用户类型联动
- E8191/E8192 布局静态分析——需样式解析（gui_lower 样式表常量化，SL-7）
- 骨架 IR 数据结构（GuiNode/Slot）——SL-7 三产物时引入，本片仍走文本 IR dump
- 检查面移入默认 check 路径——等 C 宿主 sem 对 gui_ck 助手 E2020 分歧解决（W3 登记）

## 验收

- e8_corpus 全绿（pos/neg/warn 三类）；阶梯 24/24；suite 73/73
- 独立 .ctml：语料同码双形态（内嵌/独立）各验一组
- 每片全绿即落库（pathspec 限定 compiler/src/gui_parse.ct + driver_check.ct + tests/gui/e8_corpus）
