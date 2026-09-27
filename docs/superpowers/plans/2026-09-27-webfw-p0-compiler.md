# web 框架 P0:编译销账 + 双探针 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 销掉 web 框架的四件 member-emit 编译前置(P0-1..4),并用双探针判定账本条件项 L4(Bytes)/L5(字符串缓冲)生死——web 域包 v1 的硬前置。

**Architecture:** 修正本落双处镜像:`compiler/src/trans_expr.ct`(Ctron 自举正本,经 `build.sh` 拼入 cc_emit)与 `compiler-c/src/trans_expr.c`(C 种子)。全部四件同属 trans 的成员访问/索引发射族:`EX_MEMBER` 的 `T_LIST`/T_FN 分支缺口与 `EX_INDEX` 的 List[struct] 臂。验证三口径:seed 解释(`ctronc run`)、seed 发射(`ctronc build` + 落地执行)、自举语义(`compiler/ctc.sh check`)。探针先行:每任务先固化**失败探针**(即最终套件测试),修后转正。

**Tech Stack:** Ctron(测试/正本)、C(种子发射器 `compiler-c/src/trans_expr.c`)、gcc(发射落地)、sh(run.sh/ctc.sh)。

**Spec:** `docs/superpowers/specs/2026-09-27-web-framework-design.md` §9(账本)、§14-1(L5 探针)、§7-8(二进制探针)、§10(门禁)。

## Global Constraints

- **双编译器同步**:每处发射修复必须同时落 `compiler/src/trans_expr.ct` 与 `compiler-c/src/trans_expr.c`(镜像同构;`build.sh` 的 trans 段拼接序不动)。
- **验证双臂勿只跑一边**:interp(`ctronc run X.ct`)+ emit(`ctronc build X.ct && ./a.out`);emit 挂时先 `./ctc.sh check` 拿语义真错(在册坑:cc/emit 管线不对称)。
- **验证用 seed**:`compiler-c/build/ctronc`(本会话已实弹验证);改种子后 `make -C compiler-c` 重建。
- **Ctron 源码纪律**:无位运算算符(& 是借用/| 是闭包定界/^ E1001),算术等价替代;字符串裸 `{` 须写 `\{`;禁 `;` 结尾;`or` 两侧全括号。
- **提交纪律**:pathspec 限定(并行泳道在飞);每任务一提交,全绿才落库。
- **worktree**:本计划动 compiler 共享文件,执行前按 `superpowers:using-git-worktrees` 建隔离 worktree,软链 `compiler-c/build` 与 `compiler/bin` 构建产物,基线红账先记(ci.sh 已有红项清单,见 tests/COVERAGE.md)。
- **探针即测试**:所有探针 `.ct` 文件放 `tests/` 正式套件位(命名跟随 `tests/03g_fn_types.ct` 族),修完即套件常驻,不留一次性脚本。

---

### Task 0: 探针基建 + L4/L5 生死判定

**Files:**
- Create: `tests/03i_string_builder_probe.ct`
- Create: `tests/03m_binary_nul_probe.ct`
- Modify: `docs/c-rust-divergences.md`(账本 L4/L5 判定回写)
- Modify: `docs/superpowers/specs/2026-09-27-web-framework-design.md` §9(条件项翻转)

**Interfaces:**
- Consumes: seed `compiler-c/build/ctronc`(run/build 双臂)
- Produces: L4/L5 生死结论写入 spec §9 与 divergences(后续 Task 1-5 与 web v1 计划的输入;**不修语言项本身**)

- [ ] **Step 1: 固化 L5 探针(StringBuilder 成色)**

写入 `tests/03i_string_builder_probe.ct`:

```ctron
// 03i_string_builder_probe.ct —— L5 生死探针:预演算挂名的 StringBuilder 有无真身
// 预期:编译通过且输出 "hello world" → L5 销;E 级拒绝(未知类型)→ L5 立项
fn main() -> I32 {
    let sb = StringBuilder()
    println("probe: 类型存在")
    return 0
}
```

- [ ] **Step 2: 跑 L5 探针,记录判定**

