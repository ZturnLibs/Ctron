# web 域包 v1·Plan 2b:serve 循环+会话+static/openapi+示例迁移 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** web 域包的运行时半边——serve 循环(序列化咽喉一次性写对)、会话族(收编 frm/auth)、static/openapi、`req.json`+JDoc 校验族——并以迷你 todo 示例与 todo_app 迁移对照收官。

**Architecture:** 承接 Plan 2a(已合入 main,746aa14)的 core/router/view。**宿主口径:与 2a 同——引导链 interp+check 双臂开发验证;net socket 面(serve 真循环)以 Task 3 的探针定界(interp 有 net 内建则 interp e2e,否则 e2e 挂 L6/L7 销账后原生翻账)。** 序列化咽喉(Resp→线上字节)做成纯函数 `render(resp) -> Str`,Task 0 纯函数单测先行——serve 循环只是它的调用者。

**Tech Stack:** Ctron(interp+check 口径)、net 门面、http/parse、http/frm(auth/html/static/metrics/timeout)、std/*。

**Spec:** `docs/superpowers/specs/2026-09-27-web-framework-design.md` §4.1(装配)/§6(serve 职责)/§7(语义条款)/§13(次波预置)。

## Global Constraints(承 2a,全部有效)

- 开发口径:引导链 `sh compiler/ctc.sh check <f>` + `sh compiler/ctc.sh <f>`(裸 run,无 run 子命令)双绿为门;**原生 emit 臂不在门内**(L6/L7 挂 W1 泳道)。
- Ctron 纪律:无 `;`;裸 `{` 必 `"\{" + "}"` 拼接(check OK 而 run 崩的在册坑);无位运算;负字面量 `0 - 1`;`own` 是关键字勿作标识符;E2060 泛型经 Req[S] 须显式 TypeArgs;use 选择性合并 keep 闭包 test 体引用跨文件同名即 E5030(M-T5w-1,跨文件测试载体命名避开 core 公有名)。
- §14-1 拼接纪律;收编不复制(frm 语义一律 use)。
- 提交纪律:pathspec 限定;每任务一提交;worktree `.worktrees/webfw-2b`。
- **开波前置(执行会话第一步)**:`cd /Users/zyj/Zturn/Ctron && git add tests/COVERAGE.md && git commit -m "docs(coverage): 清理并集解法残留的孤立冲突标记行" -- tests/COVERAGE.md && git add docs/superpowers/plans/2026-09-28-webfw-2b-serve.md && git commit -m "docs(plan): web 2b 实施计划" -- docs/superpowers/plans/2026-09-28-webfw-2b-serve.md`(2a 收尾会话 shell 故障遗留的两笔未提交,已在工作区)。

---

### Task 0: 序列化咽喉 render + req.json/JDoc 校验族

**Files:**
- Create: `web/serve.ct`(本任务只放 render 与 http 常量;serve 循环 Task 3 来填)
- Modify: `web/core.ct`(req.json + JDoc 校验族)

**Interfaces:**
- Produces: `render(resp: Resp) -> Str`(HTTP/1.1 报文;唯一状态行拼接点)、`reason(n: I32) -> Str`、`req.json[S](r) -> Option[JDoc]`、`JDoc.str/i64/bool/list + str_between/i64_between`

- [ ] **Step 1: render(reason 表 + 报文组装,纯函数)**

```ctron
use std.str.{ join }
use web.core.{ Resp, resp_status, resp_body, Pair }

pub fn reason(n: I32) -> Str {
    if n == 200 { return "OK" }
    if n == 201 { return "Created" }
    if n == 204 { return "No Content" }
    if n == 303 { return "See Other" }
    if n == 400 { return "Bad Request" }
    if n == 401 { return "Unauthorized" }
    if n == 403 { return "Forbidden" }
    if n == 404 { return "Not Found" }
    if n == 405 { return "Method Not Allowed" }
    if n == 409 { return "Conflict" }
    if n == 413 { return "Payload Too Large" }
    if n == 422 { return "Unprocessable Entity" }
    if n == 429 { return "Too Many Requests" }
    if n == 500 { return "Internal Server Error" }
    if n == 503 { return "Service Unavailable" }
    return "Unknown"
}

// 唯一咽喉:全仓库只有这里拼状态行/头区/体(§4.4 演进契约的支点)
pub fn render(r: Resp) -> Str {
    var ps: List[Str] = List[Str]()
    ps.push("HTTP/1.1 " + resp_status(r).to_string() + " " + reason(resp_status(r)) + "\r\n")
    var i: I32 = 0
    let hs = r.headers
    while i < hs.len {
        ps.push(hs[i].k + ": " + hs[i].v + "\r\n")
        i += 1
    }
    let body = resp_body(r)
    ps.push("Content-Length: " + body.len.to_string() + "\r\n")
    ps.push("Connection: close\r\n\r\n")
    ps.push(body)
    return join(ps, "")
}
```

注:Connection: close 为 2a 同款串行口径;keep-alive 挂 L6/L7 后的并发波(§13-④)。

- [ ] **Step 2: req.json + JDoc(core.ct 追加;收编 std/json)**

```ctron
use std.json.{ parse }   // parse(s) -> Result[List[List[Str]], Str];JDoc 即其 entries 包装

pub struct JDoc {
    let ok: Bool
    let entries: List[List[Str]]
}

pub fn json_of[S](r: Req[S]) -> JDoc {
    let p = parse(r.body)
    return JDoc { ok: p.is_ok(), entries: p.or(List[List[Str]]()) }
}

pub fn jd_str(d: JDoc, path: Str) -> Option[Str]   // qval 系收编;None=缺
pub fn jd_i64(d: JDoc, path: Str) -> Option[I64]   // jget_i64 收编
pub fn jd_bool(d: JDoc, path: Str) -> Option[Bool]
pub fn jd_str_between(d: JDoc, path: Str, lo: I32, hi: I32) -> Option[Str]   // 校验族:取值+域检查一体
pub fn jd_i64_between(d: JDoc, path: Str, lo: I64, hi: I64) -> Option[I64]
```

(实现即 std/json `parse/qval/jget_i64` 的薄包装+域检查;**执行时以 std/json 实际签名微调**,报告记录。)

- [ ] **Step 3: 测试 + 门**

render 断言(精确串):200 html 体、头序、Content-Length 与体字节一致、404 reason。json 族:合法 body 取值/坏 body None/域检查 None 边界。
Run: `sh compiler/ctc.sh check web/serve.ct web/core.ct && sh compiler/ctc.sh web/serve.ct && sh compiler/ctc.sh web/core.ct`

- [ ] **Step 4: 提交**

```bash
git add web/serve.ct web/core.ct
git commit -m "feat(web): 序列化咽喉 render+reason 表/req.json+JDoc 校验族(str/i64_between)——状态行全仓唯一点" -- web/serve.ct web/core.ct
```

---

### Task 1: 会话族(收编 frm/auth)+ grant/drop/flash

**Files:**
- Create: `web/mw.ct`
- Modify: `web/core.ct`(Resp 增 Set-Cookie 便捷即 `.with` 语法糖不必;flash 经 cookie)

**Interfaces:**
- Produces: `with_sessions[S](key: Str) -> 中间件值`(解析 sid→验签→注入 `req.session_str("uid")`——2a 的 Req 无 session 字段,以 **params 同款 Pair 列表注入或 core 增 session 列表字段**;执行时取最小改动,报告记录)、`grant_session(resp, key, uid, ttl)`、`drop_session(resp, key)`、`flash(resp, msg)`、`req_flash[S](r)`、`log_requests`、`timeout_ms(n)`(校验语义面;真超时挂并发波)

- [ ] **Step 1: 实现要点**

frm/auth 收编面(照 todo_app 验收过的口径):签发 `auth_tok_issue(key, uid, now_s, ttl)`/验证 `auth_tok_verify(key, tok, now_s)`/`auth_cookie_get/set`。**now 经参数注入**(§8-A5 可测性条款;serve 循环接 now_ms(),测试传 fake)。flash:cookie `web_flash=<urlenc msg>`;`req_flash` 读即清(响应侧由 with_sessions 自动补清除 Set-Cookie——实现取「读时置标记」的最小形态,报告记录)。

- [ ] **Step 2: 测试**

签发→注入→读 uid 全链;篡改签名拒;过期拒(now+ttl+1);grant/drop 的 Set-Cookie 头断言;flash 写读清。

- [ ] **Step 3: 门 + 提交**

```bash
git add web/mw.ct web/core.ct
git commit -m "feat(web): 会话族收编 frm/auth(with_sessions/grant/drop)+flash 读即清+log_requests/timeout_ms 中间件" -- web/mw.ct web/core.ct
```

---

### Task 2: req.json 校验族示例接线 + static/openapi

**Files:**
- Create: `web/static.ct`、`web/openapi.ct`
- Modify: `web/router.ct`(`.static(前缀, 目录)`/`.not_found(h)/.method_not_allowed(h)` 覆写挂点——2a 遗留默认文本形态升级)

**Interfaces:**
- Produces: `static_dir[S](dir: Str) -> fn(Req[S]) -> Resp`(收编 frm/static `st_serve`:ETag/Range/穿越拒)、`openapi_json[S](r: Router[S], title: Str, ver: Str) -> Resp`(自路由表生成最小 OpenAPI 3;路径参数 `:x`→`{x}`)

- [ ] **Step 1-3**: 实现收编面 + 测试(static:临时目录建文件→fs 读比对/穿越拒/ETag 命中 304;openapi:两路由快照断言含 path/method)。fs 在引导 interp 可用(03m 实证 read_or 可执行)。
- [ ] **Step 4**: 门 + 提交(pathspec: web/static.ct web/openapi.ct web/router.ct)。

---

### Task 3: serve 循环(net 探针定界)

**Files:**
- Modify: `web/serve.ct`

**Interfaces:**
- Produces: `serve(addr: Str, r: Router[S], state 构建…) -> I32`(阻塞;排空端点 `POST /__shutdown` 内置惯例)

- [ ] **Step 0: net 探针定界(先于实现)**

写 /tmp 探针(net_tcp_listen/accept 在引导 interp 是否可执行):可 → serve 循环 interp 可验(e2e 本任务内做);不可 → serve 实现后以 check+render 单测为门,e2e 挂 L6/L7 原生翻账(在册,如实记录)。

- [ ] **Step 1**: 循环形态(Todo_api 验收骨架 + §6 职责):listen→accept→http_parse_head→CL 读体(body_limit 默认 1MB)→构造 Req(hdrs/cookies/params)→匹配→预拼链→handler→render→net_write;panic 兜 500(panic 面 Ctron 捕获语义执行时核实,不可捕则进程级兜底登记);HEAD 去 body 保头;`/__shutdown` 排空。
- [ ] **Step 2**: 门:render 单测(Task 0 已绿)+ check OK + (探针通则)真 e2e nc 探针族健康检查。
- [ ] **Step 3**: 提交。

---

### Task 4: 迷你 todo 示例(spec §5 逐字落地)

**Files:**
- Create: `examples/web_todo/`(Ctron.ctcl + src/main.ct;§5 的 75 行形态 + 装配提成 full_router)

**Interfaces:**
- Produces: 框架首个活示例;`ctc.sh check` 全绿 + test_call 语义门(未登录 303/登录增删全链/flash 断言)

- [ ] 步骤:照 spec §5 代码落地(状态 Mutex[List[Todo]]+Atomic 序号)→ 语义门 → e2e(视 Task 3 探针定界)→ 提交。

---

### Task 5(收口): todo_app 迁移对照 + COVERAGE + 合并

- 迁移实验:todo_app 四模块迁到 web 形态(**分支内对照,不替换现有 todo_app**;行数账:app.ct 645→~140/sess.ct 252→0 预期);迁移版语义门全绿。
- **原生 e2e 门**:如实记录当前臂况(interp 全绿;原生门挂 L6/L7 销账翻账——在册)。
- COVERAGE web-2b 行 + README 两列表收口;合并回 main(worktree 流程;主根先查并行 WIP 重叠)。

---

## 后续(不在本计划)

- **L6/L7**(候选已入 spec §9,建议入 spec-gap W1 泳道):销账后原生 e2e 门翻账 + keep-alive/并发波(§13-④)解锁。
- **CTML 适配点**、OTLP trace 插件、csrf/limit 收编、multipart(挂 L4):既登记次波。
