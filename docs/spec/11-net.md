# §11 网络与服务器档

> 状态:**定稿(v0.8.1,2026-09-21)**——随服务器路线 S0 批次并入规范冻结面;后续修订按 v0.8.x 注记。v0.8.1 = P3/P4 终审遗留语义回写(加法式:AF_UNIX 落地面四件入 §11.2、resolve 池语义入 §11.5、TLS 门面语义指针新 §11.9),不改动 v0.8 冻结面。v0.8.2 = 局域网发现原语四件入 §11.2.2(加法式注记,亦不改动 v0.8 冻结面)。实现锚定:tests/modules/caps_net(E4010)、tests/net/(行为)。
> 执行模型语义归 §7;本章定网络门面、传输语义、HTTP 档分层。规范性约定(必须/禁止/应当/可以)同规范 README。

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
- 底层文件描述符/`SOCKET` 句柄**禁止**由程序直接触达(发射面拒绝导出;E 锚随发射面收口波次登记,登记面见 divergences 服务器面)。

### 11.2.1 AF_UNIX 落地面四件(v0.8.1 注记;实现锚 std/net.ct P3-E 面 + tests/net/unix_sock)

Unix 族句柄(`UnixListener`/`UnixStream`)在本档冻结语义之上,落地面钉死以下四件(跨平台一致口径,不得静默偏离):

1. **路径上限 = `min(sizeof sun_path)` = 104 字节**(darwin 104 / linux 108,含 NUL):`strlen(path) >= 104` → `EINVAL`(垫片统一置码,跨平台同形;不暴露各平台原生差异)。
2. **bind 前 unlink 陈旧 socket 文件 = 后绑者赢**:listener 建立时对既有路径先 `unlink`(ENOENT 为常态忽略)——残留文件自愈,不辨活/死(活 listener 被后绑者顶掉的竞态面归调用方编排,垫片不仲裁)。
3. **Drop 只关 fd,不摘 socket 文件**:句柄 RAII 作用域退出仅确定性关闭描述符;"最后一个 Drop 摘路径"会误摘继任者的活路径(后绑者赢语义下的必然后果),故文件生命周期显式归调用方。
4. **显式清理 = `net_unix_unlink(path)`**:`ENOENT` 亦返 `rc < 0`(与真失败同形)——幂等清理路径的"已不存在"判别由调用方自决,垫片不设特例。

### 11.2.2 局域网发现原语四件(v0.8.2 注记;实现锚 lib/net/net.ct + c_src/ctron_net.c + tests/net/lan_discovery)

局域网发现(定向广播/对端地址学习/限时连接/线序编解码)所需门面在此钉死(POSIX/winsock 双分支同形,rc 惯例与 §11.2 门面一致:0 成 / <0 败):

1. **广播开关 = `net_udp_set_broadcast(fd, on)`**:`SO_BROADCAST` setsockopt,`on != 0` 开 / `== 0` 关(双态可设);开后 UDP 方可向 `255.255.255.255` 定向广播。
2. **带回对端地址接收 = `net_udp_recvfrom_from` + `net_udp_last_peer`**:接收/超时口径与 `net_udp_recvfrom` 逐条相同(单报文以视图容量为上限、到时 `rc = -1` 槽 `ETIMEDOUT`);对端地址文本 `"host:port"`(v6 为 `"[host]:port"`,与 `addr_host`/`addr_port` 解析约定同形;v4 映射 v6 按 v4 点分输出)经 `net_udp_last_peer` 取——**C-owned 线程局部缓冲,Ctron 侧 `str_from_c` 深拷**;成功接收后应立即取(缓冲随下一次调用失效)。
3. **带超时连接 = `net_tcp_connect_t(host, port, timeout_ms, out_fd)`**:非阻塞 `connect` + `poll` 可写 + `getsockopt(SO_ERROR)` 判定,`timeout_ms <= 0` 同 poll 口径 = 永久;**rc 值域与 `net_tcp_accept_t` 同形:`0` = 已连接 / `-2` = 超时 / `-1` = 失败**(裸 `connect` 恒 `-1` 无超时面的缺口自此补齐);出口恢复 fd 阻塞属性(读路径依赖阻塞本体),`TCP_NODELAY`/`SO_KEEPALIVE` 默认面在 connect 出口生效(§11.3 同裸版)。
4. **字节序编解码 = `net_hton_u16/u32` + `net_ntoh_u16/u32`**:纯 Ctron(`bit.*` 移位+掩码),网络序 = 大端;`hton` 为字节倒序值变换(小端主机上线序字节方向正确),`ntoh` 同式自逆;入参取无符号值域(u32: `[0, 2^32)` / u16: `[0, 2^16)`),负值语义不担保。纯函数,`#[pure]` 许可。

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

### 11.5.1 resolve 异步化语义(v0.8.1 注记;实现锚 ctron_net.c DNS helper 池)

