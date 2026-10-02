# T35 分层 stdlib(core < alloc < std)实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地三档分层机制——std 各模块头注声明最低档(`//@ tier:`),包清单声明所需层(`tier = "..."`),bare 档/低档包引用高档模块在 use 点得 `E3040.tier` 一次诊断(加载即止)。

**Architecture:** 门在加载器(`parse_pkg.ct`):唯一同时握有模块源文本(可提头注)与清单(可读消费方申报档)的位置;sem 侧零改动(合并后单元已无 Use 节点)。消费方有效档 = 清单 tier 键(缺省 std)经 `--profile=bare` 钳到 core;模块档 = 头注标记(缺标注 = 不设门,渐进承诺)。诊断注册进 diag_msg 双语表(zh/en,meta_check R1 守护);清单键进 schema 注册表(唯一事实源)。发射/doc 驱动走旧签名包装(manifest-only 门),不碰在飞的 driver_emit.ct。

**Tech Stack:** Ctron 自举编译器(compiler/src,seed 解释 + 自发射原生双臂)、CTCL 清单、POSIX sh 门禁(smoke.sh/suite.py)。

## Global Constraints

- 规范锚:spec §9 档位矩阵(full→`core < alloc < std`,bare→`core`)、§12 stdlib 草图、「同一 crate 的代码可声明『至少需要哪层』」、§5.4/§5.6 bare 档 E3040 机械判定;设计底稿 2026-09-23-std-tiering-design.md(主题轴 T1/T2/T3)与本件分配面轴两轴交叉——**只加标注,不动结构,勿重演大迁移**。
- **并行泳道纪律**:`compiler/src/driver_emit.ct` 正被 GC 泳道(T31 M15)占用——本件**禁改 driver_emit.ct 与 driver_doc.ct**(走旧签名包装);提交一律 pathspec 限定逐文件列出,**禁 `git add -A`**;提交前 `git status` 核对本件文件集。
- Ctron 语法铁律:无三元表达式(if 表达式替代);禁 `;`;字符串裸 `{` 须写 `\{`;字符串相等用 `seq2`,布尔或用 `or2/or3`;字节比较用数字字面量(`byte_at(s,i) == 47`);测试锚禁 `;`(E1001)。
- std 三处副本漂移门(smoke.sh 3d/3i):改 `lib/std/*.ct` 必须同步 `compiler/test/stdpkg/std/` 与 5 个 `examples/*/std/`(ctgrep/ctslink/ctwc/ctwf/web_todo),否则漂移门红。
- 诊断新变体必须 zh/en 双表同注册(meta_check R1 守护);负锚用 `//@ fail:` 前缀码匹配(变体码输出前缀取 `.` 前基础码)。
- seed 解释臂 `env_get` 是哑桩(在册):CTRON_STDPATH 在 seed 下不生效——seed 臂 std 夹具一律走**夹具本地 `std/` 目录**(fallback 路径,compiler/test/stdpkg_neg 先例);真库锚只在 native 臂(`./ctron`/bin,先 `sh compiler/native.sh` 刷新)。
- 本件零运行时/发射面改动,bench 三门禁不适用;验收 = `compiler/build.sh` + `compiler/test/smoke.sh --full` + `python3 compiler/test/suite.py` 全绿(emit 臂既有红按 diff 范围归因,peer 在飞项构造性无关,判读前先跑基线)。

---

### Task 1: 档位门机制核(清单键 + 头注提取 + use 点 E3040.tier)

**Files:**
- Modify: `compiler/src/diag_msg.ct`(zh/en 各 +2 行:E3040.tier、E5040.tier)
- Modify: `compiler/src/parse_pkg.ct`(+3 fn:pkg_manifest_tier/pkg_mod_tier/pkg_tier_rank;pkg_load_use_done 加 prof 参 + 门;两个入口 fn 穿线)
- Modify: `compiler/src/driver_check.ct:283-306`(chk_prof 提前 + 传参)
- Modify: `compiler/src/driver_run.ct:60-85`(run_prof 提取提前 + 传参)
- Create: `compiler/test/tier_neg_alloc/{Ctron.ctcl,src/main.ct,std/strx.ct}`(红锚夹具)
- Modify: `tools/ctcl_manifest_schema.ctcl`(+1 regkey)

**Interfaces:**
- Consumes: `pkg_ctcl_at(t,i,key)`/`pkg_ctcl_pre(t,i)`(parse_pkg.ct:191/202,键边界扫描先例)、`diag1(diags,key,line,a0)`(diag_msg.ct:304)、`read_file/byte_at/byte_slice/seq2/or2`(前奏)。
- Produces: `pkg_tier_rank(t: Str) -> I32`(core=0/alloc=1/std=2/他=2);`pkg_manifest_tier(dir: Str, diags: List[Str]) -> Str`(缺省 "std";越域值推 E5040.tier 后返 "std");`pkg_mod_tier(src: Str) -> Str`(缺标注返 "");`pkg_load_use_t(file, dir, stack, diags, misses, prof: Str)`(**新 fn**,check/run 专用);`pkg_load_use_m`/`pkg_load_use` 签名**不变**(P1b「独立 fn 避碰」先例——driver_doc.ct:719 以 5 参调 _m、driver_emit.ct:41 调 pkg_load_use,二者零改动)。

