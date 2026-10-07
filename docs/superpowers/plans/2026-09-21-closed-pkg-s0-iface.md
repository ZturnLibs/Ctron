# 闭源包分发 S0:iface 投影 + 导出符号表 实施计划

> 上游:[2026-09-21-closed-pkg-distribution-design.md](../specs/2026-09-21-closed-pkg-distribution-design.md) §10 S0、§3.1(iface 面)、§4(检查对照表)。
> **Goal:** 新增 `ctron-doc` 驱动(`compiler/src/driver_doc.ct`,经 build.sh 拼接为 `build/cc_doc.ct`):
> 读入入口 .ct → 与 cc_check 同径(scan4 → p_file → pkg_load_use)→ 按源序倾倒包的
> **iface 面**:pub 符号表(FnPub / pub Struct / pub Enum / pub FnExt,与 loader 可见性
> 裁决同判)+ trait 面 + impl 方法面,逐行确定性文本输出。
>
> **验收(spec §10 S0 门禁):**
> 1. 投影幂等——同输入两次运行输出逐字节一致;
> 2. E5030 用表裁决与源码路径同判——驱动复用 `pkg_load_use`,双模块同名 decl 负例命中
>    E5030、导入非 pub 项命中 E2020,与 cc_check 完全一致;
> 3. 真实包跑绿——std 消费面(compiler/test/stdpkg)与单模块面(std/str.ct)均可倾倒;
> 4. smoke 无新增红(decl 锁 343 不动:锁只计 cc_run = CORE + driver_run,新增驱动文件不进 CORE)。

## 设计要点

- **消费路径同源**:driver_doc 与 driver_check 共用 pkg_load_use,§4 对照表中
  E5030/E5020/E4010/E2020 四项检查零新增代码即同判;iface 倾倒发生在合并后的单一 AST 上。
- **可见性裁决镜像 loader**(parse_pkg.ct:270-292):FnPub 按 tag 即 pub;Struct/Enum/FnExt
  按尾槽 `"pub"` 判;v0 解析器无 pub trait/const/static,故 trait 整体列入 iface(非符号计数),
  Const/Static/私有 fn 不入 iface 体,仅计入 decls 总数。
- **输出格式**(确定性文本,源序,可 grep;为 §5.3 agent 契约头 / `ctron doc` 消费预留):
  `iface <entry> decls=N` 头 + 逐行 `pub fn name[T,…](p: T, …) -> R` /
  `pub struct …` / `pub enum …` / `pub extern …` / `trait …` / `impl T for X`(方法缩进)+
  `iface symbols=M` 尾(M = 可 use 导入的 pub 符号数,与 loader 可导入集同口径)。
- **类型渲染**:doc_ty 递归 Named/Ref/Optional/TupleT/Slice/ArrayT/FnType(p_typ 全部七个
  产出形);ArrayT 尺寸表达式以 `..` 占位(v0,确定性优先)。
- **不做(spec §10 后续片)**:ctc.sh `doc` 子命令与 native.sh 原生化(下一片)、JSON 输出面、
  per-fn 契约头抽取、剥名与 AST 序列化(S1+)、traces(S3)。
- **泳道纪律**:纯新增文件(driver_doc.ct)+ build.sh 一行拼接;不触碰在途泳道脏文件
  (driver_emit/trans_*/eval_call/sem_type/gui_parse);生成物 build/cc_doc.ct 不被 git 跟踪,
  本地生成验收,不入库。

## 任务分解

| # | 任务 | 位置 | 验收 |
|---|---|---|---|
| 1 | driver_doc.ct(投影 + 符号表 + main) | `compiler/src/driver_doc.ct` | 本表 1–3 |
| 2 | build.sh 拼接一行 | `compiler/build.sh` | 生成 cc_doc.ct 可运行 |
| 3 | 幂等 + 负例 + 真实包验证 | /tmp 夹具 + stdpkg | 逐字节 diff 空;E5030/E2020 命中 |
| 4 | smoke 全量回归 | `compiler/test/smoke.sh` | 红项集合 = 实施前基线(在途泳道噪声单独归因) |

状态:✅ 完成(2026-09-21)。验收实录:①幂等——同入双跑逐字节 diff 空;②E5030/E2020
负例双中(与源码路径同判,复用 pkg_load_use);③真实包跑绿——std/str.ct(38 pub fn)、
compiler/test/stdpkg(合并 257 decls,泛型 `[K: Eq, V]` 渲染正确)、全形态夹具
(pub struct/enum + trait + impl + 私有/const 正确排除);④smoke 113 ok / 2 fail,
两红均为在途泳道已登记项(cc_run decls=345 待重锁——实测 cc_run 含 0 个本片 decl;
std/↔stdpkg 副本漂移——std 泳道待同步),本片零新增红。

## S0.5(2026-09-21 续):用户面接入

**Goal:** S0 驱动长出用户面——`ctc.sh doc` 子命令、`bin/ctron-doc` 原生化(第五产物)、
JSON 输出面(`--format=json`,schema v0 冻结)。

**JSON schema v0(冻结;文本面不动):**
`{"entry":..,"decls":N,"iface":[item…]}` 源序;item 六形态:
fn/extern `{"kind","name","generics","params":[{"name","type","mutable"}|{"vaargs":true}],"ret"}`
(ret 空串 = 无返回);struct `{"kind","name","generics","fields"}`;enum `{"kind","name",
"generics":[],"variants":[{"name,"tuple":[..]}|{"name,"fields":…}]}`(unit 变体仅 name);
trait `{"kind","name","sigs":["fn .."|"prop .."]}`;impl `{"kind":"impl","sig","sigs"}`。
旗标口径与 driver_check 同构:`--format=json|1` 旗标优先(native 经 ctron_cli_flag),
缺省回落 ANCHORFMT 构建锚(ctc.sh doc sed 注入)。

**实施教训(登记):** Ctron 字符串转义不对称——`\{` 合法(插值开括号转义)、`\}` 非法
E1001,闭括号一律裸写;JSON 片段作子串拼接时须拆掉外层花括号内联(首次实现把 fnobj
嵌成裸对象,json.loads 当场证伪)。

**验收实录:** ①文本面字节兼容——ctc.sh doc 与 S0 基线内容行全同(仅入口路径经 ctc.sh
规范化 `..` 差异);②JSON 面 python json.loads 全绿(str.ct 38 fn + 全形态夹具六 item);
③native ctron-doc 文本/JSON 与 seed 输出深度相等,`--format=json` CLI 旗标生效;
④双跑幂等;⑤负例经新路径依旧 E5030/E2020 双中;⑥smoke 113 ok / 2 fail 基线不变。

**挂账(非本片):** 仓库根用户 CLI(`ctc.ps1`/打包 ctc)的 `doc` 子命令——归工具链泳道
(Windows 面不可本机验证,须走 sh/ps1 conformance 流程);per-fn 契约头抽取——依赖
lex 层注释保留(v0 scan4 丢注释),属编译器线单独决策,登记不排期。

