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

# sha256 单行封装(darwin shasum / linux sha256sum;D8-2 L2 seal 编排用)
ctc_sha() {
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | cut -d' ' -f1
    else
        sha256sum "$1" | cut -d' ' -f1
    fi
}

mode=run
PROF=full
TAUSTED=0
DIAGLANG=zh
case ${1:-} in
    check|emit|fmt|doc|ast|build|targets|new|dep) mode=$1; shift ;;
esac
prev_t=0
for a in "$@"; do
    if [ "$prev_t" = "1" ]; then TARGET=$a; prev_t=0; continue; fi
    case $a in
        --profile=*) PROF=${a#--profile=} ;;
        --trusted) TAUSTED=1 ;;
        --lang=*) DIAGLANG=${a#--lang=} ;;
    --deterministic) export CTRON_RT_SEED=1 ;;
    --target=*) TARGET=${a#--target=} ;;
    --target) prev_t=1 ;;
    esac
done
# T36 后端注册表(§9.7 插件化地板):triple/链接器最小面;C native=首个后端。
# T40(§9.3):bare 双靶注册(thumbv7em-none-eabi/riscv32imac-unknown-none)——
# 发射 C 与 native 同文,靶差全在 lib/rt/bare 运行时文件集(minilibc+startup+链接脚本);
# wasm32 随 T37 注册——未列名=清晰诊断(fail-closed)。
TARGET=${TARGET:-native}
BARE=0
WASM=0
case $TARGET in
    native) TGT_CC="cc -O2" ;;
    thumbv7em-none-eabi|riscv32imac-unknown-none) BARE=1 ;;
    wasm32-unknown-unknown) WASM=1 ;;
    *) echo "ctc.sh: 未注册后端 target: $TARGET(注册表: native, thumbv7em-none-eabi, riscv32imac-unknown-none, wasm32-unknown-unknown)" >&2; exit 2 ;;
esac
if [ ! -x "$HOST" ]; then
    echo "ctc.sh: 缺少宿主 seed $HOST(先: make -C \"$ROOT/compiler-c\")" >&2
    exit 2
