#!/bin/sh
# tests/drv_parity/run.sh —— 原生驱动器对拍门(W4 波1:骨架批;gate 21→22 新步)
# sh 参考(仓库根 ctron)vs 原生驱动器(compiler/bin/ctron)对同一语料双跑:
# stdout+stderr 合流捕获,字节级 diff(cmp) + rc 对比,逐行报告。
# gt 纪律:两实现对同一语料的输出必须逐字节(含换行)一致——首跑即抓差异逐条修,
# 修向永远是原生侧(sh 文本是 oracle)。
# 缺件 = SKIP 显式登记,门绿(镜像 bare/wasm 环境门惯例):sh 参考或原生驱动器缺席
# (后者先跑 compiler/native.sh)。
# 语料波1 = 已实现臂全集(--version|-V/--help|-h/help/help <cmd>/<cmd> --help 拦截/
# 未知子命令/无参);未实现已知名(bench/add/…的行为臂)不进语料——sh 有真行为而
# 原生为占位 rc2(诚实账),波2/3 补臂时同步补语料行。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
SHREF="$ROOT/ctron"
NAT="$ROOT/compiler/bin/ctron"
T=$(mktemp -d /tmp/ctron_drvp.XXXXXX)
trap 'rm -rf "$T"' EXIT

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
# ---- 无参(usage rc 2)----
sh "$SHREF" > "$T/sh.out" 2>&1; r1=$?
"$NAT" > "$T/nat.out" 2>&1; r2=$?
report "无参(usage rc2)" "$r1" "$r2"

echo "== drv_parity: $pass ok / $fail fail =="
[ "$fail" -eq 0 ] || exit 1
