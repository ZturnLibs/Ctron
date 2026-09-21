# tests/http/bench/pico —— picohttpparser vendored 登记(测试线专用,体例同 vendor/deflate VENDORED.md)

| 依赖 | 版本 | 来源 | 许可 | 裁剪 | 拉取日期 |
|---|---|---|---|---|---|
| picohttpparser | 1.dev(master 快照,`PICOHTTPPARSER_VERSION "1.dev"`) | https://github.com/h2o/picohttpparser(master;release 分支为 1.x 快照,本件取 master) | MIT / Perl 双许可(README 同文) | 仅两件 `picohttpparser.c picohttpparser.h` + 上游 `README.md` 原文(去 upstream test.c 等) | 2026-09-21 |

- **源件指纹(SHA256,vendored 时点)**:
  - `picohttpparser.c` `ddada2e27e9010f678a68a93a08fc13dee32178cc497b602322d80900eb94044`
  - `picohttpparser.h` `1fc9074dd12418b2b91e55ef3a8279bccf8d3577b790dea6f2f908ab2a157f1c`
- 用途:**仅测试线基准对照**(tests/http/bench,CTRON_HTTP_BENCH=1 显式启用),不进任何产物、不入 std/、不入 CI 主环。
- 升级流程:重取 master → 覆盖三件 → 本表更新版本/SHA256 → bench.sh digest pin 复跑 → 本表同步。
- 上游 README.md 保持逐字节原文(provenance 纪律,同 miniz "provenance byte-perfect" 口径);本目录口径与采数登记在 `../README.md`。
