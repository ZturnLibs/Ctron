# 工具链分发 · 计划 1:工具链与编译器面改造 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 spec(docs/superpowers/specs/2026-09-17-toolchain-distribution-design.md)中全部编译器面与本地工具链改造:版本锚、三个新内建、std 三级解析与告警分级、E5030 补面、ctron-chk 原生化、CRLF 回归钉死、ctc sh 驱动。

**Architecture:** 全部改动沿自举编译器既有四触点模式(eval 内建分派 / trans 发射 / seed rt 存根 / 发射样板),不改自举链结构;std 解析从硬编码 `../std` 改为三级查找(纯 `parse_pkg.ct` 内聚);ctc 为新 sh 驱动,包装四个原生二进制。

**Tech Stack:** Ctron(自举编译器自身源)、POSIX sh、C(seed rt 与发射样板)。

## Global Constraints

- 自举源(`compiler/src/*.ct`)语法纪律:**无 `||` 中缀**(用 `or2`/`or3` 助手);**单行 if 块内不容 `return`**(含 return 的块一律多行);字符串内 `{` 写 `\{`(只有 `\{` 是合法转义);宿主 seed 是最严口径(构建过 ≠ 宿主可跑)。
- **黄金逐字不变**:本计划任何改动不得改变 `selfhosted/input_cc{,2,3}` 黄金输出与既有夹具的 seed==native 逐字一致性;每任务收尾跑 `sh compiler/test/smoke.sh`(必要时 `--full`)。
- **工作树前置**:仓库当前有未提交的 FFI 批次改动(rt_core.c、sem_main.ct、trans_*.ct 等)。**Task 0 必须先处理**,否则后续 `git add` 会把在途工作混入本线提交。
- 版本串:源 `git describe --tags --always`(失败回退 `0.1.0-dev`),经 `ANCHORVERSION` 锚由 build.sh 注入。
- rc 约定(贯穿 ctc):`0` 成功 / `1` 程序诊断失败 / `2` ctc 环境或用法错误。
- 新内建的 C 符号一律 `ctron_` 前缀:`ctron_exe_path`、`ctron_env_get`、`ctron_cli_flag`(Ctron 侧调用名分别为 `ctron_exe_path` / `env_get` / `ctron_cli_flag`)。
- seed 侧三个新内建**恒返回空串**(镜像 `ctron_entry` 先例):seed 路径走锚回落,行为与 ctc.sh 现状逐字兼容。

---

### Task 0: 前置门——工作树处置与基线

**Files:** 无新增;只读检查与 git 操作。

- [ ] **Step 1: 检查工作树**

Run: `git status --short`
预期:存在 FFI 批次在途改动(编译器 src、tests/ffi 等)。

- [ ] **Step 2: 处置在途工作**

若在途改动属于用户另一批次:**询问用户**是否先行提交该批次(推荐)或允许 stash;未经确认不得带着混入本计划的第一个提交。若用户已提交,继续。

- [ ] **Step 3: 基线全绿**

Run: `sh ci.sh`
预期:6 步全部通过(CI: 全部通过 ✓)。这是后续一切对照的基线。

---

### Task 1: 发射样板 v6——版本全局、`--version`、argv 旗标、_WIN32 UTF-8

**Files:**
- Modify: `compiler/src/driver_emit.ct`(样板区 ~148-155 行与 main 模板 ~454、456 行)
- Modify: `compiler/build.sh`(尾部对三产物做 ANCHORVERSION sed)

**Interfaces:**
- Produces: 每个发射产物获得 C 全局 `ctron_version`(build.sh 烘焙)与 main 分支 `--version`;C 帮手 `ctron_cli_fmt/ctron_cli_prof/ctron_cli_trusted` + `ctron_cli_flag(name)`(Task 2/3 消费);`--format=|--profile=|--trusted` argv 透传(i≥3)。

- [ ] **Step 1: driver_emit.ct 样板区加版本与旗标全局**

在 `println("static const char* ctron_read_file_cli(...)")` 一行(约 155 行)之后追加(注意 Ctron 字符串内 `\{` 转义纪律,`\"` 同理):

```ctron
        println("static const char* ctron_version = \"ANCHORVERSION\";")
        println("static const char* ctron_cli_fmt = NULL;")
        println("static const char* ctron_cli_prof = NULL;")
        println("static const char* ctron_cli_trusted = NULL;")
        println("static const char* ctron_cli_flag(const char* n) \{ if (!strcmp(n, \"format\")) \{ return ctron_cli_fmt ? ctron_cli_fmt : \"\"; \} if (!strcmp(n, \"profile\")) \{ return ctron_cli_prof ? ctron_cli_prof : \"\"; \} if (!strcmp(n, \"trusted\")) \{ return ctron_cli_trusted ? ctron_cli_trusted : \"\"; \} return \"\"; \}")
```

