# web_todo —— 迷你 todo 示例(框架首个活示例)

spec §5「体验样例」的落地:全部业务一个文件(`src/main.ct`),框架零业务泄漏。
设计全案:`docs/superpowers/specs/2026-09-27-web-framework-design.md`(下称 spec)。

## 跑法

```sh
# 语义门(check)+ 行为裸跑(test 块;零 socket,spec §4.7 testkit 口径)
sh compiler/ctc.sh check examples/web_todo/src/main.ct
sh compiler/ctc.sh examples/web_todo/src/main.ct
```

裸跑即测试:未登录 GET /app 守卫 303 + 登录失败页;登录全链(登录表单 POST →
h_login 真 grant 臂签发 Set-Cookie → dispatch_h 携 Cookie 头)列表/添加/删除;
flash 读即清;过期/篡改 303;test_call 口恒等。404/405/Allow/密钥不合等路由
与会话细粒度负例语义由 web/router.ct、web/mw.ct 单元测试钉面,示例不重复。

## 真 serve(交互式 e2e)= 原生门(L6/L7)

`src/serve_net.ct` 是真 net 臂入口(SrvIo 五件映射 net.*,task-3-report 装配例
同形)。`use net` 会把 W8052 ×2(net_tcp_listen/accept 的 out_fd extern 形参非
C-ABI)拉进合并态,interp 的 check/裸跑门全红(实测在册,见下)——故 serve 调用
从 main.ct 拆出,main.ct 保持 interp 净土。原生臂(门外语义):

```sh
compiler/bin/ctron-emit run examples/web_todo/src/serve_net.ct > /tmp/wt.c
cc -O1 -w -pthread -I net/c_src -o /tmp/wt.bin /tmp/wt.c net/c_src/ctron_net.c
/tmp/wt.bin        # 127.0.0.1:8091;POST /__shutdown 排空退出
```

浏览器/curl 交互式 e2e 挂 L6/L7 原生翻账(在册);`check src/serve_net.ct` 现况
= 仅 W8052 ×2(装配面其余全解析,`use main.{app_of, full_router}` 跨文件命中,
清单 caps 已含 `net.listen`)。

## 解析布局(消费域包的示例形态)

包根四枚符号链 `std` `web` `http` `net`(目录)/ `net.ct`(域门面文件)指回仓根
同名域包:入口目录旁 `std/` 为 std 根(parse_pkg.ct 种子回落),其父(包根)即
域根,`use web.router` 一类 2 段 use 经 `<包根>/web/router.ct` 命中,3+ 段 use
(`http.frm.html`)经 `<包根>/http/frm/html.ct` 命中;深层模块的 use 一律以入口
目录为锚传递解析。

## spec §5 → 冻结面差异(实现如实记)

| spec §5(设计意图) | 实际面 | 因由 |
|---|---|---|
| `use web.{ router, serve, html, redirect }` | 分文件 `web.core` / `web.router` / `web.mw`(视图经 `http.frm.html`) | 域包分文件实际面 |
| `serve("127.0.0.1:8091", r)` 简签 | `serve_with[S](io: SrvIo, host, port, r)` + 调用点 net 接线十余行 | interp-net 死,依赖倒置(task-3) |
| `grant_session(...)` 链式 4 参 | `grant_session(r, key, uid, ttl, now)` 5 参 | §8-A5 时钟注入优先(mw.ct 头注) |
| `with_sessions(app.key)` 1 参工厂 | `with_sessions(key, now)` 2 参 | 同上 |
| `Mutex.with` 闭包体内联 while 累加 | 临界区体提成助手 fn(`rows_of`),闭包一跳 | E3070 两遍式 lint 全文件名交集,内联累加形必触 |
| `todo_remove(l, id, uid)` 就地重建 | 墓碑行 push(`done` 墓碑,渲染侧过滤) | with_mut 闭包内对形参整体重绑不写回 Mutex(interp 实证);push 是唯一实证写回方法位 |
| 视图手拼(ht_esc 手写转义) | `ht_el`/`ht_text` 显式转义装配(动态值位零裸插值) | web.view el 族与 web.mw 合并态 E5030 撞名,见下 |

## 合并态 interp 实证(本任务探针 + 前会话在册)

本任务探针(最小复现在册 task-4-report):

1. **`use web.view` ⊕ `use web.mw` 不可共存(合并态 E5030)**:两模块 "Test 恒随"
   test 体各拖同名 `text` decl 入合并态(web.core 的 Resp 构造器 vs el 值位)——
   入口 use 面**即便完全不引 `text`** 亦 E5030;反向纪律:入口须在 use 面保留
   `web.core` 的 `text`(web.router/web.mw 的合并测试体引用它,缺之 E2020 ×3)。
   示例因此按 §4.8「B 与手拼并存」走 `http.frm.html` 的 `ht_el`/`ht_text`
   显式转义面;el 族结构性转义 dogfood 待框架面消解同名(改名属跨任务面变,
   登记志向,M-T5w-1 同根)。
2. **`now_ms()` 引导 interp 可执行**(探针 rc=0)——登录全链测试里 h_login 真
   grant 臂用真钟签发,配固定 fake 钟(clk_fix=1000)验签恒过(exp = 真钟+ttl
   远晚于 fake 钟),零 socket 走通「POST /login → Set-Cookie → 会话请求」。
3. 前会话探针在册(本任务代码按其约束成形,双门全绿背书其充分性,未独立复跑):
   with/with_mut 回调体内禁对回调形参做字段成员访问/二次成员链——合并态 sem 对
   `Mutex[List[T]]` 回调形参类型不可解析,回调直传类型完备助手 fn 即安(rows_of
   形);App 状态字段族维持 spec §5 三字段,加员(第四 Mutex 字段)panic
   `field:<名>`。
4. `check src/serve_net.ct` = 仅 W8052 ×2(listen/accept 的 out_fd),其余全解析
   ——`use net` 连 check 门都红(interp-net 死),main.ct 与 serve_net.ct 的
   文件拆分是 interp 净土纪律的落地形。

## 测试口径备注

`web.mw` 合并态会话族(签名令牌 HMAC)eval 期 CPU 重(裸跑门分钟级,机器繁忙
时更久;内存采样未超 GB 量级)。语义门所需的登录态 dispatch(每次过
with_sessions = 一次验签)按 brief 三门保留:未登录 303 / 登录全链 / flash,
负例(过期/篡改/注销)各一;细粒度负例语义由 web/mw.ct 单元测试钉面,示例不重复。
