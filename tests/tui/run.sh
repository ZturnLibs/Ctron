#!/bin/sh
# tests/tui/run.sh —— pkgs/tui 门阶梯(s1–s9+e8 负例语料;设计 §12;ci.sh 挂载随批)
# 口径:发射臂单臂(interp 臂用户 extern 须宿主内链,W3a 探针在册);
#       headless 默认(stdin </dev/null),--run 才进真终端(README 手测清单)。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$DIR/../.." && pwd)
EMIT="${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}"
[ -x "$EMIT" ] || { echo "tui: 缺 compiler/bin/ctron-emit(先: compiler/native.sh)" >&2; exit 2; }
export CTRON_STDPATH="$ROOT/lib/std"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT

pass=0; fail=0
for d in "$DIR"/*/; do
    [ -f "$d/src/main.ct" ] || continue
    name=$(basename "$d")
    if ! "$EMIT" run "$d/src/main.ct" > "$T/$name.c" 2>"$T/$name.err"; then
        echo "  FAIL $name (emit): $(tail -1 "$T/$name.err")"; fail=$((fail+1)); continue
    fi
    if ! cc -O1 -w -pthread -o "$T/$name.bin" "$T/$name.c" "$ROOT/pkgs/tui/c_src/tui_term.c" 2>"$T/$name.ccerr"; then
        echo "  FAIL $name (cc): $(head -3 "$T/$name.ccerr" | tr '\n' ' ')"; fail=$((fail+1)); continue
    fi
    if "$T/$name.bin" < /dev/null > "$T/$name.out" 2>&1 && grep -qE "$name-probe .+" "$T/$name.out"; then
        pass=$((pass+1)); echo "  PASS $name ($(grep -E 'tui-' "$T/$name.out" | head -1))"
    else
        echo "  FAIL $name (run): $(tail -2 "$T/$name.out" | tr '\n' ' ')"; fail=$((fail+1))
    fi
done
echo "== tui: $pass pass / $fail fail =="
[ "$fail" -eq 0 ] || exit 1
