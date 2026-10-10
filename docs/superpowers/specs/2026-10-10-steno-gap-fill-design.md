# Ctron 承载秘本引擎 —— 能力补齐设计(G1–G7 批)

- 状态:**裁决已收,按推荐执行**(2026-10-10 用户裁决 J1–J9 全按推荐;§13 各项即为定案。G1 实施计划 = `docs/superpowers/plans/2026-10-10-steno-g1-memory-plan.md`;后续批次随前一批复核各自出计划)。
- 日期:2026-10-10。
- 输入:`ctron-gap-roadmap.md` v0.1(2026-10-10,工作区件 `/Users/zyj/.zcode/workspace/default/ctron-gap-roadmap.md`;本设计 §1 已逐项复核其「现状」列,复核后本文自足)。
- 方法:五路现状复核(FFI/内存/并发/域包/链接与平台),所有断言过 file:line 实证;**现状以本文 §1 为准,roadmap 的现状列多处已过时**。

## 0. 结论一句话

roadmap 的「三件 P0」判断成立,但内容要修正:**dlopen 已有半张底牌**(v0.7 起 `#[dlsym]`+`dlopen/dlclose/dlsym` 内建,断在 RTLD_GLOBAL/句柄 thunk/旗标消费三缝);**单线程长跑内存已有硬证据有界**(a_jwt 41GB→70MB,S1–S4 全链在库),真缺口是 **spawn 置 GC 全进程永久退避**(并发程序连 CTRON_GC=1 都无界——秘本引擎必踩,roadmap 未见到)+ 流式 buffer 无复用约定;**线程面底座比 roadmap 估计的厚**(scope/spawn/join/取消传播/双执行形态全在),缺的是定型不是从零造。另有两枚 roadmap 没写的新缺口:**http client 无 https**(模型下载必走 https,是 P1-8 的隐含前置)与 **scope 退出 join-all 未实现**(spec 承诺, detached 任务静默丢失)。

全部缺口**不动语言宪法**:语法零新增(唯一候选 = repeat 字面量,已有独立计划 `2026-10-09-repeat-literal-plan.md`,划入 G1),走「运行时面 + 库面 + 驱动面」三条缝,分 G1–G7 七批。roadmap §5 的 FB-N+1..6 映射作废(FB 泳道 13/13 已收官),换 G 批。

## 1. 现状复核(逐项修正 roadmap)

