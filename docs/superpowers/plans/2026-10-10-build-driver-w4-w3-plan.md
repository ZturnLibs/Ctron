# 构建驱动器升格 W4 波3(pkg 族/T49 面/垫片翻转/install)Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 驱动器补齐全部残余子命令(new/add/publish/lock/bench/pkg 五族),然后**垫片翻转**(根 ctron 脚本降为薄垫片:bin/ctron 在场即 exec,缺件回落 sh 实现)、install 布局接线、自举梯接线;真终审 = 翻转合并审查。完成后驱动器分派面 11→19 臂全谱,对拍语料预计 88→130+。

**Architecture:** T49 面(add/publish/lock)= manifest 行扫 + registry 目录操作 + sha256_hex(W3 已在库双宿)+ ctron-dep 委派;pkg 族 = 编排面迁移(seal=ctron-ast spawn+SHA256SUMS+self_digest;verify=校验和重算+--deep 委派 ctron-verify+轨迹复放;attest=meta 读+attest.ctcl 生成;log=git 命令 spawn;**trace=本波最大单件**——MemFs 码gen 逐字迁移,sh 生成驱动 .ct 的字符串合成全数复刻);翻转 = sh 头部三行哨兵(bin 在场且可执行即 exec)+ ctron_smoke :28 指针翻转整门复用。

**Spec:** `docs/superpowers/specs/2026-10-10-build-driver-design.md` §7 分批尾段 + 垫片翻转/install/自举梯三节。

## Global Constraints(沿前波,增量三条)

- **翻转裁决(在册)**:发射模板 raw argv 逃生口(编译器泳道)在对端冻结期不可落——翻转以 native.sh 发射后剥臂为在册机制运转(哨兵 fail-loud+语效双绊网已历三波验证),模板逃生口降级为编译器泳道改进项(可后置替换剥臂)。此裁决随批呈报,可否决。
- 对拍即验收:每批语料全过 drv_parity;pkg/registry 族新增夹具 = 唯一目录现造现清(双版各自 registry/工件目录,CTRON_REGPATH 两侧分设)。
- 翻转批特殊门:翻转后 `sh tests/drv_parity/run.sh` 语义改变(SHREF 仍指 sh 实现,NAT 仍指 bin/ctron——**对拍门在翻转后依然有效**,因垫片回落面=sh 实现原样保留);ctron_smoke 换 :28 指针后整门跑;`tests/tasks`/`tests/drv_parity`/fast 双面全绿;dev+install 双布局冒烟(install.sh 装机仿真)。
- 对端并发/pathspec/报告命名(`task-w4-w3-{1..4}-report.md`)同前;不 push(控制器收尾推)。

---

### Task 1: new + add + publish + lock(T49 面)

**Files:**
- Modify: `compiler/driver/src/main.ct`、`tests/drv_parity/run.sh`

**Interfaces:** Consumes: exe 定位/proc 面/fs 面/sha256_hex(publish 用)。Produces: 四臂。

- [ ] **Step 1: 四臂(镜像 sh 现文,grep 定位:cmd 新=new 分派臂/add 分派臂+add_validate/publish 分派臂/lock 分派臂)**

- `new <dir>`:mkdir -p dir/src + 清单写入(pkg 块 printf 同文)+ hello main + echo 行。
- `add <名>[@<约束>]`:add_validate 白名单校验(名字/约束字符集,消息逐字)→ 清单在场守卫 → registry 探查(CTRON_REGPATH 或 ~/.ctron/registry)→ 版本解析(无约束取最大/前缀匹配——sh 的两段 case 逻辑)→ dep 块追加(printf 同文,已存在跳过行)→ pkgs/ 安装(cp module/manifest)→ ctron-dep 刷新(缺件 die2 逐字)。
- `publish`:清单在场/name/version 提取/严格 semver grep → 单文件包守卫(src/main.ct 唯一)→ 版本不可覆盖 → registry 三件落( manifest/module/sha256——**sha256 用 std/crypto sha256_hex**,sh 用 shasum;**字节一致性**:sha256_hex 与 shasum -a 256 对同一文件必同摘要,语料钉之)+ echo 行。
- `lock`:清单守卫 + ctron-dep 缺件 die2 + spawn ctron-dep run Ctron.ctcl。

