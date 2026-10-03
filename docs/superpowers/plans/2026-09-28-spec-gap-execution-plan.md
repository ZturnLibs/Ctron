# Ctron 规范缺口执行总计划(Spec Gap Execution Plan)

> 日期:2026-09-28。基线:规范 `docs/spec/`(v0.8 冻结面)× 实现(自举 `compiler/src` + std/域包 + tests 红账)差距分析(同日完成,见会话记录)。
> **驱动协议:用户说「开始 Txx 任务」= 该编号任务开工。** 执行者必须先读本文「执行公约」与该任务卡全文。

**目标:** 将规范承诺与实现之间的缺口按依赖与价值排成 9 个波次、50 件编号任务,每件独立可验收、可落库。

**架构:** 小件先行(W1/W2/W4 清红账)、深水串行主链(trait 对象→GC→并发真化)、三档最后(依赖后端插件接口)。每件 = 一个独立可测试交付物,完成即落库并回写状态行。

**验收总口径:** `bash ci.sh` 九门全绿;`compiler/test/suite.py` 双跑零新增分歧;`tests/COVERAGE.md` 红账不新增;规范锚翻转按文件头判据。

---

## 全局约束(每件任务隐含遵守)

1. **验证用 seed 勿用旧 bin**;`cc/emit` 管线不对称,emit 挂先跑 cc 拿真错。
2. Ctron 语法坑:**禁 `;`**;字符串裸 `{` 须 `\{`;无位运算算符(`&`=借用/`|`=闭包/`^`=E1001),位运算一律算术等价或编译器内建;无三元,用 if 表达式;`or2` 全括号;内嵌重建断言字面量勿带标点。
3. **跨包引用即请求铁律(含 struct)**;std 在仓库根,域包挂顶层命名空间(`gui/net/http/tls/db/ffi`)。
4. decl 锁只锁 cc_run;净增 0 是基线纪律,新增 decl 须在提交信息申报。
5. emit 主文件直发 20480B SIGKILL → 大产物走 corpus+chk 替代;lane 字面量槽数 interp/emit 不对称,双侧都要顾。
6. **能力优先于 hack**(用户横切裁决):能力缺口走扩能力,不走降级/兜底/替身。
7. 机刷泳道并行占槽:**开工前 `git status`/`git log` 重对齐**,提交 pathspec 限定;并行件走 `.worktrees` + 软链构建产物。
8. 每件完成即 commit(全绿才落库),并回写本文件该卡「状态」行 + `tests/COVERAGE.md`/`docs/c-rust-divergences.md` 相应登记。
9. S1 Val 迁移在飞(`/tmp/s1-val` 沙箱未提交):W6 GC 三件(T29–T31)与其交叠 `eval_val`/值模型,**须 S1 落库后再开工**;其余波次不受阻。
10. 规范修订顺序(§10.7):先改规范文档 + 增补测试锚,再改实现;「设计偏差回写」须在规范文档挂注记。

---

## 排期总览(9 波 50 件)

> 排期单位 = 专注人力日(d);用户驱动,无日历日期。W1/W2/W4 件间独立可并行(worktree,至多 2–3 槽);W3/W5/W6/W7 内部链式串行;W8 依赖 T36。总量约 **65–90 d**,属季度级程序,按需点名、不要求一次吃尽。

| 波 | 主题 | 任务 | 小计 |
|---|---|---|---|
| W1 | 语言面快赢清账 | T01–T08 | 4–5 d |
| W2 | 网络/并发小件 | T09–T14 | 5–6 d |
| W3 | 迭代器协议(R-P3b) | T15–T17 | 3.5–4.5 d |
| W4 | 诊断契约与红账 | T18–T23 | 5–6.5 d |
| W5 | trait 对象与闭包捕获 | T24–T27 | 6–8 d |
| W6 | GC 系列(最大) | T28–T32 | 10–15 d |
| W7 | 并发深水(P9) | T33–T34 | 8–10 d |
| W8 | 三档模型(web/bare/分层) | T35–T42 | 15–20 d |
| W9 | 工具链与生态 | T43–T53 | 11–14 d |

**推荐主链顺序:** W1 → W2 → W3 → W4 → (T24→T25) → W6 → W7;W8/W9 穿插按需。

---

## W1 语言面快赢清账(件间独立,均可并行)

### T01 · std/bit 模块(§4.5/§4.0 宪法承诺)

- **预估:** 1 d。**前置:** 无。**状态:** ✅ 已完成(2026-09-28)
- **目标:** 兑现运算符宪法「位运算恒走 `bit` 模块」的 stdlib 侧承诺;`std/crypto.ct` 的纯算术移位模拟换正。
- **范围:** 新建 `std/bit.ct`;改 `std/crypto.ct` 消费方;`tests/` 新增正例。
- **要点:** ①先定 API 面(建议:`bit.and/or/xor/not/shl/shr(I64|U64|I32|U32, I32) -> 同型`,规范未钉 API,先在本文档评审拍板);②**实现路线二选一:编译器内建命名空间 `bit.*` 发射 C 位运算(推荐,类似 `math` 内建路径,`sem_calls.ct` 注册 + `eval_call.ct`/`trans_expr.ct` 双侧),或 Ctron 算术等价模拟(crypto.ct 已证可行但性能差)**;③解释/发射双侧同判(3.8 内建三处注册惯例:`sem_calls`/`eval_call`/`trans_expr`,参考 v0.8 追加坑位「新内建三处注册」)。
- **验收:** `tests/` 新增 `bit_*.ct` 正例(含 U64 边界 `0xFFFFFFFFFFFFFFFF`、符号位保持、负数算术右移语义钉死)双侧绿;crypto.ct 回归不红;decl 锁净增 0(内建路径不新增用户 decl)。
- **坑位:** 用 Ctron 写位运算必撞无位算符坑——这就是选内建路线的理由;若走算术模拟,shl=乘 2^n、shr=除 2^n(负数截断除法语义恰为算术右移,已在 §3.1.1 钉死)。

### T02 · 模式守卫 `pat if cond =>`(R-P3c)

- **预估:** 0.5–1 d。**前置:** 无。**状态:** ✅ 已完成(2026-09-28;含或模式/字符串字面量模式三线,锚迁 tests/02f_*)
- **目标:** 翻转 `tests/roadmap/r3c_guards.ct`(现状解析拒绝 E1001):守卫依序判定、守卫臂不参与穷尽性(`r3c_guard_exhaustive.neg.ct`)。
- **范围:** `compiler/src/parse_stmt.ct`(p_pattern 扩展 GuardPat 节点)、`sem_exh.ct`(守卫臂不算穷尽证据)、`eval_pat.ct`(依序试配)、`trans_stmt.ct`(match 发射守卫分支)、R 线/宿主线同形。
- **要点:** 或模式 PatOr 已实现(`p_pattern_or`),守卫是**模式层后置谓词**:解析为 `PatGuard{pat, cond}`;穷尽性算法把含守卫臂视为不可达证据;match 发射侧沿用现有臂跳转结构加 `if(cond)` 门。
- **验收:** r3c 两锚翻转进主套件;`02e_match_patterns.ct` 回归绿;三线(suite.py)零新增分歧。
- **坑位:** 守卫内绑定已由模式引入,`sem` 作用域链须把 GuardPat 的 cond 放在模式绑定之后;发射侧 match 已有语义等价改写先例(如 PatOr 展开),照抄结构。

### T03 · StringBuilder 前奏(r2a_sb)

- **预估:** 0.5–1 d。**前置:** 无。**状态:** ✅ 已完成(2026-09-28)
- **目标:** 前奏最小清单(§3.8.1)成员 `StringBuilder` 落地,翻转 `tests/roadmap/r2a_sb.ct`。
- **范围:** `std/str.ct`(或独立 `std/sb.ct`)实现 + `compiler/src/sem_calls.ct` 前奏注册(若按前奏类型);实现建议:`struct Sb { buf: List[U8] }` 值类型 + `push_str/push_i64/to_string` 面,to_string 走既有 `Str` 构造。
- **要点:** 先查 r2a_sb.ct 文件头承诺的 API 形态,按锚实现,勿自造 API;`to_string()` 是 alloc 面(§6.5 推断自动覆盖)。
- **验收:** r2a_sb 翻转;`tests/` 字符串族回归绿;插值脱糖(fmt)路径不受扰。
- **坑位:** 前奏注册与 std 文件实现两条路——按 r2a_sb 锚头注裁决;若走 std 文件则需 `use`,与「前奏隐式可用」的规范语义有差,以锚文件为准。

### T04 · W8020/W8030/W8040 负锚补齐(§5.6/注册表)

- **预估:** 0.5 d。**前置:** 无。**状态:** ✅ 已完成(2026-09-28)
- **目标:** 三个「已发射,锚待补」码立锚:W8020 must-use(Result/Option 丢弃)、W8030 未使用绑定、W8040 遮蔽前奏。
- **范围:** `tests/` 新增三个 `.lint.ct`;若发射面有缺口补 `compiler/src` 相应 sem 检查。
- **要点:** 注册表(`tests/meta_check.py`)三码已在册,只差锚;先写锚跑一遍验证「发射」属实,不属实则该件升级为补实现。
- **验收:** 三锚进 `meta_check.py` 对应映射;ci.sh 全绿。
- **坑位:** W8030/W8040 易误报(下划线占位、`_` 开头名规范允许内部用),锚样例要含豁免正例文件。

### T05 · r1a 行尾 `.` 自举解析器守卫(§1.6)

- **预估:** 0.5 d。**前置:** 无。**状态:** ✅ 已完成(2026-09-28)
- **目标:** 自举解析器对行尾 `.` 产出 E1001(宿主已正确拒绝),翻转 `tests/roadmap/r1a_trailing_dot.neg.ct`。
- **范围:** `compiler/src/parse_expr.ct` 成员访问链解析处;换行规则在 lexer 已有延续集机制(`lex.ct`)。
- **要点:** 行首 `.` 永远是续行、行尾 `.` 非法——自举侧大概率是 postfix 循环吞掉了 NL 检查;对齐宿主线判据。
- **验收:** r1a 翻转;`tests/01d_strings.ct` 等方法链正例全量回归(首点式不受扰)。
- **坑位:** 三线解析器对 NL 的表示不同,改动须 suite.py 双跑确认;fmt 产物是首点式,fmt 套件回归一并跑。

### T06 · E6030 parametricity 检查(§8.4)

- **预估:** 0.5–1 d。**前置:** 无。**状态:** ✅ 已完成(2026-09-28;封闭性论证承载,违规不可达)
- **目标:** comptime 体内禁反射泛型参数运行时类型:码位已在注册表,补实现 + 锚。
- **范围:** `compiler/src/sem_comptime.ct`(E6020 副作用扫描同族路径加类型反射面检查)。
- **要点:** v0 comptime 值模型里「反射」形态 = 对型参 T 取 `.show()` 之外的型别名/型别码分支(如按 T 分派 match 型别字面量);按现有 comptime 表达式白名单,超出白名单即 E6020——先审计现有 ceval 白名单是否已天然封闭,**若已封闭则 E6030 仅需锚 + 文档注记(封闭性论证)**,勿造重复检查(能力优先,不做死码)。
- **验收:** 注册表 E6030 从「预留」转「已实现」或「由白名单封闭性承载(论证入 COVERAGE)」;`04h_comptime_stmt.ct` 回归。
- **坑位:** 勿为「有码必实现」造空检查——先证明现状是否已满足,结论写进 COVERAGE。

### T07 · 单态化实例预算 8192(§8.5)

- **预估:** 0.5–1 d。**前置:** 无。**状态:** ✅ 已完成(2026-09-28;env 旋钮 CTRON_MONO_BUDGET,推断位登记并入)
- **目标:** 每包单态化实例总数上限(默认 8192,清单可调),超限诊断建议 `&Trait` 化。
- **范围:** `compiler/src/trans_ty.ct`(单态化特化表 `t_名__<实参码>` 处计数)、`parse_pkg.ct`(manifest `[profile]` 或新键读阈值)、诊断码位申请(注册表新增,先入 `meta_check.py` 再用)。
- **要点:** 特化表已有共享机制(同组合复用一份),计数即表长;阈值从 Ctron.toml/CTCL 读,缺省 8192。
- **验收:** 新 neg 锚(构造超限小样,阈值可调低复现);`fx_generic.ct`/`fx_genrec_neg.ct` 回归。
- **坑位:** 码位纪律「新增码先进表再使用」;与递归特化诊断(fx_genrec)区分:那件是环,这件是量。

### T08 · comptime 预算口径收口(§8.4)【需用户裁决】

- **预估:** 0.5 d(裁决)+ 1 d(实现,若裁实现)。**前置:** 用户裁决。**状态:** ✅ 已完成(2026-09-28;裁决=步数预算 v1 终态,budget_ms 键定性声明位随 T48 定夺)
- **目标:** 规范承诺 1s 时间预算,实现为 1200 步数预算(`sem_ceval.ct`)。二选一:**(a)实现时间预算**(步数预算保留为辅助);**(b)规范修订回写「步数预算为 v1 终态」**。
- **要点:** 时间预算引入时钟依赖,与「确定性编译」(T20)有张力——同输入同步数可复现,同输入同时长不可复现;**倾向 (b)**:步数预算确定性友好,1s 口径随 T20 一并重审。裁决后走对应路径。
- **验收:** (b) 路径 = `docs/spec/08-effects-comptime.md` 挂修订注 + `tests/modules/comptime_budget/` 注记对齐;(a) 路径 = 时间门实现 + 双口径测试。
- **坑位:** 裁决前勿动工;这是规范↔实现二选一,不是缺陷修复。

