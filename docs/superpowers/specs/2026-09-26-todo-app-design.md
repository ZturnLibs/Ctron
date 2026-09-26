# todo_app —— Ctron 服务端完整应用示例设计（带登录的 Todo 管理）

日期：2026-09-26 ｜ 泳道：server ｜ 状态：已定稿（用户离线，按推荐项裁决，可翻）

## 1. 背景与定位

服务端泳道已有 `examples/todo_api`（P6-E）：框架最小装配教学示例——JSON REST + **假登录**
（任意 uid 直接签 JWT，无密码校验）、单用户（uid 恒 "demo"）、内存态。用户问："能做出一个
完整的应用的例子吗？比如一个带登录的 todo 管理应用。"

本设计补上"完整应用"缺的四块：**真密码认证、Cookie 会话、多用户数据隔离、持久化**，
并以**浏览器可直接操作的服务端渲染 HTML** 交付。与 todo_api 互补不重复：API 教学面
归 todo_api，本示例示范"用 Ctron 拼出一个真能用的 Web 应用"。

全部底层能力已在库（本设计零语言/框架扩展，纯装配）：

| 能力 | 载体 |
|---|---|
| 密码散列 | `std/crypto.pbkdf2_sha256` + `bytes_to_hex` |
| 盐/会话随机 | `std/rand.rng_fresh/rng_range` |
| 签名令牌 | `http/frm/auth.auth_tok_issue/auth_tok_verify`（HMAC-SHA256，带 exp，恒时比较） |
| Cookie | `auth_cookie_get/auth_cookie_set`（HttpOnly+SameSite=Lax 随面） |
| 表单 | `http/form.form_parse/form_key_at/form_val_at`（urlencoded） |
| HTML 安全 | `http/frm/html.ht_esc/ht_page`（五实体转义，XSS 安全默认） |
| 持久化 | `fs_write/fs_exists/fs_delete` 内建 + `std/fs.read_or` |
| 墙钟 | 内建 `now_ms()`（`std/time.now_iso_utc` 供 created 时间戳） |
| JSON 转义 | `http/frm/openapi.oa_json_escape`（NDJSON 写面复用） |

## 2. 方案对比（三选一，选 A）

- **A（选定）服务端渲染 HTML + 文件持久化 + 开放注册**：浏览器即验收面，"完整应用"说服力
  最强；零外部依赖开箱即跑；与 todo_api 教学面互补。
- B 纯 JSON API 加厚 todo_api：与现有示例高度重叠，浏览器不可用，"应用感"弱。
- C HTML+JSON 双面：工作量最大，JSON 面价值被 todo_api 覆盖，YAGNI 砍。

## 3. 包结构与模块契约

`examples/todo_app/`，包名 `todo_app`（多文件包形态，先例 `tests/modules/use_ok`）：

```
examples/todo_app/
  Ctron.ctcl          # pkg { name = "todo_app" }
  run.sh              # 两相位 e2e（nc 探针，todo_api 同款骨架）
  data/               # 运行期生成（gitignore；run.sh 用 mktemp）
  src/
    store.ct          # 存储与账号面（无 HTTP 知识）
    views.ct          # HTML 视图层（无存储知识，纯函数 Str→Str）
    main.ct           # main + HTTP 循环 + 路由 + 会话接线（保持小，见下）
```

**主文件 20480B emit 上限对策**（c-rust-divergences §emit 直发主文件确定性 SIGKILL）：
主文件发射路径产物 ≥20KiB 恒 SIGKILL，而依赖路径安好——故 `main.ct` 只留 HTTP 循环与
路由分发（瘦面），字符串拼装重头戏（views）与存储逻辑（store）全部走依赖模块。

### 3.1 store.ct（存储与账号面）

```ctron
pub struct User  { uid, salt, hash: Str; iters: I32; created: Str }
pub struct Todo  { id: I64; uid, title: Str; done: Bool; created: Str }
pub struct Store { users: List[User]; todos: List[Todo]; next_id: I64 }

pub fn store_new() -> Store
pub fn store_load(dir: Str) -> Store            // read_or users.ndjson/todos.ndjson；坏行跳过
pub fn store_save(dir: Str, s: &Store)          // 全量重写两文件（量级=演示，够用）
pub fn pw_hash(pass: Str, salt_hex: Str, iters: I32) -> Str    // pbkdf2_sha256→hex
pub fn salt_new() -> Str                        // 16B 随机 → 32 hex
pub fn uid_ok(s: Str) -> Bool                   // 与 auth au_uid_ok 同域：[A-Za-z0-9_-]{1,64}，注册再加 ≤32
pub fn user_add(&Store, uid, pass, iters) -> I64   // 0=ok 1=uid 坏 2=重名 3=pass 短(<6)
pub fn user_auth(&Store, uid, pass) -> Bool     // pbkdf2 重算 == 比对
pub fn todos_of(&Store, uid) -> List[Todo]      // 按用户过滤，id 升序
pub fn todo_add(&Store, uid, title) -> I64      // 0=ok 1=title 空/超长(≤512)
pub fn todo_toggle(&Store, uid, id) -> Bool     // 只翻属于 uid 的行
pub fn todo_delete(&Store, uid, id) -> Bool
```

