# web —— Ctron 应用层 Web 框架设计

日期:2026-09-27 ｜ 泳道:server ｜ 状态:已定稿(用户逐题裁决,见 §2 决策记录)

## 1. 背景与痛点

todo_app(四模块 ~1560 行)中真正的业务逻辑不足 400 行,其余是四层税:

1. **装配税(~500 行/应用)**:每个应用手写同款骨架——listen/accept 循环、增量头解析、
   CL 读体、`"HTTP/1.1 " + code + "\r\n"` 手拼响应、路由 if 链、`path_id` 手撕路径参数、
   Cookie 手解析。todo_api(512 行)与 todo_app 是两份几乎相同的底座拷贝。讽刺的是
   `http/frm` 十六个件(router/auth/csrf/cors/limit/static/metrics/openapi…)全在库——
   **能力不缺,缺的是把它们装配成"一台服务器"的那一层**。
2. **编译器缺陷税(架构变形根因)**:主文件直发截断 → main 做成转发 shim;
   `List[struct]` 元素读债 → 设计中的 `List[Todo]` 降级为 `List[Str]` NDJSON + 路径取值器;
   星形单父纪律 → `dstr/sdstr` 等同函数双份;frm/auth 触发截断 → 手写 HMAC 令牌
   (sess.ct 252 行);http/form 拒非 ASCII → 手写 60 行 UTF-8 百分号解码。
3. **语言税**:`0 - 1` 代负字面量、`.as[I64]()` 满天飞、`byte_at/byte_slice` 逐字节外科
   (含中段非 NUL 结尾雷)、平行 `List[Str]` 冒充 struct 列表、JSON 手拼串。
4. **API 形态税**:frm 交付形态是 C 风格原语(buffer + 访问器函数、err 码返回值、
   下标循环),无类型化 Req/Resp、无 `:id` 参数、无中间件组合、无一行起服。

本设计补上那"缺的一层":**web 应用框架包**。编译器缺陷不绕、不等,按
「能力优先于 hack」横切裁决与「缺陷销账分立」约定归编译泳道账本(§9)。

## 2. 决策记录(用户逐题裁决)

| # | 议题 | 裁决 |
|---|---|---|
| 1 | 定位边界 | **应用框架层;编译器缺陷销账分立**(框架不绕 bug 设计,也不等修完) |
| 2 | 路由基座 | **运行期路由表为基座 + comptime 增量审计层**(二期:拼错/重复路由编译期报;OpenAPI 走运行期端点,永远同源) |
| 3 | v1 范围 | **核心+应用件;frm 薄收编**(调用不复制,单一正本) |
| 4 | 视图面 | **Str 视图 + ht_esc 安全默认**;CTML HTML 形态为预留适配点(gui 泳道地盘) |
| 5 | 目标形态 | 路由表式(1 为基座),fn 值/闭包组合子为 API 语言 |
| 6 | 守卫模板噪声 | **路由组 `.mount_at` + 平铺 `.layer` 链**替代逐路由包裹;组级中间件对全组生效 |
| 7 | 状态形态 | **`Req[S]` 泛型携带 + `req.state`**(§8.1"上下文打包"钦定缓解);handler 单形参 |
| 8 | 中间件/插件 | 自定义中间件三档形态文档化;**插件 = `fn(Router[S]) -> Router[S]`,`.install()` 一行挂载**,零新机制 |
| 9 | 参数/返回面 | 六源取参全谱 + 五构造器/链式/`json_obj()` 构建器/`send_file`/`bytes` |
| 10 | struct 直出 | **`@derive(Json)` 正解**(无反射宪法 §8.4 的 sanctioned 通道),归编译账本;过渡 = 应用手写 `json_of`(每 struct ~5 行,derive 落地后删函数调用点零改) |
| 11 | 遗漏审计 | 十处遗漏按 A(补入 v1)/B(边界声明)/C(次波登记)三类处置(§8) |
| 12 | 通用性审计 | 三风险补齐:流式=演进契约三件套(§4.4,死变体否决)、二进制=plan 探针门+条件 L4(§7-8/§9)、插件 S 契约(§4.6);六小补丁(query_all/param_f64/多值头政策/method_not_allowed/timeout_ms/spawn 惯例)入 v1(§8.1) |

## 3. 分层与包结构

