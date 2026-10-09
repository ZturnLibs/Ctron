<!-- 英文译件:手维护;中文正典 = docs/spec/ 同名文件(经 tools/sync_site_spec.sh 同步至同名 .zh.md) -->
<!-- 译件滞后于正典修订时,以中文正典为准 -->
# §10 Diagnostics & Conformance

## 10.1 The Error-Code Registry (the v0.3 Master Registry)

Segments: E1xxx parsing / E2xxx types / E3xxx memory & concurrency / E4xxx effects / E5xxx modules & configuration (dual domain: E5010–5030 modules; E5040–5050 CTCL configuration, frozen as of the host-line release) / E6xxx comptime / E7xxx FFI/ABI (a new segment; the existing E404x code slots are frozen in place, and all new FFI/ABI codes go into E7 from now on) / W8xxx lint.

| Code | Meaning | Normative basis | Test anchor |
|---|---|---|---|
| E1001 | parse error (generic syntax violation; includes non-chainable comparisons, §4.3) | §1 | `01c_parse.neg.ct` |
| E2010 | type mismatch | §3 | generic |
| E2020 | unresolved name | §2.4 | generic |
| E2030 | match not exhaustive | §4.6 | `02_match_exhaustive.neg.ct` |
| E2040 | literal exceeds the expected integer type's width (the §3.7 adaptive width constraint; four sites — let annotation / assignment / return / argument; unsuffixed literals, decimal and 0x/0o/0b under the same policy, v0) | §3.1.1 | `compiler/test/fx_litfit_neg.ct` and three more (with as/ret/arg hook points) |
| E2050 | bound not satisfied (a generic argument fails the type parameter's bound; the diagnostic carries the argument's type alias) | §3.9.2 (v0.6) | `compiler/test/fx_bound_neg.ct` + `fx_bound_ann_neg.ct` (annotation site) |
| E2060 | type arguments cannot be inferred (the type parameter appears in no argument position while the arguments carry concrete information; annotate explicitly) | §3.9.1 (v0.7) | `tests/04f_infer_missing.neg.ct` + R-line same-shape |
| E2061 | conflicting type-argument candidates (multiple candidates for the same type parameter disagree) | §3.9.1 (v0.7) | `tests/04f_infer_ambig.neg.ct` + R-line same-shape |
| E2070 | break/continue outside a loop | §4.2 (v0.7) | `compiler-rust/tests/fixtures/04e_break_outside.neg.ct` |
| E2071 | break/continue would cross a scope holding Drop locals (statically rejected in v1; the spec text allows lifting this) | §4.2 (v0.7) | `compiler-rust/tests/fixtures/04e_break_drop.neg.ct` |
| E2072 | break/continue crossing a closure boundary | §4.2 (v0.7) | `compiler-rust/tests/fixtures/04e_break_closure.neg.ct` |
| E2080 | assignment to an immutable binding (`let` locals; `var` / parameters / closure parameters / for·match pattern bindings are not covered by this gate — narrowing the exemption surface is left to a later revision) | §4.0 (v0.9) | `tests/04g_let_assign.neg.ct` (three lines: bootstrap cc / C host / R line) |
| E3010 | spawn captures a non-Send value | §7.4 | `06_spawn_nonsend.neg.ct` |
| E3020 | channel send/receive of a non-Send type | §7.4 | `06_channel_nonsend.neg.ct` |
| E3030 | `static var` does not exist (the parser recovers on `static var` and emits this code specifically, rather than an E1xxx — friendly to AI iteration) | §7.6 | `06_static_var.neg.ct` |
| E3031 | a non-Send type as global/static storage | §7.4 | `06c_static_nonsend.neg.ct` |
| E3040 | a GC/String allocation in a no_alloc context | §6.5 | `05_own_alloc.neg.ct` / `08_bare_alloc.neg.ct` |
| E3050 | own-block move/borrow violation (including use-after-move) | §6.3 | `05_own_move.neg.ct` |
| E3060 | a mutable borrow of a GC value inside an own block | §6.3 | `05e_own_gc_mut.neg.ct` |
| E3070 | closure mutable capture without explicit `Mutex[T]` wrapping | §4.7 (T27 final) | `roadmap/r3a_capture_var.neg.ct` |
| E4010 | capability use exceeds the manifest declaration | §8.2 | `modules/caps` + `modules/caps_fs` |
| E4020 | `#[pure]` contains side effects | §8.3 | `07_pure.neg.ct` |
| E4030 | spawn inside a `#[no_spawn]` context | §8.3 | `08b_nospawn.neg.ct` |
| E4040 | `#[trusted]` restricted to extern "c" declarations | §9.6 (v0.6) | `compiler/test/fx_trusted_fn_neg.ct` |
| E4041 | `#[repr(c)]` applied to a non-struct declaration | §9.6 (v0.6) | `tests/ffi/repr_on_fn.neg.ct` |
| E4042 | a capturing closure as a C-ABI callback argument (no env slot) | §9.6 (v0.6) | `tests/ffi/closure_cb.neg.ct` |
| E4044 | variadic parameters (`...`) restricted to extern declarations | §9.6 (v0.7) | `tests/ffi/variadic_nonext.neg.ct` |
| E4050 | a class directly holding resource fields that need deterministic release (Mutex/Channel) (the §6.2 hard rule) | §6.2 (v0.7 GC spike) | `compiler/test/fx_res_class_neg.ct` |
| W8050 | extern "c" without the `#[trusted]` marker (trust boundary) | §9.6 (v0.6) | `compiler/test/fx_trusted_neg.ct` + `tests/ffi/no_trusted.lint.ct` |
| W8051 | a repr(c) struct containing non-C-ABI fields | §9.6 (v0.6) | `tests/ffi/repr_unsafe_field.lint.ct` |
| W8052 | extern parameters/returns of non-C-ABI types | §9.6 (v0.6) | `tests/ffi/ext_nonabi_param.lint.ct` |
| W9001 | fn top-level statement count over the limit (the official lint sample code; third-party W9xxx codes may be used only after being declared in the manifest plugin block's codes; diagnostic wording ships with the plugin) | §10.6 (T52) | `tests/plugins/lint_demo/src/main.ct` |
| W8053 | extern returning fn types (warned in v0.6; the return direction legalized in v0.7, **dormant**, code slot retained) | §9.6 (v0.6/v0.7) | — (dormant) |
| E5010 | trait orphan-rule violation | §2.5 | `modules/orphan` |
| E5020 | circular dependency | §2.6 | `modules/circular` |
| E5030 | use importing a same-named decl (previously silently shadowed as "first wins"; now intercepted) | §2.6 | reserved: multi-file use case |
| E5054 | dependency artifact digest mismatch (required digest ≠ actual self_digest; closed-source distribution D8-2 L2 record-and-compare, replacing a silently wrong version) | closed-source distribution spec D8 | `compiler/test/smoke.sh` 3p leg |
| E5055 | artifact digest unverified (meta lacks self_digest or a dep.digest record; fail-closed refusal to load) | closed-source distribution spec D8 | `compiler/test/smoke.sh` 3p leg |
| E5056 | trace replay mismatch (golden-trace expectation ≠ artifact behavior; the closed-source distribution S3 trust protocol; current vehicle = the ctron pkg verify --deep tooling surface, load-time leg reserved) | closed-source distribution spec D3/§5 | `compiler/test/smoke.sh` 3u leg |
| E5057 | attestation does not match the artifact (artifact_digest/trace_count drift or forgery; closed-source distribution S4-② release notarization; vehicle = the ctron pkg verify --deep tooling surface) | closed-source distribution spec §5.4 | `compiler/test/smoke.sh` 3x leg |
| E6010 | comptime budget exceeded | §8.4 | `roadmap/r6f_comptime_budget.neg.ct` + `modules/comptime_budget` |
| E6020 | comptime side effects / nondeterminism | §8.4 | reserved |
| E5060 | plugin not declared / plugin package failed to load / interface-surface mismatch (nodecl/load/iface subcodes; the T52 plugin sandbox) | §8.3/§10.6 (T52) | `tests/plugins/no_decl/src/main.ct` (nodecl) |
| E6040 | monomorphization instance budget exceeded (§8.5: generic fn explicit specializations + generic struct instances above the cap, with `&Trait`-ification recommended; CTRON_MONO_BUDGET adjustable, default 8192; v0 counts explicit TypeArgs positions, with inferred positions joining the count as they get registered via §3.9.1 ex_ty) | §8.5 (v0.9) | `compiler/test/fx_mono_budget_neg.ct` (smoke emit arm) |
| E6030 | comptime reflection over generic runtime types (parametricity) | §8.4 | reserved → **carried by parametricity** (2026-09-28 argument: the language has no reflection operators / the ceval value domain is integers only / type names cannot serve as values, so violations are unreachable; when type-level comptime or a reflection API is introduced, a live test must be added — see tests/COVERAGE.md item T06) |
| W8010 | a struct containing a mutable class-reference field (copies share shallowly) | §6.1 | `03_shallow_copy.lint.ct` |
| W8020 | a must-use result discarded (Result/Option) | §5.6 | reserved (already emitted, anchor pending) |
| W8030 | unused binding | — | reserved (already emitted, anchor pending) |
| W8040 | shadowing a prelude symbol | §3.8 | reserved (already emitted, anchor pending) |

- Once released, a code **never changes meaning**; deprecation only adds and never removes; new codes enter this table first, then get used (kept in sync with the `tests/meta_check.py` registry).
- Every diagnostic must contain: a stable code, a human-readable message, and a **machine-executable fix suggestion** (fix-it).

## 10.2 The JSON Diagnostic Contract (`ctron check --format=json`)

The first interface for agent-loop consumption; the format is frozen:

```json
{
  "diagnostics": [{
    "code": "E3010",
    "severity": "error",
    "message": "closure captures non-Send value `c`",
    "file": "src/main.ct",
    "span": {"line_start": 12, "col_start": 20, "line_end": 12, "col_end": 25},
    "notes": ["`Cell` has a `var` field `n` and is confined to one task"],
    "fixes": [{"title": "wrap in Mutex", "edits": [{"kind": "replace", "span": {...}, "text": "Mutex[Cell](...)"}]}]
  }]
}
```

- `severity ∈ error|warning`; `fixes[].edits.kind ∈ replace|insert|delete`; spans are 1-based.
- A single `ctron check` completes parse + types + Send + allocation effects + lint, returning all diagnostics in one pass (P3/§8.3).

## 10.3 Determinism and Reproducibility

- `ctron test --deterministic`: freezes the scheduling order and the hash seeds; concurrent-test failures are reproducible.
- Content-addressed build cache: same input, same artifact (reusable across machines).

## 10.4 doc-test

- Code blocks inside `///` doc comments are **compiled and run** (failure = test failure); the docs are the regression suite, serving the "immediately verifiable" human review of AI-produced code.

## 10.5 Spec↔Test Conformance Mapping (Master Table)

| Spec section | Topic | Anchor tests |
|---|---|---|
| §1 | lexing/grammar | `01_basics.ct`, `04_generics_comptime.ct` |
| §3/§6 | values/references/Box/shallow copy | `03_values_refs.ct`, `03_shallow_copy.lint.ct` |
| §4/§5 | expressions/error model | `01_basics.ct`, `02_option_result.ct`, `02_match_exhaustive.neg.ct`, `01_overflow.panic.ct` |
| §6 | own/bare/allocation effects | `05_own.ct`, `05_own_alloc.neg.ct`, `05_own_move.neg.ct`, `08_bare.ct`, `08_bare_alloc.neg.ct` |
| §7 | Send/concurrency | `06_concurrency.ct`, `06_spawn_nonsend.neg.ct`, `06_channel_nonsend.neg.ct`, `06_static_var.neg.ct` |
| §8 | capabilities/purity | `07_capabilities.ct`, `07_pure.neg.ct` |

Conformance definition: **a P1-conformant implementation = every test in the table above passing under the `tests/README.md` rules**. Multi-file cases (modules/orphan/FFI/packages) are added as P1 infrastructure lands; the format stays unchanged.

## 10.6 Plugin Extension Points (Diagnostics-Related)

- lint plugins: the input is typed HIR, the output is diagnostics carrying registered codes (the W9xxx code segment is reserved for third parties); sandboxed execution, determinism, cacheable (§8 plugin principles).

> **Revision note (2026-10-03, T52 v1 landed)**: a lint plugin = an ordinary Ctron package, with the conventional entry point `ctron_lint(LintUnit) -> List[LintDiag]`; the input = a declaration-level projection (fn signatures / statement counts / nesting depth; **the v1 ruling on HIR exposure width = a narrow-surface projection that does not expose the compiler's internal AST**, with the expression-level surface deferred to a v2 ruling). **Projected onto third parties, the "enter the table first, then use" code discipline reads: the code first enters the consuming manifest's plugin block `codes`, then gets used** (codes outside `codes` are dropped with a warning); the official sample code W9001 enters the diagnostic catalog. Determinism is a hard anchor (two runs identical word for word); diagnostic caching (content-addressed) is listed for v2. Execution point = after the check driver's sema is fully green; W-level does not set the rc.
- Third-party codes must fall within W9xxx; the E segment is reserved for the language.

## 10.7 Freeze-List Review

The v0.3 spec freeze = the normative items of §1–§9 of this document + the §10.1/§10.2 contracts. Spec revisions during implementation follow the order "edit this document first + add the test anchors, then change the implementation" (tests first).
