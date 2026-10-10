# 构建驱动器升格 W2(收编开闸)Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** W1 任务面立住后的第一次真实收编:给 task 表补 `cwd` 键(子目录工作面收编的使能件),把 gui_calc 与 todo_v10 两个零引用例子的 run.sh 收进任务表并删除,meta_check 增设「脚本私清缓存」扫描,补三枚门面钉子与过时文档同步。

**Architecture:** `task.cwd` 走注册表演进(D7:加键不改文法)——读取器增 CWDT 记录、驱动器双面(sh/ps1)chdir、fail-closed;试点收编 = 仓库根 ctron.ctcl 增 `ex-*` 任务(cwd 指向例子目录,步骤=run.sh 无条件核心的逐字转写)+ run.sh 删除(零外部引用经侦察证实);meta_check 扫描仿 check_diag_catalog 横切先例自建脚本枚举。

**Tech Stack:** 同 W1(POSIX sh + awk、Python、CTCL)。碰面:ctron、ctron.ps1、tools/ctcl_ctron_schema.ctcl、tools/ctcl_check.py 语料、tests/tasks/、tests/meta_check.py、仓库根 ctron.ctcl、examples/{gui_calc,todo_v10}/、README×5 与两处子文档。

**Spec:** `docs/superpowers/specs/2026-10-10-build-driver-design.md` §8 W2 行(试点域 run.sh → task 表;meta_check 增扫私清缓存;usage/文档同步)。**范围裁决(2026-10-10 依侦察事实收窄):** spec 原建议「examples 子集 + tests/plugins」,侦察证实 tests/plugins/run.sh 是 8 锚断言形态(grep 断言无法表达为步骤),不收编;试点 = gui_calc + todo_v10(均零外部引用、线性模板);gui_dash 等其余 GUI 例的链接咒语迁移(E5 归一)是 W5 量级,不进 W2。

## Global Constraints(沿 W1,增量三条)

- rc 约定 0/1/2;步骤直 exec 无 shell 中间层;元字符拒绝表不变(W1 计划 Global Constraints 全文继续有效)。
- `task.cwd` 语义:可选 Str,相对**清单所在目录**解析;驱动器在该任务的步骤执行前 chdir 进去(验证存在,fail-closed rc 2);不影响其他任务;ps1 与 sh 同义。
- 试点收编的验收口径:`ctron task ex-<名>` 的行为与原 run.sh 的**无条件核心路径**(vendor 构建 → 清单构建 → headless 运行)逐一对应;真窗分支(--run / CTRON_GUI_RUN)不收编(交互性,不在任何 CI 路径),在任务 desc 注明手工命令。
- 对端并发:每任务开工前 `git status` 核工作区(对端正在飞 trans_stmt.ct/smoke.sh 等),pathspec 限定,禁 -A。
- meta_check 现存两红(对端 E1002 在册)不属本计划;本计划新增检查不得新增任何红。

---

### Task 1: `task.cwd` 面(注册表键 + 读取器 CWDT + 双面 chdir)

**Files:**
- Modify: `tools/ctcl_ctron_schema.ctcl`、`tools/ctcl_check.py`(语料)、`ctron`(读取器 + cmd_task)、`ctron.ps1`(镜像)、`tests/tasks/run.sh`(+2 断言)、`docs/superpowers/specs/2026-10-10-build-driver-design.md`(§4 一行修订注)

**Interfaces:**
- Consumes: W1 的读取器(TASK/STEP 三行式)、`cmd_task` 的清单目录 cd 契约、schema-as-CTCL 加载器(_REGKEY_FLAGS 机制)。
- Produces: `task.cwd` 可选键(注册表 `regkey "task.cwd" { type = "str" }`);读取器输出增第四种记录 `CWDT\t<任务名>\t<cwd 原文>`;cmd_task 在清单目录 cd 之后、步骤循环之前,查 CWDT → 相对清单目录解析 → 目录存在则 chdir,缺失/非目录 = die2 rc 2;ps1 同义。Task 2 的 ex-* 任务消费本键。

- [ ] **Step 1: 失败语料与测试先行**

① `tools/ctcl_ctron_schema.ctcl` 的 `reg "task"` 区增:

```text
regkey "task.cwd" {
    type = "str"
}
```

② `tools/ctcl_check.py` 的 `POS_CTRON` 增一带 cwd 的正例(保证非空串过检;无新诊断码,不加 NEG):

