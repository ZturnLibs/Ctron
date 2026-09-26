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

- **(h) emit(C 线)的 Result/Option union 载荷跨 match 绑定 32 位截断
  (I64 载荷失真)+ 载荷槽类型缺陷(F64 载荷失真,独立两项)**:
  `Result[I64,Str]` 的 Ok 载荷跨 match 绑定读回 `-1`(`Ok(9223372036854775807)`
  实证,I32 截断),`Result[F64,_]` Ok 载荷错值(2.5 探针);同模块/
  跨模块同病,Err-Str 与 Bool 载荷绿。**机理勘误(P5-B 校勘,Task 1 复核
  勘定)**:载荷槽本身已是宽槽——C 线 emit `typedef int64_t ct_i` +
  `typedef struct { int variant; ct_i v; } ct_res`(compiler/src/
  driver_emit.ct:102-103),失真点在 match 绑定的显式截断 cast:
  `int32_t t_<名> = (int32_t)<matche>.v;`(compiler/src/trans_stmt.ct:996)
  ——即「绑定变 int32、槽恒 ct_i」;而 F64 半是**另一项槽类型缺陷**
  (ct_res 载荷槽整数型,无法承载 double;Rust 参考线 trans.rs 以
  Pay::F→double 按载荷定型,即该缺陷的正确形态)。**两缺陷均仅 C 线
  emit;Rust 线(trans.rs)整型 `__int128`/ct_i 承载 + 载荷定型,不受累。**
  **绕行 = 结构通道 + 标量 getter**
  (std/http parse.ct getter 面 prior art):std/json 数值访问器内走
  `JNum{k:I64,v:I64}` / `JReal{k:I64,v:F64}`(struct 标量字段 64 位实证绿,
  plain I64/F64 返回绿),消费方经 `jv_*/jk_*` 或 `jn_*/jr_*` getter 取值;
  `jget_*` Result 面留同模块/解释口径与 Err 原文面。修法 = emit 侧 match
  绑定去 `(int32_t)` 截断、按载荷宽度重绑(I64 档;槽已宽无需改);
  F64 档需载荷槽按载荷定型(Pay::F→double 形)——两项独立挂 P9
  (strconv.parse_i64 的 Option[I64] 大值面同疑受累,其单测未入 emit 臂
  故未暴露)。(源:P5 Task 1 探针 loc/mi/ml/mn;机理勘误:P5 Task 2 复核
  trans_stmt.ct:996/driver_emit.ct:102-103/trans.rs:4,635)
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

### (i) 家族新证 + 解释器内存记账(P5-B std/crypto 二进制面 + std/uuid,2026-09-21)

- **(i) 解释器「fn 内未用结果绑定」毒化消费调用点**:`var rc = <调用>`
  绑定在**函数体内且 rc 从未使用**(伴随 W8030)时,该函数被 test 块调用
  即**静默 rc=1、零输出**(连函数调用前 `println` 都不执行——测试块整体
  不运行,非 assert 失败;失败面与「调用内是否就地变更」无关,
  `var rc = g(); return 7` 纯调用同炸)。最小复现:
  `fn g() -> I32 { return 7 }` + `fn h2(s: Str) -> I32 { var rc = g(); return s.len }`
  + `test { println(h2("abcd")) }` → 无输出 rc=1。绿形:绑定被消费
  (`if rc != 0 {…}`)、或裸语句调用 `g();`(无绑定)、或绑定在 test 块内
  直接未用(同 W8030 不现)均绿;**emit 臂全形态恒绿(双口径漂移)**。
  夹具纪律:任何绑定必须被消费;忽略返回值一律裸语句调用形。
  (源:P5 Task 2 探针 dbg8–dbg15;最小复现 dbg15)
