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
    case $name in
        s6_panic) continue ;;   # pty 专臂(下方 script 块);headless 入口按设计防呆炸
    esac
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
# ---- s6a 解码向量(piped stdin;设计 §6 表)----
if "$EMIT" run "$DIR/s6_decode/src/main.ct" > "$T/s6d.c" 2>"$T/s6d.err" \
   && cc -O1 -w -pthread -o "$T/s6d.bin" "$T/s6d.c" "$ROOT/pkgs/tui/c_src/tui_term.c" 2>/dev/null; then
    printf '\033[A\033[Bx\r\033[<0;5;3M\177\033' | "$T/s6d.bin" > "$T/s6d.out" 2>&1
    ok6=1
    grep -qx "KEY 1001" "$T/s6d.out" || ok6=0
    grep -qx "KEY 1002" "$T/s6d.out" || ok6=0
    grep -qx "CHAR 120" "$T/s6d.out" || ok6=0
    grep -qx "KEY 1011" "$T/s6d.out" || ok6=0
    grep -qx "MOUSE 0 4 2" "$T/s6d.out" || ok6=0
    grep -qx "KEY 1013" "$T/s6d.out" || ok6=0
    grep -qx "KEY 1014" "$T/s6d.out" || ok6=0
    if [ "$ok6" -eq 1 ]; then
        pass=$((pass+1)); echo "  PASS s6_decode (7 向量)"
    else
        fail=$((fail+1)); echo "  FAIL s6_decode: $(tr '\n' '|' < "$T/s6d.out")"
    fi
else
    fail=$((fail+1)); echo "  FAIL s6_decode (build): $(tail -1 "$T/s6d.err")"
fi

# ---- s6b panic 还原+消息重放(script pty;§18)----
if "$EMIT" run "$DIR/s6_panic/src/main.ct" > "$T/s6p.c" 2>"$T/s6p.err" \
   && cc -O1 -w -pthread -o "$T/s6p.bin" "$T/s6p.c" "$ROOT/pkgs/tui/c_src/tui_term.c" 2>/dev/null; then
    printf '\033[B' | script -q /dev/null "$T/s6p.bin" > "$T/s6p.out" 2>&1
    if python3 -c "
import sys
data = open('$T/s6p.out', 'rb').read()
restore = data.find(b'?1006l')
marker = data.find(b'tui-replay-marker')
alt = data.find(b'?1049h')
if alt < 0 or restore < 0 or marker < 0:
    sys.exit(1)
sys.exit(0 if (alt < restore < marker) else 1)
"; then
        pass=$((pass+1)); echo "  PASS s6_panic (alt→还原首段→重放标记有序)"
    else
        fail=$((fail+1)); echo "  FAIL s6_panic: $(head -c 200 "$T/s6p.out" | tr '\n' '|')"
    fi
else
    fail=$((fail+1)); echo "  FAIL s6_panic (build): $(tail -1 "$T/s6p.err")"
fi

echo "== tui: $pass pass / $fail fail =="
[ "$fail" -eq 0 ] || exit 1
