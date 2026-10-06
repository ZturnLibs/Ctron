# T49 · lockfile 内容寻址 + workspace + add/publish 实装——实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 依赖解析闭环:严格 semver 约束 + 内容寻址 `Ctron.lock` + workspace 段 + `ctron add/publish/lock` 本地 registry 面(T43 骨架转实,零网络)。

**Architecture:** 装载器(`parse_pkg.ct`)新增 dep 表探针插进四级解析链(①目录相对之后、域根之前);新编译器驱动 `driver_dep.ct`(原生二进制 `ctron-dep`,dev 面 `ctc.sh dep`)负责解析+lock 生成+digest 校验;`ctron` bash CLI 将 T43 的 `add`/`publish` 骨架臂转实 + 新增 `lock` 臂(纯目录协议 registry,布局 `<reg>/<name>/<ver>/{manifest,module,sha256}`,ctron.ps1 同文);workspace = 清单顶层 `workspace {}` + `member "名" { path }` 键控块,三线注册(C/Rust/Python/bootstrap)防 E5044 分歧。

**Tech Stack:** Ctron 自举编译器(seed 宿主解释 + emit 发射)、bash(ctron CLI)、Python(ctcl_check 参考线)、C(pkg.c)、Rust(check.rs)。

## Global Constraints

- **T43 坑位(横切)**:publish 涉及对外发布面,**未经用户确认不得接真 registry**——本件只做本地目录协议,零网络、零 git fetch。
- **T50 接口(横切)**:解析链④ `deps/<pkg>.ctart` 工件回落**原样保底**,本件不触碰(S2a 前置接口)。
- 解析链序(§2.2):①目录相对 → **②dep 表探针(本件新增)** → ③域根(libroot)→ ④site root → ⑤ctart;①③④⑤既有语义零扰动。
- 三线诊断逐字一致:workspace/member 注册 + E5045 重复 member + E5051 三线(C `pkg.c` / R `check.rs` / Python `ctcl_check.py` / 自举 `pkg_dep.ct`)同码同文案。dep **version 值语法三线均不校验**(约束文法校验归 ctron-dep 工具面,防三线分歧)。
- E5049 文案镜像 C 线现役模板(`compiler-c/src/pkg.c:408-416`):`dep 来源互斥:%s 与 %s 同现`(labels 序 = path, git+rev, version,取同现前两)/ `dep 需要且仅需要一种来源:path | git+rev | version`;git 无 rev 塌缩为缺来源臂。
- 新诊断码**先进表再使用**:E5051(workspace/member 块错)/ E5052(dep 解析失败)/ E5053(lock 内容寻址不符)落 `docs/superpowers/specs/2026-09-16-config-language-v1.md` §6 注册表。
- lockfile 无时间戳(D4 禁日期的立项理由),name 字节序,CTCL 规范形态;**二跑逐字节稳定是验收锚**。
- 解析优先序(单个 version dep):①lock 钉定(满足约束)→ ②已安装 `pkgs/<名>/`(满足约束)→ ③registry 最高满足版。确定性 + 离线优先。
- Ctron 语法纪律(自举宿主):无位运算算符(`&`/`|`/`^` 一律算术等价或 or2)、无三元表达式(if 表达式)、`or2/seq2` 全括号、List[Str] 下标 0 为哨兵实数据自 1 起、字符串裸 `{` 须 `\{`、禁 `;`。
- **作业纪律(warzone 守则)**:专用 worktree(`.worktrees/t49`)作业;每 Task 全绿即独立提交(pathspec 限定);合并回 main 用单命令串联核 HEAD。
- 验证口径:夹具验收必须过 `compiler/ctc.sh`(seed 宿主);跨包引用即请求铁律不变。

---

### Task 1: worktree + 规范立表 + workspace/member 三线注册

**Files:**
- Create: worktree `.worktrees/t49`(branch `t49`,基于 main HEAD)
- Modify: `docs/superpowers/specs/2026-09-16-config-language-v1.md`(§5 注册表 + §6 诊断表 + §12 开放项翻面)
- Modify: `compiler-c/src/pkg.c`(块注册 + member 校验)
- Modify: `compiler-rust/src/check.rs`(同 C 线镜像)
- Modify: `tools/ctcl_check.py` + `tools/ctcl_manifest_schema.ctcl`(workspace/member 注册 + E5051)
- Test: `tests/manifest/positive/020_workspace.ctcl`、`tests/manifest/negative/020_member_no_path.ctcl`、`tests/manifest/negative/021_dup_member.ctcl`(编号以现场 `ls` 空位为准)

**Interfaces:**
- Consumes: 无(首件)。
- Produces: 三线对 workspace/member 块的共同判定面——workspace 记录块(≤1,无键,空块合法);member 键控块(名 = 成员名;键 path:Str 必填,缺 = E5051;重复名 = E5045,文案 `重复的 member "%s"(首次在第 %d 行)`);未知键 E5043(`块 workspace 中未知键 %s;合法键:(无)` / `块 member 中未知键 %s;合法键:path`)。后续 Task 的 ctron-dep/夹具全部依赖此注册面。

- [ ] **Step 1: 建 worktree**

```bash
cd /Users/zyj/Zturn/Ctron
git worktree add .worktrees/t49 -b t49
cd .worktrees/t49
```

注意:后续所有路径相对 worktree 根;主树保持不动(机刷守则)。worktree remove 前必先 cd 出(坑位在册)。

- [ ] **Step 2: 规范立表(config-language-v1.md)**

§5 注册表追加两行:

```markdown
| workspace | 记录,可选,≤1 | (v0 无键) | — | 工作区根标记;空块合法;未知键 = E5043 |
| member | 键控,可选,名 = 成员名,重复名 = E | path | Str | 相对工作区根;缺 path = E5051 |
```

§6 诊断表追加三行:

```markdown
| E5051 | E | workspace/member 块错(member 缺 path/重复 member) | "member %r 缺必填键 path" / "重复的 member %r(首次在第 %d 行)" |
| E5052 | E | dep 解析失败(未安装/registry 无满足版本/path 目录缺失) | "dep 解析失败:%s(试: ctron pkg add %s@%s)" |
| E5053 | E | lock 内容寻址不符(实装 digest ≠ lock digest) | "lock 内容寻址不符:%s(试: ctron pkg add %s 重装,或删 Ctron.lock 重解析)" |
```

§12 开放项前两条翻面(登记已落地,锚本计划):lockfile 注册表 = `lock { manifest_version = 1 }` + `pkg "名" { version/source/path?/git?/rev?/digest }`;workspace = `workspace {}` + `member "名" { path }`。

- [ ] **Step 3: C 线注册(pkg.c)**

先读 `pkg.c` 块分发区(约 370-410 行:`block[0]` 开块 + `strcmp(block, "dep")` 键分派 + `}` 收账区),然后:

1. 开块处理:workspace/member 与 dep 同形(member 带引号实参)入 `block`;member 开块时置 `member_active=1, m_has_path=0`,workspace 开块置 `ws_active=1`。
2. 键分派追加:

```c
        } else if (strcmp(block, "workspace") == 0) {
            push(&pend, "Ctron.ctcl", "E5043", "块 workspace 中未知键 %s;合法键:(无)", key);
        } else if (strcmp(block, "member") == 0) {
            if (strcmp(key, "path") == 0) m_has_path = 1;
            else push(&pend, "Ctron.ctcl", "E5043", "块 member 中未知键 %s;合法键:path", key);
        }
```

3. `}` 收账区(dep_active 臂后)追加:

```c
            if (member_active) {
                if (!m_has_path)
                    push(r, "Ctron.ctcl", "E5051", "member \"%s\" 缺必填键 path", block_arg);
                member_active = 0;
            }
```

