# 构建驱动器 W5 收编收官 —— 终盘报告

> 2026-10-14。spec:2026-10-10-build-driver-design §8 W5 行;plan:2026-10-10-build-driver-w5-plan。
> 三批执行态:**批1 落库**(fa668ba1,6 例收编,1/6 任务绿、5 任务停存量触发件)、
> **批2 未启动**(同触发件在册,4 例 run.sh 在库原样)、**批3 = 本报告**(情景豁免+文档核+shell 终盘+§8 终验)。
> 诚实口径:触发件未愈,本报告不含对停摆任务的胜利表述;治愈判据与证据链见 §5 与 task-w5-1-report.md。

## 1. examples 收编终态表(16 例)

| 例 | 波次 | 处置 | 任务表入口 | run.sh |
|---|---|---|---|---|
| gui_calc | W2 批2 | 收编落库;W2 时点验绿,**触发件窗内同败步骤 3**(回归落在 W2 验收之后,恰证非批1 引入) | `ex-gui-calc` | 删 |
| todo_v10 | W2 批2 | 收编落库(§10.3 合成面;linux 链接臂归 W5 链接面归一,未闭) | `ex-todo-v10` | 删 |
| gui_dash | W5 批1 | 收编落库;**任务停触发件**(正确走到发射位,治愈后零再编辑即绿) | `ex-gui-dash` | 删 |
| gui_snippets | W5 批1 | 同上 | `ex-gui-snippets` | 删 |
| gui_widgets | W5 批1 | 同上 | `ex-gui-widgets` | 删 |
| gui_contacts | W5 批1 | 同上 | `ex-gui-contacts` | 删 |
| gui_themes | W5 批1 | 同上 | `ex-gui-themes` | 删 |
| gui_files | W5 批1 | 收编落库,**rc=0 五步全绿**(显式 src 面绕开合成链,触发件不涉) | `ex-gui-files` | 删 |
| gui_counter | W5 批2 | **未启动**(触发件在册);run.sh 在库原样(含 CTRON_GUI_STATE=count=7 状态恢复步) | 未建 | 存 |
| gui_cjk | W5 批2 | 未启动;轻链特例先探后收的裁决项同悬;run.sh 在库原样 | 未建 | 存 |
| gui_zitie | W5 批2 | 未启动;run.sh 在库原样 | 未建 | 存 |
| todo | W5 批2 | 未启动;run.sh 在库原样(M1-e 演绎,--run 真窗形) | 未建 | 存 |
| ctecho | W5 批3 | **情景豁免**(集成门,§2) | `ex-ctecho`(委派入口) | 存 |
| ctslink | W5 批3 | 情景豁免;留脚本面 | — | 存 |
| todo_api | W5 批3 | 情景豁免(集成门,§2) | `ex-todo-api`(委派入口) | 存 |
| todo_app | W5 批3 | 情景豁免;留脚本面 | — | 存 |

面外 4 例(ctgrep/ctwc/ctwf/web_todo)无 run.sh,vendored/文档面,不入 W5 账。
**净账:run.sh 14→8(6 删+8 存:4 豁免+4 批2 停摆);分派任务表 ex-* 8→10(+ex-ctecho/ex-todo-api 委派入口)。**

## 2. 情景豁免登记(4 例,逻辑非步骤)