- [ ] **Step 2: 两个 main 模板加 --version 分支与旗标解析**

把 454 行(run 型 main)整行替换为:

```ctron
                println("int main(int argc, char** argv) \{ " + scall + "if (argc >= 2 && !strcmp(argv[1], \"--version\")) \{ puts(ctron_version); return 0; \} \{ int ctron_ai; for (ctron_ai = 3; ctron_ai < argc; ctron_ai++) \{ if (!strncmp(argv[ctron_ai], \"--format=\", 9)) \{ ctron_cli_fmt = argv[ctron_ai] + 9; \} else if (!strncmp(argv[ctron_ai], \"--profile=\", 10)) \{ ctron_cli_prof = argv[ctron_ai] + 10; \} else if (!strcmp(argv[ctron_ai], \"--trusted\")) \{ ctron_cli_trusted = \"1\"; \} \} \} #if defined(_WIN32) SetConsoleOutputCP(CP_UTF8); #endif if (argc >= 3 && strcmp(argv[1], \"run\") == 0) \{ ctron_cli_input = argv[2]; \} return (int)t_main(); \}")
```

456 行(test 型 main)同样在 `<scall>` 后插入同一段 `--version` + 旗标 for 循环 + `#if defined(_WIN32) ... #endif`,保留其原有 `testcalls` 与 `return 0;` 尾部结构。

- [ ] **Step 3: build.sh 注入版本**

`build.sh` 尾部 `echo` 行之前追加:

```sh
# ANCHORVERSION 注入:三产物同源版本串(黄金语料不含该锚,逐字不受影响)
VER=$(git -C "$DIR/.." describe --tags --always 2>/dev/null || echo "0.1.0-dev")
for P in cc_run cc_check cc_emit; do
    sed "s|ANCHORVERSION|$VER|" "$OUT/$P.ct" > "$OUT/$P.ct.tmp" && mv "$OUT/$P.ct.tmp" "$OUT/$P.ct"
done
```

- [ ] **Step 4: 验证——黄金不变 + 原生 --version 生效**

Run: `sh compiler/build.sh && sh compiler/test/smoke.sh`
预期:全绿(锚替换对黄金语料无感)。

Run: `sh compiler/native.sh && compiler/bin/ctron-emit --version && compiler/bin/ctron-cc --version`
预期:两个二进制都打印 `VER` 串;`sh compiler/test/smoke.sh --full` 全绿(固定点逐字节仍成立——发射器发射的 `ANCHORVERSION` 字面量在 build.sh 已被替换,发射器源与产物同串)。

- [ ] **Step 5: Commit**

```bash
git add compiler/src/driver_emit.ct compiler/build.sh
git commit -m "feat(dist): 发射样板 v6——ctron_version 全局/--version 分支/--format|--profile|--trusted argv 透传/_WIN32 UTF-8 控制台;build.sh 锚注入"
```

---

### Task 2: 三个新内建四触点(ctron_exe_path / env_get / ctron_cli_flag)

**Files:**
- Modify: `compiler/src/eval_call.ct`(`ctron_entry` 分派臂之后,~180 行)
- Modify: `compiler/src/trans_expr.ct`(`ctron_entry` 发射臂之后,~783 行)
- Modify: `compiler/src/trans_ty.ct`(内建 Str 返回型别码表,~919 行 `read_file` 臂)
- Modify: `compiler-c/src/rt_eval.c`(`ctron_entry` 存根后,~876 行)
- Modify: `compiler/src/driver_emit.ct`(样板区,Task 1 追加段之后)
- Test: `tests/dist/fx_distinfo.ct`(新)+ 黄金对照 `tests/dist/expected/fx_distinfo.out`

**Interfaces:**
- Consumes: Task 1 的 `ctron_cli_flag` C 帮手。
- Produces: Ctron 源可调用 `ctron_exe_path() -> Str`、`env_get(name: Str) -> Str`、`ctron_cli_flag(name: Str) -> Str`;Task 4 的 std 解析消费前两者。

- [ ] **Step 1: eval_call.ct 分派臂**

`ctron_entry` 臂整块之后追加:

```ctron
    if seq2(nm, "ctron_exe_path") {
        if vals.len != 0 {
            panic("ctron_exe_path arity")
            return e4("a", env, vV2(), out)
        }
        return e4("k", env, vS(ctron_exe_path()), out)
    }
    if seq2(nm, "env_get") {
        if vals.len == 1 && vals[0][0] == "S" {
            return e4("k", env, vS(env_get(vals[0][1])), out)
        }
        panic("env_get args")
        return e4("k", env, vS(""), out)
    }
    if seq2(nm, "ctron_cli_flag") {
        if vals.len == 1 && vals[0][0] == "S" {
            return e4("k", env, vS(ctron_cli_flag(vals[0][1])), out)
        }
        panic("ctron_cli_flag args")
        return e4("k", env, vS(""), out)
    }
```

(调用 Ctron 侧名字为 `env_get`,eval 臂内直呼 `env_get(...)`——**与 seed 注册名同串**(seed builtin 表按调用名解析,ctron_entry 先例);发射面由 trans_expr 映射到 `ctron_env_get`,C 符号前缀纪律不受影响。`ctron_exe_path`/`ctron_cli_flag` 源名与 seed 注册名本就相同,无此问题。)

- [ ] **Step 2: trans_expr.ct 发射臂**

`if callee == "ctron_entry" { return "ctron_entry()" }` 之后追加:

```ctron
        if callee == "ctron_exe_path" { return "ctron_exe_path()" }
        if callee == "ctron_cli_flag" { return "ctron_cli_flag(" + ct_call_args(ag, 1, env, file, "") + ")" }
        if callee == "env_get" { return "ctron_env_get(" + ct_call_args(ag, 1, env, file, "") + ")" }
```

- [ ] **Step 3: trans_ty.ct 型别码**

`if cal[1] == "read_file" { return "s" }` 同层追加三行:

```ctron
            if cal[1] == "ctron_exe_path" { return "s" }
            if cal[1] == "env_get" { return "s" }
            if cal[1] == "ctron_cli_flag" { return "s" }
```

(先读该函数上下文确认匹配形态是 `cal[1]`,与 read_file 臂同构。)

- [ ] **Step 4: seed rt_eval.c 存根(恒空串)**

`ctron_entry` 存根块之后追加:

```c
            if (!strcmp(nm, "ctron_exe_path")) {
                val o = {0};
                o.k = V_STR;
                o.s = "";
                return o;
            }
            if (!strcmp(nm, "env_get")) {
                val o = {0};
                o.k = V_STR;
                o.s = "";
                return o;
            }
            if (!strcmp(nm, "ctron_cli_flag")) {
                val o = {0};
                o.k = V_STR;
                o.s = "";
                return o;
            }
```

(恒空串 = ctron_entry 先例:seed 无 CLI 概念、无 exe 布局,一切走锚回落;env_get 在 seed 恒空意味着 ctc.sh 开发路径不受 CTRON_STDPATH 影响——符合"回落路径维持现状"。)

- [ ] **Step 5: driver_emit.ct 发射侧实现与条件 include**

(a) include 区(`#include <time.h>` 行后)追加:

```ctron
            println("#if defined(_WIN32)")
            println("#include <windows.h>")
            println("#endif")
            println("#if defined(__APPLE__)")
            println("#include <mach-o/dyld.h>")
            println("#endif")
            println("#if defined(__linux__)")
            println("#include <unistd.h>")
            println("#endif")
```

(b) Task 1 追加段之后:

```ctron
        println("static char ctron_exe_buf[4096];")
        println("static const char* ctron_exe_path(void) \{")
        println("#if defined(_WIN32)")
        println("DWORD ctron_en = GetModuleFileNameA(NULL, ctron_exe_buf, 4096); return ctron_en > 0 ? ctron_exe_buf : \"\";")
        println("#elif defined(__APPLE__)")
        println("uint32_t ctron_esz = 4096; return _NSGetExecutablePath(ctron_exe_buf, &ctron_esz) == 0 ? ctron_exe_buf : \"\";")
        println("#elif defined(__linux__)")
        println("long ctron_el = readlink(\"/proc/self/exe\", ctron_exe_buf, 4095); if (ctron_el > 0) \{ ctron_exe_buf[ctron_el] = 0; return ctron_exe_buf; \} return \"\";")
        println("#else")
        println("return \"\";")
        println("#endif")
        println("\}")
        println("static const char* ctron_env_get(const char* n) \{ const char* v = getenv(n); return v ? v : \"\"; \}")
```

- [ ] **Step 6: 夹具 + 黄金**

`tests/dist/fx_distinfo.ct`:

```ctron
// fx_distinfo —— 分发三内建:seed 恒空/native 实值
fn main() {
    if ctron_exe_path().len > 0 {
        println("exe:nonempty")
    } else {
        println("exe:empty")
    }
    println("env:" + env_get("CTRON_DIST_PROBE").len.to_string())
    println("flag:" + ctron_cli_flag("format").len.to_string())
}
```

