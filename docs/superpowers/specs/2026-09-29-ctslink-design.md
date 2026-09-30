# ctslink —— 自托管短链服务(web.* 域包示例应用设计)

> 日期:2026-09-29 · 状态:设计稿 → 实现 · 目标:`examples/ctslink`
> 模板:`examples/web_todo`(框架首个活示例;结构/纪律逐条镜像)
> 定位:与 todo_api(JWT/JSON API 形态)互补的**浏览器形态**示例——表单/cookie 会话/服务端渲染页面/302/公开-私有面分离/双存储。

## 一、功能与路由

| 路由 | 面 | 功能 | 能力点 |
|---|---|---|---|
| GET / | 页 | 未登录→落地;已登录→仪表盘(列表+新建表单) | with_sessions 判别 + 视图 |
| GET /register, /login | 页 | 表单页(+错误 flash) | flash 读即清 |
| POST /register | 表单 | PBKDF2 入库 → 302 /login | std/crypto pbkdf2 + PG INSERT |
| POST /login | 表单 | 校验 → grant_session → 302 / | 恒时比较;失败不区分无用户/错密 |
| POST /logout | 表单 | drop_session → 302 / | 现成 |
| POST /links | 表单 | 登录态建短链 → 302 / | PG INSERT + code 碰撞重试 |
| GET /:code | **公开** | 302 真实地址 + Redis INCR 计数 | RESP2 + rate_limit + 404 页 |
| GET /links/:code/stats | 页 | 点击/创建时间(属主校验) | 行映射 + Redis GET |
| POST /links/:code/delete | 表单 | 删除(属主) | PG DELETE |
| GET /healthz | — | 健康 | — |

中间件:`with_sessions` + `csrf_mw` + `rate_limit_mw`(公开端点) + `sechdr`口径(若 mw 无现成则应用侧注) + `log_requests`。

## 二、存储

- PG:`users(id, name UNIQUE, pwhash, created)` / `links(id, owner, code UNIQUE, url, created)`;
  pwhash = `pbkdf2$<iters>$<salt_hex>$<hash_hex>`(std/crypto pbkdf2_sha256,iters=4096);
- Redis:`sl:clicks:<code>` INCR 计数(失败不拦跳转,v1);
- 连接:std/db 池(PG)+ 单连(Redis);DSN 环境变量。

## 三、安全语义

密码永不明文存;登录失败统一 302(防枚举);code 7 位 [a-z0-9] uuid 熵源碰撞重试;
url 仅 http/https;会话 Cookie HttpOnly+SameSite=Lax(frm/auth 同款);页面值全走转义面;
CR/LF 出域即拒(auth_cookie_set 既有);公开端点限流。

## 四、结构与纪律(镜像 web_todo,差异登记)

- `src/main.ct`:interp 净土(零 socket;test_call 语义门)+ 业务 + 视图;
- `src/serve_net.ct`:SrvIo 五件映射 net.*(L6/L7 原生臂,POST /__shutdown);
- `net.ct` + `Ctron.ctcl` + 包根符号链 std/web/http/net/**db**(ctslink 新增 db);
- 语言纪律:字面量禁 {(concat "\{"+"}")、无 ;、显式 TypeArgs、List 收集+join、泛型 fn 不作值引用;
- use 严格树探针纪律:std.crypto 直 use 与 web.mw(→http.frm.auth→crypto)双径——
  E5020 风险按探针实测;撞则别名垫片(ctron_dbpg_entropy 先例)或经 mw 面穿透;
- 视图:ht_esc/ht_text/ht_el/ht_page(web_todo 同款;web.view 与 mw 撞名 E5030 在册,el 待框架消解)。

## 五、能力审计(实现期间逐项核,产出随交付)

预登记已知项(实现中验证+补新):
1. web.view 与 web.mw 合并态撞名(text)→ el builder 不可与 mw 共存(E5030)——框架面消解志向;
2. timeout_ms 为透传校验语义面,真超时挂并发波——能力缺失登记;
3. std/enc b64 C8 约束(b64url 需 lane-b64 自出,ws/JWT 先例);
4. SrvIo read 返回 lane_str ASCII 面——**请求体非 ASCII(中文表单值)读面缺失**,待 Bytes 语言项或 utf8 读桥;v1 用户名/URL 天然 ASCII,标题类中文字段登记受限;
5. loader 严格树(E5020/E5030)对 crypto 双径的实测结论;
6. utf8_enc interp U+FFFD(emit 正确)——页面中文渲染面双臂差异。

## 六、验收

- tests 面或 examples 自带 run:语义门(test_call)全绿(注册/登录/登出/建链/跳转 302/统计/属主校验/未登录拦截);
- 原生臂冒烟:serve_net 编译 + curl 走通 register→login→create→redirect→stats(手动/脚本);
- examples/web_todo 既有门零扰动。
