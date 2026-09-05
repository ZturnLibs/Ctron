#!/bin/sh
# Ctron LSP 启动器:定位 ctronc 并运行 Ctron 实现的 LSP 服务器。
# 查找顺序:$CTRONC → 本扩展旁的仓库布局 → 常见安装位 → PATH 中的 ctronc。
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd) || exit 1

CTRONC="${CTRONC:-}"

# 1) 仓库开发布局(扩展目录仍在仓库内时)
if [ -z "$CTRONC" ]; then
    ROOT=$(cd "$SCRIPT_DIR/../../.." 2>/dev/null && pwd) || ROOT=""
    if [ -n "$ROOT" ] && [ -x "$ROOT/compiler_c/build/ctronc" ]; then
        CTRONC="$ROOT/compiler_c/build/ctronc"
    fi
fi

# 2) 常见安装位置(GUI 启动的 VSCode PATH 可能不全)
if [ -z "$CTRONC" ]; then
    for CAND in /usr/local/bin/ctronc /opt/homebrew/bin/ctronc "$HOME/.local/bin/ctronc" "$HOME/bin/ctronc"; do
        if [ -x "$CAND" ]; then CTRONC="$CAND"; break; fi
    done
fi

# 3) PATH
if [ -z "$CTRONC" ]; then
    CTRONC=$(command -v ctronc 2>/dev/null) || true
fi

if [ -z "$CTRONC" ] || [ ! -x "$CTRONC" ]; then
    echo "ctron-lsp: 找不到 ctronc —— 任选其一:" >&2
    echo "  a) cd <仓库>/compiler_c && make,然后 ln -sf <仓库>/compiler_c/build/ctronc /usr/local/bin/ctronc" >&2
    echo "  b) VSCode 设置 ctron.server.command 指向启动脚本/包装器" >&2
    echo "  c) 启动 code 前导出 CTRONC=/path/to/ctronc" >&2
    sleep 3
    exit 1
fi

# LSP 源码:优先用扩展内置副本(打包版),回退仓库版(开发版)
LSP_SRC="$SCRIPT_DIR/main.ct"
if [ ! -f "$LSP_SRC" ]; then
    LSP_SRC="$ROOT/lsp/src/main.ct"
fi

exec "$CTRONC" run "$LSP_SRC"
