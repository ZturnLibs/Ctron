# use 别名导入 P1a · Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 三宿主落地 use 别名导入的语法、表示与改名合并(P1a:整模块合并保留,先通 S2/S3/S4 场景)。

**Architecture:** 解析层把 `Sym as Alias` 解析为 (orig, alias) 对(自举 `Syms` 交替对 / seed `cimport.alias` / Rust `ImportItem`);加载器可见性按 orig、合并期对请求符号做浅拷贝顶层槽改名;E5035 拦别名撞名。fmt 零改动。

**Tech Stack:** 自举 Ctron 编译器(compiler/src,seed 解释执行)+ C 宿主(compiler-c)+ Rust 参考(compiler-rust)。

**Spec:** `docs/superpowers/specs/2026-09-22-use-alias-design.md`(§2 语法 / §3 表示 / §4.3–4.4 本片范围;P1b=选择性合并+闭包另出计划)

## Global Constraints

- 三宿主行为一致:同一 `.ct` 输入,check 面诊断码一致(E1001/E2020.use.\*/E5030/E5035)。
- fmt 三宿主逐字节 parity(tests/fmt/parity.sh 必须 0 分歧)——本片 fmt.ct/fmt.rs/lexer.c **零改动**。
- decl 锁:smoke.sh:38 `decls=349`,本片新增 `p_kw`+`p_use_alias` 两个 decl → **349→351**。
- 提交 pathspec 限定:工作区有并行泳道 WIP(interp.rs/lex.ct scan5 hunk/gui.ct 等),`git add` 只列本片文件,提交前必查 `git diff --cached --stat`。
- 验证一律用重建后的产物:`sh compiler/build.sh && sh compiler/native.sh`(bin 存在不会自动重拼)、`make -C compiler-c`。
- 模块夹具走 `tests/modules/`(宿主只跑 pkg check 面;tests/ 顶层会踩宿主 test 口径的 use 预存缺口)。

---

### Task 1: 回归夹具与期望红基线

**Files:**
- Create: `tests/modules/use_alias_basic/Ctron.ctcl`
- Create: `tests/modules/use_alias_basic/src/util.ct`
- Create: `tests/modules/use_alias_basic/src/extra.ct`
- Create: `tests/modules/use_alias_basic/src/main.ct`
- Create: `tests/modules/use_alias_dup/Ctron.ctcl`
- Create: `tests/modules/use_alias_dup/src/util.ct`(与 basic 同文件拷贝)
- Create: `tests/modules/use_alias_dup/src/main.ct`
- Create: `compiler/test/fx_use_alias.ct`
- Modify: `compiler/test/smoke.sh`(ast 往返段,doc8 之后追加一行用例)

**Interfaces:**
- Produces: `tests/modules/use_alias_basic`(Task 3 的绿靶)、`use_alias_dup`(E5035 负例)、`fx_use_alias.ct`(Task 2 ast roundtrip / Task 6 fmt 金样素材)。

- [ ] **Step 1: 写 use_alias_basic 四件**

`Ctron.ctcl`:
```
pkg {
    manifest_version = 1
    name = "app"
    version = "0.1.0"
}
```

`src/util.ct`(与 tests/modules/use_ok/src/util.ct 相同):
```ctron
pub fn double(x: I32) -> I32 {
    return x * 2
}

pub(pkg) fn triple(x: I32) -> I32 {   // 包内可见(§2.3)
    return x * 3
}

fn private_helper(x: I32) -> I32 {    // 模块私有
    return x + 1
}

pub fn via_helper(x: I32) -> I32 {    // 私有仅限本模块文件内使用
    return private_helper(x) * 10
}
```

`src/extra.ct`:
```ctron
pub fn addpair(a: I32, b: I32) -> I32 {
    return a + b
}

pub fn widget_new() -> I32 {
    return 7
}
```

`src/main.ct`:
```ctron
use app.util.{
    double,
    triple as tri,
    via_helper,
}
use app.extra.{
    widget_new as panel_new,
    addpair,
}

test "group alias renames bindings" {
    assert_eq(double(21), 42)
    assert_eq(tri(14), 42)
    assert_eq(via_helper(4), 50)
}

test "alias and bare import from one module" {
    assert_eq(panel_new(), 7)
    assert_eq(addpair(2, 3), 5)
}
```

