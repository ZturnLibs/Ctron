#!/bin/sh
# tests/ffi/dyn_so/run.sh —— G2 真 .so 动态加载专道
# ① harness cc -shared 出插件 libg2plug.so(真实动态库,非同批静态)
# ② Ctron 程序 dlopen_global+#[dlsym] 与 #[dlsym(from_handle)] 双形态调用
# ③ 坏路径负例(句柄 0 + dlerror_str 非空)
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$DIR/../../.." && pwd)
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "dyn_so: 缺编译器二进制" >&2; exit 2; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0

# ① 插件(C 侧,g2plug_val 返回常数;显式 -fPIC 无隐藏符号剥离)
cat > "$T/g2plug.c" << 'EOF'
int g2plug_val(void) { return 4242; }
EOF
cc -O1 -w -fPIC -shared -o "$T/libg2plug.so" "$T/g2plug.c" || { echo "  FAIL dyn_so (插件构建)"; exit 1; }

# ② Ctron 程序(路径经 env 注入;双形态断言)
cat > "$T/probe.ct" << 'EOF'
#[dlsym]
extern "c" fn g2plug_val() -> I32

#[link_name(g2plug_val)]
#[dlsym(from_handle)]
extern "c" fn g2plug_val_h(h: I64) -> I32

fn main() {
    let lib: Str = env_get("G2_SO_PATH")
    let h: I64 = dlopen_global(lib)
    if h == 0 {
        println("dynso-probe err=" + dlerror_str())
        return
    }
    let a = g2plug_val()
    let b = g2plug_val_h(h)
    if a == 4242 && b == 4242 {
        println("dynso-probe ok a=4242 b=4242")
    } else {
        println("dynso-probe bad a={a} b={b}")
    }
}
EOF
CTRON_STDPATH="$ROOT/lib" "$EMIT" run "$T/probe.ct" > "$T/probe.c" 2>/dev/null \
    || { echo "  FAIL dyn_so (emit)"; exit 1; }
cc -O1 -w -pthread -o "$T/probe.bin" "$T/probe.c" || { echo "  FAIL dyn_so (cc)"; fail=1; }

if [ -x "$T/probe.bin" ]; then
    G2_SO_PATH="$T/libg2plug.so" "$T/probe.bin" > "$T/out1" 2>&1
    if grep -q "dynso-probe ok a=4242 b=4242" "$T/out1"; then
        pass=$((pass+1)); echo "  PASS dyn_so (双形态真 .so)"
    else
        echo "  FAIL dyn_so (双形态): $(tail -1 "$T/out1")"; fail=$((fail+1))
    fi
    # ③ 负例
    G2_SO_PATH="/nonexistent_g2.so" "$T/probe.bin" > "$T/out2" 2>&1
    if grep -q "dynso-probe err=" "$T/out2" && ! grep -q "dynso-probe err=$" "$T/out2"; then
        pass=$((pass+1)); echo "  PASS dyn_so (负例:句柄 0+dlerror 非空)"
    else
        echo "  FAIL dyn_so (负例): $(tail -1 "$T/out2")"; fail=$((fail+1))
    fi
fi

# ④ G4:cdylib 产物形态回环(build --lib 出 dylib+export → 第二程序 dlopen 吃回)
CTC="$ROOT/compiler/ctc.sh"
cat > "$T/libexp.ct" << 'EOF'
#[export]
fn dylib_probe_val() -> I32 {
    return 777
}

fn main() {
    println("host")
}
EOF
cat > "$T/loop.ct" << 'EOF'
#[dlsym]
extern "c" fn dylib_probe_val() -> I32

fn main() {
    let h: I64 = dlopen_global("$T/libexp.dylib")
    if h == 0 {
        println("loopback-probe load-err=" + dlerror_str())
        return
    }
    println("loopback-probe v=" + dylib_probe_val().to_string())
}
EOF
if [ "$(uname)" = "Darwin" ]; then
    "$CTC" build "$T/libexp.ct" --lib -o "$T/libexp.dylib" > /dev/null 2>&1
    sed "s|\$T/libexp.dylib|$T/libexp.dylib|" "$T/loop.ct" > "$T/loop2.ct"
    CTRON_STDPATH="$ROOT/lib" "$CTC" build "$T/loop2.ct" -o "$T/loop.bin" > /dev/null 2>&1
    if [ -x "$T/loop.bin" ] && "$T/loop.bin" run "$T/loop2.ct" 2>/dev/null | grep -q "loopback-probe v=777"; then
        pass=$((pass+1)); echo "  PASS dyn_so (cdylib 回环 777)"
    else
        echo "  FAIL dyn_so (cdylib 回环)"; fail=$((fail+1))
    fi
    # ⑤ universal(lipo 双架构)
    if "$CTC" build "$T/libexp.ct" --lib --universal -o "$T/libexp2.dylib" > /dev/null 2>&1 && lipo -info "$T/libexp2.dylib" 2>/dev/null | grep -q "x86_64 arm64"; then
        pass=$((pass+1)); echo "  PASS dyn_so (universal x86_64+arm64)"
    else
        echo "  FAIL dyn_so (universal)"; fail=$((fail+1))
    fi
else
    echo "  SKIP dyn_so (cdylib/universal:Darwin 门控)"
fi

echo "dyn_so: pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