fi
if [ "$mode" != "targets" ] && [ "$mode" != "new" ] && { [ $# -lt 1 ] || [ ! -f "$1" ]; }; then
    echo "用法: ctc.sh <input.ct> | ctc.sh check <input.ct> | ctc.sh emit <input.ct> [out.c] | ctc.sh fmt <input.ct> | ctc.sh doc <input.ct> [--format=json] | ctc.sh ast <input.ct> [--ast=dump] | ctc.sh build <input.ct> [--target native|thumbv7em-none-eabi|riscv32imac-unknown-none|wasm32-unknown-unknown] [-o bin] | ctc.sh new <dir> [--gui] | ctc.sh dep <Ctron.ctcl> | ctc.sh targets" >&2
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
    echo "ctc.sh: 注册后端 target:"
    echo "  native                   cc -O2,宿主三件套"
    echo "  thumbv7em-none-eabi      bare 档 ARM(cortex-M4;§9.3 tier-1,semihosting)"
    echo "  riscv32imac-unknown-none bare 档 RISC-V rv32imac(§9.3 tier-1,qemu virt)"
    echo "  wasm32-unknown-unknown   web 档 MVP(T37;no_alloc/core 子集,单线程口径 §7.8;node 真跑)"
    echo "  (工具链解析: bare=CTRON_BARE_CC/CTRON_BARE_LD env → 宿主 clang+ld.lld → docker ctron-bare-tools;wasm=CTRON_WASM_CC env → 宿主 clang[wasm32 后端] → docker ctron-bare-tools)"
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

# T40:bare 工具链三通道(env → 宿主 clang+ld.lld → docker ctron-bare-tools)
# 与构建管线。发射 C 与 native 同文;靶差全在 lib/rt/bare(见其 README)。
# 失败=响亮诊断 exit 2(fail-closed),不静默降级。
BARE_CC="" ; BARE_LD="" ; BARE_MODE=""
bare_toolchain() {
    NEED="$1"   # arm|riscv
    if [ -n "$BARE_MODE" ]; then return 0; fi
    if [ -n "${CTRON_BARE_CC:-}" ] && [ -n "${CTRON_BARE_LD:-}" ]; then
        BARE_CC="$CTRON_BARE_CC"; BARE_LD="$CTRON_BARE_LD"; BARE_MODE=env; return 0
    fi
    HC=""
    for c in clang /opt/homebrew/opt/llvm/bin/clang /usr/local/opt/llvm/bin/clang; do
        if command -v "$c" >/dev/null 2>&1 || [ -x "$c" ]; then HC="$c"; break; fi
    done
    HL=""
    for l in ld.lld /opt/homebrew/opt/llvm/bin/ld.lld /usr/local/opt/llvm/bin/ld.lld; do
        if command -v "$l" >/dev/null 2>&1 || [ -x "$l" ]; then HL="$l"; break; fi
    done
    TGT_OK=0
    if [ -n "$HC" ]; then
        case $NEED in
            arm)   "$HC" --print-targets 2>/dev/null | grep -q "^ *arm  *- ARM" && TGT_OK=1 ;;
            riscv) "$HC" --print-targets 2>/dev/null | grep -qi "^ *riscv.*RISC-V" && TGT_OK=1 ;;
        esac
    fi
    if [ "$TGT_OK" = "1" ] && [ -n "$HL" ]; then
        BARE_CC="$HC"; BARE_LD="$HL"; BARE_MODE=host; return 0
    fi
    if docker image inspect ctron-bare-tools:latest >/dev/null 2>&1; then
        BARE_MODE=docker; return 0
    fi
    MISS=""
    [ -z "$HC" ] && MISS="clang 未找到"
    if [ -n "$HC" ] && [ "$TGT_OK" != "1" ] && [ "$NEED" = "riscv" ]; then MISS="本机 clang 无 RISC-V 后端(Apple clang 裁剪,常态)"; fi
    if [ -n "$HC" ] && [ "$TGT_OK" != "1" ] && [ "$NEED" = "arm" ]; then MISS="本机 clang 无 ARM 后端"; fi
    [ -z "$HL" ] && MISS="$MISS${MISS:+;} ld.lld 未找到(brew install llvm)"
    echo "ctc.sh: bare target $TARGET 工具链不可用: $MISS" >&2
    echo "  通道任选: ①brew install llvm(宿主 clang+ld.lld) ②docker build -t ctron-bare-tools(docs/rogo 或 tests/bare/README) ③CTRON_BARE_CC/CTRON_BARE_LD 指认" >&2
    return 2
}

