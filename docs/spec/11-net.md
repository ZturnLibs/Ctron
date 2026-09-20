# §11 网络与服务器档(草案 v0.8-d1)

> 状态:**草案**——随服务器路线设计(`docs/superpowers/specs/2026-09-20-server-roadmap-design.md` v3,2026-09-20)落库,路线 S0 批次定稿后并入规范冻结面。测试锚点先行(`r7` 前缀,`tests/roadmap/`);实现以自举编译器为权威,R 线逐波对齐。
> 执行模型语义归 §7;本章定**网络门面、传输语义、HTTP 档分层**。规范性约定(必须/禁止/应当/可以)同规范 README。

## 11.1 能力键(规范性)

- 网络访问一律经能力对象;包级 `[caps]`(§2.7)声明上限,程序实际使用集 ⊆ 声明集,超出 = E4010:

| 键 | 授予面 | 无键后果 |
|---|---|---|
| `net.listen` | `TcpListener`/`UnixListener` 的 bind + accept | E4010 |
| `net.connect` | `TcpStream`/`UnixStream` connect、`UdpSocket` | E4010 |
| `net.resolve` | DNS 解析(§11.5) | E4010 |

- `#[pure]` 函数(§8)触达任何网络门面 = E4020(编译期拒绝)。
- 能力对象不可构造、不可复制出授予链;`listener.accept()` 派生的连接句柄**继承监听者授权**,不重复消耗 `net.connect`。
- 数据库访问的 `db.connect` 键见 §12.1。

## 11.2 地址与 socket 门面(规范性)

```c
let ln = caps.net.listen(TcpListener.bind("127.0.0.1:0")?)   // :0 = 内核分端口
let conn = ln.accept()?                                       // -> TcpStream(继承授权)
let n = conn.read(var buf)?                                   // buf: Byte[] / T[N]
conn.write(bytes)?
conn.shutdown(Write)?
let addrs = caps.net.resolve(Dns.name("example.com")?)        // List[SocketAddr],双栈
```

- `SocketAddr`:IPv4/IPv6 **双栈**(v6 不歧视);字面与解析两形态。
- 值类型句柄:`TcpListener`、`TcpStream`、`UdpSocket`、`UnixListener`、`UnixStream`(Unix 族仅 posix 目标可用,windows 目标引用即 E 编译错)。**全部 `impl Drop`**(§6.4):作用域退出确定性关闭;类不得持有(§6.2 硬规则逐字适用)。
- 语义形态:**门面 API 恒为"阻塞语义"**——read/write/connect 无回调、无 Future;实际并发由运行时承载(§11.4)。
- 底层文件描述符/`SOCKET` 句柄**禁止**由程序直接触达(发射面拒绝导出;E 锚随 S0 登记)。

## 11.3 传输语义默认值(规范性)

服务器工艺默认值在此定死,实现不得静默偏离:

- **`TCP_NODELAY` 默认开启**(禁 Nagle;低延迟为默认;批量吞吐场景可显式关闭)。
- **`SO_KEEPALIVE` 默认开启**,idle/interval/probes 用平台默认,提供显式调参。
- listener 默认 `SO_REUSEADDR`;`SO_REUSEPORT` 不在门面(P9 多租户口志向)。
- 门面 `write` 直通内核,不做用户态大缓冲;聚合/`writev` 供协议层显式使用。
- 半关闭 `shutdown(Write)` 合法;对端读到 EOF 的平台差异(winsock)由垫片垫平。
- 读写超时**应当**经上下文 deadline 参数表达(§11.4);per-call setter 允许但同形测试必须双运行时通过。

## 11.4 执行模型与同形异构契约(规范性)

- 门面之下由两种运行时承载同一语义:P1 **阻塞运行时**(1:1 线程)与 P2 **协程运行时**(N:M,§7.1 有栈协程)。**同形异构契约:同一程序源码零改动,两运行时下可观察语义一致**——以机械测试不变式钉死(每波出口必跑)。
- **挂起点契约**(协程口径):网络门面调用、`sleep`、通道操作是仅有的可挂起点;编译器与运行时识别,用户代码无感知、无标注。
- 栈口径注记:§7.1 承诺"可增长连续栈";过渡实现为 64KB 固定 mmap 栈(子集),**触顶 = 任务边界 panic**(与 §7.1 上限口径一致);完全符合在 P9 栈经济专案达成。C100K/C10M 口径以专案为准,过渡口径不得外推。
- **取消**:沿 §7.2 任务级取消令牌传播;网络操作在挂起点响应取消,返回 `NetErr::Cancelled`;阻塞运行时口径 = join 等效(取消即等待完成)。向已取消作用域的门面操作返回 `Err(ScopeCancelled)` 语义对齐 §7.2。
- **FFI 纪律(必须)**:`extern "c"` 回调内不得触达网络门面/通道/睡眠(协程栈不可重入);违者 = 调试断言 + 未定义行为声明。
- 超时预算:deadline 经上下文参数进入门面;跨任务继承随作用域树(§7.2)。

## 11.5 DNS 与解析(规范性)

- `resolve(host) -> List[SocketAddr]`:多记录返回,不歧视 v6;受 `net.resolve` 键约束。
- 实现口径:池线程 `getaddrinfo` + 协程包装(语义无色);c-ares 列志向档。
- 连接编排:调用方按序尝试,连接超时独立于读超时;happy-eyeballs 列志向档。

## 11.6 时钟与定时器(规范性)

- `now_ns() -> I64`:单调钟内建(clock_gettime / QueryPerformanceCounter 底座)。
- `sleep_ns`:无色、可取消(§11.4)。
- 测试形态:**虚拟时钟**——测试模式可跳变,定时器确定性触发(确定性异步测试的地基)。

## 11.7 HTTP 档分层(概览,详细契约随路线 P4/P6 定稿)

- **协议半层**:HTTP/1.1(RFC 9110/9112)编解码、chunked、keep-alive、100-continue、头/体上限(能力参数化)、压缩协商(gzip/deflate 起步,zstd 志向档)、SSE、WebSocket(RFC 6455)。版本路线:HTTP/2 = 志向档(P9)。
- **框架半层**:comptime 路由、中间件(认证/CORS/CSRF/限流/超时/安全头)、静态文件(条件请求/Range)、multipart、comptime OpenAPI 导出。
- 分层纪律:框架半层禁止绕过协议半层触网;协议半层禁止内嵌路由/业务概念。

## 11.8 与测试集的对应

`tests/net/`(回环纪律:`:0` 内核分端口、外部网络零依赖)、`tests/http/`;锚点 `r7a_caps_net.neg.ct`(E4010)、`r7b_pure_net.neg.ct`(E4020)、`r7c_http_caps.neg.ct`;同形兼容不变式夹具(examples/ctecho 源码跨运行时零改动)。