(`block_arg` = member 块的引号实参,dep 同款存法;若无现成变量则按 dep 块名同法存。)
4. 重复 member:开块时若与已见 member 名重复 → `push(r, "Ctron.ctcl", "E5045", "重复的 member \"%s\"(首次在第 %d 行)", 名, 首行)`(dep 的 E5045 同构参照)。

- [ ] **Step 4: Rust 线注册(check.rs)**

镜像 C 线:`workspace`/`member` 入块表;member 缺 path → `E5051`(文案与 C 逐字一致);重复 member → E5045。E5043 文案同 C(`块 workspace 中未知键 {} ;合法键:(无)` 格式化对齐)。跑 `cargo test`(compiler-rust)全绿。

- [ ] **Step 5: Python 参考线(ctcl_check.py + schema)**

`ctcl_manifest_schema.ctcl` 增 workspace/member 块注册(schema-as-CTCL);`ctcl_check.py` validate 增:member 缺 path → E5051、重复 member → E5045、workspace 未知键 → E5043。跑 `python3 tests/manifest/run.py` 既有语料全绿(零回归)。

- [ ] **Step 6: 黄金语料**

`tests/manifest/positive/020_workspace.ctcl`(编号按现场空位):

```text
pkg {
    manifest_version = 1
    name = "ws"
    version = "0.1.0"
}

workspace {
}

member "app" {
    path = "app"
}

member "libm" {
    path = "libm"
}
```

`tests/manifest/negative/020_member_no_path.ctcl`:

```text
// expect-error: E5051
pkg {
    manifest_version = 1
    name = "ws"
    version = "0.1.0"
}

member "broken" {
}
```

`tests/manifest/negative/021_dup_member.ctcl`:

```text
// expect-error: E5045
pkg {
    manifest_version = 1
    name = "ws"
    version = "0.1.0"
}

member "a" {
    path = "a"
}

member "a" {
    path = "a2"
}
```

- [ ] **Step 7: 三线对拍门**

```bash
python3 tests/manifest/run.py          # Python 参考线:全绿
python3 tests/manifest/diff.py         # 三线对拍:逐字节一致
(cd compiler-rust && cargo test)       # 13 套全绿
(cd compiler-c && make test)           # 套件全绿(坑位 36:改 pkg.c 必 make)
```

- [ ] **Step 8: 提交**

```bash
git add docs/superpowers/specs/2026-09-16-config-language-v1.md compiler-c/src/pkg.c compiler-rust/src/check.rs tools/ctcl_check.py tools/ctcl_manifest_schema.ctcl tests/manifest/
git commit -m "feat(pkg): T49-1 workspace/member 三线注册+E5051-5053 立表——规范 §5/§6/§12,C/Rust/Python 三线同判,黄金语料三例"
```

---

### Task 2: pkg_dep.ct——dep 扫描(E5049)+ 约束工具 + sha256 移植

**Files:**
- Create: `compiler/src/pkg_dep.ct`(build.sh CORE 拼接,parse_pkg.ct 之后)
- Modify: `compiler/build.sh`(CORE 清单加 `$SRC/pkg_dep.ct`)
- Modify: `compiler/src/parse_pkg.ct`(pkg_load_use_done 顶部 eager 扫描,E5049 尽早报)
- Test: `tests/modules/dep_mutex_neg/`(负锚)

**Interfaces:**
- Consumes: `parse_pkg.ct` 的 `pkg_ctcl_at`/`pkg_ctcl_pre`/`pkg_plugin_field`(文本扫描三助手,直接复用)。
- Produces(后续 Task 依赖的精确签名):
  - `pkg_deps_of(dir: Str, diags: List[Str]) -> List[Str]` — 平表每 dep 4 槽 `[name, form, value, extra]`;form ∈ `"path"`(value=路径)/`"git"`(value=URL,extra=rev)/`"version"`(value=约束);互斥/缺来源推 E5049 后该组 form=`""`。
  - `pkg_dep_slot(deps: List[Str], nm: Str) -> I32` — 按名查槽基,未命中 `-1`。
  - `pkg_constraint_check(v: Str) -> Str` — `""`=合法;非空=E5046 消息尾。
  - `pkg_constraint_ok(c: Str, v: Str) -> Bool` — caret 语义(X→[X.0.0, X+1.0.0);X.Y→[X.Y.0, X+1.0.0);全形/带 -pre = 精确)。
  - `pkg_ver_cmp(a: Str, b: Str) -> I32` — 数字三元比较 -1/0/1。
  - `pkg_sha256_hex(msg: Str) -> Str` — 64 位小写 hex(内容寻址底座)。

- [ ] **Step 1: 写 pkg_dep.ct 主体(dep 扫描)**

```ctron
// =====================================================================
// pkg_dep.ct —— T49 依赖解析(§2.7):清单 dep 块扫描(三互斥 E5049,文案镜像
// C/Rust 线)+ semver 约束工具 + sha256(内容寻址;自包含移植 std/crypto.ct)。
// phase-1 文本扫描口径同 pkg_plugins/pkg_caps_allowed(规范形态清单;
// fail-closed 全量清单诊断由 C/Rust 线与 tools 承担)。
// =====================================================================

// dep 块扫描。返回平表每 dep 4 槽 [name, form, value, extra]:
//   form = "path"    → value = 路径;extra = ""
//   form = "git"     → value = URL; extra = rev
//   form = "version" → value = 约束; extra = ""
//   form = ""(三互斥/缺来源已推 E5049)→ 调用方跳过
fn pkg_deps_of(dir: Str, diags: List[Str]) -> List[Str] {
    var out = List[Str]()
    var t = ""
    match read_file(dir + "/../Ctron.ctcl") {
        Some(x) => {
            // 发射语境 read_file 缺失回落 Some("")(T48 双口径):空文本 = 无清单
            if x.len == 0 {
                return out
            }
            t = x
        }
        None => { return out }
    }
    var i: I32 = 0
    while i < t.len {
        if pkg_ctcl_at(t, i, "dep") && pkg_ctcl_pre(t, i) {
            var j = i + 3
            while j < t.len && byte_at(t, j) == 32 {
                j += 1
            }
            if j < t.len && byte_at(t, j) == 34 {
                var k = j + 1
                while k < t.len && byte_at(t, k) != 34 {
                    k += 1
                }
                if k < t.len {
                    var nm = byte_slice(t, j + 1, k)
                    var b = k + 1
                    while b < t.len && byte_at(t, b) != 123 {
                        b += 1
                    }
                    if b < t.len {
                        var e = b + 1
                        while e < t.len && byte_at(t, e) != 125 {
                            e += 1
                        }
                        if e < t.len {
                            var body = byte_slice(t, b + 1, e)
                            var pv = pkg_plugin_field(body, "path")
                            var gv = pkg_plugin_field(body, "git")
                            var rv = pkg_plugin_field(body, "rev")
                            var vv = pkg_plugin_field(body, "version")
                            // C 线同判:g = [path, git&&rev, version](git 无 rev 塌缩缺来源)
                            var has_p = pv.len > 0
                            var has_g = or2(gv.len > 0, false) && rv.len > 0
                            var has_v = vv.len > 0
                            var np: I32 = 0
                            if has_p { np += 1 }
                            if has_g { np += 1 }
                            if has_v { np += 1 }
                            if np >= 2 {
                                // labels 注册表序 path < git+rev < version,取同现前两
                                var l1 = "path"
                                var l2 = "git+rev"
                                if !has_g {
                                    if has_v {
                                        l2 = "version"
                                    }
                                } else {
                                    if !has_p {
                                        l1 = "git+rev"
                                        l2 = "version"
                                    }
                                }
                                diags.push("Ctron.ctcl: E5049 dep 来源互斥:" + l1 + " 与 " + l2 + " 同现")
                            } else {
                                if np == 0 {
                                    diags.push("Ctron.ctcl: E5049 dep 需要且仅需要一种来源:path | git+rev | version")
                                } else {
                                    if has_p {
                                        out.push(nm)
                                        out.push("path")
                                        out.push(pv)
                                        out.push("")
                                    } else {
                                        if has_g {
                                            out.push(nm)
                                            out.push("git")
                                            out.push(gv)
                                            out.push(rv)
                                        } else {
                                            out.push(nm)
                                            out.push("version")
                                            out.push(vv)
                                            out.push("")
                                        }
                                    }
                                }
                            }
                        }
                    }
                    i = k
                }
            }
        }
        i += 1
    }
    return out
}

// dep 平表按名查槽基(4 槽组);未命中 -1
fn pkg_dep_slot(deps: List[Str], nm: Str) -> I32 {
    var i: I32 = 0
    while i + 3 < deps.len {
        if deps[i] == nm {
            return i
        }
        i += 4
    }
    return 0 - 1
}
```

