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
[ -n "$VER" ] || { echo "install.sh: 取版本失败(可设 CTRON_VERSION=x.y.z 重试)" >&2; exit 2; }
TARBALL="ctron-${VER#v}-${os}-${arch}.tar.gz"
echo "install.sh: 下载 $BASE/$TARBALL"
TMP=$(mktemp -d /tmp/ctron_install.XXXXXX)
trap 'rm -rf "$TMP"' EXIT
curl -fsSL "$BASE/$TARBALL" -o "$TMP/$TARBALL"
# SHA256SUMS 与 tarball 同源取自 $BASE:默认 BASE(releases/latest/download)即 GitHub
# "最新 release 资产" permalink,校验单作为 release 资产同路可达;自定义 BASE(镜像/
# 离线 file:// 目录)按平铺文件解析同样正确。取不到(404/无网络)则静默跳过校验,
# 不阻塞安装。
curl -fsSL "$BASE/SHA256SUMS" -o "$TMP/SHA256SUMS" 2>/dev/null || true
if [ -s "$TMP/SHA256SUMS" ]; then
    if command -v sha256sum >/dev/null 2>&1; then
        (cd "$TMP" && grep "$TARBALL" SHA256SUMS | sha256sum -c -)
    elif command -v shasum >/dev/null 2>&1; then
        (cd "$TMP" && grep "$TARBALL" SHA256SUMS | shasum -a 256 -c -)
    fi
fi
mkdir -p "$DIR"
tar xzf "$TMP/$TARBALL" -C "$TMP"
cp -R "$TMP"/ctron/. "$DIR/ctron/" 2>/dev/null || { mkdir -p "$DIR/ctron" && cp -R "$TMP"/ctron/. "$DIR/ctron/"; }
echo "install.sh: 已装到 $DIR/ctron"
"$DIR/ctron/bin/ctc" --version
echo "export PATH=\"$DIR/ctron/bin:\$PATH\"   # 加入你的 shell 配置"
# ${CC:-}:set -u 下 CC 未导出不可裸引(原稿 "$CC" 会误杀收尾提示步骤)
command -v "${CC:-}" >/dev/null 2>&1 || command -v cc >/dev/null 2>&1 || {
    echo "提示: 未检测到 C 编译器 —— ctc run/check 不需要;ctc build 需要" >&2
    case $os in
        darwin) echo "  macOS: xcode-select --install" >&2 ;;
        linux)  echo "  linux: 发行版包管理器装 gcc(如 apt install gcc)" >&2 ;;
    esac
}