Run: `compiler-c/build/ctronc run tests/03i_string_builder_probe.ct`
Expected(二选一,如实记录):①报 E(未知类型 StringBuilder 之类)→ **L5 立**;②能过 → 继续补 push/done 调用逐方法探,全绿才销。
无论哪种,把结论一行写入 `docs/c-rust-divergences.md`(节:web 框架 P0 探针,2026-09-27):`L5 StringBuilder: <立/销+证据>`。

- [ ] **Step 3: 固化二进制 NUL 探针(L4 生死)**

写入 `tests/03m_binary_nul_probe.ct`:

```ctron
// 03m_binary_nul_probe.ct —— L4 生死探针:含 NUL 字节过 Str 全链(读/byte_at/中段切片重组/写回)保真?
// 判定:nuls>0 且 mid_ok=1 且 round_len 相等 → L4 销(§7-8 门绿);
//       任一不满足 → L4 立项(Upload.data: Bytes 等 §13-③ 面待其)
use std.fs.{ read_or }

fn main() -> I32 {
    let bin = read_or("/bin/echo", "")
    println("len:" + bin.len.to_string())
    var nuls: I32 = 0
    var i: I32 = 0
    while i < bin.len {
        if byte_at(bin, i) == 0 {
            nuls += 1
        }
        i += 1
    }
    println("nuls:" + nuls.to_string())
    var mid = ""
    var j: I32 = bin.len / 2
    while j < bin.len / 2 + 16 {
        mid = mid + byte_slice(bin, j, j + 1)
        j += 1
    }
    var ok = true
    var k: I32 = 0
    while k < 16 {
        if byte_at(mid, k) != byte_at(bin, bin.len / 2 + k) {
            ok = false
        }
        k += 1
    }
    println("mid_ok:" + (if ok { "1" } else { "0" }))
    return 0
}
```

- [ ] **Step 4: 跑二进制探针(interp+emit 双臂),记录判定**

Run: `compiler-c/build/ctronc run tests/03m_binary_nul_probe.ct && compiler-c/build/ctronc build tests/03m_binary_nul_probe.ct && ./a.out`
Expected: 两臂均输出 `nuls>0` 且 `mid_ok:1` → **L4 销**;否则 L4 立。
结论回写 divergences + spec §9(同 Step 2 格式)。

- [ ] **Step 5: 提交探针与账本回写**

```bash
git add tests/03i_string_builder_probe.ct tests/03m_binary_nul_probe.ct docs/c-rust-divergences.md docs/superpowers/specs/2026-09-27-web-framework-design.md
git commit -m "test(compiler): L4/L5 生死双探针固化+判定回写——web P0 探针基建" -- tests/03i_string_builder_probe.ct tests/03m_binary_nul_probe.ct docs/c-rust-divergences.md docs/superpowers/specs/2026-09-27-web-framework-design.md
```

---

### Task 1: P0-3 fn 值调用链成员读(`f(x).body`)

最小件先行(发射器 EX_MEMBER 对"经 fn 值调用的返回值"取成员的分支缺口;普通函数调用链 `mk().body` 已绿,差异只在被调目标是 fn 值)。

**Files:**
- Create: `tests/03k_fn_call_chain_member.ct`
- Modify: `compiler-c/src/trans_expr.c`(EX_MEMBER 案例,约 L240-312:成员发射的基表达式类型解析)
- Modify: `compiler/src/trans_expr.ct`(镜像同位)

**Interfaces:**
- Produces: 发射臂支持 `f(x).field`(f 为 fn 值局部/参数);后续 web 的 `req.param_i64("id").or(0)` 族直接受益

- [ ] **Step 1: 固化失败测试**

写入 `tests/03k_fn_call_chain_member.ct`(命名函数经 fn 类型形参传递;闭包字面量留给 Task 2):

```ctron
// 03k_fn_call_chain_member.ct —— P0-3:fn 值调用链成员读(发射臂)
struct Resp { body: Str }

fn mk() -> Resp {
    return Resp { body: "hi" }
}

fn apply(f: fn() -> Resp) -> Resp {
    return f()
}

fn main() -> I32 {
    let v = apply(mk)
    println(v.body)          // 经 fn 值调用结果的成员读:普通调用 mk().body 已绿,此处曾红
    println(apply(mk).body)  // 直接链式形态:同族缺口
    return 0
}
```

