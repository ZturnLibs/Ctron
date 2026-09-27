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
| 6 | 守卫模板噪声 | **路由组 `.mount` + 平铺 `.middleware` 链**替代逐路由包裹;组级中间件对全组生效 |
| 7 | 状态形态 | **`Req[S]` 泛型携带 + `req.state`**(§8.1"上下文打包"钦定缓解);handler 单形参 |
| 8 | 中间件/插件 | 自定义中间件三档形态文档化;**插件 = `fn(Router[S]) -> Router[S]`,`.install()` 一行挂载**,零新机制 |
| 9 | 参数/返回面 | 六源取参全谱 + 五构造器/链式/`json_obj()` 构建器/`send_file`/`bytes` |
| 10 | struct 直出 | **`@derive(Json)` 正解**(无反射宪法 §8.4 的 sanctioned 通道),归编译账本;过渡 = 应用手写 `json_of`(每 struct ~5 行,derive 落地后删函数调用点零改) |
| 11 | 遗漏审计 | 十处遗漏按 A(补入 v1)/B(边界声明)/C(次波登记)三类处置(§8) |
| 12 | 通用性审计 | 三风险补齐:流式=演进契约三件套(§4.4,死变体否决)、二进制=plan 探针门+条件 L4(§7-8/§9)、插件 S 契约(§4.6);六小补丁(query_all/param_f64/多值头政策/method_not_allowed/timeout_ms/spawn 惯例)入 v1(§8.1) |
| 13 | 次波四形态 | SSE/WS/multipart/并发四形态设计预置认可(§13);落地序:P0 编译销账 → web v1 → ④并发 → ①SSE/②WS → ③multipart(独立,仅看二进制探针) |
| 14 | 四形态二次自审 | 语义洞=流式响应头收件箱+短路式中间件条款(§13-①1);依赖洞=「client chunked 增量读」入 SSE 波范围(实证 client.ct 无增量读);默认兜底全套(auto-close/panic 兜底/帧原语三件/WS ping-pong+Origin 同源/filename 交付即清洗/max_in_flight 预注册)入 §13 各小节 |
| 15 | readlet 全场景演绎 | 以书签服务(双面 SSR+API/嵌套守卫角色/CORS 组粒度/分页搜索/导出/导入/后台任务/测试/部署)全流程演绎压测:通过=组粒度 CORS/角色叠加/分页取参/CSV 下载/测试三口径/部署面;发现五项入册——①§7-7 spawn 语义错误改 `req.spawn_bg` 后台 scope(连接 scope 字面用会任务静默死)、②JDoc 校验族、③flash、④err_json、⑤响应全内存边界(§8.2) |
| 16 | 性能审计 | 实证三件:`+` 拼接 O(n²)/StringBuilder 仅挂名、Map.put 全量重建 O(n²)、no_alloc 门措辞错;定案 §14——框架内拼接收敛构建器+StringBuilder 双宿主探针+条件 L5、Req 内部平行 List 线性查写死、分配锚稳态差=0(无增长)正名+RSS 压测门 |
| 17 | 视图面 | 微 builder `el` 族入 v1(§4.8):结构性转义+`\"` 消除;运行期模板引擎正式否决;CTML HTML 形态(创新 5)维持预留适配点 |

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

暴露面总计:**router/serve 两个词起,middleware/mount/install 三个组合子,
html/json/text/redirect/status 五个构造器,req 八个访问器,test_call 一个测试口。**

### 4.1 装配