bare_build_pipeline() {
    SRC="$1"; OUT="$2"
    RT="$ROOT/lib/rt/bare"
    case $TARGET in
        thumbv7em-none-eabi)      TTGT=arm;   TCPU="--target=armv7em-none-eabi -mcpu=cortex-m4 -mthumb -mfloat-abi=soft"; TLINK=cortex-m4; TSTART=start_arm ;;
        riscv32imac-unknown-none) TTGT=riscv; TCPU="--target=riscv32-unknown-elf -march=rv32imac -mabi=ilp32 -msmall-data-limit=0"; TLINK=rv32; TSTART=start_riscv ;;
    esac
    bare_toolchain "$TTGT" || return 2
    BFLAGS="-std=gnu11 -Os -g0 -ffreestanding -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -fno-common -D__thread= -w"
    if [ "$BARE_MODE" = "docker" ]; then
        mkdir -p "$ROOT/.cache/bare"
        REL_SRC=${SRC#"$ROOT"/}
        RTREL="lib/rt/bare"
        RELOUT=$(mktemp -u ".cache/bare/out_XXXXXX")
        docker run --rm -v "$ROOT":/ctroot -w /ctroot ctron-bare-tools:latest sh -c "
            clang $TCPU $BFLAGS -isystem $RTREL/include -c '$REL_SRC' -o '$RELOUT.o' &&
            clang $TCPU $BFLAGS -isystem $RTREL/include -c $RTREL/src/bare_libc.c -o '$RELOUT.libc.o' &&
            clang $TCPU $BFLAGS -isystem $RTREL/include -c $RTREL/src/$TSTART.c -o '$RELOUT.start.o' &&
            LG=\$(case $TTGT in arm) arm-none-eabi-gcc -mcpu=cortex-m4 -mfloat-abi=soft -mthumb -print-libgcc-file-name ;; riscv) riscv64-unknown-elf-gcc -march=rv32imac -mabi=ilp32 -print-libgcc-file-name ;; esac) &&
            ld.lld -T $RTREL/link/$TLINK.ld --gc-sections -s -z max-page-size=256 -o '$RELOUT' '$RELOUT.o' '$RELOUT.libc.o' '$RELOUT.start.o' \"\$LG\"
        " || { echo "ctc.sh: bare 构建 docker 通道失败(ctron-bare-tools)" >&2; return 1; }
        mkdir -p "$(dirname "$OUT")"
        mv "$ROOT/$RELOUT" "$OUT" || { echo "ctc.sh: bare 产物取回失败" >&2; return 1; }
    else
        "$BARE_CC" $TCPU $BFLAGS -isystem "$RT/include" -c "$SRC" -o "$SRC.bare.o" || { echo "ctc.sh: bare 编译失败(app 面)" >&2; return 1; }
        "$BARE_CC" $TCPU $BFLAGS -isystem "$RT/include" -c "$RT/src/bare_libc.c" -o "$SRC.libc.o" || { echo "ctc.sh: bare 编译失败(minilibc)" >&2; return 1; }
        "$BARE_CC" $TCPU $BFLAGS -isystem "$RT/include" -c "$RT/src/$TSTART.c" -o "$SRC.start.o" || { echo "ctc.sh: bare 编译失败(startup)" >&2; return 1; }
        LG=""
        case $TTGT in
            arm)   command -v arm-none-eabi-gcc >/dev/null 2>&1 && LG=$(arm-none-eabi-gcc -mcpu=cortex-m4 -mfloat-abi=soft -mthumb -print-libgcc-file-name) ;;
            riscv) command -v riscv64-unknown-elf-gcc >/dev/null 2>&1 && LG=$(riscv64-unknown-elf-gcc -march=rv32imac -mabi=ilp32 -print-libgcc-file-name) ;;
        esac
        "$BARE_LD" -T "$RT/link/$TLINK.ld" --gc-sections -s -z max-page-size=256 -o "$OUT" "$SRC.bare.o" "$SRC.libc.o" "$SRC.start.o" $LG || { echo "ctc.sh: bare 链接失败(ld.lld)" >&2; return 1; }
    fi
    echo "ctc.sh: bare 体积口径(ELF 总量): $(wc -c < "$OUT" | tr -d ' ') 字节"
    return 0
}

