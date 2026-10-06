#!/bin/sh
# build_fb.sh —— fb1 分支过渡代自举(一次代差:主树二进制模板无 fs_mkdir_p;下代起自含)
# 用法: sh build_fb.sh [引导发射器(缺省=主树 bin/ctron-emit)]
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BOOT=${1:-/Users/zyj/Zturn/Ctron/compiler/bin/ctron-emit}
cd "$DIR"; sh build.sh > /dev/null
"$BOOT" run build/cc_emit.ct > build/ce_fb.c
grep -q "^int32_t ctron_fs_mkdir_p" build/ce_fb.c || cat >> build/ce_fb.c <<'EOC'
int32_t ctron_fs_mkdir_p(const char* path) {
#include <sys/stat.h>
    char tmp[4096]; size_t len = strlen(path);
    if (len == 0 || len >= sizeof(tmp)) { return 0; }
    memcpy(tmp, path, len + 1);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = 0; if (mkdir(tmp, 0755) != 0 && errno != EEXIST) { return 0; } *p = '/'; }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) { return 0; }
    return 1;
}
EOC
cc -O2 build/ce_fb.c -o bin/ctron-emit
./bin/ctron-emit run build/cc_run.ct > build/cc_fb.c
cc -O2 build/cc_fb.c -o bin/ctron-cc
echo "build_fb: GREEN (emit+cc rebuilt with FB-1/FB-2 semantics)"
