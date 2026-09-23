#!/bin/sh
# tests/db/nightly/run.sh —— 真靶冒烟驱动(P5-F Task 6;不入 CI 主环)。
# 口径:tests/db/run.sh 主环全夹具回放零真库;本脚本 = 真 Postgres/Redis
# 连接冒烟。env 驱动(CTRON_PG_DSN / CTRON_REDIS_URL),未设则探本机常见口
# (5432/6379,nc -z);可达则真跑并逐行记录,不可达则 SKIP 登记——
# 诚实记录实际发生的一切,不虚构结果。
# 流程:构建门(emit + cc 链自有垫片;无靶也跑 = fixtures 健康检查)
#       → PG 段 / Redis 段逐段 PASS/FAIL/SKIP → 汇总(有 FAIL 即 rc 1)。
# 前置:compiler/native.sh、cc、nc(探活;缺席 = SKIP 探测面并登记)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
CC="$ROOT/compiler/bin/ctron-cc"
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/std"
DBPG="$ROOT/db/c_src/ctron_dbpg.c"
DBENT="$ROOT/db/c_src/ctron_entropy.c"
DBREDIS="$ROOT/db/c_src/ctron_dbredis.c"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
npass=0; nfail=0; nskip=0
echo "== tests/db/nightly 真靶冒烟(P5-F;真 Postgres/Redis;非 CI 主环)=="

# ── 构建门(无靶也跑:smoke 夹具健康检查)──
for name in pg_smoke redis_smoke; do
    if "$EMIT" run "$DIR/$name.ct" > "$T/$name.c" 2>"$T/$name.e.err" \
       && cc -O1 -w -o "$T/$name.bin" "$T/$name.c" $DBPG $DBENT $DBREDIS 2>"$T/$name.cc.err"; then
        npass=$((npass+1)); echo "  PASS build $name"
    else
        nfail=$((nfail+1)); echo "  FAIL build $name(构建门红:emit/cc;非靶面问题也须记录)"; sed -n '1,3p' "$T/$name.e.err" "$T/$name.cc.err" 2>/dev/null
    fi
done

# ── PG 段 ──
PG_DSN="${CTRON_PG_DSN:-}"
if [ -z "$PG_DSN" ]; then
    if command -v nc >/dev/null 2>&1 && nc -z -w 1 127.0.0.1 5432 >/dev/null 2>&1; then
        # 本机 5432 在 listen:以常见开发 DSN 试真跑(结果照实记录;
        # SCRAM 口令认证必需——trust/口令不符将 Auth 面 FAIL 显形)
        PG_DSN="postgres://postgres:postgres@127.0.0.1:5432/postgres"
        echo "  [info] CTRON_PG_DSN 未设,本机 5432 在听,以常见开发 DSN 试跑"
    fi
fi
if [ -z "$PG_DSN" ]; then
    nskip=$((nskip+1)); echo "  [SKIP] pg_smoke: CTRON_PG_DSN 未设且本机 5432 不可达——无真靶,跳过口径登记"
elif [ ! -x "$T/pg_smoke.bin" ]; then
    nskip=$((nskip+1)); echo "  [SKIP] pg_smoke: 构建门未出二进制,不执行"
else
    echo "  [run] pg_smoke: connect+scram+simple+prepared+tx @ ${PG_DSN%%@*}@***"
    if CTRON_PG_DSN="$PG_DSN" "$T/pg_smoke.bin" 2>&1; then
        npass=$((npass+1)); echo "  PASS pg_smoke(真靶全链)"
    else
        nfail=$((nfail+1)); echo "  FAIL pg_smoke(真靶;失败步骤见上方步骤行)"
    fi
fi

# ── Redis 段 ──
REDIS_URL="${CTRON_REDIS_URL:-}"
if [ -z "$REDIS_URL" ]; then
    if command -v nc >/dev/null 2>&1 && nc -z -w 1 127.0.0.1 6379 >/dev/null 2>&1; then
        REDIS_URL="redis://127.0.0.1:6379"
        echo "  [info] CTRON_REDIS_URL 未设,本机 6379 在听,以本机 URL 试跑"
    fi
fi
if [ -z "$REDIS_URL" ]; then
    nskip=$((nskip+1)); echo "  [SKIP] redis_smoke: CTRON_REDIS_URL 未设且本机 6379 不可达——无真靶,跳过口径登记"
elif [ ! -x "$T/redis_smoke.bin" ]; then
    nskip=$((nskip+1)); echo "  [SKIP] redis_smoke: 构建门未出二进制,不执行"
else
    echo "  [run] redis_smoke: connect+set/get+incr+del @ $REDIS_URL"
    if CTRON_REDIS_URL="$REDIS_URL" "$T/redis_smoke.bin" 2>&1; then
        npass=$((npass+1)); echo "  PASS redis_smoke(真靶)"
    else
        nfail=$((nfail+1)); echo "  FAIL redis_smoke(真靶;失败步骤见上方步骤行)"
    fi
fi

echo "== nightly: pass=$npass fail=$nfail skip=$nskip(真靶面;skip = 无靶登记口径)=="
if [ "$npass" -eq 0 ] && [ "$nfail" -eq 0 ]; then
    echo "   (全 SKIP:本机无真靶亦未设 env——登记口径,非绿非红)"
fi
if [ "$nfail" -gt 0 ]; then exit 1; fi
exit 0
