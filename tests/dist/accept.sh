#!/bin/sh
# accept.sh —— 预编译 tar.gz 发布验收门禁(spec 工具链分发 §7 第 1-5 项的 CI 固化)
#
# 用法: sh tests/dist/accept.sh dist/ctron-<ver>-<os>-<arch>.tar.gz
# 环境: mac/linux 共用;POSIX sh;工作目录 ≠ 安装目录(全程 cwd=临时 scratch)。
#
# 断言面(失败计数,末尾非零退出):
#   1) 布局:bin 四件可执行 + lib/ctron/std(含 str.ct 探针)+ share/doc/examples + VERSION
#   2) ctc --version rc=0 且逐字 = "ctron <VERSION 文件内容>"(§7.3)
#   3) 版本与 tarball 名一致(防错拿旧包/异平台包)
#   4) std 装机路径命中(§7.2):任意 cwd 下 examples(ctwc)use std.str 命中——
#      预先删除包内 examples 自带的 vendored std,断 ③ 回落,命中只可能来自
#      ②(exe 旁 ../lib/ctron/std);命中失败响亮(E2020,§9 设计使然)
#   5) run/build 输出对拍(§7.1,任务书口径任选一件,取 ctwc):
#      解释器与构建产物对同一输入(单串入口模型下即 ctwc 自身源)输出逐字相等
#   6) 构建产物真喂样例文本,输出 = wc 口径期望行(ctwc 与 wc -l/-w/-c 同口径)
#   7) 无 cc 预检(§7.5):CC 指不存在路径 + PATH 收窄 → build rc=2 且 <stem>.c
#      先于预检产出且 stderr 含出路指引;同环境 ctc run rc=0(解释臂与 cc 隔离)
#      —— 隔离用 CC=/nonexistent/cc 法:macOS 无 CLT 时 /usr/bin/cc 是存在但必报错
#      的垫片,纯 PATH 隔离打不穿(计划 1 Task 7 实测),与 ctc_smoke.sh 同法
#   8) 负例拦截(§7.3):未解析名称经 check → rc=1 且 stderr 非空
#   9) ctc --help rc=0 含 run/build/check 三词;help build rc=0 非空
#  10) 未知子命令 rc=2 且 stderr 提示 ctc --help
#
# 已知边界(语义注明,不在本脚本断言面):W8901 整库缺失盲区——若安装损坏到
# "str.ct 探针自身失效"(pkg_std_installed 返 false),std 缺失静默回落 ③ 落
# E2020 而非 W8901 告警(parse_pkg.ct:pkg_std_installed 探测以 str.ct 为锚)。
# 本脚本不断言该形态;装机路径 std 模块级缺失(探针在、模块缺)的 W8901 亦不入
# CI 断言(需临时损坏安装面,与本脚本"只读安装面"的验收姿态冲突,语义见
# parse_pkg.ct W8901 注释)。
set -u
[ $# -ge 1 ] || { echo "用法: accept.sh <ctron-<ver>-<os>-<arch>.tar.gz>" >&2; exit 2; }
TARBALL=$1
[ -f "$TARBALL" ] || { echo "accept: tarball 不存在:$TARBALL" >&2; exit 2; }

T=$(mktemp -d /tmp/ctron_accept.XXXXXX) || exit 2
trap 'rm -rf "$T"' EXIT
W="$T/work"; mkdir -p "$W"

pass=0; fail=0
ok()  { echo "  ok  : $1"; pass=$((pass+1)); }
bad() { echo "  FAIL: $1"; fail=$((fail+1)); }

echo "== 0) 解压 =="
tar xzf "$TARBALL" -C "$T" || { echo "accept: tar 解压失败" >&2; exit 2; }
PKG="$T/ctron"
CTC="$PKG/bin/ctc"

echo "== 1) 布局(bin 四件/std/examples/VERSION)== "
if [ -x "$CTC" ] && [ -x "$PKG/bin/ctron-cc" ] && [ -x "$PKG/bin/ctron-chk" ] \
    && [ -x "$PKG/bin/ctron-emit" ] && [ -f "$PKG/lib/ctron/std/str.ct" ] \
    && [ -f "$PKG/share/doc/examples/ctwc/src/main.ct" ] && [ -f "$PKG/VERSION" ]; then
    ok "布局齐:bin 四件可执行 + std/str.ct + examples/ctwc + VERSION"
else
    bad "布局缺件(见 spec §2.1):$PKG"
fi

echo "== 2) ctc --version(§7.3)== "
rc=0; VEROUT=$( (cd "$W" && "$CTC" --version) 2>&1 ) || rc=$?
if [ $rc -eq 0 ] && [ "$VEROUT" = "ctron $(cat "$PKG/VERSION")" ]; then
    ok "--version = $VEROUT"
else
    bad "--version rc=$rc out=[$VEROUT] 期望=[ctron $(cat "$PKG/VERSION")]"
fi

echo "== 3) 版本与 tarball 名一致 =="
BASE=$(basename "$TARBALL"); BASE=${BASE%.tar.gz}
VER=""
case $BASE in ctron-*) VER=${BASE#ctron-} ;; esac
OS=$(uname -s | tr '[:upper:]' '[:lower:]'); M=$(uname -m)
case $M in arm64|aarch64) ARCH=arm64 ;; x86_64) ARCH=x86_64 ;; *) ARCH="" ;; esac
[ -n "$ARCH" ] && case $VER in *-"$OS"-"$ARCH") VER=${VER%-"$OS"-"$ARCH"} ;; esac
if [ -n "$VER" ] && printf '%s' "$VEROUT" | grep -q -- "$VER"; then
    ok "版本串含 tarball 名版本 $VER"
