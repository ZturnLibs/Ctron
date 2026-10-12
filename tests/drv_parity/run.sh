#!/bin/sh
# tests/drv_parity/run.sh —— 原生驱动器对拍门(W4 波1:骨架批;gate 21→22 新步)
# sh 参考(仓库根 ctron)vs 原生驱动器(compiler/bin/ctron)对同一语料双跑:
# stdout+stderr 合流捕获,字节级 diff(cmp) + rc 对比,逐行报告。
# gt 纪律:两实现对同一语料的输出必须逐字节(含换行)一致——首跑即抓差异逐条修,
# 修向永远是原生侧(sh 文本是 oracle)。
# 缺件 = SKIP 显式登记,门绿(镜像 bare/wasm 环境门惯例):sh 参考或原生驱动器缺席
# (后者先跑 compiler/native.sh)。
# 语料波1 = 已实现臂全集(--version|-V/--help|-h/help/help <cmd>/<cmd> --help 拦截/
# 未知子命令/无参);批2 委派族 run/check/doc/test;批3 fmt/lint 全态(唯一目录
# 夹具现造现清:-w 双版各写,sh/native 各侧跑前各重建,同路径基防打印路径分叉;
# -w 例加产物树 diff -r 全树逐字节)。未实现已知名不进语料——sh 有真行为而
# 原生为占位 rc2(诚实账),波2/3 补臂时同步补语料行。
# W4 波2 批1:build 双臂(单文件+项目模式)。正例加产物树比对——链接产物
# Mach-O UUID 字节非确定,树 diff -x 排二进制、二进制改双跑 stdout 行为比对
# (run_bin2);项目/单文件夹具现造现清,workspace 逐成员自 spawn 递归含成员
# 产物(m/build/<名>)。缺 cc/缺 ctron-dep 出路面无法 CI 模拟,登记不语料化。
# W4 波2 批2:task/gate/clean 臂全态(本地 CTCL 读取消息逐字/runner 元字符拒/
# 引号双层/task.cwd/clean 三域与 --all/gate 解析序三段)。沙盒每例双版唯一目录
# 现造现清,各侧各指各自 CTRON_DRV_ROOT(clean/task 写面互不越界);prep 不拷
# ctron.ctcl 的夹具(clean/gate 回落)防 darwin 大小写不敏感盘同文件互毁。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
SHREF="$ROOT/ctron"
NAT="$ROOT/compiler/bin/ctron"
T=$(mktemp -d /tmp/ctron_drvp.XXXXXX)
trap 'rm -rf "$T"' EXIT
# 委派族语料用相对夹具路径 → 钉 cwd = 仓库根(任目录可跑;PWD 同导给两侧实现,
# 绝对化拼接逐字同源)。既有语料(旗标/帮助面)不受 cwd 影响。
cd "$ROOT"

if [ ! -x "$SHREF" ]; then
    echo "drv_parity: SKIP(sh 参考 $SHREF 缺席;登记在案,门绿)"
    exit 0
fi
if [ ! -x "$NAT" ]; then
    echo "drv_parity: SKIP(原生驱动器 $NAT 缺席;先跑 compiler/native.sh;登记在案,门绿)"
    exit 0
fi

pass=0; fail=0
report() { # $1=label $2=sh_rc $3=nat_rc —— 已备好 $T/sh.out 与 $T/nat.out 后调用
    if [ "$2" = "$3" ] && cmp -s "$T/sh.out" "$T/nat.out"; then
        pass=$((pass+1)); echo "  ok  : $1(rc=$2;逐字节+rc 一致)"
    else
        fail=$((fail+1))
        echo "  FAIL: $1(sh_rc=$2 native_rc=$3)"
        echo "    ---- diff(sh ←→ native,首 10 行) ----"
        diff "$T/sh.out" "$T/nat.out" 2>&1 | sed 's/^/    /' | sed -n '1,10p'
    fi
}
run_case() { # $1=label,其余 = 语料参数
    label=$1; shift
    sh "$SHREF" "$@" > "$T/sh.out" 2>&1; r1=$?
    "$NAT" "$@" > "$T/nat.out" 2>&1; r2=$?
    report "$label" "$r1" "$r2"
}

# ---- fmt/lint 语料(W4 波1 批3)----
# 纪律:-w 会写目标文件 → 夹具每例现造现清,sh/native 各跑前各重建一次
# (同一路径基 $FX = 套件 mktemp 下固定名,两侧打印路径同串;-w 例另比对
#  产物树,sh 写后存档、native 写后 diff -r 全树逐字节)。
FX="$T/fx"
mk_fx() {
    rm -rf "$FX"
    mkdir -p "$FX/pkg1/src" "$FX/pkg2" "$FX/pkg3/src"
    printf 'fn main() {\n    println("ok")\n}\n' > "$FX/clean.ct"
    printf 'fn main() {\n    let x=1\n        println("hi")\n}\n' > "$FX/dirty.ct"
    printf 'fn main() {\n    let x: I32 = "s"\n}\n' > "$FX/ew.ct"
    printf 'fn main() {\n    let unused_x = 1\n}\n' > "$FX/wonly.ct"
    printf 'fn a() {\n    let p=1\n}\n' > "$FX/pkg1/src/a_clean.ct"
    printf 'fn b() {\n    let q=2\n        println("z")\n}\n' > "$FX/pkg1/src/z_dirty.ct"
    printf 'fn c() {\n    let r=3\n}\n' > "$FX/pkg2/flat.ct"
}
fx_case() { # 非写例:各侧跑前重建夹具,比对 stdout+stderr+rc
    label=$1; shift
    mk_fx
    sh "$SHREF" "$@" > "$T/sh.out" 2>&1; r1=$?
    mk_fx
    "$NAT" "$@" > "$T/nat.out" 2>&1; r2=$?
    report "$label" "$r1" "$r2"
}
tree_case() { # -w 例:sh 跑后存档产物树 → 重建 → native 跑 → diff -r 全树
    label=$1; shift
    mk_fx
    sh "$SHREF" "$@" > "$T/sh.out" 2>&1; r1=$?
    rm -rf "$T/sh.tree"; cp -R "$FX" "$T/sh.tree"
    mk_fx
    "$NAT" "$@" > "$T/nat.out" 2>&1; r2=$?
    if [ "$r1" = "$r2" ] && cmp -s "$T/sh.out" "$T/nat.out" && diff -r "$T/sh.tree" "$FX" > "$T/tree.diff" 2>&1; then
        pass=$((pass+1)); echo "  ok  : $label(rc=$r2;逐字节+rc+产物树 一致)"
    else
        fail=$((fail+1))
        echo "  FAIL: $label(sh_rc=$r1 native_rc=$r2)"
        diff "$T/sh.out" "$T/nat.out" 2>&1 | sed 's/^/    /' | sed -n '1,10p'
        sed 's/^/    tree: /' "$T/tree.diff" 2>/dev/null | sed -n '1,10p'
    fi
}

