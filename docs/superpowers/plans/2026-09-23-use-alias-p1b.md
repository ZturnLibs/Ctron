# use 别名导入 P1b · Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 P1b:自举选择性合并+引用闭包(补 S1/S5 冲突消解、语义收紧)、E2020.use.nat 前奏别名拦截三宿主、Rust E5035、诊断收口。

**Architecture:** 复用 driver_ast 的形态表(迁入 CORE 共享)做引用收集器(`pkg_refs_walk`:S 槽收集、N 槽递归、未知形态保守);合并集 = 请求(改名)∪ 闭包 ∪ 非 7-kind 全随;未知形态/遍历失败保守回退整模块合并(= P1a 行为,方向安全:过近似 ⊆ 整模块)。

**Tech Stack:** 同 P1a(自举 + C 宿主 + Rust 参考)。

**Spec:** `docs/superpowers/specs/2026-09-22-use-alias-design.md` §4.2/§4.6/§6/§8(P1b 行);P1a 已落库 6779587..6e18331。

## Global Constraints

- 三宿主诊断码一致(E1001/E2020.use.\*/E5030/E5035/E2020.use.nat)。
- fmt 零改动;parity 0 分歧。
- **decl 锁 = 实测校准**(P1a 教训:共享工作区上锁值含并行线批次;每次动 CORE 后跑 check 取实测值,同步 grep 值与 ok 文案)。
- 提交 pathspec 限定;暂存后 `git diff --cached --stat` 必查;并行泳道高频提交,动手前 `git log --oneline -5 -- <path>` 对齐。
- 自举语法:字符串裸 `{` 写 `\{`;禁 `;`;W8030 未用局部按 rc=1(用 `_` 前缀豁免)。
- 模块夹具走 `tests/modules/`(宿主门禁 = pkg 面)。
- 验证用新产物:`sh compiler/build.sh && sh compiler/native.sh` / `make -C compiler-c` / `cargo build --release`。

## 已裁决的实现精化(spec §4.2 的落地口径,终审背书方向)

- **非 7-kind decl(Impl/Trait/Test/Prop 等)恒随合并**(不参与"请求∪闭包"过滤):保 impl 方法分派与模块测试行为与 P1a 全等;7-kind = Fn/FnPub/Struct/Enum/FnExt/Static/Const(与既有 E5030 dup 检查面一致)。
- **保守回退**:形态表缺标签(?)或遍历异常 → 该模块整模块合并(= P1a 行为)。方向论证:过近似 ⊆ 整模块合并,不会引入 P1a 没有的红。
- E5035 入口面语义不变(别名 vs 本文件已有 decl);合并流内撞名仍 E5030 兜底;选择性合并后 S1(两模块同名各别名)自然成立——未请求的撞名件不再入流。

---

### Task 1: 夹具与红基线(conflict / nat)

**Files:**
- Create: `tests/modules/use_alias_conflict/`(Ctron.ctcl + src/{main.ct, a.ct, b.ct})
- Create: `tests/modules/use_alias_nat/`(Ctron.ctcl + src/main.ct)

**Interfaces:**
- Produces: `use_alias_conflict`(Task 2 绿靶:S1 冲突消解)、`use_alias_nat`(Task 3/4/5 的 E2020.use.nat 三宿主负例锚)。

- [ ] **Step 1: use_alias_conflict 四件**

`Ctron.ctcl`(同 use_ok 清单)。`src/a.ct`:
```ctron
pub fn open() -> I32 {
    return 1
}

fn same_name() -> I32 {
    return 9
}
```
`src/b.ct`:
```ctron
pub fn open() -> I32 {
    return 2
}

fn same_name() -> I32 {
    return 8
}
```
`src/main.ct`:
```ctron
use app.a.{
    open as open_a,
}
use app.b.{
    open as open_b,
}

test "S1 conflict resolved by alias" {
    assert_eq(open_a(), 1)
    assert_eq(open_b(), 2)
}
```
(a/b 的未引用私有件 `same_name` 同名——P1a 整模块合并下两件搭车互撞,这是红基线的来源。)

- [ ] **Step 2: use_alias_nat 两件**

`Ctron.ctcl` 同上。`src/main.ct`:
```ctron
//@ fail: E2020.use.nat
use std.time.Clock as K

fn main() -> I32 {
    return 0
}
```

- [ ] **Step 3: 跑红基线**

