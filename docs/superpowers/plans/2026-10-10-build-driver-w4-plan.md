# 构建驱动器升格 W4 波1(原生驱动器骨架 + 委派族 + fmt/lint)Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** W4 分三波;本波落波1——`compiler/driver/` 用 Ctron 写原生驱动器骨架(`--version/--help/未知子命令` 与委派族 run/check/doc/test,再加有真逻辑的 fmt/lint),与根 sh 脚本逐命令黄金对拍(stdout+rc 逐字节),对拍门入仓库 gate。波2(build 项目面/task/gate/clean 原生化)与波3(pkg 族/bench/new/垫片翻转/install 布局/自举梯)另立计划续作。

**Architecture:** 驱动器 = `compiler/driver/src/main.ct` 单文件起步(波2 长出多文件再立 Ctron.ctcl 项目化),`ctc.sh build` 入 `compiler/bin/ctron`,native.sh 接线;委派族经 `run_argv_rc`(W3)直 exec 兄弟二进制,fmt 经 `proc.run`(固定 `<bin> run <arg>` 形恰为 `ctron-fmt run <file>`)捕获;对拍 = 新门 `tests/drv_parity/run.sh`(同一命令过 sh 版与原生版 diff stdout+rc,语料逐批增厚)。

**Spec:** `docs/superpowers/specs/2026-10-10-build-driver-design.md` §7/§8 W4 行。**两处修正(2026-10-10 依侦察裁决):** ①「std/config L1 通用面」移出驱动器关键路径——驱动器消费 ctron.ctcl 需要的是 awk 读者消息逐字对拍,通用化 config.ct 还背着 ctcl_chk.ct 双镜像同步纪律,爆炸半径大于驱动器本体;波2 的 task/gate/clean 批改用驱动器本地 CTCL 读取模块(消息与 awk 读者逐字一致),std/config L1 降级为 config 泳道开放项(spec §7 该行随批修订)。②波1 驱动器单文件(usage 文本逐字复刻走逐行 println 范式,ctgrep 先例;多行串 panic 在册)。

## Global Constraints

- **对拍即验收**:每个子命令批次新增的语料必须过 `tests/drv_parity/run.sh`(sh 版 vs 原生版 stdout+rc 逐字节);ctron_smoke.sh 是第二 oracle(其 12 组断言在翻转前只跑 sh 版,翻转后换 :28 指针整门复用)。
- rc 约定 0/1/2 与 sh 版逐字同;帮助/用法文本与 sh 版逐字同(逐行 println 复刻,改动 sh 文本的批次必须同步驱动器文本)。
- 双宿纪律:驱动器自身是 Ctron 程序,它编译后的行为即原生行为;其消费的 std/lib 面全部已双宿验证(env_set/walk/list/proc 族)。
- 对端并发:`git status` 先行;T1 动 native.sh/build.sh 与根 ctron.ctcl——peer 在飞件交叠即 STOP。pathspec 限定,禁 -A,不 push(控制器收尾推)。
- smoke:本波不触 compiler/src(T1 的 native.sh/build.sh 是编排脚本非编译器源),smoke 不入各批门;但 `sh tests/dist/ctron_smoke.sh` 每批必绿(sh 版驱动器契约不许被 native.sh 新增产物破坏)。
- 报告:`.superpowers/sdd/task-w4-{1,2,3}-report.md`。

---

### Task 1: 驱动器骨架 + build 接线 + 对拍门

**Files:**
- Create: `compiler/driver/src/main.ct`、`tests/drv_parity/run.sh`
- Modify: `compiler/native.sh`(驱动器构建段)、`ctron.ctcl`(gate 增对拍步骤,21→22)

**Interfaces:**
- Produces: `compiler/bin/ctron`(原生,`--version`/`--help`/`help <cmd>`/未知子命令 rc2 与 sh 版逐字);`tests/drv_parity/run.sh` 语料框架(表驱动:命令 → sh/原生双跑 → diff);后续每批只加语料行。

- [ ] **Step 1: 骨架**

`compiler/driver/src/main.ct`:

