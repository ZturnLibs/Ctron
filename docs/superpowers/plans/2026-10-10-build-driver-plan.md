# 构建驱动器升格 W1(任务面/门禁/清理)Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 ci.sh 九步门禁与缓存清理收编进 `ctron` 驱动器:新增 CTCL 工具配置注册表(`ctron.ctcl`)+ `task/gate/clean` 三子命令(无 shell 中间层的步骤执行器),ci.sh 翻转为一行委派。

**Architecture:** 注册表走 CTCL §3 工具配置框架(schema-as-CTCL + 校验器注册表嗅探);根 `ctron` 脚本内置 awk 读取器(W1 过渡形态,原生驱动器落地后退役)+ 引号感知 argv 直 exec 执行器;repo 根 `ctron.ctcl` 定义全量门吸收 ci.sh,ci.sh 内联逻辑抽为独立脚本文件后翻转。

**Tech Stack:** POSIX sh + awk(根脚本延续)、Python(ctcl_check 校验器)、CTCL(注册表 schema)。不触碰编译器三线任何代码。

**Spec:** `docs/superpowers/specs/2026-10-10-build-driver-design.md`(D1–D7 裁决;本计划只覆盖 W1。W2 收编开闸 / W3 std 前置件 / W4 原生驱动器 / W5 收编收官,各自触发时另立计划档,见文末登记)。

## Global Constraints

- rc 约定不变:`0` 成功 / `1` 程序诊断失败 / `2` ctron 环境或用法错误(根脚本头注口径)。
- CTCL 文法冻结:深度恒 1、四类值、列表单行且元素仅 Str、注释仅 `//`、转义仅 `\"` 与 `\\`(CTCL 规范 §4)。
- 步骤执行无 shell 中间层:子进程 rc 由 waitpid 直取;元字符(竖线、&、分号、$、反引号、<、>、圆括号、星号、问号、换行)出现在步骤串任一处 = 拒绝执行 rc 2。
- `ctron.ps1` / `ctron.cmd` 与 sh 版同文(新增子命令必须三面同步)。
- 步骤串不含 TAB 与裸控制字符(CTCL 字符串禁),TAB 作读取器记录分隔安全。
- 每个任务提交前:`git status` 核分支与工作区(共享树纪律),`git add` 只限本任务 pathspec,禁止 `add -A`。
- W1 过渡债(登记,不追):awk 读取器消息与 E504x 不逐字节一致(拒收面一致即可);清单 `ctron.ctcl` 的 fmt 归 ctcl_check --fmt(读取器不做写回)。
- 测试脚本骨架沿用 house 形:`set -u` + DIR/ROOT 自定位 + 计数器 + 末尾 `[ "$FAIL" -eq 0 ]`。

---

### Task 1: CTCL 驱动器注册表——schema 文件 + 校验器注册表嗅探 + E5054

**Files:**
- Create: `tools/ctcl_ctron_schema.ctcl`
- Modify: `tools/ctcl_check.py`

**Interfaces:**
- Consumes: `ctcl_check.py` 现有 `_load_schema` / `parse_manifest` / `validate` / `render` / `judge_v1` / `selftest`(全部现有签名对 manifest 语料保持字节兼容——`tests/meta_check.py` import 了 `judge_v1`)。
- Produces: `judge_v1(text)` 升级为按首块嗅探注册表;`validate(blocks, ds, schema=None)`、`render(blocks, tail, schema=None, block_order=None)` 带默认参;E5054(空 steps);Task 2/4 的 `ctron.ctcl` 校验与 fmt 都走本任务产物。

- [ ] **Step 1: 写 schema 文件**

创建 `tools/ctcl_ctron_schema.ctcl`:

```text
// CTCL Schema:驱动器配置注册表 v1(设计:docs/superpowers/specs/2026-10-10-build-driver-design.md §3)
// schema-as-CTCL:消费者 = ctron 驱动器(task/gate/clean);加载器 = tools/ctcl_check.py。
schema {
    schema_version = 1
}

reg "ctron" {
    kind = "record"
    required = true
}

regkey "ctron.config_version" {
    type = "int"
    const = 1
    first = true
}

reg "task" {
    kind = "keyed"
    name_pattern = "[a-z][a-z0-9_-]*"
    required_keys = ["steps"]
}

regkey "task.steps" {
    type = "list"
    nonempty = true
}

regkey "task.desc" {
    type = "str"
}
```

- [ ] **Step 2: 写失败语料(ctcl_check.py selftest 增驱动器注册表用例)**

在 `ctcl_check.py` 的 `NEG` 列表定义之后新增两组语料,并让 `selftest()` 跑它们(此时应 FAIL——`nonempty` 未实现、嗅探不存在,ctron 语料会按清单 schema 报 E5044):

```python
POS_CTRON = [
("ctron 最小", '''\
ctron {
    config_version = 1
}
'''),
("task 全键", '''\
ctron {
    config_version = 1
}

task "smoke" {
    desc = "编译器冒烟单门"
    steps = ["sh compiler/test/smoke.sh --full"]
}
'''),
]
NEG_CTRON = [
("steps 空列表(E5054)", '''\
ctron {
    config_version = 1
}

task "a" {
    steps = []
}
''', 1),
("task 缺 steps(E5051)", '''\
ctron {
    config_version = 1
}

task "a" {
    desc = "x"
}
''', 1),
("缺 config_version(E5043 未知键 + E5050 缺版本键)", '''\
ctron {
    name = "x"
}
''', 2),
("config_version = 2(E5050)", '''\
ctron {
    config_version = 2
}
''', 1),
("未知块(E5044)", '''\
ctron {
    config_version = 1
}

gate {
    steps = ["x"]
}
''', 1),
("重复 task(E5045)", '''\
ctron {
    config_version = 1
}

task "a" {
    steps = ["x"]
}

task "a" {
    steps = ["y"]
}
''', 1),
("steps 尾逗号(E5048)", '''\
ctron {
    config_version = 1
}

task "a" {
    steps = ["x",]
}
''', 1),
]
```

`selftest()` 改为:

```python
def selftest():
    for name, text in POS: check(name, text, 0)
    for name, text, want in NEG: check(name, text, want)
    for name, text in POS_CTRON: check(name, text, 0)
    for name, text, want in NEG_CTRON: check(name, text, want)
    print("\n总体:", "ALL GREEN" if not FAILURES else "HAS FAILURES: %s" % FAILURES)
    return not FAILURES
```

- [ ] **Step 3: 实现嗅探 + nonempty + 消息参数化**