echo "== tests/drv_parity 原生驱动器对拍门(W4 波1:骨架)=="
# ---- 版本面 ----
run_case "--version"            --version
run_case "-V"                   -V
# ---- 帮助面(整版 usage)----
run_case "--help"               --help
run_case "-h"                   -h
run_case "help(无参)"            help
# ---- help <cmd>(详助)----
run_case "help build(多行臂)"    help build
run_case "help fmt(单行臂)"      help fmt
run_case "help bench(多行臂)"    help bench
run_case "help task(单行臂)"     help task
run_case "help nosuch(未知名=整版 usage)" help nosuch
# ---- <cmd> --help 拦截(先于分派臂)----
run_case "fmt --help(拦截臂)"    fmt --help
run_case "run --help(拦截臂)"    run --help
# ---- 未知子命令(stderr + rc 2)----
run_case "frobnicate(未知)"      frobnicate
# ---- 委派族(W4 波1 批2):真 .ct 夹具;两侧 exec 同一 ctron-cc/chk/doc,
# ---- 编译器诊断/stdout 透传即逐字,diff 只可能出自本驱动器的 argv/env/cwd 处理
FIX=tests/drv_parity/fixtures
run_case "run hello.ct(正例)"           run "$FIX/hello.ct"
run_case "run bad.ct(编译错诊断透传)"     run "$FIX/bad.ct"
run_case "run 多余参不透传"               run "$FIX/hello.ct" not-passed
run_case "check hello.ct(绝对化)"        check "$FIX/hello.ct"
run_case "check bad.ct(绝对化+诊断透传)"  check "$FIX/bad.ct"
run_case "check 裸名(cwd 绝对化)"         check hello.ct
run_case "check --format=json 透传"       check "$FIX/hello.ct" --format=json
run_case "check ..路径 json(归一判别)"     check tests/../tests/drv_parity/fixtures/bad.ct --format=json
run_case "doc 路径形(绝对化分支)"          doc "$FIX/hello.ct"
run_case "doc std.json(裸名 std 透传)"   doc std.json
run_case "test tst.ct(test 块面)"        test "$FIX/tst.ct"
run_case "check(缺参 rc2)"              check
run_case "doc(缺参 rc2)"                doc
run_case "test(缺参 rc2)"               test
run_case "run(缺参 rc2)"                run
# 注:裸 run 缺参行初批按批1 已知偏差账纪律暂缓,批2 评审 sanction 入册(账销)。
# ---- 无参(usage rc 2)----
sh "$SHREF" > "$T/sh.out" 2>&1; r1=$?
"$NAT" > "$T/nat.out" 2>&1; r2=$?
report "无参(usage rc2)" "$r1" "$r2"

# ---- fmt 臂(W4 波1 批3;夹具唯一目录现造现清,防 -w 双版互踩)----
fx_case "fmt --check 干净(rc0 静默)"        fmt "$FX/clean.ct" --check
fx_case "fmt --check 脏(点名+计数 rc1)"     fmt "$FX/dirty.ct" --check
fx_case "fmt --check 缺席单文件(cmp 同判)"   fmt "$FX/nope.ct" --check
tree_case "fmt -w 脏(写回+点名+产物树)"      fmt -w "$FX/dirty.ct"
tree_case "fmt -w 干净(未变亦点名,零写)"     fmt -w "$FX/clean.ct"
fx_case "fmt 默认打印脏(格式化文直出)"       fmt "$FX/dirty.ct"
fx_case "fmt 目录形 --check(src 优先+字节序)" fmt "$FX/pkg1" --check
fx_case "fmt 尾斜杠目录 --check(根前缀尾/折叠)" fmt "$FX/pkg2/" --check
fx_case "fmt 尾斜杠 src 形 --check(内部 // 逐字)" fmt "$FX/pkg1/" --check
fx_case "fmt 平层目录 --check(无 src 直下)"  fmt "$FX/pkg2" --check
fx_case "fmt 空 .ct 目录 --check(静默 rc0)"  fmt "$FX/pkg3" --check
fx_case "fmt 目录形默认(拼接直出 rc0)"       fmt "$FX/pkg1"
tree_case "fmt -w 目录形(多文件写回+全树)"    fmt --write "$FX/pkg1"
fx_case "fmt 缺参(rc2)"                    fmt
fx_case "fmt --check 无目标(rc2)"            fmt --check