---

## W2 网络/并发小件(件间基本独立)

### T09 · resolve → `List[SocketAddr]` 双栈多记录(§11.5)

- **预估:** 1–1.5 d。**前置:** 无。**状态:** ✅ 已完成(2026-09-28;v0 List[Str] 文本承载,struct 形随 T14)
- **目标:** `resolve(host)` 返回多记录(`List[SocketAddr]`),替换 `ctron_net_resolve_first` 单记录面;门面 API 变更,同形双运行时(coro/P1)都要过。
- **范围:** `net/c_src/ctron_net.c`(getaddrinfo 迭代全记录)、`net/bind.ct`/`net.ct` 门面签名、`tests/net/`(新夹具 + 既有消费方回切)。
- **要点:** SocketAddr 值 struct(v4/v6 族标记 + 字节数组);helper 池语义(§11.5.1)不变——池返回的是记录链而非首条;happy-eyeballs 仍是志向档不做。
- **验收:** `tests/net/` 新夹具(localhost 多记录/数值 host 单记录/域不存在 Err)双矩阵 14→N 全绿;`db/pg.ct` 等消费方回切或适配。
- **坑位:** 跨运行时同形是 §11.4 机械不变式,c_smoke 差分必跑;struct 值过通道禁令(T14 未完成前)勿把记录 List 塞 Channel。

### T10 · `sleep_ns` + 可取消 + 虚拟时钟(§11.6)

- **预估:** 1.5–2 d。**前置:** 无。**状态:** ✅ 已完成(2026-09-28)
- **目标:** ①`sleep_ns` 纳秒面(现 `ctron_net_sleep_ms`);②挂起点响应取消令牌(§11.4);③测试虚拟时钟(可跳变、定时器确定性触发)。
- **范围:** `net/c_src/ctron_net.c`+`ctron_rt.c`(定时器堆已是 ns 底座)、`net.ct` 门面、`tests/net/`。
- **要点:** 虚拟时钟 = 测试模式注入时钟源(`CTRON_CLOCK=virtual` env 口径,与既有 CTRON_EMIT 同风格);rt 定时器堆键改 ns;阻塞运行时口径 sleep 即 nanosleep+取消=join 等效。
- **验收:** 双矩阵 sleep/skip 测试;虚拟时钟跳变夹具(定时器过期批量触发顺序确定);now_ns 单调性回归。
- **坑位:** darwin `clock_gettime` 已在用,ns 无新坑;取消唤醒路径已有取消广播机制,挂 sleep 进既有 park/wake。

### T11 · 能力键细分(§8.2/§11.1/§12.1)

- **预估:** 0.5–1 d。**前置:** 无。**状态:** ✅ 已完成(2026-09-28)
- **目标:** `[caps]` 从命名空间级(net/db)细分到 `net.listen/net.connect/net.resolve/db.connect`;fs/time 维持现状(规范未细分)。
- **范围:** `compiler/src/parse_pkg.ct`(`pkg_caps_allowed` 键表 + 门面函数→键映射)、`tests/modules/caps_net/` 扩展。
- **要点:** 映射点:`net_tcp_listen/net_unix_*`→listen;`net_tcp_connect/net_udp_*`→connect;`net_resolve_*`→resolve;db 门面→db.connect。越权 E4010 消息携带缺的具体键名。
- **验收:** caps_net 夹具扩为四键各有正/负例;在库消费方清单升级(todos/todo_api 等实际用哪键补哪键);E4010 既有锚不红。
- **坑位:** 既有 `caps = ["net"]` 粗键向后兼容裁决:**不兼容硬切**(fail-closed,清单写什么有什么)——在库夹具同步升级,规范 §2.7 语义本就是「实际使用集 ⊆ 声明集」。

### T12 · gzip 压缩面(§11.7)

- **预估:** 0.5–1 d。**前置:** 无。**状态:** ✅ 已完成(2026-09-28;主体系 webfw 泳道先落地,计划卡系分析时序差)
- **目标:** HTTP 压缩协商补 gzip(现仅 deflate),`Accept-Encoding`/`Content-Encoding` 协商全。
- **范围:** `http/enc.ct`+`binddeflate.ct`(deflate 即 zlib 流,gzip = 头尾包装 + CRC32,`std/hash.ct` 或 `std/crypto.ct` 已有 CRC 面则复用)。
- **要点:** gzip 与 deflate 同为 DEFLATE 算法,差异仅在容器格式(gzip 头 10B + CRC32/ISIZE 尾);纯 Ctron 容器包装即可,零新绑定。
- **验收:** `tests/http/` 编解码回环 + 与系统 gzip 互操作夹具(python gzip 生成/解压差分);http 套件回归。
- **坑位:** CRC32 若 std 无,按 RFC 1952 表驱动纯 Ctron 实现(无位算符坑:查表法只有乘加异或?异或也没有——查表法需 xor!改用算术等价或走 T01 内建 bit;**本件依赖序:若 std 无 CRC,则 T01 先行**)。

### T13 · Atomic 真原子(§7.3)

- **预估:** 0.5–1 d。**前置:** 无。**状态:** ✅ 已完成(2026-09-28)
- **目标:** `Atomic[I32].fetch_add/load/store` 从全局 pthread 锁(`ct_glock/ct_gunlock`)换真原子(`__atomic` builtins / C11 atomics)。
- **范围:** `compiler/src/driver_emit.ct` 运行时模板(emit 侧);解释器侧本来就是单线程无碍。
- **要点:** emit 模板 `ct_atomic_fetch_add` 等直发 `__atomic_fetch_add(&v, d, __ATOMIC_SEQ_CST)`;Global[T] 的 with/with_mut 仍走 Mutex(那是互斥语义,不是原子语义,分清)。
- **验收:** 并发压测夹具(N 任务 × M 次 fetch_add,终值精确等于 N×M——全局锁版也能过,加内存序敏感样例难;至少跑既有 `fx_conc_atomic.ct` + 新增跨核争用 bench 冒烟);net 双矩阵回归。
- **坑位:** emit 模板是字符串内嵌 C,注意 `\{` 转义与 20480B 直发限制(改模板不触此限)。

### T14 · Channel 去限制(§7.3)

- **预估:** 1.5–2 d。**前置:** 无(T33 并行化不依赖此件,可独立)。**状态:** ✅ 已完成(2026-09-28 三片落地;侦察笔记留档)
- **侦察笔记(2026-09-28,W2 执行时实地确认)**:
  - 运行时三处限制(driver_emit.ct:239-328):①buf[64] 定长栈内环(堆化=calloc at make)
    ②send/recv 取模 `% 64` 硬编码(须改 `% c->cap`)③ct_chans[64] 注册表定长(取消广播
    扫描面,改增长数组)。cap<=0→64 现行为保留。
  - struct 值入通道 = **发射器类型层贯通件**(与 List[struct] 同底座):
    断点=Channel[T](cap) 构造点知道 T,但 ①send 点装箱(struct→堆盒→ct_i 槽)、
    ②recv 点拆箱(ct_res.v→struct 拷贝)都需要元素型码;env 现不记录通道元素类型
    (ct_typeof 只出 "CH" 类粗码,推断 receiver 绑定元素型的通道 = List[struct] 的
    "LPt" 元素码方案同源:构造点/注解点把元素码写入 env,收发点查询)。
    ③interp 侧 call_mem "CH" send/recv 已是值直存(host List 语义)天然支持 struct——
    只需放行 sem 的 E3020/ct_wrap_i panic 面。
  - Sender/Receiver 作 struct 字段:sem 放行(值句柄本就是指针槽)+ db/pool.ct
    组合层等待回切原生(pool_wait.ct 夹具在案)。
  - 验证矩阵:cap>64 背压(cap=128 双任务往返)、struct 通道往返(源端改后接收端不变)、
    db/pool 套件、coro 双矩阵、06_concurrency 回归。
  - 关联在册:divergences「todo_app 发射爆炸」fn 值形参 ct_clop 野跳同族(通道槽形改造
    时一并审视);T33 真并行复用本件环缓冲。
- **目标:** 三件:①缓冲上限 64 解除(堆分配环形缓冲);②struct 值可入通道;③Sender/Receiver 可作 struct 字段(`db/pool.ct` 登记偏差销账)。
- **范围:** `compiler/src/driver_emit.ct`(`ct_chan.buf[64]` 定长栈槽→堆 ring)、`trans_conc.ct`(通道值表示从标量槽改携带 struct 拷贝)、`sem_send.ct`(Send 检查面已通,查放行点)。
- **要点:** struct 值入通道 = 值拷贝语义(struct 赋值即拷贝,§3.2),通道槽从 int64 改定宽字节槽 + 型别码;Sender/Receiver 作字段 = 它们本就是值句柄,主要解锁 sem/发射的字段声明面 + `db/pool.ct` 回切。
- **验收:** `tests/` 通道族(cap>64 背压、struct 传值互不干扰[源端改后接收端不变]、pool 组合层等待回切原生);`06_channel_nonsend.neg.ct` 负例不红;db/pool 套件绿。
- **坑位:** 这是登记偏差销账件,`docs/c-rust-divergences.md` 有账,完成后销账;interp 侧 struct 通道禁令 panic 路径同步移除,双侧同判。

---

## W3 迭代器协议(R-P3b,链式串行)

### T15 · `Iterator[T]` trait + for 集成

- **预估:** 1.5–2 d。**前置:** 无。**状态:** ✅ 已完成(2026-09-28;三线 interp,锚迁 03k;发射臂 var-self 引用语义在册)
- **目标:** 用户 `impl Iterator[T] for X { fn next(&var self) -> T? }` 即可 `for x in xs`;翻转 `tests/roadmap/r3b_iter_trait.ct`。
- **范围:** `compiler/src/sem_walk.ct`/`sem_calls.ct`(for 目标解迭代协议:有 next 方法即协议满足——结构化满足谓词,同 derive/Show/Eq 口径,§3.9.2 先例)、`eval_run.ct`/`trans_stmt.ct`(for 循环糖→next 调用循环)。
- **要点:** 锚头注两可点裁决:`next` 的 self 形态按锚起草的 `&var self`;Range/List/切片/数组的既有 for 旁路保留(内建快路径),Iterator 协议是通用出口——**能力优先:协议是扩展,不是替换内建路径**。前奏 trait `Clone`/`Hash`(§3.8.1)同件裁决:有消费场景(深拷贝/哈希容器键)则随本件立协议,无消费场景则规范修订从清单移除或声明由 `@derive`/结构化谓词承载——**勿留无名注册的死 trait**,结论入 COVERAGE。
- **验收:** r3b_iter_trait 翻转;内建 for 全量回归(既有性能路径零扰动);`T?` 返回 None 终止语义锚。
- **坑位:** `&var self` 可变性要求 for 绑定目标根为 var 或临时值——sem 可变性判定复用 §4.2 路径规则;发射侧 next 直接调用户 fn(静态型别,无需虚表——泛型单态化路径,与 T25 解耦)。

### T16 · 惰性适配器链(r3b_adapters)

- **预估:** 1–1.5 d。**前置:** T15。**状态:** ✅ 已完成(2026-10-02;Seq[T,S] get 基惰性适配器 + UFCS 入口,锚迁 modules/iter_adapters;suite 99/99 双臂;发射面在册 T25 v2 域)。**续波收口(2026-10-03,R-P3b 承诺余项)**:适配器补 skip/take_while/rev/enumerate/zip/chain + 终结子补 fold/reduce/foreach/min/max/last/position + from_list 接入 List(ListSeq 包装——前奏 class 直 impl 派发不挂,新证);锚扩 iter_adapters 十二段;探针四证(无返回 fn 型/泛型 T 比较/class var 字段/Pair 推断)+ 接收者拷贝边界新证(class 经 struct 字段存储被拷贝);登记债追加,见 COVERAGE 2026-10-03 行
- **目标:** `iter.map/filter/take/...` 适配器作用 Iterator 之上,惰性、单态化零成本;翻转 `tests/roadmap/r3b_adapters.ct`。
- **范围:** `std/`(新 `iter.ct` 或随 T15 文件);纯 stdlib 实现(适配器 = 持 Iterator 的 struct + impl Iterator),**编译器零改动为设计目标**。
- **要点:** 每适配器一个泛型 struct(如 `struct MapIter[A, B] { it: A, f: fn(A) -> B? }`——fn 值作字段,函数类型字段面 §3.1 已有);惰性 = next 时才调 f;单态化自动获得零成本。
- **验收:** r3b_adapters 翻转;适配器组合冒烟(map+filter+take 链);发射产物含特化(链不塌 box)。
- **坑位:** 闭包捕获语义=拷贝终态(T27 已裁决转正),适配器内 fn 字段只存纯函数/捕获闭包——锚样例避免依赖按引用共享(T27 终态下引用捕获断言不成立,创建时拷贝即规范)。
- **落地形态(2026-10-02 收口)**:`std/iter.ct` v2 = `trait Seq[T, S]`(get 基:fn get(var self, i) -> T? 纯位置读取 + map/filter/take/sum/count/collect/any/all 默认方法)+ RawSeq/MapSeq/FilterSeq/TakeSeq 泛型 struct(类型正确 impl 形 `impl Seq[T, MapSeq[T,S]] for MapSeq[T,S]`)+ 数组 UFCS 自由函数入口(`pub fn map(xs: I32[], f)` 等,§4.8 首参接收者)。**get 基而非 next 基的裁决**:interp 逐调用拷贝接收者,var-self 写回语义在册(T15 发射臂登记同源),next 基适配器叠适配器永不推进(p12 实证挂死);get(i) 无状态,链任意深度组合安全。自定义 Iterator 经 for-重放 get 接入(03k 语义)。锚迁 `tests/modules/iter_adapters`(带 use 的多文件包:自举臂运行+宿主 pkg check——宿主无包运行口径,单文件 use 不合并)。锚修:原稿 `!xs.any(|x| x>5)` 与 xs=[2,4,6] 矛盾,首翻绿校正为正断言。
- **余债登记**:①发射臂 trait 方法调用未发射(t_bump/t_hi 未定义,04g/p11 实证——T25 v1 仅 &Trait 对象面)+ for-over-Iterator 未发射(03k emit `ct_stmt:for iter:Ident`);②var-self 写回语义(interp p6/p7/p8 实证不写回,for 糖 env 线程化为唯一特路;发射臂按值传 mutation 丢,T15 已登记)= 泛 var 引用语义系统改造;③C 宿主解析器不认 `pub trait`/`pub struct`(parse_decl TOK_PUB 只路由 fn);④自宿装载器 `pub trait`/`pub struct`+泛型字面量经 use 合并即坏(E2020 错判/E5030 幽灵)——std/iter.ct 以非 pub trait(T26 恒可见口径)+非 pub struct 绕行,④为 loader×pub 独立 bug 待修;⑤Rust 参考臂 get 基链覆盖随 T25 v2 后评估(roadmap_suite 表行已按翻转协议删除)。