`ctcl_check.py` 五处修改:

① `_REGKEY_FLAGS` 增 `nonempty`:

```python
_REGKEY_FLAGS = {"type", "const", "first", "min", "pattern", "members", "sorted", "required", "nonempty"}
```

② 文件尾初始化区(`SCHEMA, KNOWN_CAPS, BLOCK_ORDER = _load_schema(...)` 处)增载第二张表并建嗅探器:

```python
_SCHEMA_DIR = os.path.dirname(os.path.abspath(__file__))
SCHEMA, KNOWN_CAPS, BLOCK_ORDER = _load_schema(os.path.join(_SCHEMA_DIR, "ctcl_manifest_schema.ctcl"))
CTRON_SCHEMA, _CTRON_KNOWN, CTRON_ORDER = _load_schema(os.path.join(_SCHEMA_DIR, "ctcl_ctron_schema.ctcl"))

def _pick_registry(blocks):
    """按首块名选注册表:驱动器面(ctron/task)→ ctron schema;其余 → 清单 schema。"""
    if blocks and blocks[0]["name"] in CTRON_SCHEMA:
        return CTRON_SCHEMA, CTRON_ORDER
    return SCHEMA, BLOCK_ORDER
```

③ `validate(blocks, ds, schema=None)`:函数体里所有 `SCHEMA` 改为 `schema`,开头 `if schema is None: schema = SCHEMA`;两处消息参数化(对清单语料逐字节不变):

```python
            if "const" in ks and v != ks["const"]:
                ds.append(diag("E5050", f["ln"], "%s 必须为 %s" % (f["k"], ks["const"])))
```

(原消息 `"manifest_version 必须为 %s" % ks["const"]` 输出逐字节相同,替换后清单语料不受影响。)

list 分支增 nonempty 检查(在 members 循环之前):

```python
            if t == "list":
                if ks.get("nonempty") and len(v) == 0:
                    ds.append(diag("E5054", f["ln"], "键 %s 不允许空列表(空 steps 的任务不合法)" % f["k"]))
                members = ks.get("members")
                ...
```

④ `render(blocks, tail, schema=None, block_order=None)`:开头同样取默认;体内 `SCHEMA`→`schema`、`BLOCK_ORDER`→`block_order`。

⑤ `check()` 与 `judge_v1()` 嗅探:

```python
def check(name, text, want):
    blocks, ds, tail = parse_manifest(text)
    sch, border = _pick_registry(blocks)
    ds = validate(blocks, ds, sch)
    c1 = render(blocks, tail, sch, border)
    b2, d2, t2 = parse_manifest(c1)
    idem = (not d2) and render(b2, t2, sch, border) == c1
    ...  # 其余不动
```

```python
def judge_v1(text):
    """判定器:E=0 即通过(允许 W)。返回 (ok, diags)。按首块嗅探注册表。"""
    blocks, ds, tail = parse_manifest(text)
    sch, _ = _pick_registry(blocks)
    ds = validate(blocks, ds, sch)
    es = [d for d in ds if d["code"].startswith("E")]
    return (len(es) == 0), ds
```

CLI `--fmt` 路径同样嗅探:

```python
        if ok and fmt_mode:
            blocks, _, tail = parse_manifest(text)
            sch, border = _pick_registry(blocks)
            print(render(blocks, tail, sch, border), end="")
```

- [ ] **Step 4: 验证**

Run: `python3 tools/ctcl_check.py --selftest`
Expected: 全部用例 PASS(含新 POS_CTRON/NEG_CTRON),总体 ALL GREEN,rc 0。

Run: `python3 tests/meta_check.py`
Expected: rc 0(judge_v1 对清单语料行为不变)。

- [ ] **Step 5: Commit**

```bash
git add tools/ctcl_ctron_schema.ctcl tools/ctcl_check.py
git commit -m "feat(ctcl): 驱动器配置注册表立表(ctron.ctcl;任务面 W1 批1)——schema-as-CTCL 第二张表(ctron record+task keyed,steps 必填 nonempty)+校验器注册表嗅探(按首块名选表,清单/驱动器两面对拍互不误伤)+E5054 空列表新码+nonempty 键级旗标+E5050 消息参数化(清单语料逐字节不变);门:ctcl_check selftest 全绿+meta_check rc0;设计:2026-10-10-build-driver-design.md §3"
```

---

### Task 2: 根脚本读取器 + 步骤执行器 + `ctron task`

**Files:**
- Modify: `ctron`(仓库根脚本)
- Create: `tests/tasks/run.sh`
- Create: `tests/tasks/data/ok.sh`、`tests/tasks/data/fail.sh`、`tests/tasks/data/args.sh`、`tests/tasks/data/m.ctcl`、`tests/tasks/data/meta_bad.ctcl`、`tests/tasks/data/empty.ctcl`、`tests/tasks/data/metachar.ctcl`

**Interfaces:**
- Consumes: 无(Task 1 独立;本任务的读取器自足)。
- Produces: `read_drv_manifest <file>`(输出 TAB 制三行式:`CFG\t<1>` / `TASK\t<名>\t<desc>` / `STEP\t<任务名>\t<步骤串>`,fail-closed rc 2);`run_step <步骤串>`(当前 shell 内把 "$@" 设为切分后 argv 并直 exec;裸名 `ctron` 替换为 `$CTRON_SELF`);`cmd_task`(list 与 run 两形)。Task 3 的 gate/clean 复用这三件。

- [ ] **Step 1: 写失败测试(tests/tasks/run.sh 第一版)**

创建测试夹具与门(骨架沿 house 形;全部断言此刻应 FAIL——`ctron task` 尚不存在):

`tests/tasks/data/ok.sh`:
```sh
#!/bin/sh
printf 'ran ok\n'
```

`tests/tasks/data/fail.sh`:
```sh
#!/bin/sh
exit 3
```

`tests/tasks/data/args.sh`:
```sh
#!/bin/sh
i=1
for a in "$@"; do
    printf 'arg%d=%s\n' "$i" "$a"
    i=$((i+1))
done
printf 'argc=%d\n' "$#"
```

`tests/tasks/data/m.ctcl`(正例清单;列表单行是文法钉子):
```text
ctron {
    config_version = 1
}

task "hello" {
    desc = "跑一个必然成功的脚本"
    steps = ["sh data/ok.sh"]
}

task "quotes" {
    desc = "切分与转义"
    steps = ["sh data/args.sh \"say \\\"hi\\\"\""]
}
```

