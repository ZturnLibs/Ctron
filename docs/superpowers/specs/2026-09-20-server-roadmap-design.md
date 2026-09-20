# Ctron 服务器开发支持路线(设计)

> 日期:2026-09-20 · 状态:**设计稿(待评审)** · 泳道:server(新并行泳道,不占 GUI 泳道槽)
> 权威实现:自举 `compiler/`(ci.sh 口径);`compiler-rust/` 逐波对齐(双宿主纪律)。
> 关联:`docs/spec/09-profiles-ffi.md` §9.5 / `07-concurrency.md` §7.8 / `08-effects-comptime.md` §8.1(能力) /
> `docs/ffi-analysis.md`(v0.7)/ `tests/COVERAGE.md` §7 / `docs/superpowers/plans/2026-09-16-gui-mvp-ladder.md`(阶梯格式先例)。

---

## 一、目标与非目标

**目标**:让 Ctron 成为可写生产级网络服务器的语言——从 socket 到 HTTP 框架到可观测性的完整纵向栈,
同时兑现规范既有承诺(R1 服务器档、§9.5 无色异步层、§2.7 `net.listen` 能力、§8.1 能力对象),
并在异步模型、能力安全、内存模型、可测试性四个面做出**主流服务器语言没有的差异化**。

**非目标(YAGNI,本路线明确不做)**:

- HTTP/3、QUIC、gRPC(远期探索,不入排期);
- 纯 Ctron 自研 TLS(R9 完全独立的长线志向, TLS 用 vendored mbedTLS 起步);
- ORM/分布式协调/消息队列客户端(生态层,框架落地后由社区/后续路线裁决);
- web 档(Wasm)服务器运行时(§7.8 JSPI 映射只做语义兼容设计,不实现);
- 反向代理/服务网格等基础设施产品化。

**成功判据(总出口)**:examples/todo_api —— 一个 JSON REST 服务(增删改查 + 静态页 + 健康检查),
在 P7 出口达到:源码 ≤300 行业务、无一处手写 glue 网络代码、确定性测试全绿、
回环压测吞吐与手写 C epoll 基线差 ≤5%(own 档,R4 硬指标口径)、
CVE 面评估中"未授权依赖无法外联"成立(能力制验证)。

---

## 二、现状基线(2026-09-20)

| 层 | 规范承诺 | 实现现状 | 缺口 |
|---|---|---|---|
| FFI | §9.6 extern "c" + trusted | **成熟**:C-ABI 回调/`#[repr(c)]`/`str_from_c`/dlsym/变参/cimport(math.h 71 绑定)/`#[export]`,边界性能 1.00× C | extern **Str 返回值缺失**;>12 参不支持;裸 F32 标量坏 |
| 并发 | §7 结构化并发 | scope/spawn/join、Send、有界 Channel、Mutex with/with_mut ✅ | `Atomic`、`Global[T]`、**取消传播**、parallel.map ❌(COVERAGE §7: 7✅/8❌) |
| 内存 | §6.4 Drop RAII、§6.5 alloc 效果 | Drop 发射面有实施计划(p1a-drop-raii);`Arena` 值类型整体释放已规范 | Drop 发射面落地进度需核对;p1a panic 不展开(longjmp 口径) |
| std | §2 `std.net.{TcpListener,…}` 示例 | fs/time/json/csv/enc/str 等 21 模块;**无 net**;time 仅历法,**无单调钟/定时器** | net 全量;时钟/定时器全量 |
| 异步 | §9.5 io_uring/kqueue/IOCP 统一层,**API 无色** | 零实现,无排期 | 全量(本路线核心) |
| 能力 | §8.1 能力对象、E4010/E4020、§2.7 `[caps]` | fs 侧有 caps 负例锚(modules/caps_fs) | net 能力键未定义 |
| 双宿主 | — | 自举权威 + R 线对齐纪律成熟;W4 extern 直调桥(解释口径)刚通 | 已知分歧清单(gui 记忆)服务器面多数不踩,Str 返回除外 |

结论:FFI + 部分并发 + 应用层编解码(json/enc/str)足以支撑**阻塞式 MVP**;
异步运行时是唯一需要从零建设的层,也是创新密度最高的层。

