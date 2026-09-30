# 库根布局与包分区(lib/ + pkgs/)——设计方案

> 状态:**已实施(2026-09-30;用户裁决:A′ 选型 + T2 随发;W1 loader 712e98b / W2 迁移 04ff0d7 / W3 分发 4c236bd / W4 验收=装机全回路三例绿+ci 九门;实施计划 plans/2026-09-30-libroot-layout.md)**
> 上游:2026-09-23-domain-namespace-design.md(域包仓库根平铺=过渡形态,本稿升级为
> 终态)、2026-09-23-std-tiering-design.md(三档宪章 v2)、2026-09-17-toolchain-
> distribution-design.md(安装布局)、2026-09-21-closed-pkg-distribution-design.md
> (.ctart/registry 通道)。
> 触发:用户裁决「根目录乱——标准库与扩展包收拢;路径去重(三处 ctron 冗余);
> 部分包独立发布、用户安装后才有」。

## 0. 目标形态

四条不变式:

1. **语言名在每条自包含路径中至多一次、且只在根**(`~/.ctron/…`;
   系统级将来 `/usr/local/ctron/…` 同理);
2. **每段语义唯一**:bin 可执行 / lib 随发库 / pkgs 用户装包 / cache 可再生产物;
3. **lib 与 pkgs 分离 = 「语言的面」与「生态的面」分离**(≈ rustlib vs cargo
   registry,但无 rustlib 那层仪式):包永远不污染 lib/,卸载=删目录,升级语言
   不动 pkgs/;
4. **物理位置即分发政策**:住 lib/ = 随语言发布,住 pkgs/ = 独立发布用户安装
   ——收拢与独立发布统一为同一坐标系,git mv 即改政策。

| 分区 | 仓库 | 装机 | 内容 | 分发 |
|---|---|---|---|---|
| lib/ | `Ctron/lib/{std,net,http,tls,db,ffi}` + 门面 | `~/.ctron/lib/…` | T1 核心 + T2 五域(lib/ 六目录) | 随 tarball(用户裁决:T2 随发) |
| pkgs/ | `Ctron/pkgs/{web,gui,s3,pkg}` | `~/.ctron/pkgs/<名>` | registry 上游四包 | 独立发布,用户安装 |
| vendor/ | 仓库根顶层(不动) | `~/.ctron/vendor/<域>`(仅随发域所需子树:deflate/tls) | 第三方 C | 随需入 tarball |

- std/ 全量原样迁入 `lib/std/`(模块+config.ct+README);门面归位:根级
  `net.ct tls.ct ffi.ct` → `lib/`、`gui.ct` → `pkgs/`(门面双形态登记不修,§4.7)。