**smoke 常设腿(S0.5 收尾):** 夹具 `tests/doc_fix/`(全形态)与 `tests/doc_fix_neg/`
(E5030),smoke 3g 节四查——文本金样逐字(头行路径归一后 body diff)/双跑幂等/
JSON 形准(六形态 + 首尾闭合,E 依赖)/负例拦截;smoke 117 ok / 2 fail(余两红均为
peer 在途登记项:gui decls 重锁、std 副本同步)。教训:BSD grep 对 `\{` 报 invalid
repetition,字面前缀检查用 `grep -F`。

状态:✅ 完成(2026-09-21)。

## S0.6(2026-09-21 续):用户 CLI 面 + §5.3 契约头首片 + S1 触发量化

**范围:** ①用户 CLI(root `ctc` sh 版 + `ctc.ps1` 同文)增 `doc` 子命令——`ctron-doc` 直驱,
旗标透传,help/usage/`<cmd> --help` 四入口齐;ctc.ps1 无 pwsh 本机面,案头校验落库,
**登记待 Windows conformance**(β 口径)。②§5.3 契约注释抽取首片——模块头(入口源首个
// 块)+ per-fn(紧邻 decl 行上方连续 // 块,name-keyed,仅行首 pub 拼写:pub fn/struct/
enum/extern;空行断块;模块头整体跳过不附首 decl;私有 fn 不入账);文本面 `//` 前缀
还原,JSON 面 schema 升 **v0.1**:顶层与 fn/extern/struct/enum item 各增 `"doc"` 字段。
③S1 触发条件量化(§10.1 "重复解析占比显著"的实测基线):stdpkg 257 decls 下
doc(parse+load)≈ 2.0s vs check(+sem)≈ 5.0s,**解析占比 ≈ 40%**;
**S1 立项线登记:std 面积 ×3 或解析占比 > 60% 或出现无源码分发需求方,三者其一**。

**语义细节(登记):** 契约抽取是入口源码行级启发(fmt 规范形态假设),非语义面——
跨模块合并后无法归属文件,故仅入口源;导入符号的同名入口私有 decl 不误附
(行首无 pub 不匹配)。std/str.ct 实测立现价值:38 fn 中已有 per-fn 注释直接出面。

**验收:** str.ct/geom.ct 双面抽取正确(模块头单次、area.doc 命中、Point 未误附);
root `ctc doc` 文本/JSON/`help doc` 三路通;native ctron-doc 重建后旗标生效;
smoke **119 ok / 1 fail**(doc 腿扩至五查全绿;余 1 红 = std 副本漂移,peer 泳道待同步)。

状态:✅ 完成(2026-09-21,S0.6)。

## S0.7(2026-09-22):`doc std.<module>` 形 + 用户 CLI 透传

**范围(spec §5.3 点名的 agent 面):** `ctc doc std.str` 模块名形态——driver 侧锚读失败
(原生 CLI 直参非文件)时按 std 根四级解析重读:①CTRON_STDPATH ②exe 旁
`../lib/ctron/std` ③cwd `./std`(dev 仓库根)④cwd `../std`(种子约定 cwd 变体);
命中后 entry 归一为实路径,契约抽取照常。root `ctc` 与 `ctc.ps1` 对无 `/` 无 `.ct`
的 token 原样透传(不绝对化),std 解析仍全在编译器内(ctc 零 std 路径逻辑,守
toolchain-design 既定原则)。

**实施教训(登记两条):**
1. **main 锚读的形态契约**:`ct_swap_anchor` 只认 main 内 `let x = read_file("字面量")`
   形;锚读必须保持该形态(本次扁平化重构把锚读挪进 match scrutinee,发射产物即退化为
   字面量读;幸而运行时另有 CLI 兜底使平面读不炸,但**形态契约不可依赖巧合兜底**)。
2. **驱动验证先查二进制代际**:std.str 三轮排障实际是 bin/ctron-doc 未重建(仍 S0.6 代);
   `ctc.sh emit <file>` 的语义是"发射 file 的 C"(file 进锚),不是"发射 cc_emit"——
   排障时手动 emit 产物要先认领来源,否则机制考古全空转。

**验收:** `./ctc doc std.str` 文本/JSON 双面通(JSON entry 归一 `std/str.ct`,38 items);
`std.nosuchmod` rc=1;CTRON_STDPATH ①号探针 rc=0;smoke 3g 腿扩至六查全绿
(+std.<module> 查)。全量套件在 peer 泳道高频落库期呈波动(gui β2 帧级调试、
std/db P5-D 连发),红项集合逐轮漂移且均映射 peer 在途编辑;doc 腿各轮稳定全绿。

状态:✅ 完成(2026-09-22,S0.7)。

## S0.7a(2026-09-22 续):域目录形态 + S1 触发线重测

**① doc 域目录形态**:`std.http.parse`/`std.db.pg` 等域目录模块——doc_mod_rel 把
"std." 后余下点号转路径分隔(模块名本身无点),四级 std 根解析照旧。HEAD 基线实测
五形态全通:http.parse 61 decls、db.pg 130、net.bind 26、tls.bind 12、str 64 回归;
pg/http 模块头契约注释直接出面。

**② S1 触发线重测(2026-09-22,基线对照 S0.6)**:stdpkg 面积 257 decls(不变——
std 新域模块 http/db/tls 由各自夹具消费,未入种子包);doc(parse+load)≈1.2s、
check(+sem)≈2.7s——工具链提速后**绝对耗时反降约一半**,解析占比 ≈44%(原 40%)。
**门禁维持且更稳**:面积×3/占比>60%/真实需求方,三者均远。

**③ 环境登记:** peer 泳道(lex/diag/trans)高频在途编辑期,树上 CORE 存在不可解析
中间态 → native.sh 静默早夭、seed 解析瞬断;本片域解析以 HEAD 基线合并验证
(`git show HEAD:` 逐文件取干净 CORE + 本片驱动),原生重建待 peer 落库后
`sh native.sh` 一次补齐(与 ps1 Windows conformance 同列挂账)。

状态:✅ 完成(2026-09-22,S0.7a)。

## S1a 底座(2026-09-22):.ctast 格式冻结 + 标签清单 + 前置登记

> 触发说明:owner 连续四次"推进剩余全部任务",视为对登记门禁的授权改判;S1 按
> 最小切片启动——本片只冻结**格式与前置**,序列化器实现为下片(S1a 本体),
> 缓存接线(S1b)仍按加速触发线挂账。

**.ctast 记录格式 v1(冻结;行序 = 先序遍历):**
- `N <tag> <nchildren>` — 节点开记录(tag = AST 标签,nchildren = 子记录数);
- `S <bytelen> <payload>` — 字符串载荷记录(字节长前缀,载荷任意字节含换行,
  免转义;确定性由遍历序与记录结构保证,规范化无自由度)。
