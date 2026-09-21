#!/bin/sh
# bench.sh —— P4-D 解析基准门禁(ctron http_parse_head vs vendored picohttpparser)
# 本地/nightly 门禁,不入 CI 主环;默认跳过(CTRON_HTTP_BENCH=1 启用,tests/net
# bench.sh 同款惯例)。
#
# 口径(README.md 详):
#   - 语料 = 三报文(GET 无体 / POST+CL 体 / chunked 响应),两侧逐字节同文;
#     【字节 digest 对 pin】两侧 digest= 不一致即 FAIL(同字节公平性自证)。
#   - 每轮:热身 1000 轮不计时 + 正式 N 轮(env CTRON_HTTP_BENCH_N,缺省
#     100000;每轮 3 报文 = 3 次 parse);CLOCK_MONOTONIC 同源计时。
#   - 采数 ×3(默认同机连跑),每侧取 min(ns_per_req)(bench.sh 家族"3 取
#     最小"惯例),ratio = ctron_min / pico_min。
#   - 门:ratio ≤ 2 → GREEN;> 2 → RED+REGISTER(诚实口径,不粉饰:解析器
#     工作在 &I64[] 字节道,每字节一条 I64 lane —— 门禁设定按字节宽度,超门
#     即按 8× lane 税归因登记,归 P9 I8-typedef 处置,不改数、不换语料)。
# 退出码:0 = 门内 / 1 = 超门(数字照录,登记归文档)/ 2 = 环境缺件。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/std"

if [ "${CTRON_HTTP_BENCH:-}" != "1" ]; then
    echo "bench: SKIP(置 CTRON_HTTP_BENCH=1 启用;本地/nightly 门禁,不入 CI 主环)"
    exit 0
fi
[ -x "$EMIT" ] || { echo "bench: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; }
[ -f "$DIR/pico/picohttpparser.c" ] || { echo "bench: 缺 pico/picohttpparser.c(vendored)" >&2; exit 2; }

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT

N="${CTRON_HTTP_BENCH_N:-100000}"

echo "== bench-parse: ctron http_parse_head vs picohttpparser(N=$N ×3,各取最小)=="

# ---- 构建 ----
"$EMIT" run "$DIR/bench_parse.ct" > "$T/ctron.c" 2>"$T/ctron.err" \
    && cc -O1 -w -pthread -I"$ROOT/std/net/c_src" -o "$T/ctron.bin" "$T/ctron.c" "$ROOT/std/net/c_src/ctron_net.c" 2>"$T/ctron.cc.err" \
    || { echo "bench: ctron 侧构建失败"; sed -n '1,5p' "$T/ctron.err" "$T/ctron.cc.err" 2>/dev/null; exit 2; }
cc -O1 -w -I"$DIR" -o "$T/pico.bin" "$DIR/bench_pico.c" "$DIR/pico/picohttpparser.c" \
    || { echo "bench: pico 侧构建失败"; exit 2; }

# ---- 公平性 pin:字节 digest 必须一致(同字节才可对拍) ----
DG_C=$(env CTRON_HTTP_BENCH_N=2 "$T/ctron.bin" | sed -n 's/^digest=//p')
DG_P=$(env CTRON_HTTP_BENCH_N=2 "$T/pico.bin" | sed -n 's/^digest=//p')
if [ -z "$DG_C" ] || [ -z "$DG_P" ] || [ "$DG_C" != "$DG_P" ]; then
    echo "bench: FAIL 语料字节 digest 不一致(ctron=$DG_C pico=$DG_P)—— 两侧语料漂移,禁止采数"
    exit 1
fi
echo "  digest pin: $DG_C(两侧同字节)"

MIN_C=""; MIN_P=""
R=1
while [ "$R" -le 3 ]; do
    OUT_C=$(env CTRON_HTTP_BENCH_N="$N" "$T/ctron.bin") || { echo "bench: ctron 侧运行失败(round $R)"; echo "$OUT_C"; exit 2; }
    OUT_P=$(env CTRON_HTTP_BENCH_N="$N" "$T/pico.bin") || { echo "bench: pico 侧运行失败(round $R)"; echo "$OUT_P"; exit 2; }
    NS_C=$(printf '%s\n' "$OUT_C" | sed -n 's/^ns_per_req=//p')
    NS_P=$(printf '%s\n' "$OUT_P" | sed -n 's/^ns_per_req=//p')
    DG_C2=$(printf '%s\n' "$OUT_C" | sed -n 's/^digest=//p')
    DG_P2=$(printf '%s\n' "$OUT_P" | sed -n 's/^digest=//p')
    if [ "$DG_C2" != "$DG_C" ] || [ "$DG_P2" != "$DG_P" ]; then
        echo "bench: FAIL round $R digest 漂移($DG_C2 / $DG_P2)"; exit 1
    fi
    CS_C=$(printf '%s\n' "$OUT_C" | sed -n 's/^checksum=//p')
    CS_P=$(printf '%s\n' "$OUT_P" | sed -n 's/^checksum=//p')
    if [ -z "$CS_C" ] || [ "$CS_C" = "0" ] || [ -z "$CS_P" ] || [ "$CS_P" = "0" ]; then
        echo "bench: FAIL round $R checksum 为空/零(解析被折叠?)"; exit 1
    fi
    echo "  round $R: ctron ${NS_C} ns/req | pico ${NS_P} ns/req"
    if [ -z "$MIN_C" ] || [ "$NS_C" -lt "$MIN_C" ]; then MIN_C="$NS_C"; fi
    if [ -z "$MIN_P" ] || [ "$NS_P" -lt "$MIN_P" ]; then MIN_P="$NS_P"; fi
    R=$((R + 1))
done

RATIO=$(awk -v c="$MIN_C" -v p="$MIN_P" 'BEGIN { printf "%.3f", c / p }')
echo "  min-of-3: ctron ${MIN_C} ns/req | pico ${MIN_P} ns/req | ratio ${RATIO}x"
VERDICT=$(awk -v r="$RATIO" 'BEGIN { print (r <= 2.0) ? "GREEN" : "RED" }')
if [ "$VERDICT" = "GREEN" ]; then
    echo "  门禁 ≤2×: PASS(ratio ${RATIO}x)"
    exit 0
fi
echo "  门禁 ≤2×: RED(ratio ${RATIO}x)—— 登记档:字节道 8× lane 税归因(lane = I64/字节),P9 I8-typedef 处置;数字照录不粉饰"
exit 1