期望(seed 侧,`compiler/ctc.sh tests/dist/fx_distinfo.ct`):`exe:empty` / `env:0` / `flag:0`。
期望(native 侧,`CTRON_DIST_PROBE=xyz compiler/bin/ctron-cc run tests/dist/fx_distinfo.ct --format=json`):`exe:nonempty` / `env:3` / `flag:4`。
(用长度与空非空断言而非内容断言——exe 路径随安装位置变化。)

- [ ] **Step 7: 验证——构建、冒烟、双面对拍**

Run: `sh compiler/build.sh && make -C compiler-c && compiler/ctc.sh tests/dist/fx_distinfo.ct`
预期:输出 `exe:0` `env:0` `flag:0`(seed 恒空)。

Run: `sh compiler/native.sh && CTRON_DIST_PROBE=xyz compiler/bin/ctron-cc run tests/dist/fx_distinfo.ct --format=json`
预期:`exe:1` `env:3` `flag:4`。

Run: `sh compiler/test/smoke.sh --full && python3 compiler/test/suite.py`
预期:全绿(三内建对既有语料无感)。

- [ ] **Step 8: Commit**

```bash
git add compiler/src/eval_call.ct compiler/src/trans_expr.ct compiler/src/trans_ty.ct compiler-c/src/rt_eval.c compiler/src/driver_emit.ct tests/dist/
git commit -m "feat(dist): ctron_exe_path/env_get/ctron_cli_flag 三内建四触点——seed 恒空回落、native 实值;三平台 exe 路径 #ifdef"
```

---

### Task 3: driver_check 锚改旗标回退

**Files:**
- Modify: `compiler/src/driver_check.ct`(207、233 行附近)

**Interfaces:**
- Consumes: Task 2 的 `ctron_cli_flag`。
- Produces: 原生 `ctron-chk` 二进制接受 `--format=json|--profile=bare|--trusted`(Task 5 消费);seed 路径 ctc.sh sed 锚行为**逐字不变**。

- [ ] **Step 1: 三处锚读改回退式(一次改齐)**

207-208 行附近改为(注意 `sem_walk2` 调用换用 `chk_prof`;fmt 判定换成 or2):

```ctron
            var chk_fmt = ctron_cli_flag("format")
            if chk_fmt.len == 0 {
                chk_fmt = "ANCHORFMT"
            }
            var chk_prof = ctron_cli_flag("profile")
            if chk_prof.len == 0 {
                chk_prof = "ANCHORPROFILE"
            }
            var sems = sem_walk2(file, chk_prof)
            if or2(seq2(chk_fmt, "1"), seq2(chk_fmt, "json")) {
```

trusted(233 行)同构:`var chk_trusted = ctron_cli_flag("trusted")`,空则回退 `"ANCHORTAUSTED"`,比较 `seq2(chk_trusted, "1")`。

- [ ] **Step 2: 验证——seed 路径黄金不变**

Run: `compiler/ctc.sh check compiler/build/cc_run.ct && compiler/ctc.sh check compiler/build/cc_run.ct --format=json | head -3`
预期:文本面 `check OK decls=N`(N 与改动前一致);JSON 面 `"code"` 字段正常。`sh compiler/test/smoke.sh` 全绿。

- [ ] **Step 3: Commit**

```bash
git add compiler/src/driver_check.ct
git commit -m "feat(dist): check 驱动锚改旗标回退——seed 走 ANCHORFMT sed、native 走 ctron_cli_flag,双口径同源"
```

---

### Task 4: std 三级解析 + 告警分级 + E5030 补面

**Files:**
- Modify: `compiler/src/parse_pkg.ct`(std 路径块 ~171 行、E5030 kind 集 ~255-270 行)
- Test: `tests/modules/stdpath/src/main.ct`、`tests/modules/dup_static/src/main.ct`(新;登记方式先读 `compiler/test/suite.py` 的 modules 小节,按 `visibility/` 用例同构登记)

**Interfaces:**
- Consumes: Task 2 的 `ctron_exe_path`/`env_get`;宿主级 `fs_exists`(既有)。
- Produces: `pkg_std_root(dir) -> Str`、`pkg_std_installed(dir) -> Bool`(同文件内 fn,Task 7 的 ctc 不直接消费);std 解析顺序 ①`CTRON_STDPATH` ②exe 旁 `../lib/ctron/std` ③回落 `../std`。

- [ ] **Step 1: 先查 std 内部 static/const 私有名碰撞面**

