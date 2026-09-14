# P1-A 发射面 Drop/RAII(§6.4)实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans(本会话内联执行)或
> superpowers:subagent-driven-development 按任务执行。步骤用 checkbox(`- [ ]`)跟踪。

**Goal:** Ctron 自举发射器为 `impl Drop for T` 的绑定生成确定性析构——作用域退出按声明
逆序、return 路径保证执行,镜像 seed 解释器可观察语义(三方逐字:seed 解释 == seed 发射
产物 == native 发射产物)。

**Architecture:** 四个插桩点:(a) Drop 方法合成发射(`t_<T>__drop`,driver_emit 原型区 +
pass2 定义,走 ct_body 通道);(b) per-scope drop 层栈(以 env 键 `#dls` 序列化串线程化,
ct_block/ct_body/BlockExpr/match 臂四个 C 作用域边界 push/emit/pop);(c) Let 分支按
`has_drop_impl` 登记;(d) return 路径按层栈逆序补发。panic 路径 v0 不展开(longjmp/exit,
E2071 静态门同口径,挂账);while 体非提升 Drop 局部 v0 命中即 panic(响亮拒发)。

**Tech Stack:** Ctron(compiler/src 36 模块;改 trans_stmt.ct + driver_emit.ct +
trans_expr.ct)、compiler-c seed(解释 oracle + 首次引导)、smoke/suite/固定点门禁。

## Global Constraints

- 门禁底线:smoke --full 全绿(std 漂移项除外——并行 std 泳道既有红,与本切片无关)+
  suite 双侧一致 + 自举固定点逐字节(compiler/src 无 impl 块,Drop 合成不影响固定点产物)。
- 语义镜像 seed 解释器 oracle:`eval_run.ct:335-361`(env 前插式,[0,extra) 下标升序 =
  声明逆序;flow 不 gating,return/panic 前都跑;while 体 keep=true 泄漏到外层)。
- 命中即停纪律:不支持形态 `panic("...")` 响亮拒发,禁止静默错译(仓库既有发射器口径)。
- E2071 静态门保持:break/continue 越过 Drop 局部仍拒绝(sem_calls.ct:245-246),故
  Break/Continue 分支与循环出口无需 cleanup;按 spec 明文解除留后续纯增量片。
- 单独提交:每 Task 一提交,`feat(trans):`/`test(trans):` 前缀; decls 锁变化同步 smoke.sh。
- 改 trans 源后测 bin 路径前必跑 `bash native.sh`(HANDOFF 教训:陈旧二进制)。

## 背景事实(实现者必读)

- eval oracle:`run_block` 出口(keep=false)对新增段 `[0,extra)` 逐个取 `env[di][1]`,
  值标签 `"U"` 且 `find_impl_method(file, 名, "drop")` 命中 → 以 `self` 绑定同一值对象
  (免克隆)执行方法体。**class 实例也触发**(与 spec §6.4"类引用不触发"存在语言级分歧,
  三线现状一致按"U"统一;发射面 v0 仅 struct 值,类引用命中即 panic,分歧挂账)。
- AST:`Impl` 节点 = `[0]"Impl" [1]TPs [2]trait型 [3]for型 [4]items`;Method 节点
  `[1]`名 `[3]`参数 `[5]`体;drop 方法签名 `fn drop(var self)`,self 按普通 Param 落树。
- CORE 助手可直接调用(build.sh:19-29 拼接序 trans 在 sem/eval 后):
  `has_drop_impl(file, tnm) -> Bool`(sem_type.ct:926)、`ty_head(t)`、
  `ct_struct_name_of`(trans_ty.ct:952)、`ct_ctype(code)`(:228)。
- C 作用域矩阵(与 eval keep 语义对齐):

| 程序形态 | eval drop 时机 | C 侧落点 |
|---|---|---|
| fn 体 / test 体 | 体块出口 | ct_body 闭括号前(尾值先求值落临时) |
| 裸块 Block/BlockExpr(语句位) | 块出口 | ct_block 闭括号前 |
| 裸块(值位) | 块出口,值外泄 | BlockExpr 分支:临时变量在花括号外声明,尾值先落临时,drops 后用 |
| if/else 臂 | 臂块出口 | ct_block(自动) |
| for 体 | 每轮(keep=false) | ct_block(自动,每轮花括号) |
| while 体 | 泄漏到外层(keep=true) | **v0:非提升 Drop 局部 panic;提升名挂外层层** |
| match 臂 | 臂块出口 | ct_match_value 两处臂花括号闭前 |
| own 块 | 块出口 | v0:体内 Drop 局部 panic(体平铺无独立作用域) |
| static let | 永不 | 不登记 |
| panic 展开 | eval 会跑 | **v0 不做**:longjmp/exit 无 cleanup,挂账 P1-A2 |

