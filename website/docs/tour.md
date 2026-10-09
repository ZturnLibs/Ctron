# The Ten-Minute Tour

A rapid tour of the language: variables, control flow, structs and traits, error handling, the standard library, and tests — in about ten minutes. For toolchain installation and build details, see [Getting Started](getting-started.md).

## Minute 0: What is Ctron

An **AI-native systems programming language**, designed for "AI writes, humans review": every diagnostic carries a stable error code (`E2010`, `E2080`, …), the grammar is unambiguous, and the language does nothing behind your back —

| What Ctron doesn't have | What you use instead |
|---|---|
| `null` | `Option[T]` |
| Implicit numeric conversions | explicit `x.as[U64]()` |
| Operator overloading, macros | `+` means addition — source is the truth |
| Undefined behavior | the spec enumerates every legal behavior |

One codebase, two execution paths: `ctron run` interprets (zero dependencies, fastest dev loop), `ctron build` emits readable, self-contained C and invokes the local cc to produce a native executable. The standard library ships **as Ctron source** alongside the toolchain; `use std.*` reads the source at compile time and merges it into a single AST — you can read stdlib source to understand API behavior, no docs required.

## Minutes 1–2: Install and run

```bash
curl -fsSL https://github.com/ZturnLibs/Ctron/releases/latest/download/install.sh | sh
export PATH="$HOME/.ctron/bin:$PATH"
```

```bash
$ ctron new hello && cd hello    # scaffolds Ctron.ctcl (manifest) + src/main.ct
$ ctron run src/main.ct          # parse → check → evaluate, one pipeline
hello, ctron
$ ctron check src/main.ct --format=json
{"diagnostics":[]}
$ ctron build src/main.ct        # emit C → local cc → executable
$ ./src/main
```

`run` / `check` / `new` need no C toolchain; only `build` requires a local cc (override with the `CC` env var). The manifest is the CTCL-format `Ctron.ctcl`:

```ctcl
pkg {
    manifest_version = 1
    name = "hello"
    version = "0.1.0"
}
```

## Minutes 2–4: Variables, types, functions

No semicolons — **a newline terminates a statement**. `let` is immutable, `var` is mutable; both must be initialized. Function signatures (parameters, returns, fields, consts) must be annotated — the signature is the contract; local bindings are inferred:

```ctron
fn main() {
    let name = "ctron"          // immutable
    var n = 3                   // mutable
    n += 1                      // reassigning a let = E2080 compile error

    let ratio = 2.5             // floats default to F64
    let width: I64 = 1_000_000  // integer literals adapt to the expected type; unconstrained = I32
    println("{name} v{n}")      // interpolation: field/method chains and indexing work
}
```

Type system in one glance: fixed-width integers `I8…I64/U8…U64`, `F32/F64`, `Bool`; strings come in two forms — `Str` (an immutable borrowed view; string literals are `Str`) and `String` (GC-heap owned, implicitly degrades to `Str`; `s.to_string()` upgrades). **No implicit numeric conversions** — both widening and narrowing go through `as`:

```ctron
fn shout(msg: Str) -> I64 {
    let n: U64 = msg.len.as[U64]()   // explicit conversion; narrowing truncates
    return n.as[I64]() + 1
}
```

Three details you will use immediately:

```ctron
assert_eq(7 / 2, 3)            // truncating division; checked arithmetic by default
assert_eq(255u8 +% 1u8, 0u8)   // +% -% are the only doorway to wrapping arithmetic
assert_eq(21.double(), 42)     // UFCS: 21.double() ≡ double(21)
```

## Minutes 4–5: Control flow — everything is an expression

**There is no ternary operator** — `?` and `:` are taken by other syntax; the `if` expression is the one and only form:

```ctron
let label = if n > 0 { "pos" } else { "neg" }   // else is required when used as a value
```

`match` is an expression too, with **compile-time exhaustiveness** — add a variant to an enum and every non-wildcard match fails to compile:

```ctron
match read_file(path) {
    Some(s) => { println("got {s.len} bytes") }
    None    => { println("missing: {path}") }
}
```

Loops and ranges; `break`/`continue` are available:

```ctron
var sum = 0
for i in 0..5   { sum += i }    // 0..5 = 0,1,2,3,4
for i in 0..=5  { sum += i }    // inclusive
var n = 1
while n < 100 { n = n * 3 }
```

Closures are delimited by `|` and capture **a copy of the binding at creation time** (mutable sharing only through `Mutex`/`Atomic`/`Global`):

