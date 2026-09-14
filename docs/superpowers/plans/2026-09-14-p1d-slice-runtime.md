# P1-D 切片 T[] / &T[] 运行面 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans(内联)或
> subagent-driven-development。checkbox 跟踪。

**Goal:** 自举发射器支持 `T[]`/`&T[]`/`T[N]` 注解位与切片视图语义(写透过/退化/len/索引/
for-in),镜像 seed 解释器可观察语义(03f_slices.ct 已在解释面绿,本片补齐发射面)。

**Architecture:** 三线同形 C 表示 `ctron_view_<ec> { <T>* d; int64_t n; }`(宿主
ctron_arr_* / R 线 ct_arr* 同形)。eval 零改动("A" 值已引用语义,注解位 eval 本就忽略);
trans 五处插桩:ct_ty_code 补 Slice/ArrayT/Ref-Slice 臂、ct_ctype 补 v 码、视图 typedef
按需预扫、let/索引/索引写/len/for-in 各补 v 臂。越界 panic 镜像 eval("index out of
bounds"),借 Try 分支先例的 GNU statement-expr 形态。

**Tech Stack:** Ctron(compiler/src:trans_ty/trans_expr/trans_stmt/driver_emit)、
seed 解释 oracle、03f_slices.ct/06b_slice_nonsend.neg.ct 既有语料、suite/smoke/固定点。

## Global Constraints

- 既有绿面不回退:`let xs = [1,2,3]` 无注解路径(内联 C 数组 a<N><ec>)保持逐字节不变
  (trans_v2 夹具含数组);仅注解位(Slice/ArrayT)走新视图/定长臂。
- 诊断面走 SOP:非 var 根元素写(§4.2)的静态诊断 v0 不做(三线均未实现,挂账);
  越界 panic 消息镜像 eval "index out of bounds"。
- iter()/适配器(map/filter/sum)不属本片;r3b 红锚不承诺翻转(roadmap 明文)。
- 门禁:smoke --full(std 漂移除外)+ suite 双侧 + 固定点逐字节;decls 锁同步。

## 事实底座(侦察结论,实现者必读)

- eval:`"A"` 值绑定不拷贝(bind_of 只对 "U" 非类深拷贝)、索引写原地透传(eval_run.ct:
  140-172)、`.len` = recv.len-1、for-in 三形态(eval_run.ct:218-268)——注解位 eval 零消费
  (ty_head 剥 Slice/ArrayT 只服务宽度域)。**解释面 03f 已绿,本片纯发射面。**
- trans 断点:`ct_ty_code`(trans_ty.ct:162-235)`if nt[0] != "Named" { return "i" }` →
  Slice/ArrayT 注解一律 "i";`a<N><ec>` 内联 C 数组无法表达共享;索引写无 a 臂;
  ArrLit 表达式位 panic(trans_expr.ct:263-265)。
- 三线矩阵:解析全 ✅;Send 全 ✅;sem let-compat 仅自举有(compat "slice"/"ref:slice" 臂,
  sem_type.ct:249-296);eval/interp 全 ✅(宿主 V_ARR/R 线 Rc);发射面宿主 heap 句柄 ✅/
  R 线 ct_arr ✅/自举 ❌。
- 宿主 C 形态参照:`typedef struct { <T>* d; int64_t n; } ctron_arr_<wl>;`(trans.c:364)。
- 嵌套切片/切片的切片 v0 不支持(命中即 panic);List 退化挂后续(roadmap 口径)。

---

### Task 1: 类型码臂(ct_ty_code/ct_ctype)+ 视图 typedef

**Files:** Modify `compiler/src/trans_ty.ct`(ct_ty_code :162-235、ct_ctype :228-266)

- [ ] ct_ty_code 加三臂(在 `if nt[0] != "Named"` 之前):

```ctron
    if nt[0] == "Slice" {
        return "v" + ct_ty_code(nt[1], file)
    }
    if nt[0] == "Ref" && nt[1].len > 1 && nt[1][0] == "Slice" {
        // &T[] 只读视图:v0 与可变视图同 C 表示(Send 判定已在检查面区分)
        return "v" + ct_ty_code(nt[1][1], file)
    }
    if nt[0] == "ArrayT" {
        // T[N] 定长注解 → 既有 a<N><ec> 内联码(N 缺失按 1 兜底,语料不触)
        var an = "1"
        if nt[2].len > 1 && nt[2][0] == "Int" { an = num_text(nt[2][1]) }
        return "a" + an + ct_ty_code(nt[1], file)
    }
```

(ArrayT 节点槽位以 parse_expr.ct:619 现场为准:`mk("ArrayT")` 后 push 顺序。)

- [ ] ct_ctype 加 v 臂(首字符分派处):

```ctron
    if byte_at(code, 0) == 118 {
        return "ctron_view_" + ct_ctype(byte_slice(code, 1, code.len))
    }
```

(v 为单字符前缀,余串即元素码;嵌套 "va3i" = 视图的视图,v0 由命中即 panic 兜底。)

- [ ] 构建自检:`bash build.sh && ../compiler-c/build/ctronc check build/cc_run.ct`
  预期 0 diagnostics;`ctc.sh emit test/fixtures/... trans_v2` 往返回归(应不变)。

- [ ] Commit: `feat(trans): 切片/定长注解类型码臂(v<ec> 视图码 + ArrayT a 码)`

### Task 2: 视图 typedef 预扫 + let 三形态发射

**Files:** Modify `compiler/src/driver_emit.ct`(typedef 区 :116-135 后)、
`compiler/src/trans_stmt.ct`(Let 分支 :549-580 一带)

- [ ] driver_emit pass1:扫 file 顶层(Signature 参数/返回/let 注解位递归找 Slice)太宽——
  简化:扫 let 注解与 fn 形参/返回的类型节点收集 `v<ec>` 码(实现时以一次前序遍历
  collect_view_codes(file) 助手落地,放 trans_ty.ct),typedef 区输出:

```ctron
    println("typedef struct \{ " + ct_ctype(ec) + "* d; int64_t n; \} ctron_view_" + ct_ctype(ec) + ";")
```

- [ ] Let 分支:ty 为 `v…` 时三分支初值:
  - ArrLit → 堆字面量(eln 多行):

```ctron
            eln("ctron_view_" + ec_ctype + " t_" + pat[1] + "; t_" + pat[1] + ".n = " + n + ";", env)
            eln("t_" + pat[1] + ".d = (" + ec_ctype + "*)ctron_amalloc(sizeof(" + ec_ctype + ") * " + n + ");", env)
            // 逐元素 d[k] = expr;
```

  - Ident 且 env 型为 a 码 → `ctron_view_<ec> t_x = { t_buf, <N> };`(内联数组退化,写透过 ✓)
  - 其余(视图/Calls)→ `= <expr>;`(结构体值拷贝,d 指针共享 ✓)
  - env_bind(env, 名, ty)(v 码入 env,下游 typeof 自动传播)。

- [ ] 回归:trans_v0–v3 往返逐字;`ctc.sh check build/cc_run.ct` decls 新锁记录。

- [ ] Commit: `feat(trans): 视图 typedef + let 注解位三形态(堆字面量/内联退化/视图拷贝)`

### Task 3: 索引/len/索引写/for-in 的 v 臂

**Files:** Modify `compiler/src/trans_expr.ct`(Index :168-174、Member len)、
`compiler/src/trans_stmt.ct`(Assign 索引位 :599-610、For :1014-1023)

- [ ] Index 读(目标 typeof 为 v 码):

```ctron
            if byte_at(ot, 0) == 118 {
                var ec = ct_ctype(byte_slice(ot, 1, ot.len))
                var tmp = "t_vx" + nline(e)
                return "(\{ int64_t " + tmp + " = " + ct_expr(e[2], env, file) + "; if (" + tmp + " < 0 || " + tmp + " >= (" + ct_expr(e[1], env, file) + ").n) \{ ctron_panic(\"index out of bounds\"); } (" + ec + ")((" + ct_expr(e[1], env, file) + ").d[" + tmp + "]); })"
            }
```

(ct_expr(e[1]) 双求值风险:Ident 位无害;非常量目标命中即接受,语料为 Ident。)

- [ ] Member `len` → `(int32_t)((t_x).n)`;索引写(Assign 目标 Index,typeof v 码)→
  语句位同款越界守卫 + `(t_x).d[i] = v;`(写透过);for-in → 计数循环
  `i < (t_x).n` + `<ctype> t_v = (t_x).d[ctron_i];`(仿 a 臂 :1014-1023)。

- [ ] Commit: `feat(trans): 切片索引读/写越界守卫 + len + for-in 视图臂`

### Task 4: fx_slice 夹具三方绿 + 门禁注册

**Files:** Create `compiler/test/fx_slice.ct`;Modify `compiler/test/smoke.sh`(fx 循环加
`slice`;decls 锁同步)

- [ ] 夹具(镜像 tests/03f_slices.ct 语义,含写透过/退化/len/for-in/越界 panic 边界):

```ctron
fn total(xs: &I32[]) -> I32 {
    var s: I32 = 0
    for x in xs {
        s = s + x
    }
    return s
}
fn main() -> I32 {
    var buf: I32[3] = [1, 2, 3]
    var view: I32[] = buf
    view[1] = 20
    println(buf[1])
    let ro: &I32[] = view
    println(ro.len)
    println(total(buf))
    let lit: I32[] = [7, 8, 9]
    println(lit[2])
    return 0
}
```

预期输出:`20 / 3 / 24 / 9`(seed 解释 == seed 发射 == native 发射)。

- [ ] smoke fx 循环加 `slice`;decls 锁改 Task 2 记录值;smoke --full 全绿(std 漂移除外)。

- [ ] Commit: `test(trans): fx_slice 三方逐字——切片视图写透过/退化/len/for-in`

### Task 5: suite + 固定点 + 文档同步

- [ ] suite.py 全量(03f_slices/06b 既有语料应保持双侧绿;新增 fx 不进 suite 目录)。
- [ ] 固定点逐字节(compiler/src 无切片注解,产物不变)。
- [ ] roadmap P1-D ✅(挂账:非 var 根写诊断码、iter()/适配器、List 退化、嵌套视图);
  HANDOFF 补条目;提交。

## Self-Review

- spec §3.6 三条隐式转换:T[N]→T[](Ident 退化臂)、T[]→&T[](同码结构体拷贝)、
  数组字面量直接型 T[N](无注解 a 码路径不变)✓;§4.2 var 根写门挂账(三线一致缺口)✓。
- 占位符:ArrayT 槽位序、collect_view_codes 的遍历面——标注"以现场为准,先实测"。
- 类型一致性:v 码单字符前缀,ec 提取 = byte_slice(1, len),与 a 码(N 前缀数字段)不冲突。
