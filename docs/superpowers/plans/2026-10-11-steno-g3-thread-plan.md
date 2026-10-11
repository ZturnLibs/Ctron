# G3 lib/thread 定型 实施计划(scope join-all / sleep / 超时·取消 / TaskPool / 半端硬禁 / parallel 接线)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 秘本 P0-3 落地——线程面定型:spec 承诺的 scope 退出 join-all 兑现、sleep 脱网内建化、join_t/recv_t 超时形+scope.cancel() 取消用户面、TaskPool(n)、Channel 半端硬禁、parallel.map/reduce 接线真 fork-join(J7),tests/thread 门族 pthread/coro 双臂。

**Architecture:** 全部落在发射模板 `compiler/src/driver_emit.ct` 的并发运行时(ct_scope/ct_task/ct_chan 扩槽+新助手)+ `trans_conc.ct`(scope 语句尾 join-all;pool.spawn 发射面)+ `trans_expr.ct`(recv_t/cancel/join_t 成员臂;parallel 路由改真并行)+ `eval_call.ct`/`sem_calls.ct` 三处注册惯例 + `compiler-c` 双宿镜像(rt_eval/sem)。coro 档(CTRON_RT=coro)超时形 v0 回落无限等并文档化(协程停车机制 deadline 暴露挂后续);pthread 档为超时正典。

**Tech Stack:** Ctron 自举编译器+发射 C 运行时(pthread 条件等待超时)+ POSIX。门:`tests/thread/` 新族、既有 06 并发族、gc 三档、suite 双宿、coro 双臂。

## Global Constraints

- 三处注册惯例(内建/诊断):sem prelude 名 + trans 臂 + eval 镜像(C 宿主 rt_eval 随批)。
- 字符串裸 `{` 须 `\{`(闭花括号裸写);自举语境自由 fn 形(方法形=t_ 前缀);发射文本变更后 `.cache/emit` 清缓存真跑。
- 提交 pathspec 限定;共享树对端 WIP 断自举时走洁净 worktree 装配流(HEAD+我方文件→bin 拷回)。
- coro 档语义维持:超时形/池化在 coro 下的回落口径逐条文档化,不静默变义。
- 每 Wave 独立绿、独立提交;G1 任务段/通道边界值语义为既有地基不得回退。

---

### Wave 1: scope 退出 join-all(spec 07 承诺兑现)

**Files:**
- Modify: `compiler/src/driver_emit.ct`(ct_task 增 `snext/joined`;ct_scope 增 `head/tail`;ct_spawn 入链;ct_join/ct_join_or 置 joined;新助手 `ct_scope_join_all`)
- Modify: `compiler/src/trans_conc.ct`(scope 语句发射尾追加 join-all 行)
- Create: `tests/thread/run.sh`、`tests/thread/src/joinall.ct`

**Interfaces:**
- Produces(C):`static void ct_scope_join_all(ct_scope* sc)`——链表快照走查(单写者=scope 属主线程,免锁),未 joined 任务逐一 `ct_join`(panic 重抛/段回收/置 joined 复用既有语义);`ct_task` 增 `int joined; struct ct_task* snext;`。
- 语义:detached 任务静默丢失自此不存在——scope 块结束=全部子任务已 join;显式 join 过的跳过。

- [ ] Step 1: ct_task/ct_scope 扩槽 + ct_spawn 入链 + ct_join 族置 joined + 助手(driver_emit.ct)
- [ ] Step 2: trans_conc scope 语句尾 `ct_scope_join_all(<scope 变量>);`(找 ct_emit_scope_stmt 的发射文本锚)
- [ ] Step 3: 夹具 joinall.ct(scope 内 spawn 三任务不显式 join,块后断言副作用可见;任务 panic 传播一例)——**先红**(现状:任务体不执行完程序就退出/输出缺失)
- [ ] Step 4: 门:joinall 双形态(默认 pthread+CTRON_RT=coro)+06 并发族+gc 三档+suite
- [ ] Step 5: Commit(pathspec:driver_emit.ct trans_conc.ct tests/thread)

### Wave 2: sleep_ms/sleep_ns 脱网内建化

**Files:**
- Modify: `compiler/src/driver_emit.ct`(模板 `ctron_sleep_ms/ms→ns nanosleep`,POSIX 直调,零 net 依赖)
- Modify: `compiler/src/sem_calls.ct`(prelude 名)、`compiler/src/trans_expr.ct`(call 臂)、`compiler/src/eval_call.ct`(call_id 镜像)、`compiler-c/src/rt_eval.c`(C 宿主镜像)
- Create: `tests/thread/src/sleep.ct`

**Interfaces:**
- Produces:`sleep_ms(ms: I64)` / `sleep_ns(ns: I64)` 内建双宿;语义=阻塞当前任务/线程指定时长(非停车,coro 档下=v0 忙等口径文档化)。