Run: `grep -hn "^static let\|^static const" std/*.ct std/**/*.ct 2>/dev/null | awk '{print $3}' | sort | uniq -d`
预期:若输出非空,E5030 补面会使同用两模块的语料爆新错——逐个重命名碰撞的 std 私有名(私有,安全),并记录到提交说明。

- [ ] **Step 2: parse_pkg.ct 加两个助手(pkg_load_use 之前)**

```ctron
// pkg_std_root —— std 根三级查找:①CTRON_STDPATH ②exe 旁 ../lib/ctron/std ③回落 ../std(种子约定)
fn pkg_exe_dir(exe: Str) -> Str {
    var i = exe.len - 1
    while i >= 0 {
        if byte_at(exe, i) == 47 {
            return byte_slice(exe, 0, i)
        }
        i -= 1
    }
    return ""
}

fn pkg_std_installed(dir: Str) -> Bool {
    if env_get("CTRON_STDPATH").len > 0 {
        return true
    }
    return ctron_exe_path().len > 0
}

fn pkg_std_root(dir: Str) -> Str {
    var envp = env_get("CTRON_STDPATH")
    if envp.len > 0 {
        return envp
    }
    var exe = ctron_exe_path()
    if exe.len > 0 {
        var root = pkg_exe_dir(exe) + "/../lib/ctron/std"
        if fs_exists(root + "/str.ct") {
            return root
        }
    }
    return dir + "/../std"
}
```

- [ ] **Step 3: std 路径块改走 pkg_std_root + 缺失告警分级**

`if isstd { rel = "/../std" }` 起的路径拼接块改为:

```ctron
            var mpath = ""
            if isstd {
                mpath = pkg_std_root(dir)
                var j2: I32 = 2
                while j2 < segs.len {
                    mpath = mpath + "/" + segs[j2]
                    j2 += 1
                }
                mpath = mpath + ".ct"
                if !fs_exists(mpath) {
                    if pkg_std_installed(dir) {
                        diags.push("W8901: std 模块缺失:" + mpath + "(安装损坏或 CTRON_STDPATH 配错)")
                        return out
                    }
                }
            } else {
                var rel = ""
                var j2b: I32 = 2
                while j2b < segs.len {
                    rel = rel + "/" + segs[j2b]
                    j2b += 1
                }
                mpath = dir + rel + ".ct"
            }
```

(装机路径缺失 → W8901 诊断、rc=1——驱动把 W 码与 E 码同视,而 use std.* + std 缺失本就无从编译,响亮失败是正确口径;回落路径缺失维持静默,种子语料兼容。)

- [ ] **Step 4: E5030 kind 集补 Static/Const**

合并去重两处 kind 判定(merged 侧与 mf 侧的 or2 链)各追加:

```ctron
or2(merged[j5][0] == "Static", merged[j5][0] == "Const")
or2(mf[j4][0] == "Static", mf[j4][0] == "Const")
```

(沿既有嵌套 or2 结构并入最外层;两侧同步——任一侧在集内且同名即 dup。)

- [ ] **Step 5: 夹具**

`tests/modules/stdpath/src/main.ct`:

```ctron
// stdpath —— CTRON_STDPATH 覆盖生效(套餐根指向 tests/modules/stdpath/fakestd)
use std.str.{words}

fn main() {
    println(words("a b").len.to_string())
}
```

`tests/modules/stdpath/fakestd/str.ct`(最小仿 std;run 时 `CTRON_STDPATH=<repo>/tests/modules/stdpath/fakestd` 断言命中):

```ctron
pub fn words(s: Str) -> List[Str] {
    var out = List[Str]()
    out.push(s)
    return out
}
```

(断言:设 env 后输出 `1`(fakestd 的 words 不分词),不设 env 输出 `2`(真 std 分词)——两态差分即证明解析顺序生效。)

`tests/modules/dup_static/src/main.ct`(E5030 补面负例):

```ctron
use app.duplib.{marker}

static let marker: I32 = 1

fn main() {
    println(marker.to_string())
}
```

`tests/modules/dup_static/src/duplib.ct`:

```ctron
pub fn marker() -> I32 {
    return 7
}
```

(期望:rc=1,诊断含 `E5030: use 导入同名 decl:marker`。)

- [ ] **Step 6: 验证——新负例拦截 + 全量不破**

Run: `compiler/ctc.sh tests/modules/dup_static/src/main.ct; echo rc=$?`
预期:诊断含 E5030,rc=1。

Run: `CTRON_STDPATH=$PWD/tests/modules/stdpath/fakestd compiler/ctc.sh run tests/modules/stdpath/src/main.ct`
预期:`1`。不带 env 重跑预期 `2`。

