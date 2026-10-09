# std HashMap[K,V] W1 实施计划（注入形 v1）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 `lib/std/hashmap.ct`——可变泛型哈希映射 `HMap[K,V]`（Box 载体、开放寻址、逐调用注入 hash/eq），宪章 3a 附则修订，全部门禁绿后单批落库。

**Architecture:** `Box[HMap[K,V]]` 引用语义载体（写穿别名共享）+ slots/keys/vals 三平行 List（FMap 零值构造规避先例）+ 负载 3/4 倍增。哈希/相等以逐调用 fn 形参注入（sort.ct 发射绿先例），fn 值不入 struct 字段（T16 在册红）。设计文档：`docs/superpowers/specs/2026-10-09-std-hashmap-design.md`。

**Tech Stack:** 纯 Ctron（T1，零 C 源，零 use——宪章 2 核心互不 use，自带哈希）；门禁 = bootstrap 解释（`compiler/bin/ctron-cc run`）+ 发射臂 parity（`ctron-emit run` → `cc` → 编译产物，与解释输出逐字对拍）+ Rust 臂（ctronr）+ stdpkg 种子副本漂移门 + std_doc 参考页漂移门。

## Global Constraints

- **机刷纪律**：对端泳道正在本树实时施工（gui/文档域包）。全程 pathspec 限定提交；**绝不重建编译器驱动**（用盘上 `compiler/bin/*`）；**绝不 `git stash` / `add -A`**。
- 禁闭包字面量作 fn 值（C13）——比较器/哈希一律具名 fn。
- fn 值不入 struct 字段（T16 发射在册红，iter 先例）。
- 无比特算符（`&`/`|`/`^` 解释面缺口）——哈希用乘加 + mod。
- 哈希中间值全程 < 2^31（C10 eval 乘法溢出门；fmap `mod 100003` 先例）；负值归一 `((x % m) + m) % m` 形（fmap:34 先例）。
- T1 核心互不 `use`（宪章 2）：hashmap.ct 零 use 行，`byte_at` 是前奏内建。
- 命名 `hm_*` / 结构 `HMap[K,V]`：use 合并单一命名空间同名 decl 静默首胜，与 map/set/heap 的 put/get/has 错开（FMap `f*` 先例）。
- 容器操作 = 顶层泛型自由函数（v0.2 API 规范 C4 契约），UFCS 不作主验证面。
- 新文件必须同步 `compiler/test/stdpkg/std/hashmap.ct`（字节级一致，smoke 漂移门）；参考页必须生成并挂 nav（std_doc --check 门 + pages.yml strict）。
- decls 锁（smoke.sh:38，`decls=521`）只锁 cc_run——本批零 compiler/src 改动，锁值不得移动；若被迫动编译器源，`sh compiler/build.sh` 后 `./compiler/ctc.sh check compiler/build/cc_run.ct` 实测回写。

---

### Task 1: 探针 P1/P2——Box×泛型 struct×var List 写穿双臂验证

**Files:**
- Create (scratch, 不入库): `/tmp/hm_probe1.ct`

**Interfaces:**
- Produces: 载体形态裁决。P1 绿 → Task 2 按 Box 载体实施；P1 红 → 回退函数式形态（`hm_put` 返回新映射，宪章 3 零改动，Task 2 换 FMap 泛型化骨架）。

- [ ] **Step 1: 写探针文件** `/tmp/hm_probe1.ct`（覆盖四个风险点：泛型 Box 构造、经 Box 形参 push、经 Box 下标写、经 Box 字段整体重赋值；外加 P2 局部特化形参）：

