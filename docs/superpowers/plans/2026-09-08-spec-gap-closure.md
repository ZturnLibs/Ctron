# Ctron 规范差距收补（spec-gap-closure）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 按 `docs/spec` v0.5 与 `compiler/` 自举编译器的差距分析，分五阶段收补；本文件详述 Phase 0（诊断与检查面补强），Phase 1–5 给出范围裁决、各自实施前再展开为独立计划。

**Architecture:** 所有改动落在自举编译器的 Ctron 源内（`compiler/src/*.ct`），沿 build.sh 确定性拼接的三产物（cc_run/cc_check/cc_emit）不变；新检查进 sem.ct（check/run 两面共享），JSON 输出进 driver_check.ct（仅 check 面），`ctc.sh` 用既有 sed 锚注入机制传参。验收 = 既有 smoke 18 项 + suite.py 50/50 全绿不回退，新增能力各有 compiler/test/ 下夹具。

**Tech Stack:** Ctron（自举）+ C 宿主 seed（仅首次引导）+ sh + python3（仅测试脚本）。

## Global Constraints

- 新诊断**只允许使用 §10.1 已注册错误码**（E2010/E2020 已注册；新增码须先改 spec 再实现）。
- 任何 sem 新检查不得使 `tests/` 一致性集（50/50）与 smoke 黄金输出回退；新负例夹具放 `compiler/test/`（不进 `tests/`，因 C 参考宿主无同款检查，suite 会对照失败）。
- Ctron 字符串字面量中的 `{` 必须写 `\{`（插值转义）；`;` 与 `::` 禁用；无 `||`。
- smoke.sh 的 decl 锁定数字（当前 cc_run=188）随新增 fn 数量同步更新。
- 提交纪律：每任务一提交；提交信息中文，格式沿仓库惯例 `feat(compiler): ...`。

## 阶段总览（差距 → 阶段映射）

| 阶段 | 收补差距 | 状态 |
|---|---|---|
| **Phase 0（本计划详述）** | JSON 诊断契约 v0（§10.2）、E2020 调用目标解析、E2010 调用 arity（BOOTSTRAP 挂账解除）、for-in List 迭代 | 本文档 Task 1–5 |
| Phase 1 类型检查器 v0 | E2020 全量（变量读）、E2010 基础类型统一（注解/字面量/实参）、解析器 span 标注替换 JSON probe 定位、W8030/W8040、`?` 合法性检查 | **已落**(2026-09-09,含 Phase 1.5 span 标注) |
| Phase 2 comptime CVM | comptime fn 编译期执行、const 编译期求值、E6010 预算、E6030 parametricity、泛型单态化 v0 + 实例预算 | 同上 |
| Phase 3 模块与包 | use 生效、多文件程序、pub(pkg)/字段可见性、E5010 孤儿、E5020 循环依赖、Ctron.toml | **7/7 已落**(2026-09-09:use 合并/可见性 E2020/循环 E5020/pub(pkg)/E5010 孤儿/E4010 caps/E6010 runaway 前置 + FFI extern "c"·c_src 链接;与宿主 pkg oracle 7/7 对齐) |
| Phase 4 真并发运行时 | 发射侧 pthread 真线程/真 Channel/Mutex/Atomic、结构化取消传播（可参考 compiler-rust 的 trans.rs 运行时） | 未启动(解释器顺序化模拟口径不变;2026-09-09 状态核对) |
| Phase 5 档位与 FFI | --profile full/web/bare、extern "c" 解析+发射、#[trusted]、发射器全语言覆盖（struct/闭包/并发/插值）、\u{HEX} 通用解码（需宿主内建 utf8_enc 配套） | 同上 |

---

## Phase 0 全局事实（实施者必读）

