#!/bin/sh
# tests/db/run.sh —— std/db 数据访问层回放验收(P5-C/P5-D/P5-E;§P5 Task 3/4/5)
# 口径:std/db(pg.ct/db.ct/redis.ct/pool.ct/rowmap.ct)为纯 Ctron 叶
# (不触 std.net;fd 真源经自有垫片 std/db/c_src/ctron_dbpg.c +
# ctron_dbredis.c;P5-D SCRAM 单子扩展 std.crypto;P5-E pool 单子扩展
# std.db.pg、rowmap 单子扩展 std.db.pg——严格互斥树:消费方对
# pool/rowmap/pg 三门面**二选一** use,单路径导入树,直 use pg 而复用
# pool/rowmap 即 E5020 环),
# tests/crypto_vec 同款双臂跑法:
#   主环 = 解释器(ctron-cc run):corpus/*.ct + replay_scram/s_*.ct +
#          redis_replay/rr_*.ct + pool/*.ct + rowmap/*.ct 断言式夹具逐文件
#          跑(x_ 除外;回放夹具经 read_file 内建装载 *.script,
#          CT_DB_FIX/CT_DB_SCRAM/CT_DB_REDIS/CT_DB_POOL/CT_DB_ROW 指根)。
#   副 = emit 同源对拍(ctron-emit → cc → 原生):各目录全量含 x_。
#   x_ 前缀 = 仅 emit 臂入计(tests/crypto_vec x_uuid_live 同款结构性登记):
#          x_fd_edge / x_fd_pipeline 承载 fd 真源面(EBADF → err 6 + errno 槽;管线帧 mtype/fill 持态/send-all 整发三钉)——
#          interp 无 extern 运行时,需链 std/db/c_src/ctron_dbpg.c。
#          fd 长度域门(读前即拒,不触 extern)已由 e_protocol_violation
#          双臂承载;真库 fd 冒烟 Task 6 nightly。
#          P5-D:x_scram_neg(错口令/签名不符两条 SCRAM 重链)、
#          x_scram_rfc7677(RFC 7677 原例 c=4096)——解释器堆不回收
#          (P5-B 登记;实测 ≈1GB/块压缩,s_ 单链文件已抵预算线);
#          x_scram_nonce(nonce 真熵,interp 无 extern 运行时)链
#          ctron_dbpg.c + ctron_entropy.c(ctron_dbpg_entropy 别名转发)。
#          P5-E:x_rd_fd(Redis fd 真源:P5-F 真分片跨两次 recv(incomplete
#          面)+ incomplete 三臂 + 管线包 fill 持态/send-all 整发钉)链
#          ctron_dbredis.c(与 ctron_dbpg.c 分置:同名 extern decl 合并
#          即 E5030;socketpair 两端 O_NONBLOCK,P5-F)。
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
export CT_DB_SCRAM="$DIR/replay_scram"
export CT_DB_REDIS="$DIR/redis_replay"
export CT_DB_POOL="$DIR/pool"
export CT_DB_ROW="$DIR/rowmap"
if [ ! -x "$CC" ]; then echo "db/run: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/db std/db 数据访问层回放用例(P5-C/P5-D/P5-E)=="

# ── std 规范源与种子副本漂移守卫(smoke.sh 3d 同款,本地即查)──
for m in "db/pg" "db/db" "db/redis" "db/pool" "db/rowmap"; do
    base=$(basename "$m")
    if diff -q "$ROOT/std/$m.ct" "$ROOT/compiler/test/stdpkg/std/$m.ct" > /dev/null 2>&1; then
        pass=$((pass+1)); echo "  PASS std/$m.ct 种子副本无漂移"
    else
        fail=$((fail+1)); echo "  FAIL std/$m.ct 种子副本漂移(cp std/$m.ct compiler/test/stdpkg/std/)"
    fi
done

# ── std/db 声明冒烟(无 inline test 块;装载/类型检查即过,C17 口径)──
for m in pg db redis pool rowmap; do
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
nsc=$(ls "$CT_DB_SCRAM"/*.script 2>/dev/null | wc -l | tr -d ' ')
if [ "$nsc" -ge 12 ]; then
    pass=$((pass+1)); echo "  PASS replay_scram 脚本在位($nsc 个)"
else
    fail=$((fail+1)); echo "  FAIL replay_scram 脚本缺失($nsc < 12)"
fi
nr=$(ls "$CT_DB_REDIS"/*.script 2>/dev/null | wc -l | tr -d ' ')
if [ "$nr" -ge 8 ]; then
    pass=$((pass+1)); echo "  PASS redis_replay 脚本在位($nr 个)"
else
    fail=$((fail+1)); echo "  FAIL redis_replay 脚本缺失($nr < 8)"
fi
np=$(ls "$CT_DB_POOL"/*.script 2>/dev/null | wc -l | tr -d ' ')
if [ "$np" -ge 4 ]; then
    pass=$((pass+1)); echo "  PASS pool 脚本在位($np 个)"
else
    fail=$((fail+1)); echo "  FAIL pool 脚本缺失($np < 4)"
fi
nm=$(ls "$CT_DB_ROW"/*.script 2>/dev/null | wc -l | tr -d ' ')
if [ "$nm" -ge 1 ]; then
    pass=$((pass+1)); echo "  PASS rowmap 脚本在位($nm 个)"
else
    fail=$((fail+1)); echo "  FAIL rowmap 脚本缺失($nm < 1)"
fi

# ── 主环:解释器(x_ 仅 emit 臂,不入计)──
for f in "$DIR"/corpus/*.ct "$DIR"/replay_scram/s_*.ct \
         "$DIR"/redis_replay/rr_*.ct "$DIR"/pool/*.ct "$DIR"/rowmap/*.ct; do
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

# ── 副:emit 对拍(含 x_;fd 源面/熵面链 db 垫片)──
if [ -x "$EMIT" ]; then
    for f in "$DIR"/corpus/*.ct "$DIR"/replay_scram/*.ct \
             "$DIR"/redis_replay/*.ct "$DIR"/pool/*.ct "$DIR"/rowmap/*.ct; do
        name=$(basename "$f" .ct)
        EXTRA=""
        if [ "$name" = "x_fd_edge" ] || [ "$name" = "x_fd_pipeline" ] || [ "$name" = "e_protocol_violation" ]; then
            # fd 源面(extern 引用进 emit C):链 std/db 自有垫片
            # (ctron_dbpg_entropy 别名引 ctron_entropy_fill → 并链熵垫片)
            EXTRA="$ROOT/std/db/c_src/ctron_dbpg.c $ROOT/std/db/c_src/ctron_entropy.c"
        fi
        if [ "$name" = "x_scram_nonce" ]; then
            # nonce 真熵:ctron_dbpg_entropy 别名转发 → 并链熵垫片
            EXTRA="$ROOT/std/db/c_src/ctron_dbpg.c $ROOT/std/db/c_src/ctron_entropy.c"
        fi
        if [ "$name" = "x_rd_fd" ]; then
            # Redis fd 真源(P5-E):自有垫片(ctron_dbredis_*;与 dbpg 分置
            # ——同名 extern decl 合并即 E5030)
            EXTRA="$ROOT/std/db/c_src/ctron_dbredis.c"
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