```ctron
// P1/P2 探针:Box+泛型 struct+var List 字段写穿(不入库)
struct Bag[K, V] {
    var names: List[K]
    var marks: List[V]
    var grid: List[I32]
}

pub fn bag_new[K, V]() -> Box[Bag[K, V]] {
    var g = List[I32]()
    var i: I32 = 0
    while i < 4 {
        g.push(-1)
        i += 1
    }
    return Box[Bag[K, V]](Bag { names: List[K](), marks: List[V](), grid: g })
}

pub fn bag_add[K, V](b: Box[Bag[K, V]], k: K, v: V) {
    b.names.push(k)
    b.marks.push(v)
}

pub fn bag_set[K, V](b: Box[Bag[K, V]], i: I32, v: V) {
    b.marks[i] = v
}

pub fn bag_get[K, V](b: Box[Bag[K, V]], i: I32, dft: V) -> V {
    if i >= b.names.len {
        return dft
    }
    return b.marks[i]
}

pub fn bag_grid_reset(b: Box[Bag[Str, I64]]) {
    var g = List[I32]()
    g.push(5)
    b.grid = g
}

// P2:局部特化 Box[Bag[Str, I64]] 作形参(糖面决策依据)
pub fn bag_first_mark(b: Box[Bag[Str, I64]], dft: I64) -> I64 {
    return bag_get[Str, I64](b, 0, dft)
}

fn main() {
    var b = bag_new[Str, I64]()
    var alias = b
    bag_add[Str, I64](b, "a", 10)
    bag_add[Str, I64](b, "b", 20)
    bag_set[Str, I64](b, 0, 99)
    println(alias.names.len)
    println(bag_get[Str, I64](alias, 0, 0 - 1))
    println(bag_get[Str, I64](alias, 1, 0 - 1))
    println(bag_first_mark(b, 0 - 1))
    bag_grid_reset(b)
    println(alias.grid[0])
    var fresh = bag_new[Str, I64]()
    println(fresh.names.len)
}
```

期望输出（双臂逐字一致，6 行）：

```
2
99
20
99
5
0
```

（第 4 行 = bag_first_mark 经 P2 特化形参读到 alias 写穿的 99；第 5 行 = 字段整体重赋值经别名可见。）

- [ ] **Step 2: 解释臂跑**

Run: `compiler/bin/ctron-cc run /tmp/hm_probe1.ct`
Expected: 上面 6 行输出，rc=0。（注意是 6 行：len/get/get/first/grid/fresh。）

- [ ] **Step 3: 发射臂跑（抄 smoke 4c 命令形，smoke.sh:651-656）**

Run: `compiler/bin/ctron-emit run /tmp/hm_probe1.ct > /tmp/hm_p1.c && cc -O1 -w -o /tmp/hm_p1.bin /tmp/hm_p1.c && /tmp/hm_p1.bin run /tmp/hm_probe1.ct`
Expected: 与 Step 2 逐字一致。

- [ ] **Step 4: 裁决**

- 双臂绿 → Box 载体成立，进 Task 2（主形态）。
- 解释臂绿、发射臂红（或双臂分歧）→ 该缺口**登记 COVERAGE 债**（不修编译器——零 compiler/src 纪律），Task 2 回退函数式形态。
- 若 `pub fn bag_add[K, V](...)`（无 `->`）报语法错 → 补 `-> Void` 重跑（gui d_* 先例应无箭头，此为保险探针）。

---

### Task 2: `lib/std/hashmap.ct` 本体 + 模块双臂门

**Files:**
- Create: `lib/std/hashmap.ct`
- Create (副本): `compiler/test/stdpkg/std/hashmap.ct`（字节级拷贝）

**Interfaces:**
- Consumes: 前奏内建 `List[T]`（push/pop/len/下标读写）、`Box[T]`（构造/自动解引用读写）、`byte_at`。
- Produces（Task 3 消费面按此签名）:

```ctron
pub fn hm_new[K, V]() -> Box[HMap[K, V]]
pub fn hm_put[K, V](b: Box[HMap[K, V]], k: K, v: V, h: fn(K) -> I32, eq: fn(K, K) -> Bool)
pub fn hm_get[K, V](b: Box[HMap[K, V]], k: K, dft: V, h: fn(K) -> I32, eq: fn(K, K) -> Bool) -> V
pub fn hm_has[K, V](b: Box[HMap[K, V]], k: K, h: fn(K) -> I32, eq: fn(K, K) -> Bool) -> Bool
pub fn hm_del[K, V](b: Box[HMap[K, V]], k: K, h: fn(K) -> I32, eq: fn(K, K) -> Bool)
pub fn hm_len[K, V](b: Box[HMap[K, V]]) -> I32
pub fn hm_keys[K, V](b: Box[HMap[K, V]]) -> List[K]
pub fn hm_vals[K, V](b: Box[HMap[K, V]]) -> List[V]
pub fn hm_hash_str(s: Str) -> I32      // djb2 mod 100003(fmap 拷贝)
pub fn hm_eq_str(a: Str, b: Str) -> Bool
pub fn hm_hash_i64(k: I64) -> I32      // 中间值 < 2^31;I64→I32 收窄走 as(探针失败则本件裁撤)
pub fn hm_eq_i64(a: I64, b: I64) -> Bool
```

- [ ] **Step 1: 写 `lib/std/hashmap.ct`**（完整文件；哈希签名 **I32** 设计修订：全程回避 I64/I32 混宽缝隙，fmap 同域先例——随批回写设计文档 §4）：

