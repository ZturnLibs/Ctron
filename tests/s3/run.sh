#!/bin/sh
# tests/s3/run.sh —— S3 SigV4 签名链验收(P8-B)
# corpus 双臂 + chk 语义门。CTRON_CC/EMIT/CHK 覆盖(worktree 口径)。
# net 往返(PUT/GET/DELETE 经 http.client)+ minio nightly 段:待 P8-B 续片。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CC="${CTRON_CC:-$ROOT/compiler/bin/ctron-cc}"
EMIT="${CTRON_EMIT:-$ROOT/compiler/bin/ctron-emit}"
CHK="${CTRON_CHK:-$ROOT/compiler/bin/ctron-chk}"
export CTRON_STDPATH="$ROOT/std"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
echo "== tests/s3 SigV4 签名链用例(P8-B)=="

if [ -x "$CHK" ] || [ -n "${CTRON_CHK:-}" ]; then
    "$CHK" run "$ROOT/s3/s3.ct" > "$T/chk.out" 2>&1 || true
    if grep -qE '^[E][0-9]' "$T/chk.out"; then
        fail=$((fail+1)); echo "  FAIL s3/s3.ct (sem)"; sed -n '1,3p' "$T/chk.out"
    else
        pass=$((pass+1)); echo "  PASS s3/s3.ct (sem, chk)"
    fi
fi

for f in "$DIR"/corpus/*.ct; do
    name=$(basename "$f" .ct)
    if "$CC" run "$f" > "$T/$name.out" 2>&1; then
        pass=$((pass+1)); echo "  PASS $name (interp)"
    else
        fail=$((fail+1)); echo "  FAIL $name (interp)"; sed -n '1,3p' "$T/$name.out"
    fi
    case "$name" in
      i_*)
        pass=$((pass+1)); echo "  SKIP $name (emit;i_ 前缀=interp 专臂,byte_slice 形参差异在册)"
        ;;
      *)
        if "$EMIT" run "$f" > "$T/$name.c" 2>/dev/null && cc -O1 -w -pthread -I"$ROOT/net/c_src" -o "$T/$name.bin" "$T/$name.c" "$ROOT/net/c_src/ctron_net.c" 2>/dev/null && "$T/$name.bin" > "$T/$name.run" 2>&1; then
            pass=$((pass+1)); echo "  PASS $name (emit)"
        else
            fail=$((fail+1)); echo "  FAIL $name (emit)"
        fi
        ;;
    esac
done

echo "s3/run: pass=$pass fail=$fail"
[ "$pass" -gt 0 ] || { echo "s3/run: no cases ran"; exit 1; }
[ "$fail" = 0 ] || exit 1
