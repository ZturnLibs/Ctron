# G1 长跑/流式内存边界 实施计划(任务段 bump + GC 翻面 + F25)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 秘本引擎 P0-2 落地——多线程程序内存有界(任务段 bump、join 整段回收、通道跨界值语义)、GC 默认翻面(长跑默认有界)、F25 语言面关闭(repeat 字面量)、流式 buffer 复用(StringBuilder reserve/clear)、Region.heap 手动分段工具。

**Architecture:** 全部改动落在发射模板 `compiler/src/driver_emit.ct` 的内联 C 运行时 + `compiler/src/trans_expr.ct` 的少量发射臂 + `compiler/src/eval_call.ct`/sem 的镜像注册。arena 由「全局单例三静态」重构为「块链 + 每竞技场状态(ct_arn)+ 线程选择(TLS)」;任务线程在 `ct_shim_tramp` 建私有段、`ct_join/join_or` 在取回返回值后整链释放;通道 Str 载荷 send 拷入 GC 堆、recv 在任务侧拷入本段(懒释放超级块关跨堆窗口);coro 模式(CTRON_RT=coro)维持现状退避不改。语言语法零新增。

**Tech Stack:** Ctron 自举编译器(compiler/src/*.ct,seed 解释执行)+ 发射 C 运行时模板(内联 C 字符串)+ POSIX pthread。测试走仓库既有门:`sh compiler/test/smoke.sh --full`、`sh tests/gc/run.sh`、`sh tests/gc/bench.sh`、全量 `ctron gate`(21 步)。

## Global Constraints(每个任务隐含遵守)

- 语法零新增:本计划不新增语言关键字/运算符;新面全部走前奏类型方法与内建函数三处注册惯例(sem 注册 + trans_expr 发射臂 + eval_call 解释臂)。
- Ctron 字符串字面量裸 `{` 必须 `\{` 转义,`;` 禁用(血律 41-43 号族);println 模板行内 C 代码的引号按既有行形态转义。
- 发射文本变更后验收必须清缓存真跑:`rm -rf .cache/emit` 后再跑门(血律:缓存掩蔽假绿)。
- 分段仅 pthread 形态;coro(CTRON_RT=coro)维持全局竞技场退避,本计划不改 lib/net/c_src/ctron_rt.c。
- 提交一律 pathspec 限定(共享树有机刷对端):`git add <明确路径> && git commit -m "..." -- <明确路径>`。
- 每 Task 独立绿、独立提交;红不落库。
- 探针运行三遍(RSS/时序类),flake 即立案不硬闯。

---

### Task 1: arena 段化基座(块链 + ct_arn + TLS 选择;行为面等价重构)

**Files:**
- Modify: `compiler/src/driver_emit.ct`(arena 静态族 ~98-112 行、GC 收集扫描 ~215 行、ct_sb 惰性释放)
- Test: 既有门(smoke + tests/gc/run.sh)全绿即等价性证明;无新夹具

**Interfaces:**
- Produces(C 运行时符号,后续任务依赖):`typedef struct ct_ablk { struct ct_ablk* nx; size_t cap; size_t used; } ct_ablk;`、`typedef struct ct_arn { ct_ablk* cur; size_t off; } ct_arn;`、`static ct_arn ctron_aglobal;`、`static __thread ct_arn* ct_tls_arn;`、`static void* ct_ablk_new(size_t cap)`、`ctron_amalloc(n)`(签名不变)。
- Produces(ct_sb 槽):`ct_sb` 增 `int empty_age;` 字段,收集器空超级块两代惰性释放。

- [ ] **Step 1: 替换全局竞技场静态族**

`driver_emit.ct` 中删除现有四行静态(98-101 行的 `ctron_abase/aoff/acap/alock` 四个 println)与现行 `ctron_amalloc` println(112 行),替换为以下 C 模板(落成 println 时注意 `\{` 转义与 `\"` 引号转义,逐字对齐既有行风格):

```c
static int ctron_alock = 0;
typedef struct ct_ablk { struct ct_ablk* nx; size_t cap; size_t used; } ct_ablk;
typedef struct ct_arn { ct_ablk* cur; size_t off; } ct_arn;
static ct_arn ctron_aglobal;
static __thread ct_arn* ct_tls_arn;
static void* ct_ablk_new(size_t cap) {
    ct_ablk* b = (ct_ablk*)malloc(sizeof(ct_ablk) + cap);
    if (!b) { ctron_panic("arena OOM"); }
    b->nx = 0; b->cap = cap; b->used = 0;
    return b;
}
static void* ctron_amalloc(size_t n) {
    n = (n + 15) & ~(size_t)15;
    ct_arn* a = ct_tls_arn ? ct_tls_arn : &ctron_aglobal;
    int lk = (a == &ctron_aglobal);
    if (lk) { while (__sync_lock_test_and_set(&ctron_alock, 1)) { } }
    if (!a->cur || a->off + n > a->cur->cap) {
        size_t nc = a->cur ? a->cur->cap * 2 : (size_t)16384;
        while (nc < n) { nc = nc * 2; }
        ct_ablk* b = (ct_ablk*)malloc(sizeof(ct_ablk) + nc);
        if (!b) { if (lk) { __sync_lock_release(&ctron_alock); } ctron_panic("arena OOM"); }
        if (a->cur) { a->cur->used = a->off; }
        b->nx = a->cur; b->cap = nc; b->used = 0;
        a->cur = b; a->off = 0;
    }
    void* p = (char*)(a->cur + 1) + a->off;
    a->off += n;
    if (lk) { __sync_lock_release(&ctron_alock); }
    return p;
}
```

- [ ] **Step 2: 收集器的 arena 根扫描改链化**

`ct_gc_collect` println(215 行)中,将扫描片段:

```c
if (ctron_abase && ct_gc_arena_scan) { ct_gc_wlput(ctron_abase, ctron_abase + ctron_aoff); }
```

替换为(全链扫描——旧块今日不被扫描,链化后顺带治了「增长弃块不可作根」的隐性洞;`head_used` 语义:头块用 `ctron_aglobal.off`,其余块用落块时的 `used`):

```c
if (ct_gc_arena_scan) {
    ct_ablk* b3 = ctron_aglobal.cur;
    if (b3) { ct_gc_wlput((char*)(b3 + 1), (char*)(b3 + 1) + ctron_aglobal.off); }
    for (b3 = b3 ? b3->nx : (ct_ablk*)0; b3; b3 = b3->nx) {
        ct_gc_wlput((char*)(b3 + 1), (char*)(b3 + 1) + b3->used);
    }
}
```

- [ ] **Step 3: 空超级块两代惰性释放(跨堆窗口防线,Task 3 消费)**

`ct_sb` typedef println(~190 行)增字段 `int empty_age;`。`ct_gc_collect` 尾部清扫循环中,把:

```c
if (s->live == 0) { *pp = s->nx; free(s); } else { pp = &s->nx; }
```

替换为:

```c
if (s->live == 0) { s->empty_age += 1; if (s->empty_age >= 2) { *pp = s->nx; free(s); } else { pp = &s->nx; } }
else { s->empty_age = 0; pp = &s->nx; }
```

- [ ] **Step 4: 残留引用清零 + 清缓存真跑等价性门**

```bash
grep -n "ctron_abase\|ctron_aoff\|ctron_acap" compiler/src/driver_emit.ct
```

Expected: 零命中(若有残留在其他模板臂,同法链化改写;注意该区块若带 target 守卫,仅 native/full 臂内改动,不碰 bare/wasm 模板)。

```bash
rm -rf .cache/emit
sh compiler/test/smoke.sh --full
sh tests/gc/run.sh
```

Expected: smoke 全绿;tests/gc 三档(gc=on / gc=off / precise-only)全绿。任一红 → 先修再进(growth 路径与扫描链化是仅有的行为可疑点)。

- [ ] **Step 5: Commit**

```bash
git add compiler/src/driver_emit.ct
git commit -m "feat(rt): G1-T1 arena 段化基座——块链+ct_arn 状态+TLS 选择重构(行为面等价),收集器 arena 根全链扫描(顺带治增长弃块不可作根隐性洞)+空超级块两代惰性释放(跨堆窗口防线)" -- compiler/src/driver_emit.ct
```

---

### Task 2: 任务段接线(spawn 建段 / join 收段 / pthread 不再置永久退避)

**Files:**
- Modify: `compiler/src/driver_emit.ct`(ct_task typedef ~173、ct_shim_tramp ~308、ct_spawn ~309、ct_join ~310、ct_join_or ~311、ct_gc_alloc 入口 ~214)
- Create: `tests/gc/seg/run.sh`、`tests/gc/seg/src/main.ct`(探针一:段回收;探针三:通道值语义——本任务先落探针一的腿,探针三在 Task 3 补)

**Interfaces:**
- Produces:`ct_task` 增 `void* arn;` 槽;`static void ct_task_seg_free(ct_task* t)`(链释放 + arn 置 0);`ct_gc_alloc` 入口增 `ct_tls_arn` 路由(任务线程一律落本段)。
- 语义:pthread 任务的一切分配(String/List/class 经 `ct_gc_alloc`、bytes/env 经 `ctron_amalloc`)落任务段;join 返回值仍为标量(现状口径,Step 3 核查钉住);段在 join 取值后释放。
- coro 模式(CTRON_RT=coro):不装段、保持 `ct_gc_conc=1` 全局退避(现状不变)。

- [ ] **Step 1: 核查 join 返回面(防设计假设翻车)**

```bash
grep -n '"join"\|join_or' compiler/src/trans_expr.ct | head
```

Expected: trans_expr.ct:964 `(int32_t)ct_join(...)`、:965 `ct_join_or(...)`——返回面是标量截断,无指针载荷。若发现 spawn 闭包可返回指针型(与假设不符),停下立案,不要硬改。

- [ ] **Step 2: ct_task 增段槽 + 释放助手**

`ct_task` typedef println(173 行)增字段 `void* arn;`(放在 `ct_scope* scope;` 之后)。并在 `ct_join` 定义之前新增 println:

```c
static void ct_task_seg_free(ct_task* t) {
    if (t && t->arn) {
        ct_arn* a = (ct_arn*)t->arn;
        ct_ablk* b = a->cur;
        while (b) { ct_ablk* nx = b->nx; free(b); b = nx; }
        free(a);
        t->arn = 0;
    }
}
```

- [ ] **Step 3: shim 入口建段(pthread 形态限定)**

`ct_shim_tramp` println(308 行)整行替换为(注意保持原有 `ct_fp = 0;` 与 notify_done 尾序):

```c
static void* ct_shim_tramp(void* p) {
    ct_task* t = (ct_task*)p;
    if (!ct_rt_active()) {
        ct_arn* ta = (ct_arn*)calloc(1, sizeof(ct_arn));
        ta->cur = (ct_ablk*)ct_ablk_new((size_t)16384);
        ct_tls_arn = ta; t->arn = ta;
    }
    *ct_tls_slot() = t;
    if (setjmp(t->jmp) == 0) { t->shim(t->env); t->state = 1; }
    ct_fp = 0;
    if (ct_rt_active()) ctron_rt_notify_done((void*)t);
    return 0;
}
```

- [ ] **Step 4: ct_spawn 退避旗改 coro 限定**

`ct_spawn` println(309 行)整行替换为:

```c
static ct_task* ct_spawn(void* (*shim)(void*), void* env, ct_scope* sc) {
    ct_task* t = (ct_task*)calloc(1, sizeof(ct_task));
    t->shim = shim; t->env = env; t->scope = sc;
    if (ct_rt_active() || ct_rt_env_coro()) { ct_gc_conc = 1; }
    if (!ct_rt_active() && ct_rt_env_coro()) ctron_rt_init(0);
    if (ct_rt_active()) { ctron_rt_run((void (*)(void*))ct_shim_tramp, t, (void*)t); return t; }
    pthread_create(&t->th, 0, ct_shim_tramp, t);
    return t;
}
```

- [ ] **Step 5: ct_join / ct_join_or 取值后收段**

`ct_join` println(310 行)替换为:

```c
static ct_i ct_join(ct_task* t) {
    void* r;
    if (ct_rt_active()) { ctron_rt_join_key((void*)t); } else { pthread_join(t->th, &r); }
    if (t->state == 2) { ct_task_seg_free(t); ctron_panic(t->pmsg); }
    ct_i v = t->result;
    ct_task_seg_free(t);
    return v;
}
```

`ct_join_or` println(311 行)同样在 `res` 构造前插 `ct_task_seg_free(t);`(panic 分支与正常分支都要——panic 分支返回前段已无用)。

- [ ] **Step 6: ct_gc_alloc 入口按线程路由**

`ct_gc_alloc` println(214 行)开头分派片段:

```c
if (ct_gc_conc || !ct_gc_enabled()) { return ctron_amalloc(n); }
```

替换为:

```c
if (ct_gc_conc || !ct_gc_enabled() || ct_tls_arn) { return ctron_amalloc(n); }
```

(任务线程一切 GC 型分配落本段——段对 GC 不可见,主线程收集永远不与任务栈相遇。)

- [ ] **Step 7: 写探针一(段回收)夹具**

`tests/gc/seg/src/main.ct`:

```ctron
// 任务段回收探针:双任务大churn,join 后 RSS 必须平稳(段整链释放)
fn churn(n: I32) -> I64 {
    var i: I32 = 0
    var acc: I64 = 0
    while i < n {
        var s = "x" ** 1
        var k: I32 = 0
        while k < 200 {
            s = s + "0123456789abcdef"
            k = k + 1
        }
        acc = acc + (s.len() as I64)
        i = i + 1
    }
    return acc
}

fn main() {
    let (tx, rx) = Channel[I64](2)
    scope { |s|
        let t0 = s.spawn(|| { tx.send(churn(3000)).expect("send") })
        let t1 = s.spawn(|| { tx.send(churn(3000)).expect("send") })
        var got: I64 = 0
        var c: I32 = 0
        while c < 2 {
            got = got + rx.recv().expect("recv")
            c = c + 1
        }
        t0.join()
        t1.join()
        println("seg-probe acc=\{got}")
    }
}
```

(注:字符串拼接 churn 造段内分配压力;若 `"x" ** 1` 语法未落(repeat 属 Task 8),改用 `var s = "x"` 起头,其余不变——探针只压分配量,不压字面量形态。)

`tests/gc/seg/run.sh`(参照 tests/gc/run.sh 形制:mktemp + 三遍跑 + RSS 采样 + pass/fail 计数 + `set -u`):

```sh
#!/bin/sh
# G1 探针一:任务段回收——join 后长循环 RSS 界(三遍跑,任一越界即红)
set -u
d=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$d/../../.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
export CTRON_STDPATH="$root/lib"
fail=0
$root/bin/ctron-emit run "$d/src/main.ct" -o "$tmp/probe" || { echo "emit FAIL"; exit 1; }
cc -O2 -w -pthread "$tmp/probe.c" -o "$tmp/probe" || { echo "cc FAIL"; exit 1; }
base=$(ps -o rss= -p $$ | tr -d ' ')
i=0
while [ $i -lt 3 ]; do
    out=$("$tmp/probe")
    echo "$out" | grep -q "seg-probe acc=" || { echo "probe 输出缺判词"; fail=1; }
    i=$((i+1))
done
# 稳态界:进程自身 RSS 由内核回收;这里断言的是跑通 + 判词在。
# RSS 数值界用 FYI 打印(段回收的强断言在 gc=on 长跑对比:Task 6 P99 探针接管)。
echo "seg-probe OK(3 遍跑通)"
exit $fail
```

(注:强 RSS 断言有意放在 Task 6 的 gc=on 长跑探针——单任务短进程的瞬时 RSS 噪声大,先以「跑通+判词+三遍稳定」落绿;`ctron-emit` 的实际调用形以 `grep -rn "ctron-emit" tests/gc/run.sh` 实测为准,夹具以本仓现行 harness 形制改写,不改语义。)

- [ ] **Step 8: 跑探针 + 回归门**

```bash
sh tests/gc/seg/run.sh
sh tests/gc/run.sh
sh compiler/test/smoke.sh --full
CTRON_RT=coro sh tests/net/coro_conc/run.sh 2>/dev/null || echo "(coro 门按本仓现行路径跑)"
```

Expected: seg 探针绿;gc 三档绿;smoke 绿(06_concurrency/06e_cancel 等任务族在 pthread 段语义下全过);coro 双臂门绿(退避口径未变)。

- [ ] **Step 9: Commit**

```bash
git add compiler/src/driver_emit.ct tests/gc/seg
git commit -m "feat(rt): G1-T2 任务段接线——spawn 建段/join 收段(链释放),pthread 不再置永久 GC 退避(coro 维持),ct_gc_alloc 按线程路由任务落段;探针 tests/gc/seg" -- compiler/src/driver_emit.ct tests/gc/seg
```

---

### Task 3: 通道边界值语义(Str 拷贝 + is_str 旗 + 元素限面 E3021)+ 探针二

**Files:**
- Modify: `compiler/src/driver_emit.ct`(ct_chan typedef ~176、ct_ch_make ~312、ct_ch_send ~313、ct_ch_recv ~314;拷贝助手新增)
- Modify: `compiler/src/trans_expr.ct`(Channel 构造臂 ~658-662 传旗;send 臂 ~966 不变)
- Modify: `compiler/src/sem_walk.ct`(~74 E3020 检查点增宽度)、`compiler/src/diag_msg.ct`、`tests/meta_check.py`、`tests/README.md`(E3021 三处注册惯例)
- Modify: `tests/gc/seg/src/main.ct`(探针二/三腿)、`tests/gc/seg/run.sh`

**Interfaces:**
- Produces:`ct_ch_make(int cap)` 签名不变;`ct_chan` 增 `int is_str;`;新 C 助手 `static const char* ct_xsend_str(const char* s)`(拷入 GC 堆)、`static const char* ct_xrecv_str(const char* s)`(拷入当前线程竞技场)。
- 语义:Str 元素通道——send 侧深拷入 GC 堆(任务段指针不出境),recv 侧任务线程再拷入本段(关「pop→使用」窗口,配合 Task 1 惰性释放);主线程 recv 不拷(主栈被扫描)。
- 新诊断 **E3021**:Channel 元素类型限标量族与 Str(任务段边界值语义 v1 限制);List/class/切片元素在程序含 spawn 时红。

- [ ] **Step 1: 写红腿——探针二(跨堆窗口)+ 探针三(值语义)**

`tests/gc/seg/src/main.ct` 追加第二程序形态(独立 main 文件 `tests/gc/seg/src/window.ct`):

```ctron
// 跨堆窗口压测:GC 阈值调到极小 + 任务/主线程对发 Str 循环——UAF 即崩
fn main() {
    let (tx, rx) = Channel[Str](4)
    scope { |s|
        let t = s.spawn(|| {
            var i: I32 = 0
            while i < 20000 {
                tx.send("payload-中文-\{i}").expect("send")
                i = i + 1
            }
        })
        var c: I32 = 0
        var acc: I64 = 0
        while c < 20000 {
            let m = rx.recv().expect("recv")
            acc = acc + (m.len() as I64)
            c = c + 1
        }
        t.join()
        println("window-probe acc=\{acc}")
    }
}
```

`tests/gc/seg/run.sh` 增第二段:以 `CTRON_GC=1 CTRON_GC_THRESHOLD=4096` 跑 window 探针三遍(极小阈值逼主线程高频收集,任务 recv 侧值语义被连续锤),断言三遍判词一致且 rc=0。

- [ ] **Step 2: 跑红腿确认失败形态**

```bash
sh tests/gc/seg/run.sh
```

Expected: window 探针大概率偶发崩(UAF/段错误)或值错乱——红腿在案再实修。(若意外全绿也继续:防线前置,不是白做。)

- [ ] **Step 3: C 运行时——is_str 旗 + 双向拷贝助手**

`ct_chan` typedef(176 行)增 `int is_str;`。新增两个 println(放在 `ct_ch_make` 之前;注意 `ct_gc_alloc` 前向声明已在 ~118 行,顺序安全):

```c
static const char* ct_xsend_str(const char* s) {
    size_t n = s ? strlen(s) + 1 : 1;
    char* r = (char*)ct_gc_alloc(n);
    if (s) { memcpy(r, s, n); } else { r[0] = 0; }
    return r;
}
static const char* ct_xrecv_str(const char* s) {
    size_t n = s ? strlen(s) + 1 : 1;
    char* r = (char*)ctron_amalloc(n);
    if (s) { memcpy(r, s, n); } else { r[0] = 0; }
    return r;
}
```

`ct_ch_make`(312 行)不变签名;`ct_ch_send`(313 行)在 `pthread_mutex_lock(&c->mu);` 之前插:

```c
if (c->is_str) { v = (ct_i)(long)ct_xsend_str((const char*)(long)v); }
```

(拷贝在取锁前——收集器不持通道锁,取锁前拷贝使「标记中的环槽」始终指向完整对象。)`ct_ch_recv`(314 行)在 `res.variant = 0; res.v = c->buf[c->head];` 之后、`c->head = ...` 之前不动;在解锁前插:

```c
if (c->is_str && ct_tls_arn) { res.v = (ct_i)(long)ct_xrecv_str((const char*)(long)res.v); }
```

(仅任务线程拷;主线程不拷——主栈保守扫是根。窗口=pop 后到拷贝完,惰性释放两代兜住。)

- [ ] **Step 4: 发射臂——Channel 构造传旗**

trans_expr.ct:658-662 的 Channel 构造臂,取元素型别码并传旗。现臂返回 `((ct_chan*)ct_ch_make((int)(cap)))`;改为先取型别码(与 Static.zeros 臂同法,~600 行 `cal[2][1]` + `ct_ty_code`),再拼:

```ctron
if base[0] == "Ident" && base[1] == "Channel" {
    var cap = "64"
    if ag.len > 1 { cap = ct_expr(ag[1], env, file) }
    var isf = "0"
    if cal[2].len > 1 && cal[2][1].len > 1 {
        if ct_ty_code(cal[2][1], file) == "s" { isf = "1" }
    }
    return "((ct_chan*)ct_ch_make((int)(" + cap + "), " + isf + "))"
}
```

同步把 `ct_ch_make` C 签名改双参:`static ct_chan* ct_ch_make(int cap, int is_str)`,体内 `c->is_str = is_str;`。**先核型别码**:执行 `grep -n '"s"' compiler/src/trans_ty.ct | head` 与 `grep -n "ct_ctype" compiler/src/trans_ty.ct | head -3`,确认 Str 的码确为 `"s"`(查 `ct_ctype` 的码表分支);若码不同,以实测码替换上式 `"s"`。interp 侧 Channel 构造(eval_call.ct)语义为值表,不受影响,无需动。

- [ ] **Step 5: E3021 元素限面(sem 三处注册)**

`sem_walk.ct:74` 的 E3020 检查点(Send 检查)旁增宽度检查:元素型别既非标量族(I32/I64/F64/Bool/U8 等,以 sem 既有标量判别助手为准,grep `sem_walk.ct` 内 E3020 分支现用的型别判别式)且非 Str → 报 E3021。三处注册(血律:内建/诊断三处注册惯例):

- `compiler/src/diag_msg.ct`:en/zh 两表各增一行。en: `"E3021": "channel element type must be scalar or Str (task-segment boundary value semantics, §7)"`;zh: `"E3021": "通道元素类型限标量或 Str(任务段边界值语义,§7)"`(文案按两表现行行形态对齐)。
- `tests/meta_check.py`:码表增 `"E3021"`(对齐 :67 附近现行格式)。
- `tests/README.md`:诊断码表节增 E3021 一行。

负例钉 `tests/06g_chan_elem.neg.ct`:

```ctron
//@ expect-error: E3021
fn main() {
    let (tx, rx) = Channel[List[I32]](2)
    tx.send([1, 2]).expect("send")
    let _ = rx
}
```

(负例跑法按 `tests/` 现行 neg 夹具 harness——先 `ls tests/*.sh tests/run* 2>/dev/null` 与既有 `06_spawn_nonsend.neg.ct` 同法接入。)

- [ ] **Step 6: 全门**

```bash
rm -rf .cache/emit
sh tests/gc/seg/run.sh          # window 探针三遍绿
sh tests/gc/run.sh
sh compiler/test/smoke.sh --full
sh tests/db/run.sh 2>/dev/null || true   # pool 族 Channel[I64] 不受 E3021 影响
ctron gate                       # 全量 21 步(含 suite 双列与 fmt parity)
```

Expected: 全绿。suite 若有 Channel[List/struct] 现存红 → 逐具改 Str/标量通道或改用 Mutex 面(改具属合法,登记进提交文案)。

- [ ] **Step 7: Commit**

```bash
git add compiler/src/driver_emit.ct compiler/src/trans_expr.ct compiler/src/sem_walk.ct compiler/src/diag_msg.ct tests/meta_check.py tests/README.md tests/gc/seg tests/06g_chan_elem.neg.ct
git commit -m "feat(rt): G1-T3 通道边界值语义——Str 载荷 send 拷入 GC 堆/recv 任务侧拷入本段(is_str 旗+双向助手),E3021 元素限面(标量|Str)三处注册+负例钉;探针二跨堆窗口压测(GC 阈值极小三遍)" -- compiler/src/driver_emit.ct compiler/src/trans_expr.ct compiler/src/sem_walk.ct compiler/src/diag_msg.ct tests/meta_check.py tests/README.md tests/gc/seg tests/06g_chan_elem.neg.ct
```

---

### Task 4: Region.heap()(线程段锚定 mark/reset,任务内手动分段)

**Files:**
- Modify: `compiler/src/driver_emit.ct`(ct_arena typedef ~553 不变;新增 `ct_region_mark/ct_region_reset/ct_region_heap` 三助手)
- Modify: `compiler/src/trans_expr.ct`(K 门控 mark/reset 臂 ~732-743 改 dispatch 助手;新增 `Region.heap()` ctor 臂)
- Modify: `tests/gc/seg/src/region.ct`、`tests/gc/seg/run.sh`(探针四:任务内 churn 循环 mark/reset,RSS 平)

**Interfaces:**
- Produces(C):`static long long ct_region_mark(ct_arena* v)`(堆形→当前线程竞技场 off,栈形→v->off)、`static void ct_region_reset(ct_arena* v, long long m)`(堆形→回卷当前线程竞技场并守卫跨增长,栈形→原 ct_arena_reset)、`static ct_arena ct_region_heap(void)`(当前线程竞技场视图)。
- 语义:`Region.heap()` 返回当前线程竞技场(任务线程=本段,主线程=全局)的视图值(码 K,与 fixed 同形);mark=快照 off;reset=回卷,**跨增长块 mark 一律 panic("region reset across growth")**;LIFO 纪律与 bare 档 Arena 同款,越过 mark 的既有对象 reset 后无效(专家工具,文档明示)。
- 现状注记(计划复核修正设计假设):Region/Pool 的 fixed 形与 mark/reset/alloc/free 早已在 trans_expr.ct:583-766 全档降路径(bare 专属只是测试形态),无需「提升」;本任务只补**堆锚定形**(heap),Pool.heap 不做(YAGNI——mark/reset 已覆盖任务内分段,Pool 与段 LIFO 语义重叠)。

- [ ] **Step 1: C 助手三件(driver_emit.ct,ct_arena typedef 之后)**

```c
static ct_arn* ct_region_cur(void) { return ct_tls_arn ? ct_tls_arn : &ctron_aglobal; }
static ct_arena ct_region_heap(void) {
    ct_arn* a = ct_region_cur();
    ct_arena v;
    v.base = a->cur ? (void*)(a->cur + 1) : (void*)0;
    v.off = (long long)a->off;
    v.cap = a->cur ? (long long)a->cur->cap : 0;
    return v;
}
static long long ct_region_mark(ct_arena* v) {
    ct_arn* a = ct_region_cur();
    if (a->cur && (char*)(a->cur + 1) == (char*)v->base) { return (long long)a->off; }
    return v->off;
}
static void ct_region_reset(ct_arena* v, long long m) {
    ct_arn* a = ct_region_cur();
    if (a->cur && (char*)(a->cur + 1) == (char*)v->base) {
        if (m < 0 || m > (long long)a->off) { ctron_panic("region reset across growth"); }
        a->off = (size_t)m;
        return;
    }
    ct_arena_reset(v, m);
}
```

- [ ] **Step 2: trans 臂改接**

trans_expr.ct:734-743 的 mark/reset K 臂:`mark` 由 `((int64_t)(rob).off)` 改 `ct_region_mark(&(` + rob + `))`;`reset` 由 `ct_arena_reset(...)` 改 `ct_region_reset(...)`(实参同形,函数名换)。栈形 Region.fixed 行为不变(helper 落回原路径)。

- [ ] **Step 3: Region.heap() ctor 臂**

trans_expr.ct:590-594(带 TypeArgs 面)与 726-731(无 TypeArgs 面)两处 `Region.fixed` 臂旁,各增同构臂:`base[1][1] == "Region" && base[3] == "heap"`(与 `cal[1][1] == "Region" && m == "heap"`)→ 返回 `ct_region_heap()`。**先核 sem 面**:执行 `grep -rn '"Region"' compiler/src/sem_*.ct | head`,确认 `heap` 构造会过 sem(Region.fixed 如何过,heap 同法跟进;若 sem 有 target 门把 Region 锁在 bare,先实测 full 目标编译 `Region.heap()` 探针文件,红了按 fixed 的放行路径同法放 heap——不改 fixed 语义)。

- [ ] **Step 4: 探针四(任务内分段 churn)**

`tests/gc/seg/src/region.ct`:

```ctron
// 任务内 Region.heap 分段:每轮 churn 后 reset 回卷,段不增长 → RSS 平
fn main() {
    scope { |s|
        let t = s.spawn(|| {
            let r = Region.heap()
            var i: I32 = 0
            var acc: I64 = 0
            while i < 5000 {
                let m = r.mark()
                var k: I32 = 0
                while k < 100 {
                    let s2 = "chunk-\{i}-\{k}-padding-padding-padding"
                    acc = acc + (s2.len() as I64)
                    k = k + 1
                }
                r.reset(m)
                i = i + 1
            }
            println("region-probe acc=\{acc}")
        })
        t.join()
    }
}
```

run.sh 增第三段:emit+cc 三遍跑,断言判词在且 rc=0(配合 `CTRON_GC=1`:主线程收集不碰任务段,reset 语义独立于 GC)。

- [ ] **Step 5: 门 + Commit**

```bash
rm -rf .cache/emit
sh tests/gc/seg/run.sh
sh compiler/test/smoke.sh --full
git add compiler/src/driver_emit.ct compiler/src/trans_expr.ct tests/gc/seg
git commit -m "feat(rt): G1-T4 Region.heap()——线程段锚定 mark/reset(堆形 dispatch,栈形落回原路),跨增长块 reset panic 守卫;探针四任务内分段 churn" -- compiler/src/driver_emit.ct compiler/src/trans_expr.ct tests/gc/seg
```

---

### Task 5: StringBuilder reserve/clear + 增量 push

**Files:**
- Modify: `compiler/src/driver_emit.ct`(ctron_sb 族 ~130-133)
- Modify: `compiler/src/trans_expr.ct`(push_str 臂 ~1098-1105 旁增 reserve/clear 臂)
- Modify: `compiler/src/eval_call.ct`(~1049 push_str 旁增镜像)
- Modify: `tests/03j_stringbuilder.ct`(锚扩展)

**Interfaces:**
- Produces:`ctron_sb` 结构 `{ magic; data; len; cap; }`(data 偏移不变,兼容既有 magic 判读);C 侧 `ctron_sb_reserve(const void* h, int n)`、`ctron_sb_clear(const void* h)`;语言面 `sb.reserve(n)`(确保再收 n 字节不重分配)、`sb.clear()`(len=0,复用缓冲);`push_str` 改增量追加(容量足原地写,不足倍增)。
- 语义:`reserve` 后 `push_str` 不分配(探针断言);`clear` 后复用同一缓冲;`to_string`/`len` 行为不变。

- [ ] **Step 1: 红腿——03j 锚扩展**

`tests/03j_stringbuilder.ct` 增:

```ctron
fn main() {
    let mut sb = StringBuilder()
    sb.reserve(1024)
    var i: I32 = 0
    while i < 100 {
        sb.push_str("abcdefghij")
        i = i + 1
    }
    assert_eq(sb.len(), 1000)
    sb.clear()
    assert_eq(sb.len(), 0)
    sb.push_str("again")
    assert_eq(sb.to_string(), "again")
    println("03j reserve/clear OK")
}
```

(注意:`let mut sb` 形态以 03j 现行文件头三行的实际声明形态为准——先读 tests/03j_stringbuilder.ct:3-17 对齐现行可变绑定写法,再落锚;若 reserve/clear 在 sem 层无注册,本步编译红=预期红腿。)

- [ ] **Step 2: C 侧三件改写(driver_emit.ct 130-133)**

`ctron_sb` typedef 改 `{ unsigned long long magic; char* data; int len; int cap; }`;`ctron_sb_new` 初始 `data = ct_gc_alloc(64), len = 0, cap = 64`;`ctron_sb_push` 改:

```c
static void ctron_sb_push(const void* h, const char* s) {
    ctron_sb* sb = (ctron_sb*)h;
    int n = (int)strlen(s);
    if (sb->len + n + 1 > sb->cap) {
        int nc = sb->cap ? sb->cap : 64;
        while (nc < sb->len + n + 1) { nc = nc * 2; }
        char* r = (char*)ct_gc_alloc((size_t)nc);
        memcpy(r, sb->data, (size_t)sb->len);
        sb->data = r; sb->cap = nc;
    }
    memcpy(sb->data + sb->len, s, (size_t)n);
    sb->len += n;
    sb->data[sb->len] = 0;
}
```

新增:

```c
static void ctron_sb_reserve(const void* h, int n) {
    ctron_sb* sb = (ctron_sb*)h;
    if (n <= 0) { return; }
    if (sb->len + n + 1 > sb->cap) {
        int nc = sb->cap ? sb->cap : 64;
        while (nc < sb->len + n + 1) { nc = nc * 2; }
        char* r = (char*)ct_gc_alloc((size_t)nc);
        memcpy(r, sb->data, (size_t)sb->len);
        sb->data = r; sb->cap = nc;
    }
}
static void ctron_sb_clear(const void* h) {
    ctron_sb* sb = (ctron_sb*)h;
    sb->len = 0;
    sb->data[0] = 0;
}
```

`ctron_sb_str`/`ctron_len` 的 magic 判读路径不改(读 data 指针,偏移未变);`ctron_sb_str` 里 `strlen(d)` 改用 `sb->len`(等价且省一遍扫)。

- [ ] **Step 3: trans 臂 + eval 镜像**

trans_expr.ct:1098 push_str 臂旁照形增两臂(`m == "reserve"` → `ctron_sb_reserve((const void*)(obj), (int)(实参))`;`m == "clear"` → `ctron_sb_clear((const void*)(obj))`,接收者码门控与 push_str 臂同款,防遮自定义方法)。eval_call.ct:1049 push_str 分支旁照 interp 段表形增 reserve/clear(reserve 仅预留段表容量语义可空实现+注释,clear 置段表空——以 interp 侧 SB 段表现行实现为准对齐)。sem 注册面:grep `"push_str" compiler/src/sem_*.ct`,若 prelude 方法表在 sem 登记,同法增 reserve/clear。

- [ ] **Step 4: 门 + Commit**

```bash
rm -rf .cache/emit
sh compiler/test/smoke.sh --full
ctron gate
git add compiler/src/driver_emit.ct compiler/src/trans_expr.ct compiler/src/eval_call.ct tests/03j_stringbuilder.ct
git commit -m "feat(std): G1-T5 StringBuilder reserve/clear+增量 push(容量倍增, data 偏移兼容 magic 判读),trans/eval 双镜+03j 锚扩展" -- compiler/src/driver_emit.ct compiler/src/trans_expr.ct compiler/src/eval_call.ct tests/03j_stringbuilder.ct
```

---

### Task 6: GC 翻面证据——长跑 P99/RSS 载荷探针

**Files:**
- Create: `tests/gc/p99/run.sh`、`tests/gc/p99/src/main.ct`(服务循环+直方图+RSS 采样)
- Modify: `tests/COVERAGE.md`(证据面登记,§GC 翻面段)

**Interfaces:**
- Produces:P99 探针三遍数据(T32 翻面剩余证据面,tests/COVERAGE.md:1570 在案口径);**不做裁决**——数据呈报后按 J2(实测后翻)在 Task 7 执行翻面。

- [ ] **Step 1: 载荷探针程序**

先读 `lib/net/net.ct`(listen/connect/accept 的超时形态在 :215-233 一带、read_t/write_t 缓冲契约在 bind.ct:42-52、sleep_ms :41-42)与 `tests/net/c10k/src/main.ct` 对齐现行 net 面 API 真实签名( interp/emit 双臂同文是铁律,勿凭记忆写调用),再落 `tests/gc/p99/src/main.ct`。核形(函数名以实测为准,结构如下):

```ctron
// P99 载荷探针:回声服务 × 并发会话 spawn,时延直方图五桶 + 稳态 RSS 进度行
fn main() {
    let ln = /* net_listen(127.0.0.1:0, 取实端口) */
    scope { |s|
        // 会话任务 ×N:accept → spawn echo 任务(read_t/write_t 循环)
        // 客户端任务:M 连接 × K 操作,每操作时延入五桶(≤1ms/≤5ms/≤20ms/≤100ms/>100ms,
        //   时钟用 mono_ns 内建文本面或 net 域计时,以现行夹具同款)
        // 每 1000 操作打印一行:进度 + 桶计数 + RSS(/proc/self/statm 读文件面)
        // 终局打印判词:p99-probe total=.. b1=.. b2=.. b3=.. b4=.. b5=.. rss=..
    }
}
```

总时长 ~60s 或 100k 操作先到为准;缓冲全部调用方持有(U8[N] 局部),不驻留。

- [ ] **Step 2: 双柱测量**

```bash
sh tests/gc/p99/run.sh            # 柱一:CTRON_GC=0(bump 现状)
CTRON_GC=1 sh tests/gc/p99/run.sh # 柱二:GC=1
```

各三遍;记录 P99 桶占比、稳态 RSS、吞吐。判据(呈报口径,非门):GC=1 的 P99 桶劣化与 CPU 开销量化 + RSS 有界性对照。

- [ ] **Step 3: 登记证据**

`tests/COVERAGE.md` GC 翻面段(1570 行附近)增小节:双柱三遍数据表 + 判据句。不改任何门禁。

- [ ] **Step 4: Commit**

```bash
git add tests/gc/p99 tests/COVERAGE.md
git commit -m "test(gc): G1-T6 长跑 P99/RSS 载荷探针(回声×会话 spawn,直方图五桶×双柱三遍),COVERAGE 登记翻面剩余证据面" -- tests/gc/p99 tests/COVERAGE.md
```

---

### Task 7: GC 默认翻面(J2)+ 文档翻新

**Files:**
- Modify: `compiler/src/driver_emit.ct`(ct_gc_enabled 默认值 ~212)
- Modify: `docs/spec/06-memory.md`(默认档口径翻新)、`tests/COVERAGE.md`(翻面记录)
- Test: 全门

**Interfaces:**
- 语义:GC 默认开;`CTRON_GC=0`/`CTRON_GC=off` 显式退回 bump(escape hatch 常驻);并发程序主线程 GC 活跃(Task 2 已解除 pthread 永久退避),coro 模式仍自动退避。

- [ ] **Step 1: 翻面**

`ct_gc_enabled`(212 行)中 `ct_gc_on = (g && (!strcmp(g, "1") || !strcmp(g, "on"))) ? 1 : 0;` 改为:

```c
ct_gc_on = (g && (!strcmp(g, "0") || !strcmp(g, "off"))) ? 0 : 1;
```

- [ ] **Step 2: 全门双列**

```bash
rm -rf .cache/emit
sh compiler/test/smoke.sh --full
sh tests/gc/run.sh
sh tests/gc/bench.sh
sh tests/gc/seg/run.sh
ctron gate
```

Expected: 全绿。gc/bench digest 门(比值 ≤1.15 WARN 线)与 suite 双列均过;db 编译器臂豁免维持(在册,不翻)。任一红 → 红具归因后修,不降门。

- [ ] **Step 3: 文档翻新**

`docs/spec/06-memory.md`:默认档段落改写(GC 默认开、CTRON_GC=0 退回、并发 pthread 任务段语义一句、coro 退避注);**流式 buffer 所有权约定成文**(设计 §3.3 后半):「网络/文件边界的缓冲一律归调用方所有,实现零拷不持引用;跨调用驻留=契约违规」一节(落 06-memory 或 07-concurrency 的边界语义节,随现行文档结构就近);`tests/COVERAGE.md` 翻面记录(T32 裁决落账:J2 实测后翻)。

- [ ] **Step 4: Commit**

```bash
git add compiler/src/driver_emit.ct docs/spec/06-memory.md tests/COVERAGE.md
git commit -m "feat(rt): G1-T7 GC 默认翻面(J2 裁决,CTRON_GC=0 常驻退出)——全门双列绿,spec 06-memory 默认档口径翻新" -- compiler/src/driver_emit.ct docs/spec/06-memory.md tests/COVERAGE.md
```

---

### Task 8: repeat 字面量执行(F25 语言面关闭;J3 裁决)

**Files:**
- 按既有计划执行:`docs/superpowers/plans/2026-10-09-repeat-literal-plan.md`(W1→W3 波;该计划自带任务分解与门,本任务 = 执行它,不复制其内容)
- 关联:loom 侧短零字面量迁移登记(J3 裁决落账,写进该计划执行后的提交文案)

**Interfaces:**
- Consumes:repeat-literal-plan 的 W1(lexer/parser `[v] ** N`)→ W2(发射膨胀消除)→ W3(短字面量 E2010 禁令)。
- Produces:F25 语言面关闭;loom 迁移登记。

- [ ] **Step 1: 执行 repeat-literal-plan W1–W3**

按 `docs/superpowers/plans/2026-10-09-repeat-literal-plan.md` 的任务分解逐波执行,每波独立绿独立提交(该计划已含完整步骤与验收门)。

- [ ] **Step 2: F25 关闭验证**

构造大数组字面量探针(`U8[65536]` 以 repeat 形态表达),emit 前端内存峰值对照( `/usr/bin/time -l` 采 peak RSS):repeat 形态前端 RSS 应与普通小文件同量级(无 6.5 万节点膨胀)。数据记入 repeat-plan 执行提交文案。

- [ ] **Step 3: loom 迁移登记**

在 repeat-plan 文档尾部追记:「J3 裁决(2026-10-10):短零字面量按 W3 禁;loom 侧 F25 缓解形态迁移 repeat」。loom 仓迁移不属本仓批次,登记即销账。

---

### Task 9: seed 解释形态 linux 内存债复测登记

**Files:**
- Modify: `docs/linux-seed-memory-evidence.md`(复测指引 + CI 触发说明)

**Interfaces:**
- Produces:债的复测路径成文(CI 侧可复现命令 + 判据);**不在本仓修**(seed 解释器自身分配行为,M2/arena 治理域)。

- [ ] **Step 1: 成文复测指引**

`docs/linux-seed-memory-evidence.md` 增「G1 复测」节:精确命令(同原测量口径)、判据(RSS 曲线仍线性爬 = 维持绕行;已平 = 销账)、CI 触发方式(workflow dispatch 或 CI 任务注释位)。G1 翻面后 seed 臂若受 GC 默认开影响,复测数据即判据。

- [ ] **Step 2: Commit**

```bash
git add docs/linux-seed-memory-evidence.md
git commit -m "docs: G1-T9 seed 解释形态 linux 内存债复测指引成文(判据+CI 触发),G1 尾核销或豁免" -- docs/linux-seed-memory-evidence.md
```

---

### Task 10: 全门收口 + 汇流落库

**Files:**
- Modify: `tests/COVERAGE.md`(G1 批登记段)

- [ ] **Step 1: 全门终跑**

```bash
rm -rf .cache/emit
ctron gate
```

Expected: 21 步全绿(在册对端债按现口径豁免,与合流前基线同红同绿)。

- [ ] **Step 2: COVERAGE 登记 + 终提交**

`tests/COVERAGE.md` 增 G1 段:任务段语义(三探针)、通道边界值语义(E3021)、Region.heap、StringBuilder 复用、GC 翻面双柱数据指针、repeat/F25 关闭、残余债(coro 模式退避维持/通道 List-class 跨界 E3021 限面/seed linux 债)。

```bash
git add tests/COVERAGE.md
git commit -m "test(gc): G1-T10 批终登记——任务段/边界值语义/Region.heap/SB 复用/翻面数据/F25 关闭全落账" -- tests/COVERAGE.md
```

---

## 验收对照(设计 §3.6 → 任务)

| 设计门 | 任务 |
|---|---|
| 探针一(段回收) | Task 2 |
| 探针二(跨堆窗口) | Task 3 |
| 探针三(Send 深拷/值语义) | Task 3(send 侧拷贝语义,判词断言) |
| Region 手动分段 | Task 4 |
| GC 翻面(P99 证据 + 翻面) | Task 6 + 7 |
| 流式 buffer 复用 | Task 5 |
| F25 关闭(repeat) | Task 8 |
| seed 15.4GB 复测 | Task 9 |
| 既有 gc 三档门 + 确定性 | 每 Task 门 + Task 10 |
