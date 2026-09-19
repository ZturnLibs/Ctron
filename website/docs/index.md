# Ctron

**Ctron** is a self-hosted systems programming language: its compiler is written in Ctron itself (`compiler/`, 39 modules). Programs run under interpretation with zero external dependencies, and can also be emitted as **equivalent C source** that the platform's C compiler turns into a native executable with one command. One source tree, two execution paths.

```bash
curl -fsSL https://github.com/ZturnLibs/Ctron/releases/latest/download/install.sh | sh
```

> Releases have not been published yet. Until the first release (v0.0.1) lands, build from a clone — see [Getting Started](getting-started.md).

## Three commands

```bash
ctc run main.ct      # interpret — no C toolchain required
ctc check main.ct    # static checks; --format=json for structured diagnostics
ctc build main.ct    # emit C → local cc → native executable
```

`ctc` is the thin driver shipped with the toolchain — a POSIX sh and a PowerShell implementation sharing one command surface — and the only entry point you need to remember. Toolchain versions are decoupled from language milestones; the release line starts at v0.0.1.
All three paths are instant: no build step, no dependencies, nothing to configure first.

## Why it's worth a look

- **Genuinely self-hosted**: the entire compiler is Ctron source. The three-stage bootstrap fixed point is scriptably reproducible — C emitted on different platforms is **byte-for-byte identical**, and that is verified as a release promise in CI.
- **Zero-dependency by default**: `ctc run` and `ctc check` are pure interpretation paths that never touch a C compiler; only `ctc build` borrows the local cc.
- **C is the backend**: emitted programs are self-contained — the `ctron_*` runtime is inlined, with system headers only. Any platform with a cc is a target: macOS (arm64 / x86_64), Linux (x86_64 / arm64), and Windows (x86_64, beta).
- **Structured concurrency & error chains**: tasks run under `scope` / `spawn` / `join` — block-scoped, with cancellation propagating out of the scope — and failures flow as `Result` error chains carrying `message` / `cause` / `trace`.
- **The standard library ships as source**: `use std.*` reads source at compile time and merges it with your program into a single AST — as readable and hackable as Python's stdlib, with no precompiled black box.
- **Examples are deliverables**: ctgrep, ctwc, and ctwf in `examples/` are complete CLI tools, and the release acceptance runs right through them.

## Where to go next

| Page | Contents |
|---|---|
| [Getting Started](getting-started.md) | Three ways to install, hello ctron, your first executable, project mode |
| [Examples](examples.md) | Annotated sources of ctgrep / ctwc / ctwf |
| [Download](download.md) | Five-platform asset matrix, checksums, Windows notes |
| [Language Spec](spec/README.md) | v0.7 frozen draft; diagnostics carry stable error codes |
| [Std Reference](std/README.md) | Per-module signature tables |

Repository: [ZturnLibs/Ctron](https://github.com/ZturnLibs/Ctron) · Design and implementation notes live in `docs/superpowers/specs/` and `compiler/BOOTSTRAP.md`.
