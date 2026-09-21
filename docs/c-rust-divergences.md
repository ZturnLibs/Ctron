# C 版与 Rust 版语义/能力分歧清单

> 来源:2026-09-07 性能对比(见 `tools/bench.py`)过程中的实测发现。
> 口径:双实现共享 `docs/spec` 规范与 `tests/` 语料;分歧分两类——**待修项**(某版
> 违反规范或存在解析缺陷,须对齐)与**行为差异**(能力面/上限不同,记录在案,是否
> 收敛由规格所有者裁决)。

## 待修项

### T1. 解析器:`x[expr] {` 被误判为泛型实参(双解析器同款)——已修复

`if` 条件等位置的**下标访问紧跟块起始**时,`]`+`{` 前瞻启发式(`bracket_followed_
by_call_or_lit`)把它当成 TypeArgs+结构字面量,产生 E1001:

```ct
if a[j] > a[j + 1] { ... }   // 修复前:C、Rust 解析器均 E1001
```

**勘误**:初版档案记"Rust 版正常"系误导——Rust `run` 对解析错误静默 exit 0
(见 D5),早期探针据此误判。实为双解析器同款缺陷。

**修复(2026-09-07)**:双版 `bracket_followed_by_call_or_lit` 收紧为仅 `](` 触发
(`List[I32](…)`/`Atomic[I32](0)` 等构造调用);`]/{` 不再触发——`Pair[T] { }`
形态双版语法皆不存在,`x[expr] {` 只能是下标后跟块。回归锚 `tests/01g_index_
block.ct`(双版 check 零诊断/run 一致/ast_diff 逐字节一致;campaign 52/52)。

### T2. `+` 字符串拼接:C 执行层接受,双版 check 层拒绝

```ct
var s = ""
s = s + "x"        // C check:E2010 拒;C rt/trans:接受;Rust rt:panic "算术需要数值"
```

**层面对齐矩阵(2026-09-07 实测)**:拒绝阵营 = C check + Rust rt;接受阵营 = C rt +
C trans。C 版三层不自洽(check 拒、执行层收)。

**成本证据(关键)**:`selfhosted/*.ct`(ev_num/cc 等树打印代码)大量使用
`out = out + ", "` 形态,跑在 C 种子解释器上——收紧 C rt 将打断自举固定点,
需千级位点改写为插值。

修法三选一,待规格所有者裁决:
(a) C 收紧(对齐规格)= 自举线大改写,**代价最高,不建议**;
(b) 规格修订接纳 `+` 拼接 → Rust rt/trans 各补一处 binop 分支,C check 同步放开
    E2010 的 Str 分支(修复三层不自洽)——**证据倾向此项**;
(c) 维持现状,文档化为「check=规格严格档,执行层=宽容档」,接受双面性。

**状态:规格已裁决方向 (b),2026-09-08 落地**(规格 §4.5 修订 + C sem 放行 +
Rust interp/check/trans 对齐;回归锚 `tests/01h_str_plus.ct` 四路逐字一致)。
**已收敛(2026-09-08)**:T2 落地后追查,连修四个 Rust interp 语义缺陷——
① `&&` 不短路(LHS 假仍求值 RHS);② UInt/Int 混合算术兜底拒绝(补 i128 中介
分支);③ Str `[]` 下标静默返回 Void(收紧为 panic「索引目标非数组」,对齐 C rt);
④ `.len`/`char_len` 返回 UInt(对齐 C v_int 有符号;byte_at/byte_slice/read_bytes
参数同步强化)。另补 `List[T]()` 构造器与标量 `to_string`。
**里程碑:cc.ct 全管道双种子逐字一致**——input_small/pay(双版同点 abort)+
input_cc/cc2/cc3(21/8/11 行输出 + 退出码),五个输入 diff 全零。
范畴:本项为「自举代码 × Rust interp 域验收」首批成果;后续随自举泳道
cc.ct 演进持续跑 `genmod` 矩阵即可。

## 行为差异(记录在案)

### D1. CLI 运行入口 —— 已对齐(2026-09-08)