- [ ] Step 1: 红腿 sleep.ct(时延下界断言:mono_ns 差值 ≥ ms)*先红*
- [ ] Step 2: 四处注册+模板 nanosleep
- [ ] Step 3: 门:双宿绿+06 族零回归
- [ ] Step 4: Commit

### Wave 3: 超时形+取消用户面(join_t / recv_t / scope.cancel)

**Files:**
- Modify: `compiler/src/driver_emit.ct`(ct_task 增 `done_cv/done`;shim 尾信号;`ct_join_t/ct_ch_recv_t/ct_scope_cancel`;join 等待点取消检查)
- Modify: `compiler/src/trans_expr.ct`(成员臂 recv_t/cancel)、`compiler/src/eval_call.ct`(镜像)
- Create: `tests/thread/src/timeout.ct`、`tests/thread/src/cancel.ct`

**Interfaces:**
- Produces:`t.join_t(ms) -> Result`(超时 Err("JoinTimeout"),任务续跑;其后 join/scope join-all 仍收口)、`rx.recv_t(ms) -> Result`(超时 Err("RecvTimeout");Str 载荷边界拷贝语义与 recv 同)、`s.cancel()`(cancelled 置位+广播;阻塞 recv/send/join_t 立即 Err("ScopeCancelled"))。
- coro 档:join_t/recv_t v0 回落无限等(文档化;协程 deadline 停车暴露挂后续)。
- Mutex with 等待不响应取消(v0 文档化,检查点清单成文)。

- [ ] Step 1: 红腿 timeout.ct(recv_t 超时 Err+到点数据不丢;join_t 超时后 join 收口)*先红*
- [ ] Step 2: C 助手三件+shim done 信号+成员臂双宿
- [ ] Step 3: cancel.ct(取消风暴:cancel 后阻塞 recv/join_t 全员 Err;兄弟 panic 传播回归 06e)
- [ ] Step 4: 门:pthread/coro 双臂+06e 取消族回归
- [ ] Step 5: Commit

### Wave 4: TaskPool(n)

**Files:**
- Modify: `compiler/src/driver_emit.ct`(ct_pool 运行时:N 工作者+无界任务队列(shim,env 对);ct_pool_spawn)
- Modify: `compiler/src/trans_conc.ct`(pool.spawn 闭包发射=env 打包入队,复用 spawn 闭包机器)、`compiler/src/sem_calls.ct`/`eval_call.ct`
- Create: `tests/thread/src/pool.ct`

**Interfaces:**
- Produces:`let p = TaskPool(4)`;`p.spawn(closure)` 入队(返回队列满/池关 Result,v0 无界=恒 ok);池生命周期=绑定作用域(析构=排空+停工)。
- 语义:池任务与 scope 任务同受 G1 任务段纪律(工作者线程=长命,段=任务粒度入队时建);GC 档=工作者线程置退避口径与 pmap 同。

- [ ] Step 1: 红腿 pool.ct(4 工作者消化 100 任务,计数原子面)*先红*
- [ ] Step 2: C 运行时+发射面双宿
- [ ] Step 3: 门:pool 双形态+池打满背压+gc 三档
- [ ] Step 4: Commit

### Wave 5: Channel 半端硬禁 + parallel 接线(J7)

**Files:**
- Modify: `compiler/src/sem_walk.ct`(Channel 作 struct 字段/fn 形参探针→硬禁或实证修通,裁决点随探针定)、`compiler-c/src/sem.c`(镜像)
- Modify: `compiler/src/trans_expr.ct`(parallel.map/reduce 路由 ct_pmap_par/ct_pred_par)、`compiler/src/driver_emit.ct`(pmap 入口置 ct_gc_conc=1 退避)
- Create: `tests/thread/src/pmap.ct`

**Interfaces:**
- Produces:parallel.map/reduce 真 K=8 fork-join(件已在库零调用转正);半端硬禁新码或修通记录。
- 风险:T33 段错误史(ct_emit_clov 已修);pmap 工作者线程分配路径=全局 arena 锁面,conc 退避置位兜底。

- [ ] Step 1: 半端探针(Channel[T] 作字段/形参双口径编译+运行)→ 硬禁 E 新码 or 修通
- [ ] Step 2: parallel 接线+conc 兜底;06f 回归(串行转并行的输出一致性)
- [ ] Step 3: 门:tests/thread 全族双形态+suite+smoke 并发块
- [ ] Step 4: Commit + COVERAGE 批终登记

---

## 验收对照(设计 §5.4)

| 设计门 | Wave |
|---|---|
| join 超时 | W3 |
| 池打满背压 | W4 |
| 取消风暴 | W3 |
| scope-exit join-all | W1 |
| pthread/coro 双臂 | 全 Wave |
| 半端硬禁/修通 | W5 |
| parallel 接线 | W5 |
| TSAN 冒烟(gate 可选步) | 挂 gate 可选,不阻塞批 |
