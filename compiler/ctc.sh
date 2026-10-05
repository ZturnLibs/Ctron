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
    check|emit|fmt|doc|ast|build|targets|new|dep) mode=$1; shift ;;
esac
for a in "$@"; do
    case $a in
        --profile=*) PROF=${a#--profile=} ;;
        --trusted) TAUSTED=1 ;;
        --lang=*) DIAGLANG=${a#--lang=} ;;
    --deterministic) export CTRON_RT_SEED=1 ;;
    --target=*) TARGET=${a#--target=} ;;
    --target) TARGET_NEXT=1 ;;
    esac
done
# T36 后端注册表(§9.7 插件化地板):triple/链接器最小面;C native=首个后端,
# wasm32/bare 随 T37/T40 注册——未列名=清晰诊断(fail-closed)。
TARGET=${TARGET:-native}
case $TARGET in
    native) TGT_CC="cc -O2" ;;
    *) echo "ctc.sh: 未注册后端 target: $TARGET(注册表: native;wasm32/bare 随 T37/T40)" >&2; exit 2 ;;
esac
if [ ! -x "$HOST" ]; then
    echo "ctc.sh: 缺少宿主 seed $HOST(先: make -C \"$ROOT/compiler-c\")" >&2
    exit 2
fi
if [ "$mode" != "targets" ] && [ "$mode" != "new" ] && { [ $# -lt 1 ] || [ ! -f "$1" ]; }; then
    echo "用法: ctc.sh <input.ct> | ctc.sh check <input.ct> | ctc.sh emit <input.ct> [out.c] | ctc.sh fmt <input.ct> | ctc.sh doc <input.ct> [--format=json] | ctc.sh ast <input.ct> [--ast=dump] | ctc.sh build <input.ct> [--target native] [-o bin] | ctc.sh new <dir> [--gui] | ctc.sh dep <Ctron.ctcl> | ctc.sh targets" >&2
    exit 2
fi

# J19-⑤:脚手架 ctc new <dir> [--gui]——ctcl(pkg+gui.entry+dependencies.gui)
# + src/main.ct(headless 守卫+真窗锚双路模板)+ app.ctml。--gui 缺省 = 裸 .ct 脚手架。
if [ "$mode" = "new" ]; then
    NEWDIR=""
    NEWGUI=0
    for a4 in "$@"; do
        case $a4 in
            --gui) NEWGUI=1 ;;
            *) if [ -z "$NEWDIR" ]; then NEWDIR="$a4"; fi ;;
        esac
    done
    if [ -z "$NEWDIR" ]; then
        echo "用法: ctc.sh new <dir> [--gui]" >&2
        exit 2
    fi
    APP=$(basename "$NEWDIR")
    mkdir -p "$NEWDIR/src"
    if [ "$NEWGUI" = "1" ]; then
        cat > "$NEWDIR/Ctron.ctcl" <<EOF
pkg {
    manifest_version = 1
    name = "$APP"
    version = "0.1.0"
}

gui {
    entry = "app.ctml"
}

dependencies {
    gui
}
EOF
        cat > "$NEWDIR/app.ctml" <<'EOF'
// 入口 ctml(清单 gui.entry 指向;开发环默认源;内嵌兜底块见 src/main.ct)
view App {
  <vbox class="root">
    <label class="title">Hello Ctron</label>
    <button class="btn" on:click={tap}>点我</button>
    <label class="st">{hits}</label>
  </vbox>
}
style root { direction: column gap: 12 padding: 24 }
style title { size: 20 h: 30 }
style btn { w: 120 h: 36 }
style st { fg: "#888888" size: 14 h: 22 }
EOF
        cat > "$NEWDIR/src/main.ct" <<EOF
// $APP —— ctc new --gui 脚手架(J19-⑤)
// 构建:ctc.sh build src/main.ct -o app(dependencies.gui 自动链接 GUI 域库)
// 真窗:./app run src/main.ct;headless 冒烟:CTRON_GUI_HEADLESS=1 ./app run src/main.ct
use gui.{rt_run_kb_anchor, test, d_frame, d_expect_text}