- [ ] **Step 2: 复现失败**

Run: `compiler-c/build/ctronc run tests/03k_fn_call_chain_member.ct`(Expected: 通过——interp 臂已绿,作对照)
Run: `compiler-c/build/ctronc build tests/03k_fn_call_chain_member.ct`(Expected: FAIL `trans: v1:成员 .body 不支持(目标类型 1)`)

- [ ] **Step 3: 修种子 `compiler-c/src/trans_expr.c`**

定位:`EX_MEMBER` 案例(trans_expr.c,成员发射 fallthrough `terr(c, "v1:成员 .%s 不支持(目标类型 %d)"...)` 约在 L311;基表达式类型解析在 case 开头)。先读该 case 全体,理解 `ob/ov`(基表达式缓冲/值文本)如何产生。缺口:当基是**经 fn 类型形参的调用**(EX_CALL,被调符号类型为 T_FN)时,调用返回类型的解析落到 `ty_unk()`,导致 struct 分支不命中。修法:在调用返回类型解析处,对被调者的 `fn(T)->R` 取 `R`(与普通命名函数同路——普通调用已正确,复用其返回类型求取函数);使 `f(r1)` 的静态类型 = `Resp`,后续既有的 struct 成员分支(约 L270 起 `( ov)->%s` 形态)自然命中。**先读后改,镜像既有 T_ARR/box_elem struct 分支的写法,勿新造发射形态。**

- [ ] **Step 4: 镜像修正本 `compiler/src/trans_expr.ct`**

同位同因:Ctron 侧 trans_expr.ct 的成员发射函数(文件头注即接口;搜 `成员` 诊断串定位)。修法同 Step 3,产出 C 的拼接逻辑对应 .ct 的串拼接逻辑。

- [ ] **Step 5: 三口径验证**

Run: `make -C compiler-c && compiler-c/build/ctronc run tests/03k_fn_call_chain_member.ct && compiler-c/build/ctronc build tests/03k_fn_call_chain_member.ct && ./a.out`
Expected: 两臂均输出 `hi`/`hi`。
Run: `sh compiler/ctc.sh check tests/03k_fn_call_chain_member.ct`(Expected: `check OK`)
Run: `sh ci.sh` 或按 COVERAGE 红账口径跑全套件(Expected: 无新增红,基线红账不变)

- [ ] **Step 6: 提交**

```bash
git add tests/03k_fn_call_chain_member.ct compiler-c/src/trans_expr.c compiler/src/trans_expr.ct
git commit -m "fix(compiler): P0-3 fn 值调用链成员读——EX_MEMBER 基为 T_FN 调用时返回类型解析取 R,struct 分支自然命中;双编译器镜像;探针转正 03k" -- tests/03k_fn_call_chain_member.ct compiler-c/src/trans_expr.c compiler/src/trans_expr.ct
```

---

### Task 2: P0-2 闭包形参成员读(闭包体内 `r.path`)

**Files:**
- Create: `tests/03j_closure_param_member.ct`
- Modify: `compiler-c/src/trans_expr.c`(EX_MEMBER/闭包发射族:闭包参数的类型环境)
- Modify: `compiler/src/trans_expr.ct`(镜像)

**Interfaces:**
- Produces: 闭包体内可对闭包形参取成员/调方法——web 中间件形态 `|req, next| { req.session(...) }` 的硬前置

- [ ] **Step 1: 固化失败测试**

写入 `tests/03j_closure_param_member.ct`:

```ctron
// 03j_closure_param_member.ct —— P0-2:闭包形参成员读(发射臂)
// interp 已绿;emit 曾报 "trans: v1:成员 .path 不支持(目标类型 1)"(2026-09-27 探针 p2e)
struct Req { path: Str }
struct Resp { body: Str }

fn wrap(tag: Str) -> fn(Req) -> Resp {
    return |r| {
        return Resp { body: tag + ":" + r.path }   // 闭包形参成员读:曾红点
    }
}

fn main() -> I32 {
    let mid = wrap("mw")
    let r = Req { path: "/x" }
    let v = mid(r)
    println(v.body)
    return 0
}
```

