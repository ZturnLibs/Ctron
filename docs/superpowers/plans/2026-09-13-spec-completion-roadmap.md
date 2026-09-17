# 语言规范未实现部分补齐路线图(Spec Completion Roadmap)

> **For agentic workers:** 本文档是主路线图。P0 各切片为任务级可执行;P1/P2 为切片概要,
> 执行时按仓库 SOP(HANDOFF 同步 + fx/负例/锚 + 三线一致 + 单独提交)各自产出日期命名的
> 详细计划后再实施。REQUIRED SUB-SKILL: superpowers:executing-plans 或 subagent-driven-development。

**Goal:** 补齐 v0.7 规范已规划但实现面缺失的部分,使三线(自举/C 宿主/R)与 spec 对齐。

**Architecture:** 按依赖与设计风险分四期。每期切片独立可测、独立提交;语言级特性保持
三线一致纪律(自举/C 宿主/R 线);所有新诊断码走「spec §10 登记 + 码表 + fx/负例锚」SOP。

**Tech Stack:** Ctron(自举编译器 compiler/,36 模块)、compiler-c(C 参考宿主)、
compiler-rust(R 线)、smoke/suite/meta_check 三层门禁。

## Global Constraints

- 三线一致:语言语义变更必须自举 + C 宿主 + R 线同步,或明确登记单线口径(如 E2050 先例)。
- 诊断新码:先 spec §10 + compiler/README 码表登记,再实现;负例/夹具锚随片落。
- 门禁底线:smoke --full 全绿 + suite 双侧一致 + meta_check 一致;自举固定点逐字节。
- 规范修订走「先改本文档 + 增补测试锚点,再改实现」(§10 末尾冻结规则)。
- 发射面语义变更必须同步考虑编译器自举传播(发射的检查会进入编译器自身,
  参见 HANDOFF「I32 发射面检查被自举传播阻断」条目)。

---

## 已核实撤案(不列入计划)

- §2.5 coherence(孤儿规则 E5010):**已实现**(sem_comptime.ct:178,双实现差分)。
- §7.3 Atomic API:load/store/fetch_add **已实现**(eval/trans 双面)。
- §3.5/§8.1 trait 对象 `&Trait` 动态分发:**能力对象路径已实现**(07_capabilities.ct,
  `&Clock` 注入 + 动态调用,双实现通过);泛型 bound 的 `&Trait` 泛化待观察,非当前缺口。

---

## P0 立即可做(小切片、无设计依赖)

### 切片 P0-A:struct 泛型注解实例化点 bound 核对(✅ 已完成,0f0957d)

**Spec:** §3.9.2(v0.6 修订遗留挂账,fn 调用点已有 bound_sat,struct 注解点缺)。
**Scope:** `let p: Pair[I32, NoShow] = ...` 注解位的 TypeArgs bound 核对,复用 bound_sat
结构化谓词;负例 `fx_bound_ann_neg.ct`(E2050);正例扩展 fx_gstruct。
**Files:** compiler/src/sem_type.ct(Let 注解处理位,参照调用点 check_tpar_bounds 先例)、
compiler/test/fx_bound_ann_neg.ct(新)、smoke.sh(tc_fx 行)。
**Verify:** 负例 check 拦截 E2050;正例 emit 双面逐字;全量门禁。
**依赖:** 无。**规模:** 0.5 天。

### 切片 P0-B:调用点推断 C 线 neg 面(✅ 已完成 2026-09-13)

**Spec:** v0.7 修订三(E2060/E2061)——自举/R 线已落,C 宿主缺 neg 判定。
**Scope:** compiler-c 语义检查补 E2060(无法推断)/E2061(候选冲突)最小面;
suite 现有 04f_infer_* 用例双侧通过(当前宿主侧缺诊断,以「自举过/宿主未过」
能力缺口条目在册)。
**Files:** compiler-c/src/(语义检查 C 实现处,镜像自举 sem 的 infer 判定)、
compiler-rust 对应面核对。
**Verify:** suite 58/58 且能力缺口清单不再含推断条目;负例双侧拦截。
**依赖:** 需先读 compiler-c 的检查结构(侦察 0.5 天)。**规模:** 1-2 天。

### 切片 P0-C:S/D 域 const 穿发射(✅ 已完成 2026-09-13)