| 例 | 不可步骤化的逻辑 | 委派入口裁决 |
|---|---|---|
| ctecho | nc 就绪门(50ms 轮询 10s 上限,2026-10-07 flake 治愈体)+3 探针逐字回显比对+kill 收口;**活调用者 compiler/test/smoke.sh:804-806**(`(cd examples/ctecho && timeout 90 sh run.sh)`)——run.sh 在库未动(git 净),调用径读验存在,活调用者零扰。注:smoke.sh 工作树现载 peer 在途改动,其 ctecho 段与本批无涉 | 入表:`ex-ctecho` steps=["sh run.sh"] cwd=examples/ctecho(零参即回环自验,env CTECHO_PORT 可换) |
| ctslink | 三段状态机(语义硬门/原生臂发射+链接 db c_src 三件/CSRF cookie-jar curl 全链)+**BLOCKED 软门**(框架卡在册时印 BLOCKED 退 0)+env 情景(CTRON_REDIS_URL/CTRON_PG_DSN/CTS_LINK_PORT)——软门 rc0 语义与任务表红绿信号冲突,留脚本面 | 不入表 |
| todo_api | 端口/密钥/RT 三 env 矩阵(TODO_API_PORT/TODO_API_KEY/CTRON_TODO_RT=coro 双 RT)+回环 e2e 探针族(健康/OpenAPI/静态 ETag/304/CORS/JWT/CRUD/限流 503/优雅停机排空) | 入表:`ex-todo-api` steps=["sh run.sh"] cwd=examples/todo_api;活指引 website/docs/server-guide.md:9 原句仍真(`sh examples/todo_api/run.sh`) |
| todo_app | 两相位 e2e(相位一注册/登录/隔离/登出全链+相位二同 data 目录**重启持久化**)+TODO_APP_PORT/CTRON_EMIT env;发射臂红账在册(C-todo_app 复现族,四模块 check 0E 为当前语义门) | 不入表 |

裁决口径:run.sh 零参缺省自洽者给 `sh run.sh` 委派入口(task 表全谱入口);env 情景重/软门语义冲突者留脚本面,不为入表而改脚本。批3 本体 `python3 tools/ctcl_check.py ctron.ctcl` E=0 W=0;原生驱动器 `ctron task` 列表实证 10 枚 ex-* 在册。

## 3. 编排 shell 终盘(14 份分类;地基非债)

| 族 | 件数 | 文件(今日实测行) | 定性 |
|---|---|---|---|
| 自举 bootstrap | 4 | selfhosted/bootstrap.sh(79)/ladder.sh(191)/ladder_keep.sh(99)/cc.sh(31) | **地基非债**:"Ctron 用 Ctron 构建 Ctron"叙事的梯子本身,C 宿主引导白名单 |
| 安装/分发 install-dist | 2 | install.sh(53)/tools/release.sh(72) | **地基非债**:装机白名单(spec §8 W5"安装垫片"面);tarball 面归 release.sh |
| 编译器构建 compiler-build | 6 | compiler/build.sh(41)/native.sh(118)/ctc.sh(618)/build_fb.sh(25)/bench.sh(113)/test/smoke.sh(1377) | 编译器泳道的宿主侧驱动;ctc.sh/ctron-emit 是收编机械的被消费面(清单 gui 自动链接即 ctc.sh:447-452) |
| 薄垫片 thin-shim | 1 | ctron(1566) | W4 波3 批4 已翻转:头部哨兵 exec 原生件三定位,sh 实现全量保留为回落体(对拍门逃逸开关);垫片是壳非编排 |
| 工具 tool | 1 | tools/sync_site_spec.sh(63) | 网站规格同步工具,一次性职责 |

合计 14 份 / **今日实测 4446 行**(2026-10-14,peer 在途窗内测)。评估快照口径 **1627 行**与根三件之和恰合(ci.sh 8 + ctron 1566 + install.sh 53)——差异主因 smoke.sh(1377)与 ctc.sh(618)计入分类口径;数字漂移不改定性:**编排面已收敛至任务表+门清单,残余 shell 全部是地基/引导/垫片/编译器宿主侧,非编排债**。spec §8 W5 "≤10(bootstrap/安装/薄垫片)"按字面仅指地基三族=7 件,达标。

## 4. tests/ 147 份 run.sh = 集成门(非编排面)

`find tests -name run.sh` = **147**。与 examples 情景豁免同一定性:每份是一个域的集成门脚本(断言协议逐域异构),编排外壳已归任务表(fast/gate 22 步),内核断言面不收编。**测试协议内建**(目录即套件/期望格式/断言协议,spec §9 开放项 1)= **在册开放项**,独立设计轮,不搭车本泳道。

## 5. 触发件在册(停摆账,诚实分册)

