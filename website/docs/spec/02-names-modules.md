<!-- 英文译件:手维护;中文正典 = docs/spec/ 同名文件(经 tools/sync_site_spec.sh 同步至同名 .zh.md) -->
<!-- 译件滞后于正典修订时,以中文正典为准 -->

# §2 Names & Modules

## 2.1 Structural Model

- **Package**: the distribution and versioning unit, whose root manifest is `Ctron.toml` (→ migrated to **CTCL** `Ctron.ctcl`, see the 2.7 revision note below); the package name = the manifest's `name`, all lowercase.
- **Module**: one `.ct` file = one module; **a directory = a namespace module**, in one-to-one correspondence with the file tree. A module path = a package-relative path.
- A symbol's full path looks like `pkg.module_path.symbol` — physically grep-able (P8).

## 2.2 Imports

- `use pkg.module.symbol;` is an explicit named import; **wildcard imports are forbidden** (`use m.*`) and **re-exports are forbidden** (a module must not re-`pub` imported symbols).
- Group imports: `use net.{TcpListener, Request}` (a trailing comma is allowed).
- Paths always start **from the package root** (Go-style fully qualified); there are no relative imports and no `super/self` path modules. `self` is used only for type reference inside impl (§1.7).
- **Namespaces (2026-09-23; physical lib-root layout 2026-09-30)**: `std.*` = standard library core; **domain packages hang off the top-level namespace** — `net` / `http` / `tls` / `db` / `ffi` (shipped domains; physical `lib/<domain>`, installed at `~/.ctron/lib/<domain>`) plus `web` / `gui` / `s3` / `pkg` (registry upstream; physical `pkgs/<domain>`, installed at `~/.ctron/pkgs/<domain>`). `use net.{...}` resolves the lib-root facade `lib/net.ct`, and `use net.bind.{...}` resolves `lib/net/bind.ct` — the segment mapping mirrors the `std.*` branch segment by segment.
- **Non-std first-segment resolution chain**: ① directory-relative (project-local packages, existing semantics) → ② lib root (the `lib/` directory that parents the std root) → ③ site root (derived from ① `CTRON_SITEPATH` and ② the `pkgs/` directory next to the lib root; registry-installed user packages) → ④ `deps/<pkg>.ctart` artifact fallback (closed-source distribution, S2a). If the whole chain misses, collection stays silent and the driver layer adds W8902 as the installation remedy; E2020 is the final fallback. Normative anchors: `docs/superpowers/specs/2026-09-23-domain-namespace-design.md` + `2026-09-30-libroot-layout-design.md`.

## 2.3 Visibility

| Level | Syntax | Visible scope |
|---|---|---|
| Module-private (default) | (no marker) | this module's file only |
| Package-wide | `pub(pkg)` | all modules in the same package |
| Public | `pub` | any importer |

- Applies to: types, fields, functions, constants, statics, trait items.
- Field visibility is independent of type visibility; fields not marked `pub` cannot be read or written outside the package (structured construction literals are restricted the same way).

## 2.4 Name Resolution

- Resolution order (scope chain): local block → module top level → the `use` import set → the prelude (§3.8).
- **Shadowing is allowed**: a later `let/var` in the same block shadows an outer binding of the same name; duplicate declarations of the same name within one block are forbidden.
- An unresolved name → E2020.

## 2.5 Trait Orphan Rule (coherence)

- `impl T for X` is legal **if and only if** at least one of the trait `T` or the type `X` is defined in the current package; no exceptions, no generic-parameter exemptions. Violation → E5010.
- Corollary: capability gaps on prelude types must be solved on new types within the package (the newtype pattern), or by waiting for stdlib evolution.

## 2.6 Cyclic Dependencies

- **Cyclic dependencies are forbidden both between packages and between modules** (E5020). This is the language-level guarantee behind the compile-speed veto (P4): parsing and checking can be single-pass, and incremental caches can be invalidated per module.

## 2.7 Package Metadata (`Ctron.ctcl`)

> **Revision note (proposed 2026-09-16; hard-cut landed 2026-10-02 via T48)**: the manifest format has completed its migration from the TOML dialect (`Ctron.toml`) to **CTCL (Ctron Config Language, `Ctron.ctcl`)**. The normative definition is [`docs/superpowers/specs/2026-09-16-config-language-v1.md`](https://github.com/ZturnLibs/Ctron/blob/main/docs/superpowers/specs/2026-09-16-config-language-v1.md) (the block-style grammar, the fail-closed registry, the `caps = ["fs"]` list form, the three mutually exclusive deps forms, the three-line parser contract). The TOML surface has been removed: when any of the three lines reads a legacy `Ctron.toml`, it always emits the E5040 migration diagnostic (verbatim identical), and the installer's `ctron build` project mode refuses to build fail-closed; in-repo manifests are 100% migrated (104 files) and `ctron new` emits only `.ctcl`. The TOML example below is kept as historical record only.

```toml
[package]
name    = "myapp"
version = "0.1.0"

[deps]
ctron-http = "1.2"          # strict semver; pinned by the lockfile

[caps]                      # capability declarations (§8.2): use beyond grants = compile error
fs.read = true
net.listen = true
net.connect = true
net.resolve = true
db.connect = true

[profile]                   # profile and target (§9)
default = "full"
```

- Dependency resolution: strict semver + lockfile (content-addressed); workspace support.
- Capability declarations are the package-level **ceiling**: the set of capabilities the program actually uses must be ⊆ the declared set; exceeding it → E4010.

> **Landing note (2026-10-06 T49)**: dependency resolution v0 is implemented — `Ctron.lock` (content-addressed:
> `lock {}` + `pkg "name" { version/source/path|git/rev/digest }`, the CTCL canonical form,
> generated by `ctron-dep`, byte-stable across two runs; resolution priority = lock pinning → installed `pkgs/` → registry);
> workspace (the `workspace {}` root marker + `member "name" { path }` keyed blocks, registered across all four lines);
> `ctron publish/add/lock` with a local registry surface (the pure directory protocol `CTRON_REGPATH`/`~/.ctron/registry`,
> zero network; publish for single-file packages + versions are immutable); resolution chain ② = the deps-table probe (after §2.2 chain step ① directory-relative),
> and ⑤ = the `deps/<pkg>.ctart` fallback slot, reserved pending S2a. Design:
> docs/superpowers/plans/2026-10-06-t49-lockfile-workspace-add-publish.md

## 2.8 Correspondence with the Test Suite

Executable counterexamples for the orphan rule/cyclic dependencies are multi-file cases, carried by `tests/modules/` from P1 on (the test format is defined in `tests/README.md`).