- 节点 = `List[Str]`，child[0] 是 tag。关键布局：`Fn/FnC/FnPub = [tag, name, TPs, Ps, ret, body(, attr)]`；`Ps` 子项 = `Receiver`(1槽) 或 `Param`(3槽)，**arity = ps.len - 1**；`Call = ["Call", callee, args]`（实参数 = args.len - 1）；`Ident = ["Ident", name]`；`Member = ["Member", recv, "Nm"/"TIdx", name]`；`Let = ["Let", isv, pattern, ty, expr]`；`For = ["For", pattern, iter, body]`；`Assign = ["Assign", target, op, value]`；`Expr = ["Expr", e]`；块尾值标记 = `["None"]`（len==1）；`Closure = ["Closure", CPs, ret, body]`；`If = ["If", cond, then, else]`；`Match = ["Match", scrut, arms]`，arm = `["Arm", pattern, expr]`。
- bare 内建名集（eval call_id 分发）：`print println assert assert_eq assert_ne panic read_file read_dir byte_at byte_slice Some None Ok Err`；另 bare 合法前奏构造/函数：`fmt char_len Box List String Channel Mutex Atomic Global Task Scope Simd Option Result AnyError`。
- 助手可跨模块直接调用（拼接单文件）：`in_list(ls,nm)`（sem.ct:910）、`seq2/seq/or2/or3`、`dvi`（eval，数值串→I32）、`byte_at/byte_slice/panic`（内建）、I32 `.to_string()`。
- 宿主 seed 解释器运行一切；新增 fn 即改变 cc_run decl 计数 → smoke.sh 锁定值必须更新。

---

### Task 1: eval for-in 支持 List 迭代

**Files:**
- Modify: `compiler/src/eval.ct`（`For` 分支，约 2688 行 `if it[0] != "R"` 之前）
- Test: `compiler/test/fx_forlist.ct`（新建）

**Interfaces:** 消费既有 `run_block/env_add/env_drop/s4/vV2`；产出：`for x in list_expr` 对 `L` 值逐元素绑定迭代（与 `A` 数组分支同构）。

- [ ] **Step 1: 确认 L 值元素布局**（`grep -n '"L"' src/eval.ct`，List push 处元素自 index 1 起、与 A 同构则照搬；若元素带包装则解包）
- [ ] **Step 2: 写失败夹具** `compiler/test/fx_forlist.ct`：

```ctron
// fx_forlist.ct —— for-in 对 List 的迭代(§4.6 可迭代值)
fn main() -> I32 {
    var xs = List[Str]()
    xs.push("a")
    xs.push("b")
    var acc = ""
    for x in xs {
        acc = acc + x
    }
    println(acc)
    return 0
}
```

- [ ] **Step 3: 跑夹具确认失败**：`compiler/ctc.sh compiler/test/fx_forlist.ct` → 预期 `panic: for iter not range`、rc≠0
- [ ] **Step 4: 最小实现**——在 `For` 分支的 `if it[0] == "A" {...}` 之后、`if it[0] != "R"` 之前插入（变量名避让既有 c2/envw3）：

```ctron
        if it[0] == "L" {
            var envl = ir[1]
            var outl = ir[3]
            var ci: I32 = 1
            while ci < it.len {
                var eit = env_add(envl, nm, it[ci])
                var br = run_block(file, eit, outl, st[3], false)
                if br[0] != "k" {
                    return s4(br[0], br[1], br[2], br[3])
                }
                envl = env_drop(br[1], 1)
                outl = br[2]
                ci += 1
            }
            return s4("k", envl, outl, vV2())
        }
```

- [ ] **Step 5: 验证**：`compiler/ctc.sh compiler/test/fx_forlist.ct` → 输出 `ab`、rc=0；`compiler/test/smoke.sh` 快面不回退
- [ ] **Step 6: Commit** `feat(compiler): eval for-in 支持 List 迭代——补 §4.6 可迭代缺口`

### Task 2: sem E2020 调用目标解析 + E2010 调用 arity（类型检查 v0 第一步，解除 BOOTSTRAP arity 挂账）

**Files:**
- Modify: `compiler/src/sem.ct`（文件尾追加新节；`sem_walk2` 尾部接一道 `sem_calls_all(file, diags)`）
- Modify: 编译器自身源中被查出的**潜伏缺参调用点**（BOOTSTRAP.md:134 记载 trans 发射按 0 补齐所镜像的那处）
- Test: `compiler/test/fx_unresolved_neg.ct`、`compiler/test/fx_arity_neg.ct`（新建）