- 上次回退教训(HANDOFF:212-217):①裸块值位曾是 `ct_expr:BlockExpr` panic 爆点 → 本片
  在 ct_expr 补分支;②合成代码一律走 eln 常规通道,禁止字符串直拼表达式位;③return
  cleanup 用层栈,不做全局任务栈。

---

### Task 1: fx_drop 夹具与解释面基线

**Files:**
- Create: `compiler/test/fx_drop.ct`
- Modify: 无(先不加 smoke,Task 6 注册)

**Interfaces:**
- Produces: fx_drop 的 seed 解释输出(黄金,后续 Task 的三方对照基线)

- [ ] **Step 1: 写夹具(println 序探针,避开 Atomic 发射面不确定性)**

```ctron
// fx_drop.ct —— 发射面 Drop/RAII(§6.4):作用域退出逆序 + return 路径 + 尾值先求值
struct Ticket {
    let id: I32
}

impl Drop for Ticket {
    fn drop(var self) {
        println("drop:" + self.id.to_string())
    }
}

fn early() -> I32 {
    let a: Ticket = Ticket { id: 1 }
    if true {
        let b: Ticket = Ticket { id: 2 }
        return 7
    }
    return 0
}

fn tailval() -> I32 {
    let c: Ticket = Ticket { id: 3 }
    // 尾值必须先于 drop 求值:输出顺序应为 drop:3 之后才见返回值使用
    return c.id * 10
}

fn main() -> I32 {
    let t1: Ticket = Ticket { id: 10 }
    {
        let t2: Ticket = Ticket { id: 20 }
        let t3: Ticket = Ticket { id: 30 }
        println("inner")
    }
    let r = early()
    println(r)
    let v = tailval()
    println(v)
    println("end")
    return 0
}
```

预期解释输出(声明逆序;`early` 的 return 先 drop 内层 b 再 drop 外层 a;`tailval`
先 drop c 再用返回值):

```
inner
drop:30
drop:20
drop:2
drop:1
7
drop:3
30
end
drop:10
```

- [ ] **Step 2: seed 解释面跑绿**

Run: `compiler-c/build/ctronc run compiler/test/fx_drop.ct; echo rc=$?`
Expected: 上述输出,rc=0。若语义不符,先修夹具认知(不得改 eval 侧 oracle)。

- [ ] **Step 3: 确认发射面当前为红**

Run: `compiler/ctc.sh emit compiler/test/fx_drop.ct /tmp/fx_drop.c`
Expected: panic(裸块/字段……形态之一),rc≠0 —— 记录爆点行,Task 2 起逐个转绿。

- [ ] **Step 4: 删除临时复现文件**

Run: `rm compiler/test/repro_struct_method.ct`(struct 方法分派已由 d614eaf 修复,
04g 语料覆盖,该临时文件不再需要)

- [ ] **Step 5: Commit**

```bash
git add compiler/test/fx_drop.ct
git commit -m "test(trans): fx_drop 夹具——Drop 逆序/return/尾值先求值(解释面黄金)"
```

### Task 2: drop 层栈 + Let 登记 + 裸块发射

**Files:**
- Modify: `compiler/src/trans_stmt.ct`(ct_block :156-169、ct_stmts 后、Let 分支 :402-408、
  ct_stmt 新增 Block/BlockExpr 语句位分支)
- Modify: `compiler/src/trans_expr.ct`(ct_expr 新增 BlockExpr 值位分支,消 :756 爆点)

**Interfaces:**
- Produces(层栈约定,后续 Task 消费):env 键 `#dls`,值 = 层串,层间 `;`,层内条目间
  `,`,条目 = `名@型码`。四个助手(放 trans_stmt.ct 顶部,ct_stmts 之前):

