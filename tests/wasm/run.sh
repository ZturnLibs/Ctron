#!/bin/sh
# tests/wasm/run.sh —— wasm 档验收门(T37 §9.2;T38 桥/T39 dom 扩腿)
#
# 门序:
#   ① ctc.sh targets 注册表面(wasm32 在册)
#   ② hello:.ct→C→wasm32→node 真跑,输出断言(MVP 主链验收)
#   ③ arith:循环/I64/F64/递归真跑断言(无分配 core 子集)
#   ④ strfmt:插值/拼接真跑断言(GC arena→16MiB 静态池分配面)
#   ⑤ gcload:5 万次插值 churn 分配压力真跑(T38 宿主 GC 压力同源锚;线性内存 MVP)
#   ⑥ dom:set_title/set_body 桥真跑断言(T39;node=host shim,浏览器页=真 DOM 自检)
#   ⑦ ffi 负锚:#[link] 面 wasm target 必拒(诊断非静默,rc=2)
#   ⑧ 体积报告(.wasm 字节;web 档无 100KB 口径,仅报告)
#
# node 通道:PATH → 打包内置运行时通配(Raycast 式)→ docker node 镜像;
# 全缺 = 打 SKIP 摘要 exit 0(环境登记,非门红);有 node 则门红不豁免。
# 编译工具链随 ctc.sh 三通道(env CTRON_WASM_CC → 宿主 clang[wasm32] → docker ctron-bare-tools)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$(dirname -- "$DIR")/.." && pwd)
CTC="$ROOT/compiler/ctc.sh"
T=$(mktemp -d /tmp/ctron_wasm_gate.XXXXXX)
trap 'rm -rf "$T"' EXIT
PASS=0; FAIL=0; SKIP=0

ok()   { PASS=$((PASS+1)); echo "ok $1 - $2"; }
bad()  { FAIL=$((FAIL+1)); echo "BAD $1 - $2"; }
skp()  { SKIP=$((SKIP+1)); echo "SKIP $1 - $2"; }

run_to() { # run_to <secs> <cmd...>:perl alarm 超时兜底(macOS 无 timeout)
    local secs=$1; shift
    perl -e 'alarm shift; exec @ARGV' "$secs" "$@"
}

echo "== ① 注册表面 =="
if "$CTC" targets 2>&1 | grep -q "wasm32-unknown-unknown"; then
    ok 1 "wasm32 靶在册"
else
    bad 1 "targets 表缺 wasm32"
fi

# node 三通道解析(PATH → 打包内置运行时通配 → docker node 镜像)
NODE_BIN=""; NODE_DOCKER=0
node_resolve() {
    if [ -n "$NODE_BIN" ] || [ "$NODE_DOCKER" = "1" ]; then return 0; fi
    if command -v node >/dev/null 2>&1; then NODE_BIN=$(command -v node); return 0; fi
    for g in "$HOME/Library/Application Support/"*"/NodeJS/runtime/"*/bin/node; do
        if [ -x "$g" ]; then NODE_BIN="$g"; return 0; fi
    done
    if docker image inspect node:22-alpine >/dev/null 2>&1; then NODE_DOCKER=1; return 0; fi
    return 1
}
wasm_run() { # wasm_run <wasm-path>:经解析通道跑胶水(docker 时 T/DIR 须挂载)
    if [ -n "$NODE_BIN" ]; then
        run_to 60 "$NODE_BIN" "$DIR/ct_wasm_run.js" "$1" 2>&1
    else
        run_to 120 docker run --rm -v "$T":/t -v "$DIR":/w -w /w node:22-alpine node ct_wasm_run.js "/t/$(basename "$1")" 2>&1
    fi
}

WASM_OK=1
"$CTC" build "$DIR/hello.ct" --target wasm32-unknown-unknown -o "$T/hello.wasm" >"$T/b_hello.out" 2>&1 || WASM_OK=0

if [ "$WASM_OK" != "1" ]; then
    if grep -q "工具链不可用" "$T/b_hello.out"; then
        # 编译工具链三通道全缺 = 环境登记 SKIP 门绿(bare 门③惯例;有工具链则门红不豁免)
        echo "== ②~⑧ 全跳(工具链缺;环境登记) =="
        skp 2 "wasm 编译工具链不在(env CTRON_WASM_CC/宿主 clang[wasm32]/docker 皆缺)"
        for n in 3 4 5 6 7 8; do skp "$n" "(随工具链)"; done
        echo "wasm 门: $PASS 过 / $FAIL 败 / $SKIP 跳(环境登记)"
        exit 0
    fi
    echo "== ②~⑥ 全跳(构建失败) =="
    bad 2 "hello.wasm 构建失败: $(tail -2 "$T/b_hello.out")"
    echo "wasm 门: $PASS 过 / $FAIL 败 / $SKIP 跳(环境登记)"
    exit 1
fi