```
应用代码(todo_app 等:handler + views + store,零 HTTP 底座)
────────────────────────────────────────
web    应用框架(本设计):Req/Resp/Router/中间件/serve/session/static/openapi/testkit
────────────────────────────────────────
http   协议件:parse(增量解析)/form/enc + frm(十六原语件,零改动)
net    TCP 门面
```

新建**仓库根域包 `web/`**(`use web.*`;域包命名空间先例 gui/net/http):

```
web/
  Ctron.ctcl        # pkg web;caps: net.listen(供应链审计:依赖 web 才可能听端口)
  core.ct           # Req[S]/Resp/构造器/访问器/json_obj/csv_rows
  router.ct         # Router[S]/匹配/参数提取/启动自检/路径规范化
  mw.ct             # 中间件框架 + log_requests/with_sessions/secure_headers/allow_origins
  static.ct         # 静态文件适配(收编 frm/static:ETag/Range/If-None-Match/traversal 防护)
  openapi.ct        # 运行期 /openapi.json 端点(路由元数据同源)
  server.ct         # serve 循环(net + http/parse + frm/timeout 装配,一次写对)
  testkit.ct        # test_call/req_of 夹具(仅 test 口径编译)
  README.md         # API 表
```

能力面:`web` 包清单声明 `caps: net.listen`;应用 manifest 继承审计——依赖了 web 的包
才可能监听端口,供应链"装了就偷偷外联"在审计面不可表达(路线图创新 4 的框架兑现)。

## 4. 核心 API(定稿全暴露面)

暴露面总计:**router/serve 两个词起,layer/mount_at/install 三个组合子,
html/json/text/redirect/status 五个构造器,req 八个访问器,test_call 一个测试口。**

### 4.1 装配

```ctron
router(state)                         // Router[S];S = 应用状态类型,无态应用传 0
  .get/.post/.put/.delete/.patch(路径, handler)   // 路径::id 单段参数,*name 尾通配
  .layer(mw)                          // 组级中间件:组内全部路由生效;声明序=执行序
  .mount_at(前缀, 子路由器)            // 前缀+中间件随组叠加(外层→内层→handler)
  .install(plugin)                    // 插件挂载:plugin: fn(Router[S]) -> Router[S]
  .static(前缀, 目录)                 // 收编 frm/static
  .body_limit(n)                      // 请求体上限,默认 1MB,超限 413
  .not_found(h)                       // 默认 404 体可覆写
  .method_not_allowed(h)              // 默认 405 体可覆写(Allow 头框架自动带)
  .openapi(标题, 版本)                // 挂 /openapi.json(运行期同源导出)

serve("127.0.0.1:8091", r)            // 阻塞;排空后返回
```

匹配语义:静态段 > `:param` > `*splat`;路径命中方法不中 → 405+Allow;启动期重复
(method,path) panic(**失败在启动,不在半夜**);匹配热路径预算 ≤200ns@16 路由。
实现注:Router 是值,`.layer/.mount_at/.install` 均返回新值;mount 合并时对每条子路由
预拼链条(外层 mw+子组 mw+handler),最终一张平铺表,热路径无嵌套开销。

### 4.2 handler 与中间件

```ctron
// handler 形(单形参;状态经 req.state 取用——§8.1 上下文打包)
fn h_del(req: Req[App]) -> Resp

// 中间件形(两形参普通函数;自定义中间件三档形态见 §4.5)
fn need_login(req: Req[App], next: fn(Req[App]) -> Resp) -> Resp
```

### 4.3 参数获取(六源全谱;六个 Str 取值器统一「缺回空串、解码就绪、不 panic」)

```ctron
req.param("id")            // 路径参数 "42";req.param("path") 通配 "css/main.css"
req.param_i64("id").or(0)  // 类型化族:Option[I64],默认值显式写在调用点
req.param_f64("x").or(0)   // 类型化族第二件:坐标/评分类 API(注意双宿主 F64 分歧坑位)
req.query("q")             // URL 查询,百分号解码 + UTF-8 重组就绪
req.query_all("tag")       // 同名多值查询参数 List[Str](与 form_all 对称)
req.form("title")          // 表单 urlencoded;'+'→空格、%XX、中文就绪
req.form_all("tag")        // 同名多值 List[Str](复选框族)
req.json()                 // Option[JDoc]:JSON body,JDoc 取值器 str/i64/bool/list + .or
req.header("User-Agent")   // 大小写不敏感
req.cookie("sid")          // 未带回 ""
req.session("uid")         // with_sessions 层注入后可用
req.state                  // S:应用状态(App 结构,内含 Mutex/Atomic 与配置)
```

