# vendor/tls —— TLS 依赖登记(规范 §12.4,体例同 vendor/gui)

| 依赖 | 版本 | 来源(Release tarball) | SHA256 | 许可 | 裁剪 | 拉取日期 |
|---|---|---|---|---|---|---|
| mbedTLS | 3.6.7(LTS 3.6 分支) | https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2 | a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6 | Apache-2.0(上游双许可取一) | 仅 `include/ library/ configs/ 3rdparty/ LICENSE README.md`(去 programs/ tests/ docs/ doxygen/ framework/ scripts/ 等,50M→8.3M);配置 = 默认全量 + pthread 线程安全(`config-thread.h`),未手工裁 | 2026-09-21 |

- 构建:`build.sh`(cc 直编,三库对象清单构建期解析自上游 `library/Makefile`,含
  library/*.c 全覆盖守卫;上游 Makefile 的再生成机器需 scripts/+framework/,故不走 make)。
  产物 `build/lib/libmbedcrypto.a`(788K)/`libmbedx509.a`(88K)/`libmbedtls.a`(434K),
  全量重建约 13s(8 核),完全离线。
- 线程安全:`MBEDTLS_THREADING_C + MBEDTLS_THREADING_PTHREAD`(默认关;server shim 多
  worker 线程调用必须开)。注意上游自带 `configs/config-thread.h` 是 Thread 协议极简配置,
  与本项目 `vendor/tls/config-thread.h`(默认全量 + 两宏)无关。
- 冒烟:`smoke.sh` → 链三库实例化 ssl_config/ssl_context/ctr_drbg(全离线),过 = `TLS-OK <version>`。
- 升级流程:换 tarball → 按 `build.sh` 头注更新版本/SHA256 → `smoke.sh` 绿 → 本表同步。
- `build/` 为构建产物,不进 git(根 .gitignore 全局 `build/`、`*.o` 规则已覆盖)。
