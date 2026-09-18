#!/bin/sh
# ctc_smoke.sh —— ctc 驱动冒烟(ci.sh [5/7])
#
# 固化 Task 7 的端到端验证序列,断言集:
#   1) --version rc=0 有输出
#   2) --help rc=0 含 run/build/check 三词
#   3) ctc run --help rc=0(五臂守卫抽查一臂)
#   4) new → run → check → build 单文件 → build 项目模式全链(各步 rc 与产物)
#   5) 未知子命令 rc=2
#   6) 无 cc 路径:CC 指向不存在的绝对路径 + PATH 收窄,build rc=2 且 .c
#      先于预检产出且 stderr 含双出路指引("安装编译器");同环境 run rc=0
#      (隔离锁在 build 路径,解释臂不受 cc 影响)
# 前置:仓库根 ctc + compiler/bin/ctron-{cc,chk,emit}
#       (ci.sh [4/7] native.sh 产出;dev 回落由 ctc 内建,本脚本不依赖 PATH)。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$DIR/../.." && pwd)
CTC="$ROOT/ctc"
CBIN="$ROOT/compiler/bin"

[ -x "$CTC" ] || { echo "ctc_smoke: 缺少 $CTC" >&2; exit 2; }
[ -x "$CBIN/ctron-cc" ] || { echo "ctc_smoke: 缺少 $CBIN/ctron-cc(先跑 compiler/native.sh)" >&2; exit 2; }

T=$(mktemp -d /tmp/ctc_smoke.XXXXXX)
trap 'rm -rf "$T"' EXIT

pass=0; fail=0
ok()  { echo "  ok  : $1"; pass=$((pass+1)); }
bad() { echo "  FAIL: $1"; fail=$((fail+1)); }

echo "== 1) --version =="
rc=0; "$CTC" --version > "$T/v.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && [ -s "$T/v.out" ]; then
    ok "--version rc=0 有输出($(cat "$T/v.out"))"
else
    bad "--version rc=$rc out=[$(cat "$T/v.out")]"
fi

echo "== 2) --help =="
rc=0; "$CTC" --help > "$T/h.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q 'run' "$T/h.out" && grep -q 'build' "$T/h.out" && grep -q 'check' "$T/h.out"; then
    ok "--help rc=0 含 run/build/check"
else
    bad "--help rc=$rc 或缺命令词"
fi

echo "== 3) 子命令 --help 守卫(五臂抽查 run 臂) =="
rc=0; "$CTC" run --help > "$T/rh.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && [ -s "$T/rh.out" ]; then
    ok "ctc run --help rc=0 出详助"
else
    bad "ctc run --help rc=$rc(应落详助而非把 --help 当文件名)"
fi

echo "== 4) 全链:new → run → check → build 单文件 → build 项目模式 =="
rc=0; ( cd "$T" && "$CTC" new probe ) > "$T/new.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && [ -f "$T/probe/Ctron.toml" ] && [ -f "$T/probe/src/main.ct" ]; then
    ok "new 脚手架(Ctron.toml + src/main.ct)"
else
    bad "new rc=$rc 或缺脚手架文件"
fi

rc=0; "$CTC" run "$T/probe/src/main.ct" > "$T/run.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q 'hello, ctron' "$T/run.out"; then
    ok "run 解释执行 hello"
else
    bad "run rc=$rc out=[$(cat "$T/run.out")]"
fi

rc=0; "$CTC" check "$T/probe/src/main.ct" > "$T/chk.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q 'check OK' "$T/chk.out"; then
    ok "check 静态检查绿"
else
    bad "check rc=$rc out=[$(cat "$T/chk.out")]"
fi

rc=0; "$CTC" build "$T/probe/src/main.ct" > "$T/b1.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && [ -f "$T/probe/src/main.c" ] && [ -x "$T/probe/src/main" ]; then
    ok "build 单文件(<stem>.c + 可执行)"
else
    bad "build 单文件 rc=$rc out=[$(cat "$T/b1.out")]"
fi

rc=0; "$T/probe/src/main" > "$T/exe1.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q 'hello, ctron' "$T/exe1.out"; then
    ok "单文件产物执行 hello"
else
    bad "单文件产物执行 rc=$rc out=[$(cat "$T/exe1.out")]"
fi

rc=0; ( cd "$T/probe" && "$CTC" build ) > "$T/b2.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && [ -f "$T/probe/build/probe.c" ] && [ -x "$T/probe/build/probe" ]; then
    ok "build 项目模式(build/probe.c + build/probe,读 Ctron.toml)"
else
    bad "build 项目模式 rc=$rc out=[$(cat "$T/b2.out")]"
fi

rc=0; "$T/probe/build/probe" > "$T/exe2.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q 'hello, ctron' "$T/exe2.out"; then
    ok "项目产物执行 hello"
else
    bad "项目产物执行 rc=$rc out=[$(cat "$T/exe2.out")]"
fi

echo "== 5) 未知子命令 =="
rc=0; "$CTC" frobnicate > "$T/unk.out" 2>&1 || rc=$?
if [ $rc -eq 2 ]; then
    ok "未知子命令 rc=2"
else
    bad "未知子命令 rc=$rc(约定 2)"
fi

echo "== 6) 无 cc 路径(双出路)+ run 隔离性 =="
printf 'fn main() {\n    println("hello, ctron")\n}\n' > "$T/nc.ct"
rc=0; ( cd "$T" && CC=/nonexistent/cc PATH=/usr/bin:/bin "$CTC" build nc.ct ) > "$T/nocc.out" 2> "$T/nocc.err" || rc=$?
if [ $rc -eq 2 ] && [ -f "$T/nc.c" ] && grep -q '安装编译器' "$T/nocc.err"; then
    ok "无 cc:build rc=2,nc.c 先产出,stderr 双出路指引"
else
    bad "无 cc build rc=$rc .c=$([ -f "$T/nc.c" ] && echo 有 || echo 无) err=[$(cat "$T/nocc.err")]"
fi

rc=0; ( cd "$T" && CC=/nonexistent/cc PATH=/usr/bin:/bin "$CTC" run nc.ct ) > "$T/nocc_run.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q 'hello, ctron' "$T/nocc_run.out"; then
    ok "无 cc 同环境 run rc=0(解释臂隔离,不受 cc 影响)"
else
    bad "无 cc run rc=$rc out=[$(cat "$T/nocc_run.out")]"
fi

echo "ctc_smoke: $pass ok / $fail fail"
[ "$fail" -eq 0 ] || exit 1