---

## 三、核心决策与备选方案

### D1 异步执行模型(本路线最重要的决策)

| 方案 | 内容 | 优势 | 代价 | 判定 |
|---|---|---|---|---|
| A 纯阻塞线程 | thread-per-conn,无异步层 | 最快可用(天级) | C10K 后内存/调度崩溃;无差异化;§9.5 承诺落空 | 不采纳,但**作为 P1 交付** |
| B 有色异步 | Future/poll/async fn(Rust 式) | 零成本、精确 | 违背"API 无色"规范承诺;函数染色传染;**R7(AI 一次通过率)最差实践**;自举发射器做状态机变换成本极高 | 否决 |
| **C 无色异步(推荐)** | 运行时栈切换协程(N:M)+ 就绪通知 reactor;IO 调用即普通函数调用,遇阻塞运行时挂起协程 | 兑现 §9.5"API 无色"逐字承诺;用户代码零 async 噪声;复用 §7 结构化并发做任务树 | ucontext/asm 栈切换;FFI 回调不得重入挂起栈(纪律 + 调试断言) | **采纳** |

**关键降险设计:「同形异构」兼容契约**——std.net 只设计一次 API,P1 在**阻塞运行时**
(1:1 线程)上交付,P2 换 **N:M 协程运行时**,用户源码逐字节不变。P1 的 API 形状
从第一天就按异步就绪设计:无 thread-local 语义、一切经能力对象、取消经 scope 树、
超时经上下文参数。**同形兼容不变式测试**钉死这一承诺(见 §八)。

协程实现选型:ucontext(可移植,三平台通吃)v1;asm 快路径(各平台 swapcontext 热身)
P8 档。栈默认 64KB + 高水位标记,guard page 检测远期。

### D2 泳道与宿主策略

新并行泳道 `server`,GUI 泳道不受影响。自举编译器权威(CI 口径),R 线(compiler-rust)
**逐波对齐**——每波出口门禁含 R 线锚点翻转。S0 前置项与 FFI 泳道(Str 返回)、
并发缺口(COVERAGE §7)协同销账,不重复建设:发现槽位冲突按 [[ctron-peer-lane-reground]]
纪律先 git 对齐再动。

### D3 TLS:vendored mbedTLS + cimport 绑定

| 备选 | 判定 |
|---|---|
| OpenSSL | 体积/许可噪音/API 面过大;否决 |
| 系统 TLS(Secure Transport 等) | 平台 API 分歧大、macOS 已弃用;否决 |
| 纯 Ctron 自研 | 密码学自研是多年歧路,R9 长线志向不绑定本路线;否决 |
| **mbedTLS(Apache-2.0,vendor 子集编译)** | **采纳**:小、C、许可干净,复用 GUI 泳道 vendored raylib/FreeType 的全套构建惯例;CA 根证书走系统 bundle(各平台加载器垫片) |

纯自研 `ctls` 列入 P8 后远期志向,不承诺。

### D4 协议范围

HTTP/1.1(RFC 9110/9112)服务器 + 客户端先交付;WebSocket(RFC 6455)upgrade 随后;
HTTP/2(需 ALPN + HPACK)入 P8;HTTP/3 远期探索。

### D5 资源与内存模型(直接长在规范既有语义上)

- **socket = 值类型句柄 + `impl Drop`**(§6.4 硬规则逐字:类不得持有 socket,值句柄持有);
  作用域退出确定性关闭,p1a-drop-raii 落地是 S0 前置。
- **每任务 Arena**:§6.4 "Arena 值类型 Drop 整体释放" + r-roadmap D2"任务作用域 arena"
  在服务器档的具体化——请求级 arena 在响应发出后随 scope 退出整体释放,
  **请求热路径零 free、零 GC 停顿**;长命会话(websocket 连接)状态走 own 盒/会话池,分层登记。
- **门禁抓手**:§6.5 alloc 效果推断——路由/解析热路径函数标 `no_alloc` 断言,
  arena 分配走显式 Arena 参数不算隐式 alloc。

### D6 规范落点