- [ ] **Step 2: 写 use_alias_dup 三件(E5035 负例)**

`Ctron.ctcl` 同 Step 1。`src/util.ct` 拷贝 basic 的。`src/main.ct`:
```ctron
//@ fail: E5035
fn helper() -> I32 {
    return 1
}

use app.util.{
    double as helper,
}
```

- [ ] **Step 3: 写 fx_use_alias.ct**

```ctron
// fx_use_alias.ct —— use 别名解析固定点(P1a:Syms 交替对;.ctast roundtrip 面)
use app.util.{double as dbl, triple}
use app.extra.widget_new as panel_new

fn main() -> I32 {
    return dbl(1) + triple(1) + panel_new()
}
```

- [ ] **Step 4: smoke.sh 加 ast 往返行**

在 `compiler/test/smoke.sh` doc8 段(第 615-618 行 `ctc.sh ast stdpkg/src/main.ct` 块)之后追加:
```sh
"$COMP/ctc.sh" ast "$COMP/test/fx_use_alias.ct" > "$T/doc9.out" 2>&1
if [ $? -eq 0 ] && grep -q "ast roundtrip OK" "$T/doc9.out"; then
    ok "ast 往返固定点(use 别名交替对 Syms)"
else
    bad "ast 往返异常(use 别名): $(head -c 120 "$T/doc9.out")"
fi
```

- [ ] **Step 5: 跑期望红基线**

```sh
cd /Users/zyj/Zturn/Ctron
compiler/bin/ctron-chk run tests/modules/use_alias_basic/src/main.ct; echo "rc=$?"
compiler/bin/ctron-chk run tests/modules/use_alias_dup/src/main.ct; echo "rc=$?"
```
Expected: 两条均 `E2020: use 导入未找到符号:as`(as 被当符号名)、rc=1。**这是本计划的 TDD 红基线**;`use_alias_dup` 在 Task 2 后变为"未拦截"红、Task 3 后转绿。

---

### Task 2: 自举解析器 —— Syms 交替对 + `as` 上下文关键字

**Files:**
- Modify: `compiler/src/parse_decl.ct:393`(p_use 整体重写)+ `:693`(调用点签名)
- Test: Task 1 夹具

**Interfaces:**
- Produces: `p_use(toks, cur, lns, pdiags)`(签名扩参);`p_kw(t: Str) -> Bool`;`p_use_alias(toks, cur, lns, pdiags) -> Str`。`Syms` 节点不变式:**交替对 `[orig1, alias1, …]`,alias 缺省 = orig,偶数长度**(Task 3 消费)。

- [ ] **Step 1: p_use 前插入两个 helper(p_use 定义之前)**

```ctron
// use 别名关键字面(镜像 fmt.ct fmt_kw;fmt.ct 仅入 cc_fmt 拼接,parse 侧自持一份)
fn p_kw(t: Str) -> Bool {
    var ks = List[Str]()
    ks.push("fn")
    ks.push("let")
    ks.push("var")
    ks.push("const")
    ks.push("static")
    ks.push("comptime")
    ks.push("if")
    ks.push("else")
    ks.push("match")
    ks.push("while")
    ks.push("for")
    ks.push("in")
    ks.push("break")
    ks.push("continue")
    ks.push("return")
    ks.push("struct")
    ks.push("class")
    ks.push("enum")
    ks.push("trait")
    ks.push("impl")
    ks.push("own")
    ks.push("scope")
    ks.push("test")
    ks.push("use")
    ks.push("pub")
    ks.push("extern")
    ks.push("prop")
    ks.push("true")
    ks.push("false")
    ks.push("void")
    ks.push("self")
    var k: I32 = 0
    while k < ks.len {
        if ks[k] == t { return true }
        k += 1
    }
    return false
}

// `as` 后别名槽:须非关键字 ident(spec §2),否则 E1001;推进并返回名(失败返 "")
fn p_use_alias(toks: List[Str], cur: Atomic[I32], lns: List[Str], pdiags: List[Str]) -> Str {
    var t = tok(toks, cur)
    if t == "#EOF" {
        pdiag(pdiags, lns, cur, "E1001", "use 别名须为标识符")
        return ""
    }
    if !is_al(byte_at(t, 0)) {
        pdiag(pdiags, lns, cur, "E1001", "use 别名须为标识符")
        return ""
    }
    if p_kw(t) {
        pdiag(pdiags, lns, cur, "E1001", "use 别名须为标识符")
        return ""
    }
    adv(cur)
    return t
}
```

