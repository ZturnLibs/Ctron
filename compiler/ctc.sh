#!/bin/sh
# ctc.sh —— Ctron 自举编译器统一驱动(四种模式,共用同一套模块拼接)
#
# 用法:
#   ./ctc.sh <input.ct>              # 运行:parse → 语义 12 项 → 解释执行(正例 rc=0/负例诊断 rc=1)
#   ./ctc.sh check <input.ct>        # 检查:parse → 语义 12 项即止,打印 "check OK decls=N"
#   ./ctc.sh emit <input.ct> [out.c] # 发射:parse → 生成等价 C(产物 gcc 可编译,`<bin> run <file>` 可覆锚)
#   ./ctc.sh fmt <input.ct>          # 格式化:R-P2d token 流重排 → stdout(docs/fmt-spec.md;诊断 rc=1)
#   ./ctc.sh doc <input.ct>          # iface 投影(闭源包分发 S0):pub 符号表 + trait/impl 面
#                                    #   --format=json 走 JSON 面(schema v0,见 driver_doc.ct 头注)
#   ./ctc.sh ast <input.ct> [--ast=dump]
#                                    # .ctast 序列化(S1a):默认 roundtrip 固定点自验;--ast=dump 出记录流
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
    check|emit|fmt|doc|ast) mode=$1; shift ;;
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
    echo "用法: ctc.sh <input.ct> | ctc.sh check <input.ct> | ctc.sh emit <input.ct> [out.c] | ctc.sh fmt <input.ct> | ctc.sh doc <input.ct> [--format=json] | ctc.sh ast <input.ct> [--ast=dump]" >&2
    exit 2
fi

IN=$(CDPATH= cd -- "$(dirname -- "$1")" && pwd)/$(basename -- "$1")
shift

case $mode in
    run)
        "$DIR/build.sh" >/dev/null
        TMP=$(mktemp /tmp/ctron_cc.XXXXXX)
        sed -e "s|ANCHORINPUT|$IN|" -e "s|ANCHORPROFILE|$PROF|" -e "s|ANCHORLANG|$DIAGLANG|" "$DIR/build/cc_run.ct" > "$TMP"
        "$HOST" run "$TMP"
        rc=$?
        ;;
    check)
        FMT=0
        DUMPGUI=0
        PROF=full
        for a in "$@"; do
            case $a in --format=json) FMT=1 ;; --dump-gui) DUMPGUI=1 ;; esac
        done
        for a in "$@"; do
            case $a in --profile=*) PROF=${a#--profile=} ;; esac
        done
        "$DIR/build.sh" >/dev/null
        TMP=$(mktemp /tmp/ctron_cc.XXXXXX)
        sed -e "s|ANCHORINPUT|$IN|" -e "s|ANCHORFMT|$FMT|" -e "s|ANCHORPROFILE|$PROF|" -e "s|ANCHORTAUSTED|$TAUSTED|" -e "s|ANCHORDUMPGUI|$DUMPGUI|" -e "s|ANCHORLANG|$DIAGLANG|" "$DIR/build/cc_check.ct" > "$TMP"
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
        if [ $rc -eq 0 ]; then
            echo "ctc.sh: 已发射 $OUTC(编译: cc -O2 $OUTC -o bin;运行: ./bin run $IN)"
            LF=$(grep -o "ctron:link -l[^ ]*" "$OUTC" 2>/dev/null | awk '{print $2}' | tr '\n' ' ')
            if [ -n "$LF" ]; then
                echo "ctc.sh: 链接标志(#[link] 收集): $LF"
                # pkg-config 解析(v0.9):对每个 -l<名>,若 pkg-config 在册且
                # 认识该名(--exists 成功),展开其 --libs 输出;否则保留 -l<名>。
                # 发射 C 文本不变,解析只发生在驱动壳层(构建系统可取本行)。
                if command -v pkg-config >/dev/null 2>&1; then
                    PR=""
                    for ln in $LF; do
                        pn=${ln#-l}
                        if pkg-config --exists "$pn" 2>/dev/null; then
                            PR="$PR $(pkg-config --libs "$pn" 2>/dev/null)"
                        else
                            PR="$PR $ln"
                        fi
                    done
                    echo "ctc.sh: 链接标志(pkg-config 解析):$PR"
                fi
            fi
        fi
        ;;
    fmt)
        "$DIR/build.sh" >/dev/null
        TMP=$(mktemp /tmp/ctron_fmt.XXXXXX)
        sed -e "s|ANCHORINPUT|$IN|" -e "s|ANCHORLANG|$DIAGLANG|" "$DIR/build/cc_fmt.ct" > "$TMP"
        "$HOST" run "$TMP"
        rc=$?
        ;;
    ast)
        ASTMODE=roundtrip
        ASTOUT="ANCHOROUT"
        ASTNAME="ANCHORNAME"
        for a in "$@"; do
            case $a in
                --ast=dump) ASTMODE=dump ;;
                --ast=seal) ASTMODE=seal ;;
                --astout=*) ASTOUT=${a#--astout=} ;;
                --astname=*) ASTNAME=${a#--astname=} ;;
            esac
        done
        "$DIR/build.sh" >/dev/null
        TMP=$(mktemp /tmp/ctron_ast.XXXXXX)
        sed -e "s|ANCHORINPUT|$IN|" -e "s|ANCHORAST|$ASTMODE|" -e "s|ANCHOROUT|$ASTOUT|" -e "s|ANCHORNAME|$ASTNAME|" -e "s|ANCHORLANG|$DIAGLANG|" "$DIR/build/cc_ast.ct" > "$TMP"
        "$HOST" run "$TMP"
        rc=$?
        ;;
    doc)
        FMT=0
        for a in "$@"; do
            case $a in --format=json) FMT=1 ;; esac
        done
        "$DIR/build.sh" >/dev/null
        TMP=$(mktemp /tmp/ctron_doc.XXXXXX)
        sed -e "s|ANCHORINPUT|$IN|" -e "s|ANCHORFMT|$FMT|" -e "s|ANCHORLANG|$DIAGLANG|" "$DIR/build/cc_doc.ct" > "$TMP"
        "$HOST" run "$TMP"
        rc=$?
        ;;
esac
rm -f ${TMP:-/dev/null}
exit $rc
