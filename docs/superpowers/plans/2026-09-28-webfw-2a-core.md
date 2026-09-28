# web 域包 v1·Plan 2a:核心纯函数面 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 web 域包的核心纯函数面——Resp 构造器族、Req 取参族(UTF-8 安全解码)、json_obj/csv_rows/err_json、视图微 builder、Router 匹配、中间件组合 + test_call——全部可经 interp+check 双臂验证。

**Architecture:** 新仓库根域包 `web/`(`use web.*`),收编 http/frm 原语(调用不复制);七个单职责文件。**宿主口径(定案):开发与验证走引导链 interp+check 双臂(`compiler/ctc.sh run|check`)——List[struct]/捕获闭包在 interp 全通;原生 emit 臂依赖账本候选 L6/L7(建议入 spec-gap W1 泳道排期),native e2e 门挂其销账后翻账,本计划不做任何绕行替身形态。** Plan 2b(server 循环/socket 面/static/openapi/示例迁移)待 2a 落库后另行成文。

**Tech Stack:** Ctron(interp+check 口径)、http/frm 原语、std/str·std/map。

**Spec:** `docs/superpowers/specs/2026-09-27-web-framework-design.md` §4(API 全暴露面)/§7(语义条款)/§14-1(拼接常数:一切多段拼接收敛 List[Str] 段收集+join)。

## Global Constraints

- **开发口径**:每任务 `sh compiler/ctc.sh check <file>` + `sh compiler/ctc.sh run <file>` 双绿为门;**原生 emit 臂不在本计划门内**(L6/L7 在册,挂 W1 泳道)。
- **Ctron 纪律**:无 `;` 结尾;无位运算算符;字符串裸 `{` 写 `\{`;负字面量写 `0 - 1`;`or` 两侧全括号。
- **§14-1 拼接纪律**:循环内禁止 `out = out + x` 裸拼——一律 List[Str] 段收集 + `std.str.join(parts, "")` 一次成型。
- **收编不复制**:frm 已有语义(ht_esc/auth_tok/cookie/st_serve/mx_*)一律 `use` 调用,禁止复制实现;web 自建面仅限 frm 缺口(UTF-8 安全百分号解码/类型化取参/路由组合)。
- **安全默认**(§7):构造器自带 charset/nosniff;redirect=303;URL 解码 UTF-8 四字节重组;重复请求头逗并。
- **提交纪律**:pathspec 限定;每任务一提交;worktree 执行(`.worktrees/webfw-2a`)。
- **代码即起点**:计划中的 Ctron 代码是完整起点,执行时允许经证据的微调(Plan 1 先例:诊断驱动重诊,报告如实记录)——但 API 签名(§4 定稿面)不得变形。

---

### Task 0: 包骨架 + 语义门

**Files:**
- Create: `web/Ctron.ctcl`
- Create: `web/core.ct`(本任务只放包头注与占位 pub 结构,后续任务填充)
- Modify: `tests/COVERAGE.md`(web-2a 行登记)

**Interfaces:**
- Produces: `web` 包可被 `use web.core.{...}` 消费;测试驱动口径确认

- [ ] **Step 1: 确认包测试驱动**

Run: `ls examples/todo_app/run.sh && grep -rn "ctc test\|ctronc test" examples/todo_app/run.sh docs/superpowers/specs/2026-09-26-todo-app-design.md | head -3`
记录本仓包测试命令先例(todo_app 为 `ctc test` 族);若 bootstrap 无 test 子命令,则包内 test 块经 `compiler-c/build/ctronc test`(seed 单文件口径,需把被测文件按 build.sh 式拼接)——**以 todo_app run.sh 实际命令为准,照抄其骨架**。

- [ ] **Step 2: 建包**

`web/Ctron.ctcl`:
```
pkg web
caps: net.listen
```
`web/core.ct` 包头:
```ctron
// core.ct —— web 核心:Resp/Req/构造器/取参/构建器(Plan 2a Task 1-4 填充)
// 设计: docs/superpowers/specs/2026-09-27-web-framework-design.md §4;纪律 §14-1
```
先只写包头,`sh compiler/ctc.sh check web/core.ct` 需 `check OK`。

- [ ] **Step 3: COVERAGE 登记 + 提交**

