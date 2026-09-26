# GUI 规范未完成部分路线图(2026-09-23)

> 承接完成度评估,为剩余面定序。原则:v10 终锚依赖优先、可立即实施者先行、
> 阻塞面保持登记不空转。

## 状态基线(本日实测)

- 已解锁:s26 自愈(run_kb_d 段错误随 peer 落库消失)、gui_calc 绿、
  trans_expr/stmt/ty 已落库(仅 trans_emit.ct 在飞)。
- 已落:8c-3(09-24,on: 表达式事件两口径——捕获/签名门 E8120/形态与
  实参根门 E8110/ev_fire 触发面/语料 7 件/s22 扩/s29 新夹具/calc 切表达式
  事件;decl 锁 363→366);8c-2(09-24,props 名表双端同形+props 链解析
  (D-8c1:prop: 前缀先问 props 环境、未答回落裸名)+槽求值切换(props 视图
  四站点全走求值器,§7 并存开关)+s22 双轮差分+w2_fold 黄金同步+s30 双阶段
  夹具;decl 锁 366→367)。
- 余:8c-4b(gui.run 单入口+auto-act 装配闭包发射+钩子退役;4a 值传播已落,见下)。
  蓝本:plans/2026-09-23-sl8c-design.md(§3/§4/§7 已兑现)。

## 8c-4a 落库登记(09-25;事件实参值传播,act v2 契约)

- ev_fire 实参求值后传出(弃值退役):act 契约 `fn(Str)` → `fn(Str, List[Str])`
  (打标串,8b 约定);ev_arg_len/s/i/b 四解码器(越界回零宽容契约);gui.ct
  act 面 18 处签名 + 5 调用点(波次一快捷键 payload/指针管线)随片加宽。
- **实参约定裁决**:事件实参 = 事件局部数据,模型经闭包不入实参表(props 根
  实参位求值 = 空串);终形态 `{addn(5)}` + stub 门 + `_impl` 真逻辑。
- **bxv_istag 负数缺陷修复(顺手)**:tagi/iv 已有负号而 istag 未跟,负整数
  打标串全家族误判(显示上屏 "i:-3" 原文/求值回零);istag 补可选 '-',负实参
  判据入 s40。
- 迁移 21 文件 lambda(`|name|`→`|name, args|`,转发目标签名不动);验收
  阶梯 35/6(6 红 = 基线真子集,零新增)+ 五示例绿 + s40 全绿。
- 8c-4b(auto-act/auto-bind 装配闭包发射 + gui.run 单入口 + 钩子退役)以本
  v2 契约为装配目标;eval 侧装配仍随 P1b(interp 口径域包夹具红在册)。

## 8c-2 落库登记(09-24;坑与发现)

- ctron_embedded 重建器(gui_blocks_src)原先丢 d[5]——内嵌兜底源无 props,
  gt_parse 捕获空表,链解析/求值切换静默失效;已修(d[5] 随重建 + 令牌剥 ~)。