// 内嵌兜底块(app.ctml 缺席时真窗/测试可用;SL-7α 独立二进制零 CWD 依赖)
view App {
  <vbox class="root">
    <label class="title">Hello Ctron</label>
  </vbox>
}
style root { direction: column gap: 12 padding: 24 }
style title { size: 20 h: 30 }

struct Model {
    var hits: I32
}

fn bind_all(buf: List[Str], m: Box[Model]) {
    if buf[0] == "hits" {
        buf.push(m.hits.to_string())
    }
}

fn act(name: Str, m: Box[Model]) {
    if name == "tap" {
        m.hits = m.hits + 1
    }
}

fn main() -> I32 {
    var m = Box[Model](Model { hits: 0 })
    if env_get("CTRON_GUI_HEADLESS") != "" {
        var src: Str = read_file("app.ctml")
        if src == "" {
            src = ctron_embedded()
        }
        return test(src, 360, 240,
            |buf| bind_all(buf, m),
            |nm| act(nm, m),
            |k| { },
            |t, actc, keyc| {
                d_frame(t, |buf| bind_all(buf, m))
                d_expect_text(t, "Hello Ctron")
                println("scaffold: headless OK")
            })
    }
    return rt_run_kb_anchor("Hello Ctron", 360, 240,
        |buf| bind_all(buf, m),
        |nm| act(nm, m),
        |k| { })
}
EOF
        echo "ctc.sh: 已脚手架 $NEWDIR(pkg+gui.entry+dependencies.gui+src/main.ct+app.ctml)"
        echo "  下一步: cd $NEWDIR && (ctc.sh build src/main.ct -o app) && CTRON_GUI_HEADLESS=1 ./app run src/main.ct"
    else
        cat > "$NEWDIR/Ctron.ctcl" <<EOF
pkg {
    manifest_version = 1
    name = "$APP"
    version = "0.1.0"
}
EOF
        cat > "$NEWDIR/src/main.ct" <<EOF
// $APP —— ctc new 脚手架
fn main() -> I32 {
    println("hello from $APP")
    return 0
}
EOF
        echo "ctc.sh: 已脚手架 $NEWDIR(pkg+src/main.ct)"
        echo "  下一步: cd $NEWDIR && ctc.sh run src/main.ct"
    fi
    exit 0
fi

if [ "$mode" = "targets" ]; then
    echo "ctc.sh: 注册后端 target:native(cc -O2,宿主三件套;wasm32/bare 随 T37/T40 注册)"
    exit 0
fi

IN=$(CDPATH= cd -- "$(dirname -- "$1")" && pwd)/$(basename -- "$1")
shift

