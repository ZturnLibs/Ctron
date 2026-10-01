# 编译泳道移交任务单——服务器/框架泳道实测缺口全谱(2026-09-30)

> 来源:服务器泳道 S0→P8 各波 + web_todo/ctslink 两示例实测;逐项带症状/根因(file:line)/复现/影响面/修法建议。
> 登记底册:docs/c-rust-divergences.md (f)/(g)/(h)/(i) 各节——本单是**行动视图**,修复后请回写底册销账。
> 优先级:**P0** = 已交付示例的原生臂被堵(用户可见);**P1** = 信任面/正确性;**P2** = 框架质量;**P3** = 登记在册的边角。

---

## P0-A emit 泛型 struct 载荷限定 → web.* 原生臂全家不可用

- **症状**:泛型 struct 字面量携 struct 值 → `panic("emit:泛型 struct 载荷限标量/Str:u:App")`,emit 臂确定性失败。
- **根因**:`compiler/src/trans_ty.ct:1393-1421`(`ct_inst_encode`:型参载荷仅 i/s/b/6 四码)。
- **复现**(2 行):`pub struct Box2[S] { let v: S }` + `Box2 { v: App { ... } }`(App 为任意值 struct)。
- **影响面**:`Router { state: <应用状态 struct> }` 是 web.* 的核心形态 → **web_todo 与 ctslink 两示例原生臂全挂**;任何泛型容器携 struct 均触。
- **修法**:载荷扩位——struct 实参装箱(或栈取址)后载荷放指针,镜像既有的 Box/`ct_clop` 双形先例;注意与 32 位 Result 载荷(P1-1)分立:那是绑定 cast,这是槽内容纳面。

## P0-B net.ct 门面整树不可载(bind.ct Box[Box64] × E4046)

- **症状**:`use net` 即 check/emit 双臂硬错 E4046(sem_main.ct:208,"三 Box 收口")——tcp_listen/accept/sockname/connect/udp_*/unix_* 全族出参。
- **根因**:`net/bind.ct` 出参 `Box[Box64]` 形撞 v0.9 E4046(HEAD 已提交行为)。
- **影响面**:todo_api run.sh 原生段、tests/net 原生路径、ctslink serve 臂**全部悬挂**;README「仅 W8052×2」口径过期。
- **修法**:bind.ct 出参迁 `&T`(p<ec> 过界已放行,ABI 同,消费侧 `&T` 直 declare 即可链——ctslink serve_net 现行形,符号名对齐 ctron_net.c 零改);**同族潜伏**:std/db/c_src/ctron_entropy.c 的 lane 形参(P5-B 登记)随本单一并核。
- **验证**:tests/net 双矩阵原生段 + todo_api run.sh [2]/[3] 段 + ctslink run.sh [2]/[3] 段三处复跑。

---

## P1 信任面/正确性

| # | 项 | 根因/复现 | 修法 |
|---|---|---|---|
| P1-1 | **Result/Option match 绑定 32 位截断**(数值载荷跨模块必坏);F64 载荷另系槽类型缺陷(整型槽不可携 double) | `compiler/src/trans_stmt.ct:996` `int32_t t_v = (int32_t)<m>.v`(槽本体 `ct_i` 已宽,driver_emit.ct:102-103);Rust 线 trans.rs:2841 用 `__int128`/`ct_i` 不受影响 | 绑定按声明宽度发射;**修时勿按 (h) 旧处方扩槽**(槽已宽,扩槽不治) |
| P1-2 | **c6sub 双负符号翻转**:负减负分支 `c6sub(|a|,|b|)` 应为 `c6sub(|b|,|a|)` | eval_val.ct:1068;复现 `-1 - (-86400)` interp 得 -86399 | 一行换序;std/json date.ct、time.ct 已结构规避 |
| P1-3 | **按值域定宽乘法带**:2^28–2^31 带 interp panic(emit 恒正确)——std/http 四处、date/time、ws len64 均已 C10 规避,新数值代码持续踩 | eval_val.ct 乘法界检;fuzz seed2 实证(divergences (i) 有复现带) | 根治 = interp 算术口径对齐 emit(检算/宽域);过渡期 divergences (i) 维持 |
| P1-4 | **utf8_enc interp 恒返 U+FFFD**(emit 正确)——非 ASCII 文本双臂劈叉 | eval_call.ct:271 / driver_emit.ct:90 / parse_node.ct:42(P5-C 精确复现三处) | interp 侧修;修前各面 ASCII fail-closed + hex/bytes 精确面维持 |
| P1-5 | **F64 interp 四实例** + emit F64 to_string 乱值:≥2^63 整值乘 panic / 19 位字面量 panic / e 形字面量误析 / >2^53 不舍入 | P5-A 实测(divergences (h) F64 分立条) | interp/emit 各自对齐 IEEE;修前 f64 量级窗 \|v\|≤I64_MAX 口径维持 |
| P1-6 | **CL 19 位字面量尾位 add 溢出**(parse.ct:269 邻域,`...5808` add 溢出 panic 先于 err_cl);chunk 邻域已由 2^59 乘前门关闭(86eac25) | P4 终审实鉴定,与 P1-3 同根不同形 | 随 P1-3 根治;过渡期依赖 panic 即 fail-closed |

## P2 框架质量(web.* 高阶面)