- **解释器堆不回收定量(P5-A C17 的量化续证)**:解释域循环工作负载
  每块压缩等效(64 轮 div/mod 合成)折 ~0.9s/约 1.5 GB 峰值,峰值随
  调用量线性增长(20 次 sha256 = 40 块 → 33 GB;200 次即 macOS 内存压
  力 SIGKILL rc=137)——循环型重负载在解释口径有硬性规模顶。emit 臂
  同负载 41 µs/块(千倍差)且无此顶。口径落点:crypto_vec interp 逐
  文件 ≤ ~10 块等效、高迭代档 PBKDF2(c=4096 ≈ 8192 块)与百万字节
  SHA 归 emit 专臂(x_ 前缀,crypto.ct 头注「整向量不可实用」的定量版);
  nightly 真靶以 emit 口径为准。(源:P5 Task 2 /usr/bin/time -l 实测)
- **std/sort sorted 系 O(n²)(API 特性登记,非缺陷)**:`sorted`/
  `sorted_desc` 为重建式稳定插入排序(每插一个元素整表重建),10 万枚
  字符串即内存压力 SIGKILL(x_uuid_live 首跑实证)——大集合排序走夹具内
  就地快排(下标读写 + `Str <` 比较均已证)或标升版 O(n log n)。
  (源:P5 Task 2 x_uuid_live)
- **emit 的 &I64[] lane 参数按槽计(Marshal 契约再确认)**:extern 垫片
  的 `n` 语义恒「lane 数」而非「字节数」(lane = int64 槽,每 lane 承
  一字节值;read_t/recv 同约定)——ctron_entropy_fill 首版按字节填,
  16 lane 只得 2 lane 有效数据,uuid 随机面尽零(已修)。垫片侧契约
  注记随文件头。(源:P5 Task 2 std/db/c_src/ctron_entropy.c 首版)

---
维护约定:新发现分歧先记本档(附最小复现),修复后在条目标注 commit。

### (i) 家族新证(P5-C 协议夹具回放 + Postgres wire v3,2026-09-22)

- **(i) 解释器 `utf8_enc` 恒返 U+FFFD(emit 恒正确)**:自举解释口径下
  任意码点 `utf8_enc(cp)` 恒产 3 字节替换符(EF BF BD;逐码点
  `len/byte_at` 程序化实证 65/123/233/20013 四档;emit 臂同探针全绿)。
  病灶在编译器自举 eval 链:eval_call.ct:271 `utf8_enc(vals[0][1])` 的
  实参为 Str 槽值,发射 C 里 `(int)(u0)` 直转指针值 → 出 0..10FFFF 域 →
  走 replacement 分支(ctron_utf8_enc 本体 driver_emit.ct:90 正确)。
  连带:**词法面 `\u{…}` 逃逸经同一内建展开**(parse_node.ct:42),宿主
  敏感语境下不可依赖(夹具表字面量因此弃用 `\u`,见 std/db/pg.ct
  pgx_ascii_char)。连带失真:d_parse_nested(interp)在 `utf8_enc(123)`
  产 3 字节垃圾前缀下仍绿——json parse 对文档前导垃圾宽容,该 corpus 的
  utf8 面为**空覆盖**(emit 臂真锚)。修法 = eval 链 dvi 转换;修复后
  pg 单元格 ASCII 面可收敛为全文本面(字节精确面 pgr_cell_hex/bytes
  不受影响,锚不变)。(源:P5 Task 3 探针 pba/pb2–pb9)
- **(i) 解释器内建 `byte_at`/`byte_slice` 拒收 I64 域标签("K")**:
  内建解释分发严格按 `vals[i][0]=="I"`(I32 域)匹配,pgz 宽域播种值
  (`z + b[i]`,"K" 标签)直入即 `byte_slice arity` panic;client.ct
  cx_slice_str 之所以双臂绿,是其入参出自 lane 装载值(I32 域标签)。
  纪律:内建字节面入参恒 I32 域表达式,宽域值经 `.as[I32]()`/直接
  I32 形参中转。(源:P5 Task 3 std/db/pg.ct pgx_ascii_bytes 首版)