# 链接标志摘要(#[link] 收集 → pkg-config 解析;v0.9):缓存命中与全量发射
# 两路同印——构建系统以本输出为契约,T20 缓存命中跳过发射但摘要仍须在。
emit_link_summary() {
    LF=$(grep -o "ctron:link -l[^ ]*" "$1" 2>/dev/null | awk '{print $2}' | tr '\n' ' ')
    if [ -n "$LF" ]; then
        echo "ctc.sh: 链接标志(#[link] 收集): $LF"
        # pkg-config 解析:对每个 -l<名>,若 pkg-config 在册且认识该名
        # (--exists 成功),展开其 --libs 输出;否则保留 -l<名>。
        # 发射 C 文本不变,解析只发生在驱动壳层。
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
}

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
        # T20 内容寻址缓存:源文件哈希命中 → 复用发射产物(同输入同产物)
        CACHE_DIR="$ROOT/.cache/emit"
        IN_HASH=$(shasum "$IN" | cut -d' ' -f1)_$(shasum "$DIR/build/cc_emit.ct" | cut -d' ' -f1 | cut -c1-12)
        CACHED="$CACHE_DIR/$IN_HASH.c"
        if [ -f "$CACHED" ]; then
            cp "$CACHED" "$OUTC"
            echo "ctc.sh: 已发射 $OUTC(缓存命中: $IN_HASH)"
            emit_link_summary "$OUTC"
            exit 0
        fi
        "$DIR/build.sh" >/dev/null
        TMP=$(mktemp /tmp/ctron_cc.XXXXXX)
        sed "s|ANCHORINPUT|$IN|" "$DIR/build/cc_emit.ct" | sed "s|ANCHORLANG|$DIAGLANG|" > "$TMP"
        "$HOST" run "$TMP" > "$OUTC"
        rc=$?
        if [ $rc -eq 0 ]; then
            mkdir -p "$CACHE_DIR"
            cp "$OUTC" "$CACHED" 2>/dev/null
            echo "ctc.sh: 已发射 $OUTC(编译: cc -O2 $OUTC -o bin;运行: ./bin run $IN)"
            emit_link_summary "$OUTC"
        fi
        ;;
    build)
        OUTBIN="ctron_app"
        prev_o=0
        for a2 in "$@"; do
            if [ "$prev_o" = "1" ]; then OUTBIN="$a2"; prev_o=0; fi
            case $a2 in -o) prev_o=1 ;; esac
        done
        # J19-④:清单 dependencies.gui → 自动链接 GUI 域库(vendored raylib/freetype
        # + ctron_gui 桥 + 平台框架参数)——30 行链接咒语从示例 run.sh 消失。
        # 域解析只在驱动壳层(与 #[link] pkg-config 面同哲学)。
        manifest_gui_dep() {
            MF="$1"
            [ -f "$MF" ] || return 1
            # 注:awk 的 END 无条件覆盖块内 exit 状态,须以 found 标志收口
            awk 'BEGIN{f=0; found=0} /dependencies[[:space:]]*\{/{f=1} f{ if ($0 ~ /[[:space:]]\}[[:space:]]*$/ || $0 ~ /^\}/) f=0; if ($0 ~ /(^|[[:space:]])gui([[:space:]]*$|[[:space:]]+[^=])/) { found=1; exit 0 } } END{ exit !found }' "$MF"
        }
        GUILDFLAGS=""
        if manifest_gui_dep "$(dirname "$IN")/../Ctron.ctcl"; then
            export CTRON_STDPATH="${CTRON_STDPATH:-$ROOT/lib/std}"
            sh "$ROOT/vendor/gui/build.sh" > /dev/null
            case "$(uname)" in
                Darwin) GUILDFLAGS="-I$ROOT/vendor/gui/clay -I$ROOT/vendor/gui/raylib -I$ROOT/vendor/gui/freetype/include $ROOT/pkgs/gui/c_src/ctron_gui.c $ROOT/pkgs/gui/c_src/ft_shim.c $ROOT/vendor/gui/build/libfreetype.a $ROOT/vendor/gui/build/libraylib.a -framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
                Linux)  GUILDFLAGS="-I$ROOT/vendor/gui/clay -I$ROOT/vendor/gui/raylib -I$ROOT/vendor/gui/freetype/include $ROOT/pkgs/gui/c_src/ctron_gui.c $ROOT/pkgs/gui/c_src/ft_shim.c $ROOT/vendor/gui/build/libfreetype.a $ROOT/vendor/gui/build/libraylib.a -lX11 -lGL -lm -lpthread -ldl" ;;
                *) echo "ctc.sh: GUI 自动链接不支持平台 $(uname)" >&2; exit 2 ;;
            esac
        fi
        TMPC=$(mktemp /tmp/ctron_build.XXXXXX) && mv "$TMPC" "$TMPC.c" && TMPC="$TMPC.c"
        "$0" emit "$IN" "$TMPC" > /dev/null 2>&1 || { echo "ctc.sh: build 发射失败" >&2; exit 1; }
        # shellcheck disable=SC2086
        $TGT_CC -w "$TMPC" -o "$OUTBIN" $GUILDFLAGS || { echo "ctc.sh: build 链接失败($TGT_CC)" >&2; exit 1; }
        echo "ctc.sh: 已构建 $OUTBIN(target: ${TARGET:-native};运行: ./$OUTBIN run $IN)"
        rc=0
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
    dep)
        "$DIR/build.sh" >/dev/null
        TMP=$(mktemp /tmp/ctron_dep.XXXXXX)
        sed -e "s|ANCHORINPUT|$IN|" -e "s|ANCHORLANG|$DIAGLANG|" "$DIR/build/cc_dep.ct" > "$TMP"
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
rm -f ${TMP:-}
exit $rc
