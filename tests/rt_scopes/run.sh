#!/bin/sh
# tests/rt_scopes/run.sh —— P7-F /debug/scopes 确定性夹具(coro 臂)
# 口径:ctron_rt_scopes_json()(rt 锁内 g_all 走链+jnext 反查等待边)在
# scope 固定树(join 前时点:四子 parked、main running)上快照;shell 解析
# JSON 钉:entries=5、parked=4、wait 边=4 且 waiter 同指。serial 臂照跑
# (g_all 空 → "[[],[]]",登记 serial 形口径)。CTRON_CC/EMIT 覆盖。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}"
export CTRON_STDPATH="$ROOT/std"
[ -x "$EMIT" ] || { echo "rt_scopes/run: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/rt_scopes /debug/scopes 确定性用例(P7-F)=="

if "$EMIT" run "$DIR/src/main.ct" > "$T/rs.c" 2>"$T/rs.err" \
   && cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/rs.bin" "$T/rs.c" "$ROOT/net/c_src/ctron_net.c" "$ROOT/net/c_src/ctron_rt.c" 2>"$T/rs.cc.err"; then
    pass=$((pass+1)); echo "  PASS 构建(含 ctron_rt.c 链接)"
else
    fail=$((fail+1)); echo "  FAIL 构建"; sed -n '1,5p' "$T/rs.err" "$T/rs.cc.err"; exit 1
fi

# serial 形:无 CTRON_RT → g_all 空 → "[[],[]]"
OUT_S=$(CTRON_RT=pthread timeout 10 "$T/rs.bin" 2>/dev/null)
if echo "$OUT_S" | head -2 | grep -q '\[\[\],\[\]\]'; then
    pass=$((pass+1)); echo "  PASS serial 形 [[],[]](g_all 空)"
else
    fail=$((fail+1)); echo "  FAIL serial 形"; echo "$OUT_S" | head -2
fi

# coro 形:固定树快照
OUT=$(CTRON_RT=coro timeout 20 "$T/rs.bin" 2>/dev/null)
JS=$(echo "$OUT" | sed -n 's/^\[\[/[[/p' | head -1)
if [ -z "$JS" ]; then
    fail=$((fail+1)); echo "  FAIL coro 形无 JSON"; echo "$OUT" | head -3; exit 1
fi
N_ENT=$(printf '%s' "$JS" | grep -o '"i":' | wc -l)
N_PARK=$(printf '%s' "$JS" | grep -o '"s":"parked"' | wc -l)
N_EDGE=$(printf '%s' "$JS" | grep -oE '\[[0-9]+,[0-9]+\]' | wc -l)

if [ "$N_ENT" -eq 4 ]; then pass=$((pass+1)); echo "  PASS entries=4(main 非协程,四子全在)"; else fail=$((fail+1)); echo "  FAIL entries=$N_ENT"; fi
if [ "$N_PARK" -eq 4 ]; then pass=$((pass+1)); echo "  PASS parked=4(sleep 80ms 稳态化)"; else fail=$((fail+1)); echo "  FAIL parked=$N_PARK"; fi
if [ "$N_EDGE" -eq 1 ]; then pass=$((pass+1)); echo "  PASS rt 等待边=1(t2 join t1 → [2,3])"; else fail=$((fail+1)); echo "  FAIL rt 等待边=$N_EDGE"; fi

echo "rt_scopes/run: pass=$pass fail=$fail"
[ "$pass" -gt 0 ] || { echo "rt_scopes/run: no cases ran"; exit 1; }
[ "$fail" = 0 ] || exit 1
