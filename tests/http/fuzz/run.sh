#!/bin/sh
# run.sh —— P4-D fuzz 长跑 runner(结构化生成 × 双臂 × watchdog;不入 CI 主环)
#
# 口径(fuzz_http.ct / fuzz_gz.ct 头注详):
#   - 臂分工:interp 臂 = fuzz_http.ct(ctron-cc run;parse/chunk/ws 十类);
#     emit 臂 = 同文件原生跑(同语料流,跨口径对拍)+ fuzz_gz.ct(gzip/deflate
#     垃圾;解释口径 extern 桥不支持 lane 参,结构性 emit 专臂,enc x_ 同款);
#     差分 = emit 臂 DIFF 模式吐 hex → pico_diff.c(picohttpparser 同语料对拍)。
#   - 判据 = 零崩溃零挂死:任段退出码非 0 即 CRASH,watchdog timeout 即 HANG
#     —— 皆 fail。rc/err 只计数不断言(分布打印供登记)。
#   - 差分计数:pico_accepts_we_reject = 严格子集预期差(obs-fold/TE+CL/上限
#     面,>0 正常,登记);we_accept_pico_rejects 应 = 0(>0 异常,登记排查;
#     不自动 fail —— 语义分歧归文档,崩溃挂死才是本门)。
#   - 资源口径(登记):解释器逐 case 内存增长 ~2.4MB/case 实测(2000 case
#     峰值 4.7GB;同 P2 已登记的解释器内存债族)⇒ interp 段 iters 上限
#     1500(峰值 ~3.6GB)且每 seed 重启进程;emit 臂原生无此界。
#   - 本地门禁(缺省):SEEDS=24 × {interp 1500 + emit 50000 + gz 50000 +
#     diff 256} ≈ 11 分钟 ≥ 10 分钟。
#   - nightly(建议,≥30 分钟):CTRON_FUZZ_SEEDS=64 CTRON_FUZZ_ITERS_EMIT=200000
#     CTRON_FUZZ_DIFF_N=1024 sh tests/http/fuzz/run.sh(interp 段上限不动 ——
#     内存口径;拉种子数不拉段长)。
# 前置:compiler/native.sh;gz/diff 臂需 cc + vendor/deflate/build/lib/libminiz.a
#(缺席响亮失败并指路 vendor/deflate/build.sh,enc x_ 夹具同款)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
CC="$ROOT/compiler/bin/ctron-cc"
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/std"

SEEDS="${CTRON_FUZZ_SEEDS:-24}"
ITERS_I="${CTRON_FUZZ_ITERS_INTERP:-1500}"
ITERS_E="${CTRON_FUZZ_ITERS_EMIT:-50000}"
DIFFN="${CTRON_FUZZ_DIFF_N:-256}"
TMO_I="${CTRON_FUZZ_TIMEOUT_INTERP:-120}"
TMO_E="${CTRON_FUZZ_TIMEOUT_EMIT:-60}"

