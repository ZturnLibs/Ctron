# Ctron 服务器部署指南(P8-E;todo_api 全链示例)

## 产物形态

todo_api 等 std/http 应用 = **单文件原生二进制**:

```sh
compiler/bin/ctron-emit run examples/todo_api/src/main.ct > todo_api.c
cc -O1 -w -pthread -Inet/c_src -o todo_api todo_api.c net/c_src/ctron_net.c
# 协程运行时(可选):追加 net/c_src/ctron_rt.c 并以 CTRON_RT=coro 运行
```

- 静态链接:`cc -static`(glibc 静态化注意事项随发行版;scratch 容器 = 二进制
  + 无运行时依赖面)。net 垫片仅依赖 libc(socket 族);无 TLS 时零第三方库。
- 交叉编译:emit 产物 = 平台无关 C;宿主 cc 换交叉工具链即可(逐平台验证归
  dist 泳道)。

## 运行期

| 项 | 值 |
|---|---|
| 监听 | `net_tcp_listen("127.0.0.1", port)`(生产改 0.0.0.0 + 反代/ LB) |
| 优雅停机 | `POST /__shutdown` → 停收新连、排空、退出(退出码 0) |
| 健康检查 | `GET /health` → `{"ok":true,"served":N}`(LB 探针直用) |
| 指标 | `GET /metrics` → Prometheus 文本(http_total counter / todo_served gauge) |
| 配置 | env 形(TODO_API_PORT/KEY/RLIMIT);std/config 应用面提请评审(P7 as-built) |
| 运行时 | 默认串行 accept(P6-E 形);CTRON_RT=coro 协程环(P2 同形性能实证) |

## scratch 容器配方(示例)

```dockerfile
FROM ctron-builder AS build
COPY . /src
RUN cd /src && compiler/bin/ctron-emit run examples/todo_api/src/main.ct > app.c \
    && cc -O1 -w -pthread -Inet/c_src -static -o /out/todo_api app.c net/c_src/ctron_net.c

FROM scratch
COPY --from=build /out/todo_api /
EXPOSE 8091
ENTRYPOINT ["/todo_api"]
```

- 容器内验收(真靶):镜像启动 → /health 探活 → /metrics 抓取 → /__shutdown
  排空退出。CI 零容器依赖,容器轮归 nightly 真靶段(回环纪律)。
- 资源:串行形单线程;coro 形 64KB 栈/任务(P9 栈经济专案前置)。

## 可观测接线(P7 三件)

1. **Prometheus**:任何 std/http 服务挂 `/metrics` 路由(metrics.ct 写面 +
   调用方 lane 计数),文本格式抓取即用。
2. **OTLP**:http/frm/otlp.ct 编码 span → POST application/x-protobuf 至
   Collector(夹具接收端 e2e 已证);traceparent 头沿 http/frm/trace.ct
   提取/注入串联。
3. **NDJSON 日志**:std/log 行构造 + std/ndjson 游标消费(逐行,不整载)。

## 已知限制(登记)

- keep-alive 服务端列志向(P6-E 形为 Connection:close 一连接一请求)。
- TLS 终结建议 LB 层(mbedTLS vendored 面 = P3 已交付,端内 TLS 随需接线)。
- 大文件流式 multipart 列 P8 后续;sendfile 零拷贝列 P9。