(注意 `has_g` 的与运算:`(gv.len > 0) && (rv.len > 0)` 直写即可,上面 or2 形仅示范全括号纪律;以能过 seed 宿主为准。)

- [ ] **Step 2: 约束工具**

```ctron
// ---- semver 约束工具(ctron-dep 工具面;三线解析面不校验 dep 值语法)----

// 数字段读取:i 处须为数字且无前导零;返回段尾下标,-1 = 非法
fn pkg_cseg(v: Str, i: I32) -> I32 {
    if i >= v.len {
        return 0 - 1
    }
    var c = byte_at(v, i)
    if c < 48 || c > 57 {
        return 0 - 1
    }
    if c == 48 {
        return i + 1
    }
    var j = i
    while j < v.len {
        var d = byte_at(v, j)
        if d < 48 || d > 57 {
            break
        }
        j += 1
    }
    return j
}

// pre/build 标签读取:i 处起 [0-9A-Za-z.\-]+ 非空;返回尾下标,-1 = 非法
fn pkg_ctag(v: Str, i: I32) -> I32 {
    if i >= v.len {
        return 0 - 1
    }
    var j = i
    while j < v.len {
        var c = byte_at(v, j)
        var okc = or2(or2(or2(c >= 48 && c <= 57, c >= 97 && c <= 122), c >= 65 && c <= 90), or2(c == 45, c == 46))
        if !okc {
            return 0 - 1
        }
        j += 1
    }
    return j
}

// 约束文法:X | X.Y | X.Y.Z(数字段无前导零;全形可续 -pre 与 +build)。
// 返回 "" = 合法;非空 = E5046 消息尾。
fn pkg_constraint_check(v: Str) -> Str {
    var bad = "约束文法:X | X.Y | X.Y.Z(数字段无前导零)"
    var i = pkg_cseg(v, 0)
    if i < 0 { return bad }
    if i >= v.len { return "" }
    if byte_at(v, i) != 46 { return bad }
    var i2 = pkg_cseg(v, i + 1)
    if i2 < 0 { return bad }
    if i2 >= v.len { return "" }
    if byte_at(v, i2) != 46 { return bad }
    var i3 = pkg_cseg(v, i2 + 1)
    if i3 < 0 { return bad }
    if i3 >= v.len { return "" }
    var c = byte_at(v, i3)
    if c == 45 {
        var j = pkg_ctag(v, i3 + 1)
        if j < 0 { return bad }
        if j >= v.len { return "" }
        if byte_at(v, j) == 43 {
            var k = pkg_ctag(v, j + 1)
            if or2(k < 0, k < v.len) { return bad }
            return ""
        }
        return bad
    }
    if c == 43 {
        var k2 = pkg_ctag(v, i3 + 1)
        if or2(k2 < 0, k2 < v.len) { return bad }
        return ""
    }
    return bad
}

// 版本数字段 n(0/1/2)取值;-pre/+build 截断;缺省 0
fn pkg_ver_seg(v: Str, n: I32) -> I64 {
    var i: I32 = 0
    var seg: I32 = 0
    var acc: I64 = 0
    while i < v.len {
        var c = byte_at(v, i)
        if or2(c >= 48 && c <= 57, false) {
            acc = acc * 10 + (c - 48).as[I64]()
            i += 1
        } else {
            if c == 46 {
                if seg == n {
                    return acc
                }
                seg += 1
                acc = 0
                i += 1
            } else {
                break
            }
        }
    }
    if seg == n {
        return acc
    }
    return 0
}

// 版本比较(仅数字三元):-1/0/1
fn pkg_ver_cmp(a: Str, b: Str) -> I32 {
    var s: I32 = 0
    while s < 3 {
        var va = pkg_ver_seg(a, s)
        var vb = pkg_ver_seg(b, s)
        if va < vb {
            return 0 - 1
        }
        if va > vb {
            return 1
        }
        s += 1
    }
    return 0
}

// 约束满足:caret 语义;带 -pre 的约束仅精确匹配
fn pkg_constraint_ok(c: Str, v: Str) -> Bool {
    var dots: I32 = 0
    var pre = false
    var q: I32 = 0
    while q < c.len {
        var c2 = byte_at(c, q)
        if c2 == 46 {
            dots += 1
        }
        if c2 == 45 {
            pre = true
        }
        q += 1
    }
    if pre {
        return seq2(c, v)
    }
    if dots == 2 {
        return seq2(c, v)
    }
    var lo0 = pkg_ver_seg(c, 0)
    var lo1 = pkg_ver_seg(c, 1)
    var v0 = pkg_ver_seg(v, 0)
    var v1 = pkg_ver_seg(v, 1)
    if v0 != lo0 {
        return false
    }
    if dots == 0 {
        return true
    }
    if v1 < lo1 {
        return false
    }
    return true
}
```

- [ ] **Step 3: sha256 移植**

将 `lib/std/crypto.ct` 的 `z64`/`block_word`/`sha256_hex` 三 fn 原样复制进 pkg_dep.ct,改名 `pkg_z64`/`pkg_block_word`/`pkg_sha256_hex`,体内调用点同步改名(内容寻址底座;自包含,编译器拼接模型无 use 面)。port 后自验:`pkg_sha256_hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"`。

- [ ] **Step 4: build.sh 注册 + eager 扫描接线**

`compiler/build.sh` CORE 清单 `$SRC/parse_pkg.ct` 之后插 `$SRC/pkg_dep.ct`。

`compiler/src/parse_pkg.ct` 的 `pkg_load_use_done` 顶部,caps 检查早退之后插入:

```ctron
    // T49:dep 块扫描(三互斥 E5049 尽早报;phase-1 文本扫描口径)。清单极小,
    // 每次调用重扫(同 pkg_caps_allowed 每文件重扫先例);表同时供下方 dep 探针用。
    var ddeps = pkg_deps_of(dir, diags)
    if diags.len > 0 {
        return file
    }
```

- [ ] **Step 5: 负锚夹具**

`tests/modules/dep_mutex_neg/Ctron.ctcl`:

```text
pkg {
    manifest_version = 1
    name = "depneg"
    version = "0.1.0"
}

dep "x" {
    path = "x"
    version = "1.0"
}
```

`tests/modules/dep_mutex_neg/src/main.ct`:

```ctron
fn main() -> I32 {
    return 0
}
```

- [ ] **Step 6: 验证**

```bash
compiler/build.sh > /dev/null
TMP=$(mktemp /tmp/t49s2.XXXXXX)
sed -e "s|ANCHORINPUT|$PWD/tests/modules/dep_mutex_neg/src/main.ct|" -e "s|ANCHORPROFILE|full|" -e "s|ANCHORLANG|zh|" compiler/build/cc_run.ct > "$TMP"
compiler-c/build/ctronc run "$TMP"; echo "rc=$?"
```

期望:rc=1,输出含 `E5049 dep 来源互斥:path 与 version 同现`。再跑 `compiler/test/suite.py`(既有 100 例零回归——所有无 dep 清单的夹具扫描为空表零扰动)。

- [ ] **Step 7: 提交**

