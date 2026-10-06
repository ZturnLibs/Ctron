#!/bin/sh
# ctron_smoke.sh —— ctron 驱动冒烟(ci.sh [5/9])
#
# 固化 Task 7 的端到端验证序列,断言集:
#   1) --version rc=0 有输出
#   2) --help rc=0 含 run/build/check 三词
#   3) ctron run --help rc=0(五臂守卫抽查一臂)
#   4) new → run → check → build 单文件 → build 项目模式全链(各步 rc 与产物)
#   5) 未知子命令 rc=2
#   6) 无 cc 路径:CC 指向不存在的绝对路径 + PATH 收窄,build rc=2 且 .c
#      先于预检产出且 stderr 含双出路指引("安装编译器");同环境 run rc=0
#      (隔离锁在 build 路径,解释臂不受 cc 影响)
#   7) 分发三内建 fx_distinfo(native/dev 布局面):exe:nonempty/env:0/flag:0
#   8) stdpath 两态:①CTRON_STDPATH 指路 → 1;缺省走③回落 → 2
#      (①态需 env_get 实值面,仅 native 可观察;seed 面归 ci.sh 既有链路)
#   9) 装机布局仿真(库根布局 2026-09-30):②exe/../lib 探测/T2 随发门面/site 同级推导,零 env
#  10) T43 lint:干净件归零/警告只汇总不红/--strict 红/错误红/--trusted 信任审计
#      透传/pkg 目录展开(跨文件 use 解析+多文件头)/未知旗标 rc=2
#  11) T43 bench:无族名注册表/未注册族 fail-closed/lang 真跑(digest+×3min)/net 缺省 SKIP
#  12) T49 add/publish/lock 实装(本地 registry 协议,零网络,隔离 CTRON_REGPATH):
#      缺清单/名非法/registry 无包 fail-closed;publish 三件落盘+版本不可覆盖;
#      add 装 dep 块+pkgs/+lock;lock 二跑逐字节稳定(内容寻址)
# 前置:仓库根 ctron + compiler/bin/ctron-{cc,chk,emit}
#       (ci.sh [3/9] native.sh 产出;dev 回落由 ctron 内建,本脚本不依赖 PATH)。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$DIR/../.." && pwd)
CTRON="$ROOT/ctron"
CBIN="$ROOT/compiler/bin"

[ -x "$CTRON" ] || { echo "ctron_smoke: 缺少 $CTRON" >&2; exit 2; }
[ -x "$CBIN/ctron-cc" ] || { echo "ctron_smoke: 缺少 $CBIN/ctron-cc(先跑 compiler/native.sh)" >&2; exit 2; }
[ -x "$CBIN/ctron-fmt" ] || { echo "ctron_smoke: 缺少 $CBIN/ctron-fmt(先跑 compiler/native.sh)" >&2; exit 2; }

T=$(mktemp -d /tmp/ctron_smoke.XXXXXX)
trap 'rm -rf "$T"' EXIT

pass=0; fail=0
ok()  { echo "  ok  : $1"; pass=$((pass+1)); }
bad() { echo "  FAIL: $1"; fail=$((fail+1)); }

echo "== 1) --version =="
rc=0; "$CTRON" --version > "$T/v.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && [ -s "$T/v.out" ]; then
    ok "--version rc=0 有输出($(cat "$T/v.out"))"
else
    bad "--version rc=$rc out=[$(cat "$T/v.out")]"
fi

echo "== 2) --help =="
rc=0; "$CTRON" --help > "$T/h.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q 'run' "$T/h.out" && grep -q 'build' "$T/h.out" && grep -q 'check' "$T/h.out"; then
    ok "--help rc=0 含 run/build/check"
else
    bad "--help rc=$rc 或缺命令词"
fi

echo "== 3) 子命令 --help 守卫(五臂抽查 run 臂) =="
rc=0; "$CTRON" run --help > "$T/rh.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && [ -s "$T/rh.out" ]; then
    ok "ctron run --help rc=0 出详助"
else
    bad "ctron run --help rc=$rc(应落详助而非把 --help 当文件名)"