(此步骤串过两层:CTCL 反转义 → `sh data/args.sh "say \"hi\""`;切分器再解一层引号与 `\"` → argv = [`sh`, `data/args.sh`, `say "hi"]` 三参——两层转义规则同形,一套心智,本用例即其全 fidelity 钉。)

`tests/tasks/data/meta_bad.ctcl`(未知块):
```text
ctron {
    config_version = 1
}

gate {
    steps = ["x"]
}
```

`tests/tasks/data/empty.ctcl`:
```text
ctron {
    config_version = 1
}

task "a" {
    steps = []
}
```

`tests/tasks/data/metachar.ctcl`:
```text
ctron {
    config_version = 1
}

task "a" {
    steps = ["echo one | tee x"]
}
```

`tests/tasks/run.sh`:
```sh
#!/bin/sh
# tests/tasks —— 驱动器任务面门(task/gate/clean;设计 2026-10-10-build-driver-design §4/§5)
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$DIR")
DRV="sh $ROOT/ctron"
SBOX=$(mktemp -d "${TMPDIR:-/tmp}/ctron_tasks.XXXXXX")
trap 'rm -rf "$SBOX"' EXIT
PASS=0
FAIL=0
ok() { PASS=$((PASS+1)); }
bad() { FAIL=$((FAIL+1)); echo "[FAIL] $1" >&2; }

# 沙盒夹具:拷 data 并把清单放沙盒根(cwd = 清单目录是执行器契约)
cp -r "$DIR/data" "$SBOX/data"

T() {
    NAME=$1; WANT_RC=$2; PATTERN=$3; shift 3
    OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV "$@" 2>&1)
    RC=$?
    if [ "$RC" -ne "$WANT_RC" ]; then
        bad "$NAME: rc=$WANT_RC 期望,实得 $RC(输出: $OUT)"
    elif [ -n "$PATTERN" ] && ! printf '%s' "$OUT" | grep -q "$PATTERN"; then
        bad "$NAME: 输出缺模式 [$PATTERN](输出: $OUT)"
    else
        ok
    fi
}

# T1 task 列表
T "task 列表" 0 "hello" task
# T2 task 运行正例
T "task 运行" 0 "ran ok" task hello
# T3 未知任务
T "未知任务" 2 "未知任务" task nosuch
# T4 清单非法(未知块)
cp "$DIR/data/meta_bad.ctcl" "$SBOX/bad.ctcl"
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV task -f bad.ctcl 2>&1)
if [ $? -eq 2 ] && printf '%s' "$OUT" | grep -q "E5044\|未知块"; then ok; else bad "清单非法: $OUT"; fi
# T5 空 steps
cp "$DIR/data/empty.ctcl" "$SBOX/empty.ctcl"
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV task -f empty.ctcl 2>&1)
if [ $? -eq 2 ] && printf '%s' "$OUT" | grep -q "E5054\|空列表"; then ok; else bad "空 steps: $OUT"; fi
# T6 元字符拒绝
cp "$DIR/data/metachar.ctcl" "$SBOX/metachar.ctcl"
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV task -f metachar.ctcl 2>&1)
if [ $? -eq 2 ] && printf '%s' "$OUT" | grep -q "元字符"; then ok; else bad "元字符拒绝: $OUT"; fi
# T7 引号切分与转义(双层:CTCL 反转义 + 切分器引号分组)
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV task quotes 2>&1)
if [ $? -eq 0 ] && printf '%s' "$OUT" | grep -q 'arg1=say "hi"' && printf '%s' "$OUT" | grep -q 'argc=1'; then ok; else bad "引号切分: $OUT"; fi
# T8 self 解析:步骤里裸名 ctron 指向驱动器自身
cat > "$SBOX/self.ctcl" <<'EOF2'
ctron {
    config_version = 1
}

task "selfcheck" {
    steps = ["ctron --version"]
}
EOF2
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV task -f self.ctcl selfcheck 2>&1)
if [ $? -eq 0 ] && printf '%s' "$OUT" | grep -q "ctron "; then ok; else bad "self 解析: $OUT"; fi

echo "tests/tasks: $PASS 过 / $FAIL 败"
[ "$FAIL" -eq 0 ]
```

说明:`task -f <文件>` 形(T4–T6、T8)允许测试指向沙盒内清单而非 cwd 清单——驱动器实现时 `task/gate` 增 `-f <file>` 旗标(缺省 `./ctron.ctcl`),这也是多清单仓库(子目录自有 ctron.ctcl)的自然逃生口,usage 一并写明。

- [ ] **Step 2: 跑测试确认失败**

Run: `sh tests/tasks/run.sh`
Expected: FAIL——`未知子命令 'task'`(rc 2 路径歪),多数断言红。

- [ ] **Step 3: 实现读取器与执行器**

`ctron` 脚本三处增补(位置:`sha256_file()` 之后、`usage()` 之前放 awk 程序与助手;分派臂放文件尾 `case`):

① awk 程序(heredoc 存变量,`set -u` 下安全):

```sh
# ---- 驱动器面 ctron.ctcl 读取器(W1 过渡形态;fail-closed 一错即停;E 码族复用) ----
DRV_READER_AWK=$(cat <<'AWK'
function fail(msg) { printf "ctron: ctron.ctcl:%d: %s\n", NR, msg > "/dev/stderr"; bad = 1; exit 1 }
function rtrim(s) { sub(/[ \t\r]+$/, "", s); return s }
function ltrim(s) { sub(/^[ \t]+/, "", s); return s }
function strip_comment(s,  i, c, inq, bs, out) {
    inq = 0; bs = 0; out = ""
    for (i = 1; i <= length(s); i++) {
        c = substr(s, i, 1)
        if (inq) {
            out = out c
            if (bs) bs = 0
            else if (c == "\\") bs = 1
            else if (c == "\"") inq = 0
        } else {
            if (c == "\"") { inq = 1; out = out c }
            else if (c == "/" && substr(s, i, 2) == "//") return out
            else out = out c
        }
    }
    return out
}
function unquote(s,  body, i, c, out) {
    UQ_OK = 1
    if (length(s) < 2 || substr(s, 1, 1) != "\"" || substr(s, length(s), 1) != "\"") { UQ_OK = 0; return "" }
    body = substr(s, 2, length(s) - 2)
    out = ""
    i = 1
    while (i <= length(body)) {
        c = substr(body, i, 1)
        if (c == "\\") {
            if (i == length(body)) { UQ_OK = 0; return "" }
            c = substr(body, i + 1, 1)
            if (c != "\"" && c != "\\") { UQ_OK = 0; return "" }
            out = out c
            i += 2
        } else { out = out c; i += 1 }
    }
    return out
}
{
    line = rtrim(ltrim(strip_comment($0)))
    if (line == "") next
    if (blk == "") {
        if (line ~ /^\[/) fail("本语言不用 [section] 段头;请用块:ctron { ... } / task \"名\" { ... }")
        head = line
        if (substr(head, length(head), 1) != "{") fail("块头 { 必须行尾:ctron { ... } / task \"名\" { ... }")
        head = rtrim(substr(head, 1, length(head) - 1))
        if (match(head, /[ \t]/) == 0) { id = head; arg = "" }
        else { id = substr(head, 1, RSTART - 1); arg = rtrim(substr(head, RSTART + 1)) }
        if (id != "ctron" && id != "task") fail("E5044 未知块 " id ";合法块:ctron, task")
        if (id == "ctron") {
            if (arg != "") fail("E5041 ctron 是记录块,不带名字实参")
            if (saw_ctron) fail("E5045 ctron 块最多一个")
            saw_ctron = 1
        } else {
            if (arg == "") fail("E5041 task 是键控块:task \"名\" { ... }")
            arg = unquote(arg)
            if (!UQ_OK) fail("E5040 task 名实参非法")
            if (arg !~ /^[a-z][a-z0-9_-]*$/) fail("E5046 task 名非法(形:[a-z][a-z0-9_-]*):" arg)
            if (arg in seen_task) fail("E5045 重复的 task \"" arg "\"")
            seen_task[arg] = 1
            cur = arg; nsteps = 0; has_steps = 0; desc = ""
        }
        blk = id
        next
    }
    if (line == "}") {
        if (blk == "task") {
            if (!has_steps) fail("E5051 task \"" cur "\" 缺必填键 steps")
            if (nsteps == 0) fail("E5054 steps 不允许空列表(空 steps 的任务不合法)")
            printf "TASK\t%s\t%s\n", cur, desc
        }
        blk = ""; cur = ""
        next
    }
    if (index(line, "{") || index(line, "}")) fail("E5040 块内禁止嵌套块/单行块(深度恒 1)")
    eq = index(line, "=")
    if (eq == 0) fail("E5040 块内每行必须是 键 = 值")
    key = rtrim(substr(line, 1, eq - 1))
    val = ltrim(substr(line, eq + 1))
    if (key !~ /^[a-z_][a-z0-9_]*$/) fail("E5040 块内每行必须是 键 = 值")
    if (blk == "ctron") {
        if (key != "config_version") fail("E5043 块 ctron 中未知键 " key ";合法键:config_version")
        if (seen_cfg) fail("E5045 重复键 " key)
        seen_cfg = 1
        if (val !~ /^-?(0|[1-9][0-9]*)$/) fail("E5046 config_version 的类型应为 int")
        if (val != "1") fail("E5050 config_version 必须为 1")
        cfg_ok = 1
        next
    }
    if (key != "steps" && key != "desc") fail("E5043 块 task 中未知键 " key ";合法键:steps, desc")
    if ((cur SUBSEP key) in seen_key) fail("E5045 重复键 " key)
    seen_key[cur SUBSEP key] = 1
    if (key == "desc") {
        if (substr(val, 1, 1) != "\"") fail("E5046 desc 的类型应为 str")
        desc = unquote(val)
        if (!UQ_OK) fail("E5048 desc 字符串非法(转义仅 \\\" 与 \\\\)")
        next
    }
    if (substr(val, 1, 1) != "[") fail("E5046 steps 的类型应为 list")
    if (substr(val, length(val), 1) != "]") fail("E5040 列表必须单行且以 ] 结尾")
    inner = ltrim(rtrim(substr(val, 2, length(val) - 2)))
    if (inner == "") fail("E5054 steps 不允许空列表(空 steps 的任务不合法)")
    if (substr(inner, length(inner), 1) == ",") fail("E5048 列表不允许尾逗号(最后元素后直接 ])")
    has_steps = 1
    rest = inner
    while (1) {
        inq = 0; bs = 0; elem = ""
        m = length(rest)
        for (j = 1; j <= m; j++) {
            c = substr(rest, j, 1)
            if (inq) {
                elem = elem c
                if (bs) bs = 0
                else if (c == "\\") bs = 1
                else if (c == "\"") inq = 0
            } else {
                if (c == "\"") { inq = 1; elem = elem c }
                else if (c == ",") break
                else elem = elem c
            }
        }
        e2 = rtrim(ltrim(elem))
        if (e2 == "") { UQ_OK = 0 }
        else { v = unquote(e2) }
        if (!UQ_OK) fail("E5048 列表元素必须是双引号字符串(转义仅 \\\" 与 \\\\)")
        nsteps++
        printf "STEP\t%s\t%s\n", cur, v
        if (j > m) break
        rest = substr(rest, j + 1)
    }
    next
}
END {
    if (bad) exit 1
    if (blk != "") { printf "ctron: ctron.ctcl: 块未闭合(缺 })\n" > "/dev/stderr"; exit 1 }
    if (!saw_ctron) { printf "ctron: ctron.ctcl: E5047 缺 ctron 块\n" > "/dev/stderr"; exit 1 }
    if (!cfg_ok) { printf "ctron: ctron.ctcl: E5050 ctron 缺语言版本键 config_version(必须存在且 = 1)\n" > "/dev/stderr"; exit 1 }
}
AWK
)
```

注意:错误哨兵走全局 `UQ_OK`(unquote 求值成功/失败),不用空串哨兵——空串是合法字符串值(`desc = ""`),用值本身判错会误拒。awk 里未赋值变量即空串,首次调用前 UQ_OK 天然为假,无害。

② 步骤切分器与执行器、`cmd_task`(shell 侧):

```sh
DRV_SPLIT_AWK=$(cat <<'AWK'
function fail(msg) { printf "ctron: %s\n", msg > "/dev/stderr"; bad = 1; exit 1 }
{
    s = $0
    if (index(s, "|") || index(s, "&") || index(s, ";") || index(s, "$") ||
        index(s, "`") || index(s, "<") || index(s, ">") || index(s, "(") ||
        index(s, ")") || index(s, "*") || index(s, "?")) {
        fail("E5040 步骤含 shell 元字符(| & ; $ ` < > ( ) * ?):" s ";两条出路:改写为多步骤 / 写脚本文件后以 sh 调用")
    }
    i = 1
    n = length(s)
    while (i <= n) {
        c = substr(s, i, 1)
        if (c == " " || c == "\t") { i++; continue }
        arg = ""
        if (c == "\"") {
            i++
            closed = 0
            while (i <= n) {
                c = substr(s, i, 1)
                if (c == "\\") {
                    if (i == n) fail("步骤串孤立的尾部反斜杠:" s)
                    arg = arg substr(s, i + 1, 1)
                    i += 2
                } else if (c == "\"") { closed = 1; i++; break }
                else { arg = arg c; i++ }
            }
            if (!closed) fail("步骤串引号未闭合:" s)
        } else {
            while (i <= n) {
                c = substr(s, i, 1)
                if (c == " " || c == "\t" || c == "\"") break
                arg = arg c
                i++
            }
        }
        print arg
    }
}
AWK
)