- [ ] **Step 2: 重写 p_use(替换现 fn 全体)**

```ctron
fn p_use(toks: List[Str], cur: Atomic[I32], lns: List[Str], pdiags: List[Str]) -> List[Str] {
    adv(cur)
    var nu = mk("Use")
    var segs = mk("Segs")
    var syms = mk("Syms")
    var braced = false
    var ng_alias = ""
    var more = true
    while more {
        // use 语句内换行不敏感(f15554c)
        while tok(toks, cur) == "NL" {
            adv(cur)
        }
        var a = tok(toks, cur)
        if a == "." {
            adv(cur)
        } else if a == "\{" {
            adv(cur)
            braced = true
            var more2 = true
            while more2 {
                while tok(toks, cur) == "NL" {
                    adv(cur)
                }
                var b2 = tok(toks, cur)
                if or2(b2 == "}", b2 == "#EOF") {
                    more2 = false
                } else {
                    syms.push(b2)
                    adv(cur)
                    // 组项别名 `Sym as Alias`:as 为上下文关键字;别名槽缺省=本名
                    if tok(toks, cur) == "as" {
                        adv(cur)
                        var al = p_use_alias(toks, cur, lns, pdiags)
                        if al == "" { syms.push(b2) } else { syms.push(al) }
                    } else {
                        syms.push(b2)
                    }
                    if tok(toks, cur) == "," {
                        adv(cur)
                    }
                }
            }
            if tok(toks, cur) == "}" {
                adv(cur)
            }
            more = false
        } else if a == "#EOF" {
            more = false
        } else {
            segs.push(a)
            adv(cur)
            while tok(toks, cur) == "NL" {
                adv(cur)
            }
            var nx = tok(toks, cur)
            if nx == "as" {
                // 单符号别名 `use path.Sym as Alias`(mpath 逻辑不动,spec §4.7)
                adv(cur)
                ng_alias = p_use_alias(toks, cur, lns, pdiags)
                more = false
            } else if nx != "." {
                more = false
            }
        }
    }
    if !braced {
        // 无花括号:末段即符号;别名形态下仍为交替对
        var last = segs[segs.len - 1]
        syms.push(last)
        if ng_alias != "" {
            syms.push(ng_alias)
        } else {
            syms.push(last)
        }
    }
    nu.push(segs)
    nu.push(syms)
    return nu
}
```

- [ ] **Step 3: 改调用点签名(parse_decl.ct:693)**

```ctron
            nf.push(p_use(toks, cur, lns, pdiags))
```

- [ ] **Step 4: 重建 + 验证解析推进**

```sh
cd /Users/zyj/Zturn/Ctron && sh compiler/build.sh && sh compiler/native.sh
compiler/bin/ctron-chk run tests/modules/use_alias_basic/src/main.ct; echo "rc=$?"
compiler/bin/ctron-chk run tests/modules/use_alias_dup/src/main.ct; echo "rc=$?"
compiler/bin/ctron-chk run tests/modules/use_ok/src/main.ct; echo "rc=$?"
```
Expected: basic红变为 `E2020: 未解析的名称:tri`(别名尚未改名绑定,Task 3 消);dup红变为"未拦截"(rc=0,Task 3 补 E5035);use_ok 仍绿(`(sym,sym)` 对,行为不变)。负例语法面快查:
```sh
printf 'use std.str.{lines as fn}\n' > /tmp/alias_neg.ct && compiler/bin/ctron-chk run /tmp/alias_neg.ct
printf 'use std.str.{lines as\n}\n' > /tmp/alias_neg2.ct && compiler/bin/ctron-chk run /tmp/alias_neg2.ct
```
Expected: 两条均 E1001"use 别名须为标识符"。
注意:此时不要跑全量 smoke(decl 锁未 bump,Task 3 处理)。