| # | roadmap 断言 | 今日实证 | 判定 |
|---|---|---|---|
| R1 | P0-1「无 dlopen(ffi-analysis.md 明确)」 | v0.6 时点口径;v0.7 起 `#[dlsym]` extern → `ct_dyn_<名>` thunk(`dlsym(RTLD_DEFAULT)` 首调缓存)+ `dlopen/dlclose/dlsym` 三内建(编译通道;解释器拒绝)已落(docs/ffi-analysis.md:84,187-188) | **半就位** |
| R2 | P0-2「arena 免 free,内存单调增长」 | `CTRON_GC=1` mark-sweep 全链在库:精确帧槽/池界重置/自适应阈值(churn>max(8MB,活集));a_jwt 1 小时级负载 41GB→70-74MB 有界(tests/COVERAGE.md:1549-1563);默认档仍 bump,翻面待裁(driver_emit.ct:212) | **过时一半** |
| R3 | (roadmap 未见) | **`ct_spawn` 置 `ct_gc_conc=1`,整个进程所有分配永久回落 bump,GC 从此不参与**(driver_emit.ct:309,214,117)——并发长跑无界,任务线程 GC 化/分段是流式多线程最大缺口 | **新发现①** |
| R4 | P0-3「无文档化用户面」 | 底座在:`scope { s.spawn }`/`join()/join_or()`/`Channel[T](cap)`/`Mutex[T]` with/with_mut/`Atomic`/`Global` 全有钉形(tests/06_concurrency.ct:16-53、06d/06e);双执行形态(pthread 默认 + `CTRON_RT=coro` N:M 协程 work-stealing,lib/net/c_src/ctron_rt.c) | **厚于 roadmap** |
| R5 | (roadmap 未见) | **scope 退出 join-all 未实现**(spec 07-concurrency.md:19 承诺;ct_scope 无子任务表,driver_emit.ct:168)——detached 任务静默丢失;sleep 不在 std(只 net 域);join/recv 无超时形(rt deadline 机器在库未暴露,ctron_rt.c:1815-1833);无 `scope.cancel()` 用户面(仅兄弟 panic→广播);Channel 半端 tx/rx 同指针,作字段/形参 = emit 截断在册(lib/db/pool.ct:11-14);parallel.map/reduce 发射实走串行(T33 fork-join 件已发射全仓零调用,driver_emit.ct:338-351 vs trans_expr.ct:767-796) | **新发现②** |
| R6 | P1-5「Windows 台账以 Darwin/ELF 为主」 | Windows 线已真实存在但是 β:ctron.ps1 + release.yml windows job(msys2 MINGW64)+ mingw-w64 已裁(MSVC 不入 v0,toolchain-distribution 设计 41-43);缺:日常 ci.yml 单平台 ubuntu、并发发射 `__sync` 内建 MSVC 缺(mingw=gcc 可用)、dlopen 桥 `_WIN32` 整段排除(driver_emit.ct:405-406) | **厚于 roadmap** |
| R7 | P1-6 链接旗标 | rpath/install_name/codesign/universal 全仓零命中;接缝 = `// ctron:link` 注释协议(driver_emit.ct:859)+ 三处硬编码链接行(ctron:449/487、ctc.sh:493、ctron.ps1:135/155);`#[link]` 今天只打印不进 cc(ctc.sh:210-230) | **确认零,接缝已勘明** |
| R8 | P1-7 SQLite | lib/db = pg(wire v3+SCRAM)/redis/pool/rowmap,门 56/56;SQLite/SQLCipher 零代码 | **确认零** |
| R9 | P1-8 下载路径 | 重定向✅(301/302/303/307/308,5 跳,client.ct:781-822);**但 https 直接拒——client 未集成 TLS**(client.ct:841-842)→ 模型下载(HF 等)不可用,此项是 P1-8 隐含前置;Range:服务端有、客户端无(206 无处理);流式体读无(整体驻留,max_body 1MiB,parse.ct:45-46);整请求 deadline 无(client.ct:39-40 在册);sha256 one-shot(Str 入 + 512MB 上限,crypto.ct:703-704;内部流式件 Sha256St 未公开 :709) | **新发现③(https)** |
| R10 | P1-4 lib/audio | CoreAudio/WASAPI/ALSA/PCM 全仓零命中 | **确认零;roadmap 自荐规避成立** |
| R11 | P2 W8052「只警示不治理」 | W8052 = extern C-ABI 边界类型治理警告(List/Atomic/Result/Mutex/Channel/Global 禁跨界,sem_main.ct:388-410);`ffi.sys_result` errno→Result 包装已存在(lib/ffi/ffi.ct:27-32)作正典模式 | **确认;正典化即可** |

已就位不再列为缺口(与 roadmap §0 一致并经复核):crypto(ed25519/sha256/hmac/rand)、json+ndjson、proc、fs/path/fio、FFI 基础(CBox/非捕获回调/repr(c)/str_from_c/cimport)、http 传输面(client/ws/sse)、tls(mbedTLS vendored,client+server 可用)、postgres/redis。

## 2. 总则:三缝原则与批次依赖

- **三缝**:①运行时面(发射模板 `compiler/src/driver_emit.ct` 与 `lib/net/c_src/ctron_rt.c`);②库面(`lib/` 新域/扩域);③驱动面(根 `ctron`、`compiler/ctc.sh`、`ctron.ps1` 的链接行与注册表)。**编译器前端语法零新增**;`#[...]` 属性族按既有堆叠机制增殖。
- **与在册泳道衔接**:链接旗标清单面挂构建驱动器 lane 的 `ctron.ctcl`(W1 已立表;若裁决走 env 直通则零耦合);GC 翻面沿用 T32 证据链(长驻 server P99 实测是剩余证据面,tests/COVERAGE.md:1570);F25 语言面 = repeat 字面量计划划入 G1 执行。
- **批次依赖与顺序(推荐)**:G1(内存)→ G3(线程面;与 G1 同缝必须串)→ G2(dlopen)→ G4(链接旗标;与 G2 同缝相邻)→ G5(下载,独立缝可并行)→ G6(SQLite,站 G2/G4 肩上或走 c_src 可并行)→ G7(Windows 收口验证,最后)。多泳道并行时 G5/G6 可与 G2/G4 分道(不同缝);G1/G3 禁并行(同文件)。