# ---- lint 臂(W4 波1 批3;--trusted/--strict/--缺目标/汇总行全谱)----
fx_case "lint 干净(check OK 滤后 0/0)"       lint "$FX/clean.ct"
fx_case "lint E+W(计数 rc1)"                lint "$FX/ew.ct"
fx_case "lint W-only(默认不红)"              lint "$FX/wonly.ct"
fx_case "lint W-only --strict(警告也红)"      lint "$FX/wonly.ct" --strict
fx_case "lint W-only --trusted(透传)"        lint "$FX/wonly.ct" --trusted
fx_case "lint 目录形(分隔行+多文件汇总)"      lint "$FX/pkg1"
fx_case "lint 目录无 .ct(rc2)"              lint "$FX/pkg3"
fx_case "lint 目标不存在(rc2)"              lint "$FX/nope.ct"
fx_case "lint 缺参(rc2)"                    lint
fx_case "lint 未知旗标(rc2)"                 lint "$FX/clean.ct" --bogus

# ---- build 臂(W4 波2 批1)----
# 单文件:夹具 $FX/b 现造现清;正例加产物树比对——链接产物 Mach-O UUID 字节非
# 确定,树 diff -x 排除二进制、二进制改双跑 stdout 行为比对;<stem>.c 逐字节。
# wasm 靶委派 ctc.sh(两侧同件;ctc.sh build 内 emit 静音,工具链诊断即全量
# 输出,逐字同;emit 缓存同哈希,首侧实发次侧命中皆静音,无分叉面)。
BK="$FX/b"
mk_bk() {
    rm -rf "$BK"
    mkdir -p "$BK"
    printf 'fn main() {\n    println("bk-ok")\n}\n' > "$BK/hello.ct"
}
run_bin2() { # 链接产物双跑(stdout+rc 行为比对;UUID 非确定面的诚实替位)
    o1=$("$1" 2>&1); b1=$?
    o2=$("$2" 2>&1); b2=$?
    if [ "$b1" = "$b2" ] && [ "$o1" = "$o2" ]; then
        echo "    bin  : $3 双跑一致(rc=$b1)"
    else
        echo "    bin  : $3 双跑分歧(sh=[$o1] native=[$o2])"
        fail=$((fail+1))
    fi
}
bk_case() { # 非产物例(stdout+stderr+rc);BK_CWD 可覆执行 cwd(缺省仓库根)
    label=$1; shift
    mk_bk
    (cd "${BK_CWD:-$ROOT}" && sh "$SHREF" "$@") > "$T/sh.out" 2>&1; r1=$?
    mk_bk
    (cd "${BK_CWD:-$ROOT}" && "$NAT" "$@") > "$T/nat.out" 2>&1; r2=$?
    report "$label" "$r1" "$r2"
}
bk_tree_case() { # $1=label $2=二进制名(排除+双跑;BIN_SUB=二进制子目录,缺省树根),其余 = 语料
    label=$1; binnm=$2; shift 2
    mk_bk
    (cd "${BK_CWD:-$ROOT}" && sh "$SHREF" "$@") > "$T/sh.out" 2>&1; r1=$?
    rm -rf "$T/sh.tree"; cp -R "$BK" "$T/sh.tree"
    mk_bk
    (cd "${BK_CWD:-$ROOT}" && "$NAT" "$@") > "$T/nat.out" 2>&1; r2=$?
    if [ "$r1" = "$r2" ] && cmp -s "$T/sh.out" "$T/nat.out" && diff -r -x "$binnm" "$T/sh.tree" "$BK" > "$T/tree.diff" 2>&1; then
        pass=$((pass+1)); echo "  ok  : $label(rc=$r2;逐字节+rc+产物树 一致)"
        run_bin2 "$T/sh.tree${BIN_SUB:+/$BIN_SUB}/$binnm" "$BK${BIN_SUB:+/$BIN_SUB}/$binnm" "$binnm"
    else
        fail=$((fail+1))
        echo "  FAIL: $label(sh_rc=$r1 native_rc=$r2)"
        diff "$T/sh.out" "$T/nat.out" 2>&1 | sed 's/^/    /' | sed -n '1,10p'
        sed 's/^/    tree: /' "$T/tree.diff" 2>/dev/null | sed -n '1,10p'
    fi
}

# 项目模式:夹具 $FX/pj(cwd 钉夹具目录 = sh 项目模式现场;清单 name 提取 /
# dep→ctron-dep(lock 落盘入树 diff)/ workspace 逐成员递归全谱)。
PJ="$FX/pj"
mk_pj() {
    rm -rf "$PJ"
    mkdir -p "$PJ/src" "$PJ/c_src"
    printf 'pkg {\n    manifest_version = 1\n    name = "myapp"\n    version = "0.1.0"\n}\n' > "$PJ/Ctron.ctcl"
    printf 'extern "c" fn pj_add2(x: I32) -> I32\n\nfn main() {\n    println("pj-" + pj_add2(2).to_string())\n}\n' > "$PJ/src/main.ct"
    printf 'int pj_add2(int x) { return x + 20; }\n' > "$PJ/c_src/extra.c"
}
prep_pj_noname() { # name 行缺席 → 目录名回退
    printf 'pkg {\n    manifest_version = 1\n    version = "0.1.0"\n}\n' > "$PJ/Ctron.ctcl"
}
prep_pj_dep() { # dep 行 → ctron-dep run(Ctron.lock 落盘入树)
    printf 'dep "localdep"\n' >> "$PJ/Ctron.ctcl"
}
prep_pj_ws() { # workspace 根 → 逐成员子构建(根入口仍必需 = sh 顺序语义)
    mkdir -p "$PJ/m/src"
    printf 'workspace\nmember "m"\npath = "m"\n' >> "$PJ/Ctron.ctcl"
    printf 'pkg {\n    name = "mem"\n}\n' > "$PJ/m/Ctron.ctcl"
    printf 'fn main() {\n    println("mem-ok")\n}\n' > "$PJ/m/src/main.ct"
}
prep_pj_nocl() { rm -f "$PJ/Ctron.ctcl"; }
prep_noentry()  { rm -rf "$PJ/src"; }
prep_pj_toml()  { rm -f "$PJ/Ctron.ctcl"; printf 'name = "legacy"\n' > "$PJ/Ctron.toml"; }
pj_case() { # $1=label $2=形态(out|tree) $3=prep(空串跳过) $4=二进制名,其余 = 语料
    label=$1; mode=$2; prep=$3; binnm=$4; shift 4
    mk_pj
    [ -n "$prep" ] && "$prep"
    (cd "$PJ" && sh "$SHREF" "$@") > "$T/sh.out" 2>&1; r1=$?
    [ "$mode" = tree ] && { rm -rf "$T/sh.tree"; cp -R "$PJ" "$T/sh.tree"; }
    mk_pj
    [ -n "$prep" ] && "$prep"
    (cd "$PJ" && "$NAT" "$@") > "$T/nat.out" 2>&1; r2=$?
    if [ "$r1" = "$r2" ] && cmp -s "$T/sh.out" "$T/nat.out" && { [ "$mode" != tree ] || diff -r -x "${binnm##*/}" "$T/sh.tree" "$PJ" > "$T/tree.diff" 2>&1; }; then
        pass=$((pass+1)); echo "  ok  : $label(rc=$r2;逐字节+rc$( [ "$mode" = tree ] && echo '+产物树') 一致)"
        [ "$mode" = tree ] && run_bin2 "$T/sh.tree/$binnm" "$PJ/$binnm" "${binnm##*/}"
    else
        fail=$((fail+1))
        echo "  FAIL: $label(sh_rc=$r1 native_rc=$r2)"
        diff "$T/sh.out" "$T/nat.out" 2>&1 | sed 's/^/    /' | sed -n '1,10p'
        sed 's/^/    tree: /' "$T/tree.diff" 2>/dev/null | sed -n '1,10p'
    fi
}

