#!/bin/sh
# tests/win/smoke.sh —— G7 Windows 链路冒烟道(设计 §9;双平台同源脚本)
# Windows 腿 = ci.yml windows-smoke job 的实质载体(msys2 MINGW64 + 预发射 C
# 出的四件:ctron-cc/emit/chk.exe + ctron.exe 驱动);POSIX 腿 = 本地回归哨
# (同探针 dlfcn 路,任何编译器面 Windows 垫片改动后本地即可复验探针逻辑)。
# 五腿:
#   A hello      emit→cc→run(工具链线活性)
#   B dlopen DLL gcc -shared 插件 + dlopen_global/#[dlsym(from_handle)] 双件 +
#                dlerror_str 负例(Windows = LoadLibraryW 宽字符真运行)
#   C 并发       scope/spawn/Channel(06 家法核心;winpthreads 承载)
#   D __sync 钉  Atomic fetch_add/load/store(发射 __sync 内建 mingw 可用性)
#   E CJK 路径   CJK 目录下源码 emit + 产物落位 + DLL dlopen(三路径全链;
#                Windows = CP_UTF8→LoadLibraryW 转换真验证)
# 挂账(在册):RTLD_DEFAULT exe-export 符号面须 -Wl,--export-all-symbols;
# cdylib+DLL 搜索路径(Darwin 门控先例 tests/ffi/dyn_so;Windows 入册待驱动器
# cdylib 面首证);coro RT 不入 Windows 冒烟(排除项)。
# 前置:ctron-emit(CC 侧件)+ cc/gcc。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$DIR/../.." && pwd)
EMIT="${CTRON_EMIT:-}"
if [ -z "$EMIT" ]; then
    for c in "$ROOT/compiler/bin/ctron-emit" "$ROOT/compiler/bin/ctron-emit.exe"; do
        if [ -x "$c" ]; then EMIT="$c"; break; fi
    done
fi
[ -n "$EMIT" ] && [ -x "$EMIT" ] || { echo "win-smoke: 缺 ctron-emit(先: compiler/native.sh;CI 由 prebuilt 件出)" >&2; exit 2; }
CC="${CTRON_SMOKE_CC:-}"
if [ -z "$CC" ]; then
    CC=cc
    command -v cc > /dev/null 2>&1 || CC=gcc
fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
export CTRON_STDPATH="$ROOT/lib/std"
echo "== tests/win G7 冒烟道(uname=$(uname -s))=="

# msys2 下原生 exe 不识 POSIX 路径:env 注入的 DLL/产物路径一律过 cygpath -m
winpath() {
    case "$(uname -s)" in
        *MINGW*|*MSYS*|*CYGWIN*) cygpath -m "$1" ;;
        *) printf '%s' "$1" ;;
    esac
}

build_bin() { # <src.ct> <out.bin> —— emit+cc 一步;失败自诊断返非零
    "$EMIT" run "$1" > "$T/prog.c" 2>"$T/prog.err" \
        || { echo "    emit: $(tail -1 "$T/prog.err")"; return 1; }
    "$CC" -O1 -w -pthread -o "$2" "$T/prog.c" 2>"$T/prog.cc.err" \
        || { echo "    cc: $(tail -1 "$T/prog.cc.err")"; return 1; }
    return 0
}

# ── A hello ──
cat > "$T/hello.ct" << 'EOF'
fn main() {
    println("win-probe ok hello")
}
EOF
if build_bin "$T/hello.ct" "$T/hello.bin" && "$T/hello.bin" 2>&1 | grep -q "win-probe ok hello"; then
    pass=$((pass+1)); echo "  PASS A hello"
else
    fail=$((fail+1)); echo "  FAIL A hello"
fi

# ── B dlopen DLL(插件名按平台;路径 env 注入过 winpath)──
case "$(uname -s)" in
    Darwin) DLL=fx.dylib; SOPT=-dynamiclib ;;
    *MINGW*|*MSYS*|*CYGWIN*) DLL=fx.dll; SOPT=-shared ;;
    *) DLL=fx.so; SOPT=-shared ;;
esac
cat > "$T/fx.c" << 'EOF'
#include <stdint.h>
int64_t fx_add(int64_t a, int64_t b) { return a + b; }
const char* fx_tag(void) { return "fx-g7"; }
EOF
"$CC" -O1 -w -fPIC $SOPT -o "$T/$DLL" "$T/fx.c" 2>"$T/fx.err" \
    || { echo "  FAIL B 插件构建: $(tail -1 "$T/fx.err")"; exit 1; }