## 3. G1 长跑/流式内存边界(roadmap P0-2 + F25)

### 3.1 任务线程内存策略(本批核心,三案)

秘本引擎形态 = dlopen 大库 + 多任务(解码/推理/会话)长跑。今天 `ct_spawn` 置全局 `ct_gc_conc=1` 后一切分配永久落 bump 不回收(R3)——这是必须先裁决的结构点。

| 案 | 形态 | 得 | 失 |
|---|---|---|---|
| A | 任务线程全量参与 GC(STW 安全点:分配点轮询全局收集请求,全员停靠后主线程收集,各线程 TLS 帧链已可走 driver_emit.ct:200-204) | 语义最正,单堆无跨界 | 阻塞 FFI(sherpa 单次推理秒级)堵停全世界;安全点协议 + 每线程根集注册工程最大;M2 收敛前 GC CPU 墙(15-20×)雪上加霜 |
| **B(推荐)** | **任务段 bump:spawn 起每任务独立 arena 段,join 时整段回收;跨任务值传递一律深拷(通道 send 拷入 GC 堆,join 返回值拷回主堆)——「指针不入任务段外」立为 E 规则;spawn 不再置永久 `ct_gc_conc`** | 秘本「每音频段一任务」天然有界;FFI 阻塞零影响;实现量中等(arena 单例假设破除 + 段挂 ct_task + send/join 拷贝点);这正是 roadmap 要的「arena 分段生命周期」 | 单任务内 1 小时循环仍涨(化解:引擎分段结构 + Region 全档化见下);跨堆引用窗口需压测钉死(§3.6) |
| C | 维持现状 + 文档声明「并发进程 GC 不参与」 | 零工程 | 不满足 P0 验收线,否决 |

- 案 B 配套:**Region/Pool 从 bare 档提升到 full 档**(tests/08_bare.ct:19-48 已有 mark/reset 与 LIFO 复用钉形,落库 b0a9b970/9b28c4d2)——任务内手动分段工具,单任务长循环的官方出路。
- Global/Atomic 核对全局寿命分配路径(ctron_cell_new,trans_expr.ct:632-641)不入任务段。

### 3.2 GC 默认翻面(衔接 T32)

翻面唯一在册剩余证据 = 长驻 server P99 实测(tests/COVERAGE.md:1570,1597)。G1 内补跑该载荷(C10K/长连接混合,P99 采样),证据齐后按既有三选项裁决(记忆在案:A 现翻 / B 等 M2 / C 先测 P99,时点即此)。案 B(§3.1)落地后,多线程程序主线程 GC 保持活跃,翻面的收益面从单线程扩到全部。

### 3.3 流式 buffer 复用

- StringBuilder 发射臂单缓冲每 push 全量重分配(driver_emit.ct:132,O(n²)):补 `reserve(n)` + `clear()`,push 走容量倍增。
- 网络边界「调用方持缓冲」约定成文(ctron_net_read_t/lib/http client 全族已是此形):缓冲归调用方所有、实现零拷、生命周期一节写进 spec §7/§8。

### 3.4 F25 关闭 = repeat 字面量计划执行

F25(emit 内存膨胀)根因 = 巨型全零字面量(U8[65536] 6.5 万 AST 节点 ~GB 级前端常驻)。repo 侧修复 = `[v] ** N` repeat 计划(docs/superpowers/plans/2026-10-09-repeat-literal-plan.md,未动工),划入 G1 执行。**冲突消解呈裁**:loom 侧 F25 缓解依赖「短零字面量 + C 零填充」,而 repeat-plan §4 W3 拟将 `T[N]` 短字面量定为 E2010——两口径必须二选一,推荐按 repeat-plan 执行、loom 侧迁移 repeat 形态(J3)。

### 3.5 附带核销

linux glibc 下 seed 解释形态 emit RSS 线性爬至 15.4GB(docs/linux-seed-memory-evidence.md,发布链绕行):G1 内复测,治愈或豁免成文,不留无声债。