```bash
git add compiler/src/pkg_dep.ct compiler/src/parse_pkg.ct compiler/build.sh tests/modules/dep_mutex_neg
git commit -m "feat(pkg): T49-2 pkg_dep.ct dep 扫描(E5049 镜像 C 线)+semver 约束工具+sha256 移植,eager 接线 parse_pkg"
```

---

### Task 3: 装载器 dep 探针(解析链②)

**Files:**
- Modify: `compiler/src/parse_pkg.ct`(`pkg_load_use_done` 非 std use 分支)
- Test: `tests/modules/dep_path_ok/`(正例:门面命中)

**Interfaces:**
- Consumes: Task 2 的 `pkg_deps_of`/`pkg_dep_slot`。
- Produces: 解析链②dep 探针——path 形:`<mfdir>/<path>/<名>.ct`(1 段)或 `<mfdir>/<path>/<子路径>.ct`(≥2 段);version/git 形:`<mfdir>/pkgs/<名>/<名>.ct`(1 段)或 `<mfdir>/pkgs/<名>/<子路径>.ct`(≥2 段)。W8902 miss 收集扩为 `segs.len >= 3 || dep 表点名`。

- [ ] **Step 1: 探针接线**

`pkg_load_use_done` 非 std 分支(`} else {` 后、chain① `mpath = dir + rel + ".ct"` 之前)提升:

```ctron
                var dslot = pkg_dep_slot(ddeps, segs[1])
```

chain① `if !fs_exists(mpath)` 之后、域根分支之前插入:

```ctron
                // T49 dep 探针(解析链②):dep 表点名首段 → path 形取依赖目录门面/
                // 子路径,version/git 形取项目 pkgs/<名>/ 安装位。目录相对(①)优先
                // 不变;dep 表之后仍有域根/site/ctart 兜底,既有语义零扰动。
                if !fs_exists(mpath) && dslot >= 0 {
                    var dform = ddeps[dslot + 1]
                    var dval = ddeps[dslot + 2]
                    var cand = ""
                    if dform == "path" {
                        if segs.len == 2 {
                            cand = dir + "/../" + dval + "/" + segs[1] + ".ct"
                        } else {
                            var jr: I32 = 2
                            var relr = ""
                            while jr < segs.len {
                                relr = relr + "/" + segs[jr]
                                jr += 1
                            }
                            cand = dir + "/../" + dval + relr + ".ct"
                        }
                    } else {
                        if segs.len == 2 {
                            cand = dir + "/../pkgs/" + segs[1] + "/" + segs[1] + ".ct"
                        } else {
                            var jr2: I32 = 1
                            var relr2 = ""
                            while jr2 < segs.len {
                                relr2 = relr2 + "/" + segs[jr2]
                                jr2 += 1
                            }
                            cand = dir + "/../pkgs" + relr2 + ".ct"
                        }
                    }
                    if fs_exists(cand) {
                        mpath = cand
                    }
                }
```

W8902 收集臂条件 `if !fs_exists(mpath) && segs.len >= 3 {` 改为:

```ctron
                if !fs_exists(mpath) && or2(segs.len >= 3, dslot >= 0) {
```

- [ ] **Step 2: 正例夹具**

`tests/modules/dep_path_ok/Ctron.ctcl`:

```text
pkg {
    manifest_version = 1
    name = "depapp"
    version = "0.1.0"
}

dep "libmath" {
    path = "libmath"
}
```

`tests/modules/dep_path_ok/libmath/libmath.ct`:

```ctron
// libmath —— T49 path 依赖夹具(门面形)
pub fn add(a: I32, b: I32) -> I32 {
    return a + b
}
```

`tests/modules/dep_path_ok/src/main.ct`:

```ctron
use libmath.{add}

fn main() -> I32 {
    println(add(2, 3).to_string())
    return 0
}
```

- [ ] **Step 3: 验证**

```bash
compiler/build.sh > /dev/null
TMP=$(mktemp /tmp/t49s3.XXXXXX)
sed -e "s|ANCHORINPUT|$PWD/tests/modules/dep_path_ok/src/main.ct|" -e "s|ANCHORPROFILE|full|" -e "s|ANCHORLANG|zh|" compiler/build/cc_run.ct > "$TMP"
compiler-c/build/ctronc run "$TMP"
```

期望:输出 `5`,rc=0。再跑 `compiler/test/suite.py` 零回归;`tests/modules/dep_mutex_neg` 仍 rc=1 E5049。

- [ ] **Step 4: 提交**

```bash
git add compiler/src/parse_pkg.ct tests/modules/dep_path_ok
git commit -m "feat(pkg): T49-3 装载器 dep 探针入解析链②——path 门面/pkgs 安装位双形,W8902 收集扩 dep 表点名"
```

---

### Task 4: driver_dep.ct + ctron-dep——解析 + lock 生成 + 内容校验

**Files:**
- Create: `compiler/src/driver_dep.ct`
- Create: `compiler/build/cc_dep.ct`(薄入口;由 build.sh 生成配方定义)
- Modify: `compiler/build.sh`(`cat $CORE "$SRC/driver_dep.ct" > "$OUT/cc_dep.ct"` + ANCHORVERSION 循环加 `cc_dep`)
- Modify: `compiler/ctc.sh`(`dep` 模式)
- Modify: `compiler/native.sh`(发射 `bin/ctron-dep`;检查 `install.sh` 是否显式列二进制清单,是则同步)
- Test: `tests/modules/dep_path_ok/`(lock 生成 + 二跑稳定 + 篡改 E5053,驱动脚本见 tests/pkg/run.sh 于 Task 6)

**Interfaces:**
- Consumes: Task 2 全部 + `pkg_plugin_field`(清单 name/version 提取)。
- Produces:
  - 命令面:`ctc.sh dep <Ctron.ctcl 路径>` 与 `ctron-dep run <Ctron.ctcl 路径>` 同语义:解析 → 校验 → 写 `Ctron.lock` → 摘要行;诊断 `Ctron.ctcl: E50xx msg` / `Ctron.lock: E5053 ...` rc=1。
  - lock 格式(规范形态,name 字节序,键序 = version/source/path|git/rev/digest):

```text
// Ctron.lock —— 依赖解析产物(内容寻址;勿手编,ctron-dep 生成)
lock {
    manifest_version = 1
}

pkg "libmath" {
    version = "1.0.0"
    source = "path"
    path = "libmath"
    digest = "sha256:<64hex 小写>"
}
```

  - digest 口径:version dep = 安装 module 文件(单文件)sha256;path dep = 依赖目录顶层 `*.ct` 按 name 字节序,`名 + "\n" + 字节数 + "\n" + 内容` 依序拼接后 sha256;git dep 同 path(安装树)。
  - registry 布局:`<reg>/<name>/<ver>/{manifest,module,sha256}`;reg root = `CTRON_REGPATH` → `$HOME/.ctron/registry`。

- [ ] **Step 1: 写 driver_dep.ct**

核心结构(全部 fn 落 `compiler/src/driver_dep.ct`):