```ctron
router(state)                         // Router[S];S = 应用状态类型,无态应用传 0
  .get/.post/.put/.delete/.patch(路径, handler)   // 路径::id 单段参数,*name 尾通配
  .middleware(mw)                     // 组级中间件:组内全部路由生效;声明序=执行序
  .mount(前缀, 子路由器)               // 前缀+中间件随组叠加(外层→内层→handler)
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
实现注:Router 是值,`.middleware/.mount/.install` 均返回新值;mount 合并时对每条子路由
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
j.str_between("title", 1, 200)   // 校验族(readlet 演绎定案):取值+域检查一体回 Option,
j.i64_between("page", 1, 10_000) //   str/i64/bool 三型;None 由应用统一转 422——无此族
                                 //   则每 handler ~15 行取值校验样板
req.header("User-Agent")   // 大小写不敏感
req.cookie("sid")          // 未带回 ""
req.session("uid")         // with_sessions 层注入后可用
req.flash()                // 读即清的一次性提示(PRG 标配;session 载体框架管理)
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
err_json(n, msg)   // JSON API 错误统一:status + {"error": msg}(readlet 演绎补)
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

// flash:PRG 成功提示(readlet 演绎补:PRG 钉死则提示是标配)
return redirect("/read").flash("书签已添加")     // 写 session,一次性
// 视图侧:let tip = req.flash()   // 读即清

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

每路由/每组的超时档位不引入新机制,就是中间件:`.middleware(timeout_ms(5000))`
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

### 4.8 视图微 builder(决策 17:治 `\"` 地狱与漏转义,CTML 适配点不变)

```ctron
use web.view.{ el, raw }

fn row(b: Bookmark) -> El {
    return el("li").cls(if b.done { "done" } else { "open" })
        .child(el("a").attr("href", b.url).text(b.title))   // text/attr 值自动转义
        .child(el("button").text("删除"))
}
el("ul").children(rows).done()      // -> Str,喂 html()
```

- 转义变**结构性**:`.text()/.attr()` 自动 ht_esc,信任 HTML 必须显式 `.raw()`——
  漏转义在形态上不可能,XSS 从纪律问题变类型问题;`\"` 手拼引号消失;
- 布局无需继承机制:`ht_page` + 函数组合即模板继承(语言即模板语言);
- ~30 行纯函数,产出仍为 Str(`html()` 口径零变);受 §14-1 拼接常数约束,
  内部 List[Str] 段收集末次 join,大视图等 L5;
- **运行期模板引擎(`{{ }}` 系)正式否决**(运行期解析/零编译检查,反 Ctron 哲学);
  CTML HTML 输出形态(创新 5,编译期查标签/属性)仍是正解与预留适配点,
  落地后 B 与手拼并存,`html(str)` 口径不变。

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
        .middleware(log_requests)
        .middleware(with_sessions(app.key))
        .get("/", |req| redirect("/app"))
        .post("/login", h_login)
        .mount("/app", guarded(app))
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
        .middleware(need_login)               // 组级守卫写一次,组内全路由生效
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

1. **组级中间件对组内全部路由生效**(不按注册位置)——"路由写在 middleware 之前就静默
   漏守卫"在形态上不可表达;同组多条按声明序,叠加组外层先执行。
2. 解码纪律全框架化(§4.3);请求字符集假定 UTF-8,响应 charset=utf-8 默认。
3. Cookie 安全默认:`grant_session` 随 HttpOnly+SameSite=Lax(frm/auth 同款)。
4. 安全头:默认 `X-Content-Type-Options: nosniff`;HSTS/CSP 经 `secure_headers()` 显式开。
5. Router 启动自检 panic 优于运行期错路由;comptime 增量审计层(二期)把该项提前到编译期。
6. OpenAPI 走运行期端点,永远与运行中服务同源;E6030 无反射下 schema 元数据显式挂
   (`.doc(...)`),不做反射魔法。
7. **后台任务语义(readlet 演绎改错)**:handler 内裸 `spawn` 挂**调用方 scope=连接
   scope**——连接断即取消(发响应后的小尾巴可用,但绝不能承载后台作业)。框架提供
   **`req.spawn_bg(fn)`**:spawn 到框架持有的后台 scope(server 根之下,`/__shutdown`
   排空时才整体取消),发邮件/抓取类后台作业一律走它;排空语义=排空端点等后台 scope
   自然终结,不强制杀。
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
**`req.spawn_bg`(§7-7 后台 scope)**、**JDoc 校验族(str/i64_between)**、
**`.flash`/`req.flash()`(PRG 提示)**、**`err_json(n,msg)`**、
**二进制安全探针门(§7-8)**、
testkit(`ctron test` 零 socket 确定性测试)、**视图微 builder `el/.text/.attr/.raw`(§4.8)**。

### 8.2 边界声明(B 类,把"不做"写透)