```ctron
// ===== Drop 层栈(§6.4 发射面):env "#dls" = 层;层内 "名@码,名@码";层间 ";" =====
fn dls_of(env: List[Str]) -> Str {
    var i: I32 = 0
    while i < env.len {
        if seq2(env[i], "#dls") {
            if i + 1 < env.len { return env[i + 1] }
            return ""
        }
        i += 1
    }
    return ""
}

fn dls_set(env: List[Str], v: Str) -> List[Str] {
    return env_bind(env, "#dls", v)
}

// 开新层:追加空层
fn dls_push(env: List[Str]) -> List[Str] {
    var cur = dls_of(env)
    return dls_set(env, cur + ";")
}

// 当前层登记一个 Drop 局部
fn dls_add(env: List[Str], nm: Str, ty: Str) -> List[Str] {
    var cur = dls_of(env)
    var cut = cur.len - 1
    var head = byte_slice(cur, 0, cut)
    var last = byte_slice(cur, cut, cur.len)
    var entry = nm + "@" + ty
    if last != "" { entry = last + "," + entry }
    return dls_set(env, head + entry)
}

// 逆序发射当前层 drop 调用并弹层;返回弹层后的 env
fn dls_pop_emit(env: List[Str], file: List[Str]) -> List[Str] {
    var cur = dls_of(env)
    var cut = cur.len - 1
    var head = byte_slice(cur, 0, cut)
    var last = byte_slice(cur, cut, cur.len)
    if last != "" {
        // 逆序:条目从尾向头(声明逆序)
        var pos = last.len
        while pos > 0 {
            var comma = -1
            var k: I32 = 0
            while k < pos {
                if byte_at(last, k) == 44 { comma = k }
                k += 1
            }
            var seg = last
            var nxt = ""
            if comma >= 0 {
                seg = byte_slice(last, comma + 1, pos)
                nxt = byte_slice(last, 0, comma)
            }
            // seg = 名@码
            var at = -1
            var k2: I32 = 0
            while k2 < seg.len {
                if byte_at(seg, k2) == 64 { at = k2 }
                k2 += 1
            }
            var nm = byte_slice(seg, 0, at)
            eln("t_" + byte_slice(seg, 0, at) + "__drop(t_" + nm + ");", env)
            pos = nxt.len
            last = nxt
            if comma < 0 { pos = 0 }
        }
    }
    return dls_set(env, head)
}
```

- [ ] **Step 1: 写四个助手(上文代码)于 trans_stmt.ct 顶部(ct_stmts 之前)**

- [ ] **Step 2: ct_block 接入层栈**

```ctron
fn ct_block(b: List[Str], env: List[Str], file: List[Str], hreg: List[Str]) -> List[Str] {
    eln("\{", env)
    var denv = dls_push(env)
    var env2 = ct_stmts(b, denv, file, "", hreg)
    // 尾槽:p_block 末元素为裸尾表达式(无 "None" 哨兵时),按语句发射
    var tl = b[b.len - 1]
    if tl[0] != "None" {
        var es = List[Str]()
        es.push("Expr")
        es.push(tl)
        env2 = ct_stmt(es, env2, file, hreg)
    }
    env2 = dls_pop_emit(env2, file)
    eln("}", env)
    return env2
}
```

- [ ] **Step 3: Let 分支登记(普通分支 :407 eln 之后)**

```ctron
            eln(ct_ctype(ty) + " t_" + pat[1] + " = " + rhs + ";", env)
            if ty.len > 2 && byte_slice(ty, 0, 2) == "u:" {
                var dtn = byte_slice(ty, 2, ty.len)
                if has_drop_impl(file, dtn) {
                    env = dls_add(env, pat[1], ty)
                }
            }
            return env_bind(env, pat[1], ty)
```

(注:数组特殊分支 :388-401 的定长数组不登记——元素型 Drop 数组 v0 不支持,该分支已有
语料边界;若后续语料触达再按命中即停加 panic。)

- [ ] **Step 4: 语句位裸块分支(ct_stmt 内,Break 分支之前插入)**

```ctron
    if or2(t == "Block", t == "BlockExpr") {
        var bl = st[1]
        if t == "BlockExpr" { bl = st[1] } else { bl = st }
        // 语句位裸块:独立 C 作用域,ct_block 自管层栈(尾值按语句丢弃)
        var blk = bl
        if t == "Block" { blk = st }
        env = ct_block(blk, env, file, hreg)
        return env
    }
```

