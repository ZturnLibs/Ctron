#!/bin/sh
# tests/pb/run.sh —— std/pb protobuf 线格式编解码验收(P7-B)
# 口径(tests/log 同款):种子漂移守卫 + inline test + corpus/*.ct 双臂;
# std/pb 为零 use 纯 Ctron 叶模块。CC/EMIT 支持 CTRON_CC/CTRON_EMIT 覆盖。
# 注:inline 仅留 1 条(解释器堆不回收在册债,多 test 合跑即 137——P6-D
# 「拆小文件」惯例),其余断言全在 corpus(独立进程天然隔离)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CC="${CTRON_CC:-$ROOT/compiler/bin/ctron-cc}"
EMIT="${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}"
export CTRON_STDPATH="$ROOT/std"
if [ ! -x "$CC" ] || [ ! -x "$EMIT" ]; then echo "pb/run: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/pb protobuf 线格式用例(P7-B)=="

if diff -q "$ROOT/std/pb.ct" "$ROOT/compiler/test/stdpkg/std/pb.ct" > /dev/null 2>&1; then
    pass=$((pass+1)); echo "  PASS std/pb.ct 种子副本无漂移"
else
    fail=$((fail+1)); echo "  FAIL std/pb.ct 种子副本漂移(cp std/pb.ct compiler/test/stdpkg/std/)"
fi

if "$CC" run "$ROOT/std/pb.ct" > "$T/li.out" 2>&1; then
    pass=$((pass+1)); echo "  PASS std/pb.ct (inline, interp)"
else
    fail=$((fail+1)); echo "  FAIL std/pb.ct (inline, interp)"; sed -n '1,5p' "$T/li.out"
fi
# emit 直发主文件腿(2026-09-25):确定性 SIGKILL @20480B(PbField typedef 处;
# 同文件作依赖被 corpus emit 臂全量发射且全绿,归编译泳道,divergences 在册)
# → 本腿以 chk 语义门替代,代码生成面由 corpus emit 臂覆盖。
CHK="${CTRON_CHK:-$ROOT/compiler/bin/ctron-chk}"
if [ -x "$CHK" ] || [ -n "${CTRON_CHK:-}" ]; then
    if "$CHK" run "$ROOT/std/pb.ct" > "$T/li.chk" 2>&1; then
        pass=$((pass+1)); echo "  PASS std/pb.ct (sem, chk)"
    else
        fail=$((fail+1)); echo "  FAIL std/pb.ct (sem, chk)"; sed -n '1,5p' "$T/li.chk"
    fi
fi

for f in "$DIR"/corpus/*.ct; do
    name=$(basename "$f" .ct)
    if "$CC" run "$f" > "$T/$name.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS $name (interp)"
    else
        fail=$((fail+1)); echo "  FAIL $name (interp)"; sed -n '1,5p' "$T/$name.out"
    fi
    if "$EMIT" run "$f" > "$T/$name.c" 2>"$T/$name.err" && cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/$name.bin" "$T/$name.c" "$ROOT/net/c_src/ctron_net.c" 2>"$T/$name.cc.err" && "$T/$name.bin" > "$T/$name.run" 2>&1; then
        pass=$((pass+1)); echo "  PASS $name (emit)"
    else
        fail=$((fail+1)); echo "  FAIL $name (emit)"; sed -n '1,5p' "$T/$name.err" "$T/$name.cc.err" "$T/$name.run" 2>/dev/null
    fi
done

echo "pb/run: pass=$pass fail=$fail"
[ "$pass" -gt 0 ] || { echo "pb/run: no cases ran"; exit 1; }
[ "$fail" = 0 ] || exit 1
