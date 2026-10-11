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

echo "== drv_parity: $pass ok / $fail fail =="
[ "$fail" -eq 0 ] || exit 1
