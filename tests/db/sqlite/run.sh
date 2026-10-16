#!/bin/sh
# tests/db/sqlite/run.sh —— G6 sqlite 门面门(P1-7;vendored amalgamation)
# interp 无 extern 运行时 → 行为夹具全走 emit 臂(tests/https 同形);
# interp 臂 = decl smoke(门面本件装载/类型检查,tests/db decl 冒烟同款)。
# amalgamation 3.54.0 一次编译成 .o,各夹具只链接(全量编译 ~秒级,预编译
# .a 缓存挂 P2 vendoring 约定);-DSQLITE_OMIT_LOAD_EXTENSION 省 -ldl,
# Linux 补 -lpthread(THREADSAFE 默认)。
# 夹具断言契约:成功 = 打印 "sqlite-probe ok <tag>" 行;失败 = FAIL 行 +
# 早退(run.sh grep 判,tests/down 家法)。
# 前置:compiler/native.sh、cc
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$DIR/../../.." && pwd)
CC="$ROOT/compiler/bin/ctron-cc"
EMIT="$ROOT/compiler/bin/ctron-emit"
export CTRON_STDPATH="$ROOT/lib/std"
if [ ! -x "$EMIT" ]; then echo "sqlite: 缺少编译器二进制(先: compiler/native.sh)" >&2; exit 2; fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/db/sqlite G6 门面门 =="

# ── interp 臂:decl smoke ──
if env -u CTRON_GC "$CC" run "$ROOT/lib/db/sqlite.ct" > "$T/decl.out" 2>&1; then
    pass=$((pass+1)); echo "  PASS db/sqlite.ct (decl smoke, interp)"
else
    fail=$((fail+1)); echo "  FAIL db/sqlite.ct (decl smoke, interp)"; sed -n '1,5p' "$T/decl.out"
fi

# ── amalgamation 一次编译 ──
SQLSRC="$ROOT/lib/db/c_src/sqlite3.c"
SHIM="$ROOT/lib/db/c_src/ctron_sqlite.c"
LDPKGS=""
case "$(uname -s)" in
    Linux) LDPKGS="-lpthread" ;;
    *) LDPKGS="" ;;
esac
if cc -O1 -w -DSQLITE_OMIT_LOAD_EXTENSION -c "$SQLSRC" -o "$T/sqlite3.o" 2>"$T/sqlite3.cc.err"; then
    pass=$((pass+1)); echo "  PASS sqlite3.c 一次编译(3.54.0)"
else
    echo "  FAIL sqlite3.c 编译"; sed -n '1,5p' "$T/sqlite3.cc.err"; exit 1
fi

build() { # <src.ct> <out.bin>;失败自打诊断,返非零
    CTRON_STDPATH="$ROOT/lib/std" "$EMIT" run "$1" > "$T/prog.c" 2>"$T/prog.err" \
        || { echo "    emit: $(tail -1 "$T/prog.err")"; return 1; }
    cc -O1 -w -o "$2" "$T/prog.c" "$T/sqlite3.o" "$SHIM" $LDPKGS 2>"$T/prog.cc.err" \
        || { echo "    cc: $(tail -1 "$T/prog.cc.err")"; return 1; }
    return 0
}

# ── emit 臂:行为夹具(成功标记 = sqlite-probe ok <tag>;tag = 文件名去 s_)──
export CT_SQLITE_FILE="$T/probe.db"
for f in "$DIR"/s_*.ct "$DIR"/use_smoke.ct; do
    name=$(basename "$f" .ct)
    tag=${name#s_}
    rm -f "$CT_SQLITE_FILE"
    if build "$f" "$T/$name.bin" \
       && "$T/$name.bin" > "$T/$name.out" 2>&1 \
       && grep -q "sqlite-probe ok $tag" "$T/$name.out"; then
        pass=$((pass+1)); echo "  PASS $name (emit)"
    else
        fail=$((fail+1)); echo "  FAIL $name (emit)"
        sed -n '1,4p' "$T/$name.out" 2>/dev/null
    fi
done

# ── 空集守卫 ──
if [ "$pass" -eq 0 ]; then
    echo "sqlite/run: 零 pass(空集或全红),拒判绿" >&2
    exit 1
fi

echo "== sqlite: pass=$pass fail=$fail =="
if [ "$fail" -gt 0 ]; then exit 1; fi
exit 0
