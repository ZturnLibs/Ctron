#!/bin/sh
# tests/gui/w2_fold/run.sh —— W2:--dump-gui 折叠 IR 黄金差分
# 竞态免疫:构建→哨兵校验→快照→host 直跑(规避 ctc.sh 内部 build 与并行泳道的撞车)
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT

for i in 1 2 3 4 5; do
    sh "$ROOT/compiler/build.sh" >/dev/null 2>&1
    cp "$ROOT/compiler/build/cc_check.ct" "$T/chk.ct"
    if grep -q "gui E8100: on 期待块开" "$T/chk.ct" && grep -q "var ob: Str" "$T/chk.ct"; then
        break
    fi
    sleep 1
done

sed -e "s|ANCHORINPUT|$DIR/src/main.ct|" \
    -e "s|ANCHORFMT|0|" -e "s|ANCHORPROFILE|full|" -e "s|ANCHORTAUSTED|0|" \
    -e "s|ANCHORDUMPGUI|1|" -e "s|ANCHORLANG||" "$T/chk.ct" > "$T/chk_run.ct"

"$ROOT/compiler-c/build/ctronc" run "$T/chk_run.ct" > "$T/out.txt" 2>&1

if diff -u "$DIR/expected.txt" "$T/out.txt" > "$T/diff.txt" 2>&1; then
    echo "w2: --dump-gui 折叠 IR 黄金全绿"
else
    echo "w2: 黄金差分失败:" >&2
    cat "$T/diff.txt" >&2
    exit 1
fi
