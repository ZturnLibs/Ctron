# 服务器泳道 P7 实施计划(可观测与运维)

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development(既定模式)。Steps 用 checkbox 跟踪。

**Goal:** 落地路线 P7——std/log 结构化 kv、std/pb protobuf 线格式编解码、W3C traceparent 提取/注入、Prometheus 文本 metrics、OTLP/HTTP 导出(经本地 Collector 夹具解码比对)、/debug/scopes 任务树自省端点、优雅停机演练 + std/config 对接。出口 = todo_api 输出合法 Prometheus 文本 + OTLP 夹具解码一致 + traceparent 跨服务串联同 trace-id + /debug/scopes 与确定性 scope 树逐层一致 + tracing 层级与连接/请求树一致。

**Architecture:** 全部**纯 Ctron 组合层**(P5-E/P6 惯例沿用:门面 struct+自由函数+rc/getter 形);字节道 = &I64[] lane(I8-typedef 列 P9,每字节一条 I64);span 树 = scope 树零插桩(创新 2:/debug/scopes 走 rt 全协程链快照,不做手工埋点 API);OTLP 导出 = std/pb 编码 + http client POST(P8 Connect 桥的地基先行);metrics 热路径计数 = 预分配定长槽(零分配,沿 P6-F no_alloc 口径)。

**Tech Stack:** 纯 Ctron 层、std/time(iso_utc 时间戳)、std/json(OTLP JSON 形态候选·v1 以 pb 为主)、http/client(P4-C client_request)、net/c_src/ctron_rt.c(rt_coro 全协程链 = 自省快照唯一新增 C 面)。

## Global Constraints