解码纪律全在框架:百分号解码/UTF-8 四字节重组、头折叠、Cookie 分号解析、JSON 容量
上限(收编 frm/body)。重复请求头按 RFC 逗并(`Set-Cookie` 响应侧例外,保序多值)。
应用永远见不到字节缓冲——todo_app 的 `pctb/pdec/f_fld/lane_str/
hval` 约 200 行解码外科在这个面上零残留。

### 4.4 返回(五构造器 + 链式 + 格式矩阵)

```ctron
html(body)   // 200 text/html; charset=utf-8
json(body)   // 200 application/json
text(body)   // 200 text/plain
redirect(loc)   // 303 + Location(PR-G 惯例)
status(n, body) // 任意状态码
bytes(data, mime)       // 二进制直出(Ctron Str 即字节串)
send_file(path, mime)   // 框架读盘(经能力审计)

// 链式
return html(page).with_status(201).with("X-Request-Id", rid)
    .with("Set-Cookie", "theme=dark; Path=/").attachment("report.csv")

// JSON 类型化构建器(值自动转义;终结 \" 手拼地狱)
return json_obj().str("title", title).i64("count", n).bool("ok", true)
    .strs("tags", tags).end()

// 会话签发/注销(HttpOnly+SameSite=Lax 默认随;Secure 留配置点)
return redirect("/app").grant_session(req.state.key, uid, 604800)
return redirect("/login").drop_session(req.state.key)

// 其他格式
xml(body)    // text/xml 自动;xml_esc 助手(builder 登记不做)
yaml(body)   // application/yaml 自动(输出场景少,配置读走 std/config)
csv_rows(表头, List[List[Str]])   // 报表导出小助手 → text/attachment
```

**struct 直出**:`@derive(Json)` 编译期生成 `impl Json`,消费面 `json_of[T: Json](v)`
一行直出;`@derive(Json, Yaml, Xml)` 多格式同源(代码即文档的无反射诚实形态)。
derive 未落地期间应用手写 `json_of`(每 struct ~5 行机械代码,derive 落地后删函数、
调用点零改动)。struct 反序列化 `from_json` 随 derive 登记志向,不做承诺。
错误就是响应:业务 4xx 直接构造对应 Resp;handler panic 由框架兜 500 不泄栈。

**流式演进契约(通用性审计定案,SSE/WS 波次的架构预留)**:v1 的 Resp 为
`struct { status, headers, body: Str }`,不引入死变体(Ctron enum 变体须可构造,
v1 造不出流句柄,空臂反成死码)。预留以三件套落位:①**门面契约**——应用与中间件
只经构造器与 `.with()` 族触 Resp,禁直构字面量、不依赖字段布局(写进 README 与
13-web.md;testkit 字段断言为框架内口径,演进时框架统一迁移);②**单一咽喉**——
Resp 序列化收敛于 server.ct 一处,SSE/WS 波次将 `body: Str` 演进为
`body: Body` 判别和(`text(Str)` / `chunked(流句柄)`)时应用与中间件零改;
③**语义挂点**——超时/日志/限流对流式路由的计法(响应头写出 ≠ 完成)在
SSE/WS 波设计内定,挂点即咽喉处。

### 4.5 自定义中间件三档形态(全是普通函数/闭包,零框架机制)

```ctron
// ① 纯函数          fn no_cache(req, next) -> Resp { return next(req).with("Cache-Control","no-store") }
// ② 工厂(带配置)   fn timing_ms(limit: I64) -> fn(Req[App], fn(Req[App]) -> Resp) -> Resp { |req, next| {...} }
// ③ 带状态工厂      fn rate_limiter(per_min: I64) -> ... { let hits: Mutex[Map[Str,I64]] = ...; |req, next| {...} }
```

每路由/每组的超时档位不引入新机制,就是中间件:`.layer(timeout_ms(5000))`
(框架件,收编 frm/timeout;全局档 `.timeout(ms)` 之外组级可细化覆盖)。

