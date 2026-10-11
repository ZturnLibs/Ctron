# 构建驱动器升格 W4 波2(build 项目面 + task/gate/clean 原生化)Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 驱动器补齐 build(单文件+项目)与 task/gate/clean 三臂——后者含驱动器本地 CTCL 读取模块(与 W1 awk 读者消息逐字一致),runner 语义(argv 切分+元字符拒绝+步骤进度行+cwd/env 注入)全数迁 Ctron。完成后驱动器分派面 8→11 臂,对拍语料预计 54→85+。

**Architecture:** 波2 仍单文件(compiler/driver/src/main.ct,波3 项目化时再拆);runner 的结构优势要吃满——steps 读进内存 List,sh 的 fd3/stdin 问题结构性消失;task.cwd 不做 chdir,直接把解析后的绝对目录传给 spawn 的 cwd 参数(ctron_proc.c 空串跳 chdir 的对称面:非空即 chdir);env 注入用 W3 env_set(CTRON_STDPATH 注入自有进程,子进程继承)。CTCL 读取器 = 目的形(只认 ctron/task 两块四键),fail-closed,E 码与消息逐字镜像根脚本 DRV_READER_AWK。

**Spec:** `docs/superpowers/specs/2026-10-10-build-driver-design.md` §7 分批(本波=批2+批4 两族;批1 委派族/批3 fmt·lint 波1 已落)。

## Global Constraints(沿 W4 波1,增量五条)

- 对拍即验收:新语料全过 `tests/drv_parity/run.sh`;task/gate/clean 的行为 oracle 另有 `tests/tasks/run.sh`(21 断言,W1-W2 落的 sh 面回归门)——**原生驱动器跑 tests/tasks 语料必须同绿**(sh 版与原生版语义同一份夹具),这比 drv_parity 更狠(它测语义不测字节),两门都要。
- 读取器消息纪律:与根脚本 DRV_READER_AWK **逐字**(E5044 未知块/E5043 未知键/E5045 重复/E5050 config_version/E5051 缺 steps/E5054 空 steps/E5048 串非法/E5046 类型);波1 的 parity 捕获含 stderr,消息差一字节即红。
- 元字符拒绝表逐字迁移:竖线、&、分号、$、反引号、<、>、圆括号、星号、问号、换行;切分=空白切+双引号分组+`\"`/`\\` 转义(sh DRV_SPLIT_AWK 同构);裸名 `ctron` 替换为 ctron_exe_path()。
- 对端并发与 pathspec 纪律同前;报告 `task-w4-w2-{1,2}-report.md`;不 push(控制器收尾推)。
- 长门后台+轮询(在册教训);驱动器重建 = `sh compiler/native.sh`(后台,数分钟)。

---

### Task 1: build 单文件 + 项目模式

**Files:**
- Modify: `compiler/driver/src/main.ct`、`tests/drv_parity/run.sh`

**Interfaces:**
- Consumes: T2 委派族的 exe 定位与 run_argv_rc;fs 面(read_file/fs_exists)。
- Produces: `build <file.ct> [--target]` 单文件臂 + `build`(项目模式:清单 name 提取/dep→ctron-dep/workspace 逐成员递归)。

- [ ] **Step 0: 多文件探针(定波3 形态)**

两文件探针:`/tmp/w4probe/{main.ct,lib.ct}`,main `use` 本地 lib 的 fn,`ctc.sh build` 编译运行。结果写入报告——**多文件 emit 可用则波3 驱动器项目化拆分解锁;不可用则波3 维持单文件**(不阻塞本波)。

- [ ] **Step 1: 单文件臂**

镜像 sh cmd_build 现文(grep 定位,行号已漂移;`--target` 解析循环+ct_target_check 未注册拦截),native 臂:ctron-emit 落 `<stem>.c`?——**sh 是 emit 重定向到文件再 cc**;驱动器对应=spawn `ctron-emit run <file>` **捕获输出**(proc.run 定形,argv=[emit,run,file])→ fs_write `<stem>.c` → spawn cc -O2 -w -pthread。have_cc 探针=spawn `cc` 冒烟(失败走 cc_missing_guidance 两条出路文案,逐字)。非 native 靶委派 ctc.sh(照抄 sh 分支)。

- [ ] **Step 2: 项目模式臂**

