<!-- 英文译件:手维护;中文正典 = docs/spec/ 同名文件(经 tools/sync_site_spec.sh 同步至同名 .zh.md) -->
<!-- 译件滞后于正典修订时,以中文正典为准 -->

# §4 Expressions & Control Flow

## 4.0 Operator Constitution (v0.7)

1. **One meaning, one symbol**: exactly one spelling per semantic. Bool logical or is only `||`; default-taking is only the `or`/`.or()` pair; synonymous aliases or spelling sugar are never introduced.
2. **Symbol/keyword division of labor**: symbolic operators = pure Bool-domain combination (`&&`/`||`/`!`); keyword infix = operations touching infallible semantics (`or` takes the default).
3. **Never repurposed**: `&&`/`||` never perform bitwise operations (bitwise always goes through `bit` module methods, §4.5); `|` is always the closure delimiter, never an operator.
4. **Role separation**: the "zero-argument closure / logical or" duality of `||` is two syntactic roles of one token, uniquely determined by syntactic position (§4.7), not semantic ambiguity.

## 4.1 Bindings

- `let pattern [: Type] = expr` immutable binding; `var` mutable binding; **must be initialized** (no uninitialized reads). Reassigning a `let` binding (including the `+=` family) = **E2080** (v0.9, already enforced in the C host; this gate's criterion = the innermost binding of the assignment's left-hand target being a `let` local — `var`/parameters/closure parameters/for·match pattern bindings are not covered by this gate).
- Binding patterns: identifiers, `_` (discard), tuple patterns `let (a, b) = pair`.
- Shadowing allowed (§2.4).

## 4.2 Assignment

