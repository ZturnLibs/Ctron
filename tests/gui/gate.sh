#!/bin/sh
# tests/gui/gate.sh —— GUI 阶梯包装(自 ci.sh 移植;headless 断言无显示依赖;
# Linux 缺 X11/GL 开发头时显式 skip 并指路,非静默假绿)。
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
RUN_GUI=1
if [ "$(uname)" = "Linux" ]; then
    if [ ! -f /usr/include/X11/Xlib.h ] || [ ! -f /usr/include/GL/gl.h ]; then
        RUN_GUI=0
    fi
fi
if [ "$RUN_GUI" = "1" ]; then
    sh "$DIR/run.sh"
else
    echo "[skip] GUI 阶梯:Linux 缺 X11/GL 开发头——apt install libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev libgl-dev 后重跑即接入(W5 环境前置)"
fi