(实现时以解析器实测节点形态为准:`Block` 节点本身即块串;`BlockExpr[1]` 为块串——
先用 `ctc.sh emit` 一个裸块小程序打印确认,再定 `blk` 取法,不猜。)

- [ ] **Step 5: 值位 BlockExpr 分支(trans_expr.ct ct_expr 主分派,插入在兜底 panic 之前)**

```ctron
    if t == "BlockExpr" {
        // 裸块值位(R 线 trans.rs:1660 同口径):临时变量在花括号外声明,
        // 尾值先落临时,块内 drops 先于使用点执行
        var bl = e[1]
        var tcode = ct_typeof(bl[bl.len - 1], env, file)
        var tmp = "t_blk" + nline(e)
        eln(ct_ctype(tcode) + " " + tmp + ";", env)
        eln("\{", env)
        var denv = dls_push(env)
        var env2 = ct_stmts(bl, denv, file, "", List[Str]())
        var tl = bl[bl.len - 1]
        if tl[0] != "None" {
            eln(tmp + " = " + ct_expr(tl, env2, file) + ";", env)
        }
        dls_pop_emit(env2, file)
        eln("}", env)
        return tmp
    }
```

- [ ] **Step 6: 构建 + 自编译检查面绿**

Run: `cd compiler && bash build.sh && compiler-c/build/ctronc check build/cc_run.ct`
Expected: `check OK decls=N`(N 较基线增长,记录新值;Task 7 同步 smoke.sh 锁)。

- [ ] **Step 7: Commit**

```bash
git add compiler/src/trans_stmt.ct compiler/src/trans_expr.ct
git commit -m "feat(trans): Drop 层栈 + Let 登记 + 裸块(语句/值位)发射"
```

### Task 3: Drop 方法合成发射

**Files:**
- Modify: `compiler/src/driver_emit.ct`(原型区 :276-286 前、pass2 函数发射循环后)

**Interfaces:**
- Consumes: Task 2 的 ct_body 通道;CORE `ty_head`。
- Produces: 每个 `impl Drop for T`(非泛型)产出 `static void t_<T>__drop(t_<T> t_self)`。

- [ ] **Step 1: pass1 发现遍历 + 原型(在既有 fn 原型循环同层加一段)**

```ctron
    // Drop 方法原型(§6.4):impl Drop for T → t_<T>__drop(值接收者)
    var di0: I32 = 1
    while di0 < file.len - 1 {
        var d = file[di0]
        if d[0] == "Impl" {
            if seq2(ty_head(d[2]), "Drop") {
                var ftn = ty_head(d[3])
                if d[1].len > 1 && d[1][0] == "TPs" {
                    panic("emit:泛型 impl Drop 未支持:" + ftn)
                }
                var mi: I32 = 1
                while mi < d[4].len {
                    var it = d[4][mi]
                    if it[0] == "Method" && seq2(it[1], "drop") {
                        println("static void t_" + ftn + "__drop(t_" + ftn + " t_self);")
                    }
                    mi += 1
                }
            }
        }
        di0 += 1
    }
```

(eln vs println:原型区既有代码用 println 直出——照抄该区惯例;若该区在 pass1 门控内,
用同门控形态。以 :276-286 现场代码为准。)

- [ ] **Step 2: pass2 定义(pass2 函数发射循环之后)**

```ctron
    // Drop 方法定义:走 ct_body 通道,self 按值接收者入 env(u: 码)
    var di1: I32 = 1
    while di1 < file.len - 1 {
        var d = file[di1]
        if d[0] == "Impl" && seq2(ty_head(d[2]), "Drop") {
            var ftn = ty_head(d[3])
            var mi: I32 = 1
            while mi < d[4].len {
                var it = d[4][mi]
                if it[0] == "Method" && seq2(it[1], "drop") {
                    println("static void t_" + ftn + "__drop(t_" + ftn + " t_self) ")
                    var menv = env_bind(env, "self", "u:" + ftn)
                    ct_body(it[5], menv, file, "v")
                }
                mi += 1
            }
        }
        di1 += 1
    }
```

(确切变量名 `env`/`file` 以 driver_emit 函数现场为准;定义区须在原型区之后、
main/test 之前。)

