#!/bin/sh
# tests/plugins/run.sh —— T52 插件沙箱阶梯(derive + lint;§8.3/§10.6)
# 五锚:json_demo 端到端(interp+emit 双臂)/ lint_demo W9001 + 确定性双跑 /
# no_decl E5060 负锚 / sandbox_escape E6020 负锚(插件触 I/O)。
# 前置:compiler/native.sh(bin/ctron-cc/-chk/-emit)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CC="${CTRON_CC:-$ROOT/compiler/bin/ctron-cc}"
CHK="${CTRON_CHK:-$ROOT/compiler/bin/ctron-chk}"
EMIT="${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}"
export CTRON_STDPATH="$ROOT/lib/std"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
ok()  { pass=$((pass+1)); echo "  PASS $1"; }
bad() { fail=$((fail+1)); echo "  FAIL $1"; [ $# -gt 1 ] && { echo "    ---- 实际输出 ----"; sed -n '1,6p' "$2"; }; }

echo "== tests/plugins T52 插件沙箱阶梯 =="

# 1) derive(Json) 端到端(解释臂;W2 起 round-trip+三 kind 负例随锚)
"$CC" run "$ROOT/tests/plugins/json_demo/src/main.ct" > "$T/json.out" 2>&1
if [ $? -eq 0 ] && grep -q '{"x":1,"s":"hi"}' "$T/json.out" && grep -q 'json-ok' "$T/json.out" && grep -q 'rt-ok' "$T/json.out"; then
    ok "json_demo interp(derive 展开端到端+W2 round-trip)"
else
    bad "json_demo interp" "$T/json.out"
fi

# 2) derive(Json) 端到端(发射臂:emit → cc → 运行)
if "$EMIT" run "$ROOT/tests/plugins/json_demo/src/main.ct" > "$T/json.c" 2>"$T/json.err" && \
   cc -o "$T/jsonbin" "$T/json.c" 2>/dev/null && "$T/jsonbin" > "$T/json_native.out" 2>&1; then
    if grep -q '{"x":1,"s":"hi"}' "$T/json_native.out" && grep -q 'json-ok' "$T/json_native.out" && grep -q 'rt-ok' "$T/json_native.out"; then
        ok "json_demo emit(interp/emit 同判+W2 round-trip)"
    else
        bad "json_demo emit(输出不符)" "$T/json_native.out"
    fi
else
    bad "json_demo emit(构建失败)" "$T/json.err"
fi

# 3) lint_demo:W9001 警告(W 级 rc=0)
"$CHK" run "$ROOT/tests/plugins/lint_demo/src/main.ct" > "$T/lint.out" 2>&1
if grep -q 'W9001' "$T/lint.out" && grep -q '函数过长' "$T/lint.out"; then
    ok "lint_demo W9001(lint 插件管线)"
else
    bad "lint_demo W9001" "$T/lint.out"
fi

# 4) 确定性锚:同输入双跑诊断逐字一致
"$CHK" run "$ROOT/tests/plugins/lint_demo/src/main.ct" > "$T/lint2.out" 2>&1
if cmp -s "$T/lint.out" "$T/lint2.out"; then
    ok "确定性双跑(lint 诊断逐字一致)"
else
    bad "确定性双跑" "$T/lint2.out"
fi

# 5) no_decl 负锚:@derive 未声明插件 = E5060
"$CHK" run "$ROOT/tests/plugins/no_decl/src/main.ct" > "$T/nodecl.out" 2>&1
if grep -q 'E5060' "$T/nodecl.out"; then
    ok "no_decl E5060(未声明 derive 插件)"
else
    bad "no_decl E5060" "$T/nodecl.out"
fi

# 6) sandbox_escape 负锚:插件内触 I/O = E6020
"$CHK" run "$ROOT/tests/plugins/sandbox_escape/src/main.ct" > "$T/escape.out" 2>&1
if grep -q 'E6020' "$T/escape.out" && grep -q '插件沙箱违规' "$T/escape.out"; then
    ok "sandbox_escape E6020(沙箱逃逸拦截)"
else
    bad "sandbox_escape E6020" "$T/escape.out"
fi

# 7) derive_unsupported 负锚(W1,2026-10-10):F64/带实参型别 → 陷阱 fn
#    (sem 未解析调用,型别名嵌于陷阱名;插件无诊断通道的 v1 告警信道)
"$CC" run "$ROOT/tests/plugins/derive_unsupported/src/main.ct" > "$T/uns.out" 2>&1
if [ $? -ne 0 ] && grep -q 'unsupported_json_field_WithF_F64' "$T/uns.out"; then
    ok "derive_unsupported 陷阱(F64/带实参型别,错误嵌型别名)"
else
    bad "derive_unsupported 陷阱" "$T/uns.out"
fi

# 8) 嵌套 round-trip(W2,2026-10-10;interp 专臂——emit struct 值链面静默错编在册)
"$CC" run "$ROOT/tests/plugins/json_demo/src/nested.ct" > "$T/nested.out" 2>&1
if [ $? -eq 0 ] && grep -q 'rt2-ok' "$T/nested.out"; then
    ok "json_demo_nested interp 专臂(嵌套 round-trip)"
else
    bad "json_demo_nested interp 专臂" "$T/nested.out"
fi

echo "== 插件沙箱阶梯:$pass 过 / $fail 败 =="
[ "$fail" -eq 0 ]
