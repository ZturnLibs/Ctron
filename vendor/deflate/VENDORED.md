# vendor/deflate —— 压缩依赖登记(规范 §12.4,体例同 vendor/tls)

| 依赖 | 版本 | 来源(Release zip) | SHA256(zip) | 许可 | 裁剪 | 拉取日期 |
|---|---|---|---|---|---|---|
| miniz | 3.1.2(MZ_VERSION "11.3.2") | https://github.com/richgel999/miniz/releases/download/3.1.2/miniz-3.1.2.zip | f0446d863f9c19926ad9483c523fdc42e42b8d4a6a431d27e09d49c79a140d9a | MIT | 仅 amalgamated 三件 `miniz.c miniz.h LICENSE`(上游单文件惯例发布;去 examples/ readme.md ChangeLog.md);默认全量编译,未裁 API 宏 | 2026-09-21 |

- 构建:`build.sh`(cc -O2 直编单 TU → ar)。产物 `build/libminiz.a`(约 101K),
  完全离线,重建 <2s。
- 源件指纹:miniz.c `e2c1aeb66eef9191d8c3feb164db2def2335a61d039bf04ed849f6b042433b30`、
  miniz.h `b53b62ed122e559b8f679e3cb787a0b0035fe87a58f909da0e44931678f4e85f`。
- 消费面:本项目只用三件无状态 API —— `tdefl_compress_mem_to_mem`(raw deflate)、
  `tinfl_decompress_mem_to_mem`(raw inflate)、`mz_crc32`(gzip CRC-32);zlib/gzip
  容器组框在 Ctron 侧(std/http/enc.ct),不经 miniz 的 zlib 头/zip 归档层。三件
  均无共享状态(压缩器状态为调用栈局部),多 worker 并发安全,无线程宏需求。
- 冒烟:`smoke.sh` → 往返(重复/随机/空/单字节 × level 1/6/9)+ 炸弹出为负判据 +
  crc32 已知向量,过 = `DEFLATE-OK 11.3.2`。
- 升级流程:换 release zip → 按 `build.sh` 头注更新版本/SHA → `smoke.sh` 绿 → 本表同步。
- `build/` 为构建产物,不进 git(根 .gitignore 全局 `build/`、`*.o` 规则已覆盖)。