echo "== build 臂(W4 波2 批1:单文件+项目模式)=="
# ---- 单文件 ----
bk_tree_case "build 单文件正例(产物树+二进制双跑)"  hello build "$BK/hello.ct"
BK_CWD="$T" bk_tree_case "build 相对路径(cwd 钉 $T)"  hello build fx/b/hello.ct; BK_CWD=""
bk_case "build --target 未注册(rc2 逐字)"            build "$BK/hello.ct" --target nosuch-tgt
bk_case "build --target wasm32(委派 ctc.sh,= 形)"   build "$BK/hello.ct" --target=wasm32-unknown-unknown
bk_case "build --target wasm32(委派 ctc.sh,空格形)"  build "$BK/hello.ct" --target wasm32-unknown-unknown
bk_case "build 缺席文件(空模块 emit,两侧同形)"       build "$BK/nope.ct"
# ---- 项目模式 ----
pj_case "build 项目正例(name 提取+c_src 链接)"       tree ""     build/myapp build
pj_case "build name 缺席(目录名回退)"                tree prep_pj_noname build/pj build
pj_case "build dep 行(ctron-dep run+lock 入树)"      tree prep_pj_dep    build/myapp build
pj_case "build workspace(逐成员自 spawn 递归)"       tree prep_pj_ws     m/build/mem build
pj_case "build Ctron.toml fail-closed(T48 逐字)"     out prep_pj_toml ""  build
pj_case "build 缺入口 src/main.ct(rc2)"              out prep_noentry ""  build
pj_case "build 缺清单(rc2)"                          out prep_pj_nocl "" build

# ---- task/gate/clean 臂(W4 波2 批2)----
# 夹具 = fixtures/tasks 静态件(工作清单/坏清单族/data 脚本,只读拷入)。沙盒每例
# 双版唯一目录现造现清(-w 纪律同 fmt 臂;clean/task 写面互不越界),每侧各指各
# 自 CTRON_DRV_ROOT。清单提供走 prep(缺省 = 工作清单;clean/gate 回落夹具不拷
# ctron.ctcl——darwin 大小写不敏感盘上 ctron.ctcl ≡ Ctron.ctcl 同文件,共存即
# 互毁,嗅探面亦随 FS 语义分叉)。
TKFIX=tests/drv_parity/fixtures/tasks
TK="$T/tk"
tk_mf() { cp "$ROOT/$TKFIX/ctron.ctcl" "$1/ctron.ctcl"; }
tk_side() { # $1=side 目录 $2=prep 名(空 = 工作清单)
    mkdir -p "$1"
    cp -r "$ROOT/$TKFIX/data" "$1/data"
    if [ -n "$2" ]; then "$2" "$1"; else tk_mf "$1"; fi
}
tk_case() { # $1=label $2=prep(空 = 工作清单),其余 = 语料
    label=$1; prep=$2; shift 2
    rm -rf "$TK"
    tk_side "$TK/sh" "$prep"
    (cd "$TK/sh" && CTRON_DRV_ROOT="$TK/sh" sh "$SHREF" "$@") > "$T/sh.out" 2>&1; r1=$?
    rm -rf "$TK"
    tk_side "$TK/nat" "$prep"
    (cd "$TK/nat" && CTRON_DRV_ROOT="$TK/nat" "$NAT" "$@") > "$T/nat.out" 2>&1; r2=$?
    report "$label" "$r1" "$r2"
}
t16_case() { # clean --all 非项目目录 T16 状态钉:输出逐字节 + build 留/缓存净 双面
    label=$1
    rm -rf "$TK"
    tk_side "$TK/sh" tk_prep_nonproj
    (cd "$TK/sh" && CTRON_DRV_ROOT="$TK/sh" sh "$SHREF" clean --all) > "$T/sh.out" 2>&1; r1=$?
    s1c=$([ ! -d "$TK/sh/.cache/emit" ] && echo y || echo n)
    s1b=$([ -d "$TK/sh/build" ] && echo y || echo n)
    rm -rf "$TK"
    tk_side "$TK/nat" tk_prep_nonproj
    (cd "$TK/nat" && CTRON_DRV_ROOT="$TK/nat" "$NAT" clean --all) > "$T/nat.out" 2>&1; r2=$?
    s2c=$([ ! -d "$TK/nat/.cache/emit" ] && echo y || echo n)
    s2b=$([ -d "$TK/nat/build" ] && echo y || echo n)
    if [ "$r1" = "$r2" ] && cmp -s "$T/sh.out" "$T/nat.out" \
       && [ "$s1c$s1b$s2c$s2b" = "yyyy" ]; then
        pass=$((pass+1)); echo "  ok  : $label(rc=$r2;逐字节+rc+T16 状态钉 一致)"
    else
        fail=$((fail+1))
        echo "  FAIL: $label(sh_rc=$r1 native_rc=$r2 状态=$s1c$s1b$s2c$s2b)"
        diff "$T/sh.out" "$T/nat.out" 2>&1 | sed 's/^/    /' | sed -n '1,10p'
    fi
}
tk_prep_cwd() { # task.cwd 正例件:sub/ 目录 + 用 $PWD 点名的脚本
    mkdir -p "$1/sub"
    printf '#!/bin/sh\nprintf "cwd-ok pwd=%%s\\n" "$(basename "$PWD")"\n' > "$1/sub/go.sh"
    tk_mf "$1"
}
tk_prep_caches() { # clean --all 项目面:三域中两域在场 + 项目 build/pkgs(Ctron.ctcl 在场)
    mkdir -p "$1/.cache/emit/x" "$1/.cache/wasm" "$1/build" "$1/pkgs"
    printf 'pkg {\n    manifest_version = 1\n    name = "tkp"\n    version = "0.1.0"\n}\n' > "$1/Ctron.ctcl"
}
tk_prep_nonproj() { # T16 件:非项目目录(build 留)+ 一缓存域
    mkdir -p "$1/.cache/emit" "$1/build"
}
tk_prep_pkg() { # gate 内建回落件:项目模式(Ctron.ctcl + fmt 规范形 src/main.ct;无 ctron.ctcl)
    mkdir -p "$1/src"
    printf 'pkg {\n    manifest_version = 1\n    name = "tkp"\n    version = "0.1.0"\n}\n' > "$1/Ctron.ctcl"
    printf 'fn main() {\n    println("hi")\n}\n' > "$1/src/main.ct"
}

