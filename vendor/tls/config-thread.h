/*
 * vendor/tls/config-thread.h —— Ctron 的 mbedTLS 编译配置
 *
 * 内容 = 官方默认配置(mbedtls/config.h)全量 + pthread 线程安全两宏。
 * 背景:server shim 将从多个 worker 线程调用 mbedTLS(ssl / ctr_drbg 等),
 * 默认配置 MBEDTLS_THREADING_C / MBEDTLS_THREADING_PTHREAD 皆关(非线程安全),
 * 必须显式启用;本任务不做手工体积裁剪(体积后置独立 size pass)。
 *
 * 启用途径:build.sh 以 -DMBEDTLS_CONFIG_FILE=\"config-thread.h\" 编译三库与冒烟;
 * 注意上游自带的 configs/config-thread.h 是 Thread 协议极简配置,与本文件无关。
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef CTRON_MBEDTLS_CONFIG_THREAD_H
#define CTRON_MBEDTLS_CONFIG_THREAD_H

#include "mbedtls/mbedtls_config.h" /* 官方默认全量配置(3.x 名称;2.x 曾叫 config.h) */

#define MBEDTLS_THREADING_C       /* 加锁抽象层 */
#define MBEDTLS_THREADING_PTHREAD /* pthread_mutex 具体实现 */

#endif /* CTRON_MBEDTLS_CONFIG_THREAD_H */
