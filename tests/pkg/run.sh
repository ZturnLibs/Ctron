#!/bin/sh
# tests/pkg/run.sh —— T49 本地 registry 全链路 e2e:
#   publish → add → lock 二跑稳定(内容寻址)→ dep 探针编译消费 → 篡改 E5053 拦截
# 用法:tests/pkg/run.sh(需 compiler/bin/{ctron-cc,ctron-dep} 已构建;native.sh)
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CTC="$ROOT/compiler/ctc.sh"
export CTRON_STDPATH="$ROOT/lib/std"
T=$(mktemp -d /tmp/ctron_pkg_e2e.XXXXXX)
trap 'rm -rf "$T"' EXIT
fails=0
ck() { if [ "$1" = 0 ]; then echo "ok: $2"; else echo "FAIL: $2"; fails=$((fails+1)); fi; }

export CTRON_REGPATH="$T/reg"
mkdir -p "$T/libmath/src"
printf 'pkg {\n    manifest_version = 1\n    name = "libmath"\n    version = "1.0.0"\n}\n' > "$T/libmath/Ctron.ctcl"
printf 'pub fn add(a: I32, b: I32) -> I32 {\n    return a + b\n}\n' > "$T/libmath/src/main.ct"
(cd "$T/libmath" && "$ROOT/ctron" publish) >/dev/null 2>&1; ck $? "publish 落盘"
[ -f "$CTRON_REGPATH/libmath/1.0.0/sha256" ]; ck $? "sha256 产物在"
(cd "$T/libmath" && "$ROOT/ctron" publish) >/dev/null 2>&1; [ $? -ne 0 ]; ck $? "二次发布被拒(版本不可覆盖)"

mkdir -p "$T/app/src"
printf 'pkg {\n    manifest_version = 1\n    name = "app"\n    version = "0.1.0"\n}\n' > "$T/app/Ctron.ctcl"
printf 'use libmath.{add}\n\nfn main() -> I32 {\n    println(add(2, 3).to_string())\n    return 0\n}\n' > "$T/app/src/main.ct"
(cd "$T/app" && "$ROOT/ctron" add libmath >/dev/null 2>&1); ck $? "add 装包"
grep -q 'dep "libmath"' "$T/app/Ctron.ctcl"; ck $? "dep 块写入清单"
[ -f "$T/app/pkgs/libmath/libmath.ct" ] && [ -f "$T/app/pkgs/libmath/Ctron.ctcl" ]; ck $? "安装位 module+manifest"
[ -f "$T/app/Ctron.lock" ]; ck $? "lock 生成"
cp "$T/app/Ctron.lock" "$T/lock1"
(cd "$T/app" && "$ROOT/ctron" lock >/dev/null 2>&1)
cmp -s "$T/lock1" "$T/app/Ctron.lock"; ck $? "lock 二跑稳定(内容寻址)"
"$CTC" check "$T/app/src/main.ct" >/dev/null 2>&1; ck $? "dep 探针编译消费"
printf '\n// tampered\n' >> "$T/app/pkgs/libmath/libmath.ct"
"$ROOT/compiler/bin/ctron-dep" run "$T/app/Ctron.ctcl" >/dev/null 2>&1; [ $? -ne 0 ]; ck $? "篡改被拦(rc=1)"
"$ROOT/compiler/bin/ctron-dep" run "$T/app/Ctron.ctcl" 2>&1 | grep -q E5053; ck $? "E5053 诊断在"

if [ $fails -eq 0 ]; then echo "tests/pkg: 全链路绿"; exit 0; fi
echo "tests/pkg: $fails 失败"; exit 1