```ctron
// =====================================================================
// driver_dep.ct —— T49 依赖解析驱动(ctron-dep;ctc.sh dep 同面)
// 输入锚 ANCHORINPUT = Ctron.ctcl 路径。流程:清单 → dep 扫描 → workspace
// 成员并集(Task 5)→ 逐 dep 解析(lock 钉定 → 已安装 → registry)→ digest
// 计算/校验 → Ctron.lock 规范形态写回(无时间戳,二跑逐字节稳定)。
// =====================================================================
fn dep_dir_of(p: Str) -> Str {
    // 取目录(无分隔符回落 ".");pkg_dir_of 同构
    var i = p.len - 1
    while i >= 0 {
        if byte_at(p, i) == 47 {
            return byte_slice(p, 0, i)
        }
        i -= 1
    }
    return "."
}

// 清单 pkg 块 name/version 键提取(phase-1:pkg 块居首,首键命中即用)
fn dep_mf_field(t: Str, key: Str) -> Str {
    return pkg_plugin_field(t, key)
}

// 目录顶层 *.ct 树摘要:名按字节序,名+换行+字节数+换行+内容依序拼接 sha256
fn dep_tree_digest(dir: Str) -> Str {
    var names = List[Str]()
    match read_dir(dir) {
        Some(ls) => {
            var i: I32 = 0
            var cur = ""
            while i < ls.len {
                var c = byte_at(ls, i)
                if c == 10 {
                    // 只收 .ct 结尾;read_dir 顺序不定,收完排序
                    if cur.len > 3 {
                        if byte_slice(cur, cur.len - 3, cur.len) == ".ct" {
                            names.push(cur)
                        }
                    }
                    cur = ""
                } else {
                    cur = cur + byte_slice(ls, i, i + 1)
                }
                i += 1
            }
        }
        None => { return "" }
    }
    // 插入排序(字节序;read_dir 顺序不定)
    var a: I32 = 1
    while a < names.len {
        var v = names[a]
        var b = a - 1
        while b >= 0 {
            if pkg_names_lt(v, names[b]) {
                names[b + 1] = names[b]
                b -= 1
            } else {
                break
            }
        }
        names[b + 1] = v
        a += 1
    }
    var acc = ""
    var k: I32 = 0
    while k < names.len {
        var src = read_file(dir + "/" + names[k])
        match src {
            Some(s) => {
                acc = acc + names[k] + "\n" + s.len.to_string() + "\n" + s
            }
            None => { return "" }
        }
        k += 1
    }
    return pkg_sha256_hex(acc)
}

// 字节序小于(name 排序用;逐字节,短前缀为小)
fn pkg_names_lt(a: Str, b: Str) -> Bool {
    var i: I32 = 0
    while i < a.len && i < b.len {
        if byte_at(a, i) != byte_at(b, i) {
            return byte_at(a, i) < byte_at(b, i)
        }
        i += 1
    }
    return a.len < b.len
}
```

解析主流程 `dep_resolve_project(mfpath, diags) -> List[Str]`,条目 7 槽 `[name, version, source, path, git, rev, digest]`:

1. 读清单文本(空/缺 → `diags.push("E5047 缺 pkg 块")` 风格诊断,rc=1 由 main 收口)。
2. `pkg_deps_of(dep_dir_of(mfpath), diags)`;workspace 分支见 Task 5(v1 先单项目)。
3. lock 既有钉定:`dep_lock_of(mfdir)` 平表 `[name, version, digest]`(扫 `pkg "名" {` 块,`pkg_plugin_field` 取键;文件缺 → 空表)。
4. registry root:`env_get("CTRON_REGPATH")`,空则 `env_get("HOME") + "/.ctron/registry"`。
5. 逐 dep(按平表序;form=`""` 跳过):
   - **path**:`depdir = mfdir + "/" + value`;`fs_exists(depdir)` 否则 `diags.push("Ctron.ctcl: E5052 dep 解析失败:path 依赖 " + nm + " 目录不存在:" + depdir)`;digest = `dep_tree_digest(depdir)`;version = 依赖清单 version 键(`read_file(depdir + "/Ctron.ctcl")` 有则取,无则 `"0.0.0"`)。
   - **version**:`pkg_constraint_check(value)` 非空 → `diags.push("Ctron.ctcl: E5046 键 version 值 '" + value + "' 不符合 " + 该消息)`;已安装位 = `mfdir + "/pkgs/" + nm`;安装 manifest = `已安装位 + "/Ctron.ctcl"`,安装 module = `已安装位 + "/" + nm + ".ct"`。
     ① lock 钉定:`pkg_lock_slot(locks, nm)` 命中且 `pkg_constraint_ok(lockver, value)` → ver=lockver,digest=lockdigest;校验 `pkg_sha256_hex(read_file(module))` == digest,不符 → `diags.push("Ctron.lock: E5053 lock 内容寻址不符:" + nm + "(试: ctron pkg add " + nm + " 重装,或删 Ctron.lock 重解析)")`。
     ② 已安装满足:安装 manifest 存在且其 version 满足约束 → ver/digest 自算(module sha256)。
     ③ registry:`dep_version_pick(regroot, nm, value)`(read_dir + 约束过滤 + `pkg_ver_cmp` 取最大,无 → `diags.push("Ctron.ctcl: E5052 dep 解析失败:" + nm + " 无满足 " + value + " 的已发布版本(registry: " + regroot + ")")`);命中 ver 后必须已安装且安装 version == ver,否则 `diags.push("Ctron.ctcl: E5052 dep 解析失败:" + nm + " 需 " + ver + "(试: ctron pkg add " + nm + "@" + ver + ")")`;digest = registry `<reg>/<nm>/<ver>/sha256` 文件内容去尾换行(与自算 module sha256 双核对,不符 E5053)。
   - **git**:已安装位同 version 形;lock 钉定(rev 相等才认)→ 校验 digest;无 lock → 首钉:ver 取安装 manifest version(缺省 `"0.0.0"`),digest 自算,诊断面打印提示 `ctron-dep: git " + nm + " 首次钉定 rev=" + rev`(非错误)。
6. 去重:同名条目第二次出现且 7 槽全等 → 丢弃;不等 → `diags.push("Ctron.ctcl: E5051 workspace 成员依赖冲突:" + nm + "(多次解析结果不一致)")`(单项目时同名 dep 重复已被 C 线 E5045 拦,此处为 workspace 并集防线)。

lock 组装(排序 + 渲染 + 写回):

```ctron
// 条目平表按 name 插入排序(7 槽组整体换位),渲染规范形态,内容变化才写
fn dep_lock_write(mfdir: Str, entries: List[Str]) -> Bool {
    // ... 排序 ...
    var out = "// Ctron.lock —— 依赖解析产物(内容寻址;勿手编,ctron-dep 生成)\n"
    out = out + "lock {\n    manifest_version = 1\n}\n"
    var i: I32 = 0
    while i + 6 < entries.len {
        out = out + "\npkg \"" + entries[i] + "\" {\n"
        out = out + "    version = \"" + entries[i + 1] + "\"\n"
        out = out + "    source = \"" + entries[i + 2] + "\"\n"
        if entries[i + 3].len > 0 {
            out = out + "    path = \"" + entries[i + 3] + "\"\n"
        }
        if entries[i + 4].len > 0 {
            out = out + "    git = \"" + entries[i + 4] + "\"\n"
        }
        if entries[i + 5].len > 0 {
            out = out + "    rev = \"" + entries[i + 5] + "\"\n"
        }
        out = out + "    digest = \"sha256:" + entries[i + 6] + "\"\n}\n"
        i += 7
    }
    var prev = ""
    match read_file(mfdir + "/Ctron.lock") {
        Some(p) => { prev = p }
        None => { }
    }
    if prev == out {
        return false
    }
    return fs_write(mfdir + "/Ctron.lock", out)
}
```

main:

```ctron
fn main() -> I32 {
    let mfpath = "ANCHORINPUT"
    var diags = List[Str]()
    var entries = dep_resolve_project(mfpath, diags)
    var note = ""
    ... // git 首钉等提示行经 note 传递
    if diags.len > 0 {
        var i: I32 = 0
        while i < diags.len {
            println(diags[i])
            i += 1
        }
        return 1
    }
    var changed = dep_lock_write(dep_dir_of(mfpath), entries)
    var verb = "无变化"
    if changed {
        verb = "已写回"
    }
    println("ctron-dep: 已解析 " + (entries.len / 7).to_string() + " 依赖 → " + mfpath + " 同目录 Ctron.lock(" + verb + ")")
    return 0
}
```

- [ ] **Step 2: 薄入口 + 构建接线**

`compiler/build.sh` 追加一行(与既有五产物同式):

