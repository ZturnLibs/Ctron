<div align="center">

English | [简体中文](README.zh-CN.md) | [Deutsch](README.de.md) | [Français](README.fr.md) | [Español](README.es.md) | [日本語](README.ja.md)

</div>

# Ctron

**The AI-native systems programming language** — built for "AI writes, humans review": errors caught at compile time, code that means exactly what it says, no magic.

```bash
ctron run main.ct      # interpret and run — zero external dependencies
ctron check main.ct    # static checks — --format=json for structured diagnostics
ctron build main.ct    # emit equivalent C → local cc → native executable
```

- Repository: <https://github.com/ZturnLibs/Ctron>
- Docs site: <https://zturnlibs.github.io/Ctron/> (English / 简体中文)

---

> **Project status:** Ctron is under active development and **not yet recommended for production** — the language and toolchain are still evolving. Interested developers are very welcome to build it with us: see [Contributing](#contributing).

## Why Ctron

AI-generated code has a stable failure pattern: **correct structure, wrong details** — misspelled identifiers, type mismatches, missed edge cases. Dynamic languages surface these at runtime; in C/C++ they can become undefined behavior. Ctron's answer is not "a better AI" but stricter language design:

- **Every diagnostic carries a stable error code** (E2010, E2030, E3040…). Given a code, an AI can fix precisely — no guessing intent from prose.
- **Structured diagnostics** (`ctron check --format=json`) consumed directly by editors and CI pipelines.
- **Unambiguous grammar and explicit type-inference rules** — the same code gets the same verdict everywhere.

**No magic.** What an AI fears in a language is not complex syntax but what the language does behind its back:

| What Ctron doesn't have | What you use instead |
|---|---|
| No null | `Option[T]` |
| No implicit numeric conversions | explicit `as[T]()` |
| No operator overloading | `+` means addition |
| No macros | no textual substitution — source is the truth |
| No undefined behavior | the spec enumerates every legal behavior; what it doesn't state doesn't exist |

**Everything is source.** The standard library ships as Ctron source, not compiled binaries — `use std.*` reads the source at compile time and merges it into a single AST with your program. An AI can read stdlib source to understand API behavior — no docs required.

## Quick start

```bash
# macOS / Linux one-line install (SHA256 verified)
curl -fsSL https://github.com/ZturnLibs/Ctron/releases/latest/download/install.sh | sh
export PATH="$HOME/.ctron/bin:$PATH"
```

```bash
$ ctron new hello && cd hello     # scaffolds Ctron.toml + Ctron.ctcl + src/main.ct

$ ctron run src/main.ct           # parse, check, evaluate — one pipeline
hello, ctron

$ ctron check src/main.ct --format=json
{"diagnostics":[]}

$ ctron build src/main.ct         # emits readable, self-contained C, then calls local cc
$ ./src/main
hello, ctron
```

`run` / `check` / `new` need no C toolchain; only `build` requires a local cc (`CC` env overrides). On Windows, `run` / `check` work out of the box; `build` needs mingw-w64. Exit codes: `0` success / `1` program diagnostics failed / `2` environment or usage error.

## The language at a glance

No null, `Result` propagation, error chains, in-language tests:

```ctron
@derive(Error)
enum MathErr {
    DivByZero
}

fn safe_div(a: I32, b: I32) -> Result[I32, MathErr] {
    if b == 0 { return Err(DivByZero) }
    return Ok(a / b)
}

fn ratio(a: I32, b: I32, c: I32) -> Result[I32, MathErr] {
    let x = safe_div(a, b)?                   // `?` propagates, attaching context
    return safe_div(x, c)
}

test "result propagation with ?" {
    assert_eq(ratio(100, 5, 4).or(-1), 5)
    match ratio(1, 0, 2) {
        Err(DivByZero) => assert(true)
        Ok(_)          => assert(false)
    }
}

test "error context chain" {
    let res = ratio(1, 0, 1).context("computing ratio")
    match res {
        Err(e) => {
            assert_eq(e.message, "computing ratio")
            assert(e.cause.is_some())          // the cause chain keeps the root
        }
        Ok(_) => assert(false)
    }
}
```

A real, complete tool (`examples/ctwc`, a wc-style utility):

```ctron
use std.str.{words, count_ch}

fn main() -> I32 {
    var path = ctron_entry()
    if path == "" {
        println("ctwc: usage: ctwc run <file>")
        return 1
    }
    match read_file(path) {
        Some(s) => {
            var nw = words(s).len
            var nl = count_ch(s, 10)
            println(nl.to_string() + " " + nw.to_string() + " " + s.len.to_string() + " " + path)
        }
        None => {
            println("ctwc: cannot open " + path)
            return 1
        }
    }
    return 0
}
```

Going further:

- **Structured concurrency** — tasks live under `scope` / `spawn` / `join`, their lifetime bound to the block scope; cancellation propagates along the scope, and a panicking task triggers a structured cancellation broadcast. All three `Send` checkpoints run at compile time.
- **Two memory tiers** — on top of the GC sit `own` blocks (a no-GC-memory subset within a scope) and `Arena`; the `bare` profile rejects GC allocation entirely and targets bare-metal microcontrollers.
- **Compile-time execution** — `const` / `comptime fn` evaluate at compile time; `@derive(Show, Eq)` synthesizes common impls; code blocks in `///` doc comments are compiled and executed (doc-test).
- **Explicit capabilities** — I/O permissions are explicit `Cap` objects declared in the package manifest; overreach fails at compile time.
- **First-class FFI** — `extern "c"` declarations plus `c_src/*.c` linked in automatically.

## Two execution paths

```
ctron run main.ct      interpret: zero external dependencies, fastest dev loop
ctron build main.ct    emit C → local cc: native executable, same language end to end
```

The emitted artifact is **readable, self-contained C** (runtime fully inlined, system headers only), kept next to the source for audit. One source codebase spans development and deployment.

## Standard library and domain packages

The standard library ships **as source** alongside the toolchain; domain packages attach to top-level namespaces: `use std.*` / `use net.*` / `use gui.*`.

| Package | Contents |
|---|---|
| `std` | 26 modules: json / csv / uuid / crypto (SHA-256, HMAC, PBKDF2) / fmap / heap / iter / sort / unicode / strconv / rand / path / time… |
| `net` | TCP transport facade: listen / connect / accept / read / write |
| `http` | HTTP/1.1 parser + client + WebSocket + SSE + a **web-framework middleware family** (router / JWT auth / cors / csrf / openapi / metrics / timeout / static…) |
| `tls` | TLS facade |
| `db` | pure-Ctron wire-protocol drivers: **PostgreSQL** / RESP2 (Redis) + connection pool + row mapping |
| `ffi` | C-boundary error wrapping (errno folded into `Result`) |

## Toolchain at a glance

`ctron` is the single user entry point:

| Command | Purpose |
|---|---|
| `run` / `check` / `build` | interpret / static checks (JSON diagnostics) / emit and build |
| `test` | run in-language `test` blocks |
| `fmt` | canonical formatter |
| `lint` | diagnostic summary (`--strict` escalates warnings to errors) |
| `doc` | interface projection: public symbol table + contract comments |
| `bench` | benchmark families: lang / gc / http / net / ffi |
| `new` | project scaffolding |
| `add` / `publish` / `lock` | install packages / publish / lockfile |

## Editor support

- **Language server** (`lsp/`) — diagnostics, documentSymbol, hover, completion, definition/references, rename, signature help, inlay hints, formatting, folding.
- **VS Code extension** (`editors/vscode-ctron`) — syntax highlighting, snippets, on-save semantic diagnostics with quick fixes, semantic tokens, parameter-name inlay hints; also supports CTML (GUI markup) and CTCL (config language).

## Status and boundaries

- The first release, **v0.0.1**, is out; the language spec is a frozen draft (v0.8). If the download path is unavailable, build from source — see [compiler/BOOTSTRAP.md](compiler/BOOTSTRAP.md).
- Good fit: CLI tools, network services, compilers, parsers.
- Not a fit: GPU-heavy compute; hard real-time systems (GC pauses are uncontrollable — the `bare` profile mitigates but does not eliminate them).
- `build` and Windows support are overall in β.

## Contributing

Issues, examples, docs, and code are all welcome — most contributions need no knowledge of compiler internals.

**Report a bug** — open an issue with a minimal `.ct` reproducer and the output of `ctron check <file> --format=json`. Diagnostics carry stable error codes, so fixes can be precise.

**Grow the ecosystem** — standard-library modules, examples, editor tooling, docs, and the website: PRs welcome.

**Evolve the language** — the spec is a frozen draft: start with a design discussion in an issue; accepted changes land as a spec revision entry plus corpus pins in `tests/`. Language behavior must stay aligned across all three implementations (C seed / Rust reference / self-hosted compiler), with the conformance suite as arbiter. New diagnostics need a stable error code registered in the spec's error-code registry.

**Dev workflow**

```bash
git clone https://github.com/ZturnLibs/Ctron && cd Ctron
bash ci.sh    # one command: bootstraps the toolchain, runs the full 9-stage acceptance gate
```

PRs should land with `ci.sh` green. Keep `ctron` (sh) and `ctron.ps1` / `ctron.cmd` behaviorally identical. Commit messages follow conventional commits (`feat:` / `fix:` / `docs:`).

## Documentation map

| Document | Contents |
|---|---|
| [docs/spec/](docs/spec/README.md) | Language spec v0.8 (12 chapters) |
| [compiler/BOOTSTRAP.md](compiler/BOOTSTRAP.md) | Building the toolchain from source |
| [examples/](examples/) | Complete example applications |
| [website/](https://zturnlibs.github.io/Ctron/) | Getting started, stdlib reference, examples, downloads |

## License

MIT — see [LICENSE](LICENSE).