- **并发吞吐**:v1 阻塞串行(一连接一请求)+ 每请求超时上限;慢客户端卡全服是已知
  边界。并发随 P2 协程 RT 同码解锁(同形契约),框架零改动;max-in-flight/过载 503
  仅并发形态有意义,随 RT 落地。
- **TLS**:v1 明文 HTTP;生产经反代终结 TLS;`std/tls` 落地后挂 TLS 列志向;
  `Secure` cookie 标志留配置点。
- **响应全内存(readlet 演绎定案)**:v1 响应体一次性构造(`body: Str`),超大导出
  (十万行 CSV 量级)受此约束——导出量级以 body_limit 同档为参考;§13-① 的 Body
  判和落地后导出可走 chunked 写句柄,无需新机制。

### 8.3 次波登记(C 类)

gzip/deflate 响应压缩(**http/enc 全套在库,协商函数现成,次波第一件**)·
csrf/limit 中间件收编 · **SSE/WS(流式,走 §4.4 演进契约的 Body 判别和;设计预置 §13-①②;波内含「client chunked 增量读」新件,§13-①2)** ·
OTLP trace 插件 · multipart(受 §7-8 二进制门约束;设计预置 §13-③)· 自定义错误页 ·
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
| L5 | 可增长字符串缓冲(**条件登记**) | `+` 拼接每次 `ctron_str_concat` 新分配,循环拼接 O(n²);`StringBuilder` 现仅 sem.c 类型名表挂名(std 零使用、rt 零函数)。plan 首批探针双宿主;红则立此项——web 构建器与用户视图共同受益(§14-1) |

按「能力优先于 hack」裁决:web 按目标形态设计,**不做平行 List/act 表/id 分发替身**;
P0 四件全部 member-emit 同族小面。

## 10. 验收门

1. web 包 `ctron test` 全绿(handler/路由/中间件纯函数零 socket 直测 + testkit 全链);
   tests/http 双臂回归全绿(**frm 零改动证明收编是薄适配**)。
2. **todo_app 迁移为对照实验**:e2e nc 探针族全绿;行数账——app.ct 645→约 140
   (删 read_req/respond/解码外科/头搜索/路由手撕/Res 构造器族)、sess.ct 252→0
   (session 层替代,s-前缀副本族全灭)、views/data 基本不动;总账 ~1560→~900。
3. todo_api 不动(教学面),次波迁移登记。
4. 性能门:路由匹配 ≤200ns@16 路由;**分配锚计数稳态差=0(无增长;P6 amalloc A/B
   差分口径;非"零分配"——Resp 构造天然分配)**;压测门:10 万请求 RSS 漂移有界
   (阈值 plan 定,§14-4)。
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

## 13. 次波四形态设计预置(2026-09-27 用户裁决认可)

落地顺序:**P0 编译销账 → web v1 → ④并发 → ①SSE/②WS → ③multipart**(③独立,
仅看二进制探针门)。四形态的协议底座均已核实:multipart 解析在 `frm/body.ct`
(P6 交付,语料在库)、SSE 帧编解码在 `http/sse.ct`、WS 帧编解码在 `http/ws.ct`。

### 13-① SSE / LLM 流式输出:Body 判和 + 写句柄

```ctron
fn h_chat(req: Req[App]) -> Resp {          // LLM 流式代理:上游逐 token → 下游 SSE
    let resp = stream_sse()                 // body=chunked 变体;自动 text/event-stream + 禁缓冲头
    let w = resp.writer()
    let up = client_stream("POST", req.state.llm_url, prompt_of(req))
    loop {
        let chunk = up.next()
        if chunk.is_none() || !w.event("delta", chunk.unwrap()) {
            break                           // 上游完 或 客户端断(w.event 回 false)
        }
    }
    w.close()
    return resp                             // 串行语义:handler 返回 = 流写完
}
```

机制:`Body` 判和(`text(Str)`/`chunked(句柄)`)是 §4.4 演进契约的兑现点;帧格式
收编 `http/sse.ct`;上游流式读走 http/client。语义定案:中间件链在**响应头写出前**
全部完成(认证/限流天然安全);`.with()` 对 chunked 只能动 headers;`timeout_ms`
对流式 = **空闲超时**(每次写重置),非总时长;客户端断开 = `net_write` 错 →
句柄置断位,handler 循环感知退出,上游取消由应用自判。**依赖:与 ④并发绑定同一波**
——串行下一个慢流卡全服,流式推送只在 coro RT 下有产品价值。

