<!-- 英文译件:手维护;中文正典 = docs/spec/ 同名文件(经 tools/sync_site_spec.sh 同步至同名 .zh.md) -->
<!-- 译件滞后于正典修订时,以中文正典为准 -->

# §6 Memory Model

Goal: the default tier keeps generation smooth and reading clean (GC); performance and bare-metal scenarios go through the explicit channel (own/bare). Hard targets: own/hot paths stay within 5% of C in both directions; ≤15% for the GC profile is a target with exit conditions attached (§9.4).

## 6.1 Value/Reference Dichotomy

- `struct`/`enum`/tuples/fixed-size arrays/scalars = **value types**: assignment copies, stack/inline storage; the compiler eliminates copies provided observable semantics are unchanged.
- `class` = **reference type**: GC heap; variables hold references; fields default to `let`, `var` is explicit.
- `Box[T]`: explicitly heap-boxes a value type; access auto-dereferences; `Box` variables copy by handle — **assignment/argument passing/binding share the heap cell, and mutable field writes through aliases are fully visible** (§3.11.1, v0.6 refinement; supersedes the vague v0.5 wording of "exclusive ownership").

## 6.2 GC Contract (full/web Profiles)

- Reachability-based reclamation: no leak problems from reference cycles; precise stack scanning.
- Pause targets: P99 < 0.5ms (concurrent generational); allocation goes through thread-local size classes (typical small object ~10ns).
- **No finalizers**: the GC never runs user destructors; resources must be RAII (§6.4). **Hard rule: a class must not directly hold resources requiring deterministic release** (files/locks/sockets); resources may only be held by value-type handles or capability objects — violators get an E-level lint.
- Within a profile the GC is pluggable (concurrent generational by default / RC on mobile), with zero impact on language semantics.
- **Capability extension slot (T27)**: sharing closure-captured variables by reference (closure environments into the GC heap, outer reassignment visible to the closure) is reserved as an on-demand project once the GC matures — prerequisite = the precise root set (M1.5); it does not conflict with the §4.7 copy-capture end state.

```c
own (arena) {
    var buf = arena.array[I32](8)      // explicit arena allocation
    ...
    return acc                          // block-exit rules below
}
```

- `own (id) {}` introduces a brand-new arena; the whole arena is released at block exit (one free); nesting is allowed (arena parent-child chains).
- **GC allocation is forbidden inside the block** (E3040, mechanism in §6.5); GC values enter read-only (a mutable borrow of a GC value = E3060).
- **Move semantics**: arena handles (the return values of `arena.array/zeros/list`) are **move-only** — assignment is a move, using a moved value = E3050; the other in-block values are Copy (scalars/Copy structs).
- **Borrow checking (a decidable subset)**: for a given arena object, at any moment either "multiple read-only borrows" or "a single mutable use" — the two never coexist; violation = E3050. The check covers local flow within the block, and inference **usually succeeds**; on failure the diagnostic gives mechanical downgrade advice (copy the value / switch to a GC value / lift it to a parameter); **no lifetime annotations are introduced**.
- **Block-exit rules**: only (a) Copy values; (b) an explicit `into_gc()`.
  - `into_gc()` v1 normative semantics: a **deep copy** into the GC heap (O(data size), explicit at the call site); arena promotion (a single pass, no copying) is an allowed optimization (v2), semantically equivalent.
  - Design implication: own suits "big in, small out" workloads; constructing a large result to return pays an exit copy in v1.
- own blocks are available in all profiles (used for hot-path optimization within the full profile).

## 6.4 RAII and Deterministic Destruction

- Value types may implement the `Drop` trait (`fn drop(var self)`), executed **deterministically**: at scope exit in reverse declaration order, and guaranteed to run during panic unwinding.
- panic message stream policy (both arms consistent): panic/`assert`-family messages are **written to stderr immediately** (a panic inside a task body is written at the panic point, `join` then re-emits it as a second output, `join_or` does not reprint); `println` inside a `Drop` body goes to stdout. `eprint(s)` is a prelude builtin: writes to the host stderr immediately, no newline, does not enter the output buffer — the only capability gateway for separating diagnostics from the stream.
- Class references are reclaimed by the GC and do not trigger `Drop` — this is the basis for the §6.2 hard rule.
- `Arena` itself is a value type; its `Drop` releases everything at once.

## 6.5 The alloc Effect

- Every function carries a compiler-inferred attribute: **`alloc`** (includes GC allocation: constructing class instances/`String`/`List`/`Box`, growth operations, interpolated strings) or **`no_alloc`**.
- Inference: the function body allocates directly → `alloc`; it calls an `alloc` function → `alloc`; **dynamic dispatch through `&Trait` is undecidable → conservatively `alloc`**.
- A trait method may be explicitly annotated `#[no_alloc]` as a **contract**: all impls must be `no_alloc` (violators = E3040); used to publish allocation-free interfaces (ISR callbacks and the like).
- The attribute costs users nothing to write (inferred by default) and is shown in signature docs and LSP hover (P5).
- **Enforcement points**: inside own blocks, in `#[no_alloc]` function bodies, in all functions of the bare profile, and in **`static let` initializers of the bare profile** → calling/exhibiting `alloc` = E3040. (`static let` in the full/web profiles allows allocation inside `#[pure]` lazy initialization, §7.6 — allocation by a pure function is unobservable, §8.3.)

## 6.6 Memory in the `bare` Profile (Profiles Detailed in §9.3)

- No GC, no implicit allocation; **all memory goes through explicit allocator parameters** (`arena: Arena`, etc.).
- The `Arena.fixed(n)` / `Region` / `Pool` / `Static` allocator family; `core`-layer containers are usable (just pass an arena).
- ISRs default to the `#[no_alloc] #[no_spawn]` constraints.
- The hard real-time path: zero implicit allocation + no GC pauses + deterministic builds.

## 6.7 Memory Safety Guarantee (safe Subset)

Without `#[trusted]` involvement, compiled artifacts **guarantee**: no use-after-free, no unauthorized out-of-bounds access (bounds checks), no data races (§7.4), no reads of uninitialized memory, checked arithmetic by default. `#[trusted]` is the only exemption gate (§9.6), audited at the package level.

## 6.8 Corresponding Tests

`tests/03_values_refs.ct` (values/references/Box), `tests/03_shallow_copy.lint.ct` (W8010), `tests/05_own.ct` (own positive cases), `tests/05_own_alloc.neg.ct` (E3040), `tests/05_own_move.neg.ct` (E3050), `tests/08_bare.ct` and `tests/08_bare_alloc.neg.ct` (the bare profile).
