#!/bin/sh
# native.sh —— 编译出原生 Ctron 编译器二进制(bin/)
#
#   bin/ctron-cc    编译器·运行驱动(parse → 语义 12 项 → 解释执行,<bin> run <file>)
#   bin/ctron-emit  编译器·发射驱动(parse → 生成等价 C,<bin> run <file> > out.c)
#   bin/ctron-chk   编译器·检查驱动(parse → 语义 12 项,<bin> run <file> [--format=json|--profile=bare|--trusted])
#   bin/ctron-fmt   格式化驱动(R-P2d 移植:scan5 → token 流重排 → stdout,<bin> run <file>;
#                   词法诊断 rc=1;-w/--check/pkg 目录由 ctc 层编排,docs/fmt-spec.md)
#
# 生成路径(自举链):发射器(cc_emit)编译编译器源 → C → 本机 cc。
# 宿主 seed(compiler-c/build/ctronc)只在 ctc.sh emit 内部出现 —— 首次引导职责。
#
# W4 E1(2026-09-19):bin/ctron-cc 额外链接域库 shim + vendored raylib
# (whole-archive + rdynamic)——解释口径 extern 直调桥的 dlsym 符号源(§11.5);
# vendored 库缺席时降级为纯解释器(extern 调用运行期报"符号未找到")。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$DIR")
BIN="$DIR/bin"
TMP=$(mktemp -d /tmp/ctron_native.XXXXXX)
trap 'rm -rf "$TMP"' EXIT

[ -x "$DIR/build/cc_run.ct" ] || "$DIR/build.sh" > /dev/null
mkdir -p "$BIN"

"$DIR/ctc.sh" emit "$DIR/build/cc_run.ct"  "$TMP/ctron_cc.c"   > /dev/null
"$DIR/ctc.sh" emit "$DIR/build/cc_emit.ct" "$TMP/ctron_emit.c" > /dev/null
"$DIR/ctc.sh" emit "$DIR/build/cc_check.ct" "$TMP/ctron_chk.c" > /dev/null
"$DIR/ctc.sh" emit "$DIR/build/cc_fmt.ct"  "$TMP/ctron_fmt.c"  > /dev/null

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-ldl -lX11 -lGL -lm -lpthread -ldl" ;;
    *) FW="" ;;
esac

# W4 E1:ctron-cc 的 extern 符号源(域库 shim + raylib;缺席则纯解释器降级)
GUI_O=""
GUI_F=""
if [ -f "$ROOT/vendor/gui/build/libraylib.a" ]; then
    cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" \
       -c "$ROOT/std/gui/c_src/ctron_gui.c" -o "$TMP/ctron_gui.o"
    GUI_O="$TMP/ctron_gui.o"
    case "$(uname)" in
        Darwin)
            GUI_F="-Wl,-force_load,$ROOT/vendor/gui/build/libraylib.a $FW"
            ;;
        Linux)
            GUI_F="-Wl,--whole-archive $ROOT/vendor/gui/build/libraylib.a -Wl,--no-whole-archive $FW"
            ;;
    esac
    echo "native: ctron-cc 附带解释口径 extern 符号源(域库 shim + raylib)"
fi

cc -O2 -w -o "$BIN/ctron-cc" "$TMP/ctron_cc.c" $GUI_O $GUI_F
echo "native: bin/ctron-cc(运行驱动)← $(wc -l < "$TMP/ctron_cc.c") 行 C"

cc -O2 -w -o "$BIN/ctron-emit" "$TMP/ctron_emit.c"
echo "native: bin/ctron-emit(发射驱动)← $(wc -l < "$TMP/ctron_emit.c") 行 C"

cc -O2 -w -o "$BIN/ctron-chk" "$TMP/ctron_chk.c"
echo "native: bin/ctron-chk(检查驱动)← $(wc -l < "$TMP/ctron_chk.c") 行 C"

cc -O2 -w -o "$BIN/ctron-fmt" "$TMP/ctron_fmt.c"
echo "native: bin/ctron-fmt(格式化驱动)← $(wc -l < "$TMP/ctron_fmt.c") 行 C"