1. `fn main()`:取 `env.args()`;空 = usage + rc 2(镜像 sh `usage; exit 2`);按 argv[0] 分派。
2. `--version|-V`:`ctron $(cat VERSION)` 形——读 `$ROOT/VERSION`?**驱动器无 ROOT 概念**,sh 版 `ROOT=$(dirname BIN)`;驱动器自身路径 = `ctron_exe_path()`(prelude),bin 目录 = 其父,仓库根 = bin 父的父(compiler/bin → compiler → 根);ROOT/VERSION 存在则 `ctron <内容>` 否则 `ctron dev`(逐字镜像 sh :757-759)。
3. `--help|-h|help` 与 `help <cmd>`:逐字复刻 sh usage()(ctron:229-277)与 help_cmd()(ctron:279-315)文本——**逐行 println,转写时逐行核对 sh 原文**(含标点与空格);help 未知命令臂 = 整版 usage。
4. 其余一切子命令名:波1 只落 run/check/doc/test/fmt/lint(占位返回"未知子命令"前的分派表先只挂已实现臂),未实现臂一律走 sh 同款 `未知子命令 '<cmd>'(详见 ctron --help)` rc 2?——**否**:未实现臂必须与 sh 行为一致,而 sh 对已知名有真行为;对拍语料只测已实现臂,未实现的已知名(bench/add/…)在波1 期间原生版会与 sh 分歧——**处置:分派表先只收 --version/--help/help/run/check/doc/test/fmt/lint,其余名字一律 rc2 未知处理,drv_parity 语料波1 不含未实现臂;波2/3 补臂时同步补语料**(诚实账,翻转只在对拍全谱后)。
5. 帮助文本里的子命令清单与 sh usage 全集一致(文本是全集,行为分期——文本先行避免翻转时再改)。

- [ ] **Step 2: build 接线**

`compiler/native.sh` 尾段(cc 循环 :73-92 之后)增驱动器段:

```sh
# W4:用户驱动器(Ctron 自写;委派族+fmt/lint;对拍门 tests/drv_parity)
if [ -f "$DIR/driver/src/main.ct" ]; then
    echo "== build driver: bin/ctron =="
    "$DIR/ctc.sh" build "$DIR/driver/src/main.ct" --target native -o "$BIN/ctron" || exit 1
fi
```

(具体回显风格对齐文件既有段;若 ctc.sh build 的 -o 路径规则不符,读 ctc.sh:420/517 修正——产物必须落 `$BIN/ctron`。)

- [ ] **Step 3: 对拍门**

`tests/drv_parity/run.sh`(house 骨架):定位 `sh $ROOT/ctron` 与 `$ROOT/compiler/bin/ctron`(缺件 = SKIP 显式登记,门绿——镜像 bare/wasm 环境门惯例);语料表驱动,每行 = 参数串;双跑(各自 stdout/stderr 合流捕获)diff + rc 对比,逐行报告。波1 语料:`--version`、`--help`、`help`、`help build`、`help fmt`、未知子命令 `frobnicate`、无参。**gt 纪律:两版实现对同一语料的 stdout 必须逐字节(含换行)一致**——首跑即抓差异逐条修。

- [ ] **Step 4: gate 接线**

根 `ctron.ctcl` 的 gate steps 在 `sh compiler/native.sh` 之后插 `sh tests/drv_parity/run.sh`(21→22);`python3 tools/ctcl_check.py ctron.ctcl` + fmt 幂等复验。

- [ ] **Step 5: 验证 + Commit**

```bash
sh compiler/native.sh          # 产物 bin/ctron 在场
sh tests/drv_parity/run.sh     # 全语料 diff 干净
sh tests/dist/ctron_smoke.sh   # sh 版契约不破
python3 tools/ctcl_check.py ctron.ctcl
```

```bash
git add compiler/driver/src/main.ct compiler/native.sh tests/drv_parity/run.sh ctron.ctcl
git commit -m "feat(driver): 原生驱动器骨架(W4 波1 批1)——compiler/driver/ Ctron 自写 bin/ctron(ctc.sh build 单文件;native.sh 接线段)+--version/--help/help 全集文本逐字复刻(逐行 println,ctgrep 范式)+未知子命令 rc2+ctron_exe_path 定位 VERSION;对拍门 tests/drv_parity(sh vs 原生 stdout+rc 逐字节,缺件 SKIP 登记)入 gate 21→22;分派面波1=委派族+fmt/lint,未实现臂 rc2 诚实账(翻转只在对拍全谱后);门:drv_parity 全绿+ctron_smoke 绿"
```