# T37:wasm32 工具链三通道(env → 宿主 clang[wasm32 后端] → docker ctron-bare-tools)
# 与构建管线。发射 C 与 native 同文;靶差全在 lib/rt/wasm32(wasm_libc:输出/退出面
# 走 env.* import,堆=16MiB 线性内存静态池;单线程口径 §7.8)。失败=响亮诊断 exit 2。
WASM_CC="" ; WASM_MODE=""
wasm_toolchain() {
    if [ -n "$WASM_MODE" ]; then return 0; fi
    if [ -n "${CTRON_WASM_CC:-}" ]; then
        WASM_CC="$CTRON_WASM_CC"; WASM_MODE=env; return 0
    fi
    HC=""
    for c in clang /opt/homebrew/opt/llvm/bin/clang /usr/local/opt/llvm/bin/clang; do
        if command -v "$c" >/dev/null 2>&1 || [ -x "$c" ]; then HC="$c"; break; fi
    done
    TGT_OK=0
    if [ -n "$HC" ]; then
        "$HC" --print-targets 2>/dev/null | grep -qi "^ *wasm32" && TGT_OK=1
    fi
    if [ "$TGT_OK" = "1" ]; then
        WASM_CC="$HC"; WASM_MODE=host; return 0
    fi
    if docker image inspect ctron-bare-tools:latest >/dev/null 2>&1; then
        WASM_MODE=docker; return 0
    fi
    MISS="带 wasm32 后端的 clang 未找到(Apple clang 裁剪,常态)"
    echo "ctc.sh: wasm target $TARGET 工具链不可用: $MISS" >&2
    echo "  通道任选: ①brew install llvm(宿主 clang+wasm-ld) ②docker 镜像 ctron-bare-tools ③CTRON_WASM_CC 指认" >&2
    return 2
}