### 3.6 验收门

- 探针一(段回收):双任务循环分配 + join,RSS 曲线平(段回收生效)。
- 探针二(跨堆窗口,案 B 最险点):任务 recv GC 堆对象期间主线程高频收集(CTRON_GC 阈值 env 调小),压力循环无 UAF——通道环/拷贝时序的唯一裁判。
- 探针三(Send 深拷):任务 send 后改写本地再读对端,对端不受扰(值语义过界钉形)。
- 既有三档 gc 门(tests/gc/run.sh)+ bench digest 门全绿;`CTRON_GC=1` 下发射逐字一致探针(在规)不破。

## 4. G2 dlopen 补完(roadmap P0-1)

语法零新增,四缝 + 一夹具:

1. **`dlopen_global(path)` 新内建**:现 `dlopen` 硬编码 `RTLD_NOW`(trans_expr.ct:1429),无 GLOBAL 则 dlopen 进来的符号不进全局命名空间,`#[dlsym]` 的 `RTLD_DEFAULT` thunk 永远找不到。Ctron 无位运算算符(血律),不做 flags 形参,直接第二内建(发射 `RTLD_NOW|RTLD_GLOBAL`)。配 `dlerror_str()` 内建(经 str_from_c,错误面 Result 化的原料)。
2. **`#[dlsym(from_handle)]` 句柄 thunk 形态**:现 `dlsym` 内建返回裸 I64 不可调,`#[dlsym]` 只认全局域。新形态:extern 声明首个参数为 `I64` 句柄,thunk 发射为 `dlsym((void*)h, "<名>")` 逐调解析(v0 不缓存,登记在册)。sherpa-onnx 形态即:`let h = dlopen_global("..."); #[dlsym(from_handle)] fn sherpa_xxx(h: I64, ...) -> I64;`。
3. **链接旗标真消费**:`// ctron:link` 从「打印」变「进 cc」——三处链接行(ctron:449/487、ctc.sh:493、ctron.ps1:135/155)统一消费;属性面增 `#[link(search = "<dir>")]`(→ `-L`)与 `#[link(framework = "名")]`(darwin → `-framework 名`);任意旗标逃逸口 = `CTRON_LDFLAGS`/`CTRON_CFLAGS` env 直通(rpath/`-Wl,` 全走这,裁决 J 项见 §13 J8 备注)。
4. **c_src 自动收集对齐**:根 ctron 项目模式已自动并 `c_src/*.c`(ctron:485-487);ctc.sh emit 链补 use 图收集,两驱一致的最低限。
5. **真 .so 夹具**:tests/ffi 增 `dyn_so/` 专道(harness `cc -shared` 出插件,参照 link_math 专道形制 run.sh:134-151),断言:dlopen_global + #[dlsym] 命中、from_handle 命中、坏路径 dlerror 面负例。wasm/bare 维持硬拒(ctc.sh:462-465 fail-closed 已对);Windows 随 G7 换 LoadLibrary。

大库接入约定(sherpa-onnx/onnxruntime/llama.cpp 通用)写进 docs:预编译 dylib/dll vendoring 布局 + rpath(驱动旗标)与 dlopen_global(运行期)双形态取一。

## 5. G3 lib/thread 定型(roadmap P0-3)

### 5.1 模块面

`lib/thread/thread.ct` 门面(域包惯例:门面 re-export + 能力面)。三类:

| 类 | 件 | 说明 |
|---|---|---|
| 直通(文档化) | scope/spawn/join/join_or、Channel/Mutex/Atomic/Global | 底座已在(R4),写进 §07 spec 用户面一节 |
| 补实现 | **scope 退出 join-all**(ct_scope 挂子任务链 + scope 语句尾 join-all,spec 承诺兑现)、**sleep_ms/sleep_ns 提升 std**(现只 net 域,net.ct:41-42;内建化,不依赖 net) | 硬缺口 |
| 新增 | **join_t(handle, timeout_ms) / recv_t(ch, timeout_ms)**(rt deadline 机器已有 ctron_rt.c:1815-1833,只欠暴露;返回 Result)、**scope.cancel()**(检查点扩到 join 等待;Mutex with 等待 v0 不响应→文档化)、**TaskPool(n)**(coro 档天然池;pthread 档 = 池化线程 + 任务 Channel) | roadmap 点名三件 |

