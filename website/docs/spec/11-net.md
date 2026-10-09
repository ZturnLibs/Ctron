<!-- 英文译件:手维护;中文正典 = docs/spec/ 同名文件(经 tools/sync_site_spec.sh 同步至同名 .zh.md) -->
<!-- 译件滞后于正典修订时,以中文正典为准 -->
# §11 Networking

> Status: **finalized (v0.8.1, 2026-09-21)** — merged into the spec freeze surface along with the server roadmap's S0 batch; subsequent revisions are recorded as v0.8.x notes. v0.8.1 = write-back of semantics left over from the P3/P4 final review (additive: the four landed AF_UNIX items into §11.2, the resolve pool semantics into §11.5, and a new TLS facade semantics pointer as §11.9); it does not alter the v0.8 frozen surface. Implementation anchors: tests/modules/caps_net (E4010), tests/net/ (behavior).
> Execution-model semantics belong to §7; this chapter defines the network facade, transport semantics, and the HTTP profile layering. Normative conventions (MUST / MUST NOT / SHOULD / MAY) follow the spec README.

## 11.1 Capability Keys (Normative)

- All network access goes through capability objects; the package-level `[caps]` (§2.7) declares the upper bounds, the set the program actually uses ⊆ the declared set, and exceeding them = E4010:

| Key | Granted surface | Consequence without the key |
|---|---|---|
| `net.listen` | bind + accept for `TcpListener`/`UnixListener` | E4010 |
| `net.connect` | `TcpStream`/`UnixStream` connect, `UdpSocket` | E4010 |
| `net.resolve` | DNS resolution (§11.5) | E4010 |

- A `#[pure]` function (§8) touching any network facade = E4020 (rejected at compile time).
- Capability objects cannot be constructed, nor copied out of the grant chain; connection handles derived from `listener.accept()` **inherit the listener's authorization** and do not consume `net.connect` again.
- For the database-access `db.connect` key, see §12.1.

## 11.2 Addresses and the Socket Facade (Normative)

```c
let ln = caps.net.listen(TcpListener.bind("127.0.0.1:0")?)   // :0 = kernel-assigned port
let conn = ln.accept()?                                       // -> TcpStream (inherits authorization)
let n = conn.read(var buf)?                                   // buf: Byte[] / T[N]
conn.write(bytes)?
conn.shutdown(Write)?
let addrs = caps.net.resolve(Dns.name("example.com")?)        // List[SocketAddr], dual-stack
```

- `SocketAddr`: IPv4/IPv6 **dual-stack** (no v6 discrimination); two forms, literal and resolved.
- Value-type handles: `TcpListener`, `TcpStream`, `UdpSocket`, `UnixListener`, `UnixStream` (the Unix family is available only on posix targets; referencing them on a windows target is a compile error, E). **All `impl Drop`** (§6.4): deterministic close on scope exit; classes must not hold them (the §6.2 hard rule applies verbatim).
- Semantic shape: **facade APIs always have "blocking semantics"** — read/write/connect with no callbacks and no Futures; actual concurrency is carried by the runtime (§11.4).
- The underlying file descriptors / `SOCKET` handles are **forbidden** to be touched directly by programs (the emission surface refuses to export them; E anchors are registered with the wave that closes off the emission surface — see the divergences server surface for the register).

### 11.2.1 The Four Landed AF_UNIX Items (v0.8.1 note; implementation anchors: the std/net.ct P3-E surface + tests/net/unix_sock)

On top of this profile's frozen semantics, the Unix-family handles (`UnixListener`/`UnixStream`) pin down the following four landed items (one consistent policy across platforms; silent divergence is not allowed):

1. **Path limit = `min(sizeof sun_path)` = 104 bytes** (darwin 104 / linux 108, including NUL): `strlen(path) >= 104` → `EINVAL` (the shim sets the code uniformly, same shape across platforms; the native per-platform differences are not exposed).
2. **Unlinking a stale socket file before bind = last binder wins**: when a listener is created, an existing path is `unlink`ed first (ENOENT is the normal case and is ignored) — leftover files self-heal, with no alive/dead discrimination (the race surface where a live listener is displaced by a later binder belongs to caller orchestration; the shim does not arbitrate).
3. **Drop only closes the fd, never removes the socket file**: the handle's RAII scope exit only deterministically closes the descriptor; "the last Drop removes the path" would wrongly remove a successor's live path (an inevitable consequence of last-binder-wins semantics), so the file's lifecycle belongs explicitly to the caller.
4. **Explicit cleanup = `net_unix_unlink(path)`**: `ENOENT` also returns `rc < 0` (same shape as a real failure) — the "already gone" discrimination for idempotent cleanup paths is decided by the caller; the shim sets no special case.