cat > "$T/dl.ct" << 'EOF'
#[link_name(fx_add)]
#[dlsym(from_handle)]
extern "c" fn fx_add_h(h: I64, a: I64, b: I64) -> I64

#[link_name(fx_tag)]
#[dlsym(from_handle)]
extern "c" fn fx_tag_h(h: I64) -> Str

fn main() {
    let lib: Str = env_get("G7_DLL")
    let h: I64 = dlopen_global(lib)
    if h == 0 {
        println("win-probe dl-err=" + dlerror_str())
        return
    }
    let s = fx_add_h(h, 40, 2)
    let t = fx_tag_h(h)
    if s == 42 && t == "fx-g7" {
        println("win-probe ok dll add=42 tag=fx-g7")
    } else {
        println("win-probe bad dll add=" + s.to_string() + " tag=" + t)
    }
}
EOF
if build_bin "$T/dl.ct" "$T/dl.bin"; then
    if G7_DLL="$(winpath "$T/$DLL")" "$T/dl.bin" 2>&1 | grep -q "win-probe ok dll add=42"; then
        pass=$((pass+1)); echo "  PASS B dlopen DLL"
    else
        fail=$((fail+1)); echo "  FAIL B dlopen DLL"
    fi
    if G7_DLL="$(winpath "$T/no_such_g7.dll")" "$T/dl.bin" 2>&1 | grep -q "win-probe dl-err=.."; then
        pass=$((pass+1)); echo "  PASS B dlerror 负例"
    else
        fail=$((fail+1)); echo "  FAIL B dlerror 负例(须非空)"
    fi
else
    fail=$((fail+1)); echo "  FAIL B dlopen DLL(构建败)"
fi

# ── C 并发(scope/spawn/Channel;06 家法)──
cat > "$T/conc.ct" << 'EOF'
fn main() {
    let sum = scope { |s|
        let (tx, rx) = Channel[I32](8)
        s.spawn(|| {
            for i in 0..10 {
                tx.send(i * 7).expect("send")
            }
        })
        var acc: I32 = 0
        for k in 0..10 {
            acc += rx.recv().expect("recv")
        }
        acc
    }
    if sum == 315 {
        println("win-probe ok conc sum=315")
    } else {
        println("win-probe FAIL conc sum=" + sum.to_string())
    }
}
EOF
if build_bin "$T/conc.ct" "$T/conc.bin" && "$T/conc.bin" 2>&1 | grep -q "win-probe ok conc"; then
    pass=$((pass+1)); echo "  PASS C 并发"
else
    fail=$((fail+1)); echo "  FAIL C 并发"
fi

# ── D __sync 钉(Atomic 发射 __sync 内建;mingw 可用性钉形)──
cat > "$T/atom.ct" << 'EOF'
fn main() {
    let n = Atomic[I32](41)
    let old = n.fetch_add(1)
    if old == 41 && n.load() == 42 {
        println("win-probe ok atomic 42")
    } else {
        println("win-probe FAIL atomic")
    }
}
EOF
if build_bin "$T/atom.ct" "$T/atom.bin" && "$T/atom.bin" 2>&1 | grep -q "win-probe ok atomic"; then
    pass=$((pass+1)); echo "  PASS D __sync Atomic"
else
    fail=$((fail+1)); echo "  FAIL D __sync Atomic"
fi

# ── E CJK 路径(CJK 目录:CJK 源码路径 emit + 产物落位 + DLL dlopen)──
CJKDIR="$T/中文目录-確認"
mkdir -p "$CJKDIR"
cat > "$CJKDIR/你好.ct" << 'EOF'
fn main() {
    println("win-probe ok cjk-src")
}
EOF
if build_bin "$CJKDIR/你好.ct" "$CJKDIR/你好.bin" \
   && "$CJKDIR/你好.bin" 2>&1 | grep -q "win-probe ok cjk-src" \
   && cp "$T/$DLL" "$CJKDIR/$DLL" \
   && G7_DLL="$(winpath "$CJKDIR/$DLL")" "$T/dl.bin" 2>&1 | grep -q "win-probe ok dll add=42"; then
    pass=$((pass+1)); echo "  PASS E CJK 路径三件(源码/产物/dlopen)"
else
    fail=$((fail+1)); echo "  FAIL E CJK 路径"
fi

if [ "$pass" -eq 0 ]; then
    echo "win-smoke: 零 pass(空集或全红),拒判绿" >&2
    exit 1
fi
echo "win-smoke: pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
