# Phase 4 真并发运行时(发射侧)实施计划

> 上游:`docs/superpowers/plans/2026-09-08-spec-gap-closure.md` Phase 4(状态:未启动 → 本计划)。
> 参考:`compiler-rust/src/trans.rs` 并发 C 运行时(PREAMBLE 2814–2975、emit_spawn 1054–1144、
> Mutex 2115–2145/2429–2494、parallel 3075–3084)。语义锚:**eval 顺序化模拟的可观察行为**
> + `tests/06*.ct / 05b_panic_join.ct` 语料。

**Goal:** 自举发射器(trans_*)支持并发构造的真 pthread 发射:scope/spawn(捕获闭包)/
join/join_or/Channel(send·recv)/Mutex(with·with_mut)/Atomic(fetch_add)/Global/
parallel(map·reduce)/任务 panic → 结构化取消传播。解释器口径不变。

**Architecture:** 新模块 `trans_conc.ct`(并发/闭包发射,build.sh TRANS 组);发射层
println → **静态缓冲**(`static let CT_OUT/CT_PEND/CT_TMP/CT_MODE` + `eln`/`etgt`),
使发射期才发现的文件作用域件(spawn shim、mcell typedef)先于函数体 flush;
`driver_emit.ct` 追加 pthread/setjmp 运行时样板并新增 `Static` 声明发射(缓冲静态
自身的自举闭环)。类型码扩展:`g`=ct_scope* `k`=ct_task* `h`=ct_chan* `R`=ct_res
`m<内>`=Mutex 单元 `P<内>`=内值指针伪码(with_mut 体内 Ident 解引用)。

## 语义对齐锚(eval ⇒ 发射)

| 构造 | eval 可观察行为 | 发射 |
|---|---|---|
| `scope {\|s\| b}` | 透明:跑 b,s=`SCOPE` | `ct_scope_new()` + s 绑 `g`;不开新 C 作用域语义 |
| `s.spawn(clo)` | 闭包即刻全执行;P/K 任务 | env=ct_i 数组(捕获按值浅拷贝)+ shim;`ct_spawn` |
| `t.join()` | P → 重抛 panic | `ct_join`:state==2 → `ct_panic(pmsg)` |
| `t.join_or()` | P → `Err("task panic")` 固定字面量 | `ct_join_or` → `ct_res{1,"task panic"}` |
| `ch.send(v)` | 满即 `Err("ScopeCancelled")`(不阻塞);`Ok(单位)` | 满时**阻塞等**,但取消广播后返回 Err;真并发差异点 |
| `ch.recv()` | 游标尽 → Err;否则 Ok(元素) | 对称;单指针双端 |
| Mutex.with | 回调得内值拷贝,写不可见 | lock+值拷贝+unlock |
| Mutex.with_mut | 内值经句柄可见写 | lock+`P<>` 指针绑定+unlock |
| Global/Atomic.with_mut | 整 cell 可见写 | glock+`&cell->v` 指针绑定 |
| `fetch_add(v)` | 返回旧值 | glock 下读改写,回 old |
| parallel.map/reduce | 逐元素即刻调用,串行 | fnptr 串行循环(与参考 v1 同) |

取消传播链:任务 panic → `scope->cancelled=1` + 广播全部通道 → 阻塞中 send/recv
重查取消 → `Err("ScopeCancelled")`(06e 语料锚,唯一不可顺序化行为)。

## 明不在本片(挂账 Phase 5)

一等闭包值(非 spawn/parallel/with 实参位)、用户枚举/Option·Result 字面量构造
(`Some`/`Ok` 裸构造;仅运行时 ct_res)、struct 值捕获(spawn 捕获限标量/Str/List/句柄)、
定长数组上的 parallel、`Atomic` 宽度域。

## 风险与对策

1. **println→eln 机械替换破坏既有发射产物** → 替换后对 trans_v0–v5 夹具产物与基线
   diff 必须逐字节一致(缓冲只改写出行时机,不改内容)。
2. **静态缓冲的自举闭环** → CT_* 是 core 静态,`bin/ctron-cc` 由发射编译自身而来,
   发射器必须支持 `Static` 声明(文件域 `static <ct> t_X = <init>;`),否则自编译产物
   丢静态。smoke --full 自发射收官/固定点即回归网。
3. **decls 锁定漂移** → 新 fn/Static 改变 decls=243,smoke 锁定值同步实测更新。

## Tasks

- [x] T1 trans_conc.ct:静态缓冲(eln/etgt)+ 类型码 `g/k/h/R/m/P`(trans_ty)+ Ident `P` 解引用
- [x] T2 driver_emit:pthread/setjmp 运行时(ct_scope/ct_task/ct_chan/ct_res/ct_fnptr/
      ct_spawn/ct_join/ct_join_or/ct_panic/ct_ch_send·recv/取消广播/glock)+ Static 发射 + 缓冲 flush
- [x] T3 println→eln 全量替换(trans_stmt/trans_emit/driver 测试包装);产物逐字节回归
- [x] T4 构造与成员发射:Channel/Mutex/Global 构造;spawn(捕获 env+shim)/join/join_or/
      send/recv/with/with_mut/fetch_add/expect/is_err/parallel.map·reduce;Scope 块
- [x] T5 struct 字段赋值(with_mut 体需要)+ match/ct_res Ok·Err 臂 + hoist 排除新码
- [x] T6 夹具 ×7(fx_conc_*)+ smoke 新节(逐夹具:seed 解释 == 原生执行逐字)
- [x] T7 全量回归:smoke --full + suite.py 51/51;decls 锁定更新;README/BOOTSTRAP/计划勾账

## 验收

1. 七夹具双向逐字(seed 解释 vs 发射→gcc→原生);
2. fx_conc_cancel 在真并发下验证取消传播(阻塞 send 得 Err,不悬挂);
3. smoke --full 全绿(含自发射收官与自举固定点);suite.py 与 C 宿主对齐不回退。