```sh
cd /Users/zyj/Zturn/Ctron
compiler/bin/ctron-chk run tests/modules/use_alias_conflict/src/main.ct; echo "rc=$?"
compiler/bin/ctron-chk run tests/modules/use_alias_nat/src/main.ct; echo "rc=$?"
```
Expected(红基线,记入报告):
- conflict:rc=1,`E5030`(same_name 两件搭车互撞)——Task 2 转 TDD 绿。
- nat:rc=1 但码为 `E2020`(K 未解析;P1a 前奏别名静默略过)——Task 3 转 `E2020.use.nat`。

- [ ] **Step 4: 提交**

```sh
git add tests/modules/use_alias_conflict tests/modules/use_alias_nat
git diff --cached --stat && git commit -m "test(modules): use 别名 P1b 夹具——S1 冲突消解绿靶+前奏别名 nat 负例(红基线:conflict E5030 搭车互撞/nat 缺码)"
```

---

### Task 2: 自举选择性合并 + 引用闭包(核心)

**Files:**
- Modify: `compiler/src/parse_node.ct`(迁入 ast_put/ast_shape/ast_kind + 新 fn ast_tables)
- Modify: `compiler/src/driver_ast.ct`(main 改用共享 ast_tables,删本地表段)
- Modify: `compiler/src/parse_pkg.ct`(pkg_is_named_kind + pkg_refs_walk + 选择性合并)
- Test: Task 1 夹具 + use_ok/use_alias_basic/use_alias_dup 回归

**Interfaces:**
- Produces: `ast_tables(ktags: List[Str], kshapes: List[Str])`(CORE 共享形态表);`pkg_is_named_kind(t: Str) -> Bool`;`pkg_refs_walk(d, ktags, kshapes, acc, depth) -> Bool`(false = 保守信号)。

- [ ] **Step 1: 形态表共享化**

把 `driver_ast.ct` 的 `ast_put`(:14)/`ast_shape`(:19)/`ast_kind`(:30)三 fn 与 main 里的 101 行 `ast_put` 表(:133 起)整体迁入 `parse_node.ct`,表段包成:
```ctron
// 形态表(语料驱动增量;未知标签 fail-closed → 加表再验)——cc_ast(driver_ast)与
// pkg 加载器(选择性合并引用收集)共用;迁自 driver_ast.ct,逐字不变
fn ast_tables(ktags: List[Str], kshapes: List[Str]) {
    ast_put(ktags, kshapes, "File", "N*")
    ...(101 条逐字迁入)...
}
```
`driver_ast.ct` main 中表段改为 `ast_tables(ktags, kshapes)`,并删本地三 fn(`cc_ast` 经 CORE 拼接仍可见)。
验证:`sh compiler/build.sh && compiler/ctc.sh ast compiler/test/fx_use_alias.ct` → roundtrip OK;`compiler/ctc.sh ast tests/doc_fix/geom.ct` → OK。

- [ ] **Step 2: parse_pkg.ct 三个新 fn(pkg_load_use_done 之前)**

```ctron
// 名槽 decl 种类(与 E5030 dup 检查面一致;spec §4.2 P1b 精化口径)
fn pkg_is_named_kind(t: Str) -> Bool {
    return or2(t == "Fn", or2(t == "FnPub", or2(t == "Struct", or2(t == "Enum", or2(t == "FnExt", or2(t == "Static", t == "Const"))))))
}

// 引用收集:形态表驱动,S 槽收集、N 槽递归;未知标签/非法槽/超深 → false(保守信号)
fn pkg_refs_walk(d: List[Str], ktags: List[Str], kshapes: List[Str], acc: List[Str], depth: I32) -> Bool {
    if depth > 4000 { return false }
    var sh = ast_shape(ktags, kshapes, d[0])
    if sh == "?" { return false }
    var i: I32 = 1
    while i < d.len {
        var kind = ast_kind(sh, i - 1)
        if kind == "S" {
            acc.push(d[i])
        } else {
            if kind == "N" {
                if !pkg_refs_walk(d[i], ktags, kshapes, acc, depth + 1) { return false }
            } else {
                return false
            }
        }
        i += 1
    }
    return true
}
```

- [ ] **Step 3: 选择性合并(pkg_load_use_done 的合并段改造)**