**二次自审补全(2026-09-27,用户裁决入册)**:

1. **流式响应头收件箱(语义洞修补,SSE/WS 共用)**:中间件后处理(调 `next` 后对
   Resp `.with()`)对流式不生效——handler 中途已上 body,后处理到时头已出。框架内
   消化:req 级"响应头收件箱",框架中间件(cors/secure_headers)把头写入请求级
   缓冲,`stream_sse()/websocket()` 构造时并入;认证类中间件照旧短路拦截(不调
   `next` 直接 401)。**语义条款:应用自定义中间件对流式路由只能短路式**。
2. **上游增量读(依赖洞,实证)**:`http/client.ct` 只有连接级 I/O 与整包 Resp,
   无 chunked 响应体增量读 API——**SSE 波范围必须含「client chunked 增量读」件**
   (基于 parse 层既有编解码原语封装),否则 LLM 代理主用例空转。
3. **默认兜底**:handler return 即框架 auto-close(忘 `close()` 不漏流);流中
   panic = 尽力 `event("error")` + close(头已出,不能 500);句柄原语补
   `w.id / w.retry(ms) / w.comment(str)` 三件(`sse_event_str` 参数现成);
   auto-heartbeat 空闲保活归 ④并发波(串行下无定时面)。

### 13-② WebSocket 双向:升级接管 + 连接循环函数

```ctron
fn h_ws(req: Req[App]) -> Resp {
    return websocket(|conn: WsConn| {      // 中间件全在升级(握手)前走完 ✓
        loop {
            let m = conn.recv()             // Option[WsMsg];None=对端关
            if m.is_none() { break }
            conn.send(render(m.unwrap()))   // -> Bool,false=断
        }
    })
}
```

机制:升级握手(RFC 6455 key/accept)在 serve 层;帧编解码收编 `http/ws.ct`。
handler 签名不动——`websocket(fn(WsConn))` 构造器返回**接管型 Resp**,循环函数即
连接生命周期;v1 串行下收发同循环单线程,P2 后可 spawn 分离读写。房间/广播状态归
`req.state`(Mutex[Map[room, List[WsConn]]]),框架不藏第二个状态通道(插件 S 契约
同款纪律)。

**二次自审补全**:框架默认全套(hidden)——ping/pong 自动应答、close 握手兜底
(`recv()` 回 `None` 即框架完成关闭序列)、单消息上限沿用 body_limit 档、
**升级前 Origin 同源默认校验**(跨站 WS 劫持是真实攻击面;放行白名单复用
`allow_origins`);流式路由的头收件箱与短路式中间件条款同 13-①第 1 条;
**与 ④并发绑定同一波**(串行下一条 WS 连接独占服务器,只够单连接调试);
subprotocol 协商不做(登记)。

### 13-③ multipart 文件上传:req.file() 与 form 对称

```ctron
fn h_upload(req: Req[App]) -> Resp {
    let note = req.form("note")                   // 普通字段照旧(同一 boundary 流解析)
    match req.file("avatar") {                    // Option[Upload]
        Some(u) => {
            if u.size > 2_000_000 { return status(413, "太大") }
            fs_write("./uploads/" + u.filename, u.data)   // filename 交付即清洗(安全名)
            return redirect("/ok")
        }
        None => { return status(422, "缺文件") }
    }
}
let many = req.files("photos")                    // List[Upload],多文件
// Upload { filename: Str, raw_name: Str, ctype: Str, size: I64, data: Str }
```

机制:解析收编 `frm/body.ct` multipart 面。安全默认:per-file 上限(body_limit 之下)、
**`u.filename` 交付即清洗**(basename、去 `..` 段、空名兜 `upload.bin`——清洗在框架,
应用拿到即可安全拼路径;原始名另存 `u.raw_name`)、ctype 白名单归应用、
`filename*`(RFC 5987)解码 hidden、body 解析一次缓存(form/file 混调不重复解析)。
依赖门:`u.data` 是字节串——§7-8 二进制探针先走,红则 L4 `Bytes` 立项、
`Upload.data: Bytes`;大文件流式(不落内存)登记 P8 时代志向。

