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
    if [ "$name" = "cimport" ]; then
        # cimport 端到端:头文件 → 绑定生成(宿主 seed 驱动;自举 sem 对该源型崩溃在册)
        # → 绑定 + 使用体拼接 → 发射 → 链接 → 运行。使用体单独不可编译(引用生成面)。
        HOST="${CTRON_HOST:-$ROOT/compiler-c/build/ctronc}"
        sed -e "s|ANCHORHEADER|$d/sample.h|" -e "s|ANCHOROUT|$T/$name.bind.ct|" \
            "$ROOT/compiler/tools/cimport.ct" > "$T/$name.tool.ct"
        if ! "$HOST" run "$T/$name.tool.ct" > /dev/null 2>"$T/$name.cimperr"; then
            echo "  [FAIL] $name — cimport 工具失败: $(head -1 "$T/$name.cimperr")"
            fail=$((fail + 1))
            continue
        fi
        cat "$T/$name.bind.ct" "$e" > "$T/$name.all.ct"
        if ! "$EMIT" run "$T/$name.all.ct" > "$T/$name.c" 2>"$T/$name.emiterr"; then
            echo "  [FAIL] $name — emit 失败: $(head -1 "$T/$name.emiterr")"
            fail=$((fail + 1))
            continue
        fi
        cc -O1 -w -o "$T/$name.bin" "$T/$name.c" "$d"/c_src/*.c 2>"$T/$name.ccerr" || {
            echo "  [FAIL] $name — cc 失败: $(head -1 "$T/$name.ccerr")"
            fail=$((fail + 1))
            continue
        }
        if "$T/$name.bin" run "$T/$name.all.ct" > "$T/$name.out" 2>&1; then
            echo "  [ok] $name(生成绑定 → 编译 → 运行)"
            pass=$((pass + 1))
        else
            echo "  [FAIL] $name — 运行 rc=$?: $(head -c 120 "$T/$name.out")"
            fail=$((fail + 1))
        fi
        continue
    fi
    if ! "$EMIT" run "$e" > "$T/$name.c" 2>"$T/$name.emiterr"; then
        echo "  [FAIL] $name — emit 失败: $(head -1 "$T/$name.emiterr")"
        fail=$((fail + 1))
        continue
    fi
    if [ "$name" = "export" ]; then
        # 嵌入面:发射产物自带测试 main,以 -Dmain 重命名让位于 C 宿主 main
        # -Dmain 仅作用于发射产物(分开编译再链;embed.c 自带真 main)
        cc -O1 -w -Dmain=ctron_embed_main -c "$T/$name.c" -o "$T/$name.o" 2>"$T/$name.ccerr" \
            && cc -O1 -w -c "$d/c_src/embed.c" -o "$T/$name.embed.o" \
            && cc -o "$T/$name.bin" "$T/$name.o" "$T/$name.embed.o" || {
            echo "  [FAIL] $name — cc 失败: $(head -1 "$T/$name.ccerr")"
            fail=$((fail + 1))
            continue
        }
        out=$("$T/$name.bin" 2>&1)
        rc=$?
        case "$out" in
            *"export embed OK"*) echo "  [ok] $name(C 宿主直链导出符号)"; pass=$((pass + 1)) ;;
            *) echo "  [FAIL] $name — 嵌入运行 rc=$rc: $(echo "$out" | head -1)"; fail=$((fail + 1)) ;;
        esac
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