- 判定规则:AST 节点 = `List[Str]`,[0] 恒为标签串;混合子槽(名串/子节点)按
  **形态表**(tag → 槽位种类)驱动,形如:前缀串槽(名/isv/"pub")+ 节点槽 +
  尾部变长串槽(anm/stamp/"pub" 尾标,Fn/FnPub/FnC/FnExt/Struct/Enum/Method);
  容器节点三分:全串容器(Segs/Syms/Bounds/Drvs)/全节点容器(File/Fields/Ps/
  Vars/Items/TArgs/KTuple/FnT/TupleT…)/混合节点(见上)。
- **解析器当前实测 95 个唯一标签**(parse_node/expr/stmt/decl/gui/pkg 的 mk() 构造
  位去重);形态表须全覆盖,**未知标签 = fail-closed 拒绝**(缓存与文法
  schema_version = 工具链 VERSION 锚定,§10.1 缓存键同源)。

**硬前置(登记,跨线):** 通用序列化需 `is_list(x) -> Bool` 类型判定内建——
现有语言面不存在(rt_eval/eval_expr/eval_call 无 is_list/is_str/typeof);
**无它则只能走 95 项形态表**(维护税随文法演化,等价 std 宪章"三宿主税"论证)。
is_list 三线改动点:seed `compiler-c/src/rt_eval.c` 内建表 + `compiler-rust`
interp 对应分支 + `trans_expr.ct` 降到 C 的内建分派;均处 peer 泳道在途文件,
待其稳定后由本泳道提三线小片(每线 ~5 行)。

**验收预案(S1a 本体,下片):** 往返逐字节固定点——`dump(src) ==
redump(load(dump(src)))` 于真实语料(stdpkg 合并 + fx 夹具 + 编译器自身),
seed 解释路径实现与验证(发射路径非必需);另有 P1 环境事实:当前
`ctc.sh emit build/cc_run.ct` rc=1 且产物截断 2387B(发射泳道落库回归,
native 全构建冻结)——**归发射泳道,本泳道仅上报**。

状态:底座完成(2026-09-22);S1a 本体(序列化器/回读器/往返验收)按下片实施。

## S1a-i(2026-09-22 续):序列化器/回读器落地——往返固定点全绿

**实施:** `compiler/src/driver_ast.ct`(形态表路线,is_list 不依赖)+ build.sh 拼接行
(cc_ast.ct,生成物不入库)。游标模型读写(非按行 tokenize,S 载荷免转义任意字节);
形态表 66 标签语料驱动增量,fail-closed(未知标签/子项越形态 = E-AST-UNKNOWN/
E-AST-SHAPE);模式 dump / roundtrip(ctron_cli_flag("ast") + ANCHORAST 锚)。

**验收(往返逐字节固定点 `dump == redump(load(dump))`):**
- std/str.ct:**75374 字节 / 4740 节点 OK**;
- stdpkg 全量合并(17 模块):**21482 字节 OK**;
- fx 夹具族:**62/65 OK**(含闭包/结构体字面量/枚举/模式/泛型/derive/Scope/Own/
  ComptimeVal/Range 等);余 3 = 2 个负例夹具按设计解析失败(预期)+ fx_uhex;
- fx_uhex 报"or2 期望 2 实得 1":**与本驱动无关的既存问题**——维护路径
  `ctc.sh check test/fx_uhex.ct` 同样复现(lex.ct 的 \u{} 解析路径 or2 参数错
  或 seed/lex 代际差),归解析泳道,已上报。

**踩坑登记:** ①形态槽取值须处理 `*` 尾标(SI 越界即取尾前一位);②"S 0" 空载荷
的长度解析不能扫描空格(单字符长度即越界),直接取 "S " 之后;③S 槽塞入列表的
症状是 join 期"索引目标非数组",与加载器越界("byte_slice 越界")是两类故障。

**S1a-ii 挂账:** 覆盖剩余 ~29 标签(gui 系/其余语句面,语料驱动同法);ctc.sh `ast`
模式 + smoke 常设腿(待 P1 发射冻结解除后一并上 native);is_list 三线小片
(interp.rs 净后);S1b 缓存接线(pkg_load_use 消费,加速触发线挂账)。

状态:✅ S1a-i 完成(2026-09-22)。

## S1a-ii(2026-09-22 续):形态表补齐 + ast 进 ctc.sh/smoke

**实施:** 形态表扩至 **71 标签**(selfhosted 黄金语料全语扫驱动:PatLitI/F/S、
Continue/Break、Prop 系、Static 尾戳、LField、Interp、Range/ComptimeVal/Float/Own/
ClosureParam/Scope/ArrLit/Class 等);重复表项教训(ast_put 取首个,旧条目必须删);
**O(n²) join 之死**:cc.ct 级语料的 ser 拼接触发 OOM-SIGKILL——dump 改流式直印,
roundtrip 用分治拼接 O(n log n)。

**环境:** P1 解除(发射泳道落库修复,emit 655992B 恢复),native 五件全部重建——
原生重建挂账销账。cc.ct(自举编译器单体 ~2MB ser)仍超 seed 内存上限(rc=137),
登记为规模限:**S1b 按模块分片为自然解**(缓存键本就 = VERSION + 模块源 hash)。

**验收:** selfhosted 黄金语料除 cc.ct(规模限)外全绿,sem_chk.ct 470167B OK;
fx 62/65;str.ct/stdpkg 照旧;smoke 3g 腿扩至八查(+ast 往返 geom/stdpkg)——
**全量 smoke 127 ok / 0 fail,历史首次全绿**。

状态:✅ S1a-ii 完成(2026-09-22)。


## S2a(2026-09-22):闭源工件流程端到端——seal→deps 消费→碰撞负例,全绿

**owner 需求方信号触发 S2 启动**("开始设计并编写一个闭源包,并验证流程")。最小切片:
- `src/ast_fmt.ct` 入 CORE(ast_read_line/ast_load_node,driver_ast 改用 CORE 版);
  pkg_load_use 工件分支:非 std use 源缺失时回落 `deps/<pkg>.ctart/impl<rel>.ast`,
  加载后可见性/E5030/E2020 对加载 decl 同判(spec §4 对照表);build.sh CORE 注册;
  decl 锁 347→349;
- driver_ast 增 seal 模式(--ast=seal --astout= --astname=):写 impl/<stem>.ast +
  meta.ctcl(示意键面,注册表随 S4 立表);ctc.sh ast 臂扩旗;
- 夹具 tests/artifact_demo/{provider/geom.ct, consumer, consumer_neg};
  smoke 3l 腿:seal→仅 deps 工件消费(rect=12/circle=27 行为正确,源码缺席)+
  碰撞负例 E5030 拦截;