### 4.6 插件:`fn(Router[S]) -> Router[S]`,零新机制

插件 = 往路由器装「路由+中间件+文档」捆绑包的普通函数;`.install()` 一行挂载,
顺序即装配序;带配置的插件是工厂;第三方包发插件 = 发一个普通 `.ct` 模块,
能力审计照常管住(要听端口/读盘照样得声明 caps)。框架自带件
(with_sessions/log_requests/static/openapi)本身是第一批插件(自举吃狗粮)。

**插件 S 契约(生态通用性条款)**:加路由的插件(如 auth 要签发 cookie)必然要求
`S` 含特定字段;Ctron 无反射、字段约束走不了 trait,故契约 = 插件文档**必须声明
最小字段集**(形如"要求 S 含 `key: Str`"),缺字段在插件体编译点报错(报错位置
确定、消息可读)。纯中间件插件(不限路由)一律写成 `[S]` 泛型,对任意 S 组合。

### 4.7 测试(整台服务器零 socket)

```ctron
test "未登录访问 /app 踢回登录" {
    let app = App { ...fake... }
    let resp = test_call(full_router(app), "GET", "/app", "")
    assert(resp.status == 303)
}
```

装配提成 `fn full_router(app: App) -> Router[App]`,main 与测试同源(§8-A8 条款)。
handler 需要时钟/随机时经 `req.state` 字段注入,测试传 fake(§8-A5 条款)。

## 5. 体验样例(迷你 todo,全部业务 ~75 行;今天同物 1560 行)

```ctron
use web.{ router, serve, html, redirect }
use web.mw.{ log_requests, with_sessions, grant_session }
use http.frm.html.{ ht_esc, ht_page }

struct Todo { id: I64, title: Str, owner: Str, done: Bool }
struct App  { todos: Mutex[List[Todo]], key: Str, next_id: Atomic[I64] }

fn main() {
    let app = App { todos: Mutex[List[Todo]](), key: "dev-mini", next_id: Atomic[I64](1) }
    let r = router(app)
        .layer(log_requests)
        .layer(with_sessions(app.key))
        .get("/", |req| redirect("/app"))
        .post("/login", h_login)
        .mount_at("/app", guarded(app))
    serve("127.0.0.1:8091", r)
}

fn need_login(req: Req[App], next: fn(Req[App]) -> Resp) -> Resp {
    if req.session("uid").len == 0 {
        return redirect("/login")
    }
    return next(req)
}

fn guarded(app: App) -> Router[App] {
    return router(app)
        .layer(need_login)               // 组级守卫写一次,组内全路由生效
        .get("/", h_list)
        .post("/add", h_add)
        .post("/del/:id", h_del)
}

fn h_login(req: Req[App]) -> Resp {
    let uid = req.form("uid")
    if !user_auth(req.state, uid, req.form("pass")) {
        return html(view_login("用户名或密码不对"))
    }
    return redirect("/app").grant_session(req.state.key, uid, 604800)
}

fn h_list(req: Req[App]) -> Resp {
    let uid = req.session("uid")
    let rows = req.state.todos.with(|l| {            // Mutex.with:闭包体即临界区
        var out = ""
        var i: I32 = 0
        while i < l.len {
            let t = l[i]                              // P0-1 解锁处
            if t.owner == uid {
                out = out + "<li>" + ht_esc(t.title) + "</li>"
            }
            i += 1
        }
        return out
    })
    return html(ht_page("todo", add_form() + "<ul>" + rows + "</ul>"))
}

fn h_add(req: Req[App]) -> Resp {
    let title = req.form("title")
    if title.len == 0 || title.len > 512 {
        return redirect("/app")
    }
    let id = req.state.next_id.fetch_add(1)
    req.state.todos.with_mut(|var l| {
        l.push(Todo { id: id, title: title, owner: req.session("uid"), done: false })
        return true
    })
    return redirect("/app")
}

fn h_del(req: Req[App]) -> Resp {
    let id = req.param_i64("id").or(0)
    req.state.todos.with_mut(|var l| {
        todo_remove(l, id, req.session("uid"))        // 应用自有逻辑
        return true
    })
    return redirect("/app")
}
```

样例略去的 `user_auth/view_login/add_form/todo_remove` 均为应用自有面
(认证/视图/存储逻辑,与框架无关),照 todo_app 现有写法平移即可。

