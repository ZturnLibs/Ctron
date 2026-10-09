<!-- 英文译件:手维护;中文正典 = docs/spec/ 同名文件(经 tools/sync_site_spec.sh 同步至同名 .zh.md) -->
<!-- 译件滞后于正典修订时,以中文正典为准 -->

# §3 Type System

## 3.1 Type Kinds

| Kind | Syntax examples | Semantics |
|---|---|---|
| Integer | `I8 I16 I32 I64 ISize U8 U16 U32 U64 USize` | Fixed width; checked arithmetic by default (§4.5); `ISize/USize` match the target pointer width |
| Floating-point | `F32 F64` | IEEE-754 |
| Boolean | `Bool` | `true`/`false`; no interchange with integers |
| Borrowed string | `Str` | Immutable UTF-8 view; `.len` (bytes), `.char_len` (characters) |
| Owned string | `String` | GC-heap UTF-8; implicitly downgrades to `Str` |
| Struct | `struct` declaration | **Value type** (§6.1) |
| Class | `class` declaration | **Reference type** (GC heap, §6.1) |
| Enum | `enum` declaration | Sum type |
| Tuple | `(I32, Str)` | Anonymous product; `.0 .1` access; the unit `()` has type `Void` and value `void` |
| Slice | `T[]` | **Mutable view**: length + pointer; elements writable only when the binding root is `var` (§4.2); **never Send** (§7.4) |
| Read-only slice | `&T[]` | Read-only view; Send if and only if `T` is Send (§7.4); `T[] → &T[]` implicit (§3.6) |
| Fixed-size array | `I32[3]` | Inline contiguous value type; may implicitly decay to a slice (§3.6) |
| Fixed-size vector | `Simd[F32, 8]` | Hardware vector (§9.5) |
| Optional | `Option[T]`, sugar `T?` | No null (§5.1) |
| Result | `Result[T, E]` | Carrier for error propagation (§5.1) |
| Reference | `&T` / `&Trait` | Shared **read-only** view / trait object (§3.5) |
| Boxed | `Box[T]` | Explicit GC-heap single value; access auto-dereferences; writes through aliases to mutable payload fields are shared-visible (v0.6 §3.11.1) |
| Function | `fn(I32) -> Bool` | **Function type**: used only in parameter/return-type positions; closure literals are values of this type (§4.7) |
| Bottom type | `Never` | The type of never-returning expressions such as `panic`; usable wherever any type is expected |

- **No `null`, no `any`, no implicit numeric conversions, no gradual typing** (P2; rejection list).

### 3.1.1 I64 value domain (v0 implementation stance)

The type table nominally specifies fixed-width integers; in the v0 self-hosting implementation `I64` is carried as **canonical decimal text** (optionally with a single leading `-`; no leading zeros; `0` is the only zero form), and the emission side maps it to `int64_t`. Semantic anchors:

- **Arithmetic semantics**: `+ -` textual decimal addition/subtraction; `*` decimal long multiplication (sign XOR); `/` `%` are **truncating division** (C99 semantics): the quotient rounds toward zero, the quotient's sign is the XOR of the operand signs, and **the remainder's sign follows the dividend** (consistent with the §4.5 `%` semantics); division by zero panics ("division by zero", never disabled, §4.5).
- **v0 limitations (implementation stance, not a language promise)**:
  1. Integer literals pass an expected-type width gate (E2040, §3.7/§10): unsuffixed literals (same rule for decimal and `0x/0o/0b`; underscore separators stripped) are checked to be in range at four points — let annotations, assignments, returns, and actual arguments — and rejected when out of range; **under an `I64` expectation, literals exceeding `I32` width go directly into the I64 domain** (decimal value); the comptime domain (const folding) follows the same rule.
  2. `I64` arithmetic implements **overflow checking** (interpreter side: a 2^63 bound gate; emission side: `__builtin_*_overflow` helpers; both sides agree; `INT64_MIN / -1`, `INT64_MIN % -1`, and `-INT64_MIN` are all judged overflow, §4.5). Literals in annotated positions exceeding int64 width are rejected by the width gate (E2040); when an unannotated literal exceeding int64 width participates in arithmetic, the interpreter and emission sides diverge — the v0 test fixtures do not cover this.

## 3.2 Value Semantics and Reference Semantics (a reader-visible dichotomy)

- `struct` assignment/argument passing/return **copies the whole value** (the compiler may eliminate redundant copies; observable semantics unchanged); field copies are **shallow**: if a field holds a class reference, the copy shares the same instance — `ctron lint` emits W8010 by default for structs containing `var` class-typed fields.
- `class` assignment **shares the instance**; fields default to `let` (immutable); mutable fields must be declared `var` explicitly. **A class with `var` fields is not Send** (§7.4).
- `enum` is a value type; variant payloads follow the field rules.