- **emit 的 `List[I64]` struct 字段元素读坏(items 按 char* 出)**:
  struct 字段为 `List[I64]` 时,发射 C 对元素读按 Str 表处理
  (`(const char*)((ctron_list*)…)->items[i]` 直返 char*,cc 报
  pointer-to-integer 转换错或产垃圾值);`List[Str]`/`List[I32]` 字段
  双臂恒绿(探针 probe2/probe3)。纪律:struct 字段容器只用
  `List[Str]`/`List[I32]`(I64 量以 I32 承或拆标量字段;pg.ct 头注②)。
  修法 = 发射器按元素类型出 items 读法。(源:P5 Task 3 探针 probe2)
- **标识符 `L` 不可用(E2020 未解析)**:`var L: I64 = u` 及后续引用
  即 `E2020: 未解析的名称(unresolved):L`(最小复现 bisect2/bisect3,
  同形改名 `n2` 即绿)——单字母大写 L 疑被词法/语义层保留。改名绕行
  (pg.ct 用 `clen`)。(源:P5 Task 3)
- **lane 视图 `.len` = 字面量显式槽数(非类型标注容量)**:
  `var buf: I64[4096] = [0,0,0,0]` 的 `buf.len == 4`(初始化字面量槽数);
  tcp_echo/client_fixtures 以逐槽写满字面量的约定规避(4096 个 0 显式
  落盘)。读面按 `.len` 边界的代码(如 pg_recv_frame_fd 满缓冲门)必须
  按此语义建 lane,extern 垫片 cap 另传显式值。(源:P5 Task 3
  probe_lane;约定证实于 tests/net/tcp_echo main.ct:51 与
  tests/http/client_fixtures x_client_e2e.ct:162)
- **extern 边界 lane 形参 = 视图胞,裸指针签名靠 ABI 巧合**:发射器把
  `&I64[]` 实参编为 `(ctron_view_6){ d, n }` 结构按值传递 —— 垫片形参
  写 `int64_t*` 时首槽恰落指针寄存器"能跑",但下一形参实收 view.len
  而非调用方 n(P5-C x_fd_pipeline 首版实证:write 的 n 恒 = view.len,
  返回值断言红)。ctron_net.c 以 `ct_view6{d,n}` 镜像为正解;垫片新增
  一律镜像视图胞。连带注:std/db/c_src/ctron_entropy.c(P5-B)的
  `int64_t* buf` 签名同属此类,现调用面 lane 长与 n 恒等值故行为正确,
  Task 6 nightly 扩展调用面时应同改视图胞。(源:P5 Task 3 评审修正)

### (i) 家族新证 + 分臂预算定档(P5-D SCRAM + 扩展查询 + 事务,2026-09-22)

- **同名 extern decl 跨模块合并即 E5030**:pg.ct 直宣
  `ctron_entropy_fill`(P5-B 已由 std/uuid.ct 宣)+ 消费方同程序
  `use std.uuid` 即 `E5030: use 导入同名 decl`(探针实证)——decl 合并
  单命名空间纪律不分 struct/extern。绕行 = db 前缀别名符号
  (`ctron_dbpg_entropy`,ctron_dbpg.c 内 view 胞形转发
  ctron_entropy_fill;链接须并链 ctron_entropy.c,run.sh 已扩)。
  同类预检:任何 std 模块新增 extern 前先 grep 既有声明。(源:P5 Task 4 探针)
- **解释器堆不回收定档(P5-B 的量化续证)**:实测 ≈1GB 峰值/块压缩
  (SHA-256 64 轮块):c_pbkdf2_low(3×PBKDF2 c=1 ≈ 18 块)= 24GB 绿;
  SCRAM 单全链文件(PBKDF2+证明+签名 ≈ 23 块)= 40GB 绿;双链同文件
  ≈ 70GB 即 SIGKILL(exit 137,机器级 OOM)。**分臂预算线由此定档**:
  interp 单文件 ≤ 1 条 SCRAM 全链;重链多走/x_ 承载(x_scram_neg/
  x_scram_rfc7677 结构性登记,emit 口径 < 1s)。修法不变 = 解释器
  堆回收;修后 x_ 面可收编双臂。(源:P5 Task 4 tests/db/replay_scram)