echo "== task/gate/clean 臂(W4 波2 批2:本地 CTCL 读取+runner)=="
tk_case "task 列表(名\\tdesc 全表)"                      "" task
tk_case "task 正例多步(进度行+完成)"                     "" task hello
tk_case "task 未知任务(rc2 判词)"                        "" task nosuch
tk_case "task -f E5044 未知块(行号逐字)"                 "" task -f bad/e5044.ctcl a
tk_case "task -f E5054 空 steps"                         "" task -f bad/e5054.ctcl a
tk_case "task -f E5047 缺 ctron 块(END 路径)"            "" task -f bad/e5047.ctcl a
tk_case "task -f E5050 缺 config_version(END 路径)"      "" task -f bad/e5050.ctcl a
tk_case "task -f 块未闭合(END 路径)"                     "" task -f bad/unclosed.ctcl a
tk_case "task -f E5045 重复键"                           "" task -f bad/dupkey.ctcl a
tk_case "task 元字符拒(两条出路行)"                       "" task -f bad/meta.ctcl a
tk_case "task 引号转义双层(CTCL+切分)"                    "" task quotes
tk_case "task fail-stop(rc 透传+跳后续)"                 "" task failstop
tk_case "task.cwd 正例(spawn cwd+PWD 导出)"              tk_prep_cwd task cwdr
tk_case "task.cwd 缺目录(rc2 fail-closed)"               "" task badcwd
tk_case "clean 默认(在场域逐行;缺席静默)"                 tk_prep_caches clean
tk_case "clean --all(项目域 build/pkgs 加清)"             tk_prep_caches clean --all
tk_case "clean 未知旗标(rc2)"                            "" clean --nope
tk_case "gate 表路径(默认名走表,无横幅)"                  "" gate
tk_case "gate 内建回落横幅(项目模式 build+fmt)"           tk_prep_pkg gate
tk_case "gate nosuch(表内无 → rc2 未知任务)"              "" gate nosuch
t16_case "clean --all 非项目守卫(build 留+缓存净)"

