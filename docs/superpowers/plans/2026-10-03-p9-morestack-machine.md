# P9 morestack 序言机器·实施计划(可增长连续栈全量解锁件)

> 授权链:T34 正案判决(docs/superpowers/specs/2026-09-26-server-p9-address-audit.md
> 终节)单列解锁条件「发射器 morestack 序言检查」;2026-10-03 用户令
> 「继续 work-stealing + 可增长连续栈全量 P9 栈经济」= 拍板触发。
> work-stealing 调度面已合龙(838b3a11,ws_steal 锚+c10k 满额),本件 = 栈面全量。

## 方案(唯一可行径,判决书指名)

**A′ = 分段+钉住+再入续跑(re-entry)**,go 1.2 分段栈同族:

- 发射器在**每个用户函数定义**序言插栈限检查;检查失败 → 运行时分配新段
  (mmap+guard,尺寸=任务栈同款),把「本函数重入」经新段执行,返回值拷回
  原调用点——**零帧搬移、零栈图、零寄存器镜像修复**,B 案三件机器全部回避。
- 段钉住(协程存活期不回收,逃逸栈地址恒有效;审计 A 案安全条件);
  任务死亡时段链随 munmap 一起终结(死亡任务的栈地址本就悬垂)。
- 切换复用既有 rt_swap/rt_ctx_init(红线②:切换 asm 零改动,只加调用方);
  段分配不走池不碰 G(段为任务私有,死亡即 munmap)。

序言形态(发射器产出,C 宿主 cc 编译):

```c
static <ret> t_<name>(<params>) {
    if (ctron_rt_stk_low(<EST>)) {              /* 序言检查(唯一热路径增量) */
        t_<name>_ms_a ar = { p0, p1, ... };     /* 实参值拷入桶 */
        <ret> r__; ctron_rt_stk_grow(t_<name>_ms, &ar, &r__); return r__;
    }
    ...原函数体(逐字不动)...
}
static void t_<name>_ms(void* a, void* r) {     /* 再入 thunk(段上执行) */
    t_<name>_ms_a* p = (t_<name>_ms_a*)a;
    *(<ret>*)r = t_<name>(p->f0, p->f1, ...);   /* 新段上重入;void 变体免写 r */
}
typedef struct { <T0> f0; <T1> f1; } t_<name>_ms_a;   /* 字段名 f<i> 免撞名 */
```

- 检查失败路径:grow 切到新段 → thunk 重入 t_f → 检查再跑(新段余量足,
  通过)→ 函数体在新段执行;返回 → thunk 写 r → 切回 → grow 返回 →
  原 t_f 帧立即返回调用方。调用方帧(旧段)从头到尾未搬动。
- 深嵌套:thunk 体里的调用继续序言检查,段链自然延伸;panic/longjmp 从段上
  展开 = 栈指针由 jmp_buf 还原到任务主栈,既有 ctron_panic 语义不变。

## EST(帧预算)与余量

- 发射器步查函数体计数:`est = 256 + 96 × let/形参节点数`(粗估,数组按
  节点计,不展开元素数——真帧超估由两级兜底:全局 MARGIN 2KB + guard 触顶
  诊断链 rc=101 原样保留为最后防线)。
- `ctron_rt_stk_low(need)`:强钩子内 `rem ≈ &局部 - c->stk_lo`,
  `rem < need + 2048` 即告。tls_cur 经 noinline 访问器读(TLS 槽位缓存危害
  对策同款,ctron_rt.c:412 在册)。

## 开关与兼容(缺省零变化)

- `CTRON_MORESTACK=1`:**发射期**门(bin/ctron-cc emit 读 env),缺省不注入,
  全部现有门禁/黄金/套件零变化。运行时无第二旋钮(检查未注入则钩子不可达)。
- 弱哑元 + 强符号顶替(P2-D 模板模式原样):垫片发
  `__attribute__((weak)) ctron_rt_stk_low/stk_grow` 哑元(未链 rt 恒 0/恒返回),
  ctron_rt.c 强定义顶替;Mach-O/ELF 双侧已证语义。
- 同形兼容:生长态跑 coro_det 同种子输出逐字节不变(S3 锚钉)。

## 红线遵守

1. ctron_rt.c 共享基建:worktree 先行(/tmp/ctron-p9ms),逐 commit pathspec
   限定;GUI 机刷在飞(gui_parse.ct 已被 peer 暂存),提交前核 HEAD 归属。
2. 切换 asm 零改动:rt_swap/rt_ctx_init 只加调用方,不动实现。
3. 三线口径:本机 = emit 面自宿发射器(bin/ctron-cc 全量用户程序链路);
   interp 无栈概念不涉;chk 不涉。C 宿主(ctronc emit)不同步 = 登记债
   (C 宿主仅 bootstrap 链内用,用户程序不经其发射面;divergences 立档)。

## 切片

- S1 rt 面:rt_coro 增 stk_lo/stk_hi/段链/ms 交接字段;创建点初始化栈界;
  强钩子 stk_low/stk_grow/ms_tramp(复用 rt_swap);段链死亡回收
  (stack_retire 邻位);文件头注 T34 节续写。验收:net 双矩阵/w7/coro_det
  默认路径全绿(零行为变化)。
- S2 发射器:trans_emit.ct ct_fn pass2 序言+thunk+est 步查(含 impl 合成 fn
  免费覆盖);泛型特化 pass1 直出位同批;driver_emit.ct 垫片 weak 哑元对;
  Test 块与 main 不注入(main=宿主栈 OS 自生长;test=主线程跑)。
  验收:默认发射字节不变(黄金对照);CTRON_MORESTACK=1 深递归夹具手验生长。
- S3 锚:tests/w7/ms_grow(生长对:同一深递归程序,机器开=算完返回,
  机器关=rc=101 触顶诊断;断言不钉生长次数)+ 生长态 coro_det 同形对;
  w7/run.sh 接线。
- S4 门禁:rt_core_smoke ns/yield ≤200ns 门(机器开与关双读);bench_cycle
  机器开 ≤1.05×;net 18/18;suite 99/99;smoke 余红照录基线。
- S5 文档:divergences C 宿主发射债;COVERAGE 工作志;设计底稿终局指针续写;
  判决档解锁条件翻面;记忆。

## 风险与回退

- 热路径增量 = 每函数入口一次 weak 调用+比较(仅机器开构建);门禁超标 →
  est 常量下沉/访问器内联化再议,缺省关始终保底。
- 分段抖动(go 1.2 已知病):调用方驻留旧段尾部循环调深函数 → 每轮一段;
  对策 = 每任务段数上限 64,超限走 rc=101 同族诊断(明确消息),v2 再议
  drop 注册表逃逸扫描回收。
- 发射器改动的合并竞态:trans_emit.ct 历史 peer 在飞档,提交前逐次重对齐。