## 6. serve 循环(框架内一次性写对的装配税)

listen → accept → 增量头解析(http_parse_head)→ CL 读体(限 `.body_limit`)→
**keep-alive 循环**(http_keep_alive;两示例的 Connection: close 一次性浪费终结)→
构造 Req → 匹配 → 预拼中间件链 → handler → Resp 序列化一次写出 → 访问日志行(可选)。
panic 兜 500 不泄栈;读/处理超时收编 frm/timeout;HEAD 自动应 GET 路由(去 body 保头);
路径规范化安全默认(多余斜杠归一、拒 `..` 段);默认 404/500 文本页,404 体可覆写;
排空端点惯例 `POST /__shutdown`(v1 无信号处理)。

## 7. 语义条款(定稿细则)

1. **组级中间件对组内全部路由生效**(不按注册位置)——"路由写在 layer 之前就静默
   漏守卫"在形态上不可表达;同组多条按声明序,叠加组外层先执行。
2. 解码纪律全框架化(§4.3);请求字符集假定 UTF-8,响应 charset=utf-8 默认。
3. Cookie 安全默认:`grant_session` 随 HttpOnly+SameSite=Lax(frm/auth 同款)。
4. 安全头:默认 `X-Content-Type-Options: nosniff`;HSTS/CSP 经 `secure_headers()` 显式开。
5. Router 启动自检 panic 优于运行期错路由;comptime 增量审计层(二期)把该项提前到编译期。
6. OpenAPI 走运行期端点,永远与运行中服务同源;E6030 无反射下 schema 元数据显式挂
   (`.doc(...)`),不做反射魔法。
7. **响应后异步动作惯例**:handler 返回 Resp 前可 `spawn` 后续任务(发邮件/清缓存类),
   归调用方 scope 树;排空时由结构化并发兜底取消。框架不设第二套后台任务机制。
8. **二进制安全门(plan 期探针)**:`bytes()`/上传面依赖 Str 承载任意字节,而
   `byte_slice` 中段有在册非 NUL 结尾雷。实施计划须含二进制往返探针(含 NUL 的
   body 经 进→取→byte_slice 运算→出 全链);红则登记 `Bytes` 类型语言项(§9 L4)
   后放行文件上传相关波次,纯下行(静态文件,已证可用)不受阻。

## 8. 范围:交付 / 边界 / 次波

### 8.1 v1 交付(含遗漏审计 A 类八条 + 通用性审计补齐)

Req/Resp/Router/serve/中间件框架/插件 install、session(收编 frm/auth)、static
(frm/static)、CORS+安全头(frm/cors+sechdr)、form/query/cookie 解析(http/form)、
`json_obj()`/`csv_rows()` 构建器、`/openapi.json`、访问日志、`/__shutdown` 排空端点、
**body_limit(默认 1MB)**、**HEAD 自动语义**、**路径规范化安全默认**、**默认 500 页+
panic 日志**、**时钟/随机经 state 注入惯例**、**路由匹配 ≤200ns 门禁**、
**docs/spec/13-web.md 规范草案 + README API 表**、**装配提成 fn 惯例条款**、
**`query_all`/`param_f64` 取参对称族**、**多值头政策(请求侧逗并/响应侧保序)**、
**`.method_not_allowed(h)`**、**`timeout_ms(n)` 组级超时中间件**、
**流式演进契约条款(门面/咽喉/挂点,§4.4)**、**插件 S 契约条款(§4.6)**、
**响应后 spawn 惯例(§7-7)**、**二进制安全探针门(§7-8)**、
testkit(`ctron test` 零 socket 确定性测试)。

### 8.2 边界声明(B 类,把"不做"写透)

- **并发吞吐**:v1 阻塞串行(一连接一请求)+ 每请求超时上限;慢客户端卡全服是已知
  边界。并发随 P2 协程 RT 同码解锁(同形契约),框架零改动;max-in-flight/过载 503
  仅并发形态有意义,随 RT 落地。
- **TLS**:v1 明文 HTTP;生产经反代终结 TLS;`std/tls` 落地后挂 TLS 列志向;
  `Secure` cookie 标志留配置点。

### 8.3 次波登记(C 类)