```bash
git add web/Ctron.ctcl web/core.ct tests/COVERAGE.md
git commit -m "feat(web): 域包骨架——pkg web/caps net.listen;Plan 2a 开波" -- web/Ctron.ctcl web/core.ct tests/COVERAGE.md
```

---

### Task 1: Resp 构造器族 + 链式

**Files:**
- Modify: `web/core.ct`

**Interfaces:**
- Produces: `Pair{k,v}`、`Resp{status,headers,body}`、`html/json/text/redirect/status/err_json(bytes/send_file 归 2b)`、`.with/.with_status`、`resp_header(r,k)`

- [ ] **Step 1: 写实现(web/core.ct 追加)**

```ctron
use std.str.{ join }

pub struct Pair {
    let k: Str
    let v: Str
}

pub struct Resp {
    let status: I32
    let headers: List[Pair]
    let body: Str
}

fn base(status: I32, ctype: Str, body: Str) -> Resp {
    var hs: List[Pair] = List[Pair]()
    hs.push(Pair { k: "Content-Type", v: ctype })
    hs.push(Pair { k: "X-Content-Type-Options", v: "nosniff" })
    return Resp { status: status, headers: hs, body: body }
}

pub fn html(body: Str) -> Resp {
    return base(200, "text/html; charset=utf-8", body)
}

pub fn json(body: Str) -> Resp {
    return base(200, "application/json", body)
}

pub fn text(body: Str) -> Resp {
    return base(200, "text/plain", body)
}

pub fn redirect(loc: Str) -> Resp {
    var hs: List[Pair] = List[Pair]()
    hs.push(Pair { k: "Location", v: loc })
    return Resp { status: 303, headers: hs, body: "" }
}

pub fn status(n: I32, body: Str) -> Resp {
    return base(n, "text/plain", body)
}

pub fn err_json(n: I32, msg: Str) -> Resp {
    return json_obj().str("error", msg).end_status(n)
}

pub fn with(r: Resp, k: Str, v: Str) -> Resp {
    var hs: List[Pair] = List[Pair]()
    var i: I32 = 0
    while i < r.headers.len {
        hs.push(r.headers[i])
        i += 1
    }
    hs.push(Pair { k: k, v: v })
    return Resp { status: r.status, headers: hs, body: r.body }
}

pub fn with_status(r: Resp, n: I32) -> Resp {
    return Resp { status: n, headers: r.headers, body: r.body }
}

pub fn resp_header(r: Resp, k: Str) -> Str {
    var i: I32 = 0
    while i < r.headers.len {
        if r.headers[i].k == k {
            return r.headers[i].v
        }
        i += 1
    }
    return ""
}

pub fn resp_body(r: Resp) -> Str {
    return r.body
}

pub fn resp_status(r: Resp) -> I32 {
    return r.status
}
```

JSON 构建器(同文件,§4.4;函数式段收集,§14-1 合规):

```ctron
pub struct JObj {
    let parts: List[Str]
}

pub fn json_obj() -> JObj {
    return JObj { parts: List[Str]() }
}

pub fn j_str(b: JObj, k: Str, v: Str) -> JObj {
    return jpush(b, "\{" + json_esc(k) + "\":\"" + json_esc(v) + "\"")
}

pub fn j_i64(b: JObj, k: Str, v: I64) -> JObj {
    return jpush(b, "\{" + json_esc(k) + "\":" + v.to_string() + "")
}

pub fn j_bool(b: JObj, k: Str, v: Bool) -> JObj {
    var s = "false"
    if v {
        s = "true"
    }
    return jpush(b, "\{" + json_esc(k) + "\":" + s + "")
}

pub fn j_strs(b: JObj, k: Str, xs: List[Str]) -> JObj {
    var items: List[Str] = List[Str]()
    var i: I32 = 0
    while i < xs.len {
        items.push("\"" + json_esc(xs[i]) + "\"")
        i += 1
    }
    return jpush(b, "\{" + json_esc(k) + "\":[" + join(items, ",") + "]")
}

fn jpush(b: JObj, piece: Str) -> JObj {
    var ps: List[Str] = List[Str]()
    var i: I32 = 0
    while i < b.parts.len {
        ps.push(b.parts[i])
        i += 1
    }
    ps.push(piece)
    return JObj { parts: ps }
}

fn jfinish(b: JObj, wrap: Str) -> Str {
    var ps: List[Str] = List[Str]()
    ps.push("\{")
    var i: I32 = 0
    while i < b.parts.len {
        if i > 0 {
            ps.push(",")
        }
        ps.push(b.parts[i])
        i += 1
    }
    ps.push("}" + wrap)
    return join(ps, "")
}

pub fn end(b: JObj) -> Str {
    return jfinish(b, "")
}

pub fn end_status(b: JObj, n: I32) -> Resp {
    return status(n, jfinish(b, ""))
}

fn json_esc(s: Str) -> Str {
    var ps: List[Str] = List[Str]()
    var i: I32 = 0
    while i < s.len {
        let c = byte_at(s, i)
        if c == 34 {
            ps.push("\\\"")
        } else if c == 92 {
            ps.push("\\\\")
        } else if c == 10 {
            ps.push("\\n")
        } else if c == 13 {
            ps.push("\\r")
        } else if c == 9 {
            ps.push("\\t")
        } else {
            ps.push(byte_slice(s, i, i + 1))
        }
        i += 1
    }
    return join(ps, "")
}
```

