<!-- 英文译件:手维护;中文正典 = docs/spec/ 同名文件(经 tools/sync_site_spec.sh 同步至同名 .zh.md) -->
<!-- 译件滞后于正典修订时,以中文正典为准 -->

# §7 Concurrency

Model: **colorless** — no async/await keywords, all code is a single color; suspendable points are recognized by the compiler and the runtime.

## 7.1 Tasks and Scheduling

- `spawn` starts a **lightweight task** (a stackful coroutine); scheduling = work-stealing; the stack is a **growable contiguous stack (copying)** with a default 1MB limit (configurable); hitting the limit = a panic at the task boundary; the bare profile uses static stacks + a guard page (overflow = trap).
- The scheduler guarantees no fairness or real-time behavior; hard real-time goes through the bare profile (§9.3).

## 7.2 Structured Concurrency

```c
let total = scope { |s|
    let t = s.spawn(|| compute())
    t.join()                    // join() -> T; rethrows if the task panicked
}
```

- `scope` is an expression; at scope exit **all child tasks are necessarily joined** (value on success / propagation on failure).
- Cancellation is a first-class citizen: a child task fails → sibling tasks are cancelled → the error propagates up the scope tree; cancellation is implemented via task-level cancellation tokens, and blocking points (channels/locks/sleep) respond to cancellation.
- Channel operations on an already-cancelled scope return `Err(ScopeCancelled)` — no panic, no silence.
- `join() -> T`: panic-rethrow semantics; `join_or() -> Result[T, TaskPanic]` for explicit handling.

## 7.3 Channels and Shared Primitives (Prelude)

```c
let (tx, rx) = Channel[T](cap)     // bounded; send/recv -> Result (backpressure/cancellation explicit)
let m = Mutex[T](value)            // m.with_mut(|var x| ... ) exclusive access, returns the closure's value; m.with(|x| ...) read-only
Atomic[I32]                        // atomic integer family (fetch_add etc., stdlib)
```

- Channels are bounded by default (capacity required) — backpressure is explicit (P5).
- `Mutex.with` guarantees the critical section is never left unlocked (closure scope).

## 7.4 Send (the Core Mechanism for Eliminating Data Races)

**Definition (inferred automatically by the compiler; users cannot implement it by hand)**:

| Type | Send if and only if |
|---|---|
| Scalars / value-type struct/enum/tuple/`T[N]` | All fields/elements are Send |
| `class` | **All fields are `let` and every field type is Send** (deeply immutable) |
| Closures | **Checked one by one against the literal captures at the spawn site** (E3010); passed via channels/static storage = not Send (captures unknowable) |
| Values of `fn(...)` function types | Same as closures: decidable at the literal site; via channels/static storage = not Send |
| `&T[]` read-only slices / `Str` | Element type is Send |
| `T[]` mutable slices | **Never Send** |
| `&Trait` | **Never Send in v0.3** (conservative ruling: the concrete type is erased and statically undecidable; a dynamic Send bit is reserved for v2) |
| `Mutex[T]` `Atomic[T]` `Global[T]` | Always Send (T arbitrary) |
| Classes with any `var` field | **Not Send** |

**Enforcement checkpoints (three of them, hard compile-time checks)**:

1. Every value captured by a `spawn` closure must be Send → violation E3010;
2. The `T` of `Channel[T]`/`Sender[T]`/`Receiver[T]` must be Send → violation E3020;
3. Non-Send types may not serve as global/static storage → violation **E3031**.

**Guarantees that follow** (with no `#[trusted]` involvement): any data touched by two tasks concurrently is either deeply immutable or behind a lock — **all references to a non-Send instance are naturally trapped within a single task** (they cannot escape), and within the task mutation is free and race-free. Data races are eliminated at compile time, with no lifetime annotations.

Relation to existing languages: the same goal as Rust's Send, no lifetimes involved, errors mechanically fixable ("field x is var → add a Mutex or change it to let"); the same shape as Swift's Sendable but with **hard checks** (no ObjC interop baggage).

## 7.5 Task Locality of Mutable Access

- Mutable use of `var self`/`var` fields requires path exclusivity: since non-Send types cannot cross tasks, a reference within a task is exclusive (guaranteed by §7.4); the only channel for mutable use across tasks is `Mutex.with`.
- Accessing a mutable field through a `let` binding = compile error (a mutable path must be rooted in `var`).

## 7.6 Global State

- `static let NAME: T = constant/pure lazy value`: legal; initialization is either a comptime constant or `#[pure]` lazy evaluation on first access (thread-safe once; the full/web profiles allow pure allocation inside it, §6.5). **The bare profile allows only comptime constants**.
- **`static var` does not exist** (E3030); the only path to a mutable global is explicit registration via `Global[T]` (a Mutex inside), visible in the manifest capability audit (§8.2).

## 7.7 Data Parallelism (Independently Modeled)

- `parallel.map / reduce / fold` (stdlib, `iter` module): fork-join + work-stealing, never mixed with I/O tasks (Rayon as evidence); closures are judged by **inferred purity** (they capture no `&Cap` capabilities, do not spawn, do not touch global mutable state — same mechanism as §8.3, no annotation syntax needed) and their captures are Send; data inputs are `&T[]` read-only views (§3.1).
- Automatic SIMD vectorization and chunking; under the deterministic mode (§10.4) chunk order is fixed.

## 7.8 web Profile Differences (Normative)

- Default single-threaded event loop: JS callbacks do not construct parallel races — Send checks run as a "compile-time decidable approximation" (sharing relaxed, blocking disabled); full checks resume once wasm threads are enabled.
- JSPI/stack switching carries task suspension; semantics identical to the full profile (§9.2).

## 7.9 Corresponding Tests

`tests/06_concurrency.ct` (Send positive cases/channels/Mutex), `tests/06_spawn_nonsend.neg.ct` (E3010), `tests/06_channel_nonsend.neg.ct` (E3020), `tests/06_static_var.neg.ct` (E3030).

## 7.10 Async Execution Model (server profile, normative, v0.8)

- Facade IO (§11.2) always has **blocking semantics**; two forms actually carry it: the P1 blocking runtime (1:1 threads) and the P2 coroutine runtime (N:M, §7.1 stackful coroutines). **Same-shape, different-carrier contract**: the same source code with zero changes, with identical observable semantics on both runtimes; pinned down by mechanical invariant tests (must run at every wave exit).
- **Suspension-point contract**: network facade calls, `sleep`, and channel operations are the only suspension points; recognized by the compiler and the runtime, imperceptible and unannotated in user code (the semantic landing point of §9.5 "colorless APIs").
- **Transition note**: the §7.1 "growable contiguous stack + work-stealing" scheduling face landed in code on 2026-10-02 (a per-worker ring double-ended local queue + neighbor-rotation stealing; the seed mode is fully bypassed to preserve determinism); the stack face landed on 2026-10-03 (the emitter's morestack prologue machine: A′ segmentation + call-window reclamation + resume-on-reentry, gated at emission time by `CTRON_MORESTACK`; the default path is the literal final shape of a 1MB VA limit + guard + panic on hitting the limit, with the limit configurable). The observable semantics of both faces are byte-for-byte identical to before the transition (the same-shape contract stands guard with mechanical tests); C100K/C10M claims are governed by measured results.
- **Cancellation**: network operations respond to the §7.2 cancellation token at suspension points and return `Err(NetErr::Cancelled)`; on the blocking runtime the equivalent is join semantics.
- **FFI discipline**: `extern "c"` callbacks must not touch the network facade/channels/sleep (coroutine stacks are not reentrant); violators get a debug assertion + a declared-undefined-behavior statement.