- **已知问题订正(2026-09-22 复核)**:①"工件内 std.use E2020"**根因不在工件机制**——
  最小复现证明源码模式同败:seed 的 env_get 为恒空 stub(rt_eval.c),CTRON_STDPATH
  在 seed 路径不可见,只能走 ../std 回落(符号链接即可用;正例腿已加,真修=compiler-c
  实现 env_get,归编译器 C 线);③match/枚举工件路径失败**根因同在解析泳道**:完整
  geom 源码/工件双模式同败(match+KStruct+=> 恢复式解析吞后续 pub fn,伴生幻影
  decl 与 E5030"NL")——最小复现交解析泳道;②cc.ct 规模限照旧;④doc 非幂等 =
  peer 重生成瞬态(孤验双 OK)。演示夹具按已验证子集收敛(std.use 正例腿已加)。

**验收:** seal(geom.ct→1201B 工件)→ 消费方仅持工件跑出 rect=12/circle=27 →
碰撞负例 E5030 拦截;smoke 3l 双查全绿(全量分数随 peer 落库波动,切片腿为准)。

状态:✅ S2a 完成(2026-09-22)。

## S2a-ii(2026-09-22 续):双版本共存 + 传递依赖——四场景全绿

owner 点名测试。语义确认:**use 合并是整模块粒度**(显式符号只做可见性门),
故同名符号跨版本同程序 = E5030(设计口径:需收敛/改名,显式文化)。
- 双版本共存 ✓:deps/mygeom.ctart + deps/mygeom2.ctart 各持一份,use 前缀即
  目录名,非重叠符号同程序并存(v1=12 + v2p=14);
- 同符号碰撞负例 ✓:E5030 精准拦截(provider_clash 夹具);
- 传递依赖 ✓:calc(工件)的 use mybase 在消费端 deps 扁平解析(base 工件),
  t=12 —— 零新代码,S2a 的目录约定天然覆盖;
- 缺传递依赖负例 ✓:E2020 拦截(S4 改进:错误信息应点名 deps/<pkg>.ctart 缺失)。
smoke 3m 腿四查固化。教训:cp -r src/. target 需先建目标目录;smoke 长腿内的
工件目录逐一 mkdir。

状态:✅ S2a-ii 完成(2026-09-22)。

## S2a-iii(2026-09-22 续):版本解析洞实证 + 修正设计(owner 指名场景)

**洞实证(非理论):** calc 工件封印时依赖 base_mul=a*b;消费方 deps 放同名同签名但
a+b 语义的 "mybase" 工件 → `t=7`、rc=0、**零报错静默错版本**。根因:S2a 解析按名取
目录 + meta.dep 无摘要校验;封印名焊死于 Use 节点,安装期改名即失配/静默错配。

**修正设计(D8,三条):**
1. **主版本即身份**:包名编码主版本(fmtkit=1.x 永远;fmtkit2=2.x 永远;Go /v2 同款
   约定)。安装槽名 = 声明名,**安装期永不改名** —— 传递依赖的封印名永远可解析;
   同主版本槽内次/补丁升级必须向后兼容(semver 契约),lockfile 钉 digest。
2. **加载期摘要校验**:dependent 的 meta.dep.digest vs 实际安装工件摘要
   (SHA256SUMS 规范化摘要;in-language 校验用 std.crypto.sha256,能力已备),
   不符 = E-PKG-DEP-MISMATCH 明确诊断(替代今天的静默错配)。
3. **跨发布者同名**:发布者前缀命名空间(如 `pub/…`)—— S4 待决,登记。
   版本统一策略(MVS 式收敛 vs 严格槽位共存)随 S4 依赖解析一并裁定,推荐槽位共存
   优先(零新机制,与显式文化一致)。

**实现差距(诚实):** 当前 S2a 无 1/2 两条的强制(实验 t=7 即证);摘要校验为
下一片首件(前置 = driver 侧接入 std.crypto 或 shell 侧摘要传递);夹具
base_evil(base_mul=a+b)已留 /tmp 亦随本片入 tests/ 作负例语料。

状态:设计成文(2026-09-22);摘要校验实现按下片。

## D8-2 支持方案设计(2026-09-22):版本摘要校验的实施设计

> 目标:关闭 S2a-iii 实证的"静默错版本"洞(t=7)。威胁三分:①错版本/错内容
> (同名工件);②封印后篡改;③跨发布者同名。防线分三层,本期实施 L2。

**摘要定义(冻结):**
- 工件摘要 = `sha256(SHA256SUMS 字节)`;SHA256SUMS 行 = `<hex>  <相对路径>\n`,
  成员按路径字节序排序(与发布产物校验单同文化)。
- self_digest:封印时编排层算出工件摘要,写入**自身 meta.ctcl** 的
  `artifact` 块 `self_digest` 键 —— 工件自证身份的锚。
- 摘要计算在编排层(shell shasum / 未来 ctc pkg),不在编译器热路径。

**meta.ctcl schema v1 增订(CTCL dep 注册表随立表,键面示意):**
```text
artifact "mybase" {
  manifest_version = 1
  self_digest = "sha256:e3b0…"      // 新增:自身工件摘要
}
dep "mybase" {                       // 消费方工件(calc)的 meta 内:
  digest = "sha256:e3b0…"            // 新增:封印时对依赖的要求摘要
}
```

**seal 流程(编排层与驱动分工):**
1. driver(seal 模式):parse → impl/<stem>.ast + 基础 meta(不含摘要);
2. 编排层(ctc pkg seal / ctc.sh 步骤):对新依赖逐个 shasum → 以
   `--depdigest=<name>:sha256:…` 旗标回传 driver → driver 追写 meta 的
   `dep` 块 digest;编排层算工件摘要 → 追写 self_digest + SHA256SUMS。
- **fail-closed**:编排层跳过摘要步骤 = meta 无 self_digest/dep.digest
  → 加载期拒载(E-PKG-DEP-UNVERIFIED),不存在"未校验也能跑"的路径。

**加载算法(pkg_load_use_done 工件分支增补):**
1. 解析 use → 工件 impl 文件定位(现有);
2. 读**消费方工件** meta 的 dep 需求集(键 = 依赖名 → 需求 digest;
   文本扫描同 pkg_caps_allowed 先例);
3. 读**被依赖工件** meta 的 self_digest;
4. 比对:不等或任一方缺记录 → `E-PKG-DEP-MISMATCH` / `E-PKG-DEP-UNVERIFIED`
   (消息点名依赖名 + 双摘要);相等 → 继续现流程(ast_load/合并);
5. done 集语义不变(已验过的依赖随 done 跳过,不重复校验)。

**诊断(占位码,归编译器线分配):**
E-PKG-DEP-MISMATCH(需求摘要 ≠ 实际 self_digest,消息含双摘要与依赖名)/
E-PKG-DEP-UNVERIFIED(meta 缺 self_digest 或缺 dep 记录)。