注:段间逗号已并入每段前缀(每 piece 自带前导 `"k":` 形,段间以 `,` join 由 jfinish 处理——piece 形如 `"k":v`,jfinish 以逗号连接)。执行时若 `\{` 转义与拼接顺序有出入,以 `ctc.sh run` 实测为准微调,报告记录。

- [ ] **Step 2: 写测试(同文件尾)**

```ctron
test "resp 构造器安全默认" {
    let r = html("<h1>hi</h1>")
    assert(resp_status(r) == 200)
    assert(contains(resp_header(r, "Content-Type"), "text/html"))
    assert(contains(resp_header(r, "X-Content-Type-Options"), "nosniff"))
    let rd = redirect("/app")
    assert(resp_status(rd) == 303)
    assert(resp_header(rd, "Location") == "/app")
    let r2 = with_status(with(json("{}"), "Set-Cookie", "a=b"), 201)
    assert(resp_status(r2) == 201)
    assert(resp_header(r2, "Set-Cookie") == "a=b")
}

test "json_obj 转义与类型" {
    let s = json_obj().str("t", "a\"b").i64("n", 42).bool("ok", true).end()
    assert(contains(s, "\"t\":\"a\\\"b\""))
    assert(contains(s, "\"n\":42"))
    assert(contains(s, "\"ok\":true"))
    let e = err_json(422, "bad")
    assert(resp_status(e) == 422)
    assert(contains(resp_body(e), "\"error\":\"bad\""))
}
```

(`contains` 来自 `use std.str.{ contains, join }`——文件头补 use。)

- [ ] **Step 3: 验证 + 提交**

Run: `sh compiler/ctc.sh check web/core.ct && sh compiler/ctc.sh run web/core.ct`
Expected: `check OK` + run 无失败输出(test 断言全过,rc=0)。
```bash
git add web/core.ct
git commit -m "feat(web): Resp 构造器族+链式+json_obj/csv 构建器底座——安全默认(nosniff/charset)内置;§14-1 段收集合规" -- web/core.ct
```

---

### Task 2: Req 取参族(UTF-8 安全解码)

**Files:**
- Modify: `web/core.ct`

**Interfaces:**
- Produces: `Req[S]`泛型结构、`req_of[S](method, path, body, state)` 构造、`param/param_i64/param_f64/query/query_all/form/form_all/header/cookie`、`pdec`(UTF-8 安全百分号解码,web 自建面——frm/form 的 C8 域拒非 ASCII 为在册缺口)

- [ ] **Step 1: 写实现**