可见性/E5035 前检之后、j4 合并环之前插闭包计算(关键变量沿用现文:`mf`/`syms`):
```ctron
                        // —— 选择性合并(spec §4.2):7-kind = 请求 ∪ 闭包;非 7-kind 恒随;
                        //    遍历失败/未知形态 → 保守整模块(= P1a 行为)——
                        var keep = List[Str]()
                        var k9: I32 = 0
                        while k9 < syms.len {
                            keep.push(syms[k9])
                            k9 += 2
                        }
                        var refs_ok = true
                        var grow = true
                        while grow {
                            grow = false
                            var acc = List[Str]()
                            var j9: I32 = 1
                            while j9 < mf.len {
                                if mf[j9].len > 1 && pkg_is_named_kind(mf[j9][0]) {
                                    var ink = false
                                    var ka: I32 = 0
                                    while ka < keep.len {
                                        if keep[ka] == mf[j9][1] { ink = true }
                                        ka += 1
                                    }
                                    if ink {
                                        if !pkg_refs_walk(mf[j9], ktags, kshapes, acc, 0) { refs_ok = false }
                                    }
                                }
                                j9 += 1
                            }
                            if !refs_ok { grow = false }
                            if refs_ok {
                                var j10: I32 = 1
                                while j10 < mf.len {
                                    if mf[j10].len > 1 && pkg_is_named_kind(mf[j10][0]) {
                                        var ink2 = false
                                        var kb: I32 = 0
                                        while kb < keep.len {
                                            if keep[kb] == mf[j10][1] { ink2 = true }
                                            kb += 1
                                        }
                                        if !ink2 {
                                            var kc: I32 = 0
                                            while kc < acc.len {
                                                if acc[kc] == mf[j10][1] {
                                                    keep.push(mf[j10][1])
                                                    grow = true
                                                }
                                                kc += 1
                                            }
                                        }
                                    }
                                }
                                j10 += 1
                            }
                        }
```
j4 合并环:dup 检查前加跳过判定(7-kind 不在 keep → 不并入):
```ctron
                                var named = pkg_is_named_kind(mf[j4][0])
                                if named && !refs_ok {
                                    // 保守回退:视为 keep(整模块合并)
                                } else if named {
                                    var ink3 = false
                                    var kd: I32 = 0
                                    while kd < keep.len {
                                        if keep[kd] == mf[j4][1] { ink3 = true }
                                        kd += 1
                                    }
                                    if !ink3 {
                                        j4 += 1
                                        continue
                                    }
                                }
```
(语言支持 continue:parse_stmt.ct:267 分流、trans_stmt 有发射面。)
同时把 dup 检查里两处 7-kind or2 长链换 `pkg_is_named_kind(...)`(行为逐字等价,DRY)。

- [ ] **Step 4: 重建 + 五夹具 + smoke**

```sh
cd /Users/zyj/Zturn/Ctron && sh compiler/build.sh && sh compiler/native.sh
compiler/bin/ctron-chk run tests/modules/use_alias_conflict/src/main.ct && echo CONFLICT_OK
compiler/bin/ctron-chk run tests/modules/use_alias_basic/src/main.ct && echo BASIC_OK
compiler/bin/ctron-chk run tests/modules/use_alias_dup/src/main.ct 2>&1 | grep -q E5035 && echo DUP_OK
compiler/bin/ctron-chk run tests/modules/use_ok/src/main.ct && echo USEOK_OK
sh compiler/test/smoke.sh 2>&1 | tail -2
```
Expected: 四夹具绿;smoke 全绿(除 std/gui.ct 漂移类预存)。**smoke 红 = 收紧审计第一轮信号**:红点即搭车依赖,按"补显式 use"修复(属本任务范围,修的是调用方文件);若红点在并行泳道领地(tests/gui 等),登记台账并最小补 use。
- decl 锁:parse_node.ct 迁入 +4 decl(ast_put/ast_shape/ast_kind/ast_tables)→ 实测新值,同步 smoke.sh grep 与 ok 文案。

- [ ] **Step 5: 提交**

```sh
git add compiler/src/parse_node.ct compiler/src/driver_ast.ct compiler/src/parse_pkg.ct compiler/test/smoke.sh
git diff --cached --stat && git commit -m "feat(compiler): use 别名 P1b——选择性合并+引用闭包(形态表共享化)

spec §4.2:7-kind = 请求∪闭包(形态表驱动 S 槽收集),非 7-kind 恒随(impl 分派/模块测试
行为与 P1a 全等);未知形态/遍历失败保守整模块(过近似⊆整模块,方向安全)。S1 冲突消解
落地(use_alias_conflict 绿)。形态表 ast_put/ast_shape/ast_kind+ast_tables 迁 CORE
parse_node.ct,driver_ast 共享;decl 锁实测校准 <N>。"
```