else
    bad "版本串 [$VEROUT] 不含 tarball 名版本 [$VER]"
fi

echo "== 4) std 装机路径命中(§7.2,cwd ≠ 安装目录,断 vendored std)== "
rm -rf "$PKG/share/doc/examples/ctwc/std"
rc=0; CTWC_RUN=$( (cd "$W" && "$CTC" run "$PKG/share/doc/examples/ctwc/src/main.ct") 2>&1 ) || rc=$?
if [ $rc -eq 0 ] && [ -n "$CTWC_RUN" ]; then
    ok "examples/ctwc 于临时 cwd 解释执行(rc=0):$CTWC_RUN"
else
    bad "ctwc run rc=$rc out=[$CTWC_RUN](vendored std 已删,失败即 ② 装机路径未命中)"
fi

echo "== 5) run/build 对拍(§7.1,单串入口同输入)== "
EXE="$PKG/share/doc/examples/ctwc/src/main"
if [ $rc -eq 0 ]; then
    (cd "$W" && "$CTC" build "$PKG/share/doc/examples/ctwc/src/main.ct") > "$T/b.log" 2>&1
    brc=$?
    rc=0; EXE_RUN=$( (cd "$W" && "$EXE" run "$PKG/share/doc/examples/ctwc/src/main.ct") 2>&1 ) || rc=$?
    if [ $brc -eq 0 ] && [ $rc -eq 0 ] && [ -n "$EXE_RUN" ] && [ "$EXE_RUN" = "$CTWC_RUN" ]; then
        ok "解释与构建产物同输入输出逐字相等:[$EXE_RUN]"
    else
        bad "对拍失配 build_rc=$brc exe_rc=$rc 解释=[$CTWC_RUN] 构建=[$EXE_RUN]"
    fi
else
    bad "对拍跳过(前置 4 失败)"
fi