### 13-④ 高并发(微服务):应用零改动的准确含义

**应用零改动是真零**:阻塞调用(`net_read_t/net_write/accept`)在 P2 coro RT 下
自动变挂起点(reactor + ucontext 栈切换,同形异构契约 D1),serve 循环一字不改,
一个连接等待时 RT 调度别的连接。已实证:coro 臂全周期 0.998–1.053× vs 手写
C epoll、todo_api 双 RT 20/20。

框架内部预置三件(本节落位,届时即插):
1. **连接即子任务**:`while { accept; handle }` → `while { spawn handle(accept()) }`
   ——一处差异,连接=scope 树子任务,取消/排空/`/debug/scopes` 免费(创新 2 兑现);
2. **框架共享态原子化**:metrics 计数/限流桶封装一处,串行档普通 I64、并发档
   Mutex/Atomic(框架自己的;应用状态本就该 Mutex,§5 已示范);
3. **过载保护解锁**:max-in-flight 信号量 + 503 shed(`frm/limit` 现成)——串行档
   不启用,并发档注册即活;`.max_in_flight(n)` 路由器旋钮**本设计预注册**
   (串行档 no-op,API 面一次定死)。

并发正确性回归 = 同码双 RT + 限流计数原子性测试(todo coro 22/22 先例口径)。

## 14. 性能预算与实现策略(2026-09-27 性能审计定案,决策 16)

底座已有证据:P6 路由 66ns、全请求周期 1.049× vs 手写 C epoll、frm 分配锚稳态差=0
——慢的从来不是底座,是 Ctron 惯用法的常数。本节管控常数与框架内部形态。

### 14-1 字符串构建(语言级,最重)

`+` 拼接每次 `ctron_str_concat` 新分配,循环拼接 O(n²) 拷贝:100 行 ×100B 列表页
≈500KB 拷贝/请求(µs 级,可忍);万行 CSV 导出 ≈GB 级拷贝/请求(不可忍)。策略:

1. 框架内一切多段拼接收敛到内部构建器(解码/query·form 组装/`json_obj`/`csv_rows`/
   日志行,web 私有实现);
2. **StringBuilder 双宿主探针(plan 首批)**:现证据仅为 sem.c 类型名表挂名——探针红
   则立 L5(§9);绿则框架构建器直接受益;
3. 探针落定前的用户视图指引:小页面(≤几百次拼接)循环拼接可忍;大构建等
   L5/构建器,勿裸拼。

### 14-2 Req 内部形态(设计级,写死)

`std/map.put` 每次全量重建双 List(实证:O(n) 拷贝/put)——**Req 的 headers/params/
cookies 内部一律平行 List 存储 + 访问器线性查**(典型 ≤20 项,亚 µs);Map 仅在应用
显式索取时惰性物化。**禁止按字段示意类型逐 put 构建 Map**(每请求 O(n²) 陷阱)。

### 14-3 其余实现注

- 路由线性匹配 v1 可行(典型 ≤64 路由亚 µs);>几百路由回 `frm/router` trie 的升级点;
- `/openapi.json` 首次构建后缓存(惰性单次),不逐请求重建;
- `log_requests` 行缓冲,不逐条刷盘(串行档同步 println 的 syscall 预算);
- 每请求 HMAC 会话校验(std/crypto 纯 Ctron)µs 级,预算内;**interp 测试口径禁
  pbkdf2**(在册 OOM 债;HMAC 可),密码面测试走原生口径;
- 静态大文件受 §8.2 响应全内存边界约束;Body 判和(§13-①)落地后走 chunked 写句柄;
- 串行 accept 吞吐天花板与响应全内存为既登记边界(§8.2),不在本节重复开洞。

### 14-4 门禁语义(修正 §10-4)

"no_alloc" 正名:**分配锚计数稳态差=0(无增长)**,P6 amalloc A/B 差分口径——
Resp/Req 构造天然分配,零分配是frm 解析/匹配层的局部口径,不是全框架口径。
压测门:10 万请求 RSS 漂移有界(阈值 plan 定)。