- **std/enc b64 任意字节缺口维持登记(ws.ct 先例沿用)**:SCRAM 盐/
  证明/签名携任意字节,std/enc b64_decode 对非 printable 载荷拒
  (None)、b64_encode 入参受 C8 Str 约束——pg.ct 自出
  pg_b64_encode/pg_b64_decode(List[I32] 容器;RFC 4648 全字节
  round-trip 0..255 有锚)。std/enc 字节构建面放宽后可收编。
  (源:P5 Task 4;先例 std/http/ws.ct 头注①)

### (h)/(i) 家族新证 + 并发面发射缺口成谱(P5-E Redis RESP2 + 连接池 + 行映射,2026-09-22)

- **Sender/Receiver 作 struct 字段或 fn 形参 = emit 截断 int32**:
  `Sender[I64]`/`Receiver[I64]` 仅可作局部(let 解构)+ spawn 闭包字面捕获
  (06_concurrency 形);字段化(PoolH{tx,rx})或参数化
  (`fn f(rx: Receiver[I64])`)在 emit C 中按 `int32_t` 出,通道指针截断
  cc 报 incompatible conversion(探针实证)。interp 臂两形皆绿(双口径
  分叉)。绕行 = 池对象不持通道,通道由属主任务局部持有(标准接线 =
  idle Channel[容量=池容量],miss → recv 阻塞 = 背压;tests/db/pool/
  pool_wait.ct 双臂钉)。**连带判定:std 对象持有通道半端在 emit 现实下
  不可用,池/服务类 std 门面须为"纯核 + 组合层接线"形态。**
  (源:P5 Task 5 探针 probe1/probe3;tests/06_concurrency.ct 本身 emit
  红为既存面——`for _` 通配(下条)+ class 字面量入 Mutex 构造位)
- **emit 缺口小谱(本轮探针复证实,均 emit 专红 / interp 绿)**:
  ① `for _ in`(PatWild)→ `ct_stmt:for pat:PatWild` 占位文本进 C;
  ② class 字面量在 Mutex[T] 构造位 → `ct_expr:StructLit 非值类型`
    (struct 字面量同位绿);
  ③ match 于 spawn 闭包内 → `ct_stmt:match R arm body` 占位
    (闭包体走 expect/if 形即绿);
  ④ `Mutex.with/with_mut` 作裸语句位 → `emit:with 仅语句位(let/Expr)`
    (let 绑定/尾表达式位绿);
  ⑤ **闭包内对捕获局部赋值 interp 不回写**(closure 捕获槽拷贝语义,
    `got = 7` 于 with_mut 闭包体内不出闭包)——跨任务共享一律经
    Mutex 内容物/通道,禁走捕获槽写回;
  ⑥ 定长数组 `T[N]` 作 fn 返回值 emit 截断(x_rd_fd 首版实证;lane
    声明内联即绿,tests/net 同口径)。
  (源:P5 Task 5 探针 b1–b6/xa–xe;修法均 = 按绿形改写,登记 v2 收编面)
- **List[用户 struct] 字段容器 + struct 字段列表原地 push**:emit 臂
  `it.flags.push(1)`(struct 字段 List 经 member 链调用)→
  `ct_expr:Member@30` 占位;List[I64] 字段为 P5-C 既登记(读坏)。
  绕行 = 平行标量表(List[I32])+ 值语义整体重建(pool.ct PoolState
  形;局部新表逐槽搬运,不在共享表上原地写)。(源:P5 Task 5 探针 b4)

### (h)/(i) 家族新证 + 收口核对(P5-F 门禁 + 登记收口,2026-09-22)

