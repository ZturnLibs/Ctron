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
    [ -d "$d" ] || continue
    name=$(basename "$d")
    [ "$name" = "bench" ] && continue  # 微基准由 compiler/test/bench_ffi.sh 驱动
    if [ ! -d "$d/src" ]; then continue; fi
    [ "$name" = "link_math" ] && continue  # #[link] 面由下方 link_math 专道驱动(无 c_src)
    [ "$name" = "pkgconf" ] && continue   # pkg-config 解析面由下方 pkgconf 专道驱动(假 .pc,不可真链)
    [ "$name" = "err_wrap" ] && continue  # std.ffi 包装面由下方 err_wrap 专道驱动(须 CTRON_STDPATH)
    if [ ! -d "$d/c_src" ]; then
        # 纯 libc 夹具(无 c_src):emit → cc → 运行(errno_basics 等)
        e0="$d/src/main.ct"
        if "$EMIT" run "$e0" > "$T/$name.c" 2>"$T/$name.emiterr" \
           && cc -O1 -w -o "$T/$name.bin" "$T/$name.c" 2>"$T/$name.ccerr" \
           && "$T/$name.bin" run "$e0" > "$T/$name.out" 2>&1; then
            echo "  [ok] $name(纯 libc)"
            pass=$((pass + 1))
        else
            echo "  [FAIL] $name — $(head -1 "$T/$name.ccerr" 2>/dev/null)$(head -1 "$T/$name.emiterr" 2>/dev/null)"
            fail=$((fail + 1))
        fi
        continue
    fi
    e="$d/src/main.ct"
    if [ "$name" = "cimport" ]; then
        # cimport 端到端:头文件 → 绑定生成(自举 ctron-cc 原生驱动;#11② 修复后
        # 崩溃消除,seed 退化备用)→ 绑定 + 使用体拼接 → 发射 → 链接 → 运行。
        # 使用体单独不可编译(引用生成面)。
        HOST="${CTRON_HOST:-$ROOT/compiler-c/build/ctronc}"
        sed -e "s|ANCHORHEADER|$d/sample.h|" -e "s|ANCHOROUT|$T/$name.bind.ct|" \
            "$ROOT/compiler/tools/cimport.ct" > "$T/$name.tool.ct"
        if ! "$CC_BIN" run "$T/$name.tool.ct" > /dev/null 2>"$T/$name.cimperr" \
           && ! "$HOST" run "$T/$name.tool.ct" > /dev/null 2>>"$T/$name.cimperr"; then
            echo "  [FAIL] $name — cimport 工具失败: $(head -1 "$T/$name.cimperr")"
            fail=$((fail + 1))
            continue
        fi
        if ! grep -q "cimport: 跳过(位域" "$T/$name.bind.ct"; then
            echo "  [FAIL] $name — 位域 struct 未整构跳过"
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
    "$T/$name.bin" run "$e" > "$T/$name.out" 2>&1
    rrc=$?
    pmark=$(grep -o '^//@ panic: .*' "$e" | head -1 | sed 's|^//@ panic: ||')
    if [ $rrc -eq 0 ] && [ -z "$pmark" ]; then
        echo "  [ok] $name"
        pass=$((pass + 1))
    elif [ -n "$pmark" ] && [ $rrc -ne 0 ] && grep -q "$pmark" "$T/$name.out"; then
        echo "  [ok] $name(panic 语义: $pmark)"
        pass=$((pass + 1))
    else
        echo "  [FAIL] $name — 运行 rc=$rrc: $(head -c 120 "$T/$name.out")"
        fail=$((fail + 1))
    fi
done

# ---- link_math:#[link] 标记 → cc 参数(无 c_src;libm 系统库) ----
ld="$DIR/link_math"
if [ -d "$ld/src" ]; then
    e="$ld/src/main.ct"
    if "$EMIT" run "$e" > "$T/link_math.c" 2>"$T/link_math.emiterr" \
       && grep -q "ctron:link -lm" "$T/link_math.c"; then
        lflags=$(grep -o "ctron:link -l[^ ]*" "$T/link_math.c" | awk '{print $2}' | tr '\n' ' ')
        cc -O1 -w -o "$T/link_math.bin" "$T/link_math.c" $lflags 2>"$T/link_math.ccerr" \
            && "$T/link_math.bin" run "$e" > "$T/link_math.out" 2>&1
        if [ $? -eq 0 ]; then
            echo "  [ok] link_math(#[link] 标记 → $lflags)"
            pass=$((pass + 1))
        else
            echo "  [FAIL] link_math — 标记/链接/运行失败: $(head -c 120 "$T/link_math.out" 2>/dev/null)$(head -1 "$T/link_math.ccerr" 2>/dev/null)"
            fail=$((fail + 1))
        fi
    else
        echo "  [FAIL] link_math — emit 失败或缺 ctron:link 标记"
        fail=$((fail + 1))
    fi
fi

# ---- pkgconf:ctc.sh #[link] 标记 → pkg-config 解析(假 .pc,hermetic) ----
pcd="$DIR/pkgconf"
if [ -d "$pcd/src" ]; then
    if command -v pkg-config >/dev/null 2>&1; then
        if PKG_CONFIG_PATH="$pcd/pc" sh "$ROOT/compiler/ctc.sh" emit "$pcd/src/main.ct" "$T/pkgconf.c" > "$T/pkgconf.log" 2>&1 \
           && grep -q "lfakeextra" "$T/pkgconf.log"; then
            echo "  [ok] pkgconf(pkg-config 解析 -lfakefoo → -lfakefoo -lfakeextra)"
            pass=$((pass + 1))
        else
            echo "  [FAIL] pkgconf — 解析输出缺 -lfakeextra: $(grep 'pkg-config' "$T/pkgconf.log" | head -1)"
            fail=$((fail + 1))
        fi
    else
        echo "  [ok] pkgconf(pkg-config 不在册,跳过解析断言)"
        pass=$((pass + 1))
    fi
fi

# ---- err_wrap:std.ffi Result 包装(std 路径 + 编译通道) ----
ewd="$DIR/err_wrap"
if [ -d "$ewd/src" ]; then
    e="$ewd/src/main.ct"
    if CTRON_STDPATH="$ROOT/std" "$EMIT" run "$e" > "$T/ew.c" 2>"$T/ew.emiterr" \
       && cc -O1 -w -o "$T/ew.bin" "$T/ew.c" "$ROOT"/ffi/c_src/*.c 2>"$T/ew.ccerr" \
       && "$T/ew.bin" run "$e" > "$T/ew.out" 2>&1; then
        echo "  [ok] err_wrap(std.ffi sys_result Ok/Err 双臂 + strerror 深拷)"
        pass=$((pass + 1))
    else
        echo "  [FAIL] err_wrap — $(head -1 "$T/ew.emiterr" 2>/dev/null)$(head -1 "$T/ew.ccerr" 2>/dev/null)$(head -c 120 "$T/ew.out" 2>/dev/null)"
        fail=$((fail + 1))
    fi
fi

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