```python
("task cwd 键", '''\
ctron {
    config_version = 1
}

task "build" {
    cwd = "sub"
    steps = ["sh go.sh"]
}
'''),
```

③ `tests/tasks/run.sh` 增 T17/T18(插在 T16 之后,计数自动 16→18):

```sh
# T17 task.cwd:步骤以 cwd 目录为工作目录
mkdir -p "$SBOX/sub"
printf '#!/bin/sh\nprintf "cwd-ok pwd=%%s\\n" "$(basename "$PWD")"\n' > "$SBOX/sub/go.sh"
cat > "$SBOX/cwd.ctcl" <<'EOF2'
ctron {
    config_version = 1
}

task "build" {
    cwd = "sub"
    steps = ["sh go.sh"]
}
EOF2
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV task -f cwd.ctcl build < /dev/null 2>&1)
if [ $? -eq 0 ] && printf '%s' "$OUT" | grep -q "cwd-ok pwd=sub"; then ok; else bad "task.cwd 正例: $OUT"; fi
# T18 task.cwd 指向不存在目录 → rc2 fail-closed
cat > "$SBOX/cwd_bad.ctcl" <<'EOF2'
ctron {
    config_version = 1
}

task "build" {
    cwd = "nope"
    steps = ["sh go.sh"]
}
EOF2
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV task -f cwd_bad.ctcl build < /dev/null 2>&1)
if [ $? -eq 2 ] && printf '%s' "$OUT" | grep -q "cwd 目录不存在"; then ok; else bad "task.cwd 缺目录: rc2+判词 期望,实得: $OUT"; fi
```

- [ ] **Step 2: 跑红**

Run: `sh tests/tasks/run.sh && python3 tools/ctcl_check.py --selftest`
Expected: T17/T18 红(cwd 键被读取器 E5043 拒,或 CWDT 未实现时 pwd 断言红);selftest 新正例过(注册表键生效后即绿,语料先行仅为钉形)。

- [ ] **Step 3: 实现**

① 读取器 awk(DRV_READER_AWK)task 块键分派增 `cwd` 臂(与 desc 同形:type str、UQ_OK 求值、(cur SUBSEP key) 查重),块内状态变量 `tcwd`,`}` 关块时在 TASK 行后增发:

```awk
            if (!has_steps) fail("E5051 task \"" cur "\" 缺必填键 steps")
            if (nsteps == 0) fail("E5054 steps 不允许空列表(空 steps 的任务不合法)")
            printf "TASK\t%s\t%s\n", cur, desc
            if (tcwd != "") printf "CWDT\t%s\t%s\n", cur, tcwd
```

(键合法性白名单行 `key != "steps" && key != "desc"` 扩为 `key != "steps" && key != "desc" && key != "cwd"`,错误消息合法键清单同步加 cwd。)

② `cmd_task`:清单目录 cd 之后、`未知任务` 判定之后(此刻 STEP_LIST 尚未创建,只能清 TASKS_CFG),增:

```sh
    TASK_CWD=$(awk -F '\t' -v t="$TASK_NAME" '$1 == "CWDT" && $2 == t { print $3 }' "$TASKS_CFG")
    if [ -n "$TASK_CWD" ]; then
        TASK_CWD_ABS="$PWD/$TASK_CWD"
        [ -d "$TASK_CWD_ABS" ] || { rm -f "$TASKS_CFG"; die2 "task $TASK_NAME: cwd 目录不存在: $TASK_CWD(相对清单目录)"; }
        cd -- "$TASK_CWD_ABS" || exit 2
    fi
```

(语句顺序敏感:cwd 块必须落在 STEP_LIST 创建之前、只清 TASKS_CFG;若实现时把块放到 STEP_LIST 之后,清理行才补 `"$STEP_LIST"`。)

③ `ctron.ps1` 镜像:Dv-Load 的 task 块增 cwd 键(同 desc 形),记录对象带 Cwd;Cmd-Task 在步骤循环前 Resolve-Path(相对清单目录),失败 = stderr + exit 2。

④ usage 的 task 行补半句「task 可带 cwd = 相对清单目录」。

- [ ] **Step 4: 验证**