**Spec:** §3.3(String)/§8.4(const 编译期求值);现状 I/6/B 已穿,S/D 缺。
**Scope:** driver_emit const 折叠直出扩展——S:运行时字符串值经 C 转义辅助
(引号/反斜杠/换行/制表)emit 为 `static const char* t_X = "...";`;
D:df_can 规范文本核对(仅接受合法 C double 字面量形态,含 `-`/小数点;
指数形态先拒或转写)。fx_comp_ok 扩正例;不可表示形态走 E2010 或静默回退登记。
**Files:** compiler/src/driver_emit.ct(const 分支)、新增 C 转义助手(Ctron 写,
发射器自身函数,+1 decls 需同步锁)、compiler/test/fx_comp_ok.ct、fx_comp_type_neg.ct。
**Verify:** Str/F64 const 程序 emit 编译运行双面一致;全量门禁。
**依赖:** 需先核 D 域文本格式口径(df_can 输出形态盘点,0.5 天)。**规模:** 1 天。

---

## P1 运行面/发射面语义对齐(中等,方向既定)

### 切片 P1-A:发射面 Drop/RAII(§6.4)(✅ 已完成 2026-09-14,计划见
### 2026-09-14-p1a-drop-raii.md;前置 struct 方法/UFCS 分派已由 d614eaf 三线修复)

**已落地:** Drop 方法合成发射(`t_<T>__drop` 值接收者)+ Drop 层栈(env `#dls` 串栈:
ct_block/ct_body/BlockExpr/match 臂/test 体五个 C 作用域边界 push/emit/pop)+ Let 分支
`has_drop_impl` 登记 + return 路径全层逆序弹栈 + 尾值先落临时(`t_rv`/`t_blk`)再发 drops
+ 裸块语句/值位发射(消 `ct_expr:BlockExpr` 回退爆点)。fx_drop 三方逐字
(seed 解释 == seed 发射 == native 发射);smoke 100 ok;suite 61/61;固定点逐字节。
**v0 命中即停(响亮拒发):** while 体非提升 Drop 局部、own 体内 Drop 局部、
类引用 Drop(§6.4 类引用不触发;eval 统一 "U" 的三线分歧另挂账)、泛型 impl Drop。
**R 线:** 发射侧已有 emit_scope_drops(trans.rs),本片为其自举镜像,无需回移。

### 切片 P1-A2:panic 路径 Drop 展开(§6.4 余量)(✅ 已完成 2026-09-15,faa8986)

**已落地:** 运行时 drop 栈(ct_drent{fn,obj} 指针条目,2048 深;指针语义天然跟随
重赋值/循环重注册) + 每类型 thunk(`ctron_dth_<T>`,与 drop 原型同扫描生成) +
`ctron_panic` 入口先 `ctron_drop_unwind()`(弹后再调,嵌套 panic 有界)再
longjmp/exit;let/while 提升 → push,作用域出口内联 drop → 配对 pop(运行时 LIFO
与发射逆序一致);驱动 panic 流补输出累积 out(Drop 体 println 可观察);
非任务 panic 消息统一 stdout 无换行(与 eval 逐字对齐)。fx_drop_unwind 三方逐字
(boom → drop:2 → drop:1 → drop:10,rc=1);smoke 101 ok;suite 63/63;固定点逐字节。
**挂账:** 并发任务共享 drop 栈(任务隔离需 tls 化,语料避——spawn 本就拒 struct 捕获);
宿主 rt panic 不跑 Drop(与 U64 上界同族宿主落后点);**E2071 解除拆 P1-A3**
(break/continue cleanup 需循环基线深度追踪 + popn 计数,独立增量)。

### 切片 P0-E(新增,2026-09-14 发现):自举解析器非法输入韧性(✅ 已完成 2026-09-14,4da955f)