---

### Task 3: 自举加载器 —— 对读、改名合并、E5035、诊断表、decl 锁

**Files:**
- Modify: `compiler/src/parse_pkg.ct`(pkg_load_use:k3 可见性循环步长 2、E5035 前检、j4 合并环改名)
- Modify: `compiler/src/diag_msg.ct`(:87 后 en、:172 后 zh 各加 E5035)
- Modify: `compiler/test/smoke.sh:38`(decls=349→351)
- Test: Task 1 夹具

**Interfaces:**
- Consumes: Task 2 的 Syms 交替对不变式。
- Produces: bootstrap 对 `Sym as Alias` 的改名绑定 + E5035 诊断。

- [ ] **Step 1: 可见性循环改对读(parse_pkg.ct k3 环)**

把(约 293-322 行)
```ctron
                            var k3: I32 = 0
                            while k3 < syms.len {
                                var sn = syms[k3]
```
起的环境改为步长 2(循环体内 `sn` 用法不变,末尾 `k3 += 1` 改 `k3 += 2`;补 alias 读取):
```ctron
                            var k3: I32 = 0
                            while k3 < syms.len {
                                var sn = syms[k3]
                                var al = syms[k3 + 1]
```
(环内 found/vis 检查保持按 `sn`;末尾 `k3 += 2`。)

- [ ] **Step 2: 可见性后的 `if diags.len > 0 return` 之前插 E5035 前检**

```ctron
                        // E5035:别名与本文件已有 decl 撞名(spec §4.4;P1a 先查入口面,
                        // 合并流内撞名由既有 E5030 兜底)
                        var k8: I32 = 0
                        while k8 < syms.len {
                            if syms[k8 + 1] != syms[k8] {
                                var m9: I32 = 1
                                while m9 < out.len {
                                    if out[m9].len > 1 && out[m9][1] == syms[k8 + 1] {
                                        diag1(diags, "E5035", "", syms[k8 + 1])
                                        return out
                                    }
                                    m9 += 1
                                }
                            }
                            k8 += 2
                        }
                        if diags.len > 0 {
                            return out
                        }
```

- [ ] **Step 3: j4 合并环加改名(替换"同名 dup 检查 + merged.push"块)**

现有块(dup 检查用 `mf[j4][1]`)改为:**先算改用名 rn,dup 检查与落名都用 rn**(完整替换 j4 环内从 `var dup = false` 到 `merged.push(mf[j4]);` 的整段):
```ctron
                                var rn = ""
                                var k6: I32 = 0
                                while k6 < syms.len {
                                    if mf[j4].len > 1 && syms[k6] == mf[j4][1] && syms[k6 + 1] != syms[k6] {
                                        rn = syms[k6 + 1]
                                    }
                                    k6 += 2
                                }
                                var iname = mf[j4][1]
                                if rn != "" { iname = rn }
                                var dup = false
                                var j5: I32 = 1
                                while j5 < merged.len {
                                    if or2(merged[j5][0] == "Fn", or2(merged[j5][0] == "FnPub", or2(merged[j5][0] == "Struct", or2(merged[j5][0] == "Enum", or2(merged[j5][0] == "FnExt", or2(merged[j5][0] == "Static", merged[j5][0] == "Const")))))) {
                                        if or2(mf[j4][0] == "Fn", or2(mf[j4][0] == "FnPub", or2(mf[j4][0] == "Struct", or2(mf[j4][0] == "Enum", or2(mf[j4][0] == "FnExt", or2(mf[j4][0] == "Static", mf[j4][0] == "Const")))))) {
                                            if mf[j4].len > 1 && merged[j5].len > 1 && merged[j5][1] == iname {
                                                dup = true
                                            }
                                        }
                                    }
                                    j5 += 1
                                }
                                if dup {
                                    // E5030:同名 decl 曾"首个胜出"静默遮蔽 → 现拦截(报名用改用名)
                                    diag1(diags, "E5030", "", iname)
                                    return out
                                }
                                if rn != "" {
                                    // 合并期重命名:浅拷贝顶层槽,子节点共享(spec §4.3)
                                    var nd = mk(mf[j4][0])
                                    nd.push(rn)
                                    var k7: I32 = 2
                                    while k7 < mf[j4].len {
                                        nd.push(mf[j4][k7])
                                        k7 += 1
                                    }
                                    merged.push(nd)
                                } else {
                                    merged.push(mf[j4])
                                }
```

