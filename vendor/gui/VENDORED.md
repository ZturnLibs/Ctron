# vendor/gui —— GUI 依赖登记(规范 §12.4)

| 依赖 | 版本 | 来源(Release tarball) | 许可 | 裁剪 | 拉取日期 |
|---|---|---|---|---|---|
| raylib | 5.5 | https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz | zlib | 仅 `src/`(去掉 examples/projects);编译子集 = rcore/rglfw/rshapes/rtext/rtextures/utils | 2026-09-16 |
| FreeType | 2.13.3 | https://download.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.gz | FTL | 官方 release(configure 可执行);可选依赖全关(zlib/brotli/bzip2/png/harfbuzz) | 2026-09-19 |
| SheenBidi | 3.0.0 | https://codeload.github.com/Tehreer/SheenBidi/tar.gz/refs/tags/v3.0.0 | Apache-2.0 | 仅 `Headers/`+`Source/`(去 Tests/Tools/构建脚本);unity 单编译单元(`-DSB_CONFIG_UNITY`) | 2026-09-19 |
| Clay | v0.14 | https://github.com/nicbarker/clay/archive/refs/tags/v0.14.tar.gz | zlib(见上游 LICENSE) | 仅单头 `clay.h` | 2026-09-16 |

- 升级流程:换 tarball → `build.sh` 重建 → `tests/ffi` + GUI 黄金快照全绿 → 本表与
  `ctron_abi.h` 同步(发射面唯一真源,§11.1)。
- `build/` 为构建产物,不进 git。
