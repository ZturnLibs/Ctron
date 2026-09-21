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

---

## 执行记录(P4 出口判定,2026-09-21)

### 执行轨迹(子代理驱动;评审 Approved)

| 任务 | 提交 | 要点 |
|---|---|---|
| P4-A(Task 1) | `78adbe0`+`47c5985` | forget_fd 重排(ABA 收口)+ bench 三门禁/c10k 复验 + std/http {parse,message} + corpus 14 夹具 + 11-net v0.8.1 回写 |
| P4-B(Task 2) | `a655488`+`9d405b4`+`82328e5` | miniz 3.1.2 vendored(provenance byte-perfect)+ gzip 纯 Ctron 组框 + 协商(`*;q=0` 排除 identity 评审必修)+ RFC 1123 + form;移交编译泳道四件 |
| P4-C(Task 3) | `90240b4`+`2d7e47d` | client/sse/ws + std/crypto SHA-1 + 双 RT e2e;评审必修 = WS 三 MUST(分片序列/UTF-8 1007/version 13)+ len64 在库证据 + Minor 三件;修复波断言逮两真 bug(零长帧悬挂/len7 误算) |
| P4-D(Task 4) | `136aeb2` + fuzz/docs 波 | 解析基准 + fuzz 结构化长跑 + 差分对拍 + 登记收口(本节) |

### 出口判定门

| 门 | 门限 | 实测 | 判定 |
|---|---|---|---|
| 解析吞吐 vs picohttpparser | ≤ 2× | ctron 209 ns/req vs pico 46 ns/req = **4.54×**(复跑 4.44×;N=1e6 ×3 取最小,字节 digest 双侧 pin) | **RED → 诚实登记**:解析器工作在 `&I64[]` 字节道,每字节一条 I64 lane;4.5× < 8× 悲观线性外推,仍在 lane 税量级(~4.8M parse/s ≈ 754 MB/s 语料吞吐);处置 = P9 I8-typedef(字节道换窄 lane 后重对拍本基准);不改数、不换语料、不粉饰(§Global Constraints 预案执行)。全请求周期 ≤1.05× 承诺按计划归 P6 出口复核 |
| fuzz 零崩溃/零挂死 | 双臂 × seeds 零非零退出/零 watchdog 超时 | 本地 24 seeds × {interp 1500 + emit 50k + gz 50k + diff 256} = **120 段全绿**(≈11 min);结构化十类;差分 6144 样本:`we_accept_pico_rejects = 0`、`pico_accepts_we_reject = 1265`(严格子集预期差,登记) | **PASS**(nightly ≥30min 惯例入 run.sh 头注;不入 CI 主环) |
| 双矩阵回归 | tests/http 全绿 | **57/57 双臂**(x_ e2e × {默认, CTRON_RT=coro}) | **PASS** |
| 走私面/上限/RFC 9110 合规子集 | P4-A 全拒姿态保持 | 57/57 内含 obs-fold/TE+CL/重复 CL/裸 LF·裸 CR/值内 CTL 全拒 + 上限四类独立 err 码回归 | **PASS**(2026-09-21 终审收尾复验:chunk 边 `FFFFFFFFFFFFFFFE` 溢出/负值面新钉 `r_chunkneg.ct` 后全绿 **59/59**,见「终审收尾波」) |

### fuzz 附带实证与修复

fuzz 结构化生成(seed 2,chunk 10-hex 用例)实证**解释器按值域定宽乘法**
在 std/http 四处同族触发(chunk size `acc*16` / CL `acc*10` / WS len64
`acc*256` / WS 掩码键 `×2^24`;值跨 [2^28,2^31) 带即 panic,emit 侧 int64
恒正确)—— 即 P4-B 移交编译泳道的在册缺陷新实例;修 = C10 宽域惯用法
`(x + 2^32 - 2^32) * k` 四处结构性规避(纯恒等,time.ct 先例),编译器侧
正解挂账 divergences (i)/P9。此为 fuzz 门「零崩溃」判据的首个实战捕获。

### 捎带收口(P4-C 评审遗留 Minor)