**现象:** 含非法标点(如 `;`)或 inherent impl(缺 `for`)的 .ct 过 `bin/ctron-cc run`
直接 SIGSEGV(rc=139;盲进 adv + p_typ 兜底吞 token + toks[cur+2] 越界直读),
seed 宿主正确报 E1001。native.sh 重建后仍复现(非陈旧二进制)。
**已落地:** 词法器列号跟踪(cols 与 toks 平行)+ 三守卫镜像 seed(`;` 报后丢弃续扫/
`::` 报后降级单冒号/集合外字符报"无法识别的字符")+ p_impl 三连守卫
(缺 for 不消费/类型位非起始报"预期类型,实际 X"消费一枚/体缺 `{` 不消费,
NL 跳过对齐)+ 三驱动打印 `PATH:LINE:COL CODE: MSG` 止于 rc=1。
诊断行与 seed 逐字(含组合场景);负例 01i/01j 双侧入 suite(63/63);
smoke 102 ok;固定点逐字节;decls 锁 273→277(+pdiag/lex_single/type_start/scan4)。
**边界:** 宿主 check 汇总行("N diagnostics")native 面不打——与既有 sem 负例的
native 口径一致; suitescope 负例协议只核 rc+码+消息子串,不受影响。

### 切片 P1-B:ISize/USize/U64 定宽存储(§3.1)(✅ 已完成 2026-09-15,
### 计划见 2026-09-14-p1b-u64-widths.md)

**已落地:**
- 解释侧新增 **"7" 值域**(U64/USize:无符号十进制文本,0..2^64-1 精确承载,复用 6 域
  c6 竖式算术全家,界门 `c6_in_u64` ≤ 2^64-1 字符串比较);val_arith/vcmp/veq/fmt 四处
  分派补齐;let 注解消费扩 U64/USize→v7、ISize→"6"(与 I64 同宽同检查);u64/usize
  后缀字面量 → v7(lit_to_dec 文本累算精确);conv_as 7↔全类型(as[U32] 既有回绕产物
  不变,03b 巧合相等点复核保持)。
- 发射侧:`ct_ty_code` U64/USize→"7"、ISize→"6";`ct_ctype` "7"→uint64_t;
  driver_emit 预发 ctron_u64_add/sub/mul/div/mod(__builtin_*_overflow + panic 消息
  镜像宿主);Binary/复合赋值 "7" 路由;as[U64]/as[USize] 发射 = TypeArgs 位拦截 →
  `(uint64_t)(x)`(§3.6 窄化=截断)。
- **顺带修复(自举传播缺陷,roadmap 预警项):** txt_num/dvi 的 I32 直接累加在
  seed-rt 检查算术下对超 I32 文本必炸("integer overflow (*)",ctc.sh 路径从未跑过
  03b 故潜伏)——迁移为 "7" 域文本累加 + mod 2^32 + MIN 特判(真 I32 回绕语义,
  免界门/免递归);as[U64] 发射缺口随片闭环。
- **Verify:** fx_u64/fx_u64_ovf(上溢 panic 探针)bootstrap 双面一致;03b 双通道绿
  (host 2/2 + bootstrap);smoke 101 ok;suite 63/63;固定点逐字节;decls 277→281。
**挂账:** 宿主 rt 64 位无符号 Add 缺上界检查(big+1 不 panic 而打印 2^64——host
ck_int 仅判 x≥0,emit/eval 均按 spec panic,宿主缺陷待修);u32 后缀的发射面
(语料无;fx_u64 避)。

### 切片 P1-B2:W 域(U8/I8/U16/I16)发射面定宽(§3.1)(✅ 已完成 2026-09-15)

**已落地:** 类型码 `w8u/w8s/w16u/w16s`(ct_ty_code Named 臂 + ct_ctype 定宽 C 型);
`ctron_w_add/sub/mul` 检查帮手(int32 中间量 + 宽度界 [0,255]/[-128,127]/[0,65535]/
[-32768,32767],panic 消息镜像 w_arith)+ div/mod 除零门(镜像 eval ari 口径);
Binary 取双操作数较宽宽度域(w_code_wider 镜像 w_rank);Let/复合赋值 W 臂
(Int 字面量转十进制——0b/0o 原文非法 C);u8/i8/u16/i16 后缀 typeof → w 码;
as[U8] 等强转泛化。fx_w8 三方逐字(满界/字面量进制/检查算术/比较/as);
fx_w8_ovf 上溢 panic 双面一致;03b 双通道绿;smoke 107 ok;suite 63/63;固定点逐字节。
**§3.1 定宽整数自举线全量收口**(I8..I64/ISize/U8..U64/USize 十档解析/sem/eval/发射贯通)。