## 3.3 `Str` and `String`

- Literals are `Str`, stored in static storage, available in every profile.
- `String` is an `alloc`-layer type; `String` → `Str` is implicit (read-only view); `s.to_string()` upgrades explicitly (allocating, §6.5).
- Slicing indexes by **byte** and must land on a UTF-8 boundary, otherwise panic ("invalid utf8 boundary"); grapheme/code-point iteration is a read-only API.

## 3.4 trait (nominal, explicit impls)

```c
trait Clock {
    fn now(&self) -> U64          // method
    prop name: Str                // property: zero-argument computed read-only value, called without parentheses
}

impl Clock for FakeClock {
    fn now(&self) -> U64 { return self.base }
    prop name: Str { return "fake" }
}
```

- **Nominal**: methods take effect only through an explicit `impl`; see §2.5 for the orphan rule.
- A trait may contain default method bodies / default properties.
- **Properties (`prop`)**: zero-argument computed read-only values; must be side-effect-free (pure reads, `#[pure]` semantics); `xs.len` and `e.message` are properties.
- **Bounds**: `fn render[T: Show](x: T)`; bounds are combined with `+` (`T: Hash + Eq`).
- **Supertraits**: `trait Env: Clock + Fs + Log { ... }` — the implementor must implement all supertraits at once; capability context composition (§8.1) uses exactly this mechanism.
- Trait objects: only `&Trait` (borrowed form, v0.3); dynamic dispatch; whether non-Send propagates is judged by the object's real type (the object carries a Send bit, §7.4).

## 3.5 References and Trait Objects

- `&T` = shared read-only view: an **immutability constraint**, not a lifetime annotation. Under the full/web profiles liveness is guaranteed by the GC; in own/bare contexts the additional "must not escape the callee" rule applies (decidable, no annotations needed, §6.3).
- Mutable passing goes through `var` parameters / `Mutex`; there is no `&mut`.
- Upcast: `&FakeClock` → `&Clock` implicit; downcasts forbidden.

## 3.6 Implicit Conversions (exhaustive list; everything else is forbidden)

1. `String` → `Str`;
2. `T[N]` → `T[]` (fixed-size array decays to slice; **the direct type of the array literal `[a, b, c]` is `T[N]`**, with the element type adapted to context);
3. `T[]` → `&T[]` (slice made read-only; the Send determination changes accordingly, §7.4);
3. `T` → `T?` (`Some` wrapping), and `T` → `Result[T, E]` limited to `Ok` wrapping in explicit construction — never implicit;
4. value → `&T` / `&Trait` (automatically borrowed as a read-only view);
5. integer literal adaptation (§3.7).

Numeric widening/narrowing conversions must be explicit: `x.as[U64]()` (built-in method; narrowing semantics = truncation).

## 3.7 Type Inference and Literal Adaptation

- **Local inference**: `let/var` bindings and closure parameters (omittable) are inferred within the block; type arguments of generic calls (when TypeArgs are omitted, inferred per §3.9.1, v0.7).
- **Signatures must be annotated**: function parameters, return types, fields, and `const`/`static` must have explicit types — the signature is the contract (P1/P5).
- Integer literals are interpreted according to the expected type (variable type, comparison target, argument); unconstrained, they default to `I32`; floating-point defaults to `F64`.

## 3.8 Standard Prelude (implicitly available, no `use` needed)

### 3.8.1 Types and Values

```
Types: Option Result Box List Map Set String Str StringBuilder
       Channel Sender Receiver Task Scope Mutex Atomic Global AnyError
       Arena Region Pool Simd Never Void Bool and all numeric types
traits: Show Eq Error Cap Clone Hash Iter
values/functions: assert assert_eq assert_ne panic expect fmt
variants: Some None Ok Err true false void
```

- `Option[T] { Some(T) | None }` and `Result[T, E] { Ok(T) | Err(E) }` are ordinary enums, matchable (§4.6).
- Prelude symbols may be shadowed by local declarations (lint hint).

### 3.8.2 Minimal Prelude API Checklist (normative: P1 must provide it; extensions go through RFC)