---

### Task 3: 自举诊断收口(nat / 链式 as / i18n)

**Files:**
- Modify: `compiler/src/parse_pkg.ct`(None 分支 nat)
- Modify: `compiler/src/parse_decl.ct`(链式 as E1001;三处别名文案 diag_text0 化)
- Modify: `compiler/src/diag_msg.ct`(E2020.use.nat 双语 + E1001 别名文案键)
- Test: use_alias_nat + 负例探针

**Interfaces:**
- Produces: `E2020.use.nat`(前奏符号不支持别名)。

- [ ] **Step 1: nat(None 分支)**

pkg_load_use_done 的 `None =>` 分支改为:
```ctron
                None => {
                    if !isstd {
                        diag1(diags, "E2020.use.read", "", mpath)
                        return out
                    }
                    // 前奏类文件缺失路径:带别名请求 = E2020.use.nat(spec §4.6)
                    var k13: I32 = 0
                    while k13 < syms.len {
                        if syms[k13 + 1] != syms[k13] {
                            diag1(diags, "E2020.use.nat", "", syms[k13 + 1])
                            return out
                        }
                        k13 += 2
                    }
                }
```
diag_msg.ct 双语(en 段 E5035 行后 / zh 段同位):
```ctron
    if seq2(key, "E2020.use.nat") { return "use alias on native/prelude symbol:%0 (not supported; import without alias)" }
    if seq2(key, "E2020.use.nat") { return "use 前奏/native 符号不支持别名:%0(请去掉 as 别名导入)" }
```

- [ ] **Step 2: 链式 as E1001(parse_decl.ct 组项别名成功后)**

`p_use_alias` 成功 push 后、`,` 检查前加:
```ctron
                    var nx2 = tok(toks, cur)
                    if or2(nx2 == "as", or2(nx2 != "," && nx2 != "}" && nx2 != "NL", nx2 == "#EOF")) {
                        // 精确口径:别名后仅容 , / } / NL;其余(含第二个 as)E1001
```
以现语言 or2 形态写清四分支;文案用 diag_text0 化后的键(见 Step 3)。**语义目标(终审发现 4):`{a as b as c}` → E1001,不再产出 (as,as) 垃圾对。**实现时以一个最小分支直写:
```ctron
                    if tok(toks, cur) != "," && tok(toks, cur) != "}" && tok(toks, cur) != "NL" && tok(toks, cur) != "#EOF" {
                        pdiag(pdiags, lns, cols, cur, "E1001", diag_text0("E1001.use.alias.tail"))
                        // 消费该记号防死循环,保 (orig,orig) 配对
                        syms.push(b2)
                        adv(cur)
                    }
```

- [ ] **Step 3: E1001 别名文案 i18n 化**

diag_msg.ct 增键(en/zh):
```ctron
    if seq2(key, "E1001.use.alias.ident") { return "use alias must be an identifier" }
    if seq2(key, "E1001.use.alias.tail") { return "expected , or } after use alias" }
    if seq2(key, "E1001.use.alias.ident") { return "use 别名须为标识符" }
    if seq2(key, "E1001.use.alias.tail") { return "use 别名后应为 , 或 }" }
```
parse_decl.ct 三处字面 `"use 别名须为标识符"` 改 `diag_text0("E1001.use.alias.ident")`(核实 diag_text0 签名/用法于 parse_decl.ct:324 既有调用)。

- [ ] **Step 4: 重建 + 验证**

```sh
cd /Users/zyj/Zturn/Ctron && sh compiler/build.sh && sh compiler/native.sh
compiler/bin/ctron-chk run tests/modules/use_alias_nat/src/main.ct 2>&1 | grep -q "E2020.use.nat" && echo NAT_OK
printf 'use std.str.{lines as l as m}\n' > /tmp/chain.ct && compiler/bin/ctron-chk run /tmp/chain.ct 2>&1 | grep -q E1001 && echo CHAIN_OK
printf 'use std.str.{lines as fn}\n' > /tmp/kw.ct && compiler/bin/ctron-chk run /tmp/kw.ct 2>&1 | grep -q E1001 && echo KW_OK
compiler/bin/ctron-chk run tests/modules/use_alias_basic/src/main.ct && echo BASIC_OK
compiler/ctc.sh check compiler/test/fx_use_alias.ct --lang=en 2>&1 | head -2
```
Expected: 全过;--lang=en 面别名 E1001 出英文文案。

