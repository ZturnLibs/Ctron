# http 域包 —— HTTP/1.1 严格子集(T2;P4-C 面)

解析/报文/客户端/SSE/WebSocket/form/日期/编码 + 中间件框架(frm)的单域集合。
**消费形态 = 顶层命名空间 `use http.<子模块>.{...}`**;规范面 = [语言规范
§11 网络](../spec/11-net.md),服务端全形见 [Server 指南](../server-guide.md)。

## 布局

```
http/parse.ct        请求行/头/体的增量解析器(参考页 parse.md)
http/message.ct      报文构造与响应写面(message.md)
http/client.ct       连接管理 + 请求客户端 + 服务端 accept/read/write 面(client.md)
http/sse.ct          Server-Sent Events(sse.md)
http/ws.ct           WebSocket 升级与帧(ws.md)
http/form.ct         表单/urlencoded(form.md)
http/date.ct         HTTP 日期(date.md)
http/enc.ct          编码助手(enc.md)
http/binddeflate.ct  deflate extern 窄桥(用户不接触,无参考页)
http/frm/            中间件框架 17 件(router/middleware/serve/connect/cors/csrf/
                     auth/body/static/limit/timeout/trace/metrics/otlp/openapi/
                     sechdr/html;参考页 frm_*.md)
http/c_src/          C 胶水
```

注:**http 域无 `<域>/<域>.ct` 门面文件**(0930 库根布局裁决的例外形态),
按子模块直取 `use http.client.{...}` / `use http.frm.router.{...}`。

## 参考页

| 页 | 内容 |
|---|---|
| [parse.md](parse.md) | 请求解析纯面 |
| [message.md](message.md) | 报文/响应构造 |
| [client.md](client.md) | 连接与请求客户端(http_net/client_request/HttpResp) |
| [sse.md](sse.md) / [ws.md](ws.md) | SSE / WebSocket |
| [form.md](form.md) / [date.md](date.md) / [enc.md](enc.md) | 表单/日期/编码 |
| [frm_router.md](frm_router.md) / [frm_middleware.md](frm_middleware.md) / [frm_serve.md](frm_serve.md) | 中间件核心三件 |
| [frm_cors.md](frm_cors.md) / [frm_csrf.md](frm_csrf.md) / [frm_auth.md](frm_auth.md) / [frm_body.md](frm_body.md) 等 | 安全与载荷中间件 |

## 口径与坑位

1. parse 面为**严格子集**(RFC 9112 收窄),语料驱动:`tests/http/corpus/`;
   `parse.ct` 的 `http_head_end` = 体起点语义(勿 +4,在册坑)。
2. 服务循环必须**显式读体再切片**(分片竞态 ~30% Heisenberg,在册)。
3. frm 中间件零 use 叶优先(router/cors/sechdr/limit/timeout);csrf/auth→
   std.crypto、body→std.json;`middleware.ct → http.frm.router` 经
   `use http.frm.router.{...}`(域内下行,禁环)。
4. 中间件/fn 值派发 = 返回 wire,勿包裹(在册坑)。

验收:`tests/http/run.sh`(主环解释器 + corpus 断言夹具 + emit 对拍副臂);
行为端到端 = `tests/http/frm_serve`(IO 粘合在 serve.ct)。