| Type/trait | Members (methods with `()` / properties without) |
|---|---|
| `Option[T]` | `is_some` `is_none` (props); `map(f)` `or(default)` `expect(msg)` |
| `Result[T, E]` | `is_ok` `is_err` (props); `map(f)` `or(default)` `expect(msg)` `context(str)` (§5.4) |
| `Show` | `fn show(&self) -> Str`; can be generated by `@derive(Show)` |
| `Eq` | `fn eq(&self, other: &self) -> Bool`; can be generated by `@derive(Eq)` |
| `Error` | `prop message: Str`, `prop cause: &Error?`, `prop trace: Str` (location chain, empty by default, §5.3/§5.4); can be generated by `@derive(Error)` |
| `AnyError` | The prelude error-erasure type (a class implementing Error): the return error type of `context`; `?` auto-erases to AnyError (§5.4) |
| `Cap` | Empty marker trait: **capability traits must inherit from it** (`trait Clock: Cap`); the `#[pure]` check is decided on this basis (§8.3); concrete types **need not, and cannot, implement Cap on their own** — it only marks the category of a trait |
| Numeric types | `as[T]()` (explicit conversion; narrowing = truncation, §3.6); `abs()` `min(a,b)` `max(a,b)` |
| `Simd[E, N]` | `Simd[E, N].splat(v)`; `lane(i) -> E`; `to_array() -> E[N]`; `+ - * /` element-wise (operator whitelist, §3.1/§9.5). **Self-hosting status (T51 note, 2026-10-03)**: v0 = semantic simulation (interpreter) / scalar simulation (emission, P1-C2 heap view `ctron_view`); element-wise loops are realized by backend-compiler auto-vectorization (clang -O2 empirically NEON width 4; gcc needs -O3); register residency and direct intrinsic emission remain aspirational — see `docs/simd-vectorization-analysis.md` for the evaluation |
| `Str` / `String` | `len` (bytes), `char_len` (characters); `slice(range)` (byte slicing, must land on a character boundary, §3.3); `contains(s)`; `to_string()` (allocating, §6.5); `iter()` |
| `T[]` / `&T[]` | `len` (prop); `iter()`; indexing `[i]` (§4.5) |
| `List[T]` | `new()` `push(v)` `pop()` `len` (prop); indexing |
| `Arena` | `array[T](n)` `zeros[T](n)` `list[T]()`; `Arena.fixed(n)` (bare); handles are move-only (§6.3); `into_gc()` (the sole exit path for arena data leaving a block, §6.3) |
| `Mutex[T]` | `with(f: fn(&T) -> R) -> R` (read-only access); `with_mut(f: fn(var T) -> R) -> R` |
| `Atomic[I32]` | `Atomic[I32](init)`; `load()` `store(v)` `fetch_add(d) -> I32` (returns the old value; integer family) |
| `Global[T]` | `Global[T](name, init)`; `with`/`with_mut` same as Mutex (§7.6; registration is audited via the manifest) |
| `Drop` | `fn drop(var self)`; deterministic destruction of value types (§6.4), executed in reverse order on scope exit |
| `Channel[T]` | `Channel[T](cap) -> (Sender, Receiver)`; `send(v) -> Result` `recv() -> Result` (§7.3) |
| `Task[T]` | `join() -> T` (re-panics); `join_or() -> Result[T, TaskPanic]` |
| `fmt` | `fmt(parts: Str, values...) -> String` (desugaring target of interpolation, §4.11; allocating) |

- Ruling on the naming conflict: `Mutex.with` (read-only) and `with_mut` (mutable) form a pair — in pinned ruling 14, `m.with(|var a| ...)` was unified to `with_mut` as of v0.4, with `with` being read-only only.
- This table is a **minimal set**, not a closed one; the remaining stdlib modules (`iter`/`net`/`fs`/...) are not part of the prelude and require `use`.

## 3.9 Generics (revised in v0.6: instantiation/bounds/nesting semantics)

- Syntax `fn f[T, V](...)` / `struct Pair[A, B]`; **square brackets**.
- Implementation: **monomorphization** by default (static dispatch, zero cost); monomorphization explosion is bounded by the compile budget (§8.5); when over the limit, the hint is to switch to `&Trait`.
- Value parameters are comptime (fixed-size array dimensions etc., §8.4); type-level comptime is reserved.
- Generic parameters involve no lifetimes (memory safety is guaranteed by the GC/Send system, not by lifetimes).

### 3.9.1 Instantiation (v0.6)