fi

echo "== 4) 全链:new → run → check → build 单文件 → build 项目模式 =="
rc=0; ( cd "$T" && "$CTRON" new probe ) > "$T/new.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && [ ! -f "$T/probe/Ctron.toml" ] && [ -f "$T/probe/Ctron.ctcl" ] && [ -f "$T/probe/src/main.ct" ]; then
    ok "new 脚手架(Ctron.ctcl + src/main.ct;T48 硬切后不再产 .toml)"
else
    bad "new rc=$rc 或缺脚手架文件"
fi

rc=0; "$CTRON" run "$T/probe/src/main.ct" > "$T/run.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q 'hello, ctron' "$T/run.out"; then
    ok "run 解释执行 hello"
else
    bad "run rc=$rc out=[$(cat "$T/run.out")]"
fi

rc=0; "$CTRON" check "$T/probe/src/main.ct" > "$T/chk.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q 'check OK' "$T/chk.out"; then
    ok "check 静态检查绿"
else
    bad "check rc=$rc out=[$(cat "$T/chk.out")]"
fi

rc=0; "$CTRON" build "$T/probe/src/main.ct" > "$T/b1.out" 2>&1 || rc=$?
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

rc=0; ( cd "$T/probe" && "$CTRON" build ) > "$T/b2.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && [ -f "$T/probe/build/probe.c" ] && [ -x "$T/probe/build/probe" ]; then
    ok "build 项目模式(build/probe.c + build/probe,读 Ctron.ctcl)"
else
    bad "build 项目模式 rc=$rc out=[$(cat "$T/b2.out")]"
fi

# T48 fail-closed:遗留 .toml 项目 = 迁移诊断且拒绝构建(TOML 面移除)
mkdir -p "$T/legacy/src"
printf 'fn main() {\n    println("x")\n}\n' > "$T/legacy/src/main.ct"
printf '[package]\nname = "legacy"\nversion = "0.1.0"\n' > "$T/legacy/Ctron.toml"
rc=0; ( cd "$T/legacy" && "$CTRON" build ) > "$T/b3.out" 2>&1 || rc=$?
if [ $rc -ne 0 ] && grep -q "硬切 CTCL" "$T/b3.out"; then
    ok "T48 fail-closed(遗留 Ctron.toml → 迁移诊断且拒构)"
else
    bad "T48 fail-closed rc=$rc out=[$(cat "$T/b3.out")]"
fi

rc=0; "$T/probe/build/probe" > "$T/exe2.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q 'hello, ctron' "$T/exe2.out"; then
    ok "项目产物执行 hello"
else
    bad "项目产物执行 rc=$rc out=[$(cat "$T/exe2.out")]"
fi

echo "== 5) 未知子命令 =="
rc=0; "$CTRON" frobnicate > "$T/unk.out" 2>&1 || rc=$?
if [ $rc -eq 2 ]; then
    ok "未知子命令 rc=2"
else
    bad "未知子命令 rc=$rc(约定 2)"
fi

echo "== 6) 无 cc 路径(双出路)+ run 隔离性 =="
printf 'fn main() {\n    println("hello, ctron")\n}\n' > "$T/nc.ct"
rc=0; ( cd "$T" && CC=/nonexistent/cc PATH=/usr/bin:/bin "$CTRON" build nc.ct ) > "$T/nocc.out" 2> "$T/nocc.err" || rc=$?
if [ $rc -eq 2 ] && [ -f "$T/nc.c" ] && grep -q '安装编译器' "$T/nocc.err"; then
    ok "无 cc:build rc=2,nc.c 先产出,stderr 双出路指引"
else
    bad "无 cc build rc=$rc .c=$([ -f "$T/nc.c" ] && echo 有 || echo 无) err=[$(cat "$T/nocc.err")]"
fi

rc=0; ( cd "$T" && CC=/nonexistent/cc PATH=/usr/bin:/bin "$CTRON" run nc.ct ) > "$T/nocc_run.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q 'hello, ctron' "$T/nocc_run.out"; then
    ok "无 cc 同环境 run rc=0(解释臂隔离,不受 cc 影响)"
