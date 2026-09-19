# 工具链分发 · 计划 2:Windows / 源码线 / 打包与 CI 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 spec 的发布工程半场:ctc PowerShell 驱动与 Windows 专项、源码线 Makefile/install.sh、release.sh 打包、双阶段 GitHub Actions 五平台发布。

**Architecture:** Windows 侧 = ctc.ps1(与 sh 版同命令面)+ cmd 垫片 + mingw 专项旗标(栈/.exe);源码线 = 预发射 C 三件 + cc-only Makefile;发布 = 每平台组装同一目录树 → tar.gz/zip → 双阶段 workflow(Stage 1 出预发射 C 与四平台二进制,Stage 2 Windows 编译预发射 C)→ 汇总 Release。

**Tech Stack:** PowerShell 5+(windows-latest 自带)、MSYS2 mingw-w64、GitHub Actions、POSIX sh。

## Global Constraints

- **命令面基准 = 计划 1 的 `ctc`(sh)**:ctc.ps1 的子命令、参数、帮助文本、rc 约定必须与之同文;conformance 校验纳入两平台门禁。
- Windows 专项:`CC` 默认 `gcc`;cc 参数 `-O2 -w -pthread -Wl,--stack,8388608`;产物加 `.exe` 后缀;二进制 `_WIN32` 样板(Task 1 随 Ctron 源一次落)。
- 所有产物树布局以 spec §2 为准:`bin/ + lib/ctron/std/ + share/doc/ + VERSION`。
- workflow 发布门:**全部平台 job 绿才 publish**;任一失败 → 不发版(比 spec 的 pre-release 降级更严,v0 简化,记录在案)。
- 前置:计划 1 已全部落地(ctc、ctron-chk、三内建、std 三级解析)。

---

### Task 1: Windows 编译器面补丁(随发射样板一次落)

**Files:**
- 无——已由计划 1 Task 1/2 完成(`#include <windows.h>` 条件段、`SetConsoleOutputCP(CP_UTF8)`、`ctron_exe_path` 的 `GetModuleFileNameA` 分支均已入 `driver_emit.ct` 样板)。

- [ ] **Step 1: 验证 _WIN32 面已在**

Run: `grep -c "SetConsoleOutputCP\|GetModuleFileNameA\|_NSGetExecutablePath\|readlink" compiler/src/driver_emit.ct`
预期:≥ 4 处。缺哪条补哪条(按计划 1 Task 2 Step 5 的代码)。

- [ ] **Step 2: 交叉语法自检(mingw 若本机有)**

```bash
sh compiler/build.sh && compiler/ctc.sh emit compiler/build/cc_emit.ct /tmp/w32probe.c
x86_64-w64-mingw32-gcc -O2 -w -pthread -o /tmp/w32probe.exe /tmp/w32probe.c 2>&1 | head -5 || echo "本机无 mingw,留待 CI"
```
预期:本机有 mingw 则零告警出 .exe;无则 CI 兜底。

- [ ] **Step 3: Commit(若有补)**

```bash
git add compiler/src/driver_emit.ct
git commit -m "fix(dist): _WIN32 编译器面查漏(条件 include/UTF-8 控制台/exe 路径)"
```

---

### Task 2: ctc.ps1 + ctc.cmd(Windows 驱动)

**Files:**
- Create: `ctc.ps1`(仓库根)
- Create: `ctc.cmd`(仓库根)

**Interfaces:**
- Consumes: `ctron-{cc,chk,emit}.exe`(同目录)。
- Produces: 与 sh 版同命令面;Windows 旗标差集收在 `Build-Args` 一处。

- [ ] **Step 1: 写入 ctc.ps1**