echo "== 6) 构建产物喂样例文本(wc 口径)== "
printf 'hello ctron world\nhello again\n' > "$W/sample.txt"
EXP="$(wc -l < "$W/sample.txt" | tr -d ' ') $(wc -w < "$W/sample.txt" | tr -d ' ') $(wc -c < "$W/sample.txt" | tr -d ' ') $W/sample.txt"
if [ $fail -eq 0 ]; then
    rc=0; SMP_RUN=$( (cd "$W" && "$EXE" run "$W/sample.txt") 2>&1 ) || rc=$?
    if [ $rc -eq 0 ] && [ "$SMP_RUN" = "$EXP" ]; then
        ok "样例喂入 = wc 期望行:[$SMP_RUN]"
    else
        bad "样例喂入 rc=$rc out=[$SMP_RUN] 期望=[$EXP]"
    fi
else
    bad "样例喂入跳过(前置失败)"
fi

echo "== 7) 无 cc 预检(§7.5,CC=/nonexistent/cc 法)== "
printf 'fn main() {\n    println("hello, ctron")\n}\n' > "$W/nc.ct"
rc=0; NOCC_ERR=$( (cd "$W" && CC=/nonexistent/cc PATH=/usr/bin:/bin "$CTC" build nc.ct) 2>&1 >/dev/null ) || rc=$?
if [ $rc -eq 2 ] && [ -f "$W/nc.c" ] && printf '%s' "$NOCC_ERR" | grep -q '安装编译器'; then
    ok "无 cc:build rc=2,nc.c 先产出,stderr 含出路指引"
else
    bad "无 cc build rc=$rc .c=$([ -f "$W/nc.c" ] && echo 有 || echo 无) err=[$NOCC_ERR]"
fi

rc=0; NOCC_RUN=$( (cd "$W" && CC=/nonexistent/cc PATH=/usr/bin:/bin "$CTC" run nc.ct) 2>&1 ) || rc=$?
if [ $rc -eq 0 ] && printf '%s' "$NOCC_RUN" | grep -q 'hello, ctron'; then
    ok "无 cc 同环境 run rc=0(解释臂隔离)"
else
    bad "无 cc run rc=$rc out=[$NOCC_RUN]"
fi

echo "== 8) 负例拦截 rc=1(§7.3)== "
# 注:chk 诊断走 stdout(build 无 cc 指引走 stderr),此处并流捕获
printf 'fn main() {\n    println(nope)\n}\n' > "$W/neg.ct"
rc=0; NEG_ERR=$( (cd "$W" && "$CTC" check neg.ct) 2>&1 ) || rc=$?
if [ $rc -eq 1 ] && [ -n "$NEG_ERR" ]; then
    ok "负例 check rc=1,诊断非空:$(printf '%s' "$NEG_ERR" | head -1)"
else
    bad "负例 check rc=$rc err=[$NEG_ERR]"
fi

echo "== 9) --help 面(§7.3)== "
rc=0; HLP=$( (cd "$W" && "$CTC" --help) 2>&1 ) || rc=$?
if [ $rc -eq 0 ] && printf '%s' "$HLP" | grep -q 'run' && printf '%s' "$HLP" | grep -q 'build' \
    && printf '%s' "$HLP" | grep -q 'check'; then
    ok "--help rc=0 含 run/build/check"
else
    bad "--help rc=$rc 或缺命令词"
fi

rc=0; HB=$( (cd "$W" && "$CTC" help build) 2>&1 ) || rc=$?
if [ $rc -eq 0 ] && [ -n "$HB" ]; then
    ok "help build rc=0 非空"
else
    bad "help build rc=$rc out=[$HB]"
fi

echo "== 10) 未知子命令 rc=2(§7.3)== "
rc=0; UNK=$( (cd "$W" && "$CTC" frobnicate) 2>&1 >/dev/null ) || rc=$?
if [ $rc -eq 2 ] && printf '%s' "$UNK" | grep -q 'ctc --help'; then
    ok "未知子命令 rc=2 且提示 ctc --help"
else
    bad "未知子命令 rc=$rc err=[$UNK]"
fi

echo "accept: $pass ok / $fail fail($TARBALL)"
[ "$fail" -eq 0 ] || exit 1