Run: `python3 compiler/test/suite.py && sh compiler/test/smoke.sh --full`
预期:modules 7/7(加新件后按 suite.py modules 小节口径为 7+2)、suite 全对齐、smoke 全绿。

- [ ] **Step 7: Commit**

```bash
git add compiler/src/parse_pkg.ct tests/modules/stdpath/ tests/modules/dup_static/
git commit -m "feat(dist): std 三级解析(STDPATH/exe 旁/回落)+ W8901 装机缺失告警分级 + E5030 补 Static/Const 面"
```

---

### Task 5: native.sh 出 ctron-chk(检查驱动原生化)

**Files:**
- Modify: `compiler/native.sh`

**Interfaces:**
- Consumes: Task 3 的旗标回退(cc_check.ct 已可在无 sed 下作为原生源工作,锚即安全默认)。
- Produces: `bin/ctron-chk`(`<bin> run <file> [--format=json|--profile=bare|--trusted]`)。

- [ ] **Step 1: native.sh 追加第三件**

`ctron-emit` 段之后追加:

```sh
"$DIR/ctc.sh" emit "$DIR/build/cc_check.ct" "$TMP/ctron_chk.c" > /dev/null
cc -O2 -w -o "$BIN/ctron-chk" "$TMP/ctron_chk.c"
echo "native: bin/ctron-chk(检查驱动)← $(wc -l < "$TMP/ctron_chk.c") 行 C"
```

- [ ] **Step 2: 验证**

Run: `sh compiler/native.sh && compiler/bin/ctron-chk run compiler/build/cc_run.ct`
预期:`check OK decls=N`(N 与 seed 面一致)。

Run: `compiler/bin/ctron-chk run compiler/build/cc_run.ct --format=json | head -2 && compiler/bin/ctron-chk run selfhosted/input_cc_neg.ct; echo rc=$?`
预期:JSON 面含 `"code"`;负例 rc=1。

- [ ] **Step 3: Commit**

```bash
git add compiler/native.sh
git commit -m "feat(dist): ctron-chk 原生化——检查驱动第三件,旗标走 ctron_cli_flag"
```

---

### Task 6: CRLF 回归夹具(双侧已支持,钉死)

**Files:**
- Test: `tests/06_crlf.ct`(新;按 tests/README.md 的标记语义登记,正例断言输出)

**背景**:`compiler/src/lex.ct` 扫描循环已把 b==13 与空格同列跳过;`compiler-c/src/lexer.c:92` 同。spec 第 8 项"lex \r 过滤"**无需代码改动**,本任务只补夹具防回归。

- [ ] **Step 1: 造 CRLF 夹具**

```bash
printf 'fn main() {\r\n    println("crlf ok")\r\n}\r\n' > tests/06_crlf.ct
printf 'crlf ok\n' > tests/expected/06_crlf.out
```

- [ ] **Step 2: 验证双面**

Run: `compiler/ctc.sh tests/06_crlf.ct && sh compiler/native.sh && compiler/bin/ctron-cc run tests/06_crlf.ct`
预期:两侧均输出 `crlf ok`。

Run: `python3 compiler/test/suite.py`
预期:记分卡 51→52 件全对齐。

- [ ] **Step 3: Commit + spec 同步**

```bash
git add tests/06_crlf.ct tests/expected/06_crlf.out
git commit -m "test(dist): CRLF 源回归夹具——词法双侧已支持,钉死防回归"
```

同时把 spec §5 第 8 项改为:"~~lex \r 过滤~~ 已满足(双侧词法既有),本项仅 CRLF 回归夹具(已落 tests/06_crlf.ct)"。

---

### Task 7: ctc sh 驱动(统一用户入口)

**Files:**
- Create: `ctc`(仓库根,chmod +x)
- Test: 手工冒烟序列(本任务内联)

**Interfaces:**
- Consumes: `bin/ctron-{cc,chk,emit}`(同目录,dev 场景 = `compiler/bin/`;安装场景 = `<prefix>/ctron/bin/`)、Task 4 的 std 解析(驱动零 std 逻辑)。
- Produces: `ctc run|check|build|test|new|help|--version`;无 cc 失败路径;rc 0/1/2。Plan 2 的 ctc.ps1 以本文件为命令面基准。

- [ ] **Step 1: 写入完整脚本**

