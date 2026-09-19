# 下载

> **即将发布**:Releases 尚未上线,以下直链在首次发布(v0.0.1)后生效。当前请按[入门](getting-started.md)的源码路径构建。

## 一键安装(macOS / Linux)

```bash
curl -fsSL https://github.com/Zturn/Ctron/releases/latest/download/install.sh | sh
```

脚本依次做四件事:`uname` 检测平台与架构 → 下载匹配的 tarball → 按 `SHA256SUMS` 校验 → 解压到 `${CTRON_INSTALL_DIR:-$HOME/.ctron}`。结尾打印 PATH 提示,并顺带检测本机 cc——缺失时给出平台化安装指引(不阻塞 `run` / `check`)。可调环境变量:`CTRON_INSTALL_DIR`(安装根)、`CTRON_VERSION`(钉版本)、`CTRON_RELEASE_BASE`(镜像源)。依赖仅 POSIX sh + curl + tar,零 root。

## 产物矩阵

每次发布出 8 个资产,全部可经 `https://github.com/Zturn/Ctron/releases/latest/download/<资产名>` 直达。版本号不带 `v` 前缀,下表以 v0.0.1 为例:

| 资产 | 平台 / 用途 |
|---|---|
| [ctron-0.0.1-darwin-arm64.tar.gz](https://github.com/Zturn/Ctron/releases/latest/download/ctron-0.0.1-darwin-arm64.tar.gz) | macOS Apple Silicon |
| [ctron-0.0.1-darwin-x86_64.tar.gz](https://github.com/Zturn/Ctron/releases/latest/download/ctron-0.0.1-darwin-x86_64.tar.gz) | macOS Intel |
| [ctron-0.0.1-linux-x86_64.tar.gz](https://github.com/Zturn/Ctron/releases/latest/download/ctron-0.0.1-linux-x86_64.tar.gz) | Linux x86_64 |
| [ctron-0.0.1-linux-arm64.tar.gz](https://github.com/Zturn/Ctron/releases/latest/download/ctron-0.0.1-linux-arm64.tar.gz) | Linux arm64 |
| [ctron-0.0.1-windows-x86_64.zip](https://github.com/Zturn/Ctron/releases/latest/download/ctron-0.0.1-windows-x86_64.zip) | Windows x86_64(β) |
| [ctron-0.0.1-src.tar.gz](https://github.com/Zturn/Ctron/releases/latest/download/ctron-0.0.1-src.tar.gz) | 源码线(附预发射 C,cc-only) |
| [SHA256SUMS](https://github.com/Zturn/Ctron/releases/latest/download/SHA256SUMS) | 六个发布件的 SHA-256 校验单 |
| [install.sh](https://github.com/Zturn/Ctron/releases/latest/download/install.sh) | 一键安装脚本 |

预编译包布局:`ctron/bin/`(`ctc` 驱动 + `ctron-cc` / `ctron-chk` / `ctron-emit` 三个原生二进制)、`ctron/lib/ctron/std/`(标准库源码)、`ctron/share/doc/`(README 与 examples)、`ctron/VERSION`(版本 + git-sha)。

## 手动安装

```bash
curl -fsSLO https://github.com/Zturn/Ctron/releases/latest/download/SHA256SUMS
curl -fsSLO https://github.com/Zturn/Ctron/releases/latest/download/ctron-0.0.1-darwin-arm64.tar.gz
shasum -a 256 -c SHA256SUMS | grep OK
tar xzf ctron-0.0.1-darwin-arm64.tar.gz
export PATH="$PWD/ctron/bin:$PATH"      # 写入 shell 配置
```

Windows:解压 zip 到 `%LOCALAPPDATA%\ctron`,把 `ctron\bin` 加入 `PATH`(`setx` 或系统设置)。

## 依赖与平台说明

- `ctc run` / `check` / `test` / `new`:零外部依赖,任何平台开箱即用。
- `ctc build`:需本机 C 编译器(默认 `cc`,`CC` 可覆盖)。macOS `xcode-select --install`;Linux 发行版 gcc。
- Windows(β):仅 `build` 需 mingw-w64(MSYS2 `pacman -S mingw-w64-x86_64-gcc`,或单包免安装的 w64devkit);产物带 `.exe`。
- 标准库以源码随包分发,`use std.*` 在任意工作目录都能解析到装机路径;macOS Gatekeeper 拦截时 `xattr -cr ctron`。

源码线无需任何预编译二进制:`make`(直接编译 `prebuilt/*.c`,只需 cc)→ `make install PREFIX=...`。发射固定点保证这些预发射 C 与任何平台自举产物逐字节一致。
