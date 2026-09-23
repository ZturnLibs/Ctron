# 服务器泳道 P6 实施计划(ctron-http 应用框架)

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development(既定模式)。Steps 用 checkbox 跟踪。

**Goal:** 落地路线 P6——comptime 路由核(静态表+运行时 match,类型化路径参数,≤100ns 门)、公网中间件套件(CORS/CSRF/安全头/限流+过载 shed/每路由超时)、认证三件套(cookie HMAC/JWT HS256/PBKDF2 会话)、multipart、JSON body 绑定、静态文件(ETag/Range)、CTML HTML 输出形态、comptime OpenAPI 同源导出、优雅停机、健康端点、examples/todo_api。出口 = todo_api 端到端(JWT 拒未授权写/CORS 预检/限流 503)全绿 + OpenAPI 快照一致 + 路由 ≤100ns + 回环全请求周期 vs 手写 C epoll ≤1.05× + no_alloc 断言绿。

**Architecture:** 全部**纯 Ctron 组合层**(P5-E 裁定沿用:Sender/Receiver 字段化截断 ⇒ std 门面禁持通道半端,中间件链/handler 装配/后台任务一律组合层接线);comptime 路由 = **const 静态表 + 运行时 match 显式分发**(§8.4 无反射决策一致——comptime 构表,handler 引用运行时 match 派发,零反射零注册);body 绑定消费 std/json **强制 jv_*/jk_ 结构面**(32 位载荷纪律);multipart/auth/sse-ws 复用 P4 面。

**Tech Stack:** Ctron 纯层、std/crypto(HMAC/JWT/PBKDF2 既有)、std/json 结构面、vendor miniz(压缩已在 P4)、CTML(P6 窄面:HTML 输出形态)。

## Global Constraints

- pathspec/hunk 纪律;CI 回环;std/http/net 接口冻结(只消费不改;若必须改 net 面 → 升协调裁决)。
- **预登记 emit 缺口谱(P5-E + view 形参)**:①Sender/Receiver 字段化截断——禁持通道半端,组合层接线;②for 通配;③闭包内 match;④裸 with;⑤捕获槽写回;⑥lane .len=字面量槽数;⑦view 形参索引赋值禁;⑧spawn 闭包内 assert/panic 禁;⑨while 体 Drop 局部禁;⑩closure-return fn 禁 Drop 局部。中间件/装配/spawn 必踩 ③④——写码前对照。
- body 绑定 = std/json **jv_*/jk_* 结构面强制**(32 位载荷)。
- 32 位 Result 载荷、C10 宽域算术、fmt×emit scope 单行——全部沿 P4/P5 在册纪律。

---

### Task 1 (P6-A): comptime 路由核 + 中间件链骨架

**Files:**
- Create: `std/http/frm/` 子层(router.ct:comptime 构表——`comptime fn` 从路由字面量表构建静态模式数组(const ROUTES: T);运行时匹配器:方法+路径 → route id + 类型化路径参数提取(:id/:name 段,percent-decode 经 std/enc);middleware.ct:组合层链 `fn(req, ctx, next) -> resp` 形(按组合层裁定,fn 值组装);ctx 载体:请求 arena 句柄 + 路径参数 + 中间件数据槽)
- Create: `tests/http/frm_route/`(路由匹配语料:静态段/参数段/405/404/优先级/percent-decode;中间件链序断言;≤100ns 路由命中基准段)

**Steps:**
- [ ] comptime 构表 + 运行时 match 派发 + 参数提取 + 语料
- [ ] 中间件链组合层骨架(链序/短路/next 语义)
- [ ] 路由命中基准(回环本地,目标 ≤100ns;超了如实登记归因)
- [ ] 提交

### Task 2 (P6-B): 公网中间件套件