```powershell
# ctc.ps1 —— Ctron 工具链用户驱动(Windows 版;命令面基准 = sh 版 ctc)
# rc 约定:0 成功 / 1 程序诊断失败 / 2 ctc 环境或用法错误(exit code 同 sh)
$ErrorActionPreference = 'Stop'
$Bin = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = Split-Path -Parent $Bin
if ($env:CC) { $Cc = $env:CC } else { $Cc = 'gcc' }
$global:LASTEXITCODE = 0

function Usage {
@'
ctc - Ctron 工具链驱动
用法:
  ctc run <file.ct>          解释执行(不需要 C 编译器)
  ctc check <file.ct> [--format=json] [--profile=bare]
                             静态检查
  ctc build <file.ct>        发射 C -> 本机 cc -> 可执行 <stem>(C 侧 <stem>.c)
  ctc build                  项目模式:读 Ctron.toml(入口 src/main.ct,链接 c_src/*.c)
  ctc test <file.ct>         test 块执行(无 main 走解释;含 main 文件的 test 执行挂账)
  ctc new <dir>              脚手架:hello + Ctron.toml
  ctc --version              版本
  ctc --help | help [cmd]    帮助(亦可 ctc <cmd> --help)
环境变量:CC(默认 gcc)、CTRON_STDPATH(覆盖标准库位置)
rc 约定:0 成功 / 1 程序诊断失败 / 2 ctc 环境或用法错误
'@
}

function Help-Cmd($c) {
  if ($c -eq 'build') {
@'
ctc build - 发射 C 并编译为可执行
  ctc build <file.ct>   产物 <stem>.exe 与 <stem>.c(与源同目录)
  ctc build             项目模式:入口 src/main.ct,产物 build/<name>.exe
前置:mingw-w64(MSYS2 或 w64devkit);缺失时 C 照常发射,并给出两条出路
'@
  } else { Usage }
}

function Have-Cc {
  $probe = Join-Path ([System.IO.Path]::GetTempPath()) ("ctcp" + [guid]::NewGuid().ToString('N'))
  Set-Content -Path "$probe.c" -Value 'int main(void){return 0;}' -NoNewline
  & $Cc -O0 -w -o "$probe.exe" "$probe.c" 2>$null | Out-Null
  $ok = ($LASTEXITCODE -eq 0) -and (Test-Path "$probe.exe")
  if (Test-Path "$probe.exe") { Remove-Item "$probe.exe" }
  if (Test-Path "$probe.c") { Remove-Item "$probe.c" }
  return $ok
}

function Cc-Missing($cfile) {
  [Console]::Error.WriteLine("ctc: 未找到可用的 C 编译器('$Cc' 冒烟失败) - ctc build 需要。")
  [Console]::Error.WriteLine("  ① 安装后重跑:MSYS2 'pacman -S mingw-w64-x86_64-gcc' 或 w64devkit")
  [Console]::Error.WriteLine("  ② C 已发射到 $cfile - 可手动编译,或拿到任何有 gcc 的机器上")
}

function Build-File($f) {
  $full = (Resolve-Path $f).Path
  $dir = Split-Path -Parent $full
  $stem = [System.IO.Path]::GetFileNameWithoutExtension($full)
  & (Join-Path $Bin 'ctron-emit.exe') run $full | Set-Content -Path (Join-Path $dir "$stem.c") -Encoding Ascii
  if ($LASTEXITCODE -ne 0) { exit 1 }
  if (-not (Have-Cc)) { Cc-Missing (Join-Path $dir "$stem.c"); exit 2 }
  Push-Location $dir
  & $Cc -O2 -w -pthread -Wl,--stack,8388608 "$stem.c" -o "$stem.exe"
  $rc = $LASTEXITCODE
  Pop-Location
  if ($rc -ne 0) { exit 2 }
  Write-Output "ctc: 已构建 $(Join-Path $dir $stem).exe"
}

function Build-Proj {
  if (-not (Test-Path 'Ctron.toml')) { [Console]::Error.WriteLine('ctc: 项目模式需 Ctron.toml'); exit 2 }
  $name = (Select-String -Path Ctron.toml -Pattern '^name\s*=\s*"(.*)"').Matches.Groups[1].Value
  if (-not $name) { $name = Split-Path -Leaf (Get-Location) }
  if (-not (Test-Path 'src/main.ct')) { [Console]::Error.WriteLine('ctc: 缺入口 src/main.ct'); exit 2 }
  New-Item -ItemType Directory -Force -Path build | Out-Null
  & (Join-Path $Bin 'ctron-emit.exe') run (Resolve-Path 'src/main.ct').Path | Set-Content -Path "build/$name.c" -Encoding Ascii
  if ($LASTEXITCODE -ne 0) { exit 1 }
  if (-not (Have-Cc)) { Cc-Missing "build/$name.c"; exit 2 }
  $srcs = @(Get-ChildItem -Path 'c_src/*.c' -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName })
  & $Cc -O2 -w -pthread -Wl,--stack,8388608 "build/$name.c" @srcs -o "build/$name.exe"
  if ($LASTEXITCODE -ne 0) { exit 2 }
  Write-Output "ctc: 已构建 build/$name.exe"
}

if ($args.Count -lt 1) { Usage; exit 2 }
$cmd = $args[0]; $rest = @($args | Select-Object -Skip 1)
switch ($cmd) {
  { $_ -in '--help','-h','help' } { if ($rest.Count -ge 1) { Help-Cmd $rest[0] } else { Usage }; exit 0 }
  { $_ -in '--version','-V' } {
      $vfile = Join-Path $Root 'VERSION'
      if (Test-Path $vfile) { Write-Output "ctron $(Get-Content $vfile -Raw)".Trim() } else { Write-Output 'ctron dev' }
      exit 0 }
  'run' {
      if ($rest.Count -lt 1) { [Console]::Error.WriteLine('ctc: run 需要输入文件'); exit 2 }
      & (Join-Path $Bin 'ctron-cc.exe') run $rest[0]; exit $LASTEXITCODE }
  'check' {
      if ($rest.Count -lt 1) { [Console]::Error.WriteLine('ctc: check 需要输入文件'); exit 2 }
      $full = (Resolve-Path $rest[0]).Path
      & (Join-Path $Bin 'ctron-chk.exe') run $full @($rest | Select-Object -Skip 1); exit $LASTEXITCODE }
  'test' {
      if ($rest.Count -lt 1) { [Console]::Error.WriteLine('ctc: test 需要输入文件'); exit 2 }
      & (Join-Path $Bin 'ctron-cc.exe') run $rest[0]; exit $LASTEXITCODE }
  'build' { if ($rest.Count -ge 1) { Build-File $rest[0] } else { Build-Proj } }
  'new' {
      if ($rest.Count -lt 1) { [Console]::Error.WriteLine('ctc: new 需要目录名'); exit 2 }
      New-Item -ItemType Directory -Force -Path "$($rest[0])/src" | Out-Null
      Set-Content -Path "$($rest[0])/Ctron.toml" -Value "name = `"$(Split-Path -Leaf $rest[0])`"`n`n[caps]"
      Set-Content -Path "$($rest[0])/src/main.ct" -Value "fn main() {`n    println(`"hello, ctron`")`n}"
      Write-Output "ctc: 已生成 $($rest[0])/"
      exit 0 }
  default { [Console]::Error.WriteLine("ctc: 未知子命令 '$cmd'(详见 ctc --help)"); exit 2 }
}
```

- [ ] **Step 2: 写入 ctc.cmd(垫片)**

```bat
@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0ctc.ps1" %*
exit /b %ERRORLEVEL%
```

- [ ] **Step 3: 验证(Git Bash 下跑 cmd 垫片,或留 CI)**

本机为 darwin,无法跑 PS——语法验证留给 Task 6 的 windows CI job;本机仅做 `powershell -Command "Get-Command"` 不适用的如实记录,并把"ps1 语法解析"加进 Task 6 门禁。

- [ ] **Step 4: Commit**

```bash
git add ctc.ps1 ctc.cmd
git commit -m "feat(dist): ctc.ps1 + cmd 垫片——与 sh 版同命令面,mingw 旗标(-Wl,--stack/.exe/gcc 默认)"
```

---

### Task 3: 源码线 Makefile + install.sh

**Files:**
- Create: `Makefile`(仓库根;源码 tarball 同体复用)
- Create: `install.sh`(仓库根;install.ps1 挂账不建)

**Interfaces:**
- Consumes: `prebuilt/{ctron-cc,ctron-chk,ctron-emit}.c`(release.sh 产,Task 4)、`std/`、`ctc`。
- Produces: `make`(cc-only,出 `bin/` 三件)、`make install PREFIX=…`(装出 spec §2.1 布局)。

- [ ] **Step 1: Makefile**

```make
# Makefile —— 源码线:预发射 C → 工具链(cc-only;seed 引导走 git 仓库,不进 tarball)
CC ?= cc
CFLAGS ?= -O2 -w -pthread
PREFIX ?= $(HOME)/.ctron
DEST = $(PREFIX)/ctron