- `=` and `+= -= *= /= %=` are **statements**, not expressions (misreads like `if (a = b)` are forbidden).
- Legal targets: `var` bindings, `var` fields via a mutable access path, **slice/array elements (only when the slice's binding root is `var`; `&T[]` read-only views are never writable)**. Mutability determination: the **root binding** of the target path must be `var`, and the fields along the way must be declared `var` (for traversal through class references see §7.5).

## 4.3 Precedence (low → high)

| Level | Operators | Associativity |
|---|---|---|
| 0 | `\|\|` (logical or, short-circuit; Bool only) | left |
| 1 | `or` (default-taking) | left |
| 2 | `&&` | left |
| 3 | `== != < > <= >=` | **non-chaining** |
| 4 | `.. ..=` (range) | none |
| 5 | `+ - +% -%` | left |
| 6 | `* / %` | left |
| 7 | unary `- !` | right |
| 8 | postfix: call `()` index `[]` member `.` type args `[]` propagation `?` | left |

## 4.4 Boolean and Default-Taking

- `&&`/`||` are short-circuit logical and/or, accepting only `Bool` (otherwise E2010; diagnostic determination aligned with the implementation stance: only known non-Bool scalar categories report an error, aggregate/unknown types pass through); `!` is logical negation, accepting only `Bool` (violation E2010, same determination criteria; spelled out in v0.8). Comparisons do not chain: `a < b && b < c`.
- **Veto note (v0.7)**: v0.6 had declined to introduce `||` on the grounds of "preventing ambiguity". After an audit, the conflict between zero-argument closures and logical or is fully resolved by syntactic position (§4.0 principle 4, §4.7; `||` in expression-initial position is a closure parameter list, in infix position it is logical or — the two positions are mutually exclusive and exhaustive), so the grounds for the veto no longer hold and `||` was introduced. `or` remains exclusive to default-taking (§4.0 principles 1, 2).
- `x or default`: for Option/Result, yields the value on success and the default on failure; equivalent to `x.or(default)`; the two forms coexist with a single semantics (pinned ruling). For a left side that is not Option/Result: it is evaluated, then the default falls back (unified stance across the three implementation tracks, v0.8 emission-side aligned).

## 4.5 Arithmetic

- **Binary `+` is concatenation when both sides are `Str`** (revised 2026-09-08, T2; previously only interpolation was the convention). A single-sided `Str`, or any other operator (`- * / %`) meeting a `Str`, is still E2010.
- **Checked arithmetic** by default: integer overflow / division by zero panic (messages contain "overflow"/"division by zero"); in release, the overflow check may be turned off via build configuration (division by zero is never disabled).
- Explicit wrapping: `+% -%`; `%` is remainder, **the result's sign follows the dividend** (same as Rust `%`, not Euclidean modulo); bitwise operations go through the stdlib (`bit` module).
- Indexing is **always bounds-checked** (no unchecked in the safe subset); out-of-bounds panics ("index out of bounds"). Index expressions accept any integer type.

## 4.6 Control Flow

- **`if` is an expression**: when used as a value, `else` is required; branch types must agree (or be `Void`).
- **`match` is an expression**:

```c
match value {
    Pattern1 => expr1
    Pattern2 => { ...; exprN }
}
```

  - Arms are separated by newlines; exhaustiveness is enforced at compile time (E2030); adding a new enum variant → every non-wildcard match errors.
  - Patterns: literals, wildcard `_`, bindings, variants (tuple/named fields), tuples, struct patterns.
- **Loops**: `while cond`, `for pattern in iter`; ranges `0..n` / `0..=n` are lazy iterable values; no `do-while`.
- **break / continue** (v0.7): statements, not expressions, carry no value; they bind to the nearest enclosing loop **within the same function body**. Three static gates: **E2070** outside a loop; **E2071** crossing a scope that holds locals with a Drop impl (statically rejected in v1 — direct-to-C emission has no cleanup path, and the RAII contract takes precedence; this restriction may be lifted per the spec's explicit wording, and lifting it is purely additive); **E2072** crossing a closure boundary (a closure body is a separate function). `scope {}` and `own (arena) {}` are not loops and do not participate in binding.
- **Blocks are expressions**: the block's final expression is the block's value; `return` exits the function, and `return` has type `Never` participating in inference.

## 4.7 Closures

```c
|x| x + 1            // single-expression body
|| expr              // zero arguments
|var a| { ...; a }   // mutable parameter + block body; the block's value is the return value
|x: I32| -> I32 { x * 2 }
```

- **Role separation of zero-argument closures and `||` (v0.7)**: `||` appearing in **expression-initial position** (no left operand, e.g. an argument position or the start of an assignment's right-hand side) → zero-argument closure parameter list; appearing in **infix position** (a left operand already present) → logical or (§4.3 level 0). The two positions are mutually exclusive and exhaustive, and the lexer emits a single unified token.
- Closure capture = **binding values are copied at creation** (T27 ruling, final state, 2026-09-30; the full profile and the arena profile share the same semantics). After a closure is created, reassignment of the outer binding is **invisible** to the closure (the creation-time snapshot is the normative semantics). Mutable sharing goes only through explicit cells: `Mutex[T]`/`Atomic[T]`/`Global[T]` (E3070 = the semantic enforcement of this rule, not a warning); closures are disabled inside `own` blocks. Industry alignment: isomorphic to Java/Kotlin effectively-final; capture-by-reference is reserved as a capability extension slot once the GC matures (§6.2; does not conflict with the final state).
- A closure's type is the function type `fn(Params) -> Ret` (§3.1) — parameters accepting closures are declared with function types, e.g. `Mutex.with_mut(f: fn(var T) -> R)`; closure literals adapt automatically in contexts expecting a function type.
- Closure Send determination: closures (and values of function type) are checked capture-by-capture against the literal **at the spawn site** (E3010); when passed through a channel / static storage, values of function type are treated as capture-agnostic → **not Send** (§7.4).

## 4.8 UFCS (Uniform Function Call Syntax)

- `recv.m(a)` first resolves to "a function/method taking `recv` as its first argument"; resolution order: **the type's own inherent methods → impl methods of currently visible traits** (§3.4) → the prelude.
- `21.double()` ≡ `double(21)`; chained calls are readable and introduce no OOP inheritance.

## 4.9 scope / own expressions

- `scope { |s| ... }`: a structured-concurrency scope expression; the block's value is the expression's value (§7.2).
- `own (arena) { ... }`: an ownership-mode block; the block's value is subject to the block-exit rules (§6.3).

## 4.10 `test` blocks

- `test "name" { ... }` top-level declaration; compiled and run only under `ctron test`; the block's value is ignored.
- Prelude assertions: `assert(cond)`, `assert_eq(a, b)`, `assert_ne(a, b)` (`T: Eq + Show`); `expect(msg)` (§5.2).
- Assertion failure = panic, with the message containing the `Show` representation of the expected/actual values — AI-readable failure output.

## 4.11 String Interpolation (semantics)

`"hi {name} x{n}"` desugars into fragment concatenation via `fmt` (a single `String` construction; allocation attribute alloc); fragments support field/method chains, not statements or nested `{}`.

## 4.12 Correspondence with the test corpus

`tests/01_basics.ct` (operators/control flow/UFCS), `tests/02_option_result.ct` (match/`?`), `tests/06_concurrency.ct` (closures/scope), `tests/01_overflow.panic.ct` (checked arithmetic).