## 11.3 Transport Semantics Defaults (Normative)

The server-engineering defaults are pinned here; implementations must not silently deviate:

- **`TCP_NODELAY` on by default** (Nagle disabled; low latency is the default; bulk-throughput scenarios may turn it off explicitly).
- **`SO_KEEPALIVE` on by default**; idle/interval/probes use the platform defaults, with explicit tuning knobs provided.
- Listeners default to `SO_REUSEADDR`; `SO_REUSEPORT` is not in the facade (the P9 multi-tenancy aspiration).
- Facade `write` goes straight to the kernel; no large user-space buffers. Batching / `writev` is there for protocol layers to use explicitly.
- Half-close `shutdown(Write)` is legal; platform differences in the peer reading EOF (winsock) are smoothed over by the shim.
- Read/write timeouts **should** be expressed via context deadline parameters (§11.4); per-call setters are allowed, but same-shape tests must pass under both runtimes.

## 11.4 Execution Model and the Same-Shape-Different-Wiring Contract (Normative)

- Beneath the facade, two runtimes carry the same semantics: the P1 **blocking runtime** (1:1 threads) and the P2 **coroutine runtime** (N:M, §7.1 stackful coroutines). **Same-shape-different-wiring contract: the same program source, with zero changes, has identical observable semantics under both runtimes** — pinned down by mechanical test invariants (run at every wave exit).
- **Suspension-point contract** (coroutine policy): network facade calls, `sleep`, and channel operations are the only suspension points; recognized by the compiler and the runtime, imperceptible and unannotated in user code.
- Stack policy note: §7.1 promises a "growable contiguous stack"; the transitional implementation is a fixed 64KB mmap stack (a subset), and **hitting the top = task-boundary panic** (consistent with §7.1's cap policy); full conformance is reached with the P9 stack-economics project. The C100K/C10M figures are governed by that project; the transitional figures must not be extrapolated.
- **Cancellation**: propagated along the §7.2 task-level cancellation tokens; network operations respond to cancellation at suspension points, returning `NetErr::Cancelled`; on the blocking runtime the policy is join equivalence (cancellation means waiting for completion). Facade operations targeting an already-cancelled scope return `Err(ScopeCancelled)`, semantically aligned with §7.2.
- **FFI discipline (MUST)**: inside `extern "c"` callbacks, no touching the network facade / channels / sleep (coroutine stacks are not reentrant); violators = debug assertion + a declared undefined behavior.
- Timeout budgets: deadlines enter the facade via context parameters; cross-task inheritance follows the scope tree (§7.2).

## 11.5 DNS and Resolution (Normative)

- `resolve(host) -> List[SocketAddr]`: returns multiple records, no v6 discrimination; constrained by the `net.resolve` key.
- Implementation policy: pooled threads running `getaddrinfo` + coroutine wrapping (semantically colorless); c-ares is listed as an aspiration tier.
- Connection orchestration: the caller attempts in order; the connect timeout is independent of the read timeout; happy-eyeballs is listed as an aspiration tier.

### 11.5.1 Asynchronous resolve Semantics (v0.8.1 note; implementation anchor: the ctron_net.c DNS helper pool)

- **Pool-thread semantics**: under the coroutine policy, resolve completes via a 2-thread helper pool + park/wake (the pool is built lazily and stays resident for the process lifetime) — resolution does not sit on the worker hosting the calling coroutine (with workers=1 the old shape starved the whole runtime; differentially proven); on the bare-thread surface (rt not linked), the P1 inline original path is unchanged byte for byte.
- **Not cancellable**: the task-cancellation broadcast **does not interrupt an in-flight resolution** — `getaddrinfo` itself has no cancellation surface; after the coroutine is woken by cancellation it parks again in a `while !done` loop to absorb spurious wakeups, until the resolution returns; **the maximum wait = getaddrinfo itself** (registered policy: the CI surface uses only localhost/numeric hosts, millisecond scale; resolution duration for arbitrary outbound hosts is not covered by the facade's guarantee).
- **Degraded mode**: if pool creation fails entirely (thread creation failure), it degrades to inline blocking on the calling surface (the old semantics; degradation does not hang); signature and return values are unchanged throughout.

## 11.6 Clock and Timers (Normative)

- `now_ns() -> I64`: a monotonic clock built in (on a clock_gettime / QueryPerformanceCounter foundation).
- `sleep_ns`: colorless, cancellable (§11.4).
- Testing mode: the **virtual clock** — in test mode it can jump, and timers fire deterministically (the foundation of deterministic async testing).

## 11.7 HTTP Profile Layering (Overview; Detailed Contracts Finalized Along Roadmap P4/P6)

- **Protocol half-layer**: HTTP/1.1 (RFC 9110/9112) codec, chunked, keep-alive, 100-continue, header/body limits (capability-parameterized), compression negotiation (gzip/deflate to start, zstd as an aspiration tier), SSE, WebSocket (RFC 6455). Version roadmap: HTTP/2 = aspiration tier (P9).
- **Framework half-layer**: comptime routing, middleware (auth/CORS/CSRF/rate limiting/timeouts/security headers), static files (conditional requests/Range), multipart, comptime OpenAPI export.
- Layering discipline: the framework half-layer is forbidden from bypassing the protocol half-layer to touch the network; the protocol half-layer is forbidden from embedding routing/business concepts.
- **Landed-surface note (v0.8.1, P4-A)**: the protocol half-layer's first landed item = `std/http/` (parse.ct for request-line/status-line/header parsing + message.ct for message construction / chunked codec / 100-continue hooks), a **pure-Ctron half-layer with zero use** (it does not import std.net: avoiding the loader's diamond-use false-positive E5020 + keeping it testable on the interpreter, same policy as std/tls.ct); request-smuggling hardening posture is strict (RFC 9112 §5.2 obs-fold rejected, §6.1 TE+CL coexistence rejected, duplicate CL rejected, bare LF rejected); five parameterized limit slots (line / header count / single header / total headers / body, each over-limit with its own err code; the 400/414/431/413/505 mapping belongs to the framework wave); the IO glue (write-side/read-side timeouts over the net facade) lands with the framework half-layer.

## 11.8 Correspondence with the Test Suite

`tests/net/` (loopback discipline: `:0` kernel-assigned ports, zero dependence on the external network), `tests/http/`; anchors `tests/modules/caps_net` (E4010, multi-file package negative case), `r7b_pure_net.neg.ct` (E4020), `r7c_http_caps.neg.ct`; same-shape compatibility invariant fixtures (examples/ctecho source unchanged across runtimes).

## 11.9 TLS Facade Semantics Pointer (v0.8.1; P3-C landed surface, implementation anchors: std/tls.ct + ctron_tls.c)

The three facade semantics are pinned here (details in the std/tls.ct header comment; an isomorphic extension of the §11.2/§11.3 frozen surface):

- **Hostname matching is opt-out by default**: not calling `tls_set_hostname` skips **name matching**; certificate-chain verification (REQUIRED) is **retained** and is not turned off along with it. Passing an empty string = back to the opt-out state. Explicitly validating the host name is the caller's obligation (connection-orchestration-layer craft).
- **Timeouts are per-record**: `tls_read`'s `timeout_ms` takes effect via the mbedTLS `conf_read_timeout` + BIO `recv_timeout` contract and **governs the wait for a single record's arrival**, not "a total budget for the whole record chain" (when a multi-record chain is in hand, the wait resets per record); on timeout it returns `rc < 0` (slot = `SSL_TIMEOUT`). The overall read budget (deadline) belongs to the §11.4 context parameters; the facade sets none.
- **Both EOF forms map to 0**: the two forms of peer close — the BIO-layer fd FIN (mbedTLS fetch_input receiving 0, converted to `CONN_EOF`) and the record-layer close_notify (`PEER_CLOSE_NOTIFY`) — **both map to `tls_read() == 0`** (the eof policy, aligned with §11.2's single read-EOF form); callers do not distinguish them, and must not rely on distinguishing them.
