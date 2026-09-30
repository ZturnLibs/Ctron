# web —— Ctron 应用框架(域包)

**包定位**:仓库根域包 `use web.*`(域包命名空间先例 gui/net/http)。应用框架层:Req/Resp/Router/
中间件/视图微 builder,坐于 http(net 之上协议半层)与 net(TCP 门面)两域包之上;应用代码
(handler + views + store)零 HTTP 底座。设计全案:`docs/superpowers/specs/2026-09-27-web-framework-design.md`
(下称 spec;API 定稿面 §4,语义条款 §7,范围边界 §8)。

**门面契约(spec §4.4 流式演进条款,SSE/WS 波次零改迁移的锚)**:应用与中间件只经构造器与
`.with()` 族触 Resp,禁直构 `Resp` 字面量、不依赖字段布局;Resp 序列化咽喉收敛于 server 一处
(2b✓,serve.ct `render`——全仓唯一状态行/头区/CL 拼接点)。

**测试口径**:引导链双臂(2c 收口起全包口径)——语义门 `sh compiler/ctc.sh check` 十文件
(core/router/mw/json/serve/view/static/openapi/compress/guard)全 OK + 行为裸跑每文件 rc=0
(`sh compiler/ctc.sh web/<file>`;web 文件带 use 须走引导链,seed 单文件口径不适用)。
含 crypto 的 mw/guard 裸跑走 canonical 编译器快道 `CTRON_STDPATH=$PWD/std
compiler/bin/ctron-cc run web/<f>`(C seed 宿主 crypto 闭包面挂死/错果,2c Task 2 在册)。
压缩原语 extern lane 帧不可 interp(compress.ct 头注①):其 interp 臂只覆判定面,压缩往返
e2e 属 emit 臂(夹具待落盘,L4/L6/L7 线)。路由/取参全为纯函数,零 socket 确定性测试(spec §4.7)。

## API 表(spec §4 定稿面照录)

状态列(2c 收口实态):**2a / 2b✓ / 2c✓** = 已交付(实现+断言,波次注记;落地名与
spec 链式糖有差者随注);裸 **2b** = 在册待交付(后继波,余项见各行)。

### 装配(§4.1;router.ct)

| API | 状态 | 注 |
|---|---|---|
| `router(state) -> Router[S]` | 2a | S=应用状态;无态传 0 |
| `.get/.post/.put/.delete/.patch(路径, handler)` | 2a | 路径 `:param` 单段参数、`*name` 尾通配 |
| `.middleware(mw)` | 2a | 组级;声明序=执行序;已注册路由回填(spec §7-1) |
| `.mount(前缀, 子路由器)` | 2a | 落地名 `mount_at`;子组中间件随逐路由预拼链在册 |
| `.install(plugin)` | 2a | 插件=`fn(Router[S]) -> Router[S]` 普通函数,零新机制(§4.6) |
| `.static(前缀, 目录)` | 2b✓ | 落地名 `static` 挂链 + `static_dir`(收编 frm/static:ETag/304/Range·416/穿越拒 403 fail-closed;web 侧补 nosniff) |
| `.body_limit(n)` | 2b✓ | 落地面 `serve_with_limit(…, body_limit)` 参(非链式糖)+ 默认 1MB;形坏 400/超限 413;超 I32 上界装配期 panic(终审修复) |
| `.not_found(h)` / `.method_not_allowed(h)` | 2b✓ | 覆写挂点交付(router.ct);默认 404/405 文本仍内置 |
| `.openapi(标题, 版本)` | 2b✓ | 落地名 `openapi_json(r, 标题, 版本) -> Resp`:路由表同源最小 OpenAPI 3,装配点一行挂载(链式糖 v1 不另入 Router 面,openapi.ct 头注) |
| `serve_with(io, host, port, r)` | 2b✓ | SrvIo 值驱动阻塞循环:头区 32KB→400/CL 读体/HEAD 自动应 GET/`POST /__shutdown` 排空;`Connection: close` 串行口径;真 net 接线=调用点十余行映射(`use net` 拉入即 W8052 破 interp 净土,serve.ct 头注) |
| `serve("host:port", r)` 简签名 / keep-alive / panic→500 | 2b | 待:简签名糖;keep-alive 挂 L6/L7 并发波(§13-④);panic=进程级边界在册(无进程内 catch,handler panic 即进程亡;缓解=装配期 has_conflict+test_call 门) |
| 启动期重复 (method,路径) panic | 2a | `r_add`/`mount_at` 装配即失败 + `has_conflict` 自检面 |
| 匹配:静态段 > `:param` > `*splat` | 2a | 路径命中方法不中 → 405 + `Allow` 头 |
| `route_match(r, method, path) -> Match` | 2a | 匹配直读面(params/splat/allow) |
| `dispatch(r, method, path, body) -> Resp` | 2a | serve 循环复用本体(serve_with 经 dispatch_h 已接) |
| `test_call(r, method, path, body)` | 2a | dispatch 别名,spec §4.7 测试口(零 socket) |

### handler 与中间件(§4.2/§4.5)