- [ ] **Step 2: 复现失败**

Run: `compiler-c/build/ctronc build tests/03j_closure_param_member.ct`
Expected: FAIL `trans: v1:成员 .path 不支持(目标类型 1)`

- [ ] **Step 3: 修种子(定位→镜像既有分支)**

定位:闭包字面量的发射(trans_expr.c 的 EX_CLOSURE/lambda 族)为闭包形参建的局部环境里,形参类型未随 fn 签名带入(成员发射解析 `r` 得 `ty_unk` → fallthrough 报错)。修法:闭包形参声明发射时,从闭包被赋予/返回的**期望函数类型**(上下文已有,§4.7 自动适配)逐参取类型注入环境;成员发射即可命中既有 struct 分支。先读 EX_CLOSURE 与形参环境构建,后改;勿动闭包调用约定本身。

- [ ] **Step 4: 镜像修 `compiler/src/trans_expr.ct`**

同位同因(搜闭包发射函数 + `成员` 诊断串)。

- [ ] **Step 5: 三口径验证 + 全套件**

同 Task 1 Step 5(命令把文件名换成 03j)。Expected: 两臂输出 `mw:/x`;check OK;无新增红。

- [ ] **Step 6: 提交**

```bash
git add tests/03j_closure_param_member.ct compiler-c/src/trans_expr.c compiler/src/trans_expr.ct
git commit -m "fix(compiler): P0-2 闭包形参成员读——闭包形参从期望 fn 签名注入类型环境;双编译器镜像;探针转正 03j" -- tests/03j_closure_param_member.ct compiler-c/src/trans_expr.c compiler/src/trans_expr.ct
```

---

### Task 3: P0-1 `List[struct]` 元素成员读(在册债销账)

在册最重件(todo_app 数据层降级根因)。已知:interp 全绿;emit 对 `tab[i]`(List,元素 struct)的索引读静默截断/成员读报错;**定长数组 `Route[8]` 含 fn 字段另有缺口(Task 4),本任务只治 List**。

**Files:**
- Create: `tests/03l_list_struct_member.ct`
- Modify: `compiler-c/src/trans_expr.c`(EX_INDEX 的 List 臂 + EX_MEMBER 的 List 元素臂,约 L265-285 现有 box_elem struct 分支旁)
- Modify: `compiler/src/trans_expr.ct`(镜像)

**Interfaces:**
- Produces: `let e = list[i]` 后 `e.field`/`e.method()` 可用;web Router 的 `List[Route]` 与应用 `List[Todo]` 的地基

- [ ] **Step 1: 固化失败测试**

写入 `tests/03l_list_struct_member.ct`:

```ctron
// 03l_list_struct_member.ct —— P0-1:List[struct] 元素读+成员读(发射臂)
// interp 已绿;emit 曾报 "trans: v1:成员 .p 不支持(目标类型 1)"(2026-09-27 探针 final.ct;
// 在册债:c-rust-divergences「List[struct] 索引读静默截断」)
struct Req { path: Str, uid: Str }
struct Resp { body: Str }
struct Route { m: Str, p: Str, h: fn(Req) -> Resp }

fn h_root(r: Req) -> Resp {
    return Resp { body: "root" }
}
fn h_app(r: Req) -> Resp {
    return Resp { body: "app:" + r.uid }
}

fn main() -> I32 {
    var tab: List[Route] = List[Route]()
    tab.push(Route { m: "GET", p: "/", h: h_root })
    tab.push(Route { m: "GET", p: "/app", h: h_app })
    let r = Req { path: "/app", uid: "alice" }
    var i: I32 = 0
    while i < tab.len {
        let e = tab[i]              // List[struct] 索引读(曾:静默截断)
        if e.p == r.path {          // 元素成员读(曾:trans 报错)
            let h = e.h             // 元素 fn 字段提取(Task 1/2 修复后可用)
            let resp = h(r)
            println("HIT " + e.p + " -> " + resp.body)
        }
        i += 1
    }
    return 0
}
```