**分期与诚实边界:**
- L2(下片首件):上述**记录-比对**校验 —— 依赖双方均为"诚实封印"时,
  错版本/改名静默配错被精准拦截(base_evil 夹具:digest 不同 → 拒载)。
  边界:比对的是**记录串**而非实时重算 —— 封印后篡改 impl 字节不改变记录串,
  由 L1(SHA256SUMS shell 校验,`ctc pkg verify`)覆盖;
- L3(S3/S4 合并):in-language sha256 重算 + attest/trace;
- 命名空间(跨发布者同名)随 S4;E2020.use.read 错误信息点名工件缺失同批小改。

**验收预案:** base_evil 负例(digest 不符 → E-PKG-DEP-MISMATCH,现 t=7 静默
转为明确拒绝)+ 正例(digest 一致 → 行为不变)+ 未验负例(去 self_digest →
E-PKG-DEP-UNVERIFIED)+ 篡改负例(改字节 → L1 shasum -c 失败)。

状态:设计成文(2026-09-22);L2 实现按下片。

## D8-2 L2 落地(2026-10-06,T50 批):摘要校验 + seal 编排 + S1a-iii 补验全绿

- **诊断码分配**:E-PKG-DEP-MISMATCH = **E5054**、E-PKG-DEP-UNVERIFIED = **E5055**
  (E5054-5056 核实空闲;diag_msg.ct 双语模板 + meta_check.py 码表 +
  docs/spec/10-diagnostics-conformance.md 三面注册)。
- **seal 编排(ctc.sh ast 面编排层,编译器热路径零触碰)**:`--depdigest=<名>:sha256:<hex>`
  (分号多值)经临时文件锚 ANCHORDEPFILE 注入 driver_ast seal 模式 → meta dep 块;
  编排层算 SHA256SUMS(`impl/**` 载荷,路径字节序,**不含 meta**——self_digest 入
  meta,含之即循环)+ self_digest = sha256(SHA256SUMS 字节) 插入 artifact 块
  (meta 首块,首个 `^}` 即其闭括号);darwin shasum / linux sha256sum 双封装 ctc_sha。
- **加载期校验(pkg_load_use_done 工件分支)**:五 fn(pkg_meta_find/pkg_meta_strval/
  pkg_meta_self_digest/pkg_meta_dep_digest/pkg_art_verify,decl 锁 506→511 随批申报);
  消费方身份经第九参 ameta 穿线(包装 fn 签名零扰动);done 语义不变(已验依赖随
  done 跳过);顶层源消费方无记录面 = L2 诚实边界(钉定归 T49 lock 域)。
- **S1a-iii 补验**:geom_match 夹具(pub enum + match 表达式位,4991132 解析修复后
  工件路径回归锚)密封消费 r1=7/r2=12 双臂绿——enum 裸变体不跨模块,消费方只用
  fn 值导入形。
- **同批小改**:E2020.use.read 于 deps/<pkg>.ctart 存在而 impl 未命中时点名工件。
- **门**:smoke 3p 腿八断言(编排自证/正例 t=12/E5054 双摘要/E5055 两侧形/L1 篡改
  shasum -c/match 补验)+ 3m/3n 封印改走依赖序 depdigest;全量 smoke 166 ok / 2 fail
  (两败主树在册:conc_parallel 发射红 + Rust 臂 iter 翻绿待清账);meta_check 1 败
  主树在册(dep_mutex_neg E5049,基线实证);native 七驱动重建。
- **坑**:art_dir/next_meta 声明须在 std/非 std 分支外(校验点在分支汇合后,
  块内声明不可见 = 未解析名称实红一次)。

状态:✅ L2 完成(2026-10-06)。余:L1 升 `ctc pkg verify` 命令面(S4)、L3
in-language sha256 重算(S3/S4)、命名空间(S4)、S1b 缓存接线(加速触发线挂账维持,
2026-09-22 实测占比 ≈44% 未触)。

## L1 落地(2026-10-06,T50 批):`ctron pkg verify` 命令面全绿

- **命令面**:用户 CLI `./ctron` 新增 `pkg verify <artifact-dir>`(纯 shell 编排层,
  与 T49 add/publish/lock 同面;ctc.sh 不进安装布局不加面,零重复);ctron.ps1 四处
  同文(usage/Help-Cmd/拦截表/dispatch 臂,Get-FileHash 直译同算法)。
- **四段算法(fail-closed,rc 0/1/2)**:①形态面(meta.ctcl/SHA256SUMS/impl/ 三件齐,
  §3.5 后缀即契约)②SHA256SUMS 逐行解析重算(畸形行/路径越界[绝对形或含 `..`=
  不可信输入面]/成员缺失/摘要不符逐项点名,多失败累积)③self_digest 重算
  (sha256(SHA256SUMS 字节) vs meta artifact 块记录,与成员腿相互独立)④判决。
  载体 = `./ctron` 既有 sha256_file(darwin shasum / linux sha256sum 双封装)。
- **门**:smoke 3r 腿六断言(正例/篡改 impl/记录行翻改/缺 SHA256SUMS/越界/
  self_digest 翻改)两轮 6/6;全量 **189 ok / 0 fail**(首跑 ctecho probe3 一败 =
  在册 flake 复跑过,同 D8-2 批先例);meta_check 通过;**编译器零触碰**
  (decl 锁不动、native 不涉、无新诊断码——shell 面不进 E 码注册表)。
- **ps1 挂账**:ctron.ps1 pkg verify 待 Windows conformance(同 doc 先例;本机无 pwsh)。
- **诚实边界**:verify 对账的是"SHA256SUMS 内部自洽 + self_digest 与 SHA256SUMS
  字节一致";封印方若一开始就伪造自洽的 SUMS+self_digest,verify 不能发现——
  那是 L3(in-language 重算+attest)与 trace 复放(S3)的辖区,维持触发线挂账。

状态:✅ L1 完成(2026-10-06)。余:L3 in-language sha256 重算(载体重已证=
lib/std/crypto.ct `sha256_hex`;S3/S4)、trace 录制/复放协议(S3,§7.2 verify
后半段)、命名空间(S4)、S1b 缓存接线(加速触发线挂账维持,≈44% 未触)。

## L3 三宿主测量裁决(2026-10-07,T50 批):load 期重算不可默认开,维持 S3/S4 挂账

- **测量目击**(同一 2560B/40 块输入,三宿主摘要逐字一致 `68a9fa90…`=crypto.ct 与
  CORE vendored 副本双实现 parity 锚):

  | 宿主 | 40 块(2560B) | 75KB 工件(str.ct.ast 档,1170 块)外推 |
  |---|---|---|
  | seed 解释器(ctc.sh/ctronc) | 单块即 86.4s CPU | **≈27 小时——判死** |
  | 自宿主发射解释器(bin/ctron-cc) | 9.6s | ≈4.7 分钟——仅 CI 小件可用 |
  | native 发射 C | <0.4s(计时粒度内) | 毫秒级——**唯一可行载体** |

  与 crypto_vec run.sh 在册「interp 臂逐文件预算 ≈10 块」完全互证(其 interp 臂
  = bin/ctron-cc,10 块 ≈2.4s ✓;seed 面从来不可用)。