```sh
#!/bin/sh
# ctc —— Ctron 工具链用户驱动(v0.1;命令面基准,ctc.ps1 与此同文)
# rc 约定:0 成功 / 1 程序诊断失败 / 2 ctc 环境或用法错误
set -u
BIN=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$BIN")
CC_BIN=${CC:-cc}

die2() { echo "ctc: $*" >&2; exit 2; }

usage() {
cat <<'USAGE'
ctc —— Ctron 工具链驱动
用法:
  ctc run <file.ct>          解释执行(不需要 C 编译器)
  ctc check <file.ct> [--format=json] [--profile=bare]
                             静态检查
  ctc build <file.ct>        发射 C → 本机 cc → 可执行 <stem>(C 侧 <stem>.c)
  ctc build                  项目模式:读 Ctron.toml(入口 src/main.ct,链接 c_src/*.c)
  ctc test <file.ct>         test 块执行(无 main 走解释;含 main 文件的 test 执行挂账)
  ctc new <dir>              脚手架:hello + Ctron.toml
  ctc --version              版本
  ctc --help | help [cmd]    帮助(亦可 ctc <cmd> --help)
环境变量:CC(默认 cc)、CTRON_STDPATH(覆盖标准库位置)
rc 约定:0 成功 / 1 程序诊断失败 / 2 ctc 环境或用法错误
USAGE
}

help_cmd() {
case $1 in
    build) cat <<'H'
ctc build —— 发射 C 并编译为可执行
  ctc build <file.ct>   产物 <stem> 与 <stem>.c(与源同目录)
  ctc build             项目模式:入口 src/main.ct,产物 build/<name>;
                        Ctron.toml 声明的 c_src/*.c 一并链接
前置:本机 C 编译器(可用 CC 覆盖);缺失时 C 照常发射,并给出两条出路
H
;;
    check) echo "ctc check <file.ct> [--format=json] [--profile=bare] —— 静态检查,不改任何文件";;
    run) echo "ctc run <file.ct> —— 解释执行;不需要 C 编译器";;
    test) echo "ctc test <file.ct> —— 执行 test 块(无 main 文件);含 main 文件的 test 执行暂走宿主口径挂账";;
    new) echo "ctc new <dir> —— 生成 <dir>/Ctron.toml + src/main.ct(hello)";;
    *) usage;;
esac
}

have_cc() {
    TMPCC=$(mktemp /tmp/ctc_ccp.XXXXXX)
    printf 'int main(void){return 0;}' > "$TMPCC.c"
    if "$CC_BIN" -O0 -w -o "$TMPCC" "$TMPCC.c" >/dev/null 2>&1; then
        rm -rf "$TMPCC" "$TMPCC.c"; return 0
    fi
    rm -rf "$TMPCC" "$TMPCC.c"; return 1
}

cc_missing_guidance() {
    echo "ctc: 未找到可用的 C 编译器('$CC_BIN' 冒烟失败)—— ctc build 需要。" >&2
    echo "  两条出路:" >&2
    echo "  ① 安装编译器后重跑:macOS 'xcode-select --install' / linux 发行版 gcc / windows MSYS2 或 w64devkit" >&2
    echo "  ② C 已发射到 $1 —— 可手动编译,或拿到任何有 cc 的机器上: cc -O2 -w $1 -o <名>" >&2
}

cmd_build() {
    IN=$(CDPATH= cd -- "$(dirname -- "$1")" && pwd)/$(basename -- "$1")
    STEM=$(basename "${IN%.ct}")
    OUTDIR=$(dirname "$IN")
    "$BIN/ctron-emit" run "$IN" > "$OUTDIR/$STEM.c" || exit 1
    if ! have_cc; then
        cc_missing_guidance "$OUTDIR/$STEM.c"
        exit 2
    fi
    (CDPATH= cd -- "$OUTDIR" && "$CC_BIN" -O2 -w -pthread "$STEM.c" -o "$STEM") || exit 2
    echo "ctc: 已构建 $OUTDIR/$STEM"
}

cmd_build_proj() {
    [ -f Ctron.toml ] || die2 "项目模式需 Ctron.toml"
    NAME=$(sed -n 's/^name *= *"\(.*\)"/\1/p' Ctron.toml | head -1)
    [ -n "$NAME" ] || NAME=$(basename "$PWD")
    [ -f src/main.ct ] || die2 "缺入口 src/main.ct"
    mkdir -p build
    "$BIN/ctron-emit" run "$PWD/src/main.ct" > build/$NAME.c || exit 1
    if ! have_cc; then
        cc_missing_guidance "build/$NAME.c"
        exit 2
    fi
    SRCS=""
    for C in c_src/*.c; do [ -f "$C" ] && SRCS="$SRCS $C"; done
    "$CC_BIN" -O2 -w -pthread build/$NAME.c $SRCS -o build/$NAME || exit 2
    echo "ctc: 已构建 build/$NAME"
}

[ $# -lt 1 ] && { usage; exit 2; }
CMD=$1; shift
case $CMD in
    --help|-h|help) if [ $# -ge 1 ]; then help_cmd "$1"; else usage; fi; exit 0 ;;
    --version|-V)
        if [ -f "$ROOT/VERSION" ]; then echo "ctron $(cat "$ROOT/VERSION")"; else echo "ctron dev"; fi
        exit 0 ;;
    run)
        [ $# -ge 1 ] || die2 "run 需要输入文件"
        exec "$BIN/ctron-cc" run "$1"
        ;;
    check)
        [ $# -ge 1 ] || die2 "check 需要输入文件"
        IN=$(CDPATH= cd -- "$(dirname -- "$1")" && pwd)/$(basename -- "$1"); shift
        exec "$BIN/ctron-chk" run "$IN" "$@"
        ;;
    build)
        if [ $# -ge 1 ]; then cmd_build "$1"; else cmd_build_proj; fi ;;
    test)
        [ $# -ge 1 ] || die2 "test 需要输入文件"
        exec "$BIN/ctron-cc" run "$1"
        ;;
    new)
        [ $# -ge 1 ] || die2 "new 需要目录名"
        mkdir -p "$1/src" || exit 2
        printf 'name = "%s"\n\n[caps]\n' "$(basename "$1")" > "$1/Ctron.toml"
        printf 'fn main() {\n    println("hello, ctron")\n}\n' > "$1/src/main.ct"
        echo "ctc: 已生成 $1/(ctc run $1/src/main.ct 试跑)"
        ;;
    *)
        echo "ctc: 未知子命令 '$CMD'(详见 ctc --help)" >&2
        exit 2 ;;
esac
```