- [ ] **Step 3: 构建并做冒烟**

Run: `cd compiler && bash build.sh && ctc.sh emit test/fx_own.ct /tmp/t2.c && gcc -o /tmp/t2 /tmp/t2.c && /tmp/t2`
Expected: 与 seed 解释 fx_own 输出逐字一致(own 块无 Drop 局部,纯回归)。

- [ ] **Step 4: Commit**

```bash
git add compiler/src/driver_emit.ct
git commit -m "feat(trans): Drop 方法合成发射(t_<T>__drop 原型+定义)"
```

### Task 4: ct_body 尾值/return 路径 drops

**Files:**
- Modify: `compiler/src/trans_stmt.ct`(ct_body 尾槽分派 :259-285、Return 分支 :846-876)

**Interfaces:**
- Consumes: Task 2 层栈助手。

- [ ] **Step 1: ct_body 尾槽改造(pass2 侧,`eln("}", env)` 之前)**

尾值形态三分:值尾(else 分支 `return expr;`)→ 临时落值 + drops + return;
Match 尾(`return t_mr;`)→ drops + return;If 尾(三元 return)→ 同值尾。

```ctron
        // 语句类尾槽按语句发射(原有 :262 分支保持)
        if or2(rc == "v", !ct_is_value(tl[0])) {
            // ……原有语句发射……
            env2 = dls_pop_emit(env2, file)
        } else if tl[0] == "Match" {
            ct_match_value(tl, env2, file, rc, hreg)   // 原有,产出 t_mr
            env2 = dls_pop_emit(env2, file)
            eln("return t_mr;", env)
        } else if tl[0] == "If" {
            var rvn = "t_rv" + nline(tl)
            eln(ct_ctype(rc) + " " + rvn + " = " + ct_if_value(tl, env2, file) + ";", env)
            env2 = dls_pop_emit(env2, file)
            eln("return " + rvn + ";", env)
        } else if tl[0] == "Own" {
            /* 体平铺(原有) */
            env2 = dls_pop_emit(env2, file)
        } else {
            var rvn2 = "t_rv" + nline(tl)
            eln(ct_ctype(rc) + " " + rvn2 + " = " + ct_expr(tl, env2, file) + ";", env)
            env2 = dls_pop_emit(env2, file)
            eln("return " + rvn2 + ";", env)
        }
```

(以现场代码为底稿做最小改写;pass1 侧同构分派**不加**层栈——eln 门控丢弃输出,
但 dls_pop_emit 有副作用输出,pass1 必须跳过 pop 或以 `ct_pass(env)` 门控。)

- [ ] **Step 2: Return 分支补发全开层逆序(eval: return 穿透各层都跑 drops)**

Return 分支发射 `return ...;` 之前插入:

```ctron
        // return 穿透所有开层:逆序补发各层 drops(内层→外层;当前层之后外层)
        env = dls_return_emit(env, file)
```

新助手(dls_pop_emit 的"只读不发当前层"变体:从最内层到最外层依序弹栈发射):

```ctron
// return 路径:弹空全部层并按 内层→外层 逆序发射(每层内部仍声明逆序)
fn dls_return_emit(env: List[Str], file: List[Str]) -> List[Str] {
    var cur = dls_of(env)
    while cur.len > 1 {
        env = dls_pop_emit(env, file)
        cur = dls_of(env)
    }
    return env
}
```

(层间序:层栈串 `L0;L1;L2` 中 L2 最内;pop 依次发 L2、L1、L0 = 内→外,与 eval
嵌套块出口顺序一致。)

- [ ] **Step 3: 构建 + Task 1 夹具发射面跑通**

Run: `cd compiler && bash build.sh && ctc.sh emit test/fx_drop.ct /tmp/fx_drop.c && gcc -o /tmp/fxd /tmp/fx_drop.c && /tmp/fxd > /tmp/fxd.out; echo rc=$?; diff /tmp/fxd.out <(compiler-c/build/ctronc run test/fx_drop.ct)`
Expected: diff 空(Task 1 的黄全对上;裸块/return/尾值三路径全绿)。若有形态命中
panic,按爆点回到 Task 2/3 补形态,禁止跳过。

- [ ] **Step 4: Commit**