- [ ] **Step 5: 提交**

```sh
git add compiler/src/parse_pkg.ct compiler/src/parse_decl.ct compiler/src/diag_msg.ct
git diff --cached --stat && git commit -m "feat(compiler): use 别名 P1b 诊断收口——E2020.use.nat+链式as E1001+别名文案 i18n"
```

---

### Task 4: C 宿主(nat + E5035 种类面)

**Files:**
- Modify: `compiler-c/src/pkg.c`(check_use_alias_collisions 补 D_CONST;新 check_use_alias_nat 挂 check_use_visibility 后)

**Interfaces:**
- Produces: 宿主 pkg 面对前奏别名报 E2020.use.nat;E5035 种类面含 D_CONST。

- [ ] **Step 1: nat 检查**

`check_caps` 的键集即前奏面({fs,time,net,db},pkg.c:834)。新 fn(镜像 check_caps 的键判定):
```c
// E2020.use.nat:前奏/native 符号不支持别名(spec 2026-09-22 §4.6;check 面)
static void check_use_alias_nat(pkg_res* r, const pkg* p, const mod* m) {
    (void)p;
    const cfile* f = m->pr.file;
    for (size_t j = 0; j < f->ndecls; j++) {
        const cdecl* d = &f->decls[j];
        if (d->kind != D_USE) continue;
        for (size_t k = 0; k < d->use.nimports; k++) {
            const cimport* imp = &d->use.imports[k];
            if (!imp->alias) continue;
            if (imp->nsegs < 2 || strcmp(imp->segs[0], "std") != 0) continue;
            const char* key = imp->segs[1];
            if (strcmp(key, "fs") != 0 && strcmp(key, "time") != 0 &&
                strcmp(key, "net") != 0 && strcmp(key, "db") != 0) continue;
            push(r, m->rel, "E2020.use.nat", "use 前奏/native 符号不支持别名:%s", imp->alias);
        }
    }
}
```
挂载在 `check_use_alias_collisions` 调用后。注意组导入的 imp->segs 已拆全路径(nsegs=3 时 segs[1]=模块键);非组 `use std.time.Clock as K` nsegs=3 同构 ✓。

- [ ] **Step 2: E5035 switch 补 D_CONST**

`check_use_alias_collisions` 的 switch 加 `case D_CONST: nm = dd->cnst.name; break;`(以 ast.h 实际字段名为准,先 grep)。

- [ ] **Step 3: 重建 + 验证**

```sh
cd /Users/zyj/Zturn/Ctron && make -C compiler-c 2>&1 | tail -1
compiler-c/build/ctronc pkg tests/modules/use_alias_nat 2>&1 | grep -q "E2020.use.nat" && echo HOST_NAT_OK
compiler-c/build/ctronc pkg tests/modules/use_alias_conflict && echo HOST_CONFLICT_OK
compiler-c/build/ctronc pkg tests/modules/use_alias_dup 2>&1 | grep -q E5035 && echo HOST_DUP_OK
compiler-c/build/ctronc pkg tests/modules/use_alias_basic && echo HOST_BASIC_OK
compiler-c/build/ctronc pkg tests/modules/use_ok && echo HOST_USEOK_OK
```

- [ ] **Step 4: 提交**

```sh
git add compiler-c/src/pkg.c
git diff --cached --stat && git commit -m "feat(compiler-c): use 别名 P1b——pkg 面 E2020.use.nat+E5035 补 D_CONST"
```

---

### Task 5: Rust(nat + E5035)

**Files:**
- Modify: `compiler-rust/src/sem.rs`(std/stdweb 分支 nat 诊断;绑定环 E5035)
- Modify: `compiler-rust/src/parser.rs`(或 sem tests 处)新增单测

**Interfaces:**
- Produces: Rust 宿主对前奏别名报 E2020.use.nat;bind 撞自有符号/前序别名报 E5035。

- [ ] **Step 1: nat**