```ctron
// std/hashmap.ct —— HMap[K,V]:可变泛型哈希映射(开放寻址,线性探查,Box 载体)
//@ tier: alloc
// since: std-0.9 stability: experimental
// 口径:put/get 均摊 O(1)(负载 3/4 倍增重建);del = 活词表重建 O(n)
// (v0.9 无墓碑,fdel 先例;不缩容,文档化);迭代序 = 插入序(keys 追加式,
// 宪章 8 确定性)。可变载体 = Box[HMap[K,V]] 写穿别名共享(宪章 3a 附则,
// 2026-10-09;fx_boxalias/gui_driver 先例)。
// 哈希/相等逐调用注入(具名 fn 值,C13;fn 值不入 struct 字段,T16 在册红;
// sort.ct 比较器先例)。哈希签名 I32:全程 < 2^31(C10 eval 乘法门,
// fmap mod 100003 先例),回避 I64/I32 混宽;无比特算符(&/|/^ 解释面缺口)。
// 命名 hm_* 前缀:use 合并单一命名空间,与 map/set/heap 的 put/get/has
// 错开(同名 decl 静默首胜,FMap f* 先例)。T1 零 use(宪章 2)。
struct HMap[K, V] {
    var slots: List[I32]
    var keys: List[K]
    var vals: List[V]
}

// 空映射:初容 16 槽全 -1;keys/vals 追加式(规避泛型 V 零值构造,FMap 先例)
pub fn hm_new[K, V]() -> Box[HMap[K, V]] {
    var s = List[I32]()
    var i: I32 = 0
    while i < 16 {
        s.push(-1)
        i += 1
    }
    return Box[HMap[K, V]](HMap { slots: s, keys: List[K](), vals: List[V]() })
}

// 哈希 → 容量内槽位(负值归一,fmap:34 先例)
fn hm_slot_of(h: I32, cap: I32) -> I32 {
    var m = (h % cap + cap) % cap
    return m
}

// 键 → keys 下标;不存在返回 -1(至多探查 slots.len 步)
fn hm_find[K, V](b: Box[HMap[K, V]], k: K, h: fn(K) -> I32, eq: fn(K, K) -> Bool) -> I32 {
    var cap = b.slots.len
    var idx = hm_slot_of(h(k), cap)
    var n: I32 = 0
    while n < cap {
        var s = b.slots[idx]
        if s < 0 {
            return -1
        }
        if eq(b.keys[s], k) {
            return s
        }
        idx += 1
        if idx == cap {
            idx = 0
        }
        n += 1
    }
    return -1
}

// 倍增重建:slots 翻倍,活键按插入序重探查
fn hm_grow[K, V](b: Box[HMap[K, V]], h: fn(K) -> I32) {
    var ncap = b.slots.len * 2
    var ns = List[I32]()
    var i: I32 = 0
    while i < ncap {
        ns.push(-1)
        i += 1
    }
    var j: I32 = 0
    while j < b.keys.len {
        var idx = hm_slot_of(h(b.keys[j]), ncap)
        while ns[idx] >= 0 {
            idx += 1
            if idx == ncap {
                idx = 0
            }
        }
        ns[idx] = j
        j += 1
    }
    b.slots = ns
}

// 就地写:存在则覆盖 vals;否则负载 > 3/4 先倍增再探空槽插入
pub fn hm_put[K, V](b: Box[HMap[K, V]], k: K, v: V, h: fn(K) -> I32, eq: fn(K, K) -> Bool) {
    var hit = hm_find[K, V](b, k, h, eq)
    if hit >= 0 {
        b.vals[hit] = v
        return
    }
    if b.keys.len * 4 > b.slots.len * 3 {
        hm_grow[K, V](b, h)
    }
    var cap = b.slots.len
    var idx = hm_slot_of(h(k), cap)
    var n: I32 = 0
    while n < cap {
        if b.slots[idx] < 0 {
            b.slots[idx] = b.keys.len
            b.keys.push(k)
            b.vals.push(v)
            return
        }
        idx += 1
        if idx == cap {
            idx = 0
        }
        n += 1
    }
}

// 取值:缺键返回 dft(FMap fget 先例,避 Option 泛型面)
pub fn hm_get[K, V](b: Box[HMap[K, V]], k: K, dft: V, h: fn(K) -> I32, eq: fn(K, K) -> Bool) -> V {
    var s = hm_find[K, V](b, k, h, eq)
    if s < 0 {
        return dft
    }
    return b.vals[s]
}

pub fn hm_has[K, V](b: Box[HMap[K, V]], k: K, h: fn(K) -> I32, eq: fn(K, K) -> Bool) -> Bool {
    if hm_find[K, V](b, k, h, eq) < 0 {
        return false
    }
    return true
}

// 删除键:活词表原序重建 O(n)(v0.9 无墓碑;不缩容);不存在原样返回
pub fn hm_del[K, V](b: Box[HMap[K, V]], k: K, h: fn(K) -> I32, eq: fn(K, K) -> Bool) {
    var hit = hm_find[K, V](b, k, h, eq)
    if hit < 0 {
        return
    }
    var nk = List[K]()
    var nv = List[V]()
    var i: I32 = 0
    while i < b.keys.len {
        if i != hit {
            nk.push(b.keys[i])
            nv.push(b.vals[i])
        }
        i += 1
    }
    var cap = b.slots.len
    var ns = List[I32]()
    i = 0
    while i < cap {
        ns.push(-1)
        i += 1
    }
    var j: I32 = 0
    while j < nk.len {
        var idx = hm_slot_of(h(nk[j]), cap)
        while ns[idx] >= 0 {
            idx += 1
            if idx == cap {
                idx = 0
            }
        }
        ns[idx] = j
        j += 1
    }
    b.keys = nk
    b.vals = nv
    b.slots = ns
}

// 键数
pub fn hm_len[K, V](b: Box[HMap[K, V]]) -> I32 {
    return b.keys.len
}

// 键表拷贝(插入序;拷贝非别名,保 keys 追加式不变量)
pub fn hm_keys[K, V](b: Box[HMap[K, V]]) -> List[K] {
    var out = List[K]()
    var i: I32 = 0
    while i < b.keys.len {
        out.push(b.keys[i])
        i += 1
    }
    return out
}

// 值表拷贝(与 hm_keys 一一对应)
pub fn hm_vals[K, V](b: Box[HMap[K, V]]) -> List[V] {
    var out = List[V]()
    var i: I32 = 0
    while i < b.vals.len {
        out.push(b.vals[i])
        i += 1
    }
    return out
}

// ===== 注入助手(具名 fn 值,供调用点传递;C13 禁闭包字面量) =====

// Str 键哈希:djb2 mod 100003(全程 < 2^31;逐字节;拷贝自 fmap.fidx,双面精确一致)
pub fn hm_hash_str(s: Str) -> I32 {
    var h: I32 = 5381
    var i: I32 = 0
    while i < s.len {
        h = (h * 33 + byte_at(s, i)) % 100003
        i += 1
    }
    return h
}

pub fn hm_eq_str(a: Str, b: Str) -> Bool {
    return a == b
}

// I64 键哈希:两步 mod 压域(中间值 < 2^31),as 收窄 I32(发射臂若红则本件裁撤,登记债)
pub fn hm_hash_i64(k: I64) -> I32 {
    var h = k % 1000003
    if h < 0 {
        h += 1000003
    }
    return (h * 31 + 11) % 1000003 as I32
}

pub fn hm_eq_i64(a: I64, b: I64) -> Bool {
    return a == b
}

test "hm basics str keys" {
    var m = hm_new[Str, I64]()
    assert_eq(hm_len[Str, I64](m), 0)
    assert(!hm_has[Str, I64](m, "a", hm_hash_str, hm_eq_str))
    hm_put[Str, I64](m, "a", 1)
    hm_put[Str, I64](m, "b", 2)
    assert_eq(hm_len[Str, I64](m), 2)
    assert_eq(hm_get[Str, I64](m, "a", -1), 1)
    assert_eq(hm_get[Str, I64](m, "b", -1), 2)
    assert_eq(hm_get[Str, I64](m, "c", -1), -1)
    assert(hm_has[Str, I64](m, "a", hm_hash_str, hm_eq_str))
    assert(!hm_has[Str, I64](m, "c", hm_hash_str, hm_eq_str))
}

test "hm overwrite same key" {
    var m = hm_new[Str, I64]()
    hm_put[Str, I64](m, "k", 1)
    hm_put[Str, I64](m, "k", 7)
    assert_eq(hm_len[Str, I64](m), 1)
    assert_eq(hm_get[Str, I64](m, "k", 0), 7)
}

test "hm i64 keys incl negative" {
    var m = hm_new[I64, Str]()
    hm_put[I64, Str](m, 1001, "zou")
    hm_put[I64, Str](m, -42, "neg")
    assert_eq(hm_len[I64, Str](m), 2)
    assert_eq(hm_get[I64, Str](m, 1001, "?"), "zou")
    assert_eq(hm_get[I64, Str](m, -42, "?"), "neg")
    assert_eq(hm_get[I64, Str](m, 7, "?"), "?")
}

test "hm alias write-through" {
    var m = hm_new[Str, I64]()
    var alias = m
    hm_put[Str, I64](m, "x", 10)
    assert_eq(hm_len[Str, I64](alias), 1)
    assert_eq(hm_get[Str, I64](alias, "x", 0), 10)
    hm_put[Str, I64](alias, "x", 20)
    assert_eq(hm_get[Str, I64](m, "x", 0), 20)
}

test "hm growth to 200 keys" {
    var m = hm_new[I64, I64]()
    var i: I64 = 0
    while i < 200 {
        hm_put[I64, I64](m, i * 7, i)
        i += 1
    }
    assert_eq(hm_len[I64, I64](m), 200)
    i = 0
    while i < 200 {
        assert_eq(hm_get[I64, I64](m, i * 7, -1), i)
        i += 1
    }
    assert_eq(hm_get[I64, I64](m, 1401, -1), -1)
}

test "hm insertion order keys vals" {
    var m = hm_new[Str, I32]()
    hm_put[Str, I32](m, "b", 2)
    hm_put[Str, I32](m, "a", 1)
    hm_put[Str, I32](m, "c", 3)
    hm_put[Str, I32](m, "a", 9)
    var ks = hm_keys[Str, I32](m)
    assert_eq(ks.len, 3)
    assert_eq(ks[0], "b")
    assert_eq(ks[1], "a")
    assert_eq(ks[2], "c")
    var vs = hm_vals[Str, I32](m)
    assert_eq(vs[0], 2)
    assert_eq(vs[1], 9)
    assert_eq(vs[2], 3)
}

test "hm del removes and keeps order" {
    var m = hm_new[Str, I32]()
    hm_put[Str, I32](m, "a", 1)
    hm_put[Str, I32](m, "b", 2)
    hm_put[Str, I32](m, "c", 3)
    hm_del[Str, I32](m, "b", hm_hash_str, hm_eq_str)
    assert_eq(hm_len[Str, I32](m), 2)
    assert(!hm_has[Str, I32](m, "b", hm_hash_str, hm_eq_str))
    assert(hm_has[Str, I32](m, "a", hm_hash_str, hm_eq_str))
    assert(hm_has[Str, I32](m, "c", hm_hash_str, hm_eq_str))
    var ks = hm_keys[Str, I32](m)
    assert_eq(ks[0], "a")
    assert_eq(ks[1], "c")
    hm_del[Str, I32](m, "zz", hm_hash_str, hm_eq_str)
    assert_eq(hm_len[Str, I32](m), 2)
}

test "hm re-put after del" {
    var m = hm_new[I64, Str]()
    hm_put[I64, Str](m, 1, "a")
    hm_put[I64, Str](m, 2, "b")
    hm_del[I64, Str](m, 1, hm_hash_i64, hm_eq_i64)
    hm_put[I64, Str](m, 3, "c")
    assert_eq(hm_len[I64, Str](m), 2)
    assert_eq(hm_get[I64, Str](m, 3, "?"), "c")
    assert_eq(hm_get[I64, Str](m, 2, "?"), "b")
}

test "hm empty map edges" {
    var m = hm_new[Str, I64]()
    assert_eq(hm_len[Str, I64](m), 0)
    assert_eq(hm_get[Str, I64](m, "nope", 7), 7)
    assert_eq(hm_keys[Str, I64](m).len, 0)
    assert_eq(hm_vals[Str, I64](m).len, 0)
    hm_del[Str, I64](m, "nope", hm_hash_str, hm_eq_str)
    assert_eq(hm_len[Str, I64](m), 0)
}
```

