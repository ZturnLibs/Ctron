# 服务器泳道 P8 实施计划(生态与部署)

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development(既定模式)。Steps 用 checkbox 跟踪。

**Goal:** 落地路线 P8——`ctron pkg` 拉取走 std/http(本地 registry 夹具 e2e + lockfile)、Connect 协议客户端(std/pb 地基,grpc 生态互操作桥)、S3 兼容对象存储客户端(SigV4 经 std/crypto)、NDJSON/流式 JSON 起编、部署面(todo_api scratch 容器配方+website 服务器指南)。出口 = 本地 registry `pkg add` e2e + Connect 客户端对夹具服务互调 + S3 夹具读写往返(minio 真靶 nightly)+ website 服务器指南页上线 + todo_api 容器配方可复现。

**Architecture:** pkg = **组合层 CLI**(独立 driver 面:CTCL manifest 解析复用 parse_pkg 语义 + std/http GET 拉 tar/目录 + lockfile 哈希钉);Connect = **HTTP POST + protobuf 帧**(unary;std/pb 编码,http.client 发送——发射债已清);S3 = **SigV4 纯算术签名链**(hmac_sha256×派生 + canonical 请求串)+ net/http 传输;NDJSON = std/json 逐行流式读面。

**Tech Stack:** std/http(P4 全波)、std/pb(P7-B)、std/crypto hmac_sha256/sha256(在册 RFC 向量)、std/json、parse_pkg.ct(CTCL 语义)、net 门面。**http.client 发射债已清**(40702d5 契约补全后 client_request 全链可发射可运行——P7-E e2e 实证)。

## Global Constraints

- pathspec 提交;门禁采数 CTRON_EMIT=worktree 隔离构建(bench 家族惯例)。
- P1b 契约:跨包消费直 use 全部符号(含 struct);中游模块**零依赖自持**优先(otlp 先例:两级传递依赖 emit 死旋在册)。
- 【Ctron 算术纪律】无位运算算符(& =借用/| 死旋/^ E1001)——hex/掩码/移位一律算术等价(%、*、/、+)+zseed 播种;字符串字面量裸 `{` 禁(`\{`),双反斜杠+裸 `{` 同毒。
- 双臂语料纪律;真靶(minio/registry 服务)nightly 标记,CI 全夹具回环。
- 32 位载荷纪律;no_alloc 热路径(签名链在请求装配期,冷路径 Str 构建合法)。

---

### Task 1 (P8-A): Connect 协议客户端

**Files:**
- Create: `http/frm/connect.ct`(零依赖自持或仅依赖 http.client;Connect unary:POST `<service>/<method>` + Connect-Protocol-Version 头 + protobuf 体 = std/pb 帧;响应解析:message/错误帧 connect-error JSON;超时透传)
- Create: `tests/connect/`(夹具服务=本地 Ctron server 模拟 Connect 端点回显 protobuf;客户端互调断言:请求帧字节/响应解码/错误路径;双臂)

**Steps:**
- [x] connect.ct 客户端面 + 夹具服务互调 e2e(4/4 含双轮 e2e;a9a5878)
- [x] 双臂 + 提交

### Task 2 (P8-B): S3 兼容客户端(SigV4)

**Files:**
- Create: `s3/s3.ct`(域包;SigV4:string-to-sign/canonical request/签名链 = hmac_sha256 派生;PUT/GET/DELETE/DELETE 批量对象面;endpoint/bucket/key 注入)
- Create: `tests/s3/`(夹具 S3 模拟服务:校验 Authorization 头四件/Cookie 无关面;PUT/GET 往返断言;真靶 minio nightly 段 SKIP 登记)
- Notes: 签名链全算术(hmac_sha256 输出 List[I32] → hex);时间 = std/time iso→basic 格式换算

**Steps:**
- [x] SigV4 签名链 + 语料(python 独立实现黄金向量钉值;5/5;d858343;scope 关键字撞名实证入册)
- [x] 夹具服务往返(mock 鉴权结构三针+单槽存储;PUT/GET/DELETE 全链 200/200/204;裸 socket 形——http.client StructLit 残缺陷绕行在册;minio nightly 段待真靶环境)
- [x] 提交

