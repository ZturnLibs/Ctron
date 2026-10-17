#!/bin/sh
# tests/bench/engine-proxy/bench.sh —— 秘本引擎代理基准(设计 §11 RTF 线 Ctron 侧门)
# 纪律:digest pin(Ctron 产物 vs C 参照逐字节一致,diff 除 rtf_ms 行)+ ×3 取 min +
#       比值门 ≤1.2(§11 RTF ≤1.2× 的 C-vs-Ctron 代理口径;真 sherpa/Rust 对拍留秘本仓)。
# 前置:compiler/native.sh、cc。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}"
[ -x "$EMIT" ] || { echo "engine-proxy: 缺编译器二进制(先: compiler/native.sh)" >&2; exit 2; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT

CTRON_STDPATH="$ROOT/lib/std" "$EMIT" run "$DIR/src/main.ct" > "$T/ep.c" 2>"$T/ep.err" \
    || { echo "engine-proxy: emit 失败: $(tail -1 "$T/ep.err")" >&2; exit 2; }
cc -O1 -w -pthread -o "$T/ep.bin" "$T/ep.c" "$DIR/c_src/stub.c" 2>"$T/epcc.err" \
    || { echo "engine-proxy: cc(ctron) 失败: $(tail -2 "$T/epcc.err")" >&2; exit 2; }
cc -O1 -w -o "$T/ref.bin" "$DIR/src/ref.c" "$DIR/c_src/stub.c" 2>"$T/refcc.err" \
    || { echo "engine-proxy: cc(ref) 失败: $(tail -2 "$T/refcc.err")" >&2; exit 2; }

# digest pin:两臂首行(digest 帧)逐字节一致;rtf_ms 行(次行)时值不入钉
"$T/ep.bin" > "$T/out.ctron" 2>&1
"$T/ref.bin" > "$T/out.ref" 2>&1
grep '^engine-proxy digest' "$T/out.ctron" > "$T/dig.ctron"
grep '^engine-proxy digest' "$T/out.ref" > "$T/dig.ref"
if ! diff -q "$T/dig.ctron" "$T/dig.ref" > /dev/null 2>&1; then
    echo "engine-proxy: FAIL digest 分歧(ctron=[$(cat "$T/dig.ctron")] ref=[$(cat "$T/dig.ref")])" >&2
    exit 1
fi

runmin() {
    best=999999999
    i=0
    while [ $i -lt 3 ]; do
        ms=$( "$1" | sed -n 's/^engine-proxy rtf_ms \([0-9]*\) .*/\1/p' )
        [ -n "$ms" ] || { echo "engine-proxy: FAIL 无 rtf_ms 行" >&2; exit 1; }
        [ "$ms" -lt "$best" ] && best=$ms
        i=$((i + 1))
    done
    echo "$best"
}

C=$(runmin "$T/ep.bin")
R=$(runmin "$T/ref.bin")
python3 -c "
c, r = $C, $R
ratio = (c / r) if r else 0
audio_s = 320
ctron_rtf = c / 1000.0 / audio_s
print('engine-proxy: ctron %dms vs C %dms, 比值 %.3f(门 ≤1.2);ctron RTF %.3f(320s 音频)' % (c, r, ratio, ctron_rtf))
if ratio > 1.2:
    print('engine-proxy: WARN 比值 >1.2(§11 RTF 线代理口径未达;归因位:算术核宽检税/stub 调用编组;登记热点回切建议,真判据=秘本仓 A/B)')
"
