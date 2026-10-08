# GUI 无胶水装配 W1a+W1b 实施计划(属性 = 归一 + 桩裁剪)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> 规格 = docs/superpowers/specs/2026-10-09-gui-glueless-assembly-design.md §5.1(补 a)/§7(裁 1/裁 2 落点)/§9(W1a/W1b 行)。
> 惯例:夹具先行 → 双口径验证 → 阶梯全绿 → 落库(pathspec 限定)→ COVERAGE/decls 记账。

**Goal:** ①组件实例属性统一 `=` 形(冒号形降 legacy 双收);②合成事件表从"合并全树根"改为"实例化根可达闭包",桩堆消失,合约 E 码上线。

**Architecture:** W1a 改四个解析点的实例 prop 分隔符消费(编译器骨架面 gui_sk_node/检查面 gui_ck_elem/折叠面 gui_lower_element + 运行时 gt_node),值三形(串/裸词/{表达式})不变;W1b 在 gui_ds_collect_slots 的 gui_sk_build 调用前做 IR 行级可达闭包过滤(骨架平面数组无视图名表,行级过滤免根名映射),并补合约 E 码。

**Tech Stack:** Ctron 自举编译器(compiler/src,seed 解释 + cc 发射双口径)、域包 pkgs/gui(运行时解析)。

## Global Constraints

- 静态树四基石不可破(设计 §8);`d_click` 返回名必须交 actc 分发;smoke 禁带 CTRON_STDPATH;管道验收重定向文件(假 rc 三犯在册);改 compiler/src 后先跑任一 ctc.sh 再 native.sh;夹具槽位执行前实测再占(s93/s94 已核实空闲,run.sh 现尾 s92);跨包引用即请求;pathspec 限定提交,driver_emit/trans_conc/trans_expr 为对端在飞文件禁触。
- 冒号形 W1a **双收不删**(存量夹具数十处用冒号,W4 才迁移+翻转 E 码)——本波阶梯必须全绿。

---

### Task 1: s93_attreq 夹具先行(失败态)

**Files:**
- Create: `tests/gui/s93_attreq/app.ctml`
- Create: `tests/gui/s93_attreq/src/main.ct`
- Modify: `tests/gui/run.sh`(s92 后追加 `s93_attreq`)

**Interfaces:**
- Consumes: 既有 `use gui.{test, d_frame, d_expect_text}` 驱动面;s41_run_d 的 `run(ViewCall)`+`__gui_bind_X` 合成形态。
- Produces: W1a 全部任务的验收场(内嵌档编译三面 + 文件档运行时面)。

- [ ] **Step 1: 写夹具(app.ctml 组件 + `=` 实例,双口径 main)**

`tests/gui/s93_attreq/app.ctml`:

```ctml
// s93_attreq —— W1a:组件实例属性 = 形(冒号形 legacy 双收)。值三形:串/裸词/{表达式}
pub view Badge (label: Str, tone: Str) {
  <vbox class="bd">
    <label class="bt">{label}</label>
    <label class="b2">{tone}</label>
  </vbox>
}
view Root (m: Model) {
  <vbox class="root">
    <Badge label="OK93" tone={m.tone}/>
    <label class="done">DONE93</label>
  </vbox>
}
style root { direction: column gap: 8 padding: 16 }
style bd { direction: column gap: 4 bg: SURFACE padding: 8 }
style bt { fg: TEXT size: 16 h: 24 }
style b2 { fg: TEXT_MUTED size: 13 h: 20 }
style done { fg: ACCENT size: 14 h: 22 }
```

`tests/gui/s93_attreq/src/main.ct`(结构照 s41_run_d;`tone` 走 `{表达式}` 形证 desugar bind 路径):