**Interfaces:** 消费 walk 节点布局（见全局事实）；产出诊断：`E2020: 未解析的名称(unresolved):NAME`、`E2010: 调用实参数不匹配(arity):NAME 期望 P 实得 A`。仅检查 **bare 调用**（callee 根为 Ident/TypeArgs）；成员调用（UFCS/方法）、内建/前奏 arity 本任务不查（记入 Phase 1）。

- [ ] **Step 1: 写负例夹具**：

```ctron
// fx_unresolved_neg.ct —— E2020:调用未定义名
fn main() -> I32 {
    prinlt("hi")
    return 0
}
```

```ctron
// fx_arity_neg.ct —— E2010:实参数不匹配
fn add2(a: I32, b: I32) -> I32 {
    return a + b
}
fn main() -> I32 {
    let r = add2(1)
    println(r.to_string())
    return 0
}
```

- [ ] **Step 2: 跑夹具确认现状不报**（两夹具当前均 rc=0 或运行期才炸）
- [ ] **Step 3: 实现**——sem.ct 尾追加（完整代码）：

```ctron
// ---------------- E2020/E2010:调用目标解析与 arity(类型检查 v0) ----------------
fn prelude_ok(nm: Str) -> Bool {
    if or2(seq2(nm, "print"), or2(seq2(nm, "println"), seq2(nm, "panic"))) { return true }
    if or2(seq2(nm, "assert"), or2(seq2(nm, "assert_eq"), seq2(nm, "assert_ne"))) { return true }
    if or2(seq2(nm, "read_file"), or2(seq2(nm, "read_dir"), or2(seq2(nm, "byte_at"), seq2(nm, "byte_slice")))) { return true }
    if or2(seq2(nm, "Some"), or2(seq2(nm, "None"), or2(seq2(nm, "Ok"), seq2(nm, "Err")))) { return true }
    if or2(seq2(nm, "fmt"), or2(seq2(nm, "char_len"), or2(seq2(nm, "Box"), seq2(nm, "List")))) { return true }
    if or2(seq2(nm, "String"), or2(seq2(nm, "Channel"), or2(seq2(nm, "Mutex"), seq2(nm, "Atomic")))) { return true }
    if or2(seq2(nm, "Global"), or2(seq2(nm, "Task"), or2(seq2(nm, "Scope"), seq2(nm, "Simd")))) { return true }
    return or2(seq2(nm, "Option"), or2(seq2(nm, "Result"), seq2(nm, "AnyError")))
}

fn root_name(e: List[Str]) -> Str {
    var t = e[0]
    if t == "Ident" { return e[1] }
    if t == "TypeArgs" { return root_name(e[1]) }
    return ""
}

fn pat_name_push(pp: List[Str], out: List[Str]) {
    var t = pp[0]
    if t == "PatId" {
        out.push(pp[1])
    } else if t == "PatTup" {
        var i: I32 = 1
        while i < pp.len {
            pat_name_push(pp[i], out)
            i += 1
        }
    }
}

fn uce(file: List[Str], e: List[Str], fns: List[Str], ars: List[Str], loc: List[Str], diags: List[Str]) {
    var t = e[0]
    if t == "Call" {
        var cal = e[1]
        var rt = root_name(cal)
        var ag = e[2]
        if rt.len > 0 && !in_list(loc, rt) && !in_list(diags, "E2020: 未解析的名称(unresolved):" + rt) {
            if !in_list(fns, rt) && !prelude_ok(rt) {
                diags.push("E2020: 未解析的名称(unresolved):" + rt)
            } else {
                var g: I32 = 0
                while g < fns.len {
                    if seq2(fns[g], rt) && ars[g].len > 0 {
                        var got = ag.len - 1
                        if got != dvi(ars[g]) {
                            diags.push("E2010: 调用实参数不匹配(arity):" + rt + " 期望 " + ars[g] + " 实得 " + got.to_string())
                        }
                    }
                    g += 1
                }
            }
        }
        uce(file, cal, fns, ars, loc, diags)
        var i: I32 = 1
        while i < ag.len {
            uce(file, ag[i], fns, ars, loc, diags)
            i += 1
        }
        return
    }
    if t == "Member" { uce(file, e[1], fns, ars, loc, diags); return }
    if t == "Binary" {
        uce(file, e[2], fns, ars, loc, diags)
        uce(file, e[3], fns, ars, loc, diags)
        return
    }
    if t == "Unary" { uce(file, e[2], fns, ars, loc, diags); return }
    if t == "Range" {
        uce(file, e[2], fns, ars, loc, diags)
        uce(file, e[3], fns, ars, loc, diags)
        return
    }
    if t == "Index" {
        uce(file, e[1], fns, ars, loc, diags)
        uce(file, e[2], fns, ars, loc, diags)
        return
    }
    if t == "TypeArgs" { uce(file, e[1], fns, ars, loc, diags); return }
    if t == "Try" { uce(file, e[1], fns, ars, loc, diags); return }
    if or2(t == "TupleE", t == "ArrLit") {
        var i2: I32 = 1
        while i2 < e.len {
            uce(file, e[i2], fns, ars, loc, diags)
            i2 += 1
        }
        return
    }
    if t == "BlockExpr" { ucb(file, e[1], fns, ars, loc, diags); return }
    if t == "If" {
        uce(file, e[1], fns, ars, loc, diags)
        ucb(file, e[2], fns, ars, loc, diags)
        uce(file, e[3], fns, ars, loc, diags)
        return
    }
    if t == "Match" {
        uce(file, e[1], fns, ars, loc, diags)
        var arms = e[2]
        var i3: I32 = 1
        while i3 < arms.len {
            var a = arms[i3]
            pat_name_push(a[1], loc)
            uce(file, a[2], fns, ars, loc, diags)
            i3 += 1
        }
        return
    }
    if t == "Closure" {
        var cp = e[1]
        var i4: I32 = 1
        while i4 < cp.len {
            loc.push(cp[i4][1])
            i4 += 1
        }
        uce(file, e[3], fns, ars, loc, diags)
        return
    }
    if or2(t == "Own", t == "Scope") { ucb(file, e[2], fns, ars, loc, diags); return }
    if t == "Interp" {
        var i5: I32 = 1
        while i5 < e.len {
            var pt = e[i5]
            var j: I32 = 1
            while j < pt.len {
                if pt[j].len > 1 && !seq2(pt[j][0], "Text") {
                    uce(file, pt[j], fns, ars, loc, diags)
                }
                j += 1
            }
            i5 += 1
        }
        return
    }
    if t == "StructLit" {
        var i6: I32 = 1
        while i6 < e.len {
            var c = e[i6]
            if seq2(c[0], "LField") && c.len >= 3 {
                uce(file, c[2], fns, ars, loc, diags)
            }
            i6 += 1
        }
        return
    }
}

fn ucb(file: List[Str], b: List[Str], fns: List[Str], ars: List[Str], loc: List[Str], diags: List[Str]) {
    var i: I32 = 1
    while i < b.len {
        var s = b[i]
        var t = s[0]
        if t == "Let" {
            pat_name_push(s[2], loc)
            uce(file, s[4], fns, ars, loc, diags)
        } else if t == "While" {
            uce(file, s[1], fns, ars, loc, diags)
            ucb(file, s[2], fns, ars, loc, diags)
        } else if t == "For" {
            pat_name_push(s[1], loc)
            uce(file, s[2], fns, ars, loc, diags)
            ucb(file, s[3], fns, ars, loc, diags)
        } else if t == "Return" {
            if s[1].len > 1 { uce(file, s[1], fns, ars, loc, diags) }
        } else if t == "Assign" {
            uce(file, s[1], fns, ars, loc, diags)
            uce(file, s[3], fns, ars, loc, diags)
        } else if t == "Expr" {
            uce(file, s[1], fns, ars, loc, diags)
        } else if or2(seq2(t, "None"), t == "Text") {
            // 块尾标记/文本:跳过
        } else {
            uce(file, s, fns, ars, loc, diags)
        }
        i += 1
    }
}

fn sem_calls_all(file: List[Str], diags: List[Str]) {
    var fns = List[Str]()
    var ars = List[Str]()
    var i: I32 = 1
    while i < file.len {
        var d = file[i]
        var t = d[0]
        if or2(t == "Fn", or2(t == "FnC", or2(t == "FnPub", t == "Method"))) {
            fns.push(d[1])
            ars.push((d[3].len - 1).to_string())
        }
        i += 1
    }
    i = 1
    while i < file.len {
        var d = file[i]
        var t = d[0]
        if or2(t == "Fn", or2(t == "FnC", t == "FnPub")) {
            ucb(file, d[5], fns, ars, List[Str](), diags)
        } else if t == "Method" {
            ucb(file, d[5], fns, ars, List[Str](), diags)
        } else if t == "Test" {
            ucb(file, d[2], fns, ars, List[Str](), diags)
        }
        i += 1
    }
}
```