- [ ] **Step 2: 语料 + 验证 + Commit**

drv_parity 增(~12 行):new 脚手架产物 diff -r、add 全链(先 publish 造 registry 再 add,双版各自 CTRON_REGPATH)、add 重复 dep 跳过行、add 版本约束/非法名 rc2、publish 全链+sha256 文件逐字、publish 重复版本 rc2、非 semver rc2、lock 缺 ctron-dep 场(临时 PATH 手法或登记)、lock 正例。跑:drv_parity + ctron_smoke(T49 组 ⑫ 是现成第二 oracle)。

```bash
git add compiler/driver/src/main.ct tests/drv_parity/run.sh
git commit -m "feat(driver): new/add/publish/lock T49 面(W4 波3 批1)——脚手架/校验白名单/registry 版本解析与安装/严格 semver+sha256_hex 双源摘要一致钉/dep 追加与 ctron-dep 委派;语料+12(registry 双版 CTRON_REGPATH 分设);门:drv_parity 全绿+ctron_smoke 绿"
```

---

### Task 2: bench + pkg verify/attest/log/seal

**Files:**
- Modify: `compiler/driver/src/main.ct`、`tests/drv_parity/run.sh`

**Interfaces:** Consumes: sha256_hex、run_argv_rc、fs 面。Produces: 五臂。

- [ ] **Step 1: 五臂(镜像 sh 现文:cmd_bench/cmd_lint 同区定位;cmd_pkg_trace/attest/log/seal 各 fn)**

- `bench [族]`:注册表五族路径表 + 无族清单输出逐字 + 未注册 die2 逐字 + spawn `sh <族脚本>`。
- `pkg verify <dir> [--deep]`:①形态面四守卫逐字 ②SHA256SUMS 逐行重算(行解析/路径越界守卫/逐成员 sha256_hex 对账)③self_digest 重算 ④判决行逐字;`--deep` = 委派 ctron-verify + 轨迹复放腿(**复放腿若体量超批,可先落 --deep=委派 ctron-verify+复放账登记移批C**——trace 复放与 C 批共用机械,裁决随实现报)。
- `pkg attest`:meta.ctcl 读 artifact/self_digest + traces 计数 + attest.ctcl 生成(printf 同文)+ VERSION 读。
- `pkg log append/query`:git init/add/commit 委派(run_argv_rc)+ records 目录操作 + 幂等/拒绝翻改守卫逐字 + query 输出逐字。
- `pkg seal`:ctron-ast spawn(env 通道 CTRON_AST_MODE/OUT/NAME)+ traces 拾取 + SHA256SUMS 生成(排序+逐成员摘要)+ self_digest 注入(awk 插行逻辑→字节扫重写)。

- [ ] **Step 2: 语料 + 验证 + Commit**

drv_parity 增(~14 行):bench 无族清单/未注册 rc2;seal 全链(源→工件目录 diff -r+SHA256SUMS 逐字+meta 自摘要行);verify 轻面全态(好工件/缺成员/摘要篡改/路径越界);attest 生成逐字;log append+query 全链(双版各自日志仓 git 身份一致注意——sh 用 -c user.name=ctron-log,照抄);verify --deep 有轨工件(若复放腿已落)。跑:drv_parity + ctron_smoke。

```bash
git add compiler/driver/src/main.ct tests/drv_parity/run.sh
git commit -m "feat(driver): bench+pkg verify/attest/log/seal(W4 波3 批2)——bench 注册表/verify 校验和三段判决+seal ctron-ast env 通道+SHA256SUMS+self_digest 注入/attest 生成/log git 委派与 append-only 守卫;--deep 复放腿随 C 批或本批(随实现报);语料+14;门:drv_parity 全绿+ctron_smoke 绿"
```

