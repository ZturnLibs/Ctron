# web —— Ctron 应用框架(域包)

**包定位**:仓库根域包 `use web.*`(域包命名空间先例 gui/net/http)。应用框架层:Req/Resp/Router/
中间件/视图微 builder,坐于 http(net 之上协议半层)与 net(TCP 门面)两域包之上;应用代码
(handler + views + store)零 HTTP 底座。设计全案:`docs/superpowers/specs/2026-09-27-web-framework-design.md`
(下称 spec;API 定稿面 §4,语义条款 §7,范围边界 §8)。

**门面契约(spec §4.4 流式演进条款,SSE/WS 波次零改迁移的锚)**:应用与中间件只经构造器与
`.with()` 族触 Resp,禁直构 `Resp` 字面量、不依赖字段布局;Resp 序列化咽喉收敛于 server 一处(2b)。

**测试口径**:引导链双臂——语义门 `sh compiler/ctc.sh check web/core.ct web/router.ct web/view.ct`
(全 OK)+ 行为裸跑 `sh compiler/ctc.sh web/<file>`(rc=0;web 文件带 use 须走引导链,seed 单文件
口径不适用)。路由/取参全为纯函数,零 socket 确定性测试(spec §4.7)。

## API 表(spec §4 定稿面照录)

状态列:**2a** = 本波已交付(实现+断言);**2b** = 次波(server 循环/session/static/openapi/示例迁移)。

### 装配(§4.1;router.ct)

| API | 状态 | 注 |
|---|---|---|
| `router(state) -> Router[S]` | 2a | S=应用状态;无态传 0 |
| `.get/.post/.put/.delete/.patch(路径, handler)` | 2a | 路径 `:param` 单段参数、`*name` 尾通配 |
| `.middleware(mw)` | 2a | 组级;声明序=执行序;已注册路由回填(spec §7-1) |
| `.mount(前缀, 子路由器)` | 2a | 落地名 `mount_at`;子组中间件随逐路由预拼链在册 |
| `.install(plugin)` | 2a | 插件=`fn(Router[S]) -> Router[S]` 普通函数,零新机制(§4.6) |
| `.static(前缀, 目录)` | 2b | 收编 frm/static(ETag/Range/穿越防护) |
| `.body_limit(n)` | 2b | 默认 1MB,超限 413 |
| `.not_found(h)` / `.method_not_allowed(h)` | 2b | 默认 404/405 体可覆写;2a 固定默认文本 |
| `.openapi(标题, 版本)` | 2b | 运行期 /openapi.json 同源导出 |
| `serve("127.0.0.1:8091", r)` | 2b | 阻塞循环(net 装配/keep-alive/panic→500/HEAD) |
| 启动期重复 (method,路径) panic | 2a | `r_add`/`mount_at` 装配即失败 + `has_conflict` 自检面 |
| 匹配:静态段 > `:param` > `*splat` | 2a | 路径命中方法不中 → 405 + `Allow` 头 |
| `route_match(r, method, path) -> Match` | 2a | 匹配直读面(params/splat/allow) |
| `dispatch(r, method, path, body) -> Resp` | 2a | serve 循环复用本体(2b) |
| `test_call(r, method, path, body)` | 2a | dispatch 别名,spec §4.7 测试口(零 socket) |

### handler 与中间件(§4.2/§4.5)

| API | 状态 | 注 |
|---|---|---|
| `fn h(req: Req[S]) -> Resp` | 2a | 单形参;状态经 `req.state` |
| `fn mw(req: Req[S], next: fn(Req[S]) -> Resp) -> Resp` | 2a | 两形参普通函数;三档形态(纯函数/工厂/带状态工厂)零框架机制 |

### 取参(§4.3;core.ct)