| API | 状态 | 注 |
|---|---|---|
| `fn h(req: Req[S]) -> Resp` | 2a | 单形参;状态经 `req.state` |
| `fn mw(req: Req[S], next: fn(Req[S]) -> Resp) -> Resp` | 2a | 两形参普通函数;三档形态(纯函数/工厂/带状态工厂)零框架机制 |
| `with_sessions(key, now)` / `log_requests` / `timeout_ms(ms)` | 2b✓ | mw.ct 自带件:会话注入(时钟 fn 注入,now/ttl 恒 unix 秒)/声明序日志/超时=透传校验形(真超时挂并发波,头注诚实口径) |
| `compress_mw[S](threshold)` | 2c✓ | gzip 响应压缩(http/enc 薄收编):四门(已带 CE→协商 content_coding_negotiate→阈值→文本族)→ 压缩 + CL 原位换值 + `Vary: Accept-Encoding`;压缩 ≥ 原大回退(Vary 仍追)。**勿裸上线(头注即闸)**:wire 载体面未解——gzip_bytes 落 U+0100 映射载体(Str 全 strlen,NUL 即失),待 render 咽喉解码或 Bytes 槽(L4);带内 NUL 文本体入压缩面静默截尾(文本族门唯一防线) |
| `csrf_mw[S](key)` | 2c✓ | 双提交(frm/csrf 薄收编):写方法 cookie 端 vs form 端 `csrf_verify`(nonce 域+HMAC 重算),败 403 "csrf rejected";通过面响应侧恒发新 csrf cookie(SameSite=Lax 随 frm;HttpOnly 不设——双提交 JS 可读);nonce=序号支,确定性可测 |
| `rate_limit_mw[S](cap, refil, ivl, maxif, now)` | 2c✓ | 令牌桶 per-key + max-in-flight 闸(frm/limit 薄收编):in-flight 先于桶(shed 503+Retry-After 1/超限 429+Retry-After);键=XFF 首段(**可伪造=在册边界**,真键挂 Req.remote/serve peer 落位 P8);时钟注入可测;装配期参数非正 panic;键表满 fail-open(纯面测试钉死) |

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
| `json_of(r) -> JDoc`(spec 链式名 req.json) | 2b✓ | web/json.ct 独立成件(core use std.json 即翻转 view 合并态 E5030,头注在册);坏体 ok=false + jd_* 恒 None——422 交调用点 err_json 回形;容量上限随 serve body_limit |
| `jd_str / jd_bool / jd_i64 / jd_str_between / jd_i64_between` | 2b✓ | JDoc 校验族五件(json.ct;i64 走 std/jnum_i64 结构通道,小数/指数/越界 → None) |
| `session(r, "uid")`(spec 链式名 req.session) | 2b✓ | with_sessions 验签后经 req_set_session 注入;验签失败原样放行(拦截属应用守卫,不越权代拦) |
| `req_flash(r)`(spec 链式名 req.flash) | 2b✓ | 读即清一次性提示(PRG;载体 cookie web_flash,清除挂 with_sessions 响应侧——不经链不自清,mw.ct 登记边界) |

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
| `bytes(data, mime)` | 2b | 待(挂 §7-8 二进制安全门/L4 Bytes 判立在册;compress 载体边界同根) |
| `send_file(path, mime)` | 2b | 待(框架读盘,经能力审计) |
| `grant_session(r, key, uid, ttl, now)` / `drop_session(r, key)` / `flash(r, msg)` | 2b✓ | mw.ct(收编 frm/auth 调用不复制):grant 5 参含 now=§8-A5 注入(spec 链式示例 4 参系伪码);Set-Cookie 行 `Path=/; HttpOnly; Max-Age; SameSite=Lax` 序随 frm;drop_session 忽略 key(v1 边界在册) |
| `@derive(Json)` struct 直出 | 2b | 待(语言项 L2,spec §9);未落地前经 `json_obj()` builder 手拼 |

### 视图微 builder(§4.8;view.ct)

| API | 状态 | 注 |
|---|---|---|
| `el(tag)` / `.cls` / `.attr` / `.child` / `.children` / `.text` / `.raw` / `.done` | 2a | text/attr/cls **值**自动 ht_esc(五实体);raw 显式逃生口;**信任边界:tag 与属性 KEY 是调用点可信常量不转义**,动态串一律走值位 |

### 能力面

| 项 | 状态 | 注 |
|---|---|---|
| `Ctron.ctcl` `caps: ["net.listen"]` | 2a | 供应链审计标记(依赖 web 才可能听端口);caps 词表暂无 net.listen,惰性标记对齐 2b/2c 未动,归后继波 |

## 调用点注记(实证口径)

- **显式 TypeArgs(E2060)**:泛型经 `Req[S]` 形参不参与推断,取参调用点须显式
  `query[I32](r, "q")` 形(spec §4.3 示例的运行注记;v0 限制)。
- **泛型 fn 值引用不可用(E2020)**:占位 handler/插件用闭包字面量;泛型 struct 字面量一律裸形
  (`Route { ... }`)。引导 interp 口径实测,见 router.ct 头注。
- **包内跨文件同名 decl**:被 use 方(core)的 test 体引用会把 decl 拉进选择性合并 keep 集
  (pkg_load_use "Test 恒随"),与 use 方同名 decl(如 view 的 `text`)撞 E5030——core 测试
  载体用 `html()` 顶位,真实导出用法照上行 csv_rows 注。
