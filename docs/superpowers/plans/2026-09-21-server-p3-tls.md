# 服务器泳道 P3 实施计划(TLS + 传输补全 + 时延首件)

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development(既定模式)。Steps 用 checkbox 跟踪。

**Goal:** 落地路线 P3——时延首件(coro-vs-P1 ≤1.5 中间检查点)、vendored mbedTLS、std/tls 门面(TLS 客户端+服务端+ALPN,CA 系统包)、HTTPS 回环互操作矩阵(vs openssl s_server/s_client 双向)、Unix domain socket、DNS 异步化(池线程+协程包装)。出口 = 互操作矩阵绿 + HTTPS 回显绿 + 握手吞吐 vs s_server ≤1.5× + net 双矩阵 12→14/14 + 登记收口。

**Architecture:** 时延首件改两层:rt worker 唤醒改**事件量交付**(退避随交付复位)+ reactor **兴趣驻留**(水平触发常驻注册,垫片每读零探针零 EV_ADD/DELETE——recv 直试,EAGAIN 才 park);TLS 集成走 **BIO-over-hybrid-shim**:mbedTLS BIO 回调直接调 ctron_net_read_t/write_t,TLS 挂起继承垫片混合化(协程上自动 park,裸线程自动阻塞)——零新增停车点。CA = 系统 bundle 文件路径(macOS /etc/ssl/cert.pem、Linux /etc/ssl/certs/ca-certificates.crt;Windows 证书库列 P8 登记)。

**Tech Stack:** Ctron、ctron_rt.c/ctron_net.c(既有)、vendor/mbedtls(3.6.x,Apache-2.0,子集编译仿 raylib 惯例)、本地 openssl(LibreSSL)做互操作对端与证书夹具。

## Global Constraints

- pathspec/hunk 纪律;并行泳道工作树避开;编译器热文件先 git status。
- vendor 体积纪律:源码包进 vendor/mbedtls/(构建产物 .a/.o 不进 git,仿 raylib);构建脚本离线(下载只发生在 vendor 一次性入库时)。
- 默认路径逐字节纪律持续有效(P2 固定点口径:driver_emit.ct 本波预期零改动)。
- std/crypto(纯 Ctron)不动;mbedTLS 只服务 TLS 面。
- 门禁口径:coro-vs-P1 **≤1.5 中间检查点**(1.15 维持为目标不挪柱,P3 内先验 1.5 再评估);握手吞吐 vs s_server ≤1.5×;CI 回环纪律(interop 对端为本地 openssl 进程)。

---

### Task 1 (P3-A): 时延首件——事件量交付 + 兴趣驻留 + 探针消除

**Files:**
- Modify: `std/net/c_src/ctron_rt.c`(worker 空闲退避→事件量交付:worker 弹空队时按最近视期定时器/管道自旋有界等待,交付即复位退避;reactor 注册表改**常驻兴趣**:fd+方向注册一次,水平触发持续生效,fd 关闭即摘)
- Modify: `std/net/c_src/ctron_net.c`(net_read_t/net_write/udp_recvfrom 停车点改驻留式:删 poll(0) 探针与 EV_ADD/EV_DELETE 每读路径;recv 直试 → EAGAIN → rt_wait_fd(park) → 重试;wait_fd 语义不变,注册表内部幂等)
- Modify: `tests/net/bench/bench.sh`(门禁三复测;新增分量计时注记——探针计数如果可得)

**Interfaces:** rt 对外语义不变(wait_fd/join/park 签名零改);兴趣驻留内部化(注册表幂等,重复 wait_fd 同 fd+方向 = no-op)。**回归硬门**:rt_core/rt_reactor 冒烟、tests/net 默认+coro 双矩阵全绿(驻留改变 reactor 注销时机,fd 关闭竞争路径要重验)。

- [ ] 实现(事件量交付 + 驻留注册表 + 垫片探针消除)
- [ ] 三冒烟 + 双矩阵回归
- [ ] bench 三门禁复测:yield 门禁二不退化(≤200);门禁三 coro-vs-P1 实测(目标 ≤1.5 检查点;1.15 维持目标不挪柱,差距归因进报告)
- [ ] pathspec 提交 + 报告(p2-task-1-report.md,含分量对比)

### Task 2 (P3-B): mbedTLS vendoring + 子集构建

**Files:**
- Create: `vendor/tls/mbedtls-3.6.x/**`(源码,Apache-2.0 头保留)、`vendor/tls/build.sh`(子集编译:libmbedcrypto/libmbedx509/libmbedtls 静态库;产物 build/ 不进 git)