### 5.2 语义定案(执行形态不承诺)

- **承诺面**:scope 生命周期(join-all)、Send 契约(sem_send.ct 三检查点不变)、取消传播点清单(通道等待环✅/join✅/Mutex✗ 明文)、跨任务值传递深拷(G1 规则)。
- **不承诺面**:pthread 抢占式(默认)vs `CTRON_RT=coro` 协作式为执行形态,写明「语义同、性能档不同」,三平台验证随批次(darwin/linux 先,G7 补 windows)。
- 硬限制处置:spawn 捕获 >8 → env 槽扩容(trans_conc.ct:160,小改);嵌套 spawn 维持禁(v0 挂账)。
- Channel 半端截断在册坑:sem 硬禁先行(Sender/Receiver 作字段/形参 = 负例钉),真半端类型挂账。

### 5.3 parallel.map/reduce 接线裁决(J7)

T33 真 fork-join 件(ct_pmap_par/ct_pred_par,K=8 定长分块)已发射进每个程序但全仓零调用,trans_expr.ct:767-796 实走串行 mapL。推荐:接线真并行(件已在库,回归面用 coro_conc 压测守);否决项 = 登记串行(死代码常驻发射文本,不如删)。

### 5.4 验收门

tests/thread/ 新族:join 超时、池打满背压、取消风暴(cancel + 阻塞 recv/join 全员 Err)、scope-exit join-all(detached 不再静默)、pthread/coro 双臂全跑;TSAN 冒烟为 gate 可选步(env 门,P2 正式化)。并发发射三平台冒烟(darwin 先行)。

## 6. G4 链接旗标面与产物形态(roadmap P1-6)

与 G2 缝 3 同面,独立成批因含产物形态裁决:

1. **产物形态扩展**:native 靶注册表增 kind:`bin`(默认)/ `cdylib`(darwin `-dynamiclib` + install_name `@rpath/lib<名>.dylib`;linux `-shared -fPIC`;windows `-shared` 随 G7)。发射 C 文本不变,纯驱动面。这是 G2 夹具与秘本垫片库(若有)的地基。
2. **rpath/搜索路径**:`#[link(search)]` + `CTRON_LDFLAGS` 透传(G2 已立);`@rpath` 约定成文。
3. **universal binary**:`ctron build --universal`(darwin)——单遍 `cc -arch x86_64 -arch arm64` 透传(发射文本平台分叉全在 `#ifdef`,两 arch 同文本可编),失败回落 lipo 双遍(登记在册)。
4. **签名/公证不入 ctron(J8 推荐)**:codesign/entitlements/notarytool 是分发链职责,留打包脚本;ctron 只保证「产物可签」(标准 Mach-O,无 hardened 违规段)。roadmap「产物要能签名+公证」由 cdylib/bin 形态正确 + 分发文档满足。
5. **`-Wl,--stack` 可覆盖挂账**(toolchain-distribution 设计 164-165)同缝销账:`CTRON_LDFLAGS` 优先于平台默认。

## 7. G5 模型下载路径(roadmap P1-8)

1. **https 集成(J6)**:`client_request` 增 scheme 分支——https 走 tls 域握手后同状态机(tls_client/tls_read/tls_write_str 全件已在,tls.ct:66-100)。use 图树约束(http→tls→net 链形,禁菱形)随批验。否决组合层手工拼(模型下载是主路径,缝留给用户是倒置)。
2. **Range/206 客户端面**:Range 头今天可经 `extra` 手发,缺的是 206 识别(Content-Range 偏移校验、单范围、err 面);多段 Range 明确不做。
3. **流式体读**:响应状态机步进三件(`response_open/read_chunk/close`,与 sse_step 同形,client.ct:515-642 的就地分框复用)——边收边落盘,max_body 上限只辖非流式面。
4. **整请求 deadline**:client_request 增总预算参(内部换算每读截止,client.ct:39-40 销账)。
5. **sha256 流式**:lib/crypto C 快路径加 `sha256_init/update/final` 三 extern(U8[] 块入,破 Str 域 + 512MB 上限);std/crypto 纯 Ctron 臂公开 Sha256St 三件作回放对拍源(db 双源惯例)。
6. **lib/down 下载器域包**(编排层):Range 续传循环(断点重连)+ 流式哈希校验 + 重试退避——秘本模型拉取的直接消费面。
7. 验收:本地 serve e2e(Range 续传断点对比、流式哈希对拍 one-shot、https 夹具自签证书)。