新增 `docs/spec/11-net.md`(网络与服务器档:socket 类型/API/能力键/超时与取消语义/HTTP 档),
同步修订 §7(异步执行模型节)、§8(能力键表)、§9.5(落地注记)、§2(`[caps]` 键集)——
规范 v0.8 批次,S0 交付。

---

## 四、架构总览

```
用户代码        examples/todo_api(纯业务)
────────────────────────────────────────────
框架层  std/http(框架):comptime 路由 · 中间件 · CTML 模板 · 静态文件
协议层  std/http(协议):HTTP/1.1 编解码(零拷贝切片) · WebSocket
安全层  std/tls:mbedTLS 绑定(cimport) · 系统 CA 加载垫片
──────────────────────────────────────────────  以上纯 Ctron,双运行时透明
传输层  std/net:Tcp/Udp/Unix · SocketAddr · DNS(能力对象门面)
执行层  ctron_rt(c_src):reactor(kqueue/epoll/IOCP) · 协程(ucontext) ·
        定时器轮 · 确定性调度器 · 任务 arena
并发层  语言内建:scope/spawn/Channel/Mutex/Atomic + 取消传播
──────────────────────────────────────────────
FFI 层  extern "c" · cimport · dlsym(既有,v0.7)
```

关键结构性质:**执行层以上所有代码(Ctron 写的)不感知运行时形态**——P1 阻塞与 P2 协程
共享同一 std/net 门面;reactor 差异(kqueue/epoll/IOCP)封在 c_src 垫片后。

---

## 五、创新点清单(与主流服务器栈对比)

| # | 创新 | 机制 | 最近对标 | 差异点 |
|---|---|---|---|---|
| 1 | **无色异步逐字兑现** | IO 即普通函数;运行时栈切换挂起;语言无 async/await/Future 概念 | Go goroutine | Go 靠 runtime 魔法隐式调度;Ctron 把挂起点契约写进规范语义(§7 新节)+ 结构化 scope 显式管理,且全程无 GC 依赖 |
| 2 | **一棵 scope 树,四个免费系统** | 连接 = scope,请求 = 子任务:取消传播 / 任务 arena 整体释放 / tracing span 树 / 有界背压,全部由同一棵结构化并发树导出 | Java Loom/Erlang | 无一主流栈把观测树与内存域同时挂在结构化并发树上;tracing 零插桩 |
| 3 | **comptime 路由 + 编译期效果检查** | 路由表 comptime 构建为静态 trie;handler 的能力需求(net/clock/fs)编译期对照 `[caps]` 声明(E4010 前移到编译期) | Rust axum(宏)/Spring(反射) | 零反射零运行时注册;能力违约在编译期报错而非运行时 |
| 4 | **能力制网络(无环境权限)** | 网络访问仅经能力对象;依赖包想外联必须主程序显式授柄;E4020 纯函数碰网即编译错 | Pony(小众)/主流全环境权限 | 主流服务器语言首次:供应链依赖"装了就偷偷外联"在语言层不可表达 |
| 5 | **CTML 双端复用** | GUI 标记语言增加 HTML 输出形态;插值默认 HTML 转义;模板编译期查标签/属性 | JSX/ERB | 一门模板语言横跨桌面与 Web,类型检查一致;转义安全默认 |
| 6 | **确定性异步测试** | 调度器可播种(单线程确定性模式) + 虚拟时钟(Fake time 定时器即跳) + FakeNet;录制/回放交错 | Tokio `#[test]`/Go(无) | 并发 bug 可重现:同种子逐字节重放;沿用 r2b_fs_fake 注入先例 |
| 7 | **请求热路径可证明零分配** | §6.5 alloc 效果推断当门禁:热路径 `no_alloc` 断言 + 任务 arena 显式分配 | Rust(人工审计) | 语言效果系统自动核算,CI 拦截热路径隐式 GC 分配 |

创新 1/2/6 组合是本路线的护城河:**"结构化并发即服务器运行时"**——不是给语言加一个
web 框架,而是证明 §7 的并发语义天然就是服务器语义。

---

## 六、泳道阶梯 S0–P8

> 格式沿用 gui-mvp-ladder;每波出口 = 门禁全绿 + R 线锚点翻转 + COVERAGE.md 登记。
> 预估为单人泳道人日,机刷并行可压缩;波间依赖:S0→P1→P2→(P3→P4→P5)→P6→P7,P8 不排期。