Rust 版补齐:`interp::run_main`(const/static 预求值 + main 体执行,EarlyReturn
→ 退出码)+ lib `run_main_file`(解析诊断非空 → Err)+ CLI `run` main 优先分派
(有 fn main 跑 main,否则 test 块;退出码 = main 返回值)。
随附解释器内建补齐:println/print(fmt_val 同格式)、read_file(Option[Str])/
read_line/read_bytes/flush_out、byte_at/byte_slice、`List[T]()` 构造器。
残余:`cc.ct` 全管道在 Rust interp 上仍被 T2 阻塞(见上)。

### D5. Rust `run`/`test` 对解析错误静默成功

文件存在解析诊断时,Rust `run` 不报错、无输出、exit 0(其 `run_test_file` 内部
解析失败 → 测试表为空 → 失败计数 0);C `run` 打印诊断并 exit 1。曾致 T1 的
"Rust 版正确"误判——排查 Rust 侧行为时务必用 `ctron check` 而非 `run` 验证解析。

### D2. 解释器步数上限:机制已对齐(CTRON_MAX_STEPS),默认值有意保留差异

双版解释器支持同名环境变量 **`CTRON_MAX_STEPS`**(N = 步上限,超限 panic
"instruction limit exceeded (可能的无限循环)";`0` = 无限;未设/非法 = 各自默认):

| 实现 | 默认 | 理由 |
|---|---|---|
| Rust interp | 2_000_000 | 防无限循环护栏(原行为保留) |
| C rt | 无限(0) | 自举负载(cc.ct 解释自身)远超 2M 步,护栏默认会打断阶梯 |

默认值统一待规格所有者在工具链章节(B 口径)定案;在此之前以本档为准。
2026-09-07 验证:300K 迭代负载 Rust 默认触发/`0` 档解锁,C 全档通过。

### D3. 转译后端形态支持面

- `fn main` 文件:双版均完整发射(2026-09-08 Rust 补齐:无 test 且有 main 时
  发射 `int main(void) { alarm(20); return (int)ctn_main(); }`;main 体本就随
  用户 fn 发射为 ctn_main)。
- `println`/`print` 发射:双版均支持(2026-09-08 Rust 补齐 ct_print_* 家族,
  格式面 = 解释器 fmt_val:浮点整值 %.1f 否则 %g)。
- `09_simd`:C 后端已覆盖(34/34);Rust 后端未覆盖(33/34)。

### D4. 检查器档案

C 版 sem 的 E2010/E3040 等为 Rust `check.rs` 的保守子集(推导不出不报),检查面
更小——性能对比中 check 阶段 C 快 2.6× 有一部分源于此,不能全记为实现优势。

## 服务器面(2026-09-20)

服务器泳道(S0+P1,`tests/net/` 全绿)执行中实测的跨面事实登记;细节见
`docs/superpowers/plans/2026-09-20-server-s0-p1.md` 执行记录(Task 4–9)。

### (a) FFI 边界事实(Task 4 探针 + Task 5–7 垫片/ctecho 实证)

- **可用签名白名单**:标量 / `Str` / `&I64[]`·`&I32[]`(视图复合字面量,零拷贝)/ `Box[struct]`(按值堆胞指针,`b->v` 与 `b.v` 互为镜像)。垫片与用户 extern 均按此选型。
- **裸 `T[N]` extern 形参静默发射 `int32_t`**(ct_ctype 无 `a<N><码>` 映射)——不可作边界签名,且无诊断,静默错型。
- **未初始化 `var buf: T[N]` 致 ctron-emit 段错误(exit 139)**——编译器缺陷,夹具一律带初始化器规避(转编译泳道在册)。
- **`&Struct` 形参发射 `t_&`(非法 C)**——struct 按引用出参只走 `Box[struct]`(修正 Task 4 初记的 int32_t——形参位 int32_t、实参位 t_&,两说各为一半)。
- **`Result[Struct, _]` 的 Ok 成员访问发射即 panic**;**trait 方法调用不发射**;**class 字面量不可发射** → 落 struct。
- **spawn 仅标量过界且须有返回值**;**while 体内禁 Drop 局部**(emission 面限制,垫片以显式 close 规避)。

