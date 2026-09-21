#!/bin/sh
# vendor/deflate/smoke.sh —— libminiz.a 链接冒烟(体例同 vendor/tls/smoke.sh)
# 前置:sh build.sh、cc
set -eu
cd "$(dirname "$0")"
[ -f build/lib/libminiz.a ] || sh build.sh
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
cc -O1 -w -Iminiz -o "$T/smoke" smoke.c build/lib/libminiz.a
"$T/smoke"