```ctron
let square = |x: I32| -> I32 { x * 2 }   // annotations are elidable in expected-fn-type position
let y = xs
    .filter(|x| x > 0)                    // multi-line chains use leading-dot style:
    .map(|x| x * 2)                       // a line starting with . continues the previous statement
```

## Minutes 5–6: structs / classes / Box and traits

Assignment behavior is written on the type name: `struct` **copies**, `class` **shares**, `Box` is an explicit heap cell (access auto-dereferences):

```ctron
struct Point {
    var x: I32
    var y: I32
}

class Tag {
    let id: I32               // fields are immutable by default; mutable requires var
}

test "three assignment semantics" {
    var a = Point { x: 1, y: 2 }
    var b = a
    b.x = 10                  // changes b only: a.x is still 1 (value copy)

    let t1 = Tag { id: 7 }
    let t2 = t1               // same instance (reference shared)

    let p = Box[Point](Point { x: 3, y: 4 })
    assert_eq(p.x, 3)         // Box auto-dereferences; writes through aliases are visible
}
```

Traits are nominal and explicitly implemented (orphan rule: the trait or the type must be defined in your package). A `prop` is a zero-argument read-only computed value, called **without parentheses**; generics use **square brackets**, bounds combine with `+`; `@derive` synthesizes common impls:

```ctron
trait Clock {
    fn now(&self) -> U64
    prop name: Str
}

@derive(Show, Eq)
struct Pixel {
    let x: I32
    let y: I32
}

struct Pair[A: Show + Eq, B: Show + Eq] { // generics + bounds
    let first: A
    let second: B
}

impl Clock for FakeClock {
    fn now(&self) -> U64 { return self.base }
    prop name: Str { return "fake" }
}
```

> The full trait story (two receiver forms, trait objects, the orphan rule, UFCS resolution order, v0 boundaries) is in the appendix below.

## Minutes 6–8: No null, and error handling

Errors come in two kinds: **expected errors** live in the type system (`Option`/`Result`), **bugs** panic. There is no exception mechanism.

```ctron
@derive(Error)                            // generates the Error impl for the enum
enum MathErr {
    DivByZero
}

fn safe_div(a: I32, b: I32) -> Result[I32, MathErr] {
    if b == 0 { return Err(DivByZero) }
    return Ok(a / b)
}

fn ratio(a: I32, b: I32, c: I32) -> Result[I32, MathErr] {
    let x = safe_div(a, b)?               // ?: on Err, return early with location metadata
    return safe_div(x, c)
}
```

Four ways to extract a value (`T?` is just sugar for `Option[T]`):

```ctron
div_opt(1.0, 0.0).or(-1.0)                // default on failure (infix: x or default)
ratio(1, 0, 1).expect("must not fail")    // panics on failure (message carries the Show form)
ratio(1, 0, 1).context("computing ratio") // wraps the error chain: message/cause/trace
match ratio(1, 0, 2) {                    // exhaustive branches
    Ok(v)  => println("ok {v}")
    Err(e) => println("err: {e.message}")
}
```

Discarding a `Result`/`Option` return value draws a must-use warning (W8020) — errors must be consumed.

## Minutes 8–9: Standard library, collections, tests

`use` paths always start from the package root / top-level namespace; group imports use braces; **wildcard imports are forbidden**:

```ctron
use std.str.{words, count_ch}    // std modules: json csv uuid crypto fmap heap
                                 // iter sort strconv rand path time hashmap…
```

Collections and string building (prelude types — no `use` needed):

```ctron
var nums: List[I64] = List[I64]()    // note: the array literal [1,2,3] is a fixed-size
nums.push(42)                        // array; it never becomes a List — use push to grow
nums.push(7)
assert_eq(nums.len, 2)
assert_eq(nums[0], 42)               // indexing is always bounds-checked; out of range panics

var sb = StringBuilder()
sb.push_str("hello")
sb.push_str(", ctron")
assert_eq(sb.to_string(), "hello, ctron")
```

Tests are in-language `test` blocks, run with `ctron test`; a failed assertion panics with a message carrying the `Show` form of expected and actual values:

```ctron
test "loops and ranges" {
    var sum = 0
    for i in 0..5 { sum += i }
    assert_eq(sum, 10)
}
```

```bash
ctron test tests/01_basics.ct     # run test blocks
ctron fmt -w .                    # canonical formatting (one form repo-wide)
ctron lint --strict .             # warnings fail too — good for CI
```

## Minute 10: A GUI teaser, and the pitfalls