else
    bad "无 cc run rc=$rc out=[$(cat "$T/nocc_run.out")]"
fi

echo "== 7) 分发三内建:ctron run fx_distinfo(native/dev 布局面) =="
# dev 下 exe 旁无 lib → std 走③回落;fx_distinfo 不用 std,不受影响。
# seed 面(compiler/ctc.sh 宿主 env_get 恒空)不入本脚本:依赖 seed 构建,归 ci.sh 既有链路。
rc=0; "$CTRON" run "$ROOT/tests/dist/fx_distinfo.ct" > "$T/dist.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q '^exe:nonempty$' "$T/dist.out" \
    && grep -q '^env:0$' "$T/dist.out" && grep -q '^flag:0$' "$T/dist.out"; then
    ok "fx_distinfo 三内建:exe:nonempty/env:0/flag:0"
else
    bad "fx_distinfo rc=$rc out=[$(cat "$T/dist.out")]"
fi

echo "== 8) stdpath 两态:①CTRON_STDPATH 指路 / 缺省③回落 =="
rc=0; CTRON_STDPATH="$ROOT/tests/modules/stdpath/fakestd" "$CTRON" run "$ROOT/tests/modules/stdpath/src/main.ct" > "$T/sp1.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q '^1$' "$T/sp1.out"; then
    ok "CTRON_STDPATH=fakestd 命中①(不分词):输出 1"
else
    bad "stdpath ①态 rc=$rc out=[$(cat "$T/sp1.out")]"
fi

rc=0; "$CTRON" run "$ROOT/tests/modules/stdpath/src/main.ct" > "$T/sp2.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q '^2$' "$T/sp2.out"; then
    ok "缺省走③回落(真分词):输出 2"
else
    bad "stdpath 缺省态 rc=$rc out=[$(cat "$T/sp2.out")]"
fi

echo "== 9) fmt 契约(R-P2d:file/pkg 目录/-w/--check/负例) =="
printf 'fn main() {\nlet x=1\n}\n' > "$T/fmt_a.ct"
rc=0; "$CTRON" fmt "$T/fmt_a.ct" > "$T/fmt_d.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && printf 'fn main() {\n    let x = 1\n}\n' | diff - "$T/fmt_d.out" > /dev/null 2>&1; then
    ok "fmt 默认打印规范格式"
else
    bad "fmt 默认打印 rc=$rc out=[$(cat "$T/fmt_d.out")]"
fi

rc=0; "$CTRON" fmt "$T/fmt_a.ct" --check > "$T/fmt_c.out" 2> "$T/fmt_c.err" || rc=$?
if [ $rc -eq 1 ] && grep -q "$T/fmt_a.ct" "$T/fmt_c.out" && grep -q '待格式化' "$T/fmt_c.err"; then
    ok "fmt --check 列出待格式化 + rc=1"
else
    bad "fmt --check rc=$rc out=[$(cat "$T/fmt_c.out")] err=[$(cat "$T/fmt_c.err")]"
fi

rc=0; "$CTRON" fmt "$T/fmt_a.ct" -w > "$T/fmt_w.out" 2>&1 || rc=$?
rc2=0; "$CTRON" fmt "$T/fmt_a.ct" --check > /dev/null 2>&1 || rc2=$?
if [ $rc -eq 0 ] && [ $rc2 -eq 0 ] && grep -q 'let x = 1' "$T/fmt_a.ct"; then
    ok "fmt -w 原位写回,写后 --check rc=0"
else
    bad "fmt -w rc=$rc 写后 check rc=$rc2"
fi

mkdir -p "$T/fmt_pkg/src"
printf 'fn a() {}\n' > "$T/fmt_pkg/src/lib.ct"
printf 'fn m() {\nlet y=2\n}\n' > "$T/fmt_pkg/src/main.ct"
rc=0; "$CTRON" fmt "$T/fmt_pkg" --check > "$T/fmt_p.out" 2>&1 || rc=$?
if [ $rc -eq 1 ] && grep -q 'src/main.ct' "$T/fmt_p.out" && ! grep -q 'src/lib.ct' "$T/fmt_p.out"; then
    ok "fmt pkg 目录(src/*.ct 展开,已格式化文件不列)"