- sse `id:` NUL 整字段忽略 → 夹具常设钉(a_ssefmt.ct 新测试)✅ 本波
- 单帧文本消费方 UTF-8 校验示范(1007 同规)→ x_ws_e2e.ct 新断言 ✅ 本波
- 装饰性余项(typo `shar256`、`cx_slice_str` 拒 `{`、重试不分方法 PEDANTIC
  档、301·308 与分片中控制帧覆盖补齐)→ 留档 P9/终审裁量(非协议 MUST 面)

### 登记账

- divergences:新增 **(i) P4 执行发现续**(值域定宽两实例族收口 + use 路径
  `-` segv + (h) 家族新证 + fmt×emit scope 拆臂挂死 + 加载器严格树
  E5020/E5030 加码 + std/enc b64 C8 约束依赖)。
- COVERAGE:P4-A/B/C/D 四行 as-built 门数字。
- 溢出边条目拆分(2026-09-21 终审收尾):(i) 乘法定宽/溢出边的 chunk size
  面已修销账(乘前 `2^59-1` 界门 + CR 负值防御档,`r_chunkneg.ct` 钉,
  divergences (i) 有条目);**CL 十进制 19 位 I64_MAX 邻域真溢出开口仍留
  P9**(hs_parse_dec 界门 `acc > 922337203685477580` 放行等值情形,尾位
  +digit 可越 I64_MAX —— 与 chunk 边同族不同面,随编译器侧乘法/加法定宽
  正解同波处置)。
- 工具链在途注记:本波收口时点 `ctron-fmt --check` 对 canonical 既有文件
  报 read-failed(并行编译泳道在途树状态,P4-C 报告 §5.4 同款漂移注记);
  fuzz/bench 产物以 interp/emit 双臂实跑为准。

### 明确不做(按计划)

P6 框架面(路由/中间件)、permessage-deflate(P9)、代理 CONNECT(P6 视
需求)、h2(P9)、zlib 容器与 FHCRC(登记未实现)、非 ASCII SSE/WS 载荷
(随 C8/Str 构建面放宽同波)。

### 终审收尾波(2026-09-21)

终审必修两件 + 登记三处,单提交收口(`fix(http): P4 终审必修`):

- **chunk 边修复**(std/http/message.ct):恰 16 位 hex `FFFFFFFFFFFFFFFE`
  (15×F+'E';`digits>=16` 门接受前检查,第 16 位放行)chunk size 实证
  **远程 DoS**——当前算术语义(emit `__builtin_mul_overflow` / interp
  大数域界检)下双臂在 `acc*16` 处确定性 panic rc=1,单请求杀进程;回绕
  语义下同一触发串使 acc 静默成负,CR 处 `acc > 1e18` 对负值失明 →
  remain<0 → 服务端形负下标 / 客户端形在地分框腐化(双失效形态,登记入
  divergences)。修 = 乘前 I64 界门 `acc > 576460752303423487`(2^59-1,
  parse.ct hs_parse_dec 界内累加同款)即拒 err 槽 2(≤1e18 接受面零回归)
  + CR 处改 `acc < 0 || acc > 1e18`(负值防御冗余档);语料钉
  `tests/http/corpus/r_chunkneg.ct`(触发串在案:修前双臂红 rc=1,修后
  双臂绿)。
- **client 端口第五处惯用法**(std/http/client.ct:Location 端口 `acc*10`,
  (i) 族第五处):C10 宽域惯用法改写(端口 ≥10 位数字 interp 误判 panic /
  emit 垃圾端口 fail-closed 的双口径漂移收口),emit 行为恒等;重定向
  夹具 x_locparse / x_client_e2e(双 RT)复验绿。
- 登记三处:divergences (i)「四处同族」→「五处」+ chunk 边独立条目;
  bench/README.md 补两侧每请求语义功不等口径一句;出口表走私面 PASS 行
  chunk 边复验注 + 登记账溢出边拆分(CL 19 位 I64_MAX 邻域开口留 P9
  —— `9223372036854775808` 实证 add 溢出 panic,chunk 边已修销账)。
- 回归:tests/http/run.sh 57/57 → **59/59**(r_chunkneg 双臂各计一例,
  `http/run: pass=59 fail=0`)。
