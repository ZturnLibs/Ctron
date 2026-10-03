#!/bin/sh
# tests/doctest/run.sh —— doc-test(/// ```c 围栏块)机制验收(spec §10.4;README §8)
#
# 口径(与 suite.py 自宿臂同子命令:ctron-cc run;检查驱动 ctron-chk run 同提升):
#   格式锚 tests/00_doctest.ct 由主流 suite 承载(行为面);本件钉提升器的
#   负向/纪律面,主流四套件不便表达的夹具由此直驱:
#     执行面负例  例码断言失败 = run rc≠0 且输出含 assert_eq failed
#     编译面负例  例码类型错   = chk run rc≠0 且诊断含码
#     标签纪律    非 `c` 标签围栏 = 散文不执行
#     丢弃纪律    未闭合/中途混非文档行 = 丢块不泄漏
#     行号零漂移  提升只尾部追加,原文诊断行号不变
#     面盲区      emit 驱动不见合成块
# 前置:compiler/native.sh(产出 bin/ctron-cc、bin/ctron-chk、bin/ctron-emit)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CC_BIN="$ROOT/compiler/bin/ctron-cc"
CHK_BIN="$ROOT/compiler/bin/ctron-chk"
EMIT="$ROOT/compiler/bin/ctron-emit"
CORPUS="$DIR/corpus"

if [ ! -x "$CC_BIN" ] || [ ! -x "$EMIT" ]; then
    echo "doctest/run: 缺少编译器二进制(先: compiler/native.sh)" >&2
    exit 2
fi

T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
pass=0
fail=0

ok()   { pass=$((pass + 1)); echo "  ok: $1"; }
bad()  { fail=$((fail + 1)); echo "BAD: $1"; }

echo "== tests/doctest doc-test 机制验收(§10.4)=="

# ---- 格式锚:多块提升 + 非 c 标签跳过(run rc=0 即两 c 块全过、text 块未执行)----
"$CC_BIN" run "$ROOT/tests/00_doctest.ct" > "$T/anchor.run" 2>&1 && \
    ok "00_doctest.ct 格式锚:run 绿(多 c 块提升,text 块跳过)" || \
    bad "00_doctest.ct 格式锚: $(cat "$T/anchor.run")"

# ---- 执行面负例:例码断言失败 = 测试失败 ----
f="$CORPUS/dt_fail_assert.ct"
if "$CC_BIN" run "$f" > "$T/fa.run" 2>&1; then
    bad "dt_fail_assert: 断言失败未被判定为测试失败(rc=0)"
elif grep -q "assert_eq failed" "$T/fa.run"; then
    ok "dt_fail_assert: run rc≠0 且输出含 assert_eq failed"
else
    bad "dt_fail_assert: rc≠0 但输出无断言消息: $(cat "$T/fa.run")"
fi

# ---- 编译面负例:例码类型错 = 检查驱动拦截(运行驱动同判) ----
f="$CORPUS/dt_fail_compile.ct"
if "$CHK_BIN" run "$f" > "$T/fc.chk" 2>&1; then
    bad "dt_fail_compile: 例码类型错未被 chk 拦截"
elif grep -q "E2010" "$T/fc.chk"; then
    ok "dt_fail_compile: chk run rc≠0 且诊断含 E2010"
else
    bad "dt_fail_compile: rc≠0 但缺 E2010: $(cat "$T/fc.chk")"
fi
if "$CC_BIN" run "$f" > "$T/fc2.run" 2>&1; then
    bad "dt_fail_compile: 运行驱动未同判(例码须先编译)"
else
    ok "dt_fail_compile: ctron-cc run 同判 rc≠0(双驱动同源提升)"
fi

# ---- 标签纪律:非 c 标签围栏 = 散文 ----
f="$CORPUS/dt_skip_prose.ct"
"$CC_BIN" run "$f" > "$T/sp.run" 2>&1 && \
    ok "dt_skip_prose: text/裸围栏未执行(999/998 断言未触发)" || \
    bad "dt_skip_prose: $(cat "$T/sp.run")"

# ---- 丢弃纪律:未闭合围栏 = 丢块 ----
f="$CORPUS/dt_skip_unterminated.ct"
"$CC_BIN" run "$f" > "$T/su.run" 2>&1 && \
    ok "dt_skip_unterminated: 未闭合块丢弃(999 断言未执行)" || \
    bad "dt_skip_unterminated: $(cat "$T/su.run")"

# ---- 丢弃纪律:中途混非文档行 = 丢块不泄漏、真声明照常编译 ----
f="$CORPUS/dt_skip_midbreak.ct"
"$CC_BIN" run "$f" > "$T/sm.run" 2>&1 && \
    ok "dt_skip_midbreak: 块丢弃且 add2 照常编译(test 块 2+3=5 过)" || \
    bad "dt_skip_midbreak: $(cat "$T/sm.run")"

# ---- 行号零漂移:原文诊断行号不变(JSON 面 span 精确判读;文本面 sem 诊断不带行号)----
f="$CORPUS/dt_lineno.ct"
if "$CHK_BIN" run "$f" --format=json > "$T/ln.chk" 2>&1; then
    bad "dt_lineno: 预期类型错未被拦截"
elif grep -q '"line_start":10,' "$T/ln.chk"; then
    ok "dt_lineno: JSON span 仍报第 10 行(提升未插行)"
else
    bad "dt_lineno: 诊断行号漂移: $(cat "$T/ln.chk")"
fi

# ---- 面盲区:emit 驱动不见合成块 ----
f="$CORPUS/dt_emit_blind.ct"
if "$EMIT" run "$f" > "$T/eb.c" 2>"$T/eb.err" && grep -q "add2" "$T/eb.c"; then
    if grep -q "doctest" "$T/eb.c"; then
        bad "dt_emit_blind: 发射 C 混入合成 doctest 块"
    else
        ok "dt_emit_blind: 发射 C 含 add2 且无 doctest(面盲区成立)"
    fi
else
    bad "dt_emit_blind: emit 失败: $(cat "$T/eb.err")"
fi

echo
echo "doctest/run: $pass ok, $fail bad"
[ "$fail" -eq 0 ] || exit 1