else
    bad "fmt pkg --check rc=$rc out=[$(cat "$T/fmt_p.out")]"
fi

printf 'fn main() {\nvar x = 1;\n}\n' > "$T/fmt_neg.ct"
rc=0; "$CTRON" fmt "$T/fmt_neg.ct" > "$T/fmt_n.out" 2>&1 || rc=$?
if [ $rc -eq 1 ] && grep -q 'E1001' "$T/fmt_n.out"; then
    ok "fmt 词法脏报错退出(规范 R8, rc=1)"
else
    bad "fmt 词法脏 rc=$rc out=[$(cat "$T/fmt_n.out")]"
fi

echo "== 9) 装机布局仿真(库根布局 2026-09-30:②exe/../lib 探测 + T2 随发 + site 同级推导)=="
IN="$T/inst"
mkdir -p "$IN/bin" "$IN/pkgs/zsite"
cp "$CBIN/ctron-cc" "$IN/bin/ctron-cc"
cp -R "$ROOT/lib/." "$IN/lib/"
printf 'use std.str.{contains}\nfn main() { if contains("abc", "b") { println("std-hit") } }\n' > "$T/i_std.ct"
rc=0; "$IN/bin/ctron-cc" run "$T/i_std.ct" > "$T/i_std.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q "std-hit" "$T/i_std.out"; then
    ok "装机态 std 命中(②探测 exe/../lib/std,零 env)"
else
    bad "装机态 std 异常 rc=$rc: $(tail -2 "$T/i_std.out")"
fi
printf 'use net.{Net_probe}\nfn main() { println("net-merged") }\n' > "$T/i_net.ct"
rc=0; "$IN/bin/ctron-cc" run "$T/i_net.ct" > "$T/i_net.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q "net-merged" "$T/i_net.out"; then
    ok "装机态 T2 域命中(net 门面,tarball 随发面)"
else
    bad "装机态 net 异常 rc=$rc: $(tail -2 "$T/i_net.out")"
fi
printf 'pub fn hi() -> Str { return "site-hit" }\n' > "$IN/pkgs/zsite/core.ct"
printf 'use zsite.core.{hi}\nfn main() { println(hi()) }\n' > "$T/i_site.ct"
rc=0; "$IN/bin/ctron-cc" run "$T/i_site.ct" > "$T/i_site.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q "site-hit" "$T/i_site.out"; then
    ok "装机态 site 推导命中(工具链根/pkgs 同级,零 env)"
else
    bad "装机态 site 异常 rc=$rc: $(tail -2 "$T/i_site.out")"
fi

echo "== 10) T43 lint(check 命令化+汇总:警告不红/错误红/--strict/--trusted/pkg 目录) =="
printf 'fn main() {\n    println("x")\n}\n' > "$T/lint_ok.ct"
rc=0; "$CTRON" lint "$T/lint_ok.ct" > "$T/l1.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q '0 个错误 / 0 警告' "$T/l1.out"; then
    ok "lint 干净件 rc=0 汇总归零"
else
    bad "lint 干净件 rc=$rc out=[$(cat "$T/l1.out")]"
fi

printf 'fn main() {\n    var unused1 = 7\n    println("x")\n}\n' > "$T/lint_w.ct"
rc=0; "$CTRON" lint "$T/lint_w.ct" > "$T/l2.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q 'W8030' "$T/l2.out" && grep -q '1 警告' "$T/l2.out"; then
    ok "lint 警告只汇总不红(常规 lint 口径;check 同件 rc=1 二面并存)"
else
    bad "lint 警告件 rc=$rc out=[$(cat "$T/l2.out")]"
fi

rc=0; "$CTRON" lint "$T/lint_w.ct" --strict > "$T/l3.out" 2>&1 || rc=$?
if [ $rc -eq 1 ] && grep -q '1 警告' "$T/l3.out"; then
    ok "lint --strict 警告也红(rc=1)"
