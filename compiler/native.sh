#!/bin/sh
# native.sh —— 编译出原生 Ctron 编译器二进制(bin/)
#
#   bin/ctron-cc    编译器·运行驱动(parse → 语义 12 项 → 解释执行,<bin> run <file>)
#   bin/ctron-emit  编译器·发射驱动(parse → 生成等价 C,<bin> run <file> > out.c)
#
# 生成路径(自举链):发射器(cc_emit)编译编译器源 → C → 本机 cc。
# 宿主 seed(compiler_c/build/ctronc)只在 ctc.sh emit 内部出现 —— 首次引导职责。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BIN="$DIR/bin"
TMP=$(mktemp -d /tmp/ctron_native.XXXXXX)
trap 'rm -rf "$TMP"' EXIT

[ -x "$DIR/build/cc_run.ct" ] || "$DIR/build.sh" > /dev/null
mkdir -p "$BIN"

"$DIR/ctc.sh" emit "$DIR/build/cc_run.ct"  "$TMP/ctron_cc.c"   > /dev/null
cc -O2 -w -o "$BIN/ctron-cc" "$TMP/ctron_cc.c"
echo "native: bin/ctron-cc(运行驱动)← $(wc -l < "$TMP/ctron_cc.c") 行 C"

"$DIR/ctc.sh" emit "$DIR/build/cc_emit.ct" "$TMP/ctron_emit.c" > /dev/null
cc -O2 -w -o "$BIN/ctron-emit" "$TMP/ctron_emit.c"
echo "native: bin/ctron-emit(发射驱动)← $(wc -l < "$TMP/ctron_emit.c") 行 C"