| # | 项 | 说明 | 修法 |
|---|---|---|---|
| P2-1 | **csrf_seal 响应侧轮换 × SSR 表单死锁**:每安全响应新签令牌,SSR handler 渲染表单时无法预知响应侧令牌 → 首访 POST 必 403 | ctslink 实测(逐头核查 csrf_seal 调用序);双提交+HMAC 原语本身可用 | web.guard 增非旋转 seed-on-miss 第二形态(ctslink csrf_ssr_mw 即候选实现),或 frm 提供 handler 侧预签 API |
| P2-2 | **E5030 合并态作用域含模块私有 fn**:guard.ct 私有 `csrf_nm`/`csrf_body` 与入口同名(非导入)即撞 | ctslink main.ct check 首跑 E5030:csrf_nm | loader:合并测试体/私有名作用域隔离;过渡 = 示例侧命名前缀纪律 |
| P2-3 | **std/enc b64 无 b64url 面 + C8 可印约束**(WS accept-key/JWT 段均需):ws.ct/JWT 各自 lane-b64 自出(三处重复) | P4-C/P6-C 实测 | std/enc 增 b64url(标准 RFC 4648 字母表,字节道进出) |
| P2-4 | **ctron-fmt 把 `scope { \|sc\|` 拆行后 emit 前端挂死**(fmt×emit 不一致) | P6-A 实测;示例以单行 scope 规避 + fmt-check 豁免 | fmt 保持单行或 emit 容多行;先修 fmt 判定 |
| P2-5 | **Sender/Receiver 字段化/参数化 emit 截断** ⇒ 已裁定 std 门面禁持通道半端(组合层接线) | P5-E;pool = 纯核+Channel 组合层 | 字段化支持落地后解除禁令,pool 可回收为字面形态 |
| P2-6 | **timeout_ms 为透传校验语义面**(真超时挂并发波,§8.2 v1 阻塞串行档) | mw.ct 头注在册;调用点零改的闭包换形已预留 | 并发 RT 落地波闭包内换真实现 |
| P2-7 | **web.view ⊕ web.mw 合并态撞名(`text`)**:el 结构转义 builder 与 mw 不可共存(E5030)——结构性转义 builder 被 Duck | web_todo task-4 探针在册;两示例均绕行 ht_esc | 撞名消解(改名或作用域隔离)后 el 系复位,XSS-变类型全面可用 |

## P3 登记在册(边角/志向,不阻塞)

- interp 堆不回收(每编译块 ~1.5GB 等效量化;fuzz 实测 ~2.4MB/case 增长)——arena 治理族(有在册前案);
- fn 值形参 ct_clop 野跳转白名单缺口(f5fb824 在册志向 sem 拒绝);泛型 fn 不作值引用;泛型调用显式 TypeArgs(E2060)维持;
- **emit 缺口族(逐项有绿改写,落地后逐项解除)**:while 体 Drop 局部禁 / 闭包 return fn 禁 Drop 局部 / spawn 闭包禁 assert·panic 语句 / fn 值调用裸尾返回缺 cast / 嵌套闭包深度计号重名 / 未注解 let 闭包形参 I32 shim / for 通配 / 闭包内 match / 裸 with / 捕获槽写回 / lane .len=字面量槽数 / byte 内建拒 I64 标签 / List[I64] 字段 emit 坏 / Option[struct]·List[struct] 错型 / match-on-Result emit 不支持;
- **interp 面**:fn 体 const 引用 unbound;入口 use 面 E2020 要求(合并测试体引用 `text` 需入口 use);未用结果绑定毒化消费调用点(静默 rc=1);live-socket 计时探针偶红(coro_hybrid workers=4);
- **use 面**:use 路径含 `-` 字符宿主 segv(改名规避在案);use 多段路径 ctc.sh emit 驱动 E2020 `./db.ct`(W1 库根布局 seam,并行在制);loader 域根符号链示例形 W8902 不自动命中(符号链为现行落地形);E5020「严格树」保守口径建议松绑(P5-B 实证:同名模块路径双达不构成,ctslink probe_dual decls=363/521 复证);
- KDF/时钟纪律(非缺陷,推广建议):KDF 强度一律装配注入(§8-A5);时钟注入 unix 秒约定维持。

---

## 已销账(核对用,勿重复立案)

forget_fd/close 重排(P4-A)、chunked 2^59 乘前门(86eac25)、c10k 泄漏门换血(ce43e83)、WS 三 MUST(2d7e47d)、*;q=0(82328e5)、csrf 双径 E5020 保守口径(P5-B 实测推翻)、(h) 处方改写(绑定 cast,见 P1-1)、SCRAM Z 门 phase==3(277f235)、pool Z('T') 陷阱+drain keep(b1e0592)、fd 半包判别(67acde6)。

## 移交执行建议

1. **P0-A + P0-B 同批做**(同一验证面:web_todo/ctslink/todo_api/tests-net 四处原生臂一次复活,验证脚本现成——三例 run.sh [2]/[3] 段);
2. P1-1 与 P0-A 触及相邻发射面(trans_stmt/trans_ty),建议同一会话处理避免二次冲突;P1-2/P1-5 是 interp 一行级,随手批;
3. 每项修复后:回写 docs/c-rust-divergences.md 销账 + 对应示例 run.sh 复跑(验证清单见 P0-B);
4. 本单未含服务器泳道自身的 P9 志向档(io_uring/h2 等),彼处与编译项无依赖耦合。
