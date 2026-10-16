#!/bin/sh
# native.sh —— 编译出原生 Ctron 编译器二进制(bin/)
#
#   bin/ctron-cc    编译器·运行驱动(parse → 语义 12 项 → 解释执行,<bin> run <file>)
#   bin/ctron-emit  编译器·发射驱动(parse → 生成等价 C,<bin> run <file> > out.c)
#   bin/ctron-chk   编译器·检查驱动(parse → 语义 12 项,<bin> run <file> [--format=json|--profile=bare|--trusted])
#   bin/ctron-fmt   格式化驱动(R-P2d 移植:scan5 → token 流重排 → stdout,<bin> run <file>;
#                   词法诊断 rc=1;-w/--check/pkg 目录由 ctron 层编排,docs/fmt-spec.md)
#   bin/ctron-doc   iface 投影驱动(闭源包分发 S0:pub 符号表 + trait/impl 面,
#                   --format=json JSON 面;ctc.sh doc 同面)
#   bin/ctron-dep   依赖解析驱动(T49:lock 内容寻址生成+digest 校验;ctc.sh dep 同面)
#   bin/ctron-verify 工件深验驱动(T50/L3 载体:SHA256SUMS 逐成员+self_digest
#                   in-language 重算;ctron pkg verify --deep 委派面)
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

# CC2(2026-10-16):str_byte/utf8_enc 解释臂宿主口(eval_call extern 直调桥符号源)
cc -O2 -w -c "$DIR/host_shim/cc2_str.c" -o "$TMP/cc2_str.o"
SHIM_O="$TMP/cc2_str.o"

"$DIR/ctc.sh" emit "$DIR/build/cc_run.ct"  "$TMP/ctron_cc.c"   > /dev/null
"$DIR/ctc.sh" emit "$DIR/build/cc_emit.ct" "$TMP/ctron_emit.c" > /dev/null
"$DIR/ctc.sh" emit "$DIR/build/cc_check.ct" "$TMP/ctron_chk.c" > /dev/null
"$DIR/ctc.sh" emit "$DIR/build/cc_fmt.ct"  "$TMP/ctron_fmt.c"  > /dev/null
"$DIR/ctc.sh" emit "$DIR/build/cc_doc.ct"  "$TMP/ctron_doc.c"  > /dev/null
"$DIR/ctc.sh" emit "$DIR/build/cc_dep.ct"  "$TMP/ctron_dep.c"  > /dev/null
"$DIR/ctc.sh" emit "$DIR/build/cc_verify.ct" "$TMP/ctron_verify.c" > /dev/null
"$DIR/ctc.sh" emit "$DIR/build/cc_ast.ct" "$TMP/ctron_ast.c" > /dev/null

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-ldl -lX11 -lGL -lm -lpthread -ldl" ;;
    *) FW="" ;;
esac

# W4 E1:ctron-cc 的 extern 符号源(域库 shim + raylib;缺席则纯解释器降级)
GUI_O=""
GUI_F=""
if [ -f "$ROOT/vendor/gui/build/libraylib.a" ]; then
    # M3 文件管线合流(6696433)起 ctron_gui.c 引 ft_shim 三符号——与域包 run.sh
    # 惯例同形:ft_shim 源 + freetype 头 + libfreetype/libsheenbidi 静态链
    cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" \
       -I"$ROOT/vendor/gui/freetype/include" \
       -c "$ROOT/pkgs/gui/c_src/ctron_gui.c" -o "$TMP/ctron_gui.o"
    cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" \
       -I"$ROOT/vendor/gui/freetype/include" \
       -c "$ROOT/pkgs/gui/c_src/ft_shim.c" -o "$TMP/ft_shim.o"
    GUI_O="$TMP/ctron_gui.o $TMP/ft_shim.o"
    case "$(uname)" in
        Darwin)
            GUI_F="-Wl,-force_load,$ROOT/vendor/gui/build/libraylib.a $FW $ROOT/vendor/gui/build/libfreetype.a $ROOT/vendor/gui/build/libsheenbidi.a"
            ;;
        Linux)
            GUI_F="-Wl,--whole-archive $ROOT/vendor/gui/build/libraylib.a -Wl,--no-whole-archive $FW $ROOT/vendor/gui/build/libfreetype.a $ROOT/vendor/gui/build/libsheenbidi.a"
            ;;
    esac
    echo "native: ctron-cc 附带解释口径 extern 符号源(域库 shim + raylib)"
fi