```ctron
pub struct Req[S] {
    let method: Str
    let path: Str
    let query: Str
    let body: Str
    let headers: List[Pair]
    let cookies: List[Pair]
    let params: List[Pair]
    let state: S
}

pub fn req_of[S](method: Str, path: Str, state: S) -> Req[S] {
    return req_full(method, path, "", List[Pair](), state)
}

pub fn req_full[S](method: Str, path: Str, body: Str, hdrs: List[Pair], state: S) -> Req[S] {
    var q = ""
    var p = path
    let qpos = index_of(path, "?")
    if qpos > 0 - 1 {
        p = strip_to(path, qpos)
        q = byte_slice(path, qpos + 1, path.len)
    }
    var cs: List[Pair] = List[Pair]()
    let ck = hdr_get(hdrs, "Cookie")
    if ck.len > 0 {
        cs = cookie_parse(ck)
    }
    return Req { method: method, path: p, query: q, body: body, headers: hdrs, cookies: cs, params: List[Pair](), state: state }
}

fn hdr_get(hdrs: List[Pair], name: Str) -> Str {
    var i: I32 = 0
    while i < hdrs.len {
        if eq_ignore_ascii_case(hdrs[i].k, name) {
            return hdrs[i].v
        }
        i += 1
    }
    return ""
}

fn strip_to(s: Str, n: I32) -> Str {
    var ps: List[Str] = List[Str]()
    var i: I32 = 0
    while i < n {
        ps.push(byte_slice(s, i, i + 1))
        i += 1
    }
    return join(ps, "")
}

// —— UTF-8 安全百分号解码(web 自建;frm/form C8 域在册缺口;todo_app pdec 经验收的算法 + §14-1 段收集形)——
fn hexv(c: I32) -> I32 {
    if c >= 48 && c <= 57 { return c - 48 }
    if c >= 65 && c <= 70 { return c - 55 }
    if c >= 97 && c <= 102 { return c - 87 }
    return 0 - 1
}

fn pctb(s: Str, i: I32) -> I32 {
    if i + 2 >= s.len || byte_at(s, i) != 37 {
        return 0 - 1
    }
    let h1 = hexv(byte_at(s, i + 1))
    let h2 = hexv(byte_at(s, i + 2))
    if h1 < 0 || h2 < 0 {
        return 0 - 1
    }
    return h1 * 16 + h2
}

pub fn pdec(s: Str) -> Str {
    var ps: List[Str] = List[Str]()
    var i: I32 = 0
    while i < s.len {
        let c = byte_at(s, i)
        if c == 43 {
            ps.push(" ")
            i += 1
            continue
        }
        if c == 37 {
            let b0 = pctb(s, i)
            if b0 < 0 {
                ps.push("%")
                i += 1
                continue
            }
            if b0 < 128 {
                ps.push(utf8_one(b0))
                i += 3
                continue
            }
            var seq = 2
            if b0 >= 240 { seq = 4 } else if b0 >= 224 { seq = 3 }
            let b1 = pctb(s, i + 3)
            if b1 < 128 || b1 > 191 {
                ps.push("%")
                i += 1
                continue
            }
            if seq == 2 {
                ps.push(utf8_enc((b0 - 192) * 64 + b1 - 128))
                i += 6
                continue
            }
            let b2 = pctb(s, i + 6)
            if b2 < 128 || b2 > 191 {
                ps.push("%")
                i += 1
                continue
            }
            if seq == 3 {
                ps.push(utf8_enc(((b0 - 224) * 64 + b1 - 128) * 64 + b2 - 128))
                i += 9
                continue
            }
            let b3 = pctb(s, i + 9)
            if b3 < 128 || b3 > 191 {
                ps.push("%")
                i += 1
                continue
            }
            ps.push(utf8_enc((((b0 - 240) * 64 + b1 - 128) * 64 + b2 - 128) * 64 + b3 - 128))
            i += 12
            continue
        }
        ps.push(byte_slice(s, i, i + 1))
        i += 1
    }
    return join(ps, "")
}

fn utf8_one(v: I32) -> Str {
    // ASCII 域单字符(todo_app one_ascii 同款表查法;域外回空)
    if v < 32 || v > 126 {
        return ""
    }
    return byte_slice(" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~", v - 32, v - 31)
}

// —— 取参访问器(六 Str 缺回空 + 类型化 Option)——
pub fn param[S](r: Req[S], name: Str) -> Str {
    return pair_get(r.params, name)
}

pub fn pair_get(ps: List[Pair], k: Str) -> Str {
    var i: I32 = 0
    while i < ps.len {
        if ps[i].k == k {
            return ps[i].v
        }
        i += 1
    }
    return ""
}

pub fn query[S](r: Req[S], name: Str) -> Str {
    return pdec(q_raw(r.query, name))
}

pub fn query_all[S](r: Req[S], name: Str) -> List[Str] {
    return pdec_all(r.query, name)
}

pub fn form[S](r: Req[S], name: Str) -> Str {
    return pdec(q_raw(r.body, name))
}

pub fn form_all[S](r: Req[S], name: Str) -> List[Str] {
    return pdec_all(r.body, name)
}

pub fn header[S](r: Req[S], name: Str) -> Str {
    return hdr_get(r.headers, name)
}

pub fn cookie[S](r: Req[S], name: Str) -> Str {
    return pair_get(r.cookies, name)
}

fn q_raw(q: Str, name: Str) -> Str {
    var seg = 0
    var i: I32 = 0
    while i <= q.len {
        if i == q.len || byte_at(q, i) == 38 {
            let pair = strip_to(q, 0 - 1)
            let segs = partition_range(q, seg, i)
            let eq = index_of(segs, "=")
            if eq > 0 {
                let k = strip_to(segs, eq)
                if k == name {
                    return byte_slice(segs, eq + 1, segs.len)
                }
            }
            seg = i + 1
        }
        i += 1
    }
    return ""
}
```