---

### Task 3: pkg trace record/replay(MemFs 码gen)

**Files:**
- Modify: `compiler/driver/src/main.ct`、`tests/drv_parity/run.sh`

**Interfaces:** Consumes: proc.run 捕获(ctron-cc run 驱动码)、fs 面、码gen 字符串合成。Produces: trace record/replay 全语义(黄金轨迹录制=值 to_string 规范形+--fs 种子 MemFs 脚本化+写捕获;复放=逐字比对+E5056 点名)。

- [ ] **Step 1: 码gen 迁移**

sh cmd_pkg_trace 的驱动码合成逐字迁移:manifest 行制解析(--case/--fs 参数循环)/WSB+MemFs 类合成(字段声明/read 分支链/exists 链/log Box 累积——转义层层:`\\"` 在 Ctron 串字面量内的表达)/expect 提取/wrote 段落。**逐字节对拍锚:同源同参数,双版生成的 drv.ct 必须逐字节一致**(这是本批最强验收, drv_parity 夹具直接 diff 产物文件)。

- [ ] **Step 2: record/replay 臂 + 语料 + Commit**

record:用例循环+单值守卫(多行输出拒)+轨迹块写(printf 同文);replay:轨迹块解析(awk→字节扫)+复放+E5056 mismatch 判词逐字。语料(+8):record 纯函数单例/--fs 种子用例/wrote 面断言/多用例;replay 正例/篡改 expect E5056/运行失败 rc;**drv.ct 产物逐字节 diff 语料 ×2**。

```bash
git add compiler/driver/src/main.ct tests/drv_parity/run.sh
git commit -m "feat(driver): pkg trace record/replay(W4 波3 批3,最大单件)——MemFs 码gen 逐字迁移(WSB/MemFs 合成+转义层),drv.ct 产物逐字节 diff 入语料;record 单值守卫/轨迹块写/replay E5056 判词;语料+8;门:drv_parity 全绿+ctron_smoke 绿"
```

---

### Task 4: 垫片翻转 + install 布局 + 自举梯 + 真终审

**Files:**
- Modify: `ctron`(头部哨兵)、`tests/dist/ctron_smoke.sh`(:28 指针)、`Makefile`、`install.sh`/装机仿真面、`selfhosted/bootstrap.sh`、`tests/drv_parity/run.sh`(全谱语料解锁)

**Steps:**

- [ ] **Step 1: 翻转**:根 ctron 头部(native.sh 剥臂同款哨兵语义)——`BIN 下 bin/ctron 在场且 -x 即 exec`(带 `--w4-shim` 逃生旗标?不需要——回落路径=缺件自然回落;sh 实现全量保留为回落体)。**翻转后 drv_parity 语义核验**:SHREF 指垫片=经 bin/ctron 与 NAT 汇合——对拍门退化为自比;改 SHREF 指 sh 实现保留体(垫片内 sh 路径),对拍持续有效(垫片回落面 vs 原生)。
- [ ] **Step 2: 整门复验**:ctron_smoke(:28 指针翻转为 bin/ctron)全门;tests/tasks 21;tests/drv_parity 全谱;fast 双面;smoke --full 基线零差。
- [ ] **Step 3: install 布局**:Makefile prebuilt 家族加 driver 发射 C(ctron.c)+ ctron_proc.c 链;install.sh 复制 bin/ctron;装机仿真冒烟(ctron_smoke ⑨ 的布局仿真段)。
- [ ] **Step 4: 自举梯**:bootstrap.sh 阶梯加驱动器一级(与编译器同梯)。
- [ ] **Step 5: 真终审(控制器派全分支终审代理)**:范围=波1+波2+波3 全部驱动器提交 vs sh 实现;翻转合并审查(本泳道真正 merge gate)。