### S0 前置销账与规范批次(1–2 天,协同为主)

- **销账**:extern Str 返回值(FFI 泳道协调,标准 `char*` 深拷即可);`Atomic[T]` 基础三件
  (load/store/CAS)+ `Global[T]` + 取消传播(`ScopeCancelled`,锚 06e_cancel 翻绿);
  Drop 发射面(p1a-drop-raii)落地确认——socket 关闭语义依赖它。
- **规范 v0.8 批次**:新 `docs/spec/11-net.md` 初稿(socket 门面/能力键/超时取消/HTTP 档);
  §7 增"异步执行模型"节(无色语义、挂起点契约、协程与 Send 交互);
  §2 `[caps]` 键集增 `net.listen` / `net.connect` / `net.resolve`。
- **锚点**:tests/roadmap/ `r7a_caps_net.neg.ct`(E4010 net 键)、`r7b_pure_net.neg.ct`(E4020)。
- 出口:销账项上游绿 + 规范批次合入 + 锚点按测试先行转红登记。

### P1 阻塞基线:「先有」(4–6 天)

- std/net 阻塞核:`SocketAddr`、`TcpListener(bind/accept)`、`TcpStream(connect/read/write/
  close)`、`UdpSocket(send_to/recv_from)`、单调钟 `now_ns`(clock_gettime/QueryPerformanceCounter
  内建)、`sleep_ms`;DNS getaddrinfo(阻塞,P1 口径)。
- c_src 垫片:posix.ct + winsock.ct 同门面(fd/USize 句柄差异内衬 I64)。
- 能力门面从第一天生效:listener/stream 构造仅经能力对象,`[caps]` E4010 全程在线。
- 示例:examples/ctecho(thread-per-conn 回显服务器,~60 行)。
- 测试:tests/net/(全回环):bind/connect 往返、graceful close、SO_REUSEADDR、
  读写超时、fd 泄漏计数(ulimit 前后对照)、错误面锚(E net_refused 等)。
- **出口门禁**:回显 1k 并发(loopback)无 fd 泄漏;own 档吞吐 vs 手写 C thread-per-conn ≤1.05×;
  CI 纪律:只碰回环、端口内核分配(:0)、外部网络零依赖。

### P2 异步内核:「再好」(8–12 天,创新核心波)

- ctron_rt(c_src):reactor 三后端(kqueue/epoll/IOCP 统一"就绪通知"接口,IOCP 完成口
  垫成 one-shot+rearm);ucontext 协程(64KB 栈 + 高水位);定时器轮(与 reactor 合一);
  N 线程 × 多队列调度器(v1 无 work-stealing,先确定性强)。
- 无色化:net 读写 EWOULDBLOCK → 挂起/唤醒;`sleep` 同色;取消:scope.cancel 唤醒
  挂起任务为 Cancelled 错误,fd 随 scope 出口清算(同形契约:cancel 语义 P1 阻塞口径 =
  join 等效)。
- **确定性调度器**:单线程 + 种子化交错(创新 6),测试模式默认。
- FFI 重入纪律:extern 回调内禁触网(挂起栈不可重入)——调试断言 + 规范条文。
- 兼容不变式:examples/ctecho **源码零改动**跑在协程运行时(同形异构契约的机械验证)。
- **出口门禁**:C10K(回环 10k 并发连接)通过;同形测试绿;确定性种子 1000 交错全绿;
  协程切换微基准(≤200ns/切换 v1 口径);echo p50 附加时延 vs P1 ≤1.15×(换伸缩性的显式定价,
  P8 优化收回)。

### P3 TLS + 传输补全(5–8 天)

- vendor mbedTLS(子集编译,构建惯例抄 raylib);cimport 驱动绑定垫片;系统 CA 加载
  (macOS/Windows/Linux 三垫片);TLS 客户端 + 服务端(含 ALPN 顺带)。
- Unix domain socket;DNS 异步化(getaddrinfo 入池线程 + 协程包装,语义不变)。
- **出口门禁**:与 openssl s_server/nginx 互操作矩阵;HTTPS 回显回环;握手吞吐 vs
  openssl s_server ≤1.5×(密码学主导,宽口径)。