### T17 · std/iter 模块归位 + parallel 迁 stdlib(§7.7)

- **预估:** 1 d。**前置:** T15(可 T16 后)。**状态:** ✅ 已完成(2026-09-28;06f 翻转,suite 89/89 首次 100%)
- **目标:** 并发设施按规范归 stdlib `iter` 模块:`parallel.map/reduce/fold` API 面 std 化(实现仍可用编译器内建背书);顺带把 T16 适配器纳入同一模块面。
- **范围:** 新 `std/iter.ct`(域面汇聚);`compiler/src/sem_calls.ct`(parallel 前奏保留但文档标注「stdlib 化过渡」或迁出)。
- **要点:** 分两步:①API 面 std 化(std/iter.ct 薄包装内建 parallel);②内建面转纯 stdlib 依赖 T33 真并行后一并做——本件只做 ①,规范 §7.7「stdlib,iter 模块」的归属落位。
- **验收:** `use std.iter` 后 parallel.map 可用;前奏直用 parallel 保持兼容(过渡双轨,迁移注记入 COVERAGE);`06f_parallel.ct` 回归。
- **坑位:** 双轨期 decl 锁与符号唯一性;规范修订注(归属迁移)挂 §7.7。

---

## W4 诊断契约与红账(服务所有后续泳道的 agent 循环)

### T18 · 列级精确 span 贯通(§10.2)

- **预估:** 1.5–2 d。**前置:** 无。**状态:** ✅ 已完成(2026-09-28;pdiags JSON 精确 LINE:COL;sem 列=1 近似;部分 nline 垃圾值在册)
- **目标:** JSON 诊断 span 从行级(col 恒 1)到 1-based 列级;`driver_check.ct json_diag` 结构已冻结,填真值。
- **范围:** `compiler/src/lex.ct`(token 起止列已在词法层?先审计)→ `parse_*.ct` 节点 span 字段 → `sem_*` 诊断携带 → `driver_check.ct` 透出。
- **要点:** 先审计:lexer 是否已记列(`lex.ct` E1001.interp 报错质量暗示有部分位置面);缺口大概率在 AST 节点未携带 span——给 Node 加 `pos` 对(`sl,sc,el,ec` 四元组或字符偏移对,后者更省,列由 check 时换算)。
- **验收:** `ctron check --format=json` 对已知错样输出精确列(锚样例进 `tests/`);diag 双语消息不回退;probe 近似路径退役(或仅 fallback)。
- **坑位:** 节点加字段触 decl 锁与内存面(AST 分配放大——解释器 arena 债在册,量入为出:存字符偏移 int 对,勿存四元组字符串);全部 sem 检查点数量大,**分两片:先主干诊断(E1/E2/E3 高频)后长尾**,本件验收以主干为界。

### T19 · fix-it 首批(§10.1)

- **预估:** 1.5–2 d。**前置:** T18(消费 span)。**状态:** ✅ 已完成(2026-09-28;E3030 带 replace edit,E2020/W8010 带 note)
- **目标:** 高频码带机器可执行 `fixes`(JSON edits: replace/insert/delete)+ `notes`。首批选 5 码:E2010(类型不匹配→as 建议)、E2020(未解析名→就近名建议/漏 use 建议)、E3010(非 Send→Mutex 包装建议)、E3030(static var→Global[T] 建议)、W8010(浅共享→lint 注记)。
- **范围:** `compiler/src/driver_check.ct`(fixes 产出)、各 sem 检查点附建议载荷;`diag_msg.ct` 双语 notes。
- **要点:** fix-it 数据从诊断点就地构造(不搞独立「建议引擎」);edit span 用 T18 列级 span;就近名建议=编辑距离 ≤2 的符号表扫描(纯 Ctron,comptime 面可复用)。
- **验收:** 五码各有 JSON 契约锚(含 edits 结构断言);`meta_check.py` 契约扩展;无建议的码 `fixes:[]` 保持(契约允许空)。
- **坑位:** 契约冻结面——新增字段须走规范修订注(§10.2 挂 v0.8.x);建议错误比无建议更糟,每条 fix-it 必须带正例锚证明可编译。

### T20 · `--deterministic` + 内容寻址缓存(§10.3)

- **预估:** 1–1.5 d。**前置:** 无。**状态:** ✅ 已完成(2026-09-28;CTRON_RT_SEED 既有机制+CLI 旋钮;emit 缓存 key=源哈希+编译器哈希)
- **目标:** ①`ctron test --deterministic`:冻结调度序与哈希种子;②构建内容寻址缓存(同输入同产物)。
- **范围:** `compiler/src/driver_run.ct`(测试驱动)、发射运行时模板(种子注入 env)、`ctc.sh`。
- **要点:** ①调度冻结 = scope/spawn 的 worker 分配从时间片改确定性序(现有「确定性优先调度」已是过渡实现,加种子化哈希 + `--deterministic` 单 worker 串行档最简可达);②缓存键 = 源文件集哈希 + 编译器版本 + 目标,产物 `build/` 命中跳过;③顺带补 §5.3 构建配置面:release 档关闭 `?` 位置链采集(`[profile]` 键,除零永不关口径不受扰)。
- **验收:** 并发测试失败可复现锚(故意竞态样例在 deterministic 下稳定红/绿);缓存命中二跑产物逐字节一致(digest pin 惯例)。
- **坑位:** 单 worker 串行档会掩盖并发 bug——定位为「复现工具」非「验证档」,文档写明;缓存失效面(pathspec/mtimes)宁多勿漏。

### T21 · 宿主检查面红账五件

- **预估:** 1.5–2 d。**前置:** 无。**状态:** ✅ 已完成(2026-09-28;r6d/r6e 先前已绿,r6b/r6c/r6f 三件补齐)
- **目标:** C 宿主(`compiler-c/`)补五道检查,翻转 r6b(E4050)/r6c(as 负源 2^128 模)/r6d(W8050)/r6e(E4040)/r6f(comptime budget 不评)。
- **范围:** `compiler-c/` 对应 sem 检查面(逐件对照自举线实现移植)。
- **要点:** r6c 病灶已在册细化(wrap_int bits≥64 透传 + conv 误用 2^128 模,COVERAGE 240 行),按单修;五件独立,可拆小 PR。
- **验收:** 五锚翻转;`cargo test`/宿主套件全绿;suite.py 双跑零新增分歧。
- **坑位:** 宿主线与自举线结构不同源,移植的是**判据**不是代码;每件翻转前先读锚头注判据行。

### T22 · 宿主运行面红账两件

- **预估:** 1 d。**前置:** 无。**状态:** 待办
- **目标:** 翻转 r4d(宿主 rt panic 不跑 Drop 展开)与 r6a(U64 加法无上界检查)。
- **范围:** `compiler-c/` 运行时(rt panic 展开路径补 drop 派发;U64 Add 补上溢门)。
- **要点:** r4d 与自举线 `ct_drstack` 语义对齐(逆序、panic 保序);r6a 对照自举 `ctron_i64_add` 的 `__builtin_*_overflow` 形态。
- **验收:** 两锚翻转;`01_overflow.panic.ct` 族双线回归。
- **坑位:** 宿主 rt 无 longjmp 展开则需补——查实后若工程量大,拆两片:r6a 先行(小),r4d 挂专案注记。

### T23 · e2e 缺声明 6 件红销账

- **预估:** 1–1.5 d。**前置:** 无。**状态:** 待办
- **目标:** COVERAGE 368 行在册「e2e 缺声明 6 红(饿死族 client/message 面)」销账:P1b 复通后遗留的发射缺声明(t_client_write_str/t_http_method_start 类)补齐。
- **范围:** `compiler/src/trans_*.ct`/`driver_emit.ct`(声明补齐)、`tests/http/` e2e 红件。
- **要点:** 饿死族根因=一处发射缺声明饿死同族,按 09-24 复核记录逐件定位;修后 `run.sh` 站岗面回切原生 e2e(todo_app 红账同源可一并试);§11.2「fd 禁触达=发射面拒绝导出」的 E 锚同批登记(同属发射面收口在册账)。
- **验收:** 6 件转绿;todo_app 原生 e2e 红账状态复核并登记(同源则销,不同源另立)。
- **坑位:** 20480B 直发限制——e2e 产物大,走 CTRON_EMIT 覆盖口(在册惯例)。

---

## W5 trait 对象与闭包捕获(语言深水,链式)

### T24 · `&Trait` 虚表 ABI 设计定稿

- **预估:** 1–1.5 d(设计文档+评审)。**前置:** 无。**状态:** ✅ 已完成(2026-09-29;docs/superpowers/specs/2026-09-29-trait-object-abi.md)
- **目标:** 设计文档:对象表示(data ptr + vtable ptr)、vtable 布局(方法槽序=trait 声明序,含默认方法/prop getter)、上行转换零成本、Send 位承载(§7.4 动态 Send 位预留面)。产出 `docs/superpowers/specs/2026-09-28-trait-object-abi.md`。
- **要点:** 关键裁决项:①prop 在 vtable 是 getter 槽;②泛型 trait(省略:先只支持非泛型 trait 对象,泛型对象列 v2);③超 trait 的 vtable 前缀复用;④与 E3031(非 Send 静态存储)/E4042(C-ABI 回调禁捕获)交互;⑤能力对象(§8.1)是首要消费场景——`&Clock` 注入必须经此落地。
- **验收:** 设计文档过用户评审(裁决项逐条拍板);T25/T26 按此施工。
- **坑位:** 无 finalizer + GC 管存活(§6.2)意味着对象表示无需引用计数——好减法;vtable 静态常量放发射产物 rodata。

### T25 · 发射侧动态分发 codegen

- **预估:** 2–3 d。**前置:** T24。**状态:** ✅ 已完成(2026-09-29;型码+vtable+装箱+分发四步;引用门防 Drop/UFCS 干扰;非泛型+ct_i 返回=v1 面,prop/泛型/超 trait=v2)(0929 侦察:interp 侧 &Trait 已通——07_capabilities 全绿;发射侧完全缺位——&Trait 参数发射为 int32_t,无 vtable,StructLit 崩;最小路径=①&Trait 型码入 ct_ty_code(新码如 "t:<Trait>")②vtable 常量发射(static const vt_<T> vt_<T>_for_<Type>)③构造位装箱({data,vtable})④调用位 ((vt_T*)obj.vtable)->m(obj.data);2-3d 专注)
- **目标:** `trans_expr.ct` 对 `&Trait` 接收者方法调用发间接跳转(vtable 槽);上行转换发对象包装;解释器侧已有动态分派语义对齐即可。
- **范围:** `compiler/src/trans_expr.ct`(方法解析分派点)、`trans_ty.ct`(&Trait 表示码)、`driver_emit.ct`(vtable 常量发射)、测试锚。
- **要点:** 分派点识别:「接收者静态型别为 trait」的调用——sem 已知 trait 型别信息,直接槽号;非 trait 接收者路径(静态直调)零扰动。
- **验收:** 新锚(上行转换+动态分发+超 trait 默认方法);`07_capabilities.ct`/`04g_methods.ct` 回归;E3031/E4042 负例不红。
- **坑位:** ct_fn0..3 函数指针 ABI(§204-207)是现成跳转底座,槽类型对齐;20480B 直发限制(vtable 多则产物大,走 chk 口径验证)。

### T26 · 能力对象注入形态归位(§8.1/§11.2)

