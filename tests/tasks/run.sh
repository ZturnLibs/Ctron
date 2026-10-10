#!/bin/sh
# tests/tasks —— 驱动器任务面门(task/gate/clean;设计 2026-10-10-build-driver-design §4/§5)
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$DIR/../.." && pwd)
DRV="sh $ROOT/ctron"
SBOX=$(mktemp -d "${TMPDIR:-/tmp}/ctron_tasks.XXXXXX")
trap 'rm -rf "$SBOX"' EXIT
PASS=0
FAIL=0
ok() { PASS=$((PASS+1)); }
bad() { FAIL=$((FAIL+1)); echo "[FAIL] $1" >&2; }

# 沙盒夹具:拷 data 并把清单放沙盒根(cwd = 清单目录是执行器契约)
cp -r "$DIR/data" "$SBOX/data"
cp "$DIR/data/m.ctcl" "$SBOX/ctron.ctcl"

T() {
    NAME=$1; WANT_RC=$2; PATTERN=$3; shift 3
    OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV "$@" 2>&1)
    RC=$?
    if [ "$RC" -ne "$WANT_RC" ]; then
        bad "$NAME: rc=$WANT_RC 期望,实得 $RC(输出: $OUT)"
    elif [ -n "$PATTERN" ] && ! printf '%s' "$OUT" | grep -q "$PATTERN"; then
        bad "$NAME: 输出缺模式 [$PATTERN](输出: $OUT)"
    else
        ok
    fi
}

# T1 task 列表
T "task 列表" 0 "hello" task
# T2 task 运行正例
T "task 运行" 0 "ran ok" task hello
# T3 未知任务
T "未知任务" 2 "未知任务" task nosuch
# T4 清单非法(未知块)
cp "$DIR/data/meta_bad.ctcl" "$SBOX/bad.ctcl"
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV task -f bad.ctcl 2>&1)
if [ $? -eq 2 ] && printf '%s' "$OUT" | grep -q "E5044\|未知块"; then ok; else bad "清单非法: $OUT"; fi
# T5 空 steps
cp "$DIR/data/empty.ctcl" "$SBOX/empty.ctcl"
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV task -f empty.ctcl 2>&1)
if [ $? -eq 2 ] && printf '%s' "$OUT" | grep -q "E5054\|空列表"; then ok; else bad "空 steps: $OUT"; fi
# T6 元字符拒绝(拒绝在执行面,须点名任务;列表不跑步骤)
cp "$DIR/data/metachar.ctcl" "$SBOX/metachar.ctcl"
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV task -f metachar.ctcl a 2>&1)
if [ $? -eq 2 ] && printf '%s' "$OUT" | grep -q "元字符"; then ok; else bad "元字符拒绝: $OUT"; fi
# T7 引号切分与转义(双层:CTCL 反转义 + 切分器引号分组)
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV task quotes 2>&1)
if [ $? -eq 0 ] && printf '%s' "$OUT" | grep -q 'arg1=say "hi"' && printf '%s' "$OUT" | grep -q 'argc=1'; then ok; else bad "引号切分: $OUT"; fi
# T8 self 解析:步骤里裸名 ctron 指向驱动器自身
cat > "$SBOX/self.ctcl" <<'EOF2'
ctron {
    config_version = 1
}

task "selfcheck" {
    steps = ["ctron --version"]
}
EOF2
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV task -f self.ctcl selfcheck 2>&1)
if [ $? -eq 0 ] && printf '%s' "$OUT" | grep -q "ctron "; then ok; else bad "self 解析: $OUT"; fi
# T9 fail-stop:步骤 rc 透传 + 失败后后续步骤被跳过
cat > "$SBOX/failstop.ctcl" <<'EOF2'
ctron {
    config_version = 1
}