test 块（`ctc test`，解释口径，低 iters=8）：密码往返/错密拒/重名拒/uid 域拒/todo
增删翻只作用于本人/NDJSON 落盘重载往返。

### 3.2 views.ct（HTML 视图层）

```ctron
pub fn view_login(err: Str) -> Str        // POST /login 表单；err 非空时红字横幅（ht_esc 后入）
pub fn view_register(err: Str) -> Str     // POST /register
pub fn view_app(uid: Str, items: List[Todo]) -> Str  // 加列表+逐行 toggle/delete 表单+登出
pub fn view_404() -> Str
```

纪律：一切用户可控文本（uid、title、err 回显）过 `ht_esc`；表单全部 POST + 303 重定向
（POST-Redirect-GET）；页面骨架用 `ht_page`。CSRF 令牌不在 v1（Cookie SameSite=Lax
为示例级缓解），登记志向。

### 3.3 main.ct（HTTP 面接线）

todo_api 骨架（net_tcp_listen/accept → http_parse_head → CL 读体 → 串行一连接一请求，
Connection: close）。会话口径：`Cookie: sid=<tok>` → `auth_tok_verify(key, tok, now_ms()/1000)`，
uid 取 `tv_uid`。TTL 7 天（604800s）。

| 路由 | 语义 |
|---|---|
| GET / | 302 → /app（已登录）或 /login |
| GET /login, POST /login | 登录页/校验→303 /app + Set-Cookie sid；错密 200 页内横幅 |
| GET /register, POST /register | 注册页/建号（user_add 码分支回横幅）→303 /login |
| POST /logout | 清 sid（Max-Age=0）→303 /login |
| GET /app | 会话必需，否则 302 /login；view_app 渲染本人 todos |
| POST /app/add, /app/toggle/:id, /app/delete/:id | 会话必需；变更即 store_save；303 /app |
| GET /health | JSON {ok,served,users,todos} |
| GET /metrics | Prometheus 文本：http_total{code} counter + todo_users/todo_items gauge |
| POST /__shutdown | 排空停机（演示/测试便利，todo_api 同款） |

env：`TODO_APP_PORT`(8092)/`TODO_APP_KEY`(dev 缺省)/`TODO_APP_DATA`(缺省 ./data)/
`TODO_APP_ITERS`(2000)。监听 127.0.0.1。

## 4. 持久化格式（NDJSON，`data/` 下）

```
users.ndjson  {"uid":"alice","salt":"<32hex>","hash":"<64hex>","iters":2000,"created":"<iso>"}
todos.ndjson  {"id":1,"uid":"alice","title":"alpha","done":false,"created":"<iso>"}
```

读：`read_or(path, "")` + 手工按 `\n` 分行 + `body_json` 逐行解（坏行跳过）。
写：全量重写 `fs_write`。登记志向：原子写（tmp+rename）、文件锁（示例单进程串行无需）。

## 5. e2e 验收门（run.sh，两相位）

相位一（全链）：注册 alice→登录取 sid→/app 空态→加 alpha/beta→双现→toggle alpha→
删 beta→登出→/app 再访 302→错密登录拒→重复注册拒→bob 注册登录**看不到 alice 的条目**
（隔离门）。
相位二（持久化）：kill 后同 data 目录重启→再登录→alpha 在册且 done 态保持。

门：① `ctc test src/store.ct` 全绿 ② run.sh 两相位全绿 ③ `ctc fmt --check` 净。
收官加真浏览器烟测（注册/登录/增/翻/删截图取证）。

## 6. 实施次序与已登记风险

第一步为**骨架探针**（证伪优先，三文件空壳 emit+run）：①包内 `use todo_app.store` 解析
②struct/List 跨模块可变传递的发射口径（gui 泳道有索引复合赋值发射拒前科；不通则降级
函数式返回新 Store 或 Box 包裹，公共行为不变）③主文件 emit 限额实测。探针绿后才写满
实现：store.ct（含 test）→ views.ct → main.ct → run.sh，逐片验证逐片绿。

## 7. 非目标（YAGNI 登记志向）

CSRF 令牌接线、编辑/排序/截止日、并发连接（串行一连接一请求沿 todo_api）、原子写、
TLS、openapi.json 面（教学面归 todo_api）、密码改档。
