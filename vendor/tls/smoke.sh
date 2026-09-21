#!/bin/sh
# vendor/tls/smoke.sh —— 链接冒烟:重建三静态库,链 smoke.c 并运行(全离线)。
set -eu
cd "$(dirname "$0")"

sh build.sh

cc -O2 -I. -Imbedtls/include -DMBEDTLS_CONFIG_FILE=\"config-thread.h\" \
   smoke.c -o build/smoke \
   build/lib/libmbedtls.a build/lib/libmbedx509.a build/lib/libmbedcrypto.a \
   -lpthread

exec ./build/smoke