wasm_build_pipeline() {
    SRC="$1"; OUT="$2"
    WTGT="--target=wasm32-unknown-unknown"
    WFLAGS="-std=gnu11 -Os -g0 -ffreestanding -ffunction-sections -fdata-sections -fno-common -D__thread= -w"
    WLINK="-nostdlib -Wl,--no-entry -Wl,--export=main -Wl,--export-memory -Wl,--gc-sections -Wl,-s -Wl,-z,stack-size=1048576"
    wasm_toolchain || return 2
    RTREL="lib/rt/wasm32"
    if [ "$WASM_MODE" = "docker" ]; then
        mkdir -p "$ROOT/.cache/wasm"
        REL_SRC=${SRC#"$ROOT"/}
        RELOUT=$(mktemp -u ".cache/wasm/out_XXXXXX")
        docker run --rm -v "$ROOT":/ctroot -w /ctroot ctron-bare-tools:latest sh -c "
            clang $WTGT $WFLAGS -isystem $RTREL/include -c '$REL_SRC' -o '$RELOUT.app.o' &&
            clang $WTGT $WFLAGS -isystem $RTREL/include -c $RTREL/src/wasm_libc.c -o '$RELOUT.libc.o' &&
            clang $WTGT $WLINK -o '$RELOUT.wasm' '$RELOUT.app.o' '$RELOUT.libc.o'
        " || { echo "ctc.sh: wasm 构建 docker 通道失败(ctron-bare-tools)" >&2; return 1; }
        mkdir -p "$(dirname "$OUT")"
        mv "$ROOT/$RELOUT.wasm" "$OUT" || { echo "ctc.sh: wasm 产物取回失败" >&2; return 1; }
    else
        "$WASM_CC" $WTGT $WFLAGS -isystem "$ROOT/$RTREL/include" -c "$SRC" -o "$SRC.app.o" || { echo "ctc.sh: wasm 编译失败(app 面)" >&2; return 1; }
        "$WASM_CC" $WTGT $WFLAGS -isystem "$ROOT/$RTREL/include" -c "$ROOT/$RTREL/src/wasm_libc.c" -o "$SRC.libc.o" || { echo "ctc.sh: wasm 编译失败(wasm_libc)" >&2; return 1; }
        mkdir -p "$(dirname "$OUT")"
        "$WASM_CC" $WTGT $WLINK -o "$OUT" "$SRC.app.o" "$SRC.libc.o" || { echo "ctc.sh: wasm 链接失败(clang→wasm-ld)" >&2; return 1; }
        rm -f "$SRC.app.o" "$SRC.libc.o"
    fi
    echo "ctc.sh: wasm 体积口径(.wasm 字节): $(wc -c < "$OUT" | tr -d ' ')"
    return 0
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
        # S4-⑤:emit 管线注入 std 根(dev 布局 seed 宿主三探皆空,std use 曾静默
        # fail-open 产废品 C;装机态/显式指路不受影响 ${:-} 保用户覆写)
        CTRON_STDPATH="${CTRON_STDPATH:-$ROOT/lib/std}" "$HOST" run "$TMP" > "$OUTC"
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
        [ "$WASM" = "1" ] && OUTBIN="ctron_app.wasm"
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
        if [ "$BARE" = "1" ]; then
            # bare 靶:先解析工具链(定 BARE_MODE),TMPC/ELF 落 ROOT/.cache/bare(docker 挂载面内)
            case $TARGET in
                thumbv7em-none-eabi) bare_toolchain arm || exit 2 ;;
                *) bare_toolchain riscv || exit 2 ;;
            esac
            mkdir -p "$ROOT/.cache/bare"
            TMPC=$(mktemp "$ROOT/.cache/bare/src_XXXXXX") && mv "$TMPC" "$TMPC.c" && TMPC="$TMPC.c"
        elif [ "$WASM" = "1" ]; then
            # wasm 靶:TMPC 落 ROOT/.cache/wasm(docker 挂载面内);
            # 工具链解析延迟到 wasm_build_pipeline——FFI 语义诊断先于环境解析(与工具链无关)
            mkdir -p "$ROOT/.cache/wasm"
            TMPC=$(mktemp "$ROOT/.cache/wasm/src_XXXXXX") && mv "$TMPC" "$TMPC.c" && TMPC="$TMPC.c"
        else
            TMPC=$(mktemp /tmp/ctron_build.XXXXXX) && mv "$TMPC" "$TMPC.c" && TMPC="$TMPC.c"
        fi
        "$0" emit "$IN" "$TMPC" > /dev/null 2>&1 || { echo "ctc.sh: build 发射失败" >&2; exit 1; }
        if [ "$WASM" = "1" ] && grep -q "ctron:link" "$TMPC"; then
            # T37:wasm 无 dlopen——FFI 面(#[link])在 wasm target 禁用,诊断非静默
            echo "ctc.sh: wasm32 target 检测到 #[link] FFI 面——wasm 无 dlopen,FFI 在 wasm target 禁用(T37;诊断非静默)" >&2
            rm -f "$TMPC"
            exit 2
        fi
        if [ "$BARE" = "1" ]; then
            TELF="$OUTBIN"
            if [ "$BARE_MODE" = "docker" ]; then
                TELF=$(mktemp -u "$ROOT/.cache/bare/app_XXXXXX.elf")
            fi
            bare_build_pipeline "$TMPC" "$TELF" || { rm -f "$TMPC" "$TELF"; exit 2; }
            if [ "$BARE_MODE" = "docker" ] && [ "$TELF" != "$OUTBIN" ]; then
                mv "$TELF" "$OUTBIN" || { echo "ctc.sh: bare 产物归位失败" >&2; exit 1; }
            fi
            rm -f "$TMPC"
            echo "ctc.sh: 已构建 $OUTBIN(target: $TARGET;运行: qemu 半主机,见 lib/rt/bare/README.md)"
        elif [ "$WASM" = "1" ]; then
            TWASM="$OUTBIN"
            if [ "$WASM_MODE" = "docker" ]; then
                TWASM=$(mktemp -u "$ROOT/.cache/wasm/app_XXXXXX.wasm")
            fi
            wasm_build_pipeline "$TMPC" "$TWASM" || { rm -f "$TMPC" "$TWASM"; exit 2; }
            if [ "$WASM_MODE" = "docker" ] && [ "$TWASM" != "$OUTBIN" ]; then
                mv "$TWASM" "$OUTBIN" || { echo "ctc.sh: wasm 产物归位失败" >&2; exit 1; }
            fi
            rm -f "$TMPC"
            echo "ctc.sh: 已构建 $OUTBIN(target: $TARGET;运行: node 胶水,见 lib/rt/wasm32/README.md)"
        else
            # shellcheck disable=SC2086
            $TGT_CC -w "$TMPC" -o "$OUTBIN" $GUILDFLAGS || { echo "ctc.sh: build 链接失败($TGT_CC)" >&2; exit 1; }
            echo "ctc.sh: 已构建 $OUTBIN(target: ${TARGET:-native};运行: ./$OUTBIN run $IN)"
        fi
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
        ASTDEP=""
        for a in "$@"; do
            case $a in
                --ast=dump) ASTMODE=dump ;;
                --ast=seal) ASTMODE=seal ;;
                --astout=*) ASTOUT=${a#--astout=} ;;
                --astname=*) ASTNAME=${a#--astname=} ;;
                --depdigest=*) ASTDEP="$ASTDEP${ASTDEP:+;}${a#--depdigest=}" ;;
            esac
        done
        "$DIR/build.sh" >/dev/null
        # D8-2 L2:依赖需求摘要条目(名:sha256:hex;分号分隔)经临时文件注入驱动
        #(锚换靶为单行 sed,多行块过不去;文件面由 shell 写,驱动原样拼入 meta)
        DEPF=$(mktemp /tmp/ctron_depd.XXXXXX)
        : > "$DEPF"
        if [ -n "$ASTDEP" ]; then
            OIFS=$IFS
            IFS=';'
            for e in $ASTDEP; do
                dn=${e%%:*}
                dv=${e#*:}
                printf 'dep "%s" {\n  digest = "%s"\n}\n' "$dn" "$dv" >> "$DEPF"
            done
            IFS=$OIFS
        fi
        TMP=$(mktemp /tmp/ctron_ast.XXXXXX)
        sed -e "s|ANCHORINPUT|$IN|" -e "s|ANCHORAST|$ASTMODE|" -e "s|ANCHOROUT|$ASTOUT|" -e "s|ANCHORNAME|$ASTNAME|" -e "s|ANCHORDEPFILE|$DEPF|" -e "s|ANCHORLANG|$DIAGLANG|" "$DIR/build/cc_ast.ct" > "$TMP"
        "$HOST" run "$TMP"
        rc=$?
        # D8-2 L2 seal 编排:SHA256SUMS(impl/** 载荷,路径字节序;不含 meta——
        # self_digest 入 meta,含之即循环)+ self_digest=sha256(SHA256SUMS 字节)
        # 插入 artifact 块(meta 首块,首个 ^} 即其闭括号)。摘要计算在编排层,
        # 编译器热路径零触碰(D8-2 分工冻结)。
        if [ "$rc" -eq 0 ] && [ "$ASTMODE" = seal ] && [ -f "$ASTOUT/meta.ctcl" ] && [ -d "$ASTOUT/impl" ]; then
            # S3-α:源旁 traces/(黄金轨迹)→ 工件;轨迹字节随 SHA256SUMS/self_digest
            # 覆盖,防篡改免费继承(§3 布局 traces/ 首兑现)。
            SRCDIR=$(dirname -- "$IN")
            if [ -d "$SRCDIR/traces" ] && [ ! -e "$ASTOUT/traces" ]; then
                cp -r "$SRCDIR/traces" "$ASTOUT/traces"
            fi
            (
                cd "$ASTOUT" || exit 1
                rm -f SHA256SUMS
                { find impl -type f; if [ -d traces ]; then find traces -type f; fi; } | LC_ALL=C sort | while IFS= read -r f; do
                    printf '%s  %s\n' "$(ctc_sha "$f")" "$f"
                done > SHA256SUMS
            )
            SELF=$(ctc_sha "$ASTOUT/SHA256SUMS")
            if [ -n "$SELF" ]; then
                awk -v sd="$SELF" 'BEGIN{ins=0} ins==0 && $0=="}" { print "  self_digest = \"sha256:" sd "\""; ins=1 } { print }' "$ASTOUT/meta.ctcl" > "$ASTOUT/meta.ctcl.n"
                mv "$ASTOUT/meta.ctcl.n" "$ASTOUT/meta.ctcl"
            fi
        fi
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
rm -f ${TMP:-} ${DEPF:-}
exit $rc