执行注意:`q_raw/pdec_all/partition_range` 的段切分用 `std.str` 既有件(`split`/`partition`)优先,以上手写切分若与 std 重复则改调 std(`use std.str.{ split, partition, join, index_of, eq_ignore_ascii_case, contains }`),报告记录取舍。`param_i64/param_f64` 返回 `Option[I64]/Option[F64]`(parse_i64/strconv 族 + `Some`),照 §4.3 面。

- [ ] **Step 2: 测试**

```ctron
test "query/form 解码 UTF-8 安全" {
    let r = req_full("GET", "/s?q=a%20b&tag=%E4%B8%AD%E6%96%87&tag=x", "", List[Pair](), 0)
    assert(query(r, "q") == "a b")
    let tags = query_all(r, "tag")
    assert(tags.len == 2)
    assert(tags[0] == "中文")
    assert(query(r, "nope") == "")
}

test "cookie/header/params" {
    var hs: List[Pair] = List[Pair]()
    hs.push(Pair { k: "Cookie", v: "sid=abc; theme=dark" })
    hs.push(Pair { k: "User-Agent", v: "t" })
    let r = req_full("GET", "/x", "", hs, 0)
    assert(cookie(r, "sid") == "abc")
    assert(cookie(r, "theme") == "dark")
    assert(header(r, "user-agent") == "t")
}
```

- [ ] **Step 3: 验证 + 提交**(同 Task 1 门;`ctc.sh check` + `run` 双绿)

```bash
git add web/core.ct
git commit -m "feat(web): Req 泛型取参族——UTF-8 安全百分号解码(frm C8 域缺口自建位)/query_all 对称/cookie 解析;六 Str 缺回空纪律" -- web/core.ct
```

---

### Task 3: Router 匹配 + 中间件 + test_call

**Files:**
- Create: `web/router.ct`

**Interfaces:**
- Produces: `Router[S]`/`router(state)`/`.get/.post/.put/.delete/.patch`/`.middleware(mw)`/`.mount_at(前缀, 子)`/`route_match(r, method, path) -> Match`/`dispatch(r, method, path, body) -> Resp`(test_call 的本体,Plan 2b 的 serve 循环直接复用);mw 形 `fn(Req[S], fn(Req[S]) -> Resp) -> Resp`

- [ ] **Step 1: 写实现(web/router.ct)**

核心形态(完整起点;泛型 + fn 字段全在 interp 口径验证):

