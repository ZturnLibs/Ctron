#!/bin/sh
# tests/steno/soak/run.sh —— 秘本引擎 soak 门(设计 §11 内存线/零崩线 Ctron 侧)
# 面:TaskPool(8) × 100 会话 × 20 段 = 2000 任务;三臂(默认 GC on / CTRON_GC=off /
#     CTRON_RT=coro)各 rc=0 + digest 钉(逐字节确定)+ 无 panic 文案 + max RSS < 界。
# RSS 口径:macOS /usr/bin/time -l maximum resident set size(字节);界 64MB =
# 标定实测 1.9MB(2000 任务任务段回收后)×30 余量——界只拦「无界增长」,不微调。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}"
[ -x "$EMIT" ] || { echo "soak: 缺编译器二进制(先: compiler/native.sh)" >&2; exit 2; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
RSS_BOUND=64000000
EXPECT="soak-probe tasks 2000 digest 306971647"

CTRON_STDPATH="$ROOT/lib/std" "$EMIT" run "$DIR/src/main.ct" > "$T/soak.c" 2>"$T/emit.err" \
    || { echo "soak: FAIL emit: $(tail -1 "$T/emit.err")" >&2; exit 1; }
cc -O1 -w -pthread -o "$T/soak.bin" "$T/soak.c" 2>"$T/cc.err" \
    || { echo "soak: FAIL cc: $(tail -2 "$T/cc.err")" >&2; exit 1; }

pass=0; fail=0
for arm in "default" "CTRON_GC=off" "CTRON_RT=coro"; do
    if [ "$arm" = "default" ]; then
        /usr/bin/time -l "$T/soak.bin" > "$T/out" 2>"$T/time.err"
    else
        env "$arm" /usr/bin/time -l "$T/soak.bin" > "$T/out" 2>"$T/time.err"
    fi
    rc=$?
    rss=$(grep -i "maximum resident set size" "$T/time.err" | awk '{print $1}')
    if [ "$rc" -ne 0 ]; then
        echo "  FAIL soak $arm (rc=$rc): $(tail -2 "$T/out" | tr '\n' ' ')"; fail=$((fail+1)); continue
    fi
    if ! grep -q "$EXPECT" "$T/out"; then
        echo "  FAIL soak $arm (digest 钉不合): $(cat "$T/out")"; fail=$((fail+1)); continue
    fi
    if grep -qi "panic" "$T/out"; then
        echo "  FAIL soak $arm (panic 文案): $(grep -i panic "$T/out" | head -1)"; fail=$((fail+1)); continue
    fi
    if [ -z "$rss" ] || [ "$rss" -ge "$RSS_BOUND" ] 2>/dev/null; then
        echo "  FAIL soak $arm (RSS $rss ≥ 界 $RSS_BOUND 或未测得)"; fail=$((fail+1)); continue
    fi
    pass=$((pass+1)); echo "  PASS soak $arm (rc0 + digest 钉 + RSS ${rss}B < 界)"
done
echo "== soak: $pass pass / $fail fail =="
[ "$fail" -eq 0 ] || exit 1