# ---- new/add/publish/lock 臂(W4 波3 批1:T49 面)----
# 同路径基纪律(防打印路径分叉,同 wave1 批3 tree_case):单沙盒 $PUB 两侧各
# 重建现造现清 —— sh 侧 prep+语料跑后存档树,重建同态沙盒,nat 侧重跑,diff -r
# 全树逐字节。CTRON_REGPATH=reg(语料 cwd 相对;子例 cwd 用 ../reg 同落沙盒根)
# — ctron-dep "已写回" 行嵌 $PWD 绝对路径,同路径基即逐字。registry 三版造法:
# publish 正例腿走真发布链(pub_fx_chain,side 各自执行),其余腿 fabrication
# (manifest/module 同文,add 只读此两件;sha256 文件随件但 add 不读)。
PUB="$T/pub"
PUBT="$T/pub.tree"
PUB_VER="" # pub_fx_probe 的版本参数(空 = 1.0.0)
pub_w() { # $1=内容 $2=路径:printf 同文包装
    printf '%s' "$1" > "$2"
}
pub_fx_probe() { # $1=side:probe 包夹具(version 取 PUB_VER,缺省 1.0.0)
    local side=$1 v=${PUB_VER:-1.0.0}
    mkdir -p "$PUB/probe/src"
    pub_w 'pkg {
    manifest_version = 1
    name = "probe"
    version = "'"$v"'"
}
' "$PUB/probe/Ctron.ctcl"
    pub_w 'fn probe_hi() {
    println("probe-'"$v"'")
}
' "$PUB/probe/src/main.ct"
}
pub_fx_reg3() { # $1=side:fabricated registry 三版(module/manifest 同 publish 同文;sha256 真摘要)
    local side=$1 v
    for v in 1.0.0 1.1.0 2.0.0; do
        mkdir -p "$PUB/reg/probe/$v"
        pub_w 'pkg {
    manifest_version = 1
    name = "probe"
    version = "'"$v"'"
}
' "$PUB/reg/probe/$v/manifest"
        pub_w 'fn probe_hi() {
    println("probe-'"$v"'")
}
' "$PUB/reg/probe/$v/module"
        shasum -a 256 "$PUB/reg/probe/$v/module" | cut -d' ' -f1 > "$PUB/reg/probe/$v/sha256"
    done
}
pub_fx_dupreg() { # $1=side:probe 夹具 + 1.0.0 已占位(重复版本例;语料零写面)
    local side=$1
    pub_fx_probe "$side"
    mkdir -p "$PUB/reg/probe/1.0.0"
    cp "$PUB/probe/Ctron.ctcl" "$PUB/reg/probe/1.0.0/manifest"
    cp "$PUB/probe/src/main.ct" "$PUB/reg/probe/1.0.0/module"
    shasum -a 256 "$PUB/reg/probe/1.0.0/module" | cut -d' ' -f1 > "$PUB/reg/probe/1.0.0/sha256"
}
pub_fx_app_mf() { # $1=side:裸 app 清单(new 同文;name=app)—— 目录建到 app/
    mkdir -p "$PUB/app"
    pub_w 'pkg {
    manifest_version = 1
    name = "app"
    version = "0.1.0"
}
' "$PUB/app/Ctron.ctcl"
}
pub_fx_chain() { # $1=side:真发布链 1.0.0→1.1.0→2.0.0(side 各自执行)+ 裸 app;语料 = 全链 add
    local side=$1 exe
    case $side in
        sh) exe="sh $SHREF" ;;
        *) exe="$NAT" ;;
    esac
    PUB_VER=1.0.0 pub_fx_probe "$side"
    (cd "$PUB/probe" && CTRON_REGPATH=../reg $exe publish) > /dev/null 2>&1
    PUB_VER=1.1.0 pub_fx_probe "$side"
    (cd "$PUB/probe" && CTRON_REGPATH=../reg $exe publish) > /dev/null 2>&1
    PUB_VER=2.0.0 pub_fx_probe "$side"
    (cd "$PUB/probe" && CTRON_REGPATH=../reg $exe publish) > /dev/null 2>&1
    PUB_VER=""
    pub_fx_app_mf "$side"
}
pub_fx_app() { # $1=side:registry 三版 + 裸 app 清单
    local side=$1
    pub_fx_reg3 "$side"
    pub_fx_app_mf "$side"
}
pub_fx_app_dep() { # $1=side:app 已装态(dep 块 + pkgs/probe(2.0.0)+ Ctron.lock 缺省重解析)
    local side=$1
    pub_fx_app "$side"
    printf '
dep "probe" {
    version = "2.0.0"
}
' >> "$PUB/app/Ctron.ctcl"
    mkdir -p "$PUB/app/pkgs/probe"
    cp "$PUB/reg/probe/2.0.0/module" "$PUB/app/pkgs/probe/probe.ct"
    cp "$PUB/reg/probe/2.0.0/manifest" "$PUB/app/pkgs/probe/Ctron.ctcl"
}
pub_run() { # $1=side(sh|nat) $2=子目录(""=根),其余 = 语料(捕获至 sh/nat.out)
    local side=$1 sub=$2; shift 2
    if [ -n "$sub" ]; then
        if [ "$side" = sh ]; then
            (cd "$PUB/$sub" && CTRON_REGPATH=../reg sh "$SHREF" "$@") > "$T/sh.out" 2>&1
        else
            (cd "$PUB/$sub" && CTRON_REGPATH=../reg "$NAT" "$@") > "$T/nat.out" 2>&1
        fi
    else
        if [ "$side" = sh ]; then
            (cd "$PUB" && CTRON_REGPATH=reg sh "$SHREF" "$@") > "$T/sh.out" 2>&1
        else
            (cd "$PUB" && CTRON_REGPATH=reg "$NAT" "$@") > "$T/nat.out" 2>&1
        fi
    fi
}
pub_case() { # $1=prep 名(空=无) $2=子目录(""=根) $3=label,其余 = 语料
    local prep=$1 sub=$2 label=$3; shift 3
    rm -rf "$PUB"; mkdir -p "$PUB"
    [ -n "$prep" ] && "$prep" sh
    pub_run sh "$sub" "$@"; r1=$?
    rm -rf "$PUBT"; cp -R "$PUB" "$PUBT"
    rm -rf "$PUB"; mkdir -p "$PUB"
    [ -n "$prep" ] && "$prep" nat
    pub_run nat "$sub" "$@"; r2=$?
    if [ "$r1" = "$r2" ] && cmp -s "$T/sh.out" "$T/nat.out" && diff -r "$PUBT" "$PUB" > "$T/tree.diff" 2>&1; then
        pass=$((pass+1)); echo "  ok  : $label(rc=$r2;逐字节+rc+沙盒树 一致)"
    else
        fail=$((fail+1))
        echo "  FAIL: $label(sh_rc=$r1 native_rc=$r2)"
        diff "$T/sh.out" "$T/nat.out" 2>&1 | sed 's/^/    /' | sed -n '1,10p'
        sed 's/^/    tree: /' "$T/tree.diff" 2>/dev/null | sed -n '1,10p'
    fi
}