- [ ] **Step 4: diag_msg.ct 补 E5035 双语**

en 段(:87 E5030 行后):
```ctron
    if seq2(key, "E5035") { return "use alias collision:%0 (alias conflicts with an existing name; pick another)" }
```
zh 段(:172 E5030 行后):
```ctron
    if seq2(key, "E5035") { return "use 别名撞名:%0(别名与既有名冲突;请改别名)" }
```

- [ ] **Step 5: decl 锁 bump(smoke.sh:38)**

```sh
grep -q 'check OK decls=351' "$T/chk.out" && ok "自检 cc_run 绿,decls=351" || bad "自检 cc_run: $(cat "$T/chk.out")"
```

- [ ] **Step 6: 重建 + 夹具转绿**

```sh
cd /Users/zyj/Zturn/Ctron && sh compiler/build.sh && sh compiler/native.sh
compiler/bin/ctron-chk run tests/modules/use_alias_basic/src/main.ct && echo BASIC_OK
compiler/bin/ctron-chk run tests/modules/use_alias_dup/src/main.ct 2>&1 | grep -q E5035 && echo DUP_E5035_OK
compiler/bin/ctron-chk run tests/modules/use_ok/src/main.ct && echo USE_OK_STILL_GREEN
```
Expected: 三条全过。ast 往返早验(交替对在 `"Use","NN"` 形态下 roundtrip):
```sh
compiler/ctc.sh ast compiler/test/fx_use_alias.ct | grep -q "ast roundtrip OK" && echo AST_OK
```
再跑 seed 解释口径同夹具(不经 C 宿主,seed 只是解释器):
```sh
CTRON_STDPATH=$PWD/std compiler-c/build/ctronc run tests/modules/use_alias_basic/src/main.ct 2>&1 | head -2
```
Expected: 若 seed 尚无别名解析(Task 4 前),报错属预期(此条仅观察,不判绿;Task 4 后重跑)。

---

### Task 4: C 宿主 —— cimport.alias + pkg E5035 + ast_show

**Files:**
- Modify: `compiler-c/src/ast.h:23-25`(cimport 加 alias 字段)
- Modify: `compiler-c/src/parser_decl.c`(组项/非组别名解析)
- Modify: `compiler-c/src/pkg.c`(check_use_alias_collisions + :995 调用)
- Modify: `compiler-c/src/ast_show.c:511-515`(别名显示)
- Test: Task 1 夹具(host pkg 面)

**Interfaces:**
- Consumes: cimport(svas: `char** segs; size_t nsegs;`);`dup_text(p, text)->char*`(parser_internal.h,与 :313 同款);TOK_IDENT 值不含关键字(词法器分型,`as fn` 天然 E1001 路径)。
- Produces: `cimport.alias`(NULL = 无别名);宿主 pkg 对 use_alias_dup 报 E5035。

- [ ] **Step 1: ast.h cimport 加字段**

```c
typedef struct {
    char** segs;   // 一个完整导入路径(组导入已拆为全路径)
    size_t nsegs;
    char* alias;   // 别名(NULL = 无别名;spec 2026-09-22 §3)
} cimport;
```
(仅示意增量;保留原注释。)

- [ ] **Step 2: parser_decl.c 组项别名**

组环内在 `free(seg.d);` 与 `imp->segs = sv_done(...);` 之间插:
```c
            // 组项别名:`Sym as Alias`(as 上下文关键字;peek2 须 IDENT,关键字另型天然排除)
            char* alias = NULL;
            if (at_k(p, TOK_IDENT) && strcmp(tokp_at(p, 0)->text, "as") == 0 && tok_at(p, 1) == TOK_IDENT) {
                bump_tok(p);
                alias = dup_text(p, tokp_at(p, 0)->text);
                bump_tok(p);
            }
```
`imp->segs = ...` 行后加 `imp->alias = alias;`。