- **定长 lane 复制经 view 形参索引赋值 = emit 拒(本波唯一新证)**:
  `fn fill(dst: &I64[], …) { dst[i] = v }`(形参为 `&I64[]` 视图,函数体
  内索引赋值)→ emit `ct_expr:index 目标非 List/数组:i 名:dst 下标:i 节点
  元数:3 尾槽tag:Ident`;同形**局部 lane 下标赋值恒绿**(x_ 夹具逐槽搬运
  先例),List 形参经 push 传播绿(P5-C 已证形)。interp 臂不判(纯层无
  extern 语境,该面 emit 专红/interp 不可达)。绕行 = 字节 List 先在调用
  方解码、lane 声明与逐槽搬运恒内联在 test 块体内(x_pg_fd_session
  srvb/srv 形;与 ⑥ 定长数组返回值截断同属「数组作抽象边界」缺口谱,
  v2 收编面:发射器按形参视图胞出 items 写法)。(源:P5 Task 6 探针)
- **P5 全波编译器面收口核对(Task 6 去重结论)**:评审移交候选清单——
  未用结果绑定毒化(P5-A §i)、解释器堆 ≈1GB/块定量 + 1.5GB 定档
  (P5-B/P5-D)、(i) 值域定宽乘法第五处惯用法(P4 终审已收口)、c6sub、
  use 路径 `-` 宿主崩溃、Option[struct]/List[struct]、Sender/Receiver
  字段化截断 + 池「纯核 + 组合层接线」裁定、utf8_enc 解释口径 U+FFFD、
  Result/Option 32 位载荷 match 绑定截断(trans_stmt.ct:996,勘误处方
  已随 P5-B 登记)、加载器严格树 E5020/E5030、b64 C8 域约束、fmt×emit
  scope 拆臂挂死——**经逐条比对均已在 P3/P4/P5-A..E 各节持久登记**,
  本波无重复条目;上文 view 形参索引赋值为唯一新增。
  (源:P5 Task 6 登记收口核对)

### 终审收口新证(P5 终审收尾波,2026-09-21)

- **redisx_uint 共用使 ':' 整数回包承 bulk 域门(误分类;fail-closed 但
  分类错,登记修法随 12-db v0.9)**:redis.ct 整数回包(':' 形)与 bulk/
  数组长度域共用 `redisx_uint` 解析——该解析为 bulk 域设双门:累计中途
  `> redis_bulk_max()`(512MiB = 536870912)即拒 + 域宽 `> 18` 位即拒,
  两门对 ':' 整数形同样生效。而 RESP2 整数是合法 I64 域(INCR 计数器可至
  2^63-1 = 19 位):值 > 536870912(如 6000000000)或 ≥ 19 位
  (如 I64_MAX 9223372036854775807)的**合法整数回包被判 err 2"协议违例"**
  ——err 2 = 流信任失档,池侧随之弃连(pg_conn_dirty 分级;健康连接因合法
  大整数被弃)。fail-closed(不误收)但**误分类**:协议面无恙,值域门错用。
  **修法登记**(本波不落码):':' 整形独立 I64 哨兵解析(rm_i64 同款——
  乘 10 前按 I64 界哨兵预门,负域裸 "-" 已有 P5-F 负向累计面),bulk/数组
  长度域维持 redisx_uint 不变;随 docs/spec/12-db.md v0.9 批次落地,随行
  锚 = 大整数 INCR 回包钉(I64_MAX/I64_MIN 两端)。(源:P5 终审收尾波实证
  ——redis.ct:214 redisx_uint 双门 × redis.ct:390 ':' 形共用)

### utf8_enc 内建双层委派分歧(2026-09-23 实证,stdpkg json 消费首踩亮)

- **现象**:seed→cc_run→程序双层语境下 `utf8_enc(cp)` 返回空串(jprobe 直调
  123/125/65 三行全空);单层 seed(`ctronc test std/json.ct` 10/10 绿)、发射臂、
  自举臂(ctron-cc)全绿。write_json 根对象/数组恒经 utf8_enc(123)/(125) 出花括号,
  故合并面写出半边在双层语境缺 `{}` 且行序错位(stdpkg 夹具两臂对数必分歧)。