- **预估:** 1.5–2 d。**前置:** T25。**状态:** ✅ 已完成(2026-09-29;r2b_fs_fake/r2b_env 翻转主套件 07a/07b;Fs 能力 trait 入 std/fs.ct+FsError;loader Trait 恒可见;Env.system/get/args 三线能力对象(ctron_prog_args_* 运行时底座);发射 vtable 全签名扩展(带参槽+真返回+thunk 桥接+调用位实参);net cap_inject 真窗 FakeNet/StdNet 同槽注入双矩阵 18/18;@+class 解析路由修复;E4010/E4020 锚维持;db trait 化缓期=连接面随 P5,登记 COVERAGE)
- **目标:** `&Fs`/`&Clock` 能力注入成为可发射的主流形态:net/db 门面从「struct+自由函数」增加 trait 门面形(或规范回写 as-built 形态为终态——**二选一需评审**,倾向前者,能力优先裁决)。
- **范围:** `net.ct`/`db/*.ct`(trait 门面层)、E4010 审计锚点迁移(现 `&Net` 形参仅审计)、`tests/roadmap/r2b_env.ct`/`r2b_fs_fake.ct` 翻转。
- **要点:** FakeClock/FakeFs 测试注入模式是验收核心(§8.1「无需 mock 框架」);E4020 判定已按 `&Cap` 接收者调用实现(`sem_pure.ct`),trait 化后自然贯通。
- **验收:** r2b 系列翻转(能力注入正例真窗跑通);caps 锚不红;`#[pure]` 触网 E4020 锚不红。
- **坑位:** 门面函数是 26 个 extern(`net/bind.ct`),trait impl 包一层薄路由即可,勿动 C 垫片;若裁「回写 as-built」,则规范 §8.1/§11.2 示例改写 + 本件缩为注记件。

### T27 · 闭包捕获语义收口【需用户裁决】