### P4 HTTP 协议层(8–10 天)

- std/http 协议半层:HTTP/1.1 编解码,零拷贝(基于既有 byte_at/byte_slice 切片面),
  chunked、keep-alive、100-continue、header/body 上限(能力参数化);客户端连接池/重定向/
  代理;WebSocket upgrade + 帧编解码(RFC 6455)。
- 会话内存分层落地:请求 arena / 连接级 own 状态登记(§6.2 硬规则对照表)。
- **出口门禁**:解析吞吐 vs picohttpparser 基线 ≤2×(全请求周期 ≤1.05× 承诺在 P5 出口
  复核);fuzz(结构化生成器 + 与 llhttp 差分,仅测试线引用)累计 ≥24h 零崩溃;
  RFC 9110 合规语料集。
- 锚点:`r7c_http_caps.neg.ct` 等,r7 前缀顺延。

### P5 ctron-http 应用框架(8–10 天)

- comptime 路由(创新 3):路由 DSL → 静态 trie;类型化路径参数;handler 能力需求
  编译期对照 `[caps]`;中间件链(`fn(Req, next) -> Resp` 组合)。
- JSON body ↔ struct 绑定(std/json + comptime);静态文件;优雅停机(scope 树排空);
  健康端点;CTML HTML 输出形态(创新 5,插值默认转义)。
- 示例:**examples/todo_api**(REST + 静态页,examples/todo 同域双端叙事)。
- **出口门禁**:todo_api 端到端(std/http 自客户端打自服务端)全绿;路由命中 ≤100ns;
  回环压测全请求周期 vs 手写 C epoll 基线 ≤1.05×;热路径 no_alloc 断言绿(创新 7)。

### P6 可观测与运维(4–5 天)

- std/log(结构化 kv);metrics(Prometheus 文本格式);tracing:span 树 = scope 树
  (创新 2,零插桩导出 json 文本形态,OTLP 远期);优雅停机演练;std/config 对接。
- **出口门禁**:todo_api 输出合法 Prometheus 文本;tracing 层级与连接/请求树一致
  (确定性测试断言)。

### P7 生态与部署(5–7 天,部分长线)

- 包管理 dogfood:`ctron pkg` 拉取走 std/http(规范 §2 `ctron-http = "1.2"` + lockfile 落地,
  对接 toolchain-dist 泳道);部署面:静态产物 + scratch 容器配方 + 交叉编译(dist 泳道);
  LSP 路由诊断;website 服务器指南。
- **出口门禁**:对本地 registry 夹具 `pkg add` e2e;website 文档页上线;todo_api 在
  scratch 容器内跑通。

### P8 硬件利用档(不排期,志向登记)

io_uring 后端;SO_REUSEPORT 多 reactor;NUMA 亲和(§9.5);kTLS;SIMD 解析
(`Simd[E,N]`);work-stealing;HTTP/2(+HPACK);协程栈 asm 快路径;web 档 JSPI 映射
——**同一个无色 std.net API 在 web 档落到 fetch 后端**(服务器代码跑边缘,§7.8 钩子)。

---

## 七、规范修订清单(v0.8 批次,S0 交付;后续波次附注)

| 文件 | 修订 |
|---|---|
| `docs/spec/11-net.md`(新) | socket 门面类型/能力键/超时与取消/同形异构契约/HTTP 档分层 |
| `07-concurrency.md` | 新节:异步执行模型(无色语义、挂起点契约、协程 × Send、取消) |
| `08-effects-comptime.md` | 能力键表增 net.listen/net.connect/net.resolve;纯函数网络禁令 |
| `02-names-modules.md` | `[caps]` 键集与包级上限语义对齐 |
| `09-profiles-ffi.md` §9.5 | 异步层落地注记(v1 = ucontext + kqueue/epoll/IOCP) |
| `06-memory.md` | 服务器档内存分层注记(任务 arena / 会话 own)对照 §6.2 硬规则 |

---

## 八、测试与验收策略

- **回环纪律**:CI 全部回环流量、`:0` 内核分端口、并行泳道不互踩(沿多泳道撞车教训);
  外联测试(真实 DNS/TLS)仅本地靶标记,CI 跳过。
