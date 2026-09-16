#!/bin/sh
# tests/ffi/run.sh —— FFI 用例独立验收(spec §9.8:FFI 多文件用例由 tests/ffi/ 承载)
#
# 口径:
#   行为夹具(目录含 c_src/):ctron-emit 发射 C → cc 链接 c_src/*.c → 原生运行 test 块
#   负例(*.neg.ct)        :bin/ctron-cc run 编译失败,诊断含全部 //@ fail: 码
#   lint(*.lint.ct)        :诊断含全部 //@ warn: 码(rc 不判,与 suite.py lint 口径一致)
# 前置:compiler/native.sh(产出 bin/ctron-cc、bin/ctron-emit)、cc。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CC_BIN="$ROOT/compiler/bin/ctron-cc"
EMIT="$ROOT/compiler/bin/ctron-emit"

if [ ! -x "$CC_BIN" ] || [ ! -x "$EMIT" ]; then
    echo "ffi/run: 缺少编译器二进制(先: compiler/native.sh)" >&2
    exit 2
fi

T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
pass=0
fail=0

echo "== tests/ffi FFI 用例(§9.6·§9.8)=="

# ---- 行为夹具:发射 → 链接 → 原生运行 ----
for d in "$DIR"/*/; do
    [ -d "$d/c_src" ] || continue
    name=$(basename "$d")
    [ "$name" = "bench" ] && continue  # 微基准由 compiler/test/bench_ffi.sh 驱动
    e="$d/src/main.ct"
    if ! "$EMIT" run "$e" > "$T/$name.c" 2>"$T/$name.emiterr"; then
        echo "  [FAIL] $name — emit 失败: $(head -1 "$T/$name.emiterr")"
        fail=$((fail + 1))
        continue
    fi
    if ! cc -O1 -w -o "$T/$name.bin" "$T/$name.c" "$d"/c_src/*.c 2>"$T/$name.ccerr"; then
        echo "  [FAIL] $name — cc 失败: $(head -1 "$T/$name.ccerr")"
        fail=$((fail + 1))
        continue
    fi
    if "$T/$name.bin" run "$e" > "$T/$name.out" 2>&1; then
        echo "  [ok] $name"
        pass=$((pass + 1))
    else
        echo "  [FAIL] $name — 运行 rc=$?: $(head -c 120 "$T/$name.out")"
        fail=$((fail + 1))
    fi
done

# ---- 负例 / lint:诊断码 ----
for f in "$DIR"/*.neg.ct "$DIR"/*.lint.ct; do
    [ -f "$f" ] || continue
    name=$(basename "$f")
    out=$("$CC_BIN" run "$f" 2>&1)
    rc=$?
    codes=""
    if [ "${name%.neg.ct}" != "$name" ]; then
        codes=$(grep -o '^//@ fail: [A-Z0-9]*' "$f" | awk '{print $3}' | tr '\n' ' ')
        ok=1
        for c in $codes; do
            case "$out" in
                *"$c"*) ;;
                *) ok=0 ;;
            esac
        done
        if [ $rc -eq 0 ]; then ok=0; fi
    else
        codes=$(grep -o '^//@ warn: [A-Z0-9]*' "$f" | awk '{print $3}' | tr '\n' ' ')
        ok=1
        for c in $codes; do
            case "$out" in
                *"$c"*) ;;
                *) ok=0 ;;
            esac
        done
    fi
    if [ $ok -eq 1 ]; then
        echo "  [ok] $name($codes)"
        pass=$((pass + 1))
    else
        echo "  [FAIL] $name — 期望码[$codes] 输出: $(echo "$out" | head -1)"
        fail=$((fail + 1))
    fi
done

echo "ffi: $pass 过 / $fail 败"
[ $fail -eq 0 ]