GUI is a first-class domain package — UI lives in `.ctml` markup, logic stays in Ctron, and the platform (parsing, layout, hit-testing, the event loop) lives entirely in the `gui` package: user code writes no event loop at all (see the complete runnable `examples/gui_counter` in the repository):

```xml
view Counter {
  <vbox class="root">
    <label>count: {count}</label>
    <button class="btn" on:click={inc}>+1</button>
  </vbox>
}
```

The ten most common beginner pitfalls:

1. **No semicolons** — a newline ends the statement (`;` is a syntax error); `} else {` must stay on one line.
2. **No ternary** — the `if cond { a } else { b }` expression is the only form.
3. **`||` is logical or** — `or` is only for Option/Result defaults; bitwise operations are not language operators (use std's bit module).
4. **Comparisons don't chain** — write `a < b && b < c`.
5. **No implicit numeric conversions** — `n.as[U64]()`; unconstrained integer literals default to `I32`.
6. **A literal `{` in a string is `\{`** — no nested `{}` inside interpolation; string literals cannot span lines.
7. **`[1, 2, 3]` is a fixed-size array** (it degrades to a slice); for a growable list use `List[I32]()` + `push`.
8. **struct assigns by copy, class by reference, Box aliases share** — fields are immutable by default; mutable requires `var`.
9. **`let` bindings cannot be reassigned** (including `+=`, E2080); closures capture a snapshot at creation.
10. **match must be exhaustive** — add an enum variant and every non-wildcard match is a compile error.

**Next steps**: the language spec lives under [Spec](spec/README.md) (12 chapters, frozen draft v0.8); complete runnable applications are in the repository's `examples/` directory (`ctwc` for CLI, `todo_app` for network services, `gui_calc` for GUI); toolchain and project-mode details are under [Getting Started](getting-started.md).

---

# Appendix: Traits in depth — seven cases

One idea first: **Ctron is a nominal-trait language — methods come only from explicit `impl`; there is no inheritance and no implicit implementation**. A type's behavior comes from `impl Trait for Type`, free functions + UFCS, and (for classes) methods declared in the class body. A struct's declaration body has **fields only, no methods** — the behavior surface can always be found in an `impl` block.

## ① Basics: trait declaration, impl, two receiver forms

There are exactly two receivers: `&self` (a read-only view) and `var self` (mutable — the body can advance state); mutable sharing across boundaries goes through `var` parameters or `Mutex`:

```ctron
struct Counter {
    var cur: I32
    let limit: I32
}

trait Iterator[T] {
    fn next(var self) -> T?          // var self: state-machine traits take a mutable receiver
}

impl Iterator[I32] for Counter {     // impl must pin every type parameter (monomorphization)
    fn next(var self) -> I32? {
        if self.cur >= self.limit { return None }
        let v = self.cur
        self.cur += 1
        return Some(v)
    }
}

test "for iterates any Iterator" {
    let c = Counter { cur: 0, limit: 4 }
    var acc = 0
    for v in c { acc += v }          // implementing Iterator[T] enables for
    assert_eq(acc, 6)
}
```

A trait may contain method signatures, `prop` signatures, and default method bodies — **never fields**; state always lives in the implementing type.

## ② props, default bodies, supertraits

A `prop` is a zero-argument read-only computed value, called **without parentheses** (`xs.len` and `e.message` are props); it must be pure and side-effect free. Default method bodies may call supertrait members, and an empty `impl` inherits them:

```ctron
trait Clock: Cap {                   // : Cap only for I/O capability traits (see ④)
    fn now(&self) -> U64
}

trait Named: Cap {
    prop name: Str                   // trait property declaration
}

trait Env: Clock + Named {           // supertraits: implementing Env requires both
    fn describe(&self) -> Str {      // default method body
        return "{self.name} @ {self.now()}"
    }
}

class FakeEnv {
    let base: U64
}

impl Clock for FakeEnv {
    fn now(&self) -> U64 { return self.base }
}

impl Named for FakeEnv {
    prop name: Str { return "fake" }
}

impl Env for FakeEnv {               // describe uses the default body → empty impl is legal
}

test "props, default methods, supertraits" {
    let env = FakeEnv { base: 100 }
    assert_eq(env.now(), 100)
    assert_eq(env.name, "fake")      // props take no parentheses
    assert_eq(env.describe(), "fake @ 100")
}
```

## ③ Generics + bounds: constrain at the signature, unlock derived methods in the body

Bounds sit on the type parameters and combine with `+`; with a bound in hand, derived methods like `.show()`/`.eq()` become callable inside the generic body:

```ctron
@derive(Show, Eq)
struct Pixel {
    let x: I32
    let y: I32
}

fn render[T: Show](v: T) -> Str {        // bound: T must satisfy Show
    return v.show()                      // without that bound, this call is unavailable
}

struct Pair[A: Show + Eq, B: Show + Eq] {
    let first: A
    let second: B
}
```

`Show`/`Eq` satisfaction is a **structured predicate** — no per-type impl needed: a struct whose fields are all scalars/`Str`/derivable value types automatically has both methods (recursive, depth limit 6); `List`/`Atomic`/enum/class fields are **not derivable**. The normative `.show()` format is `Name(field=value,field=value)` (declaration order, comma-separated). `@derive(Error)` likewise generates `message`/`cause`/`trace` for error enums. An impl may also carry its own type parameters to implement a generic type: `impl[T] Seq[T, List[T]] for List[T]`.

## ④ Trait objects `&Trait`: dependency injection, the intended way

`&Trait` is the only trait-object form in v0 (a read-only borrow, dynamically dispatched). Upcasts are implicit (`&FakeClock` → `&Clock`); **downcasts are forbidden**. A value passed to a `&Trait` parameter is borrowed automatically — tests inject fake implementations:

```ctron
trait Clock: Cap {                   // capability traits must inherit Cap (an empty marker)
    fn now(&self) -> U64             // concrete types never implement Cap; it only tags the trait
}

class FakeClock {
    let base: U64                    // let fields → deeply immutable → Send
}

impl Clock for FakeClock {
    fn now(&self) -> U64 { return self.base }
}

fn elapsed_since(clock: &Clock, start: U64) -> U64 {
    return clock.now() - start       // dynamic dispatch through the trait object
}

test "capability injection via trait object" {
    let clock = FakeClock { base: 100 }
    assert_eq(elapsed_since(clock, 58), 42)   // value → &Clock: auto-borrow + upcast
}
```

`: Cap` is only needed when the trait represents an I/O capability (it participates in `#[pure]`/manifest capability checks); ordinary traits don't write it. The std `Fs` injection (`fn read_a(fs: &Fs) -> Result[String, FsError]` plus a `MemFs` fake) is the same pattern in production form — see `tests/07a_cap_fs_inject.ct` in the repository.

## ⑤ The orphan rule: E5010 and the newtype pattern

`impl T for X` is legal **iff the trait or the type is defined in the current package** — no generic exceptions. Implementing a prelude type directly is a compile error; the standard fix is a local newtype wrapper:

```ctron
// impl Show for I32                  // ✗ E5010: Show and I32 both come from the prelude
struct UserId {
    let v: I32
}

impl Show for UserId {               // ✓ UserId is a local type → legal
    fn show(&self) -> Str {
        return "UserId({self.v})"
    }
}
```

## ⑥ UFCS: free functions are methods, with a fixed resolution order

`recv.m(a)` first resolves as "the function taking `recv` as its first argument", in this order: **the type's own inherent methods → impl methods of visible traits → the prelude**. Free functions grow on their receivers naturally:

```ctron
fn double(x: I32) -> I32 { return x * 2 }

test "ufcs" {
    assert_eq(double(21), 42)
    assert_eq(21.double(), 42)       // two spellings, one function
}
```

The std iteration chain is the canonical "two-parameter trait + UFCS free functions" assembly — the adapter methods of `trait Seq[T, S]` (`map`/`filter`/`take`…) carry default bodies, the terminators `sum`/`count`/`collect` are free functions, and `xs.map(|x| x * 2).filter(|x| x > 4).sum()` runs lazily with zero intermediate collections. Runnable anchor: `tests/modules/iter_adapters` in the repository.

## ⑦ v0 boundaries (known limitations)

- **Trait declarations take no `pub`** — v0 has no pub-trait syntax; trait interface faces are always visible across modules; visibility is marked on members only.
- **Trait objects are `&Trait` read-only borrows only**; the owned form `Box[&Trait]` is a reserved slot, not yet implemented.
- **`&Trait` is conservatively non-Send**; a class with `var` fields is non-Send as a whole — cross-task sharing goes through `Mutex`.
- An impl's type parameters must be **fully pinned** (`impl Iterator[I32] for Counter`); the "implement generic-for-generic" form is `impl[T] ... for Type[T]`.
- Prelude symbols can be shadowed by local declarations (lint-hinted), but the orphan rule blocks impls, not names.