echo "== new/add/publish/lock 臂(W4 波3 批1:registry 协议,同路径基每侧现造现清)=="
pub_case ""    ""     "new 缺参(rc2)"                          new
pub_case ""    ""     "new 正例(app/)"                          new app
pub_case ""    ""     "new 脚手架产物树(diff -r:清单+hello)"        new app
pub_case ""    ""     "publish 缺清单(rc2)"                      publish
pub_case ""    ""     "add 缺清单(rc2)"                         add probe
pub_case ""    ""     "lock 缺清单(rc2)"                        lock
pub_case ""    ""     "add 名非法(rc2;首字符数字)"                 add 7bad
pub_case ""    ""     "add 名非法(rc2;越界字符)"                   add "pr obe"
pub_case ""    ""     "add 约束非法(rc2;越界字符)"                 add "probe@1.0!"
PUB_VER="" pub_case pub_fx_probe probe "publish 正例(三件落+sha256 文件逐字)" publish
pub_case pub_fx_dupreg probe "publish 重复版本(rc2;版本不可覆盖)"       publish
PUB_VER=1.1.0 pub_case pub_fx_probe probe "publish 第二版 1.1.0"        publish
PUB_VER=2.0.0 pub_case pub_fx_probe probe "publish 第三版 2.0.0"        publish
PUB_VER=1.0   pub_case pub_fx_probe probe "publish 非 semver(rc2)"      publish
pub_case pub_fx_app    app  "add registry 无包(rc2)"                 add nosuchpkg
pub_case pub_fx_app    app  "add 无满足约束(rc2;前缀 9)"               add "probe@9"
pub_case pub_fx_chain  app  "add 正例无约束(publish 链造 registry;取最大)" add probe
pub_case pub_fx_app_dep app "add 重复 dep(stderr 跳过行+产树)"          add probe
pub_case pub_fx_app    app  "add 前缀约束 @1(取 1.1.0)"               add "probe@1"
pub_case pub_fx_app    app  "add 精确约束 @1.0.0(两点=精确)"           add "probe@1.0.0"
pub_case pub_fx_app_dep app "lock 正例(rc0)"                        lock
PUB_VER=""
# lock 缺 ctron-dep 场:两侧 exe/脚本各拷异位同布局目录(BIN=同目录无 ctron-dep)→ die2 逐字
NOD="$T/nod"
rm -rf "$NOD"; mkdir -p "$NOD/sh/w" "$NOD/nat/w"
cp "$SHREF" "$NOD/sh/w/ctron"; chmod +x "$NOD/sh/w/ctron"
cp "$NAT" "$NOD/nat/w/ctron"; chmod +x "$NOD/nat/w/ctron"
for side in sh nat; do
    printf 'pkg {
    manifest_version = 1
    name = "w"
    version = "0.1.0"
}
' > "$NOD/$side/w/Ctron.ctcl"
done
(cd "$NOD/sh/w" && sh ./ctron lock) > "$T/sh.out" 2>&1; r1=$?
(cd "$NOD/nat/w" && ./ctron lock) > "$T/nat.out" 2>&1; r2=$?
report "lock 缺 ctron-dep(异位 exe,die2 rc2)" "$r1" "$r2"

# ---- bench/pkg seal/attest/log 臂(W4 波3 批2)----
# bench:注册表清单+未注册 die2 入册;族 spawn = 真基准计时面(ns/op 逐行非确定)
# 字节不可对拍,登记暂缓不入册(sh cmd_bench :386-411 实行为 oracle,在案)。
# seal/attest/log:同路径基沙盒($ART/$LOGD 每侧现造现清,同 wave3 批1 pub_case
# 纪律)——self_digest/commit 短哈希逐字面对拍;log 侧 git 日期钉死
# (GIT_AUTHOR/COMMITTER_DATE,两侧同值 → 同树同哈希,commit 短哈希才可对拍),
# 记录树 diff -r(attest.ctcl 内容寻址字节;.git 不入 diff)。
run_case "bench 无参(注册表清单 rc0)"  bench
run_case "bench 未注册族(rc2 逐字)"    bench nosuch