- **裁决**:加载期 in-language 重算**不可默认开**——它必须跑在消费方驱动宿主上,
  而三宿主两不可用(seed 灾难级/ctron-cc 分钟级),默认开 = 在慢宿主上
  fail-open-by-slowness,违 fail-closed 宪法(D7)与 parity 纪律。
  **L3 维持 S3/S4 触发线挂账**,载体定为**独立发射验证器**(ctron-dep 先例:
  in-language 写、发射 native 跑,C 速哈希,零编译器热路径触碰)——与
  attest/trace 工具同批是 S3/S4 的自然首件。
- **顺手收获**:①std crypto.ct 与 compiler CORE 的 pkg_dep.ct vendored sha256
  结构同构,三宿主摘要一致——将来 S3/S4 验证器两实现任选;②emit 面 std 合并缺口
  再添两实例(用户程序 `use std.crypto`/StringBuilder → 发射产物 t_sha256_hex/
  t_StringBuilder 未定义,host-divergences 在册债同族);③解释器性能债(自举
  interp 每步 arena 分配无回收)新增具体锚:seed 算术模拟循环 ≈1.3s/轮 64-bit。
- **测量教训(假缺陷警示)**:本轮一度把「timeout 击杀+管道尾读+stdout 缓冲丢失+
  管道后 `$?` 取尾命令」误读成 seed 非确定挂死,追查半日方知全部是慢——
  hash 类探针一律「外置 time+直跑不管道+完整跑完不设短 timeout」。

状态:✅ L3 裁决落账(2026-10-07,维持挂账+载体定案)。余不变:L3(S3/S4)、
trace(S3)、命名空间(S4)、S1b(加速线)。

## L3 载体落地(2026-10-07,T50 批):bin/ctron-verify 深验驱动全绿

- **落地形态**(即 L3 裁决定案的独立发射验证器):`compiler/src/driver_verify.ct`
  与 driver_dep 同构(CORE + 单 driver)——sha256 直呼 pkg_dep.ct 的
  `pkg_sha256_hex`、meta 扫描复用 parse_pkg.ct 的 `pkg_meta_find`/
  `pkg_meta_self_digest`,**零重复实现**;build.sh +cc_verify 拼接件、
  native.sh 发射 `bin/ctron-verify`(36983 行 C);输入走 `ctron_entry()`
  (`ctron-verify run <dir>`,ctron-dep 同通道,编译器热路径零触碰)。
- **用户面**:`ctron pkg verify --deep <dir>`(sh+ps1 同文;无 --deep = shell 面
  不变)。deep 与 shell 面四段算法/文案逐字同构,同工件同判(实测 self_digest
  双面一致);多失败累积、路径越界(绝对形/含 `..`)拒绝、rc 0/1/2 全同。
- **性能兑现**:75KB 档工件深验 **11ms 端到端**(L3 裁决"native 毫秒级"经
  用户 CLI 实证;对照 seed interp ≈27h / ctron-cc interp ≈4.7min)。
- **门**:smoke 3s 腿三断言(deep 正例同判/成员腿字节篡改/self_digest 腿独立
  同判;binary 缺席则跳过如 nc 守卫先例);全量 **193 ok / 0 fail**(含 ctecho
  就绪门修复后首次无需复跑);native 八驱动族重建核齐。
- **同批治愈**:examples/ctecho run.sh 就绪门(固定 sleep 0.5 → nc -z 轮询
  10s 上限;仪器实测冷启就绪 0.869s 超固定等待 = 在册 probe3 flake 根因;
  修复后 run.sh 10/10+全量 190/0,fa3d9b6a)。

状态:✅ L3 载体落地(2026-10-07)。余:trace 录制/复放协议(S3)、命名空间
(S4)、S1b(加速线)——均触发线挂账。

## S3-α 落地(2026-10-07,T50 批):黄金轨迹录制/复放(纯函数)全绿

- **命令面**:`ctron pkg trace record <src.ct> --case <fn>=<args 字面量>
  [--case ...]` / `ctron pkg trace replay <src.ct>`(sh+ps1 同文;宿主 =
  bin/ctron-cc source 复放)。码gen = 编排层拼一次性驱动(src 原文 +
  `fn main { println(<fn>(<args>).to_string()) }`),值规范形 = to_string
  原文(同 fn 同参确定性等价;**标量 JSON 规范写入器 = S3-β 的 std 能力件**,
  届时重录轨迹即迁移)。
- **轨迹文件**:traces/<stem>.ctrt——CTCL 键控块机写(`trace "<fn>" { args /
  expect }`);seal 编排拾取源旁 traces/ 入工件 + 并入 SHA256SUMS → 轨迹字节
  由 self_digest 覆盖,防篡改免费继承(§3 布局 traces/ 首兑现;deep verify
  对含 traces 工件直接过验)。
- **诚实边界**:α 纯函数 only——效果函数(FakeFs 脚本化录制)、零参 fn
  (tab 切分丢字段)、含 main 源(预检拦截)、加载期复放腿(verify 接线 +
  E4040 编译器码)均划归 S3-β;字符串值引号转义仅 `\"` 一层(反斜杠原文保留)。
- **门**:smoke 3t 腿四断言(录制 2 用例/复放一致/翻改 expect 点名双值拒/
  seal 覆盖 traces + deep 过验);全量 **198 ok / 0 fail**。

状态:✅ S3-α 完成(2026-10-07)。S3 余:S3-β(FakeFs 效果函数录制/加载期
复放腿接线 + E4040/标量 json 写入器/零参 fn)。S4 余:命名空间 + deps
`blob =` 形。S1b:加速线挂账维持。

## S3-β 首片落地(2026-10-07,T50 批):verify 复放腿 + E5056 + 轨迹面健壮化

- **deep verify 复放腿(§7.2 verify = 校验和 + 复放轨迹,首兑现)**:
  `ctron pkg verify --deep` = ctron-verify native 摘要 + 编排层逐案复放——
  构临时消费工程(deps/<pkg>.ctart 拷入),按轨迹码gen `use <pkg>.<mod>.{fns}`
  消费驱动经 loader 跑(bin/ctron-cc source 模式),工件行为 vs 录制期望逐字
  比对;traces 在场即必复放(fail-closed:缺 ctron-cc 硬失败不静默)。