.PHONY: all install clean
all: bin/ctron-cc bin/ctron-chk bin/ctron-emit

bin:
	mkdir -p bin
bin/ctron-%: prebuilt/%.c | bin
	$(CC) $(CFLAGS) -o $@ $<

install: all
	install -d $(DEST)/bin $(DEST)/lib/ctron/std $(DEST)/share/doc
	install bin/ctron-cc bin/ctron-chk bin/ctron-emit ctc $(DEST)/bin/
	cp -R std/. $(DEST)/lib/ctron/std/
	install README.md docs/superpowers/specs/2026-09-17-toolchain-distribution-design.md $(DEST)/share/doc/ 2>/dev/null || true

clean:
	rm -rf bin
```

- [ ] **Step 2: install.sh**

```sh
#!/bin/sh
# install.sh —— 一键安装(curl -fsSL <releases>/install.sh | sh)
# 平台检测 → 下载 tarball → sha256 校验 → 解压到 ${CTRON_INSTALL_DIR:-$HOME/.ctron}
set -eu
BASE=${CTRON_RELEASE_BASE:-https://github.com/ZturnLibs/Ctron/releases/latest/download}
VER=${CTRON_VERSION:-}
DIR=${CTRON_INSTALL_DIR:-$HOME/.ctron}
OS=$(uname -s); M=$(uname -m)
case $OS in Darwin) os=darwin ;; Linux) os=linux ;; *) echo "install.sh: 不支持的平台 $OS" >&2; exit 2 ;; esac
case $M in arm64|aarch64) arch=arm64 ;; x86_64|amd64) arch=x86_64 ;; *) echo "install.sh: 不支持的架构 $M" >&2; exit 2 ;; esac
if [ -z "$VER" ]; then
    VER=$(curl -fsSL "https://api.github.com/repos/ZturnLibs/Ctron/releases/latest" | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p')
fi
[ -n "$VER" ] || { echo "install.sh: 取版本失败(可设 CTON_VERSION=x.y.z 重试)" >&2; exit 2; }
TARBALL="ctron-${VER#v}-${os}-${arch}.tar.gz"
echo "install.sh: 下载 $BASE/$TARBALL"
TMP=$(mktemp -d /tmp/ctron_install.XXXXXX)
trap 'rm -rf "$TMP"' EXIT
curl -fsSL "$BASE/$TARBALL" -o "$TMP/$TARBALL"
curl -fsSL "${BASE%/*/*}/download/$VER/SHA256SUMS" -o "$TMP/SHA256SUMS" 2>/dev/null || true
if [ -s "$TMP/SHA256SUMS" ] && command -v shasum >/dev/null 2>&1; then
    (cd "$TMP" && grep "$TARBALL" SHA256SUMS | shasum -a 256 -c -)
fi
mkdir -p "$DIR"
tar xzf "$TMP/$TARBALL" -C "$TMP"
cp -R "$TMP"/ctron/. "$DIR/ctron/" 2>/dev/null || { mkdir -p "$DIR/ctron" && cp -R "$TMP"/ctron/. "$DIR/ctron/"; }
echo "install.sh: 已装到 $DIR/ctron"
"$DIR/ctron/bin/ctc" --version
echo "export PATH=\"$DIR/ctron/bin:\$PATH\"   # 加入你的 shell 配置"
command -v "$CC" >/dev/null 2>&1 || command -v cc >/dev/null 2>&1 || {
    echo "提示: 未检测到 C 编译器 —— ctc run/check 不需要;ctc build 需要" >&2
    case $os in
        darwin) echo "  macOS: xcode-select --install" >&2 ;;
        linux)  echo "  linux: 发行版包管理器装 gcc(如 apt install gcc)" >&2 ;;
    esac
}
```

- [ ] **Step 3: 本机干跑 install.sh(指向本地假 releases 目录)**

```bash
VER=0.1.0-test CTRON_RELEASE_BASE=file:///tmp/fakerel CTRON_VERSION=0.1.0-test sh -c 'true'
# 造 /tmp/fakerel/ctron-0.1.0-test-darwin-arm64.tar.gz(用 Task 4 的 release.sh 先产)后:
CTRON_RELEASE_BASE=file:///tmp/fakerel CTRON_VERSION=0.1.0-test CTRON_INSTALL_DIR=/tmp/ctroninst sh install.sh
```
预期:装到 /tmp/ctroninst/ctron,`ctc --version` 打 0.1.0-test,末尾 PATH 提示。

- [ ] **Step 4: Commit**

```bash
git add Makefile install.sh
git commit -m "feat(dist): 源码线 Makefile(cc-only)+ install.sh 一键装(平台检测/sha256/cc 指引)"
```

---

### Task 4: tools/release.sh(打包)

**Files:**
- Create: `tools/release.sh`

**Interfaces:**
- Consumes: `compiler/native.sh` 四件套、`std/`、`ctc`、`compiler/build.sh` 的预发射能力。
- Produces: `dist/ctron-<ver>-<os>-<arch>.tar.gz`(当前平台)+ `dist/prebuilt/*.c` + `VERSION`;源码 tarball 与 zip 由 workflow 汇总时组装(release.sh 只做单平台)。

- [ ] **Step 1: release.sh**

```sh
#!/bin/sh
# release.sh —— 单平台打包(release 工程每平台各跑一次)
#   用法: tools/release.sh <version>
#   产物: dist/ctron-<ver>-<os>-<arch>.tar.gz
#         dist/prebuilt/{ctron-cc,ctron-chk,ctron-emit}.c
#         dist/VERSION
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
VER=${1:?用法: release.sh <version>}; VER=${VER#v}
OS=$(uname -s | tr '[:upper:]' '[:lower:]'); M=$(uname -m)
case $M in arm64|aarch64) ARCH=arm64 ;; x86_64) ARCH=x86_64 ;; *) echo "不支持的架构 $M" >&2; exit 2 ;; esac
DIST="$DIR/dist"; PKG="$DIST/ctron"
rm -rf "$DIST" && mkdir -p "$PKG/bin" "$PKG/lib/ctron" "$PKG/share/doc" "$DIST/prebuilt"

sh "$DIR/compiler/build.sh"
sh "$DIR/compiler/native.sh"
install -m 755 "$DIR/compiler/bin/ctron-cc" "$DIR/compiler/bin/ctron-chk" "$DIR/compiler/bin/ctron-emit" "$PKG/bin/"
install -m 755 "$DIR/ctc" "$PKG/bin/ctc"
cp -R "$DIR/std/." "$PKG/lib/ctron/std/"
cp "$DIR/README.md" "$PKG/share/doc/" 2>/dev/null || true
cp -R "$DIR/examples" "$PKG/share/doc/examples"
printf '%s %s\n' "$VER" "$(git -C "$DIR" rev-parse --short HEAD)" > "$PKG/VERSION"

# 预发射 C(固定点:任何平台发射应逐字节一致,workflow 内跨平台 diff 验证)
TMP=$(mktemp -d /tmp/ctron_rel.XXXXXX) && trap 'rm -rf "$TMP"' EXIT
"$DIR/compiler/ctc.sh" emit "$DIR/compiler/build/cc_run.ct"   "$DIST/prebuilt/ctron-cc.c"   >/dev/null
"$DIR/compiler/ctc.sh" emit "$DIR/compiler/build/cc_check.ct" "$DIST/prebuilt/ctron-chk.c"  >/dev/null
"$DIR/compiler/ctc.sh" emit "$DIR/compiler/build/cc_emit.ct"  "$DIST/prebuilt/ctron-emit.c" >/dev/null

# 源码 tarball 组装件(仅打包,不重复构建)
SRC="$DIST/ctron-src-$VER"; mkdir -p "$SRC/prebuilt"
cp -R "$DIR/compiler/src" "$SRC/compiler-src"
cp -R "$DIR/std" "$SRC/std"
cp "$DIST/prebuilt/"*.c "$SRC/prebuilt/"
cp "$DIR/ctc" "$SRC/"; cp "$DIR/Makefile" "$SRC/"; cp "$DIR/README.md" "$SRC/"
cp "$DIR/install.sh" "$SRC/" 2>/dev/null || true
printf '%s %s\n' "$VER" "$(git -C "$DIR" rev-parse --short HEAD)" > "$SRC/VERSION"

tar -C "$DIST" -czf "$DIST/ctron-$VER-$OS-$ARCH.tar.gz" ctron
tar -C "$DIST" -czf "$DIST/ctron-$VER-src.tar.gz" "ctron-src-$VER"
cd "$DIST" && shasum -a 256 ctron-$VER-*.tar.gz > SHA256SUMS
echo "release.sh: 产物在 $DIST/"
ls -la "$DIST"
```

- [ ] **Step 2: 本机干跑**

```bash
sh tools/release.sh 0.1.0-test
mkdir -p /tmp/acc && tar xzf dist/ctron-0.1.0-test-*.tar.gz -C /tmp/acc
/tmp/acc/ctron/bin/ctc --version
cd /tmp && /tmp/acc/ctron/bin/ctc run /tmp/acc/ctron/share/doc/examples/ctwc/src/main.ct -- <样例输入或空>
```
预期:版本正确;examples 经**装机布局**(工作目录 ≠ 安装目录)跑通——这一步同时是 §7 验收第 2 项(std 解析回归)的手工证明。

- [ ] **Step 3: Commit**

```bash
git add tools/release.sh
git commit -m "feat(dist): release.sh 单平台打包——tarball/src 源码件/prebuilt C/SHA256SUMS"
```

---

### Task 5: 双阶段 GitHub Actions release workflow

**Files:**
- Create: `.github/workflows/release.yml`

**Interfaces:**
- Consumes: Task 3/4 的 Makefile/release.sh;计划 1 的 ctc 与四件套。
- Produces: tag 推送 → 五平台产物 + src + SHA256SUMS → GitHub Release。

- [ ] **Step 1: workflow**

```yaml
name: release
on:
  push:
    tags: ['v*']

jobs:
  prebuilt:
    runs-on: macos-14            # 固定点基准平台
    steps:
      - uses: actions/checkout@v4
      - run: make -C compiler-c && sh compiler/build.sh
      - name: 发射三件预发射 C
        run: |
          mkdir -p prebuilt
          compiler/ctc.sh emit compiler/build/cc_run.ct   prebuilt/ctron-cc.c
          compiler/ctc.sh emit compiler/build/cc_check.ct prebuilt/ctron-chk.c
          compiler/ctc.sh emit compiler/build/cc_emit.ct  prebuilt/ctron-emit.c
      - uses: actions/upload-artifact@v4
        with: { name: prebuilt, path: prebuilt/ }

  natives:
    needs: prebuilt
    strategy:
      fail-fast: false
      matrix:
        include:
          - { os: macos-14,       platform: darwin-arm64 }
          - { os: macos-13,       platform: darwin-x86_64 }
          - { os: ubuntu-24.04,   platform: linux-x86_64 }
          - { os: ubuntu-24.04-arm, platform: linux-arm64 }
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - uses: actions/download-artifact@v4
        with: { name: prebuilt, path: prebuilt }
      - run: make -C compiler-c                      # linux 首跑风险点:seed 侧 glibc/cc 差异
      - run: sh compiler/native.sh                   # 自举链出四件套
      - name: 固定点承诺——本平台发射与基准逐字节 diff
        run: |
          for P in cc_run:ctron-cc cc_check:ctron-chk cc_emit:ctron-emit; do
            src=${P%%:*}; out=${P##*:}
            compiler/ctc.sh emit compiler/build/$src.ct /tmp/$out.c
            diff /tmp/$out.c prebuilt/$out.c
          done
      - name: 打包
        run: |
          VER=${GITHUB_REF_NAME#v}
          sh tools/release.sh "$VER"
      - name: 验收(解压即用/装机路径/无 cc/E5030/help)
        run: sh tests/dist/accept.sh dist/ctron-*.tar.gz
      - uses: actions/upload-artifact@v4
        with: { name: rel-${{ matrix.platform }}, path: dist/ }

  windows:
    needs: prebuilt
    runs-on: windows-latest
    defaults: { run: { shell: msys2 {0} } }
    steps:
      - uses: actions/checkout@v4
      - uses: msys2/setup-msys2@v2
        with: { msystem: MINGW64, update: false, install: 'mingw-w64-x86_64-gcc make git' }
      - uses: actions/download-artifact@v4
        with: { name: prebuilt, path: prebuilt }
      - name: cc-only 构建(绕过 seed)
        run: |
          mkdir -p bin
          gcc -O2 -w -pthread -Wl,--stack,16777216 -o bin/ctron-cc.exe   prebuilt/ctron-cc.c
          gcc -O2 -w -pthread -Wl,--stack,16777216 -o bin/ctron-chk.exe  prebuilt/ctron-chk.c
          gcc -O2 -w -pthread -Wl,--stack,16777216 -o bin/ctron-emit.exe prebuilt/ctron-emit.c
      - name: 组装 zip
        run: |
          VER=${GITHUB_REF_NAME#v}
          mkdir -p ctron/bin ctron/lib/ctron ctron/share/doc
          cp bin/*.exe ctron/bin/; cp ctc.ps1 ctc.cmd ctron/bin/
          cp -R std ctron/lib/ctron/std
          printf '%s %s\n' "$VER" "$(git rev-parse --short HEAD)" > ctron/VERSION
          powershell -Command "Compress-Archive -Path ctron -DestinationPath dist-windows.zip -Force"
      - name: 验收(并发/深递归/CRLF/help conformance)
        run: sh tests/dist/accept_windows.sh
      - uses: actions/upload-artifact@v4
        with: { name: rel-windows-x86_64, path: dist-windows.zip }

  publish:
    needs: [prebuilt, natives, windows]
    runs-on: ubuntu-24.04
    permissions: { contents: write }
    steps:
      - uses: actions/download-artifact@v4
        with: { pattern: rel-*, merge-multiple: true, path: rel }
      - run: |
          VER=${GITHUB_REF_NAME#v}
          cd rel
          mv dist-windows.zip ctron-$VER-windows-x86_64.zip
          sha256sum ctron-$VER* > SHA256SUMS
      - uses: softprops/action-gh-release@v2
        with:
          files: rel/*
```

(实现时按 matrix.platform 组装验收脚本入参;`accept.sh`/`accept_windows.sh` 见 Task 6。)

- [ ] **Step 2: 验收脚本 tests/dist/accept.sh(mac/linux 共用,入参 tarball)**

固化 spec §7 第 1-5 项:解压临时目录 → `ctc --version` → examples 三件 run+build 输出对拍 → 任意 cwd `use std.*` 命中(以 examples 为载体)→ 无 cc 用例(PATH 隔离,rc=2 且 .c 已产出)→ `ctron-chk` 负例 rc=1 → `ctc --help` rc=0。断言失败即非零退出。

- [ ] **Step 3: tests/dist/accept_windows.sh**

固化 spec §7 第 6-9 项:并发夹具(spawn/Channel,取 tests/ 并发件之一)经 ctc build + run;深递归夹具(栈参数生效);CRLF 夹具;`ctc.ps1 --help` 与 sh 版文本归一化 diff(conformance)。

- [ ] **Step 4: workflow 干跑校验**

Run: `python3 -c "import yaml,sys; yaml.safe_load(open('.github/workflows/release.yml'))" 2>/dev/null || ruby -ryaml -e 'YAML.load_file(".github/workflows/release.yml")'`
预期:YAML 可解析。(首跑真验证在打 tag 时——计划内不实际发版,发布动作由用户择机执行。)

- [ ] **Step 5: Commit**

```bash
git add .github/workflows/release.yml tests/dist/accept.sh tests/dist/accept_windows.sh
git commit -m "feat(dist): 双阶段 release workflow——五平台 matrix+固定点 diff+windows cc-only+验收门禁+publish"
```

---

### Task 6: spec/文档同步与收口

**Files:**
- Modify: `docs/superpowers/specs/2026-09-17-toolchain-distribution-design.md`
- Modify: `README.md`(仓库根;若有安装节则补,无则加最简安装小节)

- [ ] **Step 1: spec 修正三处**

① §5 第 8 项标记"已满足(双侧词法既有),仅 CRLF 回归夹具";② §4 `ctc test` 口径补注"含 main 文件的 test 执行挂账,宿主两阶段兜底";③ §2 源码线 Makefile 注明"bootstrap 走 git 仓库,不进 tarball"。

- [ ] **Step 2: README 安装节**

```markdown
## 安装与上手(v0.1)

​```bash
curl -fsSL <releases>/install.sh | sh      # 或手动下载 tarball 解压
export PATH="$HOME/.ctron/ctron/bin:$PATH"
​```

​```bash
ctc run hello.ct   # 解释执行(零依赖)
ctc build hello.ct # 出可执行(需本机 C 编译器)
​```
```

- [ ] **Step 3: 全量门禁收口**

Run: `sh ci.sh`
预期:全绿。

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/specs/2026-09-17-toolchain-distribution-design.md README.md
git commit -m "docs(dist): spec 同步(§5.8 已满足/test 口径/Makefile 注记)+ README 安装节"
```