else
    bad "lint --strict rc=$rc out=[$(cat "$T/l3.out")]"
fi

printf 'fn main() {\n    let x: I32 = "s"\n}\n' > "$T/lint_e.ct"
rc=0; "$CTRON" lint "$T/lint_e.ct" > "$T/l4.out" 2>&1 || rc=$?
if [ $rc -eq 1 ] && grep -q 'E2010' "$T/l4.out" && grep -q '1 个错误' "$T/l4.out"; then
    ok "lint 错误红(rc=1,E 计数)"
else
    bad "lint 错误件 rc=$rc out=[$(cat "$T/l4.out")]"
fi

rc=0; "$CTRON" lint "$ROOT/tests/modules/ffi_math/src/main.ct" --trusted > "$T/l5.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q 'trusted: ctron_add@' "$T/l5.out"; then
    ok "lint --trusted 信任审计枚举透传(§9.6)"
else
    bad "lint --trusted rc=$rc out=[$(cat "$T/l5.out")]"
fi

mkdir -p "$T/lint_pkg/src"
printf 'pub fn twice(x: I32) -> I32 { return x * 2 }\n' > "$T/lint_pkg/src/lib.ct"
printf 'use lib.{twice}\nfn main() {\n    var u = 1\n    println(twice(21))\n}\n' > "$T/lint_pkg/src/main.ct"
rc=0; "$CTRON" lint "$T/lint_pkg" > "$T/l6.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q '2 文件' "$T/l6.out" && grep -q 'W8030' "$T/l6.out" && grep -q '^-- ' "$T/l6.out"; then
    ok "lint pkg 目录(src/*.ct 展开+跨文件 use 解析+多文件头)"
else
    bad "lint pkg rc=$rc out=[$(cat "$T/l6.out")]"
fi

rc=0; "$CTRON" lint "$T/lint_ok.ct" --bogus > "$T/l7.out" 2>&1 || rc=$?
if [ $rc -eq 2 ]; then
    ok "lint 未知旗标 rc=2"
else
    bad "lint 未知旗标 rc=$rc(约定 2)"
fi

echo "== 11) T43 bench(族注册表+dev 脚本入口+fail-closed) =="
rc=0; "$CTRON" bench > "$T/b1.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q 'lang' "$T/b1.out" && grep -q 'ffi' "$T/b1.out"; then
    ok "bench 无族名列注册表"
else
    bad "bench 注册表 rc=$rc out=[$(cat "$T/b1.out")]"
fi

rc=0; "$CTRON" bench frobnicate > "$T/b2.out" 2>&1 || rc=$?
if [ $rc -eq 2 ]; then
    ok "bench 未注册族 fail-closed rc=2"
else
    bad "bench 未注册族 rc=$rc(约定 2)"
fi

rc=0; "$CTRON" bench lang > "$T/b3.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q '比值' "$T/b3.out"; then
    ok "bench lang 真跑(digest pin+×3 取 min): $(grep '比值' "$T/b3.out")"
else
    bad "bench lang rc=$rc out=[$(cat "$T/b3.out")]"
fi

rc=0; "$CTRON" bench net > "$T/b4.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q 'SKIP' "$T/b4.out"; then
    ok "bench net 缺省 SKIP rc=0(env 门禁惯例透传)"
else
    bad "bench net rc=$rc out=[$(cat "$T/b4.out")]"
fi

echo "== 12) T49 add/publish/lock 实装(本地 registry 协议,零网络,隔离 CTRON_REGPATH) =="
REGT="$T/t49reg"
mkdir -p "$T/pub_empty"
rc=0; ( cd "$T/pub_empty" && CTRON_REGPATH="$REGT" "$CTRON" publish ) > "$T/p2.out" 2>&1 || rc=$?
if [ $rc -eq 2 ] && grep -q 'Ctron.ctcl' "$T/p2.out"; then
    ok "publish 缺清单拦截 rc=2"
else
    bad "publish 缺清单 rc=$rc out=[$(cat "$T/p2.out")]"
fi

