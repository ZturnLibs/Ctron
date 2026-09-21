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