**症状**:`emit:实参类型不符:rt_window_loop_d 第2参 形参码=s 实参码=i`——`run(ViewCall)` 编译器合成面(rt_run_anchor→rt_run_d→rt_window_loop_d 链)装配处,检查器在调用位见 int 而前向声明为 `const char*`。

**停摆面**:批1 五任务(ex-gui-dash/snippets/widgets/contacts/themes)停此(六步走位正确,步骤 3 发射位败);批2 四例未启动、任务未建(同触发,启动即撞);连带在库 ex-gui-calc(W2 验绿后回归窗内转败)与 tests/gui 阶梯 25 FAIL/28 ok。

**非批1 引入三证**(详 task-w5-1-report.md 事故账):HEAD 原 run.sh 同败;ex-gui-calc W2 验绿今同败;HEAD 净源码 /tmp 重建复现 = 86478cc0 后 34 个 compiler/src 提交窗内在库回归,非 peer WIP 非陈旧 bin。

**治愈判据**:五例 `ctron task ex-gui-*` rc0 + 各自 headless 特征行 + tests/gui 阶梯回 92/2 基线;批2 四例随治愈按批1 同模板收编(零再编辑路径已铺)。bisect 窗与观察点见 task-w5-1-report.md 治愈线索节。

## 6. §8 W5 终验(2026-10-14 实测)

| 门 | 结果 | 态 |
|---|---|---|
| `sh tests/drv_parity/run.sh` | **136 ok / 0 fail,rc=0** | 绿 |
| `sh tests/dist/ctron_smoke.sh` | **43 ok / 0 fail,rc=0** | 绿 |
| `sh tests/tasks/run.sh` | **21 过 / 0 败,rc=0** | 绿 |
| `ctron task fast` 步 2-4(standalone) | manifest run ALL GREEN / diff 四线对拍 167 例 ALL GREEN / ctcl_check --selftest ALL GREEN | 绿 |
| `ctron task fast` 步 1(meta_check) | **13 处 FAIL = 在库既有债**(W4 波3 批4 已登记与本线无涉;599 测试文件:485 behavior+7 lint+74 neg+11 panic) | 已知红,在册 |
| `ctron gate` 全 22 步 | **未跑**(本收官明令豁免:太重;长门后台归 CI 侧);不 push,CI 未验 | 记账 |
| ctcl_check 根 ctron.ctcl | E=0 W=0 | 绿 |

## 7. 文档同步账(核真,零改动)

- **website/docs/server-guide.md:9** `sh examples/todo_api/run.sh` —— todo_api 豁免留 run.sh,原句仍真,不动。
- **tour.md:245/269** gui_counter"complete runnable"/三例指路 —— gui_counter(批2 停摆)run.sh 在库,句真,不动。
- **tests/COVERAGE.md:375/380/429/490** —— 引例名(ctecho/todo_api/todo_app)非命令,四例皆豁免留脚本,行真,不动。
- **examples README 11 份**:引 run.sh 的 7 份(ctecho/ctslink/gui_cjk/gui_counter/gui_zitie/todo/todo_app)全部仍有 run.sh(豁免/批2 停摆),句真;已收编 4 份账净(gui_dash/gui_files 批1 改任务面,gui_calc W2 净,todo_v10 无 README)。
- 网站其余 run.sh 引用(web/compress、http/*、spec/09)全指 tests/ 门,不受本线影响。

## 8. 收官定性

W5 的**收编机械全部 proven**:清单 gui 面(dependencies{gui}=ctc 自动链接)三动作模板、task.cwd 委派、headless env 注入、情景豁免定性、委派入口形——各有一例以上全绿实证(ex-gui-files 批1、ex-gui-calc W2 时点)。**残余唯一拦路 = §5 触发件**(编译器发射面,归治愈任务,非本泳道文件)。治愈后:批1 五任务零再编辑翻转绿,批2 四例同模板收编,run.sh 8→4(全豁免),§8 W5 验收门("run.sh 存量收敛至白名单")全兑。
