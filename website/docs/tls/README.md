# tls 域包 —— TLS 门面(T2;P3-C,BIO-over-hybrid)

mbedtls(vendored `vendor/tls/mbedtls`)之上的加工层:TLS 是 **net 句柄之上的
加工**,复用 net 能力,不新增能力键。**消费形态 = `use tls.{...}`**。

## 布局

```
tls/tls.ct   门面:握手/读写/close + ALPN/SNI + tls_last_error(参考页 tls.md)
tls/bind.ct  extern 窄桥(#[trusted];用户不接触,无参考页)
tls/c_src/   C 胶水(ctron_tls.c;BIO 桥接 net 垫片)
```

参考页:[tls.md](tls.md)(tls_client / tls_server / tls_handshake /
tls_set_alpn / tls_set_hostname / tls_read / tls_write / tls_close /
tls_alpn_selected_s / tls_last_error)。

## 口径与坑位(写码前必读)

1. **句柄 `TlsConn { var ctx: I64 }`**(ctx < 0 视为已关闭)。`tls_close` 后须
   `conn.ctx = -1`(同 net fd=-1 纪律)。
2. **fd 本体归 net 层所有**:TLS close 绝不关 fd;两句柄 Drop 先后无关
   (ctron_tls.c 文件头「Drop 序」注)。
3. **握手序**:`tls_client` / `tls_server` 建句柄(不握手)→ `tls_set_alpn` /
   `tls_set_hostname`(可选)→ `tls_handshake`。ALPN 必须握手前设;握手阻塞
   (协程上经 net 垫片停车),门面不设超时。
4. **读返回**:0 = eof(对端 close_notify),<0 = err(超时 = SSL_TIMEOUT);
   单次 `tls_read` 至多搬运 4096 字节(net CT_CHUNK 同值)。
5. **本模块不 import std.net**(加载器无 pop 的栈式查环会把 tls→net + 消费方
   use net 判成菱形 E5020,实证);`StdNet` 形参按名经 use 文本合并解析,
   消费方 `use net.{Net_probe}` 即供给。
6. 句柄出参 = **直接返回 ctx 指针**(<0 = 失败,符号即判据)——不用 net 的
   rc+Box[Box64] 通道(冻结 C 面口径)。

验收:域包消费测试 headless 绿;行为面随服务器泳道(P3-C)。