resolve_import 的 std/stdweb 分支:alias 为 Some 且命中原生面(Fs/Clock/Net/Log/parallel 或任意 std 头)→ 推 `E2020.use.nat` 诊断(message 对齐 zh 文案),不绑定别名。实现落点:绑定环(带 import_diags 的处)在调 resolve_import 前判:
```rust
            if let Some(a) = &imp.alias {
                if matches!(imp.segs.first().map(String::as_str), Some("std") | Some("stdweb")) {
                    import_diags.push(Diagnostic {
                        code: "E2020.use.nat",
                        message: format!("use 前奏/native 符号不支持别名:{a}"),
                        span: crate::token::Span::new(1, 1, 0, 0),
                    });
                    continue;
                }
            }
```
(Diagnostic 构造形态以 sem.rs 现有为准;span 若不可得用现有占位形态。)resolve_import 头注释的"P1b 落 E2020.use.nat"同步摘除。

- [ ] **Step 2: E5035**

绑定环内,本包分支 resolve 成功插入 bind 前后查:
```rust
            // E5035:别名/绑定名与本模块自有符号或前序绑定撞名
            let dup = mod_syms[pf.file_idx].contains_key(bind)
                || own_names.contains(bind);   // own_names = 第一遍收集的本模块 decl 名集
            if dup {
                import_diags.push(Diagnostic {
                    code: "E5035",
                    message: format!("use 别名撞名:{bind}(与既有名冲突)"),
                    span: crate::token::Span::new(1, 1, 0, 0),
                });
                continue;
            }
```
`own_names` 在绑定环前从 `mod_syms[pf.file_idx]` 键集快照(第一遍是本模块自有 decl;导入环逐步插入,故快照须在环前)。message 码面与自举/宿主一致。

- [ ] **Step 3: 单测**

sem 面两个断言(落 parser.rs tests 或 sem 现有测试位,按仓内惯例):
- `use app.util.{double as triple}` + 本模块有 triple → E5035;
- `use std.time.Clock as K` → E2020.use.nat。
(以现测试基建能构造多模块/诊断断言为准;若 sem 测试基建不支持多模块,则放最小集成探针并报告。)

- [ ] **Step 4: 构建 + 测试**

```sh
cd /Users/zyj/Zturn/Ctron/compiler-rust && cargo build --release 2>&1 | tail -1 && cargo test --lib 2>&1 | tail -3
```
Expected: 全绿,通过数 = Task 5 基线(49)+ 新增。

- [ ] **Step 5: 提交**

```sh
git add compiler-rust/src/sem.rs compiler-rust/src/parser.rs
git diff --cached --stat && git commit -m "feat(compiler-rust): use 别名 P1b——E2020.use.nat+E5035 绑定撞名"
```

---

### Task 6: 收紧审计 + 全量门禁 + 提交

**Files:**
- Modify: 视审计结果(调用方补显式 use 行;并行泳道领地登记不代改)

**Interfaces:**
- Consumes: Task 1-5 全部就位。

- [ ] **Step 1: 全量门禁(审计 = 门禁全绿即审计过)**

```sh
cd /Users/zyj/Zturn/Ctron
sh tests/fmt/parity.sh | tail -1
python3 compiler/test/suite.py 2>&1 | tail -8
sh compiler/test/smoke.sh 2>&1 | tail -2
```
Expected: parity 0 分歧;suite 自举 73/73 + modules 15/15 自举(13+2)、宿主 pkg ≥13/15(预存 dup_static 红维持);smoke 全绿除预存漂移。**任何非预存红 = 搭车依赖点**:定位调用方文件,补显式 use 行(语义等价,spec §2.2 本意),复跑至绿;并行泳道领地(tests/gui 域包等)的红登记台账并最小补 use(机械改动,不算越界)。

- [ ] **Step 2: 抽查域包(门禁外置信面)**

```sh
grep -rn "use gui\|use app\." tests/gui/*/src/*.ct examples/*/src/*.ct 2>/dev/null | head -10
```
对 bdb4265 落的 Todo 域包:确认主文件调用的包内符号都在 use 行里;缺则补。发现的每处记入报告(收敛审计证据)。

- [ ] **Step 3: 提交**

```sh
git add <审计改动文件>
git diff --cached --stat && git commit -m "fix(std/tests): use 别名 P1b 收紧审计——搭车依赖点补显式 use(清单见报告)"
```
若无改动则跳过本步。

---

## 已知边界(P1c/后续,不在本计划)

- E5035 合并流内撞名升级(现 E5030 兜底)与别名互撞 seed pkg 面——终审发现 3。
- C 宿主 E5035 种类面 D_TEST(本计划补 D_CONST 即与自举主导面一致)。
- "NL" 伪记号系统性歧义(lexer 面,backlog)。
