#!/bin/sh
# vendor/tls/build.sh —— mbedTLS 静态库构建(libmbedcrypto / libmbedx509 / libmbedtls)
#
# 版本登记:
#   版本  : mbedtls 3.6.7(LTS 3.6 分支最新补丁;指定候选 3.6.2 发布页上已有 3.6.7 可达,取新)
#   来源  : https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2
#   SHA256: a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6
#   许可  : Apache-2.0(上游双许可 Apache-2.0 OR GPL-2.0-or-later,本项目取 Apache-2.0;
#           LICENSE 随 vendor 入库,登记见 VENDORED.md)
#
# 构建系统决策:cc 直编,但源清单不手抄——三库对象清单(OBJS_CRYPTO/X509/TLS)在构建期
# 从上游 mbedtls/library/Makefile 原文解析,另配守卫:library/*.c 必须被三清单恰好全覆盖,
# 上游升级新增源文件时漏配会响亮失败而非静默缺编。
# 为何不走上游 Makefile:library/Makefile 的再生成机器(error.c / version_features.c /
# ssl_debug_helpers_generated.c / psa_crypto_driver_wrappers* 五件的 order-only 依赖)硬性
# 要求 scripts/ + framework/ 在树,缺文件即 "No rule to make target" 致命——即便这五件生成
# 物已随官方 tarball 发货、根本无需再生成。要么 vendor 多吞 7.5M(scripts/+framework/),
# 要么放假 stub 脚本(来源卫生灾难),都不如 cc 直编。为何不裸 glob library/*.c:crypto /
# x509 / tls 的三库划分只存在于上游 Makefile,glob 无从分库——故取「解析上游清单」折中。
# 裁剪后仅留 include/ library/ configs/ 3rdparty/ LICENSE README.md(50M → 8.3M);3rdparty/
# 必留:上游 Makefile 无条件 include 其 Makefile.inc,everest/p256-m 默认配置下为空壳
# (#if 门),对象仍按上游惯例编入 libmbedcrypto.a(零逻辑)。
# 配置 = 官方默认全量(config-thread.h 追加 pthread 线程安全),本任务不手工裁剪。
# 产物:build/lib/*.a(不进 git——根 .gitignore 全局 build/ 规则已覆盖);中间 .o 入
# build/objs/(同上);离线可重建(本脚本无任何网络动作)。
# 线程安全:server shim 多 worker 线程调用 → 经 -DMBEDTLS_CONFIG_FILE=\"config-thread.h\"
# 启用 MBEDTLS_THREADING_C + MBEDTLS_THREADING_PTHREAD(默认两者皆关)。
set -eu
cd "$(dirname "$0")"
ROOT="$(pwd)"
MF="mbedtls/library/Makefile"
mkdir -p build/lib build/objs

# ---- 从上游 Makefile 原文解析三库对象清单(单一真源,不手抄) ----
parse_objs() {
    awk -v name="$1" '
        index($0, "OBJS_" name "=") == 1 { grab = 1 }
        grab {
            line = $0
            cont = (line ~ /\\[ \t]*$/)
            sub(/\\[ \t]*$/, "", line)
            n = split(line, tok, /[ \t]+/)
            for (i = 1; i <= n; i++)
                if (tok[i] ~ /^[A-Za-z0-9_./-]+\.o$/) print tok[i]
            if (!cont) exit
        }' "$MF"
}
OBJS_CRYPTO=$(parse_objs CRYPTO)
OBJS_X509=$(parse_objs X509)
OBJS_TLS=$(parse_objs TLS)
[ -n "$OBJS_CRYPTO" ] && [ -n "$OBJS_X509" ] && [ -n "$OBJS_TLS" ] || {
    echo "build.sh FAIL: 上游 $MF 清单解析为空(Makefile 结构变了?)" >&2; exit 1; }

# ---- 3rdparty 对象清单(everest/p256-m,默认配置空壳;路径同上游 Makefile.inc) ----
# P3-C 收账修复(半解析):awk 的 `if (!cont) exit` 在清单块结束时终止【整个】
# awk——传入两个 Makefile.inc 时 everest 块结束即退,p256-m 的 2 个对象从不被
# 解析(静默半清单)。改为逐文件各跑一遍 awk(exit 语义不变:每文件首块即停),
# 外层拼接。修后 5 个 3rdparty 对象齐(p256-m_driver_entrypoints / p256-m 均入
# libmbedcrypto.a,同上游 OBJS_CRYPTO += THIRDPARTY_CRYPTO_OBJECTS 惯例;默认
# 配置下两者无引用,驱动入口 #if 门为空壳、p256-m 本体是死代码,入档无害)。
THIRD_OBJS=""
for inc in mbedtls/3rdparty/everest/Makefile.inc mbedtls/3rdparty/p256-m/Makefile.inc; do
    THIRD_OBJS="$THIRD_OBJS $(awk '
        /THIRDPARTY_CRYPTO_OBJECTS\+=/ { grab = 1; next }
        grab {
            line = $0
            cont = (line ~ /\\[ \t]*$/)
            sub(/\\[ \t]*$/, "", line)
            sub(/^.*THIRDPARTY_DIR\)./, "", line)  # 文本形如 $(THIRDPARTY_DIR)/x.o:剥到 ")/" 为止
            n = split(line, tok, /[ \t]+/)
            for (i = 1; i <= n; i++)
                if (tok[i] ~ /^[A-Za-z0-9_./-]+\.o$/) print tok[i]
            if (!cont) exit
        }' "$inc")"