- [ ] **Step 2: 模块独立跑（宪章 6）**

Run: `compiler/bin/ctron-cc run lib/std/hashmap.ct`
Expected: rc=0，全部 test 块过。（若 void fn 无箭头报错 → 全部 `hm_put/hm_del` 补 `-> Void`；若 `as I32` 报错 → 裁撤 hm_hash_i64/hm_eq_i64 两件及 "hm i64 keys"/"hm growth"/"hm re-put" 三测试，登记债，其余照常。）

- [ ] **Step 3: 发射臂 parity（4c 命令形单文件）**

Run: `compiler/bin/ctron-emit run lib/std/hashmap.ct > /tmp/hm_mod.c && cc -O1 -w -o /tmp/hm_mod.bin /tmp/hm_mod.c && /tmp/hm_mod.bin run lib/std/hashmap.ct; echo "rc=$?"`
Expected: rc=0，输出与 `compiler/bin/ctron-cc run lib/std/hashmap.ct` 一致（test 块静默）。

- [ ] **Step 4: 字节级副本同步**

Run: `cp lib/std/hashmap.ct compiler/test/stdpkg/std/hashmap.ct && diff -q lib/std/hashmap.ct compiler/test/stdpkg/std/hashmap.ct`
Expected: 无输出（一致）。