```ct
// s93_attreq —— W1a 验收:= 形实例属性穿编译三面(骨架/检查/折叠)+运行时 gt_parse
use gui.{test, d_frame, d_expect_text}

struct Model {
    var tone: Str
}

fn make() -> Model {
    return Model { tone: "WARN93" }
}

fn bind_noop(buf: List[Str]) {
}

fn act_noop(name: Str, args: List[Str]) {
}

fn headless_suite() -> I32 {
    // 内嵌档:编译三面吃 = 形(骨架面收集/检查面放行/折叠面重建)
    var src: Str = ctron_embedded()
    var m = Box[Model](make())
    var rc: I32 = test(src, 320, 240,
        |buf| __gui_bind_Root(buf, m),
        |name, ar| __gui_act_Root(name, ar, m),
        |k| {
        },
        |t, actc, k2| {
            d_frame(t, |buf| __gui_bind_Root(buf, m))
            d_expect_text(t, "OK93")
            d_expect_text(t, "WARN93")
            d_expect_text(t, "DONE93")
            println("S93-EMBED-GREEN")
        })
    // 文件档:运行时 gt_parse 吃 = 形(read_file 形态,今日锚惯例)
    var src2: Str = read_file("app.ctml")
    var m2 = Box[Model](make())
    var rc2: I32 = test(src2, 320, 240,
        |buf| bind_noop(buf),
        |name, ar| act_noop(name, ar),
        |k| {
        },
        |t, actc, k2| {
            d_frame(t, |buf| bind_noop(buf))
            d_expect_text(t, "OK93")
            d_expect_text(t, "DONE93")
            println("S93-FILE-GREEN")
        })
    if rc != 0 {
        return rc
    }
    return rc2
}

fn main() -> I32 {
    if env_get("CTRON_GUI_HEADLESS") != "" {
        return headless_suite()
    }
    return run(Root(m: make()))
}
```

注:`__gui_bind_Root`/`__gui_act_Root` 为 desugar 合成名(s41 先例);`tone` 无 bind 通道应答时展开期 bxv_ask `prop:m.tone` 走装配通道——若 headless 面显示空,照 s79 bind_host 形包装应答(实现时实测)。run.sh 循环串 s92 后追加 ` s93_attreq`。

- [ ] **Step 2: 跑夹具验证失败**

Run: `sh tests/gui/s93_attreq/run.sh 2>&1 | tail -5`
Expected: FAIL——`label="OK93"` 的 `=` 在实例分支被当裸词/错位消费(label 缺失或 E8100)。

### Task 2: 运行时 gt_node 双分隔符

**Files:**
- Modify: `pkgs/gui/gui_parse.ct:519-542`(gt_node 实例循环)

**Interfaces:**
- Produces: 文件档 `=` 形实例属性解析(Task 1 文件口径绿的前提)。

- [ ] **Step 1: 改分隔符消费(glit(58) → 58/61 双收)**

现文(`pkgs/gui/gui_parse.ct:525-527`):

```ct
                var pn: Str = gword(src, cur)
                glit(src, cur, 58)
                gws(src, cur)
```

改为:

```ct
                var pn: Str = gword(src, cur)
                gws(src, cur)
                var sep: I32 = byte_at(src, cur.load())
                if sep == 58 {
                    cur.store(cur.load() + 1)
                } else {
                    if sep == 61 {
                        cur.store(cur.load() + 1)
                    } else {
                        panic("gt E8100: 实例 prop 期待 : 或 =")
                    }
                }
                gws(src, cur)
```

- [ ] **Step 2: 夹具文件口径转绿**

Run: `sh tests/gui/s93_attreq/run.sh 2>&1 | grep -c "S93-FILE-GREEN"`
Expected: 1(内嵌口径仍红,属 Task 3)。

### Task 3: 编译器三面双分隔符

**Files:**
- Modify: `compiler/src/gui_parse.ct:3256-3268`(gui_sk_node 骨架面)
- Modify: `compiler/src/gui_parse.ct:2175-2177`(gui_ck_elem 检查面)
- Modify: `compiler/src/gui_parse.ct:274-275`(gui_lower_element 折叠面)

**Interfaces:**
- Produces: 内嵌档 `=` 形全链(骨架收集 npost/检查面深查/内嵌重建);Task 1 内嵌口径绿的前提。

- [ ] **Step 1: 骨架面(gui_sk_node,`gui_sk_word(pn); gui_sk_word(sep)` 处)**

```ct
                gui_sk_word(tks, cur)
                var sep2: Str = tks[cur.load()]
                if sep2 != ":" {
                    if sep2 != "=" {
                        panic("gui E8100: 实例 prop 期待 : 或 =")
                    }
                }
                gui_sk_word(tks, cur)
```

- [ ] **Step 2: 检查面(gui_ck_elem,`pn2 = eat_tok; eat_tok;` 处)**

```ct
                var pn2: Str = eat_tok(tks, cur)
                var sep3: Str = tks[cur.load()]
                if sep3 != ":" {
                    if sep3 != "=" {
                        gdiags.push("E8100: 实例 prop 期待 : 或 =")
                        bad = -1
                        break
                    }
                }
                eat_tok(tks, cur)
```

