# Ctron: an AI Native systems programming language

Ctron is designed from day one for AI code generation as the primary authoring workflow. The syntax, type system, error model, and diagnostic format are each shaped by a single premise: **AI writes, humans review.**

## Design principles

**AI writes, humans read.** AI-generated code is structurally correct but fails in the details — misspelled identifiers, type mismatches, missed edge cases. Ctron catches these at compile time: every diagnostic carries a stable error code, diagnostics are structured output, and type inference follows explicit rules. AI receives the error code and can fix precisely — no guessing intent from prose.

**Semantics are clear. What you see is what you mean.** An assignment doesn't trigger constructors or custom operators. No null, no implicit numeric conversion, no macros, no undefined behavior. Code does what it looks like it does.

**No magic.** The standard library is Ctron source, not a compiled binary. The compiler is Ctron too. AI can read stdlib source to understand API behavior — no documentation needed.

**As concise as clarity allows.** `?` for error propagation, `scope` for structured concurrency, `use` for imports. Each language construct does one thing.

## Three commands

```bash
ctc run main.ct      # interpret — no C toolchain required
ctc check main.ct    # static checks; --format=json for structured diagnostics
ctc build main.ct    # emit C → local cc → native executable
```

The interpretation path has zero external dependencies. The emit path is self-contained — the runtime is fully inlined with system headers only. Same source, no language switch between development and deployment.

## Genuinely self-hosted

The entire compiler is Ctron source. The three-stage bootstrap fixed point is scriptably reproducible — C emitted on different platforms is byte-for-byte identical, verified as a release promise in CI.

The standard library is also Ctron source. AI can read stdlib source to understand API behavior — no documentation needed.

## Structured concurrency & error chains

Tasks run under `scope` / `spawn` / `join` — block-scoped, with cancellation propagating out of the scope — and failures flow as `Result` error chains carrying `message` / `cause` / `trace`. `Send` checks happen at compile time.

## Where to go next

| Page | Contents |
|---|---|
| [Getting Started](getting-started.md) | Three ways to install, hello ctron, your first executable, project mode |
| [Examples](examples.md) | Annotated sources of ctgrep / ctwc / ctwf |
| [Download](download.md) | Five-platform asset matrix, checksums, Windows notes |
| [Language Spec](spec/README.md) | v0.7 frozen draft; diagnostics carry stable error codes |
| [Std Reference](std/README.md) | Per-module signature tables |

Repository: [ZturnLibs/Ctron](https://github.com/ZturnLibs/Ctron) · Design and implementation notes live in `docs/superpowers/specs/` and `compiler/BOOTSTRAP.md`.