- **同形兼容不变式**:P1 示例源码逐字节不变跑 P2+ 运行时——每波出口必跑,违背即波次打回。
- **确定性优先**:默认种子调度 + 虚拟时钟;压测/soak(取消风暴、慢客户端、半写)
  作为独立门禁不入 CI 主环。
- **fuzz**:HTTP 解析器结构化生成 + 差分(llhttp 仅测试线);协议层每波 24h 累计。
- **锚点前缀** `r7*`(已核实空闲),tests/net/ + tests/http/ 行为夹具,
  COVERAGE.md 增"§网络与服务器"小节逐波登记。
- **三线一致口径**:seed 解释 == 自举发射产物 == native 发射产物(镜像 p1a-drop-raii 门禁纪律)。

---

## 九、性能门禁汇总(R4 口径)

| 路径 | 门禁 | 波次 |
|---|---|---|
| 回显吞吐(own 档,回环) | vs 手写 C 同构 ≤1.05×(硬) | P1 出 / P2 复核 ≤1.15× |
| 协程切换 | ≤200ns(v1) | P2 |
| HTTP 全请求周期 | vs 手写 C epoll ≤1.05×(硬) | P5 |
| 路由命中 | ≤100ns | P5 |
| 热路径分配 | no_alloc 断言零违例 | P5 |
| TLS 握手 | vs openssl s_server ≤1.5× | P3 |
| GC 档请求路径 | 不适用(热路径走 arena/own,GC 只承会话态) | 设计约束 |

---

## 十、风险与对策

| 风险 | 对策 |
|---|---|
| 协程 × FFI 回调重入挂起栈 | 纪律条文 + 调试断言;W4 直调桥仅解释口径,互不干扰 |
| IOCP 完成模型与就绪模型分歧 | reactor 接口从第一天按 one-shot+rearm 设计,三后端垫片对齐 |
| Drop 发射面延期阻塞 socket 关闭语义 | S0 硬前置;未落地前 P1 以 scope 出口清算兜底(显式 close 层) |
| 双宿主分歧拖累(Str 返回、USize 等) | S0 销账;逐波 R 线对齐门禁;分歧清单增服务器面小节 |
| CI 无外部网络/端口冲突 | 回环纪律 + :0 + 套件命名空间隔离 |
| 长命连接(websocket)内存分层 | 请求 arena / 连接 own 两档显式分层,规范注记 + 示例 |
| C10K 门禁在 CI 弱机不可复现 | CI 冒烟 1k;10k 门禁本地 + nightly 靶 |
| 并行泳道槽位/工作树冲突 | 新泳道登记;提交 pathspec 限定;续接先 git 对齐(机刷纪律) |
| ucontext 在新平台/新架构缺位 | 三目标平台(darwin/linux/windows × arm64/x64)矩阵冒烟入 dist 泳道 |

---

## 十一、与现有泳道/路标的关系

- **GUI 泳道**:零资源争抢(唯一交叠 = vendored 构建惯例,直接复用);CTML 双端复用(创新 5)
  是 GUI 投入的服务器面回报。
- **FFI 泳道**:S0 Str 返回值销账与 W4 桥缺口同一张清单,协同不重复。
- **并发缺口**(COVERAGE §7):S0 销掉 Atomic/Global/取消三项,§7 完成度 7/15 → 10/15,
  双向受益。
- **toolchain-dist / website 泳道**:P7 部署与文档对接,无前置冲突。
- **r-roadmap / spec-completion-roadmap**:服务器条目此前不在任何路标——本设计即补位,
  批准后登记入 COVERAGE.md 与路线图索引。

## 十二、未决问题(留评审裁决)

1. P2 调度器线程数默认 = CPU 核数还是 4 上限起步(推荐后者,弱机友好,env 可调)?
2. DNS P3 异步化是否直接上 c-ares vendored(推荐否,池线程够用,c-ares 列 P8)?
3. `r7` 锚点前缀与机刷泳道的占用裁决时点(S0 登记时再核一次)。
4. examples/todo_api 是否纳入 website 域名下实跑(demo 服务器常驻运维成本,推荐否,
   仅本地/CI)。