- [ ] **Step 3: 折叠面(gui_lower_element,`eat_tok; eat_tok;` 处——dump 重建面,同形放行)**

```ct
                eat_tok(tks, cur)
                var sep4: Str = tks[cur.load()]
                if sep4 != ":" {
                    if sep4 != "=" {
                        panic("gui E8100: 实例 prop 期待 : 或 =")
                    }
                }
                eat_tok(tks, cur)
```

- [ ] **Step 4: 重建 seed/cc + 夹具双口径全绿**

Run: `sh tests/gui/run_ctc.sh >/dev/null 2>&1; sh tests/gui/s93_attreq/run.sh 2>&1 | grep -c "GREEN"`
Expected: 2(EMBED+FILE 双绿;若 emit 缓存掩蔽先 `rm -rf .cache/emit` 再跑——amalloc 批在册铁律)。

- [ ] **Step 5: 全门 + 提交**

Run: `sh tests/gui/run.sh 2>&1 | tail -3` 后 suite+smoke(阶梯 94 过/2 在册败不变)。
```bash
git add tests/gui/s93_attreq tests/gui/run.sh pkgs/gui/gui_parse.ct compiler/src/gui_parse.ct
git commit -m "feat(gui): W1a 属性 = 归一——实例 prop 分隔符四解析点双收(:/=),值三形不变;冒号形降 legacy(W4 迁移后翻转),s93_attreq 双口径夹具"
```

### Task 4: W1b 探针——桩需求面确证

**Files:** 无新增(探针用 gui_snippets 副本)。

- [ ] **Step 1: 删桩实验**

复制 `examples/gui_snippets` 到 /tmp,删一个未实例化组件桩(如 `fn dp0`),双跑(cc check + emit headless):
Expected 记录三选一:①check E8120(检查面也在全量注册)②链接未定义符号(仅合成面)③全绿(该桩本就多余)。结论决定 Task 6 的闭包过滤是否须同时罩检查面走查入口。

### Task 5: 可达闭包过滤(IR 行层)

**Files:**
- Modify: `compiler/src/gui_parse.ct` gui_ds_collect_slots 头部(~3896 gui_sk_build 调用前)

**Interfaces:**
- Consumes: `file` IR 行(d[0]="GuiBlock",d[1]=view/style,d[2]=名,d[4]=体源,gui_block 行构造;靶视图=行序末位 view,与既有"靶视图居末"约定同源)。
- Produces: 过滤后 `file` 喂 gui_sk_build——闭包外 view 行不进骨架,collect 的 rst 全根走查自然只收闭包内事件。

- [ ] **Step 1: gui_ds_collect_slots 内 gui_sk_build 调用前插闭包过滤**

新助手(gui_parse.ct,collect_slots 上方;token 级实例引用扫描,`<` 后大写首 = 实例):

```ct
// W1b:可达闭包过滤——从靶视图(末位 GuiBlock view 行)沿实例边闭包,
// 闭包外 view 行剔除(style 行保留:样式表与实例化无关)。
// 引用扫描 = 体 token 流里 '<' 后随大写首词;fixpoint 迭代至不动点。
fn gui_ds_reach_filter(file: List[Str], out: List[Str]) -> I32 {
    var names = List[Str]()
    var idx = List[I32]()
    var i: I32 = 1
    while i < file.len {
        var d = file[i]
        if d[0] == "GuiBlock" {
            if d[1] == "view" {
                names.push(d[2])
                idx.push(i)
            }
        }
        i += 1
    }
    if idx.len == 0 {
        var j: I32 = 0
        while j < file.len {
            out.push(file[j])
            j += 1
        }
        return 0
    }
    var inset = List[Str]()
    var wl = List[Str]()
    wl.push(names[names.len - 1])
    inset.push(names[names.len - 1])
    var w: I32 = 0
    while w < wl.len {
        var body: Str = ""
        var k: I32 = 0
        while k < idx.len {
            if names[k] == wl[w] {
                body = file[idx[k]][4]
            }
            k += 1
        }
        var tks = List[Str]()
        gui_split(body, tks)
        var q: I32 = 0
        while q + 1 < tks.len {
            if tks[q].len == 1 {
                if byte_at(tks[q], 0) == 60 {
                    var tg: Str = tks[q + 1]
                    if tg.len > 0 {
                        var fb: I32 = byte_at(tg, 0)
                        if fb >= 65 {
                            if fb <= 90 {
                                var seen: Bool = false
                                var s2: I32 = 0
                                while s2 < inset.len {
                                    if inset[s2] == tg {
                                        seen = true
                                    }
                                    s2 += 1
                                }
                                var known: Bool = false
                                var s3: I32 = 0
                                while s3 < names.len {
                                    if names[s3] == tg {
                                        known = true
                                    }
                                    s3 += 1
                                }
                                if known {
                                    if !seen {
                                        inset.push(tg)
                                        wl.push(tg)
                                    }
                                }
                            }
                        }
                    }
                }
            }
            q += 1
        }
        w += 1
    }
    out.push(file[0])
    var r: I32 = 1
    while r < file.len {
        var d2 = file[r]
        var keep: Bool = true
        if d2[0] == "GuiBlock" {
            if d2[1] == "view" {
                keep = false
                var s4: I32 = 0
                while s4 < inset.len {
                    if inset[s4] == d2[2] {
                        keep = true
                    }
                    s4 += 1
                }
            }
        }
        if keep {
            out.push(d2)
        }
        r += 1
    }
    return 0
}
```