### 切片 P1-C:Simd 运算面盘点与口径(§9.5)(✅ 盘点+决策完成 2026-09-14;补齐拆 P1-C2)

**盘点结论(2026-09-14):**
- eval 侧已按**标量模拟**实现:`Simd[E,N]` TypeArgs → "SIMD" 值(eval_expr.ct:439-447,
  携元素头名+N)、`.splat(v)` → "VEC" 逐元素值(eval_call.ct:608-619)、VEC `.lane(i)`/
  `.to_array()` → "A" 数组(eval_call.ct:620-633)。tests/09_simd.ct(白名单算术
  `a * b + a` + lane + to_array)解释面绿(在 suite 63 件内)。
- 发射侧零覆盖:TypeArgs-Simd 无臂(ct_ty_code 无分支)、VEC Binary 无元素级循环、
  lane/to_array 无发射;R 线已有 ct_farr{float* d} 句柄(trans.rs:860)。
**口径决策(成文):标量模拟。** spec §9.5 明文允许实现口径;自举发射器以 C 标量循环
模拟向量语义(与 eval 同构),不做 SIMD intrinsics(与自举"C 可编译产物"定位一致,
R 线 ct_farr 同为标量句柄)。f32 语义经既有"浮点二进制舍入/f32 精度"挂账统一承担
(eval 为十进制定点域,发射为 C double,语料值域内观察等价)。
**补齐拆 P1-C2(✅ 已完成 2026-09-14,f3d9080,规格全兑现):** ①ct_expr TypeArgs-Simd
+ Member splat → arena 分配 + 填充循环(GNU 语句表达式);②Binary 白名单(* + - / Mod)
对 SIMD 码 → 元素级循环;③lane → 越界守卫直取;④to_array → 视图恒等(文档化);
⑤SIMD 码 `q<N><ec>`(ct_typeof TypeArgs/Binary 传播/lane·splat·to_array 返回臂 +
ct_ctype 复用 ctron_view_<ec>);⑥fx_simd 三方逐字(镜像 09_simd.ct);
**顺带修复**:成员调用 typeof 尾 `ct_fn_ret` 未知名崩溃路径 + Index panic 的
nline(未带戳节点)陷阱。smoke 100 ok(3 红全为 P0-G std 泳道项);suite 63/63;固定点逐字节。

---

### 旧 P1-C 原文(已被上条取代)

**Scope:** 盘点 eval/trans 的 Simd 现状(前奏 splat/lane/to_array 已钉);
决策:标量模拟口径成文(spec 允许实现口径)或补齐向量发射。
**Verify:** Simd 夹具双面逐字。**规模:** 盘点 0.5 天,补齐视决策 1-3 天。

### 切片 P1-D:切片 T[] / &T[] 运行面(§3.1/§3.6/§3.8.2/§7.4)(✅ 已完成 2026-09-14,
### 计划见 2026-09-14-p1d-slice-runtime.md)

**已落地(发射面;eval 零改动——"A" 值本就引用语义,03f 解释面早已绿):**
类型码 `v<ec>` 视图码(Slice/Ref-Slice)+ ArrayT→既有 a 码;`ctron_view_<ec>{T* d;
int64_t n}` 视图 typedef(标量五码前置,宿主 ctron_arr_*/R 线 ct_arr 同形);let 注解位
三形态(数组字面量→ctron_amalloc 堆/内联数组退化→复合字面量取首址(写透过)/视图拷贝);
实参退化转型(ct_arg_cast a→v 复合字面量);索引读/写越界守卫(GNU 语句表达式,panic
镜像 eval "index out of bounds")+ `.len` + for-in 视图臂。fx_slice 三方逐字
(seed 解释==seed 发射==native 发射);trans_v0–v3 往返绿;smoke 102 ok(两 std 项为
并行泳道漂移/P0-F);suite 63/63;固定点逐字节。
**v0 命中即停:** 视图索引/索引写/for-in 接收者限 Ident;ArrLit 表达式位仍仅 let 初值位。
**挂账:** 非 var 根元素写诊断码(§4.2,三线一致缺口)、iter()/适配器(r3b 锚)、List 退化、
嵌套视图;已发现并转登记 **P0-F**:fn 值适配器 `ct_ad_L<nline(Ident)>` 命名恒 "0"
(Ident 无行号戳),同文件 ≥2 fn 值引用即重定义——并行 std 泳道新 sort 语料首次触达。

