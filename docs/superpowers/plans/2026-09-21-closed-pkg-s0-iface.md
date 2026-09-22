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