```ctron
use web.core.{ Req, Resp, html, status, Pair, pair_get, with }
use std.str.{ split, join }

pub struct RouteSeg {
    let kind: Str      // "lit" / "par" / "splat"
    let text: Str
}

pub struct Route[S] {
    let m: Str
    let segs: List[RouteSeg]
    let h: fn(Req[S]) -> Resp
}

pub struct Router[S] {
    let state: S
    let routes: List[Route[S]]
    let mws: List[fn(Req[S], fn(Req[S]) -> Resp) -> Resp]
}

pub fn router[S](state: S) -> Router[S] {
    return Router { state: state, routes: List[Route[S]](), mws: List[fn(Req[S], fn(Req[S]) -> Resp) -> Resp]() }
}

fn segs_of(p: Str) -> List[RouteSeg] {
    var out: List[RouteSeg] = List[RouteSeg]()
    var parts = split(p, 47)   // '/'
    var i: I32 = 0
    while i < parts.len {
        if parts[i].len > 0 {
            if starts_with(parts[i], ":") {
                out.push(RouteSeg { kind: "par", text: byte_slice(parts[i], 1, parts[i].len) })
            } else if starts_with(parts[i], "*") {
                out.push(RouteSeg { kind: "splat", text: byte_slice(parts[i], 1, parts[i].len) })
            } else {
                out.push(RouteSeg { kind: "lit", text: parts[i] })
            }
        }
        i += 1
    }
    return out
}

fn segs_append(base: List[RouteSeg], extra: List[RouteSeg]) -> List[RouteSeg] {
    var out: List[RouteSeg] = List[RouteSeg]()
    var i: I32 = 0
    while i < base.len {
        out.push(base[i])
        i += 1
    }
    i = 0
    while i < extra.len {
        out.push(extra[i])
        i += 1
    }
    return out
}

fn r_add(r: Router[S], m: Str, p: Str, h: fn(Req[S]) -> Resp) -> Router[S] {
    var rs: List[Route[S]] = List[Route[S]]()
    var i: I32 = 0
    while i < r.routes.len {
        rs.push(r.routes[i])
        i += 1
    }
    rs.push(Route { m: m, segs: segs_of(p), h: h })
    return Router { state: r.state, routes: rs, mws: r.mws }
}

pub fn get[S](r: Router[S], p: Str, h: fn(Req[S]) -> Resp) -> Router[S] { return r_add(r, "GET", p, h) }
pub fn post[S](r: Router[S], p: Str, h: fn(Req[S]) -> Resp) -> Router[S] { return r_add(r, "POST", p, h) }
pub fn put[S](r: Router[S], p: Str, h: fn(Req[S]) -> Resp) -> Router[S] { return r_add(r, "PUT", p, h) }
pub fn delete[S](r: Router[S], p: Str, h: fn(Req[S]) -> Resp) -> Router[S] { return r_add(r, "DELETE", p, h) }
pub fn patch[S](r: Router[S], p: Str, h: fn(Req[S]) -> Resp) -> Router[S] { return r_add(r, "PATCH", p, h) }
```

匹配 + 中间件链 + mount(值变换;`.middleware` 收 fn 值入 mws 表;`.mount_at` 前缀拼接 + mws 继承):

```ctron
pub struct Match[S] {
    let kind: I32      // 1=命中 0=404 -1=405
    let route: Route[S]
    let params: List[Pair]
    let splat: Str
    let allow: Str
}

pub fn route_match[S](r: Router[S], m: Str, path: Str) -> Match[S] {
    let psegs = split(path, 47)
    var allow: Str = ""
    var i: I32 = 0
    while i < r.routes.len {
        let rt = r.routes[i]
        var ps: List[Pair] = List[Pair]()
        var splat = ""
        var ok = true
        var si: I32 = 0
        var pi: I32 = 0
        while si < rt.segs.len {
            if rt.segs[si].kind == "splat" {
                var rest: List[Str] = List[Str]()
                while pi < psegs.len {
                    rest.push(psegs[pi])
                    pi += 1
                }
                splat = join(rest, "/")
                pi = psegs.len
                si += 1
                continue
            }
            if pi >= psegs.len {
                ok = false
                break
            }
            if rt.segs[si].kind == "lit" {
                if rt.segs[si].text != psegs[pi] {
                    ok = false
                    break
                }
            } else {
                ps.push(Pair { k: rt.segs[si].text, v: psegs[pi] })
            }
            pi += 1
            si += 1
        }
        if ok && pi == psegs.len {
            if rt.m == m {
                return Match { kind: 1, route: rt, params: ps, splat: splat, allow: "" }
            }
            if allow.len == 0 {
                allow = rt.m
            } else {
                allow = allow + "," + rt.m
            }
        }
        i += 1
    }
    if allow.len > 0 {
        return Match { kind: 0 - 1, route: rt_dummy[S](), params: List[Pair](), splat: "", allow: allow }
    }
    return Match { kind: 0, route: rt_dummy[S](), params: List[Pair](), splat: "", allow: "" }
}

fn rt_dummy[S]() -> Route[S] {
    return Route { m: "", segs: List[RouteSeg](), h: no_h[S] }
}

fn no_h[S](r: Req[S]) -> Resp {
    return status(404, "not found")
}

pub fn dispatch[S](r: Router[S], method: Str, path: Str, body: Str) -> Resp {
    let mt = route_match(r, method, path)
    if mt.kind == 0 {
        return status(404, "404 not found")
    }
    if mt.kind == 0 - 1 {
        return with(status(405, "405 method not allowed"), "Allow", mt.allow)
    }
    let req = req_with_params(r.state, method, path, body, mt.params)
    let h = mt.route.h
    return run_chain(r.mws, 0, req, h)
}

fn run_chain[S](mws: List[fn(Req[S], fn(Req[S]) -> Resp) -> Resp], idx: I32, req: Req[S], h: fn(Req[S]) -> Resp) -> Resp {
    if idx >= mws.len {
        return h(req)
    }
    let mw = mws[idx]
    return mw(req, |rq| run_chain(mws, idx + 1, rq, h))
}
```