# run_step <步骤串>:切分 argv(裸名 ctron → $CTRON_SELF)并在当前 shell 直 exec。
# rc 由 waitpid 直取——无 shell 中间层,管道假 rc 在结构上不存在。
run_step() {
    STEP_FILE=$(mktemp "${TMPDIR:-/tmp}/ctron_step.XXXXXX")
    if ! printf '%s\n' "$1" | awk "$DRV_SPLIT_AWK" > "$STEP_FILE"; then
        rm -f "$STEP_FILE"
        return 2
    fi
    set --
    while IFS= read -r STEP_ARG; do
        [ "$STEP_ARG" = "ctron" ] && STEP_ARG=$CTRON_SELF
        set -- "$@" "$STEP_ARG"
    done < "$STEP_FILE"
    rm -f "$STEP_FILE"
    "$@"
}

# drv_load <ctcl 文件>:读注册表 → 全局 TASKS_CFG(临时文件,三行式 TAB 制)。
# 输出 NULL_MAN=1 表示文件不存在(fail-closed 判定交调用方)。
drv_load() {
    TASKS_CFG=$(mktemp "${TMPDIR:-/tmp}/ctron_drv.XXXXXX")
    if ! awk "$DRV_READER_AWK" "$1" > "$TASKS_CFG"; then
        rm -f "$TASKS_CFG"
        return 2
    fi
    return 0
}

