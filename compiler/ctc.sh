#!/bin/sh
# ctc.sh —— Ctron 自举编译器统一驱动(三种模式,共用同一套模块拼接)
#
# 用法:
#   ./ctc.sh <input.ct>              # 运行:parse → 语义 12 项 → 解释执行(正例 rc=0/负例诊断 rc=1)
#   ./ctc.sh check <input.ct>        # 检查:parse → 语义 12 项即止,打印 "check OK decls=N"
#   ./ctc.sh emit <input.ct> [out.c] # 发射:parse → 生成等价 C(产物 gcc 可编译,`<bin> run <file>` 可覆锚)
#
# 宿主 seed(compiler-c/build/ctronc)仅充当 Ctron 解释器;输入路径经
# read_file 锚换靶注入。自举完成后产物可自替换宿主(见 test/smoke.sh --full)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$DIR")
HOST="$ROOT/compiler-c/build/ctronc"

mode=run
PROF=full
TAUSTED=0
DIAGLANG=zh
case ${1:-} in
    check|emit) mode=$1; shift ;;
esac
for a in "$@"; do
    case $a in
        --profile=*) PROF=${a#--profile=} ;;
        --trusted) TAUSTED=1 ;;
        --lang=*) DIAGLANG=${a#--lang=} ;;
    esac
done
if [ ! -x "$HOST" ]; then
    echo "ctc.sh: 缺少宿主 seed $HOST(先: make -C \"$ROOT/compiler-c\")" >&2
    exit 2
fi
if [ $# -lt 1 ] || [ ! -f "$1" ]; then
    echo "用法: ctc.sh <input.ct> | ctc.sh check <input.ct> | ctc.sh emit <input.ct> [out.c]" >&2
    exit 2
fi

IN=$(CDPATH= cd -- "$(dirname -- "$1")" && pwd)/$(basename -- "$1")
shift

case $mode in
    run)
        "$DIR/build.sh" >/dev/null
        TMP=$(mktemp /tmp/ctron_cc.XXXXXX)
        sed -e "s|\.\./selfhosted/input_cc\.ct|$IN|" -e "s|ANCHORPROFILE|$PROF|" -e "s|ANCHORLANG|$DIAGLANG|" "$DIR/build/cc_run.ct" > "$TMP"
        "$HOST" run "$TMP"
        rc=$?
        ;;
    check)
        FMT=0
        PROF=full
        for a in "$@"; do
            case $a in --format=json) FMT=1 ;; esac
        done
        for a in "$@"; do
            case $a in --profile=*) PROF=${a#--profile=} ;; esac
        done
        "$DIR/build.sh" >/dev/null
        TMP=$(mktemp /tmp/ctron_cc.XXXXXX)
        sed -e "s|\.\./selfhosted/input_cc\.ct|$IN|" -e "s|ANCHORFMT|$FMT|" -e "s|ANCHORPROFILE|$PROF|" -e "s|ANCHORTAUSTED|$TAUSTED|" -e "s|ANCHORLANG|$DIAGLANG|" "$DIR/build/cc_check.ct" > "$TMP"
        "$HOST" run "$TMP"
        rc=$?
        ;;
    emit)
        OUTC=${1:-$(basename "${IN%.ct}").c}
        "$DIR/build.sh" >/dev/null
        TMP=$(mktemp /tmp/ctron_cc.XXXXXX)
        sed "s|ANCHORINPUT|$IN|" "$DIR/build/cc_emit.ct" | sed "s|ANCHORLANG|$DIAGLANG|" > "$TMP"
        "$HOST" run "$TMP" > "$OUTC"
        rc=$?
        [ $rc -eq 0 ] && echo "ctc.sh: 已发射 $OUTC(编译: cc -O2 $OUTC -o bin;运行: ./bin run $IN)"
        ;;
esac
rm -f ${TMP:-/dev/null}
exit $rc