Run: `sh tests/tasks/run.sh`(期望 18 过 / 0 败)+ `python3 tools/ctcl_check.py --selftest`(ALL GREEN)+ `sh -n ctron` + `sh tests/dist/ctron_smoke.sh`(绿)。

- [ ] **Step 5: spec 修订注 + Commit**

spec §4 环境注入规则列表尾增一行:`- 任务可选 `cwd` 键(2026-10-10 W2 增,注册表演进 D7):相对清单目录解析,步骤执行前 chdir,缺失 = rc 2;真窗分支等交互路径不收编。`

```bash
git add ctron ctron.ps1 tools/ctcl_ctron_schema.ctcl tools/ctcl_check.py tests/tasks/run.sh docs/superpowers/specs/2026-10-10-build-driver-design.md
git commit -m "feat(driver): task.cwd 键落地(W2 批1)——注册表演进 D7 加键不改文法(task.cwd 相对清单目录,步骤执行前 chdir,缺失 rc2 fail-closed)+读取器增 CWDT 记录+sh/ps1 双面同义+tests/tasks 门 18 断言(T17 cwd 正例/T18 缺目录负例);门:tasks 18/0+selftest 绿+smoke 绿"
```

---

### Task 2: 试点收编 gui_calc + todo_v10(run.sh 删除,任务表接管)

**Files:**
- Modify: `ctron.ctcl`(仓库根,增两任务)
- Delete: `examples/gui_calc/run.sh`、`examples/todo_v10/run.sh`

**Interfaces:**
- Consumes: Task 1 的 task.cwd;两例子既有 Ctron.ctcl 与源码(不动)。
- Produces: 仓库根任务 `ex-gui-calc`、`ex-todo-v10`(以及各自的 `*-run` 真窗任务不建——desc 里写手工命令)。

- [ ] **Step 1: 转写**

通读 `examples/gui_calc/run.sh`(25 行)与 `examples/todo_v10/run.sh`(31 行),把**无条件核心**(自 scout 事实:①vendor/gui/build.sh ②清单构建(ctc build / ctron-emit+cc)③headless 运行)逐字转写为仓库根 ctron.ctcl 任务步骤;cwd 键分别指 `examples/gui_calc`、`examples/todo_v10`;步骤串里的相对路径一律改写成 cwd 相对(原 run.sh 是脚本目录 cwd,转写后是例子目录 cwd,两者恰好同目录——逐行核对每个路径)。条件尾(--run 参数分支 / CTRON_GUI_RUN 分支)不转步骤,并入任务 desc:「真窗手工:sh -c '…' 已随 run.sh 删除,命令见 git 史或 README」。若转写中发现路径依赖 run.sh 头部的 ROOT 定位变量,一律替换为 cwd 相对字面路径。

CTCL 注意:步骤串禁元字符(`| & ; $` 等);原 run.sh 若有 `$(uname)` case 或变量展开,那段属条件/逻辑,核心转写必须绕开它(gui_calc 走 ctc build 无 case,可直接转;todo_v10 的 emit/cc 若有 uname 分支,取本机 darwin 臂并在此登记「跨平台臂未收编,W5 随链接面归一处理」——诚实账,不装通用)。

- [ ] **Step 2: 验证**

```bash
python3 tools/ctcl_check.py ctron.ctcl          # E=0
printf '%s\n' '--- fmt 幂等 ---' && python3 tools/ctcl_check.py --fmt ctron.ctcl | diff - ctron.ctcl && echo OK
sh ctron task ex-gui-calc    # 期望:vendor 构建+清单构建+headless 全过,末行 完成(N 步)
sh ctron task ex-todo-v10    # 同上
sh tests/tasks/run.sh        # 18/0(回归)
```

Expected: 两任务 rc 0,headless 运行行为与原 run.sh 一致(输出关键行 grep 对拍:原 run.sh 的 headless 输出特征行在新路径下同样出现;在报告里贴双面对拍证据)。gui_calc 走 ctc build 清单自动链接(J19-④ 面),产物与旧 run.sh 同名同位。

- [ ] **Step 3: Commit**

```bash
git add ctron.ctcl examples/gui_calc/run.sh examples/todo_v10/run.sh
git commit -m "refactor(driver): 试点收编 gui_calc+todo_v10 进任务表(W2 批2)——仓库根 ctron.ctcl 增 ex-gui-calc/ex-todo-v10(task.cwd 指例子目录,步骤=run.sh 无条件核心逐字转写:vendor 构建→清单构建→headless;真窗分支不收编,desc 注明交互路径)+两 run.sh 删除(侦察证实零外部引用);E5 链接咒语归一先例=gui_calc 走 ctc build 清单面零手写链接;门:ctcl_check E=0+fmt 幂等+两任务 rc0 headless 对拍+tasks 18/0"
```

