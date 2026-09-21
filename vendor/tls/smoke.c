/*
 * vendor/tls/smoke.c —— 链接冒烟(全离线,不联网不握手)
 *
 * 验证 build/lib 三静态库可链,且默认配置(+ pthread 线程层)可实例化:
 * ssl_config / ssl_context / ctr_drbg(熵源实例化一次)。成功打 "TLS-OK <version>"。
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>

#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/version.h"

int main(void) {
    mbedtls_ssl_config cfg;
    mbedtls_ssl_context ssl;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    int rc;

    mbedtls_ssl_config_init(&cfg);
    mbedtls_ssl_init(&ssl);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&drbg);

    rc = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
                               (const unsigned char *)"ctron-smoke", 11);
    if (rc != 0) { printf("TLS-FAIL ctr_drbg_seed rc=%d\n", rc); return 1; }

    rc = mbedtls_ssl_config_defaults(&cfg, MBEDTLS_SSL_IS_CLIENT,
                                     MBEDTLS_SSL_TRANSPORT_STREAM,
                                     MBEDTLS_SSL_PRESET_DEFAULT);
    if (rc != 0) { printf("TLS-FAIL config_defaults rc=%d\n", rc); return 1; }

    mbedtls_ssl_conf_rng(&cfg, mbedtls_ctr_drbg_random, &drbg);

    rc = mbedtls_ssl_setup(&ssl, &cfg);
    if (rc != 0) { printf("TLS-FAIL ssl_setup rc=%d\n", rc); return 1; }

    printf("TLS-OK %s\n", MBEDTLS_VERSION_STRING);

    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&cfg);
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&entropy);
    return 0;
}