## 8. G6 SQLite 绑定(roadmap P1-7)

- **推荐形态(J5):vendored amalgamation 走 c_src 同批**——`lib/db/sqlite/c_src/sqlite3.c` + `sqlite.ct` 门面(StdDb 能力 token 对齐 db.ct:18-25)。零新编译器面(项目模式 c_src 自动并入)、零外部依赖、跨平台一遍过;**SQLCipher = 换 amalgamation + pragma key**,同一张门面。amalgamation 编译慢(~秒级)登记在册,预编译 .a 缓存随 P2 vendoring 约定。
- 绑定底稿:cimport sqlite3.h 子集(tools/cimport.ct decl 级已覆盖原型/枚举/标量 struct);MVP 面 = open/prepare/step/bind 标量四族/column 取值/errmsg;事务 v0 = `exec("BEGIN")`。
- 系统库形态(`#[link]` 编译期 / dlopen_global 运行期)作为第二形态随 G2/G4 之后可选,不进 MVP。
- pg 式网络回放腿不需要(本地文件无网络面),真源 + 单测即可;门挂 tests/db(56/56 基线上增)。
- SQLCipher 加密版(涉 OpenSSL 依赖)挂 P2。

## 9. G7 Windows 链路收口(roadmap P1-5)

- **范围(J9):mingw-w64 only + x64 先**;MSVC 不入 v0(既有裁决维持);ARM64 windows 挂账。
- **日常门**:ci.yml 增 windows-mingw 冒烟 job(ctron build + 06 并发族 + dlopen 探针 + CJK 路径夹具)——今天 Windows 只活在 release.yml,日常门零覆盖。
- **dlopen 桥 Windows 落地**:driver_emit.ct:405-406 的排除段换 `#ifdef` 三行垫片(dlopen→LoadLibraryW(宽字符,CJK 路径)、dlsym→GetProcAddress、dlclose→FreeLibrary)。
- **验证矩阵**:并发(spawn/join/chan/coro 排除项明示)、`__sync` 内建 mingw 可用性钉形、栈旗标覆盖(CTRON_LDFLAGS)、CJK 路径全链(源码路径/产物路径/dlopen 路径)、cdylib 产物 + DLL 搜索路径。

## 10. P2 挂账(G8,不排期,触发即动)

| 件 | 处置 |
|---|---|
| 全文检索(jieba 移植 + 倒排) | v1 降级 LIKE/正则;fmap/heap 可承载,触发 = 秘本检索需求实证 |
| FFI 错误码→Result 约定 | `ffi.sys_result` 正典化成文(lib/ffi/ffi.ct:27-32)+ 可选 lint 插件(T52 接口 v2 面) |
| ASAN/UBSAN/leak-check | ctron gate 可选步(env 门);FFI 边界 fuzz 随 T52 沙箱面 |
| 大型二进制 vendoring 约定 | pkg registry v1 + G2 分发布局合流;SQLCipher/大模型库同条约 |

## 11. 秘本验收三线的 Ctron 侧落法(roadmap Stage 2)

| 三线 | Ctron 侧门 |
|---|---|
| RTF ≤ Rust 版 1.2× | tests/bench 新族 engine-proxy:PCM 分块循环 + FFI 桩调用(忙循环等效周期,无真库依赖),digest pin + ×3min 惯例;真 sherpa 对拍留秘本仓 A/B |
| 稳态内存 ≤1.5× | RSS pin 门:1h 模拟 soak(100 会话 × 每会话 N 段任务),RSS 曲线采样界;基线 = 会话状态理论大小 |
| 100 会话零崩 | soak rc=0 + 无 panic 文案(panic 消息流已在库,消息即证据) |

**Stage 0(steno-verify 纯计算 dogfood)不等 G 批**——jsonl/ndjson/proc/fs/crypto 全就位(roadmap §0 复核一致),可即刻动工;G 批服务 Stage 1–2。