### (b) C 宿主 caps 键集已收账(M-T3-1)

`compiler-c/src/pkg.c` check_caps 的 E4010 键集原硬编码 `{fs,time}`,与自举侧
`compiler/src/parse_pkg.ct`(Task 3 已放宽为 `{fs,time,net,db}`)不一致——
宿主对 net/db 触网包静默放行(caps_net 宿主侧红)。2026-09-20 同口径放宽,
modules 段宿主pkg 8/10 → 9/10,caps_net 双线绿;R 线 `check.rs` 的
`caps_used` 本就按 `cap_key_by_def` 通用收集(无硬编码),三线键集口径自此一致。

### (c) ctecho 解释口径不支持

`examples/ctecho`(及 tests/net 全套)走 emit-only 验收(ctron-emit → cc →
原生运行);解释器 W4 桥未含 net 垫片,`use std.net.*` 在解释口径无绑定——
这不是缺陷而是口径边界:网络面在解释器内的支持未排期,夜间/CI 均走原生路径。

### (d) R 线 caps 泛化与 r7b 翻转状态

R 线 E4020(pure 触网)的 caps_used 收集已泛化(见 (b));`tests/roadmap/
r7b_pure_net.neg.ct` 今天为 **RunRed**——跨函数纯度传播未实现(`#[pure]`
经间接调用洗白能力调用不产 E4020),翻转待 R 线 std/net 解析就绪,按翻转
协议迁 NegGreen(E4020)。另:UDP 真回环已入本地门禁(`tests/net/udp_roundtrip`,
双 socket :0 协调、字节断言,计入 6/6;500 轮 fd_churn 同在本地门禁);
nightly 承载 1k/规模面。

### (e) 终审必修登记(2026-09-20)

- P1 caps 审计 = 锚点声明制:E4010 触发要求同文件含 `use std.net.*` 且存在
  `&Net` 形参 fn(审计锚);漏写锚点即免审计(与 fs 键既有机制同构)。
  强制审计/细粒度键归 R 线收口。
- §11.2 冻结形态(trait 方法/Result+List[SocketAddr]/fd 禁触)与落地面
  (自由函数+rc+last_net_error+raw fd 流通+resolve 首个 IPv4)的收敛条件 =
  发射面缺口关闭:I8/U8 视图 typedef、trait 方法分派发射、Result[Struct,_]
  Ok 成员访问(见 (a));收敛前以 std/net.ct 头注①–⑤ 为准。

### (f) P2 执行发现(协程运行时,2026-09-20)

P2 波(ctron_rt 协程运行时 + 垫片混合化 + 模板模式分支 + 确定性调度)实测的
跨平台/跨面事实;细节见 `docs/superpowers/plans/2026-09-20-server-p2-async.md`
执行记录与 `std/net/c_src/ctron_rt.c` 头注。

- **darwin ucontext 不支持 N:M 迁移 → 自绘切换**:getcontext/makecontext/
  swapcontext 在 darwin/arm64 已弃用且标注 "No longer supported";标准 N:M
  用法(2 worker × 2 协程 × 互斥就绪队列,协程跨 worker 迁移)即确定性崩溃
  (SIGBUS/SIGSEGV/协程记录被堆写穿;单 worker 同负载永不复现 ≥60 次)→
  ctron_rt 自绘切换:仅保存/恢复 SysV callee-saved + sp/pc,信号掩码不随切换
  (用户信号处理与协程混用 = 已登记限制);arm64/x86_64 同一段汇编覆盖
  darwin/Linux。
- **TLS/errno TLV 槽缓存三类串块 → noinline 收口口径**:`_Thread_local` 的
  TLV 槽位地址被 callee-saved 寄存器缓存,协程跨 worker 迁移后读到别线程的
  单元(实证三态:垫片结果写 NULL->result 段错误 / 漏取消悬挂 / 结果错值);
  errno 槽同理。收口 = 全量 noinline 槽访问器(每次调用在当前线程重新解析),
  协程态任务运行期自取 rt current()。
