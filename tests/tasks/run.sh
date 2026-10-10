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

echo "tests/tasks: $PASS 过 / $FAIL 败"
[ "$FAIL" -eq 0 ]