- [ ] **Step 2: chmod + 冒烟**

```bash
chmod +x ctc
PATH=compiler/bin:$PATH sh -c 'ctc --version && ctc --help | head -3'
```
预期:版本串 + 用法头三行。

- [ ] **Step 3: 全子命令验证**

```bash
cd /tmp && rm -rf ctcprobe && ../ctc new ctcprobe 2>/dev/null || /path/to/repo/ctc new ctcprobe
cd ctcprobe && <repo>/ctc run src/main.ct          # hello, ctron
<repo>/ctc check src/main.ct                        # check OK decls=N
<repo>/ctc build src/main.ct && ./src/main          # hello, ctron(可执行)
<repo>/ctc build                                    # 项目模式 → build/ctcprobe
<repo>/ctc help build                               # build 详助
<repo>/ctc frobnicate; echo rc=$?                   # rc=2
```
预期:全部符合 rc 约定与输出。

- [ ] **Step 4: 无 cc 失败路径**

```bash
mkdir -p /tmp/emptycc && printf 'fn main() {\n    println(1)\n}\n' > /tmp/nc.ct
PATH=/tmp/emptycc:/usr/bin:/bin CC=cc <repo>/ctc build /tmp/nc.ct; echo rc=$?
```
预期:`nc.c` 已生成;消息含两条出路;rc=2。同环境下 `ctc run /tmp/nc.ct` 正常输出(隔离在 build 路径)。

- [ ] **Step 5: Commit**

```bash
git add ctc
git commit -m "feat(dist): ctc sh 驱动——run/check/build/test/new/help/version,无 cc 双出路预检,rc 0/1/2"
```

---

### Task 8: ci.sh 接入与收口

**Files:**
- Modify: `ci.sh`(第 4 步后追加 ctc 冒烟小节)

- [ ] **Step 1: ci.sh 追加并把各步标签统一为 /7**

原有 `[1/6]…[6/6]` 全部改为 `[1/7]…[7/7]`(顺延 ctc 冒烟为第 5 步,原 [5/6] [6/6] 后移为 [6/7] [7/7]),并在原第 4 步后插入:

```sh
echo "[5/7] ctc 驱动冒烟(run/check/build/help/无 cc 路径)"
sh "$DIR/tests/dist/ctc_smoke.sh"
```

- [ ] **Step 2: 新建 tests/dist/ctc_smoke.sh**

将 Task 7 Step 3/4 的验证序列固化成脚本(断言输出与 rc,`set -e`;临时目录收尾 trap)。native.sh 步骤已在 [4/6] 产出四件套,冒烟直接用 `compiler/bin`。

- [ ] **Step 3: 全量门禁**

Run: `sh ci.sh`
预期:7 步全绿。

- [ ] **Step 4: Commit**

```bash
git add ci.sh tests/dist/ctc_smoke.sh
git commit -m "feat(dist): ci.sh 接入 ctc 冒烟小节——驱动命令面/无 cc 路径进全量门禁"
```