- **池线程语义**:协程口径下 resolve 经 2 线程 helper 池 + park/wake 完成(惰性建池,进程生命周期常驻)——解析不滞留调用协程所在 worker(workers=1 时旧形整 runtime 饿死,已实证差分);裸线程面(未链 rt)P1 内联原路径逐字节不变。
- **不可取消**:任务取消广播**不中断在途解析**——`getaddrinfo` 本身无取消面,协程被取消唤醒后以 `while !done` 再停车吸收虚假唤醒,直至解析返回;**最长等待 = getaddrinfo 本身**(登记口径:CI 面仅 localhost/数值 host,毫秒级;任意外联 host 的解析时长不在门面担保内)。
- **降级形态**:建池全败(线程创建失败)退化为调用面内联阻塞(旧语义,降级不悬挂);签名与返回值全程不变。

## 11.6 时钟与定时器(规范性)

- `now_ns() -> I64`:单调钟内建(clock_gettime / QueryPerformanceCounter 底座)。
- `sleep_ns`:无色、可取消(§11.4)。
- 测试形态:**虚拟时钟**——测试模式可跳变,定时器确定性触发(确定性异步测试的地基)。

## 11.7 HTTP 档分层(概览,详细契约随路线 P4/P6 定稿)

- **协议半层**:HTTP/1.1(RFC 9110/9112)编解码、chunked、keep-alive、100-continue、头/体上限(能力参数化)、压缩协商(gzip/deflate 起步,zstd 志向档)、SSE、WebSocket(RFC 6455)。版本路线:HTTP/2 = 志向档(P9)。
- **框架半层**:comptime 路由、中间件(认证/CORS/CSRF/限流/超时/安全头)、静态文件(条件请求/Range)、multipart、comptime OpenAPI 导出。
- 分层纪律:框架半层禁止绕过协议半层触网;协议半层禁止内嵌路由/业务概念。
- **落地面注记(v0.8.1,P4-A)**:协议半层首件落地 = `std/http/`(parse.ct 请求行/状态行/头部解析 + message.ct 报文构造/chunked 编解码/100-continue 钩子),**零 use 纯 Ctron 半层**(不 import std.net:加载器菱形 use 误报 E5020 规避 + 解释器可测,同 std/tls.ct 口径);走私加固姿态从严(RFC 9112 §5.2 obs-fold 拒、§6.1 TE+CL 并存拒、重复 CL 拒、裸 LF 拒);上限参数化五槽(行/头数/单头/头总/体,超限独立 err 码,400/414/431/413/505 映射归框架波次);IO 粘合(net 门面写道/读道超时)随框架半层落地。

## 11.8 与测试集的对应

`tests/net/`(回环纪律:`:0` 内核分端口、外部网络零依赖)、`tests/http/`;锚点 `tests/modules/caps_net`(E4010,多文件包负例)、`r7b_pure_net.neg.ct`(E4020)、`r7c_http_caps.neg.ct`;同形兼容不变式夹具(examples/ctecho 源码跨运行时零改动)。局域网发现四件的行为锚 = `tests/net/lan_discovery`(§11.2.2)。

## 11.9 TLS 门面语义指针(v0.8.1;P3-C 落地面,实现锚 std/tls.ct + ctron_tls.c)

门面三件语义在此定死(细节见 std/tls.ct 头注;与 §11.2/§11.3 冻结面同构延伸):

- **hostname 匹配默认 opt-out**:`tls_set_hostname` 不调即跳过**名字匹配**;证书链验证(REQUIRED)**保留**不随之关闭。传空串 = 回到 opt-out 态。显式校验 host 名是调用方义务(连接编排层工艺)。
- **超时为每块(per-record)口径**:`tls_read` 的 `timeout_ms` 经 mbedTLS `conf_read_timeout` + BIO `recv_timeout` 契约生效,**作用于单条记录的到达等待**,非"整记录链总预算"(一条多记录记录链在手时逐记录重置等待);超时返 `rc < 0`(槽 = `SSL_TIMEOUT`)。整读预算(deadline)归 §11.4 上下文参数,门面不设。
- **EOF 双形同映 0**:对端关闭的两形——BIO 层 fd FIN(mbedTLS fetch_input 收 0 转 `CONN_EOF`)与记录层 close_notify(`PEER_CLOSE_NOTIFY`)——**同映 `tls_read() == 0`**(eof 口径,与 §11.2 read EOF 单形对齐);调用方不区分,亦不得依赖区分。

## 承诺状态(卷 11)

> 承诺 ID = `PM-<卷>-<序>`;状态 ∈ 兑现 / 部分 / 志向 / 证伪。证伪与在册红账以 [`tests/COVERAGE.md`](../../tests/COVERAGE.md) 登记号互查;冻结面之外的承诺以本附表为准(见 README「冻结面」)。
| ID | 承诺 | 状态 | 退出条件 / 重评估触发器 |
|---|---|---|---|
| PM-11-1 | chunked 请求体解析/EOF 终结 | 部分 | G5-W3 v0 = CL 明长;挂账在册;触发 = http 客户端批次 |
| PM-11-2 | HTTP/2 | 志向 | P9 志向档;触发 = 服务器泳道 P9 立项 |
| PM-11-3 | zstd 压缩协商 | 志向 | §11 压缩档;触发 = 生态需求立项 |
