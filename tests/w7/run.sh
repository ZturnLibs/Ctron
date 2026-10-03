#!/bin/sh
# tests/w7/run.sh —— W7 并发深水泳道(T34 栈经济)行为锚
# stack_ovf:CTRON_RT_STACK_KB=64 小栈钉死,深递归任务触 guard → 诊断消息 rc=101
# ws_steal:work-stealing 锚(farmer 蓄 burst + 饥饿者偷取);夹具自带断言,
#   正常完成期望 rc=0 —— 以 expect_ok 标记文件区分两类期望。
# ms_grow:P9 栈面全量锚(专属块):同源程序 OFF=触顶/ON(CTRON_MORESTACK)=生长
#   +浅深度同形逐字节+生长态种子双跑一致。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "w7/run: 缺编译器二进制(先: compiler/native.sh)" >&2; exit 2; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
for d in "$DIR"/*/; do
    [ -f "$d/src/main.ct" ] || continue
    name=$(basename "$d")
    CTRON_STDPATH="$ROOT/lib/std" "$EMIT" run "$d/src/main.ct" > "$T/$name.c" 2>"$T/$name.err" \
        || { echo "  FAIL $name (emit)"; fail=$((fail+1)); continue; }
    cc -O1 -w -o "$T/$name.bin" "$T/$name.c" "$ROOT/lib/net/c_src/ctron_net.c" "$ROOT/lib/net/c_src/ctron_rt.c" -pthread \
        || { echo "  FAIL $name (cc)"; fail=$((fail+1)); continue; }
    CTRON_RT=coro CTRON_RT_STACK_KB=64 "$T/$name.bin" run "$d/src/main.ct" > "$T/$name.out" 2>&1
    rc=$?
    if [ -f "$d/expect_ok" ]; then
        if [ $rc -eq 0 ]; then
            pass=$((pass+1)); echo "  PASS $name (rc=0 $(tail -1 "$T/$name.out"))"
        else
            echo "  FAIL $name (期望正常完成; rc=$rc out=$(tail -1 "$T/$name.out"))"; fail=$((fail+1))
        fi
    elif [ $rc -ne 0 ] && grep -q "stack overflow" "$T/$name.out"; then
        pass=$((pass+1)); echo "  PASS $name (触顶诊断 rc=$rc)"
    else
        echo "  FAIL $name (期望触顶诊断; rc=$rc out=$(tail -1 "$T/$name.out"))"; fail=$((fail+1))
    fi
done
# ms_grow:P9 栈面全量对(A′ 分段+调用窗回收+再入续跑;CTRON_MORESTACK 发射期门)
# 四断言:同源程序 OFF@64KB=触顶诊断 rc=101 / ON@64KB=生长算完 rc=0 值正确 /
#   浅深度 OFF@1MB 与 ON@64KB 输出逐字节一致(同形兼容)/ 生长态同种子双跑一致。
mg="$DIR/ms_grow"
if [ -f "$mg/src/grow.ct" ]; then
    RT2="$ROOT/lib/net/c_src/ctron_net.c $ROOT/lib/net/c_src/ctron_rt.c"
    off_rc=-1; on_rc=-1
    if CTRON_STDPATH="$ROOT/lib/std" "$EMIT" run "$mg/src/grow.ct" > "$T/mg_off.c" 2>/dev/null \
       && cc -O1 -w -o "$T/mg_off.bin" "$T/mg_off.c" $RT2 -pthread 2>/dev/null; then
        CTRON_RT=coro CTRON_RT_STACK_KB=64 "$T/mg_off.bin" run "$mg/src/grow.ct" > "$T/mg_off.out" 2>&1
        off_rc=$?
    fi
    if CTRON_STDPATH="$ROOT/lib/std" CTRON_MORESTACK=1 "$EMIT" run "$mg/src/grow.ct" > "$T/mg_on.c" 2>/dev/null \
       && cc -O1 -w -o "$T/mg_on.bin" "$T/mg_on.c" $RT2 -pthread 2>/dev/null; then
        CTRON_RT=coro CTRON_RT_STACK_KB=64 "$T/mg_on.bin" run "$mg/src/grow.ct" > "$T/mg_on.out" 2>&1
        on_rc=$?
    fi
    sed 's/20000/1000/' "$mg/src/grow.ct" > "$T/mg_shallow.ct"
    CTRON_STDPATH="$ROOT/lib/std" "$EMIT" run "$T/mg_shallow.ct" > "$T/mg_sh_off.c" 2>/dev/null
    cc -O1 -w -o "$T/mg_sh_off.bin" "$T/mg_sh_off.c" $RT2 -pthread 2>/dev/null
    CTRON_RT=coro "$T/mg_sh_off.bin" run "$T/mg_shallow.ct" > "$T/mg_sh_off.out" 2>&1
    CTRON_STDPATH="$ROOT/lib/std" CTRON_MORESTACK=1 "$EMIT" run "$T/mg_shallow.ct" > "$T/mg_sh_on.c" 2>/dev/null
    cc -O1 -w -o "$T/mg_sh_on.bin" "$T/mg_sh_on.c" $RT2 -pthread 2>/dev/null
    CTRON_RT=coro CTRON_RT_STACK_KB=64 "$T/mg_sh_on.bin" run "$T/mg_shallow.ct" > "$T/mg_sh_on.out" 2>&1
    CTRON_RT=coro CTRON_RT_SEED=42 CTRON_RT_STACK_KB=64 "$T/mg_on.bin" run "$mg/src/grow.ct" > "$T/mg_det1.out" 2>&1
    CTRON_RT=coro CTRON_RT_SEED=42 CTRON_RT_STACK_KB=64 "$T/mg_on.bin" run "$mg/src/grow.ct" > "$T/mg_det2.out" 2>&1
    if [ "$off_rc" -ne 0 ] && grep -q "stack overflow" "$T/mg_off.out" \
       && [ "$on_rc" -eq 0 ] && grep -q "20000" "$T/mg_on.out" \
       && cmp -s "$T/mg_sh_off.out" "$T/mg_sh_on.out" \
       && cmp -s "$T/mg_det1.out" "$T/mg_det2.out"; then
        pass=$((pass+1)); echo "  PASS ms_grow (触顶 rc=$off_rc/生长 rc=$on_rc/同形逐字节/种子双跑一致)"
    else
        echo "  FAIL ms_grow (off_rc=$off_rc on_rc=$on_rc off.out=$(tail -1 "$T/mg_off.out" 2>/dev/null) on.out=$(tail -1 "$T/mg_on.out" 2>/dev/null) sh_diff=$(cmp -s "$T/mg_sh_off.out" "$T/mg_sh_on.out" && echo same || echo differ))"; fail=$((fail+1))
    fi
fi
echo "w7/run: pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