- [ ] **Step 5: 无提交**（本 Task 不单独提交——演进纪律要求模块+清单+消费面+门禁一批一提交，Task 3 收口）

---

### Task 3: 清单/宪章/消费面/参考页/COVERAGE + 全门绿 + 单批提交

**Files:**
- Modify: `lib/std/README.md`（模块表加行 + 宪章 3a 附则）
- Modify: `docs/superpowers/specs/2026-10-09-std-hashmap-design.md`（§4 哈希签名 I64→I32 修订记要）
- Modify: `compiler/test/stdpkg/src/main.ct`（use 行 + 消费面段）
- Create: `website/docs/std/hashmap.md`（生成）
- Modify: `website/mkdocs.yml`（nav 加行，hash 与 heap 之间）
- Modify: `tests/COVERAGE.md`（顶部批注块）

**Interfaces:**
- Consumes: Task 2 的全部 `hm_*` 签名。
- Produces: W1 落库批（feat 提交一个，pathspec 限定下列文件）。

- [ ] **Step 1: README.md 模块表加行**（`| T1 | uuid.ct |` 行后）

```markdown
| T1 | `hashmap.ct` | HMap[K, V]:可变泛型哈希映射(开放寻址 + 注入形哈希/相等;Box 载体,均摊 O(1);宪章 3a) | 无 | 0.9 |
```