task "failstop" {
    steps = ["sh data/ok.sh", "sh data/fail.sh", "echo never-run-marker"]
}
EOF2
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV task -f failstop.ctcl failstop 2>&1)
if [ $? -eq 3 ] && printf '%s' "$OUT" | grep -q "ran ok" && ! printf '%s' "$OUT" | grep -q "never-run-marker" && printf '%s' "$OUT" | grep -q "在步骤 2 失败(rc=3)"; then ok; else bad "fail-stop: rc=3+跳过后续+判词 期望,实得: $OUT"; fi
# T10 gate = task 别名
T "gate 别名" 0 "ran ok" gate hello
# T11 内建默认门回落:项目目录(Ctron.ctcl 在场)无 ctron.ctcl → build+fmt
mkdir -p "$SBOX/proj/src"
printf 'pkg {\n    manifest_version = 1\n    name = "proj"\n    version = "0.1.0"\n}\n' > "$SBOX/proj/Ctron.ctcl"
printf 'fn main() {\n    println("hi")\n}\n' > "$SBOX/proj/src/main.ct"
(cd "$SBOX/proj" && CTRON_DRV_ROOT="$SBOX" $DRV fmt -w src/main.ct) > /dev/null   # 先 fmt 归一,免门假红
OUT=$(cd "$SBOX/proj" && CTRON_DRV_ROOT="$SBOX" $DRV gate 2>&1)
if [ $? -eq 0 ] && printf '%s' "$OUT" | grep -q "内建默认门" && printf '%s' "$OUT" | grep -q "已构建"; then ok; else bad "内建默认门: $OUT"; fi
# T12 非项目目录 + 无 ctron.ctcl → rc2 用法错
mkdir -p "$SBOX/empty_dir"
OUT=$(cd "$SBOX/empty_dir" && CTRON_DRV_ROOT="$SBOX" $DRV gate 2>&1)
if [ $? -eq 2 ]; then ok; else bad "gate 用法错: rc 2 期望,实得(输出: $OUT)"; fi
# T13 clean 清缓存域(--all 加 build/);CTRON_DRV_ROOT 沙盒
mkdir -p "$SBOX/.cache/emit" "$SBOX/.cache/bare" "$SBOX/proj/build" "$SBOX/proj/pkgs"
OUT=$(cd "$SBOX/proj" && CTRON_DRV_ROOT="$SBOX" $DRV clean --all 2>&1)
if [ $? -eq 0 ] && [ ! -d "$SBOX/.cache/emit" ] && [ ! -d "$SBOX/.cache/bare" ] && [ ! -d "$SBOX/proj/build" ] && [ ! -d "$SBOX/proj/pkgs" ]; then ok; else bad "clean --all: $OUT"; fi
# T14 clean 未知旗标
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV clean --nope 2>&1)
if [ $? -eq 2 ]; then ok; else bad "clean 旗标: rc 2 期望"; fi
# T15 stdin 洞钉:步骤循环走专用 fd3,子步骤 stdin 不指 STEP_LIST——中间的 cat 步骤
# 读不到步骤行,三步全跑(有洞时 cat 吃掉剩余清单行,循环静默提前收敛,非 3 步)
cat > "$SBOX/stdin.ctcl" <<'EOF2'
ctron {
    config_version = 1
}

task "multistep" {
    steps = ["sh data/ok.sh", "cat", "sh data/ok.sh"]
}
EOF2
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV task -f stdin.ctcl multistep < /dev/null 2>&1)
if [ $? -eq 0 ] && printf '%s' "$OUT" | grep -q "完成(3 步)"; then ok; else bad "stdin 洞: 完成(3 步) 期望,实得: $OUT"; fi
# T16 clean --all 项目模式限定词钉:非项目目录(无 Ctron.ctcl)跳过 build/pkgs,缓存域照清
mkdir -p "$SBOX/nonproj/build" "$SBOX/.cache/emit"
OUT=$(cd "$SBOX/nonproj" && CTRON_DRV_ROOT="$SBOX" $DRV clean --all 2>&1)
if [ $? -eq 0 ] && [ ! -d "$SBOX/.cache/emit" ] && [ -d "$SBOX/nonproj/build" ]; then ok; else bad "clean --all 项目守卫: rc 0+缓存清+build 留 期望,实得: $OUT"; fi
# T17 task.cwd:步骤以 cwd 目录为工作目录
mkdir -p "$SBOX/sub"
printf '#!/bin/sh\nprintf "cwd-ok pwd=%%s\\n" "$(basename "$PWD")"\n' > "$SBOX/sub/go.sh"
cat > "$SBOX/cwd.ctcl" <<'EOF2'
ctron {
    config_version = 1
}

task "build" {
    cwd = "sub"
    steps = ["sh go.sh"]
}
EOF2
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV task -f cwd.ctcl build < /dev/null 2>&1)
if [ $? -eq 0 ] && printf '%s' "$OUT" | grep -q "cwd-ok pwd=sub"; then ok; else bad "task.cwd 正例: $OUT"; fi
# T18 task.cwd 指向不存在目录 → rc2 fail-closed
cat > "$SBOX/cwd_bad.ctcl" <<'EOF2'
ctron {
    config_version = 1
}

task "build" {
    cwd = "nope"
    steps = ["sh go.sh"]
}
EOF2
OUT=$(cd "$SBOX" && CTRON_DRV_ROOT="$SBOX" $DRV task -f cwd_bad.ctcl build < /dev/null 2>&1)
if [ $? -eq 2 ] && printf '%s' "$OUT" | grep -q "cwd 目录不存在"; then ok; else bad "task.cwd 缺目录: rc2+判词 期望,实得: $OUT"; fi

echo "tests/tasks: $PASS 过 / $FAIL 败"
[ "$FAIL" -eq 0 ]