- [ ] **Step 1: 建红锚夹具(core 包 use alloc 层模块,期望 E3040)**

```text
compiler/test/tier_neg_alloc/Ctron.ctcl:
pkg {
    manifest_version = 1
    name = "tier_neg_alloc"
    version = "0.1.0"
    tier = "core"
}

compiler/test/tier_neg_alloc/std/strx.ct:
// std/strx.ct —— T35 夹具:alloc 档 mini 模块
//@ tier: alloc
pub fn sx_count(n: I32) -> I32 {
    var xs = List[I32]()
    xs.push(n)
    return xs.len
}

compiler/test/tier_neg_alloc/src/main.ct:
// tier_neg_alloc —— core 包 use alloc 层模块 → E3040.tier(§9 档位矩阵)
use std.strx.{ sx_count }

fn main() -> I32 {
    return sx_count(1)
}
```

- [ ] **Step 2: 跑红锚验证现状不拦(红)**

Run: `sh compiler/build.sh && compiler/ctc.sh check compiler/test/tier_neg_alloc/src/main.ct; echo rc=$?`
Expected: `check OK decls=N`(rc=0)——现状无档位门,夹具本地 std 经 fallback 命中,strx 无分配调用点于 main,体检(full 档)不红。若此处意外非 0,先归因再继续。

- [ ] **Step 3: diag_msg.ct 注册两变体(zh/en 双表)**

在 `diag_tpl_en`(E3040.noalloc 行,约 :100 之后)追加:

```ctron
    if seq2(key, "E3040.tier") { return "std module %0 exceeds this package's declared tier (see //@ tier header and manifest tier key)" }
    if seq2(key, "E5040.tier") { return "unknown manifest tier value (allowed: core/alloc/std):%0" }
```

在 `diag_tpl_zh`(E3040.noalloc 行,约 :197 之后)追加:

```ctron
    if seq2(key, "E3040.tier") { return "std 模块 %0 层级超出本包申报档(见模块头 //@ tier 与清单 tier 键)" }
    if seq2(key, "E5040.tier") { return "清单 tier 值未识别(允许 core/alloc/std):%0" }
```

- [ ] **Step 4: parse_pkg.ct 三个新 fn(置于 pkg_caps_allowed 之后、pkg_exe_dir 之前,约 :265)**