cmd_task() {
    TASK_FILE=./ctron.ctcl
    TASK_NAME=""
    TASK_ARG_PREV=0
    for TA in "$@"; do
        if [ "$TASK_ARG_PREV" = 1 ]; then TASK_FILE=$TA; TASK_ARG_PREV=0; continue; fi
        case $TA in
            -f) TASK_ARG_PREV=1 ;;
            -f=*) TASK_FILE=${TA#-f=} ;;
            *) TASK_NAME=$TA ;;
        esac
    done
    [ -f "$TASK_FILE" ] || die2 "task: 缺清单 $TASK_FILE(驱动器面;试: ctron task -f <文件> 或先 ctron new)"
    # §5.4 cwd 契约:步骤一律以清单所在目录为工作目录(相对路径 = 清单相对)
    TASK_FILE=$(CDPATH= cd -- "$(dirname -- "$TASK_FILE")" && pwd)/$(basename -- "$TASK_FILE")
    cd -- "$(dirname -- "$TASK_FILE")" || exit 2
    drv_load "$TASK_FILE" || exit 2
    if [ -z "$TASK_NAME" ]; then
        awk -F '\t' '$1 == "TASK" { printf "%s\t%s\n", $2, $3 }' "$TASKS_CFG"
        rm -f "$TASKS_CFG"
        exit 0
    fi
    awk -F '\t' -v t="$TASK_NAME" '$1 == "TASK" && $2 == t { found = 1 } END { exit !found }' "$TASKS_CFG" \
        || { rm -f "$TASKS_CFG"; die2 "task: 未知任务 $TASK_NAME(试: ctron task 列表)"; }
    STEP_LIST=$(mktemp "${TMPDIR:-/tmp}/ctron_steps.XXXXXX")
    awk -F '\t' -v t="$TASK_NAME" '$1 == "STEP" && $2 == t { sub(/^[^\t]*\t[^\t]*\t/, ""); print }' "$TASKS_CFG" > "$STEP_LIST"
    TOTAL=$(wc -l < "$STEP_LIST" | tr -d ' ')
    N=0
    while IFS= read -r STEP; do
        N=$((N+1))
        echo "ctron: task $TASK_NAME 步骤 $N/$TOTAL: $STEP"
        run_step "$STEP"
        RC=$?
        if [ "$RC" -ne 0 ]; then
            rm -f "$STEP_LIST" "$TASKS_CFG"
            echo "ctron: task $TASK_NAME 在步骤 $N 失败(rc=$RC)" >&2
            exit "$RC"
        fi
    done < "$STEP_LIST"
    rm -f "$STEP_LIST" "$TASKS_CFG"
    echo "ctron: task $TASK_NAME 完成($N 步)"
}
```

(TOTAL 用 `wc -l` 数已过滤的步骤文件,不 grep TAB——步骤串虽无 TAB,但把 TAB 字面量敲进源码是维修陷阱。`awk -F '\t'` 的 `\t` 转义由 awk 处理,安全。)

③ 顶部初始化区(`CTRON_SELF` 与环境注入,放 `DEVROOT=$BIN` 判定块之后):

```sh
CTRON_SELF=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/$(basename -- "$0")
```

`drv_env_setup()`(Task 3 的 gate 复用;cmd_task 也调):

```sh
drv_env_setup() {
    DRV_ROOT=${CTRON_DRV_ROOT:-$DEVROOT}
    if [ -d "$DRV_ROOT/lib/std" ]; then
        CTRON_STDPATH="$DRV_ROOT/lib/std"
        export CTRON_STDPATH
    fi
}
```

④ 分派臂(文件尾 `case` 增):

```sh
    task)
        drv_env_setup
        cmd_task "$@"
        ;;
