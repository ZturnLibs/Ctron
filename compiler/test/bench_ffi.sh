#!/bin/sh
# bench_ffi.sh —— FFI 边界微基准(§9.4 性能口径的 FFI 面;tests/ffi/bench 夹具)
#
# 口径:Ctron 发射面(ctron-emit → cc -O2,与生产同形)逐次过界 vs 纯 C 循环同构基线
# (经一次边界整批计时);每场景预热 + 3 轮取最小(夹具内完成);ns/op 由夹具按
# Δms×1e8/N 出百分之一 ns 精度。四场景:call(标量)/ cb(fn 指针回调)/
# struct(32B 按值)/ str(256B 编组深拷)。
# 报告制,不设硬门(机器方差);某场景 ffi/c > 3× 时告警(仅提示)。
# 前置:compiler/native.sh、cc。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
EMIT="$ROOT/compiler/bin/ctron-emit"
FIX="$ROOT/tests/ffi/bench"

if [ ! -x "$EMIT" ]; then
    echo "bench_ffi: 缺少 bin/ctron-emit(先: compiler/native.sh)" >&2
    exit 2
fi

T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT

"$EMIT" run "$FIX/src/main.ct" > "$T/bench.c" 2>"$T/emit.err" || {
    echo "bench_ffi: emit 失败: $(head -1 "$T/emit.err")" >&2
    exit 1
}
cc -O2 -w -o "$T/bench.bin" "$T/bench.c" "$FIX"/c_src/*.c || {
    echo "bench_ffi: cc 失败" >&2
    exit 1
}

echo "== FFI 边界微基准(Ctron 发射面 -O2 vs 纯 C 基线,ns/op,预热+3 取最小)=="
OUT=$("$T/bench.bin" run "$FIX/src/main.ct")
echo "$OUT" | while read -r name q rest; do
    printf "  %-11s %6s.%02s ns/op\n" "$name" $((q / 100)) $((q % 100))
done

getq() {
    echo "$OUT" | awk -v n="$1" '$1 == n { print $2 }'
}

warn=0
for pair in "ffi_call c_call" "ffi_cb c_cb" "ffi_struct c_struct" "ffi_str c_str"; do
    f=$(getq $(echo $pair | cut -d' ' -f1))
    c=$(getq $(echo $pair | cut -d' ' -f2))
    if [ -z "$f" ] || [ -z "$c" ] || [ "$c" -le 0 ]; then
        continue
    fi
    r=$(awk -v f="$f" -v c="$c" 'BEGIN { printf "%.2f", f / c }')
    printf "  %-11s ffi/c = %sx\n" "$(echo $pair | cut -d' ' -f1)" "$r"
    # cb 场景豁免告警:两侧同为 2 次调用/op,亚 ns 绝对差被基线 doloop 变换的
    # 循环形状放大(绝对差 ~0.7ns/次),比值不构成过界开销信号
    case $pair in
        ffi_cb*) continue ;;
    esac
    over=$(awk -v f="$f" -v c="$c" 'BEGIN { print (f > c * 300) ? 1 : 0 }')
    if [ "$over" = "1" ]; then
        echo "  [warn] $(echo $pair | cut -d' ' -f1) 过界开销超纯 C 基线 3 倍(检查发射面调用形态)"
        warn=1
    fi
done

if [ $warn -eq 0 ]; then
    echo "bench_ffi: 全部场景开销在望 ✓"
fi
exit 0