- **预估:** 0.5 d(裁决)+ 2–3 d(实现,若裁按引用)。**前置:** T29(GC MVP)后实现。**状态:** ✅ 已完成·B 终态转正(2026-09-30 用户裁决,台账 619 行;**10-02 残余收口**:§4.7 补快照句+§8.4 错引改 §6.2+§6.2 补能力扩展位 bullet+§10 E3070 行翻终态+README T27 版记+R-P3a 只读借用条款终态注+r3a 锚头注回切+T55 快照边界注翻面[COVERAGE/挂起节];r3a 锚解释臂双宿主复验绿,原生臂红=在册 T33 ct_emit_clo 卫兵错位[p0a 合流后复验,非本件债];**10-03 原生臂复验收官=r3a 双臂全绿,本件终态无残余**:红因经干净基线二分实证非卫兵[ct_emit_clov 卫兵形态=单对 #ifndef 包整 shim,完好]——p0a 合并 46e11d70 引爆 ct_str_bsum I32 乘 31 链溢出(判别位新入捕获名/型别码,`|x| x + base` 第 3 字符即破界)+闭包体尾槽/return 位 with·with_mut ANF 漏接(ct_expr「仅语句位」恒拒,eval 两侧皆合法)+同 fn 多 with 同名形参 C 顶层重定义,三修落码 trans_conc.ct/trans_stmt.ct(修法详注=r3a 锚头注);fx_cloval/探针四形/r3a 发射+cc+运行全绿,suite 99/99+smoke 170/3(余 2 红=T33 parallel OOB/Rust iter 清账,皆在册非本件);连带小修=固定点段版本行竞态过滤(两路 build.sh 各自采样 git describe,机刷并发落库窗口即假红)+GATEDBG2 调试打印清退(trans_expr p0a 遗留发射产物污染)+decl 锁 444→449 随批申报(+1 本件 ct_clo_with_tail,+4 GUI-17/12 批已落库未申报归 GUI 泳道);**登记债=with 在任意表达式位 emit ANF 缺口(eval 合法,`return bump() + m.with(...)` 形;语句/let/return 尾/闭包体尾已全接,任意嵌套表达式位待 ANF 提升机,capability 项非 hack)**;T33 parallel ct_emit_clo OOB 独立在册非本件)
- **业界调研(0929,评估素材存档)**:有 GC 阵营(JS/Go/C#/Swift)普遍按引用共享绑定(循环变量为公共事故源,Go 1.22/JS let 均向每轮迭代绑定修补);无 GC 阵营(Rust/C++/ObjC)走拷贝/显式(借用户生命周期或自负安全);Java/Kotlin 的 effectively-final 绑定拷贝 + 显式共享可变单元,十年稳定。**映射**:Ctron 现行 E3070+Mutex 纪律 ≈ Java 系绑定拷贝 + 共享单元模型;A(按引用)=T29 后的 GC 阵营主流位;B(拷贝终态)=现行事实行为升格,与 arena 无 GC 自洽;C(混合)≈ 现状 de-facto(同任务共享 arena+spawn 边界 bind_of 克隆)但双臂 seed/native 拷贝分歧未修前不宜形式化。**T29 后评估时**:若 GC 落地且用户需按引用,按能力扩展立项(不与 B 冲突)。

**T29 后复评补记(0930,评估条件已达成)**:T29 MVP tracing GC 已落库(保守根集:
主栈+jmp_buf 寄存器+全局表+chans)。三选项的更新事实:①A(按引用)=捕获变量提升
GC 单元,生命周期已被保守 GC 兜底 ✓,但跨任务可变竞争纪律仍需 Mutex 门(E3070 不消失,
只收窄到 spawn 边界),实现 = 发射侧捕获变量 cell 提升(2-3d)+ 双臂结构拷贝分歧先修;
②B(拷贝终态)= 现行 E3070+Mutex 纪律的文档化升格,零实现债;业界锚 = Java/Kotlin
effectively-final 十年 + Go 1.22/JS-let 循环修补方向;③C(混合)= 事实行为(同任务
共享+spawn 边界 bind_of 克隆),形式化需 sem 逃逸分析(spawn 显式可判),且双臂
seed 深拷贝/native 浅拷贝分歧(registered)未修前 C 的"同任务按引用"双臂不一致。
- **目标:** 规范 §4.7「捕获按引用语义共享(full 档,GC 管存活)」vs 实现「拷贝捕获」(r3a_capture 头注/R-P3a 路线)。裁决:**(a)实现按引用共享**(env 为 GC 堆记录);**(b)规范回写拷贝捕获为 v1 终态**(R-P3a 即此设计)。
- **要点:** (a) 的收益:迭代器/回调形态自然,`var` 捕获经 Mutex 已有 E3070 门;(a) 的代价:闭包 env 进 GC 堆,与 T29 MVP 时点耦合。(b) 的代价:与现行规范文本冲突,须修订;逃逸闭包(env 随闭包存活)拷贝语义已可支撑(r3a 解释器绿)。
- **验收:** 裁决记录入规范文档(修订注或确认注);(a) 路径 = r3a 发射侧差分转绿 + 捕获别名写可见锚;(b) 路径 = §4.7 修订 + r3a 判据回写。
- **坑位:** r3b_adapters(T16)若有断言依赖捕获语义,随裁决回切——T16 卡已注。
- **复评材料(2026-10-01,T31 判决后,裁决门已解锁)**:T31 实证把 (a) 的代价显著抬高——
  M1 保守 GC 对深栈程序不稳(RSS 4GB+强制收集 SIGSEGV,COVERAGE 工作志 2026-10-01),
  而闭包逃逸恰是深栈+久堆形态;(a) 现在隐含前置 = M1.5 精确根集,2–3 d 之上再叠
  M1.5 体量。(b) 与语言重心自洽:arena 无 GC 为默认档+into_gc 显式入 GC 堆的二元
  模型已落库(T29/T30),拷贝捕获+E3070/Mutex 共享单元 ≈ Java 系十年稳定形态。
  **建议:裁 B(拷贝捕获为 v1 终态,§4.7 修订回写+R-P3a 判据回写,0.5 d 文档件);
  A 保留为 GC 阵营能力扩展位(M1.5 后按需立项,与 B 不冲突)**——即卡内调研映射的
  既有结论,本次由 T31 实证背书。待用户裁决。

---

## W6 GC 系列(最大空洞,主链串行;T29 起须 S1 Val 落库后)

### T28 · GC 契约设计按现状校准定稿

- **预估:** 1 d。**前置:** 无(可与 W1–W4 并行)。**状态:** 待办
- **目标:** 已有 `docs/superpowers/specs/2026-09-15-gc-contract-design.md`——按实现现状(bump arena/无栈扫描/into_gc 恒等)校准出**分阶段契约**:M1 非移动 mark-sweep(全局暂停)、M2 增量/分代、M3 并发(承诺 P99<0.5ms 归 M3);每阶段入口/出口判据、精确栈扫描实现径(发射侧栈帧布局已由 trans 掌控——精确扫描可行性的关键论据)。
- **验收:** 设计文档评审通过;T29–T32 按此施工;规范 §6.2 挂「实现阶段映射」注。
- **坑位:** 值类型 Drop 与 GC 无交互(§6.4 硬规则已豁免类持资源+E4050 门)——这是 Ctron GC 比 Rust/Go 简单的根本点,设计要吃满;类引用环可达性是 mark-sweep 天然解决项(承诺「无循环引用泄漏」唯一实现径);own 借用检查(E3060「env 近似」简化子集)随 arena 与 GC 堆分离在本件重审完备化判据。

### T29 · MVP tracing GC(替换 bump-only)

- **预估:** 4–5 d。**前置:** T28 + S1 Val 落库。**状态:** ✅ 已完成(0930 验收面全过:泄漏锚双档✓/suite CTRON_GC=off 双档✓/net+db GC 档绿[db GC 臂=编译器臂 env -u 豁免,GC=1 与基线同红集无害性实证];两史归一 main 24e0233;**切片3 在册**:精确帧位图 M1.5[契约 §6.3]+GC 默认档翻面[已证伪推迟:自举编译器巨型 arena 保守扫描近挂死——env 继承激活编译器自身 GC,须先 arena 界定或豁免机制];另登记:peer 库根重构 db emit 泳道 +5 红[pg_* 符号 E2020]与 5×种子漂移守卫路径陈旧,非 GC 债)
- **目标:** 发射运行时新增 mark-sweep 回收器:类实例/List/String/Box 分配可回收;分配器可插拔口(env 或链接期选择:`CTRON_GC=off` 退 bump,bump 保留为 own/arena 专用——own 语义本就不进 GC)。
- **范围:** 新 `compiler/src` 运行时模板段(或独立 `ctron_gc.c` 垫片文件,emit 打包)、`driver_emit.ct`(分配调用改 gc_alloc)、栈扫描(精确:trans 已知栈槽型别图;M1 可先保守对齐扫描但须设计注记)。
- **要点:** ①对象头(标记位+型别描述子指针);②型别描述子表(发射期生成,字段布局/引用字段位图);③根集(栈+Global+任务栈——协程栈扫描是难点,MVP 可先 P1 阻塞运行时 supported,coro 列 M1.5);④触发点(分配量阈值)。
- **验收:** 泄漏锚(循环类引用回收——bump 版必泄漏,GC 版回收,前后 RSS 差分断言);全量套件回归(CTRON_GC=off 双档);net/db 双矩阵 GC 档绿。
- **坑位:** 协程栈扫描不可行期的降级面 = coro 运行时暂持 bump(文档明记,能力优先不 hack);SSB/写屏障 M1 不需要(mark-sweep 无写屏障);与 ft_shim/自由链 C 侧内存(FFI)边界:Ctron-owned 才归 GC。

### T30 · `into_gc` 深拷贝语义真实化(§6.3)

- **预估:** 1–1.5 d。**前置:** T29。**状态:** ✅ 已完成(0930;发射侧真深拷=let #elem/#elemsz/#ielem 侧条目+ct_ig_l 静态模式分派[0 标量/1 Str/2 嵌套一层/3 struct 盒],无绑定位恒等回退 v1 兼容;C 宿主 list_clone_deep 内部 V_LIST 槽递归[嵌套别名 2!=1 实证;clone_val 通用口语义不动,误改曾致 conc 15 红 smoke 实证回退];新锚 05i_into_gc_deep_copy[int/Str 平坦+出块存活]双臂双档绿;05g emit 臂红=存量 own 块尾 return void 缺口[A/B 基线实证]登记;嵌套读侧 lane strcmp 缺口登记;顺手修 peer 库根重构 suite.py stdpath 两处[主循环裸跑+modules 段→lib/std,99/99 复绿])
- **目标:** arena 与 GC 堆物理分离后,`into_gc()` 发射侧从恒等变真深拷贝(O(数据量)),翻转规范 v1 语义;`tests/05g_into_gc_isolation.ct` 语义复核。
- **范围:** `trans_expr.ct`(into_gc 发射:遍历 arena 值按型别图深拷入 GC 堆)、型别描述子(T29 产出复用)。
- **验收:** 隔离锚(出块后源 arena 释放,GC 副本存活且独立);05/05g 族回归;GC=off 档下 into_gc 恒等(文档明记)。
- **坑位:** 嵌套 List/struct 递归拷贝深度(栈深防护:深度超限 panic,消息含 "into_gc depth")。

### T31 · 解释器堆回收(arena 治理,114× 慢销账)

- **预估:** 1.5–2 d。**前置:** T29(共享型别图思想,工程独立)。**状态:** 🔴 负结果落账(1001;Ctron 级帧回收证伪:mark/reset+逃逸否决[闭包/push/send/with/字段写]+标量返回门全实施后,suite user 2.62→3.22s[+23%]且 06d globals 红[Global 写回逃逸未盖]——**每调用 Ctron 级 wrapper 解释开销 > 回收收益,逃逸面未穷尽**;已干净回退 99/99 复绿;重开前置=值模型原生 mark/reset[eval Val 换代/宿主 C 级帧钩子],列 M2 面;模板助手 ct_amark/ct_areset/ct_eveto_* 设计稿留存本卡)+ 🟡 路线②GC=1 委托同日证伪(双泳道并线:纯委托 T29 首适应漫步挂死+尺寸桶后深栈保守伪标 RSS 4GB/SIGSEGV,详见卡底判决)——**两路合判:唯一健全路径=M1.5 精确帧位图(GC 契约切片3),本卡随其并案**
- **目标:** 自举解释器每步 ~2K×16B arena 分配无回收(v0.0.1 在册债)——解释器堆加回收或步级复用;既是性能债也是 emit 臂大 corpus 腐坏规避的根(COVERAGE P6-D 登记语)。
- **范围:** `compiler/src/eval_*.ct` 堆分配路径(arena 帧复用:函数调用帧出栈即重置——树遍历解释器的天然回收点)。
- **验收:** bench 前后差分(解释器整机耗时比值,目标显著收敛于 114× 基线,digest pin ×3);全量套件回归。
- **坑位:** 帧复用要求「帧内分配不出帧存活」审计(闭包逃逸 r3a 解释器面!)——逃逸值晋升久堆,逐点登记。

> **2026-10-01 探路判决**:①前提过期——114× 已被 S1 表示换代销至 4×(loop 810→140 次/迭代,
> 2026-09-25-eval-repr-overhaul.md 收官注);剩余真债 = 无回收内存无界,实证 frm_auth_a_jwt
> interp 臂 **41GB 峰值/136s**(3 亿分配,CI OOM 唯一红根因;76s sys = 零填页缺页税)。
> ②卡面原案「帧出栈重置」证伪:Str/List 全指针共享,别名即语义,逃逸拷贝晋升破坏引用
> 语义——审计在本架构不可满足。③GC=1 委托证伪两连:纯委托在 T29 首适应分配器漫步退化
> 直接挂死(旧 GC 档仅 list 头进堆=尺寸均一故无此病);尺寸桶修复 CPU 后,深栈保守伪标
> →RSS 4GB + 强制真收集 SIGSEGV(内部指针载荷当头读=野扫)。对照实验排除新码嫌疑。
> ④判决:M1.5 精确帧位图为唯一健全路径,T31 债随其销;a_jwt 红账解锁条件=M1.5。
> 本会话落库=since 计数硬化(GC 空闲链复用路径永不触发收集的 T29 潜在 bug)+全量证据入册
> (COVERAGE 工作志 2026-10-01)。T32 翻面前置同理改写为「M1.5 后」。
>
> **M1.5 重开施工中(2026-10-02,计划 docs/superpowers/plans/2026-10-02-t31-m15-precise-gc.md)**:
> **S1 ✅ 标记器硬化落库**(超块单尺寸类+内部指针 O(1) 块起点归约+头双门+calloc 清零+
> worklist 迭代标记)——路径② SIGSEGV 两机制(载荷当头读野扫/递归标记栈爆)销账;
> deep_stack 锚(4000 层深递归×帧驻 List×125 次深栈收集)双档绿;**意外红利:T32 bench
> 比值 10.28→1.000-1.150(§9.4 ≤1.15 目标首次达标)**——旧 first-fit 自由链走链+双倍
> sb 扫描即 churn 主病灶。S2=精确帧链根集(槽注册制)施工中。坑位新增:①native.sh
> 不重拼接+管道尾 $? 假 0——改 src 后必须 build.sh→native.sh 裸跑看真 RC;②用户程序
> 面 or2 无定义发射(t_or2 未声明)——测试程序用双 if 规避。
>
> **S2 ✅ M1.5 精确帧根集落库(2026-10-02,分支 t31-m15)**:槽注册制编码(契约 §6.3
> 实施注已补)——帧内联定长池 pool[32]+__thread ct_fp+cleanup 尾声+longjmp 落点复位;
> 六类帧宿主+绑定位注册+实参 ANF 提升;unbox 拷贝免注册。**gc/run 9/9 三档(新增
> precise-only=CONSERV=0 纯精确档全绿=M1.5 主证)**;suite 99/99+net 18/18+smoke
> 160/3(3 红=同伴在册债)+bench 1.000+**A/B 帧税 +3.4%**。known-limitation:循环体
> gc-let→池递增→ovf 保守回退(健全);**a_jwt 红账未解锁(interp 臂主体驻 bump,
> 全解=S4 分配位点翻面)**;池界重置+vtable 等实参面 ANF 化=后续件。坑位续增:
> `\}` E1001 四连踩(闭括号恒裸写);nline(st) 末槽=行戳仅语句节点有(For 末槽=体,
> byte_at(List) 炸)——池式注册已废除后缀。

### T32 · GC 性能门禁 ≤15%(§9.4)

- **预估:** 1 d。**前置:** T29/T31。**状态:** ✅ 已完成(1001;门禁入 ci.sh[7/9]:tests/gc/bench.sh 分配 churn 核,digest 硬门+×3min 比值;首版实测比值 **10.28**[GC 689ms vs bump 67ms,darwin arm64 -O1 同机]——卡内预言「首版不达标」兑现,WARN 登记不阻 CI,门禁硬化随 GC 调优批次;归因:纯垃圾 churn=M1 mark-sweep 最劣面[全堆保守扫描+无分代],长驻服务面摊销更好;W6 收官)+ 翻面前置注(1001 双泳道探路同判):GC 默认档翻面的实证前置 = M1.5 精确根集——保守档对深栈程序 4GB/SIGSEGV 在册(COVERAGE 工作志 2026-10-01)
- **目标:** GC 档 vs C 差距 ≤15% 进 CI 门禁(tests/bench 家族惯例:digest pin/×3 min/门禁);未达标则按 §9.4 收紧 GC 默认策略并引导热点走 own。
- **范围:** `tests/lang/bench`(提案目录,基准设施记忆在册缺 lang 族)或挂既有 bench 家族。
- **验收:** 门禁入 ci.sh;基线数登记(比值+机器口径,只信同机差分——基准设施记忆纪律)。
- **坑位:** 首版多半不达标——这没关系,§9.4 本就是「带退出条件的目标」,登记比值与归因即交付。

---

## W7 并发深水(P9 对接)

### T33 · parallel 真并行(§7.7)

- **预估:** 2–3 d。**前置:** T13(真原子);T17(API 面已 std 化)。**状态:** ✅ 已完成(1001;fork-join K=8 定长分块 pthread/join,reduce 按分块序合并=确定性,CTRON_PAR=off 串行回退[List 形;数组形恒并行登记];纯度门=ct_emit_clo 捕获即编译期拒绝[结构性先例];List+定长数组[I32/I64 宽]双形;parallel let 专用型别[map→LI/reduce→6];06f 三路绿[interp/emit 并行/off 串行];加速比 N=10^6 同 bin:串行 5ms vs 并行 1ms ≈5×[darwin arm64];顺手修 gc/run.sh bench 夹具误入行为环[net 泳道先例];suite 99/99+smoke 161/0+net 18/18)
- **目标:** `parallel.map/reduce/fold` 从串行 fnptr(`trans_conc.ct` 头注自认)换 fork-join 线程池真并行;闭包推断纯度(§7.7:不捕获 &Cap/不 spawn/不触全局可变——`sem_pure.ct` 机制复用)。
- **范围:** `trans_conc.ct`(parallel 发射:任务切分 + 计数闩)、`sem_calls.ct`/`sem_pure.ct`(纯度推断不满足→编译期诊断或串行回落?**裁决:能力优先=诊断**,规范化纯度门)、入参 `&T[]` 只读视图校验(§7.7)。
- **验收:** 数据并行加速比锚(≥2 核机器 N=10^6 map,比值登记);纯度负锚(捕获可变全局→诊断);`06f_parallel.ct` 回归;确定性模式(T20)下分块序固定锚。
- **坑位:** reduce 结合序=浮点非结合差异——文档钉「reduce 需结合律,浮点确定性模式走 fold+固定分块」;线程池与 scope 任务池共享(worker 上限 64 既有口径)。

### T34 · work-stealing + 可增长连续栈(P9 栈经济专案)

- **预估:** 6–8 d。**前置:** T33(共享池基建)。**状态:** ✅ 已完成(1002 两波;**第二波=正案收官**:①work-stealing 落码——每 worker 环形双端本地队[owner 尾端 LIFO yield 局部性/偷取头端 FIFO]+溢出半泼全局+邻位轮转偷取,全程 G 内零新增无锁不变量,种子模式全量旁路[coro_det 101/101 站岗],窃取计数 `ctron_rt_steals()` 加法导出;新锚 tests/w7/ws_steal[单点蓄 burst+饥饿者必偷+count/sum/steals 三断言]10/10 稳;ns/yield 90ns 门内)②可增长连续栈判决入册(审计文档终节:**A/B 双阻塞于发射器机器**——B 需 emit 栈图+根区间改写+callee-saved 寄存器镜像修复三件机器[保守区间改写对地址值整数不健全,FFI 面假阳性];A 零改写但 guard 续跑同需发射器序言检查[叶帧搬移破坏调用方 sp 相对寻址];退路「1MB 大栈+guard+触顶诊断」为 §7.1 字面终形,解锁条件=发射器 morestack 机器单列)。第一波=最小符合径(1002;§7.1 字面=「大栈+触顶 panic」:rt_stack_size 缺省已 1MB[env 可配 4–1024]+guard 页[存量]+**新增溢出诊断链**[sigaltstack 每 worker/主线程安装+SIGSEGV/SIGBUS 双号+g_all 定位+明确诊断 rc=101];新锚 tests/w7/stack_ovf[64KB 小栈钉死+深递归 4KB 活帧]绿;坑位实录=①SA_ONSTACK 线程局部,worker 入口须各自安装 ②darwin guard 命中走 SIGBUS 与 SIGSEGV 双号 ③SROA 删未全用数组,锚须全槽循环写 ④尾调用被 -O1 转迭代)
- **目标:** §7.10 过渡口径(64KB 固定 mmap 栈+确定性优先调度)升级为完全符合:①可增长连续栈(拷贝式,上限 1MB 默认可配,触顶=任务边界 panic);②work-stealing 调度器。设计底稿:`docs/superpowers/specs/2026-09-26-server-p9-stack-economy-design.md`(已存在,按其执行)。
- **范围:** `net/c_src/ctron_rt.c`(栈管理改拷贝式增长)、调度器就绪队列改 steal 双端队列、coro 上下文切换兼容、`tests/net/` 栈压力夹具。
- **要点:** 拷贝式增长 = 栈溢出检测(guard page)+ 分配新块 + 拷贝 + 指针修复——**协程栈内指针修复是全件最深水**(栈上局部引用/帧链,需 trans 配合栈槽元数据);若 trans 侧元数据不可得,退路=大栈预算+panic 上限(规范 §7.1 上限口径本身允许)并登记。**判决(1002):退路兑现为终形**——证据链入册 2026-09-26-server-p9-address-audit.md 终节。
- **验收:** ~~深递归任务增长锚~~(判决后改判定锚:stack_ovf 触顶诊断=上限口径锚);c10k 回归;bench 三门禁(corro-vs-P1 ≤1.15 等)不回退;C100K 口径按专案(不外推)。**第二波实测:w7 双锚 2/2[20/20 稳:静默 10+满载 10];net 双矩阵 18/18;coro_det 101/101;suite 99/99 双臂;smoke 161/2(双红=T33 发射臂 emitter OOB,8ac3d1da 在册「干净树复现」,本泳道构造性无关[diff 仅 rt C+w7 夹具,不触发射路径]);c10k 满额 PASS(N=10000/10000,fd delta=0,connect 0.6s);bench 门禁二 63/63/48ns 门内;门禁三本件 A/B 差分 ≤0.5%(B:1418912/1453851µs vs A:1419115/1447175µs,绿轮比值 1.030/1.039 优于在册带)——门禁三比值红(1.21–1.27)在原版 rt 上同等复现[A/B 交错实证],系本机窗口 P1 臂提速的环境属性[p1-vs-C 1.13→0.97 摆动],非本件回归,随 GC 调优批次静默基线复测。**
- **坑位:** 这是 P9 专案本体,已有时序预算与登记(过渡口径禁止外推);若指针修复判决不可行,「大栈+触顶 panic」也满足规范字面(§7.1 说上限 1MB 触顶 panic,未说必须从 64KB 起步)——**最小符合路径优先**(已按此终局)。第二波新坑:①ready_pop 种子路径勿死代码化(worker_pop 必须显式分流 ready_pop,cc -Wunused 站岗实证)②锚断言禁 `;`(E1001 在册坑 20 重犯)③bench 比值对同机负载极敏感(对端自举期比值 0.92→1.85 漂移,静默窗口纪律前置于门禁判读)。

---

## W8 三档模型(web/bare/分层 stdlib)

### T35 · 分层 stdlib 机制(core < alloc < std)

- **预估:** 1–1.5 d(设计+manifest+首批分层标注)。**前置:** 无。**状态:** ✅ 已完成(1002;机制核=清单 `tier` 键[缺省 std]+模块头注 `//@ tier:` 提取[缺标注不设门,渐进承诺]+use 点 `E3040.tier` 档位门一次诊断加载即止[bare 档钳 core;E5040.tier 值域 fail-closed;`pkg_load_use_t` 独立入口避碰 driver_emit/doc 调用面,emit 随 seam 落库再切]+schema regkey `pkg.tier`;首批标注 26 模块 core 5[math/hash/log/opt/pb]/alloc 16/std 5[fs/time/process/rand/uuid]+stdpkg/examples 副本同步[漂移门绿];锚 9 断言入 smoke 3c2 段[seed 臂夹具本地 std 七+native 臂真库二];**真库 core 五模块 bare 体检自证=core 档音遍性**[tier_real_pos decls=75 零诊断];修复前实证:bare+use alloc 模块=std 内部 E3040 洪水→use 点一次即止;decl 锁 439→443 随批申报;suite 99/99 双臂+smoke 170 ok[余 3 红=conc 双发射红/Rust iter 清账,基线复现在册非本件];计划 docs/superpowers/plans/2026-10-02-t35-tiered-stdlib.md;**登记债:三线 parity(pkg.c 清单 tier 键/sem.c bare 钳口径/main.rs Manifest.tier 字段+R 线检查面)、域包 tier 标注[随 registry]、emit/doc 驱动接线[随 emit seam]**)
- **目标:** 三档分层落地机制:①std 各模块声明最低档(`core`=无分配标量/切片面,`alloc`=List/String/Box,`std`=net/fs/time);②包清单声明最低所需层;③bare 档引用 alloc 层=E3040 族诊断。设计底稿:`docs/superpowers/specs/2026-09-23-std-tiering-design.md`(分层宪章 v2 已落地 6e7ad0c,T1/T2/T3 分层在册——按其延伸到档位面)。
- **范围:** `parse_pkg.ct`(清单新键)、`std/*.ct` 头注标注批次、sem 检查。
- **验收:** 锚(bare 包 use std 层模块→E;core 包 use alloc 模块→E)✓;分层宪章既有 44 处消费不红 ✓(缺标注不设门+清单缺省 std 双重宽松,漂移/stdpkg 门绿)。
- **坑位:** 分层宪章(T1 核心 20/T2/T3)按「主题」分,档位按「分配面」分——两轴交叉,清单化勿重演大迁移(标注渐进,勿一次性搬)。→ 落码印证:零结构迁移,26 行头注+清单一键;两轴条款成文 lib/std/README.md「档位轴」节。**新增坑位:①共享检出树多泳道并发,未提交编辑会被 peer `git stash` 连带卷走(本次 Task 1 四件被卷入 t31wip stash,恢复=`git checkout <stash> -- <my paths>` 单边提取,stash 副本含对方件勿整体 pop);②selective merge 连带 Test 块亦吃 bare 体检查——core 档标注前须核其自带 test 零分配(本次五模块实证 0 命中);③vendored examples std 副本有硬链接形态(ctslink/web_todo),同步经 cp「identical」即已共享**。

### T36 · `ctc target` 子命令 + 后端插件接口

- **预估:** 1 d。**前置:** 无。**状态:** ✅ 已完成(1001;ctc.sh build/targets+--target 注册表[native=cc -O2;未注册 exit 2 清晰诊断 fail-closed],ctron 驱动同表对齐[ct_target_check];smoke 钩入 [5/9](native 等价链+未注册诊断两案);wasm32/bare 注册随 T37/T40,manifest [target] 段随其批次[CTCL schema 键位另册];门 5 26 ok+suite 99/99;**CI 修:build 分支 bash 数组→POSIX for(内核 dash,26s 红归因)**)
- **目标:** §9.7 后端矩阵插件化的第一块地板:`ctc build --target <t>` 命令面 + 后端接口抽象(C 发射为第一个插件;产物后处理链 cc→obj→link 收敛到 target 描述)。
- **范围:** `ctc.sh`/`compiler/src/driver_emit.ct`(target 描述结构:triple/链接器/运行时集)、`compiler/src/parse_pkg.ct`(清单 `[target]` 可选段)。
- **验收:** `ctc build --target native` 等价既有路径(零回归);target 未注册→清晰诊断。
- **坑位:** 纯命令面重构,发射语义零变化;为 T37/T40 打接口,接口以「后处理链+运行时文件集」为最小面,勿过度设计 LLVM 绑定。

### T37 · wasm MVP 后端(wasm32)

- **预估:** 3–4 d。**前置:** T36。**状态:** 待办
- **目标:** `--target wasm32-unknown-unknown`:C 发射产物经 clang `--target=wasm32` 编译为 wasm(后处理插件路径,零新代码生成器);运行时裁剪(pthread 族→单线程 stub,通道/Mutex 列 web 档 §7.8 单线程口径)。
- **范围:** T36 接口注册 wasm target、运行时单线程变体模板、`tests/`(wasm 产物冒烟:node/wasmtime 执行)。
- **验收:** 最小程序(.ct→wasm→node 执行输出正确);no_alloc 子集先行(§9.1 兼容方向:core 子集);GC/并发全量列 T38 后。
- **坑位:** wasm 无 dlopen——FFI 面在 wasm target 上禁用(诊断而非静默);wasi 与 unknown-unknown 的 libc 差异,先 unknown-unknown+自备 minimal runtime。

### T38 · WasmGC + JSPI + 类型化 JS 桥(§9.2)

- **预估:** 4–6 d。**前置:** T37。**状态:** 待办
- **目标:** 真 web 档后端:WasmGC 目标(类/List 落宿主 GC)、JSPI 任务挂起、类型化 JS 桥(JS 异常边界转 Result,§9.2)。
- **范围:** 新代码生成路径(wasm GC 类型段)或 clang WasmGC 靶适配;`stdweb` 桥 ABI。
- **验收:** 浏览器真窗冒烟(dom.set_title 真 DOM 生效——现在 C stub 假实现换真);宿主 GC 压力样例。
- **坑位:** WasmGC 工具链成熟度依赖 clang 版本——先探测本机工具链能力,不可行则登记环境依赖(GitHub CI 加 wasm 靶);这是 W8 最大不确定件,**允许裁剪:JSPI 单列**,WasmGC+桥先行。

### T39 · stdweb 真实化(§9.2)

- **预估:** 1–1.5 d。**前置:** T38(桥可用)。**状态:** 待办
- **目标:** `dom.set_title/title` 从 C stub 换真 JS 桥;按「以此模式逐版扩充」承诺扩第二面(`dom.set_body`/`fetch` 择一,评审拍板)。
- **范围:** `stdweb` 内建面(`sem_calls.ct` prof==web 分支)、桥绑定、`tests/10_web_dom.ct` 扩展。
- **验收:** 浏览器端到端锚;`10_web_dom.ct` 双靶(full 档 E2020 拦截语义保持)。
- **坑位:** 内建命名空间逐版扩充纪律(v0.5 钉死口径),勿一次铺大面。

### T40 · bare 交叉编译(§9.3)

- **预估:** 3–4 d。**前置:** T36。**状态:** 待办
- **目标:** `ctc build --target thumbv7em-none-eabi` / `riscv32imac-unknown-none`:工具链自包含(内嵌 lld + minilibc 选项),零 OS 依赖运行时裁剪。
- **范围:** T36 注册 bare targets、运行时 bare 变体(无 pthread/无 fs/net;静态栈+guard page)、QEMU/模拟器冒烟测试(qemu-system-arm 环境探测)。
- **验收:** 裸机 hello(el,QEMU 跑通或硬件在环);`08_bare.ct` 从 suite 跳过转正。
- **坑位:** lld 依赖本机 llvm 工具链——环境探测与 CI 靶登记;minilibc 自带子集(syscall stub)工程量控制:先 semihosting。

### T41 · bare 分配器族 Region/Pool/Static(§6.6)

- **预估:** 1.5–2 d。**前置:** T35(分层面)、T40(才有 bare 消费场景)。**状态:** 待办
- **目标:** 前奏类型 `Region/Pool/Static` 落地:`Arena.fixed(n)` 转正(测试跳过解除)、`Region`(嵌套区段)、`Pool`(定长对象池)、`Static`(编译期静态分配)。
- **范围:** `sem_calls.ct` 前奏注册、发射垫片(静态内存段布局)、`tests/08_bare.ct` 扩展。
- **验收:** 08_bare 转正跑绿;E3040 负例族保持;core 层容器传 arena 可用锚(§6.6)。
- **坑位:** Static 面涉及链接期布局(段属性),MVP=固定大小静态数组声明宏面;列志向的 ISR 约束归 T42。

### T42 · 体积门禁 + ISR 约束(§9.4/§6.6)

- **预估:** 1 d。**前置:** T40/T41。**状态:** 待办
- **目标:** bare+core < 100KB(硬指标)与 full < 1MB(目标)进 CI 门禁;ISR 默认 `#[no_alloc] #[no_spawn]` 约束检查(bare 档中断处理函数标注识别)。
- **范围:** `ci.sh` 体积门禁步、`sem_walk.ct`(ISR 标注约束,属性面复用 no_alloc/no_spawn)。
- **验收:** 门禁红绿可演示(故造超限样);ISR 负锚。
- **坑位:** 100KB 口径=release 优化档(-Os)+minilibc——CI 里 clang -Os 对裸靶可用性随 T40 环境登记。

---

## W9 工具链与生态

### T43 · ctc 子命令补齐(§9.7)

- **预估:** 1 d。**前置:** 无。**状态:** 待办
- **目标:** `ctc lint`(信任审计已有 `driver_check.ct --trusted` 面,命令化+常规 lint 汇总)、`ctc bench`(tests/bench 家族入口)、`ctc add/publish`(依赖 T49 语义,先命令骨架+明确「未接 registry」诊断)。
- **验收:** lint/bench 真用;add/publish 骨架 + fail-closed 提示。
- **坑位:** publish 涉及对外发布面,**未经用户确认不得接真 registry**——骨架件只做本地协议。

### T44 · own ±5% 性能门禁(§9.4 P3 出口硬指标)

- **预估:** 1 d。**前置:** 无。**状态:** ✅ 已完成(1002;tests/lang/bench 第一族落地:src/main.ct[k1 Lehmer 纯算术核全程 <2^63 零溢出歧义+k2 每 16 元 List 填充 arena 面]+src/ref.c C 同构对照+bench.sh[digest 硬门 emit 产物 vs C 逐字节一致+×3min+CTRON_EMIT 覆盖口];入 ci.sh[7/9]。首版实测**总比值 1.161 WARN**[ctron 72ms vs C 62ms,darwin arm64 -O1 同机];k1 分解归因:**纯算术核 63 vs 67ms=比值 0.94 ≤1.05 达标——发射代码质量面过 P3**;总比值超标全在 k2 allocator 税[每轮 List vs C 复用栈缓冲],热点回切建议=复用 List/定长数组惯用法[等 list_set/定长数组惯形成熟后门禁可硬化])
- **目标:** own/热点路径与 C 互有 5% 内进门禁;基准设施记忆在册缺 lang kernel 族——本件顺带立 `tests/lang/bench` 第一族(与 T32 共用目录)。
- **验收:** own 基准(arena 数值核/C 同构对照)×3 min 比值 ≤1.05 门禁入 ci.sh;未达标登记归因(热点回切建议)。
- **坑位:** 只信同机差分(基准设施纪律);与 T32 共享 bench harness,宜连续执行。

### T45 · CBox[T] + FFI 余账三件(§9.6)

- **预估:** 2 d。**前置:** 无。**状态:** ✅ 已完成(2026-10-02;实况=三件余账中「定长数组字段塌缩」v0.9·二/四 已先期销账(本件复核),实落两件+计划外编译器泛型发射面四缺口修复:①CBox[T] 落 lib/ffi(struct+自由泛型函数面+C 侧登记表哨,双违例负锚 cbox_dblfree/cbox_useafter;class 字面量发射不可用=T25 v2 域新登记,方法面走自由函数);②解释桥 float 帧(f:/g: 帧+Ri:/Rf:/Rg: 返回别+SIMD 类 ABI cast+形状表≤4 参全展开+浮返 "%.17g" 零损;ext_finterp 编译+解释双通道;整返回升 long=close(-1) bootstrap 缺口随销);③cimport union/struct/enum 关键字限定指针形参(union Vals* 端到端);④泛型直调推断单源 ct_call_infer_tys(种子码串/型节点异型 SEGV+&T 形参永不命中)+实例槽码字母表扩位(F/G/M/N/V/W/X/Y)+void 泛型特化(None→v,尾调用捕值首证)+ct_ty_code nt[2] OOB 守卫;smoke decls 锁 443→444 申报;T35 parse_pkg pkg_load_use_t 调用 arity 破损顺修(阻塞重建,机械对齐 def 6 参)。门禁:ffi 37/37+suite 99/99 双列+smoke 166/7(7 红=T33×2+T16 iter+ctecho 端口竞态+本批翻 444 后复验余量对齐在册债))
- **目标:** ①`CBox[T]` 跨边界所有权包装(Ctron-owned:drop 责任显式,§9.6 三约定之 2);②FFI 泳道在册余账:union 指针形参、解释桥 float 帧、定长数组字段塌缩(ffi 记忆登记)。
- **范围:** `ffi.ct`/`std`(CBox 定义:含 #[trusted] drop 面)、`trans_expr.ct`/`eval_call.ct`(三件逐修)、`tests/ffi/` 扩展。
- **验收:** CBox 所有权锚(移交/释放各一次,双违例负锚);三件余账各自行为锚;ffi 27/27 保持。
- **坑位:** CBox drop 须 #[trusted](手动 free 即不健全面);union 形参走字节缓冲已有先例(v0.9 §四),指针化是扩展非重演。

### T46 · HTTP 框架半层 IO 粘合 st_serve(P4-B)+ e2e 回切

- **预估:** 2 d。**前置:** 无(独立于编译器泳道)。**状态:** 待办
- **目标:** 服务器 IO 粘合:accept→parse→router→respond 服务循环(`st_serve`),接通 `http/frm/` 16 件与 `net` 门面;`tests/http/run.sh`「IO 粘合归 P4-B+」注记兑现;设计底稿 `2026-09-27-web-framework-design.md`。
- **范围:** 新 `http/frm/serve.ct`(或 net 侧)、`tests/http/` e2e(真回环)。
- **验收:** 真窗 e2e(GET/POST/静态/中间件链)双运行时矩阵;e2e 缺声明红件联动 T23(同源则并销)。
- **坑位:** 跨包引用即请求(含 struct)——frm 纯层不 use net 的纪律保持:粘合层是唯一 use 点,菱形 use 误报坑(E5020 误报在册)注意。

### T47 · multipart(§11.7 框架半层)

- **预估:** 1 d。**前置:** T46。**状态:** 待办
- **目标:** multipart/form-data 解析(文件上传面):`http/frm/` 补 multipart,边界解析/大小上限/临时面。
- **验收:** corpus 夹具(真实浏览器上传样例)+ 上限负锚;frm 套件回归。
- **坑位:** 二进制安全(零拷贝切片视图 &T[] 面);RFC 2046 边界引号变体。

### T48 · CTCL 清单迁移(§2.7 修订注兑现)

- **预估:** 1.5–2 d。**前置:** 无。**状态:** ✅ 已完成(2026-10-02;实况=三线解析器宿主/R 线早已就绪[pkg.c ctcl_load E5040-5050 冻结面+check.rs 同码],自举 caps 面已读 .ctcl——本件实为收尾:①tests/ffi 18 份 .toml 机械迁移[全仓 .toml 清零,104 份全 CTCL];②ctron 安装器硬切[new 仅产 .ctcl/build 项目模式读 .ctcl+.toml 遗留 fail-closed 拒构+help 文案三处];③三线迁移诊断逐字一致[E5040 清单格式已硬切 CTCL:pkg.c pkg_load/parse_pkg.ct pkg_check_caps/main.rs 包级检查];④ctron_smoke 双探针[new 不再产 .toml+遗留 .toml 迁移诊断拒构];⑤规范三注翻面[§2.7/README 登记/08 budget_ms 键名维持声明位]+sync_site_spec 无漂移。门禁:ctron_smoke 25/25[含双新探针]+suite 99/99+ffi 33/0+cargo check 0;R 线 main.rs strip_prefix 硬 grep 已在先期批次拔除)
- **目标:** 包清单 `Ctron.toml` → `Ctron.ctcl` 三线硬切(规范定义:`2026-09-16-config-language-v1.md`,fail-closed 注册表/caps 列表形/deps 三互斥形全有)。
- **范围:** 三线解析器统一切换(`parse_pkg.ct`/宿主/R 线)、在库全部 `Ctron.toml` 迁移、安装/CI 脚本。
- **验收:** 在库包全迁;TOML 面移除(fail-closed:读到 .toml 给迁移诊断);全量套件+examples 回归。
- **坑位:** 「三线解析器三种语义」是本件的存在理由——逐语义差分表已在设计文档 §1,照单清账;安装面(install.sh/CTRON_STDPATH)联动。

### T49 · lockfile 内容寻址 + workspace + add/publish 实装(§2.7)

- **预估:** 2 d。**前置:** T48。**状态:** 待办
- **目标:** 依赖解析:严格 semver + lockfile(内容寻址)+ workspace;`ctc add/publish` 从 T43 骨架转实(本地 registry 面)。
- **范围:** `parse_pkg.ct`(deps 解析/lock 生成)、`ctpkg` registry 本地协议、workspace 段。
- **验收:** deps 解析锚(三互斥形/semver 约束);lock 二跑稳定(内容寻址 digest);workspace 用例。
- **坑位:** 闭源工件 S2a(T50)依赖本件 lock 寻址——接口留 `deps/<pkg>.ctart` 回落位(§2.2 解析链③)。

### T50 · 闭源分发 S1/S2【条件触发件】

- **预估:** 未估(按 spec S1+S2 触发条件)。**前置:** T49 + 用户触发。**状态:** 待办(挂起,触发条件未到)
- **目标:** 闭源包分发泳道 spec v2(`2026-09-21-closed-pkg-distribution-design.md`)的 S1(_iface 投影编译)/S2(ctart 工件分发+解析链③实装)。**登记纪律:触发条件勿提前**(记忆口径)。
- **验收:** 按该 spec 各阶段出口。
- **坑位:** 本件是占位卡——用户点名即先重读 spec 触发条件,未到则呈报不开工。

### T51 · 异步 IO 补全 + Simd 自动向量化评估(§9.5)

- **预估:** 1.5–2 d(评估面)+ 未估(实施,按评估结论)。**前置:** 无。**状态:** ✅ 已完成(2026-10-03;评估报告=docs/simd-vectorization-analysis.md,实施按报告志向分级另立)
- **目标:** §9.5 硬件利用三残项:①io_uring(linux)/IOCP(windows) 接入现有 reactor 族(kqueue/epoll/poll 已有);②NUMA 感知分配与任务亲和(运行时选项);③`Simd[E,N]` 自动向量化评估(现纯解释/发射模拟)。
- **范围:** `net/c_src/ctron_rt.c`(reactor 后端注册面)、发射模板、评估报告落 `docs/ffi-analysis.md` 同级或 bench 报告。
- **要点:** io_uring 与 kqueue/epoll 同构(完成队列轮询),reactor 抽象已有(CTRON_RT=coro);windows/IOCP 无靶机则登记环境依赖;Simd 自动向量化评估结论二选一:发射侧对 `Simd` 元素级白名单运算直发 SIMD intrinsics/`-ftree-vectorizable` 提示(工程量大,列志向),或规范挂「v0 为语义模拟,向量化志向」注——**评估报告即本件交付,实施另立**。
- **验收:** io_uring 双矩阵同形测试(linux 环境可用时);评估报告过评审;NUMA 至少给选项位(env/清单)或登记不做理由。
- **坑位:** 本机 darwin 无 io_uring——CI linux 靶才有验收环境,环境探测先行;勿为有后端而造无靶验证的死码。

### T52 · 插件沙箱体系:derive + lint(§8.3/§10.6/§12.5)

- **预估:** 3–4 d。**前置:** T25(trait 对象不必须,但 derive 展开产物「普通代码」需发射面稳)。**状态:** 待办
- **目标:** §8.3「@derive 由沙箱内 derive 插件展开(普通代码,非宏手术)」+ §10.6「lint 插件(W9xxx 码段,沙箱执行,确定性,可缓存)」:插件 = 普通 Ctron 包,编译器以受限调用面加载执行(comptime CVM 复用——`sem_ceval.ct` 白名单机制即沙箱底座)。
- **范围:** 编译器插件加载协议(包清单 `[plugins]` 声明)、derive 插件接口 trait(输入型别描述→输出合成 decl)、lint 插件接口(输入 HIR 面→输出 W9xxx 诊断)、`@derive(DbRow)`(§12.5 志向档)作为首个官方 derive 插件。
- **要点:** 沙箱 = comptime 白名单扩展(禁 I/O/spawn,预算 E6010 复用)——**不引入进程外插件**;W9xxx 段注册进 `meta_check.py`(第三方码纪律「先进表再使用」对第三方指 per-plugin manifest 声明);E6030(T06)封闭性论证是前置论据。
- **验收:** 官方 derive 插件样例(DbRow 或 Json 其一)端到端;lint 插件样例(自定义 W9xxxx 码)+ 确定性缓存锚;沙箱逃逸负锚(插件内触 I/O → E6020)。
- **坑位:** 这是「能力优先」横切裁决的直接标的——设计文档凡「降级/替身」措辞按裁决改扩能力项;HIR 面对插件的暴露宽度的信息 hiding 裁决留设计文档,勿在无设计时先码。

### T53 · emit union 载荷 64 位化(json/db 数值保真闭环)

- **预估:** 1.5–2 d。**前置:** 无。**状态:** 待办
- **目标:** `std/json.ct:422` 在册病灶:emit 的 Result/Option union 载荷槽 32 位——Ok(I64) 截断(2^63-1 读回 -1 实证)/Ok(F64) 错值;现靠 JNum/JReal struct 通道绕行。载荷槽 64 位化后绕行面回切,§12.6 前置工项闭环。
- **范围:** `compiler/src/trans_ty.ct`/`trans_expr.ct`(union 表示:载荷槽定宽 64 + 型别标签)、`std/json.ct`(JNum/JReal 绕行回切)、`tests/`。
- **验收:** Ok(I64) 大值/Ok(F64) 全域 round-trip 锚(emit 臂);json/db 套件回切后回归;interp/emit 同判(lane 字面量槽数不对称坑位对照)。
- **坑位:** union 布局变更触发 ABI 面(repr(c) struct 勿动,语言内 union 与 C-ABI 布局是两面);绕行回切一次性做,双轨期符号撞号坑(ct_clo 撞号教训)。

---

## 状态台账(完成后回写)

| ID | 件名 | 波 | 状态 | commit |
|---|---|---|---|---|
| T01 | std/bit 模块 | W1 | **已完成**(0928,四面同判+crypto 回切,bit_ops×4 臂绿,db 回基线) | 见 git |
| T02 | 模式守卫 R-P3c | W1 | **已完成**(0928,三线落地+锚迁主套件 02f,发射臂债在册) | 见 git |
| T03 | StringBuilder | W1 | **已完成**(0928,三线+锚迁 03j) | 见 git |
| T04 | W8 锚补齐 | W1 | **已完成**(0928,01m/01n 锚+三线发射补齐) | 见 git |
| T05 | r1a 行尾点守卫 | W1 | **已完成**(0928,词法层守卫+锚迁 01o) | 见 git |
| T06 | E6030 封闭性 | W1 | **已完成**(0928,论证承载,规范注记) | 见 git |
| T07 | 单态化预算 8192 | W1 | **已完成**(0928,E6040+fx 锚+smoke 门) | 见 git |
| T08 | comptime 预算口径【裁决】 | W1 | **已完成**(0928,用户裁定步数终态,§8.4 修订注) | 见 git |
| T09 | resolve 多记录 | W2 | **已完成**(0928,双矩阵 15/15×2;SocketAddr struct 随 T14 底座) | 见 git |
| T10 | sleep_ns/虚拟时钟 | W2 | **已完成**(0928,三矩阵+rt 弱钩贯通) | 见 git |
| T11 | caps 键细分 | W2 | **已完成**(0928,四线注册表+夹具;R 线对象面欠账在册) | 见 git |
| T12 | gzip | W2 | **已完成**(0928;主体=webfw 泳道 P4-B,本件补 python 双向互操作差分) | 见 git |
| T13 | Atomic 真原子 | W2 | **已完成**(0928,__atomic SEQ_CST 三面,双矩阵精确 200k) | 见 git |
| T14 | Channel 去限制 | W2 | **已完成**(0928,三片 0e004a2/b6d26fe/369b9bc;pool 消费方迁移随 db 泳道) |<!-- 并行注意:远端 main 另有编译器线平行实现(ba28f4b 链 614a5c8 堆环+dac9aec interp 克隆),两史分叉待裁决合并 --> 见 git |
| T15 | Iterator trait | W3 | **已完成**(0928,三线 interp;发射臂 var-self 引用语义在册) | 见 git |
| T16 | 适配器链 | W3 | **已完成**(1002,Seq[T,S] get 基惰性适配器+UFCS 入口;锚迁 modules/iter_adapters;suite 99/99;发射面/var-self 在册) | 见 git |
| T17 | std/iter 归位 | W3 | **已完成**(0928,06f 翻转;suite 89/89 首次 100%) | 见 git |
| T18 | 列级 span | W4 | **已完成**(0928,解析错精确 LINE:COL;sem 行对列 1) | 见 git |
| T19 | fix-it 首批 | W4 | **已完成**(0928,E3030 精确 edit+E2020/W8010 note) | 见 git |
| T20 | deterministic+缓存 | W4 | **已完成**(0928,--deterministic 旋钮+emit 内容寻址缓存) | 见 git |
| T21 | 宿主检查面五件 | W4 | **已完成**(0928,五锚迁主套件;suite 94/94) | 见 git |
| T22 | 宿主运行面两件 | W4 | **已完成**(0928 核销;前批已修,COVERAGE 过期条目清) | 见 git |
| T23 | e2e 缺声明 6 件 | W4 | **已完成**(0928 核销;前批 P1b 已清) | 见 git |
| T24 | 虚表 ABI 设计 | W5 | **已完成**(0929,设计文档 9fbd332) | 见 git |
| T25 | 动态分发 codegen | W5 | **已完成**(0929,四步全通+引用门;suite 94/94+net 17/17) | 见 git |
| T26 | 能力注入归位 | W5 | **已完成**(0929,07a/07b 翻转+Fs 入 std+Env 三线+vtable 全签名+cap_inject 真窗) | 见 git |
| T27 | 闭包捕获收口【裁决】 | W5 | **已完成·B 终态转正**(0930 用户裁决落注 r-roadmap §4.7;E3070=语义执行;实现零债=as-built 即终态) | — |
| T28 | GC 契约校准 | W6 | **已完成**(0929,现状校准九处+M1/M2/M3 分阶段契约+栈扫描帧位图径;spec §6.2 挂注) |
| T29 | MVP tracing GC | W6 | **WIP**(f21fee5;GC 堆+保守根集+排序二分 mark 已实证 74× RSS 回收;余=垫片文件机制绕字符串插值极限,见卡) | — |
| T30 | into_gc 真实化 | W6 | 待办 | — |
| T31 | 解释器回收 | W6 | 待办 | — |
| T32 | GC 门禁 ≤15% | W6 | 待办 | — |
| T33 | parallel 真并行 | W7 | **已完成+登记**:0930 后宏发现 fx_conc_parallel 发射臂 emitter OOB("index out of bounds",ct_emit_clo 捕获分析路径;干净 origin/main 复现,归 T33 泳道修复;**0930 精确定稿**:ct_emit_clo 的 #ifndef 卫兵在 prewalk/实发两遍
间错位——发射 C 出现「fn 首行后即 #endif、return 与 } 落卫兵外」的截断 shim(闭包无
返回路径→运行期垃圾;fx_conc_parallel/cloval 双夹具实证)。修法=卫兵只在实发遍 emission
(pass-1 预扫只直出内层 shim,不落卫兵行),或卫兵行纳入 eln 门控。T33 作者按此分钟级可修。**10-03 T27 复验注**:r3a 面所经 ct_emit_clov 卫兵形态实证完好(单对 #ifndef 包整 shim),r3a 原生臂真因=bsum I32 溢出+with ANF 漏接(已随 T27-B 收口三修);本件 parallel 面 ct_emit_clo OOB("index out of bounds")独立仍在册,bsum 修复后复验依旧复现,卫兵假说待 T33 作者按上法定稿时再证 | — |
| T34 | 栈经济 P9 | W7 | **已完成**:1002 两波——最小符合径(触顶诊断链 a77f141f)+正案收官(work-stealing 落码+可增长栈判决入册:A/B 双阻塞于发射器机器,退路 1MB 大栈=§7.1 字面终形,解锁条件单列;ws_steal 锚+c10k 满额 PASS+A/B 差分 ≤0.5%) | — |
| T35 | 分层 stdlib 机制 | W8 | ✅ 1002 | — |
| T36 | ctc target+后端接口 | W8 | 待办 | — |
| T37 | wasm MVP | W8 | 待办 | — |
| T38 | WasmGC+JSPI | W8 | 待办 | — |
| T39 | stdweb 真实化 | W8 | 待办 | — |
| T40 | bare 交叉编译 | W8 | 待办 | — |
| T41 | bare 分配器族 | W8 | 待办 | — |
| T42 | 体积门禁+ISR | W8 | 待办 | — |
| T43 | ctc 子命令 | W9 | 待办 | — |
| T44 | own ±5% 门禁 | W9 | 待办 | — |
| T45 | CBox+FFI 余账 | W9 | ✅ 完成(1002) | — |
| T46 | st_serve IO 粘合 | W9 | 待办 | — |
| T47 | multipart | W9 | 待办 | — |
| T48 | CTCL 迁移 | W9 | 待办 | — |
| T49 | lockfile+workspace | W9 | 待办 | — |
| T50 | 闭源 S1/S2【条件】 | W9 | 挂起 | — |
| T51 | 异步 IO+Simd 向量化评估 | W9 | **已完成**(1003;①io_uring 后端落库[POLL_ADD 天然 one-shot 同构映射+G 内单生产者+免 tick+双 NOP 自检门响亮回退,6.10-linuxkit array 异常立案];②NUMA 选项位+拓扑探测[行为位=志向];③Simd 评估报告选 B 落 docs/simd-vectorization-analysis.md[clang -O2 width4 实证/gcc -O3;寄存器驻留=志向];IOCP 环境依赖登记;reactor 冒烟双臂挂 ci.sh;net 18/18+coro_det 101/101+w7+suite 100/100) | 见 git |
| T52 | 插件沙箱(derive+lint) | W9 | **已完成**(2026-10-03;协议=清单 plugin 块+接口包 ctron.plugin+约定入口;沙箱=纯度门 E6020.sandbox+静态规模门[执行期预算列 v2];derive(Json)=自由 fn/UFCS 产物[trait impl 发射缺口在册];lint_toolong=W9001 清单 codes 先进表;阶梯六锚挂 ci.sh[5.5/9] 含确定性双跑; suite 100/100 双跑;烟主段 159/2 双红在册[T33 conc_parallel/Rust iter 清账]+ctron_smoke 25/25;债八项入 COVERAGE) | 见 git |
| T53 | emit union 载荷 64 位化 | W9 | **已完成**(1002;五病灶全修[expect 型别/expect 位还原/语句形 match-Ok 硬编码 int32/Try 两语句位/json (h) 绕行],json JNum/JReal 回切摘除,芯=wrap+or 先行 a000a6ad;探针 14 项双臂绿+e_t53_payload64 双臂锚+json_fidelity 13/13+suite 99/99+web_todo 92 裸跑 rc=0) | 见 git |

---

## 执行公约(每件开工时照此)

1. **对齐:** `git status && git log --oneline -5` 重查(机刷泳道在飞);冲突面任务改约 worktree。
2. **读卡:** 读本文件该任务卡 + 其「前置」任务的产出契约;设计底稿文件(卡内已列)一并读。
3. **依赖检查:** 前置未完成 → 停,呈报用户,勿跳级。
4. **展开实施计划:** 按本卡写该件详细实施计划(红锚先行/TDD 分步),再动码。
5. **验收:** 卡内验收条逐条过;`bash ci.sh` 全绿;suite.py 双跑;红账不新增。
6. **落库:** 全绿才 commit(pathspec 限定);回写本文件状态行 + 台账;COVERAGE/divergences 相应登记;规范触点(若动规范)挂修订注。
7. **两件裁决门(T08/T27)与一件挂起件(T50):** 点名即先呈报裁决项/触发条件,用户拍板后才动工。

---

## 会话交接(2026-09-29 收官)

**进度**: 25/53 件落库(47%);suite 94/94 双线全绿。

### 已完成

| 波次 | 件数 | 核心 |
|---|---|---|
| W1(T01-T08) | 8/8 | bit 模块/模式守卫/StringBuilder/W 码锚/行尾点/E6030/E6040/T08 裁决 |
| W2(T09-T14) | 6/6 | resolve 多记录/sleep_ns+虚拟钟/caps 细分/gzip/Atomic 真原子/Channel 三片 |
| W3(T15-T17) | 3/3 | Iterator for 三线/std/iter/06f 翻转(T16 收口:Seq get 基+UFCS,发射面在册) |
| W4(T18-T23) | 6/6 | 列级 span/fix-it/deterministic+缓存/宿主红账全清 |
| W5(T24-T25) | 2/4 | 虚表 ABI 设计/&Trait 发射侧动态分发(vtable+thunk+装箱+分发) |

### 下一步推荐(按优先级)

1. **T26 能力注入归位**(可做,T25✅ 已解锁)
2. **W7 T33 parallel 真并行**(前置 T13✅+T17✅ 已满足)
3. **T16 收口**(UFCS 链=需泛型 trait 分发,已登记)

### 挂起/阻塞

- ~~**T27 裁决门**~~: **已裁决(2026-09-30,B 终态转正,台账 608 行;r-roadmap §4.7 终态确认在案;spec §4.7 正文修订 10-01 补齐)**
- **T54 ✅(已销账,2026-09-30)参数化 List 码 + 装箱容器 ABI**: 容器 ABI 由 T14-② 落地(参数化 List 码 `Lu:<名>`/LI/L6+堆盒 push+索引解引用);收口件补 fn 值链(#fret 字段提取位 ct_fnfield_ret+裸 fn 蹦床/shim `u:` 解盒)——03l/03n 正本 emit 臂绿(03l:`HIT /app -> app:alice`+`SUM 42`),spec §9 P0-1/L6 销账。suite 96/96+96/96 零移动;遗留另录(不扩界):e.h 直呼形态、>8B struct fn 值返回、ct_cb_ref extern 回调
- **T55(✅ 已销账 2026-09-30,按引用 v0 形=arena 格+P 码解引;创建时快照边界在册,外层帧创建后再赋值可见性已由 T27-B 终态判定 = 不可见,创建时快照即规范语义,10-02 收口)值位置闭包捕获**: 种子按设计非捕获(trans_expr.c:1587)+正本同族硬停——web 中间件/守卫原生臂前置(spec §9 L7);T27 裁 capture 语义,A(按引用)即通向本件实现口径,裁 T27 时一并裁本件
- **T29-T32 GC 全件**: 等 S1 Val 迁移落库(在飞 /tmp/s1-val)
- **T50 闭源 S1/S2**: 触发条件未到
- **T37-T40 wasm/bare**: T36 target 接口是前置

### 关键坑位速查(10 条血泪)

1. Ctron 语法: 禁`;`/裸`{`须`\{`/`\}`**不是合法转义**/or2 全括号/无三元
2. 发射怪癖: scope 闭包 return 泄漏/双 scope 撞名 t_sc/x_ 夹具 fn main+&数组→SIGSEGV
3. var self 方法: interp 三线各自方案;**发射臂按值传 mutation 丢**
4. &Trait vtable: **引用门**(ct_trait_used_as_ref)防 net 域包全量产 vtable
5. C 宿主 ceval depth: 须对称递减或移除
6. wrap_int U64: bits==64&&us 须掩码 2^64-1
7. msg 子串锚: 消息须逐字对齐
8. 机刷泳道并发: 其 WIP 冲突标记/探针会堵门禁——非己债勿修只登记
9. R 线 check_suite: tests/ 顶层新文件零诊断=三线同步必须
10. diff.py 四线对拍: 消息逐字节;改注册表须四臂同步

### T53 · emit union 载荷 64 位化(实施中断,2026-10-02)

- **预估:** 1.5–2 d。**前置:** 无。**状态:** ✅ 已完成(2026-10-02,S1 平面复开即愈——上轮 #prel 助手表方案整体弃用,改 P0-A 同款内联 C11 union 复合字面量位双关,零新符号零 #prel)。**芯已先行**:wrap 写侧+or 两读侧由 a000a6ad(P0-A 收口四缝)落库;本件补齐余面+回切。**病灶全图(五点)**:①expect 静态型别硬编码 "i"(trans_ty 两处)——推断 let 按 int32 消费,I64 载荷 >2^31 截(2^63-1→-1 实证)、F64 错值;②expect 发射无位还原(trans_expr);③语句形 match 的 Ok 臂硬编码 `int32_t t_x=(int32_t)rme.v`(trans_stmt 1184——match 面截断根,注意 match-R 发射共**四条路径**:let-match 形/语句形/块尾值形 ct_match_value=Option 模型/表达式形,只有前二载标量载荷);④Try 两语句位 `(double)` 值直转;⑤json (h) 族绕行面。**修法**:载荷码统一经 ct_opt_elem_expr 恢复(接收者 Ident=#elem 侧条目[P0-G 已绑]/被调声明 Result[..] 首参——跨包 ct_fn_decl 通);f 码位双关回读(与 ct_res_wrap f 臂配对),6/7 整宽直槽,标量安全族 {f,6,7,z,b} 入 expect 型别,聚合/Str 码回落旧行为(零扰动)。**json JNum/JReal 回切**:jget_* Result 面成唯一 pub 数值面,jnum_*/jreal_*/jn_*/jr_* 摘除,jint64/jf64 私有 (k,v) 对保留,jv_*/jk_* 内迁 `.or(dft)`/match(pkgs/web/json.ct jd_i64+lib/http/frm/body.ct 注释同步)。**验收**:探针 14 项四读侧×I64(2^63 双界)/F64 双臂全绿(基线 emit bad=9 清零);新锚 tests/02_option_result.ct 两 test 块(套件 interp 双宿主)+tests/json_fidelity/corpus/e_t53_payload64.ct(run.sh 双臂);json_fidelity 13/13、suite 99/99 双列、web_todo 原生裸跑 92 rc=0(big=5000000000 大值载荷实证)、http/run 99 过(红=基线同红:e2e 五件 origin/main 预存、gzip 族 vendored 前置已补建)。**坑位**:①match-R 四条发射路径逐条排摸(1184 硬编码行藏在 okIx 臂);②`?` 表达式位只在 Result 返回函数内合法(emit `return t_q` 撞宿主 fn 签名=cc 硬错,interp 动态返回侥幸——探针首版踩);③I64_MIN 字面量直写合法(绿夹具在案),`0 - imax - 1` 不可用(seed 算术宽度取左);④自举期宿主静默崩(S1 平面前)确证随去 cx 化消失——本件三次自举全过)
