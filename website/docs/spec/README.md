<!-- 英文译件:手维护;中文正典 = docs/spec/ 同名文件(经 tools/sync_site_spec.sh 同步至同名 .zh.md) -->
<!-- 译件滞后于正典修订时,以中文正典为准 -->

# Ctron Language Spec v0.8 (§11/§12 v0.8; remaining chapters v0.7)

Status: **frozen draft** — this document is the normative specification of the Ctron language (the P0 phase exit deliverable). Implementations (from P1 on) follow this document; conformance is accepted against [`tests/`](https://github.com/ZturnLibs/Ctron/blob/main/tests/README.md).

> **v0.6 revision (2026-09-11)**: the generics system is codified — §3.9 instantiation/bound/nesting semantics (explicit TypeArgs at call sites, E2050 structured bound checking, TPar propagation, recursive specialization diagnostics); new §3.11 `@derive(Show, Eq)` structured methods and format contracts; E2050 added to the §10 registry. Basis: the self-hosted compiler's (`compiler/`) deep-generics implementation plus the corpora `tests/03e_generics_types.ct`, `compiler/test/fx_derive.ct`, `fx_generic.ct`, `fx_bound_neg.ct`, `fx_genrec_neg.ct`. The v0.5 frozen scope is unaffected; §4.7 (v0.6 draft, E3070) remains a draft.

> **v0.7 revision (2026-09-12)**: three notational relaxations plus the §4.0 operator constitution — ① new **§4.0 operator constitution** (one meaning one symbol / the symbol–keyword division-of-labor law / the never-repurpose law / the role separation law); ② **`||` logical or** (§4.3 precedence level 0, §4.4 rewritten with a veto note attached, §1.5 token table and §1.6 continuation set extended; zero-parameter closures disambiguated by syntactic position, §4.7); ③ **break/continue promoted to keywords** (§1.3 reserved-word list shrunk, §4.2 loop semantics, the three static gates E2070/E2071/E2072); ④ **generic call-site type inference** (§3.9.1 two-phase contract, Go-style arguments only, E2060/E2061). Full design text and the record of rejected alternatives: internal design documents in the repository. Landing status: **fully landed on all three lines** — the R line (the oror/breakc/infer suites), the self-host line (revision 1 15e5b00 / revision 2 9d56b73 / revision 3 866cb1b, name-level inference at bare type-parameter positions; the structured form awaits ex_ty retaining arguments), and the C line oracle (the minimal surface of both revisions + the eval_block flow-skip fix + direct calls of user fns with TypeArgs); E2071/E2072 and the C line's inference neg surface remain future work. The v0.6 frozen scope is unaffected (all items are new extension slots).

> **v0.8 revision (2026-09-20)**: new §11 networking & server profile and §12 data access profile (finalized); §7.10 asynchronous execution model; §8.2/§2.7 server key sets. Design: internal design documents in the repository (v3). §12's implementation anchoring starts with P5; §11's with P1.

> **T27 final-state revision (2026-10-02)**: §4.7 closure capture is finalized — capture = **copying the bound value at creation time** (later reassignment of the outer binding is invisible); mutable sharing only via the explicit units `Mutex[T]`/`Atomic[T]`/`Global[T]`; E3070 = semantic enforcement of this rule. Capture by reference is left as a §6.2 capability extension slot (to be scoped as needed once GC matures). The v0.6 note that "§4.7 remains a draft" is hereby overturned. For the ruling record and industry alignment, see the T27 card in internal design documents in the repository.

> **let immutability gate revision (2026-10-08)**: the immutable semantics of §4.1 `let` bindings are now enforced by the implementation — reassignment to a `let` local (including the `+=` family) = **E2080** (new code, registered in §10); the gate criterion is the innermost binding of the assignment's left-hand target, with `var`/parameters/closure parameters/for·match pattern bindings listed as the exemption surface (narrowing is left to a later revision). Pinned ruling: `tests/04g_let_assign.neg.ct`.

## Normative Conventions

- **Must / Must not**: hard requirements on implementations and programs; violation = compile error or runtime contract breach.
- **Should**: the default requirement; explicit exemptions are allowed (exemption points are listed in the text, e.g. `#[trusted]`).
- **May**: permitted freedom.
- "Diagnostics" refers to errors/warnings produced by the compiler, always carrying a stable error code (§10).
- Relationship to historical documents: design doc v0.2 (internal design documents in the repository) records design motivation; where the test pinned rulings (`tests/README.md` §3) conflict with this document, **this document** prevails.

## Chapter Map

| Chapter | Contents | In one sentence |
|---|---|---|
| §1 | Lexical & grammar | source encoding, tokens, the full EBNF, newline termination rules |
| §2 | Names & modules | packages/modules, visibility, imports, the orphan rule, the cyclic-dependency ban |
| §3 | Type system | type kinds, traits, generics, inference, the standard prelude |
| §4 | Expressions | precedence, control flow, closures, match, UFCS, test blocks |
| §5 | Error model | Option/Result, `?`, panic, the Error trait, error chains |
| §6 | Memory model | value/reference dichotomy, GC contract, RAII, own blocks, allocation effects, the bare profile |
| §7 | Concurrency | tasks, structured scopes, channels, Mutex, the three Send checkpoints |
| §8 | Effects & comptime | capability objects, annotation contracts, compile-time execution |
| §9 | Profiles & interop | full/web/bare, the target matrix, C ABI/FFI, the JS bridge |
| §10 | Diagnostics & conformance | the error code registry, the JSON diagnostic contract, the spec↔test mapping, the frozen scope |
| §11 | Networking & server | capability keys, the socket facade, transport defaults, the same-shape heterogeneous contract, HTTP profile layering |
| §12 | Data access | the db.connect capability key, the pure-Ctron wire-protocol driver contract, connection pooling, comptime row mapping |

## Frozen Scope Statement (v0.4)

The following is **normative** and implementations must comply: all of §1–§7 (including the v0.4 additions of function types and the slice dichotomy); §8's `#[pure]`/`#[no_alloc]`/`#[no_spawn]`, the Cap marking mechanism, and comptime constant evaluation; §9's three-profile model and C ABI ownership conventions; §10's error codes and diagnostic schema; §3.8.2's minimal prelude API list.

The following is **reserved (non-normative, does not block P1)**: type-level comptime (type-producing functions, §8.4), GPU/`kernel` blocks, editions evolution details, Unicode identifiers, raw strings, owned trait objects (`Box[&Trait]`), the `&Trait` dynamic Send slot, `debug_assert`.

## Terminology Quick Reference

| Term | Definition |
|---|---|
| profile | one of the three runtime configurations `full` / `web` / `bare` (§9.1) |
| value type / reference type | `struct` copies on assignment / `class` shares on assignment (§6.1) |
| Send | a compile-time type property allowing migration across tasks (§7.4) |
| allocation attribute | a function's inferred `alloc`/`no_alloc` attribute (§6.5) |
| capability object | an explicitly injected I/O permission value (§8.1) |
| own block | a scoped ownership subset with no GC memory (§6.3) |
| pinned ruling | a syntactic ruling pinned by the test suite ahead of the implementation (now merged into this spec) |

## Revision History

- **v0.3 → v0.4 (2026-09-04, review revision: expressive completeness and three-way consistency)**:
  1. **Function types** (§3.1/§4.7/EBNF): new type syntax `fn(Params) -> Ret` (parameter/return positions only) — fixes the gap that "closure parameter types could not be expressed and the prelude could not declare them"; `Mutex` split into `with` (read-only) / `with_mut` (mutable).
  2. **Slice dichotomy** (§3.1/§3.6/§4.2/§7.4): `T[]` mutable view (writable when the root is `var`, **never Send**) / `&T[]` read-only view (Send iff the element type is Send); `T[] → &T[]` implicit; `parallel.map` takes `&T[]` — eliminating the data-race hole of "mutable views crossing tasks".
  3. **`&Trait` Send conservatism** (§7.4): in v0.4, always non-Send (dynamic Send slot reserved), preserving the promise that "all three checkpoints are statically decidable"; non-Send static storage split out as **E3031**.
  4. **`static let` three-way contradiction resolved** (§6.5/§7.6): E3040's enforcement point narrowed to the bare profile; full/web allow `#[pure]` lazy-initialization allocation.
  5. **Keyword table fixes** (§1.3): removed leftover rows; `as` is not a keyword (`.as[U64]()` is legal); `or` moved to the reserved operator words; a reserved-word list added.
  6. **Newline rules completed** (§1.6): added "a line does not terminate if the next line starts with `.` or a binary operator" — multi-line method chains (leading-dot style) are legal, a trailing `.` at end of line is illegal.
  7. **De Morgan fix** (§4.4): the correct spelling of logical or is `!(!a && !b)` (the original `!(a && b)` was a mathematical error).
  8. **Capability determination mechanism** (§3.8.2/§8.3): the prelude marks `trait Cap`, and capability traits must inherit from it; `#[pure]` = no `&Cap` calls — E4020 becomes decidable; `parallel` closure purity changed to inferred.
  9. **Minimal prelude API list** (§3.8.2, normative): the P1-essential members such as Option/Result/Show/Eq/Error/Cap/Arena/Mutex/Channel/Task/fmt; `Task[T]` added to the prelude.
  10. **EBNF completion** (§1.7): `pub(pkg)` visibility, `@derive`/attributes wired into type declarations, trait supertraits (`:` Bound), `PathPattern` de-duplicated; §1.8 rewritten with the disambiguation rules for `IDENT {` (always a construction literal) and `IDENT [` (generic instantiation iff immediately followed by `(`/`{`).
  11. **E3030 reachability** (§10.1): the parser must recover from `static var` and emit E3030 (rather than E1xxx).
  12. **Miscellaneous**: the `%` sign follows the dividend (§4.5); the `barer`→`bare` typo; ISize/USize comments put in place (§3.1); meta_check drops the undocumented `profile` key; test fixes — the `06_concurrency.ct` Mutex case's original assertion was scheduler-dependent (always failing at 52/74), changed to read the final state; `Clock` in `07_*.ct` annotated `: Cap`.

- **v0.4 → v0.5 (2026-09-04, covering the closing pinned rulings, target = 100% language conformance tests)**:
  1. FFI syntax enters the spec: `extern` added to the keyword table; EBNF `FnDecl` supports `"extern" STRING_LIT` and allows omitting the function body; §9.6 adds a declaration example.
  2. Error erasure type pinned: prelude `AnyError` (a class implementing Error); `context(msg) -> Result[T, AnyError]`; `?` auto-erases toward AnyError return types (the only built-in form of §5.3's "convertible"); the `Error` trait gains `prop trace: Str`; the location chain becomes the **record/materialize two-phase scheme** (§5.3).
  3. Prelude patch pins: `Simd[E, N].splat/lane/to_array` plus element-wise whitelisted operations, `Str.contains`; §9.2 pins the minimal stdweb API (`dom.set_title/title`).
  4. Tests: the fourth batch completes Simd/trace/stdweb anchors/FFI (c_src)/`} else {` formatting/explicit Void; the `middle` signature in `05i_deep_cause.ct` corrected per the AnyError design; the multi-file format gains a `c_src/` rule (README §6). Coverage is accounted in three layers: language conformance (.ct) = 100%, toolchain behavior belongs to compiler integration tests, performance/size belongs to CI gates.

- **Registration (2026-09-12, implementation account, not a language revision)**: §3.1.1 adds the **I64 value range (v0 implementation account)**, codifying four points — the canonical decimal text value model, truncating division with C99 semantics (the quotient rounds toward zero / the remainder takes the dividend's sign), literals parsed through the I32 range (panic when too wide), and v0 arithmetic having no overflow checks (the two implementations diverge beyond int64 width). Implementation source: feat/i64-arith (d2f6b1d).

- **Registration (proposed 2026-09-16; hard-cut landed 2026-10-02 via T48)**: the package manifest format has completed its migration to **CTCL (Ctron Config Language, `Ctron.ctcl`)** — the normative definition is in internal design documents in the repository; in-repo manifests are 100% migrated and the TOML surface is removed (E5040 migration diagnostics on all three lines + the installer refuses to build fail-closed). Design motivation: three in-repo parsers with three semantics, empirical evidence of `//` dialect drift, and silent defaults violating the diagnostic constitution (see that document's §1 evidence table).