- [ ] **Step 3: parser_decl.c 非组别名**

prefix 循环 break 后(:322 `}` 之后、`iv imports = {0};` 之前)插同款解析;else 分支(:352-356)在 `imp->segs = sv_done(&prefix, ...)` 后加 `imp->alias = alias;`。

- [ ] **Step 4: pkg.c 新检查 + 挂载**

`check_use_visibility`(744)之前加:
```c
// E5035:use 别名与本模块既有 decl 撞名(spec 2026-09-22 §4.4;check 面)
static void check_use_alias_collisions(pkg_res* r, const pkg* p, const mod* m) {
    (void)p;
    const cfile* f = m->pr.file;
    for (size_t j = 0; j < f->ndecls; j++) {
        const cdecl* d = &f->decls[j];
        if (d->kind != D_USE) continue;
        for (size_t k = 0; k < d->use.nimports; k++) {
            const cimport* imp = &d->use.imports[k];
            if (!imp->alias) continue;
            for (size_t a = 0; a < f->ndecls; a++) {
                const cdecl* dd = &f->decls[a];
                const char* nm = NULL;
                switch (dd->kind) {
                case D_FN: nm = dd->fn_.name; break;
                case D_STRUCT: nm = dd->strukt.name; break;
                case D_CLASS: nm = dd->klass.name; break;
                case D_ENUM: nm = dd->en.name; break;
                case D_TRAIT: nm = dd->trait.name; break;
                case D_STATIC: nm = dd->statik.name; break;
                default: break;
                }
                if (nm && strcmp(nm, imp->alias) == 0) {
                    push(r, m->rel, "E5035", "use 别名撞名:%s(与既有名冲突)", imp->alias);
                }
            }
        }
    }
}
```
:994 `check_use_visibility(&r, &p, &p.m[i]);` 之后加:
```c
        check_use_alias_collisions(&r, &p, &p.m[i]);
```

- [ ] **Step 5: ast_show.c 别名显示**

:512-515 每项打印内,strlist 输出 segs 之后追加:
```c
            if (imp->alias) fprintf(o, " as %s", imp->alias);
```

- [ ] **Step 6: 重建 + 双宿主绿**

```sh
cd /Users/zyj/Zturn/Ctron && make -C compiler-c 2>&1 | tail -1
compiler-c/build/ctronc pkg tests/modules/use_alias_basic && echo HOST_PKG_OK
compiler-c/build/ctronc pkg tests/modules/use_alias_dup 2>&1 | grep -q E5035 && echo HOST_DUP_OK
compiler-c/build/ctronc check tests/modules/use_alias_basic/src/main.ct && echo HOST_FILE_OK
compiler-c/build/ctronc test tests/modules/use_alias_dup/src/main.ct 2>&1 | grep -q E5035 && echo HOST_FILE_DUP_OK
```
Expected: 全过(host pkg / 单文件 check 两口径都拦 E5035;basic 双口径 0 诊断)。

---

### Task 5: Rust 参考宿主 —— ImportItem + sem 绑定

**Files:**
- Modify: `compiler-rust/src/ast.rs:12`
- Modify: `compiler-rust/src/parser.rs`(parse_use 组/非组 + :1466 单测重写 + 新增别名单测)
- Modify: `compiler-rust/src/sem.rs`(collect_use_targets :432-439、collect_imports :441-448、导入绑定环 :369-384、resolve_import :643 加 bind 参)
- Test: `cargo test --lib`(基线 48 全绿)

**Interfaces:**
- Produces: `ImportItem { segs: Vec<String>, alias: Option<String> }`(derive Clone, PartialEq, Debug,与 ast.rs 现有风格一致);`resolve_import(..., sym_name, bind, ...)`。

- [ ] **Step 1: ast.rs**

```rust
pub struct ImportItem { pub segs: Vec<String>, pub alias: Option<String> }   // 别名:Some= Sym as Alias
pub struct UseDecl { pub imports: Vec<ImportItem> }                          // 组导入已拆为全路径
```
(derive 行沿用 ast.rs 既有宏;若 UseDecl 现有 derive 含 Clone/Partial/Debug 则 ImportItem 对齐。)