**Files:**
- Create: `std/http/frm/mw*.ct`(cors.ct:预检/白名单/凭据;csrf.ct:双提交令牌+SameSite 默认;sechdr.ct:HSTS/nosniff/CSP 基线;limit.ct:per-IP 令牌桶+max-in-flight 信号量+过载 503 shed——进程内 v1,分布式列 P8;timeout.ct:每路由超时经 net deadline)
- Create: `tests/http/frm_mw/`(每中间件夹具:CORS 预检矩阵/CSRF 双提交正反/安全头快照/限流触发 503+恢复/超时 504)

**Steps:**
- [ ] 五件中间件 + 语料矩阵
- [ ] 双臂 + 提交

### Task 3 (P6-C): 认证三件套 + multipart + JSON body 绑定

**Files:**
- Create: `std/http/frm/auth.ct`(cookie 解析/签发 HMAC-SHA256 签名;JWT HS256 验证(header.alg 钉 hs256/签名恒时比较——hmac + 常时比较助手;RS256 列志向)、own 盒会话存储(内存版,外置 Redis 注记))
- Create: `std/http/frm/body.ct`(JSON body 绑定:std/json jv_*/jk_ 结构面强制;multipart:字段+文件,内存上限内,大文件流式列 P8)
- Create: `tests/http/frm_auth/`(JWT 篡改负例/过期/alg 混淆拒;cookie 签名篡改拒;multipart 字段+文件+超限拒;body 绑定数值保真)

**Steps:**
- [ ] 认证三件 + multipart + body 绑定 + 语料
- [ ] 双臂 + 提交

### Task 4 (P6-D): 静态文件 + CSML/CTML 输出 + OpenAPI

**Files:**
- Create: `std/http/frm/static.ct`(ETag/Last-Modified/If-None-Match 304/Range 206;路径穿越拒;sendfile 列 P9)
- Create: `std/http/frm/openapi.ct`(comptime 路由表同源导出 OpenAPI 3 JSON:路径/方法/参数/响应码;快照夹具)
- Create: `std/http/frm/html.ct`(CTML HTML 输出窄面:插值默认 HTML 转义——安全默认)
- Create: `tests/http/frm_openapi/`、静态/转义语料

**Steps:**
- [x] 静态文件语义(条件请求/Range 206/416)+ 穿越负例
- [x] OpenAPI 同源导出 + 快照夹具(路由表改 → 快照必变)
- [x] CTML 转义面(插值默认转义/安全豁免显式)
- [x] 提交

### Task 5 (P6-E): 优雅停机 + 健康端点 + examples/todo_api

**Files:**
- Modify: 框架装配面(scope 树排空停机;健康端点模板)
- Create: `examples/todo_api/**`(REST CRUD + JWT 保护写路由 + 静态页 + OpenAPI + 健康;存储可插拔:内存默认;≤300 行业务)

**Steps:**
- [ ] 停机排空 + 健康端点
- [ ] todo_api(端到端:自客户端打自服务端——CRUD 全链/JWT 拒未授权写/CORS 预检/限流 503/OpenAPI 输出/静态页)
- [ ] 提交

### Task 6 (P6-F): 门禁 + 登记收口 + P6 终审

- [ ] 全请求周期回环基准 vs 手写 C epoll ≤1.05×(P4 遗留承诺,本波复核)+ no_alloc 断言(路由/解析热路径)
- [ ] COVERAGE P6 行、divergences 收口、计划执行记录 P6 出口判定、设计文档 §六 P6 as-built 回写
- [ ] pathspec 提交;P6 终审(全波包)

## Self-Review

- 设计覆盖:§六 P6 全项(路由/中间件五件/认证三件套/multipart/JSON 绑定/静态/OpenAPI/优雅停机/健康/todo_api)有任务;CTML 双端复用窄面落 Task 4。
- 风险前置:comptime 构表能力已实证(递归 comptime fn+const);组合层接线形态由 P5-E 裁定预登记;emit 缺口谱十条预登记入约束。
- 明确不做:RS256(志向)、大文件流式 multipart(P8)、分布式限流(P8)、sendfile(P9)、h2(P9)、ORM。
