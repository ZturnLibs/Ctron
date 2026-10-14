# 构建驱动器升格 W5(收编收官)Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 泳道终波:examples 剩余 14 份 run.sh 分类收编(11 份纯构建运行 GUI 例全收编=清单链接面+task.cwd 任务+删除;4 份情景例(ctecho/ctslink/todo_api/todo_app,含就绪门/两相位/持久化断言逻辑)豁免退役归集成门,ctecho 活调用者 smoke.sh 不受扰),examples README 与 website 活指引同步,编排 shell 面终盘报告,spec §8 终验。完成后 examples run.sh 14→4(全豁免),分派任务表 +10(ex-* 族全谱)。

**Architecture:** 收编机械=每例三动作:①清单加 `gui { entry = ... }` + `dependencies { gui }`(ctc.sh:447-452 的 manifest_gui_dep 扫 dependencies 块裸 token `gui` → GUILDFLAGS 全套自动链接,uname 分支已内建——11 例的 33 行链接咒语与 ctc.sh:458-459 逐字同,E5 归一的兑现机制);②根 ctron.ctcl 增 `ex-<名>` 任务(cwd=例子目录,steps=vendor 构建→ctc build→headless env 前缀运行,gui_calc 先例同形);③run.sh 删除+README 改任务面。**情景例豁免**:逻辑(nc 就绪门/env 矩阵/持久化断言)不可为步骤,保留为集成门脚本(与 tests/*/run.sh 同类)。

**Spec:** `docs/superpowers/specs/2026-10-10-build-driver-design.md` §8 W5 行。

## Global Constraints(沿前波,增量三条)

- **gui_cjk 特例先探后收**:其 run.sh 缺 ctron_gui.c 且 -I 无 clay(轻链变体)——`dependencies { gui }` 面会给全套(多链 ctron_gui.c)。先试:全套链接+headless 若绿即收编(冗余符号无害);若冲突,登记分歧该例豁免(不为此扩 ctc.sh 面)。
- **todo_app 路径 bug 顺手修**:其 run.sh 链接路径 `$ROOT/net/c_src`(缺 lib)与 -I 不一致——情景豁免例,但 bug 在删前最后一代修正(脚本保留,修正路径)。
- 对端并发/pathspec/报告命名(`task-w5-{1,2,3}-report.md`)同前;长门后台;不 push(控制器收尾推)。

---

### Task 1: GUI 例收编第一批(6 例:gui_dash/gui_snippets/gui_widgets/gui_contacts/gui_themes + gui_files)

**Files:**
- Modify: `ctron.ctcl`(+6 ex-* 任务)、上 6 例各自的 `Ctron.ctcl`(补 gui 块)与 `README.md`(若有,改任务面)
- Delete: 上 6 例 `run.sh`

**Interfaces:** Consumes: ctc.sh 清单 gui 面(J19-④)、task.cwd(波2)、echo_line 进度行。Produces: ex-* 任务 6 枚;同模板批2 复制此形。

- [ ] **Step 1: 逐例三动作(先 gui_dash 试金,余 5 例同模板复制)**

① 清单:无则 `new` 式最小清单+`gui { entry = <原 run.sh 所用 entry,读源码定> }`+`dependencies { gui }`;有则补两块。② 根 ctron.ctcl 增任务(steps=「mkdir -p build」「sh ../../vendor/gui/build.sh」「sh ../../compiler/ctc.sh build src/main.ct -o build/<名>.bin」「env CTRON_GUI_HEADLESS=1 build/<名>.bin」,gui_files 的 headless 形同;desc 注明真窗手工命令与原 --run 语义,gui_calc 同构)。③ run.sh 删除+README(若有)改任务面。

- [ ] **Step 2: 验证**

每例 `sh compiler/bin/ctron task ex-<名>`(bin/ctron 跑=原生驱动器直接消费,双重身份)rc0+headless 特征输出;`sh tests/drv_parity/run.sh` 不涉(无新驱动器面);`sh tests/dist/ctron_smoke.sh` 绿;ctcl_check 根 ctron.ctcl E=0+fmt 幂等。

- [ ] **Step 3: Commit**

```bash
git add ctron.ctcl examples/gui_dash examples/gui_snippets examples/gui_widgets examples/gui_contacts examples/gui_themes examples/gui_files
git commit -m "refactor(examples): GUI 例收编第一批 6 例(W5 批1)——清单 gui 面(dependencies{gui}=ctc 自动链接,E5 归一)+根任务表 ex-* 6 枚(task.cwd+headless env 前缀步骤)+run.sh 6 删(零外部引用侦察实证)+README 任务面;真窗交互路径 desc 注明;门:六例 ex-* rc0 headless+ctron_smoke 绿+ctcl_check 幂等"
```

---

### Task 2: GUI 例收编第二批 + 特例处理(5 例:gui_counter/gui_cjk/gui_zitie/todo + gui_cjk 特例裁决)

**Files:**
- Modify: `ctron.ctcl`、5 例清单/README、`examples/todo_app/run.sh`(路径 bug 修正)
- Delete: 4 例 run.sh(gui_cjk 视裁决)

**Steps:**

- [ ] **Step 1: 同模板 4 例**(gui_counter[含 CTRON_GUI_STATE=count=7 状态恢复步骤——env 前缀 env(1) 形]/gui_cjk[先探全套链接]/gui_zitie/todo)。

- [ ] **Step 2: gui_cjk 特例裁决**:全套链接试跑;绿=收编;红=豁免登记+run.sh 保留(集成门化)。

- [ ] **Step 3: todo_app 路径 bug 修正**(情景豁免例,删前修:`$ROOT/net/c_src`→`$ROOT/lib/net/c_src`)。

- [ ] **Step 4: 验证 + Commit**(同批1 口径,逐例 rc0)。

---

### Task 3: 情景例豁免 + 文档同步 + shell 终盘 + §8 终验

**Files:**
- Modify: `website/docs/server-guide.md:9`(todo_api 活指引)、`tests/COVERAGE.md`(账面行按需)、`ctron.ctcl`(可选:ex-ctecho 等情景任务委派条目)、`docs/superpowers/specs/2026-10-10-build-driver-design.md`(§8 终验勾)
- Create: `docs/superpowers/specs/2026-10-10-build-driver-w5-close.md`(终盘报告:examples 收编终态表/编排 shell 14 份分类清单[自举 4+安装 2+编译器构建 6+垫片 1+工具 1,自举与安装=地基非债]/tests run.sh 147 份=集成门非编排面/测试协议内建=在册开放项)

**Steps:**

- [ ] **Step 1: 情景例定性**:ctecho(run.sh 保留,smoke.sh:804-806 活调用者零扰)/ctslink/todo_api/todo_app 四例豁免登记;可选 ex- 委派任务条目(steps=["sh run.sh"] 形,给 task 表全谱入口)。
- [ ] **Step 2: 文档同步**:website server-guide:9 改 ctron task 形;9 份 README 已随批1/2 者核净;tour.md gui_counter 句核。
- [ ] **Step 3: 终盘报告落档** + spec §8 W5 行勾验 + 全门终跑(drv_parity 136+ctron_smoke 43+tasks 21+gate 22 步)。
- [ ] **Step 4: Commit + 泳道收官(summit 记录)。**

---

## 执行记录(2026-10-10,W5 三批落库,收官带触发件)

- **批1 `fa668ba1`**:GUI 例收编 6 例(清单 gui 面+ex-* 任务+run.sh 删+README)。**1/6 绿(gui_files,显式 src 装配绕过合成面=任务管线获证);5 例挂对端在飞发射回归**(run(ViewCall) 合成面 rt_window_loop_d 实参码 s/i 失配;三重归因:HEAD 原 run.sh 同挂/W2 验收过的 ex-gui-calc 同挂/净 /tmp 重建复现;窗口=10-09 阶梯绿后 34 compiler/src 提交,对端正飞 driver_emit.ct)。治愈判据归 task-w5-1-report.md;五任务零改等树愈复验。
- **批2:停摆(同触发件)**——gui_counter/gui_cjk/gui_zitie/todo 四例 run.sh 原样在库,治愈后照批1 模板收编(简报模板已定)。
- **批3 `2fa58fd7`**:情景例豁免登记(ctecho[smoke.sh:804-806 活调用者零扰实证]/ctslink[BLOCKED 软门与任务信号语义冲突]/todo_api/todo_app[env 矩阵两相位])+委派条目 ex-ctecho/ex-todo-api(ex-* 族 8→10)+终盘报告落档(`2026-10-10-build-driver-w5-close.md`:16 例终态表 2+6[5 停]+4 豁免+4 停;编排 shell 14 份五族=地基非债;tests 147=集成门,测试协议内建=开放项)+spec §8 W5 行诚实勾验。评审 Approved(全部量化断言对树复现:4446 行/147 run.sh/10 ex-*/8 残余 run.sh)。
- **终态门**:drv_parity 136/0+ctron_smoke 43/0+tasks 21/0+fast 2-4 绿(步1=对端 meta_check 13 处在库债);gate 22 步归 CI。
- **触发件(泳道唯一开放项)**:对端 run(ViewCall) 合成面回归治愈后 → 批2 四例照模板收编 + 批1 五任务复验(零改)→ spec §8 全勾。
- **泳道总账(W1-W5)**:W1 任务面+全量门+ci.sh 翻转;W2 task.cwd+试点收编+私清缓存扫描;W3 三前置件;W4 三波=驱动器 19 臂 Ctron 自写+**垫片翻转成立(真终审 Yes)**;W5 收编收官(带触发件)。examples run.sh 14→8(6 删+4 批2 在库);编排 shell 14 份=地基五族;对拍语料终态 136。
