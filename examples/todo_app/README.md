# todo_app —— 带登录的 Todo 管理应用(服务端方向完整应用示例)

浏览器即验收面的多用户 Todo 管理:注册/登录/登出(pbkdf2_sha256 真密码认证)、
Cookie 会话(HMAC 签名令牌,HttpOnly+SameSite=Lax)、每用户数据隔离、NDJSON 文件
持久化(重启不丢)、服务端渲染 HTML(ht_esc 全用户文本,XSS 安全默认)。

## 运行

    sh run.sh          # 两相位 e2e:全链(注册/登录/增删翻/隔离/登出) + 重启持久化
    # 或手工:
    mkdir -p data && TODO_APP_PORT=8092 <ctron-cc/binary> run src/main.ct  # 见红账

## 结构(星形单路径拆分;app 为唯一父,data/sess/views 互不依赖)

    src/main.ct   入口 shim(仅转发,见下"发射红账")
    src/app.ct    HTTP 装配:listen/accept、读请求、路由分发、Res 响应通道
    src/data.ct   存储+账号:NDJSON 行即内存形态、pbkdf2 密码面、平面 JSON 取值器
    src/sess.ct   会话:HMAC-SHA256 签名令牌 + Cookie 读写(frm/auth 的 sess-store
                  段会触发发射缺陷,故以 std.crypto 直实现同语义小面)
    src/views.ct  视图:登录/注册/列表/404 四页,纯 HTML 装配,零本地依赖

与 `todo_api` 分工:那是框架最小装配教学(JSON API + 假登录);本示例是完整应用
(真密码 + 会话 + 隔离 + 持久化 + 浏览器界面)。设计文档:
`docs/superpowers/specs/2026-09-26-todo-app-design.md`。

## 验收(2026-09-27)

`sh run.sh` 两相位 e2e **26/26 全绿**(注册/登录/重复与短密错密拒/Cookie 会话/
增删翻/多用户隔离/登出/metrics/重启持久化)。途中定案双层发射缺陷(字符串裸 `{`
lexer 毒 + fn 值形参野跳转),复盘见 `docs/c-rust-divergences.md`
「todo_app 发射爆炸+运行野跳双层案」节。

## 已登记志向(演示口径,见 spec §7)

CSRF 令牌接线(Cookie 同站 Lax 为缓解)、登出仅清客户端 cookie(无状态令牌不吊销)、
密码比对恒时化、原子写(tmp+rename)、并发连接、编辑/排序。
