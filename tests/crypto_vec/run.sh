#!/bin/sh
# tests/crypto_vec/run.sh —— std/crypto 二进制面 + std/uuid 验收(P5-B;§P5 Task 2)
# 口径:std/crypto 二进制面(List[I32] 字节容器)与 std/uuid 纯形为零链接
# 纯层,tests/json_fidelity 同款双臂跑法:
#   主环 = 解释器(ctron-cc run):corpus/*.ct 断言式夹具逐文件跑(x_ 除外)。
#   副 = emit 同源对拍(ctron-emit → cc → 原生):corpus/*.ct 含 x_。
#   x_ 前缀 = 仅 emit 臂入计(tests/json_fidelity x_p53_rounding 同款结构性
#          登记):x_pbkdf2_hi(c=4096 档)、x_sha256_million(百万 'a')——
#          解释器堆不回收(P5-B 登记 docs/c-rust-divergences.md),高块数
#          档解释器口径不可实用;emit 口径各 ≈1s。x_uuid_live 另需真熵
#          (interp 无 extern 运行时),单独 emit 块链 std/db/c_src/
#          ctron_entropy.c(tests/net 链法;uuid 纯形面已由 e_uuid_pure
#          双臂承载)。
#   interp 逐文件预算 ≈ 10 块压缩等效内(同上登记),故 RFC 4231 七例拆
#   四文件、RFC 6070 低迭代单文件。
#   两臂同一夹具集双计 pass;任一臂红即红。pass>0 空集守卫。
#   双运行时矩阵(CTRON_RT=coro)不适用:纯层无停车点,无 rt 依赖。
# 前置:compiler/native.sh、cc
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CC="$ROOT/compiler/bin/ctron-cc"
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/std"
if [ ! -x "$CC" ]; then echo "crypto_vec/run: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/crypto_vec 密码学向量/uuid 用例(P5-B)=="

# ── std 规范源与种子副本漂移守卫(smoke.sh 3d 同款,本地即查)──
for m in crypto uuid; do
    if diff -q "$ROOT/std/$m.ct" "$ROOT/compiler/test/stdpkg/std/$m.ct" > /dev/null 2>&1; then
        pass=$((pass+1)); echo "  PASS std/$m.ct 种子副本无漂移"
    else
        fail=$((fail+1)); echo "  FAIL std/$m.ct 种子副本漂移(cp std/$m.ct compiler/test/stdpkg/std/)"
    fi
done

# ── std/uuid.ct inline 声明冒烟(无 test 块;装载/类型检查即过)──
if "$CC" run "$ROOT/std/uuid.ct" > "$T/std_uuid.out" 2>&1; then
    pass=$((pass+1)); echo "  PASS std/uuid.ct (decl smoke, interp)"
else
    fail=$((fail+1)); echo "  FAIL std/uuid.ct (decl smoke, interp)"; sed -n '1,5p' "$T/std_uuid.out"
fi
# std/crypto.ct inline 测试归 tests/http/run.sh 常设岗(P4 起),此处不重复计。

# ── corpus:主环 = 解释器(x_ 仅 emit 臂,不入计)──
for f in "$DIR"/corpus/*.ct; do
    name=$(basename "$f" .ct)
    case "$name" in
        x_*) continue ;;
    esac
    if "$CC" run "$f" > "$T/$name.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS $name (interp)"
    else
        fail=$((fail+1)); echo "  FAIL $name (interp)"; sed -n '1,5p' "$T/$name.out"
    fi
done

# ── corpus:副臂 = emit 对拍(含 x_;纯层零 extern 直链;x_uuid_live 链熵垫片)──
if [ -x "$EMIT" ]; then
    for f in "$DIR"/corpus/*.ct; do
        name=$(basename "$f" .ct)
        EXTRA=""
        if [ "$name" = "x_uuid_live" ]; then
            EXTRA="$ROOT/std/db/c_src/ctron_entropy.c"
        fi
        if "$EMIT" run "$f" > "$T/$name.e.c" 2>"$T/$name.e.err" \
           && cc -O1 -w -o "$T/$name.e.bin" "$T/$name.e.c" $EXTRA 2>"$T/$name.e.cc.err" \
           && "$T/$name.e.bin" > "$T/$name.e.out" 2>&1; then
            pass=$((pass+1)); echo "  PASS $name (emit)"
        else
            fail=$((fail+1)); echo "  FAIL $name (emit)"; sed -n '1,5p' "$T/$name.e.out" "$T/$name.e.cc.err" "$T/$name.e.err" 2>/dev/null
        fi
    done
else
    echo "  [skip] emit 臂:缺 ctron-emit(先: compiler/native.sh)——不入计例,interp 臂已覆盖"
fi

# ── 空集守卫 ──
if [ "$pass" -eq 0 ]; then
    echo "crypto_vec/run: 零 pass(空集或全红),拒判绿" >&2
    exit 1
fi

echo "== crypto_vec: pass=$pass fail=$fail =="
if [ "$fail" -gt 0 ]; then exit 1; fi
exit 0