- [ ] **Step 2: 复现失败**

Run: `compiler-c/build/ctronc build tests/03l_list_struct_member.ct`
Expected: FAIL(trans 成员错或 C 编译错/静默错输出)——如实记录初始形态。

- [ ] **Step 3: 修种子**

定位:`EX_INDEX` 的 List 臂(元素读的 C 形态;List 是 `{data, n, cap}` 结构,标量元素已走 `ctron_list_get` 族)与 `EX_MEMBER` 的 T_LIST 元素分支(trans_expr.c 约 L270 的 `box_elem(ot)` struct 分支只覆盖 T_ARR/Box,T_LIST 未达)。修法:①索引读:List 元素为 T_STRUCT 时产出与 T_ARR 同构的元素地址表达式(struct 指针);②成员读:`box_elem` 的 struct 分支放行 T_LIST(元素类型经 List 的类型实参取)。**镜像 T_ARR 既有正确形态,先读 `box_elem` 与 EX_INDEX 全 case 再改。**

- [ ] **Step 4: 镜像修 `compiler/src/trans_expr.ct`**

同位同因(搜 `box_elem` 对应 Ctron 函数与索引/成员发射)。

- [ ] **Step 5: 三口径验证 + 全套件**

同前(文件名 03l)。Expected: 两臂输出 `HIT /app -> app:alice`;check OK;无新增红。**特别注意**:在册债原文含"List[I64] 索引读静默截断"——补跑一个 List[I64] 元素读的小断言(可并入本测试文件尾部)确认同批修复。

- [ ] **Step 6: 提交**

```bash
git add tests/03l_list_struct_member.ct compiler-c/src/trans_expr.c compiler/src/trans_expr.ct
git commit -m "fix(compiler): P0-1 List[struct] 元素读+成员读——EX_INDEX/EX_MEMBER 的 T_LIST 臂镜像 T_ARR struct 形态,在册债销账;List[I64] 同批验证;双编译器镜像;探针转正 03h2" -- tests/03l_list_struct_member.ct compiler-c/src/trans_expr.c compiler/src/trans_expr.ct
```

---

### Task 4: P0-4 含 fn 字段 struct 入定长数组

**Files:**
- Create: `tests/03n_struct_fn_field_array.ct`
- Modify: `compiler-c/src/trans_stmt.c`(定长数组字面量/赋值的元素类型门,报"数组元素类型不支持"处)
- Modify: `compiler/src/trans_stmt.ct`(镜像;注意并行泳道在该文件有在飞改动——rebase 对齐后再动)

**Interfaces:**
- Produces: `var tab: Route[8] = [...]; tab[0] = Route{...}; tab[i].h` 全链可用(路由表降级备援形态与用户态查表)

- [ ] **Step 1: 固化失败测试**

写入 `tests/03n_struct_fn_field_array.ct`:

```ctron
// 03n_struct_fn_field_array.ct —— P0-4:含 fn 字段 struct 入定长数组(发射臂)
// interp 已绿;emit 曾报 "trans: v1:数组元素类型不支持"(2026-09-27 探针 fixed.ct)
struct Req { path: Str, uid: Str }
struct Resp { body: Str }
struct Route { m: Str, p: Str, h: fn(Req) -> Resp }

fn h_root(r: Req) -> Resp {
    return Resp { body: "root" }
}
fn h_app(r: Req) -> Resp {
    return Resp { body: "app:" + r.uid }
}

fn main() -> I32 {
    var tab: Route[8] = [Route { m: "", p: "", h: h_root }, Route { m: "", p: "", h: h_root }, Route { m: "", p: "", h: h_root }, Route { m: "", p: "", h: h_root }, Route { m: "", p: "", h: h_root }, Route { m: "", p: "", h: h_root }, Route { m: "", p: "", h: h_root }, Route { m: "", p: "", h: h_root }]
    tab[0] = Route { m: "GET", p: "/", h: h_root }
    tab[1] = Route { m: "GET", p: "/app", h: h_app }
    let r = Req { path: "/app", uid: "alice" }
    var i: I32 = 0
    while i < 2 {
        let e = tab[i]
        if e.p == r.path {
            let h = e.h
            let resp = h(r)
            println("HIT " + e.p + " -> " + resp.body)
        }
        i += 1
    }
    return 0
}
```