- [ ] **Step 2: parser.rs parse_use**

组环 `imports.push(full);` 改:
```rust
                let is_as = matches!(self.peek(), Tok::Ident(s) if s == "as");
                let next_ident = matches!(self.peek2(), Tok::Ident(_));
                let mut alias = None;
                if is_as && next_ident {
                    self.bump();
                    if let Tok::Ident(a) = self.peek().clone() { alias = Some(a); }
                    self.bump();
                }
                imports.push(ImportItem { segs: full, alias });
```
非组:prefix 循环后、`let mut imports` 前插同款 `alias` 解析;else 分支改:
```rust
            imports.push(ImportItem { segs: std::mem::take(&mut prefix), alias });
```

- [ ] **Step 3: parser.rs 单测重写 + 新增**

`use_group_expands_to_full_paths`(:1466)的 want 改:
```rust
                let want = vec![
                    ImportItem { segs: vec!["std".into(), "net".into(), "TcpListener".into()], alias: None },
                    ImportItem { segs: vec!["std".into(), "net".into(), "Request".into()], alias: None },
                ];
```
紧随其后新增:
```rust
    #[test]
    fn use_alias_parses_item_and_solo_forms() {
        let (f, d) = file("use std.net.{TcpListener as TL, Request}
use app.extra.widget_new as panel_new");
        assert!(d.is_empty());
        match &f.decls[0] {
            Decl::Use(u) => {
                assert_eq!(u.imports[0].alias.as_deref(), Some("TL"));
                assert_eq!(u.imports[1].alias, None);
            }
            other => panic!("{:?}", other),
        }
        match &f.decls[1] {
            Decl::Use(u) => {
                assert_eq!(u.imports[0].segs.last().map(String::as_str), Some("widget_new"));
                assert_eq!(u.imports[0].alias.as_deref(), Some("panel_new"));
            }
            other => panic!("{:?}", other),
        }
    }
```

- [ ] **Step 4: sem.rs**

`collect_use_targets`(:432):
```rust
            for p in &u.imports {
                if p.segs.len() >= 2 { out.push(p.segs[..p.segs.len() - 1].join(".")); }
            }
```
`collect_imports`(:441)返回 `Vec<ImportItem>`:
```rust
            out.extend(u.imports.iter().cloned());
```
(签名 `-> Vec<Vec<String>>` 改 `-> Vec<ImportItem>`。)绑定环(:369-384):
```rust
        for imp in &imports {
            let Some((sym_name, target)) = imp.segs.split_last() else { continue };
            let target_syms = sema.mod_by_path.get(&target.join("."))
                .and_then(|&idx| mod_syms.get(idx))
                .cloned()
                .unwrap_or_default();
            let bind: &str = imp.alias.as_deref().unwrap_or(sym_name);
            let mut import_diags = Vec::new();
            resolve_import(&sema, &target_syms, target, sym_name, bind, &mut mod_syms[pf.file_idx], &mut import_diags);
            per_module[pf.file_idx].1.extend(import_diags);
        }
```
`resolve_import`(:643)签名加 `bind: &str`(sym_name 后);**本包模块分支**查 orig、插 bind:
```rust
                let found = target_syms.get(sym_name).cloned();
                match found {
                    Some(sym) => {
                        // ... 可见性判定不动 ...
                        syms.insert(bind.to_string(), sym);   // 原 insert(sym_name...) 改此
                    }
```
(std/stdweb 分支维持按原名绑定——P1b 落 E2020.use.nat 统一拦截;函数头注释补一行说明。)

- [ ] **Step 5: 构建与单测**

```sh
cd /Users/zyj/Zturn/Ctron/compiler-rust && cargo build --release 2>&1 | tail -1 && cargo test --lib 2>&1 | tail -3
```
Expected: 编译零错;`test result: ok`,通过数 ≥ 49(基线 48 + 新增 1)。

---

### Task 6: fmt 金样重冻结 + 全量门禁 + 提交

**Files:**
- Modify: `compiler/test/fx_fmt_golden.ct`(追加别名段)
- Modify: `compiler/test/fx_fmt_golden.expected`(Rust 参考实现重冻结)

**Interfaces:**
- Consumes: Task 1-5 全部就位。