### Task 3 (P8-C): NDJSON/流式 JSON

**Files:**
- Modify/Create: `std/json.ct` 增逐行读面(或 `std/ndjson.ct`;大文件不整载,lane 游标逐行 → 既有 jget 面消费)
- Create: `tests/ndjson/`(逐行解析/坏行跳过策略/空行/大行;双臂)

**Steps:**
- [x] 逐行流式面 + 语料(6/6 双臂;std/ndjson.ct T1 零 use 游标;坏行策略=消费方 json.parse,Result 面修复后可深集成)
- [x] 提交

### Task 4 (P8-D): ctron pkg 拉取走 std/http

**Files:**
- Create: `tools/pkg.ct`(或 compiler 驱动新面——parse_pkg 语义复用:CTCL manifest 解析 + `pkg add <name>@<ver>` = registry GET manifest → 解析依赖闭包 → 逐包 GET tar/目录 → 本地 vendor 目录落盘 → lockfile(名@ver+内容哈希,zseed 十六进制链))
- Create: `tests/pkg/`(本地 registry 夹具 = 静态文件服务的 mini server;add 全链 e2e:manifest→依赖→落盘→lockfile 断言;断网安全性全夹具)
- Notes: registry 协议 v1 = 纯 GET 静态约定(`/<name>/<ver>/ctcl` + `/<name>/<ver>/src.tar`);认证/私有 registry 列志向

**Steps:**
- [x] pkg add 链(拍板:tools/ctpkg.ct 独立 CLI+registry v1 纯 GET 两端点;80f669a 构建面全绿;**运行时集成(vendor/lockfile 落盘)调试待新会话**——registry 服务 nc 实证正常,工具运行链末次验证因测试脚本 STDPATH 环境缺陷无定论)
- [x] 本地 registry e2e 段已入 runner(CTRON_PKG_E2E)——集成通过即翻绿

### Task 5 (P8-E): 部署面 + website 服务器指南

**Files:**
- Create: `docs/deploy.md`(todo_api scratch 容器配方:静态产物=emit+cc -static?交叉编译现状登记;Dockerfile 样例;健康检查/优雅停机接线说明)
- Modify: `website/`(服务器指南页:todo_api 全链示例+Prometheus/OTLP/traceparent 三件接线段)
- Create: `tests/deploy/`(配方可复现的本地验收段:产物清单+启动探活;容器内跑通归 nightly 真靶段)

**Steps:**
- [x] 部署文档+配方验收段(docs/deploy.md:产物/运行期表/scratch 配方/可观测三件/限制登记;本地验收=e2e 22/22 同源,容器内跑通归 nightly)
- [x] website 指南页 + 提交(website/server-guide.md)
- [x] 提交

### Task 6 (P8-F): 门禁收口 + P8 终审

**Files:**
- Modify: `tests/COVERAGE.md`(P8 行)、`docs/c-rust-divergences.md`(执行发现)、本计划(出口判定)、spec §六 P8 as-built

**Steps:**
- [x] 出口门禁逐项:pkg add e2e ⏸ defer(随 D) / Connect 互调 ✅ / S3 夹具往返 ✅(minio nightly 待环境) / website ✅ / todo_api 容器配方 ✅(容器内跑通 nightly)
- [x] pathspec 提交;P8 终审(2026-09-26 00:39 全波 98 断言 0 红;3.5/5 门 + D 构建面交付/运行时集成待新会话,台账 tests/COVERAGE.md P8 行)

## Self-Review

- 设计覆盖:§六 P8 主线五件(pkg/Connect/S3/NDJSON/部署)+ LSP 路由诊断与 schema 迁移显式列后续(LSP 泳道/生态后续,不入本波)。
- 风险前置:http.client 已可发射(40702d5 契约补全实证);SigV4 时间格式换算与 hex 链全算术纪律;pkg CLI 落点(driver vs tools)开工前按 driver_run 结构定。
- 明确不做:私有 registry 认证(志向)、gRPC 原生(h2=P9)、schema 迁移工具(生态后续)、大文件流式 multipart(P8 计划内仅登记,P9 重审)。