[ -x "$CC" ] || { echo "fuzz/run: 缺少 ctron-cc(先: compiler/native.sh)" >&2; exit 2; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT

echo "== fuzz: 结构化生成长跑(seeds=$SEEDS interp=$ITERS_I emit=$ITERS_E diff=$DIFFN)=="

pass=0
fail=0

seg() { # seg <name> <rc>
    if [ "$2" = 0 ]; then
        pass=$((pass + 1))
        echo "  PASS $1"
    elif [ "$2" = 124 ] || [ "$2" = 137 ] || [ "$2" = 143 ]; then
        fail=$((fail + 1))
        echo "  FAIL $1 (HANG/资源杀 rc=$2 —— watchdog 口径,零挂死判据破)"
    else
        fail=$((fail + 1))
        echo "  FAIL $1 (CRASH rc=$2 —— 零崩溃判据破)"
    fi
}

# ---- 构建三臂(一次;env 换种子复跑)----
env CTRON_FUZZ_ITERS=1 "$CC" run "$DIR/fuzz_http.ct" > /dev/null 2>"$T/i.chk" || { echo "fuzz/run: fuzz_http.ct 载入失败" >&2; sed -n '1,5p' "$T/i.chk"; exit 2; }

if [ -x "$EMIT" ]; then
    "$EMIT" run "$DIR/fuzz_http.ct" > "$T/e.c" 2>"$T/e.err" \
        && cc -O1 -w -o "$T/e.bin" "$T/e.c" 2>"$T/e.cc.err" \
        || { echo "fuzz/run: emit 臂构建失败" >&2; sed -n '1,5p' "$T/e.err" "$T/e.cc.err"; exit 2; }
    if [ -f "$ROOT/vendor/deflate/build/lib/libminiz.a" ]; then
        "$EMIT" run "$DIR/fuzz_gz.ct" > "$T/g.c" 2>"$T/g.err" \
            && cc -O1 -w -I"$ROOT/http/c_src" -I"$ROOT/vendor/deflate/miniz" -o "$T/g.bin" "$T/g.c" \
               "$ROOT/http/c_src/ctron_deflate.c" "$ROOT/vendor/deflate/build/lib/libminiz.a" 2>"$T/g.cc.err" \
            || { echo "fuzz/run: gz 臂构建失败" >&2; sed -n '1,5p' "$T/g.err" "$T/g.cc.err"; exit 2; }
        HAVE_GZ=1
    else
        HAVE_GZ=0
        echo "  [skip] gz 臂:缺 vendor/deflate/build/lib/libminiz.a(先: sh vendor/deflate/build.sh)"
    fi
    cc -O1 -w -o "$T/pico_diff" "$DIR/pico_diff.c" "$DIR/../bench/pico/picohttpparser.c" 2>/dev/null \
        || { echo "fuzz/run: pico_diff 构建失败" >&2; exit 2; }
    HAVE_E=1
else
    HAVE_E=0
    HAVE_GZ=0
    echo "  [skip] emit/gz/diff 臂:缺 ctron-emit(interp 臂仍跑)"
fi

S=1
while [ "$S" -le "$SEEDS" ]; do
    # ---- interp 臂(段界 iters:解释器内存口径,见头注)----
    env CTRON_FUZZ_SEED="$S" CTRON_FUZZ_ITERS="$ITERS_I" CTRON_FUZZ_KIND=interp \
        timeout "$TMO_I" "$CC" run "$DIR/fuzz_http.ct" > "$T/i.$S.out" 2>&1
    seg "seed=$S interp" $?
    # ---- emit 臂(同流跨口径对拍)----
    if [ "$HAVE_E" = 1 ]; then
        env CTRON_FUZZ_SEED="$S" CTRON_FUZZ_ITERS="$ITERS_E" CTRON_FUZZ_KIND=emit \
            timeout "$TMO_E" "$T/e.bin" > "$T/e.$S.out" 2>&1
        seg "seed=$S emit" $?
        # ---- gz 臂(emit 专臂)----
        if [ "$HAVE_GZ" = 1 ]; then
            env CTRON_FUZZ_SEED="$S" CTRON_FUZZ_ITERS="$ITERS_E" CTRON_FUZZ_KIND=emit \
                timeout "$TMO_E" "$T/g.bin" > "$T/g.$S.out" 2>&1
            seg "seed=$S gz" $?
        fi
        # ---- 差分(emit DIFF 模式 → pico;两段分开跑,watchdog 不被管道吞)----
        env CTRON_FUZZ_SEED="$S" CTRON_FUZZ_DIFF=1 CTRON_FUZZ_DIFF_N="$DIFFN" CTRON_FUZZ_KIND=emit \
            timeout "$TMO_E" "$T/e.bin" > "$T/d.$S.out" 2>&1
        seg "seed=$S diff-gen" $?
        "$T/pico_diff" < "$T/d.$S.out" >> "$T/pico.sum"
        seg "seed=$S diff-pico" $?
    fi
    S=$((S + 1))
done

if [ -f "$T/pico.sum" ]; then
    awk '/^PICO /{for (i = 1; i <= NF; i++) { split($i, kv, "="); s[kv[1]] += kv[2] } }
         END { printf "  differential: total=%d ours_ok=%d pico_ok=%d pico_accepts_we_reject=%d we_accept_pico_rejects=%d\n",
               s["total"], s["ours_ok"], s["pico_ok"], s["pico_accepts_we_reject"], s["we_accept_pico_rejects"] }' "$T/pico.sum"
fi

echo "fuzz/run: pass=$pass fail=$fail(零崩溃/零挂死判据;差分计数登记见上)"
[ "$pass" -gt 0 ] || { echo "fuzz/run: no segments ran"; exit 1; }
[ "$fail" = 0 ]