cc -O2 -w -o "$BIN/ctron-cc" "$TMP/ctron_cc.c" $SHIM_O $GUI_O $GUI_F
echo "native: bin/ctron-cc(运行驱动)← $(wc -l < "$TMP/ctron_cc.c") 行 C"

cc -O2 -w -o "$BIN/ctron-emit" "$TMP/ctron_emit.c" $SHIM_O
echo "native: bin/ctron-emit(发射驱动)← $(wc -l < "$TMP/ctron_emit.c") 行 C"

cc -O2 -w -o "$BIN/ctron-chk" "$TMP/ctron_chk.c" $SHIM_O
echo "native: bin/ctron-chk(检查驱动)← $(wc -l < "$TMP/ctron_chk.c") 行 C"

cc -O2 -w -o "$BIN/ctron-fmt" "$TMP/ctron_fmt.c" $SHIM_O
echo "native: bin/ctron-fmt(格式化驱动)← $(wc -l < "$TMP/ctron_fmt.c") 行 C"

cc -O2 -w -o "$BIN/ctron-doc" "$TMP/ctron_doc.c" $SHIM_O
echo "native: bin/ctron-doc(iface 投影驱动)← $(wc -l < "$TMP/ctron_doc.c") 行 C"

cc -O2 -w -o "$BIN/ctron-dep" "$TMP/ctron_dep.c" $SHIM_O
echo "native: bin/ctron-dep(依赖解析驱动,T49)← $(wc -l < "$TMP/ctron_dep.c") 行 C"

cc -O2 -w -o "$BIN/ctron-verify" "$TMP/ctron_verify.c" $SHIM_O
echo "native: bin/ctron-verify(工件深验驱动,T50/L3 载体)← $(wc -l < "$TMP/ctron_verify.c") 行 C"

cc -O2 -w -o "$BIN/ctron-ast" "$TMP/ctron_ast.c" $SHIM_O
echo "native: bin/ctron-ast(封印/序列化驱动,T50/S4-④)← $(wc -l < "$TMP/ctron_ast.c") 行 C"

# W4 波1:用户驱动器 bin/ctron(Ctron 自写,compiler/driver/src/main.ct 单文件;
# 委派族+fmt/lint 波2/3 补臂;对拍门 tests/drv_parity)。
# 两步走而非 ctc.sh build:发射模板的 C main 包装层劫持 argv[1]=="--version"
# (puts 模板版串,≠sh "ctron <VERSION 内容>/dev" 语义)且 argv[1]=="run" 时
# 重偏 args(off=3)——emit 后剥除两臂再链接,驱动器自带的 --version/--help 面
# (自读 VERSION;args 稳在 argv[1:])才可达;模板变形即 grep 守卫响(fail-loud)。
# driver 树缺席 = SKIP(幂等:重跑整段重建)。
if [ -f "$DIR/driver/src/main.ct" ]; then
    echo "== build driver: bin/ctron =="
    DRV_C="$TMP/ctron_drv.c"
    "$DIR/ctc.sh" emit "$DIR/driver/src/main.ct" "$DRV_C" > /dev/null
    DRV_A='if (argc >= 2 && !strcmp(argv[1], "--version")) { puts(ctron_version); return 0; }'
    DRV_B='if (argc >= 3 && strcmp(argv[1], "run") == 0) { ctron_cli_input = argv[2]; ctron_argv_off = 3; }'
    if ! grep -qF "$DRV_A" "$DRV_C" || ! grep -qF "$DRV_B" "$DRV_C"; then
        echo "native: 驱动器发射 C 缺模板包装臂(发射模板已变?守卫失败)" >&2
        exit 1
    fi
    DRV_SED_A='if (argc >= 2 && !strcmp(argv\[1\], "--version")) { puts(ctron_version); return 0; }'
    DRV_SED_B='if (argc >= 3 && strcmp(argv\[1\], "run") == 0) { ctron_cli_input = argv\[2\]; ctron_argv_off = 3; }'
    sed -e "s|$DRV_SED_A||" -e "s|$DRV_SED_B||" "$DRV_C" > "$DRV_C.s"
    mv "$DRV_C.s" "$DRV_C"
    # W4 波1 批2:委派族 use proc → 垫片同链(纯 emit 单文件缺 ctron_proc_run_argv_rc 符号)
    cc -O2 -w -o "$BIN/ctron" "$DRV_C" "$ROOT/lib/proc/c_src/ctron_proc.c"
    echo "native: bin/ctron(用户驱动器,W4 波1)← $(wc -l < "$DIR/driver/src/main.ct") 行 Ctron"
fi
