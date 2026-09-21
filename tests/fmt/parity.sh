#!/bin/sh
# parity.sh —— ctron fmt 三宿主对拍门禁(R-P2d 双宿主移植验收,ci.sh 可挂载)
#
# 三宿主:compiler-rust(参考实现,ctron fmt)/ compiler-c(ctronc fmt)/ compiler(自举,ctron-fmt)
# 对语料逐文件断言:三方退出码一致,且成功时 stdout 逐字节一致(docs/fmt-spec.md 唯一权威)。
# 词法脏语料(如 *.neg.ct 分号面)= 三方一致非零退出,计入 errskip(规范 R8:fmt 报错退出)。
# 根目录:默认 tests examples std(roadmap/ 阶段区跳过);可传参覆盖。
#
# 前置:
#   make -C compiler-c                     # C 宿主 ctronc(含 fmt 子命令)
#   sh compiler/native.sh                  # 自举 ctron-fmt
#   compiler-rust/target/release/ctron     # 缺则自动 cargo build --release
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
RUST="$DIR/compiler-rust/target/release/ctron"
CC="$DIR/compiler-c/build/ctronc"
BOOT="$DIR/compiler/bin/ctron-fmt"

if [ ! -x "$RUST" ]; then
    echo "parity: 构建 Rust 参考实现(cargo build --release)..."
    (cd "$DIR/compiler-rust" && cargo build --release) > /dev/null 2>&1 \
        || { echo "parity: 缺少 $RUST 且 cargo 构建失败" >&2; exit 2; }
fi
[ -x "$CC" ] || { echo "parity: 缺少 $CC(先: make -C compiler-c)" >&2; exit 2; }
[ -x "$BOOT" ] || { echo "parity: 缺少 $BOOT(先: sh compiler/native.sh)" >&2; exit 2; }

T=$(mktemp -d /tmp/ctron_parity.XXXXXX)
trap 'rm -rf "$T"' EXIT

ROOTS=${*:-"$DIR/tests $DIR/examples $DIR/std"}
FILES=$(find $ROOTS -name '*.ct' -type f | grep -v '/roadmap/' | LC_ALL=C sort)
ok=0; errskip=0; bad=0
for f in $FILES; do
    "$RUST" fmt "$f" > "$T/r.out" 2>/dev/null; rr=$?
    "$CC" fmt "$f" > "$T/c.out" 2>/dev/null; cr=$?
    "$BOOT" run "$f" > "$T/b.out" 2>/dev/null; br=$?
    if [ "$rr" -ne 0 ] && [ "$rr" -eq "$cr" ] && [ "$rr" -eq "$br" ]; then
        errskip=$((errskip+1))
        continue
    fi
    if [ "$rr" -eq 0 ] && [ "$cr" -eq "$rr" ] && [ "$br" -eq "$rr" ] \
       && cmp -s "$T/r.out" "$T/c.out" && cmp -s "$T/r.out" "$T/b.out"; then
        ok=$((ok+1))
    else
        bad=$((bad+1))
        echo "  DIFF($rr/$cr/$br): $f"
    fi
done
echo "parity: $ok 绿 / $errskip 词法脏一致报错 / $bad 分歧"
[ "$bad" -eq 0 ]