在 `sem_walk2` 的 diags 汇总循环之前接入一行：`sem_calls_all(file, diags)`。
（注：`loc` 全程只增不清——绑定集过近似，宁可漏报不误报；`ars`/`fns` 下标平行；`Method` 参与解析集但 UFCS 成员调用不查 arity。）

- [ ] **Step 4: 验证夹具**：两负例 rc=1 且分别含 E2020/E2010
- [ ] **Step 5: 自检排查潜伏缺参点**：`compiler/ctc.sh check compiler/build/cc_run.ct` → 若报 E2010，按诊断定位编译器源中缺参调用，补全实参（该点即 BOOTSTRAP.md:134 挂账所镜像处）；重复至自检绿
- [ ] **Step 6: 回归**：`compiler/test/smoke.sh` 快面全绿（黄金逐字不变）；`compiler/test/test/suite.py`（或仓库约定路径）50/50
- [ ] **Step 7: Commit** `feat(compiler): sem 新增 E2020 调用目标解析与 E2010 调用 arity 检查——类型检查 v0 第一步`

### Task 3: driver_check JSON 诊断输出（§10.2 v0）+ ctc.sh --format=json

**Files:**
- Modify: `compiler/src/driver_check.ct`（重写）
- Modify: `compiler/ctc.sh`（check 分支）
- Test: `compiler/test/fx_json_neg.ct`（新建）、`compiler/test/smoke.sh` 追加 1 项