---

### Task 3: meta_check 私清缓存扫描

**Files:**
- Modify: `tests/meta_check.py`

**Interfaces:**
- Consumes: check_diag_catalog 横切先例(定义 ~258 行、main 调用点 ~297 行);脚本集合 = 仓库根 *.sh、examples/**/*.sh、tests/**/*.sh、tools/**/*.sh、selfhosted/**/*.sh、compiler/**/*.sh(自建枚举,现 rglob("*.ct") 射程外)。
- Produces: `check_script_cache_cleanup()`——脚本对 `.cache/` 路径的**清理动作**检测(匹配 `rm`/`unlink` 与含 `.cache` 的路径同现;`mkdir -p .cache/...` 只建不算);命中即报错(格式对齐既有报告风格)。预期全仓零命中(tests/tasks/run.sh 的清理经 `ctron clean` 在沙盒发生,脚本自身无 rm .cache 字样——若实查有命中,停下升级裁决,勿自行改别人的脚本)。

- [ ] **Step 1: 实现**

新增函数(置于 check_diag_catalog 旁,同样不逐文件循环、自建枚举):

```python
CACHE_CLEAN_RE = re.compile(r"(\brm\b[^#\n]*|unlink\s+)[^\n]*\.cache")

def check_script_cache_cleanup(errors):
    """驱动器 clean 是缓存唯一合法清理口(设计 build-driver-design D5/§6):
    扫全部编排脚本,凡脚本私 rm/unlink .cache 路径 = 报错。只建(mkdir)不算。"""
    roots = [ROOT / d for d in ("examples", "tests", "tools", "selfhosted", "compiler")]
    scripts = sorted(
        p for root in roots if root.is_dir()
        for p in root.rglob("*.sh") if p.is_file()
    )
    scripts.append(ROOT / "ci.sh")
    for p in scripts:
        for i, line in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
            if CACHE_CLEAN_RE.search(line):
                errors.append(f"{p.relative_to(ROOT)}:{i}: 脚本私清缓存(唯一合法口 = ctron clean): {line.strip()}")
```

`main()` 的 `check_diag_catalog()` 调用点旁并列调用(实参名按 main 里现有 errors 容器实名对齐)。正则按实仓形态微调时保持一条纪律:`mkdir`/`CTRON_DRV_ROOT` 沙盒/`ctron clean` 字样不误报;若 `.cache` 出现在注释行(行内 `#` 前)应豁免——按需在匹配前剥 `#` 后注释,与既有检查风格一致。

- [ ] **Step 2: 验证(绿面 + 负例探针)**

```bash
python3 tests/meta_check.py 2>&1 | tail -5   # 期望:除在册两红外零新增;若 sweep 零命中则无新行
# 负例探针(临时文件,探完即删):
printf '#!/bin/sh\nrm -rf "$ROOT/.cache/emit"\n' > /tmp/probe_cache.sh
python3 - <<'PY'
import sys; sys.path.insert(0, "tests")
import meta_check
es = []
meta_check.CACHE_CLEAN_RE and meta_check.check_script_cache_cleanup.__doc__  # 触点自检
PY
# 直接单测正则:
python3 -c "
import sys; sys.path.insert(0,'tests'); import meta_check as m
assert m.CACHE_CLEAN_RE.search('rm -rf \"\$ROOT/.cache/emit\"')
assert m.CACHE_CLEAN_RE.search('unlink .cache/x')
assert not m.CACHE_CLEAN_RE.search('mkdir -p \"\$ROOT/.cache/bare/gate_\$\$\"')
assert not m.CACHE_CLEAN_RE.search('echo .cache is managed by ctron clean  # rm 不在注释里')
print('probe OK')"
rm -f /tmp/probe_cache.sh
```

Expected: meta_check 输出红数与基线一致(2,对端 E1002);正则四断言过。**若全仓扫描出现真命中:STOP,报告命中清单升级裁决(涉及他人泳道脚本),不得顺手改。**

- [ ] **Step 3: Commit**