### 切片 P0-F(新增,2026-09-14):fn 值适配器命名冲突(✅ 已完成 2026-09-14,734c4f8)

**现象:** `ct_ad_L` + nline(Ident)(trans_expr.ct:92)恒得 "0"(Ident 不在批次九
行号戳节点族),同文件多个 fn 值引用 → 多个 `ct_ad_L0` 文件级定义,gcc 重定义。
**已落地:** 适配器按被调名命名(`ct_ad_<被调>`;Ctron 无重载,名即签名)+ `#ifndef
CT_AD_DEF_<被调>` include 守卫去重(同被调多处引用仅落地一份定义)。fx_fnval_multi
(6 处 fn 值引用:不同被调 + 同被调多次)三方逐字;suite 63/63;固定点逐字节。

### 切片 P0-G:发射面 std 新域适配(✅ 大部分完成 2026-09-15,a143164;json 运行期余量在册)

**已落地:**
- `char_len` prop 发射(Str UTF-8 首字节计数,ctron_char_len 帮手,§3.1.1 前奏 API);
- `.or(默认)` 组合子 typeof + 发射(ct_i 槽往返,variant 0 取载荷);
- `is_some/is_ok/is_err` 发射臂(variant 判定);
- `let x = match R {…}` let 初值位 R-match ANF(Ok/Some 绑定写值 + Err/None 块,
  PatWild 丢弃目标);csv/unicode 双模块 emit→gcc→运行与解释逐字一致。
- **顺带修复(while 提升声明跨兄弟块作用域缺陷):** 提升声明原发于首个 while 站点的
  嵌套块,同 fn 后续循环复用 hreg 不发声明 → 未定义标识符;现每站点块作用域内
  各发零初始化声明(C 遮蔽安全),提升声明与 while 同包一块。
**json 运行期已修复(2026-09-15):** 段万能三连根因——①`ct_opt_elem_of` 对
Result[List[…],E] 载荷回落 "i"(指针 int32 截断,符号扩展段万能)→ 补 List 臂;
②let R 码绑定未挂 `#elem:<名>` 侧条目(Ident 接收者载荷码不可查)→ Call 初始化
即绑;③test 块内用户 return 在 eval 语义=提前退出且通过,发射侧误作判定值 →
`#intest` 区分,用户 return 发 `return 0`(断言失败仍 return 1)。json 模块与
std 包 main 双引擎 rc=0 逐字一致。**余量仅 vendored 快照同步(std 泳道义务)。**
**P1-A3 定口径(2026-09-15):** 宿主发射器 Drop 零支持 + 宿主 rt panic 不跑 Drop,
E2071 解除需宿主先行,跨线协同暂缓(bootstrap 发射侧 cleanup 机制已具备)。

### GC spike(§6.2,✅ 设计记录完成 2026-09-15,见 specs/2026-09-15-gc-contract-design.md)

**结论:§6.2 语言语义面 = 四条性质**(无手动 free/全档内存安全/无 finalizer 交互/
资源持有 lint),收集算法与停顿目标属"可插拔"运行时工程,语义零影响。现状 bump
arena = 合法实现口径(永不回收 arena,回收时机不可观察,代价=长驻进程内存单调
增长,v0 明示取舍)。**随片落地 E4050**(§10 登记 + fx_res_class_neg 负例):
类直接持有 Mutex/Channel 资源字段 → E 级拦截;Field 节点补行号尾槽(修 nline
未带戳崩溃);真实收集迁移路径(shadow-stack 精确 GC)登记远期 P3,依赖类型
信息精细化,语言层零改动(无 finalizer ⇒ 清扫只还内存不调 Drop)。

---

## P2 需设计 spike(先出设计记录再排期)

### 切片 P2-A:comptime 完整形态(§8.4)(✅ 语句/循环已落地 2026-09-15,
### 设计记录+实施见 2026-09-15-p2a-comptime-stmt.md)