- **E5056 = 轨迹复放不符**(三面注册:meta_check 码表/conformance 表/diag_msg
  双语模板——spec §5.2 占位 E4040 已被 `#[trusted]` 面占用,实配 E5056;
  当前载体 = verify --deep 工具面,加载期编译器发射腿预留,模板已预置)。
- **陈旧信任证据硬拒**:traces/<mod>.ctrt 的模块不在工件 impl/ = 拒
  (重封未重录的漂移场景强制显式清理——信任工件要求精确)。
- **轨迹面健壮化**:replay 解析改行制(每用例三行 fn/args/expect,read 保空
  字段)——零参 fn 与空 expect 全支持(α 的 tab 切分丢字段病灶拔除);
  编译器诊断走 stdout,失败捕获改 2>&1 合并打印。
- **门**:smoke 3u 腿二断言(deep 复放正例 2 用例全符/行为漂移 vs 旧轨迹
  E5056 精准拒载)+ 3t 顺序修复(篡改步后恢复原值——复放腿上岗后 3t 旧序
  自我拦截);全量 **200 ok / 0 fail**;native 八驱动族重建。
- **D3 论题端到端实演**:行为漂移工件(源改 6 重封)+ 旧轨迹(期望 5)→
  deep verify 拒载,E5056 点名 `five() 记录 5 实际 6`——「确定性重放取代
  信任发布者」首次全链贯通(摘要链 → 轨迹 → 拒载)。

状态:✅ S3-β 首片完成(2026-10-07)。S3 余:S3-γ(FakeFs 效果函数脚本化
录制/复放)、标量 json 写入器(std 能力件)、加载期编译器发射腿。S4 余:
命名空间 + deps `blob =` 形。S1b:加速线挂账维持。

## S3-γ 落地(2026-10-07,T50 批):效果函数轨迹(脚本化 MemFs)全绿

- **机制**:效果函数 = `&Fs` 能力接收者(§8.1 trait Fs: Cap;std.fs.Fs +
  07a MemFs 注入先例)。轨迹块新增 `fs "<path>" = "<content>"` 种子行
  (0..N,机写);录制/复放/verify 复放腿三处码gen 同构:按种子内嵌
  MemFs class+impl(读-only;write no-op=写效果捕获登记),γ 用例主为
  `match <fn>(args) { Ok(v) => println(v.to_string())  Err(_) => println("<err>") }`
  ——Ok 值与 Err 哨兵都进对账(Err/Ok 漂移即 E5056,非"运行失败")。
- **CLI**:`--fs <path>=<content>` 种子挂最近 `--case`(可多对);args 位写
  MemFs 变量名 `fs`(如 `--case 'greet=fs'`);码gen 驱动运行逐命令带
  `CTRON_STDPATH=ROOT/lib/std`(dev 布局 ctron-cc exe 旁 std 解析落空;
  单命令作用域不污染 smoke 环境,避 W8901 雷)。
- **实现警示(血泪)**:python 生成 shell 时转义层会吞 `\"`(TR_INIT 裸值
  实红);管道假 rc 四犯再现(PIPESTATUS 是 bash,zsh 用 pipestatus);
  测试 sed 打点必须锚定唯一行(=`"ada"` 双行命中把期望一起翻了,漂移检验
  自我失效)。sh+ps1 双面同文(ps1 dict-case + regex fs 行扩展)。
- **边界(诚实登记)**:读-only 效果(write 捕获=S3-δ,需 mut 能力形态);
  单 `fs` 变量(多能力参数随 β 后);种子经 CLI 单行(content 不含换行/tab);
  Err 哨兵 `<err>` 字面冲突不复存在场景=返回值恰为该串(概率级,登记)。
- **门**:smoke 3v 腿五断言(γ 录制/复放/种子漂移 mismatch/恢复/deep γ 复放);
  全量 **205 ok / 0 fail**。插曲:一轮 13/9 败集跨无关腿且计数漂移 = 对端
  机刷正于本 worktree 并发 checkout+build+smoke(进程在册实拿),等槽位
  清场后复跑即清——败集跨腿漂移先查并发作业再立案。

状态:✅ S3-γ 完成(2026-10-07)。信任协议(S3)录制/复放/verify 三层全通。
S3 余:S3-δ(写效果捕获)、标量 json 写入器(std 能力件)、加载期编译器
发射腿。S4 余:命名空间 + deps `blob =` 形。S1b:加速线挂账维持。

## S4-① 落地(2026-10-07,T50 批):deps `blob =` 第四来源形 + 源消费方 record 面

- **E5049 扩四形**:dep 来源互斥注册表序 path < git+rev < version < blob
  (pkg_deps_text:同现点名扩至四标签;缺来源文案扩 "| blob");blob 槽 =
  4 槽组 [name, "blob", value, ""]。
- **driver 消解(dep_resolve_one blob 臂)**:E5046 值形态校验(sha256:+64
  小写 hex,dep_blob_check);消费 = deps/<nm>.ctart,artifact self_digest 与
  blob 值对账(缺/未验 → E5052,不符 → E5054 点名双摘要);lock 钉
  source="blob" + digest=blob 值。
- **loader record 面(源消费方诚实边界闭合)**:pkg_load_use_done 工件分支,
  顶层源(ameta="")时读消费方清单 Ctron.ctcl 的 dep blob 记录——有记录即与
  工件 self_digest 对账(不符 → E5054;工件缺 self_digest → E5055);无 blob
  记录 = 记录面缺位,维持 L2 边界不拒。新 fn pkg_meta_dep_field(清单 dep 块
  字段提取泛化,+1 decl,锁 514→515 随批申报)。
- **门**:smoke 3w 腿五断言(blob 正例 t=12/E5054 错摘要/E5049 四形互斥/
  driver 消解 lock 钉定/E5046 值形态);全量 **210 ok / 0 fail**;meta_check
  1 败 = 对端 L4 在飞件(tests/03m_binary_nul_probe.ct 未提交 WIP,git status
  实证,非本批);native 八驱动族重建。
- **D8-2 L2 诚实边界清算**:「顶层源消费方无记录面」自此有解——清单 blob
  记录 = 显式信任锚(§7.2 消费锚三件的 digest 件);t=7 静默错版本洞对
  源消费方同步闭合。

状态:✅ S4-① 完成(2026-10-07)。S4 余:透明日志仓/attest、跨发布者
命名空间。S3 余:S3-δ(写效果捕获)、标量 json 写入器、加载期发射腿。
S1b:加速线挂账维持。

## S4-② 落地(2026-10-07,T50 批):`ctron pkg attest` 发布公证 + deep attest 腿

- **命令面**:`ctron pkg attest <artifact-dir>`(sh+ps1 同文)——生成
  attest.ctcl(§5.4 形):artifact_digest = meta self_digest、toolchain =
  VERSION(dev 布局 "dev" 同 --version 口径)、hosts = ["interp-cc"](α 单线,
  三线一致判定 = 需求方触发件,诚实登记)、trace_count = traces/ 块数实点。
  attest 位于摘要链外(SHA256SUMS 不含——其内容依赖 self_digest,入链即
  循环,同 meta 理据)。
