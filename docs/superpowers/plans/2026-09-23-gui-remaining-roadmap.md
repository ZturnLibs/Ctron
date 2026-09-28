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
→逐行删)可用。**checkbox 勾选行已销账(21508a3+21508a3 后续,dones=List[Str] "0"/"1"
惯例;List[Bool] 索引读比较 strcmp SEGV 家族第三形态入册)。**
**跨文件 view(P2)今日攻而未克,发现全数入册**:①GuiBlock 合并恒随
(非 7-kind 任何 use 即随行,视图合并零成本);②相对 use 形态=项目根相对
(use src.todo.{...},src 段=入口子目录;单段 use todo 解析为 dir+".ct"
即断);③跨包业务 fn 须 pub+显式请求;④Box 跨包 struct 载荷=emit 限值
(两步注解绕行);⑤desugar 后置段的合并 AST 访问面有多处未护(stub struct
字段表已护,emit 期 "]extern" 字节串当指针的下一层待查)——续接按此五条
推进,机械(生成器/收集器)全数在库可复用。
**两阶段重构(pre-改写/post-生成)已试并回退(0926)**:同文件 todo_v10 绿,
但 s41(checkbox 行)native 运行期 SEGV+chk E2020(rt_run_anchor/ev_suffix_i
miss)+ctron-cc interp SEGV 叠加——注入 use 触发 gui.ct 全量合并,与改写后
AST、P1b refs walk(ast_shape 未注册 GuiBlock→保守整模块)、interp 面存在
深层交互。回退=gui_parse/parse_pkg/todo_v10 至 21508a3 态,基线 52/0 回稳,
decl 锁回 394。**P2 跨文件 desugar 已落(70fcda7)**:marker/ordn 走查族+两阶段
premerge/postmerge+field_tables 守卫+family rew 去重;跨文件 emit+cc+
link+run 全通;todo_v10 单文件面全绿。**跨文件拆分夹具(todo.ct+main.ct)
待下次复原挂阶梯验证。**
**余 ③钩子退役已完成(5b5e910)**。SL-8c 全片收官。
**emit 面 premerge SEGV 专项销账(0927 收官,推翻 0927 登记结论)**:s41
SEGV 根因非「合并面类型安全缺失」,而是两个确定性 bug 叠加+两个接线缺口,
lldb -O0 -g 逐帧取证定案:
①**made[0] 头串拼入(SEGV 直接根因)**——gui_ds_gen 返回 p_file 产物
(File 节点,[0]="File" 头串),gui_ds_postmerge 拼接循环 k 从 0 起,
把头串当 decl 注入合并产物;trans/emit 遍历族 d[0] 解引用即崩
(此前 gui_blocks_src/ct_find_main_anchor/ct_swap_anchor 三处逐函数守卫
拦的正是它,P1b stub 定性为误判)。修=拼接 k 从 1 起(gui_parse.ct)。
②**sanitize 白名单缺 FnPub(类型塌陷根因)**——gui_ds_sanitize 只留
Fn/FnExt/Struct/Enum/GuiBlock/Use/Static/Const,合并面 pub fn(rt_bind_items
等)整类剥除→trans 期 ct_fn_ret 查无 decl 塌 "i"→ct_expr:index panic。
修=白名单补全顶层 decl 全 kind(FnPub/FnC/Method/Trait/Impl/Test)。
③**premerge 注入落地**——合成三 fn 引 rt_run_anchor/ev_suffix_i,合并
闭包(pkg_refs_walk)看不到 postmerge 才生成的引用,选择性合并剔除即
C 级 undeclared;gui_ds_premerge 现 view 命中即注入 use gui.{rt_run_anchor,
ev_suffix_i} Use 节点(顶层 use 走 redo 补并路径,dup 处静默跳过)。
④**闭包命名跨文件撞号**——ct_clo_L<行号> 跨文件合并后 todo.ct/main.ct
同行号撞 C 符号;修=ct_fn/ct_drop_fn 经 env 绑 #clofn,命名改
ct_clo_<fn>_L<行>(合并面 fn 名 E5030 保证唯一,跨 pass 稳定)。
**P2 跨文件 desugar 接线复通**:premerge 单文件 find 失败即走
gui_ds_marker_file(纯标记检测,不依赖本文件 views)+gui_ds_ordn_file
(调用点命名实参;70fcda7 机械全在库,此为接线);sanitize 仅 run(ViewCall)
装配程序触发,web/服务器泳道不受影响。**s48_crossfile_d 拆分夹具
(todo.ct 视图+业务全 pub / main.ct use 请求+直驱+run)落库挂阶梯**,
键入/添加/删末/空态翻转全交互绿。
**基线(0927)**:阶梯 54/54(新增 s48);smoke 151/3(3=对端 log/ndjson/pb
在册不变);calc rc=0;decl 锁净增 0。「runtime 类型安全改造」专项按本
定性降级为备查:若合并面再现串流,先查拼接起点与 sanitize 白名单两处。
**组件跨文件分发落库(0928,s49_xfilecomp;波次四 ViewCall × 视图导入
解锁)**:s48 视图导入落地后组件实例化跨文件仅差三件——①**靶视图居末**
(gt_parse 应用根=内嵌源末声明视图;跨文件合并序靶在前,不挪则根选中组件
渲染空树;gui_ds_postmerge 挪靶 GuiBlock 至文件末,单文件靶本就居末
no-op);②**sk_node ViewCall 实例分支**(desugar 骨架面此前无大写识别,
实例被当普通元素吞掉;现 args 三形解析,表达式值 0x01 连接入 npost);
③**collect 实参抽取+多树走查**(call: 节点 npost 按 0x01 拆为合成 bind
路径——展开期 bxv_ask 以 prop:<表达式> 问装配通道;走查改从全部树根
起步——靶居末后单根 stid=[0] 只走组件树,主文件实参全漏)。**三发现
入册**:①E2020 裸模块 use 不存在——braced 请求须命中真 decl,视图非
7-kind 随模块合并恒随(夹具用 pub fn 作请求锚);②内嵌重建插空格坑
再证("ROOT-HERE"→"ROOT - HERE",断言字面量须无标点);③组件事件跨
文件实参映射契约**未设计**(合成 act 只有根模型捕获,组件 prop 根实参
无映射目标;零参/字面参形态可用但观察面待设计)——登记随组件事件
专项,夹具锁定显示面(Str/I32 带型解标)。阶梯 55 夹具。
**内容投影落库(0928,s50_dialogslot;「dialog slot」销账)**:ViewCall 实例
支持非自闭合携带子树,组件体 `<slot/>` 锚位拼接。机制:①gt_node 实例分支
分自闭合/投影两形——投影形子树链接到实例节点(nfc/ns,宿主上下文解析);
②slot 入元素白名单(gt 面挂自闭合空元素分支,sk 面 ck 白名单+大写实例
认领);③展开期实例垫 env "__slot__"(末位优先,嵌套组件内层覆盖外层
免弹栈;无子树垫 -1=空投影),rt_emit slot 分支读 env 拼接子树链;
④sk_node 镜像(投影子树入骨架,collect 多树走查即覆盖——slot 内容
bind 路径/事件全按主文件语义合成)。宿主语义三钉入夹具:slot 路径经
prop: 通道应答/slot 内事件合成根模型捕获分支/组件 when 门包 slot 关态
零渲染。**边界在册**:v1 组件 prop 与宿主路径根同名时组件 env 遮蔽;
check 面对实例仅认领不深查(E81xx 完整契约随组件检查专项);gui_sk_load
编译骨架面无 vreg,投影仅 gt_parse 形态(既有登记顺延)。阶梯 56 夹具。
**二次攻坚新实锤(0926 深夜)**:①**trans 缺 NParg 发射分支**——未改写的
ViewCall 到 trans 即 "ct_expr:NParg@行号" 硬 panic(ct_expr 无 NParg case),
interp eval 同面待查——此为 P2 的 trans 侧确定性缺口(修=trans_expr/eval_expr
补 NParg→值表达式透传,3 行级);②emit 驱动的 premerge 标记/收集走查在
该 fixture 上先行崩溃(fs_write 探针未达)——emit 口的走查崩溃与 chk 口
不一致,双口径分叉再证;③跨文件夹具三形态全试(use src.todo 相对形态
+pub 面),sem/merge 通,堵在 trans。**续接序落地状态**:①NParg trans/eval 补口 ✅(0022a47+本片);②emit 口
lldb 专项——根因=desugar premerge 的 interp 面崩溃(双口径分叉),两阶段
统一后 s48 域包面夹具 rc=0 ✅;③合并时序 co-design——**跨文件 loader
限制**为根本阻塞(同目录兄弟文件 import 不支持,仅域根/STDPATH/ctart
三路径;项目根相对 src.todo 形态可达但需 parse_pkg 面对齐)。
**SL-8c 全片收官+T1 销账+P2 desugar 全链入库。余=loader 兄弟文件 import
(P2 前置)+todo_v10 拆分夹具复原(依赖前置)+多级 props(随终锚)。**
**③ 已落(5b5e910)**:钩子退役——run/run_kb/run_d/run_kb_d 降内部
(rt_run_src/rt_run_kb_src/rt_run_anchor/rt_run_kb_anchor);合成面
(run(ViewCall))即唯一文档化用户入口;消费面 9 文件机械迁移。
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
> 泳道状态(0926):自动装配正统已裁归 desugar(简报 plans/2026-09-26-gui-
> assembly-arbitration.md;emit 合成归档分支 sl8c4b 可复活);read_file NULL
> 修复与 panic/assert stderr 可见性已入 main(688f65e/ef2dffe,阶梯 52/0)。
> 本泳道小件队列清空,下一片 = P-M3 专项会话(设计尖刺先行)。

### P-M4 远期
`ctron build --release`(<2MB 静态单二进制)、多窗口、动画 tween、
无障碍树、CTML→web 档(§11.8 登记)。

## 明确不做(维持登记)
E8170 子回写(组件模型落地后才非空)、求值缓存/脏追踪(D5 裁决每帧全量)、
字面量叶标点根治(骨架 IR 槽表直通,远期)。