**Interfaces:** 产出冻结 schema：`{"diagnostics":[{"code","severity","message","file","span":{line_start,col_start,line_end,col_end},"notes":[],"fixes":[]}]}`。span 为 **v0 近似**：以消息尾段名在源码中定位首含行（列恒 1），Phase 1 解析器 span 标注落地后替换。模式开关：源内常量锚 `"ANCHORFMT"`，ctc.sh sed 换 `"1"/"0"`；非 sed 路径（原生二进制）恒文本模式。

- [ ] **Step 1: 写夹具** `compiler/test/fx_json_neg.ct`：

```ctron
// fx_json_neg.ct —— JSON 诊断契约夹具:struct 含类引用字段 → W8010
class Engine {
    let rpm: I32
}
struct Car {
    let eng: Engine
}
fn main() -> I32 {
    return 0
}
```

- [ ] **Step 2: 重写 driver_check.ct**（完整代码；注意 Ctron 串内 `{` 必须 `\{`）：

```ctron
// =====================================================================
// driver_check.ct —— 编译器入口(检查模式)
// parse → 单文件语义检查即止;诊断逐条 "CODE: msg" 打印并止(rc=1),
// 干净打印 "check OK decls=N" 并返回 rc=0。
// --format=json(§10.2):经构建锚 ANCHORFMT 注入,输出冻结 schema 诊断 JSON。
// span 为 v0 近似:以消息尾段名定位首含行(列恒 1);Phase 1 解析器 span 落地后替换。
// 输入锚:read_file("../selfhosted/input_cc.ct"),构建/运行脚本换靶。
// =====================================================================
fn json_escape(s: Str) -> Str {
    var out = ""
    var j: I32 = 0
    var n = s.len
    while j < n {
        var c = byte_at(s, j)
        if c == 34 {
            out = out + "\\\""
        } else if c == 92 {
            out = out + "\\\\"
        } else if c == 10 {
            out = out + "\\n"
        } else if c == 13 {
            out = out + "\\r"
        } else if c == 9 {
            out = out + "\\t"
        } else {
            out = out + byte_slice(s, j, j + 1)
        }
        j += 1
    }
    return out
}

fn has_sub(hay: Str, nd: Str) -> Bool {
    var nn = nd.len
    if nn == 0 { return true }
    var hn = hay.len
    var i: I32 = 0
    while i + nn <= hn {
        var k: I32 = 0
        var ok = true
        while k < nn {
            if byte_at(hay, i + k) != byte_at(nd, k) { ok = false }
            k += 1
        }
        if ok { return true }
        i += 1
    }
    return false
}

fn probe_line(src: Str, msg: Str) -> I32 {
    // 取 msg 最后一段(": " 之后)作探针;找不到再退整消息,仍找不到 → 1
    var probe = msg
    var j: I32 = 0
    var last: I32 = -1
    while j + 1 < msg.len {
        if byte_at(msg, j) == 58 && byte_at(msg, j + 1) == 32 { last = j }
        j += 1
    }
    if last >= 0 { probe = byte_slice(msg, last + 2, msg.len) }
    if probe.len == 0 { return 1 }
    var line: I32 = 1
    var start: I32 = 0
    var i2: I32 = 0
    var n2 = src.len
    while i2 <= n2 {
        if i2 == n2 || byte_at(src, i2) == 10 {
            if has_sub(byte_slice(src, start, i2), probe) { return line }
            line += 1
            start = i2 + 1
        }
        i2 += 1
    }
    return 1
}

fn json_diag(path: Str, src: Str, ln: Str, first: Bool) {
    var sp: I32 = 0
    while sp < ln.len && byte_at(ln, sp) != 32 { sp += 1 }
    var code = byte_slice(ln, 0, sp)
    var msg = byte_slice(ln, sp + 1, ln.len)
    var sev = "error"
    if byte_at(code, 0) == 87 { sev = "warning" }
    var at: I32 = 1
    if sp > 0 { at = probe_line(src, msg) }
    if !first { print(",") }
    print("\{\"code\":\"" + json_escape(code) + "\",\"severity\":\"" + sev + "\",\"message\":\"" + json_escape(msg) + "\",\"file\":\"" + json_escape(path) + "\",\"span\":\{\"line_start\":" + at.to_string() + ",\"col_start\":1,\"line_end\":" + at.to_string() + ",\"col_end\":1\},\"notes\":[],\"fixes\":[]\}")
}

fn main() -> I32 {
    let path = "../selfhosted/input_cc.ct"
    let src = read_file(path)
    match src {
        Some(s) => {
            var toks = List[Str]()
            scan(s, toks)
            var cur = Atomic[I32](0)
            var file = p_file(toks, cur)
            var sems = sem_walk2(file)
            if seq2("ANCHORFMT", "1") {
                print("\{\"diagnostics\":[")
                if !seq2(sems, "") {
                    var i: I32 = 0
                    var first = true
                    while i < sems.len {
                        var e: I32 = i
                        while e < sems.len && byte_at(sems, e) != 10 { e += 1 }
                        if e > i {
                            json_diag(path, s, byte_slice(sems, i, e), first)
                            first = false
                        }
                        i = e + 1
                    }
                    print("]")
                    return 1
                }
                print("]")
                return 0
            }
            if !seq2(sems, "") {
                print(sems)
                return 1
            }
            println("check OK decls=" + (file.len - 1).to_string())
            return 0
        }
        None => {
            print("read-failed")
            return 1
        }
    }
}
```