```bash
git add tests/meta_check.py
git commit -m "feat(meta): 私清缓存扫描(check_script_cache_cleanup)——驱动器 clean 唯一合法口的制度性守卫(D5/§6):examples/tests/tools/selfhosted/compiler 六域 *.sh+ci.sh 枚举,rm/unlink 与 .cache 同现即报,mkdir 只建不误报,注释豁免;负例正则四断言钉形;基线两红(对端 E1002)不变"
```

---

### Task 4: 门面钉子 ×3 + 过时文档同步

**Files:**
- Modify: `tests/tasks/run.sh`(+2 断言,T19/T20)
- Modify: `README.md`、`README.zh-CN.md`、`README.fr.md`、`README.de.md`、`README.ja.md`、`README.es.md`(各 205 行句)、`pkgs/gui/README.md:270`、`lib/rt/wasm32/README.md:67`

**Interfaces:** Consumes W1 全部命令面。Produces: 三枚回归钉 + 文档对齐。

- [ ] **Step 1: 三枚钉(T19–T21,计数 18→21)**

```sh
# T19 生产门路径钉:ctron.ctcl 定义 task "gate" 时,默认名走表不走内建回落
cat > "$SBOX/gatetbl.ctcl" <<'EOF2'
ctron {
    config_version = 1
}

task "gate" {
    steps = ["sh data/ok.sh"]
}
EOF2
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV gate < /dev/null 2>&1)
if [ $? -eq 0 ] && printf '%s' "$OUT" | grep -q "ran ok" && ! printf '%s' "$OUT" | grep -q "内建默认门"; then ok; else bad "生产门路径: $OUT"; fi
# T20 fmt 腿钉:脏文件使内建默认门 fmt --check 腿真咬人(rc1)
(cd "$SBOX/proj" && printf 'fn  main( ) {\n    println("hi")\n}\n' > src/main.ct)   # 故意脏(双空格/括号内空格)
OUT=$(cd "$SBOX/proj" && CTRON_DRV_ROOT="$SBOX" $DRV gate < /dev/null 2>&1)
if [ $? -eq 1 ] && printf '%s' "$OUT" | grep -q "待格式化"; then ok; else bad "fmt 腿: rc1+待格式化 期望,实得: $OUT"; fi
```

(T20 依赖 T10 建立的 $SBOX/proj 夹具;脏文件形取 `fn  main( )` 双空格——fmt 规范形态必改它;若该形 fmt 恰不改,换 `println ( "hi" )` 并在报告注明。T19 断言三重:rc0+核心跑过+内建门横幅缺席。)

- [ ] **Step 2: 文档同步**

① 六份 README 的 205 行句:`bash ci.sh # one command: bootstraps the toolchain, runs the full 9-stage acceptance gate` → `runs the full acceptance gate` 并补半句 `(21 steps via ctron gate; see ctron.ctcl)`;五份翻译同位同义改写(zh:「跑满 9 级验收门禁」→「跑满全量验收门禁(经 ctron gate,21 步;见 ctron.ctcl)」;fr/de/ja/es 同义)。各语言 208 行句(PR green)不动。
② `pkgs/gui/README.md:270`:「`ci.sh [8/9]` 挂载」→「`ctron.ctcl` gate 步 `sh tests/gui/gate.sh` 挂载」。
③ `lib/rt/wasm32/README.md:67`:「`ci.sh [6/9]` 前挂载」→「`ctron.ctcl` gate 步挂载」。
④ tests/COVERAGE.md 的历史志行**不改**(叙述史性质)。

- [ ] **Step 3: 验证 + Commit**

```bash
sh tests/tasks/run.sh      # 21 过 / 0 败
sh -n ctron
grep -rn "9-stage\|9 级\|9 étapes\|9 Stufen\|9 段階\|9 etapas" README*.md || echo "九步措辞清零 ✓"
grep -n "ctron.ctcl" pkgs/gui/README.md lib/rt/wasm32/README.md
```

```bash
git add tests/tasks/run.sh README.md README.zh-CN.md README.fr.md README.de.md README.ja.md README.es.md pkgs/gui/README.md lib/rt/wasm32/README.md
git commit -m "test(driver)+docs: 门面三钉与文档同步(W2 批4)——T19 生产门路径钉(ctron.ctcl 有 task gate 时默认名走表,内建横幅缺席三重断言)+T20 fmt 腿钉(脏文件 rc1 待格式化,关 M-T3-5 盲区)+六语言 README 205 行 9-stage 措辞清零(经 ctron gate 21 步)+pkgs/gui 与 wasm32 README 的 [n/9] 死标签改指 ctron.ctcl gate 步;COVERAGE.md 历史志不动;门:tasks 21/0"
```