gzip/deflate 响应压缩(**http/enc 全套在库,协商函数现成,次波第一件**)·
csrf/limit 中间件收编 · **SSE/WS(流式,走 §4.4 演进契约的 Body 判别和)** ·
OTLP trace 插件 · multipart(受 §7-8 二进制门约束)· 自定义错误页 ·
信号处理 · CTML 模板适配(gui 泳道地盘,Resp 适配点预留)·
struct 反序列化 from_json(随 @derive)· 内容协商 · comptime 路由审计层。

## 9. 编译泳道账本(分立销账;web 的硬前置)

探针实案(`compiler-c/build/ctronc`,2026-09-27;解释臂全绿,发射臂逐条定位):

| # | 缺口 | 探针 | 状态 |
|---|---|---|---|
| P0-1 | `List[struct]` 元素成员读(`tab[i].p` 红) | final.ct | **在册债**(todo_app 数据层降级同根因) |
| P0-2 | 闭包形参成员读(闭包体内 `r.path` 红) | p2e.ct | 新登记 |
| P0-3 | fn 值调用链成员读(`f(x).body` 红;绕行:提取局部) | p2a.ct | 新登记 |
| P0-4 | 含 fn 字段 struct 入定长数组("数组元素类型不支持") | fixed.ct | 新登记 |

| # | 项 | 说明 |
|---|---|---|
| L2 | `@derive(Json)` | 语言扩展(derive 插件机制,§8.3 注解契约 sanctioned 通道);`@derive(DbRow)` 同池 |
| L3 | 主文件直发截断债(在册) | 关联:todo_app 迁移后仍守"逻辑在依赖模块"纪律,该债销账前 main.ct 不回胖 |
| L4 | `Bytes` 字节串类型(**条件登记**) | 若 §7-8 二进制往返探针红(Str 含 NUL 过 byte_slice/比较运算静默截断)则立此项;探针绿则销 |

按「能力优先于 hack」裁决:web 按目标形态设计,**不做平行 List/act 表/id 分发替身**;
P0 四件全部 member-emit 同族小面。

## 10. 验收门

1. web 包 `ctron test` 全绿(handler/路由/中间件纯函数零 socket 直测 + testkit 全链);
   tests/http 双臂回归全绿(**frm 零改动证明收编是薄适配**)。
2. **todo_app 迁移为对照实验**:e2e nc 探针族全绿;行数账——app.ct 645→约 140
   (删 read_req/respond/解码外科/头搜索/路由手撕/Res 构造器族)、sess.ct 252→0
   (session 层替代,s-前缀副本族全灭)、views/data 基本不动;总账 ~1560→~900。
3. todo_api 不动(教学面),次波迁移登记。
4. 性能门:路由匹配 ≤200ns@16 路由;解码/构造器 no_alloc 稳态差=0(既有门口径)。
5. **二进制往返探针门**:含 NUL 字节的 body 经 取参→运算→响应 全链保真(§7-8);
   红则 L4 立项,文件上传类波次待其销账。

## 11. 与既有资产的关系

- `http/frm` 十六件**零改动**,web 以薄适配器收编(调用不复制,单一正本);
- `http/parse`/`form`/`enc`/`sse`/`ws` 原样为协议层,web 不重复;
- todo_api 保持教学面不动;todo_app 迁移后成为框架首个真实应用与行数对照实验;
- CTML(gui 泳道)经 Resp 适配点接入,本设计不越界。

## 12. 创新点定位(相对主流栈与路线图)

1. **装配税归零**:框架 = 一次写对的 serve 循环 + 函数即中间件 + 值语义路由组;
   1560→~900 行实证,业务代码里一行 HTTP 底座不剩。
2. **handler 纯函数化 → 零 socket 确定性单测**:整台服务器经 `test_call` 走真路径
   (路由/参数/中间件链/重定向),无 mock 框架——路线图创新 6 在应用层的体感。
3. **能力审计随包生效**:`web` 声明 `net.listen`,供应链插件要听端口照样过 manifest
   审计——创新 4 的生态兑现。
4. **无反射宪法下的诚实 OpenAPI/多格式**:运行期端点永远同源;`@derive` 落地后
   struct 多格式同源导出,代码即文档。
5. **Ctron 特色**:无 async 染色、无生命周期标注、闭包即中间件、值语义组合子
   (Router 是值,装配序=读码序)、Mutex/Atomic 内建并发原语护持状态。