- **链路**:内层 eval_call.ct:269 委托宿主 `utf8_enc`;seed rt_eval.c:984 有内建;
  双层传值槽位错位嫌疑,根因归 compiler-c 线。
- **归因**:提交地板复现(以 HEAD compiler/src 拼装 cc_run 复刻 ctc.sh run 实证),
  非 2026-09-23 在制 lex.ct 所致;系潜在分歧,json 写出半边消费尝试首暴露。
- **阻塞与复位**:stdpkg 全量消费夹具的 json 写出半边(write_json/jw_*)暂缓——
  待本分歧修复后按同款 ent→write→parse 回读夹具复位(形态已备案于 2026-09-23
  会话记录);夹具回归 reverted(未落库,两臂逐字门禁不让红)。

### P1b 选择性合并 × extern 再导出发射缺声明(2026-09-23 实证,net 三夹具红)

- **现象**:`use net.{Net_probe}`(门面)+ `use net.bind.{ctron_net_last_errno}` 共同
  合并时,emit 侧对 last_errno 出 `t_ctron_net_last_errno()` 调用但**无 extern 声明**
  → cc "undeclared function"。仅 `use net.bind.{...}` 直连时声明正常(最小差分:
  m2.ct vs m.ct,后者双 extern 全_DECLARED)。
- **波及**:tests/net clock_sanity/tcp_echo/coro_hybrid(emit 臂)、tests/http
  frm_route_a_mw_chain(E2020 形态,同族 merge-through 收紧)。时间线:P1b 编译器
  落地(2d2c98c/8093990/b3a5d20)即现,与域包迁移无关(迁移前后 merge 结构同形,
  本日 76be7ff 审计清单外的门面再导出形态漏网)。
- **归因**:P1b 选择性合并的来源标记在"门面 use 请求 + 消费方直连请求"同符号
  双径下,emitter 侧 extern 性丢失 → 按 Ctron 级 fn 出 t_ 调用。归 compiler/
  emitter 泳道(trans_emit.ct 在制中)。
- **复位**:修后 tests/net 三夹具免改即绿(显式 use 已补到位)。
- **复位复核(2026-09-23 晚,双臂净场对照:工作区在飞件 vs 干净基线)**:三夹具
  仍红但死法分化——clock_sanity 仍 cc 缺 `t_ctron_net_last_errno` 声明(本分歧
  本体,在制 trans_emit 修法对口);tcp_echo/coro_hybrid 已前移死在 emit 阶段
  (`ct_expr:StructLit 非值类型`,见下节)——**须先销该断裂,本节复位条款才可达**。
  在飞 ct_impl_method_fns 再加回为半接线态:零调用方,四夹具探针与干净基线
  行为零差。mw_chain 复核:interp E2020 已在案;emit 臂现测出 C 后 cc 缺
  `t_frm_mcode_buf` 声明——同族第二实例(门面再导出形态),修复须一并覆盖。
- **销账(2026-09-23 深夜,09c22a7)**:根因非备案原猜的"emitter 侧 extern 性
  丢失",而是 parse_pkg 的 done-skip 把消费方直连 use 的请求**整组吞掉**——门面
  先行合并同一模块时 keep 集不同,请求符号缺失;ctron-emit 不跑语义,缺失名
  放行成 `t_` 调用且无声明。修复=done 模块顶层重入补并(缺失符号照常并,已在
  视图者 dup 处静默跳过;递归层保持旧 skip——补进中层 mf 反而消费级撞名,smoke
  3n 实证;E5020 栈检豁免已 done 者,栈系单调不弹)。修后 clock_sanity extern
  声明+裸调齐、mw_chain 双臂绿;tests/net **14/14**、tests/http 97/11(余额全为
  在册解释器债/e2e);smoke 149/1(ctecho 冒烟红系 HEAD 既有,基线差分实证)、
  自举 suite 73/73。在制 trans_emit.ct ct_impl_method_fns 本修复不需要(仍零
  调用方,留 peer 泳道处置);在制 lex.ct or2 断链修复已先落(4f8e545)。

