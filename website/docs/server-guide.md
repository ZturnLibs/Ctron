# Ctron 服务器指南(todo_api 全链)

> 单二进制 HTTP 服务:REST CRUD + JWT 写保护 + 限流 + Prometheus 指标 +
> OTLP 导出 + traceparent 串联 + 优雅停机。业务 ≤300 行。

## 快速开始

```sh
sh examples/todo_api/run.sh        # 22 探针自验(回环)
```

服务路由:GET /health · GET|POST /todos · DELETE /todos/:id · POST /login ·
GET /openapi.json(同源导出)· GET /(静态页,ETag/304)· GET /metrics ·
POST /__shutdown(优雅停机)。

## 三件可观测

- `/metrics` = Prometheus 文本(counter+gauge;热路径零分配)。
- OTLP:otlp.ct 编码 span → POST 至任意 Collector;std/pb 线格式。
- traceparent:trace.ct 严格 W3C 解析/注入(00-32hex-16hex-2hex)。

## 测试口径

tests/{log,pb,trace,metrics,otlp,ndjson,s3,rt_scopes}/run.sh 双臂(解释+发射);
bench 家族(CTRON_*_BENCH=1)本地/nightly 门禁;真靶(minio/registry)nightly。