collect_slots 中(3896 附近)改为:

```ct
    var ffile = List[Str]()
    gui_ds_reach_filter(file, ffile)
    gui_sk_build(ffile, ntag, nflag, ncls, npre, nbid, npost, nfc, ns, nes, nec, f_evn, f_evf, btns, sk, sv, skprops)
```

- [ ] **Step 2: s94_reach 夹具(库两件只实例化一件)**

`tests/gui/s94_reach`:app 内嵌两本地组件 view(VUsed 挂 `on:click={vused_hit()}` 形事件,VUnused 挂 `vunused_hit()` 事件),根只实例化 VUsed;宿主**只定义** `vused_hit`,不定义 `vunused_hit`。
Expected(实现后):check/emit/headless 全绿(VUnused 的事件臂未合成=桩免备);实现前红(=失败态先行)。

- [ ] **Step 3: 桩裁剪实证 gui_snippets**

/tmp 副本(Task 4)桩堆清空(约 20 个空壳 fn),全绿判据=check+emit+headless 三净。回写正式 examples/gui_snippets。

- [ ] **Step 4: 合约 E 码 + e8 负例**

闭包内组件事件漏处理器/错签 = E 码(`E8121: 事件处理器未声明`/`E8122: 事件处理器签名不符`,名随实现登记 spec §10);e8_corpus 两负例随件。诊断码若与在册冲突,取下一空位并在 COVERAGE 记。

### Task 6: W1b 全门 + 提交

- [ ] **Step 1: 全门**

Run: 阶梯(94 过/2 在册败)+ suite 双列 + smoke + e8 51/51;`d_frame 后勿用 d_click` 等夹具铁律照旧。

- [ ] **Step 2: 记账提交**

decls 锁实测申报;COVERAGE 本日记;spec §12 表两行(GUI spec §4/§13 的 `=` 与 ev 前置条款属 W2.5,W1b 只动 §6 收集语义一段——随本批改)。
```bash
git add tests/gui/s94_reach tests/gui/run.sh compiler/src/gui_parse.ct examples/gui_snippets tests/e8_corpus/<新负例> docs/
git commit -m "feat(gui): W1b 桩裁剪——合成事件表改实例化根可达闭包(IR 行级过滤),gui_snippets 桩堆清零实证;合约 E 码(漏处理器/错签)+e8 负例;s94_reach 夹具"
```

## Self-Review

- 覆盖:设计 §5.1(W1a 四站点全)、§7 桩根因修法(IR 行过滤=平面数组无名表的绕行,验证面=Task 4 探针定检查面是否需同罩)、§9 W1a/W1b 行、§10 门禁。W2.5 的 ev/默认值不在本计划(下一计划)。
- 无占位:Task 1/2/3/5 均含实码;Task 4 为探针(输出决定分支,已写明三选一判据)。
- 型一致:`gui_sk_build`/`gui_split`/`gui_bx_expr` 等签名照现文件;`__gui_bind_Root` 合成名照 s41。 risks=①`{表达式}` prop 在 headless 的装配通道应答(Task 1 注已预案 bind_host 形)②检查面若也全量注册则 Task 5 的过滤入口需复用于检查面走查(Task 4 判据①分支)。
