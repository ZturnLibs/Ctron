#!/bin/sh
# release.sh —— 单平台打包(release 工程每平台各跑一次)
#   用法: tools/release.sh <version>
#   产物: dist/ctron-<ver>-<os>-<arch>.tar.gz
#         dist/ctron-<ver>-src.tar.gz
#         dist/prebuilt/{ctron-cc,ctron-chk,ctron-emit,ctron-fmt}.c
#         dist/SHA256SUMS(VERSION 落两包根:dist/ctron/VERSION 与 src 件根)
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
VER=${1:?用法: release.sh <version>}; VER=${VER#v}
OS=$(uname -s | tr '[:upper:]' '[:lower:]'); M=$(uname -m)
case $M in arm64|aarch64) ARCH=arm64 ;; x86_64) ARCH=x86_64 ;; *) echo "不支持的架构 $M" >&2; exit 2 ;; esac
DIST="$DIR/dist"; PKG="$DIST/ctron"
rm -rf "$DIST" && mkdir -p "$PKG/bin" "$PKG/lib/ctron" "$PKG/share/doc" "$DIST/prebuilt"

# README 单点解析:git 仓库根现无 README.md(文档在 compiler/README.md),回落取之;
# 真 release 摆入根 README 后自动优先生效(两落点同源,装机包与 src 件不缺页)
README="$DIR/README.md"; [ -f "$README" ] || README="$DIR/compiler/README.md"

sh "$DIR/compiler/build.sh"
# CTRON_FROM_PREBUILT=1:跳过 seed 自举(Windows/linux 走此路——linux 上 seed 解释器
# 内存冲破 runner 上限,见台账编译器线挂账),直接 cc 编译预发射 C;产物与自举链逐字节同源
if [ "${CTRON_FROM_PREBUILT:-0}" = "1" ] && [ -f "$DIR/prebuilt/ctron-cc.c" ]; then
    mkdir -p "$DIR/compiler/bin"
    cc -O2 -w -pthread -o "$DIR/compiler/bin/ctron-cc"   "$DIR/prebuilt/ctron-cc.c"
    cc -O2 -w -pthread -o "$DIR/compiler/bin/ctron-chk"  "$DIR/prebuilt/ctron-chk.c"
    cc -O2 -w -pthread -o "$DIR/compiler/bin/ctron-emit" "$DIR/prebuilt/ctron-emit.c"
    cc -O2 -w -pthread -o "$DIR/compiler/bin/ctron-fmt"  "$DIR/prebuilt/ctron-fmt.c"
else
    sh "$DIR/compiler/native.sh"
fi
install -m 755 "$DIR/compiler/bin/ctron-cc" "$DIR/compiler/bin/ctron-chk" "$DIR/compiler/bin/ctron-emit" "$DIR/compiler/bin/ctron-fmt" "$PKG/bin/"
install -m 755 "$DIR/ctc" "$PKG/bin/ctc"
cp -R "$DIR/std/." "$PKG/lib/ctron/std/"
cp "$README" "$PKG/share/doc/"
cp -R "$DIR/examples" "$PKG/share/doc/examples"
printf '%s %s\n' "$VER" "$(git -C "$DIR" rev-parse --short HEAD)" > "$PKG/VERSION"

# 预发射 C(固定点:任何平台发射应逐字节一致,workflow 内跨平台 diff 验证)
# prebuilt 模式:artifact 携带的预发射 C 即权威产物,原样采用(linux 上 seed emit 会
# 冲破内存上限,见台账编译器线挂账);darwin 自举链模式:由 seed 链现发射
TMP=$(mktemp -d /tmp/ctron_rel.XXXXXX) && trap 'rm -rf "$TMP"' EXIT
if [ "${CTRON_FROM_PREBUILT:-0}" = "1" ]; then
    cp "$DIR/prebuilt/"ctron-*.c "$DIST/prebuilt/"
else
    "$DIR/compiler/ctc.sh" emit "$DIR/compiler/build/cc_run.ct"   "$DIST/prebuilt/ctron-cc.c"   >/dev/null
    "$DIR/compiler/ctc.sh" emit "$DIR/compiler/build/cc_check.ct" "$DIST/prebuilt/ctron-chk.c"  >/dev/null
    "$DIR/compiler/ctc.sh" emit "$DIR/compiler/build/cc_emit.ct"  "$DIST/prebuilt/ctron-emit.c" >/dev/null
    "$DIR/compiler/ctc.sh" emit "$DIR/compiler/build/cc_fmt.ct"   "$DIST/prebuilt/ctron-fmt.c"  >/dev/null
fi

# 源码 tarball 组装件(仅打包,不重复构建;布局对齐源码线 Makefile:prebuilt/ std/ ctc Makefile)
SRC="$DIST/ctron-src-$VER"; mkdir -p "$SRC/prebuilt"
cp -R "$DIR/compiler/src" "$SRC/compiler-src"
cp -R "$DIR/std" "$SRC/std"
cp "$DIST/prebuilt/"*.c "$SRC/prebuilt/"
cp "$DIR/ctc" "$SRC/"; cp "$DIR/Makefile" "$SRC/"; cp "$README" "$SRC/"
# P2-4 挂账补录:spec §2.3(L102)源码线布局含 ctc.ps1/ctc.cmd——Windows 在 MSYS2 里
# 走 cc-only make 时驱动同包可得,src 件命令面三驱动齐备
cp "$DIR/ctc.ps1" "$SRC/"; cp "$DIR/ctc.cmd" "$SRC/"
cp "$DIR/install.sh" "$SRC/" 2>/dev/null || true
printf '%s %s\n' "$VER" "$(git -C "$DIR" rev-parse --short HEAD)" > "$SRC/VERSION"

tar -C "$DIST" -czf "$DIST/ctron-$VER-$OS-$ARCH.tar.gz" ctron
tar -C "$DIST" -czf "$DIST/ctron-$VER-src.tar.gz" "ctron-src-$VER"
cd "$DIST" && shasum -a 256 ctron-$VER-*.tar.gz > SHA256SUMS
echo "release.sh: 产物在 $DIST/"
ls -la "$DIST"