```

usage 增行(`ctron task [-f <file>] [名]  列出/执行 ctron.ctcl 任务(驱动器面;步骤直 exec,拒 shell 元字符)`)。

- [ ] **Step 4: 跑测试确认通过**

Run: `sh tests/tasks/run.sh`
Expected: `tests/tasks: 8 过 / 0 败`,rc 0。

- [ ] **Step 5: 全量门抽样**

Run: `python3 tools/ctcl_check.py --selftest && sh tests/dist/ctron_smoke.sh`
Expected: 双绿(ctron_smoke 是驱动器既有契约门——usage 变更不许打坏它)。

- [ ] **Step 6: Commit**

```bash
git add ctron tests/tasks
git commit -m "feat(driver): ctron task 任务面落地(W1 批2)——驱动器面 ctron.ctcl awk 读取器(fail-closed 一错即停,E 码族复用;过渡形态,原生驱动器后退役)+引号感知 argv 切分直 exec 执行器(无 shell 中间层,rc=waitpid 直取,元字符拒绝 rc2;管道假 rc 类在结构上不存在)+裸名 ctron 自解析+CTRON_DRV_ROOT 沙盒覆盖+task -f 多清单口;tests/tasks 门 8 断言(列表/正例/未知任务/非法清单/空steps/元字符/引号转义/self 解析);门:tasks 8/0+ctron_smoke 绿+selftest 绿"
```

---

### Task 3: `ctron gate`(内建回落)+ `ctron clean` + ps1 同文

**Files:**
- Modify: `ctron`(仓库根脚本)
- Modify: `ctron.ps1`、`ctron.cmd`
- Modify: `tests/tasks/run.sh`(增 gate/clean 断言)

**Interfaces:**
- Consumes: Task 2 的 `drv_load` / `run_step` / `drv_env_setup` / `cmd_task` 主体(`gate` ≡ task + 回落逻辑)。
- Produces: `cmd_gate`(解析序:D4——cwd `ctron.ctcl` task → 项目模式内建默认门 `["ctron build", "ctron fmt --check ."]` → rc 2)、`cmd_clean`(默认缓存三域;`--all` 加 `build/ pkgs/`)。Task 4 的 repo 全量门与 ci.sh 翻转只消费 `cmd_gate`。

- [ ] **Step 1: 写失败测试(tests/tasks/run.sh 追加)**

在 T8 之后追加(T9–T12):

```sh
# T9 gate = task 别名
T "gate 别名" 0 "ran ok" gate hello
# T10 内建默认门回落:项目目录(Ctron.ctcl 在场)无 ctron.ctcl → build+fmt
mkdir -p "$SBOX/proj/src"
printf 'pkg {\n    manifest_version = 1\n    name = "proj"\n    version = "0.1.0"\n}\n' > "$SBOX/proj/Ctron.ctcl"
printf 'fn main() {\n    println("hi")\n}\n' > "$SBOX/proj/src/main.ct"
(cd "$SBOX/proj" && CTRON_DRV_ROOT="$SBOX" $DRV fmt -w src/main.ct) > /dev/null   # 先 fmt 归一,免门假红
OUT=$(cd "$SBOX/proj" && CTRON_DRV_ROOT="$SBOX" $DRV gate 2>&1)
if [ $? -eq 0 ] && printf '%s' "$OUT" | grep -q "内建默认门" && printf '%s' "$OUT" | grep -q "已构建"; then ok; else bad "内建默认门: $OUT"; fi
# T11 非项目目录 + 无 ctron.ctcl → rc2 用法错
mkdir -p "$SBOX/empty_dir"
OUT=$(cd "$SBOX/empty_dir" && CTRON_DRV_ROOT="$SBOX" $DRV gate 2>&1)
if [ $? -eq 2 ]; then ok; else bad "gate 用法错: rc 2 期望,实得(输出: $OUT)"; fi
# T12 clean 清缓存域(--all 加 build/);CTRON_DRV_ROOT 沙盒
mkdir -p "$SBOX/.cache/emit" "$SBOX/.cache/bare" "$SBOX/proj/build" "$SBOX/proj/pkgs"
OUT=$(cd "$SBOX/proj" && CTRON_DRV_ROOT="$SBOX" $DRV clean --all 2>&1)
if [ $? -eq 0 ] && [ ! -d "$SBOX/.cache/emit" ] && [ ! -d "$SBOX/.cache/bare" ] && [ ! -d "$SBOX/proj/build" ] && [ ! -d "$SBOX/proj/pkgs" ]; then ok; else bad "clean --all: $OUT"; fi
# T13 clean 未知旗标
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV clean --nope 2>&1)
if [ $? -eq 2 ]; then ok; else bad "clean 旗标: rc 2 期望"; fi
```

尾部计数行改为 `13` 口径(不用改,计数器自动)。Task 名称参数覆盖注意:T10 走 `gate`(无 task 名),T13 验 rc2。

- [ ] **Step 2: 跑测试确认失败**

Run: `sh tests/tasks/run.sh`
Expected: T9–T13 红(`未知子命令 'gate'` / `'clean'`),T1–T8 保持绿。

- [ ] **Step 3: 实现 cmd_gate 与 cmd_clean**

```sh
cmd_gate() {
    GATE_NAME=""
    [ $# -ge 1 ] && GATE_NAME=$1
    [ -n "$GATE_NAME" ] || GATE_NAME=gate
    if [ -f ctron.ctcl ]; then
        cmd_task "$GATE_NAME"
        return
    fi
    if [ "$GATE_NAME" = "gate" ] && [ -f Ctron.ctcl ]; then
        echo "ctron: 内建默认门(build + fmt --check;项目自定义门:写 ctron.ctcl 的 task \"gate\")"
        run_step "ctron build"
        RC=$?
        if [ "$RC" -ne 0 ]; then echo "ctron: gate 在步骤 1 失败(rc=$RC)" >&2; exit "$RC"; fi
        run_step "ctron fmt --check ."
        RC=$?
        if [ "$RC" -ne 0 ]; then echo "ctron: gate 在步骤 2 失败(rc=$RC)" >&2; exit "$RC"; fi
        echo "ctron: gate 完成(2 步)"
        exit 0
    fi
    die2 "gate: 缺 ctron.ctcl 且非项目模式(Ctron.ctcl 亦缺);试: ctron task / 先 ctron new"
}

cmd_clean() {
    CLEAN_ALL=0
    for CA in "$@"; do
        case $CA in
            --all) CLEAN_ALL=1 ;;
            *) die2 "clean 未知旗标: $CA(支持: --all)" ;;
        esac
    done
    DRV_ROOT=${CTRON_DRV_ROOT:-$DEVROOT}
    for CD in emit bare wasm; do
        if [ -d "$DRV_ROOT/.cache/$CD" ]; then
            rm -rf "$DRV_ROOT/.cache/$CD"
            echo "ctron: 已清理 .cache/$CD"
        fi
    done
    if [ "$CLEAN_ALL" = 1 ]; then
        for CD in build pkgs; do
            if [ -d "$CD" ]; then
                rm -rf "$CD"
                echo "ctron: 已清理 $CD/"
            fi
        done
    fi
    exit 0
}
```

分派臂:

```sh
    gate)
        drv_env_setup
        cmd_gate "$@"
        ;;
    clean)
        cmd_clean "$@"
        ;;