**Steps:**
- [ ] 下载 mbedtls 3.6.x 官方 tarball,解包入库(LICENSE 保留;可选裁剪:删 programs/ tests/ docs/ 只留 library/ include/ configs/——体积纪律)
- [ ] build.sh:cc 直编或其 cmake(Makefile 优先,最少依赖);产物 vendor/tls/build/lib/*.a + 头文件布局;**离线可重建**
- [ ] 链接冒烟:一个 C main 调 mbedtls_ssl_init/handshake 空转(不联网),验证 .a 可链
- [ ] pathspec 提交 + 报告(体积数字登记)

### Task 3 (P3-C): TLS 绑定垫片 + std/tls.ct 门面

**Files:**
- Create: `std/net/c_src/ctron_tls.c`(+ std/tls/ 目录形如 std/net/:bind.ct + 门面)
- Create: `std/tls/bind.ct`、`std/tls.ct`
- Create: `tests/net/tls_smoke/{c_src 符号链接(net+rt+tls+mbedTLS .a 链接行), src/main.ct, run.sh}`

**Interfaces(冻结):**

```c
/* ctron_tls.c —— mbedTLS 包装;BIO = ctron_net_read_t/write_t(继承混合化) */
int64_t ctron_tls_ctx_new(int is_server);                        /* 返回 ctx 句柄(I64);<0 错 */
int64_t ctron_tls_use_cert(int64_t ctx, const char* cert_pem, const char* key_pem);
int64_t ctron_tls_use_ca_bundle(int64_t ctx, const char* ca_path);  /* 0 = 系统 default 路径探测 */
int64_t ctron_tls_set_alpn(int64_t ctx, const char* protos);     /* "h2,http/1.1" 逗号串 */
int64_t ctron_tls_handshake(int64_t ctx, int64_t fd);            /* BIO 走 net_read_t/write_t;协程上自动 park;返回 0/<0(errno 槽同 net) */
int64_t ctron_tls_read(int64_t ctx, char* buf, int64_t cap, int64_t timeout_ms);
int64_t ctron_tls_write(int64_t ctx, const char* buf, int64_t n);
int64_t ctron_tls_alpn_selected(int64_t ctx, char* out, int64_t cap);
void    ctron_tls_close(int64_t ctx);
```

门面(std/tls.ct):`pub fn tls_client(net: StdNet, fd: I64, ca: Str) -> Result/句柄`(P1 rc 口径沿用)、`tls_server(net, fd, cert, key)`、`tls_read/tls_write_str/tls_close`、`tls_alpn`——句柄为值 struct{TlsCtx{ctx:I64}} + impl Drop(ctron_tls_close)。rc 错误面 + `tls_last_error()`(mbedtls_strerror 进 pmsg 槽同 net)。能力审计:`use std.tls.Tls` + `&Tls` 锚(键集需 Task 5 扩 caps `tls`?——**不加新键**:TLS 是 net 句柄之上的加工层,复用 net.connect 能力;锚点 fn 仍可写,TLS 门面函数首参 net: StdNet 贯穿)。

- [ ] ctron_tls.c(以上签名;BIO recv/send 回调转调 ctron_net_read_t/write_t——继承混合化与 cancel 语义;mbedtls 配置:MBEDTLS_CONFIG_FILE 精简或默认,登记体积)
- [ ] std/tls.ct 门面 + bind.ct(单行参数表;白名单形参)
- [ ] tls_smoke 夹具:自签证书(run.sh 内 openssl 生成,临时目录)→ 服务端协程 TLS accept+回显,客户端 TLS connect+写读——coro 模式跑通(混合化继承的机械证明)
- [ ] run.sh 双模式;net 双矩阵回归
- [ ] 提交 + 报告

### Task 4 (P3-D): HTTPS 回环互操作矩阵(vs openssl 双向)

**Files:**
- Create: `tests/net/tls_interop/{src/main.ct, run.sh}`(双向:我们的 client ↔ openssl s_server;openssl s_client ↔ 我们的 server;TLS1.2/1.3 两档;ALPN 协商断言)

**Steps:**
- [ ] 本地 openssl 进程对端(s_server -www -quiet / s_client -quiet);自签证书夹具
- [ ] 矩阵 2×2×2 全绿(方向×对端×版本);ALPN 选中值断言
- [ ] 握手吞吐基线:整握手(建连+handshake+关) vs `openssl s_server` 同协议对端 ≤1.5×(bench 段,本地/nightly)
- [ ] 提交 + 报告

### Task 5 (P3-E): Unix domain socket + DNS 异步化

**Files:**
- Modify: `std/net/c_src/ctron_net.c`(AF_UNIX listen/accept/connect 三件;路径 108 字节上限登记)
- Modify: `std/net.ct`(UnixListener/UnixStream 值句柄 + 门面函数;DNS resolve 改池线程:getaddrinfo 入 2 线程池 + done 队列,协程 park 等完成——resolve 不再阻塞调用协程所在 worker)
- Create: `tests/net/unix_sock/{...}`、resolve 异步在既有 tcp_echo/ctecho 语义下回归(resolve 调用点签名不变)

**验收:** unix_sock 夹具(_bind/accept/connect/读写)双模式绿;DNS 异步化后 resolve 在协程内不再卡 worker(c_smoke 式进度协程证明);net 双矩阵绿。

### Task 6 (P3-F): 门禁 + 登记收口 + P3 终审

- [ ] 全门禁汇总:interop 矩阵、HTTPS 回显、握手吞吐 ≤1.5×、bench 四数字(yield/门禁一/三/时延检查点)、双矩阵计数更新
- [ ] COVERAGE P3 行、divergences 服务器面 (g)、计划执行记录 P3 出口判定
- [ ] pathspec 提交;P3 终审(全波包)

## Self-Review

- 设计覆盖:§六 P3 全项(时延首件/mbedTLS/TLS 双向/Unix socket/DNS 异步)有任务;门禁三件(互操作/HTTPS 回显/握手 ≤1.5×)落 Task 4/6。
- 风险前置:mbedTLS 下载已探通;BIO-over-hybrid 是本波唯一架构承重点(Task 3 冒烟直证);CA 走文件路径简化已登记(Windows 证书库 P8)。
- 明确不做(本波外):nginx 对端(openssl 双向已构成矩阵)、Windows 证书库(P8)、h2 ALPN 实装(ALPN 协商本波,HTTP/2 语义 P9)、纯 Ctron ctls(志向)。