- [ ] **Step 2: 复现失败**

Run: `compiler-c/build/ctronc build tests/03n_struct_fn_field_array.ct`
Expected: FAIL `trans: v1:数组元素类型不支持`

- [ ] **Step 3: 修种子**

定位:trans_stmt.c 定长数组字面量/元素赋值的元素类型检查(`terr(c, "v1:数组元素类型不支持")`)。现门仅放行标量/Str 族。修法:struct 元素(含 fn 字段)放行,发射形态镜像既有 struct 数组支持(若 v1 定长数组本不支持任何 struct,则按 Task 3 的 T_ARR struct 元素形态补齐;先读该 case 与 T_ARR 的 box_elem 形态再动)。**若发现定长 struct 数组(无 fn 字段)同样被拒,一并放行并在测试里加对照组。**

- [ ] **Step 4: 镜像修 `compiler/src/trans_stmt.ct`**

同位同因(搜 `数组元素类型不支持`)。**执行注意**:trans_stmt.ct 有并行泳道在飞改动(git status 常态 M)——开工前先 `git log --oneline -3 -- compiler/src/trans_stmt.ct` 对齐,提交 pathspec 限定本任务三文件。

- [ ] **Step 5: 三口径验证 + 全套件**

同前(文件名 03n)。Expected: 两臂输出 `HIT /app -> app:alice`;check OK;无新增红。

- [ ] **Step 6: 提交**

```bash
git add tests/03n_struct_fn_field_array.ct compiler-c/src/trans_stmt.c compiler/src/trans_stmt.ct
git commit -m "fix(compiler): P0-4 定长数组放行 struct 元素(含 fn 字段)——镜像 struct 数组形态;双编译器镜像;探针转正 03n" -- tests/03n_struct_fn_field_array.ct compiler-c/src/trans_stmt.c compiler/src/trans_stmt.ct
```

---

### Task 5: 回归收口 + 账本回写

**Files:**
- Modify: `docs/c-rust-divergences.md`(P0 节:四件销账记录 + 探针编号)
- Modify: `tests/COVERAGE.md`(web P0 行)
- Modify: `docs/superpowers/specs/2026-09-27-web-framework-design.md` §9(P0-1..4 状态翻"已销账")

**Interfaces:**
- Consumes: Task 1-4 全绿;基线红账
- Produces: web v1 计划(下一份计划文档)的开工令

- [ ] **Step 1: 全套件双口径回归**

Run: `sh ci.sh`(或 COVERAGE 红账口径的等价全套件)
Expected: 新增 5 个测试文件全绿;基线红账无扩大。若有新红:逐条归因,属本计划引入则回修,不属则记红账并注明。

- [ ] **Step 2: 账本三处回写**

divergences:§9 四件各一行(销账提交号+探针文件);COVERAGE:web-P0 行(门=五探针双臂绿+全套件无新红);spec §9 表:P0-1..4 状态列改 `已销账(<提交号>)`,L4/L5 按 Task 0 判定改 `已立项/已销`。

- [ ] **Step 3: 提交**

```bash
git add docs/c-rust-divergences.md tests/COVERAGE.md docs/superpowers/specs/2026-09-27-web-framework-design.md
git commit -m "docs(compiler): web P0 四件销账回写——divergences/COVERAGE/spec §9;双探针判定附档" -- docs/c-rust-divergences.md tests/COVERAGE.md docs/superpowers/specs/2026-09-27-web-framework-design.md
```

---

## 后续(不在本计划内)

- **Plan 2(web 域包 v1)**:Task 0 的 L4/L5 判定落定后另行成文——`Upload.data` 类型随 L4、视图大构建指引随 L5;分片预计:core(Req/Resp/构造器)→ router → mw(session/log)→ server(装配循环)→ static/openapi → testkit → view builder → todo_app 迁移对照(验收门 §10)。
- 并发/SSE/WS/multipart 波、comptime 审计层:§13 既定,远期各自成计划。