- [ ] **Step 2: README.md 宪章 3 后追加附则**（第 66 行 `3.` 条之后新起一条）

```markdown
   3a. **可变容器附则**(2026-10-09):可变容器以引用语义载体(Box 持堆上结构)
   入 std,与函数式族并存;必须申报 `//@ tier: alloc`;API 用顶层泛型自由
   函数 + 显式注入(hash/eq 比较器,sort.ct 先例),fn 值不入 struct 字段
   (T16 在册红);本条「不可变更新」继续约束函数式族(map/set/heap/FMap)。
```

- [ ] **Step 3: 设计文档 §4 修订记要**——`hm_hash_*` 签名 `fn(K) -> I64` 改 `fn(K) -> I32`，在 §4 表格后追加一行：

```markdown
（修订 2026-10-09 实施 batch:哈希签名定 I32 非 I64——全程回避 I64/I32 混宽
缝隙与 as 收窄面，fmap 同域先例;I64 键的压域在 hm_hash_i64 内部完成。）
```

- [ ] **Step 4: 消费面**——`compiler/test/stdpkg/src/main.ct` 顶部 use 区追加：

```ctron
use std.hashmap.{hm_new, hm_put, hm_get, hm_has, hm_del, hm_len, hm_keys, hm_hash_str, hm_eq_str, hm_hash_i64, hm_eq_i64}
```

`return 0` 前追加消费段（确定性输出，种子/发射两臂逐字对拍承载）：

```ctron
    // ============ HMap[K,V]:可变泛型哈希映射(Box 载体;注入形 hash/eq) ============
    var hm = hm_new[Str, I64]()
    hm_put[Str, I64](hm, "alpha", 1)
    hm_put[Str, I64](hm, "beta", 2)
    hm_put[Str, I64](hm, "alpha", 9)
    println(hm_len[Str, I64](hm).to_string())
    println(hm_get[Str, I64](hm, "alpha", -1).to_string())
    println(hm_get[Str, I64](hm, "gamma", -1).to_string())
    println(hm_has[Str, I64](hm, "beta").to_string())
    hm_del[Str, I64](hm, "alpha", hm_hash_str, hm_eq_str)
    println(hm_len[Str, I64](hm).to_string())
    println(hm_has[Str, I64](hm, "alpha").to_string())
    var hks = hm_keys[Str, I64](hm)
    println(hks[0])
    var him = hm_new[I64, Str]()
    hm_put[I64, Str](him, -42, "neg")
    println(hm_get[I64, Str](him, -42, "?"))
    println(hm_len[I64, Str](him).to_string())
```

期望增量输出（9 行）：`2 / 9 / -1 / true / 1 / false / beta / neg / 1`。

- [ ] **Step 5: 参考页生成 + nav**

Run: `python3 tools/std_doc.py && python3 tools/std_doc.py --check && git diff --stat website/docs/std/ | tail -3`
Expected: `--check` rc=0；`hashmap.md` 为新增（其余 26 页为对端泳道在途修改，**不得提交**）。
`website/mkdocs.yml` nav 的 `- hash: std/hash.md` 行后插：

```yaml
      - hashmap: std/hashmap.md