```

`<cmd> --help` 拦截行的第一组命令名清单加 `task|gate|clean`,`help_cmd` 增三臂:

```sh
    task) echo "ctron task [-f <file>] [名] —— 列出/执行 ctron.ctcl 任务(驱动器面);步骤为直 exec 命令行,拒 shell 元字符(| & ; 等),复杂编排写脚本文件后以 sh 调用";;
    gate) echo "ctron gate [名] —— ≡ task,缺省名 gate;无 ctron.ctcl 时项目模式回落内建默认门(build + fmt --check)";;
    clean) echo "ctron clean [--all] —— 清工具链缓存三域(.cache/emit|bare|wasm);--all 加项目 build/ 与 pkgs/;CTRON_DRV_ROOT 可覆盖缓存根";;
```

usage 面板增两行(gate/clean;task 行已在 Task 2)。

- [ ] **Step 4: ps1 / cmd 同文**

读 `ctron.ps1` 的子命令分派结构,按其既有风格镜像三命令(语义逐条对齐 sh 版):`task`(`-f` 旗标 + 列表/执行;PowerShell 侧步骤执行 = `Start-Process -NoNewWindow -Wait -PassThru` 取 ExitCode,同样无 shell 中间层;元字符同一拒绝表)、`gate`(同一回落序)、`clean`(同域)。步骤切分器在 PowerShell 用自写 tokenizer(双引号分组 + `` ` `` 转义不做——拒绝 `` ` `` 元字符,只支持 `\"` `\\`,与 sh 版同表)。`ctron.cmd` 若仅转发 ps1 则零改动(核一眼即写明)。

验证(PowerShell 缺席则显式 SKIP 登记,不静默):

```bash
command -v pwsh >/dev/null 2>&1 && pwsh -NoProfile -File ctron.ps1 --help || echo "[skip] pwsh 缺席,ps1 面留 CI/Windows 线验"
```

- [ ] **Step 5: 跑测试与既有门**

Run: `sh tests/tasks/run.sh && sh tests/dist/ctron_smoke.sh && python3 tools/ctcl_check.py --selftest`
Expected: `tests/tasks: 13 过 / 0 败` + ctron_smoke 绿 + selftest 绿。

- [ ] **Step 6: Commit**

```bash
git add ctron ctron.ps1 ctron.cmd tests/tasks
git commit -m "feat(driver): ctron gate/clean 落地(W1 批3)——gate≡task+内建默认门回落(项目模式 build+fmt --check;D4 解析序 fail-closed)+clean 缓存三域收编(.cache/emit|bare|wasm;--all 加 build/pkgs;唯一合法清理口)+ps1/cmd 三面同文(Start-Process 直 exec,无 shell 中间层同表元字符拒绝);tests/tasks 门 13 断言(别名/默认门回落/用法错/清理域/旗标);门:tasks 13/0+ctron_smoke 绿+selftest 绿"
```

---

### Task 4: repo 根 `ctron.ctcl` 全量门 + ci.sh 翻转

**Files:**
- Create: `ctron.ctcl`(仓库根)
- Create: `tests/bare/gate.sh`、`tests/wasm/gate.sh`、`tests/gui/gate.sh`、`tests/gc/determinism.sh`(自 ci.sh 原文抽出)
- Modify: `ci.sh`

**Interfaces:**
- Consumes: Task 3 的 `cmd_gate`;ci.sh 现行九步(2026-10-10 版原文)。
- Produces: `ctron gate` = 本仓全量门(21 步);ci.sh 成为一行委派——GitHub CI 无感翻转。

- [ ] **Step 1: 抽出 ci.sh 内联逻辑为四个脚本文件**

各文件 = ci.sh 对应块的**逐字移植**,仅把 `"$DIR"` 自定位改为各自的 DIR/ROOT 头(house 骨架):

`tests/gc/determinism.sh`(自 ci.sh 70–82 行;T31 发射确定性双环境探针):

```sh
#!/bin/sh
# tests/gc/determinism.sh —— T31 事故回归探针(2026-10-05):编译器在 CTRON_GC=1 下
# 自身被 GC 化,发射产物必须与 off 逐字一致(s/N 槽注册+盒类 bump 的常驻守门哨)。
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$DIR")
PT=$(mktemp -d)
CTRON_STDPATH="$ROOT/lib/std" CTRON_GC=1 "$ROOT/compiler/bin/ctron-emit" run "$ROOT/compiler/build/cc_run.ct" > "$PT/emit_gc1.c" 2>/dev/null
env -u CTRON_GC CTRON_STDPATH="$ROOT/lib/std" "$ROOT/compiler/bin/ctron-emit" run "$ROOT/compiler/build/cc_run.ct" > "$PT/emit_off.c" 2>/dev/null
if ! diff -q "$PT/emit_gc1.c" "$PT/emit_off.c" > /dev/null 2>&1; then
    echo "[FAIL] 发射确定性双环境探针分歧(编译器 GC 化输出漂移;归 GC 泳道)" >&2
    rm -rf "$PT"
    exit 1
fi
rm -rf "$PT"
echo "  ok  : 发射确定性双环境探针(GC=1/off 大语料发射逐字一致)"
```

`tests/bare/gate.sh` 与 `tests/wasm/gate.sh`(自 ci.sh 38–61 行;tail 压缩包装原样保留——输出压缩是壳逻辑,留在脚本侧,task 表只调门):

```sh
#!/bin/sh
# tests/bare/gate.sh —— T40/T42 bare 档门包装(自 ci.sh 移植;tail 压缩壳)。
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$DIR")
BARE_OUT=$(mktemp)
if sh "$ROOT/tests/bare/run.sh" > "$BARE_OUT" 2>&1; then
    tail -6 "$BARE_OUT"
else
    echo "[FAIL] bare 档门红(T40/T42;见上)" >&2
    cat "$BARE_OUT" >&2
    rm -f "$BARE_OUT"
    exit 1
fi
rm -f "$BARE_OUT"
```

(wasm 版同构:把 `bare`/`T40/T42` 换为 `wasm`/`T37`,run.sh 路径换 `tests/wasm/run.sh`,FAIL 文案换"[FAIL] wasm 档门红(T37;见上)"——两文件分开写,不做参数化模板,见山是山。)

`tests/gui/gate.sh`(自 ci.sh 88–98 行;Linux 缺头显式 skip):

