#!/bin/sh
# tests/db/run.sh —— std/db Postgres 线协议回放验收(P5-C;§P5 Task 3)
# 口径:std/db(pg.ct/db.ct)为零 use 纯 Ctron 叶(不触 std.net;fd 真源
# 经自有垫片 std/db/c_src/ctron_dbpg.c),tests/crypto_vec 同款双臂跑法:
#   主环 = 解释器(ctron-cc run):corpus/*.ct 断言式夹具逐文件跑(x_ 除外;
#          回放夹具经 read_file 内建装载 *.script,CT_DB_FIX 指根)。
#   副 = emit 同源对拍(ctron-emit → cc → 原生):corpus/*.ct 含 x_。
#   x_ 前缀 = 仅 emit 臂入计(tests/crypto_vec x_uuid_live 同款结构性登记):
#          x_fd_edge / x_fd_pipeline 承载 fd 真源面(EBADF → err 6 + errno 槽;管线帧 mtype/fill 持态/send-all 整发三钉)——
#          interp 无 extern 运行时,需链 std/db/c_src/ctron_dbpg.c。
#          fd 长度域门(读前即拒,不触 extern)已由 e_protocol_violation
#          双臂承载;真库 fd 冒烟 Task 6 nightly。
#   两臂同一夹具集双计 pass;任一臂红即红。pass>0 空集守卫。
#   双运行时矩阵(CTRON_RT=coro)不适用:纯层无停车点,无 rt 依赖。
# 前置:compiler/native.sh、cc(副臂)
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CC="$ROOT/compiler/bin/ctron-cc"
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/std"
export CT_DB_FIX="$DIR/replay_fixtures"
if [ ! -x "$CC" ]; then echo "db/run: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/db Postgres 线协议回放用例(P5-C)=="

# ── std 规范源与种子副本漂移守卫(smoke.sh 3d 同款,本地即查)──
for m in "db/pg" "db/db"; do
    base=$(basename "$m")
    if diff -q "$ROOT/std/$m.ct" "$ROOT/compiler/test/stdpkg/std/$m.ct" > /dev/null 2>&1; then
        pass=$((pass+1)); echo "  PASS std/$m.ct 种子副本无漂移"
    else
        fail=$((fail+1)); echo "  FAIL std/$m.ct 种子副本漂移(cp std/$m.ct compiler/test/stdpkg/std/)"
    fi
done

# ── std/db 声明冒烟(无 inline test 块;装载/类型检查即过,C17 口径)──
for m in pg db; do
    if "$CC" run "$ROOT/std/db/$m.ct" > "$T/std_$m.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS std/db/$m.ct (decl smoke, interp)"
    else
        fail=$((fail+1)); echo "  FAIL std/db/$m.ct (decl smoke, interp)"; sed -n '1,5p' "$T/std_$m.out"
    fi
done

# ── 回放夹具在位守卫(空集防呆:脚本面即规范消费面)──
nf=$(ls "$CT_DB_FIX"/*.script 2>/dev/null | wc -l | tr -d ' ')
if [ "$nf" -ge 8 ]; then
    pass=$((pass+1)); echo "  PASS replay_fixtures 脚本在位($nf 个)"
else
    fail=$((fail+1)); echo "  FAIL replay_fixtures 脚本缺失($nf < 8)"
fi

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

# ── corpus:副臂 = emit 对拍(含 x_;fd 源面链 db 垫片)──
if [ -x "$EMIT" ]; then
    for f in "$DIR"/corpus/*.ct; do
        name=$(basename "$f" .ct)
        EXTRA=""
        if [ "$name" = "x_fd_edge" ] || [ "$name" = "x_fd_pipeline" ] || [ "$name" = "e_protocol_violation" ]; then
            # fd 源面(extern 引用进 emit C):链 std/db 自有垫片
            EXTRA="$ROOT/std/db/c_src/ctron_dbpg.c"
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
    echo "db/run: 零 pass(空集或全红),拒判绿" >&2
    exit 1
fi

echo "== db: pass=$pass fail=$fail =="
if [ "$fail" -gt 0 ]; then exit 1; fi
exit 0