```bash
git add compiler/src/trans_stmt.ct
git commit -m "feat(trans): ct_body 尾值先求值 + return 路径全层逆序 drops"
```

### Task 5: 边界形态(while/own/match 臂/泛型/类引用)响亮拒发 + 提升名外层登记

**Files:**
- Modify: `compiler/src/trans_stmt.ct`(While 分支 :479-532、Own 分支、ct_match_value :104-152)

- [ ] **Step 1: While 提升名 Drop 登记(提升 eln 处 :509-517)**

```ctron
            if prev == "" {
                eln(ct_ctype(ty) + " t_" + nm + " = " + ct_zero(ty) + ";", env)
                hreg.push(nm + ":" + ty)
                hl.push(nm + ":" + ty)
                envw = env_bind(envw, nm, ty)
                if ty.len > 2 && byte_slice(ty, 0, 2) == "u:" && has_drop_impl(file, byte_slice(ty, 2, ty.len)) {
                    // while 体绑定泄漏到外层(eval keep=true):drop 挂外层
                    envw = dls_add(envw, nm, ty)
                }
            }
```

- [ ] **Step 2: While 体非提升 Drop 局部响亮拒发(While 分支开头,hoist 计算后)**

```ctron
        var wi: I32 = 1
        while wi < body.len - 1 {
            var wst = body[wi]
            if wst[0] == "Let" && wst[2][0] == "PatId" {
                var wty = ""
                if wst[3][0] != "None" { wty = ct_ty_code(wst[3], file) }
                if wty.len > 2 && byte_slice(wty, 0, 2) == "u:" {
                    var wnm = wst[2][1]
                    var hoisted = false
                    var hk: I32 = 0
                    while hk < hl.len {
                        if byte_slice(hl[hk], 0, byte_at2c(hl[hk])) == wnm { hoisted = true }
                        hk += 1
                    }
                    if !hoisted && has_drop_impl(file, byte_slice(wty, 2, wty.len)) {
                        panic("发射面:while 体非提升 Drop 局部未支持(移入内层裸块):" + wnm)
                    }
                }
            }
            wi += 1
        }
```

