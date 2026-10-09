<!-- 英文译件:手维护;中文正典 = docs/spec/ 同名文件(经 tools/sync_site_spec.sh 同步至同名 .zh.md) -->
<!-- 译件滞后于正典修订时,以中文正典为准 -->
# §9 Profiles & Interop

## 9.1 The Three-Profile Model

| Profile | Target | Memory | Concurrency | stdlib layer |
|---|---|---|---|---|
| `full` | Linux / macOS / Windows / mobile NDK | concurrent generational GC (pluggable) | tasks + channels + parallelism | `core < alloc < std` |
| `web` | WasmGC (browser / edge / sandbox) | host GC | JSPI / wasm threads | `core < alloc < stdweb` |
| `bare` | Cortex-M / RISC-V / bare-metal wasm | none (arena / static pools) | optional cooperative-scheduler library | `core` |

- Layering is organized by **profile** (not feature flags); packages may declare the minimum layer they require.
- **Compatibility direction (hard rule)**: `bare ⊆ full ⊆ (any profile)` is upward compatible — bare/own code is usable under any profile; code that depends on GC allocation (the alloc attribute) entering bare = E3040 (§6.5 mechanical determination).

## 9.2 The web Profile

- The backend is WasmGC; GC is the host's; task suspension goes through JSPI / stack switching; when wasm threads is enabled, full Send checking is restored (§7.8).
- The JS bridge is typed (no `any` leaking through); JS exceptions are converted to `Result` at the boundary; JS callbacks follow the web-profile Send approximation rules; DOM/Canvas/Fetch go through `stdweb`.
- **The stdweb minimal API (pinned in v0.5, available from P1-D)**: after `use stdweb.dom` — `dom.set_title(Str) -> Void`, `dom.title() -> Str`. The rest of DOM/Canvas/Fetch is expanded release by release following this pattern (anchoring test: `10_web_dom.ct`).
- **Bootstrap-side status (T45 note, v0.9·6)**: ownership convention 2 of the three has landed — `CBox[T]` (a package in the std ffi domain): C→Ctron handoff via `cbox_own`, call-duration borrow via `cbox_borrow` (convention 3), handoff to C via `cbox_into_raw`, explicit release via `cbox_free` (a #[trusted] release surface); the ownership sentinel = a C-side registration table (value copies do not copy the sentinel state; double free / use-after-free panic loudly), and CBox always goes through the compiled channel (the interpreter bridge truncates pointers). On the interpreter side, the extern direct-call bridge closes the float-frame gap (f:/g: frames driven by declared types + Ri:/Rf:/Rg: return aliases + ABI-correct SIMD-class casts + float returns round-tripping losslessly as "%.17g" text; float shapes with more than 4 arguments panic loudly); integer returns widened to full-width long (the int-truncation gap destroyed on the bootstrap side). cimport union/struct/enum keywords qualify pointer parameters (`union Vals*` strips the keyword and resolves through the typedef name table). Anchors: `tests/ffi/` (the three cbox directories / ext_finterp dual-channel / cimport union parameters).
- **Bootstrap-side status (v0.7 note)**: the dynamic surface — `#[dlsym]` runtime thunks (`dlsym(RTLD_DEFAULT)`) + `dlopen/dlclose/dlsym` built in; variadic externs (trailing `...` in the parameter list; non-extern = E4044); `#[link_name(x)]` symbol renaming and stackable attributes; the export surface `#[export]` (wrapping non-static C symbols, for the embedding surface); extern-returning fns unblocked (boxed at the declaration site, dual paths for the env sentinel); `USize` → `size_t` (code "z"); `#[repr(packed)]`/`#[repr(align(N))]`; automatic consumption of C headers via `compiler/tools/cimport.ct` (a decl-level subset: prototypes / #define / scalar structs; 71 bindings measured against the real math.h). Anchors: `tests/ffi/` (cimport/dyn_link/variadic/export/ext_fn_ret/layout).
- **Bootstrap-side status (v0.6 note)**: under `--profile=web`, the `stdweb.dom` checking surface and runtime surface are already usable (`dom` is a built-in namespace; the title is stored in runtime state; the full profile = intercepted with E2020); the emission side produces the `ctron_dom_set_title/ctron_dom_title` C stubs, with the real JS bridge to be substituted by the WasmGC backend (P1-D).

## 9.3 The bare Profile

- Cross-compilation is built in: `ctron build --target <triple>`, with a self-contained toolchain (bundled lld + a minilibc option).
- First bare targets (tier-1): `thumbv7em-none-eabi`, `riscv32imac-unknown-none`; the remaining LLVM-supported targets are tier-2.
- For ISR constraints, explicit allocators, and hard-real-time paths, see §6.6.

## 9.4 Performance Targets (R4 Normalization)

| Path | Metric | Nature |
|---|---|---|
| own / hot-path profiles | within 5% of C in both directions | **hard target** (P3 exit) |
| GC profile | gap to C ≤ 15% | a target with an exit condition: if unmet, tighten the GC default policies and steer hot paths to own (§15 risk mitigation) |
| bare | zero implicit allocation, no GC pauses, deterministically buildable | hard target |

## 9.5 Hardware Utilization

- `Simd[E, N]` fixed-length vectors as first-class citizens; auto-vectorization + comptime unrolling.
- A unified async I/O layer: io_uring / kqueue / IOCP (built into the runtime, colorless API).
- NUMA-aware allocation and task affinity (runtime options).
- GPU (`kernel` blocks → SPIR-V/PTX): reserved for v2, via codegen plugins (§10.5).

- **Bootstrap-side status (T51 note, 2026-10-03)**: the unified async I/O layer's four backends are in the tree
  (the `lib/net/c_src/ctron_rt.c` reactor) — kqueue (darwin) / epoll (linux) /
  **io_uring (linux, runtime-probed: a NODROP feature gate + a boot-time double-NOP self-check; on failure, loudly registered with fallback to
  epoll; suppressed via the env `CTRON_RT_REACTOR=epoll|io_uring`; observed via `ctron_rt_reactor_name()`)**/
  POSIX poll (others). Isomorphic mapping = POLL_ADD is natively one-shot ≙ EV_ONESHOT/EPOLLONESHOT,
  armed swallow-on-delivery / last-wins, zero new contracts; the reactor's enter is single-consumer blocking, eliminating the 100ms fallback tick.
  The `CTRON_RT_NUMA=off|on|auto` option slot + `ctron_rt_numa_nodes()` topology probing are in place
  (linux `/sys/devices/system/node`); aware allocation / task affinity remain aspirations (trigger condition = multi-node
  target machines). IOCP: no windows net target machine; registered as environment-dependent. The dual-arm same-shape smoke test is hooked into `ci.sh`
  (linux = epoll + io_uring double roll call; darwin = kqueue). Simd v0 = semantics / scalar emulation;
  for the vectorization assessment conclusions and aspiration tiers, see `docs/simd-vectorization-analysis.md` (same note as §3.11).

## 9.6 FFI and `#[trusted]`

- An `extern "c"` function declaration + the `#[trusted]` marker = the **only** entry point outside the safe subset; audited at the package level (`ctron lint --trusted`). Declaration syntax (v0.5):

```c
#[trusted]
extern "c" fn ctron_add(a: I64, b: I64) -> I64     // no function body; defined on the C side
```
- **C ABI type mapping** (excerpt): `I8↔int8_t` `USize↔size_t` `F64↔double` `Bool↔bool(C99)` `T[N]↔T[N]` `T[]↔(ptr,len)` (via wrappers); structs are laid out as declared (`#[repr(c)]` is the default for FFI exports).
- **The three ownership conventions** (bindgen generates wrappers according to them):
  1. `C-owned`: C allocates, C frees; Ctron only borrows for the duration of the call;
  2. `Ctron-owned`: transferring ownership across the boundary must go through a wrapper type (e.g. `CBox[T]`), with drop responsibility explicit;
  3. `borrowed`: a temporary borrow whose lifetime is the duration of the call, never escaping the wrapper layer.
- **Forbidden** to hand arena memory to C for long-term holding (freeing it leaves a dangling pointer); when needed, deep-copy it across the boundary.
- **Bootstrap-side status (v0.7 note)**: the dynamic surface — `#[dlsym]` runtime thunks (`dlsym(RTLD_DEFAULT)`) + `dlopen/dlclose/dlsym` built in; variadic externs (trailing `...` in the parameter list; non-extern = E4044); `#[link_name(x)]` symbol renaming and stackable attributes; the export surface `#[export]` (wrapping non-static C symbols, for the embedding surface); extern-returning fns unblocked (boxed at the declaration site, dual paths for the env sentinel); `USize` → `size_t` (code "z"); `#[repr(packed)]`/`#[repr(align(N))]`; automatic consumption of C headers via `compiler/tools/cimport.ct` (a decl-level subset: prototypes / #define / scalar structs; 71 bindings measured against the real math.h). Anchors: `tests/ffi/` (cimport/dyn_link/variadic/export/ext_fn_ret/layout).
- **Bootstrap-side status (v0.6 note)**: fixed the external linkage of extern prototypes (the old implementation used `static`, relying on linker leniency); C-ABI callbacks — fn-type parameters map across the boundary to `ct_fn1..3`, bare fn names pass through unwrapped, and capturing closures are intercepted with E4042 (no env slot); the `#[repr(c)]` struct declaration surface (E4041 intercepts misuse / W8051 warns on non-C-ABI fields), with the emission side emitting the C struct in declaration order — same shape = ABI compatible; Str marshalling via `str_from_c` (arena deep copy, Ctron-owned); boundary governance W8052 (container types) / W8053 (returning fn). Bool crosses the boundary as int (unified across the three lines; ABI-equivalent to bool). Anchoring tests: `tests/ffi/` (the landing point of the §9.8 commitments); performance: `compiler/test/bench_ffi.sh` (scalar calls / struct by value at 1.00× vs pure C). For the defect list and the comparison with mainstream languages, see `docs/ffi-analysis.md`.

## 9.7 Artifacts and Toolchain (Normative Summary)

- A static single binary by default; size targets: bare+core runtime < 100KB (hard target), full complete runtime < 1MB (goal).
- The backend matrix (all pluggable): Cranelift (dev) / LLVM (release) / WasmGC (web) / Wasm MVP (embedded wasm) / the CVM interpreter (comptime, `ctron run` scripts, debugging).
- One command: `ctron build/test/fmt/doc/lint/bench/run/publish/add/target/check`; see §10.2 for `ctron check --format=json`.

## 9.8 Correspondence with the Test Suite

`tests/08_bare.ct` (bare explicit arena), `tests/08_bare_alloc.neg.ct` (bare E3040); FFI multi-file cases are carried by `tests/ffi/` from P1 on (landed in v0.6: four behavior cases + seven negative/lint cases, accepted through both channels — `tests/ffi/run.sh` and the suite.py `ffi/` section).