- [ ] **Step 3: ctc.sh check 分支加 --format=json**：

```sh
    check)
        FMT=0
        for a in "$@"; do
            case $a in --format=json) FMT=1 ;; esac
        done
        "$DIR/build.sh" >/dev/null
        TMP=$(mktemp /tmp/ctron_cc.XXXXXX)
        sed -e "s|\.\./selfhosted/input_cc\.ct|$IN|" -e "s|ANCHORFMT|$FMT|" "$DIR/build/cc_check.ct" > "$TMP"
        "$HOST" run "$TMP"
        rc=$?
        ;;
```

- [ ] **Step 4: 验证**：`compiler/ctc.sh check compiler/test/fx_json_neg.ct --format=json` → rc=1，输出为合法 JSON 且含 `"code":"W8010"`、`"severity":"warning"`；`python3 -c "import json,sys; json.load(sys.stdin)"` 通过；干净文件 → `{"diagnostics":[]}` rc=0；默认无 flag → 文本面原样
- [ ] **Step 5: smoke.sh 追加**（第 2 节后）：

```sh
echo "== 2b) JSON 诊断契约(§10.2 v0) =="
"$COMP/ctc.sh" check "$COMP/test/fx_json_neg.ct" --format=json > "$T/js.out" 2>&1
jrc=$?
if [ $jrc -eq 1 ] && grep -q '"code":"W8010"' "$T/js.out" \
   && python3 -c "import json,sys; json.load(sys.stdin)" < "$T/js.out" 2>/dev/null; then
    ok "JSON 诊断输出合法且含 W8010"
else
    bad "JSON 诊断面异常(rc=$jrc): $(cat "$T/js.out")"
fi
```

