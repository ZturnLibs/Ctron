<!-- 英文译件:手维护;中文正典 = docs/spec/ 同名文件(经 tools/sync_site_spec.sh 同步至同名 .zh.md) -->
<!-- 译件滞后于正典修订时,以中文正典为准 -->

# §5 Error Model

Principle: errors come in two kinds — **expected errors** (they enter the types and must be handled) and **bugs** (panic, catchable only at task boundaries). There is no exception mechanism (it is on the rejection list).

## 5.1 Carrier Types (Prelude Definitions)

```c
enum Option[T] { Some(T) | None }
enum Result[T, E] { Ok(T) | Err(E) }
```

- No null: `T?` is merely syntactic sugar for `Option[T]`; unwrapping goes only through `?`, `or`, `expect`, `match`.
- `E` must implement the `Error` trait (§5.4) before it can serve as the error type propagated by `?`.

## 5.2 Extraction and Defaults

| Form | Option | Result | Failure behavior |
|---|---|---|---|
| `x or default` / `x.or(default)` | Yields the Some value | Yields the Ok value | Returns the default |
| `x.expect(msg)` | Yields the value | Yields the value | panic(msg + Show of the original value) |
| `x?` | Propagates None | Propagates Err | Early return (§5.3) |
| `match` | Exhaustive branches | Exhaustive branches | — |

## 5.3 `?` Propagation

- `expr?` is legal **only if** the enclosing function returns `Result[_, E']` (E implements `Error` and is convertible/identical) or `Option[_]`.
- Semantics: `Err(e)` → immediately `return Err(convert(e))`; `None` → `return None`.
- **Location metadata (a record/materialize two-phase scheme)**: every `?` **records** the call site (file:line) into the location chain (diagnostic metadata; it does not change the `E` type; implementations may carry it as attached metadata); `context` and top-level failure printing are responsible for **materializing** it into `AnyError.trace` / diagnostic output. In release, collection can be disabled via build configuration. The error chain serves human review and agent-side repair localization.

## 5.4 The `Error` Trait and the Error Chain

```c
trait Error {
    prop message: Str          // human-readable summary
    prop cause: &Error?        // cause chain; None when there is no cause
    prop trace: Str            // location chain: file:line entries joined by "; ", empty by default (§5.3)
}

@derive(Error)                  // generates the impl for the enum (§8.3)
enum HttpError { Timeout(U64) | BadStatus(I32) }
```

- **`AnyError`** (a prelude class implementing `Error`) is the **error erasure type**: `message` = the most recent context, `cause` = the wrapped error, `trace` = the accumulated location chain.
- `result.context(msg) -> Result[T, AnyError]`: wraps the error and **materializes** the location chain (message = msg, cause = the original error, trace appends the current file:line).
- When `?` propagates into a function returning `AnyError`, any `E: Error` is **automatically converted with erasure** (the only built-in form of "convertible" in §5.3).
- Diagnostic display: when an error value is rendered by `Show`, the chain is printed as `msg … while msg2 … while …(trace)`.

## 5.5 panic

- `panic(msg: Str) -> Never`; expresses bugs (broken invariants, overflow, out-of-bounds access, assertion failure).
- Unwinding rule: **caught at task boundaries** — a panic unwinds along the stack to its owning task; the task handle's `join()` returns `Err(TaskPanic)`, or the panic propagates through the scope tree (§7.2); a panic in the main task = the process exits with a nonzero code and prints the location chain.
- **Resource responsibility is never carried through a panic**: RAII drops are guaranteed to run during unwinding (§6.4); GC memory is unaffected.
- `assert/assert_eq/assert_ne/expect` panic on failure.

## 5.6 Design Rulings and Rationale (Normative)

- No exceptions: control flow must be visible (P1); agents doing blanket catch with botched recovery is an empirically demonstrated antipattern.
- No forced error-code enums: errors are values, and the `Error` trait unifies chained context; libraries are free to define their own E types.
- `Result` must be consumed: discarding a `Result`/`Option` return value = the W8020 warning (must-use).

## 5.7 Corresponding Tests

`tests/02_option_result.ct` (propagation / default extraction / error chain), `tests/02_match_exhaustive.neg.ct` (exhaustiveness), `tests/07_pure.neg.ct` (capability calls are not pure).
