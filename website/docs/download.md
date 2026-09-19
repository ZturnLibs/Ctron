# Download


## One-liner install (macOS / Linux)

```bash
curl -fsSL https://github.com/ZturnLibs/Ctron/releases/latest/download/install.sh | sh
```

The script does four things: detects platform and architecture via `uname`, downloads the matching tarball, verifies it against `SHA256SUMS`, and extracts to `${CTRON_INSTALL_DIR:-$HOME/.ctron}`. It ends with a PATH hint and a check for a local cc — if none is found it prints platform-specific guidance, without blocking `run` / `check`. Tunables: `CTRON_INSTALL_DIR` (install root), `CTRON_VERSION` (pin a version), `CTRON_RELEASE_BASE` (mirror). Requires only a POSIX sh, curl, and tar — no root.

## Asset matrix

Every release ships 7 assets, all reachable at `https://github.com/ZturnLibs/Ctron/releases/latest/download/<asset>`. Version numbers carry no `v` prefix; v0.0.1 shown as the example:

| Asset | Platform / purpose |
|---|---|
| [ctron-0.0.1-darwin-arm64.tar.gz](https://github.com/ZturnLibs/Ctron/releases/latest/download/ctron-0.0.1-darwin-arm64.tar.gz) | macOS Apple Silicon |
| [ctron-0.0.1-linux-x86_64.tar.gz](https://github.com/ZturnLibs/Ctron/releases/latest/download/ctron-0.0.1-linux-x86_64.tar.gz) | Linux x86_64 |
| [ctron-0.0.1-linux-arm64.tar.gz](https://github.com/ZturnLibs/Ctron/releases/latest/download/ctron-0.0.1-linux-arm64.tar.gz) | Linux arm64 |
| [ctron-0.0.1-windows-x86_64.zip](https://github.com/ZturnLibs/Ctron/releases/latest/download/ctron-0.0.1-windows-x86_64.zip) | Windows x86_64 (beta) |
| [ctron-0.0.1-src.tar.gz](https://github.com/ZturnLibs/Ctron/releases/latest/download/ctron-0.0.1-src.tar.gz) | Source line (pre-emitted C included, cc-only) |
| [SHA256SUMS](https://github.com/ZturnLibs/Ctron/releases/latest/download/SHA256SUMS) | SHA-256 checksums for the release artifacts |
| [install.sh](https://github.com/ZturnLibs/Ctron/releases/latest/download/install.sh) | One-liner install script |

> macOS Intel: no prebuilt tarball in v0.0.1 — use the source line (`ctron-0.0.1-src.tar.gz`, `make` with any cc).

Prebuilt package layout: `ctron/bin/` (the `ctc` driver plus the `ctron-cc` / `ctron-chk` / `ctron-emit` native binaries), `ctron/lib/ctron/std/` (standard library source), `ctron/share/doc/` (README and examples), and `ctron/VERSION` (version + git-sha).

## Manual install

```bash
curl -fsSLO https://github.com/ZturnLibs/Ctron/releases/latest/download/SHA256SUMS
curl -fsSLO https://github.com/ZturnLibs/Ctron/releases/latest/download/ctron-0.0.1-darwin-arm64.tar.gz
shasum -a 256 -c SHA256SUMS | grep OK
tar xzf ctron-0.0.1-darwin-arm64.tar.gz
export PATH="$PWD/ctron/bin:$PATH"      # add to your shell config
```

Windows: unzip into `%LOCALAPPDATA%\ctron` and add `ctron\bin` to your `PATH` (via `setx` or System Settings).

## Dependencies and platform notes

- `ctc run` / `check` / `test` / `new`: zero external dependencies, out of the box on every platform.
- `ctc build`: needs a local C compiler (`cc` by default, override with `CC`). macOS: `xcode-select --install`; Linux: your distribution's gcc.
- Windows (beta): only `build` needs mingw-w64 (MSYS2 `pacman -S mingw-w64-x86_64-gcc`, or the single-file w64devkit); binaries get an `.exe` suffix.
- The standard library ships as source, so `use std.*` resolves against the installed layout from any working directory. If macOS Gatekeeper complains: `xattr -cr ctron`.

The source line needs no prebuilt binaries at all: `make` (compiles `prebuilt/*.c` directly, only needs a cc) followed by `make install PREFIX=...`. The emission fixed point guarantees this pre-emitted C is byte-for-byte identical to what any platform's bootstrap produces.
