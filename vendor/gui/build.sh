#!/bin/sh
# vendor/gui/build.sh —— GUI 依赖静态库构建(raylib 精简子集;clay 为单头,无需编译)
# 产物:build/libraylib.a;版本/来源/许可登记见 VENDORED.md(规范 §12.4)
# 子集:rcore/rglfw/rshapes/rtext/rtextures/utils(砍 models/audio——GUI 不需要)
set -eu
cd "$(dirname "$0")"
mkdir -p build

RAY_SUBSET="rcore.c rglfw.c rshapes.c rtext.c rtextures.c utils.c"
OBJS=""
for f in $RAY_SUBSET; do
    o="build/${f%.c}.o"
    if [ ! -f "$o" ] || [ "raylib/$f" -nt "$o" ]; then
        XC=""
        # rglfw.c 捆绑 GLFW 的 Cocoa 平台源(.m,ObjC)——macOS 须按 objective-c 编译
        if [ "$f" = "rglfw.c" ] && [ "$(uname)" = "Darwin" ]; then
            XC="-x objective-c"
        fi
        cc -O1 -w -fPIC $XC -DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 \
           -Iraylib/external/glfw/include \
           -c "raylib/$f" -o "$o"
    fi
    OBJS="$OBJS $o"
done
ar rcs build/libraylib.a $OBJS
echo "vendor/gui/build/libraylib.a OK"