- **web 归 pkgs/**:应用层框架非基础传输面,且已具 `Ctron.ctcl` 清单=registry
  形态先行;若复审欲随发,一行 git mv 可改。
- 装机根:`~/.ctron/{bin,lib,pkgs,vendor,share/doc}`;`cache/` 布局预留不落地
  (AST 缓存 §10.1 触发时启)。

## 1. Loader 规则(小步)

- **②探测串去重**:`../lib/ctron/std` → `../lib/std`,三处:parse_pkg.ct
  `pkg_std_installed`(L278 附近)与 `pkg_std_root`(L294)、driver_doc.ct(L645);
  两文件镜像同步改。
- ③回落 `dir/../std` **一字不动**(vendored 语义,自带快照项目零扰动);
  `CTRON_STDPATH` 变量名与语义(std 子目录锚)不动。
- **libroot = dirname(std 根)规则原文不动**(效果:仓库根 → `lib/`)。
- **site root 新增**:非 std 首段解析链 = 目录相对 → libroot → **site root** →
  ctart 保底(保底位不动;插入槽与 3bcc61e 域根同位)。site root 取值:
  ① `CTRON_SITEPATH` ② `dirname(libroot)/pkgs`(同级推导)。repo 开发零新
  env——STDPATH 指 `lib/std` 后自动得 `$ROOT/pkgs`;默认装机 = `~/.ctron/pkgs`。
  段映射镜像域根分支(`<首段>[/<次段>…].ct` + 门面同名)。
- caps 推广(net/db)/静默口径/W8901 口径全部不动。
- **E2020 带出路(本波纳入)**:不动解析层静默;增强 E2020 报文(落点 sem/驱动
  层,实现计划定位)——非 std 首段全链未命中时附解析链摘要与安装出路
  (「包 `<seg>` 未安装;试 `ctron pkg add <seg>`」形;命令名以 registry CLI
  现名 tools/ctpkg.ct 为准,措辞实现期定)。验收负例:未装 web 时
  `use web.core.*` → 报文含安装出路与解析链摘要。
- inline 实现纪律:site root 探测**零新增 decl**(3bcc61e 先例),decl 锁按
  当时基线核对。
- 自举链:loader 变更 → native.sh 重建 + prebuilt 固定点重发(W1 内完成,
  共同成本)。

## 2. 物理迁移与消费面(git mv 保历史)

- 迁移清单:`std/ → lib/std`;`net/ http/ tls/ db/ ffi/`(+根级门面三件)
  `→ lib/`;`web/ gui/ s3/ pkg/`(+门面 gui.ct)`→ pkgs/`;`vendor/` 不动。
- 脚本面:**82 处 CTRON_STDPATH 脚本** `$ROOT/std` → `$ROOT/lib/std`(sed
  机械);域路径直引(net/c_src 等)同波;夹具符号链接 21 处重指。
- 种副:`compiler/test/stdpkg/std/` 结构不动,同步/比对脚本源路径改
  `lib/std`;smoke 逐字节断言语义不变。
- 宪章 v2.2:`lib/std/README` 物理条款改写(lib/=std+五域随发、pkgs/=registry
  上游、安装布局 `~/.ctron/{lib,pkgs}`、拆分触发条件① gui 物理预落位);
  种副 README 镜像同步;docs/spec 示例 + 文档站(sync_site_spec.sh)。
- 清噪:.gitignore 增补(`imefix*.bin`、`ul_*.log`、`.DS_Store`、examples
  产物形态)+ rm 三件(imefix-151438.bin、ul_out.log、ul_err.log)。
- **前置对齐(硬前置)**:并行泳道在制文件先落库或明确归属(当前
  compiler/src 五件在制);本波全程 pathspec 限定提交;W2 触碰 gui.ct 前确认
  GUI 泳道无未落库改动。

## 3. 分发面

- release.sh:`cp -R lib/ → $PKG/lib/`(std+五域+门面);pkgs/ 不入 tarball;
  `$PKG/vendor/` 仅携所发域所需子树(deflate、tls);src 件同构。
- install.sh:去内层 `$DIR/ctron`(tarball 内层目录仅作解包卫生);
  `CTRON_INSTALL_DIR` 语义 = **工具链根本身**;PATH 提示 `$DIR/bin`;检测旧
  布局 `~/.ctron/ctron` 打印清理提示(**不自动删**,诚实边界)。
- Makefile:源码线 `$(DEST)/lib/ctron/std` → `$(DEST)/lib`。
- ctc_smoke 扩三件:装机态 hello(use std.x)/ use net.x(libroot ② 路径)/
  模拟 site 包命中(pkgs)。

## 4. 决策记录

1. **A′ 选型**(用户 2026-09-30):lib/+pkgs/ 双区、路径去重。09-23「pkg/
   子目录否决」理由(多一层无收益)随域包 6→9、CTCL registry 临近而失效;
   本稿为该裁决的**升级延续**而非推翻——「去 std 层、安装布局同构」两诉求
   均保持且更彻底(物理相同)。
2. **T2 随发**(用户 2026-09-30):tarball 含 std+net/http/tls/db/ffi;ffi 未
   入宪章 T 档表,按 T2 口径随发(C 宿主互操作面,语言邻接)。web 归 pkgs/
   (§0;复审可否)。
3. **site root 锚定修正**(对讨论稿「恒定 ~/.ctron/pkgs HOME 锚」的修正):
   装机目录定名 `pkgs/`(与仓库同名,单规则推导;避开与 pkg 包同名混淆),
   取值 = CTRON_SITEPATH 覆盖 + dirname(libroot)/pkgs 同级推导——repo 零新
   env、默认装机两说重合;多用户系统级安装未到(YAGNI),CTRON_SITEPATH 为
   逃生口。
4. **vendor/ 保持仓库根顶层**(构建期 -I/-link 面,不属 loader 世界);
   tarball 仅携所发域需要的子树。
5. **E2020 带出路纳入本波**(cold-start 配套)。
6. **硬切文化**:旧 `~/.ctron/ctron` 用户重跑 install.sh;旧路径不命中响亮
   (W8901/E2020);pre-1.0 口径,与 e3bc073 同款。
7. **门面双形态登记不修**(根级 vs 目录内,既有小债,非本波范围)。

## 5. 用户面影响摘要

语言面零感知(use/命令/环境变量不动);既有装机用户一次性成本 = 重跑
install.sh + 改一条 PATH + 清残留目录(响亮失败,无静默错);长期收益 =
包可装卸、语言与包独立升级、报错路径可读、E2020 自带出路。vendored/离线
项目完全不受影响(解析链第一优先)。

## 6. 明确不做

- 不做 `ctc pkg` 子命令面与 registry 协议(registry 泳道所有,E2020 只指路);
- 不动 ctart/deps 语义与 realdep/S2a 优先级(保底位不动);
- 不做多用户系统级安装 per-user site 锚(触发未到);
- 不动 Rust/C 臂源(Rust 面无 std/域解析;C 种随自举链重建自然同步);
- 不做 cache/ 目录落地;不动 ③ vendored 回落;
- gui 分发通道沿用 09-23 终结条款(registry 触发条件照旧,物理先落 pkgs/)。

## 7. 波次与验收

- **W1 loader 小步**:②串三处 + site root 探测 + E2020 增强 + 正负例
  (目录相对优先/libroot 命中/site 命中/ctart 保底/realdep 零扰动/W8901
  口径不变)+ native.sh 重建 + decl 锁核对(零新增纪律)。
- **W2 物理迁移**:git mv 全清单 + 82 脚本 sed + 夹具/种副/文档路径 + 清噪;
  前置 = 在制泳道对齐。
- **W3 分发面**:release.sh/install.sh/Makefile + ctc_smoke 扩三件 +
  宪章 v2.2 + 文档站。
- **W4 全量验收**:ci.sh 门禁全绿 + tests/{net,http,web,gui,s3} 关键腿 +
  smoke(含种副逐字节)+ 真装机烟测(install.sh → ctc run hello,含
  use std.x/net.x)。
- 分波 pathspec 落库,每波全绿即提交。