---

### Task 2: 委派族 run/check/doc/test

**Files:**
- Modify: `compiler/driver/src/main.ct`、`tests/drv_parity/run.sh`(增语料)

**Interfaces:**
- Consumes: T1 骨架与分派表;W3 `run_argv_rc`/`argv_to_blob`(use proc);`ctron_exe_path` 定位兄弟件(BIN = exe 父目录;安装/开发两布局同式——原生件恒与 ctron-cc 同目录,sh 版的回落逻辑不再需要)。
- Produces: 四臂 = 组 argv → `run_argv_rc("" 直通 cwd? **否——sh 版 run/check/doc/test 用 exec 且 cwd 继承**,驱动器 spawn cwd 传空串=继承(argv_p stdin_path 同款语义,cwd 空串继承在 ctron_proc.c:146 有 chdir("") 吗?**核**:ctron_proc.c 的 chdir 分支——cwd 空串是否跳过 chdir,读码钉定;若空串不跳,传 `.`)→ exit(rc)。

- [ ] **Step 1: 四臂**

镜像 sh 分派臂(ctron:761-792):
- `run <file>`:argv = [BIN/ctron-cc, run, file];缺文件参 → `run 需要输入文件` rc2(逐字)。
- `check <file> [--format=json] [--profile=bare]`:其余参原样透传 ctron-chk(sh :766-768 把 $1 绝对化后透传其余——`IN=$(cd dirname&&pwd)/basename` 绝对化逻辑照抄)。
- `doc <file|std.模块>`:sh :770-778 的 `*/*` 与裸名透传规则照抄。
- `test <file>`:≡ ctron-cc run(sh :789-792)。

- [ ] **Step 2: 语料 + 验证 + Commit**

drv_parity 增:`run tests/tasks/data/ok.sh`?——run 执行的是 Ctron 源非 sh;语料用真 .ct(建 fixtures:hello.ct 正例 + 坏.ct 编译错例,断言两版诊断输出与 rc 全等——**编译器诊断透传即逐字**)。语料:`run hello.ct`、`run 坏.ct`、`run`(缺参 rc2)、`check hello.ct`、`check 坏.ct`、`doc hello.ct`、`doc std.json`、`test tasks样例.ct`(含 test 块的)、未知旗标臂。每批跑:`sh tests/drv_parity/run.sh` + `sh tests/dist/ctron_smoke.sh`。

```bash
git add compiler/driver/src/main.ct tests/drv_parity/run.sh
git commit -m "feat(driver): 委派族 run/check/doc/test(W4 波1 批2)——run_argv_rc 直 exec 兄弟件(cwd 继承语义核码钉定)+rc 直通+参数绝对化/透传规则照抄 sh 臂;drv_parity 语料+9(正例/编译错例诊断透传逐字/缺参 rc2/std.json doc 面/test 块面);门:drv_parity 全绿+ctron_smoke 绿"
```

---

### Task 3: fmt + lint(真逻辑首批)

**Files:**
- Modify: `lib/std/fs.ct`(+`compiler/test/stdpkg/std/fs.ct` 镜像)、`compiler/driver/src/main.ct`、`tests/drv_parity/run.sh`

**Interfaces:**
- Consumes: `proc.run(cwd, bin, arg, var out)`(捕获 ctron-fmt 单文件输出,rc -1=败);fs 字节读(read_file_bytes 族内建);T2 的 exe 定位。
- Produces: fs.ct 增 `pub fn list(dir: Str) -> List[Str]`(非递归条目名表,字节序——**walk_entries 的 pub 化薄壳**,fmt/lint 目录展开 = `src/*.ct` 顶层,非递归,与 sh `find -maxdepth 1` 同义);驱动器 fmt/lint 全语义。

- [ ] **Step 1: fs.list**

fs.ct:`pub fn list(dir: Str) -> List[Str] { return walk_entries(dir) }`(一行薄壳+口径注释:非递归,与 walk 的递归面互补;名字走 walk_entries 已排序)。stdpkg 镜像随批;双宿验证同批2 口径。

- [ ] **Step 2: fmt 臂(镜像 sh :795-838)**

旗标解析(-w/--write/--check;未知旗标 sh 静默忽略?核 sh :799-802——case 无 *)照抄;目标集:目录 → 有 src 用 src 否则直下,`list`+**只留 .ct 后缀**(sh find -name '*.ct';list 返回全部条目,驱动器过滤 .ct)+字节序(list 已排序=sh 的 LC_ALL=C sort 同序);单文件直通。逐文件:`proc.run(".", BIN/ctron-fmt, "run " + f, out)` 捕获 → rc≠0:stderr 直出 out+ERR=1;--check:与 read_file_bytes(f) 逐字节比对,异则打印文件名计数;-w:异则 fs_write(f, out 文本) 并打印;默认:直出 out。尾部语义(计数行/退出码)逐字镜像 sh :830-836。
**注意**:proc.run 的 out 是 U8[];与 read_file_bytes 对比走字节循环或先转 Str(捕获文本面,转 Str 比较——转形用现成内建,核 bytes_to_str/str_from_bytes 哪个在册)。

- [ ] **Step 3: lint 臂(镜像 sh :132-174)**

目录展开同 fmt;逐文件捕获 ctron-chk run(可选 --trusted 透传);输出按行扫 E####/W#### 计数(字节扫:行首 E+4 数字);rc≥2 且无 E 行 = 硬错误计数;汇总行 `lint: N 文件,X 个错误 / Y 警告` 逐字;退出码三态(--strict 警告也红)照抄。

- [ ] **Step 4: 语料 + 验证 + Commit**

drv_parity 增 fmt/lint 语料:干净文件 fmt --check(rc0)、脏文件 fmt --check(rc1+文件名)、fmt -w 就地归一(双版各自写后**产物 diff**,跑前双份同脏输入)、目录形(--check 计数行)、单文件默认打印、lint 干净/脏/--strict/--trusted 四态、缺目标 rc2。fixtures 入 tests/drv_parity/fixtures/(同脏输入供双版消费——**注意 -w 会写文件,语料每例现造现清,mktemp 式唯一目录**,防双版互踩)。

```bash
git add lib/std/fs.ct compiler/test/stdpkg/std/fs.ct compiler/driver/src/main.ct tests/drv_parity/run.sh
git commit -m "feat(driver): fmt/lint 原生化(W4 波1 批3)——fs.list 薄壳 pub 化(walk_entries 非递归字节序,与 sh find -maxdepth 1 同义)+fmt 全语义(proc.run 捕获 ctron-fmt;--check 字节比对/-w 就地写/默认直出;目录展开只留 .ct)+lint 六态(行首 E/W 字节扫计数+硬错误规则+--strict/--trusted);drv_parity 语料+fmt/lint 全态(唯一目录现造现清防双版互踩);门:drv_parity 全绿+ctron_smoke 绿+fs 双宿"
```

---

## 波2/波3 预告(另立计划)

- **波2**:build(单文件+项目:清单 name 提取/dep/workspace 递归)、task/gate/clean 原生化(驱动器本地 CTCL 读取模块,awk 读者消息逐字;run_step 语义:argv 直 exec/fd3/cwd 契约全数迁 Ctron;**std/config L1 已裁出关键路径**,见修正①)
- **波3**:add/publish/lock(sha256_hex 已在库)/pkg 族(verify/trace/attest/log/seal——trace 的 MemFs 码gen 是最大单件)/bench/new;**垫片翻转**(根 ctron 头部 exec bin/ctron 缺件回落)+install 布局(Makefile prebuilt 家族)+自举梯接线
- 每波验收不变:drv_parity 语料全谱 + ctron_smoke + 现有全门;翻转后 dev+install 双布局冒烟。

## 执行记录

(执行时逐任务回写。)