echo "== ②~④ node 真跑 =="
if node_resolve; then
    GLUE="$DIR/ct_wasm_run.js"

    OUT=$(run_to 60 "$NODE_BIN" "$GLUE" "$T/hello.wasm" 2>&1); rc=$?
    if [ "$rc" = "0" ] && [ "$OUT" = "hello wasm" ]; then
        ok 2 "hello 真跑(wasm32→node;输出断言)"
    else
        bad 2 "hello 真跑不符: rc=$rc out=[$OUT]"
    fi

    "$CTC" build "$DIR/arith.ct" --target wasm32-unknown-unknown -o "$T/arith.wasm" >"$T/b_arith.out" 2>&1 || WASM_OK=0
    if [ "$WASM_OK" = "1" ]; then
        OUT=$(run_to 60 "$NODE_BIN" "$GLUE" "$T/arith.wasm" 2>&1); rc=$?
        if [ "$rc" = "0" ] && [ "$OUT" = "55
55
4.75" ]; then
            ok 3 "arith 真跑(I64/F64/递归)"
        else
            bad 3 "arith 真跑不符: rc=$rc out=[$OUT]"
        fi
    else
        bad 3 "arith.wasm 构建失败: $(tail -2 "$T/b_arith.out")"
    fi

    "$CTC" build "$DIR/strfmt.ct" --target wasm32-unknown-unknown -o "$T/strfmt.wasm" >"$T/b_strfmt.out" 2>&1 || WASM_OK=0
    if [ "$WASM_OK" = "1" ]; then
        OUT=$(run_to 60 "$NODE_BIN" "$GLUE" "$T/strfmt.wasm" 2>&1); rc=$?
        if [ "$rc" = "0" ] && [ "$OUT" = "hello wasm 42
13" ]; then
            ok 4 "strfmt 真跑(插值/拼接;GC arena 分配面)"
        else
            bad 4 "strfmt 真跑不符: rc=$rc out=[$OUT]"
        fi
    else
        bad 4 "strfmt.wasm 构建失败: $(tail -2 "$T/b_strfmt.out")"
    fi

    "$CTC" build "$DIR/gcload.ct" --target wasm32-unknown-unknown -o "$T/gcload.wasm" >"$T/b_gcload.out" 2>&1 || WASM_OK=0
    if [ "$WASM_OK" = "1" ]; then
        OUT=$(run_to 120 "$NODE_BIN" "$GLUE" "$T/gcload.wasm" 2>&1); rc=$?
        if [ "$rc" = "0" ] && [ "$OUT" = "1338890" ]; then
            ok 5 "gcload 真跑(5 万次插值 churn≈4.8MB;分配压力 T38 同源锚)"
        else
            bad 5 "gcload 真跑不符: rc=$rc out=[$OUT]"
        fi
    else
        bad 5 "gcload.wasm 构建失败: $(tail -2 "$T/b_gcload.out")"
    fi

    "$CTC" build "$DIR/dom.ct" --target wasm32-unknown-unknown -o "$T/dom.wasm" >"$T/b_dom.out" 2>&1 || WASM_OK=0
    if [ "$WASM_OK" = "1" ]; then
        OUT=$(run_to 60 "$NODE_BIN" "$GLUE" "$T/dom.wasm" 2>&1); rc=$?
        if [ "$rc" = "0" ] && [ "$OUT" = "dom-done
dom-title: Ctron wasm dom
dom-body: <b id='ct-body-marker'>ctron</b>" ]; then
            ok 6 "dom 桥真跑(set_title/set_body;node host shim;浏览器页=真 DOM 自检)"
        else
            bad 6 "dom 桥真跑不符: rc=$rc out=[$OUT]"
        fi
    else
        bad 6 "dom.wasm 构建失败: $(tail -2 "$T/b_dom.out")"
    fi
else
    skp 2 "node 不在(PATH 无 node 且无打包运行时且无 docker node 镜像;环境登记)"
    skp 3 "arith 真跑(随 node)"
    skp 4 "strfmt 真跑(随 node)"
    skp 5 "gcload 真跑(随 node)"
    skp 6 "dom 桥真跑(随 node)"
fi

echo "== ⑦ ffi 负锚 =="
if "$CTC" build "$DIR/ffi.neg.ct" --target wasm32-unknown-unknown -o "$T/ffi.wasm" >"$T/ffi.out" 2>&1; then
    bad 7 "wasm target 未拦截 #[link] FFI 面"
elif grep -q "FFI 在 wasm target 禁用" "$T/ffi.out"; then
    ok 7 "ffi 负锚(#[link] 诊断非静默;rc=2)"
else
    bad 7 "ffi 诊断文案缺失: $(tail -1 "$T/ffi.out")"
fi

echo "== ⑧ 体积报告 =="
if [ -f "$T/hello.wasm" ]; then
    SZ=$(wc -c < "$T/hello.wasm" | tr -d ' ')
    echo "  hello.wasm = ${SZ} 字节(web 档无 100KB 口径,仅报告)"
    ok 8 "体积报告(.wasm 口径)"
else
    bad 8 "hello.wasm 缺席(体积报告无源)"
fi

echo "wasm 门: $PASS 过 / $FAIL 败 / $SKIP 跳(环境登记)"
[ "$FAIL" = "0" ] && exit 0
exit 1