- 独立 .ctml 的 props 括号须空格独立:`view V ( m: T ) {`;`V(m:` 粘词
  (indep tokenizer 的 ( 非特殊字节)→ props 不捕获且引用门空转。嵌入式不受累。
- 域包夹具解释执行(ctron-cc run s25/s29/s30)同错 "index out of bounds
  idx=1 base=nsl"——既有缺口归 interp/合并泳道(s25 先于本片已同错);
  w4_interp 的 extern 直调形态不受累。
- trans 既有:闭包捕获 fn 值 native 发射坏(capture 槽按 ct_i 传参)——
  8c-2 链解析因此取树上下文设计(bxv 线程 t,无 fn 捕获),双口径绿。

## 8c-3 落库登记(09-24;绕行与发现,trans 线候选)

- 闭包捕获 List + 下标,在 test script 闭包内触发既有 trans 发射症状
  ("ct_expr:index 目标非 List/数组:i";同文件同形 probe9 复现,与本片无关)。
- 同行双 lambda 撞 ct_clo_L<行> 名(C 重定义)——每 lambda 独占一行绕行。
- Void fn 裸 `return` 发射 `return;` 触非 void 告警——if/else 嵌套绕行;
  「闭包体保持单调用」配方扩:变异提具名 fn。
- 8b 补口:简单名通道补 bxv_show 解标(带型应答 i:/b: 上屏取值,裸串零破坏)。

## 执行队列

### P-W1 checkbox 元素(本日开工;v10 依赖)
`<checkbox checked={done} class="ck" on:click={toggle}/>`:
- 解析:三面白名单 + 自闭合(镜像 input);checked 必填(E8100);登记 btns
  (点击走既有 rt_hit_name→act 名字分发,实例索引缺口维持登记)。
- 渲染:选中 = accent 底 + "x" 文本,未选 = 暗底无文本(ASCII 字形安全)。
- 验收:e8 pos + 原生探针(点击→状态翻转→重渲)。

### P-W2 style extends(§5 StyleExt;本日次件)
`style b extends a { … }`:解析认领 + 样式表构建期单亲合并(子覆盖父);
循环继承 E8100。验收:e8 pos/neg + 现有样式夹具回归。

### P-8c-4(余;按蓝本动工)
gui.run 单入口+钩子退役(run/run_kb 降内部;装配闭包=事件闭包字面量的
发射落点,native 面彼时动 trans;props 环境由 gui.run 生成的解析闭包注入,
当前 prop: 前缀协议为其替身)。8c-2/8c-3 已落(09-24)。
**地基已落(06c1142)**:props 段级类型门+构型字段表(stns/sfk/stv)——合成器
的槽表达式类型化依据就绪;段级门语料 2 件。
**① 已落(ce6ee6d)**:ViewCall 文法面(NParg 命名实参+sem 组件核对)。
**② 已落(1463bca,完成 0ea0513 WIP)**:文本注入式 desugar 双口径端到端
(合成面/直驱面/真窗面三层;s41_run_d);根因修复=出参节点经 List[Str]
通道的静态型谎言(字符串索引码对 list 指针即崩),全 Str 名单+删 locate。
**④ 已落(examples/todo_v10,119e5d1+119e5d1 后续)**:§10.3 合成用户面——
main 一行装配,零手写钩子;headless 直驱合成 fn;on:input 键入持久化
(名通道前缀分支+隐式尾参豁免,119e5d1)与每行实例分发(隐式下标+
ev_suffix_i+d_click_inst,2a982e8)均已销账——Todo 全交互链(键入→添加
→逐行删)可用。**余差集:checkbox 勾选行(T1 里程碑)、跨文件 view 导入
(组件模型 P2)、多级 props(随终锚)。**
**余 ③钩子退役**:rt_run 族降内部待对端 act v2 波次收口后统一更名
(现 pub 保持夹具/示例兼容;合成面已单入口文档化)。
**act v2(07fcf19,对端)**:ev_fire 实参求值传出+ev_arg_* 解码器;窄闭包
ABI 兼容(额外寄存器实参被 callee 忽略),本泳道夹具/生成器零破坏。
**执行序建议(下一专项会话)**:①ViewCall 文法面——`TodoApp(model: make())`
命名实参形态(今日为解析错,放开向后兼容;p_post Args 环节 NParg 尾槽,
sem_calls 认 GuiBlock 视图名校验 props 必填 E8100/未知 prop E8110,语料先行);
②装配合成——文本注入式(p_file 后对 run(ViewCall) 调用点改写+__gui_bind/
__gui_act/__gui_run 三 fn 以源文本生成→scan4+p_file 真解析→拼写进 File,
双口径免费;微型类型推(prop 型字段表+.len/比较/算术)定 i:/b: 标签;钩子点
parse_pkg.pkg_load_use 尾=全驱动单点);③钩子退役(rt_run 族降内部);
④§10.3 Todo 照抄验收(差集:key/checkbox/props 多层随终锚如实登记)。
**注意:driver_emit 对端常驻在飞——钩子点避开,取 pkg_load_use 单点。**

### P-v10 终锚装配(依赖 8c)
§10.3 Todo 照抄能跑:props 视图 + 表达式事件 + `disabled={}`(需按钮
禁用态与 RECT 映射契约的每帧化,或 props 环境直查)+ `key={}`(实例分发,
L1 缺口对偶面)+ checkbox 行级勾选(P-W1 交付)。

### P-M3 中文输入(独立大山头,不阻塞于 8c)
IME 组词 preedit 一等状态(§6.4);平台 shim + 字形栈协作;焦点/IME 不丢
为 §6.3 硬验收(P3 遗留面一并兑现)。

### P-M4 远期
`ctron build --release`(<2MB 静态单二进制)、多窗口、动画 tween、
无障碍树、CTML→web 档(§11.8 登记)。

## 明确不做(维持登记)
E8170 子回写(组件模型落地后才非空)、求值缓存/脏追踪(D5 裁决每帧全量)、
字面量叶标点根治(骨架 IR 槽表直通,远期)。