SLFX="$T/slfx"
ART="$T/art"
LOGD="$T/plog"
LOG_DATE="@1700000000 +0000"
mk_slfx() { # seal 夹具:无 main 模块(provider 面;现造现清)
    rm -rf "$SLFX"
    mkdir -p "$SLFX"
    printf 'fn seal_add(x: I32, y: I32) -> I32 {\n    return x + y\n}\n' > "$SLFX/thing.ct"
}
seal_art() { # $1=side:夹具现造 + seal 至 $ART(同路径基;stdout 丢弃,验证由语料例担)
    mk_slfx
    rm -rf "$ART"
    if [ "$1" = sh ]; then
        sh "$SHREF" pkg seal "$SLFX/thing.ct" --out "$ART" > /dev/null 2>&1
    else
        "$NAT" pkg seal "$SLFX/thing.ct" --out "$ART" > /dev/null 2>&1
    fi
}
tree_ok() { # $1=label $2=sh_rc $3=nat_rc + 树比对面($T/sh.tree ←→ $ART):产物树例合流
    if [ "$2" = "$3" ] && cmp -s "$T/sh.out" "$T/nat.out" && diff -r "$T/sh.tree" "$ART" > "$T/tree.diff" 2>&1; then
        pass=$((pass+1)); echo "  ok  : $1(rc=$3;逐字节+rc+产物树 一致)"
    else
        fail=$((fail+1))
        echo "  FAIL: $1(sh_rc=$2 native_rc=$3)"
        diff "$T/sh.out" "$T/nat.out" 2>&1 | sed 's/^/    /' | sed -n '1,10p'
        sed 's/^/    tree: /' "$T/tree.diff" 2>/dev/null | sed -n '1,10p'
    fi
}
seal_case() { # $1=label,其余 = pkg seal 语料:stdout+rc+产物树(SHA256SUMS/meta 字节即入树)
    label=$1; shift
    mk_slfx; rm -rf "$ART"
    sh "$SHREF" pkg seal "$@" > "$T/sh.out" 2>&1; r1=$?
    rm -rf "$T/sh.tree"; [ -d "$ART" ] && cp -R "$ART" "$T/sh.tree"
    mk_slfx; rm -rf "$ART"
    "$NAT" pkg seal "$@" > "$T/nat.out" 2>&1; r2=$?
    tree_ok "$label" "$r1" "$r2"
}
mut_missing() { rm "$ART/impl/thing.ast"; }
mut_tamper()  { printf 'X' >> "$ART/impl/thing.ast"; }
mut_escape()  { printf '%064d  ../evil\n' 0 >> "$ART/SHA256SUMS"; }
verify_case() { # $1=label $2=mutator(空 = 好工件):seal 后改损,verify stdout+rc(只读例)
    label=$1; mut=$2; shift 2
    seal_art sh; [ -n "$mut" ] && "$mut"
    sh "$SHREF" pkg verify "$@" > "$T/sh.out" 2>&1; r1=$?
    seal_art nat; [ -n "$mut" ] && "$mut"
    "$NAT" pkg verify "$@" > "$T/nat.out" 2>&1; r2=$?
    report "$label" "$r1" "$r2"
}
attest_case() { # $1=label,其余 = pkg attest 语料:seal 后 attest(attest.ctcl 字节入树 diff)
    label=$1; shift
    seal_art sh
    sh "$SHREF" pkg attest "$@" > "$T/sh.out" 2>&1; r1=$?
    rm -rf "$T/sh.tree"; cp -R "$ART" "$T/sh.tree"
    seal_art nat
    "$NAT" pkg attest "$@" > "$T/nat.out" 2>&1; r2=$?
    tree_ok "$label" "$r1" "$r2"
}
log_art() { # $1=side:log 前置链 seal+attest(输出丢弃)
    seal_art "$1"
    if [ "$1" = sh ]; then
        sh "$SHREF" pkg attest "$ART" > /dev/null 2>&1
    else
        "$NAT" pkg attest "$ART" > /dev/null 2>&1
    fi
}
log_case() { # $1=label $2=rep(语料重复,幂等例=2) $3=prep(1 = 先静默 append),其余 = 语料
    label=$1; rep=$2; prep=$3; shift 3
    rm -rf "$LOGD"
    log_art sh
    [ "$prep" = 1 ] && GIT_AUTHOR_DATE="$LOG_DATE" GIT_COMMITTER_DATE="$LOG_DATE" \
        sh "$SHREF" pkg log append "$LOGD" "$ART" > /dev/null 2>&1
    : > "$T/sh.out"
    i=1; while [ "$i" -le "$rep" ]; do
        GIT_AUTHOR_DATE="$LOG_DATE" GIT_COMMITTER_DATE="$LOG_DATE" \
            sh "$SHREF" pkg log "$@" >> "$T/sh.out" 2>&1
        i=$((i+1))
    done; r1=$?
    rm -rf "$T/sh.tree"; [ -d "$LOGD/records" ] && cp -R "$LOGD/records" "$T/sh.tree"
    rm -rf "$LOGD"
    log_art nat
    [ "$prep" = 1 ] && GIT_AUTHOR_DATE="$LOG_DATE" GIT_COMMITTER_DATE="$LOG_DATE" \
        "$NAT" pkg log append "$LOGD" "$ART" > /dev/null 2>&1
    : > "$T/nat.out"
    i=1; while [ "$i" -le "$rep" ]; do
        GIT_AUTHOR_DATE="$LOG_DATE" GIT_COMMITTER_DATE="$LOG_DATE" \
            "$NAT" pkg log "$@" >> "$T/nat.out" 2>&1
        i=$((i+1))
    done; r2=$?
    # 记录树例:diff 面 = records 子树(内容寻址 attest.ctcl;.git 不入比)
    if [ "$r1" = "$r2" ] && cmp -s "$T/sh.out" "$T/nat.out" && diff -r "$T/sh.tree" "$LOGD/records" > "$T/tree.diff" 2>&1; then
        pass=$((pass+1)); echo "  ok  : $label(rc=$r2;逐字节+rc+记录树 一致)"
    else
        fail=$((fail+1))
        echo "  FAIL: $label(sh_rc=$r1 native_rc=$r2)"
        diff "$T/sh.out" "$T/nat.out" 2>&1 | sed 's/^/    /' | sed -n '1,10p'
        sed 's/^/    tree: /' "$T/tree.diff" 2>/dev/null | sed -n '1,10p'
    fi
}

echo "== bench/pkg seal/attest/log 臂(W4 波3 批2:同路径基沙盒,git 日期钉)=="
seal_case "pkg seal 正例(产物树 diff -r:SHA256SUMS+meta self_digest 字节)" "$SLFX/thing.ct" --out "$ART"
run_case  "pkg seal 源含 main(rc2)"                       pkg seal tests/drv_parity/fixtures/hello.ct
run_case  "pkg seal 缺参(rc2)"                            pkg seal
verify_case "pkg verify 好工件(1 成员+self_digest)"  ""          "$ART"
verify_case "pkg verify 成员缺失(fail-closed)"       mut_missing "$ART"
verify_case "pkg verify 成员摘要篡改"                 mut_tamper  "$ART"
verify_case "pkg verify 路径越界行(self_digest 连坐)"  mut_escape  "$ART"
attest_case "pkg attest 生成(attest.ctcl 落盘+树)"        "$ART"
log_case "pkg log append(git 日期钉,commit 哈希+记录树)" 1 0 append "$LOGD" "$ART"
log_case "pkg log append 幂等(已存在行)"                  2 0 append "$LOGD" "$ART"
log_case "pkg log query 名腿(toolchain/trace_count)"     1 1 query "$LOGD" thing
log_case "pkg log query 全表(名清单)"                     1 1 query "$LOGD"
# pkg trace:分派臂 rc2 诚实暂缓(批C 复放腿同批;语料不入,同 --deep 复放腿账)

echo "== drv_parity: $pass ok / $fail fail =="
[ "$fail" -eq 0 ] || exit 1