- **Generic fn**: TypeArgs at the call site **may be given explicitly or omitted** (two-phase, v0.7; the explicit form is always legal):
  - **Explicit**: `render[Pixel](p)` — type parameters are bound positionally in declaration order; a count mismatch reports E2020.
  - **Omitted (inference)**: for each type parameter, unification solves it **only from argument types** (Go-style; no return-value context, no chained-call context); literal arguments use their default types (`0`→I32, `2.5`→F64). Conflicting solutions → **E2061**; a type parameter appearing in no argument position while concrete argument information exists → **E2060** (explicit required). Bound checking happens after solving.
  - v1 boundary: fn calls only (generic struct literals remain annotation-driven, §3.9.1); no partial inference (`f[_](…)` reserved).
  - Each distinct combination of argument types yields one specialization (`t_<name>__<arg-code>`, e.g. `t_get__i_s`); multiple call sites with the same combination share one specialization; **inference precedes explicit instantiation; the specialization key and the monomorphization mechanism are unaffected**.
- **Generic struct**: two instantiation routes — (a) **annotation-driven**: `var m: Map[I32, Str] = Map { ... }` (the annotated types pin the instance); (b) inside a generic fn body, instantiation follows the signature (a `Map { ... }` literal in the body shares the signature's instance). **Instantiation arguments are currently limited to scalars/Str** (instance-code alphabet I/S/B/L).
- **Nested generic calls**: a generic fn body may call a generic fn, with type arguments resolved via the outer binding: in `fn add[V: Eq](s: Set[V], v: V) { mem[V](s, v) }`, `mem[V]` is specialized with the outer `V`.
- **Recursive specialization must be diagnosed**: a generic fn body calling its own same instance (directly or indirectly) → a compile-time "recursion limit exceeded" diagnostic (the implementation expresses this as a pre-lift depth limit; relaxing it requires a seen set, via RFC).

### 3.9.2 bound (v0.6)

- Syntax: `fn render[T: Show](v: T)`, `struct Pair[A: Show + Eq, B: Show + Eq]`; `+` joins multiple bounds.
- **Check point**: at call sites with explicit TypeArgs, checked per type parameter; a violation = **E2050** (the diagnostic carries the argument's type alias).
- **Satisfaction predicate (structured)**: `Show`/`Eq` are satisfied by a struct whose fields are all scalars (I32/I64/Bool/Str) or satisfiable value types (recursive, depth limit 6); primitives, enums, and classes do not satisfy. `Eq` additionally accepts scalars as natively equatable (top level). The predicate agrees with the derived capability surface of `.show()`/`.eq()` (§3.11).
- **TPar passthrough**: when a call-site argument is an outer type-parameter name (a name with no declaration) → let it pass; the outer bound at the argument site is responsible. Not applicable at nested field positions.
- Unknown bound names (reserved until the trait system lands) are not checked in v0.

### 3.11.1 Box alias-sharing semantics (added in v0.6)

- **Assignment/argument passing/binding share the heap cell**: `let b = a` (where a is a Box) copies the handle; b and a point to the same single heap value. A write through either alias, `x.f = v` (mutable field), is visible to all aliases.
- **Auto-dereference permeates reads and writes**: both the read `p.x` and the write `p.x = v` auto-dereference; a Box value passed as an fn argument/return passes the handle.
- **Contrast**: struct assignment is a deep copy (mutually independent, §3.2); copying a struct with an embedded Box field is shallow sharing (the box pointer is copied).
- **Anchor**: `compiler/test/fx_boxalias.ct` (alias sharing / cross-fn writes / struct-copy contrast).

## 3.10 Correspondence with the test corpus

`tests/03_values_refs.ct` (values/references/Box), `tests/04_generics_comptime.ct` (generics/array decay), `tests/07_capabilities.ct` (trait/prop/impl), `tests/03e_generics_types.ct` (bounds/derive; host ↔ self-hosted agree).

## 3.11 `@derive(Show, Eq)` and structured methods (added in v0.6)

- **Method surface**: value-type receivers can call `.show() -> Str` and `.eq(other) -> Bool` (exactly one argument).
- **Derivation stance = structured**: any struct whose fields are all scalars/Str/derivable value types has both methods; in v0 the `@derive(...)` annotation is **declarative** (parsed and retained, not gating), with semantics carried by bound checking (§3.9.2). Once the derive plugin system (design doc §10) lands, this tightens to annotation gating.
- **Format (normative; verbatim-identical across both channels)**: `.show()` produces `<name>(field=value,field=value)` — fields in declaration order, comma-separated, `field=value`; nested value types recurse in the same format; scalars are converted exactly as in interpolation (§4.11). `.eq(other)` is field-by-field equality (same type alias and all fields equal); arguments must have the same static type.
- **Capability boundary**: List/Atomic/enum/class fields are not derivable (the same limit on both sides: interpreter and emission diagnose identically).
- `@derive(Json)` and other plugin derivations: reserved (§8.4/design doc §10).