- **weak-extern 在 Mach-O 缺失 → 弱定义哑元机制**:`__attribute__((weak))
  extern` 未链接符号在 ELF 落 NULL,Mach-O 链接期即报 undefined(实证)→
  改「弱定义哑元 + 强定义顶替」:哑元与真实现同为强符号位,链接期整档案
  二选一(双平台标准语义,无同 TU 折叠实证);PE/COFF mingw 支持弱定义。
- **WIN32 哑元须无条件发射**:哑元定义被 `#if` 裁切时,触点对 rt 符号的
  引用仍存活(运行期守卫不裁符号引用本身)→ mingw 链接断;九哑元改无条件
  发射(预处理产物九存活引用全有定义、nm -u 零未定义 ctron_rt_* 实证)。
- **确定性契约 = spawn→join 单向闸**:CTRON_RT_SEED 的逐字节重放面仅覆盖
  纯 spawn/channel 程序的调度序;裸线程首个 join 开闸前 worker 不弹出
  (否则就绪集合本身非定,任何抽取法不可救)。闸单向:开闸后裸线程再度
  spawn 无声重新引入风暴竞态(无报错,输出序退回非确定);IO 到达/定时器
  到期仍是真实时间,不在契约内(虚拟时钟属后续)。
- **errno/TLS 迁移窗口(P2 终审升档,耐久回写)**:协程在 net 调用失败与
  调用方读 last_errno 之间若 park 并迁移线程,读到的是新线程槽——noinline
  访问器只治槽地址缓存串块,不治迁移语义本身;挂账 = errno 按 key 分槽
  (随协程记录携带;升自 Task 4 报告,耐久化)。
- **coro 模式 chan 成功路径 wake-all = O(全体协程) 遍历**:g_all
  append-only 不摘墓碑,唤醒遍历全体;混合负载(通道 + 万级 net 停车)下有
  O(N) 放大面;P3/P9 处置 = chan 定向唤醒或分槽。
- **G 内 syscall(fcntl/EV_DELETE)串行 worker**:fd 高流失负载下 revisit
  触发器;P3 (b) 兴趣驻留落地后该面积自然收缩,届时再评估。
- **getaddrinfo 阻塞面**:coro 口径为阻塞调用(占用 worker 至解析返回);
  P9 一行登记:池线程化或 rt_wait_fd 化。

### (g) P3 执行发现(TLS/发射面/加载器,2026-09-21)

P3 波(vendored mbedTLS + std/tls 门面 + 互操作矩阵 + Unix socket + DNS 异步)
实测的发射面/加载器事实,皆编译器泳道挂账;细节见
`docs/superpowers/plans/2026-09-21-server-p3-tls.md` 执行记录与
`.superpowers/sdd/p3-task-{3,4,5}-report.md`。

- **spawn 闭包内 assert/panic 语句发射缺口**:闭包(void* shim)体内的
  assert/panic 语句发射裸 `return 1`(int 落指针返回位,cc 报);显式
  `return <n>` 才正确走 result 槽 → 夹具绕行:协程体内全显式 return 码、
  主面 join 后断言(tls_smoke 首证,tls_interop 同款;同源实证:StdNet 值
  struct 入闭包捕获槽发射 int32 截断,协程内改自取 Net_probe())。
  修法 = emit 侧按闭合数上下文选 return 形(挂账另开任务)。(源:P3 Task 3)
- **含闭包 return 的函数体不可再有 Drop 句柄局部**:闭包 shim 误带外层函数
  drop 表(`t_UnixStream__drop(t_srv)` undeclared 实证)→ 解法 = 闭包体拆为
  独立的无 Drop 局部 fn(unix_sock 夹具 `resolve_coro_pair`)。(源:P3 Task 5)
- **while 体 Drop 局部禁令精确化 + 内层裸块惯用法**:emit 直报
  `emit:while 体:Drop 局部未支持(移入内层裸块)`——非 accept 环特有
  ((a) 条旧记的精确化),任意 while 体同禁;修法循 emit 自带指引:循环体
  移入内层裸块,句柄在裸块作用域内生灭。tls_interop 的
  INTEROP_LOOP/INTEROP_CONNS 循环 = 该约束下首例「循环内 TLS 句柄 RAII」
  实证。(源:P3 Task 4,登记族 db2b4dd)