### emit 中途崩:ct_expr:StructLit 非值类型(2026-09-23 晚实证,net 双夹具 emit 红)

- **现象**:ctron-emit 发射 tests/net tcp_echo/coro_hybrid 至 StructLit 表达式
  中途夭折,诊断 `ct_expr:StructLit 非值类型` 追加在半截 C 尾(stdout 通道,
  stderr 空),rc=1;两夹具到不了上节的 cc 缺声明面。
- **归因**:已落库断裂,非 2026-09-23 在飞件所致——干净基线双臂复现:
  55acb90+lexfix 与 1218cb0+lexfix 同崩;排除 55acb90(gui checkbox)、
  b82ada3(纯注释扫尾)、b1e7073(仅 examples)、在飞 trans_emit/lex(四夹具
  探针零差)。引入点 ≤b1e7073(19:52),候选 e3bc073 域包迁移一带;上节
  备案"cc 阶段"叙述系旧二进制观察。
- **复现**:git worktree 检出基线 → 仅贴在制 lex.ct(见下)→ build.sh +
  native.sh → `ctron-emit run tests/net/tcp_echo/src/main.ct`。
- **前置事实**:纯 HEAD 自举链断裂——lex.ct \u 转义判定处 or2 单参错字
  (`emit:调用实参个数不符:or2 期望 2 实得 1`),纯 HEAD 无法自举;在制
  lex.ct or2→|| 改写即修复,须先行落库。native.sh 的 build 拼装守卫
  (cc_run.ct 在即跳过)会吞陈旧拼装——勿用旧 bin 纪律的实例。
- **顺带归属**:tests/http interp 臂 Killed:9 群(frm_auth_a_guard/jwt/sess、
  frm_mw_a_csrf、client/sse_ws e2e、http/frm auth inline)与 mw_chain interp
  E2020 同为已落库既有债(1218cb0 复现,非 55acb90 所引),归 server/frm
  泳道账,暂不入本文档两臂口径。
- **销账更正(2026-09-23 深夜,e96d5f6)**:上节"已落库断裂"归因有误——真因
  并非任何提交损坏了发射器,而是①夹具违反 P1b 显式请求契约(tcp_echo/
  coro_hybrid 系全模块合并时代旧件,P1b 收紧审计漏网:调 ~20 门面函数、构造
  4 种跨包 struct,却仅请求 `Net_probe`);②trans 侧 `ct_is_struct` 查无即
  panic(未合成 E 诊断;sem 亦未拦未合并类型命名,直达 trans 才崩——sem/trans
  一致性另立待办)。修复=夹具补全 use 清单+net.ct TcpListener/TcpStream/
  UdpSock、bind.ct Box64 开 pub(SL-0.7,gui.ct GuiTree 先例),emit 崩不复现。
  mw_chain 同族=旧「router 符号经 middleware 单命名空间合并贯穿」设计对 P1b
  失效(非贯穿符号必饿),夹具按契约直 use router 双符号即绿——原「严格树
  E5020 规避」戒律系 P1b 前旧形,done 补并后直 use 无环(router 不依赖
  middleware)。

### 发射符号饿死新证:frm_rc_* accessor 形状依赖确定性缺失(2026-09-24,P6-F 门禁首踩;HEAD worktree 隔离复现)

- **现象**:消费方直 use `frm_rc_match` 等三 accessor(rc 码函数,`pub fn x() -> I64 { return <常量> }`
  形)的夹具,emit 产物对其**确定性缺失**(声明/定义均无,或仅声明无定义),cc 阶段
  硬错(implicit declaration;新 clang 为 error,`-w` 不掩)。可复现件:tests/http/
  frm_route/x_bench.ct(直 use 形,mit 行 322/328 调用点在、符号不在)。