---

## 收尾(控制器自做,不派工)

1. 计划档回写(本文件尾追加执行记录:四批 commit 链 + 门结果 + 偏离);ledger 关账;memory 更新。
2. 推送(本地领先笔数随对端机刷节奏,或主动推)。
3. 终审:W2 幅度小,不另派全分支终审——以各任务评审 + T19–T21 生产路径钉代之;若 Task 2 出现计划外大偏离,升级为一次终审。

## 执行记录(2026-10-10,W2 四批全落库过审)

- **批1 `c2b39aac`**(+spec §5.4 修订 `0104f320`):task.cwd 键——注册表演进 D7(schema regkey + POS 语料)、读取器 CWDT 记录、sh/ps1 双面 chdir(fail-closed 判词「cwd 目录不存在」)、门 16→18 断言(T17 正例 basename $PWD 实证 / T18 判词判别,E5043 路径不误绿经活体验证)。评审 Approved;Important 唯一枚 = §5.4「一律」与 cwd 键相抵,控制器一行修讫。
- **批2 `b5c7a8a5`**:试点收编 ex-gui-calc(5 步)/ex-todo-v10(6 步,task.cwd 指例子目录),两 run.sh 删除(零外部引用侦察证实)。**红基线对拍**:本树 gui 发射面先存红(原 run.sh 改前即 rc1,gui 阶梯 45/56 败 = 对端在飞),两任务输出与原 run.sh 特征诊断行逐字一致,评审独立复现并机制性背书(路径非因果 + 共享发射核码点);env(1) 直 exec 裁决成立(argv 直 exec、rc 直传,沙箱 rc7 透传实证);rc0 复验挂账树绿后各一条命令。
- **批3 `01fcb1e5`**:meta_check 私清缓存扫描(check_script_cache_cleanup,六域 *.sh + ci.sh,docker --rm 负向后行为真险三行实证必要)。扫描命中 smoke.sh:1307 私 rm emit 缓存(4ba1b37c fail-closed 腿)→ 裁决合规化:改经 `ctron clean`,腿单跑实证语义保持(fail-closed 仍 rc1+E2020/W8901);基线两红(对端 E1002)不变,sweep 零命中直证。
- **批4 `1dd401a8` + 复修 `6af7d5b2`**:T19 生产门路径钉(三重判别 + 负控真实落码:表无 task gate → rc2 未知任务,钉到分派臂精度)+ T20 fmt 腿钉(脏文件 rc1 待格式化,关 M-T3-5 盲区)+ 六语言 README 9-stage 措辞清零(德文 9-stufige 为 brief grep 漏项,实现者抓获)+ pkgs/gui 与 wasm32 README [n/9] 死标签改指 gate 步 + gui_calc README 死指令改任务表面。复修两 Important:T19 负控从宣称变真码(20→21),真窗指引链落地终止(ex-gui-calc desc 内联手工命令,评审草拟 env 形经 git 史证伪——真窗是无 HEADLESS 裸 exec)。复审 Approved。
- **范围裁决存档**:tests/plugins/run.sh 8 锚断言形态不收编(侦察实证);gui_dash 等其余 GUI 例链接面迁移(E5 归一)归 W5;计划「三枚钉 21」系与 gate 步数撞数,按 Files 节两枚实做(控制器裁决)。
- **挂账三枚**:①ex-* 两任务 rc0 复验(树绿后各一条命令,报告 §5 有命令与期望输出);②gui 发射面红(对端 GUI 泳道在飞,与本泳道无关);③meta_check 两红(对端 E1002,W2 验收「meta_check 绿」条款随 W1 同口径待其清偿)。
- **Minor 留档**:cwd 空串=未设未文档化 / 绝对 cwd 按清单相对重释 / ps1 通配 cwd 分歧(静态账)/ cd 泄 TASKS_CFG(TOCTOU 不可达)/ 变量携带缓存路径盲区(docstring 未注)/ pkgs/gui:274 例外注位置可读性。
