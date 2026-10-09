# Getting Started

From install to your first native executable in 10-15 minutes. `ctron` is the toolchain's only user-facing entry point: `run` and `check` never touch a C toolchain; only `build` needs a local C compiler.

## Install

**One-liner (macOS / Linux)** — detects platform and architecture, downloads the matching tarball, verifies it against `SHA256SUMS`, and extracts to `${CTRON_INSTALL_DIR:-$HOME/.ctron}`, printing a PATH hint at the end:

```bash
curl -fsSL https://github.com/ZturnLibs/Ctron/releases/latest/download/install.sh | sh
export PATH="$HOME/.ctron/bin:$PATH"   # add to your shell config as instructed
```

**Manual** — grab `ctron-<version>-<platform>.tar.gz` from [Releases](https://github.com/ZturnLibs/Ctron/releases) and put its `ctron/bin` directory on your `PATH` (see the asset matrix under [Download](download.md)).

**From source** — the source tarball ships the pre-emitted C (`prebuilt/*.c`), so building only needs a cc:

```bash
tar xzf ctron-<version>-src.tar.gz && cd ctron-src-<version>
make && make install PREFIX="$HOME/.ctron"
```

> Verify the install with `ctron --version`: an installed toolchain prints `ctron <version> <git-sha>`, a dev checkout prints `ctron dev`.

## Hello, Ctron

`ctron new` scaffolds a project: `Ctron.ctcl` (the CTCL manifest: name / version / capability declarations) and `src/main.ct` (a hello program). `ctron run` is pure interpretation — parse, check, evaluate in one pass with zero external dependencies, and the fastest way in. (Driver messages are Chinese in v0.0.x; the checker's summary line is English.)

```bash
$ ctron new hello && cd hello
ctron: 已生成 hello/(ctron run hello/src/main.ct 试跑)   # "generated hello/"

$ ctron run src/main.ct
hello, ctron
```

## Static checks

```bash
$ ctron check src/main.ct
check OK decls=1

$ ctron check src/main.ct --format=json
{"diagnostics":[]}
```

Checking stops there and touches no files. `--format=json` emits structured diagnostics for editors and CI.

## Build an executable

```bash
$ ctron build src/main.ct
ctron: 已构建 /path/to/hello/src/main   # "built"

$ ./src/main
hello, ctron
```

`build` first emits equivalent C — `src/main.c` next to your source, readable and self-contained with system headers only — then invokes the local cc (`-O2 -w -pthread`); override the compiler with `CC`. If no cc is found, the C is still emitted and the error message offers two ways out: platform-specific install guidance (macOS `xcode-select --install` / Linux distribution gcc / Windows mingw), or take the `.c` to any machine with a cc and compile it there.

## Project mode

`ctron build` without arguments is project mode: it reads `name` from `Ctron.ctcl`, uses `src/main.ct` as the entry, and produces `build/<name>`. Any `c_src/*.c` files are linked in (the FFI path):

```bash
$ ctron build
ctron: 已构建 build/hello   # "built build/hello"

$ ./build/hello
hello, ctron
```

## Worth remembering

- Subcommands: `run` / `check` / `build` / `test` / `new`; `ctron --help` for the overview, `ctron help <cmd>` for details. Exit codes: `0` success, `1` program diagnostics, `2` ctron environment or usage error.
- Windows: `run` / `check` / `new` have no prerequisites; `build` needs mingw-w64 (MSYS2 `pacman -S mingw-w64-x86_64-gcc`, or the single-file w64devkit) and produces `.exe` binaries.
- `build` and Windows are beta overall: capability boundaries are documented in the root `README.md` and the toolchain distribution design under `docs/superpowers/specs/`.

For a rapid language tour, see [The Ten-Minute Tour](tour.md).

Next: [Examples](examples.md) — three complete CLI tools with annotated sources.