- pathspec 提交纪律;**门禁采数一律 CTRON_EMIT=<worktree>/compiler/bin/ctron-emit**(worktree 隔离构建,共享树 bin 是移动地板;CTRON_STDPATH=$ROOT/std)。
- **P1b 显式请求契约**:跨包消费一律直 use 全部符号(含 struct)——锚穿形已死(divergences 2026-09-24 饿死族)。
- 预登记 emit 缺口谱(P6 沿用):①Sender/Receiver 字段化截断;②for 通配;③闭包内 match;④裸 with;⑤捕获槽写回;⑥lane .len=字面量槽数;⑦view 形参索引赋值禁;⑧spawn 闭包内 assert/panic 禁;⑨while 体 Drop 局部禁;⑩closure-return fn 禁 Drop 局部。中间件/装配/spawn 必踩 ③④——写码前对照。
- 双臂语料纪律(interp+emit);触时钟/net 子树的纯面语料用 x_ emit 专臂(W8052 解释口径)。
- **分层准入四问**(宪章 v2):std/log、std/pb = 零 IO 纯函数 → T1 档入册;metrics 注册表+exposition、traceparent、OTLP 导出 = 依 http 域 → http/frm/*(域包内,不入 std)。
- **ctron_rt.c 增量纪律**:共享基建(机刷泳道在飞)——动工前 git 重对齐,净增量(新 extern 函数+尾部),改动先在 worktree 验证再回共享树 pathspec 提交。
- no_alloc:metrics 计数/traceparent 注入热路径零分配(预分配定长槽,const 串直填 lane)。

---

### Task 1 (P7-A): std/log 结构化 kv 日志

**Files:**
- Create: `std/log.ct`(级别枚举 debug=0/info=1/warn=2/error=3;`log_line(level: I64, msg: Str, kv: Str) -> Str` 纯构行:ts=now_iso_utc + level 名 + msg + kv 串(kv="k1=v1 k2=v2" 约定形,esc 换行/等号);`log_env_level() -> I64`(env CTRON_LOG,缺省 info);`log_stderr(s: Str)`(触 net 时钟子树 → 消费方 x_ 专臂或纯构行分离))
- Create: `tests/log/`(级别过滤矩阵/时间戳在位/kv 转义(值含空格=引号包裹、换行=\n 转义、不可 printable 丢弃)/空 kv/层级缺省)

**Steps:**
- [ ] log_line 纯构行 + 级别过滤 + 转义面 + 语料(纯面双臂;stderr 写出面 x_ 专臂一条)
- [ ] 提交

### Task 2 (P7-B): std/pb protobuf 线格式编解码

**Files:**
- Create: `std/pb.ct`(写面:`pb_put_varint(buf,cap,pos,v: I64) -> I64 新 pos`;`pb_put_tag(buf,cap,pos,field,wire)`;`pb_put_key`/`pb_put_len` 定界;zigzag:`pb_zig32/pb_zig64`;读面游标形(仿 http parse 值 struct):`pb_next(buf,n,pos) -> PbField{rc, field, wire, v, start, len}`(rc 1=字段/0=EOF/-1=坏);wire 0=varint/1=64bit/2=ld/5=32bit;unknown-field skip:`pb_skip(buf,n,f) -> I64`;overlong varint/截断/越界全负例)
- Create: `tests/pb/`(官方编码向量锚:field1 varint 150=`08 96 01`、field2 ld "testing"=`12 07 74…`、zigzag sint `-1→01/-2→03`、负 i32 10 字节定宽形;嵌套 submessage 定界;未知字段 skip 往返;截断/overlong/超深负例矩阵;往返 property:encode→decode→encode 字节恒等)

**Steps:**
- [ ] 写面+读面+skip+向量语料(纯面双臂)
- [ ] 往返恒等+负例矩阵 + 提交

### Task 3 (P7-C): W3C traceparent 提取/注入

**Files:**
- Create: `http/frm/trace.ct`(值 struct `TrCtx{rc, ver, tid: Str, sid: Str, flags: I64}`+getter;`tp_parse(hfind 面: lane+头区 → TrCtx)`:W3C 严格形 `00-<32hex>-<16hex>-<2hex>`,版本非 00 走 forward 兼容(长度同形可解析)/全零 tid|sid 拒/非 hex 拒/长度错拒;`tp_serialize(buf,cap,t: TrCtx) -> I64`(定长 55 字节 const 形直填,零分配);`tp_flags_sampled(f)`/`tp_sampled_set`;请求侧提取(`tp_from_req(buf, hend)`)= 头区行首锚 traceparent;客户端注入(`tp_into_extra(extra: Str, t) -> Str`——仅在装配层调用,不在热路径))
- Create: `tests/http/frm_trace/`(W3C §3.2.2 合法矩阵/§3.2.3 非法矩阵:全零/长度/非 hex/版本 0f 假位/空白;serialize↔parse 往返;请求提取+客户端注入端到端(emit 专臂 x_ 带 socket))

**Steps:**
- [ ] parse/serialize/提取注入 + W3C 矩阵语料
- [ ] 双臂 + 提交

### Task 4 (P7-D): Prometheus metrics 注册表 + /metrics

**Files:**
- Create: `http/frm/metrics.ct`(定长注册表:预分配 `Metric{kind, name, help, vals: I64[K], labels: Str 形存储}` 槽阵(容量 const,注册期构造,热路径零分配);kind:counter/gauge/histogram(histogram 定长桶界 const 数组+cumulative 计数);`mx_inc/mx_dec/mx_add/mx_observe(name 影射 idx 由 comptime 期构造表查)`;exposition:`mx_expose(buf: &I64[], cap) -> I64` 写 Prometheus 文本格式(Help/Type/# 注释行、标签 `{code="200"}` 转义 `\"\\`、histogram `le=` 桶+`_sum/_count`;浮点样本定格式(P7 v1 整数计数为主,F64 面列志向——诚实登记))
- Create: `tests/http/frm_metrics/`(格式黄金向量(counter/gauge/histogram 全形)/标签转义/容量满注册拒/并发注:串行服务环下无竞态,coro 环登记)
- Modify: `examples/todo_api/src/main.ct`(/metrics 端点接线:http_total{code} counter、http_duration histogram、todo_items gauge;路由表 +1)

**Steps:**
- [ ] 注册表+exposition+语料(纯面双臂)
- [ ] todo_api /metrics 接线 + run.sh 断言行(文本格式合法性:Help/Type/样本行 grep 钉)+ 提交

### Task 5 (P7-E): OTLP/HTTP 导出 + traceparent 跨服务传播

**Files:**
- Create: `http/frm/otlp.ct`(span 值 struct `OtelSpan{tid, sid, parent, name, t0, t1, attr: kv}`;`otlp_encode_span(buf,cap,s)` = std/pb 定界嵌套(ResourceSpans→ScopeSpans→Span 骨架,字段号钉 OTLP v1 proto);`otlp_export(host, port, spans…) `= client_request POST application/x-protobuf,extra 携 tp 注入(传播面);批量:定长 span 槽,flush 阈值;失败计数暴露 metrics(mx_otlp_fail))
- Create: `tests/http/otlp_fixtures/`(**Collector 夹具接收端**:nc/自写 listener 收 POST → body 落盘 → std/pb 解码 → 与预期 span 字段逐项比对(rc/字段号/wire 类型/tag 值);**traceparent 跨服务传播夹具**:两服务串联(服务 A 收请求→tp 提取→起 span→client 调服务 B 携注入→B 提取验证同 trace-id→响应回 A),rt=coro 与默认双臂)
- Register: OTLP JSON 形态列志向(v1 pb 口径)

**Steps:**
- [ ] otlp_encode(std/pb 消费)+ Collector 夹具解码比对
- [ ] 导出端到端 + 传播夹具(同 trace-id 断言)+ 双臂 + 提交

### Task 6 (P7-F): /debug/scopes 任务树自省

**Files:**
- Modify: `net/c_src/ctron_rt.c`(**净增量尾部**:`ctron_rt_scopes_dump(buf, cap) -> I64` 走 rt_coro `allnext` 全链 + `jnext` join 链,输出 JSON 行形 `[{"id":N,"parent":N,"state":"run|park|join","ticks_ns":N}]`;static 全局自举计数;不动既有结构体)
- Modify: `net/`(facade 请求面:`rt_scopes_dump(buf, cap) -> I64` extern 声明+消费导出;P1b 契约显式请求)
- Create: `http/frm/debug.ct`(`/debug/scopes handler:调 rt_scopes_dump → JSON 直填响应;默认 RT=阻塞/非 coro 时返回 {"mode":"serial"}` 单字段诚实降级)
- Create: `tests/http/frm_debug/`(**确定性比对夹具**:CTRON_RT=coro 下 spawn 定形任务树(3 层,join 关系已知)→ /debug/scopes 快照 → 与 coro_det 同构期望树逐层比对 id/parent/state;emit 专臂)
- Notes: interp 臂 scope 树面 = 解释器自有权,本波不做对标(SKIP 登记口径);rt 增量先 worktree 验证

**Steps:**
- [ ] rt 净增量 extern + facade(worktree 验证 → 共享树 pathspec)
- [ ] /debug/scopes handler + 确定性比对夹具(双臂) + 提交

### Task 7 (P7-G): 停机演练 + std/config 对接 + 门禁收口 + P7 终审

**Files:**
- Create: `tests/http/p7_shutdown/`(停机中 inflight 请求演练:请求进行中发 /__shutdown → 当前请求完成响应后退出、新连接拒绝;run.sh 断言)
- Modify: `examples/todo_api/src/main.ct`(std/config 对接:log 级别/otlp endpoint/metrics 开关从 config 文件读,env 覆盖)
- Modify: `tests/COVERAGE.md`(P7 行)、`docs/c-rust-divergences.md`(执行发现)、本计划(执行记录+出口判定)、设计文档 §六 P7 as-built
- 全波包:tests/log、tests/pb、tests/http 全套、todo_api、net 14/14;bench 家族(CYCLE/ROUTE)quiet 口径

**Steps:**
- [ ] 停机演练 + config 对接
- [ ] 出口门禁五件逐项判定(Prometheus 合法性/OTLP 解码一致/传播同 trace-id/scopes 树一致/tracing 层级=连接请求树)
- [ ] pathspec 提交;P7 终审

## Self-Review

- 设计覆盖:§六 P7 全项(std/log/metrics/tracing span 树/std/pb/OTLP+traceparent//debug/scopes/停机演练/config)有任务;pprof 类 CPU 采样明确列 P9 不入。
- 风险前置:ctron_rt.c 为共享基建(净增量+worktree 先行纪律入约束);std/pb 与 OTLP 字段号钉形需官方 proto 对照(计划内锚定步骤);metrics 浮点样本 F64 面诚实列志向。
- 明确不做:pprof 采样(P9)、OTLP JSON 形态(志向)、分布式 trace 后端存储(生态档)、interp 臂 scopes 对标(登记口径)。
