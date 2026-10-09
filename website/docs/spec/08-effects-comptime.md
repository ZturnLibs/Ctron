<!-- 英文译件:手维护;中文正典 = docs/spec/ 同名文件(经 tools/sync_site_spec.sh 同步至同名 .zh.md) -->
<!-- 译件滞后于正典修订时,以中文正典为准 -->

# §8 Effects & Comptime

## 8.1 Capability Objects (capabilities) — Modeling I/O Effects

I/O-style effects = **capability values that must be held**, with the dependencies appearing in the parameters (P5):

```c
fn read(path: Path, fs: &Fs) -> Result[Bytes]
fn handler(req: &Request, clock: &Clock) -> Result[Response, HttpError]
```

- Capabilities are ordinary values/references (`&Fs`, `&Clock`) and can be composed into traits; tests inject fake implementations (`FakeClock`) with no mock framework needed.
- **No new type system / effect keywords** — the capability parameters are the signature, and the signature is the contract; the known cost is boilerplate for passing capabilities down deep chains; mitigation: construct centrally at the entry point + pass along task-local paths (compile-time decidable, never across tasks; to be refined by an RFC).
- When passing along long chains would pollute library signatures, capabilities can be packaged into a context trait (supertrait composition): `trait Env: Clock + Fs + Log` — receiving `&Env` means holding all three at once.

## 8.2 Capability Audit

- A package's manifest declares capability ceilings (§2.7 `[caps]`); the set the program actually uses ⊆ the declared set; exceeding it = E4010.
- Server-profile key set (v0.8): `net.listen` / `net.connect` / `net.resolve` (§11.1), `db.connect` (§12.1) — semantics identical to the fs keys: the manifest declares the ceiling, the actually-used set ⊆ the declared set, exceeding it = E4010; `#[pure]` touching them = E4020.
- main's capabilities are granted by **runtime initialization** according to the manifest (failing at startup beats overstepping at runtime).
- `Global[T]` mutable globals are included in the audit view (§7.6).

## 8.3 Annotation Contracts (all built-in annotations: just these four + derive)

| Annotation | Semantics | Violation |
|---|---|---|
| `#[pure]` | No `&Cap` capability calls (**capability determination mechanism**: a capability trait must inherit the prelude marker `trait Cap`, §3.8.2; a method call on a `&Cap` receiver is impure), no spawn, no global mutation; allocation allowed (unobservable) | E4020 |
| `#[no_alloc]` | No GC allocation inside the function body (§6.5); on a trait method = an implementation contract | E3040 |
| `#[no_spawn]` | spawn forbidden inside the function body | E4030 |
| `#[trusted]` | Opens unsound operations (FFI/low-level only, §9.6); an enumerable audit at the package level | lint statistics |
| `@derive(A, B)` | Declarative code generation, expanded by derive plugins inside the sandbox (ordinary code, not macro surgery) | plugin diagnostics |

> **Revision note (2026-10-03, T52 plugin sandbox v1 landed)**: a plugin is an ordinary Ctron package (declared in the manifest's `plugin "kind.name"` block; zero changes to the CTCL registry extension grammar); it executes inside the compiler process behind a restricted call surface (reusing the bootstrap interpreter = the full form of the comptime CVM; zero out-of-process plugins / dynamic linking / FFI). The sandbox boundary has three layers: (1) a static purity gate (extern banned + an I/O/clock/concurrency whitelist + a capability-call gate, the E6020.sandbox domain); (2) a static size gate (256 KiB / 512 decls; an execution-time step budget is listed for v2 — `Global[T]` proved to be a single-binding persistent box with no carrier for cross-call counting, the same evolution path as comptime v0→v1); (3) determinism (clock/environment/concurrency banned; same input, same output anchor). Derive plugins agree on the entry point `ctron_derive(DeriveInput) -> Str` (synthesized source text is injected via a re-parse; the artifacts may not recursively contain use/test/@derive); the built-in set {Show, Eq, Error} keeps its v0 declarative stance unchanged. Design doc and HIR exposure-width ruling: `docs/superpowers/specs/2026-10-03-t52-plugin-sandbox.md`.

- The compiler may exploit `#[pure]` for optimization and parallelism proofs; the purity of `parallel.map` closures is obtained by **inference** (same rules as above, §7.7) — no annotation needs to be written on the closure.
- `#[trusted]` counts and locations are reported with the package's release metadata; `ctron lint --trusted` lists every trust boundary.

## 8.4 comptime: Bounded Compile-Time Execution

- `comptime fn` executes at compile time (CVM): **`#[pure]` semantics + a total step budget** (default 1200 steps per compilation unit, adjustable). Over budget = E6010; side effects/nondeterminism = E6020.
  > **Revision note (2026-09-28, T08 user ruling)**: the budget metric changed from the v0.3 draft's "1s time budget" to a **step budget as the v1 end state** — a step count is naturally deterministic and reproducible (same input, same verdict), with no tension against §10.3 deterministic compilation; a time metric depends on the host clock and is not reproducible for the same input, so it was not adopted. The compiler ships with 1200 steps per compilation unit (the E6010 criterion, `sem_ceval.ct` ceval). The manifest key `comptime.budget_ms` (CTCL registry, C-host pkg parsing + fail-closed validation) is a declaration slot and currently does not enter budget enforcement; unifying the unit naming was originally to be settled with the CTCL migration batch (spec-gap T48) — T48 has landed (2026-10-02), and the key name remains `budget_ms` as-is (the declaration slot is inactive, a rename has no consumers; left to be handled together when budget enforcement is implemented).
- `const NAME: T = expr`: expr is evaluated at compile time (it may call `comptime fn`); likewise the constant form of `static let` (§7.6).
- Generic value parameters (`comptime N: USize`, fixed-size array dimension `T[N]`) are v0.3's only type-level comptime; **type-producing functions** (`fn Matrix(comptime N) -> type`) are reserved for v2.
- **Parametricity preserved**: comptime code must not reflect on the runtime types of generic parameters (E6030); type reflection happens only through explicit `@derive` declarations, which plugins expand into ordinary code — ruling out Zig-comptime-style generic reflection.

## 8.5 Compilation Budget (compile-speed protection paired with §8.4)

- The total number of monomorphized instances per package is capped (default 8192, adjustable); when over the limit, the diagnostic suggests erasing to `&Trait`.
- The comptime budget, the instance budget, and acyclic module dependencies (§2.6) together constitute the language-level realization of the "compile-speed veto".

## 8.6 Corresponding Tests

`tests/07_capabilities.ct` (capability injection), `tests/07_pure.neg.ct` (E4020), `tests/04_generics_comptime.ct` (comptime/const).