```ctron
// ---- T35 分层 stdlib(§9 档位矩阵:core < alloc < std)----
// pkg_tier_rank —— 档位序;未识别值按 std(调用方已各自验证值域)
fn pkg_tier_rank(t: Str) -> I32 {
    if seq2(t, "core") { return 0 }
    if seq2(t, "alloc") { return 1 }
    return 2
}

// pkg_manifest_tier —— 清单 pkg 块 tier 键(消费方申报档):缺省 "std"(不设限,
// 渐进承诺);越域值 = E5040.tier fail-closed(T48 清单面硬切同族)。文本扫描
// 镜像 pkg_caps_allowed(键首词边界 + = " 引号值;phase-1 仅规范形态清单)。
fn pkg_manifest_tier(dir: Str, diags: List[Str]) -> Str {
    var t = ""
    match read_file(dir + "/../Ctron.ctcl") {
        Some(x) => { t = x }
        None => { return "std" }
    }
    var i: I32 = 0
    while i < t.len {
        if pkg_ctcl_at(t, i, "tier") && pkg_ctcl_pre(t, i) {
            var j = i + 4
            while j < t.len && byte_at(t, j) == 32 {
                j += 1
            }
            if j < t.len && byte_at(t, j) == 61 {
                j += 1
                while j < t.len && byte_at(t, j) == 32 {
                    j += 1
                }
                if j < t.len && byte_at(t, j) == 34 {
                    j += 1
                    var v = ""
                    while j < t.len && byte_at(t, j) != 34 && byte_at(t, j) != 10 {
                        v = v + byte_slice(t, j, j + 1)
                        j += 1
                    }
                    if or2(seq2(v, "core"), or2(seq2(v, "alloc"), seq2(v, "std"))) {
                        return v
                    }
                    diag1(diags, "E5040.tier", "", v)
                    return "std"
                }
            }
        }
        i += 1
    }
    return "std"
}

// pkg_mod_tier —— std 模块头注档位提取:首注释块内 `//@ tier: core|alloc|std`;
// 缺标注/越域标注 = ""(不设门——渐进承诺,未标注模块不受档位面约束;
// 标注只认头部,首非注释行即止,空白行跳过不断链)。
fn pkg_mod_tier(src: Str) -> Str {
    var i: I32 = 0
    while i < src.len {
        var ls = i
        while ls < src.len && or2(byte_at(src, ls) == 32, byte_at(src, ls) == 9) {
            ls += 1
        }
        if ls >= src.len {
            return ""
        }
        var e = ls
        while e < src.len && byte_at(src, e) != 10 {
            e += 1
        }
        if ls + 1 < e && byte_at(src, ls) == 47 && byte_at(src, ls + 1) == 47 {
            // 注释行:"//" 后随 '@' 即标记形("//@ 词: 值")
            if ls + 2 < e && byte_at(src, ls + 2) == 64 {
                var w = ""
                var j = ls + 3
                while j < e && byte_at(src, j) == 32 {
                    j += 1
                }
                while j < e && byte_at(src, j) != 32 && byte_at(src, j) != 58 {
                    w = w + byte_slice(src, j, j + 1)
                    j += 1
                }
                if seq2(w, "tier") && j < e && byte_at(src, j) == 58 {
                    var j3 = j + 1
                    while j3 < e && byte_at(src, j3) == 32 {
                        j3 += 1
                    }
                    var v = ""
                    while j3 < e && byte_at(src, j3) != 32 && byte_at(src, j3) != 9 {
                        v = v + byte_slice(src, j3, j3 + 1)
                        j3 += 1
                    }
                    if or2(seq2(v, "core"), or2(seq2(v, "alloc"), seq2(v, "std"))) {
                        return v
                    }
                    return ""
                }
            }
            i = e + 1
        } else {
            if ls == e {
                // 空白行:跳过,注释块续扫
                i = e + 1
            } else {
                return ""
            }
        }
    }
    return ""
}
```

- [ ] **Step 5: pkg_load_use_done 加 prof 参 + 档位门**

签名(约 :359)改为(加第 8 参 `prof: Str`):

```ctron
fn pkg_load_use_done(file: List[Str], dir: Str, stack: List[Str], done: List[Str], diags: List[Str], top: Bool, misses: List[Str], prof: Str) -> List[Str] {
    pkg_check_caps(file, dir, diags)
    if diags.len > 0 {
        return file
    }
    // T35:消费方有效档 = 清单 tier 键(缺省 std);bare 档钳到 core(§9:bare 目标
    // stdlib 层 = core)。E5040.tier 已推则加载即止(驱动对 ldiags 非空 rc=1)。
    var tier = pkg_manifest_tier(dir, diags)
    if diags.len > 0 {
        return file
    }
    if seq2(prof, "bare") && pkg_tier_rank(tier) > 0 {
        tier = "core"
    }
```

递归调用(约 :633)改为:

```ctron
                        mf = pkg_load_use_done(mf, dir, stack2, done, diags, false, misses, prof)
```

std 分支读入后、parse 之前(`Some(ms) => {` 与 `var mf = List[Str]()` 之间,约 :544)插门:

```ctron
            Some(ms) => {
                    // T35 档位门:std 模块头注 //@ tier 高于消费方有效档 → use 点
                    // 一次 E3040.tier,加载即止(替代 merge 后 bare 体检在 std 内部
                    // 的洪水;缺标注不设门)。注:门只可能命中真模块文件——退化形
                    // `use std`(segs.len==2)的 mpath 恒不存在,不入此分支。
                    var mt = pkg_mod_tier(ms)
                    if mt.len > 0 && pkg_tier_rank(mt) > pkg_tier_rank(tier) {
                        diag1(diags, "E3040.tier", "", segs[2] + "(" + mt + ")")
                        return out
                    }
```

(保持该分支既有缩进风格;`return out` 与同分支诊断早退同款。)

- [ ] **Step 6: 入口面穿线(pkg_load_use / pkg_load_use_m 签名不动,新增 pkg_load_use_t)**

`pkg_load_use`(约 :312)与 `pkg_load_use_m`(约 :323)保持既有签名与注释,仅换内部委托;其后新增:

```ctron
fn pkg_load_use(file: List[Str], dir: Str, stack: List[Str], diags: List[Str]) -> List[Str] {
    // SL-8c-4②:gui.run 装配合成——预合并注入 use gui.{run_d} 请求,
    // 合并后改写 run(ViewCall) 调用点并拼写 __gui_bind/__gui_act/__gui_run
    var unit = List[Str]()
    var f2: List[Str] = gui_ds_premerge(file, unit)
    var out = pkg_load_use_t(f2, dir, stack, List[Str](), diags, List[Str](), "full")
    return gui_ds_postmerge(out, unit, diags)
}

// pkg_load_use_m —— pkg_load_use 带 miss 收集(W8902 带出路半面;独立 fn 避碰
// 在制 driver_emit.ct 调用点——emit 驱动 seam 待其落库后一行切换,2026-09-30 登记)
// T35:manifest-only 门(prof 恒 full;doc 驱动 5 参调用面保持零改动)
fn pkg_load_use_m(file: List[Str], dir: Str, stack: List[Str], diags: List[Str], misses: List[Str]) -> List[Str] {
    var unit = List[Str]()
    var f2: List[Str] = gui_ds_premerge(file, unit)
    var out = pkg_load_use_t(f2, dir, stack, List[Str](), diags, misses, "full")
    return gui_ds_postmerge(out, unit, diags)
}

// pkg_load_use_t —— T35 档位门入口(prof = sem_walk2 同源档位串;bare 钳 core
// 于 pkg_load_use_done 内)。独立 fn 避碰 driver_doc.ct:719 的 _m 5 参调用面
//(P1b「独立 fn 避碰」先例);check/run 两驱动先行接线,emit 随 seam 落库再切。
fn pkg_load_use_t(file: List[Str], dir: Str, stack: List[Str], diags: List[Str], misses: List[Str], prof: Str) -> List[Str] {
    var unit = List[Str]()
    var f2: List[Str] = gui_ds_premerge(file, unit)
    var out = pkg_load_use_done(f2, dir, stack, List[Str](), diags, true, misses, prof)
    return gui_ds_postmerge(out, unit, diags)
}
```

- [ ] **Step 7: driver_check.ct——chk_prof 提前到加载前并传入**

将 :298-305 的 chk_fmt/chk_prof 推导块整体**上移**至 :283(`var lmisses = ...` 之前),加载调用改传 prof:

```ctron
            var chk_fmt = ctron_cli_flag("format")
            if chk_fmt.len == 0 {
                chk_fmt = "ANCHORFMT"
            }
            var chk_prof = ctron_cli_flag("profile")
            if chk_prof.len == 0 {
                chk_prof = "ANCHORPROFILE"
            }
            var lmisses = List[Str]()
            file = pkg_load_use_t(file, pkg_dir_of(epath), lstack, ldiags, lmisses, chk_prof)
```

(:298-305 原位的推导块删除;:306 `sem_walk2(file, chk_prof)` 不变。)

- [ ] **Step 8: driver_run.ct——run_prof 提取提前到加载前并传入**

将 :82 的字面量提为变量并上移至 :66(`var lmisses = ...` 之前),加载调用改传:

```ctron
            var run_prof = "ANCHORPROFILE"
            var lmisses = List[Str]()
            file = pkg_load_use_t(file, pkg_dir_of(epath), lstack, ldiags, lmisses, run_prof)
```

(:82 改为 `var sems = sem_walk2(file, run_prof)`。)

- [ ] **Step 9: schema 注册表加 pkg.tier 键(唯一事实源)**

`tools/ctcl_manifest_schema.ctcl` 在 `regkey "pkg.caps"` 块后追加:

```ctron
regkey "pkg.tier" {
    type = "str"
    pattern = "(core|alloc|std)"   // T35 分层 stdlib:包申报档;缺省 std(缺键不校)
}
```

- [ ] **Step 10: 跑红锚验证变绿 + 值域负例**

Run: `sh compiler/build.sh && compiler/ctc.sh check compiler/test/tier_neg_alloc/src/main.ct; echo rc=$?`
Expected: 输出含 `E3040: std 模块 strx(alloc) 层级超出本包申报档…`,rc=1。

再验值域门(临时改清单 `tier = "Core"` 后跑,完事还原):
Expected: 含 `E5040: 清单 tier 值未识别…`,rc=1。

- [ ] **Step 11: 既有面零回归(本任务触碰 use 加载链,必跑)**

Run: `python3 compiler/test/suite.py 2>&1 | tail -6 && python3 tools/ctcl_check.py --selftest 2>&1 | tail -2`
Expected: suite 双宿主记分与在库基线一致(99/99 或在册既有红面不变);ctcl selftest OK。若 emit/seed 臂出现新红,先 `git stash` 本件改动复跑基线,确认是否构造性相关。

- [ ] **Step 12: Commit(pathspec 限定)**

```bash
git add compiler/src/diag_msg.ct compiler/src/parse_pkg.ct compiler/src/driver_check.ct compiler/src/driver_run.ct tools/ctcl_manifest_schema.ctcl compiler/test/tier_neg_alloc
git commit -m "feat(pkg): T35 分层 stdlib 机制核——清单 tier 键+模块头注 //@ tier 提取+use 点 E3040.tier 档位门(§9 档位矩阵 core<alloc<std;bare 档钳 core;缺标注不设门渐进承诺;E5040.tier 值域 fail-closed;driver_emit/doc 走 full 包装不接线)"
```

---

### Task 2: lib/std 首批 26 模块头注标注 + 三处副本同步

**Files:**
- Modify: `lib/std/*.ct` 全部 26 模块(行 2 插 `//@ tier: X`)
- Modify: `compiler/test/stdpkg/std/*.ct`(同步副本)
- Modify: `examples/{ctgrep,ctslink,ctwc,ctwf,web_todo}/std/*.ct`(vendored 副本)

**Interfaces:**
- Consumes: Task 1 的 `pkg_mod_tier`(认 `//@ tier:` 标记,首注释块内生效)。
- Produces: 首批档位表(下表即事实源;Task 4 真库锚对 core 集做 bare 体检自证,误标 core 会被锚打红)。

首批档位表(分配面+OS 面双轴判定;`alloc` 计数=List/Map/Set/Box 构造/push/to_string,`os`=fs/time/process/Env 面,均经注释剔除复核):

| tier | 模块 |
|---|---|
| core(5) | math hash log opt pb |
| alloc(16) | config crypto csv enc fmap heap iter json map ndjson path set sort strconv str unicode |
| std(5) | fs process rand time uuid |

- [ ] **Step 1: 逐文件行 2 插标注(标题行后、空行前;保持首注释块连续无空行隔断)**

以 str.ct 为例(其余 25 件同法,档位按上表):

```text
// std/str.ct —— Str 工具种子(words/contains/lines/count_ch;字节级,UTF-8 透传)
//@ tier: alloc
```

(core 模块写 `//@ tier: core`,std 模块写 `//@ tier: std`。)

- [ ] **Step 2: 机械同步三处副本(漂移门强制)**

```bash
cp lib/std/*.ct compiler/test/stdpkg/std/
for app in ctgrep ctslink ctwc ctwf web_todo; do cp lib/std/*.ct "examples/$app/std/"; done
```

- [ ] **Step 3: 漂移门 + stdpkg 消费门复验(既有 44+ 处消费不红)**

Run: `sh compiler/test/smoke.sh 2>&1 | grep -E "漂移|std 包|vendored|3d|3i" | head -8`
Expected: `std 规范源与种子副本一致(无漂移)`、`vendored std 与 stdpkg 同步`、std 包逐字一致绿。注:此处跑的是快路径 smoke(--full 由 Task 4 收官跑);若脚本默认全量,以实际输出为准。

- [ ] **Step 4: Commit**

```bash
git add lib/std compiler/test/stdpkg/std examples/ctgrep/std examples/ctslink/std examples/ctwc/std examples/ctwf/std examples/web_todo/std
git commit -m "feat(std): T35 首批分层标注——26 模块头注 //@ tier(core 5/alloc 16/std 5,分配面+OS 面双轴)+ stdpkg/examples 五处 vendored 副本同步(漂移门);标注渐进,域包(net/http/tls/db/gui)不在本批"
```

---

### Task 3: smoke.sh seed 臂锚段(负例族 + 正例双跑)

**Files:**
- Create: `compiler/test/tier_neg_std/`(core 包 use std 层模块 → E3040)
- Create: `compiler/test/tier_neg_val/`(清单 tier 越域值 → E5040)
- Create: `compiler/test/tier_prof_neg/`(无清单键 + --profile=bare → E3040;同夹具 full → 过:钳门精确性)
- Create: `compiler/test/tier_pos/`(core 包 use core+无标注模块 → full/bare 双过)
- Modify: `compiler/test/smoke.sh`(bare 档门之后,约 :250 前后插一段)

**Interfaces:**
- Consumes: Task 1 机制;夹具本地 `std/` fallback(seed 臂无 env 依赖);`ctc.sh check --profile=` sed 焙入链。
- Produces: smoke.sh 新段 7 断言(后续回归站岗)。

- [ ] **Step 1: 建四组夹具**

`compiler/test/tier_neg_std/`:`Ctron.ctcl` 同 Task 1(tier="core",name="tier_neg_std");`std/fsx.ct`:

```ctron
// std/fsx.ct —— T35 夹具:std 档 mini 模块
//@ tier: std
pub fn fx_ping() -> I32 {
    return 1
}
```

`src/main.ct`:

```ctron
// tier_neg_std —— core 包 use std 层模块 → E3040.tier(§9:bare→core 层)
use std.fsx.{ fx_ping }

fn main() -> I32 {
    return fx_ping()
}
```

`compiler/test/tier_neg_val/Ctron.ctcl` 用 `tier = "Core"`(越域),无 std/ 目录;`src/main.ct`:

```ctron
// tier_neg_val —— 清单 tier 越域值 → E5040.tier(fail-closed)
fn main() -> I32 {
    return 0
}
```

`compiler/test/tier_prof_neg/`:`Ctron.ctcl` **无 tier 键**;`std/strx.ct` 同 Task 1;`src/main.ct`:

```ctron
// tier_prof_neg —— bare 档钳门:无清单键(full 缺省 std)+ --profile=bare → E3040.tier
use std.strx.{ sx_count }

fn main() -> I32 {
    return sx_count(2)
}
```

`compiler/test/tier_pos/`:`Ctron.ctcl` tier="core";`std/mathx.ct` 与 `std/laxx.ct`:

```ctron
// std/mathx.ct —— T35 夹具:core 档 mini 模块
//@ tier: core
pub fn mx_add(a: I32, b: I32) -> I32 {
    return a + b
}
```

```ctron
// std/laxx.ct —— T35 夹具:无标注模块(缺标注不设门,渐进承诺)
pub fn lx_id(v: I32) -> I32 {
    return v
}
```

`src/main.ct`(体检查双证:main 本身亦须无分配):

```ctron
// tier_pos —— core 包 use core+无标注模块 → full/bare 双过(档位面+分配面双绿)
use std.mathx.{ mx_add }
use std.laxx.{ lx_id }

fn main() -> I32 {
    let a = mx_add(1, 2)
    let b = lx_id(a)
    if b == 3 {
        return 0
    }
    return 1
}
```

- [ ] **Step 2: 逐夹具验证(先手动过一遍再挂门)**

```bash
compiler/ctc.sh check compiler/test/tier_neg_std/src/main.ct; echo "rc=$?"      # E3040,rc=1
compiler/ctc.sh check compiler/test/tier_neg_val/src/main.ct; echo "rc=$?"      # E5040,rc=1
compiler/ctc.sh check compiler/test/tier_prof_neg/src/main.ct --profile=bare; echo "rc=$?"   # E3040,rc=1
compiler/ctc.sh check compiler/test/tier_prof_neg/src/main.ct; echo "rc=$?"     # 无诊断,rc=0
compiler/ctc.sh check compiler/test/tier_pos/src/main.ct; echo "rc=$?"          # 无诊断,rc=0
compiler/ctc.sh check compiler/test/tier_pos/src/main.ct --profile=bare; echo "rc=$?"        # 无诊断,rc=0
```

- [ ] **Step 3: smoke.sh 挂段(bare 档门段落之后插入;变量 $COMP/$T/$ok/$bad 沿用脚本既有约定)**

```sh
echo "== 3p5) T35 分层 stdlib(档位门:清单 tier 键+头注 //@ tier+E3040.tier) =="
"$COMP/ctc.sh" check "$COMP/test/tier_neg_alloc/src/main.ct" > "$T/tier1.out" 2>&1
if [ $? -ne 0 ] && grep -q "E3040" "$T/tier1.out"; then
    ok "T35 core 包 use alloc 层模块 = E3040.tier"
else
    bad "T35 alloc 档未拦: $(cat "$T/tier1.out")"
fi
"$COMP/ctc.sh" check "$COMP/test/tier_neg_std/src/main.ct" > "$T/tier2.out" 2>&1
if [ $? -ne 0 ] && grep -q "E3040" "$T/tier2.out"; then
    ok "T35 core 包 use std 层模块 = E3040.tier"
else
    bad "T35 std 档未拦: $(cat "$T/tier2.out")"
fi
"$COMP/ctc.sh" check "$COMP/test/tier_neg_val/src/main.ct" > "$T/tier3.out" 2>&1
if [ $? -ne 0 ] && grep -q "E5040" "$T/tier3.out"; then
    ok "T35 清单 tier 越域值 = E5040.tier(fail-closed)"
else
    bad "T35 值域未拦: $(cat "$T/tier3.out")"
fi
"$COMP/ctc.sh" check "$COMP/test/tier_prof_neg/src/main.ct" --profile=bare > "$T/tier4.out" 2>&1
if [ $? -ne 0 ] && grep -q "E3040" "$T/tier4.out"; then
    ok "T35 bare 档钳门(use 面)= E3040.tier"
else
    bad "T35 bare 钳未拦: $(cat "$T/tier4.out")"
fi
"$COMP/ctc.sh" check "$COMP/test/tier_prof_neg/src/main.ct" > "$T/tier5.out" 2>&1
if [ $? -eq 0 ]; then
    ok "T35 full 档同夹具不拦(钳门精确)"
else
    bad "T35 full 档误拦: $(cat "$T/tier5.out")"
fi
"$COMP/ctc.sh" check "$COMP/test/tier_pos/src/main.ct" > "$T/tier6.out" 2>&1
if [ $? -eq 0 ]; then
    ok "T35 core 包 use core+无标注模块 = 过"
else
    bad "T35 pos 误拦: $(cat "$T/tier6.out")"
fi
"$COMP/ctc.sh" check "$COMP/test/tier_pos/src/main.ct" --profile=bare > "$T/tier7.out" 2>&1
if [ $? -eq 0 ]; then
    ok "T35 core 包 bare 双证(档位面+分配面)"
else
    bad "T35 pos bare 误拦: $(cat "$T/tier7.out")"
fi
```

(段落号 `3p5` 按 smoke.sh 实际段落序微调;`ok/bad` 若为函数而非变量拼装,沿用邻近段落写法。)

- [ ] **Step 4: 全量 smoke 复验(T35 段 7 绿 + 既有面零回归)**

Run: `sh compiler/test/smoke.sh --full 2>&1 | tail -15`
Expected: T35 段 7 ok;总账除在册既有红(peer emit 臂)外无新红。

- [ ] **Step 5: Commit**

```bash
git add compiler/test/tier_neg_std compiler/test/tier_neg_val compiler/test/tier_prof_neg compiler/test/tier_pos compiler/test/smoke.sh
git commit -m "test(pkg): T35 档位门锚段入册——seed 臂夹具本地 std 七断言(alloc/std 档负例+值域 fail-closed+bare 钳门双跑+core 正例 full/bare 双证);smoke 3p5 段站岗"
```

---

### Task 4: native 臂——bin 刷新 + 真库锚(core 集 bare 体检自证)

**Files:**
- Create: `compiler/test/tier_real_pos/{Ctron.ctcl,src/main.ct}`(真库 core 五模块,bare 过)
- Create: `compiler/test/tier_real_neg/{Ctron.ctcl,src/main.ct}`(真库 str,tier=core → E3040 单响)
- Modify: `compiler/test/smoke.sh`(T35 段尾追加两断言)

**Interfaces:**
- Consumes: Task 2 真库标注;`sh compiler/native.sh`(自发射链刷新 bin,seed 仅首次引导);`./ctron check`(wrapper→bin/ctron-chk,真实 env_get)。
- Produces: core 档音遍性证明——真库 core 五模块全量消费 + bare 体检查零诊断;use 门单响证明(修复前 bare+use alloc 模块 = std 内部 E3040 洪水,修复后 use 点一次即止)。

- [ ] **Step 1: 刷新原生 bin(自举链)**

Run: `sh compiler/native.sh 2>&1 | tail -3`
Expected: 无错退出;`compiler/bin/ctron-cc` 等 mtime 更新。

- [ ] **Step 2: 建两组真库夹具**

`compiler/test/tier_real_pos/Ctron.ctcl`:tier="core",name="tier_real_pos";`src/main.ct`:

```ctron
// tier_real_pos —— 真库 core 五模块全量消费+bare 体检查 = core 档音遍性自证
use std.math.{ clamp, is_pow2 }
use std.hash.{ djb2 }
use std.opt.{ opt_get_i }
use std.log.{ lg_enabled, lg_info }
use std.pb.{ pb_w_varint }

fn main() -> I32 {
    let c = clamp(5, 0, 3)
    let p = is_pow2(64)
    let h = djb2("ctron")
    let o = opt_get_i(Some(7), 0)
    let e = lg_enabled(lg_info(), lg_info())
    let w = pb_w_varint()
    if c == 3 {
        if p {
            if h != 0 {
                if o == 7 {
                    if e {
                        if w >= 0 {
                            return 0
                        }
                    }
                }
            }
        }
    }
    return 1
}
```

`compiler/test/tier_real_neg/Ctron.ctcl`:tier="core",name="tier_real_neg";`src/main.ct`:

```ctron
// tier_real_neg —— 真库 alloc 档(core 包 use std.str)→ use 点 E3040 单响
use std.str.{ words }

fn main() -> I32 {
    return 0
}
```

- [ ] **Step 3: 真库锚验证**

Run:
```bash
CTRON_STDPATH="$PWD/lib/std" ./ctron check compiler/test/tier_real_pos/src/main.ct --profile=bare; echo "rc=$?"
CTRON_STDPATH="$PWD/lib/std" ./ctron check compiler/test/tier_real_neg/src/main.ct; echo "rc=$?"
```
Expected: 第一条 rc=0 零诊断(**若红:某 core 模块实有分配面——将该模块降档 alloc,回 Task 2 表修正并同步副本,重跑**);第二条恰一条 `E3040: std 模块 str(alloc) 层级超出本包申报档…` rc=1,无后续洪水。

- [ ] **Step 4: smoke.sh 追加两断言(T35 段尾)**

```sh
CTRON_STDPATH="$ROOT/lib/std" "$ROOT/ctron" check "$COMP/test/tier_real_pos/src/main.ct" --profile=bare > "$T/tier8.out" 2>&1
if [ $? -eq 0 ]; then
    ok "T35 真库 core 五模块 bare 体检自证(native 臂)"
else
    bad "T35 真库 core 自证红: $(cat "$T/tier8.out")"
fi
CTRON_STDPATH="$ROOT/lib/std" "$ROOT/ctron" check "$COMP/test/tier_real_neg/src/main.ct" > "$T/tier9.out" 2>&1
if [ $? -ne 0 ] && grep -q "E3040" "$T/tier9.out"; then
    ok "T35 真库 use 门单响(E3040.tier,加载即止)"
else
    bad "T35 真库 use 门异常: $(cat "$T/tier9.out")"
fi
```

- [ ] **Step 5: 全量门禁收官**

Run: `python3 compiler/test/suite.py 2>&1 | tail -6 && sh compiler/test/smoke.sh --full 2>&1 | tail -8 && python3 tests/meta_check.py 2>&1 | tail -3`
Expected: suite 与基线一致;smoke 全绿(除在册既有红);meta_check(双语表守护)OK。

- [ ] **Step 6: Commit**

```bash
git add compiler/test/tier_real_pos compiler/test/tier_real_neg compiler/test/smoke.sh
git commit -m "test(pkg): T35 真库锚——native 臂 core 五模块 bare 体检自证(core 档音遍性)+ tier=core use std.str 单响(修复前 std 内部 E3040 洪水 → use 点一次即止);smoke T35 段 9 断言"
```

---

### Task 5: 入册——宪章档位轴 + 计划卡翻完成 + COVERAGE + parity 债登记

**Files:**
- Modify: `lib/std/README.md`(分层宪章补档位轴节)
- Modify: `docs/superpowers/plans/2026-09-28-spec-gap-execution-plan.md`(T35 卡片状态/总表翻完成)
- Modify: `tests/COVERAGE.md`(工作志一行)

**Interfaces:**
- Consumes: 前四任务事实。
- Produces: 文档事实源与登记债(三线 parity:compiler-c/src/pkg.c 清单键解析、compiler-c/src/sem.c bare 档口径、compiler-rust/src/main.rs Manifest 字段 + sem.rs Profile——本件 Ctron 线先行,C/R 线随其泳道补齐,消费面零依赖)。

- [ ] **Step 1: lib/std/README.md 宪章补「档位轴」短节**

在分层与准入章之后追加(措辞对齐宪章 v2 风格,不推翻两轴既有条款):

```markdown
## 档位轴(T35,2026-10-02)

分层宪章按「主题/依赖面」分 T1/T2/T3;档位按「分配面」分三档,两轴交叉互不替代:

- `core`(全档位可用):无 GC 分配、无 OS 面——math/hash/log/opt/pb;
- `alloc`:GC 分配面(List/String/Box)——str/map/set/heap/json 等 16 模块;
- `std`:OS 服务面(fs/time/process/rand/uuid)。

机制:模块头注 `//@ tier: core|alloc|std`(缺标注 = 不设门,渐进收紧);包清单
`tier = "core|alloc|std"` 声明申报档(缺省 std);use 点档位越界 = E3040(变体
.tier)一次诊断、加载即止;`--profile=bare` 将有效档钳到 core(spec §9:bare 目标
stdlib 层 = core)。域包(net/http/tls/db/gui)暂不在标注面(缺标注不设门),
随 registry 泳道渐进入册。
```

- [ ] **Step 2: T35 卡片翻完成**

`docs/superpowers/plans/2026-09-28-spec-gap-execution-plan.md`:
- :426 状态行 `**状态:** 待办` → `**状态:** ✅ 已完成(1002;机制核=清单 tier 键+头注 //@ tier+use 点 E3040.tier 档位门[bare 钳 core;缺标注不设门渐进承诺;E5040.tier 值域 fail-closed];首批标注 26 模块 core 5/alloc 16/std 5+三处副本同步;锚 9 断言入 smoke 3p5 段[seed 臂夹具本地 std 七+native 臂真库二];真库 core 五模块 bare 体检自证=core 档音遍性;修复前 bare+use alloc 模块=std 内部 E3040 洪水→use 点一次即止;driver_emit/doc 走 full 包装不接线[emit seam 在飞])`
- :621 总表 T35 行 `待办` → `✅`(同 1002 批次)

- [ ] **Step 3: COVERAGE 工作志 + parity 债登记**

`tests/COVERAGE.md` 服务器/包管理域就近段落追加一行(格式随既有条目):T35 分层 stdlib 机制+首批标注+锚 9 断言;**登记债**:三线 parity(pkg.c 清单 tier 键/sem.c bare 钳口径/main.rs Manifest.tier 字段+R 线检查面)、域包 tier 标注、`--profile=bare` 单文件(use 门需 manifest 或钳门,prof 已入加载链,单文件裸跑仍走体检查面)。

- [ ] **Step 4: Commit**

```bash
git add lib/std/README.md docs/superpowers/plans/2026-09-28-spec-gap-execution-plan.md tests/COVERAGE.md
git commit -m "docs(p8): T35 分层 stdlib 入册——宪章档位轴节(两轴交叉口径)+计划卡/总表翻完成+COVERAGE 工作志;三线 parity(pkg.c/sem.c/main.rs)与域包标注登记债"
```

---

## Self-Review

**1. Spec coverage**(对 T35 卡片三条目标+验收):
- ①std 各模块声明最低档 → Task 2(26 模块头注,分配面+OS 面双轴表)✓
- ②包清单声明最低所需层 → Task 1(`tier` 键+schema regkey)✓
- ③bare 档引用 alloc 层=E3040 族 → Task 1(E3040.tier+bare 钳门)+Task 3/4 锚 ✓
- 验收「bare 包 use std 层模块→E;core 包 use alloc 模块→E」→ tier_neg_std/tier_neg_alloc 双锚 ✓
- 验收「既有 44+ 处消费不红」→ 缺标注不设门+清单缺省 std(双重宽松)+Task 2 Step 3 漂移/stdpkg 门+Task 3 Step 4 全量 smoke ✓
- 坑位「两轴交叉勿重演大迁移」→ 只加头注一行/清单一键,零结构迁移;README 两轴条款成文 ✓
- spec §9「同一 crate 可声明至少需要哪层」→ 清单 tier 键即申报面 ✓

**2. Placeholder scan**: 全计划无 TBD/TODO;所有代码步骤含完整代码与预期输出;真库夹具符号名(djb2/clamp/opt_get_i/lg_enabled/pb_w_varint/words)均经 grep 实证存在。Task 2 标注批次的逐档归属以表给出,执行时对 core 集有 Task 4 音遍性锚兜底(误标必红必降档)。

**3. Type consistency**: `pkg_load_use_t` 第 6 参 `prof: Str` 与 `sem_walk2(file, prof)` 同源同名(check/run 两驱动各传 chk_prof/run_prof);`pkg_load_use_m`(5 参)/`pkg_load_use`(4 参)签名不变,driver_doc.ct:719 与 driver_emit.ct:41 调用面零改动;`pkg_manifest_tier(dir, diags)`/`pkg_mod_tier(src)`/`pkg_tier_rank(t)` 三 fn 签名在 Task 1 定义、后续任务消费一致;E3040.tier 模板 %0 = `segs[2] + "(" + mt + ")"` 与锚 grep "E3040" 匹配面一致;夹具名 tier_neg_alloc/neg_std/neg_val/prof_neg/pos/real_pos/real_neg 全文一致。