(`req_with_params` = core 的 `req_full` 后填 params——core 补一个 `req_set_params`;`with`/`status`/404/405 默认体可经 `.not_found/.method_not_allowed` 覆写——2b 补挂点,本任务固定默认文本。)

- [ ] **Step 2: 测试(web/router.ct 尾)**

```ctron
fn h_hello(r: Req[I64]) -> Resp {
    return html("hi:" + param(r, "name"))
}

fn need_x(r: Req[I64], next: fn(Req[I64]) -> Resp) -> Resp {
    if cookie(r, "x").len == 0 {
        return redirect("/login")
    }
    return next(r)
}

test "路由命中/参数/405/404" {
    let r = get(post(router(0), "/hi/:name", h_hello), "/bye", h_hello)
    let hit = dispatch(r, "GET", "/hi/alice", "")
    assert(resp_status(hit) == 200)
    assert(contains(resp_body(hit), "alice"))
    let m405 = dispatch(r, "DELETE", "/hi/a", "")
    assert(resp_status(m405) == 405)
    let m404 = dispatch(r, "GET", "/nope", "")
    assert(resp_status(m404) == 404)
}

test "中间件链短路与穿透" {
    let r0 = get(router(0), "/x", h_hello)
    let r1 = middleware(r0, need_x)
    let blocked = dispatch(r1, "GET", "/x", "")
    assert(resp_status(blocked) == 303)
}
```

(`middleware(r, mw)` = mws 追加返回新 Router——本任务给单层验证;`splat`/`mount_at` 断言并入本任务测试。执行时若泛型 fn 字段的 interp 求值有坑,以最小复现记录并调整形态——报告如实。)

- [ ] **Step 3: 验证 + 提交**

Run: `sh compiler/ctc.sh check web/router.ct web/core.ct && sh compiler/ctc.sh run web/router.ct`
Expected: check OK 双文件 + run 断言全过。

```bash
git add web/router.ct web/core.ct
git commit -m "feat(web): Router 值语义匹配(静态>:param>*splat/405+Allow/启动冲突 panic)+中间件平铺链+dispatch(test_call 本体,serve 复用)" -- web/router.ct web/core.ct
```

---

### Task 4: 视图微 builder + 会话糖(Task 2b 前的最后纯函数件)

**Files:**
- Create: `web/view.ct`

**Interfaces:**
- Produces: `El`/`el(tag)`/`.cls/.attr/.child/.children/.text/.raw/.done`;`grant_session/drop_session/flash` 归 2b(依赖 with_sessions 键管理,本任务不做)

- [ ] **Step 1: 写实现**