- [ ] **Step 1: 金样输入追加(spec §5:fmt 零改写,空格表现产)**

`fx_fmt_golden.ct` 末尾追加:
```ctron

// use 别名(spec 2026-09-22 §2):组内混用/单符号;token 流直过,布局=原始间隙
use app.util.{
double,
triple as tri,
}
use app.extra.{widget_new as panel_new, addpair}
use app.extra.widget_new as panel_new
```

- [ ] **Step 2: 重冻结 + 三宿主逐字节验证**

```sh
cd /Users/zyj/Zturn/Ctron/compiler/test
compiler-rust/target/release/ctron fmt fx_fmt_golden.ct > fx_fmt_golden.expected
compiler-c/build/ctronc fmt fx_fmt_golden.ct | cmp - fx_fmt_golden.expected && echo SEED_EQ
compiler/bin/ctron-fmt run fx_fmt_golden.ct | cmp - fx_fmt_golden.expected && echo BOOT_EQ
```
Expected: SEED_EQ + BOOT_EQ。

- [ ] **Step 3: 全量门禁**

```sh
cd /Users/zyj/Zturn/Ctron
sh tests/fmt/parity.sh | tail -1          # 期望: N 绿 / M 词法脏一致报错 / 0 分歧
python3 compiler/test/suite.py 2>&1 | tail -8   # 期望: 自举 73/73 持平;modules +2(11→13)自举/宿主双绿
sh compiler/test/smoke.sh 2>&1 | tail -2  # 期望: 唯一红=std/gui.ct peer 漂移(预存)
```
注意:parity/suite 前确认三产物均已重建(Task 2-5 已做);smoke 的 std 漂移红属 gui 泳道 WIP,不算本片失败。

- [ ] **Step 4: 提交(pathspec 限定)**

```sh
cd /Users/zyj/Zturn/Ctron
git add compiler/src/parse_decl.ct compiler/src/parse_pkg.ct compiler/src/diag_msg.ct \
  compiler-c/src/ast.h compiler-c/src/parser_decl.c compiler-c/src/pkg.c compiler-c/src/ast_show.c \
  compiler-rust/src/ast.rs compiler-rust/src/parser.rs compiler-rust/src/sem.rs \
  compiler/test/fx_use_alias.ct compiler/test/fx_fmt_golden.ct compiler/test/fx_fmt_golden.expected \
  compiler/test/smoke.sh \
  tests/modules/use_alias_basic tests/modules/use_alias_dup
git diff --cached --stat   # 必查:只含上列文件;lex.ct/interp.rs/gui.ct 等peer WIP 不得混入
git commit -m "feat(compiler): use 别名导入 P1a——三宿主 Sym as Alias 解析/表示/改名合并

spec docs/superpowers/specs/2026-09-22-use-alias-design.md §2/§3/§4.3-4.4:组内混用与
单符号别名(as 上下文关键字,E1001 拒关键字/悬空);自举 Syms 交替对(ctast 形态 NN
不变,roundtrip 固定点入 smoke)、seed cimport.alias、Rust ImportItem(查 orig 绑
bind)。合并期浅拷贝顶层槽改名(整模块合并保留,P1b 再收紧);E5035 别名撞名三宿主
同码(diag_msg 双语);E5030/E2020.use.* 契约不动;decl 锁 349→351。夹具
use_alias_basic(boot run+宿主 pkg 双绿)/use_alias_dup(E5035 负例);fmt 零改动,
金样追加别名段重冻结三宿主逐字节一致。"
```

---

## 已知形态边界(实现者须知,spec §2/§9)

- `Sym as` 行尾换行 + 下行别名 → E1001(as 后须同行 ident;fmt 永不在此断行)。
- 前奏类(`std.time.Clock` 等)带 as:P1a 自举静默略过、Rust 按原名绑定——**P1b 落 E2020.use.nat 统一拦截**,本片不新增负例。
- 组内点分段项(seed/Rust 宽松接受面):别名不放宽,`a.b as c` 组项行为不变(仍按现有宽松路径,不合并本片)。
- 同一 orig 既裸导又别名导:v1 允许(两个绑定同实体,spec §9)。