```sh
cat $CORE "$SRC/driver_dep.ct" > "$OUT/cc_dep.ct"
```

ANCHORVERSION for 循环目标列加 `cc_dep`;末尾 echo 计数行加 cc_dep。

`compiler/build/cc_dep.ct`(提交的种子,build.sh 生成同形):

```ctron
// cc_dep —— 依赖解析驱动入口(ANCHORINPUT = Ctron.ctcl 路径)
```

(实作时以 build.sh 产物为准提交生成物,与其他 cc_*.ct 同例。)

`compiler/ctc.sh`:mode case 加 `dep`;分派臂(镜像 doc 臂):

```sh
    dep)
        "$DIR/build.sh" >/dev/null
        TMP=$(mktemp /tmp/ctron_dep.XXXXXX)
        sed -e "s|ANCHORINPUT|$IN|" -e "s|ANCHORLANG|$DIAGLANG|" "$DIR/build/cc_dep.ct" > "$TMP"
        "$HOST" run "$TMP"
        rc=$?
        ;;
```

用法行与头部注释补 `./ctc.sh dep <Ctron.ctcl>`。

`compiler/native.sh`:发射行加 `"$DIR/ctc.sh" emit "$DIR/build/cc_dep.ct" "$TMP/ctron_dep.c" > /dev/null`,链接行按 ctron-fmt 同式(纯解释器,无 GUI shim)产 `bin/ctron-dep`。检查 `install.sh` 二进制列举面,显式清单则补 `ctron-dep`。

- [ ] **Step 3: 验证(验收主锚:lock 二跑稳定)**

```bash
compiler/build.sh > /dev/null
compiler/ctc.sh dep tests/modules/dep_path_ok/Ctron.ctcl
cp tests/modules/dep_path_ok/Ctron.lock /tmp/lock1
compiler/ctc.sh dep tests/modules/dep_path_ok/Ctron.ctcl
cmp tests/modules/dep_path_ok/Ctron.lock /tmp/lock1 && echo "二跑稳定 ✓"
cat tests/modules/dep_path_ok/Ctron.lock
shasum -a 256 tests/modules/dep_path_ok/libmath/libmath.ct   # 与 lock digest 人工核对(摘要含长度前缀则用临时脚本重算)
```

期望:lock 生成、两次逐字节相等、digest 与手算一致、`version = "0.1.0"`(依赖清单 version 键)。再跑 `compiler/test/suite.py` 零回归。

- [ ] **Step 4: 提交**

```bash
git add compiler/src/driver_dep.ct compiler/build/cc_dep.ct compiler/build.sh compiler/ctc.sh compiler/native.sh install.sh
git commit -m "feat(pkg): T49-4 ctron-dep 依赖解析驱动——lock 内容寻址生成+钉定优先/已安装/registry 三级解析+E5052/E5053 校验,ctc.sh dep 同面"
```

---

### Task 5: workspace 语义(ctron-dep 成员并集 + build 项目模式)

**Files:**
- Modify: `compiler/src/driver_dep.ct`(workspace 检测 + 成员 deps 并集 + 根共享 lock)
- Modify: `ctron`(`cmd_build_proj` workspace 臂)
- Test: `tests/modules/ws_ok/`(workspace 用例,验收锚)

**Interfaces:**
- Consumes: Task 4 的解析主流程。
- Produces: `dep_ws_members(t, diags) -> List[Str]` 平表 `[名, path]`(缺 path → E5051;重复名 → E5045,含首次行号);workspace 根语义 = 清单有 `workspace {}` 块 → 逐成员解析其清单 deps,并集去重入根 `Ctron.lock`;成员构建 = `ctron build`(项目模式)在根清单检出 workspace 后逐成员子构建。

- [ ] **Step 1: driver_dep workspace 接线**

`dep_ws_members` / `dep_has_workspace` 两 fn(扫 `member`/`workspace` 键词,`pkg_ctcl_at`+`pkg_ctcl_pre`+`pkg_plugin_field` 复用;member 重复名检行号 = 索引前 `'\n'` 计数)。`dep_resolve_project` 改:清单有 workspace 块 → 成员清单逐个 `dep_resolve_project(<mfdir>/<path>/Ctron.ctcl)` 内联展开(递归深度 1,member 嵌 workspace = E5051 `workspace 嵌套不允许`),条目并集走第 6 步同名去重防线;lock 落根目录。摘要行加 `成员 M 个`。

- [ ] **Step 2: ctron build workspace 臂**

`ctron` 的 `cmd_build_proj` 在 `NAME=` 提取后插:

```sh
    if grep -q '^workspace' Ctron.ctcl 2>/dev/null; then
        WSPATHS=$(awk '/^member "/{n=$0; sub(/^member "/,"",n); sub(/".*$/,"",n); m=n} /^[[:space:]]*path[[:space:]]*=[[:space:]]*"/{if(m!=""){v=$0; sub(/.*= *"/,"",v); sub(/".*$/,"",v); print m" "v; m=""}}' Ctron.ctcl)
        for WS in $WSPATHS; do
            MDIR=${WS%% *}
            echo "ctron: workspace 成员 $MDIR"
            (CDPATH= cd -- "$MDIR" && "$0" build) || exit 1
        done
        exit 0
    fi
```

- [ ] **Step 3: workspace 用例夹具**

`tests/modules/ws_ok/Ctron.ctcl`(根):

```text
pkg {
    manifest_version = 1
    name = "wsdemo"
    version = "0.1.0"
}

workspace {
}

member "app" {
    path = "app"
}

member "libm" {
    path = "libm"
}
```

`tests/modules/ws_ok/libm/Ctron.ctcl`:

```text
pkg {
    manifest_version = 1
    name = "libm"
    version = "0.1.0"
}
```

`tests/modules/ws_ok/libm/libm.ct`:

```ctron
pub fn mul(a: I32, b: I32) -> I32 {
    return a * b
}
```

`tests/modules/ws_ok/app/Ctron.ctcl`:

```text
pkg {
    manifest_version = 1
    name = "wsapp"
    version = "0.1.0"
}

dep "libm" {
    path = "../libm"
}
```

`tests/modules/ws_ok/app/src/main.ct`:

```ctron
use libm.{mul}

fn main() -> I32 {
    println(mul(6, 7).to_string())
    return 0
}
```

- [ ] **Step 4: 验证(验收锚:workspace 用例)**

```bash
compiler/ctc.sh dep tests/modules/ws_ok/Ctron.ctcl        # 根 lock:libm 条目(path = "../libm",source = "path")
compiler/ctc.sh check tests/modules/ws_ok/app/src/main.ct # 成员 dep 探针 ../libm 命中
grep -q 'path = "../libm"' tests/modules/ws_ok/Ctron.lock && echo "workspace 并集 ✓"
```

- [ ] **Step 5: 提交**

```bash
git add compiler/src/driver_dep.ct ctron tests/modules/ws_ok
git commit -m "feat(pkg): T49-5 workspace 语义——成员 deps 并集+根共享 lock(E5051 防线)+ctron build 逐成员构建,ws_ok 用例"
```

---

### Task 6: ctron add/publish/lock 骨架转实 + e2e(T43 对齐)

**Files:**
- Modify: `ctron`(usage/help + `sha256_file` 助手 + `add`/`publish` 骨架臂转实 + 新 `lock` 臂 + `cmd_build_proj` dep 钩子)
- Modify: `ctron.ps1`(同文镜像 T43 惯例)
- Modify: `compiler/src/driver_run.ct`(W8902 出路文案对齐实装名,链描述补 dep 段)
- Test: `tests/pkg/run.sh`(e2e:publish → add → lock 二跑稳定 → 探针消费 → 篡改 E5053);`compiler/test/` 内 ctron_smoke 的 add/publish 骨架断言腿同步转实口径