- **deep attest 腿**:attest.ctcl 在场即验 artifact_digest == self_digest、
  trace_count == 实点数——漂移/伪造 = **E5057**(attest 与工件不符;三面
  注册;畸形缺键亦拒)。陈旧公证场景(重 seal 后旧 attest)被双键自然捕获。
- **门**:smoke 3x 腿四断言(生成/deep 过验/trace_count 漂移/伪造 digest);
  全量 213 ok/1 fail(败 = 对端 L4-② WIP 在册红 conc_bytes);decls 锁不动。

## S4-③ 落地(2026-10-07,T50 批):透明日志仓最小面(§7.1 公证层)

- **命令面**:`ctron pkg log append <日志仓> <工件目录>` / `ctron pkg log
  query <日志仓> [名]`(sh+ps1 同文)。记录 = `records/<名>/<digest>/attest.ctcl`
  内容寻址不可变;日志仓 = 本地 git 仓(缺则 init;commit 自带
  ctron-log 身份),append-only:同 digest 重放幂等,同 digest 内容不同
  = 翻改公证,拒(§7.1「只存证据不存内容」;远端同步 = git push 发布方
  自理,零网络协议)。前置:工件须 seal + attest;query 列名/列某名
  digest+公证要点。
- **门**:smoke 3y 腿五断言(追加/幂等/查询/翻改拒/git 历史透明);全量
  **218 ok / 1 fail**(唯一败 = conc_bytes 发射/编译,对端 L4 bytes 域连续
  多轮在册,非本批)。
- **D5 最小兑现**:「git 透明日志取代中心注册表」的公证层本地形态全通——
  append-only、可克隆审计、无中心权威;发现层(静态索引页)与只读代理 =
  §7.3 分层触发件。

状态:✅ S4-②/S4-③ 完成(2026-10-07)。S4 余:跨发布者命名空间。
S3 余:S3-δ(写效果捕获,需 mut 能力形态)、标量 json 写入器(std 能力件)、
加载期编译器发射腿(E5056/E5057 模板已预置)。S1b:加速线挂账维持。

## S3-δ 落地(2026-10-07,T50 批):写效果捕获(脚本化 MemFs 写日志)全绿

- **机制**:γ MemFs 扩写捕获——`log: Box[WSB]`(WSB={s:Str} 累积器);write 臂
  经**别名绕行**(`let inner = self.log; inner.s = ...`;两级直写撞
  「assign target:Member」发射缺口在册)累积 `esc(path) SP esc(data)` 行
  (esc = std json 规范形,单行保证);γ/δ 用例主出 `值行 + ---writes--- 分隔
  + wrote 行`(print 无尾换行,与块行制对齐)。
- **轨迹块新增 `wrote` 行**:`wrote "<jpath>" "<jcontent>"`(esc 形原样存储,
  复放零转义对称);record 解析 stdout(值行/分隔/wrote 行)入块;replay 与
  deep verify 复放腿按「expect + ---writes--- + wrote 序列」正典比对——
  Err/Ok 漂移与写内容/写序列漂移皆入 E5056 对账。
- **record 语义声明化**:每次 record 调用全量重写 .ctrt(多次调用=多次声明,
  后者覆前;多次用例须单调用多 --case)——smoke 3v 首版踩此坑(两次调用后
  断言 2 用例),腿改单调用修正。
- **门**:smoke 3v 腿重构后全绿(单次声明 2 用例/复放/种子漂移 mismatch/
  恢复/deep γ+δ 复放);全量 **219 ok / 1 fail**(唯一败 = conc_bytes,对端
  L4 bytes 域连续在册,非本批)。ps1 trace δ 同文待 Windows conformance 批
  (γ 块在 ps1 面照常;δ 块 ps1 面报无块=响亮降级,登记)。
- **语言面收获**:两级 Box 成员直写缺口(assign target:Member)+ 别名绕行
  实证形入册——mut 能力形态设计(§8 后续)的直接输入。

状态:✅ S3-δ 完成(2026-10-07)。信任协议四层全通:纯函数(α)/verify 复放
腿(β)/读效果(γ)/写效果(δ)。S3 余:标量 json 迁移(trace 值编码切
num/str_json,重录即迁)、加载期编译器发射腿(E5056/E5057 模板预置)。
S4 余:跨发布者命名空间。S1b:加速线挂账维持。

## ps1 trace δ 同文补齐(2026-10-07,T50 批):Windows 面四面归队

- Cmd-PkgTrace/verify 腿 δ 化:MemFs-Prelude 扩 WSB+log 写捕获(别名绕行
  形同 sh)、Case-Main γ/δ 形(match+---writes---+print log)、record stdout
  多行解析(值行/分隔/wrote 行入块;纯函数面仍单行校验)、replay/verify
  正则扩 wrote 组+正典比对(γ 无 wrote/δ 带面双形);esc use 前缀按种子
  有无注入;γ 用例 CTRON_STDPATH 作用域注入。
- ps1 trace/verify 全功能自此与 sh 同文(此前 δ 块在 ps1 面"无块"响亮降级
  的登记销账);Windows conformance 维持环境阻塞挂账。
- 门:全量 smoke(sh 面;ps1 不可测)照常绿基线归因。

## S3 配套件落地(2026-10-07,T50 批):标量 json 规范形 + esc 引号修复 + S3-δ 解禁

- **标量 json 写出器**(std/json.ct,在册能力件销账):`num_json`(I64 十进制
  原文)/`bool_json`(true/false)/`str_json`(= esc 显式别名,语义锚定 S3 轨迹
  契约);test 块锁规范字节。F64 形态未纳(浮点文本格式语言侧未钉,登记)。
- **esc 引号修复(真 std 修复)**:esc 的 `c == 34` 臂曾追加**裸引号**(仅控制
  字符/反斜杠转义)——含引号字符串经 esc/write_json 产出非法 JSON。修为
  `\"`;std 单测绿,write_json 含引号内容自此 RFC 8259 合法。
- **S3-δ 解禁(登记)**:语言前提实测成立——Box 字段经句柄/别名可变
  (`b.v = 5` ✓;两级直写 `w.log.s =` 撞「assign target:Member」发射缺口
  在册,**别名绕行 `let inner = w.log; inner.s =` 实证堆语义可见**)。δ 实现
  = 写捕获 MemFs(Box 内 accrue + esc 化 wrote 行入轨迹块),按下片开片。
- **门**:json 自测 rc=0(新标量测试+esc 回程);全量 **218 ok / 1 fail**
  (唯一败 = conc_bytes,对端 L4 bytes 域在册);meta_check 过;**std 种副
  镜像同步**(std 漂移腿一次红即打回,镜像纪律生效)。