done

# ---- 守卫:library/*.c 必须被三清单全覆盖(上游加源漏配 → 响亮失败) ----
# ALLOW_NOT_BUILT:上游 Makefile 有意不编的兼容空壳源(见 ecp_curves_new.c 头注:
# "kept only for compatibility with custom build systems in LTS branches",内容仅一行
# 防 ISO 空翻译单元 typedef),不算漏配。
ALLOW_NOT_BUILT="ecp_curves_new"
ls mbedtls/library/*.c | sed 's|.*/||; s|\.c$||' | sort > build/objs/.all_srcs
{ printf '%s\n' $OBJS_CRYPTO $OBJS_X509 $OBJS_TLS | sed 's|\.o$||'; printf '%s\n' $ALLOW_NOT_BUILT; } | sort -u > build/objs/.listed
if ! cmp -s build/objs/.all_srcs build/objs/.listed; then
    echo "build.sh FAIL: library/ 源与上游清单不一致:" >&2
    diff build/objs/.all_srcs build/objs/.listed >&2 || true
    exit 1
fi

# ---- 编译(含 3rdparty;include 路径照抄上游 */Makefile.inc 的 THIRDPARTY_INCLUDES) ----
CFLAGS_SUBSET="-O2 -I$ROOT -Imbedtls/include -Imbedtls/library \
 -Imbedtls/3rdparty/everest/include \
 -Imbedtls/3rdparty/everest/include/everest \
 -Imbedtls/3rdparty/everest/include/everest/kremlib \
 -Imbedtls/3rdparty/p256-m/p256-m/include \
 -Imbedtls/3rdparty/p256-m/p256-m/include/p256-m \
 -Imbedtls/3rdparty/p256-m/p256-m_driver_interface \
 -DMBEDTLS_CONFIG_FILE=\"config-thread.h\" -D_FILE_OFFSET_BITS=64"

compile() { # $1=源文件路径(相对 vendor/tls) $2=产物 .o(相对 vendor/tls)
    # P3-C 收账修复(陈旧度):-DMBEDTLS_CONFIG_FILE 指向的 config-thread.h 参与
    # 每个翻译单元,却不在 $1 -nt 判据里——配置头一改,.o 全数陈旧不重编。补
    # 第三判据:配置头比产物新即重编(空树冷建时 [ ! -f ] 已短路,语义不变)。
    if [ ! -f "$2" ] || [ "$1" -nt "$2" ] || [ config-thread.h -nt "$2" ]; then
        cc $CFLAGS_SUBSET -c "$1" -o "$2"
    fi
}
for o in $OBJS_CRYPTO $OBJS_X509 $OBJS_TLS; do
    compile "mbedtls/library/${o%.o}.c" "build/objs/${o%.o}.o"
done
for o in $THIRD_OBJS; do
    compile "mbedtls/3rdparty/${o%.o}.c" "build/objs/3rdparty-$(basename "$o")"
done

# ---- 出三库(crypto 吞 3rdparty 空壳,同上游 OBJS_CRYPTO += THIRDPARTY_CRYPTO_OBJECTS) ----
list_args() { for o in $1; do printf '%s ' "build/objs/${o%.o}.o"; done; }
CRYPTO_ARGS="$(list_args "$OBJS_CRYPTO")$(for o in $THIRD_OBJS; do printf '%s ' "build/objs/3rdparty-$(basename "$o")"; done)"
rm -f build/lib/libmbedcrypto.a build/lib/libmbedx509.a build/lib/libmbedtls.a
ar rcs build/lib/libmbedcrypto.a $CRYPTO_ARGS
ar rcs build/lib/libmbedx509.a $(list_args "$OBJS_X509")
ar rcs build/lib/libmbedtls.a $(list_args "$OBJS_TLS")

ls -l build/lib/
echo "vendor/tls/build/lib/*.a OK"