**Interfaces:**
- Consumes: Task 4 `ctron-dep`、Task 5 workspace 臂、T43 骨架(040b80ef:`add_validate` 参数校验保留复用)。
- Produces: 用户命令面——`ctron publish`(当前目录单文件包发布到本地 registry,版本不可覆盖)/ `ctron add <name>[@<ver>]`(写 dep 块 + 装 `pkgs/<名>/{<名>.ct,Ctron.ctcl}` + 刷新 lock)/ `ctron lock`(= ctron-dep 直调)。W8902 出路文案从 `ctron pkg add` 对齐为 `ctron add`(T43 留债销账)。

- [ ] **Step 1: usage/help + W8902 对齐**

usage() 的 add/publish 两行改为实装口径:

```sh
  ctron publish               发布当前目录单文件包到本地 registry(CTRON_REGPATH,缺省 ~/.ctron/registry)
  ctron add <name>[@<ver>]    装包:写 dep 块 + pkgs/ 安装 + 刷新 Ctron.lock
  ctron lock                  解析依赖并生成/校验 Ctron.lock(内容寻址)
```

help_cmd 的 `add)`/`publish)` 文案同步转实口径,新增 `lock)`。`compiler/src/driver_run.ct` W8902 行改:

```ctron
                    println("W8902: 包 " + lmisses[mi] + " 未命中解析链(项目目录→dep→lib→pkgs→deps);registry 包试: ctron add " + lmisses[mi])
```

ctron_smoke 中断言 add/publish 骨架消息的腿同步改断言实装消息。

- [ ] **Step 2: add/publish 骨架臂转实 + lock 臂**

`die2` 定义后加助手:

```sh
sha256_file() {
    if command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1" | cut -d' ' -f1; else sha256sum "$1" | cut -d' ' -f1; fi
}
```

`add)` 臂:`add_validate "$1"` 之后原「骨架」echo/exit 整段替换为实装体(变量沿用骨架的 ADD_NAME/ADD_REQ):

```sh
        [ -f Ctron.ctcl ] || die2 "add: 当前目录缺 Ctron.ctcl(项目模式)"
        REG=${CTRON_REGPATH:-$HOME/.ctron/registry}
        [ -d "$REG/$ADD_NAME" ] || die2 "add: registry 无包 $ADD_NAME(registry: $REG;试: ctron publish)"
        if [ -z "$ADD_REQ" ]; then
            ADD_VER=$(ls "$REG/$ADD_NAME" | sort -t. -k1,1n -k2,2n -k3,3n | tail -1)
        else
            case $ADD_REQ in
                *.*.*) ADD_VER=$ADD_REQ ;;
                *)
                    ADD_VER=$(for v in $(ls "$REG/$ADD_NAME"); do case $v in "$ADD_REQ"|"$ADD_REQ".*) echo "$v";; esac; done | sort -t. -k1,1n -k2,2n -k3,3n | tail -1)
                    ;;
            esac
        fi
        [ -n "$ADD_VER" ] || die2 "add: $ADD_NAME 无满足约束 $ADD_REQ 的版本"
        [ -f "$REG/$ADD_NAME/$ADD_VER/module" ] || die2 "add: registry 损坏:缺 module($ADD_NAME@$ADD_VER)"
        if grep -q "dep \"$ADD_NAME\"" Ctron.ctcl; then
            echo "ctron: dep \"$ADD_NAME\" 已在清单(不重复追加)" >&2
        else
            printf '\ndep "%s" {\n    version = "%s"\n}\n' "$ADD_NAME" "$ADD_VER" >> Ctron.ctcl
        fi
        mkdir -p "pkgs/$ADD_NAME"
        cp "$REG/$ADD_NAME/$ADD_VER/module" "pkgs/$ADD_NAME/$ADD_NAME.ct"
        cp "$REG/$ADD_NAME/$ADD_VER/manifest" "pkgs/$ADD_NAME/Ctron.ctcl"
        echo "ctron: 已安装 pkgs/$ADD_NAME/($ADD_NAME@$ADD_VER)"
        [ -x "$BIN/ctron-dep" ] || die2 "add: 缺 ctron-dep(重装工具链;或试: ctc.sh dep Ctron.ctcl)"
        "$BIN/ctron-dep" run "$PWD/Ctron.ctcl" || exit 1
        ;;
    lock)
        [ -f Ctron.ctcl ] || die2 "lock: 当前目录缺 Ctron.ctcl"
        [ -x "$BIN/ctron-dep" ] || die2 "lock: 缺 ctron-dep(重装工具链;或试: ctc.sh dep Ctron.ctcl)"
        "$BIN/ctron-dep" run "$PWD/Ctron.ctcl"
        ;;
```

`publish)` 臂:骨架的报告段之后(exit 2 之前)替换为实装体(沿用骨架已取的 PUB_NAME/PUB_VER;`PUB_VER` 骨架缺省 "0.0.0" 改为缺省即报错,semver 门收紧):

```sh
        [ -n "$PUB_VER" ] || die2 "publish: 清单缺 version"
        echo "$PUB_VER" | grep -Eq '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-[0-9A-Za-z.-]+)?(\+[0-9A-Za-z.-]+)?$' || die2 "publish: version '$PUB_VER' 不符严格 semver"
        [ -f src/main.ct ] || die2 "publish: v0 registry 单文件包,需 src/main.ct"
        PN=$(find src -name '*.ct' | wc -l | tr -d ' ')
        [ "$PN" = "1" ] || die2 "publish: v0 registry 单文件包(src/ 下有 $PN 个 .ct);多文件 tar 志向"
        REG=${CTRON_REGPATH:-$HOME/.ctron/registry}
        PTGT="$REG/$PUB_NAME/$PUB_VER"
        [ -e "$PTGT" ] && die2 "publish: $PUB_NAME@$PUB_VER 已发布(版本不可覆盖)"
        mkdir -p "$PTGT" || exit 2
        cp Ctron.ctcl "$PTGT/manifest"
        cp src/main.ct "$PTGT/module"
        sha256_file "$PTGT/module" > "$PTGT/sha256"
        echo "ctron: 已发布 $PUB_NAME@$PUB_VER → $PTGT(sha256:$(cat "$PTGT/sha256"))"
        ;;
```

顶层分派 `case $CMD in run|check|...` --help 拦截行与 usage 助记行同步加 `lock`。`ctron.ps1` 同文镜像(add/publish/lock 三臂 + sha256 助手 Get-FileHash -Algorithm SHA256)。`cmd_build_proj` dep 钩子同原计划。

- [ ] **Step 3: e2e 脚本**

`tests/pkg/run.sh`(全文见下,可执行位):