```ctron
use web.core.{ Pair }
use std.str.{ join }
use http.frm.html.{ ht_esc }

pub struct El {
    let tag: Str
    let attrs: List[Pair]
    let kids: List[Str]     // 子节点已是成品 HTML 段(text/child 递归 done)
}

pub fn el(tag: Str) -> El {
    return El { tag: tag, attrs: List[Pair](), kids: List[Str]() }
}

fn el_with(e: El, a: Pair, kid: Str) -> El {
    var as_: List[Pair] = List[Pair]()
    var i: I32 = 0
    while i < e.attrs.len {
        as_.push(e.attrs[i])
        i += 1
    }
    if a.k.len > 0 {
        as_.push(a)
    }
    var ks: List[Str] = List[Str]()
    i = 0
    while i < e.kids.len {
        ks.push(e.kids[i])
        i += 1
    }
    if kid.len > 0 {
        ks.push(kid)
    }
    return El { tag: e.tag, attrs: as_, kids: ks }
}

pub fn cls(e: El, v: Str) -> El { return el_with(e, Pair { k: "class", v: v }, "") }
pub fn attr(e: El, k: Str, v: Str) -> El { return el_with(e, Pair { k: k, v: ht_esc(v) }, "") }
pub fn child(e: El, k: El) -> El { return el_with(e, Pair { k: "", v: "" }, done(k)) }
pub fn children(e: El, ks: List[El]) -> El {
    var i: I32 = 0
    var acc = e
    while i < ks.len {
        acc = child(acc, ks[i])
        i += 1
    }
    return acc
}
pub fn text(e: El, v: Str) -> El { return el_with(e, Pair { k: "", v: "" }, ht_esc(v)) }
pub fn raw(e: El, v: Str) -> El { return el_with(e, Pair { k: "", v: "" }, v) }

pub fn done(e: El) -> Str {
    var ps: List[Str] = List[Str]()
    ps.push("<" + e.tag)
    var i: I32 = 0
    while i < e.attrs.len {
        ps.push(" " + e.attrs[i].k + "=\"" + e.attrs[i].v + "\"")
        i += 1
    }
    if e.kids.len == 0 {
        ps.push("/>")
        return join(ps, "")
    }
    ps.push(">")
    i = 0
    while i < e.kids.len {
        ps.push(e.kids[i])
        i += 1
    }
    ps.push("</" + e.tag + ">")
    return join(ps, "")
}
```

- [ ] **Step 2: 测试(结构性转义为核心断言)**

```ctron
test "el 结构性转义" {
    let a = el("a").attr("href", "http://x/?a=1&b=2").text("<b>&\"")
    let s = done(a)
    assert(contains(s, "href=\"http://x/?a=1&amp;b=2\""))
    assert(contains(s, "&lt;b&gt;&amp;\""))
    assert(starts_with(s, "<a ") && ends_with(s, "</a>"))
    let li = el("li").cls("done").child(el("span").text("x"))
    assert(contains(done(li), "<li class=\"done\"><span>x</span></li>"))
}
```

- [ ] **Step 3: 验证 + 提交**

```bash
git add web/view.ct
git commit -m "feat(web): 视图微 builder——text/attr 自动 ht_esc(结构性转义,XSS 纪律变类型)/raw 显式逃生口;§14-1 段收集" -- web/view.ct
```

---

### Task 5(收口): 全包语义门 + README API 表

**Files:**
- Modify: `web/core.ct`、`web/router.ct`、`web/view.ct`(测试补全至全 API 覆盖)
- Create: `web/README.md`
- Modify: `tests/COVERAGE.md`

- [ ] **Step 1: API 覆盖核对**——对照 spec §4 逐条:五构造器/`.with/.with_status`/`err_json`/六取参+`_i64`/`json_obj` 五件/`csv_rows`(若 2b 前置需要则本任务补,~10 行 join)/`el` 族/router 全组合子/middleware 链/test_call(=dispatch 别名)。缺哪个补哪个(实现+断言)。
- [ ] **Step 2**: `sh compiler/ctc.sh check web/core.ct web/router.ct web/view.ct` 全 OK;三文件 run 全过;meta 门(行为件须含 test 块)过。
- [ ] **Step 3**: README API 表(spec §4 定稿面照录)+ COVERAGE 登记(web-2a 行:七件语义门+覆盖清单)。
- [ ] **Step 4**: 提交 + 合并回 main(worktree 流程,同 Plan 1)。

```bash
git add web/ tests/COVERAGE.md
git commit -m "feat(web): 2a 收口——全 API 覆盖语义门+README API 表" -- web/ tests/COVERAGE.md
```

---

## 后续(不在本计划)

- **Plan 2b**:with_sessions/grant_session/drop_session/flash(收编 frm/auth)、server serve 循环(net 装配/keep-alive/序列化咽喉/panic→500/HEAD)、static/openapi、csv_rows、迷你 todo 示例 + todo_app 迁移对照(§10 验收门)、原生 e2e 门(挂 L6/L7 销账)。
- **L6/L7**:候选已入 spec §9,建议纳入 spec-gap W1 泳道排期(待用户裁决);销账后 2b 的原生 e2e 门翻账。