```bash
git commit -m "feat(driver): 垫片翻转+install+自举梯(W4 波3 批4)——根 ctron 头部哨兵 exec bin/ctron 缺件回落 sh 实现全量保留;ctron_smoke 指针翻转整门复用;drv_parity SHREF 改指回落面持续对拍;Makefile prebuilt+ctron_proc.c 链+install 复制;bootstrap 阶梯加驱动器级;门:全门绿+双布局冒烟+真终审"
```

---

## 执行记录(2026-10-10,W4 波3 四批全落库过审,翻转成立)

- **批1 `dcee7442`**:new/add/publish/lock T49 面。对拍 88→110;sha256 文件双钉(树 diff+echo 行);数字版本序活体探针(0.10.0>0.9.0)。评审 Approved(15 语料钉+6 手核消息全等)。
- **批2 `7dec1ea0`(WIP 保护)+`6a61803f`(收尾)**:bench+pkg verify/attest/log/seal。执行口径=实现子代理限额墙死亡遗 +926 行完整实现 → WIP 保护落库(防 stash 卷带)→ 收尾代理写语料 14 行,**语料抓活修 driver 四处**(bench 分派臂死码/seal 无参用法/log rev-parse execv 无 PATH→find_in_path 仅捕获腿/seal self_digest 尾空行 isEnd 空段)。对拍 110→**124**;git 日期钉(GIT_AUTHOR/COMMITTER_DATE 同值=同树同短哈希)。评审 Approved(四修复全对 oracle 验,语料全判别)。
- **批3 `a595d07d` + 登记 `efa44da3`**:pkg trace record/replay(MemFs 码gen,最大单件)。drv.ct 码gen dev-time 与 sh 金样 4 形态 cmp 逐字;单 `trace_build_drv` 双臂消 record/replay 码gen 偏斜险;三层转义图码注。**计划纠错**:E5056 前缀属 verify --deep 腿,trace replay 判词无前缀(oracle 文本为准)。对拍 124→**136**。评审 Approved + 两 Important 镜像或登记裁决=**登记**(fs 种子泄漏 quirk[sh NF≥1→NF=0 混序泄行,native 正确侧]/多行 --fs 内容[sh 静默截断坏轨迹 vs native E1001 响亮拒])→ `efa44da3` 码注+报告双录。
- **批4 `e5250b66` 垫片翻转**:根 ctron 头部 18 行哨兵(三定位:dev compiler/bin/ctron→源码线 bin/ctron→装机 ctron-drv;CTRON_SHIM_SH=1 强制 sh 面=对拍 stable handle+即时回退),sh 体 0 删行逐字节不动(cmp 证);drv_parity SHREF 强制回落面(翻转后门依然真实);Makefile prebuilt 家族+ctron_proc.c 链+bootstrap 阶梯段(管线实测绿)。门:136/0+43/0 经垫片+21/0 原生+fast 双面逐字+回退探针双布局。
- **真终审(翻转合并审查)**:**Ready to merge — Yes**。哨兵三布局 marker 探针/回退完整性 cmp 证/exec 无损 trace 证/CTRON_SHIM_SH 先于 exec/对拍门仍 sh-vs-native 非自比;全门终审者亲跑;债 triage 全 RIDE,**唯一硬条件=tools/release.sh 须在下一次发布切割前补 prebuilt/ctron.c 发射**(Makefile all/install 已硬依赖;登记归发布泳道)。
- **挂账终录**:①tools/release.sh 驱动器发射(发布切割前硬条件);②发射模板 raw argv 逃生口(编译器泳道改进,剥臂在册运转);③-w 捕获截断面(波2 捕获面正解);④ps1 Dv-EnvSetup(Windows CI);⑤meta_check 13 处对端在库红;⑥ex-* rc0 复验;⑦W4波3 Minor 十余枚(trace 两分歧登记/三病理角消息分歧/find_in_path X_OK/Log rc 末轮等)。
- **W5 触发件**(spec §8 尾波):gui 链接面归一(残余 examples run.sh 迁移)+残余 run.sh 退役+shell 面 ≤10 终盘。