```sh
#!/bin/sh
# T49 本地 registry 全链路:publish → add → lock 二跑稳定 → dep 探针消费 → 篡改 E5053
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CTC="$ROOT/compiler/ctc.sh"
export CTRON_STDPATH="$ROOT/lib/std"
T=$(mktemp -d /tmp/ctron_pkg_e2e.XXXXXX)
trap 'rm -rf "$T"' EXIT
fails=0
ck() { if [ "$1" = 0 ]; then echo "ok: $2"; else echo "FAIL: $2"; fails=$((fails+1)); fi; }

export CTRON_REGPATH="$T/reg"
mkdir -p "$T/libmath/src"
printf 'pkg {\n    manifest_version = 1\n    name = "libmath"\n    version = "1.0.0"\n}\n' > "$T/libmath/Ctron.ctcl"
printf 'pub fn add(a: I32, b: I32) -> I32 {\n    return a + b\n}\n' > "$T/libmath/src/main.ct"
(cd "$T/libmath" && "$ROOT/ctron" publish) >/dev/null 2>&1; ck $? "publish 落盘"
[ -f "$CTRON_REGPATH/libmath/1.0.0/sha256" ]; ck $? "sha256 产物在"
(cd "$T/libmath" && "$ROOT/ctron" publish) >/dev/null 2>&1; [ $? -ne 0 ]; ck $? "二次发布被拒(版本不可覆盖)"

mkdir -p "$T/app/src"
printf 'pkg {\n    manifest_version = 1\n    name = "app"\n    version = "0.1.0"\n}\n' > "$T/app/Ctron.ctcl"
printf 'use libmath.{add}\n\nfn main() -> I32 {\n    println(add(2, 3).to_string())\n    return 0\n}\n' > "$T/app/src/main.ct"
(cd "$T/app" && "$ROOT/ctron" add libmath >/dev/null 2>&1); ck $? "add 装包"
grep -q 'dep "libmath"' "$T/app/Ctron.ctcl"; ck $? "dep 块写入清单"
[ -f "$T/app/pkgs/libmath/libmath.ct" ] && [ -f "$T/app/pkgs/libmath/Ctron.ctcl" ]; ck $? "安装位 module+manifest"
[ -f "$T/app/Ctron.lock" ]; ck $? "lock 生成"
cp "$T/app/Ctron.lock" "$T/lock1"
(cd "$T/app" && "$ROOT/ctron" lock >/dev/null 2>&1)
cmp -s "$T/lock1" "$T/app/Ctron.lock"; ck $? "lock 二跑稳定(内容寻址)"
"$CTC" check "$T/app/src/main.ct" >/dev/null 2>&1; ck $? "dep 探针编译消费"
printf '\n// tampered\n' >> "$T/app/pkgs/libmath/libmath.ct"
"$ROOT/compiler/bin/ctron-dep" run "$T/app/Ctron.ctcl" >/dev/null 2>&1; [ $? -ne 0 ]; ck $? "篡改被拦(rc=1)"
[ -x "$ROOT/compiler/bin/ctron-dep" ] && "$ROOT/compiler/bin/ctron-dep" run "$T/app/Ctron.ctcl" 2>&1 | grep -q E5053; ck $? "E5053 诊断在"

if [ $fails -eq 0 ]; then echo "tests/pkg: 全链路绿"; exit 0; fi
echo "tests/pkg: $fails 失败"; exit 1
```

- [ ] **Step 4: 验证 + smoke 挂钩**

```bash
chmod +x tests/pkg/run.sh && tests/pkg/run.sh
```

`compiler/test/smoke.sh` 末段(modules 夹具区)追加四行:

```sh
"$COMP/ctc.sh" check "$ROOT/tests/modules/dep_path_ok/src/main.ct" > /dev/null 2>&1   # T49 dep 探针正例
"$COMP/ctc.sh" check "$ROOT/tests/modules/dep_mutex_neg/src/main.ct" > /dev/null 2>&1 && SM=$((SM+1))  # 负例须失败(按 smoke 既有计分口径接入)
"$COMP/ctc.sh" dep "$ROOT/tests/modules/ws_ok/Ctron.ctcl" > /dev/null 2>&1
sh "$ROOT/tests/pkg/run.sh" > /dev/null 2>&1
```

(以 smoke.sh 现场计分/失败口径为准接入,保持 171+ 计数格式。)

- [ ] **Step 5: 提交**

```bash
git add ctron tests/pkg/run.sh compiler/test/smoke.sh
git commit -m "feat(pkg): T49-6 ctron pkg publish/add/lock 本地 registry 面实装(T43 骨架转实,零网络)+build dep 钩子+e2e 全链路"
```

---

### Task 7: 文档 + 全量门禁 + 落库

**Files:**
- Modify: `docs/spec/02-names-modules.md`(§2.7 落地注)
- Modify: `docs/superpowers/plans/2026-09-28-spec-gap-execution-plan.md`(T49 状态回写 + 台账)
- Modify: `docs/superpowers/specs/2026-09-16-config-language-v1.md`(若 Task 1 后有口径微调)

- [ ] **Step 1: §2.7 落地注**

`docs/spec/02-names-modules.md` §2.7 修订注后追加:

```markdown
> **落地注(2026-10-06 T49)**:依赖解析 v0.6 已实装——`Ctron.lock`(内容寻址,
> `lock {}` + `pkg "名" { version/source/path|git/rev/digest }`,CTCL 规范形态,
> `ctron-dep` 生成,二跑稳定)、workspace(`workspace {}` + `member "名" { path }`)、
> `ctron pkg publish/add/lock` 本地 registry 面(纯目录协议,`CTRON_REGPATH`,零网络;
> 解析链② = dep 表探针,⑤ = `deps/<pkg>.ctart` 回落位保留待 S2a)。设计:
> docs/superpowers/plans/2026-10-06-t49-lockfile-workspace-add-publish.md
```

- [ ] **Step 2: spec-gap 台账回写**

T49 状态行改 `✅ 已完成(2026-10-06;...)` 附实况一句 + 状态台账表 T49 行翻 ✅;记残债:①git 形无网络获取(rev 钉定已锁,fetch 志向);②多文件包 tar 志向(publish 单文件 fail-closed);③真 registry 待用户裁决(T43 坑位);④ctcl_check workspace 键注册随 L2 三线迁移;⑤lock 解析三线对拍(ctron-dep 自扫,C/Rust 不读 lock)。

- [ ] **Step 3: 全量门禁**

```bash
# worktree 内:
compiler/build.sh
compiler/test/smoke.sh                     # 171+ 新口径全绿
compiler/test/suite.py                     # 100 例零回归
python3 tests/manifest/run.py && python3 tests/manifest/diff.py
(cd compiler-rust && cargo test)           # 13 套全绿
(cd compiler-c && make test)
compiler/native.sh                         # bin/ctron-dep 出厂
tests/pkg/run.sh                           # e2e 全链路绿
# 主树合并前核(worktree 全绿后):
cd /Users/zyj/Zturn/Ctron && compiler/test/suite.py
```

- [ ] **Step 4: 合并落库(warzone 串联核)**

```bash
cd /Users/zyj/Zturn/Ctron
git log --oneline -1          # 核 HEAD 归属(机刷竞态核)
git merge --no-ff t49 -m "merge: T49 lockfile 内容寻址+workspace+ctron pkg add/publish 本地 registry 实装(7 批)"
git worktree remove .worktrees/t49   # 必先 cd 出 worktree(坑位在册)
```

合并后主树复跑 `compiler/test/smoke.sh` + `compiler/test/suite.py` 确认,然后更新记忆(`ctron-spec-gap-plan.md` 41→42/53)。

---

## Self-Review

1. **Spec 覆盖**:验收三锚齐——deps 解析锚(三互斥 E5049 三线已注册 + bootstrap 镜像 = Task 1/2;semver 约束 = Task 2 工具面 + Task 6 e2e)/ lock 二跑稳定内容寻址 = Task 4 Step 3 + Task 6 e2e ③ / workspace 用例 = Task 5。范围三面齐——parse_pkg.ct(deps 探针/扫描 = Task 2/3)、ctpkg registry 本地协议(ctron pkg 面 = Task 6)、workspace 段(Task 1/5)。坑位兑现——`deps/<pkg>.ctart` 回落位零触碰(Task 3 探针在链②不触链⑤);T43「不接真 registry」= 全程目录协议。
2. **占位扫描**:driver_dep.ct 的解析主流程以步骤 1 结构伪代码+精确诊断文案给出(函数级),实现时按 Global Constraints 语法纪律落地——这是本仓既有的「锚点+文案逐字、代码实测定形」惯例(pkg.c/check.rs 的插入点同理须现场读块分发区)。无 TBD/类似 Task N 式空洞。
3. **类型一致性**:`pkg_deps_of` 4 槽平表(name/form/value/extra)在 Task 2 产出、Task 3 `pkg_dep_slot` 消费、Task 4 复用;条目 7 槽(name/version/source/path/git/rev/digest)在 Task 4 定义、Task 5 并集复用;E5051-5053 文案在 Task 1 立表、Task 4/5/6 引用同串。✓