- [ ] **Step 6: Commit** `feat(compiler): check 面 JSON 诊断契约 v0(§10.2)——code/severity/message/file/span 近似定位`

### Task 4: smoke decl 锁定更新 + 文档同步

**Files:**
- Modify: `compiler/test/smoke.sh`（`decls=188` → 实测新值）
- Modify: `compiler/README.md`（用法表 + 边界节：JSON 面/E2020/E2010/for-in List 已落；arity 挂账解除）
- Modify: `compiler/BOOTSTRAP.md`（不变量表:删除"arity 检查挂账"条目,改为"E2010 arity 已落(bare 调用面)"）

- [ ] **Step 1:** `compiler/ctc.sh check compiler/build/cc_run.ct` 取实际 decls 值,更新 smoke.sh 锁定
- [ ] **Step 2:** 文档三处按上述口径更新（如实记录:JSON span 为 v0 近似、E2020/E2010 仅 bare 调用面、成员调用与内建 arity 留 Phase 1）
- [ ] **Step 3: Commit** `docs(compiler): smoke decl 锁定同步 + README/BOOTSTRAP 挂账口径更新`

### Task 5: 全量验证

- [ ] **Step 1:** `make -C compiler-c`（确保宿主最新）
- [ ] **Step 2:** `compiler/test/smoke.sh --full` → 18+1 项全绿（含自发射收官与自举固定点）
- [ ] **Step 3:** `compiler/test/test/suite.py`（路径以仓库为准，见 smoke.sh/README）→ 50/50
- [ ] **Step 4:** 新夹具逐个过一遍（fx_forlist 正例 rc=0、两 neg 例 rc=1、JSON 面）
- [ ] **Step 5: Commit**（如有零星修复）`fix(compiler): Phase 0 验收修复`

## Self-Review 结论

- 覆盖检查：Phase 0 四项差距（JSON 契约/E2020/E2010/for-in List）均有任务与验收；\u{HEX} 通用解码**明确移入 Phase 5**（需宿主配套内建 utf8_enc，单靠 compiler/ 侧无任意字节构造手段）——已在阶段表中显式记录，无静默遗漏。
- 占位符扫描：Task 1 Step 1 与 Task 2 Step 5 是"先探明再动手"的验证步（探明 L 布局 / 定位潜伏缺参点），其实现代码均已给出，非占位。
- 类型一致性：`uce/ucb/sem_calls_all` 签名在各调用点一致；`fns/ars` 平行表在 prescan 与 arity 消费两处同构；JSON 面只依赖 `sem_walk2` 的既有 `CODE: msg` 行格式，未改 sem 输出契约（文本面逐字兼容）。

## 执行方式

自治模式下采用 **Inline Execution**（executing-plans 精神）：本会话内逐任务实施、每任务一提交、每任务跑对应验证。Phase 1–5 启动时各按本模板展开独立计划。