镜像 sh cmd_build_proj:Ctron.ctcl 缺席/发现 Ctron.toml 的 fail-closed 文案逐字;name 提取=sh 是 `sed -n 's/^ *name *= *"\(.*\)".*/\1/p'`(首个匹配)——**Ctron 侧逐行扫,首个 `name = "..."` 行取值,缺省用目录名**;dep 存在(行扫 `^dep "`)→ spawn ctron-dep(有则跑无则跳过告警行逐字);workspace 递归=行扫 member 名+path 对,逐成员 spawn **自身**(ctron_exe_path())于成员目录跑 build——与 sh `$0 build` 同构;产物 `build/<name>.c`+`build/<name>`,c_src/*.c 收集链接,echo 行逐字。

- [ ] **Step 3: 语料 + 验证 + Commit**

drv_parity 增:单文件 build 正例(产物 diff+echo 行)、缺 cc 路径没法在 CI 模拟(登记不语料化)、项目 build 正例(new 生成的最小项目?fixtures 造:name/src/main.ct,c_src 加一个 .c 验链接)、Ctron.toml fail-closed、缺入口 rc2、缺清单 rc2、`--target wasm32` 无工具链臂(sh 与原生同委派 ctc.sh,同输出)。`--target` 未注册 rc2 逐字。

```bash
git add compiler/driver/src/main.ct tests/drv_parity/run.sh
git commit -m "feat(driver): build 单文件+项目模式(W4 波2 批1)——emit 捕获落盘+cc 链+have_cc 双出路文案逐字;项目 name 提取/dep→ctron-dep/workspace 逐成员自 spawn 递归(与 sh $0 build 同构);Ctron.toml fail-closed/缺入口/缺清单 rc2 逐字;drv_parity 语料 build 全态;多文件探针结果入册定波3 形态;门:drv_parity 全绿+ctron_smoke 绿"
```

---

### Task 2: task/gate/clean 原生化(本地 CTCL 读取模块 + runner)

**Files:**
- Modify: `compiler/driver/src/main.ct`、`tests/drv_parity/run.sh`

**Interfaces:**
- Consumes: env_set(W3)、run_argv_rc/argv_to_blob、fs.walk/list(W3)、ctron_exe_path。
- Produces: `task`(list/run)、`gate`(缺省名+内建回落)、`clean`(默认三域+--all 项目守卫);本地 CTCL 读取(消息与根脚本 DRV_READER_AWK 逐字);runner(切分/元字符拒/进度行/cwd/env 注入/败即停 rc 透传)。

- [ ] **Step 0: 三探针(写码前钉死,结果入报告)**

① fs_delete 对**目录**的语义(空目录删得掉吗;`fs_mkdir`+`fs_delete` 探针)——决定 clean 形态:可删则 walk 后逐删,不可删则**文件全删+空目录壳残留=分歧账登记**(stdout 语料不受影响,清理彻底性波3 定);② clean 在 `.cache` 域不存在的输出行为(sh 逐域 `[ -d ]` 判定只报存在的域——镜像);③ tests/tasks/run.sh 的 21 断言在 CTRON_DRV_ROOT 沙箱下改跑**原生驱动器**全绿(临时把 T 函数的 DRV 指向 bin/ctron 手工跑一轮,不入库)。

- [ ] **Step 1: CTCL 读取模块(Ctron fn 群)**

输入=清单文本,输出=任务表(名/desc/steps List[List[Str]]/cwd)。逐字迁移根脚本 DRV_READER_AWK 的状态机与消息(读根脚本现文):strip_comment 字符串感知/unquote(UQ_OK 哨兵语义=空串合法)/块头 IDENT+实参+`{` 行尾/四键白名单/重复键 (cur SUBSEP key) 查重/task 关块时 steps 必填非空。**进度进度行不在读取器**。cwd 键已在册(批1 波2 task.cwd)。config_version 检查(E5050 消息逐字)与缺 ctron 块 END 路径(E5047)逐字。

- [ ] **Step 2: runner**

切分器(空白/双引号/转义/元字符拒,消息逐字含「两条出路」行)→ argv List;裸名 `ctron` 替换 ctron_exe_path();进度行 `ctron: task <名> 步骤 N/M: <步骤串>`——**走批3 的 echo_line 直写机制**(合流交错教训);env 注入=drv_env_setup 同构(CTRON_DRV_ROOT 缺省 DEVROOT=exe 父的父;lib/std 在场则 env_set CTRON_STDPATH);task.cwd=解析绝对目录传 spawn cwd(非空);步骤循环败即停+判词行逐字;`完成(N 步)` 行逐字。gate 臂=解析序三段(cwd 清单 task 块→项目模式内建默认门横幅行→rc2 文案)+首块嗅探防大小写误中(darwin 坑迁移);clean=三域删+--all 项目守卫(Ctron.ctcl 在场)。

- [ ] **Step 3: 语料 + 验证 + Commit**

drv_parity 增(夹具唯一目录,每例两版各自 CTRON_DRV_ROOT):task 列表/正例多步/未知任务/空 steps E5054/未知块 E5044/元字符拒/引号转义双层/task.cwd 正例+缺目录/clean 默认+--all+未知旗标/gate 表路径+内建回落横幅+nosuch rc2。**语义双门**:`tests/tasks/run.sh`(sh 面)不动仍绿;`sh ctron task fast` 快门两版各跑。

```bash
git add compiler/driver/src/main.ct tests/drv_parity/run.sh
git commit -m "feat(driver): task/gate/clean 原生化(W4 波2 批2)——本地 CTCL 读取模块(awk 读者消息逐字:strip/unquote UQ_OK/四键白名单/E 码族)+runner 全迁(steps 内存化=fd3 问题结构性消失;切分元字符拒逐字;裸名自解析;env_set 注入 CTRON_STDPATH;task.cwd=spawn cwd 参数非 chdir;进度行 echo_line 直写;败即停 rc 透传)+gate 解析序三段+首块嗅探+clean 三域与 --all 项目守卫;drv_parity 语料 task/gate/clean 全态;语义双门 tests/tasks 同夹具同绿;探针三枚入册(fs_delete 目录语义/clean 缺域/原生跑 tests/tasks)"
```

---

## 波3 预告(另立计划)

pkg 族(verify/trace/attest/log/seal;sha256_hex 在库;trace MemFs 码gen 最大单件)/add/publish/lock/bench/new;垫片翻转(根 ctron 头部 exec bin/ctron 缺件回落;ctron_smoke :28 指针翻转整门复用)+install 布局(Makefile prebuilt 加 bin/ctron+ctron_proc.c 链)+自举梯接线;真终审=翻转合并审查。

## 执行记录

(执行时逐任务回写。)