rc=0; "$CTRON" add '9bad name' > "$T/a3.out" 2>&1 || rc=$?
if [ $rc -eq 2 ] && grep -q '依赖名非法' "$T/a3.out"; then
    ok "add 依赖名非法拦截 rc=2"
else
    bad "add 名非法 rc=$rc out=[$(cat "$T/a3.out")]"
fi

rc=0; ( cd "$T/probe" && CTRON_REGPATH="$REGT" "$CTRON" add libmath9x ) > "$T/a4.out" 2>&1 || rc=$?
if [ $rc -eq 2 ] && grep -q 'registry 无包' "$T/a4.out"; then
    ok "add registry 无包 fail-closed rc=2"
else
    bad "add 无包 rc=$rc out=[$(cat "$T/a4.out")]"
fi

rc=0; ( cd "$T/probe" && CTRON_REGPATH="$REGT" "$CTRON" publish ) > "$T/p1.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && [ -f "$REGT/probe/0.1.0/sha256" ] && [ -f "$REGT/probe/0.1.0/module" ] && [ -f "$REGT/probe/0.1.0/manifest" ]; then
    ok "publish 三件落盘(manifest+module+sha256)"
else
    bad "publish rc=$rc out=[$(cat "$T/p1.out")]"
fi

rc=0; ( cd "$T/probe" && CTRON_REGPATH="$REGT" "$CTRON" publish ) > "$T/p3.out" 2>&1 || rc=$?
if [ $rc -eq 2 ] && grep -q '版本不可覆盖' "$T/p3.out"; then
    ok "二次发布被拒(不可变性)rc=2"
else
    bad "二次发布 rc=$rc out=[$(cat "$T/p3.out")]"
fi

mkdir -p "$T/t49app/src"
printf 'pkg {\n    manifest_version = 1\n    name = "t49app"\n    version = "0.1.0"\n}\n' > "$T/t49app/Ctron.ctcl"
printf 'fn main() -> I32 {\n    return 0\n}\n' > "$T/t49app/src/main.ct"
rc=0; ( cd "$T/t49app" && CTRON_REGPATH="$REGT" "$CTRON" add probe@0.1.0 ) > "$T/a5.out" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q 'dep "probe"' "$T/t49app/Ctron.ctcl" && [ -f "$T/t49app/pkgs/probe/probe.ct" ] && [ -f "$T/t49app/Ctron.lock" ]; then
    ok "add 实装(dep 块+pkgs/ 安装+lock 刷新)"
else
    bad "add 实装 rc=$rc out=[$(cat "$T/a5.out")]"
fi

cp "$T/t49app/Ctron.lock" "$T/lock1"
rc=0; ( cd "$T/t49app" && CTRON_REGPATH="$REGT" "$CTRON" lock ) > /dev/null 2>&1 || rc=$?
if [ $rc -eq 0 ] && cmp -s "$T/lock1" "$T/t49app/Ctron.lock"; then
    ok "lock 二跑稳定(内容寻址)"
else
    bad "lock 二跑 rc=$rc"
fi

echo "ctron_smoke: $pass ok / $fail fail"
[ "$fail" -eq 0 ] || exit 1

# ---- T36 build/target 后端接口(§9.7 插件化地板) ----
T36=$(mktemp -d /tmp/ct36.XXXXXX)
printf 'fn main() -> I32 {\n    println("t36-ok")\n    return 0\n}\n' > "$T36/a.ct"
if "$ROOT/compiler/ctc.sh" build "$T36/a.ct" -o "$T36/a.bin" >/dev/null 2>&1 \
   && "$T36/a.bin" run "$T36/a.ct" | grep -q "t36-ok"; then
    ok "ctc build --target native 等价链(构建→运行)"
else
    bad "ctc build --target native 失败"
fi
if "$ROOT/compiler/ctc.sh" build "$T36/a.ct" --target=wasm32-unknown-unknown >/dev/null 2>&1; then
    bad "ctc build 未注册 target 未拦截(期望 exit 2)"
else
    ok "ctc build 未注册 target 清晰诊断(exit 2)"
fi
rm -rf "$T36"
