# ctecho —— thread-per-connection echo server

P1 阻塞基线示例:每连接一线程(§7 结构式并发),回环协议 = 收到什么回什么;
`./run.sh` 构建(emit + cc 链 net 垫片)并自打冒烟(3 探针逐字比对)。
`CTECHO_PORT=8080 ./ctecho` 定端口,缺省 8080,仅绑 127.0.0.1。

同形不变式:并发形态只依赖 `scope { |s| s.spawn(...) }` 与 std.net 门面,
P2 协程运行时下本示例源码零改动(换运行时核,不换用户树)。