(注解型 let 才能判型;无注解 Drop let 在 while 体 → 同样落入 panic 路径的"判不出型
即保守拒发"——实现时若 ct_ty_code 对 None 注解返回 "",改为 panic 保守拒发并注明。)

- [ ] **Step 3: ct_match_value 两处臂花括号闭前接层栈**

臂内 `eln("if (t_mv != NULL) \{", env)` 后 `dls_push`,臂绑定(t_<b0>)有 Drop 型时
`dls_add`,臂 `eln("}", env)` 前 `dls_pop_emit`(两臂同构)。Option[Str] 模型载荷是串
指针,当前臂绑定无 Drop 型——本步是结构预留 + 未来用户枚举臂的正确性前置,加骨架不
改行为。

- [ ] **Step 4: Own 体与类引用拒发**

- Own 分支:体平铺前扫 `ct_hoists` 等价形态——任何 Drop 局部 → `panic("发射面:own 体内 Drop 局部未支持")`。
- Let 分支 Drop 登记v0 仅 `u:`;类引用(`B:` 码)带 Drop impl 的 let → `panic("发射面:类引用 Drop 未支持(§6.4 类引用不触发;eval 统一 U 分歧挂账)")`。
- drop 方法体内若再声明 Drop 局部 → 走 ct_block 自动支持,不拒。

- [ ] **Step 5: 构建全绿 + fx_drop 回归**

Run: `cd compiler && bash build.sh && ctc.sh emit test/fx_drop.ct /tmp/fx_drop.c && gcc -o /tmp/fxd /tmp/fx_drop.c && /tmp/fxd | diff - <(compiler-c/build/ctronc run test/fx_drop.ct)`
Expected: 空。

- [ ] **Step 6: Commit**

```bash
git add compiler/src/trans_stmt.ct
git commit -m "feat(trans): Drop 边界形态——while 提升外层登记/own·类引用·非提升响亮拒发/match 臂层栈"
```

### Task 6: 三方逐字验收 + smoke 注册

**Files:**
- Modify: `compiler/test/smoke.sh`(tc_fx/发射运行区加 fx_drop 行;decls 锁同步)
- Modify: `compiler/README.md`(批次记录追加一行)

- [ ] **Step 1: 三方逐字(seed 解释 == seed 发射产物 == native 发射产物)**

```bash
cd compiler && bash build.sh && bash native.sh
seed_out=$(../compiler-c/build/ctronc run test/fx_drop.ct)
./ctc.sh emit test/fx_drop.ct /tmp/fd1.c && gcc -o /tmp/fd1 /tmp/fd1.c
./bin/ctron-emit run test/fx_drop.ct > /tmp/fd2.c && gcc -o /tmp/fd2 /tmp/fd2.c
[ "$(/tmp/fd1)" = "$seed_out" ] && [ "$(/tmp/fd2)" = "$seed_out" ] && echo 三方一致
```
Expected: `三方一致`。

- [ ] **Step 2: smoke.sh 注册(发射运行区,仿 fx_conc 行形态)+ decls 锁更新**

smoke.sh 加行(位置仿 :96-101 发射运行型):

```bash
tc_emit_run fx_drop "fx_drop 三方逐字(Drop 逆序/return/尾值)"
```

(以 smoke.sh 现场助手名为准;若只有逐件形态,手写三方向照 fx_conc_* 件。)
decls 锁:`grep -n "decls=" test/smoke.sh` → 更新为 Task 2 Step 6 记录的新值。

- [ ] **Step 3: smoke --full 全绿(除 std 漂移项)**

Run: `cd compiler && bash test/smoke.sh --full 2>&1 | tail -3`
Expected: 除 `std 漂移` 一项(并行泳道既有红)外全 ok。

- [ ] **Step 4: Commit**

```bash
git add compiler/test/smoke.sh compiler/README.md
git commit -m "test(trans): fx_drop 入 smoke 门禁——发射面 Drop 三方逐字"
```

### Task 7: 全量门禁 + 文档同步

- [ ] **Step 1: suite 双侧**

Run: `cd compiler && python3 test/suite.py 2>&1 | tail -3`
Expected: 与基线同分(61 件可计全绿;新增 0 neg)。

- [ ] **Step 2: 自举固定点**

Run: `cd compiler && bash test/smoke.sh --full 2>&1 | grep 固定点`
Expected: `ok : 固定点:发射产物逐字节复现`(compiler/src 无 impl 块,产物不变)。

- [ ] **Step 3: roadmap/HANDOFF 同步**

- roadmap P1-A:状态改 ✅(注明 panic 展开 v0 挂账 → 新增切片 P1-A2 行:
  "panic 路径 Drop 展开(ct_task 持栈式登记,longjmp 前逆序执行)+ E2071 按 spec 解除评估");
  删除 2026-09-13 前置发现 ⚠️ 段(已被 d614eaf 修复,04g 语料覆盖)。
- HANDOFF(2026-09-10-selfhosted-compiler-HANDOFF.md)§5:Drop 发射条目改"已落地",
  登记挂账:while 非提升/own 体/类引用/泛型 impl Drop 命中即停;panic 展开 P1-A2;
  class-Drop 三线分歧语言级挂账。
- 新登记:P0-E 候选切片——自举解析器非法输入韧性(分段万能 139,seed 正确 E1001;
  复现:任一含 `;` 的 .ct 过 `bin/ctron-cc run`)。

- [ ] **Step 4: 提交**

```bash
git add docs/superpowers/plans/
git commit -m "docs(plan): P1-A 发射面 Drop 收官——roadmap/HANDOFF 同步 + P1-A2/P0-E 登记"
```

## Self-Review 记录

- 规范覆盖:§6.4 三句——作用域退出逆序(Task 2/4/5)、panic 展开保证执行(**v0 挂账
  P1-A2,计划内明示,不做静默声明**)、类引用不触发(发射面拒绝,分歧挂账);Arena
  整体释放依赖 Arena 类型化,现透明 no-op,不在本片。
- 占位符扫描:Task 2 Step 4 的 `blk` 取法、Task 3 的原型区门控形态、Task 5 Step 2 的
  无注解保守拒发,三处标注"以现场代码为准 + 先实测再定"——这是仓库纪律(不猜节点形态),
  非占位。
- 类型一致性:层栈条目统一 `名@码`,drop 调用统一 `t_<T>__drop(t_<名>)`;`t_rv/t_blk`
  临时名均带 nline 站点唯一化(仿批次二十 match 临时先例)。