```

- [ ] **Step 6: 消费面两臂对拍 + 模块种子单测（smoke 同款最小集先行）**

Run: `compiler/bin/ctron-cc run compiler/test/stdpkg/src/main.ct > /tmp/sd_seed.out && compiler/bin/ctron-emit run compiler/test/stdpkg/src/main.ct > /tmp/sd.c && cc -O1 -w -o /tmp/sd.bin /tmp/sd.c && /tmp/sd.bin run compiler/test/stdpkg/src/main.ct > /tmp/sd_got.out; diff /tmp/sd_got.out /tmp/sd_seed.out && tail -9 /tmp/sd_seed.out`
Expected: diff 无输出；tail 为 Step 4 的 9 行。

- [ ] **Step 7: smoke 全量**

Run: `sh compiler/test/smoke.sh --full 2>&1 | tail -5`
Expected: `== 结果: N ok / 0 fail ==`（N ≥ 224，新模块自动进 3j2/3j3/4c 三段）。
 contingency：3j3 Rust 臂 hashmap 红 → smoke.sh:460 `known=` 名单追加 `hashmap` 并在 COVERAGE 登记归因（heap 先例）；4c 发射臂新红 → 先复跑两遍排除机刷中间态，实红则 `KNOWN4C` 登记申报（Darwin heap 先例）+ COVERAGE 归因。

- [ ] **Step 8: suite 防御性复跑（零 tests/ 改动，应不受影响）**

Run: `python3 compiler/test/suite.py 2>&1 | tail -3`
Expected: 双列 112/112（或当前基线）满分。

- [ ] **Step 9: COVERAGE.md 顶部批注块**（按文件现有格式，日期倒序插入）

```markdown
> **2026-10-09 std/hashmap W1**（dd65d9b3 spec 后实施批）：HMap[K,V] 可变泛型哈希映射落库——Box 载体写穿（探针 P1 双臂绿：泛型 Box 构造/push/下标写/字段重赋值/局部特化形参 P2）+ 开放寻址 3/4 倍增 + 追加式平行 List（规避泛型 V 零值构造）；注入形 hash/eq（I32 域全程 < 2^31，sort 比较器先例）；宪章 3a 附则入册。门：模块双臂 + stdpkg 消费面两臂逐字 + smoke --full N/0 + suite 112×112 + std_doc --check。坑位登记：<实施时实填>。
```

- [ ] **Step 10: 单批提交（pathspec 限定，绝不 add -A）**

```bash
git add lib/std/hashmap.ct compiler/test/stdpkg/std/hashmap.ct compiler/test/stdpkg/src/main.ct lib/std/README.md docs/superpowers/specs/2026-10-09-std-hashmap-design.md website/docs/std/hashmap.md website/mkdocs.yml tests/COVERAGE.md
git commit -m "feat(std): HMap[K,V] 可变泛型哈希映射 W1 落库——…" -- lib/std/hashmap.ct compiler/test/stdpkg/std/hashmap.ct compiler/test/stdpkg/src/main.ct lib/std/README.md docs/superpowers/specs/2026-10-09-std-hashmap-design.md website/docs/std/hashmap.md website/mkdocs.yml tests/COVERAGE.md
```

（提交文案随实施实况写：探针结论、门禁数字、contingency 走向、坑位登记；宪章 3a + 准入四问过门记录入文案。）

- [ ] **Step 11: 提交后核对**

Run: `git status --short && git log --oneline -3`
Expected: 工作树只剩对端泳道在途文件（gui_parse.ct、pkgs/gui/*、tools/std_doc.py、website/docs/std/* 对端页、website/docs/{http,net}/、l4_tmp.bin）——**我方文件零残留**。