- **形状依赖(HEAD=a514f07 worktree 隔离构建,同机同刻探针矩阵)**:
  单符号直 use ✓;六符号表+简单体 ✓;全 11 符号表+简单体 ✓;**x_bench 本体
  ✗**;截半体(V10:pin1–3)✗。判别面在 body 形状与符号集的交(「rc_match 调用 ×
  method/miss 同清单」嫌疑最大,精确判据留编译泳道);产物确定性(md5 逐次一致),
  非随机。8 符号表 + 双 match + param 链 + bf_fill + net 探针形(V17/V18/V20 族)
  全绿 —— x_bench 据此绕行(钉面改字面 rc 码 1/0/-1 = router.ct 在案钉值;三
  accessor 暂离 use 清单),编译修复后回切。
- **史注**:2026-09-23 e96d5f6 已录「合并贯穿对 P1b 失效」(mw_chain E2020 形);
  本节新证 = 直 use 亦饿 + 静默形(发射 rc=0 不报,cc 才炸)+ 形状依赖确定性。
  另:2026-09-24 机刷在飞自举二进制曾呈现同族缺声明(client/sse_ws e2e 的
  t_client_write_str/t_http_method_start 形),与 HEAD 静态缺陷叠置,门禁采数
  一律走 worktree 隔离构建(CTRON_EMIT 覆盖口,bench_cycle.sh/run.sh 已备)。

### emit 直发主文件确定性 SIGKILL @20480B(P7-B std/pb.ct,2026-09-25)

- **现象**:`ctron-emit run std/pb.ct`(435 行,作为主文件)产物恒止于 20480 字节
  (恰好 20×1024;截断点 = t_PbField typedef 中途),SIGKILL;复跑两次逐字节同
  尺寸。同文件**作为依赖**(corpus `use std.pb.*` 五件)被 emit 全量发射且双臂
  全绿——仅「主文件发射」路径死。
- **归因候选**:20KB 输出缓冲边界 + 主文件发射路径(直发 main-dispatch 与全部
  顶层 decl)的内存行为;依赖路径代码相同而安好,故非单一 decl 触发。归编译
  泳道(minimal repro = std/pb.ct 本体)。
- **绕行**:tests/pb/run.sh 的 std 直发腿以 chk 语义门替代;代码生成覆盖由
  corpus 五件(作为依赖的 emit 双臂)承担。

### 主文件发射确定性 SIGKILL @36864-45056B(依赖路径同病;todo_app 复现族,2026-09-26)

- **现象**:`examples/todo_app` 多模块包(数据/会话/视图/HTTP 四件)任意主文件形态
  emit 确定性 SIGKILL:主文件直发 45056B;依赖 shim(main→app)36864B/40960B;
  星形单路径拆分(main→app→{data,sess,views})36864B;688f65e 与 main-tip 双工具链
  同死;截断点恒落在 std/http frm 原型区(mp_parse/form_key_at 等处),逐字节可复现。
- **对照**:同机同源 `todo_api`(512 行单文件主)292927B 全绿;todo_app 的
  **app_main 体 stub 化即全量 262KB 绿**,恢复真体(哪怕 `serve()` 单调用转发)即死;
  最小循环(config+listen+读头读体)271KB 绿,+4 行会话块即死。
- **归因候选**:主分发/调用图遍历在依赖图上的有界预算(疑似 32-45KB 输出窗口);
  非纯尺寸(todo_api 292KB 绿)、非单函数体(拆至 ~50 行仍死)、非双路径合并
  (星形单父仍死)、非 flattener(fn 值不透明边界仍死)。
- **已排除**:内存压力(16GB 空闲 68%)、工具链版本(三构建同死)、合并顺序、
  frm/auth 依赖(移除后仍死)、frm/body(本地 JSON 取值器替代后仍死)。
- **绕行**:todo_app run.sh 原生臂红账站岗(emit 失败即明确报错);语义面
  `ctc check`(0E)+ 四模块 `ctc test` 全绿承接到保管;发射修复后 run.sh 即为验收门。
- **最小复现**:examples/todo_app @ b0ce76d+ 星形拆分版;`ctron-emit run src/main.ct`。