- **加载器菱形 use 误报 E5020**:跨模块菱形 use 一律 E5020(栈式查环无
  pop):std/tls.ct→net.bind 之后消费方再 `use std.net` 即成"环" → 绕行 =
  std/tls.ct 零 std.net import,`net: StdNet` 形参按名于 use 文本合并后的
  单命名空间解析(E5030 同名拦截恰为该机制的守卫);类型面独立、能力面
  贯穿。正解 = 加载器侧判定完成后允许 memo 化复用。(源:P3 Task 3)

### (h) P4 执行发现(发射面跨模块 struct 形,2026-09-21)

P4 波(std/http 协议半层落地)实测的发射面事实,皆编译器泳道挂账;最小复现
= `.superpowers/sdd/p4-task-1-report.md` 探针记录。核心:跨模块 let 绑定位
的型别信息丢失 → Member 出口径;同模块面全部无损。

- **跨模块 fn 返回 struct 后,消费方成员读取不可发射**:`let h =
  http_parse_head(...)` 后 `h.rc` 即 `ct_expr:Member@N` 落出到产物
  (ct_typeof 对跨模块 let 绑定取不到 `u:` 型别)。**绿面**:同模块成员
  读写、struct 按值跨模块流动、消费方把值作实参回传跨模块 fn、消费方侧
  struct 字面量构造。绕行 = 成员访问留在所属模块,公开面 = struct 返回 +
  标量 getter(std/http parse.ct 的 http_rc/sl_vs 面即此形;Box[Box64]
  出参通道为另一已知好形)。
- **Box[多字段 struct] 深链/整读/字段赋值不可发射**:`b.v.f` 链
  (ct_expr:Member@N)、`let h = b.v` 整读、`out.v.f = x`(`ct_stmt:struct
  字段赋值未支持`)、`out.v = StructLit` 整赋全红;绿面仅 Box64 标量出参
  (`out.v = x`/`b.v`,`bind.ct` 形)与 `f(b.v)` 整传实参位。
- 同模块局部 struct 字段赋值与嵌套成员链(`o.inner.f`)皆绿 —— 缺口精确
  圈定在「跨模块类型解析」,不涉结构体布局/复制语义。

### (i) P4 执行发现续(解释器值域定宽实例 + 编译泳道移交收口,2026-09-21)

P4 服务器泳道(P4-B 压缩 / P4-C 客户端·SSE·WS / P4-D 基准·fuzz)移交编译器
泳道的在册事实收口;细节见 `docs/superpowers/plans/2026-09-21-server-p4-http.md`
执行记录与 `.superpowers/sdd/p4-task-{2,3}-report.md`。

- **解释器按值域定宽算术(两实例族,均编译器泳道挂账;emit 侧 int64 恒正确,
  双口径漂移)**:
  1. *6 域 `c6sub` 双负操作数符号翻转*:`(-1) - (-86400)` 实证得 `-86399`;
     根因 `compiler/src/eval_val.ct` c6sub 双负分支取 `c6sub(|a|,|b|)` 应为
     `c6sub(|b|,|a|)`。(源:P4 Task 2,date.ct 已结构性规避)
  2. *乘法按值域定宽判上溢*:声明 I64 而值落 I32 域的操作数相乘,解释器按
     I32 宽度判溢出即 panic(`z * 86400` epoch 段炸,源:P4 Task 2;
     std/time.ct `w - w + x` 提升惯例即此规避)。**P4-D fuzz 再证 std/http
     五处同族**:chunk size 行 `acc*16`、CL 十进制 `acc*10`(值 ≥ ~2.1e8)、
     WS len64 `acc*256`(首四字节 ≥ 00 80 00 00)、WS 掩码键首字节 `*2^24`
     (≥ 0x80 即触发,50% 随机掩码键)、client.ct Location 端口 `acc*10`
     (第五处,2026-09-21 终审补:端口 ≥10 位数字,interp 误判 panic /
     emit 垃圾端口 fail-closed,双口径漂移)—— 值跨 [2^28, 2^31) 带即炸;
     corpus 零值超位语料(17 个 0)不触发,盲区被 fuzz 结构化生成覆盖。修 =
     std/http 五处 C10 宽域惯用法 `(x + 2^32 - 2^32) * k`(纯恒等,emit
     无差;第五处 client.ct 端口行同波补);编译器侧正解 = 乘法宽度按声明
     类型/promotion 而非运行时值域(P9 挂账)。(源:P4-D fuzz,seed 2
     chunk 10-hex 用例;第五处源:P4 终审)
