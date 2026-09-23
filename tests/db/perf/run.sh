#!/bin/sh
# tests/db/perf/run.sh —— 简单查询回环性能门(P5-F Task 6 F-C;nightly 专面,
# 不入 CI 主环)。口径:ctron(pg.ct fd 真源全链)vs libpq 同构基线
# (baseline_libpq.c:connect + N × PQexec("SELECT 1") 逐发逐收逐校验),
# 两臂同构含连接/认证段(N 默认 1000 摊薄),整进程墙钟 min-of-3,
# **比值 ≤ 1.5× 为过**。
# 登记口径:无本地 libpq 开发面(pg_config/头文件/库)或无真靶 PG →
# **SKIP 登记**,逐行输出缺失前置,不虚构数字。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
CC="$ROOT/compiler/bin/ctron-cc"
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/std"
DBPG="$ROOT/db/c_src/ctron_dbpg.c"
DBENT="$ROOT/db/c_src/ctron_entropy.c"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
N="${CTRON_PERF_N:-1000}"
echo "== tests/db/perf 简单查询回环性能门(P5-F F-C;ctron vs libpq 同构 ≤1.5×,min-of-3)=="

# ── 前置 1:libpq 开发面探测(pg_config → brew 常见路径)──
PQ_INC=""; PQ_LIB=""
if command -v pg_config >/dev/null 2>&1; then
    PQ_INC="$(pg_config --includedir 2>/dev/null)"
    PQ_LIB="$(pg_config --libdir 2>/dev/null)"
fi
if [ -z "$PQ_INC" ] || [ ! -f "$PQ_INC/libpq-fe.h" ]; then
    for base in /opt/homebrew/opt/libpq /usr/local/opt/libpq; do
        if [ -f "$base/include/libpq-fe.h" ]; then
            PQ_INC="$base/include"; PQ_LIB="$base/lib"; break
        fi
    done
fi
if [ -z "$PQ_INC" ] || [ ! -f "$PQ_INC/libpq-fe.h" ]; then
    echo "  [SKIP] perf: 缺本地 libpq 开发面——pg_config 不可用且常见路径无 libpq-fe.h(缺失前置行:libpq-fe.h / libpq 库不可得)"
    echo "  [SKIP] perf: 登记跳过口径——无 libpq 基线不可对拍,不虚构数字;安装(psql dev / brew install libpq)后重跑"
    exit 0
fi
if [ ! -f "$PQ_LIB/libpq.dylib" ] && [ ! -f "$PQ_LIB/libpq.so" ]; then
    echo "  [SKIP] perf: libpq 头在($PQ_INC)而库缺席($PQ_LIB)——缺失前置行:libpq 库文件"
    echo "  [SKIP] perf: 登记跳过口径,不虚构数字"
    exit 0
fi
echo "  [info] libpq: inc=$PQ_INC lib=$PQ_LIB"

# ── 前置 2:真靶 PG(性能门需真库回环)──
PG_DSN="${CTRON_PG_DSN:-}"
if [ -z "$PG_DSN" ] && command -v nc >/dev/null 2>&1 && nc -z -w 1 127.0.0.1 5432 >/dev/null 2>&1; then
    PG_DSN="postgres://postgres:postgres@127.0.0.1:5432/postgres"
    echo "  [info] CTRON_PG_DSN 未设,本机 5432 在听,以常见开发 DSN 试跑"
fi
if [ -z "$PG_DSN" ]; then
    echo "  [SKIP] perf: CTRON_PG_DSN 未设且本机 5432 不可达——缺失前置行:可连真靶 PG"
    echo "  [SKIP] perf: 登记跳过口径,不虚构数字"
    exit 0
fi

# ── 前置 3:python3(计时壳;macOS date 无 %N)──
if ! command -v python3 >/dev/null 2>&1; then
    echo "  [SKIP] perf: 缺 python3(计时壳)——缺失前置行:python3"
    echo "  [SKIP] perf: 登记跳过口径,不虚构数字"
    exit 0
fi

# ── 构建:ctron 臂 + libpq 基线 ──
if ! "$EMIT" run "$DIR/ctron_simple_query.ct" > "$T/ctron.c" 2>"$T/ctron.e.err" \
   || ! cc -O1 -w -o "$T/ctron.bin" "$T/ctron.c" $DBPG $DBENT 2>"$T/ctron.cc.err"; then
    echo "  [FAIL] perf: ctron 臂构建红(emit/cc;见下行)"; sed -n '1,3p' "$T/ctron.e.err" "$T/ctron.cc.err" 2>/dev/null
    exit 1
fi
if ! cc -O1 -w -I"$PQ_INC" -L"$PQ_LIB" -o "$T/baseline.bin" "$DIR/baseline_libpq.c" -lpq 2>"$T/base.cc.err"; then
    echo "  [FAIL] perf: libpq 基线构建红"; sed -n '1,5p' "$T/base.cc.err"
    exit 1
fi
echo "  PASS build ctron 臂 + libpq 基线"

# macOS 运行期找 libpq(brew keg 非默认搜索路径)
case "$(uname)" in
    Darwin) export DYLD_LIBRARY_PATH="$PQ_LIB${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}" ;;
esac

# ── 预热 + min-of-3(两臂同构整进程墙钟;python 计时壳)──
"$T/baseline.bin" 1 "$PG_DSN" >/dev/null 2>&1 || true
time3() {  # $1 = bin;stdout = ms int(min-of-3)
    python3 - "$1" "$N" "$PG_DSN" <<'PY'
import subprocess, sys, time
bin_, n, dsn = sys.argv[1], sys.argv[2], sys.argv[3]
best = None
for _ in range(3):
    t0 = time.time()
    r = subprocess.run([bin_, n, dsn], capture_output=True, text=True)
    dt = int((time.time() - t0) * 1000)
    if r.returncode != 0:
        sys.stderr.write(r.stderr.strip()[:200])
        sys.exit(1)
    if best is None or dt < best: best = dt
print(best)
PY
}
BASE_MS=$(time3 "$T/baseline.bin")
CT_MS=$(time3 "$T/ctron.bin")
if ! [ "$BASE_MS" -gt 0 ] 2>/dev/null || ! [ "$CT_MS" -gt 0 ] 2>/dev/null; then
    echo "  [FAIL] perf: 计时面红(base=$BASE_MS ctron=$CT_MS;先核真靶可用性)"
    exit 1
fi
# 比值 ×100 整数面(避免浮点口径):ctron/base ≤ 150
RATIO=$((CT_MS * 100 / BASE_MS))
echo "  [result] N=$N min-of-3: libpq=${BASE_MS}ms ctron=${CT_MS}ms ratio=${RATIO}%"
if [ "$RATIO" -le 150 ]; then
    echo "  PASS perf gate: ctron/libpq = ${RATIO}% ≤ 150%"
    exit 0
fi
echo "  [FAIL] perf gate: ctron/libpq = ${RATIO}% > 150%"
exit 1
