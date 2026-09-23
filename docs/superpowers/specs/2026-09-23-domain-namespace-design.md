# 域包顶层命名空间(gui.* / net.* / http.* / tls.* / db.*)——设计方案

> 状态:**已核准(2026-09-23,用户裁决:硬切/仓库根平铺/小步时序),执行中**。
> 上游:2026-09-23-std-tiering-design.md(三档宪章;本稿 = 其 v2.1 物理与命名
> 落地)。触发:用户裁决"use gui.xxx 而非 use std.gui.xxx;非核心 std 包去掉
> std 这一层"。

## 0. 目标形态

| 命名空间 | 内容 | use 形态 | 物理位置 |
|---|---|---|---|
| `std.*` | 仅 T1 核心(20 模块) | `use std.json.{parse}`(不变) | `std/`(现状) |
| 顶层域 | gui/net/http/tls/db(T2/T3) | `use gui.{run_kb}`、`use net.bind.{...}`、`use db.pg.{...}` | 仓库根 `gui/ net/ http/ tls/ db/`(+各门面文件),与 `std/` 平级 |

域目录物理迁移后与安装布局 `lib/ctron/{std,gui,net,http,tls,db}` 1:1;宪章三档
边界物理可见,std 目录内只剩 std——tiering 的口径混居债物理清零。

## 1. Loader 规则(parse_pkg.ct;最小改动)

现状:非 std 首段 use 已有解析分支 = **目录相对**(dir + /segs[2..] + .ct)+
**deps ctart 回落**(S2a,realdep 测试依赖);`stdweb` 特判保持。

新增三级链(非 std 首段,目录相对未命中后、ctart 之前插入):

3. **域根**:libroot(= std 根的父目录,pkg_std_root 通用取 dirname)下
   `<libroot>/<segs[1]>/<segs[2]>/…/<末段>.ct` 命中则用之。镜像 std 分支的
   段映射:`gui.{x}` → 库根/gui.ct(门面);`gui.parse.{x}` → 库根/gui/parse.ct;
   `net.bind.{x}` → 库根/net/bind.ct。

配套:
- **caps 推广**(E4010):cap 键提取从 `segs[1]=="std" && cap=segs[2]` 推广为
  `segs[1] ∈ {net,db}` 时 cap=segs[1](gui/http 无 cap 键,现状保持);
- **静默口径**:域未命中保持现有静默(与 std ③回落一致),最终 E2020 兜底;
- **CTRON_STDPATH 语义升级为"库根下 std 子目录锚"**:env 名不改(脚本零扰动),
  域根 = 其父目录;③回落路径同理推父。

三宿主:自举 parse_pkg.ct(本波)+ compiler-rust interp.rs(第二波——peer 在制
文件,避碰;间隙内 Rust 面无域包消费:smoke 3j3 仅 std/*.ct 核心,核心零 use)。
seed(compiler-c)不解析 use(use 合并在 cc_run 驱动层),零改动。

## 2. 物理迁移与消费面

- `git mv std/{gui,net,http,tls,db}.ct 与同名目录` → 仓库根;std/ 只剩 T1 核心
  + README.md;
- 脚本路径修正:`std/net/c_src`→`net/c_src` 等约 50 处(smoke 4c、tests/net、
  bench、examples run.sh、-I/-link 参数);
- 消费面机械切换:`use std.gui.`→`use gui.` 等 44+ 处(examples/tests/规范示例
  /文档站);类型名(StdNet 等)与 use 无关不动;
- 副本处置:种副 stdpkg/std 剪除域目录/门面(种副=核心消费种子);examples
  钉定快照中域包副本改挂新位置;
- 文档:宪章 v2.1(std/README 注册表加"命名空间"列;域包物理位置条款)、
  docs/spec/11-net.md 与 12-db.md 示例、文档站。

## 3. 波次与验收

- **W1** loader 域根分支(inline 实现不增 decl,避 decl 锁二次扰动)+ caps
  推广 + 解析正负例(域命中/目录相对优先/ctart 保底/realdep 零扰动)+
  native.sh 重建(依赖地板可编译;lex.ct peer 在制,失败即blocked 上报)。
- **W2** 物理迁移 + 脚本路径 + 种副剪除。
- **W3** 消费面 sed + 宪章 v2.1 + 规范/文档站示例。
- **W4** 验收:tests/http(本波前 13 环境红归属不变)、tests/net 关键腿、
  smoke 快面、compiler decl 锁(现值 361 系 peer 在制扰动,本片 inline 实现
  不再增减;锁值修正归地板收敛波);分波 pathspec 落库。

## 4. 决策记录

1. **硬切**:不留 `use std.gui.` 兼容别名(pre-1.0;别名=双径永久债);44+ 消费
   点一波切完,旧形态经 ctart/目录相对均不命中 → E2020 响亮。
2. **仓库根平铺**:与安装布局 1:1;备选 pkg/ 子目录否决(多一层无收益)。
3. **时序**:parse_pkg.ct 无人在制即动;lex.ct/trans_emit.ct/interp.rs 在制
   文件不碰;Rust 臂第二波。

## 5. 明确不做

deps 工件(ctart)解析路径与语义不动;CTRON_STDPATH 变量名不改;T1 核心
use 路径一字不变;gui 迁出"分发通道"问题由本方案终结(gui 挂顶层命名空间,
仍是库随发形态,CTCL 注册表迁移触发条件照旧)。
