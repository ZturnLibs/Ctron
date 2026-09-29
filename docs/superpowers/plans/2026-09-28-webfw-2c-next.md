# web 域包 v1·Plan 2c:次波增量(gzip 压缩 + csrf/limit 收编)实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans. Steps use checkbox syntax.

**Goal:** web 次波第一波无门槛件——gzip/deflate 响应压缩中间件(次波第一件,http/enc 全套在库)+ csrf/limit 中间件收编。

**Architecture:** 全部为 frm/http-enc 薄收编(调用不复制);中间件形态承 2a/2b(`fn(Req[S], next) -> Resp` 工厂);挂在既有 render 咽喉上游(压缩对象=Resp.body 字节串)。

**Tech Stack:** Ctron(interp+check 口径)、http/enc(gzip/deflate/content_coding_negotiate)、http/frm(csrf/limit)。

**Spec:** §8.3 次波登记(次波第一件)/§4.5 中间件三档。

## Global Constraints

承 2a/2b 全部:引导链双臂门(check + 裸 run);字面量 `{` 拼接;no `;`;§14-1;E2060 显式 TypeArgs;E5030 测试命名前缀;pathspec 限定;worktree `.worktrees/webfw-2c`;**开波前置:无(2b 已合并,工作区干净即可)**。

---

### Task 1: gzip 响应压缩中间件

**Files:**
- Create: `web/compress.ct`

**Interfaces:**
- Produces: `compress_mw[S](threshold: I32) -> 中间件`——next 响应后:检查请求 `Accept-Encoding`(含 gzip → 协商收编 `content_coding_negotiate` 或按 contains 简判)→ body 字节 ≥threshold 且 Content-Type 文本族(html/json/text/csv/xml/yaml 前缀判)→ `gzip_compress` 压缩 → 替 body + 置 `Content-Encoding: gzip` + 修正 Content-Length + 追 `Vary: Accept-Encoding`;已带 Content-Encoding 的响应跳过;压缩后更大则回退原文(http/enc 家族头注有 gel 压缩率口径——如实保留较大者)。

- [ ] **Step 1**: 读 http/enc.ct 导出(gzip_compress 签名:&I64[] 进出?字节数返回)——Str↔&I64[] 转换按 todo_api 既有套路(fill/byte_at 循环或既有助手);**实现压缩/解压转换函数 gzip_bytes(s: Str, level) -> Str** 与 **wants_gzip(accept_enc: Str) -> Bool**。
- [ ] **Step 2**: 中间件 `compress_mw[S](threshold)`:next(req) → 判定 → 压缩 → 头处理。
- [ ] **Step 3**: 测试:大 HTML(>1KB 重复段)压缩后更小且 `Content-Encoding: gzip` + 解压回原(gzip_decompress 往返断言);小响应(<threshold)不压;无 Accept-Encoding 不压;`Vary` 存在。
- [ ] **Step 4**: 门(check + 裸 run + suite 87/88+88/88)→ 提交(pathspec: web/compress.ct)。

---

### Task 2: csrf/limit 中间件收编

**Files:**
- Create: `web/guard.ct`(csrf+limit 二件同文件,web/mw.ct 已有会话族面)

**Interfaces:**
- Produces: `csrf_mw[S](key: Str) -> 中间件`(GET/HEAD 直通;写方法:frm/csrf csrf_verify 双提交令牌——cookie 令牌 vs 表单/header 令牌,败 403;安全响应侧发 csrf cookie)、`rate_limit_mw[S](cap, refil, ivl, maxif) -> 中间件`(frm/limit 令牌桶 per-remote——req.remote 在 2a Req 无此字段,则以 hdr `X-Forwarded-For`/固定单桶 v1 口径,记录边界;超限 429)

- [ ] **Step 1**: 读 http/frm/csrf.ct(csrf_issue/csrf_verify/csrf_cookie_get/csrf_form_get/csrf_resp_403)与 http/frm/limit.ct(lim_cfg/lim_bkt 族)实际签名。
- [ ] **Step 2**: 实现 + 测试:csrf GET 直通/无令牌 POST 403/错令牌 403/正确令牌过;limit 连发超 cap → 429、窗口后恢复(时钟注入参数,§8-A5 同款)。
- [ ] **Step 3**: 门 + 提交(pathspec: web/guard.ct)。

---

### Task 3(收口): README/COVERAGE 收口 + 合并

- README API 表补 compress/guard 两行;COVERAGE web-2c 行;全包门;合并回 main(worktree 流程,主根查并行 WIP)。

## 不做(登记)

zstd/br(按需)、分布式限流(P8 生态)、csrf 的 SameSite=Strict 模式开关(默认 Lax 已随会话)。
