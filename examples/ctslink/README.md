# ctslink —— 自托管短链示例(web.* 域包第二活示例)

设计全案:`docs/superpowers/specs/2026-09-29-ctslink-design.md`(下称设计稿)。
模板:`examples/web_todo`(结构/纪律逐条镜像);与 todo_api(JWT/JSON)互补的
**浏览器形态**:表单/cookie 会话/服务端渲染/302/公开-私有面分离/双存储。

## 跑法(run.sh 三段;语义门硬,原生臂状态如实印)

```sh
sh examples/ctslink/run.sh
# 或分步(interp 门,web_todo 同款):
sh compiler/ctc.sh check examples/ctslink/src/main.ct
sh compiler/ctc.sh examples/ctslink/src/main.ct   # 裸跑即测试(mw 会话族 CPU 重,分钟级)
```

裸跑四组 test_call 语义门:①落地/健康/表单页/未登录拦截/csrf 写门 403/404 页;
②注册→登录(错密/未知名统一 302 防枚举)→建链→公开 302→flash 读即清→统计
(点击 2/建时)→删除→注销;③属主校验(他者统计/删除 404 与不存在同形,条目不受扰);
④碰撞重试(fake 熵预占首码,≤3 跳位)/输入负例(坏名/坏口令/坏 scheme)/
会话过期/篡改拒。细粒度负例语义由 web/router.ct、web/mw.ct、web/guard.ct 单元
测试钉面,示例不重复。

## 真 serve(原生门)= 门外语义,现况在册

`src/serve_net.ct`:SrvIo 五件映射 net 垫片 + 原生熵(uuid_v4)+ env 存储注入。
**net extern 消费侧直 declare**(五件,&T 形):net/bind.ct 的 listen/accept
`out_fd: Box[Box64]` 被 v0.9「三 Box 收口」E4046 硬错,`use net` 门面整树不可载
(web_todo serve_net check 同红实证);&I64[] lane 视图与垫片 int64_t* 同 ABI,
符号名一致,链接零改——门面迁移落库后回改 `use net`(一行)。

```sh
compiler/bin/ctron-emit run examples/ctslink/src/serve_net.ct > /tmp/sl.c
cc -O1 -w -pthread -I net/c_src -o /tmp/sl.bin /tmp/sl.c \
   net/c_src/ctron_net.c db/c_src/ctron_dbpg.c db/c_src/ctron_dbredis.c db/c_src/ctron_entropy.c
CTS_LINK_PORT=8093 /tmp/sl.bin        # POST /__shutdown 排空退出
```

**emit 现况(BLOCKED,框架侧)**:发射臂对「泛型 struct 载荷」限标量/Str
(trans_ty.ct `ct_inst_encode`)——`Router { state: <struct> }` 字面量必触
`泛型 struct 载荷限标量/Str:u:App`;`compiler/bin/ctron-emit run
examples/web_todo/src/serve_net.ct` **同红同文**(2026-09-30 实测)——
web_todo README 的原生臂配方对现行编译器已失效,非本例回归。载荷面扩位
(struct/嵌套)挂框架志向;落地前原生冒烟(run.sh 第 [3] 段)随 emit 解封。

## 存储双源(pg.ct script/fd 同构口径)

- **语义门/默认 serve = 内存 store**:`Mutex[List[User]]/[List[Link]]` 行,与
  `schema.sql` 行一一对应(users.id/links.owner 即行键);删除 = 墓碑位
  (web_todo done 先例;code 占位不回收);计数 = `Mutex[List[Click]]`。
- **Redis 计数(原生臂 env 注入)**:`CTRON_REDIS_URL=redis://host:port` →
  单连 `sl:clicks:<code>` INCR/GET(fn 值字段注入,SrvIo 先例);连接/命令
  失败回 -1/0,**不拦跳转**(设计稿 §二)。未设 → 内存计数。
- **PG(原生臂 env 注入,v1 仅 DDL)**:`CTRON_PG_DSN` → SCRAM 连接 +
  schema.sql 幂等建表,结果仅日志;行存储仍内存——PG 行存取双源化挂后续
  (无 emitting trait,双源需逐 handler 分叉,不过度工程,登记志向)。
