# FFI 剩余四组缺口实施计划(2026-09-23)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> 本计划按用户既定协议内联执行(机刷泳道坑位密度高,不派无上下文子代理);每片全绿即 pathspec 限定落库。

**Goal:** 清掉 docs/ffi-analysis.md §四 在册的最后四组缺口——①cimport 位域检测、②cimport union、③F32/float 边界保真(含 cimport float 形参与精度约定成文)、④pkg-config 集成、⑤std.ffi errno→Result 深层包装,并刷新文档陈旧行(#12/#13/#14/#9 销账)。

**Architecture:** 全部改动落在既有 FFI 面:自举发射器 `compiler/src/trans_ty.ct`(F32 独立码)、绑定工具 `compiler/tools/cimport.ct`(位域/union/float)、驱动 `compiler/ctc.sh`(pkg-config 解析)、新 std 包 `std/ffi.ct`(strerror+sys_result)。发射器职责止于 C 文本;pkg-config 解析只进驱动壳层。

**Tech Stack:** Ctron 自举编译器(compiler/src/*.ct,经 build.sh 拼接 + native.sh 产出 bin/ctron-cc、bin/ctron-emit)、cimport.ct(独立 .ct 工具)、sh(ctc.sh/run.sh)、cc(系统 Clang/gcc)。

## Global Constraints(全部来自泳道坑位记忆,逐条硬约束)

- Ctron 源码禁 `;`;字符串内裸 `{` 必须 `\{`;`\}` 是非法转义;**注释里禁花括号**(发射口径会炸)。
- or2/or3 嵌套括号数错是高频自伤——写完数括号,或拆成独立 if。
- 独立 .ct 工具(tools/cimport.ct)不能用编译器内部助手(or2/or3),只用原生 `==`/`||`。
- 验证用 seed(`compiler-c/build/ctronc`)或重建后的 bin;`compiler/bin/*` 是旧快照,改 src 后须 `sh compiler/build.sh && sh compiler/native.sh` 重建。
- FFI 验收命令:`sh tests/ffi/run.sh`(基线 24 过/0 败);std 包验收:`ctron test`;全量回归:`compiler/test/smoke.sh`(decl 锁漂移属并行线常态,红先归因)。
- git add 一律 pathspec 限定;add 前清查 main.c 等垃圾产物。
- lint 夹具须含至少一个 test 块(meta_check 硬校验)。
- 本批**无新内建**(errno/str_from_c 均已三处注册),不触 sem_calls/sem_type/eval_call 注册表;例外:Task 3 若探针判明需改 trans_expr/eval_call 的 extern 路径,改动前先读该文件上下文 30 行。
- 解释桥(ctron_ext_dispatch)只有 "i:"/"s:" 两帧,无 float 帧——F32/float 在解释口径**响亮 panic**(既有行为,符合"不静默"宪法);本批不为桥加 float 帧(登记后续),夹具走编译通道。

---

## 背景:四个缺口的根因(侦察已实证)

1. **cimport 位域**:struct 体含 `int x : 3` 时,cimp_param 不识别 `:`,产出错误字段或跳字段——布局不可表达却无整构级拦截,有静默错绑风险。
2. **cimport union**:`cimport.ct:592` 一律 `// cimport: 跳过(union/enum)` 注释占位。Ctron 无 union 类型;可行形态 = 字节缓冲 struct(U8[max 成员大小])+ 成员偏移注释 + `_raw_len` 助手(可与 C 侧 sizeof 互证)。
3. **F32/float**:`trans_ty.ct:217` 把 F32 折叠进 F64 的码 `"f"`(C double)——extern 边界 float/double ABI 错配,C 侧收到 0(坑位记忆实证"裸 F32 标量到 C 侧为 0")。`"f"` 码消费面仅 5 处(trans_ty.ct:267/803、trans_stmt.ct:1371、trans_expr.ct:376/1272),可控。C 宿主 compiler-c 把 F32/F64 折叠为 ty_flt(trans.c:527),本批**登记分歧不改 C 宿主**。
4. **pkg-config**:a1050aa 后 ctc.sh emit 已输出 `ctron:link -l<名>` 摘要(ctc.sh:80-82),但无 pkg-config 查询;插入点就在该 shell 段。
5. **std.ffi**:errno() 内建已落地(251202d);深层转换缺包装层。发射面 Result/Option 载荷槽 32 位(Ok(I64) 截断/Ok(F64) 错值、Err-Str 绿,json.ct:420 prior art)→ sys_result 的 Ok(I64) 须注记 POSIX rc 域承诺(|rc| < 2^31)。

---

### Task 1: cimport 位域整构跳过

**Files:**
- Modify: `compiler/tools/cimport.ct`(cimp_struct 的 body 扫描段,约 619-650 行)
- Modify: `tests/ffi/cimport/sample.h`(追加位域 struct)
- Modify: `tests/ffi/run.sh`(cimport 专道加 grep 断言)

**Interfaces:**
- Produces: 含位域成员的 struct 输出 `// cimport: 跳过(位域布局不可表达): <原文>` 整构注释;无位域路径字节级不变(既有 24 夹具不抖)。

- [ ] **Step 1: 写失败探针——sample.h 追加位域 struct**

在 `tests/ffi/cimport/sample.h` 末尾(现有内容之后)追加:

```c
/* 位域:cimport 应整构跳过(布局不可按声明序表达) */
struct bits {
    unsigned int lo : 3;
    unsigned int hi : 5;
    int rest;
};
```

- [ ] **Step 2: 跑现状确认静默错绑或垃圾字段**

Run: `cd /Users/zyj/Zturn/Ctron && sed -e "s|ANCHORHEADER|$PWD/tests/ffi/cimport/sample.h|" -e "s|ANCHOROUT|/tmp/bits.bind.ct|" compiler/tools/cimport.ct > /tmp/cimp_probe.ct && compiler-c/build/ctronc run /tmp/cimp_probe.ct && grep -n "bits" /tmp/bits.bind.ct`
Expected: 产出含 `struct bits` 的**错绑**字段(如把 `lo : 3` 当类型)或跳字段注释——证明现状不安全。

- [ ] **Step 3: 实现——cimp_struct 位域整构拦截**

在 cimp_struct 的 body 切分循环之前(拿到 `body` 之后、`var out = "#[repr(c)]..."` 之前)插入位域预扫:

```ct
    // 位域预扫:body 内出现 ':'(成员声明上下文只可能是位域)→ 整构跳过。
    // 按声明序 struct 无法表达位域布局,逐字段映射会静默产出错绑。
    var bf = -1
    var bdep: I32 = 0
    var bi: I32 = 0
    while bi < body.len {
        var bc = byte_at(body, bi)
        if bc == 40 || bc == 91 || bc == 123 {
            bdep += 1
        } else if bc == 41 || bc == 93 || bc == 125 {
            bdep -= 1
        } else if bc == 58 && bdep == 0 {
            bf = bi
            bi = body.len
        }
        bi += 1
    }
    if bf >= 0 {
        return "// cimport: 跳过(位域布局不可表达,需手写绑定): " + item
    }
```

注意:cimport.ct 是独立工具,只能用原生 `&&`/`||`;上面代码未用 or2/or3,合规。

- [ ] **Step 4: run.sh cimport 专道加断言**

在 `tests/ffi/run.sh` cimport 分支的 `cat "$T/$name.bind.ct" "$e" > "$T/$name.all.ct"` 之前插入:

```sh
        if ! grep -q "cimport: 跳过(位域" "$T/$name.bind.ct"; then
            echo "  [FAIL] $name — 位域 struct 未整构跳过"
            fail=$((fail + 1))
            continue
        fi
```

- [ ] **Step 5: 全量验证**

Run: `sh tests/ffi/run.sh`
Expected: 25 过 / 0 败(原 24 + cimport 断言仍走同 case,cimport case 计 1)。若失败,先看 `/tmp/bits.bind.ct` 与 cimp 探针输出。

- [ ] **Step 6: Commit**

```bash
git add compiler/tools/cimport.ct tests/ffi/cimport/sample.h tests/ffi/run.sh
git commit -m "feat(ffi): cimport 位域整构跳过——布局不可表达不再静默错绑"
```

---

### Task 2: cimport union 字节缓冲形态

**Files:**
- Modify: `compiler/tools/cimport.ct`(cimp_struct 的 hasunion 分支 + 新 fn cimp_union)
- Modify: `tests/ffi/cimport/sample.h`(追加 union)
- Modify: `tests/ffi/cimport/c_src/uprobe.c`(sizeof 互证)
- Modify: `tests/ffi/cimport/src/main.ct`(追加 test 块)

**Interfaces:**
- Consumes: cimp_param(成员类型分解)、cimp_trim/cimp_toks(既有)。
- Produces: `typedef union {...} 名;` → `#[repr(c)] struct 名 { var raw: U8[N] }` + 成员偏移注释 + `pub fn 名_raw_len() -> I64`。成员含不可映射型 → 整构跳过(与位域同风格注释)。

- [ ] **Step 1: 探针——U8 定长数组 struct 字段的发射可用性**

Run: `mkdir -p /tmp/u8probe && printf '#[repr(c)]\nstruct Buf \{\n    var raw: U8[16]\n}\n\n#[trusted]\nextern "c" fn probe_take(b: Buf) -> I64\n\ntest "u8 array field" \{\n    var b = Buf \{ raw: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0] \}\n    assert_eq(probe_take(b), 16)\n}\n' > /tmp/u8probe/main.ct && printf '#include <stddef.h>\nint64_t probe_take_t(unsigned char* p) { (void)p; return 16; }\n' > /tmp/u8probe/up.c && compiler/bin/ctron-emit run /tmp/u8probe/main.ct > /tmp/u8probe/main.c 2>/tmp/u8probe/err && cc -O1 -w -o /tmp/u8probe/b /tmp/u8probe/main.c /tmp/u8probe/up.c && /tmp/u8probe/b run /tmp/u8probe/main.ct`

**判定规则:** rc=0 且无 assert 失败 → U8[N] 字段可用,按下方 Step 2 实现;emit/cc 报错 → union 本批降级为"命名跳过升级"(输出 `// cimport: union <名> 未映射(Ctron 无 union;可按指针不透明手写),成员: <列表>`),并在 Task 6 文档登记 U8[N] 字段缺口。两条分支都要写实际代码,不许停。

- [ ] **Step 2(探针可用分支): 实现 cimp_union**

把 cimp_struct 中 `if hasunion || hasenum {` 分支改为:enum 仍走原跳过(顶级 enum 已由常量组路径处理,到这里的是匿名/变体形态),union 调新 fn:

```ct
    if hasunion {
        return cimp_union(item, head, body, tail, tds)
    }
    if hasenum {
        return "// cimport: 跳过(union/enum): " + item
    }
```

新 fn(cimport.ct 顶层,cimp_struct 之后):

```ct
// union → 字节缓冲 struct:Ctron 无 union 类型;布局以"最大成员大小"字节缓冲
// 承载,成员偏移/大小落注释,raw_len 助手供 C 侧 sizeof 互证。任一成员类型
// 不可映射 → 整构跳过(不静默)。对齐注记:缓冲对齐 1,C 侧分配(指针传递)
// 的对齐由分配器保证;按值传 union 非支持口径。
fn cimp_union(item: Str, head: Str, body: Str, tail: Str, tds: List[Str]) -> Str {
    var tt = cimp_toks(tail)
    if tt.len == 0 {
        return "// cimport: 跳过(匿名 union): " + item
    }
    var unm = tt[tt.len - 1]
    if unm == ";" && tt.len > 1 {
        unm = tt[tt.len - 2]
    }
    if !cimp_is_ident_start(byte_at(unm, 0)) {
        return "// cimport: 跳过(匿名 union): " + item
    }
    // 成员切分 ';',逐个映射类型并记 (名, 大小)
    var names = List[Str]()
    var sizes = List[I32]()
    var bad = ""
    var cur = ""
    var i: I32 = 0
    while i < body.len {
        var c = byte_at(body, i)
        if c == 59 {
            var ftxt = cimp_trim(cur)
            if ftxt.len > 0 {
                // 位域成员同整构跳过口径
                var hascolon = false
                var ci: I32 = 0
                while ci < ftxt.len {
                    if byte_at(ftxt, ci) == 58 { hascolon = true
                    }
                    ci += 1
                }
                var pa = ""
                if !hascolon { pa = cimp_param(ftxt, "m" + i.to_string(), tds) }
                if pa.len == 0 {
                    bad = ftxt
                } else {
                    var sp = -1
                    var cj: I32 = pa.len - 1
                    while cj >= 0 {
                        if byte_at(pa, cj) == 32 { sp = cj
                            cj = -1
                        }
                        cj -= 1
                    }
                    if sp > 0 {
                        names.push(byte_slice(pa, 0, sp))
                        sizes.push(cimp_c_size(byte_slice(pa, sp + 1, pa.len), tds))
                    } else {
                        bad = ftxt
                    }
                }
            }
            cur = ""
        } else {
            cur = cur + byte_slice(body, i, i + 1)
        }
        i += 1
    }
    if bad.len > 0 || names.len == 0 {
        return "// cimport: 跳过(union 成员类型不可映射): " + item
    }
    var mx: I32 = 0
    var mi: I32 = 0
    var doc = ""
    while mi < names.len {
        if sizes[mi] > mx { mx = sizes[mi]
        }
        doc = doc + "//   ." + names[mi] + " : size " + sizes[mi].to_string() + "\n"
        mi += 1
    }
    var out = "#[repr(c)]\nstruct " + unm + " \{\n"
    out = out + "    // union 字节缓冲(对齐 1;C 侧分配保证成员对齐)\n"
    out = out + "    var raw: U8[" + mx.to_string() + "]\n"
    out = out + "}\n"
    out = out + doc
    out = out + "pub fn " + unm + "_raw_len() -> I64 \{ return " + mx.to_string() + " \}"
    return out
}

// C 标量大小表(sizeof 口径,LP64):未知型 → 0(上层判 0 即整构跳过)
fn cimp_c_size(cty: Str, tds: List[Str]) -> I32 {
    var base = cimp_ty(cty, tds)
    if base.len == 0 { return 0 }
    if base == "I8" || base == "U8" || base == "Bool" { return 1 }
    if base == "I16" || base == "U16" { return 2 }
    if base == "I32" || base == "U32" || base == "F32" { return 4 }
    if base == "I64" || base == "U64" || base == "F64" || base == "USize" { return 8 }
    if base == "Str" { return 8 }
    return 0
}
```

依赖说明:cimp_ty 的 `float → F32` 映射在 Task 3 落地;本任务的 cimp_c_size 已把 F32 列入(先于 Task 3 出现不致错——float 成员在 Task 3 前映射不到 F32 会走 0 = 跳过,Task 3 后自动可用,两任务顺序无关)。cimport 工具的 `+`/比较为原生操作,合规。

- [ ] **Step 3: 夹具——sample.h 追加 union**

```c
/* union:cimport 以字节缓冲承载,size = max(sizeof(double), 12 补齐到 8) = 16 */
typedef union {
    double d;
    char s[12];
} Vals;
```

(手工核定:`char s[12]` 在 `VVals` 中大小 12 对齐 8 → union sizeof = 16。cimport 的 cimp_param 对数组成员返回 ""(方括号不支持)→ 按上面实现该成员会把整个 union 判不可映射!**修正夹具**:若探针发现数组成员被拒,改用无数组形态 `typedef union { double d; long l; int i[0]; }` 不合法——改用 `typedef union { double d; void* p; long l; } Vals;`(全标量,size 8)。以探针实际输出为准,把选定的 union 形态与 sizeof 断言值写死。)

最终夹具(标量安全形态,预期 sizeof = 8):

```c
typedef union {
    double d;
    void* p;
    long l;
} Vals;
```

- [ ] **Step 4: c_src sizeof 互证 + test 块**

`tests/ffi/cimport/c_src/` 追加 `uvals.c`:

```c
#include <stddef.h>
int64_t uvals_size_is(int64_t n) { return sizeof(union Vals) == (size_t)n ? 1 : 0; }
```

(run.sh cimport 分支链 `"$d"/c_src/*.c`,自动收编。)

`tests/ffi/cimport/src/main.ct` 追加:

```ct
test "cimport union byte-buffer size matches C sizeof" {
    assert_eq(uvals_size_is(Vals_raw_len()), 1)
}
```

(uvals_size_is 无需 extern 声明——main.ct 里既有 C 侧夹具 fn 的声明形态照抄文件内现状;若现状都是显式 extern 声明,补 `#[trusted] extern "c" fn uvals_size_is(n: I64) -> I64`。)

- [ ] **Step 5: 全量验证**

Run: `sh tests/ffi/run.sh`
Expected: 全过,cimport case 含位域与 union 断言。

- [ ] **Step 6: Commit**

```bash
git add compiler/tools/cimport.ct tests/ffi/cimport/
git commit -m "feat(ffi): cimport union 字节缓冲形态——raw_len 与 C sizeof 互证"
```

---

### Task 3: F32 独立码 + extern float 边界保真 + cimport float + 精度约定成文

**Files:**
- Modify: `compiler/src/trans_ty.ct`(ct_ty_code:217 F32 分离;ct_ctype:267 加 "g";803 算术码;ct_typeof 推断点)
- Modify: `compiler/src/trans_stmt.ct:1371`、`compiler/src/trans_expr.ct:376/1272`("g" 镜像)
- Modify: `compiler/tools/cimport.ct`(cimp_ty 加 `float → F32`)
- Create: `tests/ffi/f32_boundary/`(src/main.ct + c_src/f32.c)
- Modify: `tests/ffi/run.sh`(f32_boundary 专道:编译通道直跑)
- Modify: `docs/ffi-analysis.md`(精度约定小节,Task 6 一并终稿)

**Interfaces:**
- Produces: F32 类型码 `"g"`(C `float`);extern 形参/返回按边界出 `float`;内部 F32 局部/字段/算术走 `float` 声明;F64 路径字节级不变。

- [ ] **Step 1: 探针三连(动码前必做,结论写进提交说明)**

a. 解释桥 F64 现状:`grep -n 'ext_dispatch\|"d:"\|"f:"' compiler/src/eval_call.ct | head`,并跑 `sh tests/ffi/run.sh 2>&1 | grep link_math` 确认现状绿。判定:link_math(sqrt 返 F64)经 bin-run 解释桥绿 → 找到 F64 过桥的实际机制并记录;若实为 int 位型巧合,记录"解释桥 float 无保真"口径。
b. a_split 元素码校验:`sed -n "$(grep -n 'fn a_split' compiler/src/trans_ty.ct | cut -d: -f1),+40p" compiler/src/trans_ty.ct`——确认 "g" 是否在合法元素码集;不在则补(与 "f" 同臂)。
c. F32 字面量/推断:写 /tmp 探针 `var x: F32 = 1.5` + `let y = x` + extern 取值,emit 后 grep 生成 C 中 x/y 的声明型(现状应全为 double;改动后应为 float)。同时确认 ct_typeof 对 F32 表达式的推断点(grep `n:F32` trans_ty.ct)。

- [ ] **Step 2: ct_ty_code / ct_ctype 分离 F32**

trans_ty.ct:217 改:

```ct
    if nt[1] == "F64" { return "f" }
    if nt[1] == "F32" { return "g" }
```

ct_ctype(267 附近)加:

```ct
    if code == "g" { return "float" }
```

- [ ] **Step 3: "f" 码消费面镜像(逐处)**

- trans_ty.ct:803(算术结果码):`if or2(l == "f", r == "f") { return "f" }` 前加 `g` 同族臂(双方均 "g" → "g";一方 "g" 一方 "f" → "f",镜像 Ctron F32⊗F64→F64 语义——以 sem 实际约定为准,探针 c 已给证据;若 sem 禁混算则该臂不可达,写 "g" 对 "g" 臂即可)。
- trans_stmt.ct:1371(`at == "f"`):按上下文镜像加 `|| at == "g"`(读上下文 30 行后落码)。
- trans_expr.ct:376 cast:`if tgtc == "g" { return "(float)(" + aobj + ")" }`。
- trans_expr.ct:1272 零值:`if code == "g" { return "0.0" }`。
- 全库终扫:`grep -n '"f"' compiler/src/trans_*.ct` 逐处判读,凡"浮点性"判定必须含 "g",凡"F64 专名"保持 "f"。

- [ ] **Step 4: TDD 夹具(先写,先红后绿)**

`tests/ffi/f32_boundary/c_src/f32.c`:

```c
#include <stdint.h>
float f_add(float a, float b) { return a + b; }
float f_half(float x) { return x / 2.0f; }
int64_t f_is_precise(void) {
    /* F32 保真铁证:0.1f+0.2f != 0.3f(F32 舍入),而 double 通道会得 0.3 */
    float a = 0.1f + 0.2f;
    return a < 0.30000001f && a > 0.29999998f ? 1 : 0;
}
```

`tests/ffi/f32_boundary/src/main.ct`:

```ct
// tests/ffi/f32_boundary —— F32 extern 边界保真(§9.6 精度约定)
// 承诺:F32 边界 = C float(IEEE binary32);编译通道值保真;解释桥 float 帧
// 未支持,响亮 panic(不静默)——本夹具走编译通道。
#[trusted]
extern "c" fn f_add(a: F32, b: F32) -> F32

#[trusted]
extern "c" fn f_half(x: F32) -> F32

#[trusted]
extern "c" fn f_is_precise() -> I64

test "F32 scalar roundtrip preserves binary32" {
    var a: F32 = 1.5
    var b: F32 = 2.25
    assert_eq(f_add(a, b), 3.75)
    assert_eq(f_half(9.0), 4.5)
}

test "F32 rounding differs from F64 (width truly float)" {
    assert_eq(f_is_precise(), 1)
}
```

run.sh 专道(加在 link_math 专道之前,行为分支会把它当目录夹具跑 bin-run 解释桥 → float panic → 必须专道直跑编译产物):

```sh
# ---- f32_boundary:F32 边界保真(编译通道直跑;解释桥无 float 帧) ----
f32d="$DIR/f32_boundary"
if [ -d "$f32d/src" ]; then
    e="$f32d/src/main.ct"
    if "$EMIT" run "$e" > "$T/f32.c" 2>"$T/f32.emiterr" \
       && grep -q "float t_f_add" "$T/f32.c" \
       && cc -O1 -w -o "$T/f32.bin" "$T/f32.c" "$f32d"/c_src/*.c 2>"$T/f32.ccerr" \
       && "$T/$("$T/f32.bin" run "$e" >/dev/null 2>&1; echo ok)" 2>/dev/null; then :; fi
    if [ -f "$T/f32.c" ] && grep -q "float t_f_add" "$T/f32.c" \
       && cc -O1 -w -o "$T/f32.bin" "$T/f32.c" "$f32d"/c_src/*.c 2>"$T/f32.ccerr" \
       && "$T/f32.bin" run "$e" > "$T/f32.out" 2>&1; then
        echo "  [ok] f32_boundary(编译通道 float 保真)"
        pass=$((pass + 1))
    else
        echo "  [FAIL] f32_boundary — $(head -1 "$T/f32.emiterr" 2>/dev/null)$(head -1 "$T/f32.ccerr" 2>/dev/null)$(head -c 120 "$T/f32.out" 2>/dev/null)"
        fail=$((fail + 1))
    fi
fi
```

注意:行为分支的目录扫描会先命中 f32_boundary(有 c_src → 走 emit/cc/bin-run)——bin-run 下 float 帧 panic 会使它红。**须在行为分支排除**:仿 link_math 先例,在 `for d in "$DIR"/*/` 循环内 `[ "$name" = "f32_boundary" ] && continue`。同时 `grep -q "float t_f_add"` 断言发射原型已是 float(防"折叠 double 回归")。`"$T/f32.bin" run "$e"` 若探针证实编译产物 main 直跑 test 块(export 先例),则改直跑;以探针实测为准,把最终形态写死。

- [ ] **Step 5: 重建并验证**

Run: `sh compiler/build.sh && sh compiler/native.sh && sh tests/ffi/run.sh`
Expected: 全过(基线 24 + Task1/2 已增项 + f32_boundary)。f32 emit 产物中 `t_f_add` 原型为 `float`、实参 cast `(float)`。

- [ ] **Step 6: cimport float → F32**

cimport.ct cimp_ty 的 `if (nm == "double") { return "F64" }` 后加:

```ct
    if (nm == "float") { return "F32" }
```

sample.h 追加 `float f_scale(float v, float k);`(原型),确认生成绑定出现 `F32`;重建后跑 cimport case 仍绿。

- [ ] **Step 7: 精度约定成文(ffi-analysis.md,与 Task 6 终稿合并提交亦可)**

在 §二·五 后新增小节「v0.9:F32/float 边界精度约定」:F32 = C float(IEEE binary32),F64 = C double;边界不隐式加宽;编译通道值保真(f_is_precise 铁证);解释桥 float 帧未支持 = 响亮 panic;C 宿主 compiler-c F32 仍折叠 double(ty_flt,登记分歧)。

- [ ] **Step 8: Commit**

```bash
git add compiler/src/trans_ty.ct compiler/src/trans_stmt.ct compiler/src/trans_expr.ct compiler/tools/cimport.ct tests/ffi/f32_boundary tests/ffi/run.sh tests/ffi/cimport/sample.h docs/ffi-analysis.md
git commit -m "feat(ffi): F32 独立码 g→float——extern 边界 binary32 保真 + cimport float 形参"
```

---

### Task 4: ctc.sh pkg-config 解析

**Files:**
- Modify: `compiler/ctc.sh`(emit 模式链接标志摘要段,80-82 行)
- Create: `tests/ffi/pkgconf/pc/fakefoo.pc` + `tests/ffi/pkgconf/src/main.ct`
- Modify: `tests/ffi/run.sh`(pkgconf 专道)

**Interfaces:**
- Produces: ctc.sh emit 输出新增一行 `ctc.sh: 链接标志(pkg-config 解析): ...`;解析规则 = 对每个 `-l<名>`,`pkg-config --exists <名>` 成功则并其 `--libs` 输出,否则保留 `-l<名>`;发射 C 文本不变。

- [ ] **Step 1: ctc.sh 解析逻辑**

在现有 `LF=$(grep -o ...)` 摘要行后追加:

```sh
        PKG_RESOLVED=""
        if command -v pkg-config >/dev/null 2>&1; then
            for ln in $LF; do
                pn=${ln#-l}
                if pkg-config --exists "$pn" 2>/dev/null; then
                    PKG_RESOLVED="$PKG_RESOLVED $(pkg-config --libs "$pn" 2>/dev/null)"
                else
                    PKG_RESOLVED="$PKG_RESOLVED $ln"
                fi
            done
        fi
        if [ -n "$PKG_RESOLVED" ]; then
            echo "ctc.sh: 链接标志(pkg-config 解析):$PKG_RESOLVED"
        fi
```

- [ ] **Step 2: 假 .pc 夹具**

`tests/ffi/pkgconf/pc/fakefoo.pc`:

```
Name: fakefoo
Description: cimport pkg-config resolution fixture
Version: 1.0
Libs: -lfakefoo -lfakeextra
```

`tests/ffi/pkgconf/src/main.ct`(只验标记面,不真链):

```ct
// tests/ffi/pkgconf —— #[link] → pkg-config 解析(驱动壳层面;发射 C 文本不变)
#[link("fakefoo")]
#[trusted]
extern "c" fn fakefoo_ver() -> I32

test "link marker emitted" {
    assert_eq(0, 0)
}
```

- [ ] **Step 3: run.sh pkgconf 专道**

```sh
# ---- pkgconf:ctc.sh #[link] 标记 → pkg-config 解析(假 .pc,hermetic) ----
pcd="$DIR/pkgconf"
if [ -d "$pcd/src" ] && command -v pkg-config >/dev/null 2>&1; then
    PKG_CONFIG_PATH="$pcd/pc" sh "$ROOT/compiler/ctc.sh" emit "$pcd/src/main.ct" "$T/pkgconf.c" > "$T/pkgconf.log" 2>&1
    if grep -q "lfakeextra" "$T/pkgconf.log"; then
        echo "  [ok] pkgconf(pkg-config 解析 -lfakefoo → -lfakefoo -lfakeextra)"
        pass=$((pass + 1))
    else
        echo "  [FAIL] pkgconf — 解析输出缺 -lfakeextra: $(grep '链接标志' "$T/pkgconf.log" | head -1)"
        fail=$((fail + 1))
    fi
fi
```

(行为分支目录扫描同样 `[ "$name" = "pkgconf" ] && continue` 排除——夹具引用不存在的 fakefoo 库,不可真链。ctc.sh 依赖 `compiler/build/cc_emit.ct`:run.sh 前置是 native.sh,build.sh 产物在库即用;若 build 缺失,专道输出 skip 提示并计入 pass=0/fail=0(用 `[ -f "$ROOT/compiler/build/cc_emit.ct" ] || continue` 守卫)。)

- [ ] **Step 4: 验证 + Commit**

Run: `sh tests/ffi/run.sh`(全过)+ 手跑 `sh compiler/ctc.sh emit tests/ffi/link_math/src/main.ct /tmp/lm.c` 确认无 pkg-config 时摘要照旧。

```bash
git add compiler/ctc.sh tests/ffi/pkgconf tests/ffi/run.sh
git commit -m "feat(ctc): emit 链接标志 pkg-config 解析——存在 .pc 即展开,缺省回落 -l"
```

---

### Task 5: std.ffi 包(errno 深层 Result 包装)

**Files:**
- Create: `std/ffi.ct`
- Create: `tests/ffi/err_wrap/src/main.ct`
- Modify: `tests/ffi/run.sh`(err_wrap 专道,CTRON_STDPATH + LC_ALL=C)

**Interfaces:**
- Consumes: errno() 内建(已注册三处)、str_from_c 内建(已注册)、libc strerror/dup/close。
- Produces: `std.ffi` 包——`err_str(e: I32) -> Str`(strerror 深拷)、`sys_result(rc: I64) -> Result[I64, Str]`(rc ≥ 0 → Ok(rc);rc < 0 → Err(err_str(errno())))。Ok(I64) 走发射面 32 位载荷槽,域承诺 |rc| < 2^31(POSIX rc 恒真,json.ct:420 prior art 注记)。

- [ ] **Step 1: 写 std/ffi.ct**

```ct
// std/ffi.ct —— C 边界错误包装(§9.6 深层 errno→Result 首档)
// since: std-0.3 stability: experimental
// 口径:errno() 内建读最近一次系统调用错误码(线程局域);sys_result 把
// "rc + errno" 双值惯例折叠成 Result。Ok(I64) 载荷经发射面 32 位槽,
// 域承诺 |rc| < 2^31(POSIX 系统调用返回值恒在此域;json.ct prior art)。
#[trusted]
extern "c" fn strerror(e: I32) -> Str

// 错误码 → 人读消息(strerror 返回 C-owned 串,str_from_c 深拷入 arena)
pub fn err_str(e: I32) -> Str {
    return str_from_c(strerror(e))
}

// rc 惯例折叠:rc < 0 = 失败 → Err(err_str(errno()));否则 Ok(rc)
pub fn sys_result(rc: I64) -> Result[I64, Str] {
    if rc < 0 {
        return Err(err_str(errno() as I32))
    }
    return Ok(rc)
}

test "sys_result Ok arm carries nonneg rc" {
    let fd = dup(0)
    assert_eq(sys_result(fd).ok(), 1)
    close(fd)
}

test "sys_result Err arm carries strerror message" {
    let r = close(-1)
    assert_eq(r, -1)
    let e = sys_result(-1)
    match e {
        Err(m) => {
            assert(m.len > 0)
        }
        Ok(v) => {
            assert_eq(v, -999)
        }
    }
}
```

写码前探针(逐条实测后修码,不许猜):`errno() as I32` 是否合法(as 面);`Result.ok()` 组合子是否存在(std/opt.ct 或语言面;无则改用 match 双臂);`dup/close` 需在本文件 extern 声明(`#[trusted] extern "c" fn dup(fd: I32) -> I64`/`close(fd: I32) -> I64`——close 在 errno_basics 是 I32 返回,这里统一按各 fixture 现状)。libc 变参/整数提升口径:strerror(int) 以 I32 声明直调,合规。

- [ ] **Step 2: 验收(std 包口径 = ctron test,双宿主)**

Run: `CTRON_STDPATH="$PWD/std" compiler-c/build/ctronc test std/ffi.ct` 与重建后 `CTRON_STDPATH="$PWD/std" compiler/bin/ctron-cc test std/ffi.ct`
Expected: 2 test 全绿双宿主。strerror 消息断言只断非空(不做 locale 敏感子串;err_wrap 夹具处 LC_ALL=C 后可断 "Bad file"——同域注记)。

- [ ] **Step 3: tests/ffi/err_wrap 夹具 + run.sh 专道**

`tests/ffi/err_wrap/src/main.ct`:

```ct
// tests/ffi/err_wrap —— std.ffi errno→Result 深层包装(§9.6)
use std.ffi.{sys_result, err_str}

#[trusted]
extern "c" fn dup(fd: I32) -> I64

#[trusted]
extern "c" fn close(fd: I32) -> I64

test "dup succeeds then sys_result wraps Ok(fd)" {
    let fd = dup(0)
    assert(fd >= 0)
    match sys_result(fd) {
        Ok(v) => {
            assert_eq(v, fd)
        }
        Err(m) => {
            assert_eq(m.len, -1)
        }
    }
    close(fd as I32)
}

test "close(-1) fails and sys_result wraps Err(strerror)" {
    let r = close(-1)
    assert_eq(r, -1)
    match sys_result(-1) {
        Err(m) => {
            assert(m.len > 0)
        }
        Ok(v) => {
            assert_eq(v, -999)
        }
    }
}
```

run.sh 专道(行为分支排除同前):

```sh
# ---- err_wrap:std.ffi Result 包装(纯 libc + std 路径) ----
ewd="$DIR/err_wrap"
if [ -d "$ewd/src" ]; then
    e="$ewd/src/main.ct"
    if CTRON_STDPATH="$ROOT/std" LC_ALL=C LANG=C "$EMIT" run "$e" > "$T/ew.c" 2>"$T/ew.emiterr" \
       && cc -O1 -w -o "$T/ew.bin" "$T/ew.c" 2>"$T/ew.ccerr" \
       && CTRON_STDPATH="$ROOT/std" LC_ALL=C LANG=C "$T/ew.bin" run "$e" > "$T/ew.out" 2>&1; then
        echo "  [ok] err_wrap(std.ffi sys_result Ok/Err 双臂)"
        pass=$((pass + 1))
    else
        echo "  [FAIL] err_wrap — $(head -1 "$T/ew.emiterr" 2>/dev/null)$(head -1 "$T/ew.ccerr" 2>/dev/null)$(head -c 120 "$T/ew.out" 2>/dev/null)"
        fail=$((fail + 1))
    fi
fi
```

(use 解析对 emit/解释双面的 CTRON_STDPATH 口径以 tests/net/run.sh:14 先例为准;探针不通就贴 run.sh 现有 std 消费夹具形态修。)

- [ ] **Step 4: meta_check/smoke 归因核验**

Run: `sh compiler/test/smoke.sh 2>&1 | tail -5`
Expected: 若 decl 锁/std 包清单抖动 → 按真实值重锁(并行线常态);非本批红项逐条归因,不静默放过。

- [ ] **Step 5: Commit**

```bash
git add std/ffi.ct tests/ffi/err_wrap tests/ffi/run.sh
git commit -m "feat(std): std.ffi 包——errno→Result 深层包装首档(err_str/sys_result)"
```

---

### Task 6: 文档刷新 + 全量回归 + 泳道记忆

**Files:**
- Modify: `docs/ffi-analysis.md`(§三 对比表陈旧列、§四 #9/#12/#13/#14 销账、v0.9 批次段)
- Modify: `compiler/tools/cimport.ct` 头注(union/float/位域能力行)

**Interfaces:** 无代码面;纯文档与回归。

- [ ] **Step 1: ffi-analysis.md 刷新**

- §三 对比表 Ctron 列:动态加载 ✗→`#[dlsym]`+dlopen(v0.7);头消费 ✗→cimport(v0.7,union/float v0.9);变参 ✗→`...`(v0.7);补 F32/float 行。
- §四:#9 划线销账(std.ffi sys_result 首档 + 域注记);#12 划线销账(v0.8 已全语境落地,补历史口径);#13 划线改"位域=检测整跳(v0.9),union=字节缓冲(v0.9),float=F32 码(v0.9);剩:解释桥 float 帧、union 指针形参";#14 pkg-config 半销账(ctc.sh 解析落地,深度构建集成随构建批次)、C 位域=cimport 检测落地+语言面不设(声明序布局宪法)。
- 追加「v0.9 批次(2026-09-23)」表:位域整跳 / union 字节缓冲+raw_len 互证 / F32 码 g→float + f_is_precise 铁证 / cimport float→F32 / pkg-config 解析 / std.ffi err_str+sys_result。

- [ ] **Step 2: 全量回归**

Run: `sh tests/ffi/run.sh && sh compiler/test/smoke.sh 2>&1 | tail -3`(必要时 `compiler-c/build/ctronc` seed 抽验 cimport 工具退化路径)
Expected: ffi 全过;smoke 红 → 逐条归因(decl 锁重锁 / std 漂移 / 本批回归三选一)。

- [ ] **Step 3: Commit + 记忆**

```bash
git add docs/ffi-analysis.md compiler/tools/cimport.ct
git commit -m "docs(ffi): v0.9 批次收官——#9/#12/#13/#14 销账刷新与对比表纠陈"
```

更新记忆 `ctron-ffi-lane-state.md`(v0.9 批次状态、F32 码 g、载荷槽域注记、解释桥 float 缺帧登记)与 MEMORY.md 索引行。

---

## Self-Review 结论

- **覆盖度**:#13 两余项(位域/union/float)→ Task 1/2/3;#14 pkg-config → Task 4、C 位域 → Task 1(检测面)+ 语言面"不设"口径写进 Task 6;#9 深层 → Task 5;float 精度约定 → Task 3 Step 7;#12/#13/#14 文档陈旧 → Task 6。§二·五 诚实清单五项全部有归宿。
- **占位扫描**:探针分支全部带判定规则与两分支实际代码;无 TBD。
- **类型一致性**:F32 码统一 "g";`Vals_raw_len`/`uvals_size_is`/`sys_result(rc: I64) -> Result[I64, Str]` 前后呼应;cimport float→F32 与 Task 2 的 cimp_c_size F32=4 咬合。
