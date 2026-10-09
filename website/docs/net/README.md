# net 域包 —— 服务器档传输门面(T2;§11 冻结 v0.8.1)

TCP / UDP / AF_UNIX / 域名解析 / 时钟与睡眠的单域集合。**消费形态 =
顶层命名空间 `use net.{...}`**;分层与门禁见 `lib/std/README.md` 域包章,
规范面 = [语言规范 §11 网络](../spec/11-net.md),服务端用法全形见
[Server 指南](../server-guide.md)。

## 布局

```
net/net.ct   门面:句柄 struct + 传输免费函数 + SocketAddr 纯函数(参考页 net.md)
net/bind.ct  extern 窄桥(#[trusted];用户不接触,无参考页)
net/c_src/   C 胶水单一真源(ctron_net.c 垫片;native 链接面)
```

## 快速上手(TCP echo 骨架)

```ct
use net.{Net_probe, net_tcp_listen, net_tcp_sockname, net_tcp_accept,
         net_read_t, net_write_str, last_net_error}

fn main() -> I32 {
    let net = Net_probe()
    let out_fd = Box[Box64](Box64 { v: 0 })
    if net_tcp_listen(net, "127.0.0.1", 0, out_fd) != 0 { return 1 }
    var port: I64 = 0
    net_tcp_sockname(net, out_fd.v, out_port_cell)     // :0 分配结果回读
    // accept → TcpStream { fd: out_fd.v };read_t 返回 n>0 / 0=eof / <0=err
    // 全形(全量写循环/park/deadline)以 tests/net/tcp_echo 为可执行真源
    return 0
}
```

## API 地图

| 组 | 函数 | 参考页 |
|---|---|---|
| 监听/接受 | net_tcp_listen / net_tcp_sockname / net_tcp_accept | [net.md](net.md) |
| 连接 | net_tcp_connect | 同上 |
| 读写 | net_read_t / net_write / net_write_str / net_close / net_shutdown_write | 同上 |
| 选项 | net_set_nodelay / net_get_nodelay | 同上 |
| UDP | net_udp_socket / net_udp_bind / net_udp_sendto / net_udp_recvfrom | 同上 |
| AF_UNIX | net_unix_listen / net_unix_accept / net_unix_connect / net_unix_unlink | 同上 |
| 解析 | net_resolve / net_resolve_all | 同上 |
| 时钟 | Net_probe / last_net_error / addr_* 纯函数 | 同上 |

## 口径与坑位(写码前必读)

1. **错误面 = rc 路径**:0 成 / <0 败,详情经 `last_net_error()`(errno 槽)。
   不用 `Result[struct, _]`(Ok 绑定成员访问发射缺口在案,门面统一 rc)。
2. **句柄 = 值 struct + `var fd: I64` + `impl Drop`**(fd ≥ 0 才关)。显式
   `net_close` 后**必须** `st.fd = -1`,否则作用域出口 Drop 二次关;夹具推荐
   Drop-only(不显式 close)。
3. **传输函数首参 `net: StdNet` 按值** —— 调用点能力显形,勿改引用形。
4. **缓冲 = `var buf: U8[]` 字节平面视图**(CW1a 终形)。TCP 读/写无单次上限;
   UDP 单报文以视图容量为上限(EMSGSIZE 守卫列 P2)。
5. **出参通道 = `Box[Box64]`**(E4046 收口形):调用点实参写变量名,发射面自动取址。
6. AF_UNIX:SO_REUSEADDR 不开;路径上限 104(超限 EINVAL);listener Drop 只关
   fd 不摘 socket 文件,显式清理走 `net_unix_unlink`。
7. 时钟:`net_sleep_ns` rc 0=睡满 / 1=取消广播早醒(协程面);`net_clock_jump`
   仅 `CTRON_CLOCK=virtual` 态生效。

验收:域包消费测试 headless 绿为准入(`tests/net/` 全族;tcp_echo 64KB 回环
为传输面金标)。