- App.fn 值字段(clk/rnd/cnt_incr/cnt_get)= §8-A5 注入纪律的存储面推广:
  interp 侧 fake 钟 + fake 熵 + 内存计数;原生侧真钟 + uuid_v4 熵 + Redis。

## 解析布局(web_todo 同构 + db)

包根六枚符号链 `std` `web` `http` `net`(目录)/ `net.ct` / **`db`**(ctslink
新增)指回仓根同名域包;2 段 use 经 `<包根>/<域>/*.ct` 命中,3+ 段
(`http.frm.html`)经 `<包根>/http/frm/html.ct` 命中;入口目录旁 `std/` 为
std 根(parse_pkg.ct 种子回落)。

## 设计稿 → 冻结面差异(实现如实记)

| 设计稿(意图) | 实际面 | 因由 |
|---|---|---|
| csrf_mw 入链(guard.ct) | 自组 `csrf_ssr_mw`(http.frm.csrf 原语,非旋转双提交) | guard csrf_mw 密封在响应侧轮换令牌,SSR 表单 handler 内渲染无法预知响应侧令牌——首访 POST 必 403 死锁(实测);JS 填充形态挂志向 |
| POST /register 等「PG INSERT」 | 内存 store,schema.sql DDL + 原生臂启动 DDL 应用 | 无 emitting trait,双源需逐 handler 分叉;语义门零 PG 依赖(设计稿 §四 同旨) |
| 「7 位 code uuid 熵源」 | `code_from(rnd(16))`:uuid_v4 字节→[a-z0-9] 映射(interp = fake 熵) | uuid_v4 真熵仅发射臂可跑(std/uuid.ct 头注;extern 无 interp 运行时) |
| `use net` serve 装配 | 消费侧 &T extern 直 declare 五件 | bind.ct Box 形参 E4046(v0.9);见上「真 serve」 |
| PBKDF2 iters=4096 恒定 | App.iters 装配注入(测试 4 / 生产 4096) | interp 裸跑门分钟级(会话族)+pbkdf2 O(iters)——4096 直入语义门不可行;存储形自带 iters 段,verify 按存值 |
| 登录「恒时比较」 | hex 定长 + 全位累积无早退比较 | std/crypto 无公开 ct_memcmp 面(frm/auth 内铸不外露);时序侧写登记审计 |

## 合并态 interp 实证(本任务探针 + 新登记)

1. **std.crypto 直 use ⊕ web.mw:check/裸跑全绿,E5020 未触发**(probe_dual
   探针,check OK decls=363)——mw/guard 头注「消费方再入即同叶双径」系保守
   警示:同名模块路径(std.crypto)双达不构成 loader 双径。口令哈希故直用,
   无垫片。
2. **E5030 私有名撞名面扩及「非 pub fn」**:guard.ct 私有 `csrf_nm`/`csrf_body`
   与入口同名 decl 撞合并态(本例改名 csrf_name/csrf_rej_body 避)——合并态
   撞名不止 pub 面(web_todo E5030 `text` 为 pub;私有 fn 同样入合并测试体
   作用域)。
3. **emit 泛型 struct 载荷限定**:见「真 serve」;web_todo 同红,原生 web 臂
   全树不可用。
4. **uuid_v4() 仅发射臂**(std/uuid.ct 头注实证):熵经 App.rnd fn 字段注入,
   interp fake(确定性,碰撞重试可测)/原生真熵双源。
5. web_todo 在册项复验:with/with_mut 闭包体一跳助手 fn、Mutex 写回 push/Index
   替换、泛型 fn 值引用禁(闭包适配)、fn 值字段先落局部再调(serve.ct 直调
   panic 口径;本例 h_code/h_stats/h_register/h_login 全按此形)。

## 测试口径备注

`web.mw` 合并态会话族(HMAC 验签)eval 期 CPU 重(web_todo 在册「分钟级」);
本例四组门每 dispatch 过 with_sessions + csrf + 限流,裸跑门比 web_todo 更重,
机器繁忙时数分钟级为常态。PBKDF2 iters 注入后,口令面在门内为真算(4 轮),
非 stub。