| API | 状态 | 注 |
|---|---|---|
| `param(r, "id") -> Str` | 2a | 六 Str 取值器统一「缺回空串、解码就绪、不 panic」 |
| `param_i64(r, "id") -> Option[I64]` | 2a | `.or(0)` 默认值显式在调用点 |
| `param_f64(r, "x") -> Option[F64]` | 2a | 手搓十进制解析(std.strconv 无浮点 parse);≤18 位 |
| `query(r, "q")` / `query_all(r, "tag")` | 2a | 百分号解码 + UTF-8 四字节重组(web 自建 `pdec`,frm C8 域缺口) |
| `form(r, "title")` / `form_all(r, "tag")` | 2a | `'+'`→空格、`%XX`、中文就绪 |
| `header(r, "User-Agent")` | 2a | 大小写不敏感 |
| `cookie(r, "sid") -> Str` | 2a | 未带回 "";值原样(v1 不做 percent 解码) |
| `req.state` | 2a | S;dispatch 注入宿主 router 的 state |
| `req.json() -> Option[JDoc]` | 2b | JSON body 绑定(收编 frm/body 容量上限) |
| `j.str_between` / `j.i64_between` | 2b | JDoc 校验族(随 req.json) |
| `req.session("uid")` | 2b | with_sessions 层注入 |
| `req.flash()` | 2b | 读即清一次性提示(PRG) |

### 返回(§4.4;core.ct)

| API | 状态 | 注 |
|---|---|---|
| `html` / `json` / `text` / `redirect` / `status` | 2a | 五构造器;安全默认 `nosniff` + charset 内置 |
| `err_json(n, msg)` | 2a | `application/json`(Task 5 修正;此前误走 text/plain) |
| `with(r, k, v)` / `with_status(r, n)` | 2a | 链式面;值语义每步新 Resp |
| `attachment(r, filename)` | 2a | `Content-Disposition`;filename 调用点可信常量 |
| `xml(body)` / `yaml(body)` | 2a | Content-Type 自动;xml 不做自动转义(xml_esc spec 登记不做) |
| `json_obj().str/.i64/.bool/.strs/.end/.end_status` | 2a | 值自动转义;UFCS 按函数名精确解析 |
| `csv_rows(headers, rows) -> Str` | 2a | RFC 4180 最小引用形(逗号/引号/换行入值才加引号、引号双写);导出用法 `text(csv_rows(h, rows)).attachment("report.csv")` |
| `bytes(data, mime)` | 2b | 挂 §7-8 二进制安全门(L4 Bytes 判立在册) |
| `send_file(path, mime)` | 2b | 框架读盘(经能力审计) |
| `grant_session` / `drop_session` / `flash` | 2b | HttpOnly+SameSite=Lax 默认随(收编 frm/auth) |
| `@derive(Json)` struct 直出 | 2b | 语言项 L2(spec §9);未落地前应用手写 `json_of` |

### 视图微 builder(§4.8;view.ct)

| API | 状态 | 注 |
|---|---|---|
| `el(tag)` / `.cls` / `.attr` / `.child` / `.children` / `.text` / `.raw` / `.done` | 2a | text/attr/cls **值**自动 ht_esc(五实体);raw 显式逃生口;**信任边界:tag 与属性 KEY 是调用点可信常量不转义**,动态串一律走值位 |

### 能力面

| 项 | 状态 | 注 |
|---|---|---|
| `Ctron.ctcl` `caps: ["net.listen"]` | 2a | 供应链审计标记(依赖 web 才可能听端口);caps 词表暂无 net.listen,惰性标记 2b 对齐 |

## 调用点注记(实证口径)

- **显式 TypeArgs(E2060)**:泛型经 `Req[S]` 形参不参与推断,取参调用点须显式
  `query[I32](r, "q")` 形(spec §4.3 示例的运行注记;v0 限制)。
- **泛型 fn 值引用不可用(E2020)**:占位 handler/插件用闭包字面量;泛型 struct 字面量一律裸形
  (`Route { ... }`)。引导 interp 口径实测,见 router.ct 头注。
- **包内跨文件同名 decl**:被 use 方(core)的 test 体引用会把 decl 拉进选择性合并 keep 集
  (pkg_load_use "Test 恒随"),与 use 方同名 decl(如 view 的 `text`)撞 E5030——core 测试
  载体用 `html()` 顶位,真实导出用法照上行 csv_rows 注。