## 12. 批次总表

| 批 | 内容 | roadmap 对应 | 主缝 | 验收门 | 依赖 |
|---|---|---|---|---|---|
| G1 | 任务段 bump + Region 全档化 + GC 翻面证据 + StringBuilder 复用 + repeat 字面量(F25)+ seed 15.4GB 核销 | P0-2 + FB-N+1 | driver_emit.ct | §3.6 三探针 + gc 三档门 | 无(首刀) |
| G3 | lib/thread 门面 + join-all + sleep/join_t/recv_t/cancel/TaskPool + 半端硬禁 + parallel 接线 + 死锁门 | P0-3 + FB-N+3 | driver_emit.ct + 新 lib/thread | §5.4 新族双臂 | G1(同缝必串) |
| G2 | dlopen_global/dlerror + #[dlsym(from_handle)] + 旗标真消费 + .so 夹具 | P0-1 + FB-N+2 | trans_expr.ct + 驱动链接行 | dyn_so 双形态探针 | G1 后任意时点 |
| G4 | cdylib 产物形态 + universal + rpath 约定 + 栈旗标销账 | P1-6 + FB-N+4 | 驱动面注册表 | 三平台产物形态探针 | 与 G2 同缝相邻 |
| G5 | https 集成 + 206 + 流式三件 + 整请求 deadline + sha256 流式 + lib/down | P1-8 + FB-N+6 半 | lib/http + lib/crypto + lib/down | 下载 e2e | 独立,可并行 |
| G6 | vendored sqlite amalgamation + sqlite.ct 门面 | P1-7 + FB-N+6 半 | lib/db + c_src | tests/db 增门 | 独立,可并行 |
| G7 | ci windows 冒烟 + LoadLibrary 垫片 + 验证矩阵 | P1-5 + FB-N+5 | driver_emit.ct + ci.yml | windows job 绿 | 收口,最后 |
| G8 | P2 四件 | §3 | 各缝 | 触发即动 | — |

## 13. 裁决点清单(呈用户)

| # | 裁决点 | 推荐 |
|---|---|---|
| J1 | 任务线程内存策略:案 B 任务段 bump(join 整段回收 + 跨任务深拷 E 规则)/ 案 A STW 安全点 GC / 案 C 现状 | **案 B** |
| J2 | GC 默认翻面时点:G1 内补 P99 载荷实测后翻 / 维持 opt-in(秘本侧显式开) | **实测后翻**(T32 三选项时点即此) |
| J3 | F25 形态冲突:按 repeat-plan 执行 `[v] ** N`,loom 侧迁移 repeat / 保短零字面量正典 | **按 repeat-plan**(短字面量 = W3 拟禁形态,不该是被依赖的正典) |
| J4 | 音频采集:确认架构规避(采集留 shell 层,Ctron 只吃文件/PCM,v1 不做 lib/audio)/ v1 做 CoreAudio+WASAPI 垫片 | **确认规避** |
| J5 | SQLite 形态:vendored amalgamation 走 c_src / dlopen 系统库 / #[link] 编译期链系统库 | **vendored amalgamation** |
| J6 | https:http client 内建 tls 集成 / 维持组合层手工拼 | **内建集成** |
| J7 | parallel.map/reduce:接线真 fork-join(件已在库)/ 登记串行并删死件 | **接线** |
| J8 | 签名/公证:不入 ctron,留分发脚本(ctron 只保证产物可签)/ ctron 子命令化(codesign 包装) | **不入 ctron**;旗标逃逸口走 env(CTRON_LDFLAGS/CFLAGS) |
| J9 | Windows 范围:mingw-only + x64 先,ARM64/MSVC 挂账 / 扩 ARM64 | **mingw-only + x64 先** |

## 14. 明确不做

- 依赖图构建系统(构建驱动器 lane 已裁,本设计全部批次不改此裁决)。
- 语言语法新增(spawn/await/async 关键字族;scope+Send+取消传播已是承诺面,协程档在库)。
- MSVC 支持 v0(既有裁决);多段 Range;嵌套 spawn;真 Channel 半端类型(硬禁先行);SQLCipher/OpenSSL 进主线。
- GC 分代/增量(M2 域,CPU 墙收敛另立,不混入 G1)。