- **chunked size 累加负值/溢出门(std/http 自身逻辑缺口,独立于上述溢出边,
  2026-09-21 终审)**:恰 16 位 hex `FFFFFFFFFFFFFFFE`(15×F+'E';`digits>=16`
  门在接受前检查,第 16 位放行)使 chunk size 累加在 `acc*16` 处越过 I64 域
  —— 当前算术语义(emit `__builtin_mul_overflow` / interp 大数域界检)下
  双臂**确定性 panic rc=1,单请求远程 DoS**;回绕语义下同一触发串使 acc
  静默成负,CR 处 `acc > 1e18` 界门对负值失明 → remain<0 → 服务端形(dst
  新缓冲)负下标 / 客户端形在地分框静默腐化 + produced 错报(双失效形态)。
  修 = 乘前 I64 界门 `acc > 2^59-1` 即拒(parse.ct hs_parse_dec 界内累加
  同款,err 槽 2;≤1e18 接受面零回归)+ CR 处补 `acc < 0` 防御冗余档;
  语料钉 `tests/http/corpus/r_chunkneg.ct`(触发串在案:修前双臂红 rc=1,
  修后双臂绿)。(源:P4 终审,2026-09-21 修)
- **use 路径含 `-` 宿主崩溃**:模块名带 `-`(`use std.http.bind-deflate.{…}`)
  使 ctron-cc/ctron-emit 双双 segv(rc=139)—— 词法对 use 段无 `-` 防御;
  本波改名 `binddeflate.ct` 规避。正解 = 词法段字符合法化 + 响亮诊断。
  (源:P4 Task 2)
- **(h) 家族新证(P4-B/C)**:Option[struct] 变体载荷 match 与 List[struct]
  push 发射错型(form.ct 头注③,交错 K/V 道绕行);TupleE(tuple of
  structs)返回出口径;struct 的 Box 字段 StructLit 整赋/字段赋值出口径;
  跨模块 struct 作字段(CxRead.head)出口径(改纯标量槽 + 消费侧重解析);
  闭包内跨模块 struct 返回调用不可靠(http_net() → 传输壳去 io 参,裸标量)。
  绿面/绕行同 (h) 结论。(源:P4 Task 3 §5.3、Task 2 §8.4)
- **fmt × emit 不一致(scope 拆臂挂死)**:`ctron-fmt` 将
  `var out = scope { |sc|` 重排为两行(`scope {` + 独立 `|sc|` 行)后,
  `ctron-emit` 前端挂死(bootstrap 解释器不终止;单行形恒绿)。修法归属
  fmt/emit 任一侧对齐。(源:P4 Task 3 §5.1)
- **加载器严格树 E5020/E5030 加码实证**:任一文件的传递 use 树中每个模块
  至多出现一次(栈式查环无 pop);同父双子共享任一叶子 = E5020;同名二次
  合并 = E5030 ⇒ std/http 消费方 use 图必须严格互斥树(client 唯一 IO 面
  ← {bind,parse,message};sse 纯叶;ws → crypto)。正解仍是 (g) 已挂账的
  「加载器侧判定完成后 memo 化复用」。(源:P4 Task 3 §4)
- **std/enc b64 C8 入参约束(依赖登记)**:std/enc `b64_encode` 受 C8
  printable-ASCII 入参域约束,原始摘要字节不可作 Str 承载 ⇒ ws.ct 自出
  lane-b64(RFC 4648 标准字母表 + '=' 填充);SSE/WS 非 ASCII 载荷、form
  解码 Unicode 面同受 C8 域约束。std/enc 字节构建面放宽(C8/Str 构建面
  升版)后 ws b64 可收编、form 需随动。(源:P4 Task 3 §3、Task 2 §8)