**已落地:** ceval_block 补 Let/Assign(Eq)/While/For(range) 四分支——每迭代/每语句
计步(既有 1200 步池,无限循环必然 E6010),Break/Continue/非 I 值/非 Eq 复合赋值 →
静默回退全量求值(既有口径);**顺带修复 Ident 查找正向扫描取最旧绑定的错位**
(改反向扫描取最新,与 Assign 写入槽一致);const 折叠自动获得语句/循环能力。
fx_comp_stmt(wsum/fac)三方逐字;smoke 108 ok;suite 63/63;固定点逐字节。
**余量(2026-09-15 复审后降级/暂缓):** comptime 容器值域与 Break/Continue —— **非规范
必须**(§8 冻结范围仅"comptime 常量求值",单表达式/If/递归/While/For 语句已覆盖),
实施中发现需要完整的 k/r/b/c 流模型重构(块尾槽执行、Return 与块值区分、跨域传播),
已回退半成品、登记设计要点后暂缓;fx_comp_list 原型随片移除。

### 切片 P2-B:Ctron.toml 解析与包元数据语义(§2.7)

> **supersede 注(2026-09-16)**:清单格式已裁决迁往 CTCL(`Ctron.ctcl`),本切片解析目标改为 CTCL 文法(块式/四类值/注册表校验),TOML 子集方案作废;见 `docs/superpowers/specs/2026-09-16-config-language-v1.md` §9/§11。

**Spike 问题:** 元数据哪些键进语言语义(包名/版本/依赖)?解析器用 Ctron 写
(自举一致性)还是宿主?依赖解析与 §2.6 循环检测的关系。
**产出:** 设计记录 → 解析切片(+诊断码)。

### 切片 P2-C:构建配置系统(§4.5 release 开关/§5.4 采集关闭/§8.5 预算清单)

**Spike 问题:** 配置来源(命令行/文件/内嵌注解)、与 profile 的关系、
发射面检查开关的实现位(发射器全局标志,自举传播再评估)。
**产出:** 设计记录 → 配置系统切片。此切片解锁:I32 检查发射的可行路径之一。

### 切片 P2-D:精确诊断 §10.2(span 行列/notes/fixes)

**Spike 问题:** parser span 从「行号尾槽」升级为行列结构(节点槽位扩展,
编译器自身 AST 兼容);fixes 编辑模型(发射/应用面)。
**产出:** 设计记录 → span 切片(建议与 LSP 合并立项)。

---

## P3 跨线/远期(spec 本身标注远期,不在本轮)

- 宿主 i64 parity(compiler-c 的 i64 为 W 域模型,与自举 6 域对齐需宿主重构)
- §9 后端矩阵(WasmGC/JSPI/Cranelift/LLVM/裸机)
- LSP 完整(hover/跳转/补全,P5)
- R 线逐切片同步(上述语言级切片各自内含)

---

## 已并行推进:标准库组织(2026-09-13)

仓库根 `std/` 建立为标准库规范源(六模块:str/sort/map/set/fmap/fs + README
组织宪章:一文件一模块/模块独立不互 use/函数式值传递/命名消歧/文档头/测试
随模块/字节级纪律/无副作用);`compiler/test/stdpkg/std/` 转为同步副本,
smoke 3d 加逐字节漂移断言;示例 vendored 副本为钉定快照允许落后。新模块
(json 等)按宪章 Checklist 进std/。

## 执行顺序建议

```
P0-A/P0-B/P0-C/P0-E/P0-F(✅) → P0-G(1-3d,发射面 std 新域适配,与 std 泳道协同)
P1-A / P1-A2 / P1-D / P1-C / P1-C2 / P1-B(✅) → P1-A3(1-2d,E2071 解除+break cleanup)
P2 各 spike 穿插在门禁等待期
```

## 自查记录

- 规范覆盖:§2.5/§3.5/§7.3 已实现(核实撤案);§2.7/§3.1/§3.6/§3.9/§4.5/§5.4/§6.4/
  §6.5/§8.4/§9.5/§10.2 各有条目;§9 后端矩阵属远期标注 ✓。
- 类型一致性:切片间共享机制(#clcodes/预扫/bound_sat/ct_targ_env)均已在
  main 落地,本计划只消费 ✓。
- 占位符:P0 全任务级;P1-A/B 含设计要点与验收;P2 以设计 spike 为交付物
  (设计 spike 本身即任务产出,非占位符)✓。
