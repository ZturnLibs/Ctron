# 服务器泳道 P4 实施计划(HTTP/1.1 协议层)

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development(既定模式)。Steps 用 checkbox 跟踪。

**Goal:** 落地路线 P4——std/http 协议半层:HTTP/1.1 编解码(请求/响应、keep-alive、chunked 双向、100-continue、头/体上限)、客户端(连接、重定向、简单连接复用)、SSE、WebSocket(upgrade+帧编解码)、响应压缩(gzip/deflate,vendor miniz)、HTTP 日期(RFC 1123)、query/form 解析。出口 = 解析基准 vs picohttpparser ≤2×、压缩协商矩阵、SSE/WS 夹具、RFC 9110 语料集、fuzz 结构化生成零崩溃(累计登记)、双矩阵回归。

**Architecture:** 协议半层**纯 Ctron**(std/http/ 目录,无 C 垫片)——解析器工作在 net 层 `&I64[]` 字节道之上(8× 宽度是已登记的 P2 约束,基准实测定档,若 >2× 归因登记不粉饰);唯一 C 依赖 = miniz(压缩,vendor 仿 mbedTLS 惯例);BIO 层直接用 net 门面(tcp_connect/accept/read/write),keep-alive 的 fd 生命周期由 P3 的 Drop 句柄承担。**P4 首件捎带 P3 终审遗留**:ctron_net_close 的 forget_fd 重排(结构性关闭 ABA 挂死窗)+ 11-net.md 耐久语义回写。

**Tech Stack:** Ctron(纯层)、miniz vendored(压缩)、picohttpparser 仅测试线(基准对照)、RFC 9110/9112 语料。

## Global Constraints

- pathspec/hunk 纪律;net 层热文件(ctron_net.c)改动仅限首件捎带。
- 纯 Ctron 层纪律:单文件别超载(std/http/ 按协议关注点拆文件);extern 无新增;发射面缺口清单(divergences f/g)继续为编码约束(闭包 return 禁 Drop 局部、while 体 Drop 局部、spawn 闭包内 assert/panic)。
- CI 回环;fuzz 不入 CI 主环(结构化生成器脚本 + 登记);语料文件入 tests/http/corpus/。
- 性能门禁本波:解析吞吐 vs picohttpparser ≤2×(全请求周期 ≤1.05× 承诺在 P6 出口复核)。

---

### Task 1 (P4-A): P3 遗留捎带 + std/http 骨架 + 请求/响应编解码

**Files:**
- Modify: `std/net/c_src/ctron_net.c`(ctron_net_close:forget_fd 先于 ct_close——结构性关闭 ABA 挂死窗;重跑 bench 三门禁 + c10k delta)
- Modify: `docs/spec/11-net.md`(耐久语义回写:AF_UNIX 四件/resolve 异步不可取消/TLS 门面语义指针三件)
- Create: `std/http/`(parse.ct:请求行/状态行/头部解析——字节道零拷贝切片语义、上限参数化;message.ct:请求/响应构造、chunked 编解码、100-continue;日期在 Task 2)
- Create: `tests/http/`(corpus/ + run.sh;夹具:合法/越限/分片到达)

**Steps:**
- [ ] forget_fd 重排 + bench 三门禁 + c10k delta 复验(应不劣化)
- [ ] 11-net.md 回写
- [ ] 解析器 + 编解码 + 语料夹具(RFC 9110 合规子集:方法/版本/头部大小写/obs-fold 拒绝/Transfer-Encoding 优先/content-length 冲突拒/上限四种)
- [ ] 双矩阵回归 + 提交

### Task 2 (P4-B): 压缩 + HTTP 日期 + query/form

**Files:**
- Create: `vendor/deflate/`(miniz 单文件惯例)+ build 冒烟
- Create: `std/http/enc.ct`(gzip/deflate 封装 miniz;Accept-Encoding 协商矩阵)、`std/time` 扩展或 `std/http/date.ct`(RFC 1123 format/parse)
- Create: `tests/http/enc_fixtures/`(压缩协商矩阵 + 往返 + RFC 1123 黄金向量)

**Steps:**
- [ ] miniz vendor(单文件 amalgamated 惯例)+ build.sh + 链接冒烟
- [ ] gzip/deflate 压缩解压 + 协商(gzip/deflate/identity,大小写/空格容忍)+ 恒等回退
- [ ] RFC 1123 黄金向量(互逆 + 已知值)
- [ ] 提交

### Task 3 (P4-C): 客户端 + SSE + WebSocket

**Files:**
- Create: `std/http/client.ct`(request 执行:连接、重定向跟随(上限+方法语义 301/302/307/308)、简单 keep-alive 复用、响应解析对接)、`std/http/sse.ct`(事件流写面)、`std/http/ws.ct`(upgrade 握手 + 帧编解码 RFC 6455:掩码/分片/关闭/ping-pong;permessage-deflate 列 P9)
- Create: `tests/http/client_fixtures/`、`tests/http/sse_ws/`(ws 对端自打 + 帧边界/掩码/分片语料;SSE 多事件流断言)

**Steps:**
- [ ] 客户端(自客户端打自服务端 e2e;重定向链夹具;keep-alive 复用计数)
- [ ] SSE 写面(事件流语义:_event:_data: 多行;客户端读侧简单解析)
- [ ] WS upgrade(SHA1+base64——base64 用 std/enc;SHA-1 **纯 Ctron 新增进 std/crypto.ct**,RFC 3174 向量)+ 帧编解码语料(RFC 6455 §5.7 示例 + 掩码/分片/关闭码)
- [ ] 提交

### Task 4 (P4-D): 基准 + fuzz + 登记收口 + P4 终审

- [ ] 解析基准:corpus 计时 vs picohttpparser(测试线 vendor 单文件)≤2×;全请求周期数字登记
- [ ] fuzz:结构化生成器(合法/越限/畸形混合)+ 长跑登记(不入 CI);零崩溃判据
- [ ] COVERAGE P4 行 + divergences 新发射面发现(随记)+ 计划执行记录 P4 出口判定
- [ ] pathspec 提交;P4 终审(全波包)

## Self-Review

- 设计覆盖:§六 P4 全项(编解码/keep-alive/chunked/100-continue/上限/客户端/SSE/WS/压缩/日期/query-form/会话分层注记)有任务;query/form 并入 Task 2(std/enc pct_* 复用,薄);multipart 属 P6 框架面。
- 风险前置:字节道 8× 宽度对解析基准的影响 = Task 4 实测定档;SHA-1 新增 = RFC 向量锚定;miniz vendor 沿用 mbedTLS 惯例。
- 明确不做:P6 框架面(路由/中间件)、permessage-deflate(P9)、代理 CONNECT 隧道(简单代理转发列 P6 视需求)、h2(P9)。