```sh
#!/bin/sh
# tests/gui/gate.sh —— GUI 阶梯包装(自 ci.sh 移植;headless 断言无显示依赖;
# Linux 缺 X11/GL 开发头时显式 skip 并指路,非静默假绿)。
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
RUN_GUI=1
if [ "$(uname)" = "Linux" ]; then
    if [ ! -f /usr/include/X11/Xlib.h ] || [ ! -f /usr/include/GL/gl.h ]; then
        RUN_GUI=0
    fi
fi
if [ "$RUN_GUI" = "1" ]; then
    sh "$DIR/run.sh"
else
    echo "[skip] GUI 阶梯:Linux 缺 X11/GL 开发头——apt install libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev libgl-dev 后重跑即接入(W5 环境前置)"
fi
```

- [ ] **Step 2: 写 repo 根 ctron.ctcl**

```text
// ctron.ctcl —— Ctron 仓库驱动器面(W1:全量门收编自 ci.sh;设计 2026-10-10-build-driver-design §8)
// 前置:宿主 seed 已构建(make -C compiler-c);gcc/python3 可用;fmt 对拍需 cargo(Rust 参考臂)。
ctron {
    config_version = 1
}

task "gate" {
    desc = "全量验证门禁(收编自 ci.sh 九步)"
    steps = ["python3 tests/meta_check.py", "python3 tests/manifest/run.py", "python3 tests/manifest/diff.py", "python3 tools/ctcl_check.py --selftest", "sh compiler/build.sh", "sh compiler/native.sh", "sh compiler/test/smoke.sh --full", "python3 compiler/test/suite.py", "sh tests/net/rt_core_smoke/run.sh", "sh tests/net/rt_reactor_smoke/run.sh", "sh tests/dist/ctron_smoke.sh", "sh tests/plugins/run.sh", "sh tests/bare/gate.sh", "sh tests/wasm/gate.sh", "sh compiler/bench.sh", "sh compiler/test/bench_ffi.sh", "sh tests/gc/bench.sh", "sh tests/gc/determinism.sh", "sh tests/lang/bench/bench.sh", "sh tests/gui/gate.sh", "sh tests/fmt/parity.sh"]
}

task "fast" {
    desc = "快门(静态+清单+解析器自检,不碰编译)"
    steps = ["python3 tests/meta_check.py", "python3 tests/manifest/run.py", "python3 tests/manifest/diff.py", "python3 tools/ctcl_check.py --selftest"]
}
```

(bench 三族让位 tail 压缩改直通——CI 日志变长是已知代价,在册;[6/9]–[9/9] 的 echo 标签由驱动器的「步骤 N/21」进度行替代。)

- [ ] **Step 3: 翻转 ci.sh**

`ci.sh` 全文替换为:

```sh
#!/bin/sh
# ci.sh —— 全量验证门禁(W1 起委派 ctron gate;九步编排已迁仓库根 ctron.ctcl。
# 前置:宿主 seed 已构建(make -C compiler-c);gcc/python3 可用;fmt 对拍需 cargo。)
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec sh "$DIR/ctron" gate
```

- [ ] **Step 4: 验证(分层,先轻后重)**

轻门(秒级,必跑):

```bash
sh -n ci.sh && sh -n tests/gc/determinism.sh && sh -n tests/bare/gate.sh && sh -n tests/wasm/gate.sh && sh -n tests/gui/gate.sh
python3 tools/ctcl_check.py ctron.ctcl
printf '%s\n' "--- fmt 幂等 ---" && python3 tools/ctcl_check.py --fmt ctron.ctcl > /tmp/ctron_ctcl_fmt && diff ctron.ctcl /tmp/ctron_ctcl_fmt && echo "fmt 幂等 ✓"
sh ctron task
sh ctron task fast
```

Expected: `ctron.ctcl` PASS(E=0)、fmt 幂等、task 列出 gate/fast、fast 门绿。

重门(全量,十几分钟级;落库前必跑一遍):

```bash
sh ctron gate
```

Expected: 21 步全过,末行 `ctron: task gate 完成(21 步)`。任一步红 = 修后重跑该步起(门语义:败即停,续跑用 `ctron task <子门>` 或临时注释步骤——不要留注释态落库)。

- [ ] **Step 5: Commit**

```bash
git add ctron.ctcl ci.sh tests/gc/determinism.sh tests/bare/gate.sh tests/wasm/gate.sh tests/gui/gate.sh
git commit -m "feat(driver): 仓库全量门收编进 ctron gate(W1 批4)——repo 根 ctron.ctcl 立表(gate 21 步=ci.sh 九步全量+fast 静态快门;bench 直通让位 tail 压缩,CI 日志变长在册)+ci.sh 内联逻辑四件抽出(tests/gc/determinism.sh 发射确定性探针+tests/bare|wasm|gui/gate.sh 包装壳,逐字移植)+ci.sh 翻转为一行 exec ctron gate(GitHub CI 无感);门:ctcl_check 过+fmt 幂等+tasks 门绿+ctron gate 21/21 全过"
```

- [ ] **Step 6: 推送后盯 CI,绿则回写计划档**

`git push origin main`(推送前再核一次 `git status` 与 HEAD 归属——共享树纪律);GitHub CI 九门全绿后,在本文件 Task 4 勾选框打勾并在文末「执行记录」补一行(日期 + CI run id)。CI 红则按红点归因修复后重推(注意:对端机刷可能已汇流 main——先 `git pull --rebase` 再修)。

---

## 后续分期(另立计划,本档不展开)

| 期 | 触发 | 展开处 |
|---|---|---|
| W2 收编开闸(试点域 run.sh → task 表;meta_check 扫私清缓存) | W1 CI 绿 | 另立 `docs/superpowers/plans/YYYY-MM-DD-build-driver-w2.md`,试点域选定(建议 examples 子集 + tests/plugins)在 W2 计划首轮定 |
| W3 std 前置件三件(env 写 / 递归 walk / proc 门面转发) | 随时可动,与 W1/W2 无依赖 | 各自独立 std 泳道批次,不搭车 |
| W4 原生驱动器(std/config L1 通用面 → compiler/driver/ 逐族对拍 → 垫片翻转) | W3 齐 + W1/W2 稳 | 另立计划(对拍语料与本计划 Task 2/3 的测试面是现成基线) |
| W5 收编收官(残余 run.sh 退役;shell ≤10;文档/tour) | W4 垫片翻转后 | 另立计划 |

## 执行记录

(执行时逐任务回写:日期、批次、门结果、CI run id、偏离与原因。)