### (h)/(i) 家族新证 + C17 续证(P5-A std/json 数值保真,2026-09-21)

- **(h) emit 的 Result/Option union 载荷槽 32 位(I64/F64 载荷失真)**:
  `Result[I64,Str]` 的 Ok 载荷跨 match 绑定按 32 位槽承载——`Ok(9223372036854775807)`
  读回 `-1`(I32 截断),`Result[F64,_]` Ok 载荷错值(2.5 探针);同模块/
  跨模块同病,Err-Str 与 Bool 载荷绿。**绕行 = 结构通道 + 标量 getter**
  (std/http parse.ct getter 面 prior art):std/json 数值访问器内走
  `JNum{k:I64,v:I64}` / `JReal{k:I64,v:F64}`(struct 标量字段 64 位实证绿,
  plain I64/F64 返回绿),消费方经 `jv_*/jk_*` 或 `jn_*/jr_*` getter 取值;
  `jget_*` Result 面留同模块/解释口径与 Err 原文面。修法 = emit 侧 union
  载荷槽按载荷声明宽度(P9 挂账;strconv.parse_i64 的 Option[I64] 大值面
  同疑受累,其单测未入 emit 臂故未暴露)。(源:P5 Task 1 探针 loc/mi/ml/mn)
- **(i) 解释器 F64 值域定宽三实例 + 语义歧**(emit 真 double 恒正确,双口径
  漂移,双臂一致语义钳窗口径见 std/json.ct 区块头注):
  1. *F64 整值 ≥2^63 乘法 panic*:宿主按值域定宽判溢出,`1e18 * 1e18` 即
     `integer overflow (*)` rc=1(0.1/2.5 等分数路径真 double 恒绿)→
     std/json jf64 乘 10 前逐次哨兵界门(I64_MAX/10),量级窗 = |值| ≤
     I64_MAX(双臂一致;emit 全域可算,窗即 v0 口径)。
  2. *浮点字面量按值域溢出*:`let a: F64 = 9223372036854775800.0`(19 位,
     值 < I64_MAX)即字面量解析 panic;≤18 位档绿 → 夹具禁 ≥19 位小数
     字面量。
  3. *e 形浮点字面量误析*:`1.0e3` 读回 1.533、`2.0e1` 读回 2.531(值失真
     非 panic;emit 同面 1000/20 正确)→ 全库小数字面量平书禁指数形态。
  4. *F64 超 2^53 精确不舍入*(语义歧非错):解释器宽于 IEEE double,
     `9007199254740993` 累加读回原值,emit 舍入至 2^53——2^53+1 舍入可检
     负例锚归 emit 专臂夹具(tests/json_fidelity x_p53_rounding,C12
     「发射线夹具承载」先例),锚值选双臂稳定面。
  5. *emit F64 to_string 乱值*:`922337203685477580.0` → `"51298592"`、
     `2.5` → `"2"`(截断/乱码;`1e22` 形反而对)→ F64 断言一律 `==`/`>`
     比较,禁 to_string(emit 臂)。
  6. *f64 多步缩放双舍入漂移*:17 位尾数 + 逐步 ×10 与 strtod 逐正确舍入
     在 >2^53 档可差 1 ulp(`922337203685477580` 锚 emit 失配实证)→ 锚值
     限 ≤16 位精确尾数 + 单次缩放。(源:P5 Task 1 探针 p2–p18)
- **C17 续证(宿主对 std/json.ct 测试段敏感)**:json.ct 追加 8 测试助手
  + 4 test 块(37 decls)后,对文件 ANY 扰动(含 2 行 println 的调试性
  编辑)即宿主 segv rc=139 启动即崩、无输出;还原至「访问器 8 decls、
  测试外移」后双臂恒绿。对策:数值访问器留 std/json.ct 本体,inline 测试
  外移 tests/json_fidelity(条目直构 + utf8_enc 括号拼接,兼避字符串禁裸
  `{` 约束;json_write.ct C17 规避同款)。宿主 decl/字面量阈值归编译器
  泳道。(源:P5 Task 1)

---
维护约定:新发现分歧先记本档(附最小复现),修复后在条目标注 commit。
